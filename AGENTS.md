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
- 最高优先级约束：**公共头文件不含任何平台相关类型 / API / 宏**，
  以及 **core 层零平台引用**（两条都已有门禁脚本，必须保持绿）。

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
- 当前状态（截至 `pre-layered-api`）：**尚未达成**，`platform/win32/airui/`
  全是 Win32 类型。改造方向见「§ 5 平台层剥离」。
- 允许在**私有实现侧**出现：`*_impl/`、`src/airui/platform/<backend>/src/`、
  以及 `.cpp` 里；但私有头**不得**从 `AirUi.h` 或公开头被 include。
- Win32 类型仅允许以**不透明 `void*`** 穿过公开边界
  （如 `Canvas(void* nativeDc, …)`、`void* Native()`）。

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

- CI 四个真实门禁：`full build (windows / VS2022 Win32)`、`airui-core`
  （win/mac/ubuntu）、`ASan`、`Skia backend`。
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
    types.h        Rect / Color / Point / Size / TextFormat(dt::*)
    …语义类型（不透明句柄、消息常量、绘制项描述）在此新增
src/airui/core/src/                ← 公开面的实现（平台无关）

src/airui/platform/win32/          ← 私有实现（可出现 Win32 全部类型）
    src/…                          ← 实现 .cpp/.h
    airui/win32/Geometry.h         ← 唯一的边界转换层（已有）
```
- 私有目录**不出现在**任何 target 的 `PUBLIC` include 中；
- 公开层内部存 `void*` / 自有 POD，转换只在私有实现处发生。

### 5.3 现状基线（截至 `pre-layered-api`，需逐项归零）
公开头中的平台痕迹实测：
- 类型 **563 处**、函数调用 **90 处**、宏 **37 个**，涉及 **21 个公开头**。
- 已干净：`AirUi.h`、`PaintProviders.h`、`Responsive.h`、`ThemeManager.h`。
- 已改为平台无关（本轮已完成）：`Canvas.h`（签名用 `Rect`/`Color`/
  `TextFormat`/`void*`），边界转换在 `win32/Geometry.h`；
  `Canvas.cpp` **函数体尚未迁移完毕**，不要以为它已完工。
- 其余 17 个仍含平台类型，按 5.4 顺序推进。

### 5.4 推进顺序（每步独立可验证，一步一提交）
1. `Canvas`（进行中）——公共签名零平台类型；`.cpp` 内转换。
2. `DrawCall` / `ThemeJson` / `ObservableListBox` ——纯数据结构，影响面小。
3. `Components` / `Widgets` / `Page` ——控件与页面基类。
4. `Controls` ——若无法做到零句柄，允许保留为「平台控件层」，但**必须**在
   `AirUi.h` 之外，由使用者显式 include，且文档写明其平台相关。
5. `Shell` / `OverlayWindow` / `Win32Window` / `Accessibility` / `Fonts`
   ——窗口壳与世界面 API，最后处理；同样移出 `AirUi.h` 或改纯接口。

### 5.5 门禁升级（与 5.4 同步做）
- 新增 `scripts/check-public-headers-clean.ps1`：对「公开头清单」逐个扫描
  平台类型 / 函数 / 宏与 `windows.h`，命中即失败。
- 清单来源建议：`core/include/airui/*.h` + 显式维护的白名单文件；
  移出公开面的头不再列入。
- 接入 CI 与本地 `check-*` 目标；README / `build-conventions.md` 同步说明。

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
- 不要在未确认基线与范围时一次性大改（本轮已因生成乱码返工多次）；
  大改动先列**文件清单 + 每处签名前后 + 转换点**，再落笔。
- 不要提交 `build*` 产物、临时探针源码、`query` 等未跟踪文件。
- 不要在提交信息里写“已验证”却未附实测输出。

---

## 8. 当前基线（本文件生成时）

- `HEAD` = `d50e2c9`（在 `80c2463` 之上：`core: static_assert Rect/Color binary layout`）。
- 标签：`pre-layered-api`（= `d50e2c9`，平台层剥离前的基线）、
  `v0.9.0`、`v0.8`、`phase0-baseline`。
- 已交付第 1/2/3 项：TextBox 撤销/重做、maxLength/password 透出、
  高 DPI 动态适配 + 列表多选 + 分页（详见 `docs/ROADMAP.md` 跟进六~九）。
- 门禁状态：core-clean PASSED、公共头自包含 PASSED、全量构建 0 错误、
  ctest 28/28。
- 进行中：平台层剥离（§ 5）。工作区存在**未完成的**临时文件
  `src/airui/platform/win32/airui/win32/Geometry.h`，以及被还原回
  原状的 `Canvas.h` / `Canvas.cpp`；恢复/提交前先按 § 7 核对。
