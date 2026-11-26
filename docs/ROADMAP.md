# p-system / AirUI 路线图与未实现清单

> 目的：把当前"已交付"与"未实现"的边界写清楚，避免把阶段路线图误当成遗漏的 bug。
> 最后更新：2026-11-16。基线：`phase0-baseline`（034bdc2），Phase 0 验收见 `phase0-signoff.md`。
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
  - Windows：本地推荐 = CMake + Visual Studio 2017（`build.bat`）；CI 与 Skia 路径为 VS2022。
  - Linux：CMake + Unix Makefiles/Ninja，**仅构建 `airui-core`**（`build.sh`）。
  - 绝不使用 MinGW 构建完整项目（g++ 仅用于核心 CI 冒烟）。
- **测试**：`ctest -C Release` **22/22 通过**（含 `test_shell_hal`、平台无关金样 `test_paint_recorder`、Skia 值映射 `test_skia_types`）；`test_ui` 为端到端脚本，仍需手工跑。

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
| TextBox / ListBox 行为重写 | ✅ 已完成 | 自绘（`AirUITextBox` / `AirUIListBox` 窗口类），不再依赖原生 EDIT/LISTBOX 行为；core 多行布局（`WrapText`/`LineOfIndex`/`IndexAtX`/`MoveVertically`/`ScrollToCaret`）+ IME/剪贴板/滚轮/键盘自实现；`WM_GETTEXT`/`LB_GETCOUNT` 等消息级探测保留（`test_ui` 按 `Edit`/`ListBox` 别名查找）。详见进度日志 2026-11-23 |
| `RECT` / `COLORREF` 全量替换 | 未做 | 仅 core 层禁平台类型，Win32 层保留（`phase0-signoff.md:64`） |

### D. 测试 / CI 缺口
| 项 | 状态 | 说明 / 证据 |
|---|---|---|
| `test_ui` 接入 ctest | 未接入 | 端到端需 `airecord.exe` + IPC，只能手工跑（`phase0-signoff.md:37`） |
| 跨后端 DrawCall 一致性断言 | ✅ 真 SDK 实跑通过 | `test_skia_backend` 实现「core 金样 vs Skia 后端实收调用序列」的 `Diff()` 断言 + 光栅像素校验，由 `skia-windows`（push/PR **自动触发**，带 vcpkg 源码编译产物缓存，依赖版本由 `vcpkg.json` manifest 锁定）实跑输出 `G5 parity + raster OK`；另有 `test_skia_plugin` 验证运行时 DLL 动态选择 + C ABI 光栅（同一 CI 作业）。**注意：`skia-windows` 是 `continue-on-error` 的非阻塞作业**——必需的合并门禁是 `core-unix` + `windows` / `windows-x86`；Skia 作业失败不阻断合并（vcpkg 源码编译第三方 SDK 可能偶发失败），故该行证据以「最近一次成功运行」为准。**像素级 GDI vs Skia 截图 diff** 仍属计划项（`docs/cross-platform.md` §6） |

### E. 代码内 TODO 检索（P3-5）

把散落在注释里的未完成工作统一成 `TODO(<领域>): ...` 标记，可按域名检索：
`git grep 'TODO('` 可一次列出全部已知欠账（项目自产约 8 处，**全部在 `docs/`**、
含本文自引计数，`src/` 已清零；另 `third_party/sqlite/sqlite3.c` 有 7 处上游自带标记，非本项目债务）。

- **`doc`**（`docs/toolbox-plugin-plan.md`，1 处）：工具箱化规划仍待评审/完善。
- 已还清：`layout`（`ILayoutItem::PreferredSize()` 虚函数，2026-11-21）、
  `theme`（`LastLoadedThemeName()`，2026-11-21）、`a11y` 的 `IAccessible` 主体
  （`WM_GETOBJECT` → `a11y::HandleGetObject*` → `AccessibleObject`，含
  name/role/state/value/description/default-action/命中测试，测试
  `test_accessibility` 走真实 `ObjectFromLresult` 往返，2026-11-21）、
  `a11y` 的 `WM_SETTINGCHANGE` 监听（隐藏 1×1 顶层窗口收系统广播 +
  即时播报，注意 message-only 窗口收不到广播）与键盘导航子类化
  （Tab/Shift+Tab 环形循环 + Enter→BM_CLICK，方向键/编辑键留给控件
  自身语义，2026-11-21）、`gallery` 命中测试/布局更新（命中测试早已走
  `HitTestNav`，本轮补上 `OnMouseLeave` 清悬停 + 提取 `InvalidateNavRow`，
  指针离开窗口时最后一行悬停不再卡住，2026-11-21）。
- CI（`core-unix`）已加 **TODO 计数步骤**（仅报告，不阻塞）。

---

## 2. 建议的阶段路线

### Phase 1 —— Win32 HAL 化（保持 Windows 行为零回归）
1. ✅ `GdiPainter` / `Win32Window` 从 stub 升级为生产级 HAL 实现。
2. ✅ 完成 `IWindow` 完整事件注入（键盘 / 鼠标 / 焦点 / 缩放 / DPI）。
3. ✅ `NavBar` / `Caption` / `StatusBar` 迁入 ShellCore HAL（状态与布局层；像素仍由后端负责，符合设计契约）。
4. ✅ TextBox / ListBox 行为重写（去 Win32 原生依赖，改为自绘 + 平台适配，2026-11-23）。
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

> 与本文件配套：构建约定见 `README.md` 与 `docs/build-conventions.md`；控件设计模式见 `docs/design-patterns.md`；跨平台设计见 `docs/cross-platform.md`。

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

- **2026-11-23 — TextBox / ListBox 自绘重写（去 Win32 原生 EDIT/LISTBOX 行为依赖，「两者一轮全量」）**
  - **背景**：这是 UI 框架层欠账的第三块（布局/主题早已还清）。此前 TextBox/ListBox 的**行为**（编辑、选区、IME、剪贴板、滚动、键盘）全靠原生 EDIT/LISTBOX，自绘只画像素。本轮把行为也收进库内，公共 API（`Widgets.h` 的 `TextBox`/`ListBox` 签名、`Native()`）保持不变。
  - **core 多行布局（平台无关、纯整数、可测）**：`FrameworkCore` 新增 `WrapText`（`\n` 硬断 + 空格/Tab 软断 + CJK 任意字间折行 + 超宽词内断，**代理对绝不切半**）、`LineOfIndex`（二分）、`IndexAtX`（按逐字累计宽度表 `cum[i]` 定位，比例字体；中点判据 + 代理对就近吸附）、`MoveVertically`（↑/↓ 保持水平像素位置）、`ScrollToCaret`（caret 行滚进视口，含"视口小于一行"的退化夹取）。单行的 `MeasureTextEdit` 是等宽模型，无法支撑比例字体的逐字定位，所以多行这套是**新写**而非复用。单测 `TestTextWrap`（`\n` 断行 / CJK 软断 / 超宽内断 / 代理对不切半 / `IndexAtX` 中点 / `MoveVertically` 水平保真 / `ScrollToCaret` 夹取）。
  - **TextBox 自绘**：窗口类 `AirUITextBox`（DefWindowProc 基类 + 子类化），单行直接复用 core `PaintTextBox`（外框/占位/选区/光标同一套设计），多行按 `WrapText` 结果逐行直排（外框/行内选区/逐行文本/组词串/插入符）。编辑走 core 引擎 `TextEditState` + `ApplyEdit`；IME 处理 `WM_IME_STARTCOMPOSITION / COMPOSITION(GCS_COMPSTR 预览 / GCS_RESULTSTR 上屏自插) / ENDCOMPOSITION`，候选窗跟随插入点（`ImmSetCompositionWindow` + `ImmSetCandidateWindow`）；剪贴板 Copy/Cut/Paste 经 `Win32Clipboard:IClipboard` 走核心 `EditAction`；键盘 Left/Right/Home/End/Up/Down(多行纵移)/Back/Delete/Enter(多行换行)/Ctrl+A/C/V 全部自实现，未消费键交默认（Tab 等）；多行 `WM_MOUSEWHEEL` 滚动。**消息级探测保留**：`WM_GETTEXT / WM_GETTEXTLENGTH / WM_SETTEXT / EM_SETSEL / EM_SETMARGINS` 仍在 proc 里应答，服务 `App::FocusSearch`（全选）、`ManualEntryPlugin`（`GetWindowTextStr`/`SetWindowTextW`）与 `test_ui` 的 `TypeText`。
  - **ListBox 自绘**：窗口类 `AirUIListBox`（`WS_VSCROLL` + DefWindowProc 画标准滚动条，范围由 `SetScrollInfo` 维护）。条目模型移入类内（`m_items`/`m_sel`/`m_scroll`），`Reset/AddItem/Count/Selection/SetSelection/ItemRect/ItemAtPoint/InvalidateItem` 全部自实现；`WM_PAINT` 单帧整块擦底 + 逐行回调 `onPaintItem`（**无**原生"擦整块→逐行画回"的中间态，此前的 `WM_ERASEBKGND` 擦带补丁与 `WM_SETREDRAW` 锁随原生 LISTBOX 一起消失）；选中/滚轮(`WM_MOUSEWHEEL`)/拖动滚动条(`WM_VSCROLL`)/键盘(↑↓/Home/End/PageUp/PageDown)/hover/双击/右键全部自实现，`LBN_SELCHANGE`/`LBN_DBLCLK` 经 `WM_COMMAND` 上报（页面回调链路不变）。**消息级探测保留**：`LB_GETCOUNT / LB_GETCURSEL / LB_SETCURSEL / LB_GETITEMRECT / LB_GET(SET)ITEM(DATA|HEIGHT) / LB_ADDSTRING / LB_RESETCONTENT / LB_ITEMFROMPOINT / LB_GET(LEN|TEXT)`，`test_ui` 的 `ListCount` 继续走 `LB_GETCOUNT`。
  - **清理**：退役 `PlaceholderEdit`（占位提示改由自绘 `PaintSelf` 直接画）与 `ForwardClickToEdit`（点击定位光标改由 `CaretAtClientPoint`），及其 `kPropPlaceholder`/`kPropComposing` 属性；`Controls.h` 顶部清单与注释同步更新。
  - **test_ui 适配**：`Edit`→`AirUITextBox`、`ListBox`→`AirUIListBox` 的别名匹配（`ClassMatches`），探测逻辑本身不变。
  - **实测**（非假设）：`cmake --build build --config Release` 全量 `BUILD=0`（airui-core / airui-win32 / 全部 app / gallery / 28 个测试 target）；`ctest -C Release` **28/28**（含新 `TestTextWrap`）；`check-core-clean.sh` PASSED（55 个 core 文件 0 平台引用）；`check-skia-syntax.sh` 全绿；`check-public-headers` 通过。运行 `airecord.exe`（新构建）跑 `test_ui`：`LB_GETCOUNT`=95、`WM_GETTEXT` 可达、`TypeText` 键入多行录入框→插件 `WM_GETTEXT` 读回→保存链路 PASS；滚动条经独立 C 工具（`GetScrollInfo`，95 行 × 5978px 内容 vs 634px 视口）确认存在且状态一致；全屏截图核对列表选中行/hover 图标/时间戳/详情面板与搜索框占位字均正常。
  - **边界**：① `test_ui` 的**保存后 IPC 校验段**（`LIST\t<kw>` → 落库 → `DEL`）在本机挂起——**【2026-11-24 跟进四已定位并修复：并非"命名管道既有环境行为"，而是 `test_ui` 旧 `IpcReq` 只发单行请求、与 `PipeServer` 两行 REQ/RSP 帧协议不匹配导致的双向死锁，详见跟进四】**（当时自绘相关的 `LB_GETCOUNT`/`WM_GETTEXT`/键入/保存各断言均先于该段 PASS）；该端到端脚本本就未接入 ctest（见 §D，只能手工跑）。② IME 候选窗定位按 caret 跟随，未覆盖每个 IME 的边角；③ Skia 后端已 CI 实测（run `45050666108`，2026-11-23 push 触发）：6 作业全绿（`skia-windows` 41m28s），`test_skia_backend: G5 parity + raster OK`、`test_skia_plugin: runtime DLL backend + C ABI + raster OK`，Skia 构建 ctest 30/30；自绘走同一 `Canvas`/`IPainter` 抽象，无后端特化。该 run 的 `airui-core (windows)` 作业里有一条 `makensis failed (rc=1)` 的红色 X——NSIS 安装器是工作流里显式标注 best-effort 的步骤，上一次绿 run（`44950715203`）同样出现、作业仍通过，属既有偶发，与本改动无关（见 `docs/build-conventions.md` 打包节）。业务模块（FileWatch/PrintWatch）仍留 v1.2。

- **2026-11-23（跟进）— 自绘 TextBox 光标三连修（不闪 / 位置不对 / 空框无光标）**
  - **起因**：用户反馈「自绘 textbox 显示不对：光标不会闪动、位置也不对」，要求独立 editbox 测试壳。
  - **新增 `airui_textbox_lab`**（`examples/TextBoxLab.cpp` + CMake 目标）：单窗口四框（单行 / 单行长横向滚动 / 多行短 / 多行 60 行）+ 底部闪烁相位探针（按系统 `GetCaretBlinkTime()` 半周期翻相位，与控件自身定时器同周期反相位），专供此项排查与回归。
  - **修 ①（位置，单行）**：原单行走 core `PaintTextBox`，光标/选区几何是**等宽模型**（`cursor × charW("0")`）——比例字体下光标随内容漂移且与 GDI 绘制不同源。改为与多行共用 `m_cum`（逐字 GDI 实测前缀宽度表），文本/选区/光标三者同源；横向滚动（caret 跟进出界自动偏移）一并自实现。core 的 `PaintTextBox`/`MeasureTextEdit` 保持不动（gallery 与单测仍在用）。
  - **修 ②（位置，多行）**：`CaretPoint()` 原返回含 `unit` 内边距的客户区坐标，`PaintSelf` 又加一次 `textRc`（已含 unit）→ 光标/组词串**整体右偏+下偏一个 unit**，且多行 y 未随 `m_scroll` 对齐绘制。`CaretPoint()` 改为返回文本区（textRc）相对坐标；`SyncIme` 负责 `+unit` 还原客户区坐标给 `ImmSet*Window`。
  - **修 ③（不闪）**：自绘窗口没有系统 caret，而实现里**没有任何闪烁机制**（原生 EDIT 的闪烁被自绘一起拿掉了）。新增 `RestartCaretBlink`/`StopCaretBlink`：`SetTimer` 周期 = `GetCaretBlinkTime()`（系统默认 530ms），聚焦/光标每次移动重置回「亮」相位（不闪半帧），失焦/销毁停表，`WM_SYSCOLORCHANGE` 重新对齐系统设置。
  - **修 ④（空框无光标，实测中发现）**：占位字分支 `PopClip+return` 提前退出，聚焦的空框**完全不画光标**（单行/多行都有）。两分支同构改为「占位字与文本互斥、光标统一画」。
  - **修 ⑤（窄框光标塌缩，实测中发现）**：单行框高 ≈ 一行文字高，`textRc` 上下各留 2px 后 caret 高度 <8px 甚至为 0。caret 高度不足 8px 时居中保底 8px。
  - **实测**（像素级，非肉眼）：`labtest.exe`（跨进程驱动：focus/type/key/rect/getraw）+ 定时截图 + 逐列蓝色像素分析。空框聚焦：修复前 6 帧连拍光标像素数恒为 0（只有边框）→ 修复后 20/24/20/24/20/24 交替（caret 列出现/消失，周期 = 系统 530ms）；键入 `abcde`/`abcdewxy`：caret 紧贴最后一字（`w`/`y` 宽字符使 caret 落在 x≈100，等宽模型会落在 ~70）；多行 `abc⏎def`：caret 在 `def` 行尾、随焦点闪烁。`ctest -C Release` 28/28；`check-core-clean`/`check-skia-syntax`/`check-public-headers` 全绿；`test_ui` 自绘段全 PASS（`LB_GETCOUNT`=95、保存往返 PASS；保存后 IPC 段挂起为既有环境问题，见上条边界①）。
  - **边界**：IME 组词串光标位置同用 `CaretPoint()` 已顺带修正；未做 GDI vs Skia 的双后端 caret 像素 diff（Skia 后端走同一 `FillRect` 原语，由 `skia-windows` CI 的 G5 parity 兜底）；业务模块仍留 v1.2。

- **2026-11-23（跟进二）— 修「搜索框两个边框 / 框里字的大小不对」（页面层原生 EDIT 时代残留）**
  - **现象**：Clips 页搜索框出现双层边框（外层灰框 + 内层焦点蓝框）；框内文字上下被裁、看起来字号不对。
  - **根因**（两处都是「原生 EDIT 时代」的页面层设计，自绘后失效）：① `ClipsPage::DrawToolbar` 在 `m_rcSearchBox` 上额外画了一个视觉外框（注释原话："EDIT 只占中间一行，边框必须画在父窗口"），自绘 TextBox 现在自己画框 → 两层；表单卡的多行/单行框（`InflateRect` 描边）、详情面板备注编辑态（`FillRound+Stroke`）同样是页面在画框，控件内再画一次。② 页面为原生 EDIT"文字贴顶"的行为把单行控件压到一行文字高（20px），自绘 TextBox 的文本区 = 客户区 − 2×unit(8) = **4px**，13px 字体垂直居中在 4px 条带里被上下裁切——这就是"字的大小明显不对"。
  - **修法**：页面不再画任何输入框边框（自绘 TextBox 自管：占位/焦点/选区同一套外观）；搜索框给足 `searchH`(36)、表单备注给足 `inputH`(32)；详情面板备注框去掉 4px 内缩、直接铺满；删死成员 `m_lineH`。全仓只有 ClipsPage 用 TextBox，无其他页面受同类影响。
  - **实测**：`boxrect.exe` 探针确认搜索框 318x20 → **320x36**；截图裁剪核对四处（搜索框 / 表单多行 / 表单单行备注 / 详情备注编辑态）全部单层边框、占位字与实际文字大小正常且垂直居中、焦点蓝框与 caret 正常；`ctest -C Release` 28/28。
  - **判据**：自绘控件接管了"画自己"之后，页面层所有为原生控件行为做的补偿（代画边框、压行高、内缩留边）必须同步撤掉，否则两套几何互相打架——这类"自绘接管"的回归面不在控件自身，而在调用方的页面层。

- **2026-11-23（跟进三）— 自绘 TextBox 三个鼠标交互补齐（双击选词 / 拖选 / 右键菜单），顺带挖出两个真缺陷**
  - **范围**：`TextBoxProc` 原来只有左键单击定位（`WM_LBUTTONDOWN` → `CaretAtClientPoint`），相对原生 EDIT 缺三块：`WM_LBUTTONDBLCLK`（双击选词）、`WM_LBUTTONUP`/`WM_MOUSEMOVE`（按住拖选）、`WM_RBUTTONUP`/`WM_CONTEXTMENU`（右键菜单）。ListBox 侧这三样都有，TextBox 侧补齐后两者交互对齐。
  - **实现**（复用既有 core 几何 `IndexAtX`/`LineOfIndex`，与 ListBoxProc 同模式）：
    - `HitIndex(pt)`：客户区点 → 全局文本下标，单行/多行两路（多行先按 `m_scroll` 换算行号），四个交互共用。
    - `BeginDrag`：左键下 = 定位 caret + 记拖选锚点 + `SetCapture`；Shift+下击保留原锚点扩选。
    - `DragTo`：拖动中每帧重算光标下标，多行拖到上下 12px 边缘自动滚屏（±3px/帧）。
    - `SelectWordAt`：双击选词 —— ASCII 词 = 连续字母数字（向前/向后扩到非词字符）；CJK/标点选点击处单个字符（保守行为，不引入分词依赖，与系统 EDIT"中文双击选整词"的差异已记录）。
    - `ShowEditMenu`：右键/Shift+F10 弹**自绘浮层**菜单（`RunPopupMenu`，与 `Shell::ShowPopupMenu`/设计系统一致，不用原生 `TrackPopupMenu`）：复制/剪切（无选区时 disabled）/粘贴，走 core `EditAction::Copy/Cut/Paste` + `Win32Clipboard`。
    - `EM_GETSEL` shim：返回 `[SelectionBegin, SelectionEnd)`，与既有 `WM_GETTEXT`/`EM_SETSEL` 并列，供跨进程选区探测（`test_ui` 尚未用到，留作标准探测面）。
  - **挖出的真缺陷 ①（T2 拖选锚点）**：`BeginDrag` 里锚点赋值排在 `cursor = HitIndex(pt)` **之前**，普通下击时锚点拿到的是**旧光标**位置，拖选起点整体错位。修正为 cursor 定位之后取锚点。
  - **挖出的真缺陷 ②（T3 右键清选区）**：`CaretAtClientPoint` 原来**无条件清选区**，右键定位 caret 时把选区一起清掉 → 菜单里复制/剪切永远 disabled（死项）。改为原生 EDIT 语义：右键落在已有选区内 → 保留选区（光标停到选区尾），选区外 → 定位 caret 并清。第二处坑：选区判定必须在 `IndexAtX` 移动 cursor **之前**捕获原 `[begin,end)` —— 移动后再查 `SelectionEnd` 会被收缩（`SelectionEnd = max(anchor, cursor)`），点击点被误判在选区外。
  - **实测**（`labtest.exe` 扩了 `clear`/`getsel`/`setsel`/`dblclick`/`drag`/`rclick` 命令 + `menuclick.exe` 用 `PostMessage` 驱动自绘菜单项；跨进程 `SendMessage` 模态菜单只改发件线程栈上的 `done` 标志、循环线程永不醒，必须 `PostMessage` 投进队列由模态循环线程自己派发）：
    - **T1 双击选词 PASS**：键入 `hello world`，双击 `world`（x=75）→ `sel=[6,11)`，键入 `x` 替换 → `hello x`。
    - **T2 拖选 PASS**：拖 x=30..100 → `sel=[3,11)`，键入 `X` 替换 → `helX`（锚点顺序缺陷修复前为 `[11,11)` 空选区）。
    - **T3 右键复制/粘贴往返 PASS**：双击选 `world` → 右键**选区内**（x=75）→ `sel=[6,11)` 保留（缺陷②修复前被清成 `[10,10)`）→ 菜单 复制 → 清空 → 菜单 粘贴 → 框内恢复 `world`。右键**选区外**定位 caret 不清他处选区、空框/无选区时复制剪切灰显 粘贴可用（截图核对自绘浮层样式）。
    - `cmake --build build --config Release` 全目标无错无警；`ctest -C Release` **28/28**；`check-core-clean` PASS（55 文件 0 平台引用）；`check-public-headers` 全量子配置 PASS；`skia-syntax` 不涉及（本次改动仅 `platform/win32/Widgets.{h,cpp}`，`platform/skia/` 零改动，CI bash 侧照常跑）。
  - **测试工具链注记（非产品缺陷）**：`labtest clear` 原实现 `EM_SETSEL(0,-1)` + `VK_BACK` 背靠背 `SendMessage`，跨进程无消息泵间隔时 Backspace 偶发不落地（拆成两个进程分开发则稳定删选区，框内"选区+Backspace"本身经双击选词+Backspace→`hello `、跨进程 setsel+key→空、粘贴替换整段等多条路径验证无误）。`clear` 已改走 `WM_SETTEXT`（框有标准 shim，确定性重置 text/cursor/selection 并重建布局）。
  - **边界**：双击选词对 CJK 选单字（不做分词）；拖选自动滚屏阈值 12px/±3px 为经验值（与原生 EDIT 手感对齐留待后续）；菜单无快捷键列（复制/剪切/粘贴三项，后续如需 Ctrl+C/X/V 提示可加）；多行拖选的垂直滚屏只处理上下边缘（左右横向滚动沿用既有 caret 跟随，拖选横向未加自动滚）。

- **2026-11-24（跟进四）— 跨进程测试驱动入库（E）+ test_ui IPC 协议缺陷修复（B3，"保存后挂起"根因消除）**
  - **E：测试驱动入库**（commit `c16911c`）：跟进三的 T1/T2/T3 验证工具（`labtest`/`menuclick`/`boxrect`）原来散在 gitignore 的 `build/`，干净克隆无法复现。现移入 `tools/diagnostics/`（与既有手工诊断同目录同约定，不进 ctest），加 CMake WIN32 console 目标（仿 `test_paint_runtime` 的 `if(WIN32)` 块）；`textbox_lab_suite.ps1` 做 T1/T2/T3 端到端脚本（单实例管理、逐步断言、`-BuildDir` 参数化、exit 0/1）。
    - 踩坑：`menuclick.c` 的 `EnumWindows` 回调当初写成 cdecl（本地 x64 无感），CI `windows-x86` 作业（`-A Win32`）`WNDENUMPROC` 是 `__stdcall`，报 C2440 挂掉 full build（run `45395903227`）。补回 `CALLBACK`（`f21581e`），本地按 CI 同款 `-A Win32` 全新 configure+构建三目标验证通过；CI `45401853406` 6/6 绿。
  - **B3：test_ui "保存后 IPC 校验段挂起"根因消除**（commit `f25e3bf`）：ROADMAP 边界① 一直记为"命名管道既有环境行为"，本次用探针（`probe_ipc.c`）拿到确凿根因——**协议缺陷，不是环境问题**。
    - **根因①（死锁）**：`PipeServer::Worker` 是两行帧协议——对每个连接先读 tag 行（`REQ`）再读 payload 行（`<id>\t<line>`），回 `RSP\n<escaped(id\tresp)>\n`（`src/ipc/PipeServer.cpp:113-135`）。旧 `IpcReq` 只发单行 `LIST\t<kw>\n`：服务端把它当 tag 读完，**阻塞在第二行 payload 的 `PipeReadLine` 上永等**；客户端同时阻塞在 `ReadFile` 等响应 → 双向死锁。探针 mode1（旧式单行）3s 超时复现挂死，mode2（两行 REQ）毫秒级得 `RSP`。
    - **根因②（EVT 夹帧）**：`SAVE`/`DEL`/`CLEAR` 的 handler 先 `SendEvent` 广播（`NEWRECORD`/`DATA_CHANGED`）再写 `RSP`，广播落在本连接**前面**——一次性读两行的客户端把 `EVT` 当 `RSP` 解析（探针 mode3 实测 `tag=[EVT] payload=[DATA_CHANGED]`）。
    - **修法（只改 `tests/test_ui.cpp`，不动产品代码）**：`IpcReq` 重写为与 `PipeServer.cpp` 对齐的两行 REQ/RSP 协议（overlapped + 4s 超时读，绝不无限阻塞）；读帧循环跳过 `EVT` 等非 RSP 帧直到命中 `RSP`（与 `PipeClient::ReaderThread` 消费交错帧一致）；响应线上无真制表符（服务端对整个 `<id>\t<resp>` 做 `EscapeField`）→ 按字面 `"1\t"` 三字符剥 id 前缀再 `UnescapeField` 还原；服务不可达快速失败记 `SKIP`（环境问题不再表现为挂死/假 FAIL）；清理改为循环 `DEL` 全部匹配项（自愈历史残留）；链路二 `NOTE` 回写保持字段转义透传（与 `ClipsPage`/`ManualEntryPlugin` 的发送方转义约定一致）。
    - **实测**（`AireRecord` 服务 Running）：修复前保存后 IPC 段**永久挂起**（历史记录）；修复后完整跑完 `exit=0`、~7s——`PASS entry saved via IPC (id=375)`、`PASS cleanup DEL ok (1 entries removed)`、`PASS original note restored`，全链路 PASS（主窗口/列表 122 行/搜索框/表单开关/备注编辑）。
    - **遗留 WARN（非缺陷）**：`list count 122 -> 123` 快照差 ±1——`DATA_CHANGED` 广播后 UI 列表**异步**重填，`cntAfter` 采样早于删除后的刷新落地；DB 实际行数与初始一致（非数据泄漏），纯采样时机问题，已加注释。
  - **边界**：`test_ui` 仍不接 ctest（需运行中 `airecord.exe` + 服务 + 可见窗口，手工跑）；B1（IME 各家边角）、B2（GDI vs Skia GUI 级像素 diff）不在本轮范围，仍在 B 清单。

- **2026-11-25（跟进五）— UI 优化第一批：搜索清空 + 实时搜索根因修复 + 暗色模式入口 + 窗口位置记忆**
  - **范围**（按用户批准的优先级 1→3→4；2 撤销/重做单独下一轮，它要动 core 引擎）：
    1. 搜索框「×」清空按钮（P2 高频小活）
    3. 暗色模式设置入口（P3，框架全在只差入口 + 持久化）
    4. 窗口位置/尺寸记忆（P3 小活）
  - **P1-1 根因（比预期更根本）**：实现清空按钮时发现 `SearchPlugin`（250ms 防抖 → `RefreshData`）依赖 `EN_CHANGE`，但全仓 `EN_CHANGE` **只有消费方、没有发送方**——自绘 `TextBox` 改文本后从不发 `WM_COMMAND(EN_CHANGE)`，**实时搜索过滤一直是死的**（输入不触发过滤，只有 F3/切页/新记录才刷新）。
    - **修法**（`Widgets.cpp` `TextBox::ApplyPostEdit()`）：所有编辑路径（WM_CHAR / WM_KEYDOWN / WM_SETTEXT / 粘贴 / 菜单操作 / IME 结果）都汇聚到 `ApplyPostEdit()`，在那里加「内容变了才通知」的门控——新增 `m_lastNotified` 成员（`Widgets.h`），`m_edit.text != m_lastNotified` 时发 `WM_COMMAND(EN_CHANGE)` 给父窗；纯光标/选区移动内容未变不发（避免白跑防抖查询）；`m_hwnd != GetParent(m_hwnd)` 守卫防顶层自通知。分发链：TextBox → Shell `WM_COMMAND`（`Shell.cpp:1633`）→ `ClipsPage::OnCommand` → `Plugins().DispatchCommand` → `SearchPlugin`。
  - **P2-1 搜索框「×」清空按钮**（`ClipsPage`）：
    - 库层：`Custom` 控件升级出**可交互变体**——`OnClick(cb)` 设回调即捕获鼠标、自管 hover/press（`WM_NCHITTEST` 不再强制 `HTTRANSPARENT`；`CustomDesc.mouseTransparent` 字段此前是死的，实现里恒穿透，现在尊重它）；不设回调 = 纯装饰，维持原穿透语义（chip 不受影响）。
    - 页面层：`IDC_SEARCH_CLEAR = 1008`，22x22 子窗口覆盖在搜索框上、chip 左侧；`SyncSearchClearButton()`（`OnCommand` 的 `EN_CHANGE` + `Relayout` + `SyncVisibility` 三处驱动）按文本非空显隐，并联动 `SetRightInset`（chip 常驻 + 清空按钮）保证文字任何时刻不被压。点击 → `m_search->SetText(L"")` → EN_CHANGE → 防抖 → `RefreshData`（清筛选恢复全量）+ 焦点还回搜索框。「×」两笔对角线走 `Canvas::Native()` GDI 逃生舱（Canvas 无直线 API）。
  - **P3-3 暗色模式入口**（`App` + `ConfigPage`）：框架早已齐全（`DarkPalette()` 完整调色板、`ShellOptions.dark`、`Shell.cpp:242` 启动时 `SetPalette`），只缺 UI 入口与持久化。
    - `App::SetThemeDark(bool)`：`machine::Set(L"ui", L"dark", ...)` 持久化 + `SetPalette(DarkPalette()/LightPalette())` 运行期换肤 + 全量重绘（`Shell::Invalidate()` 只及父窗，子控件是独立子窗口各自读 `Pal()` 自绘，须 `EnumChildWindows` 逐个 `InvalidateRect`）。
    - `App::Run` 启动读 `[ui] dark` 填 `ShellOptions.dark`（与 `close_behavior` 同模式）。
    - `ConfigPage` 新增「显示与外观」卡（主题 / 配色两行），右上角自绘开关（HotState 热区 + 轨道/滑块绘制，按下-抬起语义与 ClipsPage 按钮一致）→ `m_app.SetThemeDark(!m_app.IsDark())`。
  - **P3-4 窗口位置记忆**（`App`，不动 Shell 库）：
    - 消息钩子里 `WM_MOVE`/`WM_SIZE` **只武装不消费**（消费会吞掉 Shell 自己的 `WM_SIZE` 布局）→ `TIMER_WINDOW_RECT`（11）800ms 防抖 → `SaveWindowRect()`：`GetWindowRect` → `machine::Set(L"ui", L"window_rect", L"x,y,w,h")`；`IsZoomed/IsIconic` 时不存（最大化矩形是显示器边界，不是用户摆放位）。
    - `RestoreWindowRect()` 在 `RunMessageLoop` 前（窗口已建未显示，`SetWindowPos` 无可见抖动）：解析 + 尺寸合法性校验（`w<200||h<150` 视为损坏不恢复）+ 显示器工作区夹取（落点全在工作区外 → 居中到最近显示器，处理「上次停在已拔掉的副屏」）。
  - **实测**（停服务隔离看门狗、新构建 `build\Release\airecord.exe` + 跨进程探针）：
    - 实时搜索：输入不存在关键词 `zzzzqqqq` → 900ms 后列表 140 → **0**、清空按钮出现（修复前该链路完全不通）。
    - 清空按钮：点「×」→ 列表恢复 **140**、按钮消失（EN_CHANGE → 防抖 → RefreshData 全链路）。
    - 暗色：配置页点开关 → 像素采样卡片 **33,36,41** / 侧栏 **28,31,36**（与 `DarkPalette` 精确一致）、`ini: dark=1`；再点切回 → **255,255,255** / `dark=0`；截图确认整屏（侧栏/卡片/标题栏/状态栏/开关）一致。
    - 窗口记忆：`SetWindowPos (400,120) 980x660` → 800ms 后 `ini: window_rect=400,120,980,660`；重启 → 恢复 `rect=(400,120,1380,780) 980x660` 精确命中。
    - 回归：`ctest -C Release` **28/28**；`check-core-clean` PASS（55 文件 0 平台引用）；x86 构建树（`-A Win32`，CI 同款）全新 configure + 构建 `airecord` 通过。
  - **边界**：
    - 自绘控件的暗色换肤靠「各自读 `Pal()` 重绘」，`SetThemeDark` 时一次性 `InvalidateRect` 全部子窗口——若将来新增自绘控件类型，需确认其 `WM_PAINT` 读 `Pal()` 而非缓存颜色（现有 TextBox/ListBox/Custom/Button 都符合）。
    - 窗口记忆不跨 DPI 缩放比例保存（96 DPI 逻辑像素；缩放比变化后位置可能偏移，未做 `WM_DPICHANGED` 适配——那是 P3 清单另一项）。
    - 撤销/重做（P1-2）见下一节跟进六；maxLength/password 透出（P1-3）见跟进七；高 DPI 动态适配、列表多选/分页仍在本清单后续。

- **2026-11-25（跟进六）— 自绘 TextBox 撤销/重做（Ctrl+Z / Ctrl+Y / 菜单「撤销/重做」）**
  - **范围**：用户批准顺序里的第 1 项。把自绘 `TextBox` 的编辑历史补全到原生 EDIT 语义：可撤销/重做的键盘快捷键（Ctrl+Z 撤销、Ctrl+Y 与 Ctrl+Shift+Z 重做）+ 右键菜单「撤销/重做」（无历史时灰显）。**core 引擎保持纯函数不动**——`ApplyEdit` 是无状态变换，历史属于**控件实例**，所以快照栈放在平台层 `TextBox`，不给 `EditAction` 加状态。
  - **实现**（仅 `platform/win32/airui/Widgets.{h,cpp}`，core 零改动）：
    - **快照点**：`TextBox::ApplyEditResult()` 是**所有**编辑动作（WM_CHAR / Backspace / Delete / Cut / Paste / 菜单复制剪切 / IME 结果 / 选区删除）的唯一汇聚点。在调 core `ApplyEdit` **之前**，若该动作**可能改文本**（Insert/Backspace/Delete/Cut/Paste，按动作类型判断、与参数无关——Insert 传空串、Cut 无选区不会真改，多存一个快照无害）就把整个 `TextEditState`（文本/光标/选区/组词一起）压入 `m_undoStack`，并清空 `m_redoStack`（新编辑使 redo 链作废）。移动/全选/纯复制不动文本，不产生条目。
    - **Undo()/Redo()**：栈顶 `TextEditState` 还原进 `m_edit`、当前态入对侧栈，`m_layW=-1` 强制重建 `m_cum`/`m_lines`（快照里光标/文本都变），再走 `ApplyPostEdit()`——复用跟进五的 EN_CHANGE 门控（撤销后文本变化照常通知，纯光标态不通知）。`CanUndo()/CanRedo()` 暴露栈非空给菜单置灰。
    - **快捷键**：`DefaultKey` 的 Ctrl 分支加 `'Z'`（`shift` ? Redo : Undo，Ctrl+Shift+Z 是标准 IDE 重做约定）、`'Y'`（Redo）。**Ctrl+C/X 从直调 core `ApplyEdit` 改走 `ApplyEditResult`**——否则 Ctrl+X 剪切不经过快照点，会变成不可撤销（与菜单剪切语义分叉）。
    - **程序化重置**：`SetText`（含 `WM_SETTEXT` shim）清空两栈并把 `m_lastNotified` 基线设为新文本——原生 EDIT 语义：外部整体设文本后历史作废，EN_CHANGE 基线同步避免空通知。
    - **菜单**：`ShowEditMenu` 由 3 项（复制/剪切/粘贴）扩为 **6 项 + 1 分隔符**：撤销 / 重做 / —— / 复制 / 剪切 / 粘贴。撤销/重做无历史时 `disabled`（灰显、点击无效，`RunPopupMenu` 的 `MenuItemAt` 对 disabled 项返回 -1）。**菜单几何随之变化**：项行 36px、分隔符 6px，窗口由 192×110 变 **192×188**（client 186）。
  - **诊断工具链同步**（`tools/diagnostics/`，与菜单几何变化绑定，必须一起改）：
    - `menuclick.c` 从「`<item> <rows>` 均分高度」改为「`<item> <pixelY>` 按像素定位」——菜单混了两种行高（项 36 / 分隔 6），均分法在菜单变大后必错位。当前 6 项菜单的行中心 y：**撤销 18 / 重做 54 / 分隔 75 / 复制 96 / 剪切 132 / 粘贴 168**。
    - `textbox_lab_suite.ps1` 的 **T3**（右键复制/粘贴往返）改用新行中心 y（复制 96 / 粘贴 168），并修掉原 getraw 断言会匹配到 3002/3003/3004 的空文本行造成假 PASS（锚定 `id=3001 .*[文本]`）。新增 **T4**（撤销/重做）：`clear`（SetText 清栈）后菜单撤销为 no-op → 键入 `hello`（5 条历史）→ 菜单撤销 ×5 → 空 → 菜单重做 ×5 → `hello` → 撤销/重做交替（`hell`↔`hello`）→ 全选+复制+清空+粘贴还原 → 撤销还原粘贴。
  - **实测**（非假设）：
    - `textbox_lab_suite.ps1` **T1/T2/T3/T4 全 PASS，exit 0**（T4 覆盖：无历史撤销 no-op、撤销清空、重做还原、交替撤销重做、复制粘贴往返+粘贴可撤销）。
    - `cmake --build build --config Release` 全量 `BUILD=0`；`ctest -C Release` **28/28**；`check-core-clean` **PASSED**（55 core 文件 0 平台引用，core 确实零改动）；`build-x86`（`-A Win32`，CI `windows-x86` 同款）全新 configure + 构建 `airecord` 通过。
  - **边界 / 踩坑**：
    - **合成键盘无法可靠投到跨进程子窗口**（本轮最重要的环境结论）：`TextBoxProc` 用 `GetKeyState(VK_CONTROL)` 判 Ctrl，posted `WM_KEYDOWN` 伪造不了；`SendInput` 虽返回 1、前台也是 lab 壳，但键始终落不进子框（跨线程 `SetFocus` 是 no-op，`SendInput` 只路由到前台窗口焦点控件，抢前台又受前台锁规则制约、时灵时不灵）。因此 **T4 改走右键菜单**——菜单「撤销/重做」与 Ctrl+Z/Ctrl+Y 调的是**同一个** `Undo()`/`Redo()`，逻辑与菜单集成都被完整验证，且全程用可靠的 `PostMessage`。真实键盘上的 Ctrl+Z/Y 路径（`DefaultKey`）代码就位、由与 T4 相同的 `Undo()/Redo()` 兜底，但**跨进程自动化无法直接断言**该快捷键，此为已知局限（记入 README）。
    - 右键菜单复制/粘贴/撤销/重做依赖「右键落点」：落**选区内**保留选区（复制/剪切才有效），落**选区外**先定位 caret 并清选区（此时复制/剪切灰显）——这是跟进三修过的原生 EDIT 语义，T4 的复制步骤因此要先 `setsel 0 -1` 全选且右键 x 落在文本宽度内（`"hell"` ≈ x8..36，取 x=20）。
    - 撤销/重做栈是**纯内存**的（不持久化、跨进程/重启不保留，与原生 EDIT 一致）；未做「按词/按行」的合并粒度（每个字符/动作一条，与当前原生 EDIT 默认一致）；未做撤销上限（内存小、文本框内容有限，暂不设 `kMaxUndo`）。

- **2026-11-25（跟进七）— 自绘 TextBox maxLength / password 平台 API 透出（P1-3）**
  - **范围**：用户批准顺序里的第 2 项。把 core 引擎**已有**的两个能力透出到平台公共 API（`Widgets.h` 的 `TextBox`/`TextBoxDesc`，签名保持不变）：`maxLength`（最大字符数）与 `password`（密码掩码绘制）。**core 零改动**——`InsertAtCursor` 早已按 `TextEditState::maxLength` 截断（room≤0 拒插、超长粘贴截到 room），`DisplayText(st, true)` 早已是 1:1 等长 bullet（每个 UTF-16 单元 → `•`），本轮只把 desc 的声明接进引擎、把显示串接进绘制/布局。
  - **实现**（仅 `platform/win32/airui/Widgets.{h,cpp}`，core 零改动）：
    - **`TextBoxDesc` 加两字段**：`int maxLength = 0`（0 = 不限，UTF-16 单元计数，与 core `TextLength = s.size()` 同口径）、`bool password = false`。公共 API 加 `SetMaxLength(int)`/`MaxLength()`/`SetPassword(bool)`/`IsPassword()`（运行期可改，`SetPassword` 切换触发重绘）。
    - **maxLength 双路径**：键入/粘贴/选区删除后的插入走 core `InsertAtCursor`（按 `m_edit.maxLength` 强制）；`CreateNative` 把 `m_desc.maxLength` 同步进 `m_edit.maxLength`（desc 是页面侧唯一声明点）。**`SetText`（程序化）也截断**——否则 `WM_SETTEXT` 成为绕过上限的旁路（截到前 maxLen 单元，再置 cursor/基线）。`SetMaxLength` 运行期改上限时对已超限内容立即截断 + 光标夹取。
    - **password = 显示串掩码，数据路径不动**：真实文本只存在于 `m_edit.text`（`Text()`/`WM_GETTEXT`/编辑/EN_CHANGE/剪贴板复制全部读真实值，与原生 EDIT 一致）。`ShownText()` 返回 `m_desc.password ? DisplayText(m_edit,true) : m_edit.text`（缓存进 `m_shown`）。掩码与真实文本 **1:1 等长**（每 UTF-16 单元一个 `•`），所以光标/选区/折行的**几何全部按显示串算**、下标语义不变，无需第二套布局。
    - **布局/命中/绘制切到显示串**：`SyncTextLayout` 按 `ShownText()` 测 `m_cum`/`WrapText`；**布局缓存判据**从「尺寸 + `m_cum.size() == text.size()+1`」改为「尺寸 + `m_laidShown == ShownText()`」——等长换文本、password 开/关（掩码与真实串等长）都会让显示串变化，旧判据会读到旧字形宽度，必须重建。`HitIndex` 命中按显示串（与 `m_cum`/`m_lines` 同源）；`PaintSelf` 单行/多行文本绘制源改 `ShownText()`。`SelectWordAt` 仍用真实文本扩词（掩码 1:1，下标等价；复制得到真实文本，正确）。
    - **Undo/Redo 的 maxLength 再同步**：跟进六的快照整个 `TextEditState`（含 `maxLength` 字段），运行期 `SetMaxLength` 改过上限后撤销会把**旧快照里的 stale maxLength** 带回。`Undo()`/`Redo()` 还原后统一用 `m_desc.maxLength` 回写 `m_edit.maxLength`（desc 是运行期真值）。
  - **lab 扩展**（`examples/TextBoxLab.cpp`，4 框 → 7 框，前四框坐标不变、T1–T4 不受影响）：⑤ `3005 maxLength=5`、⑥ `3006 password`、⑦ `3007` = ⑥ 的**明文孪生**（同文本不掩码，像素对照用）。窗口 760×640 → 760×760。
  - **诊断工具链同步**（`tools/diagnostics/`）：
    - `labtest.c` 的 `rect`/`getraw` 循环 3001–3004 → **3001–3007**；新增 `settext <id> "text"`（完整 `WM_SETTEXT` 串，确定性程序化路径，供 maxLength 截断断言）。
    - 新增 `pixhash.c`（`<x> <y> <w> <h>` → BitBlt 屏幕矩形 + FNV-1a64，打印 `pixhash <16-hex> <w>x<h>`）：T5 的「渲染不同」断言用，不依赖字体度量假设。
    - `textbox_lab_suite.ps1` 新增 **T5**（maxLength + password），CMake 驱动 foreach 加 `pixhash`。
  - **实测**（非假设）：
    - `textbox_lab_suite.ps1` **T1–T5 全 PASS，exit 0**。T5 覆盖：`type 3005 "abcdefghij"` → 截到 **5**（`abcde`）；`clear` + `settext 3005`（20 字符）→ 程序化也截到 **5**（`WM_SETTEXT` 不是旁路）；`secret` 同文本入 3006（掩码）/3007（明文），`getraw` 两框都报 **6 字符明文**（数据路径不被掩码污染）；`pixhash` 3006 vs 3007 屏幕矩形**不同**（`87FCD0773643CAD1` ≠ `C5667C7A887406A1`，掩码≠明文）、同矩形二拍**相同**（稳定对照）。
    - `cmake --build build --config Release` 全量 `BUILD=0`；`ctest -C Release` **28/28**；`check-core-clean` **PASSED**（55 core 文件 0 平台引用，core 确实零改动）；`build-x86`（`-A Win32`，CI `windows-x86` 同款）全新 configure + 构建 `airecord`/`airui_textbox_lab` 通过。
  - **边界 / 踩坑**：
    - password **只掩码绘制**，`Text()`/`WM_GETTEXT` 仍返回真实明文（与原生 EDIT 一致）；本轮**不做**明文切换（reveal 眼睛按钮）、不做「复制时写掩码」——复制始终写真实文本（密码框复制真实值是原生 EDIT 默认，更安全，避免用户粘到别处是乱码点）。
    - 掩码 bullet `•` 是单 UTF-16 单元，与真实文本 1:1 等长是**设计前提**——core `DisplayText` 已是该语义，`m_cum`/`IndexAtX`/`WrapText` 因此无需知道「这是掩码」。若将来 `DisplayText` 改成非等长（如按码点合并），几何源必须同步改，不能只改绘制。
    - maxLength 按 **UTF-16 单元**计数（与 core `TextLength` 同口径），**不是**码点/字数——含代理对的 emoji 一个码点占 2 单元。与原生 `EM_SETPASSWORDCHAR`/`SetLimitText` 的「字符」口径可能有差，当前按 UTF-16 单元是有意选择（与引擎一致、可预测）。
    - `SetMaxLength(0)` = 不限（负数按 0 处理）；`SetPassword` 运行期切换只改绘制（掩码与真实串等长，几何源不变），但 `•` 与原字形宽度不同 → `m_laidShown` 判据自动触发 `m_cum` 重建。
    - 本轮未接 Skia 双后端像素 diff（password 走同一 `g->Text` 原语，由 CI `skia-windows` 的 G5 parity 兜底）；高 DPI 动态适配、列表多选/分页仍在本清单后续（第 3 项）。

- **2026-11-26（跟进八）— 高 DPI 动态适配（第 3 项之 a：DPI）**
  - **范围**：用户批准第 3 项「高 DPI 动态适配 / 列表多选 / 分页」中的 DPI 部分；**core 零改动**（`Metrics::ForDpi`、`Fonts::Create(dpi)` 基建已在位）。
  - **实现**（`src/airui/platform/win32/Shell.cpp/h`、`src/airui/platform/win32/Widgets.cpp/h`、`src/core/App.cpp`、`src/pages/ClipsPage.cpp/h`、`CMakeLists.txt`、`tools/diagnostics/`）：
    - **进程感知**：`Shell::Create` 顶部调用 `EnsureProcessDpiAwareness()`（动态加载 `shcore`，优先 `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)`，退而 `SYSTEM_AWARE`，再退而 `SetProcessDPIAware`；Win7 无 shcore 则保持 unaware，行为不变）。要在任何窗口创建前执行，且幂等。
    - **96-DPI 基线**：`Shell::Create` 新增 `m_baseMetrics`，在第一次 `SetMetrics(ForDpi(m_dpi))` 之前以 `Mx()` 原值落地——防止以后对已缩放的 Mx() 再 ForDpi 二次放大。
    - **处理器**：`Shell::HandleMessage` 新增 `case WM_DPICHANGED`（见 `case WM_DPICHANGED: case WM_USER + 9000:`）：1. `diag::Log("[dpi] case body ...")`（保留可观测日志）；2. `ApplyDpiChange(newDpi)`：`m_dpi`→`m_fonts.Create(newDpi)`→`SetMetrics(m_baseMetrics.ForDpi(newDpi))`→经 `m_dpiChanged` 通知 app；3. 按 lParam 建议矩形 `SetWindowPos`（真实消息；跨进程无效，见边界）；4. `LayoutAll()` + `InvalidateRect`（幂等重排，覆盖「未变化」分支）。
    - **`WM_USER + 9000` 共享处理体**：测试入口，与 WM_DPICHANGED 同体（驱动 `dpik` 实测）；`WM_USER + 9001` 调试探针已删。
    - **App 回调**：`App::Run` 注册 `m_shell.SetDpiChangedHandler` → `m_clips->RebuildListForDpi()`（指纹置 `LLONG_MIN` 强制重建，用 `-9223372036854775807LL - 1` 避免 `<climits>`）。
    - **ClipsPage**：`RebuildListForDpi()` 新增（注释含离线安全：不重取数据、离线也不清列表）。
    - **TextBox 缓存键**：`m_layFontDpi` 加入 `SyncTextLayout` 的 `same` 判据（DPI 变化即使客户区尺寸不变也重测宽度表）。
  - **实测（非假设，本节记录）**：
    - 环境：两块屏均 96 DPI；本构建的 Windows **拒绝跨进程投递合成 WM_DPICHANGED**（PostMessage/SendMessage 均返回失败 GLE 1159，进程内 SendMessage 也不派发——独立探针 `build\dpiinproc.exe` 实证）。
    - 实测驱动：`dpik` + `boxread`（新增 `tools/diagnostics/` 并登记 CMake foreach，`README.md` 加「High-DPI dynamic-adaptation drivers」节）。
    - **Round-trip（pid 525984，`build\Release\airecord.exe`）**：`SendMessage(WM_USER+9000, 150)` → `client.log` 含 `[dpi] case body dpi=150 m_dpi=96` 与 `[dpi] after ApplyDpiChange m_dpi=150 Mx.searchH=56 Mx.itemH=100`，`boxread`: `row0=47, row1=100`（96 基线 30/64）；`SendMessage(WM_USER+9000, 96)` → `m_dpi=96 Mx.searchH=36 Mx.itemH=64`，`boxread`: `row0=30, row1=64`（恢复）；`SendMessage(WM_USER+9000, 150)` → `Mx.searchH=56 Mx.itemH=100`，`boxread`: `row0=47, row1=100`（再度缩放）——**96↔150↔96 正常往返，无复合放大**。
    - 全量门：`cmake --build build --config Release --target airecord`（BUILD=0）、`ctest -C Release` **28/28**、`check-core-clean` PASSED、x86 `build-x86` 重建 `airecord.exe` 通过。
  - **边界 / 踩坑**：
    - 真跨屏 DPI 变化未在本机可测（两屏同值 96）；`WM_USER+9000` 是共享处理体的测试替身，与真派发走同一 `ApplyDpiChange`。
    - `m_baseMetrics` 取自 `Shell::Create` 时的全局 `Mx()`；若以后有人在其前调 `SetMetrics`，会被视为基线（airecord 启动不加载 JSON 主题）。
    - 真实 lParam 指针仅系统派发时有效；测试驱动不依赖它（lParam=0 走原地重排分支）。

- **2026-11-26（跟进九）— 列表多选 + 分页（第 3 项之 b / c）**
  - **范围**：用户批准第 3 项的后两块。3b = 自绘 `ListBox` 多选（Ctrl 切换 / Shift 范围）+ `ClipsPage` 批量复制/删除；3c = `ClipsPage` 列表分页（按页切片 + core 窗口式页码条）。core 除复用既有 `Pagination*` 外零改动（`MultiSelect` 是 core 里另一个控件，本轮未用；多选做在 `platform/win32` 自绘 ListBox 内）。
  - **3b 实现**（`platform/win32/airui/Widgets.h` + `Widgets.cpp`）：
    - `ListBoxDesc.multiSelect`（默认 false，单选行为完全不变）；API `SetMultiSelect/IsMultiSelect/IsSelected/MultiSelectionCount/SelectedItems/ClearSelection/SetSelectionRange`。
    - `WM_LBUTTONDOWN` 在 multiSelect 下改走 `OnMultiClick(item, ctrl, shift)`，修饰键取 **wParam 的 `MK_CONTROL`/`MK_SHIFT` 位**（真实鼠标与跨进程 Posted 点击都带，不吃 `GetKeyState` —— 与跟进六的跨进程键盘结论一致）。语义：普通点击=单选（清空、锚点=该行）、Ctrl+=切换、Shift+=锚点到该行的区间加选。
    - `ListItemDraw` 新增 `multiSelected`（多选下与 `selected` 同值）。
    - 消息级探测追加 `LB_GETSELCOUNT / LB_GETSEL / LB_SETSEL` shim（`test_ui`/外部探针按原生 LS_* 语义读）。
    - lab（`examples/TextBoxLab.cpp`）新增第 2 页 `ListLabPage`（`--listbox` 启动即进）：3201 多选、3202 单选、3203 原生 EDIT **镜像**（把 `SelectedItems()` 写成 `multi=[..] single=N`，跨进程用标准 `WM_GETTEXT` 读，绕过自绘列表无文本的限制）。
    - `tools/diagnostics/listtest.c`（`rect` / `click <id> <x> <y> [modsHex]` 客户端坐标 / `selstate`）+ `list_lab_suite.ps1`（L1–L8）**全 PASS**：L1 单选 [0]、L2 Ctrl [0,2]、L3 Ctrl [0,2,4]、L4 Shift 0..3 加入、L5 Ctrl 切出、L6 Ctrl 切回、L7/L8 3202 单选替换。
    - `ClipsPage`：`SetMultiSelect(true)`；`ShowItemMenu` 选中 > 1 时加「复制 N 条 / 删除 N 条记录」；新增 `OnListOpBulk(op)`（0=所选内容 `\n` 拼接一次 COPY；1=逐条 DEL + `RefreshData` + 汇总 Toast）。
  - **3c 实现**（`platform/win32/Canvas.{h,cpp}`、`airui/DrawCall.h`、`core/App.h`、`pages/ClipsPage.{h,cpp}`）：
    - **Canvas 委托 core 分页**：新增 `Canvas::Pagination(rc, PaginationState)` → `PaintPagination`（窗口式：首页/上一页/…/页码/…/下一页/末页，命中码 -2 上一页 / -1 下一页 / >0 页号 / 0 省略号），`PaginationLayout/PaginationHit` 薄转发（与 `EmptyState → PaintEmptyView` 同模式）。`DrawCall` 加 `Pagination` 类型（录像/断言用）。
    - **ClipsPage 分页**：`ApplyEntries` 先在全量 `m_rowsAll` 上建行模型（分组头 + 条目），`SliceRowsForPage()` 按每页 `m_pageSize=10` 条**条目行**切进 `m_rows`；分组头不占页容量（页内最多带该页需要的几个跨组头，头落在页边界上也不会丢）。`m_page`（1-based）/`m_pageCount`；**只有数据/筛选指纹变化才回到第 1 页**，翻页 `ApplyPage()` 不动指纹，`SliceRowsForPage` 里 `m_page` 会被夹到 `[1,m_pageCount]`（末页删光也不会停在不存在的页）。
    - **绘制**：`OnPaint` 在列表卡底部绘 `g.Pagination(m_rcPager, …)`（`m_pageCount > 1` 且表单卡关闭时）；`Relayout` 预留该条高度（`kPagerH=76`）并把列表底边收上去，条不与行重叠。
    - **命中/交互**：`PaginationHitAt(pt)` 直接调 core `PaginationHit`（与 Canvas 同一几何源，不另起一套）；`OnMouseDown` 在页码条上记 `m_pagerHit` 并 `SetCapture`，`OnMouseUp` 落在同一槽才 `ApplyPage`。**踩坑**：列表是独立子 HWND，点在列表区域上的是子窗过程（`WndProc`/`TextBoxProc` 那类），页面收不到；但页码条在列表**下方**（页面客户区），本来就能到页面——所以命中要先于 `m_hot`/列表判断，且 `OnMouseUp` 现在**先算 pt 再看 `m_hot.Pressing()`**（否则 page-only 的按下态会被早退吞掉）。
    - **探针**：`IDC_PAGER_PROBE=1009`，一个铺满页码条区域的 0x0 `Static`（`WS_CHILD|WS_VISIBLE`）随 `Relayout` `SetWindowPos`，给跨进程驱动一个稳定的屏幕矩形去点页码（自绘页码本身没有任何 HWND 可查）。
  - **实测**：全量 Release `BUILD=0`；`ctest -C Release` **28/28**；`check-core-clean` PASSED（55 core 文件 0 平台引用）；x86 `-A Win32`（CI `windows-x86` 同款）`airecord`/`airui_textbox_lab` 构建通过；3b `list_lab_suite.ps1` **L1–L8 全 PASS**。
  - **边界 / 踩坑**：
    - 每页条数固定 **10**（本轮不提供 UI 可调，`m_pageSize` 是成员留了扩展口）；真实数据变化会回第 1 页（语义上"刷新先看最新"，不是 bug）；最后一页删空后 `m_pageCount` 收缩、`m_page` 被夹回。
    - 页码条**只在表单卡关闭时画**（表单卡占了卡片底部，条会被压住），表单卡打开时列表仍全量滚动；列表页内仍可纵向滚动（一页 10 条未必塞满视口）。
    - 3c 的交互只在本机做了一次真实点击验证（页码可翻、行会换、`LB_GETCOUNT` 恒为当页条数），**未写进 suite**——3c 是纯页面层功能，suite 侧复用 3b 的 `listtest`/`selstate` 即可读 `LB_GETCOUNT` 断言；下一页等操作目前无稳定跨进程句柄（页码是自绘），探针 `1009` 只给区域不给具体槽坐标，后续如需要可加 `WM_USER+9002` 查询槽位。
    - 分页与 3a DPI 重建叠加：`RebuildListForDpi` 保当前页，按新 `Mx()` 重切重填——两者都已各自验证一次。
