# AireRecord / AirUI

Windows 桌面性能录制与分析工具。UI 层 `AirUI`（`src/airui/`）为自研 owner-draw 自绘 UI 库。

## 构建（Build）

**唯一支持的构建方式：CMake + Visual Studio 2017（MSVC）。**
不支持 MinGW 或 g++ 作为构建工具链；`g++` 仅用于平台无关核心的可选 CI 冒烟测试（见 `scripts/check-core-linux-smoke.sh`），**不是**官方构建方式。

### 前提条件
- **Visual Studio 2017**（15.9+，安装「使用 C++ 的桌面开发」工作负载，含 MSVC 19.16 与 Windows SDK）
- **CMake 3.15+**

> VS2017 在本机可能安装在非默认路径（如 `C:\2017\Community`）。只要 `vswhere` 能找到，CMake 会自动定位，无需手动设置路径。

### 一键构建（推荐）
在项目根目录执行：
```
build.bat
```
产物位于 `build-cmake\Release\`：
- `airecord.exe`      — UI 客户端
- `airecordd.exe`     — 后台服务
- `airui_demo.exe`    — AirUI 复用示例
- `airui_gallery.exe` — 组件画廊（本次新增 `RatingBar` / `IconButton` 等控件均可在此查看）

其他用法：
```
build.bat clean        # 清空 build-cmake 并重新配置
build.bat test         # 构建后运行 ctest
build.bat clean test   # 清空 + 构建 + 测试
```

### 手动构建（Windows）
```
cmake -S . -B build -G "Visual Studio 15 2017" -A x64
cmake --build build --config Release
```
> ⚠️ 完整应用**必须**用 `Visual Studio 15 2017`（即 VS2017）生成器。**不要**在 Windows 上使用 `MinGW Makefiles` 构建本项目。

### Linux（仅平台无关核心）
完整应用依赖 Win32 / GDI / DWM，只能在 Windows + MSVC 下编译；**Linux 下只能构建平台无关核心 `airui-core`**（静态库 + 核心冒烟测试）。统一走 **CMake + Unix Makefiles**：

```
./build.sh            # cmake -G 'Unix Makefiles' 构建 airui-core
./build.sh test       # 构建后运行核心冒烟测试
```
等价于：
```
cmake -S . -B build-linux -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --target airui-core
```
产物：`build-linux/libairui-core.a`；核心行为验证见 `scripts/check-core-linux-smoke.sh`。

### 可选：Skia 渲染后端（Windows，默认关闭）

`AIRUI_WITH_SKIA` 默认 **OFF**，默认构建不引用、不链接 Skia，官方产物仍由 **CMake + VS2017 + GDI** 生成。开启后新增第二个 `IPainter` 后端（`SkiaPainter` / `SkiaSurface`），通过 32bpp BGRA 光栅画布 `AlphaBlend` 合成到 HDC。

```
vcpkg install skia:x64-windows
cmake -S . -B build-skia -G "Visual Studio 17 2022" -A x64 -DAIRUI_WITH_SKIA=ON -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build-skia --config Release --target test_skia_backend
```

- 也可传 `-DAIRUI_SKIA_ROOT=<prefix>` 指向预编译 release；两者都找不到时配置阶段会明确报错，不会静默降级。
- **VS2017 下开启未验证**：vcpkg 的 `skia` 官方工具集是 VS2022+，故 Skia 路径推荐在 VS2022 环境或 CI 中使用；本地正式构建保持默认 OFF。
- 引入方式的决策、代码结构、验证矩阵与已知边界见 **`docs/skia-backend.md`**。
- 无需 SDK 也能跑的验证：`test_skia_types`（ctest，三平台）与 `scripts/check-skia-syntax.sh`（API 契约语法门禁）。

### 说明
- 平台相关代码位于 `src/airui/platform/win32`，依赖 Win32 / GDI / DWM，只能在 **Windows + MSVC** 下编译。
- **构建约定（硬性）**：Windows = CMake + VS2017；Linux = CMake + Unix Makefiles（仅核心）。详细规则与"禁止猜测工具链"见项目 skill `p-system-build`。
- Skia 后端已作为**可选第二后端**在 Windows 接入（默认关闭），见上节与 `docs/skia-backend.md`；Linux / macOS 应用本体与 Skia 版窗口壳仍为设计阶段，见 `docs/cross-platform.md`、`docs/ROADMAP.md`。
- 控件采用「类体系 + 注册表」设计，新增/继承控件的做法见 `docs/design-patterns.md`。
