#pragma once
// ============================================================
// AirUI - ControlsStudioClasses.h
//
// Object layer for the controls declared in ControlsStudio.h.
// Every widget owns its state, derives from Control and can be
// hosted by ControlHost for event routing, tab focus and the
// accessible tree. The free Paint*/Measure*/Hit* functions are
// kept as a source-compatible drawing kernel.
// ============================================================
#include "airui/FrameworkCore.h"
#include "airui/ControlsStudio.h"

namespace air {
namespace ui {
namespace widgets {

// ---- Sparkline ----
class Sparkline : public Control
{
public:
    SparklineState state{};

    Sparkline() { SetRole(ControlRole::Custom); }

    SparklineLayout Measure(const Metrics& mx) const;
    int PointAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Sparkline");
    }
};

// ---- Waterfall ----
class Waterfall : public Control
{
public:
    WaterfallState state{};

    Waterfall() { SetRole(ControlRole::Custom); }

    WaterfallLayout Measure(const Metrics& mx) const;
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
        return std::wstring(L"Waterfall");
    }
};

// ---- StackedBar ----
class StackedBar : public Control
{
public:
    StackedBarState state{};

    StackedBar() { SetRole(ControlRole::Custom); }

    StackedBarLayout Measure(const Metrics& mx) const;
    StackedBarHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"StackedBar");
    }
};

// ---- Gauge ----
class Gauge : public Control
{
public:
    GaugeState state{};

    Gauge() { SetRole(ControlRole::Custom); }

    GaugeLayout Measure(const Metrics& mx) const;
    int SegmentAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Gauge");
    }
};

// ---- JsonTree ----
class JsonTree : public Control
{
public:
    JsonTreeState state{};

    JsonTree() { SetRole(ControlRole::Custom); }

    JsonTreeLayout Measure(const Metrics& mx) const;
    JsonTreeHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"JsonTree");
    }
};

// ---- LogViewer ----
class LogViewer : public Control
{
public:
    LogViewerState state{};

    LogViewer() { SetRole(ControlRole::Custom); }

    LogViewerLayout Measure(const Metrics& mx) const;
    int LogLineAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"LogViewer");
    }
};

// ---- CronEditor ----
class CronEditor : public Control
{
public:
    CronEditorState state{};

    CronEditor() { SetRole(ControlRole::TextBox); }

    CronEditorLayout Measure(const Metrics& mx) const;
    CronEditorHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"CronEditor");
    }
};

// ---- MentionInput ----
class MentionInput : public Control
{
public:
    MentionInputState state{};

    MentionInput() { SetRole(ControlRole::TextBox); }

    MentionInputLayout Measure(const Metrics& mx) const;
    MentionInputHit HitAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"MentionInput");
    }
};

// ---- OrgChart ----
class OrgChart : public Control
{
public:
    OrgChartState state{};

    OrgChart() { SetRole(ControlRole::Custom); }

    OrgChartLayout Measure(const Metrics& mx) const;
    int OrgNodeAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"OrgChart");
    }
};

// ---- Leaderboard ----
class Leaderboard : public Control
{
public:
    LeaderboardState state{};

    Leaderboard() { SetRole(ControlRole::Custom); }

    LeaderboardLayout Measure(const Metrics& mx) const;
    int LeaderRowAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"Leaderboard");
    }
};

// ---- ActivityFeed ----
class ActivityFeed : public Control
{
public:
    ActivityFeedState state{};

    ActivityFeed() { SetRole(ControlRole::Custom); }

    ActivityFeedLayout Measure(const Metrics& mx) const;
    int ActivityItemAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"ActivityFeed");
    }
};

// ---- AudioWaveform ----
class AudioWaveform : public Control
{
public:
    AudioWaveformState state{};

    AudioWaveform() { SetRole(ControlRole::Group); }

    WaveformLayout Measure(const Metrics& mx) const;
    int WaveformPositionAt(const Metrics& mx, Point pt) const;
    bool WaveformPlayButtonAt(const Metrics& mx, Point pt) const;
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
        return std::wstring(L"AudioWaveform");
    }
};

} // namespace widgets
} // namespace ui
} // namespace air
