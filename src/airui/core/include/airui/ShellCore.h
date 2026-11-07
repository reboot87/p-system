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

    // Read-only views: the backend renders pixels from these states
    // instead of keeping its own duplicate copies.
    const CaptionState& Caption() const { return m_caption; }
    const StatusState&  Status()  const { return m_status; }
    const std::vector<ShellNavGroup>& NavGroups() const { return m_groups; }

    // --- Layout ---
    // The backend supplies the geometry it actually draws, so the
    // platform-free hit test and ContentRect never drift from pixels.
    // captionActionW: width reserved on the right of the caption bar for
    // backend-owned chrome actions (e.g. the renderer switch button), so
    // the badge never overlaps them. 0 = no reserved area.
    void SetShellGeometry(int captionH, int statusH, int navW,
                          int captionActionW = 0);
    void Layout(const Rect& client);
    Rect NavRect() const     { return m_navRect; }
    Rect CaptionRect() const { return m_captionRect; }
    Rect StatusRect() const  { return m_statusRect; }
    Rect ContentRect() const { return m_contentRect; }

    // Read-only layout views. A backend may need them to overlay a text
    // layer on top of the geometry it drew (see Shell::DrawCoreChromeText).
    const std::vector<Rect>& NavItemRects() const { return m_navItemRects; }
    const std::vector<int>&  NavItemIds()   const { return m_navItemIds; }

    // --- Hit test / hover (platform free, for backends to forward events) ---
    int  HitTestNav(const Point& p) const;   // nav item id, or -1
    void SetNavHover(int id) { m_hoverNav = id; }
    int  NavHover() const { return m_hoverNav; }

    // --- Nav scroll (items may not fit the nav height) ---
    void ScrollNav(int delta);               // + scrolls content down
    int  NavScroll() const    { return m_navScroll; }
    int  NavScrollMax() const { return m_navScrollMax; }

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

    int m_captionH = 36;
    int m_statusH  = 24;
    int m_navW     = 220;
    int m_captionActionW = 0;

    std::vector<Rect> m_navItemRects;   // parallel to flattened nav items
    std::vector<int>  m_navItemIds;
    int m_navScroll    = 0;
    int m_navScrollMax = 0;
    int m_navContentH  = 0;
};

} // namespace ui
} // namespace air
