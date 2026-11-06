#pragma once
// ============================================================
// AirUI - ControlsEnterpriseClasses.h
//
// Object layer for the controls declared in ControlsEnterprise.h.
// Every widget owns its state, derives from Control and can be
// hosted by ControlHost for event routing, tab focus and the
// accessible tree. The free Paint*/Measure*/Hit* functions are
// kept as a source-compatible drawing kernel.
// ============================================================
#include "airui/FrameworkCore.h"
#include "airui/ControlsEnterprise.h"

namespace air {
namespace ui {
namespace widgets {

// ---- QueryBuilder ----
class QueryBuilder : public Control
{
public:
    QueryBuilderState state{};

    QueryBuilder() { SetRole(ControlRole::Custom); }

    QueryBuilderLayout Measure(const Metrics& mx) const;
    QueryBuilderHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"QueryBuilder");
    }
};

// ---- PropertyGrid ----
class PropertyGrid : public Control
{
public:
    PropertyGridState state{};

    PropertyGrid() { SetRole(ControlRole::Custom); }

    PropertyGridLayout Measure(const Metrics& mx) const;
    PropertyGridHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"PropertyGrid");
    }
};

// ---- Schedule ----
class Schedule : public Control
{
public:
    ScheduleState state{};

    Schedule() { SetRole(ControlRole::Custom); }

    ScheduleLayout Measure(const Metrics& mx) const;
    int EventAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Schedule");
    }
};

// ---- FileDropZone ----
class FileDropZone : public Control
{
public:
    FileDropZoneState state{};

    FileDropZone() { SetRole(ControlRole::Custom); }

    FileDropZoneLayout Measure(const Metrics& mx) const;
    FileDropZoneHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"FileDropZone");
    }
};

// ---- TreeMap ----
class TreeMap : public Control
{
public:
    TreeMapState state{};

    TreeMap() { SetRole(ControlRole::Custom); }

    TreeMapLayout Measure(const Metrics& mx) const;
    int TileAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"TreeMap");
    }
};

// ---- Funnel ----
class Funnel : public Control
{
public:
    FunnelState state{};

    Funnel() { SetRole(ControlRole::Custom); }

    FunnelLayout Measure(const Metrics& mx) const;
    int StageAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Funnel");
    }
};

// ---- Diff ----
class Diff : public Control
{
public:
    DiffState state{};

    Diff() { SetRole(ControlRole::Custom); }

    DiffLayout Measure(const Metrics& mx) const;
    int LineAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Diff");
    }
};

// ---- Score ----
class Score : public Control
{
public:
    ScoreState state{};

    Score() { SetRole(ControlRole::Custom); }

    ScoreLayout Measure(const Metrics& mx) const;
    int BarAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Score");
    }
};

// ---- Chat ----
class Chat : public Control
{
public:
    ChatState state{};

    Chat() { SetRole(ControlRole::Custom); }

    ChatLayout Measure(const Metrics& mx) const;
    int MessageAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Chat");
    }
};

// ---- MultiStatusBar ----
class MultiStatusBar : public Control
{
public:
    MultiStatusBarState state{};

    MultiStatusBar() { SetRole(ControlRole::Custom); }

    MultiStatusBarLayout Measure(const Metrics& mx) const;
    MultiStatusBarHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"MultiStatusBar");
    }
};

// ---- Ribbon ----
class Ribbon : public Control
{
public:
    RibbonState state{};

    Ribbon() { SetRole(ControlRole::Custom); }

    RibbonLayout Measure(const Metrics& mx) const;
    RibbonHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Ribbon");
    }
};

// ---- NoticeCenter ----
class NoticeCenter : public Control
{
public:
    NoticeCenterState state{};

    NoticeCenter() { SetRole(ControlRole::Custom); }

    NoticeCenterLayout Measure(const Metrics& mx) const;
    NoticeCenterHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"NoticeCenter");
    }
};

} // namespace widgets
} // namespace ui
} // namespace air
