#pragma once
// ============================================================
// AirUI - Controls.h  (public, platform-neutral)
// Page-author-facing control types.
//
// The native-only drawing helpers (DrawButton / NavBar / PaintDC /
// frame composition / list hit-testing) live in the private implementation
// header internal/Controls.h and never cross this boundary.
// ============================================================
#include "airui/types.h"
#include "airui/ControlsCore.h"
#include <string>
#include <vector>

namespace air {
namespace ui {

// ButtonStyle is defined in core (airui/ControlsCore.h); BtnStyle
// stays as a source-compatible alias for the native backend.
using BtnStyle = ButtonStyle;

// Read text from a native edit control (cross-process safe).
// The argument is an opaque native handle; the platform boundary
// casts it back inside the implementation.
std::wstring GetEditText(WindowHandle edit);

// ============================================================
// Sidebar navigation descriptor (used by Shell::SetNavGroups).
// ============================================================
struct NavItem
{
    int          id       = 0;      // business id (page id or command id)
    std::wstring glyph;             // Segoe MDL2 glyph
    std::wstring label;
    std::wstring badge;             // right-side badge text (empty = hidden)
    bool         badgeOk  = false;  // badge uses the "success" palette
    bool         disabled = false;
};

struct NavGroup
{
    std::wstring         title;     // group title (empty = no title row)
    std::vector<NavItem> items;
};

// ============================================================
// Persistent paint back buffer (platform-neutral handles).
// ============================================================
struct BackBuffer
{
    DeviceContextHandle mem = nullptr;
    BitmapHandle        bmp = nullptr;
    DrawObjectHandle    old = nullptr;
    int                 w   = 0;
    int                 h   = 0;
    // Release native resources held by the buffer (defined in the backend).
    void Release();
};

// ============================================================
// NavBar - sidebar navigation state (platform-neutral).
// The Shell owns one; drawing is done by the private backend.
// ============================================================
class NavBar
{
public:
    void SetGroups(std::vector<NavGroup> groups);
    void Layout(const Rect& side);
    void Paint(DeviceContextHandle dc, const class Fonts& fonts);
    int  HitTest(Point pt) const;
    bool OnMouseMove(Point pt);
    bool OnMouseLeave();

    void   SetActive(int id) { m_active = id; }
    int    Active() const { return m_active; }
    bool   IsDisabled(int id) const;
    void   SetBadge(int id, const std::wstring& text, bool ok);
    int    ContentHeight() const { return m_contentH; }
    Rect   Bounds() const { return m_side; }
    bool   Empty() const { return m_groups.empty(); }

private:
    struct ItemRect { int id; Rect rc; bool disabled; };
    struct GroupRect { std::wstring title; Rect rc; };

    void RebuildRects();

    std::vector<NavGroup>  m_groups;
    std::vector<ItemRect>  m_items;
    std::vector<GroupRect> m_groupsRect;
    Rect m_side        = {};
    int  m_active      = -1;
    int  m_hoverId     = -1;
    int  m_contentH    = 0;
};

// ============================================================
// Empty state - the three-line guide shown in an empty list.
// ============================================================
struct EmptyState
{
    std::wstring title;   // main heading
    std::wstring steps;   // steps (may contain newlines)
    std::wstring foot;    // muted footer
};

} // namespace ui
} // namespace air
