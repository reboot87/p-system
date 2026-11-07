# AirUI 跨平台化设计文档

状态：草稿 v0.1（仅设计，未实施）  
日期：2026-11-05  
作者：AirUI 组  
目标读者：AirUI 维护者、评审人、实施人员

> **实施状态补记（2026-11-07，Phase 1 完成后）**
> - 本文 §2.2 把 `DrawCall.h` / `TestShell` 列为「平台无关、必须复用的资产」，那是**迁移前评估**：实际二者位于 `platform/win32/`，`DrawCall.h` 直接 `#include <windows.h>` 并使用 `RECT / COLORREF / swprintf_s`，`TestShell` 依赖 `HDC / HBITMAP / Canvas`，均无法在 Linux / macOS 编译。
> - 现已在 `core` 新增平台无关的 `PaintRecorder`（`airui/PaintRecorder.h` + `core/src/PaintRecorder.cpp`）：把 `IPainter` 调用录成可比较、可 ASCII 序列化的 `PaintOp` 序列（`Matches` / `SequenceEquals` / `Diff` / `ToString`）。它统一取代各测试里的 ad-hoc 录制器，作为 **G5 跨后端一致性断言的金样基座**；`test_paint_recorder` 在 Windows / Linux / macOS 三平台 CI 产出同一份序列。
> - **Skia 后端已在 Windows 以 opt-in 方式接入**（`AIRUI_WITH_SKIA`，默认 OFF）：`SkiaPainter`（IPainter → SkCanvas）+ `SkiaSurface`（raster BGRA → DIB → AlphaBlend），`ShellOptions.skiaChrome` 可把 `ShellCore` 外壳改由 Skia 绘制。引入方式定案见 `docs/skia-backend.md`（vcpkg 首选 / `AIRUI_SKIA_ROOT` 预编译备选 / 不采用子模块源码编译）。**Linux / macOS 应用本体与 Skia 版 `IWindow` 仍未实施**。
> - §10 的「Skia 引入方式」已定案打勾；「Linux 窗口壳」等仍待评审。真 SDK 下的双后端序列断言由 `test_skia_backend` 与 `skia-windows` CI 作业（push/PR 自动触发，VS2022 + vcpkg）承担；**像素级 GDI vs Skia 截图 diff** 仍是计划项。
> - Phase 1 的三项 HAL 工作（`GdiPainter` / `Win32Window` 生产化、`IWindow` 完整事件注入、`Shell` 迁入 `ShellCore`）已完成，进展见 `docs/ROADMAP.md` §4。

---

## 0. TL;DR

把 AirUI 从「Win32 + GDI 的 Windows UI 库」重构为「三层架构的跨平台 UI 库」：

```
p-system（App）        airui_gallery（演示）
        │                      │
        ▼                      ▼
AirUI Core —— 平台无关：布局 / 主题 / 交互逻辑 / 组件语义
        │
        ▼
HAL（抽象接口） —— IWindow / IPainter / IFont / IClock
        │
        ▼
Platform Backend —— Windows(GDI/DWM) · Skia(Win/Linux/macOS) · ...
```

- **绘制全部走 `IPainter`（定制自绘）**，平台只提供窗口壳、消息/事件、字体
- **不包原生控件**（不走 Qt-Widgets 路线）——你们的 ListBox / Button / Card 本来就是 owner-draw 自绘，三套原生控件封装只会得到三套行为
- **分阶段、每阶段可独立回归**：先后端抽象化（Windows 行为不变），再 Skia + Linux 移植，再 macOS
- 现有测试基础设施（`TestShell` + `DrawCall` 录制/断言）是跨平台能力验证的杠杆，不要丢

---

## 1. 背景与目标

### 1.1 背景

p-system 是 Windows 专属桌面应用（注册表、托盘、性能计数器、Win32 消息、GDI 绘制、UIA 辅助功能）。其中 **AirUI（`src/airui/`）是自研 UI 库**，负责：

- 无标题栏窗口外壳（标题栏/状态栏/侧栏）
- 控件：Canvas 高级绘制 API + 复合组件（Card、DataGrid 等）
- 主题（Light/Dark）、布局（Flex/Grid）、响应式、动画

当前对 Windows 强耦合，无法在其他平台复用。

### 1.2 目标

| 目标 | 说明 |
|---|---|
| G1 | AirUI 核心（Layout/Theme/Components/交互）成为平台无关库，可静态编译为独立产物 |
| G2 | 提供 ≥2 个后端：Windows(GDI)、Skia（跨 Win/Linux/macOS） |
| G3 | p-system 在 Windows 上的**外观与行为零回归** |
| G4 | 构建跨平台化：CMake + vcpkg，clang-cl / MSVC / GCC / Clang 均可用 |
| G5 | 测试能力保留并扩展：DrawCall 录制跨后端一致性断言 |

### 1.3 非目标

- 不追求启动/内存/渲染性能相对 GDI 的定量提升（目标是「跑起来 + 可维护」）
- 不做 Web/WASM 移植
- 不替换 p-system 的业务层 Win32（托盘、注册表等）——那是 App 的事，AirUI 只管 UI
- 不做辅助功能层的完整移植（分阶段，见 §7.4）

---

## 2. 现状盘点（迁移依据）

### 2.1 代码规模

- AirUI：`include/airui/` 19 个头 + `src/` 16 个 cpp ≈ **3,900 行**
- 文件级耦合统计（`Select-String` 命中 `windows.h/HWND/HDC/...`）：

| 文件 | 行数 | Windows 相关度 | 处置 |
|---|---|---|---|
| `Shell.cpp` | ~861 | ~100%：CreateWindowExW、WndProc、WM_NCHITTEST、WM_NCCALCSIZE、DWM、控件色 | → `platform/win32/Win32Window.cpp` + 核心外壳逻辑 `core/Shell.cpp` |
| `Widgets.cpp` | ~423 | ~100%：SetWindowSubclass、EDIT/LISTBOX/BUTTON 消息、EM_* | → `platform/win32/Win32Widgets.cpp`；交互语义进 `core/` |
| `Controls.cpp` | ~580 | ~100%：DRAWITEMSTRUCT、DWM 框架、按钮 caption 绘制 | → `platform/win32/` + `core/ControlKit.{h,cpp}` |
| `Canvas.cpp` | ~216 | ~90%：HDC、HFONT、DT_* | → `IPainter` 语义 100% 收进 `IPainter`；Win32 实现 `platform/win32/GdiPainter.cpp` |
| `Gfx.cpp` | ~244 | ~80%：HDC、GDI 填充/描边路径 | → `IPainter` 的一部分（填充/描边/裁剪/图元） |
| `Fonts.cpp` | ~86 | ~90%：CreateFontW、Segoe 系列 | → `platform/win32/GdiFonts.cpp` |
| `Layout.cpp` | 178 | 0% | 保留在 `core/` |
| `Theme.cpp` / `ThemeJson.cpp` | 166 + 323 | 0%（COLORREF 纯值） | 保留在 `core/`；引入 `Color` 别名/包装 |
| `Responsive.cpp` | 42 | ~0% | `core/` |
| `Animation.cpp` | 79 | ~0%（含少量 GetTickCount） | `core/` + 注入 `IClock` |
| `Accessibility.cpp` | 245 | ~90%：UIA COM | `platform/win32/`；core 定义抽象接口 |
| `Components.cpp` | 69 | 0%（基于 Canvas） | `core/` |
| `Observable.h` / `DrawCall.h` / `TestShell.*` | ~380 | 0% | `core/` + 测试复用 |

### 2.2 现有资产（必须保留并复用）

1. **`DrawCall` 录制器**（`DrawCall.h` / Canvas 的 `SetRecorder`）：把「组件画了什么」完整记录为结构化调用。
2. **`TestShell`（无头）**：用录制断言验证绘制序列，跨后端时可直接比较 Win GDI vs Skia 的调用序列/截图。
3. **Component / Canvas 语义良好**：`Canvas::Button/FillRound/...` 已是纯声明式 API，页面不碰 HDC —— 这是迁移最重要的结构优势。
4. **`Layout` 是纯几何**：无平台依赖，迁移零成本。

### 2.3 需要收拢的问题（迁移前置）

- **交互逻辑散落**：hover/pressed/焦点/命中测试/滚动，现在分散在 `Shell.cpp`、`Widgets.cpp`、页面里。必须收拢为 `core/Interaction`，平台后端只负责喂鼠标/键盘/滚轮事件。
- **坐标系**：全用 `RECT`（`long` 系）。引入跨平台 `Rect`（`int32_t`）+ `Point`，Win32 后端做转换。
- **颜色**：`COLORREF` → `Color`（RGBA32），Windows 后端在边缘转换。
- **字符串**：坚持 `std::wstring`；后端按需转 UTF-8（Skia 推荐 UTF-8）。

---

## 3. 架构设计

### 3.1 三层架构

```
┌────────────────────────────────────────────────────────┐
│ App（p-system 业务页）                                  │
├────────────────────────────────────────────────────────┤
│ AirUI Core（头+源码，Header-only 不强制，独立静态库）     │
│  ├─ Theme（Palette/Metrics/JSON）                        │
│  ├─ Layout（Row/Col/Grid/Nest）                          │
│  ├─ Components / Canvas 语义 / EmptyState / IconTile     │
│  ├─ Interaction（命中测试/hover/press/焦点/滚动）         │
│  ├─ Page 接口 —— 业务页直接面向的 API                     │
│  └─ IWindow / IPainter / IFont / IClock（HAL）            │
├────────────────────────────────────────────────────────┤
│ Platform Backend                                        │
│  ├─ win32：GdiPainter / GdiFonts / Win32Window           │
│  ├─ skia：SkiaPainter / SkiaFonts / SkiaWindow           │
│  └─ (gtk/cocoa — 可选窗口壳)                              │
└────────────────────────────────────────────────────────┘
```

**核心原则**

1. `core/` **不包含任何平台头文件**（`windows.h`、`X11/…`、`Cocoa.h`）；只依赖 `<string>`、`<vector>`、`<memory>`、`<functional>`。
2. 平台差异全部通过 HAL 接口注入；后端通过工厂函数注册：`air::ui::CreateWin32Backend()` / `air::ui::CreateSkiaBackend()`。
3. 业务页（`ClipsPage` 等）继承 `Page`、实现 `OnPaint(Canvas& …)`——**API 与现状完全一致**，迁移对业务代码透明。
4. 绘制路径是「core 生成绘制指令 → IPainter 执行」；把这一点做实，跨后端像素一致性可自测量。

### 3.2 HAL 接口草图（语义对齐现有 Canvas，不发明新 DSL）

```cpp
// core/hal.h（示意，不是最终签名）
struct Rect   { float x, y, w, h; };
struct Color  { uint32_t rgba; };      // 0xRRGGBBAA
enum class FontRole { Body, Meta, Title, Section, Display, Button, Icon, IconLarge };
enum class PainterClipOp { Intersect, Replace };

class IFont {
public:
    virtual ~IFont() = default;
    virtual float  MeasureWidth(const wchar_t* text, FontRole role) = 0;
    virtual float  LineHeight(FontRole role) = 0;
    virtual void   Select(FontRole role) = 0;      // 供文本调用前设置
    // 字体子集化/字形缓存归后端，core 不关心
};

class IPainter {
public:
    virtual ~IPainter() = default;
    virtual void Save() = 0;
    virtual void Restore() = 0;
    virtual void Clip(const Rect& r, PainterClipOp op) = 0;
    virtual void Translate(float dx, float dy) = 0;      // ← 现有 gallery 的「滚动裁剪」将变为正规变换

    virtual void Fill(const Rect& r, Color c) = 0;
    virtual void FillRound(const Rect& r, Color c, float radius) = 0;
    virtual void Stroke(const Rect& r, Color c, float radius, float width) = 0;
    virtual void HLine(float x1, float x2, float y, Color c) = 0;
    virtual void VLine(float x, float y1, float y2, Color c) = 0;
    virtual void Dot(float cx, float cy, float diameter, Color c) = 0;
    virtual void Text(const Rect& r, const std::wstring& s, FontRole role,
                      Color c, TextAlign align) = 0;
    virtual void TextBlock(const Rect& r, const std::wstring& s,
                           FontRole role, Color c, int maxLines) = 0;
    virtual void PushClip/PopClip 等对应关系按需增删
};

struct WindowOptions { int width, height, minWidth, minHeight; bool frameless, dark; };

class IWindow {
public:
    virtual ~IWindow() = default;
    virtual void SetTitle(const std::wstring&) = 0;
    virtual void SetSizeLimits(int minW, int minH) = 0;
    virtual void Show() = 0;
    virtual bool PumpOneMessage() = 0;               // 或 RunLoop()
    virtual void RequestRedraw() = 0;
    // 事件回调由 core 注入：
    virtual void On(MouseMove, std::function<bool(Point)> cb) = 0;
    virtual void On(LButtonDown, ...) = 0;
    virtual void On(Wheel, ...) = 0;
    virtual void On(KeyDown, ...) = 0;
    virtual void On(Resized, std::function<void(Rect content)> cb) = 0;
    virtual void On(Closed, ...) = 0;
};
```

`Canvas` 在 core 中实现为 `IPainter` 的**强类型适配器**（带 `Recorder` 测试钩子），业务页继续写：

```cpp
g.FillRound(rc, Pal().surface, Mx().radiusLg);
g.Text(rc, L"标题", FontRole::Section, Pal().text, …);
```

### 3.3 目录结构（迁移后）

```
src/airui/
├─ core/                      ← 平台无关（不含平台头）
│  ├─ include/airui/*.h       （现有公共头挪入，改 Rect/Color 类型）
│  ├─ hal.h                   （IWindow / IPainter / IFont / IClock）
│  ├─ canvas.h / canvas.cpp   （Canvas → IPainter 适配器 + Recorder）
│  ├─ theme/                  （Theme.h / ThemeJson.cpp）
│  ├─ layout/                 （Layout.h / Layout.cpp）
│  ├─ components/             （Components.h/.cpp）
│  ├─ interaction/            （命中测试/hover/焦点/滚动）
│  └─ shell_state.cpp         （标题栏/状态栏/侧栏的**纯状态**与布局计算）
├─ platform/
│  ├─ win32/                  （GdiPainter.cpp / GdiFonts.cpp / Win32Window.cpp / controls）
│  └─ skia/                   （SkiaPainter.cpp / SkiaFonts.cpp / SkiaWindow.cpp）
└─ tests/                     （TestShell + DrawCall 断言，跨后端）
```

`CMakeLists.txt`：

```cmake
add_library(airui-core STATIC ${AIRUI_CORE_SOURCES})   # core/*.cpp —— 无平台依赖
add_library(airui-win32 STATIC ${AIRUI_WIN32_SOURCES}) # platform/win32/*.cpp
target_link_libraries(airui-win32 airui-core)
add_executable(airui_gallery gallery/main.cpp)
target_link_libraries(airui_gallery airui-win32)
```

---

## 4. API 迁移影响（业务侧）

`Page` 接口对业务页**保持不变**，迁移对 p-system 业务代码**零可见改动**：

```cpp
class ClipsPage : public air::ui::Page {
    void OnPaint(Canvas& g, const RECT& content) override;   // RECT → Rect（typedef 过渡期保留）
    …same as before…
};
```

迁移期提供 `RECT` → `Rect` 的 `using Rect = RECT;` 过渡 typedef，消除掉后再下线。`Widget` 工厂（CreateTextBox/ListBox/Button）签名保留。

---

## 5. 迁移路线（四个阶段，每阶段独立可验证）

### 阶段 0 — 边界清理（1–2 周）

- [ ] 新建 `docs/cross-platform.md`（本文档）评审通过
- [ ] 建 `core/` 与 `platform/win32/` 目录，文件按 §2 表格移动（内容暂不改）
- [ ] 核心头文件从公共头里剥离 `windows.h`：`RECT→Rect`、`COLORREF→Color` 过渡 typedef
- [ ] `Animation` 时间源注入 `IClock`
- [ ] 全部现有测试通过 + `airui_gallery` Windows 运行行为不变
- 退出标准：core 目录 `grep -r "windows.h\|HWND\|HDC"` 为空

### 阶段 1 — Win32 后端 HAL 化（2–3 周）

- [ ] `IWindow` / `IPainter` / `IFont` / `IClock` 定稿
- [ ] Win32 实现：`GdiPainter`（Gfx 逻辑直译 + 裁剪栈）、`GdiFonts`（Fonts.cpp 直译）、`Win32Window`（Shell 消息泵/无边框/WM_NCHITTEST）
- [ ] 控件（TextBox/ListBox/Button owner-draw）后端化——**这是最重的一块**
- [ ] 老 `Canvas` 改为 `IPainter` 适配器 + `Recorder` 保留
- 退出标准：`tests/` 全绿；`airui_gallery` 外观/交互像素级一致（肉眼 + DrawCall diff 双重验证）；Windows 应用 p-system 全功能冒烟

### 阶段 2 — Skia + Linux（4–6 周）

- [ ] vcpkg 引入 `skia`（或 `skia-gl`），构建链切 clang-cl/clang
- [ ] `SkiaPainter` / `SkiaFonts` / `SkiaWindow`（Wayland/X11 via winit 或自建 X11 壳）
- [ ] 文本度量对齐：Skia hinted 度量 vs GDI —— 允许 ±1px，但 `MeasureWidth/LineHeight` 必须稳定可断言
- [ ] 字体：内置 Noto Sans CJK / DejaVu + 子集化图标（替换 Segoe MDL2）
- [ ] headless 截图测试：同场景 Win GDI vs Skia 截图 diff 阈值（如 SSIM ≥ 0.98）
- 退出标准：`airui_gallery` 在 Ubuntu 22.04 原生运行；DrawCall 录制 diff 为零（或阈值内）；测试全绿

### 阶段 3 — macOS（3–4 周，可选）

- [ ] `CocoaWindow` 壳或 winit；`SkiaPainter` 复用（Metal/软件Raster）
- [ ] CoreText 字体/回退；`NSAccessibility` → P0 辅助功能对齐
- 退出标准：gallery 在 macOS 14 运行；核心交互 100% 一致

### 阶段 4 — 辅助功能 & i18n（持续）

- UIA（Win）→ AT-SPI（Linux）→ NSAccessibility（macOS）：接口形态先行（`IAccessibleNode`），逐步实现

---

## 6. 测试策略

| 类型 | 现状 | 跨平台后 |
|---|---|---|
| 无头绘制断言 | `TestShell` + `DrawCall` 录制 | 保留；新增 **Win GDI vs Skia 录制对齐**（同一组件序列录制后比对调用序列一致性） |
| 截图回归 | `gallery_gui_light/dark.png`（人工） | CI 内固定场景截图 + 逐像素/结构相似度阈值 |
| 布局测试 | `test_component_gallery`（`CalculateGalleryLayout`） | 原样保留（纯几何，天然跨平台） |
| 交互状态测试 | 文本断言 + 手工 | 注入假 `IWindow` 事件流（鼠标/键盘）驱动整页交互断言 |

**关键约束**：Core 的所有测试**不得包含平台头**，CI 可在 Linux 上跑核心断言。

---

## 7. 风险与对策

| # | 风险 | 影响 | 对策 |
|---|---|---|---|
| R1 | **IME/输入法差异**（fcitx/ibus vs NSTextInput vs TSF） | 文本输入在 Linux/macOS 不可用 | 阶段 2/3 专项；Windows 继续用原生 EDIT，Linux 先绕 |
| R2 | 字体度量差异 → 自绘文本行偏移 | 视觉错位、所有 gallery 截图失效 | Skia `FontStyle` hinted 度量 + 金样截图阈值；`MeasureWidth` 单点校验 |
| R3 | 原生控件的系统行为丢失（窗口吸附、拖拽、触摸） | 感知体验下降 | 只做**客户区**自绘；标题栏/resize 走平台壳；吸附/拖拽由 `IWindow` 提供（Win32 用 DefWindowProc 等价物） |
| R4 | 双后端行为漂移（Canvas 语义不一致） | 跨平台 bug 分叉 | DrawCall 录制强制对齐；CI 双平台跑同一断言 |
| R5 | vcpkg / Skia 构建地狱（尤其 Windows 上 clang） | 开发效率 | Linux CI 先行 + Windows 用 clang-cl；依赖预编译缓存（vcpkg binary cache） |
| R6 | p-system 业务代码里的 Win32 泄漏到 UI 调用处 | 看似迁了 UI，实则没有 | 阶段 0 做 `grep` 扫描门禁；UI 头里禁止出现 `windows.h` |
| R7 | Emoji / CJK 回退字体缺失（Linux 默认无 CJK） | 中文方框 | 内置 Noto Sans CJK Subset，按语言包可选 |

---

## 8. 工时估算（按 1 名熟悉本库的工程师）

| 阶段 | 估算 |
|---|---|
| 阶段 0 边界清理 | 1–2 周 |
| 阶段 1 Win32 HAL 化 | 2–3 周 |
| 阶段 2 Skia + Linux | 4–6 周 |
| 阶段 3 macOS | 3–4 周 |
| 阶段 4 辅助功能 | 持续 |
| **合计到 Linux 可用** | **≈ 2–3 个月** |

对照：直接用 Qt Widgets 重写 UI ≈ 同等投入但**更差**（失去 owner-draw 全自绘能力与既有测试资产，外观永远「像 Qt」而非「像 AirUI」）。

---

## 9. 决策记录（ADR）

1. **不包原生控件**。理由：ListBox/Button 本来是自绘；三套原生封装 = 三套行为 + 三倍维护。
2. **Core 与平台代码物理分目录**（`core/` / `platform/`），而非靠 `#if` 宏分叉。理由：`#ifdef` 会在 5 个文件内腐化。
3. **Skia 作为长期跨平台主线**，Win32 GDI 保留但冻结（只修 bug，不加功能）。理由：一套渲染器、统一度量、统一图标。
4. **先抽象后移植**：阶段 0/1 零第三方依赖，Windows 行为零变化。理由：降低评审与回归风险。
5. **保留 DrawCall 录制器**作为跨后端一致性测试的基础机制。

## 10. 开放问题（评审时需定）

- [ ] Linux 窗口壳：winit（更少代码，Wayland/X11 免费）还是自研 X11/Wayland 壳（更少依赖）？
- [x] Skia 引入方式（2026-11-07 定案）：vcpkg `skia` 首选 + `AIRUI_SKIA_ROOT` 预编译备选，不采用子模块源码编译；默认不进入构建图。完整决策与操作手册见 `docs/skia-backend.md`。
- [ ] macOS 是否纳入本计划，还是先交付 Linux？
- [ ] 图标方案：内置 SVG 渲染（推荐）还是 Fork 出 `AirIcons` 自带字体？
- [ ] 辅助功能优先级：是否在 Linux 首发时强制支持 AT-SPI，还是 v1 明天再做？

## 11. 附录

### A. 迁移后头文件包含图（概念）

```
airui/hal.h          ← 所有平台 HAL 接口
airui/theme.h        ← core
airui/layout.h       ← core
airui/canvas.h       ← core → IPainter
airui/page.h         ← core → 业务入口
airui/shell.h        ← core → IWindow + 标题栏/状态栏/侧栏纯逻辑
airui/platform/win32/  ← 唯一允许出现 windows.h 的目录
```

### B. 门禁（CI）

```powershell
# 核心目录不得包含平台头（阶段 0 起强制）
if (Select-String -Path "src/airui/core/**/*.h|*.cpp" -Pattern "windows\.h|HWND|HDC|HFONT|DRAWITEMSTRUCT") {
    Write-Error "Platform header leaked into core"
}
```

### C. 相关文件（迁移前参考）

- `src/airui/`（本设计的研究对象）
- `src/gallery/main.cpp`、`src/gallery/GalleryDraw.*`（跨平台后作为首个验收截图源）
- `tests/test_component_gallery.cpp`、`src/airui/include/airui/TestShell.h`（测试资产）
- `CMakeLists.txt`（`AIRUI_SOURCES` 需按 §3.3 拆分）

---

*本文档为评审稿。评审通过的下一步是阶段 0 实施计划（文件级改动清单 + 迁移脚本），不在本文档范围。*
