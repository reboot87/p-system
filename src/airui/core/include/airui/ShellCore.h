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

struct ShellNavItem
{
    int id = -1;
    std::wstring label;
    bool disabled = false;
    std::wstring badge;
    bool badgeOk = true;
};

struct ShellNavGroup
{
    std::wstring title;
    std::vector<ShellNavItem> items;
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
    void SetNavGroups(std::vector<ShellNavGroup> groups);
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

    // --- Hit test / hover (platform free, for backends to forward events) ---
    int  HitTestNav(const Point& p) const;   // nav item id, or -1
    void SetNavHover(int id) { m_hoverNav = id; }
    int  NavHover() const { return m_hoverNav; }

    // --- Paint ---
    void Paint(IPainter& painter, const Palette& pal, const Metrics& mx);

private:
    void PaintCaption(IPainter& painter, const Palette& pal, const Metrics& mx);
    void PaintStatus(IPainter& painter, const Palette& pal, const Metrics& mx);
    void PaintNav(IPainter& painter, const Palette& pal, const Metrics& mx);
    void LayoutNav(const Metrics& mx);

    std::vector<ShellNavGroup> m_groups;
    int m_activeNav = -1;
    int m_hoverNav  = -1;

    CaptionState m_caption;
    StatusState  m_status;

    Rect m_navRect;
    Rect m_captionRect;
    Rect m_statusRect;
    Rect m_contentRect;

    std::vector<Rect> m_navItemRects;   // parallel to flattened nav items
    std::vector<int>  m_navItemIds;
};

} // namespace ui
} // namespace air
