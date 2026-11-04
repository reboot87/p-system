# Phase 0 实施计划：核心骨架 + 控件先行

状态：评审稿 v0.1  
日期：2026-11-05  
前置：`docs/cross-platform.md`（总体方案 v0.1）已评审通过  
本文档：回答「阶段 0 到底做哪些事」——**把控件实现作为中心议题**（按评审意见）

---

## 0. 一句话结论

阶段 0 做两件事，不做一件事：

| 做 | 不做 |
|---|---|
| **A. 骨架**：目录/目标拆分、`core/` 平台无关化、门禁 CI 落地 | 不引入第三方依赖（不动 Skia、不改渲染器） |
| **B. 控件先行**：定稿控件 HAL（状态机在 core，渲染/原生窗口在 backend），并把「纯自绘」控件直接迁入 core | 不重写 TextBox/ListBox 行为，Windows 上保持原生实现，零行为变化 |

> 「最主要的应该是控件的实现」——本文档 §2–§4 是主体：先给出现有控件的全景盘点，再给出控件 HAL 设计（状态机进 core、平台只负责画+收事件），最后给分步实施顺序与退出标准。

阶段 0 的验收不是「跨平台跑通」（那是阶段 1/2 的事），而是：

1. `airui_gallery` 在 **Windows 上行为完全不变**（截图 Diff = 0）
2. `core/` 目录 grep 不到任何平台头/句柄（CI 强制）
3. 控件 HAL 接口定稿并评审通过（这是后续所有后端工作的契约）

---

## 1. 原阶段 0 的边界与本文档的修订

原阶段 0（总体方案）：物理移动文件 + 剥离 `windows.h` + `RECT→Rect` 过渡 typedef + `IClock` 注入 + 测试通过。

**评审修订**：直接把「控件的实现方式」提到阶段 0 的主线，原「机械搬迁」降级为步骤 0。原因：

- 控件是核心代码里**平台耦合度最高**的部分（928 行 `Widgets.cpp` + 部分 `Controls.cpp` ≈ 1200 行里 ~1000 行是平台代码），如果不先想清楚、定稿 HAL，阶段 1 必然返工。
- 画廊已经证明了方向：Checkbox / Radio / Button / Card **本来就是纯 Canvas 自绘，没有 HWND**——这意味着「自绘控件进 core」可以在阶段 0 就完成，而不是留到阶段 2。

修订后的阶段 0 范围：

```
阶段 0（本文档）
├─ 步骤 0   冻结基线（tag + 截图 + DrawCall 录制 dump）
├─ 步骤 1   core/ 与 platform/win32/ 目录建立、文件迁移（内容不变）
├─ 步骤 2   平台无关类型集（Rect/Point/Size/Color/TextFormat）+ 门禁 CI
├─ 步骤 3   控件 HAL 定稿：状态机进 core，渲染/消息进 backend（本文档 §3–§4）
├─ 步骤 4   纯自绘控件迁入 core：Button / Checkbox / Radio / Section(标签行)
├─ 步骤 5   CMake 拆分：airui-core（platform-free）/ airui-win32 / airui_gallery
├─ 步骤 6   测试与回归：DrawCall 录制前后对齐；test_ui / gallery 冒烟
└─ 步骤 7   提交 + Linux 编译烟雾测试（只需能编译 airui-core，不能运行）
```

---

## 2. 控件全景盘点（决策依据）

AirUI 里「控件」分三类，跨平台处置完全不同：

### 2.1 第一类：纯 Canvas 自绘（目标：阶段 0 进 core）

| 控件 | 实现 | 平台依赖 | 处置 |
|---|---|---|---|
| `Canvas::Button` 的视觉 | `Canvas::Button()` → `DrawButton` 的 GDI 版 | HDC/HFONT | 迁 `core/`，画 `IPainter` |
| 画廊 Checkbox / Radio | `FillRound + Stroke + Text` 直接绘制 | 无 | 迁 `core/`（已是正确的样子） |
| `Card` 组件 | `Card::Paint()` 纯 Canvas | 无 | 迁 `core/` |
| `ListBox` 的行绘制（owner-draw） | `onPaintItem` 回调 | 半 | 回调签名进 core，渲染后端执行 |
| `EmptyState` / `StatusTag` / `KbdChip` | Canvas 复合 | 无 | `core/` |
| `Layout` 行/列/网格 | 纯几何 | 无 | `core/` |

**这一类今天就已经全部可 port，只差类型替换。**

### 2.2 第二类：原生子窗口 + 自绘表面（目标：Windows 保持，Skia 后续重写）

| 控件 | 实现细节 | 平台依赖 | 主要难点 |
|---|---|---|---|
| `Button` | `BUTTON` + `BS_OWNERDRAW`；`WM_DRAWITEM` → Shell 路由 → `Button::HandleDrawItem`；hover 走 `GetPropW` | HWND/WM_DRAWITEM/GetPropW | owner-draw 的焦点矩形/键盘可用性 |
| `ListBox` | `LISTBOX` + `LBS_OWNERDRAWVARIABLE` + `LBS_NOINTEGRALHEIGHT`；`WM_MEASUREITEM`/`WM_DRAWITEM` 路由；hover 由 `ListBoxProc` 子类跟踪 + `LB_GETITEMRECT` | HWND/LB_* 消息/子类 | 可变行高 + 原生滚动条 + 原生键盘选中 |
| `Custom` | `STATIC` + 子类，`HTTRANSPARENT` 鼠标穿透 | HWND |  trivial |

### 2.3 第三类：原生深包装（目标：Windows 保持，Skia 分步重写）

| 控件 | 实现细节 | 平台依赖 | 主要难点 |
|---|---|---|---|
| `TextBox` | `EDIT` 子窗口 + `PlaceholderEdit`（占位提示：子类化 `WM_PAINT` + `WM_IME_START/ENDCOMPOSITION` + `SetPropW` 组词标记）+ `EM_SETMARGINS` + `OnKey` 回调 | HWND、IME 消息、`GetKeyState` | **整个库里最难移植的控件**（见 §6 R-1） |

### 2.4 关键结论

1. **画廊里最「难看」的部分（自绘 Checkbox/Radio）反而是最先跨平台的**；
2. **TextBox 不是阶段 0 要碰的**——它需要 IME/组词/选区/剪贴板，任何自研实现都是周级工作，Windows 继续用 `EDIT`；
3. **ListBox 的行渲染在 core**，滚动/选中逻辑要和后端分离：core 持有数据与命中逻辑，backend 持有滚动条/键盘/绘制。

---

## 3. 控件 HAL 设计（阶段 0 定稿，阶段 1/2 实施）

### 3.1 分层：core 持有「状态机 + 布局 + 交互」，backend 持有「原生窗口 + 像素 + 事件源」

```
┌──────────────────────────── AirUI Core ───────────────────────────┐
│  struct ControlState { hover, pressed, focused, checked, disabled }│
│  class Control                                                     │
│    RECT → Rect rect; ControlState st; bool visible/enabled;        │
│    virtual void Paint(IPainter& g) = 0;     // 纯逻辑绘制           │
│    virtual void OnPointerMove/Down/Up/Wheel(…) = 0;                 │
│    virtual void OnKey(...)/OnFocus()/OnBlur()                       │
│  struct ListData { std::vector<Item{height,data}> items; int sel;   │
│                    std::vector<int> heights; }                      │
│  class ListControl : Control  ← 选中/hover/滚动/可变行高逻辑        │
│  class Checkbox / Radio / Button / CustomChip : Control             │
├──────────────────── IWindow / IPainter / IFont ────────────────────┤
│  class IWindow { PumpOne(); On(MouseMove/Wheel/Key/Resize); … }     │
│  class IPainter { Save/Restore/Clip/Translate/Fill*/Text/… }        │
│  class IFont { MeasureWidth/LineHeight/Select }                     │
└───────────────────── Backends ─────────────────────────────────────┘
```

**要点**

- `Control` 不认识 `HWND`/`HDC`。它知道：自己的 `Rect`、状态位、主题色/指标（`Pal()`/`Mx()`）、要画什么文本/字形。
- `Control::Paint(IPainter&)` —— 画家是谁（GDI/Skia）不重要。
- 事件由 `IWindow` 注入（鼠标/键盘/wheel/resize/focus）；`Control` 消费后返回「是否已处理」，未消费的事件回落到 `Shell`。
- 命中测试完全在 core（`Rect::Contains` + Z-order）。

### 3.2 接口草图（阶段 0 评审用，签名不冻结）

```cpp
// core/hal.h
struct Rect  { float x, y, w, h; };
struct Point { float x, y; };
struct Color { uint32_t rgba; };
enum class FontRole { Body, Meta, Title, Section, Display, Button, Icon, IconLarge };
enum class TextAlign { Left, Center, Right };

class IFont {
public:
    virtual ~IFont() = default;
    virtual float MeasureWidth(const std::wstring& s, FontRole role) = 0;
    virtual float LineHeight(FontRole role) = 0;
    virtual void  SetRole(FontRole role) = 0;
};

class IPainter {
public:
    virtual ~IPainter() = default;
    virtual void Save() = 0;
    virtual void Restore() = 0;
    virtual void Clip(const Rect& r) = 0;
    virtual void Translate(float dx, float dy) = 0;

    virtual void Fill(const Rect& r, Color c) = 0;
    virtual void FillRound(const Rect& r, Color c, float radius) = 0;
    virtual void Stroke(const Rect& r, Color c, float radius, float width) = 0;
    virtual void HLine(float x1, float x2, float y, Color c) = 0;
    virtual void VLine(float x, float y1, float y2, Color c) = 0;
    virtual void Text(const Rect& r, const std::wstring& s,
                      FontRole role, Color c, TextAlign a) = 0;
    virtual void TextBlock(const Rect& r, const std::wstring& s,
                           FontRole role, Color c, int maxLines) = 0;
};

class IWindow {
public:
    virtual ~IWindow() = default;
    virtual void SetTitle(const std::wstring&) = 0;
    virtual void SetMinSize(int w, int h) = 0;
    virtual void Show() = 0;
    virtual int  PumpOne() = 0;      // 或 RunLoop
    virtual void RequestRedraw() = 0;
    // 事件注入（core 注册，backend 触发）
    virtual void OnMouseMove(std::function<void(Point)>) = 0;
    virtual void OnMouseDown(std::function<void(Point, int button)>) = 0;
    virtual void OnMouseUp(std::function<void(Point, int button)>) = 0;
    virtual void OnWheel(std::function<void(Point, float delta)>) = 0;
    virtual void OnKeyDown(std::function<void(int vk, bool ctrl, bool shift)>) = 0;
    virtual void OnResized(std::function<void(Rect content)>) = 0;
};
```

### 3.3 每个控件的处置（冻结）

| 控件 | 阶段 0 | 阶段 1（Win32 HAL 化） | 阶段 2（Skia + Linux） |
|---|---|---|---|
| Button（自绘） | core 状态机定稿；Windows 保持 `BS_OWNERDRAW` 路由 | 画 `GdiPainter`；状态机复用 | 画 `SkiaPainter`；**同一份 core 代码** |
| Checkbox / Radio（自绘） | **阶段 0 直接迁入 core** | 同 Button | 同 Button |
| Custom（STATIC+子类） | core `CustomControl` 状态机定稿 | 保持 STATIC 或改为无窗口自绘（推荐后者） | 无窗口自绘 |
| Card / DataGrid / EmptyState 等组件 | core（本就是 Canvas 组合） | 同现状 | 同现状 |
| ListBox | core `ListData`（数据/选中/hover/滚动/可变行高）定稿 | 保持 `LISTBOX` + owner-draw，backend 做 `ListData↔LB_*` 同步 | **全自绘 ListView**（滚动条/键盘在 core） |
| TextBox | core 接口定稿：`IEditControl`（Text/Selection/Caret/IME 钩子） | 保持 `EDIT` + `PlaceholderEdit`，包一层 `IEditControl(win32)` | 全自绘 EditBox：v1 基础输入（无 IME）→ v2 IME 组词 → v3 选区/剪贴板/撤销 |
| NavBar / StatusBar / Caption（标题栏） | core 纯状态 + `IPainter` | `GdiPainter` 画 | `SkiaPainter` 画 |

**为什么 ListBox 要 core，TextBox 不能先 core**：ListBox 的核心语义是「索引 + 高度数组 + 选中 + hover」，数学与平台无关；TextBox 的语义是 IME 协议、组词 caret、多行滚动、系统级剪贴板/选区——平台必须承办，自研是周级成本，阶段 0 明确不做。

---

## 4. 目录与构建目标（阶段 0 定稿）

```
src/airui/
├─ core/                    ← 不允许任何平台头（CI 门禁）
│  ├─ include/airui/*.h     ← 现有公共头迁入（含新 hal.h / types.h）
│  └─ src/*.cpp             ← Layout/Theme/ThemeJson/Responsive/Animation/
│                              Observable/Components/Interaction/ControlsCore
├─ platform/
│  ├─ win32/                ← 唯一允许 windows.h 的目录
│  │  ├─ GdiPainter.cpp  GdiFonts.cpp  Win32Window.cpp  Win32Widgets.cpp
│  │  └─ DwmCaption.cpp  PlaceholderEdit.cpp  ShellWindow.cpp
│  └─ skia/                 ← 阶段 2 才建（阶段 0 仅占位文档说明）
└─ tests/                   ← TestShell、DrawCall 断言，不含平台头
```

`CMakeLists.txt`（阶段 0 拆分，source list 先镜像现状）：

```cmake
set(AIRUI_CORE_SOURCES core/src/Layout.cpp core/src/Theme.cpp …)
set(AIRUI_WIN32_SOURCES platform/win32/*.cpp)
add_library(airui-core STATIC ${AIRUI_CORE_SOURCES})
target_include_directories(airui-core PUBLIC core/include)
add_library(airui-win32 STATIC ${AIRUI_WIN32_SOURCES})
target_link_libraries(airui-win32 PUBLIC airui-core)
target_link_libraries(airui-gallery PRIVATE airui-win32)
```

**迁移映射表（节选，完整版见附录 A）**

| 现有文件 | 阶段 0 目标 | 内容 |
|---|---|---|
| `src/airui/src/Canvas.cpp` | `core/src/Canvas.cpp` | 不动（过渡期 RECT 仍指向 `types.h` 的 `using Rect = RECT`） |
| `src/airui/src/Layout.cpp` | `core/src/Layout.cpp` | 不动 |
| `src/airui/src/Components.cpp` / `Theme.cpp` / `Responsive.cpp` / `Animation.cpp`（注入 `IClock`） | `core/src/…` | 不动（Anim 时间源参数化） |
| `src/airui/src/Shell.cpp` | `platform/win32/ShellWindow.cpp` | 先文件名不动，仅改 include |
| `src/airui/src/Widgets.cpp` | `platform/win32/Win32Widgets.cpp` | 不动 |
| `src/airui/src/Controls.cpp` | `platform/win32/GdiControls.cpp` | 不动（`NavBar`/`DrawButton` 后续拆分） |
| `src/airui/src/Gfx.cpp` | `platform/win32/Gfx.cpp` | 不动 |
| `src/airui/src/Fonts.cpp` | `platform/win32/GdiFonts.cpp` | 不动 |

---

## 5. 分步骤实施清单

| # | 动作 | 产出 | 验证 |
|---|---|---|---|
| 0 | 冻结基线：`git tag phase0-baseline`；gallery 浅/深截图与 DrawCall dump 存档 | tag + files | — |
| 1 | 建立目录 + CMake 目标拆分（airui-core/airui-win32），switch `AIRUI_SOURCES` | 构建通过 | MSVC VS2017 + CMake Release |
| 2 | `types.h`：`Rect/Point/Size/Color/TextFormat`；`core` 公共头移除 `windows.h`；windows.h 只存在于 `platform/win32` | grep 门禁生效 | `scripts/check-core-clean.ps1`（见 §7） |
| 3 | 控件 HAL 头定稿（本文档 §3），`shell`/`canvas` 等待评审签字 | `hal.h` 评审稿 | 设计评审 |
| 4 | **控件先行**：Checkbox/Radio/Button 自绘版迁入 `core/src/ControlsCore.cpp`；画廊改用 core 控件（Win 行为不变——此时 backend 仍是 GDI，只是代码归属变化） | core 内控件 | DrawCall 录制 diff = 0 |
| 5 | `Animation` 时间源 → `IClock`；`DrawCall`、`TestShell`、测试移入 `core/tests` 或保持 Windows 可跑 | 无头测试平台无关 | `ctest` 通过 |
| 6 | `airui-core` 在 **Linux** 仅编译（`cmake -B build-linux`）：证明 core 无平台头 | CI 可跑 | 编译成功即可，不运行 |
| 7 | 文档：`docs/phase0-signoff.md`（变更清单 + diff 统计 + 退出标准打勾） | 签收单 | 本文档 §8 |

**刻意不做（阶段 0）**：TextBox/ListBox 行为修改、Skia 引入、IME、字体跨平台、`RECT` 真正消亡（过渡 typedef 保留，阶段 1 再删）。

---

## 6. 风险登记（控件专项）

| # | 风险 | 细节 | 缓解 |
|---|---|---|---|
| R-1 | **TextBox IME/组词** | `PlaceholderEdit` 依赖 `WM_IME_STARTCOMPOSITION/ENDCOMPOSITION`、`SetPropW` 组词标记、自绘占位字位置的 DT_TOP 基线对齐；任何复刻都必须复刻这套组词语义 | 阶段 0/1 Windows 原地重构包 `IEditControl`，行为 100% 保留；Skia 自研 EditBox 按 v1→v2→v3 分三步，见 §3.3；阶段 2 之前不对 TextBox 动刀 |
| R-2 | ListBox 可变行高 | `LBS_NOINTEGRALHEIGHT` + `LB_SETITEMHEIGHT` + `LB_GETITEMRECT` 滚动偏移；core 的 `ListData` 必须能表达「插入/删除后高度变化」 | core 用索引固定映射（items 不可变顺序仅 append/reset），windows 用 `LB_SETITEMDATA` 对照索引；重写时按现有字段一一对应 |
| R-3 | owner-draw 的 `WM_DRAWITEM`/`WM_MEASUREITEM` 路由 | Shell 拦截 `WM_COMMAND/WM_DRAWITEM/WM_MEASUREITEM` 再分派给控件；跨平台后「事件路由」必须在 core | 路由逻辑收进 `core/ShellCore::RouteMessage` 以接口形式，Win32 后端适配消息格式；core 不认识 `WM_*`——后端翻译成 `OnCommand/OnDrawItem` 回调 |
| R-4 | 焦点与 Tab 序 | 原生 EDIT/BUTTON/LISTBOX 的焦点/Tab 由系统管理；全自绘后焦点圈（focus ring）、Tab 遍历、IME 激活都要自己做 | core 定义焦点组件接口（active control + traversal 顺序来自页面创建顺序），阶段 1 实现 Win32（仍用原生 tabstop），阶段 2 自研 |
| R-5 | `GetPropW`/子类化链 | hover/tracking 与旧 hover id（0xA1/0xB1/0xC1…）混用 | 迁移时统一命名 `kSub*`，core 状态机接管后删 GetPropW |
| R-6 | 双后端行为漂移 | Button hover 颜色 GDI vs Skia 度量差 | DrawCall 录制逐项 diff + 截图 SSIM 阈值，进 CI |
| R-7 | 全自绘 ListBox 键盘滚动 | PgUp/PgDn/Home/End、选择跟随、滚动条命中 | core `ListData` 直接拥有滚动偏移/可视区/选中；命中测试复用 `Rect` |

---

## 7. 门禁（CI / 脚本）

```powershell
# scripts/check-core-clean.ps1（阶段 0 加入构建）
$leak = Select-String -Path "src/airui/core/**/*.h","src/airui/core/**/*.cpp" `
          -Pattern "windows\.h|HWND|HDC|HFONT|HBRUSH|DRAWITEMSTRUCT|WM_[A-Z]" -AllMatches
if ($leak) { $leak | ForEach-Object { Write-Error $_ }; exit 1 } else { exit 0 }
```

- 每次 PR/提交必须通过；
- `airui-core` 在 Linux 编译作为红线（`cmake -B build-linux -S . && cmake --build build-linux`）；
- Windows 段截图对齐：gallery light/dark 逐项像素 diff = 0 方可签收。

---

## 8. 退出标准（签收单）

- [ ] `core/` grep 无平台头/句柄/`WM_*`（§7 脚本通过）
- [ ] Windows 构建 `airui_gallery` 成功，启动冒烟通过，截图与基线 diff = 0
- [ ] Checkbox/Radio/Button 由 core 实现（backend 画），画廊交互状态行为与基线一致
- [ ] `airui-core` 在 Linux 编译成功（不运行）
- [ ] `ctest` 全绿（现有测试通过）
- [ ] 控件 HAL 评审稿签字（`docs/phase0-signoff.md` 记录）
- [ ] 未触碰 TextBox/ListBox 运行时行为（`git diff --stat` 核对）

---

## 9. 工时与排期

| 步骤 | 内容 | 预估 |
|---|---|---|
| 0–2 | 基线 + 目录/CMake/`types.h` + 门禁 | 2–3 天 |
| 3 | 控件 HAL 头编写 + 评审 | 2 天 |
| 4 | 自绘控件迁入 core + 画廊回归 | 2–3 天 |
| 5–6 | IClock、CMake Linux 编译验证 | 1–2 天 |
| 7 | 签收 + 文档 | 1 天 |

**合计 ≈ 1–2 周**（不含评审等待）。

---

## 10. 开放问题

1. `Custom`（画廊自绘提示 chip）是否在阶段 0 就去掉 HWND、改为纯自绘子控件？（建议：是——STATIC 子类换 core `CustomControl`）
2. `NavBar`/标题栏/状态栏是否纳入阶段 0 的 HAL 定稿？（建议：纳入 `ShellCore`，否则 §3.2 `IWindow` 缺少对应事件）
3. Linux 编译烟雾测试以哪个发行版为准？（建议：Ubuntu 22.04 + GCC 11）
4. 过渡 typedef `Rect = RECT` 是否允许泄漏到 Linux 编译期？（建议：基准类型 `Rect/Point/Color` 全新定义，Win32 后端内部转换；`RECT` 仅 Windows 源码用）

---

## 附录 A. 逐文件迁移清单（阶段 0，~35 个文件）

> 原 `src/airui/include/airui/` 公共头 → `src/airui/core/include/airui/`；原 `src/airui/src/*.cpp` → `core/src/` 或 `platform/win32/`。

**进 core（内容基本不动）**：
`Layout.*`、`Theme.h/.cpp`、`ThemeJson.*`、`Responsive.*`、`Observable.h`、`Components.h/.cpp`、`Animation.*`（`IClock` 参数化后移除 `GetTickCount64`）、`DrawCall.h`、`TestShell.h/.cpp`（保留 Windows 亦可运行，位置先定 `core/tests/`）、`Interaction.h`（新建：从 Shell/Widgets 收拢 hover/焦点/命中）、`ControlsCore.cpp`（新建：core 控件状态机）、`types.h`、`hal.h`。

**进 platform/win32/**：
`Shell.cpp`（拆 `ShellWindow.cpp` + `DwmCaption.cpp` + `Controls.cpp` 的窗口部分）、`Widgets.cpp`、`Controls.cpp`（拆 `GdiControls.cpp` + `DrawButton`/`NavBar`/`PaintDC`）、`Gfx.cpp`、`Fonts.cpp`、`Accessibility.cpp`、`Accessibility.h`、`Application.h`（访问权限后续定）。

**暂留原地（阶段 1 再处理）**：`AirUi.h` 聚合头按最终结构重写。

## 附录 B. 回滚计划

阶段 0 任一步骤失败：`git reset --hard phase0-baseline`。迁移为多个小提交（每步一个 commit），保证可逐段回滚；门禁 CI 失败即阻断合入。
