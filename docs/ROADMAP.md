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
- **测试**：`ctest -C Release` **15/15 通过**（含 Phase 1 新增的 `test_shell_hal`）；`test_ui` 为端到端脚本，仍需手工跑。

---

## 1. 未实现清单（按层）

### A. 跨平台 / 后端层（Phase 1 三项已完成，Skia 仍为设计稿）
| 项 | 状态 | 说明 / 证据 |
|---|---|---|
| Skia 跨平台后端（Win/Linux/macOS 统一绘制） | 未实施 | `docs/cross-platform.md` 标注"草稿 v0.1，仅设计，未实施" |
| Linux / macOS 应用本体构建 | 不可行 | 完整 exe 依赖 Win32/GDI/DWM；目前仅 `airui-core` 可在 Linux 编 |
| `GdiPainter` / `Win32Window` 生产级 HAL | ✅ 已完成 (1) | 真圆角/虚线/alpha/FontRole 字体/裁剪栈；`SetNavBadge` 落库、`WM_GETMINMAXINFO` 强制最小尺寸、滚轮屏幕坐标转客户区 |
| `IWindow` 完整事件注入 | ✅ 已完成 (2) | 平台无关 `IWindow` 补齐 enter/leave/char/focus/blur/DPI 六个事件（默认 no-op，不破坏其他后端），`Win32Window::PumpEvent` 全量转发 |
| `NavBar` / `Caption` / `StatusBar` 迁入 ShellCore HAL | ✅ 已完成 (3) | 按 `ShellCore.h` 的设计契约「core owns state & layout, backend owns pixels」：`Shell` 删除重复状态字段、全部读写 `ShellCore`；后端把真实几何喂给 `SetShellGeometry()`，使平台无关命中与像素对齐；`ShellOptions.coreShell` 提供可选的 `GdiPainter + ShellCore::Paint` 渲染路径（默认关，富渲染零回归） |

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
| 跨后端 DrawCall 一致性断言 | 设计中 | 目前仅 Windows 端录制，Skia 后端不存在则无处断言（`docs/cross-platform.md` G5） |

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
1. 引入 Skia 后端实现 `IPainter` / `IFont` / `IWindow`。
2. 在 Linux / macOS 跑通 AirUI 核心 + 示例（`airui_demo` / `airui_gallery`）。
3. 实现跨后端 DrawCall 一致性断言（复用 `TestShell` + DrawCall 录制）。
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
