// ============================================================
// AirUI - OverlayCore.cpp
// Platform-free overlay layers, focus order and focus trapping.
// ============================================================
#include "airui/OverlayCore.h"

#include <algorithm>

namespace air {
namespace ui {

namespace {

constexpr TextFormat kLeftText =
    dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;

} // namespace

// ============================================================
// Placement
// ============================================================

Rect PlaceOverlay(const Rect& anchor, const Size& size, const Rect& client,
                  OverlayPlacement where)
{
    Rect out{};
    if (client.Empty() || size.w <= 0 || size.h <= 0) return out;

    const int w = size.w;
    const int h = size.h;

    if (where == OverlayPlacement::Center) {
        out.left = client.left + (client.Width() - w) / 2;
        out.top  = client.top  + (client.Height() - h) / 2;
    } else if (where == OverlayPlacement::Below || where == OverlayPlacement::Above) {
        out.left = anchor.left;
        const bool belowFits = anchor.bottom + h <= client.bottom;
        const bool aboveFits = anchor.top - h >= client.top;
        if (where == OverlayPlacement::Below)
            out.top = (belowFits || !aboveFits) ? anchor.bottom : anchor.top - h;
        else
            out.top = (aboveFits || !belowFits) ? anchor.top - h : anchor.bottom;
    } else {
        out.top = anchor.top;
        const bool rightFits = anchor.right + w <= client.right;
        const bool leftFits  = anchor.left - w >= client.left;
        if (where == OverlayPlacement::Right)
            out.left = (rightFits || !leftFits) ? anchor.right : anchor.left - w;
        else
            out.left = (leftFits || !rightFits) ? anchor.left - w : anchor.right;
    }

    out.right  = out.left + w;
    out.bottom = out.top + h;

    if (out.right > client.right) {
        const int d = out.right - client.right;
        out.left -= d; out.right -= d;
    }
    if (out.left < client.left) {
        const int d = client.left - out.left;
        out.left += d; out.right += d;
    }
    if (out.right > client.right) out.right = client.right;

    if (out.bottom > client.bottom) {
        const int d = out.bottom - client.bottom;
        out.top -= d; out.bottom -= d;
    }
    if (out.top < client.top) {
        const int d = client.top - out.top;
        out.top += d; out.bottom += d;
    }
    if (out.bottom > client.bottom) out.bottom = client.bottom;

    return out;
}

// ============================================================
// OverlayStack
// ============================================================

int OverlayStack::Open(const OverlayItem& item)
{
    OverlayItem copy = item;
    if (copy.id < 0) copy.id = m_nextId++;
    else {
        // Re-opening the same id replaces the old layer.
        m_layers.erase(std::remove_if(m_layers.begin(), m_layers.end(),
                                      [id = copy.id](const OverlayItem& o) { return o.id == id; }),
                       m_layers.end());
        if (copy.id >= m_nextId) m_nextId = copy.id + 1;
    }
    copy.z = m_nextZ++;
    const int id = copy.id;
    m_layers.push_back(std::move(copy));
    std::stable_sort(m_layers.begin(), m_layers.end(),
                     [](const OverlayItem& a, const OverlayItem& b) { return a.z < b.z; });
    return id;
}

bool OverlayStack::Close(int id)
{
    const auto it = std::find_if(m_layers.begin(), m_layers.end(),
                                 [id](const OverlayItem& o) { return o.id == id; });
    if (it == m_layers.end()) return false;
    m_layers.erase(it);
    return true;
}

bool OverlayStack::CloseTop()
{
    if (m_layers.empty()) return false;
    m_layers.pop_back();
    return true;
}

void OverlayStack::Clear()
{
    m_layers.clear();
    m_nextZ = 1;
}

int OverlayStack::TopId() const
{
    return m_layers.empty() ? -1 : m_layers.back().id;
}

const OverlayItem* OverlayStack::Top() const
{
    return m_layers.empty() ? nullptr : &m_layers.back();
}

const OverlayItem* OverlayStack::Find(int id) const
{
    for (const auto& o : m_layers) if (o.id == id) return &o;
    return nullptr;
}

OverlayHit OverlayStack::Hit(Point pt) const
{
    OverlayHit out;
    const OverlayItem* top = Top();
    if (!top) {
        out.outsideTop = true;
        return out;
    }

    // A modal top layer owns the whole surface: hits outside its
    // bounds never reach lower layers or the page underneath.
    if (top->modal && !top->bounds.Contains(pt)) {
        out.blocked = true;
        out.outsideTop = true;
        out.id = top->id;
        return out;
    }

    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
        if (it->bounds.Contains(pt)) {
            out.inside = true;
            out.id = it->id;
            return out;
        }
    }
    out.outsideTop = true;
    out.id = top->id;
    return out;
}

// ============================================================
// FocusScope
// ============================================================

void FocusScope::SetEntries(std::vector<FocusEntry> entries)
{
    m_entries = std::move(entries);
    std::stable_sort(m_entries.begin(), m_entries.end(),
                     [](const FocusEntry& a, const FocusEntry& b) {
                         return a.tabIndex < b.tabIndex;
                     });
    if (!Find(m_focused)) m_focused = -1;
}

const FocusEntry* FocusScope::Find(int id) const
{
    if (id < 0) return nullptr;
    for (const auto& e : m_entries) if (e.id == id) return &e;
    return nullptr;
}

void FocusScope::UpdateBounds(int id, const Rect& bounds)
{
    for (auto& e : m_entries) {
        if (e.id == id) { e.bounds = bounds; return; }
    }
}

bool FocusScope::SetEnabled(int id, bool enabled)
{
    for (auto& e : m_entries) {
        if (e.id == id) {
            e.enabled = enabled;
            if (!enabled && m_focused == id) m_focused = -1;
            return true;
        }
    }
    return false;
}

bool FocusScope::SetFocus(int id)
{
    const FocusEntry* e = Find(id);
    if (!e || !e->enabled) return false;
    m_focused = id;
    return true;
}

int FocusScope::First(int group) const
{
    for (const auto& e : m_entries)
        if (e.group == group && e.enabled) return e.id;
    return -1;
}

int FocusScope::Last(int group) const
{
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it)
        if (it->group == group && it->enabled) return it->id;
    return -1;
}

int FocusScope::Next(int group, bool backward) const
{
    std::vector<int> ids;
    for (const auto& e : m_entries)
        if (e.group == group && e.enabled) ids.push_back(e.id);
    if (ids.empty()) return -1;

    const auto it = std::find(ids.begin(), ids.end(), m_focused);
    if (it == ids.end()) return backward ? ids.back() : ids.front();

    const int n = static_cast<int>(ids.size());
    const int i = static_cast<int>(it - ids.begin());
    int next = backward ? i - 1 : i + 1;
    if (next < 0) next = n - 1;
    if (next >= n) next = 0;
    return ids[next];
}

int FocusScope::FocusAt(Point pt, int group) const
{
    // Later tab order wins, matching the top-most layer convention.
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        if (it->group == group && it->enabled && it->bounds.Contains(pt))
            return it->id;
    }
    return -1;
}

Rect FocusScope::FocusRect() const
{
    const FocusEntry* e = Find(m_focused);
    return e ? e->bounds : Rect{};
}

// ============================================================
// OverlayController
// ============================================================

void OverlayController::SyncFocus(int preferredFocus)
{
    std::vector<FocusEntry> all;
    if (m_layerFocus.empty()) {
        m_group = 0;
        all = m_page;
    } else {
        const LayerFocus& lf = m_layerFocus.back();
        m_group = lf.group;
        all = m_page;
        for (const auto& e : lf.entries) all.push_back(e);
    }
    m_focus.SetEntries(std::move(all));

    int want = preferredFocus;
    if (want < 0) {
        want = m_layerFocus.empty() ? m_restoreFocus : m_focus.First(m_group);
    }
    if (want >= 0) m_focus.SetFocus(want);
}

void OverlayController::SetPageEntries(std::vector<FocusEntry> entries)
{
    // Build() may re-register page entries after a resize while a
    // layer is open: keep the current focus (and the remembered
    // page focus) instead of resetting the focus trap.
    const int keep = m_focus.Focused();
    m_page = std::move(entries);
    for (auto& e : m_page) e.group = 0;
    if (m_layerFocus.empty()) m_restoreFocus = -1;
    SyncFocus(keep);
    if (m_focus.Focused() < 0) m_focus.SetFocus(m_focus.First(0));
}

int OverlayController::Open(const OverlayItem& item,
                            std::vector<FocusEntry> layerEntries)
{
    if (m_stack.Count() == 0) m_restoreFocus = m_focus.Focused();

    const int id = m_stack.Open(item);
    if (item.focusGroup > 0) {
        for (auto& e : layerEntries) e.group = item.focusGroup;
        m_layerFocus.push_back(LayerFocus{id, item.focusGroup, std::move(layerEntries)});
    }
    SyncFocus(-1);
    return id;
}

bool OverlayController::Close(int id)
{
    if (!m_stack.Close(id)) return false;
    m_layerFocus.erase(
        std::remove_if(m_layerFocus.begin(), m_layerFocus.end(),
                       [id](const LayerFocus& lf) { return lf.overlayId == id; }),
        m_layerFocus.end());
    SyncFocus(-1);
    return true;
}

bool OverlayController::CloseTop()
{
    const int id = m_stack.TopId();
    return id >= 0 ? Close(id) : false;
}

void OverlayController::Clear()
{
    m_stack.Clear();
    m_layerFocus.clear();
    m_group = 0;
    SyncFocus(m_restoreFocus);
    m_restoreFocus = -1;
}

OverlayResult OverlayController::OnPointer(Point pt)
{
    OverlayResult out;
    const OverlayHit hit = m_stack.Hit(pt);

    if (hit.blocked) {
        out.action = OverlayAction::Blocked;
        out.id = m_stack.TopId();
        out.redraw = true;
        return out;
    }

    const OverlayItem* top = m_stack.Top();
    if (top && hit.outsideTop && top->closeOnOutside) {
        const int closed = top->id;
        Close(closed);
        out.action = OverlayAction::Dismiss;
        out.id = closed;
        out.redraw = true;
        return out;
    }

    // Report a hit even when the entry is already focused: the
    // caller must be able to consume the click (and activate a
    // layer item) instead of letting it fall through to the page.
    const int fid = m_focus.FocusAt(pt, m_group);
    if (fid >= 0) {
        m_focus.SetFocus(fid);
        out.action = OverlayAction::FocusChanged;
        out.id = fid;
        out.redraw = true;
    }
    return out;
}

OverlayResult OverlayController::OnKey(int vk, bool shift)
{
    OverlayResult out;

    if (vk == keys::Tab) {
        const int next = m_focus.Next(m_group, shift);
        if (next >= 0) {
            m_focus.SetFocus(next);
            out.action = OverlayAction::FocusChanged;
            out.id = next;
            out.redraw = true;
        }
        return out;
    }

    if (vk == keys::Escape && m_stack.Count() > 0) {
        const int closed = m_stack.TopId();
        CloseTop();
        out.action = OverlayAction::Dismiss;
        out.id = closed;
        out.redraw = true;
        return out;
    }

    if (vk == keys::Return || vk == keys::Space) {
        if (m_focus.Focused() >= 0) {
            out.action = OverlayAction::Activate;
            out.id = m_focus.Focused();
            out.redraw = true;
        }
    }
    return out;
}

// ============================================================
// Drawing helpers
// ============================================================

void PaintOverlayScrim(IPainter& g, const Rect& area, Color scrim, uint8_t alpha)
{
    if (area.Empty() || alpha == 0) return;
    g.FillRectAlpha(area, scrim, alpha);
}

void PaintOverlayFrame(IPainter& g, const OverlayItem& item,
                       const Palette& pal, const Metrics& mx)
{
    if (item.bounds.Empty()) return;
    g.FillRound(item.bounds, mx.radiusLg, pal.surface);
    g.StrokeRound(item.bounds, mx.radiusLg,
                  item.modal ? pal.primary : pal.border, 1);

    if (item.title.empty()) return;
    Rect head = item.bounds;
    head.left   += mx.unit;
    head.right  -= mx.unit;
    head.bottom  = head.top + mx.rowH;
    g.Text(head, item.title, FontRole::Title, pal.text, kLeftText);
    g.FillRect(Rect(head.left, head.bottom, head.right, head.bottom + 1), pal.split);
}

void PaintFocusRing(IPainter& g, const Rect& rc,
                    const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    g.StrokeRound(Rect(rc.left - 2, rc.top - 2, rc.right + 2, rc.bottom + 2),
                  mx.radiusMd + 2, pal.selectRing, 2);
}

Rect OverlayContentRect(const OverlayItem& item, const Metrics& mx)
{
    if (item.bounds.Empty()) return Rect{};
    Rect rc = item.bounds;
    rc.left   += mx.unit;
    rc.right  -= mx.unit;
    rc.top    += (item.title.empty() ? mx.unit : mx.rowH + mx.unit / 2);
    rc.bottom -= mx.unit;
    return rc;
}

} // namespace ui
} // namespace air
