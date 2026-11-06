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

### 手动构建
```
cmake -S . -B build -G "Visual Studio 15 2017" -A x64
cmake --build build --config Release
```
> ⚠️ 生成器**必须**是 `Visual Studio 15 2017`（即 VS2017）。**不要**使用 `MinGW Makefiles` 或 `Unix Makefiles` 来构建本项目。

### 说明
- 平台相关代码位于 `src/airui/platform/win32`，依赖 Win32 / GDI / DWM，只能在 **Windows + MSVC** 下编译。
- 跨平台（Skia / Linux / macOS）仅为设计草稿，见 `docs/cross-platform.md`，当前未实施。
- 控件采用「类体系 + 注册表」设计，新增/继承控件的做法见 `docs/design-patterns.md`。
