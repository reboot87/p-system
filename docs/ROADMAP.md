# p-system / AirUI 路线图与未实现清单

> 目的：把当前"已交付"与"未实现"的边界写清楚，避免把阶段路线图误当成遗漏的 bug。
> 最后更新：2026-11-07。基线：`phase0-baseline`（034bdc2），Phase 0 验收见 `phase0-signoff.md`。
>
> **当前版本快照**：`docs/v0.8-baseline.md`（tag `v0.8`，2026-11-09）记录单体态的
> 完整功能清单、进程模型、架构分层与设计契约；工具箱化后续路线见
> `docs/toolbox-plugin-plan.md`。本文以下 Phase 章节是 UI 框架层的阶段记录。

---

## 0. 当前已完成（Phase 0 交付态）

- **Windows 应用可运行**：`airecord.exe` / `airecordd.exe` 自绘 UI 正常，无标题栏外壳（标题栏/状态栏/侧栏）、主题（Light/Dark）、布局（Flex/Grid）、响应式、动画。
- **剪贴板监控模块已上线**：概览页卡片显示"运行中"，已记录条目。
- **AirUI 核心平台无关化**：`airui-core`（STATIC）可在 Linux 用 CMake + Unix Makefiles 编译并烟测（`scripts/check-core-linux-smoke.sh`），core 层零平台头（由 `scripts/check-core-clean.{ps1,sh}` 门禁保证）。
- **控件 HAL 定稿（Phase 0）**：`ButtonStyle` + `ControlState` + `PaintButton/PaintCheckbox/PaintRadio` 全部在 core，Win32 后端经 `CanvasPainter` 适配。
- **构建工具链固化**：
  - Windows：唯一官方 = CMake + Visual Studio 2017（`build.bat`）。
  - Linux：CMake + Unix Makefiles/Ninja，**仅构建 `airui-core`**（`build.sh`）。
  - 绝不使用 MinGW 构建完整项目（g++ 仅用于核心 CI 冒烟）。
- **测试**：`ctest -C Release` **17/17 通过**（含 `test_shell_hal`、平台无关金样 `test_paint_recorder`、Skia 值映射 `test_skia_types`）；`test_ui` 为端到端脚本，仍需手工跑。

---

## 1. 未实现清单（按层）

### A. 跨平台 / 后端层（Phase 1 三项已完成；Windows Skia 已 opt-in 接入）
| 项 | 状态 | 说明 / 证据 |
|---|---|---|
| Skia 绘制后端（Windows） | ✅ 已接入（**运行时动态选择**） | core `PainterRegistry` 运行时按 id 选 `IPainter`：`--paint-backend` / `AIRUI_PAINT_BACKEND` / `ShellOptions.paintBackend` / `SetPaintBackend()` 热切换；`airui-skia.dll` 稳定 C ABI 动态加载或静态 provider，不可用自动回退 gdi。`AIRUI_WITH_SKIA` 默认 OFF，只决定本次是否**构建** Skia 代码，不再是编译期选择。详见 `docs/skia-backend.md` |
| Skia 跨平台（Linux/macOS 统一绘制 + 窗口壳） | 未实施 | 目前只有 Windows 绘制后端；Skia 版 `IWindow`、Linux/macOS 应用本体仍是 Phase 2 |
| Linux / macOS 应用本体构建 | 不可行 | 完整 exe 依赖 Win32/GDI/DWM；目前仅 `airui-core` 可在 Linux 编 |
| `GdiPainter` / `Win32Window` 生产级 HAL | ✅ 已完成 (1) | 真圆角/虚线/alpha/FontRole 字体/裁剪栈；`SetNavBadge` 落库、`WM_GETMINMAXINFO` 强制最小尺寸、滚轮屏幕坐标转客户区 |
| `IWindow` 完整事件注入 | ✅ 已完成 (2) | 平台无关 `IWindow` 补齐 enter/leave/char/focus/blur/DPI 六个事件（默认 no-op，不破坏其他后端），`Win32Window::PumpEvent` 全量转发 |
| `NavBar` / `Caption` / `StatusBar` 迁入 ShellCore HAL | ✅ 已完成 (3) | 按 `ShellCore.h` 的设计契约「core owns state & layout, backend owns pixels」：`Shell` 删除重复状态字段、全部读写 `ShellCore`；后端把真实几何喂给 `SetShellGeometry()`，使平台无关命中与像素对齐；`ShellOptions.coreShell` 提供可选的 `GdiPainter + ShellCore::Paint` 渲染路径（默认关，富渲染零回归） |
| 平台无关 DrawCall 录制器（G5 金样基座） | ✅ 已完成 | 旧 `DrawCall.h`/`TestShell` 绑死 `<windows.h>`（`RECT/COLORREF/swprintf_s/HDC`），无法在 Linux/macOS 编译；新增 core 层 `PaintRecorder`，把 `IPainter` 调用录成可比较/可 ASCII 序列化的 `PaintOp`，三平台 CI 产出同一金样（`test_paint_recorder`）。纠正了设计稿 §2.2 的过时描述 |

### B. App 业务模块（产品功能）
| 模块 | 状态 | 证据 |
|---|---|---|
| 剪贴板监控 | ✅ 已上线 | 概览页"运行中" |
| 文件监控（FileWatchPlugin） | ⬜ 规划中 / 未启用 | `GalleryDraw.cpp:351-353`、`OverviewPage.cpp:112-115`、`App.cpp:101`、`ConfigPage.cpp:208` |
| 打印监控（PrintWatchPlugin） | ⬜ 规划中 / 未启用 | `GalleryDraw.cpp:354-356`、`OverviewPage.cpp:117-120`、`App.cpp:102`、`ConfigPage.cpp:209` |

### C. UI 框架层（Win32 原生保留，Phase 0 刻意未做）
| 项 | 状态 | 说明 / 证据 |
|---|---|---|
| TextBox / ListBox 行为重写 | 未做 | 仍用 Win32 原生 EDIT/LISTBOX（`phase0-signoff.md:62`） |
| `RECT` / `COLORREF` 全量替换 | 未做 | 仅 core 层禁平台类型，Win32 层保留（`phase0-signoff.md:64`） |

### D. 测试 / CI 缺口
| 项 | 状态 | 说明 / 证据 |
|---|---|---|
| `test_ui` 接入 ctest | 未接入 | 端到端需 `airecord.exe` + IPC，只能手工跑（`phase0-signoff.md:37`） |
| 跨后端 DrawCall 一致性断言 | ✅ 真 SDK 实跑通过 | `test_skia_backend` 实现「core 金样 vs Skia 后端实收调用序列」的 `Diff()` 断言 + 光栅像素校验，由 `skia-windows`（push/PR **自动触发**，带 vcpkg 源码编译产物缓存）实跑输出 `G5 parity + raster OK`；另有 `test_skia_plugin` 验证运行时 DLL 动态选择 + C ABI 光栅（同一 CI 作业）。**像素级 GDI vs Skia 截图 diff** 仍属计划项（`docs/cross-platform.md` §6） |

---

## 2. 建议的阶段路线

### Phase 1 —— Win32 HAL 化（保持 Windows 行为零回归）
1. ✅ `GdiPainter` / `Win32Window` 从 stub 升级为生产级 HAL 实现。
2. ✅ 完成 `IWindow` 完整事件注入（键盘 / 鼠标 / 焦点 / 缩放 / DPI）。
3. ✅ `NavBar` / `Caption` / `StatusBar` 迁入 ShellCore HAL（状态与布局层；像素仍由后端负责，符合设计契约）。
4. TextBox / ListBox 行为重写（去 Win32 原生依赖，改为自绘 + 平台适配）。
5. Win32 层 `RECT` / `COLORREF` 逐步替换为平台无关类型。
6. 把 `test_ui` 编排进 ctest（启动 `airecord.exe` + IPC 断言）。
7. 落地 App 业务模块：**文件监控**、**打印监控**插件。

### Phase 2 —— Skia 与跨平台
1. ✅ 引入 Skia 后端实现 `IPainter`（Windows，opt-in，默认 OFF）；⬜ `IFont` / `IWindow` 的 Skia 版与跨平台窗口壳仍待做。
2. 在 Linux / macOS 跑通 AirUI 核心 + 示例（`airui_demo` / `airui_gallery`）。
3. 实现跨后端 DrawCall 一致性断言（复用 core 的 `PaintRecorder` 金样 + `Diff()`，不再用 Win32 专属的 `TestShell`）。
4. p-system 业务层 Win32 部分（托盘、注册表、性能计数器）保持 Windows 专属，不跨平台。

### 后续产品形态 —— 工具箱化 / DLL 可插拔插件（2026-11-09 新增规划）

本节上面的 Phase 1/2 是 **UI 框架层**路线；产品形态另有独立规划：**把单体剪贴板工具演进为工具箱宿主 + DLL 工具插件**，
分 `v0.9 地基 → v1.0 工具箱 MVP → v1.1 可插拔 → v1.2 自定义绘制与服务侧采集插件 → v2.0 稳定化`。
完整设计（ABI 契约、两类插件的进程边界、清单驱动导航、卸载深度、版本验收判据、现有代码迁移映射、风险登记）见
**`docs/toolbox-plugin-plan.md`**。要点：

- 现有 `src/plugins/`（`IFeaturePlugin`）是**静态编译**模块，不是可插拔插件；真正可插拔的范本是 `airui-skia.dll`
  （`PainterRegistry` + `PaintProviders.cpp` + `SkiaPluginAbi.h` + `test_skia_plugin.cpp`），新机制照它的结构推广。
- **前台 / 后台加载由插件自己声明，不是宿主分类**：manifest 的 `contexts[]`（`ui` / `service` / 两者）
  与 `load.mode`（`eager` / `lazy` / `background`）是两个正交维度。两个宿主扫描同一份 `plugins/`，
  只加载声明包含自己上下文的插件；同一 DLL 可被前后台各加载一次，经 `notify_peer()` 联动。
  需要常驻采集的插件若只做在 UI 侧，关闭主窗口就停止记录，破坏 C/S 架构；纯前台工具也不该白占服务进程。
- §1 B 的 `FileWatchPlugin` / `PrintWatchPlugin` 仍遵守「业务模块最后做」，排在 **v1.2**，以
  `contexts: ["ui","service"]` 双上下文插件形式交付。
- 规划基线 `47b33b3`；本轮只出规划，未改任何产品代码。

---

## 3. 非目标（明确不做）
- 不追求相对 GDI 的启动/内存/渲染性能定量提升（目标是"跑起来 + 可维护"）。
- 不做 Web / WASM 移植。
- 不替换 p-system 业务层的 Win32 代码（那是 App 的事，AirUI 只管 UI）。

> 与本文件配套：构建约定见 `README.md` 与项目 skill `p-system-build`；控件设计模式见 `docs/design-patterns.md`；跨平台设计见 `docs/cross-platform.md`。

---

## 4. 进展日志

- **2026-11-07 — A 起步：GitHub Actions 跨平台核心编译**
  - 新增 `.github/workflows/ci.yml`：ubuntu / macos / windows 三平台均通过 CMake 构建 `airui-core`（Linux / macOS 用 Unix Makefiles，Windows 用 MSVC）。
  - 核心冒烟测试（编译 + 链接 + 运行）在 Linux / macOS 的 CI 中执行（`scripts/check-core-linux-smoke.sh`）。
  - `check-core-linux-smoke.sh` 改为对 clang（macOS 上 `g++` 实为 clang）不启用 `-Werror`，避免误伤 CI。
  - 注：GitHub 托管的 Windows runner 只有 VS2022、无 VS2017；Windows CI 作业是跨平台编译门禁，官方本地构建仍为 CMake + VS2017（`build.bat`）。
  - 下一步（A 的后续）：`GdiPainter` / `Win32Window` 从 stub 升级为生产级 HAL；`IWindow` 完整事件注入；`NavBar`/`Caption`/`StatusBar` 迁入 ShellCore HAL；随后启动 Skia 后端原型。

- **2026-11-07 — A 的 Phase 1 三项（1 / 2 / 3）完成**
  - **(1) HAL 生产化**：`GdiPainter.h` 从 stub 升为真圆角/虚线/alpha 混合/按 `FontRole` 选字体/可嵌套裁剪栈的 HDC 版 `IPainter`；`Win32Window.h` 补齐 `SetNavBadge` 真实存储、`WM_GETMINMAXINFO` 强制最小尺寸、`WM_MOUSEWHEEL` 屏幕→客户区坐标修正。
  - **(2) 完整事件注入**：`hal.h` 的 `IWindow` 新增 `OnMouseEnter/OnMouseLeave/OnChar/OnFocus/OnBlur/OnDpiChanged` 六个平台无关事件（默认 no-op，保证其他后端不被破坏）；`Win32Window::PumpEvent` 覆盖键鼠/焦点/DPI/系统命令/最小尺寸。
  - **(3) 迁入 ShellCore HAL**：`ShellCore.h/.cpp` 新增只读状态视图与 `SetShellGeometry()`（后端喂真实几何，命中不漂移）；Win32 `Shell` 持有 `ShellCore`、删掉 6 个重复状态字段，所有 setter 写 core、`DrawCaption/DrawStatusBar` 从 core 读；`ShellOptions.coreShell` 提供可选 `GdiPainter + ShellCore::Paint` 路径（默认关，Logo/三键/状态胶囊零回归）。
  - **验证**：VS2017 `cmake --build build --config Release` 全绿；新增平台无关 `test_shell_hal`（状态/布局/命中/IPainter 驱动），`ctest -C Release` **15/15 通过**；该测试已接入 GitHub Actions 的 ubuntu / macOS 作业。
  - **顺带修复**：`build.bat` 三处 cmd 批处理陷阱（UTF-8 中文 `rem`、生成器名未加引号、块内 echo 括号提前闭合），脚本现在能端到端跑 CMake + VS2017。
  - **边界**：像素级"富渲染整体委托给 core"**不属于** step 3 —— 设计契约明确是 core 拥有状态/布局、后端拥有像素；三键/Logo/键位提示等平台装饰保留在 Win32 层。Skia 后端与 Linux/macOS 应用本体仍是 Phase 2。

- **2026-11-07 — A 续：平台无关 DrawCall 金样（G5 基座）**
  - 用户定调「业务最后做」，本轮继续 A，不碰业务插件。
  - **发现**：设计稿 `cross-platform.md` §2.2 把 `DrawCall.h`/`TestShell` 称为平台无关资产，实际二者在 `platform/win32/`，`DrawCall.h` 含 `<windows.h>` + `RECT/COLORREF/swprintf_s`，`TestShell` 依赖 `HDC/HBITMAP/Canvas` —— 这就是 G5 一直停在「设计中」的根因。
  - **落地**：新增 core 层 `PaintRecorder.h/.cpp`，把 `IPainter` 调用录为 `PaintOp`（`Matches` 通配 / `SequenceEquals` 严格比较 / `FirstDiff` / `Diff` / ASCII `ToString`），统一取代各测试里的 ad-hoc 录制器；`test_paint_recorder.cpp` 覆盖控件录制、通配匹配、确定性金样、差异定位、全操作捕获、`ShellCore::Paint` 金样。
  - **CI**：`test_paint_recorder` 加入 CMake + ctest，并在 GitHub Actions 的 ubuntu/macOS 用 CMake 与裸 g++ 冒烟两条路径运行；`scripts/check-core-linux-smoke.sh` 增加该用例。
  - **验证**：g++ 冒烟 `-Wall -Wextra -Werror` PASSED；VS2017 编译 EXIT=0；`ctest -C Release` **16/16**。
  - **边界**：这是「确定性金样 + 比对机制」，**不是**已经完成双后端 diff；真实 Win GDI vs Skia 比对要等 Skia 后端接入。`docs/cross-platform.md` 顶部已加实施状态补记，纠正 §2.2 过时描述，并保留 §10 待评审的开放问题（Skia 引入方式、Linux 窗口壳等）。

- **2026-11-07 — A 续：Windows 接入 Skia 后端（§10 开放问题 1 定案）**
  - 用户定调「先做 1，使 Windows 接入 Skia」「业务最后做」；本机实测无 Skia SDK / vcpkg / depot_tools，且 vcpkg 的 `skia` 官方工具集为 VS2022+，与本地硬性 VS2017 冲突，因此定为 **opt-in 第二后端，默认 OFF，绝不进入默认构建图**。
  - **引入方式定案**：vcpkg `skia` port 首选（`find_package(unofficial-skia)` → `unofficial::skia::skia`），`AIRUI_SKIA_ROOT` 预编译 release 备选，不采用 git 子模块源码编译。两者都找不到时 configure 明确 `FATAL_ERROR`，不静默降级。完整决策见 `docs/skia-backend.md`。
  - **新增后端**：`src/airui/platform/skia/airui/` 下 `SkiaTypes`（零 SDK 依赖的 Color/字体/UTF-8/对齐映射）、`SkiaApi`（Skia 头唯一入口，隔离版本漂移）、`SkiaPainter`（IPainter→SkCanvas，13 个方法全实现）、`SkiaSurface`（`SkiaFrame` raster BGRA→DIB→`AlphaBlend` 上屏 + `CreateSkiaBackend` 工厂）。
  - **应用接线**：`CMakeLists.txt` 加 `AIRUI_WITH_SKIA`（默认 OFF）；开启时 `AIRUI_HAVE_SKIA` 让 `Shell.cpp` 在 `ShellOptions.coreShell + skiaChrome` 下把 `ShellCore` 外壳交给 Skia 绘制。两层默认都关，富渲染零回归。
  - **G5 落地**：`test_skia_backend` 用 `PaintRecorder` 金样 vs Skia 后端**实收** `IPainter` 序列做 `Diff()` 断言，并校验光栅像素；需 SDK，由 `skia-windows`（push/PR **自动触发**，VS2022 + vcpkg + 源码编译缓存）实跑。
  - **验证**：`test_skia_types` 加入 ctest（三平台可跑，g++ `-Werror` 通过）；`check-skia-syntax.sh` 用 API 契约 mock 对 5 个源文件 `-fsyntax-only` 全绿；`AIRUI_WITH_SKIA=ON` 无 SDK 时报错指引正确；以 `-DAIRUI_HAVE_SKIA=1` 单独编译 `airui-win32` 实测 VS2017 EXIT=0；默认构建 `ctest` **17/17**。
  - **边界**：仅 Windows 绘制后端；Skia 版 `IWindow`、Linux/macOS 应用本体、字体度量/CJK 回退校准、像素级 GDI vs Skia 截图 diff 仍未做。VS2017 + Skia 未验证，真 SDK 路径只在 VS2022 CI 上验证（本机补装 vcpkg 时内置下载器受代理 SSL 限制，未能在本地跑真 SDK）。

- **2026-11-07 — CI 自动触发 + 本机 VS2022 第二工具链**
  - 用户定调「自动触发 本机也有vs2022 可以使用2022编译」。`skia-windows` 去掉 `if: github.event_name == 'workflow_dispatch'`：**push / PR 到 `main` 自动跑真 SDK 验证**，`workflow_dispatch` 仅保留给事后重跑。
  - 核查 vcpkg `skia` port 是 `vcpkg_from_github` + GN/Ninja **源码编译**（不是下载预编译库），因此作业加 `VCPKG_BINARY_SOURCES` files 缓存 + `actions/cache`（`.vcpkg-archives` / `installed` / `downloads`）、独立并发组（不被顶层 `cancel-in-progress` 中断，保证缓存写入）、`timeout-minutes: 120`，保留 `continue-on-error` 不阻塞核心 CI。
  - `vswhere` 实测本机 VS2017（`C:\2017\Community`）与 **VS2022 Professional 17.14（`C:\2022`，MSVC 14.44）** 并存；**官方构建仍是 VS2017 + `build.bat`**，VS2022 只作第二工具链用于 Skia opt-in / 交叉验证（`build-vs2022/`，已 gitignore）。
  - **踩坑**：本机同时有 `HTTPS_PROXY`/`https_proxy`、`HTTP_PROXY`/`http_proxy`（仅大小写不同），MSBuild 启动 `CL.exe` 报 `MSB6001 ... System.ArgumentException: 已添加项 ... HTTPS_PROXY / https_proxy`，CMake 全新目录探测编译器失败（`No CMAKE_CXX_COMPILER`）。configure 与 build 都用 `env -u https_proxy -u http_proxy -u all_proxy -u no_proxy` 去掉小写重复项后全绿；只改当前进程，不动系统环境。
  - 本机补装 vcpkg 结论：`git clone` 成功、官方 `vcpkg.exe` 用 `curl --ssl-no-revoke` 手动取回且 `version` 正常，但 `vcpkg install skia:x64-windows` 的内置下载器报 `curl error 35 (SSL connect error)`（Schannel 吊销检查），显式 HTTP 代理亦无效 → 按「不好编译就交给 GitHub Actions」既定策略，真 SDK 一环由自动触发的 CI 承担。
  - **验证**：VS2022 `Visual Studio 17 2022` x64 全量构建 `BUILD=0`、`ctest -C Release` **17/17 `CTEST=0`**；`-DAIRUI_HAVE_SKIA=1` 单独编译 `airui-win32` 验证 Skia 分支接线；默认 VS2017 构建与 `build.bat` 不受影响（本轮无产品代码/CMake 变更）。

- **2026-11-07 — 修复 GitHub Actions 全部 Windows 作业 Configure 失败**
  - **现象**：连续 5 次 CI 全红，但 ubuntu / macOS 全绿；两个 Windows 作业都停在 Configure 步骤，报 `CMake Error at CMakeLists.txt:2 (project): Generator Visual Studio 17 2022 could not find any instance of Visual Studio.`
  - **根因（环境而非代码）**：GitHub 于 2026-06 把 `windows-latest` / `windows-2025` 迁到 `windows-2025-vs2026` 镜像，该镜像**只装 VS2026 18.x、无 VS2022**，且自带 CMake 最高 4.1.2；`Visual Studio 18 2026` 生成器要 CMake ≥ 4.2，因此既不能用 `-G "Visual Studio 17 2022"`，也无法直接切 VS18 生成器。官方给出的兼容做法就是把 `runs-on` 固定到 `windows-2022`。
  - **修复**：`windows` 与 `skia-windows` 两个作业统一固定 `runs-on: windows-2022`（仍带 VS2022，且与本机第二工具链 VS2022 Professional 一致）；新增 `Verify VS2022 toolchain` 步骤，用 `vswhere -all -products *` 断言镜像上确实存在 VS2022 并打印 CMake 版本，镜像再漂移时能一眼定位。本地官方构建仍为 CMake + VS2017（`build.bat`），不变。
  - **顺带修缓存**：原 `actions/cache@v4` 的隐式 post 保存**只在整个 job 成功时执行**，Skia 作业失败导致每次丢弃约 35 分钟的源码编译产物；且 `${{ env.VCPKG_ROOT }}/installed` 在 `with:` 表达式里取不到动态写入 `GITHUB_ENV` 的值（env context 只含 workflow 静态定义），路径实际为空前缀。改为 `actions/cache/restore@v4` + `actions/cache/save@v4`（install 成功后立即保存），路径改用 `steps.vcpkg.outputs.root` 的正斜杠绝对路径，缓存 key 带 `windows-2022` 标识。
  - **边界**：`skia-windows` 仍 `continue-on-error`，不阻塞核心门禁；真 SDK 首次在新镜像上跑通后，才谈得上按真实 Skia API 校准 `SkiaApi.h` / 双后端 diff。业务模块仍留最后。

- **2026-11-07 — 修复后 CI 复验结果 + 真 Skia SDK 首次编译校准**
  - `e7bfa74` 的新 run：ubuntu / macOS / **windows-VS2022 三个核心作业全部 success**，workflow 整体 success；`skia-windows` 首次跑完 vcpkg 源码编译（约 39 分钟）并 Configure 成功，`Save vcpkg Skia artifacts` 也成功，后续 push 可直接命中缓存。唯一失败步骤是 Build（非阻塞）。
  - **真 SDK 报错归纳**：① `SkRect.h`/`SkSamplingOptions.h`/`SkTArray.h` 的 MSVC `C2589` 语法错 → `SkiaSurface.h` 的 `<windows.h>` 带进 `min/max` 宏，污染 Skia 头的 `std::max/numeric_limits::max`；② `SkTypeface::MakeFromName` 已移除；③ `SkPath` 变不可变、`addRect` 移到 `SkPathBuilder`；④ `SkDashPathEffect::Make` 由 `(ptr,count,phase)` 改为 `(SkSpan<const SkScalar>, phase)`。
  - **修复**：`SkiaApi.h` 在 `_WIN32 && !AIRUI_SKIA_MOCK` 下定义 `NOMINMAX` + `#undef min/max`（只作用于后端 TU，不动旧 Win32 代码），并新增 `SkPathBuilder.h`/`SkSpan.h`；`SkiaPainter.cpp` 改用 `SkFont` 平台默认 typeface（只按 `FontRole` 设字号）、`SkPathBuilder::detach()`、`SkSpan` 版 dash；`SkiaApiMock.h` 同步成新契约，避免本地语法门禁验证一套已死 API；CI 加 `Record real Skia API signatures` 诊断步骤把真 SDK 签名写进日志。细节见 `docs/skia-backend.md` §4.1。
  - **验证**：本地 `check-skia-syntax.sh`（5 个源文件 `-fsyntax-only`）与 `check-core-clean.sh`（46 core 文件）全绿；缓存命中后的第一次复验仅 **1m43s**，宏污染与 3 处 API 差异全部消除，只剩真 SDK 第 5 处差异：`SkBitmap::allocPixels` 已改返回 `void`，原 `if (!bitmap.allocPixels(...))` 报 `C2171`。已改为调用后用 `bitmap.getPixels()` 判空，并把 mock 同步成 `void`；`Record real Skia API signatures` 诊断步骤也在日志里确认了 `SkPathBuilder::detach()` / `addRect()` 与 `SkDashPathEffect::Make(SkSpan, phase)` 的真实签名。Build + `test_skia_backend` 由下一次自动 CI 复验。
  - **边界**：字体族对齐（GDI 的 Segoe UI vs Skia 默认 typeface）、CJK 回退、像素级截图 diff 仍未做；业务模块仍留最后。

- **2026-11-07 — 最终复验：CI 四个作业全绿，真 SDK 双后端测试通过**
  - 提交 `a95d0db` 触发 run `42335700042`（缓存命中，1m58s）：`airui-core` ubuntu / macOS / windows-VS2022 全绿；`skia-windows` 的 Restore→Install→Verify→诊断→Configure→**Build**→**Run G5 dual-backend parity test** 全部 success，日志输出 `test_skia_backend: G5 parity + raster OK`。
  - 至此「Windows 的 Actions 都编译失败」彻底解决，根因是三层环境与契约问题、**均非业务代码错误**：① `windows-latest` 迁到 VS2026 镜像 → 固定 `windows-2022`；② `actions/cache` 只在 job 成功时保存 → 拆成 `restore`/`save` 并在 install 后立即保存；③ 真 SDK 的 5 处 API/宏不兼容 → 按真实签名适配 `SkiaApi.h` / `SkiaPainter.cpp` / `SkiaSurface.cpp`，并同步 `SkiaApiMock.h` 契约门禁。
  - 提交链：`e7bfa74`（CI 镜像/缓存）→ `082af38`（真 SDK API 适配 + 签名诊断步骤）→ `a95d0db`（`allocPixels` 返回 void）。本地 `check-skia-syntax.sh`、`check-core-clean.sh` 与 VS2017 默认构建不受影响。
  - 边界不变：`skia-windows` 仍 `continue-on-error`（隔离第三方 SDK 源码编译的偶发波动，不阻塞核心门禁）；字体族对齐、CJK 回退、像素级截图 diff、Skia 版 `IWindow` 未做；业务模块仍留最后。

- **2026-11-07 — 重构：`AIRUI_WITH_SKIA` 改为运行时动态选择（编译开关 → 运行时注册表）**
  - 用户定调「AIRUI_WITH_SKIA 应该配置为动态选择 skia 而不是编译选择」。原方案用 `#ifdef AIRUI_HAVE_SKIA` 把 `Shell.cpp` 的 Skia 分支在编译期剔除：想用 Skia 必须带 SDK 重编整个 app，官方 VS2017 产物永远用不上。
  - **core 层**：新增 `PaintBackend.h/.cpp`（`PaintTarget` / `IPainterProvider` / `PainterRegistry` / `ResolvePaintBackend`），Strategy + Registry 在运行时按 id 解析 `IPainter`，不可用自动回退 `gdi` 并写 `LastError()`；平台无关，48 个 core 文件仍 0 平台引用。
  - **Win32 层**：`PaintProviders.cpp` 注册 `gdi`（总是）与 `skia`；有 SDK 时注册静态 provider，无 SDK 时注册 `airui-skia.dll` 动态加载 provider（同一 id，`Register()` 按 id 替换）。`Shell.cpp` 绘制路径彻底去掉 `#ifdef`。
  - **插件 ABI**：`SkiaPluginAbi.h`（零 Skia 头 / 零 STL 的 C 函数指针 + ABI 版本）+ `SkiaPlugin.cpp` 导出 `airui_skia_abi/create/destroy/pixels/row_bytes`；对象在 DLL 内创建/销毁，所有权不跨 CRT；主程序 `PluginPainter` 转发 13 个 `IPainter` 方法。
  - **配置入口**：`--paint-backend=`（`main.cpp` → `App::SetPaintBackend` → `ShellOptions`）、`AIRUI_PAINT_BACKEND`、`ShellOptions.paintBackend`、`Shell::SetPaintBackend()` 热切换、`Shell::AvailablePaintBackends()`；旧 `skiaChrome` 保留为兼容别名。
  - **CMake**：`AIRUI_WITH_SKIA` 默认 OFF，语义改为「本次是否构建 Skia 代码」；ON 时新增 `airui-skia-plugin` SHARED target（输出 `airui-skia.dll`，导入库改名 `airui-skia-import.lib`，避免与静态 `airui-skia.lib` 冲突）。默认构建产出仍不引用 / 不链接 Skia。
  - **测试 / CI**：新增 `test_paint_backend`（三平台，运行时选择 / 热切换 / 回退）、`test_paint_runtime`（Windows 无 SDK，默认构建回退路径）、`test_skia_plugin`（真 SDK，DLL 动态加载 + C ABI + 光栅像素）；`check-core-linux-smoke.sh` 加注册表用例；CI `core-unix` / `windows` 跑新用例，`skia-windows` 构建 DLL 并跑 `test_skia_plugin`。
  - **验证**：VS2017 默认全量构建 `BUILD=0`、`ctest -C Release` **19/19**；Linux smoke（g++ `-Werror`）含 `test_paint_backend` 全绿；`check-core-clean.ps1` 48 文件 0 平台引用；`check-skia-syntax.sh` 全绿；本机用 VS2017 OBJECT-library 临时工程编译 `SkiaPlugin.cpp` / `PaintProviders.cpp`（含 `AIRUI_HAVE_SKIA=1` 分支）/ `test_skia_plugin.cpp` 全通过；`test_paint_runtime` 实跑输出 `skia backend not deployed (fallback path under test)`。
  - **边界**：当时的保守判断是插件跨 DLL 依赖同代 MSVC ABI（`IPainter` 虚方法带 `std::wstring`）；VS2017 官方产物通过 DLL 使用 Skia 的端到端截图未确认；业务模块仍留最后。（**此 ABI 结论后被本节末条的下载产物实测修正**：v141↔v143 跨代可用，真正的硬约束是位数一致。）
  - **CI 复验（run `42337859169`，2m20s）**：四个作业全部 `completed success`。`skia-windows` 的 `Build Skia backend, plugin DLL + tests` 产出 `build-skia/Release/airui-skia.dll`；`Run runtime Skia plugin selection test` 输出 `test_skia_plugin: runtime DLL backend + C ABI + raster OK`，`Run G5 dual-backend parity test` 仍输出 `G5 parity + raster OK`。插件路径**首跑即通过**，无需修正 ABI / 导出名；`core-unix` 与 `windows` 的新注册表用例也全绿。

- **2026-11-07 — 下载 GitHub Actions 产物到本机实测（跨工具链 / 位数 / 真 HDC）**
  - 用户要求「把 github actions 产物下载下来 进行测试」。此前 CI 只构建 DLL 并跑测试、**未上传任何 artifact**（`total_count: 0`），故先补 `73e0be8`：Skia 作业新增 `Stage downloadable plugin bundle` + `actions/upload-artifact@v4`，打包 `airui-skia.dll`、两个 CI 测试 exe、全套 vcpkg x64 运行时 DLL 与 `README.txt`；`.gitignore` 加 `artifact-staging/`。
  - run `42338618815` 四作业全绿，artifact `airui-skia-windows-x64` 共 37 文件约 53 MB，下载到 `build/ci-artifacts/`。本机另配 `build/vs2017-x64` 构建树（`cmake -A x64`），并用临时工程 `build/plugin-abi-check/`（已 gitignore）以 VS2017 x64 编译 `test_skia_plugin.cpp` 与真实 HDC harness。
  - **实测结论**：① CI 自带 VS2022 x64 测试全绿（同代基线）；② 官方 VS2017 **x86** 产物 + x64 DLL → 位数不匹配，优雅回退 gdi、不崩；③ VS2017 **x64** 产物 + VS2022 x64 DLL → 运行时选择、`std::wstring` 跨 DLL、C ABI 光栅像素全绿，**证明 MSVC v141↔v143 跨代 ABI 可用**（修正此前「必须同代」的保守说法）；④ 真实 `CreateDIBSection` HDC 经 `AlphaBlend` 合成出 `BGRA 00 00 DD FF`，验证 Skia 上屏链路。
  - **边界**：CI 此时只出 **x64** 插件（x86 GDI 全量包见下一节），本机官方 `build/` 是 **x86**，直接用需 x64 构建树或另出 x86 Skia artifact（`skia:x86-windows` 需约 35 分钟额外源码编译，未做）；主程序窗口壳 `coreShell` 默认关，`--paint-backend` 暂不改变主窗口绘制路径；GUI 级截图 diff 未做。业务模块仍留最后。

- **2026-11-07 — CI 改为整体编译并发布全部产物（提交 `55c4327`）**
  - 用户要求「整体编译下载包括所有产物」。此前 `windows` 只编 `airui-core` + 1 个测试，`skia-windows` 只编 4 个 target，artifact 只有插件 DLL。
  - **改造**：三个 Windows job 全部 `cmake --build ... --config Release`（ALL targets）+ 全量 `ctest -C Release`；新增 `windows-x86`（`Visual Studio 17 2022 -A Win32`）覆盖官方 x86 架构；`core-unix` 因项目结构仍只构建平台无关 target（非 Windows 上 Win32 app targets 不可编译）。每个 job 把全部 exe/dll/lib 上传为 artifact。
  - **run `42339972398`**：5 个 job 全绿、5 个 artifact —— `airui-skia-windows-x64`（65 文件 / 69 MB：26 exe + 34 DLL + 4 lib，含 `airui-skia.dll` 与全套 vcpkg 运行时）、`p-system-windows-x64-gdi`（27）、`p-system-windows-x86-gdi`（27）、`p-system-core-ubuntu-latest`（6）、`p-system-core-macos-latest`（6）。
  - **下载实测**：`gh run download 42339972398 -D build/ci-all`；PE 检出的架构正确（x64 / x86 / x64），下载后的 `test_paint_backend` / `test_paint_runtime` / `test_skia_plugin` / `test_skia_backend` 全通过，两个 `airecord.exe --hidden` 启动 5 秒无崩溃。本机官方 VS2017 全量 `BUILD=0` + `ctest` **19/19**。
  - **交付**：合并 5 个 CI 包 + 本机 `build/Release`（26 个 exe/lib）为 `build/p-system-all-artifacts.zip`（46 MB，含 `MANIFEST.txt`）；Skia 全量包另存 `build/ci-all/airui-skia-windows-x64-FULL.zip`（27 MB）。
  - **边界**：仍无 x86 Skia 插件（需 `skia:x86-windows` 约 35 分钟源码编译）；主程序 `coreShell` 默认关，`--paint-backend` 不改主窗口绘制路径；GUI 截图 diff 未做。业务模块仍留最后。

- **2026-11-07 — 打通「用 Skia 渲染」并给出可量化的 GDI/Skia 分辨方法**
  - 起因：核查发现仓库里**没有任何地方把 `ShellOptions.coreShell` 设为 true**，注册表的 `PainterRegistry::Begin()` 只在 `coreShell` 分支里被调用，因此此前 `airecord.exe --paint-backend=skia` 实际不改变主窗口绘制路径（README / artifact README 的示例有误导性）。
  - **入口打通**：新增 `--core-shell`（`main.cpp` → `App::SetCoreShell()` → `opt.coreShell`），必须 `--core-shell --paint-backend=skia` 才真正由 Skia 画外壳；`App::Run()` 在 `Shell::Create()` 后往 `client.log` 写 `[paint] coreShell=? requested=? resolved=? available=[...] skiaDll=? probe=?`，`probe` 真实开一帧以暴露回退。
  - **像素对比工具**：新增 `tests/test_paint_compare.cpp`（Windows，接 ctest）——同一 240×120 32bpp DIB、同一 `IPainter` 调用序列，分别由 gdi / skia 绘制，读回 BGRA 后按图元分区输出差异像素数、平均通道差、光栅 checksum 与圆角中间色像素数；无 DLL 时只出 GDI 基线并跳过（默认构建仍可跑）。
  - **本机实测**（VS2017 x64 产物 + 已下载的 CI VS2022 x64 插件包）：`ctest -C Release` **20/20**；圆角中间色像素 GDI **0** vs Skia **69**（抗锯齿）；整帧差异 1274/28800（4.4%）、半透明矩形 **0** 差异、虚线 400/6912（5.8%）、文字 415/2592（**16.0%**、平均差 44.1，字体族未对齐所致）。四组启动日志实测：`coreShell=0`（旧版 GDI 路径）、`coreShell=1 + skia`（`probe=ok`）、`coreShell=1 + gdi`、以及 `AIRUI_SKIA_DLL` 指向不存在文件时 `available=[gdi] ... fell back to gdi`。
  - **边界**：仍是合成场景的像素 diff，不是完整 GUI 窗口截图比对；字体族/hinting 未对齐；x86 Skia 插件仍未做。业务模块仍留最后。

- **2026-11-08 — 修掉「鼠标从左侧列表移到右侧列表时整列表闪一下」**
  - 现象：指针从侧栏导航移进/移出剪贴板列表，右侧列表整块闪一帧（列表被擦成底色再逐行画回来）。
  - **定位方式**：新增 `AIRUI_HOVER_TRACE=1` 追踪（`%LOCALAPPDATA%\AireRecord\list_hover.log`，记录 LISTBOX 的 `move / leave / paint update=<invalid rect> / erase / draw idx=...`），把"肉眼看到闪"变成消息序列。修复前 `leave` 之后必跟 `paint update=0,0-549,634` + `erase` + 11 行重画（约 12ms）；修复后只有 `paint update=0,286-549,350` 一行。
  - **根因**：hover 是我们自己在子类化里维护的，但 `WM_MOUSELEAVE` 处理完直接 `break` 下传，标准 LISTBOX 自己的 leave 处理会整客户区失效一次；owner-draw 列表于是"擦整块 → 逐行画回"，就是那一下闪。`WS_EX_COMPOSITED` **不是**原因（A/B 保留合成标志后依旧只有单行失效）。
  - **修复**：① `WM_MOUSELEAVE` 吃掉消息（`return 0`），只失效原悬停行；② `Shell::WM_MOUSEMOVE` / `WM_MOUSELEAVE` 拆成 caption / nav / content 三块失效，只有外壳 chrome 变化时不再整窗失效（顺带减少无谓重绘，让子控件完全不被父窗口重绘牵连）。
  - **验证**：本地 VS2017 `ctest -C Release` 21/21；实测日志「导航悬停 0 字节（列表完全不重绘）」「跨越边界只重绘一行」；界面截图核对 hover / 选中 / 详情面板无回归。

- **2026-11-08 — 修掉「Skia 下点击筛选胶囊整窗闪一下」**
  - 现象：`--paint-backend=skia` 时点「全部/文本/链接/手动」，整个窗口闪一帧。
  - **定位**（`AIRUI_PAINT_TRACE=1` → `shell_paint.log`）：点胶囊是 `[paint-req] update=0,0-1120,780` → `[erase]` → `[paint] ms=31~47`。即「先整窗擦成背景色，再花 31~47ms 出帧」，擦底那一帧跨 2~3 次呈现，肉眼就是整窗闪；GDI 帧 2~5ms 所以看不出来。
  - **根因**：`Paint()` 本来就把整个客户区画进后备缓冲再 BitBlt，擦底永远不修正任何东西，只是把纯背景推上屏幕。此外 `Shell::SetStatus` 每次列表刷新都整窗失效（文案没变也一样）。
  - **修复**：`WM_ERASEBKGND` 只在尺寸/显隐变化后真的擦（`m_eraseAllowed`，WM_SIZE / WM_SHOWWINDOW 置位，擦一次或出帧一次即清）；`SetStatus` 文案未变直接返回；`uiDebugLog` 改为逐条开关文件以便运行中实时读日志，`[erase]` 增记 `allowed` 与失效矩形，新增 `[paint-req]`。
  - **验证**：`ctest -C Release` 21/21；窗口态与最大化态点四个胶囊都是「内容区失效 + allowed=0 + 单帧 31ms」；最大化仍保留一次 `allowed=1` 的背景擦除（DWM 黑帧保护）。

- **2026-11-08 — 修掉「点筛选胶囊时文本框和列表一起闪」**
  - 现象：上一版把整窗擦底关掉后，点「全部」仍然能看到**搜索框和列表同时闪**，且只在 Skia 后端显眼。
  - **关键实验**：从外部对主窗口做一次 `InvalidateRect + UpdateWindow`（不碰任何业务代码），子控件追踪日志里 LISTBOX 和 4 个 EDIT 全部跟着 `WM_PAINT / WM_NCPAINT / WM_ERASEBKGND` —— 说明父窗口每出一帧，就把所有子控件重画一遍。
  - **根因（坑）**：`CreateWindowExW(WS_CLIPCHILDREN, ...)` 把 `WS_CLIPCHILDREN` 传成了 **dwExStyle**。它与 `WS_EX_COMPOSITED` 同为 `0x02000000`，只是分属 style / ex-style 两个字段，所以窗口实际拿到的是「合成窗口」，**从来没有裁剪过子窗口**。Skia 一帧 30~47ms，父窗口每帧都连子控件一起重画 → 文本框 + 列表一起闪。
  - **次因**：LISTBOX 自己的 `WM_ERASEBKGND` 擦整片客户区再逐行回调 `WM_DRAWITEM`（16 行 ≈ 20ms 空白底）；它身上的 `WS_EX_COMPOSITED` 对叶子窗口无效（那是「本窗口的下级子树双缓冲」）。
  - **附带发现**：父窗口不再合成后，LISTBOX 带 `WS_EX_COMPOSITED` 创建会直接失败（`CreateWindowEx` 返回 NULL、`GetLastError=0`），整块列表消失 —— 这个扩展样式必须去掉。
  - **修复**：① `WS_CLIPCHILDREN` 放进 `dwStyle`（ex-style 传 0）；② LISTBOX 只擦「最后一行之后」那条带（条目卡片自己铺满自己的 rect），去掉 `WS_EX_COMPOSITED`；③ `Widget::Move` 几何未变时 no-op（reLayout 不再让所有子控件失效）；④ 保留上一轮的 `m_eraseAllowed` 擦底门控。
  - **验证**：强制父窗口重绘 → 子控件消息 **0** 条；点「文本」→「全部」，列表 `erase skipped items=20`（无整片擦除）、搜索框 **0** 条消息（不再重绘）；`ctest -C Release` **21/21**；截图核对列表/搜索框/详情面板渲染正常。

- **2026-11-08 — 修掉「最大化后点筛选胶囊，列表整块闪」**（补上次没修净）
  - 现象：上次修完窗口样式后，最大化下点「全部/文本/链接/手动」仍然整块闪，连点更明显。
  - **取证**：`AIRUI_HOVER_TRACE=1` 抓 LISTBOX 的消息序列，重建那一刻是
    `erase band 0,0-1366,894 items=0` → 整块列表先被擦成底色 → 4ms 后才是
    `erase skipped items=16` → 再逐行 `draw idx=0..15`。也就是说屏幕上先出现
    一张空白灰底，再被一行一行覆盖。
  - **根因**：`LB_RESETCONTENT` / `LB_ADDSTRING` 各自会带一次同步刷新，重建过程
    中间就被呈现了一帧——此时条目数还是 0，「只擦最后一行之后」的带等于整个
    客户区。GDI 一次 2~5ms 看不出来，Skia 一帧 31~35ms（实测最大化 1920×1040
    外壳 31~35ms，列表 20 行 5ms），这段中间态能占到好几帧，所以只有 Skia 明显。
  - **次因**：点已选中的胶囊也会整表重建（白闪一次），连点就是连闪。
  - **另一处**：`WM_ERASEBKGND` 用整客户区填底色。最大化时 WM_SIZE 的帧长达
    125ms，整窗填底色等于先推一屏纯背景上去。
  - **修法**：① `ListBox::BeginUpdate/EndUpdate`（`WM_SETREDRAW` + 引用计数），
    重建整段不呈现，结束时只提交一帧；② 重建期间 `WM_ERASEBKGND` 直接不擦；
    ③ 胶囊点击对「已选中」早退，完全不重画；④ 外壳擦底改成只擦这次**真正新
    暴露**的右边/下边条（`m_eraseRects`），不再整窗填底色。
  - **验证**：日志里 `items=0` 的整块擦除 0 条；连点 40 次，帧时长稳定
    15~32ms 无恶化、句柄 169→169（无泄漏）；逐帧采屏（16.7ms/帧，与 vsync 同步）
    亮度 245~247、墨量 0.017~0.026 没有掉到 0 的空白帧；`ctest -C Release` 21/21。
  - **判据补充**：原生 owner-draw 列表「重建即闪」要先看重建过程中有没有被呈现
    （`WM_SETREDRAW`），而不是先怀疑双缓冲。

- **2026-11-08 — Skia 一帧 35 ms 的分解与优化（提交 573d8db）**
  - **先换计时器**：原来只有 `GetTickCount64`，分辨率是系统 timer tick（15.6 ms），
    报出的"31~35 ms"其实是同一个桶里的两次测量。改 QPC 后一帧实测 **34.9 ms**：
    `dc 3.07 / begin 2.26 / draw 23.01 / present 5.59 / flush 0.94`。同尺寸 GDI 后端
    `draw` 只有 **4.48 ms**。
  - **draw 的重心是文字**：按原语统计，`text` 71.4 段/帧、285 us/段、合计 **20.39 ms**，
    占 draw 的 89%（`fill` 1.44、`fillRound` 1.23、`stroke` 0.17）。平均 4.7 字符/段，
    即 **58.9 us/字符**。
  - **根因不是光栅，是每段之前的重复准备**：`SkiaPainter::Text` 一段要测 2~4 遍
    （ellipsis 判断一次、ellipsis 逐字符回退每步重测一次、`totalW` 一次、`DrawRuns` 内
    再测一次），每次测量都对每个码点调 `FaceFor -> unicharToGlyph`（cmap 查找），
    且 `SkFont f = primary; f.setTypeface(face)` 会丢掉 SkFont 的 strike spec，下一次
    measure/draw 要重建描述符并重新查表。带 ellipsis 的 18.6 段/帧单价 463 us，
    不带的是 223 us。
  - **上屏重复搬运**：Skia 自己分配位图 → 画 → present 时 memcpy 到 DIB → AlphaBlend。
    1920x1040 就是每帧 8 MB 拷贝 + 8 MB 混合，而帧是 opaque 的、混合不改变任何像素。
  - **修法**：① `cp -> face` 按 role 缓存（上限 4096 防无界增长）；② 每个
    (role, face 种类) 预建 SkFont，测量与绘制都取引用；③ 构造时先建 DIB，让
    surface 直接画进 DIB 像素（WrapPixels），present 不再拷贝；④ opaque 时用 BitBlt。
    离屏帧与 DIB 创建失败仍走 Skia 位图 + 拷贝回退，`AIRUI_SKIA_ALPHA=premul` 对照构建
    保留 AlphaBlend。
  - **实测（1920x1040 / Skia / VS2022 产物）**：一帧 **34.9 → 21.3 ms**，
    `draw 23.0 → 12.2`、`present 5.6 → 2.1`、`text 285 → ~145 us/段`
    （`us/char` 58.9 → 31.7）。截图核对列表/文字/详情面板正常，还原-最大化后正常，
    `ctest -C Release` 21/21。
  - **判据**：Skia 文字慢先看"每段测了几遍"，再看 `SkFont` 有没有被每次拷贝/改 typeface；
    一帧 `present` 接近 `draw` 的一半时，看是不是拷贝 + 混合各走了一遍全屏。

- **2026-11-08 — Skia 一帧 21.3 ms → 6.0 ms：文字只排版一次，帧缓冲不再每帧新建（提交 0397f59 / f320afc / 83d5083）**
  - 上一轮把一帧从 34.9 ms 压到 21.3 ms 后，剩下的钱花在三处，全部是"重复准备"而不是"画"。
  - **文字（draw 的 75%：9.05 ms / 69 段 / 313 字）**：一段标签每帧要测 2~4 遍
    （ellipsis 判断、ellipsis 逐字符回退、totalW、DrawRuns 内各一次），每遍历码点都走
    `unicharToGlyph` 的 cmap，且每段都要 UTF-16→UTF-8→码点→逐 run 再编码回去。
    改成按 `(role, 文本)` 缓存一份 `SkTextBlob` + 逐码点累计宽度表 `cum`：
    测量退化为读字段，ellipsis 退化为扫 `cum`（原来 O(n²)），绘制退化为一次
    `drawTextBlob`（字形蒙版在 Skia 的 strike cache 里跨帧有效）。缓存是**进程级**的，
    因为 `SkiaFrame` 每帧重建，放成员里等于没有。DPI 变化会让校准 em 变，所以缓存
    带 `dpi` 版本号。
  - **帧缓冲（dc 3.2 ms + begin 2.2 ms = 25%）**：每帧新建内存 DC + 8 MB DIB
    （GDI 会清零）+ 全屏 `clear`，而这些像素随后被 `Shell::Paint` 的整窗填充全部覆盖。
    改成按尺寸保留 3 个热帧（DIB / surface / painter 都在），复用帧只
    `restoreToCount(1)` 重置 clip、跳过清屏。插件 C ABI 的 `create/destroy` 语义不变
    （destroy = 合成 + 归档），静态 provider 用一个转发包装把帧还给池。
  - **后备位图（dc 的另一半 + flush 1.0 ms）**：`PaintDC` 自己还有一张 1920×1040 的
    DDB——构造时分配并填满 8 MB，出帧时 BitBlt 8 MB 进来，`Flush()` 再 BitBlt 8 MB
    上屏。而 `frameActive` 之后所有 `gfx::*` 都被路由到 Skia，这张位图的像素**从来没被
    读过**。给 `PaintDC` 加 `useBackBuffer`，Skia 路径直接用 `BeginPaint` 的 DC，
    GDI 路径保持原样（GDI 是直接往 DC 上画，去掉会闪）。
  - **实测（1920×1040 / Skia / CI 的 VS2022 x64 产物）**：一帧 **21.3 → 6.0 ms**
    （`dc 3.24→0.03`、`begin 2.20→0.00`、`draw 12.16→4.84`、`present 2.12→0.83`、
    `flush 1.05→0.00`）；还原态（938×598）**2.0~2.7 ms**。
    文字 **145 → 5.3 us/段**、**31.7 → 0.87 us/字符**（27×/36×），`text` 9.05 → 0.39 ms。
  - **剩下的 4.8 ms draw 去了哪**：`fill` 2.34（其中整窗 8 MB 填充约 1.2~1.5）、
    `fillRound` 1.68（25 个圆角卡片的解析抗锯齿，换成 kAlias 会退回 GDI 的方块边，
    不换）、`text` 0.39、`stroke` 0.12。再加 `present` 的 8 MB BitBlt 约 1 ms，
    这是"整窗重绘 + 整窗上屏"这套架构下的下限。
  - **验证**：强制整窗重绘 40 帧逐帧采屏，亮度恒 247.6、墨量恒 0.0243（无空白帧、无中间态）；
    还原↔最大化各两次，同尺寸的亮度/墨量完全一致（无残影，说明跳过清屏是安全的）；
    CI 5 作业全绿（含 G5 双后端断言 + 全套 ctest）。
  - **踩到的坑**：① m148 的 `SkFont::getWidths` 只接受
    `(SkSpan<const SkGlyphID>, SkSpan<SkScalar>)`，没有 `(ptr, count, ptr)` 重载——
    mock 之前写的是旧形状，门禁放过了假契约，已同步改成 span；②
    `SkTextBlobBuilder::allocRunPosH` 返回的是指向 builder 内部的**引用**，
    要在下次 `alloc*` 之前把 glyph 和位置填完；③ `RunBuffer::pos` 是 `SkScalar*`
    （`allocRunPos` 时才当 `SkPoint*` 用）；④ `PrintWindow` 对"直接画到 BeginPaint DC"
    的窗口会返回上一帧，核对视觉必须用桌面 BitBlt（新增 `scripts/screen_shot.py`）。
  - **判据**：一帧里出现"每帧重建的固定大小缓冲"，先算它占多少 —— 8 MB DIB 建一次
    就是 3 ms 量级；一段文字每帧测好几遍，缓存布局比优化光栅划算得多。

- **2026-11-08 — 按钮交互审计：9 处可点元素里 8 处没有悬停，另有 2 个"点了没反应"的隐藏缺陷**
  - 起因是"有些按钮都没有 hover 效果"。逐个点名审计后，真正的分三类：
  - **① 页面自绘的按钮压根没有交互态**：筛选胶囊（全部/文本/链接/手动）、
    `＋ 手动录入`、表单卡「收起」、详情面板三键、备注虚线框、总览三张模块卡，
    全是 `FillRound + Text` 一把画死。`Canvas::Button` 本来就有
    `hover/pressed/disabled` 三个参数，但页面只传前三个（style）——默认 false。
  - **② 行内三个操作钮（复制/备注/删除）的命中矩形和点击坐标不在一个坐标系**：
    `ItemRect()` 给的是客户区坐标，而 LISTBOX 交上来的 `ptInItem` 是行内坐标，
    旧代码直接拿客户区的 `card.top + 15` 去比行内 y —— 除了列表最顶上那一行
    （top=0），每一行的图标都是错位的：点不动，也悬停不上。这次一并修掉。
  - **③ 指针从自绘按钮移进子窗口时，父窗口一条 WM_MOUSEMOVE 都收不到**：
    悬停状态会永久卡住（从胶囊直接移到列表，胶囊会一直亮着）。子窗口的
    `WM_SETCURSOR` 会转给父窗口，在那里统一清一次即可。
  - **④ 两个"看不见的悬停"**：`pal.hover` 是 #FAFAFA，铺在白色标题栏 /
    #FBFCFD 侧栏上等于没变 —— core-shell 的导航悬停和标题栏「渲染: …」按钮
    都是这个色，看起来就像"指上去毫无反应"。改用 `pal.active`(#F2F4F7)。
  - **修法（库层）**：
    - 新增 `core/include/airui/Interaction.h` 的 `HotState`：一个只记
      「指针命中谁 / 左键按在谁身上」的状态机，`Move/Down/Release/Leave`
      各自返回"要不要重绘"。语义就是桌面按钮那套：**按下只改视觉，动作落在
      抬起；拖出热区再松手 = 取消**。
    - `IPage` 新增 `OnMouseUp(pt)` 与 `HandCursorAt(pt)`；`Page::SetCapture(bool)`
      负责按住期间抓鼠标（拖到窗口外松手也能收到抬起）。
    - `Shell` 转发 `WM_LBUTTONUP`，并在 `WM_SETCURSOR` 里做两件事：指针在
      子控件上时清页面悬停；指针在自绘热区上时换成手型（只认
      `wParam == hwnd`，否则搜索框的 I-beam 会被顶掉）。
    - `ListBox::onHoverMove(index, ptInItem)`：LISTBOX 只在换行时重画整行，
      行内逐个图标的高亮必须靠这条回调自己失效。
    - `Button`(Secondary) 悬停从"只换底色"改成**底色 + 描边 + 文字**一起动
      （core 的 `widgets::Button::Paint` 与 win32 的 `DrawButton` 两条路径
      同步改，否则 GDI/Skia 立刻分叉）。
  - **修法（页面层）**：`ClipsPage` 用 `HotState` + `HitHot()`/`ActivateHot()`
    重写工具条、详情面板、备注框的点击路径；行内操作钮的几何抽成
    `OpRects(itemW, out[3])`（行内坐标），**绘制与命中共用同一个函数**，
    杜绝再次错位。
  - **实测（Win32 GDI，1120×780 客户区，逐点采色）**：
    - 胶囊：未选中 #FFFFFF → 悬停 #E6F4FF + 主色描边 + 主色文字；选中态
      #1677FF → 悬停 #4096FF；按下 #F2F4F7
    - 主按钮 / 复制：悬停 #4096FF，按下时胶囊变 #F2F4F7 并保持按下态直到松手
    - 次按钮「备注」：悬停底色 #F2F4F7、描边 #1677FF、文字 #1668DC
    - 删除：悬停 #FFF1F0 底 + 红描边；行内删除图标悬停 #FFF1F0、复制/备注 #E6F4FF
    - 备注虚线框：悬停底色 #FFFEF5 → #FFFFFF、描边转 #C6D4EC
    - 总览卡片：悬停出现 #C6D4EC 描边 + #D6E6FF 外环（规划中的两张卡不给高亮）
    - 按下后拖走松手：筛选不变（不触发）；正常点击：筛选切换成功
    - 行内「复制」点击后剪贴板 = 该行内容（证明命中修复生效）
    - 光标：胶囊/主按钮/详情三键 → 手型；搜索框仍是 I-beam；列表行保持箭头
  - **回归**：新增 `tests/test_hot_state.cpp`（8 条断言：悬停只报一次变化、
    拖出取消、拖回仍触发、跨热区不触发、离开清态），`ctest` **22/22**。
  - **判据**：凡是"画出来的按钮"，一定要同时回答三个问题——悬停/按下的样子是
    什么、动作落在按下还是抬起、命中矩形和事件坐标是不是同一个坐标系。
    第三条最容易漏，因为它在"顶部第一行"永远是好的。

- **2026-11-08 — 修：点「＋ 手动录入」后表单里的输入框显示的是列表旧像素**
  - **现象**：点右上角「＋ 手动录入」展开表单卡，多行录入框的位置显示的还是
    **刚才那些列表行**，备注框只剩一块灰底、保存按钮是个没有文字的灰块；
    点一下输入框（或最小化还原）才恢复正常。收起再展开、切页再回来同样复现。
  - **定位**：`ClipsPage::ToggleForm()` 的顺序是 `Relayout()` → `BatchDraw(false)`
    → `SyncVisibility()`（Show 子控件）→ `BatchDraw(true)`。批处理期间
    （`WM_SETREDRAW FALSE`）子控件被 Move/Show，它们挂起的那次重绘**丢了**：
    父窗口重绘标志为假时子窗口的 `WM_PAINT` 不落地，而 `WM_SETREDRAW TRUE`
    只让外壳自己重新变脏。外壳带 `WS_CLIPCHILDREN`，子控件矩形本来就排除在
    它的绘制区外 —— 于是那几块像素停在"控件搬家之前"的画面。子控件本身是好的：
    `GetWindowRect` 报的几何全对，`WindowFromPoint` 也命中的是 EDIT。
  - **修法（库层，一处）**：`Shell::SetRedraw(true)` 在 `WM_SETREDRAW TRUE`
    之后补一次 `RedrawWindow(m_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN)`
    —— 把所有后代一起标脏，把批处理吞掉的那次重绘补回来。所有走
    `Page::BatchDraw()` 的页面自动受益（目前只有剪贴板页的展开/切页两条路径）。
  - **实测（Win32 GDI，1120×780 客户区，桌面 BitBlt 逐像素比哈希）**：
    首次展开、收起后再展开、切页再回来三种路径，表单区截图的 md5 与"点击输入框
    强制重绘后"的正确渲染**逐字节一致**；收起后列表恢复满高、无卡影。
  - **判据（给下一类 bug）**：批处理（`WM_SETREDRAW`）夹住的不只是"少画两帧"，
    它可能把子控件的重绘请求一起吃掉。以后再往 `BatchDraw` 里塞 Move/Show，
    必须保证退出批处理时整窗 + 子树一起失效，否则屏幕上留下的是上一帧的像素，
    而且**父窗口永远补不上**（它压根画不到子控件那块）。

- **2026-11-08 — 补齐状态栏上写着却没实现的 F3/F4、自绘 Toast、自绘对话框**
  - **快捷键从来没注册过**：状态栏 / 侧栏 / 配置页都标着 `F3 搜索`、`F4 手动录入`，
    但全仓库只有服务进程注册了 `Ctrl+Shift+V`。更关键的是**不能挂在主窗口的
    WndProc**：按键消息投给当前焦点窗口，而焦点几乎总在 EDIT / LISTBOX 上，
    主窗口收不到 `WM_KEYDOWN`。修法是 `Shell::SetAcceleratorHandler()` 在
    `RunMessageLoop` 里、`TranslateMessage` 之前统一过一遍（库里不含业务语义），
    应用层只认「消息属于主窗口或其子窗口、没有 Ctrl/Alt/Shift、模态未打开」。
    F3 → 切剪贴板页 + 聚焦搜索框并全选；F4 → 切页 + 展开录入卡并聚焦；
    Alt+F4 / Ctrl+F4 不吞。
  - **ToastManager 以前是死代码**：`Paint(Canvas&)` 没有任何调用者，
    `Tick()` 也只在没人调的 `Shell::Tick()` 里。而且就算接进 `Shell::Paint()`
    也画不出来 —— 外壳带 `WS_CLIPCHILDREN`，右下角被列表 / 编辑框占着，
    父窗口的绘制区排除子控件；Skia 路径还会整窗合成覆盖。
    现改为独立 `WS_EX_LAYERED` 浮层：32bpp DIB 接收 core 的 `PaintToast`，
    洋红底逐像素补 alpha 后 `UpdateLayeredWindow` 上屏，自带 50ms 定时器做
    淡入淡出 / 自动消失 / 跟随主窗口平移，× 可点。位置特意抬高 52px，
    不遮住详情面板底部三个操作按钮。注入点：保存成功 / 失败 / 空内容、
    复制、删除、备注保存、清空、渲染后端切换。
  - **确认框全部改自绘**：`Shell::ShowMessageBox` 由 `MessageBoxW` 换成
    `RunModalDialog()`，复用 core 的 `PaintModal`（遮罩 + 卡片 + 主/次按钮 +
    焦点环），禁用 owner + 局部消息循环，支持 hover / 按下 / Enter / Esc / Tab。
    为单按钮场景给 `ModalState` 加了 `singleButton`（`MB_OK`），
    并为 hover/pressed 加了 `hovered` / `pressed`，默认态输出不变，
    原金样测试保持通过。    `App.cpp` 里「清空记录」的直接 `MessageBoxW` 也收编。
    （服务安装器当时仍用系统弹窗，已在下一节收尾走查中一并清掉。）
  - **实测**：F3 焦点落在 `Edit id=1001`、F4 落在 `Edit id=1003` 且表单卡展开
    （真实键盘与消息投递两条路径都通过）；Toast 卡片显示「已复制 / 已删除」、
    按钮区无遮挡、3.2s 后浮层自动隐藏；删除确认框的半透明遮罩让整窗变暗、
    Esc 取消后恢复 `(162,163,164) → (251,252,253)`、Enter 确认后出 Toast。
    `ctest -C Release` 22/22，`check-core-clean.sh` PASSED。
  - **判据**：UI 上写了快捷键就必须有注册点，且注册点要在**消息循环**而不是
    窗口过程；浮层提示 / 对话框不能画在带 `WS_CLIPCHILDREN` 的父窗口上，
    要用独立窗口；`UpdateLayeredWindow` 前必须逐像素补 alpha（GDI 只写颜色通道）。

- **2026-11-08 — 修：停在工作台总览页时复制，总数/最近动态不更新**
  - **现象**：在总览页复制内容，卡片总数、标题栏胶囊、侧栏角标、最近动态全部
    不动；必须点一下「剪贴板」页才一次性补上。修复前实测：复制后 1.6s，
    总览卡片区域截图 md5 与复制前**逐字节相同**；切页再回来才从 6 跳到 8。
  - **根因**：服务端 `NotifyClientRefresh()` 投递 `WM_APP+2`，App 起一个
    300ms 的 `TIMER_REFRESH_NOTIFY` 防抖；但 `Shell` 的 `WM_TIMER` 只分发给
    `CurrentPage()`，而全项目只有 `ClipsPage::OnTimer()` 认这个 id。
    停在总览 / 配置页时定时器空转，`m_entries`（最近动态）和 `m_stats`
    （总数）都不刷新。`ClipsPage::OnShow(false)` 还会 `KillTimer` 把挂起的
    刷新直接取消；切回剪贴板页时 `OnShow(true)` 主动拉数据，才造成
    「点一下就更新了」的假象。**把全局共享数据的刷新挂在了页面级定时器上**。
  - **修法**：新增 `App::RefreshData()` 作为唯一收口 —— 拉列表快照 +
    `STATS` + `SyncServiceUi()` + 整窗标脏；定时器由 App 的 message hook
    直接拦截，不再经页面分发。删掉 `ClipsPage::OnTimer` 和 `OnShow(false)`
    里的 `KillTimer`。把 `ClipsPage::RefreshList()` 拆成
    `ReloadEntries()`（只更新共享快照）/ `RefreshList()`（再灌 ListBox），
    这样非剪贴板页刷新时不必重建隐藏的列表控件。所有「数据变了」的路径
    （清空、删除、备注保存、手动录入、切回总览、服务管理页）统一走
    `RefreshData()`，纯 UI 操作（搜索、筛选、展开表单）仍只 `RefreshList()`。
  - **顺手修掉一个既有隐患**：共享快照必须拉**全量**。搜索框残留关键字时，
    旧路径会把过滤结果写进 `m_entries`，让总览页「最近动态」只剩几条、
    和 STATS 的总数对不上；现在非剪贴板页一律 `ReloadEntries()` 无过滤。
    手动录入保存以前只 `RefreshList()`，不刷统计，也在本次收口内。
  - **实测**：总览页复制 → 总数 9→10 且「最近动态」第一条即新内容（不切页）；
    配置页复制 → 胶囊 13→14；剪贴板页复制 → 列表首位即时出现新记录；
    复制后立刻切页 → 刷新不丢（22 条 + G 记录）；手动录入保存 → 表单收起、
    列表首位、Toast、胶囊/侧栏/信息卡/详情四处总数同帧同步；搜索框残留
    "p-system" 后回总览页复制不含关键字的内容 → 仍进「最近动态」。
    `ctest -C Release` 22/22，`check-core-clean.sh` PASSED。
  - **判据**：跨页面共享的数据（统计、快照）刷新必须挂在**应用层**，
    不能挂在「当前页面」的回调里；页面回调只负责自己的控件。
    自绘页面数据变了要显式标脏，没有子控件会替它 invalidate。

- **2026-11-08 — 收尾：原生 UI 清零、浮层边界、Toast 生命周期**
  - **原生 UI 残留**：`Shell::ShowPopupMenu` 还在用 `TrackPopupMenu`（系统菜单），
    `airecordd.exe` 的 `-install` / `-uninstall` / `RunAsService` 共 4 处
    `MessageBoxW`。后者在 Session 0 里弹窗会**阻塞服务线程且用户永远看不到**，
    不只是风格问题。现全部替换：
    - 右键菜单改走 `RunPopupMenu()`：复用 core 的 `PaintMenu / MenuItemAt /
      MenuContentHeight` + `OverlayWindow`，宽度用 `gfx::MeasureWidth` 与
      IPainter 同度量，锚点右下空间不足就翻转并 clamp 进显示器工作区；
      hover / 方向键 / Enter / Esc / 点击外部取消 / 手型光标闭环，
      `SetCapture` 保证点在菜单外不会漏给下面的控件。
    - 安装器结果通过 `WM_COPYDATA` 发给主窗口（`ipc::kUiWindowClass` +
      `kUiToastTag`），由 `App` 的 message hook 解析后走自绘 Toast；
      主窗口没开就只留日志和进程返回码。提升 → 普通完整性方向发
      `WM_COPYDATA` 是 UIPI 允许的。主界面进程内已无 `MessageBoxW` /
      `TrackPopupMenu`；托盘菜单属于系统通知区的会话代理进程（没有 airui
      链接），保持系统原生。顺手补了 UAC 被拒时的错误 Toast（以前静默）。
    - 所有 `Ipc().Request()` 现在检查 `OK` 前缀：复制 / 删除 / 备注 /
      清空 / 双击复制失败时给错误 Toast，不再「服务没连上也报已复制」；
      切换条目自动存备注只在成功时改本地缓存。
  - **浮层坐标与模态边界**：Toast 位置按 `MonitorFromWindow` 的工作区 clamp
    （跨屏 / 贴边 / 任务栏）；模态遮罩由客户区改为**整个窗口矩形**
    （`GetWindowRect` + 显示器矩形裁剪，顺带消掉最大化的隐形边框），
    命中与绘制统一用浮层自身尺寸。模态前后保存 / 恢复 owner 的
    `IsWindowEnabled`，窗口隐藏时不再硬抢前台；`ToastManager::Show()` 在
    owner 被禁用时直接丢弃，避免 Toast 浮到遮罩上。
  - **Toast 生命周期（本次最关键的坑）**：owner 最小化 / 隐藏时 Windows 会
    自动藏起 owned popup 但不通知我们，`m_visible` 缓存与真实可见性不同步，
    恢复后 `Show(true)` 被短路，Toast 再也不出现。改成以 `IsWindowVisible`
    为准；并引入**只在主窗口可见时推进的「显示时钟」** `m_visibleClock`：
    隐藏期间时钟冻结，恢复后无需逐条补偿 `startTime`。
    早期尝试过「按隐藏时长统一补 `startTime`」，在恢复动画里被重复触发，
    会出现负 `age`（淡入被无限推回 0，窗口可见但全透明）和无符号下溢
    导致 Toast 瞬间过期——这套补偿已废弃。
  - **实测**：右键菜单浮层 `(606,321)-(798,437)` 三项 + 分隔线 + hover 高亮，
    点击「复制内容」出「已复制」Toast；「删除记录」弹出遮罩覆盖整窗
    `(156,156)-(1276,936)`、`IsWindowEnabled(owner)` 0→1、期间 Toast 层隐藏；
    `WM_COPYDATA` 跨进程 Toast 标题/正文正确；最小化期间入队的 Toast
    在 3.6s 隐藏后仍存在、恢复 +0.3s 可见并正常渲染、+3.8s 到期消失。
    `ctest -C Release` 22/22，`check-core-clean.sh` PASSED（50 files）。
  - **判据**：任何浮层的显示状态都要以 Win32 真实状态为准，不能只信自己的
    bool；「用户实际看到的时间」应该是一根独立时钟，不要用墙钟减去补偿量
    反复改写起点；服务端进程永远不允许弹模态对话框。

- **2026-11-12 — GDI 重绘从「整窗重画」收敛到「只画脏区」（4 个提交）**
  - **起点数据**：900×650 画廊，全窗滚动帧 `dc 0.4~2.7 ms` + `draw 3.8~4.9 ms`（合计 5~8 ms）；单控件 hover 也把整条 caption/nav/status/content 重画一遍。
  - **① 只重绘 update region**（`4ea2b47`）：`PaintDC` 把 `BeginPaint` 的 `rcPaint` 记为 `m_update`，mem DC 裁剪到该矩形，`Flush()` 只 `BitBlt` 脏区；`Shell::PaintClip()` 把脏区交给页面（合成后端 Skia 恒为整窗，不受影响）。画廊 hover 只失效命中项，`Card::SetHover` 返回"是否真的变了"。
  - **② 后缓冲跨帧复用**（`17eeec5`）：新增 `PaintDC::BackBuffer`，每窗口一个常驻 mem DC + DDB，仅在尺寸变化时重建；新建时整块填一次底色，复用帧不再全区域填充。`dc` 段 0.4~2.7 ms → **0.06~0.10 ms**；全窗帧 5~8 ms → **3.3~4.5 ms**；1940×1100 约 7.9~9.1 ms。
  - **③ 失效矩形必须外扩**（`bac2338`）：控件会在自身矩形外多画一点（hover 底光 ±4px、分组标题分隔线 +8px），局部重绘会在边缘留下上一帧的像素。`markLocal` 统一 `InflateRect(8, 10)` 后，hover 扫过再强制全量重绘的截图**逐像素相同**（900×650，`DIFF pixels=0`）。
  - **④ 按脏区跳过整条带**（`7ddd06f`）：`Shell::Paint` 把脏区与 caption/nav/status/content 求交，不相交就整条跳过（含文本测量这类裁剪救不了的 CPU 工作）；`ShellCore::Paint` 增加可选 `clip` 参数，跳过不相交的导航行；画廊按外扩矩形跳过条目/卡片。导航行 hover **2.2 ms → 0.45~0.6 ms**，内容控件 hover → **0.33 ms**。
  - **判据**：① 局部重绘的正确性判据不是"看起来没残影"，而是「局部重绘后的截图 vs 强制全量重绘的截图逐像素相同」；② 外扩量必须同时写在失效侧和跳过侧，否则两边一个宽一个窄，残留会随机出现；③ `PaintClip` 只对 GDI 有意义——合成后端整窗重建，给它局部裁剪等于让它漏画。
  - **回归**：`ctest -C Release` **22/22**；浅/深色切换、放大再还原的 resize、长距离滚动均无残影。
