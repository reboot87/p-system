# AGENTS.md — 给 AI 编码代理的约束与约定

> 本文件约束**在本仓库中工作的所有 AI 代理**（含子代理）。它与
> `README.md`、`docs/build-conventions.md`、`docs/ROADMAP.md` 同源；
> 若与它们冲突，以那些文档为准，并及时修正本文件。
> 人类贡献者也可参考，但以 `README.md` 为入口。

---

## 0. 一分钟速览

- 项目：**AirUI**（自研 owner-draw 自绘 UI 框架，`src/airui/`）
  + **AireRecord**（剪贴板工具箱产品，`src/`、`examples/`、`tests/`）。
- 构建：**CMake + MSVC**。本地 VS2017 / CI VS2022。**禁止 MinGW / g++ 作为构建工具链**。
- 分层：`src/airui/core/`（平台无关）→ `src/airui/platform/win32/` 与
  `platform/skia/`（后端）→ `src/pages/`、`src/gallery/`、`examples/`、`tests/`（消费方）。
- 最高优先级约束：**公共头文件（含其 `#include` 传递闭包）不含任何平台
  相关类型 / API / 宏**，以及 **core 层零平台引用**。
  core 侧有 `scripts/check-core-clean.sh` / `.ps1`（文本门禁，CI 实跑）；
  公共头侧有 `scripts/check-public-headers-clean.sh` / `.ps1`（2026-11-26
  起 CI 实跑），**递归扫描公开头的 quoted include 闭包**，平台 token 命中
  即失败；豁免表 `scripts/public-headers-exempt.txt` 已清零（0 豁免）。
  另有 CMake 的 `check-public-headers` 只查**自包含**，两者互补（见 § 5.3 / § 5.5）。

---

## 1. 硬性边界（违反即回退）

### 1.1 core 层必须平台无关
- 目录：`src/airui/core/include/airui/`、`src/airui/core/src/`。
- **禁止**出现：`windows.h`、`HWND`、`HDC`、`HFONT`、`HBRUSH`、`HPEN`、
  `HMENU`、`HINSTANCE`、`HICON`、`HCURSOR`、`LRESULT`、`WPARAM`、`LPARAM`、
  `RECT`、`COLORREF`、`POINT`、`MSG`、`WM_*`、`DT_*`、`GetTickCount`、
  `GetSystemMetrics`、`CreateWindow*`、`DrawText*`、`SetTextColor` 等。
- **连注释和字符串字面量也不行**：门禁 `scripts/check-core-clean.ps1`
  按文本字面匹配，注释里写了 `Win32 RECT` 一样会失败（已在
  2026-11-26 的实际提交中踩到，必须改名或改描述）。
- 验证：`powershell -File scripts/check-core-clean.ps1` → 必须输出
  `check-core-clean: PASSED (<n> core files, 0 platform references)`。

### 1.2 公共头文件不得暴露平台相关物
- 目标：**页面作者 `#include "airui/AirUi.h"` 后，看到的头文件里
  既没有 `<windows.h>`，也没有任何 Win32 类型 / 函数 / 宏名。**
- 这条比 1.1 更宽：1.1 只约束 `core/` 目录本身；1.2 约束任何
  **被 `AirUi.h` 传递包含的公开头**（包括 `platform/win32/airui/` 里
  被当作公开表面的那些）。
- 当前状态（2026-11-26 起）：**已达成**。`AirUi.h` 的 quoted include
  闭包里 0 个平台 token、0 个 `#include <windows.h>`；`platform/win32/internal/`
  下的私有头不再被任何公开头传递包含。
- 允许在**私有实现侧**出现：`*_impl/`、`src/airui/platform/<backend>/src/`、
  `src/airui/platform/win32/internal/`、以及 `.cpp` 里；但私有头**不得**
  从 `AirUi.h` 或任何公开头被 include。
- Win32 类型仅允许以**不透明 `void*`** 穿过公开边界
  （`airui/types.h` 的 `WindowHandle` / `DeviceContextHandle` / `FontHandle`
  / `BitmapHandle` / `GdiObjectHandle` / `InstanceHandle` / `IconHandle` /
  `MenuHandle` / `CursorHandle` 等都是 `void*` 别名）。

### 1.3 公开 API 不得为平台特例开后门
- 不新增 `#ifdef _WIN32` 分支到公共头；差异全部落在后端实现。
- 新增公共类型必须先加到 `core/include/airui/types.h`（或 core 新头），
  并保持「与平台布局兼容、可逐字段复制」的既有契约
  （`types.h` 已有 `static_assert` 守住尺寸）。

---

## 2. 构建（唯一合法方式）

| 场景 | 命令 |
|---|---|
| 本地一键 | `build.bat`（产物在 `build-cmake\Release\`） |
| 手动配置 | `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` |
| x86 对齐 CI | `cmake -S . -B build-x86 -G "Visual Studio 17 2022" -A Win32` |
| 增量构建 | `cmake --build build --config Release`（可加 `--target <名>`） |
| 测试 | `ctest --test-dir build -C Release` |

- 工具链：**只用 MSVC**。`g++` 仅限 `scripts/check-core-linux-smoke.sh`
  这类「平台无关核心」冒烟，不是构建方式。
- 改了 `CMakeLists.txt` 的 include 目录 / target 列表 / 文件集合，
  必须**重新 configure**，否则 vcxproj 不会刷新（踩过：exe 不重建）。
- 本机安装 `C:\Program Files\AireRecord\airecord.exe` 会通过服务
  watchdog 拉起并抢占 UI 单实例；**验证新构建前**先
  `Stop-Service AireRecord` + 结束所有 `airecord`/`airecordd` 进程。
- `LNK1104: 无法打开 build\Release\airecord.exe` = 该 exe 正在运行，
  停进程再链。

---

## 3. 必跑门禁（提交前全绿，CI 也会跑）

| 门禁 | 命令 | 通过判据 |
|---|---|---|
| core 零平台引用 | `powershell -File scripts/check-core-clean.ps1` | `PASSED (<n> core files, 0 platform references)` |
| 公共头自包含 | `cmake --build build --target check-public-headers` | 无 `Self-containment check FAILED` |
| 全量构建 | `cmake --build build --config Release` | 退出 0，无 `error`/`LNK` |
| 全部测试 | `ctest --test-dir build -C Release` | `100% tests passed`（当前 28/28） |
| x86 一致性 | `cmake --build build-x86 --config Release --target airecord airui_textbox_lab` | 退出 0 |

- CI **阻塞合并**的门禁只有三个 job：`full build (windows / VS2022 Win32)`、
  `airui-core (windows / VS2022)`、`airui-core (${{ matrix.os }})`（ubuntu / macOS）。
- `ASan (windows / VS2022, non-blocking)` 与 `Skia backend (windows / VS2022
  + vcpkg)` 均带 `continue-on-error: true`（`ci.yml:381`、`:440`），失败**不阻塞
  合并**，也不可作为「CI 已验证」的依据。脚本门禁（core-clean / 公共头自包含）
  由上述三个 job 内部真实调用。
- `airui-core (windows)` 里 `makensis failed (rc=1)` 是**已知的
  best-effort NSIS 步骤**（`continue-on-error`），作业仍为 success，
  不作为回归信号。

---

## 4. 代码与提交约定

### 4.1 源码风格
- 缩进 4 空格、LF 换行、UTF-8 无 BOM（`.editorconfig` 已定义，勿改）。
- 注释用**中文**写「为什么」，标识符用英文；不要写「显然如此」的废话注释。
- 沿用现有命名：`PascalCase` 类型 / `camelCase` 方法 / 平台层成员 `m_xxx`。
- 不要引入新第三方依赖；不要动 `third_party/`。
- 自绘控件**禁止缓存调色板颜色**：`WM_PAINT` 里读 `Pal()` / `Mx()`，
  否则暗色切换与 DPI 变化会失效。新增自绘控件必须遵守。

### 4.2 改动范围
- 只改与任务直接相关的文件；顺手重构请单开一轮并先说明。
- **不要**动未跟踪文件（如仓库根的 `query`），也不要把它加进提交。
- `build*/`、`build-cmake/` 等产物目录已被 gitignore，勿手工 add。

### 4.3 提交
- 每个逻辑单元一个提交；**代码与 `docs/ROADMAP.md` 分开提交**（沿用
  本轮既有习惯：`<type>: <代码改动>` 与 `docs: ROADMAP 跟进<N> …`）。
- 提交信息用英文正文，说明「改了什么 / 为什么 / 实测了什么」；
  禁止写入未实测的数字与结论。
- 推送前先 `git status --short` 确认只有预期文件。

---

## 5. 平台层剥离（进行中的方向）

### 5.1 已定规则
- **对外头文件不能有平台相关的数据结构和 API**，包括类型、函数、宏。
- 句柄（`HWND`/`HDC`/`HFONT`/`HBITMAP`/`HICON`/`HINSTANCE`/`HBRUSH`/
  `HPEN`/`HGDIOBJ`/`HMENU`/`HCURSOR`）视为平台类型，不得出现在公开签名。
- 消息常量（`WM_*`）、绘制标志（`DT_*`）、控件通知（`LB_*`/`EN_*`/`BN_*`/
  `CB_*`/`EM_*`）、修饰键（`MK_*`）等宏同样不得出现在公开签名。
- 公开头必须自包含，且**不 include `<windows.h>`**。

### 5.2 架构
```
src/airui/core/include/airui/      ← 公开面（平台无关）
    types.h        Rect / Color / Point / Size / TextFormat(dt::*) /
                   平台中立句柄别名 / 消息字别名 / vkey::* / mb::*
    …语义类型（不透明句柄、消息常量、绘制项描述）在此新增
src/airui/core/src/                ← 公开面的实现（平台无关）

src/airui/platform/win32/airui/    ← Win32 后端的公开面（同样平台无关）
    AirUi.h Canvas.h Controls.h DrawCall.h Fonts.h Page.h Shell.h
    Theme.h ThemeJson.h ThemeManager.h Responsive.h Accessibility.h …
src/airui/platform/win32/internal/ ← 私有实现（可出现 Win32 全部类型）
    CanvasPainter.h Components.h Controls.h Gfx.h GdiPainter.h
    OverlayWindow.h PaintFrame.h RaII.h TestShell.h Win32Window.h …
```
- 私有目录**不出现在**任何 target 的 `PUBLIC` include 中；
- 公开层内部存 `void*` / 自有 POD，转换只在私有实现处发生；
- `platform/win32/internal/*.h` 现在是**转发头**（`#include "airui/XXX.h"`），
  旧的 Win32 签名实现留在同名 `.cpp` 里。

### 5.3 现状基线（2026-11-26 传递闭包零平台 token，0 豁免）
**公开头及其 include 闭包已全部零平台痕迹，门禁 0 豁免强制**（39 个受检头：
core 26 + `win32/airui` 13）。`scripts/public-headers-exempt.txt` 只剩注释，
无豁免行。闭包覆盖 214 个文件、0 个平台 token。

**留在公开面（`win32/airui/`）的头**（自身与闭包都零平台 token）：
`AirUi.h`（总入口）、`Canvas.h`、`Controls.h`、`DrawCall.h`、`Fonts.h`、
`Page.h`、`Shell.h`、`ThemeJson.h`、`ThemeManager.h`、`Responsive.h`、
`Accessibility.h`、`PaintProviders.h`。

**移出公开面到 `platform/win32/internal/`**（私有头，门禁不检查）共 15 个：
- 批次 2：`GdiPainter.h` / `RaII.h` / `Win32Window.h` / `TestShell.h` /
  `OverlayWindow.h`
- 批次 5：`Canvas.h` / `CanvasPainter.h` / `Components.h` / `Controls.h` /
  `Gfx.h` / `ObservableListBox.h` / `Page.h` / `PaintFrame.h` / `Shell.h` /
  `Widgets.h`

其中 `Canvas.h` / `Controls.h` / `Page.h` / `Shell.h` / `Widgets.h` 在批次 6
被**重写为平台无关的公开版**，同名 internal 头改成转发头
（`#include "airui/XXX.h"`），旧签名实现留在同名 `.cpp`。

**批次 6 引入的公开词汇（`core/include/airui/types.h`）**：
- 句柄别名（全是 `void*`）：`WindowHandle` / `DeviceContextHandle` /
  `FontHandle` / `BrushHandle` / `PenHandle` / `BitmapHandle` /
  `GdiObjectHandle` / `InstanceHandle` / `IconHandle` / `CursorHandle` /
  `MenuHandle`。
- 消息字别名：`MessageWParam` / `MessageLParam` / `MessageResult` /
  `SubclassId` / `SubclassRef` / `MessageId` / `VirtualKey` /
  `MessageBoxFlags`。
- 标志命名空间：`vkey::*`（虚拟键）、`mb::*`（消息框）、`dt::*`（文本标志）。
  **`dt::*` 的取值与 Win32 SDK 的 `um/winuser.h` 逐位对齐**——改这些常量前
  先查本机 SDK（`/c/Program Files (x86)/Windows Kits/10/Include/*/um/winuser.h`），
  不要凭记忆：`DT_RIGHT=0x2`、`DT_BOTTOM=0x8` 与「位序递增」的直觉不同，
  猜错会让 Skia 的 `Horizontal()` 把居中判成右对齐。
- 中立 POD：`Rect`（含 `Offset` / `Inflated` / `Intersect` / `Union` /
  `Contains`）/ `Point` / `Color` / `TextFormat` / `DateTime` /
  `DrawItemInfo` / `MeasureItemInfo` / `MessageInfo`。

- **传递包含的坑**：清零一个公开头后，所有**依赖它传递提供平台类型**的
  `.cpp` 都会断链，必须在那些 `.cpp` 里显式 `#include <windows.h>`
  （如 `ThemeJson.cpp` / `Page.cpp` / `Gfx.cpp`）。**每清一个头都要 grep 一遍
  它的 includer 列表**。
- `Canvas.cpp` **只部分迁移**：基础绘制（Fill / Stroke / Text 等）仍直调
  `gfx::*`；只有 `StatusTag` / `KbdChip` / `EmptyState` / `Pagination` /
  `Button` / `Checkbox` / `Radio` 走 core `Paint*` + `CanvasPainter`。
- **边界转换的位置**：`internal/CanvasPainter.h` 有 `ToRect` 的双向重载；
  各 `.cpp` 自己在平台边界做 `reinterpret_cast<HDC>` / `<HWND>` /
  `<HBITMAP>` / `<HFONT>`。**平台类型只允许出现在 `.cpp` 与 internal 头**。
- core 侧已落地：`types.h` 两条 `static_assert`（仅守 size，未守 `offsetof`
  / 字段类型）+ 平台中立别名 `WindowHandle` / `FontHandle` / `MessageWParam` /
  `MessageLParam` / `MessageResult` / `DateTime` / `Rect` / `Color` /
  `TextFormat` / `dt::*`。前 5 个已被 `Accessibility.h` / `Fonts.h` 使用；
  几何/颜色别名在 `DrawCall.h` / `ThemeJson.h` 使用。它们是清零公开头的
  公共词汇，优先复用而非各自定义。

### 5.4 推进顺序（全部完成）
已完成（一步一验证）：
- 批次 1 清零：`DrawCall` / `ThemeJson` / `Responsive`。
- 批次 2 移到 internal/：`GdiPainter` / `RaII` / `Win32Window` /
  `TestShell` / `OverlayWindow`。
- 批次 3 清零：`Accessibility`（句柄别名 + 边界转换）。
- 批次 4 清零：`Fonts`（`FontHandle`，连带 `Gfx` / `Controls` font 参数）。
- 批次 5：10 个 Win32 实现头整体移到 internal/——`Canvas` /
  `CanvasPainter` / `Components` / `Controls` / `Gfx` / `ObservableListBox` /
  `Page` / `PaintFrame` / `Shell` / `Widgets`；`AirUi.h` 注释清零、include
  改 `internal/` 路径，传递链不断（26 个文件的 include 同步更新）。
- 批次 6（2026-11-26，传递闭包门禁驱动）：把 `Canvas` / `Controls` /
  `Page` / `Shell` / `Widgets` **重写为平台无关的公开版**并提升回
  `platform/win32/airui/`，internal 同名头改为转发头；`Gfx.h` 不提升
  （它是 GDI 原语），`gfx::OneLine` 迁到公开 `DrawCall.h`；门禁从「只查文件
  自身」升级为「递归查 quoted include 闭包」。连带把消费层
  （`src/pages` / `src/gallery` / `examples` / `src/core` / `src/plugins`）
  全部改成中立类型。

**现状：公开头零豁免、闭包零平台 token、门禁全绿。** 后续若要做真正技术
隔离，才需要把 PUBLIC include 收窄到 `win32/airui`，并全仓改
`airui/XXX.h` → `XXX.h`（数百处 include，单独批次）。新增页面作者 API 头时
必须保持零平台 token，**且必须让 `AirUi.h` 的闭包仍然零平台 token**。

### 5.5 门禁升级（已实现，2026-11-26，批次 6 升级为闭包检查）
- `scripts/check-public-headers-clean.sh` / `.ps1` **已存在并接入 CI**，
  且**已从「只查文件自身」升级为「递归查 include 闭包」**：
  - 入口：`core/include/airui/*.h` + `platform/win32/airui/*.h`；
  - 解析 `#include "..."`（**不展开 `<...>`**，系统头不算平台污染），
    用「当前头所在目录 / `core/include` / `platform/win32`」三个候选解析路径，
    队列 + 集合去重遍历闭包；
  - 对闭包里每个文件按文本扫描平台类型 / 函数 / 宏与 `windows.h`；
  - 失败输出 `FAIL <入口头> -> <被污染文件>:<行号>: <命中片段>`，
    最后一行汇总 `FAILED (n platform references across m files in public
    include closures)`；
  - 通过输出 `PASSED (39 public headers checked, 0 exempt, N files in
    closures, 0 platform references)`。
  本地 target：`cmake --build build --target check-public-headers-clean`。
- **豁免表** `scripts/public-headers-exempt.txt` **已清零**（只剩注释行）；
  当前 39 个公开头全部受检、0 违规、0 豁免。豁免语义：入口整条闭包跳过
  检查——只加一行 basename 就等于放行一整条链，所以豁免是欠账不是常态，
  必须同步登记到 ADR / 工作日志并尽快清零。
  `platform/win32/internal/` 下的私有头不检查（不算公开面）。
- **门禁自检（改完门禁脚本必须做）**：往一个**被传递包含**的头里注入平台
  token（例如在 `core/include/airui/types.h` 末尾追加 `// probe: HWND`），
  确认门禁**失败**并打印出 `入口 -> 文件:行` 的链路，再还原并确认恢复通过。
  只在「自身文本」上验证过是不够的——那正是旧门禁漏掉污染的原因。
- 现有 `CMakeLists.txt` 的 `check-public-headers` 只做**自包含编译检查**
  （`#include` 单个头能否独立编译），与平台纯净门禁互补，别混淆。
- README / `build-conventions.md` 需在后续批次同步说明。

---

## 6. 验证纪律（本仓库反复踩坑的总结）

1. **先实测再写文档**：任何数字（行数、耗时、差异像素、PASS 数）都必须
   来自本次真实运行，禁止推断或复述旧数据。
2. **区分「我改了」与「我验证了」**：写完代码不等于通过；必须跑门禁并
   贴出真实输出。
3. **警惕陈旧产物**：`airecord.exe` 被占用会导致 `LNK1104` 而**构建成功
   字样仍不出现**；改完代码先确认目标 exe 的 mtime 更新，再谈行为。
4. **测试隔离**：验证前 `Stop-Service AireRecord` 并清理
   `airecord`/`airecordd`；否则服务 watchdog 拉起**已安装版本**，
   你测到的不是新构建。
5. **跨进程限制**（已实测）：
   - 合成键盘（`SendInput` / 裸 `WM_KEYDOWN`）**无法可靠**送达跨进程子窗口，
     依赖 `GetKeyState` 的 Ctrl/Shift 组合不可测；改用消息里携带的
     `wParam` 修饰位或菜单路径。
   - 合成 `WM_DPICHANGED` 不可投递（`PostMessage`/`SendMessage` 均失败），
     需用共享处理体的测试消息驱动；`lParam` 指针跨进程无效，传 0。
6. **中文文本传输风险**：脚本/源码中含中文时，本代理的生成过程曾多次混入乱码
   token。写入后**必须回读校验**（`Read` 或 `git diff`），发现乱码立即修正
   或回退，不得留在仓库。

---

## 7. 不要做的事

- 不要用 `g++`/MinGW 构建本项目。
- 不要在 `core/` 里 include `<windows.h>` 或提及 Win32 类型名（含注释）。
- 不要为“让编译通过”而删测试、改测试预期、放宽门禁脚本、或加 `#ifdef` 挡板。
- 不要把平台类型经由公开头泄漏给页面（包括 `typedef`、默认参数、模板实参）。
- 不要为了「让 `.cpp` 好写」而把平台类型塞回公开签名；边界转换写在 `.cpp` 里。
- 不要在公开头里写平台 token 的**注释**（core 门禁与闭包门禁都是文本匹配，
  注释里写 `DT_RIGHT`、`MSG` 一样会失败）。
- 不要在未确认基线与范围时一次性大改（本轮已因生成乱码返工多次）；
  大改动先列**文件清单 + 每处签名前后 + 转换点**，再落笔。
- 不要提交 `build*` 产物、临时探针源码、`query` 等未跟踪文件。
- 不要在提交信息里写“已验证”却未附实测输出。

---

## 8. 当前基线（2026-11-26 复核；HEAD 会前移，引用前先 `git log -1` 确认）

- `HEAD` = `ea06d01`（`docs: add AGENTS.md ...`，在 `d50e2c9` 之上）。
- 标签（commit 指向，用 `git rev-parse <tag>^{commit}` 解析，annotated tag
  的 `show-ref` 对象号与 commit 号不同）：`pre-layered-api` = `d50e2c9`
  （平台层剥离前的基线）、`v0.9.0`、`v0.8`、`phase0-baseline`。
- 已交付第 1/2/3 项：TextBox 撤销/重做、maxLength/password 透出、
  高 DPI 动态适配 + 列表多选 + 分页（详见 `docs/ROADMAP.md` 跟进六~九）。
- 门禁状态：core-clean PASSED（55 文件 / 0 引用）、公共头纯净门禁 PASSED
  （**39 受检 / 0 豁免 / 闭包 214 文件 / 0 平台引用**，`.sh` 与 `.ps1` 两版
  一致）、公共头自包含 PASSED、全量构建 0 错误、ctest **28/28**。
- 平台层剥离（§ 5）**公开头阶段已全部完成**：批次 1~6 落地——`types.h` 的
  static_assert + 平台中立别名（含 `vkey::*` / `mb::*` / `dt::*`）、4 个头
  清零签名（Responsive/ThemeJson/DrawCall/Accessibility/Fonts）、15 个 Win32
  实现头移到 internal/（批次 2 的 5 个 + 批次 5 的 10 个）、**批次 6 把
  `Canvas` / `Controls` / `Page` / `Shell` / `Widgets` 重写为平台无关公开版
  并提升回 `win32/airui`**，门禁升级为 include 闭包检查，消费层全部改中立类型。
  `AirUi.h` 现在的 include 闭包**零平台 token**，不再传递引入 internal 头。
  `Geometry.h` 从未创建；`Canvas.cpp` 仍只部分函数迁移。
  本轮平台无关化改动**已提交**（`git log --oneline -3` 查最近提交）；
  工作区当前只剩未跟踪的 `query`（按 § 4.2 不得提交）。
