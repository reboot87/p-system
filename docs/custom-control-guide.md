# 如何实现一个自定义控件（简单指南）

> AirUI 的控件是**平台无关 + 可后端渲染**的：控件只调用 `IPainter`（GDI / Skia / 录制器都能画），
> 不直接碰 GDI/Skia。所以一个自定义控件 = **一个状态结构 + 一个 Paint 函数**，非常薄。

## 0. 一分钟结论

自定义控件 = `State 结构` + `Paint 函数`（+ 可选 `Control` 子类），全部用 `IPainter`
画，放 `airui-core`。然后在 gallery 里加一个示例即可看到它（GDI/Skia 都渲染）。

## 1. 步骤

> 前置：控件代码在 `namespace air::ui` 内；MSVC 下需加 `/source-charset:utf-8`
> （头文件注释是 UTF-8，airui 自己的 CMake 已设置）。用 `using namespace air::ui;` 或写全 `air::ui::`。

### 步骤 1：定义状态结构

控件的数据（文本、选中、禁用等）放一个 `struct`，基础状态用 `ControlState`：

```cpp
// src/airui/core/include/airui/ControlsCore.h（或你的新头）
struct MyBadgeState {
    ControlState base = {};        // 通用：hover / pressed / disabled / focused
    std::wstring  text;            // 控件数据
    bool          primary = false; // 你的控件特有字段
};

// 声明 Paint 函数（头文件里）
void PaintMyBadge(IPainter& g, const Rect& rc, const MyBadgeState& st,
                  const Palette& pal, const Metrics& mx);
```

### 步骤 2：实现 Paint（绘制）

```cpp
// src/airui/core/src/ControlsCore.cpp
void PaintMyBadge(IPainter& g, const Rect& rc, const MyBadgeState& st,
                  const Palette& pal, const Metrics& mx)
{
    // 1) 根据状态 + 主题选颜色
    Color bg = st.base.disabled ? pal.sunken :
               st.primary      ? pal.primary : pal.surface;
    Color txt = st.base.disabled ? pal.textDisabled : pal.text;

    // 2) 用 IPainter 画（GDI/Skia 都能画）
    g.FillRound(rc, mx.radiusPill, bg);
    g.StrokeRound(rc, mx.radiusPill, pal.border, 1);
    g.Text(rc, st.text.c_str(), st.text.size(), FontRole::Body, txt,
           dt::SingleLine | dt::Center | dt::VCenter);
}
```

> 注意：`Text` 接收 `const wchar_t* + size_t`（ABI v2），不要传 `std::wstring`。

### 步骤 3（可选，做交互）：封装成控件类

要 hover / focus / 命中测试，继承 `Control`（`FrameworkCore.h`）——`Tag` 就是这么写的：

```cpp
// ControlsCoreClasses.h
#include "airui/FrameworkCore.h"

class MyBadge : public Control {
public:
    MyBadgeState state{};

    MyBadge() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override
    {
        state.base.hover    = Hovered();
        state.base.focused  = Focused();
        state.base.disabled = !Enabled();
        PaintMyBadge(g, Bounds(), state, pal, mx);
    }

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover   = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }
};
```

`Control` 已提供 `Bounds() / SetBounds() / Hovered() / Focused() / Enabled() / Hit()`
和默认的 `OnPointer / OnKey`，你只重写需要的行为。用类时可 `SetBounds(rc)` 后在容器里调用
`Paint(g, pal, mx)`；纯展示场景直接用步骤 2 的自由函数即可。

> 类在 `ControlsCoreClasses.h`（实现 `ControlsCore.cpp`），自由函数在 `ControlsCore.h`；
> 两者并存——**新代码推荐用类**（`docs/ui-library.md` 也这么约定）。

### 步骤 4：加进 gallery 预览

1. 在 `src/gallery/main.cpp` 的 `struct Item { enum Kind { ... } }` 里加 `MyBadge`；
2. 在 `Build()` 里加条目；
3. 在绘制分支里调用 Paint 函数（gallery 用 `CanvasPainter` 把 `HdcPaint` 适配成 `IPainter`）。

```cpp
// 1) Kind 枚举加一项：..., Extra, MyBadge

// 2) Build() 里：
Item it; it.kind = Item::MyBadge; it.text = L"新徽标";
it.rect = {x, y, x + 120, y + 28};
m_items.push_back(it);

// 3) 绘制分支（照 Tag 的写法）：
} else if (it.kind == Item::MyBadge) {
    Rect rc{ r.left, r.top, r.right, r.bottom };
    MyBadgeState st;
    st.base.hover    = it.hover;
    st.base.disabled = it.disabled;
    st.text          = it.text;
    st.primary       = true;
    CanvasPainter painter(g);
    PaintMyBadge(painter, rc, st, Pal(), Mx());
}
```

就这样，`airui_gallery` 里会多一个可 hover/press 的控件，且 GDI 与 Skia 两种后端都能渲染。

## 2. 关键点（为什么这么简单）

- **平台无关**：控件只依赖 `IPainter` + `Palette` + `Metrics`，GDI / Skia / 录制器都能渲染。
- **状态即数据**：控件无隐式全局，所有显示信息在 `State` 里 → 可序列化、可测试、可复用。
- **主题驱动**：颜色/圆角从 `Palette`/`Metrics` 取，不是硬编码 → 深浅色主题自动生效。
- **自包含头门禁**：公共头必须单独可编译（`check-public-headers` 强制）。

## 3. 现有控件参考

| 控件 | 自由函数（`ControlsCore.h`） | 控件类（`ControlsCoreClasses.h`） |
|---|---|---|
| Tag（胶囊） | `PaintTag` | `Tag` |
| Rating（星级） | `PaintRating` / `PaintRatingBar` | `Rating` / `RatingBar` |
| ToggleSwitch | `PaintToggleSwitch` | `Toggle` |
| ProgressBar / Ring | `PaintProgressBar` / `PaintProgressRing` | `ProgressBar` / `ProgressRing` |
| Badge / Avatar | `PaintBadge` / `PaintAvatar` | `Badge` / `Avatar` |

按 `Tag::Paint`（`ControlsCore.cpp:355`）的写法就是最标准的自定义控件：读 `Bounds()` →
按 `state`/`pal`/`mx` 算颜色 → 全用 `IPainter` 绘制。

## 4. 集成到 CMake（如果新加了源文件）

```cmake
# 头：放进 src/airui/core/include/airui/（自动进入 airui-core PUBLIC）
# 实现：放进 src/airui/core/src/（`AIRUI_CORE_SOURCES` 里加一行，或直接用现成 .cpp）
```

## 5. 检查

- 本文示例已用 MSVC x64 单独编译验证通过（`/std:c++17 /source-charset:utf-8`）。
- `cd build; ctest -C Release` 22/22（`test_paint_recorder` 金样会覆盖你的新绘制，若录入了）。
- `powershell -File scripts/check-core-clean.ps1`（core 里别用平台头）。
- 公共头自包含：`cmake --build build --config Release --target check-public-headers`。