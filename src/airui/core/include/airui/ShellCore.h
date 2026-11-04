#pragma once
// ============================================================
// AirUI - ShellCore.h
// Platform-free shell state for Caption / NavBar / StatusBar.
// Phase 1: core owns state & layout, backend owns pixels.
// ============================================================
#include "airui/types.h"
#include "airui/hal.h"
#include "airui/Theme.h"

#include <string>
#include <vector>

namespace air {
namespace ui {

struct NavItem
{
    int id = -1;
    std::wstring label;
    bool disabled = false;
    std::wstring badge;
    bool badgeOk = true;
};

struct NavGroup
{
    std::wstring title;
    std::vector<NavItem> items;
};

struct CaptionState
{
    std::wstring title;
    std::wstring subtitle;
    std::wstring badge;
    bool badgeOk = true;
};

struct StatusState
{
    std::wstring text;
    bool ok = true;
    std::wstring extra;
};

class ShellCore
{
public:
    ShellCore() = default;

    // --- Nav ---
    void SetNavGroups(std::vector<NavGroup> groups);
    void SetActiveNav(int id);
    void SetNavBadge(int id, const std::wstring& text, bool ok);
    int  ActiveNav() const { return m_activeNav; }

    // --- Caption / Status ---
    void SetCaption(const CaptionState& cs) { m_caption = cs; }
    void SetStatus(const StatusState& ss) { m_status = ss; }

    // --- Layout ---
    void Layout(const Rect& client);
    Rect NavRect() const     { return m_navRect; }
    Rect CaptionRect() const { return m_captionRect; }
    Rect StatusRect() const  { return m_statusRect; }
    Rect ContentRect() const { return m_contentRect; }

    // --- Paint ---
    void Paint(IPainter& painter, const Palette& pal, const Metrics& mx);

private:
    void PaintCaption(IPainter& painter, const Palette& pal, const Metrics& mx);
    void PaintStatus(IPainter& painter, const Palette& pal, const Metrics& mx);
    void PaintNav(IPainter& painter, const Palette& pal, const Metrics& mx);

    std::vector<NavGroup> m_groups;
    int m_activeNav = -1;
    int m_hoverNav  = -1;

    CaptionState m_caption;
    StatusState  m_status;

    Rect m_navRect;
    Rect m_captionRect;
    Rect m_statusRect;
    Rect m_contentRect;
};

} // namespace ui
} // namespace air
