#pragma once
// ============================================================
// AirUI - ControlsCoreClasses.h
//
// Object layer for the controls declared in ControlsCore.h.
// Every widget owns its state, derives from Control and can be
// hosted by ControlHost for event routing, tab focus and the
// accessible tree. The free Paint*/Measure*/Hit* functions are
// kept as a source-compatible drawing kernel.
// ============================================================
#include "airui/FrameworkCore.h"
#include "airui/ControlsCore.h"

namespace air {
namespace ui {
namespace widgets {

// ---- Button ----
class Button : public Control
{
public:
    ControlState state{};
    std::wstring text{};
    ButtonStyle style = ButtonStyle::Primary;

    Button() { SetRole(ControlRole::Button); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.pressed = ev.down && Hit(ev.pos);
        state.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Button");
    }
};

// ---- Checkbox ----
class Checkbox : public Control
{
public:
    ControlState state{};
    std::wstring text{};

    Checkbox() { SetRole(ControlRole::CheckBox); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.pressed = ev.down && Hit(ev.pos);
        state.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Checkbox");
    }
};

// ---- Radio ----
class Radio : public Control
{
public:
    ControlState state{};
    std::wstring text{};

    Radio() { SetRole(ControlRole::Radio); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.pressed = ev.down && Hit(ev.pos);
        state.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Radio");
    }
};

// ---- ProgressBar ----
class ProgressBar : public Control
{
public:
    ControlState state{};
    float progress = 0;

    ProgressBar() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.pressed = ev.down && Hit(ev.pos);
        state.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"ProgressBar");
    }
};

// ---- Slider ----
class Slider : public Control
{
public:
    SliderState state{};

    Slider() { SetRole(ControlRole::Slider); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Slider");
    }
};

// ---- Toggle ----
class Toggle : public Control
{
public:
    ToggleState state{};

    Toggle() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Toggle");
    }
};

// ---- TextField ----
class TextField : public Control
{
public:
    TextFieldState state{};

    TextField() { SetRole(ControlRole::TextBox); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"TextField");
    }
};

// ---- Segmented ----
class Segmented : public Control
{
public:
    SegmentedState state{};

    Segmented() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Segmented");
    }
};

// ---- ComboBox ----
class ComboBox : public Control
{
public:
    ComboBoxState state{};

    ComboBox() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"ComboBox");
    }
};

// ---- Tag ----
class Tag : public Control
{
public:
    TagState state{};

    Tag() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Tag");
    }
};

// ---- Rating ----
class Rating : public Control
{
public:
    RatingState state{};

    Rating() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Rating");
    }
};

// ---- Stepper ----
class Stepper : public Control
{
public:
    StepperState state{};

    Stepper() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Stepper");
    }
};

// ---- ProgressRing ----
class ProgressRing : public Control
{
public:
    ProgressRingState state{};

    ProgressRing() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"ProgressRing");
    }
};

// ---- Badge ----
class Badge : public Control
{
public:
    BadgeState state{};

    Badge() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Badge");
    }
};

// ---- Alert ----
class Alert : public Control
{
public:
    AlertState state{};

    Alert() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Alert");
    }
};

// ---- Tabs ----
class Tabs : public Control
{
public:
    TabsState state{};

    Tabs() { SetRole(ControlRole::Tab); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Tabs");
    }
};

// ---- Avatar ----
class Avatar : public Control
{
public:
    AvatarState state{};

    Avatar() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Avatar");
    }
};

// ---- Skeleton ----
class Skeleton : public Control
{
public:
    SkeletonState state{};

    Skeleton() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Skeleton");
    }
};

// ---- Table ----
class Table : public Control
{
public:
    TableState state{};

    Table() { SetRole(ControlRole::Tab); }

    TableLayout Measure(const Metrics& mx) const;
    int RowAt(const Metrics& mx, Point pt) const;
    int ColumnAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Table");
    }
};

// ---- List ----
class List : public Control
{
public:
    ListState state{};

    List() { SetRole(ControlRole::Custom); }

    int RowAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"List");
    }
};

// ---- Tree ----
class Tree : public Control
{
public:
    TreeState state{};

    Tree() { SetRole(ControlRole::Custom); }

    int RowAt(const Metrics& mx, Point pt) const;
    int ToggleAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Tree");
    }
};

// ---- Breadcrumb ----
class Breadcrumb : public Control
{
public:
    BreadcrumbState state{};

    Breadcrumb() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Breadcrumb");
    }
};

// ---- Pagination ----
class Pagination : public Control
{
public:
    PaginationState state{};

    Pagination() { SetRole(ControlRole::Custom); }

    std::vector<PaginationSlot> Measure(const Metrics& mx) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Pagination");
    }
};

// ---- Toast ----
class Toast : public Control
{
public:
    ToastState state{};

    Toast() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Toast");
    }
};

} // namespace widgets
} // namespace ui
} // namespace air
