#pragma once
// ============================================================
// AirUI - OverlayCore.h
// Platform-free overlay layers, focus order and focus trapping.
//
// Why this exists: every Paint* control is a pure function of
// (rect, state, palette, metrics). Popovers, dropdowns and dialogs
// also need a *draw order*, *hit interception* and *focus order*,
// which a single control cannot express. This module keeps those
// three concerns in core so the backend only forwards points and
// key codes:
//
//   OverlayStack       ordered layers, top-most hit, modal blocking
//   FocusScope         tab order, cycling, skip disabled, focus ring
//   OverlayController  focus trap + focus restore + input results
//
// Coordinates are logical pixels in whatever space the caller
// uses for its controls (the gallery uses content space). The
// module never uses a platform type or a native key constant.
// ============================================================
#include "airui/types.h"
#include "airui/hal.h"
#include "airui/Theme.h"

#include <string>
#include <vector>

namespace air {
namespace ui {

// ------------------------------------------------------------
// Overlay layers
// ------------------------------------------------------------
enum class OverlayKind
{
    Tooltip,
    Popover,
    Dropdown,
    Modal
};

enum class OverlayPlacement
{
    Below,
    Above,
    Right,
    Left,
    Center
};

struct OverlayItem
{
    int  id = -1;                       // < 0: assigned by Open()
    OverlayKind kind = OverlayKind::Popover;
    Rect bounds{};                      // placed layer rectangle
    Rect anchor{};                      // rectangle that opened it
    std::wstring title;
    bool modal = false;                 // blocks every hit outside bounds
    bool closeOnOutside = true;
    bool scrim = false;                 // ask the caller to dim the area
    int  focusGroup = 0;                // > 0: this layer owns a focus trap
    int  z = 0;                         // assigned by Open()
};

struct OverlayHit
{
    int  id = -1;                       // layer under the point, -1 if none
    bool inside = false;                // point lies in any layer
    bool outsideTop = false;            // point lies outside every layer
    bool blocked = false;               // a modal layer swallows the hit
};

// Place size next to anchor, flipping to the opposite side when the
// preferred side has no room, then clamping into client. Center
// ignores anchor and centres in client.
Rect PlaceOverlay(const Rect& anchor, const Size& size, const Rect& client,
                  OverlayPlacement where);

class OverlayStack
{
public:
    // Returns the assigned id. Layers() is bottom-to-top by z.
    int  Open(const OverlayItem& item);
    bool Close(int id);
    bool CloseTop();
    void Clear();

    int  Count() const { return static_cast<int>(m_layers.size()); }
    int  TopId() const;
    const OverlayItem* Top() const;
    const OverlayItem* Find(int id) const;
    const std::vector<OverlayItem>& Layers() const { return m_layers; }

    // Top-most layer wins. A modal top layer blocks everything
    // outside its bounds, even other layers below it.
    OverlayHit Hit(Point pt) const;

private:
    std::vector<OverlayItem> m_layers;
    int m_nextId = 1;
    int m_nextZ  = 1;
};

// ------------------------------------------------------------
// Focus order
// ------------------------------------------------------------
struct FocusEntry
{
    int  id = -1;
    Rect bounds{};
    int  tabIndex = 0;                  // ascending; ties keep registration order
    int  group = 0;                     // 0 = page, > 0 = overlay layer
    bool enabled = true;
    std::wstring name;                  // status line / accessibility label
};

class FocusScope
{
public:
    // Stable-sorts by tabIndex. A focused id that disappears is cleared.
    void SetEntries(std::vector<FocusEntry> entries);
    void UpdateBounds(int id, const Rect& bounds);
    bool SetEnabled(int id, bool enabled);

    const std::vector<FocusEntry>& Entries() const { return m_entries; }
    int  Count() const { return static_cast<int>(m_entries.size()); }
    const FocusEntry* Find(int id) const;

    int  Focused() const { return m_focused; }
    bool SetFocus(int id);              // false if unknown or disabled
    int  First(int group) const;
    int  Last(int group) const;
    int  Next(int group, bool backward) const;  // cycles within group
    int  FocusAt(Point pt, int group) const;    // later tab order wins
    Rect FocusRect() const;

private:
    std::vector<FocusEntry> m_entries;
    int m_focused = -1;
};

// Numeric values match the usual virtual-key values for these keys,
// so a backend may pass its native code straight through. Core does
// not include a platform header to get the constants.
namespace keys {
constexpr int Tab    = 9;
constexpr int Return = 13;
constexpr int Escape = 27;
constexpr int Space  = 32;
} // namespace keys

enum class OverlayAction
{
    None,
    Dismiss,        // a layer was closed by outside click or escape
    FocusChanged,   // id is the new focus entry
    Activate,       // id is the focused entry to activate
    Blocked         // a modal layer swallowed the hit
};

struct OverlayResult
{
    OverlayAction action = OverlayAction::None;
    int  id = -1;
    bool redraw = false;
};

// ------------------------------------------------------------
// OverlayController - composes layers + focus.
//
// Opening a layer with focusGroup > 0 traps Tab inside that layer
// and remembers the page focus. Closing the last layer restores it,
// so a dialog never leaves focus stranded.
// ------------------------------------------------------------
class OverlayController
{
public:
    void SetPageEntries(std::vector<FocusEntry> entries);

    int  Open(const OverlayItem& item,
              std::vector<FocusEntry> layerEntries = {});
    bool Close(int id);
    bool CloseTop();
    void Clear();

    OverlayResult OnPointer(Point pt);
    OverlayResult OnKey(int vk, bool shift);

    int  ActiveGroup() const { return m_group; }
    int  FocusedId() const { return m_focus.Focused(); }
    Rect FocusRect() const { return m_focus.FocusRect(); }
    const FocusEntry* FocusedEntry() const { return m_focus.Find(m_focus.Focused()); }

    const OverlayStack& Stack() const { return m_stack; }
    OverlayStack&       Stack()       { return m_stack; }
    const FocusScope&   Focus() const { return m_focus; }

private:
    struct LayerFocus
    {
        int overlayId = -1;
        int group = 0;
        std::vector<FocusEntry> entries;
    };

    void SyncFocus(int preferredFocus);

    OverlayStack m_stack;
    FocusScope   m_focus;
    std::vector<FocusEntry> m_page;
    std::vector<LayerFocus> m_layerFocus;
    int m_group = 0;
    int m_restoreFocus = -1;
};

// ------------------------------------------------------------
// Drawing helpers - the caller picks the scrim colour and opacity.
// Backends without alpha blending degrade to an opaque fill.
// ------------------------------------------------------------
void PaintOverlayScrim(IPainter& g, const Rect& area, Color scrim,
                       uint8_t alpha = 96);
void PaintOverlayFrame(IPainter& g, const OverlayItem& item,
                       const Palette& pal, const Metrics& mx);
void PaintFocusRing(IPainter& g, const Rect& rc,
                    const Palette& pal, const Metrics& mx);

// Area below the optional title, inset by one unit.
Rect OverlayContentRect(const OverlayItem& item, const Metrics& mx);

} // namespace ui
} // namespace air
