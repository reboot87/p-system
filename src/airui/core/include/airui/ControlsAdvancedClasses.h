#pragma once
// ============================================================
// AirUI - ControlsAdvancedClasses.h
//
// Object layer for the controls declared in ControlsAdvanced.h.
// Every widget owns its state, derives from Control and can be
// hosted by ControlHost for event routing, tab focus and the
// accessible tree. The free Paint*/Measure*/Hit* functions are
// kept as a source-compatible drawing kernel.
// ============================================================
#include "airui/FrameworkCore.h"
#include "airui/ControlsAdvanced.h"

namespace air {
namespace ui {
namespace widgets {

// ---- TagInput ----
class TagInput : public Control
{
public:
    TagInputState state{};

    TagInput() { SetRole(ControlRole::TextBox); }

    TagInputLayout Measure(const Metrics& mx) const;
    TagInputHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"TagInput");
    }
};

// ---- RangeSlider ----
class RangeSlider : public Control
{
public:
    RangeSliderState state{};

    RangeSlider() { SetRole(ControlRole::Slider); }

    RangeSliderLayout Measure(const Metrics& mx) const;
    int ThumbAt(const Metrics& mx, Point pt) const;
    int ValueAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"RangeSlider");
    }
};

// ---- OtpInput ----
class OtpInput : public Control
{
public:
    OtpInputState state{};

    OtpInput() { SetRole(ControlRole::TextBox); }

    OtpInputLayout Measure(const Metrics& mx) const;
    int OtpCellAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"OtpInput");
    }
};

// ---- Cascader ----
class Cascader : public Control
{
public:
    CascaderState state{};

    Cascader() { SetRole(ControlRole::Custom); }

    CascaderLayout Measure(const Metrics& mx) const;
    CascaderHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Cascader");
    }
};

// ---- AvatarGroup ----
class AvatarGroup : public Control
{
public:
    AvatarGroupState state{};

    AvatarGroup() { SetRole(ControlRole::Group); }

    AvatarGroupLayout Measure(const Metrics& mx) const;
    int At(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"AvatarGroup");
    }
};

// ---- Heatmap ----
class Heatmap : public Control
{
public:
    HeatmapState state{};

    Heatmap() { SetRole(ControlRole::Custom); }

    HeatmapLayout Measure(const Metrics& mx) const;
    HeatmapCell CellAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Heatmap");
    }
};

// ---- CodeBlock ----
class CodeBlock : public Control
{
public:
    CodeBlockState state{};

    CodeBlock() { SetRole(ControlRole::Custom); }

    CodeBlockLayout Measure(const Metrics& mx) const;
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
        return std::wstring(L"CodeBlock");
    }
};

// ---- Kanban ----
class Kanban : public Control
{
public:
    KanbanState state{};

    Kanban() { SetRole(ControlRole::Custom); }

    KanbanLayout Measure(const Metrics& mx) const;
    KanbanHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Kanban");
    }
};

// ---- Gantt ----
class Gantt : public Control
{
public:
    GanttState state{};

    Gantt() { SetRole(ControlRole::Custom); }

    GanttLayout Measure(const Metrics& mx) const;
    int TaskAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Gantt");
    }
};

// ---- Snackbar ----
class Snackbar : public Control
{
public:
    SnackbarState state{};

    Snackbar() { SetRole(ControlRole::Custom); }

    SnackbarLayout Measure(const Metrics& mx) const;
    SnackbarHit HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Snackbar");
    }
};

// ---- CommandPalette ----
class CommandPalette : public Control
{
public:
    CommandPaletteState state{};

    CommandPalette() { SetRole(ControlRole::Custom); }

    CommandPaletteLayout Measure(const Metrics& mx) const;
    CommandPaletteHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"CommandPalette");
    }
};

// ---- GuideTour ----
class GuideTour : public Control
{
public:
    GuideTourState state{};
    Rect anchor{};

    GuideTour() { SetRole(ControlRole::Custom); }

    GuideTourLayout Measure(const Metrics& mx) const;
    GuideTourHit HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"GuideTour");
    }
};

} // namespace widgets
} // namespace ui
} // namespace air
