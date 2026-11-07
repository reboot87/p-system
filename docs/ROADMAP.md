# p-system / AirUI 路线图与未实现清单

> 目的：把当前"已交付"与"未实现"的边界写清楚，避免把阶段路线图误当成遗漏的 bug。
> 最后更新：2026-11-07。基线：`phase0-baseline`（034bdc2），Phase 0 验收见 `phase0-signoff.md`。

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
  - **边界**：插件跨 DLL 依赖同代 MSVC ABI（`IPainter` 虚方法带 `std::wstring`），真 SDK 的 DLL 链接 / 导出 / 运行由下一次自动 CI 复验；VS2017 官方产物通过 DLL 使用 Skia 的端到端截图未确认；业务模块仍留最后。
