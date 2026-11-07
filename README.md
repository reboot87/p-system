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

### 交叉验证 / Skia 可选路径：本机 VS2022
`vswhere` 实测本机同时装有 VS2017（`C:\2017\Community`）与 **VS2022 Professional 17.14（`C:\2022`，MSVC 14.44）**。**官方产物仍由 `build.bat`（VS2017）生成**；VS2022 只用于 Skia 可选后端与交叉编译验证：

```bash
# Git Bash：必须去掉仅大小写不同的重复代理变量，否则 MSBuild 启动 CL.exe 会报 MSB6001
env -u https_proxy -u http_proxy -u all_proxy -u no_proxy cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
env -u https_proxy -u http_proxy -u all_proxy -u no_proxy cmake --build build-vs2022 --config Release
env -u https_proxy -u http_proxy -u all_proxy -u no_proxy ctest --test-dir build-vs2022 -C Release --output-on-failure
```

> 原因：环境里同时有 `HTTPS_PROXY` 与 `https_proxy`，MSBuild 构造 case-insensitive 环境变量字典时撞键，CMake 编译器探测会失败。只清理当前进程、保留大写代理即可，不需要改系统设置。

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

### 运行时绘制后端：GDI / Skia（动态选择，不是编译开关）

后端由 core 层 `PainterRegistry` 在**运行时**按 id 选择，默认 `gdi`；请求不可用时自动回退 gdi 并记录原因。应用绘制路径里没有 `#ifdef AIRUI_HAVE_SKIA`。

```
airecord.exe --paint-backend=skia              # 命令行（优先级最高，无需 --core-shell）
set AIRUI_PAINT_BACKEND=skia                  # 环境变量
标题栏「渲染: GDI」按钮                        # UI：点击循环切换（最省事）
ShellOptions.paintBackend = "skia";           // 代码：选后端
shell.SetPaintBackend("skia");                // 运行中热切换，下一帧生效
```

> **整窗切换**：`Shell::Paint()` 每帧打开一个 `PaintFrame`，`gfx::*` 与 `CanvasPainter` 的所有绘制原语都经它转发给当前后端，所以标题栏、侧栏、状态栏、**页面内容区**、core 控件一起换引擎，而不是另开一套简化界面。GDI 帧不激活路由，绘制代码原样执行，像素与旧版完全一致（`test_paint_frame.exe` 固化了这条契约）。`--core-shell` 只切换外壳实现（简化 HAL 外壳），与渲染后端无关；切换按钮也不会改动它。
>
> **文字层**：当前 CI 插件用 `drawSimpleText` + 默认 typeface，画不出 CJK / MDL2 图标字形。运行时探测（`SkiaPluginSupportsWideText()`，离屏帧画一个「中」看有无像素）会据此选择策略：`text=host` 时几何/填充/圆角/描边/合成全部由 Skia 画，文字在帧合成后由宿主 GDI 栅格化；插件将来支持 CJK 时自动变成 `text=backend`（纯 Skia 文字，无补画）。可用 `AIRUI_SKIA_TEXT=host|backend` 强制覆盖。
>
> **UI 切换按钮**：标题栏右侧（三键左边）常驻「渲染: GDI / 渲染: Skia」按钮，点击在**可用**后端间循环，下一帧生效，可来回切；不改外壳、不换布局。插件缺失时显示「无插件 / 已回退」且不会改动界面。每次切换向 `client.log` 追加 `[paint] UI switch -> ...`。
>
> **插件部署**：默认 VS2017 产物无需 SDK，把 `airui-skia.dll` 和它的全套依赖 DLL 放到 exe 同目录即可；也可用 `AIRUI_SKIA_DLL` 指绝对路径，但必须同时把插件目录加入 `PATH`（`LoadLibraryW` 带完整路径时不会自动搜索该 DLL 所在目录）。exe 与插件**位数必须一致**：官方 `build/` 是 x86，CI 插件是 x64，请用 `build/vs2017-x64`。

启动时 `client.log` 会写一行 `[paint] wholeWindow=1 coreShell=? requested=? resolved=? available=[...] skiaDll=? probe=? text=?`，可直接确认**实际生效的后端**、是否回退、以及文字由谁栅格化；`probe` 会真实开一帧，DLL 缺失/ABI 不符时会显示 `fell back to gdi`。`Shell::AvailablePaintBackends()` 也列出当前可用后端；`auto` 取注册表默认（gdi）。

**怎么分辨与 GDI 的差别**：`test_paint_compare.exe` 让两个后端在同一块 32bpp DIB 上接收**完全相同**的 `IPainter` 调用序列，读回像素并按图元统计差异：

```
.\build\Release\test_paint_compare.exe       # 默认构建：GDI 基线（无 DLL 时跳过对比）
```
把 exe 放到含 `airui-skia.dll` 与全套 vcpkg 运行时 DLL 的目录再跑，即输出圆角抗锯齿、虚线、文字等各区域的差异像素数与平均色差。实测数据与判读方法见 `docs/skia-backend.md` §5.3。

Skia 有两种来源，注册同一个 id `skia`：
- **插件 DLL（推荐）**：把 `airui-skia.dll` 放到 exe 同目录即可启用，官方 **CMake + VS2017 + GDI** 产物无需 SDK、无需重编；首次请求时动态加载，ABI 不匹配或缺失即回退 gdi。可用 `AIRUI_SKIA_DLL` 指定路径。
- **静态内置**：`-DAIRUI_WITH_SKIA=ON` 且有 SDK 时把 provider 链入进程。

`AIRUI_WITH_SKIA` 默认 **OFF**，**只决定本次构建是否产出 Skia 代码**（插件 DLL、静态 provider、真 SDK 测试），不决定运行时用哪个后端。构建插件：

```
vcpkg install skia:x64-windows
cmake -S . -B build-skia -G "Visual Studio 17 2022" -A x64 -DAIRUI_WITH_SKIA=ON -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build-skia --config Release --target airui-skia-plugin
cmake --build build-skia --config Release --target test_skia_plugin
```

- 也可传 `-DAIRUI_SKIA_ROOT=<prefix>` 指向预编译 release；两者都找不到时配置阶段明确报错，但**不影响默认 GDI 构建**。
- **VS2017 下编译 Skia 代码未验证**：vcpkg 的 `skia` 官方工具集是 VS2022+，插件推荐由 VS2022 / CI 产出；本地正式构建保持默认 OFF。本机实测装有 **VS2022 Professional 17.14（`C:\2022`）**；CI 的 `skia-windows` 作业在 push/PR 时自动触发，构建 DLL 并跑动态加载测试。
**CI 每次 push/PR 都做整体编译并发布全部产物**（artifact 保留 14 天）：

```
gh run download <run-id> -D build\ci-all
```

| artifact | 内容 |
|---|---|
| `airui-skia-windows-x64` | VS2022 x64 + Skia ON **全量**：app / gallery / 全部测试 exe、静态库、`airui-skia.dll`、全套 vcpkg 运行时 DLL |
| `p-system-windows-x64-gdi` | VS2022 x64 默认 GDI 全量（所有 exe + lib） |
| `p-system-windows-x86-gdi` | VS2022 Win32 x86 默认 GDI 全量（所有 exe + lib） |
| `p-system-core-ubuntu-latest` / `p-system-core-macos-latest` | 各平台 `libairui-core.a` + 4 个平台无关测试可执行 |

三个 Windows job 都跑全量 `ctest -C Release`；Linux/macOS 因项目结构只构建平台无关 target。插件包**必须整套**放到 exe 同目录，只拷 `airui-skia.dll` 会因缺依赖回退 gdi。CI 的 Skia 产物是 **x64**，`build.bat` 默认是 **x86**：位数不匹配会静默回退，x86 请直接用 `p-system-windows-x86-gdi`，或另配 VS2017 x64 构建树（`-A x64`）。跨工具链实测结果见 `docs/skia-backend.md` §5.2。

- 引入方式决策、C ABI 约束、验证矩阵与已知边界见 **`docs/skia-backend.md`**。
- 无需 SDK 也能跑的验证：`test_paint_backend` / `test_paint_runtime`（运行时选择与回退，ctest）、`test_skia_types`（三平台）与 `scripts/check-skia-syntax.sh`（API 契约语法门禁）。

### 说明
- 平台相关代码位于 `src/airui/platform/win32`，依赖 Win32 / GDI / DWM，只能在 **Windows + MSVC** 下编译。
- **构建约定（硬性）**：Windows = CMake + VS2017；Linux = CMake + Unix Makefiles（仅核心）。详细规则与"禁止猜测工具链"见项目 skill `p-system-build`。
- Skia 后端已在 Windows 接入，采用**运行时动态选择**（`airui-skia.dll` 插件 / 静态 provider），`AIRUI_WITH_SKIA` 只控制是否构建 Skia 代码，见上节与 `docs/skia-backend.md`；Linux / macOS 应用本体与 Skia 版窗口壳仍为设计阶段，见 `docs/cross-platform.md`、`docs/ROADMAP.md`。
- 控件采用「类体系 + 注册表」设计，新增/继承控件的做法见 `docs/design-patterns.md`。
