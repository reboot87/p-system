#include "airui/WidgetControls.h"
// ============================================================
// AirUI - ControlsLayout.cpp
// Platform-free layout controls: Accordion / Anchor / Splitter /
// ColumnResize. Every control is a pure function of (rect, state,
// palette, metrics) and talks to the backend only through IPainter.
// Each control owns one geometry helper shared by the hit test and
// the painter. Colours come from pal.*, sizes from mx.*; nothing here
// depends on a platform API or a native handle.
// ============================================================
#include "airui/ControlsExtra.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace air {
namespace ui {

namespace {

// Single-line formats reused by every control. Long strings carry
// EndEllipsis; every Text() call carries NoPrefix.
constexpr TextFormat kLeftText =
    dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kCenterText =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix;

// A one-pixel inset used as the clip region for framed panels.
Rect ClipInset(const Rect& r)
{
    return Rect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1);
}

// Shared rounded frame for the layout panels.
void PaintFrame(IPainter& g, const Rect& rc, const Palette& pal, const Metrics& mx)
{
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
}

Color BodyColor(const ControlState& st, const Palette& pal)
{
    return st.disabled ? pal.textDisabled : pal.text;
}

Color MutedColor(const ControlState& st, const Palette& pal)
{
    return st.disabled ? pal.textDisabled : pal.textSub;
}

// ============================================================
// Accordion geometry
//
// A flat list of header bands interleaved with the bodies of the
// expanded panels. Header and body heights are pure functions of the
// metrics, so AccordionContentHeight (which has no rect / painter) can
// reproduce exactly the same vertical stack.
// ============================================================
struct AccordionGeom
{
    std::vector<Rect> headers;   // one per panel
    std::vector<Rect> bodies;    // empty rect when collapsed
    int headerH = 0;
    int bodyH   = 0;
};

int AccHeaderHeight(const Metrics& mx)
{
    return mx.rowH + mx.unit;
}

int AccBodyHeight(const Metrics& mx)
{
    return mx.rowH * 2 + mx.unit;
}

// Missing entries in st.expanded are treated as collapsed.
bool AccExpanded(const AccordionState& st, int i)
{
    return i < static_cast<int>(st.expanded.size()) &&
           st.expanded[static_cast<std::size_t>(i)];
}

AccordionGeom MeasureAccordion(const Rect& rc, const AccordionState& st,
                               const Metrics& mx)
{
    AccordionGeom out;
    out.headerH = AccHeaderHeight(mx);
    out.bodyH   = AccBodyHeight(mx);
    const int n = static_cast<int>(st.panels.size());
    if (n == 0 || rc.Empty()) return out;

    int y = rc.top;
    for (int i = 0; i < n; ++i) {
        out.headers.push_back(Rect(rc.left, y, rc.right, y + out.headerH));
        y += out.headerH;
        if (AccExpanded(st, i)) {
            out.bodies.push_back(Rect(rc.left, y, rc.right, y + out.bodyH));
            y += out.bodyH;
        } else {
            out.bodies.push_back(Rect());
        }
    }
    return out;
}

// ============================================================
// Anchor geometry
//
// A slim vertical rail on the left plus one full-width row per item.
// ============================================================
struct AnchorGeom
{
    std::vector<Rect> rows;
    Rect rail{};
    int  rowH      = 0;
    int  labelLeft = 0;
};

AnchorGeom MeasureAnchor(const Rect& rc, const AnchorState& st, const Metrics& mx)
{
    AnchorGeom out;
    const int n = static_cast<int>(st.items.size());
    if (n == 0 || rc.Empty()) return out;

    out.rowH = mx.rowH + mx.unit / 2;
    const int railW = (std::max)(2, mx.unit / 2);
    const int railX = rc.left + mx.unit;
    out.rail = Rect(railX, rc.top, railX + railW, rc.bottom);
    out.labelLeft = out.rail.right + mx.unit;

    for (int i = 0; i < n; ++i) {
        const int y = rc.top + i * out.rowH;
        out.rows.push_back(Rect(rc.left, y, rc.right, y + out.rowH));
    }
    return out;
}

// Three short marks centred on a splitter handle. The dots run along
// the long axis of the handle so the grip reads on both orientations.
void PaintHandleGrip(IPainter& g, const Rect& handle, bool vertical, Color c)
{
    const int dot  = 3;
    const int step = dot * 3;
    const int cx = handle.left + handle.Width() / 2;
    const int cy = handle.top + handle.Height() / 2;
    for (int k = -1; k <= 1; ++k) {
        Rect d;
        if (vertical) {
            const int y = cy + k * step - dot / 2;
            d = Rect(cx - dot / 2, y, cx - dot / 2 + dot, y + dot);
        } else {
            const int x = cx + k * step - dot / 2;
            d = Rect(x, cy - dot / 2, x + dot, cy - dot / 2 + dot);
        }
        g.FillRect(d, c);
    }
}

} // namespace

// ============================================================
// Accordion
// ============================================================



int AccordionContentHeight(const AccordionState& st, const Metrics& mx)
{
    const int headerH = AccHeaderHeight(mx);
    const int bodyH   = AccBodyHeight(mx);
    const int n = static_cast<int>(st.panels.size());
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += headerH;
        if (AccExpanded(st, i)) total += bodyH;
    }
    return total;
}



// ============================================================
// Anchor
// ============================================================

int AnchorHit(const Rect& rc, const AnchorState& st,
              const Metrics& mx, Point pt)
{
    if (rc.Empty() || st.items.empty() || !rc.Contains(pt)) return -1;
    const AnchorGeom geom = MeasureAnchor(rc, st, mx);
    if (geom.rowH <= 0) return -1;
    const int row = (pt.y - rc.top) / geom.rowH;
    if (row < 0 || row >= static_cast<int>(geom.rows.size())) return -1;
    if (!geom.rows[static_cast<std::size_t>(row)].Contains(pt)) return -1;
    return row;
}



// ============================================================
// Splitter
//
// st.position is the distance from the leading edge (left for a
// vertical split, top for a horizontal one) to the divider centre;
// 0 means "centred". The handle is a narrow band between the two
// panes and is clamped so it always stays fully inside rc.
// ============================================================



bool SplitterHandleHit(const Rect& rc, const SplitterState& st,
                       const Metrics& mx, Point pt)
{
    if (rc.Empty() || !rc.Contains(pt)) return false;
    const SplitterLayout lay = MeasureSplitter(rc, st, mx);
    return lay.handle.Contains(pt);
}



// ============================================================
// ColumnResize
//
// st.widths are relative weights. Columns are laid out across the
// full width of rc (the last one absorbs rounding error) and a narrow
// drag handle is centred on each internal boundary.
// ============================================================







} // namespace ui
} // namespace air

// ============================================================
// Object layer: widget member functions migrated from the
// free-function drawing kernel above.
// ============================================================
namespace air {
namespace ui {
namespace widgets {

int Accordion::HeaderAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const AccordionState& st = state;
    (void)mx;
    (void)pt;
    if (rc.Empty() || st.panels.empty() || !rc.Contains(pt)) return -1;
    const AccordionGeom geom = MeasureAccordion(rc, st, mx);
    for (int i = 0; i < static_cast<int>(geom.headers.size()); ++i) {
        if (geom.headers[static_cast<std::size_t>(i)].Contains(pt)) return i;
    }
    return -1;
}

void Accordion::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    AccordionState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const AccordionGeom geom = MeasureAccordion(rc, st, mx);
    const int pad = mx.unit;
    const Color title = BodyColor(st.base, pal);
    const Color muted = MutedColor(st.base, pal);
    const int maxLines = (std::max)(1, geom.bodyH / (mx.fontBody + 6));

    PaintFrame(g, rc, pal, mx);
    g.PushClip(ClipInset(rc));

    const int count = static_cast<int>(geom.headers.size());
    for (int i = 0; i < count; ++i) {
        const Rect& head = geom.headers[static_cast<std::size_t>(i)];
        if (head.top >= rc.bottom) break;

        const bool hov = (i == st.hovered && !st.base.disabled);
        if (hov) g.FillRect(head, pal.hover);

        const bool expanded = AccExpanded(st, i);
        const Rect glyphRc(head.left + pad, head.top,
                           head.left + pad + mx.rowH, head.bottom);
        g.Text(glyphRc, expanded ? L"\u25BE" : L"\u25B8",
               FontRole::Meta, muted, kCenterText);
        g.Text(Rect(head.left + pad + mx.rowH, head.top,
                    head.right - pad, head.bottom),
               st.panels[static_cast<std::size_t>(i)].first,
               FontRole::Body, title, kLeftText);
        g.FillRect(Rect(rc.left, head.bottom - 1, rc.right, head.bottom), pal.split);

        const Rect& bodyRc = geom.bodies[static_cast<std::size_t>(i)];
        if (expanded && !bodyRc.Empty() && bodyRc.top < rc.bottom) {
            g.FillRect(Rect(bodyRc.left, bodyRc.top, bodyRc.right,
                            (std::min)(bodyRc.bottom, rc.bottom)), pal.sunken);
            g.TextBlock(Rect(bodyRc.left + pad, bodyRc.top,
                             bodyRc.right - pad, bodyRc.bottom),
                        st.panels[static_cast<std::size_t>(i)].second,
                        FontRole::Body, muted, maxLines);
        }
    }

    g.PopClip();
}

void Anchor::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    AnchorState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const AnchorGeom geom = MeasureAnchor(rc, st, mx);
    const Color normal = MutedColor(st.base, pal);
    const Color activeC = st.base.disabled ? pal.textDisabled : pal.primary;

    PaintFrame(g, rc, pal, mx);
    g.PushClip(ClipInset(rc));

    if (!geom.rail.Empty()) g.FillRect(geom.rail, pal.split);

    const int count = static_cast<int>(geom.rows.size());
    for (int i = 0; i < count; ++i) {
        const Rect& row = geom.rows[static_cast<std::size_t>(i)];
        if (row.top >= rc.bottom) break;

        const bool isActive = (i == st.active);
        const bool hov = (i == st.hovered && !st.base.disabled);
        if (hov) g.FillRect(row, pal.hover);

        if (isActive) {
            const int barH = (std::max)(geom.rowH / 2, mx.unit);
            const int cy = row.top + geom.rowH / 2;
            g.FillRect(Rect(geom.rail.left - 2, cy - barH / 2,
                            geom.rail.right + 2, cy + barH / 2), pal.primary);
        }

        const Color c = isActive ? activeC : normal;
        g.Text(Rect(geom.labelLeft, row.top, rc.right - mx.unit, row.bottom),
               st.items[static_cast<std::size_t>(i)],
               FontRole::Body, c, kLeftText);
    }

    g.PopClip();
}

SplitterLayout Splitter::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const SplitterState& st = state;
    (void)mx;
    SplitterLayout out;
    if (rc.Empty()) return out;

    int hw = (std::max)(3, mx.unit);
    if (st.vertical) hw = (std::min)(hw, rc.Width());
    else             hw = (std::min)(hw, rc.Height());
    if (hw < 1) hw = 1;

    if (st.vertical) {
        const int center = (st.position == 0)
                         ? rc.left + rc.Width() / 2
                         : rc.left + st.position;
        int hLeft = center - hw / 2;
        hLeft = (std::max)(rc.left, (std::min)(hLeft, rc.right - hw));
        const int hRight = hLeft + hw;
        out.first  = Rect(rc.left, rc.top, hLeft, rc.bottom);
        out.handle = Rect(hLeft, rc.top, hRight, rc.bottom);
        out.second = Rect(hRight, rc.top, rc.right, rc.bottom);
    } else {
        const int center = (st.position == 0)
                         ? rc.top + rc.Height() / 2
                         : rc.top + st.position;
        int hTop = center - hw / 2;
        hTop = (std::max)(rc.top, (std::min)(hTop, rc.bottom - hw));
        const int hBottom = hTop + hw;
        out.first  = Rect(rc.left, rc.top, rc.right, hTop);
        out.handle = Rect(rc.left, hTop, rc.right, hBottom);
        out.second = Rect(rc.left, hBottom, rc.right, rc.bottom);
    }
    return out;
}

void Splitter::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    SplitterState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (rc.Empty()) return;
    const SplitterLayout lay = MeasureSplitter(rc, st, mx);

    PaintFrame(g, rc, pal, mx);
    g.PushClip(ClipInset(rc));

    g.FillRect(lay.first, pal.surface);
    g.FillRect(lay.second, pal.sunken);

    const Color handleBg = st.dragging ? pal.primary
                          : st.hovered  ? pal.hover
                                        : pal.sunken;
    g.FillRect(lay.handle, handleBg);
    g.StrokeRect(lay.handle, pal.split, 1);
    const Color grip = st.dragging ? pal.textInverse : pal.primaryText;
    PaintHandleGrip(g, lay.handle, st.vertical, grip);

    g.PopClip();
}

ColumnResizeLayout ColumnResize::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const ColumnResizeState& st = state;
    (void)mx;
    ColumnResizeLayout out;
    const int n = static_cast<int>(st.widths.size());
    if (n == 0 || rc.Empty()) return out;

    long long sum = 0;
    for (int w : st.widths) sum += (w > 0 ? w : 1);

    const int total = rc.Width();
    int x = rc.left;
    for (int i = 0; i < n; ++i) {
        const int raw = st.widths[static_cast<std::size_t>(i)];
        const int weight = raw > 0 ? raw : 1;
        int w = (i == n - 1)
              ? (rc.right - x)
              : static_cast<int>(static_cast<long long>(total) * weight / sum);
        if (x + w > rc.right) w = rc.right - x;
        if (w < 1) w = 1;
        out.columns.push_back(Rect(x, rc.top, x + w, rc.bottom));
        x += w;
    }

    const int hw = (std::max)(3, mx.unit / 2);
    for (int i = 0; i < n - 1; ++i) {
        const int bx = out.columns[static_cast<std::size_t>(i)].right;
        const int hx = bx - hw / 2;
        out.handles.push_back(Rect(hx, rc.top, hx + hw, rc.bottom));
    }
    return out;
}

int ColumnResize::HandleAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const ColumnResizeState& st = state;
    (void)mx;
    (void)pt;
    if (rc.Empty() || st.widths.empty() || !rc.Contains(pt)) return -1;
    const ColumnResizeLayout lay = MeasureColumnResize(rc, st, mx);
    for (int i = 0; i < static_cast<int>(lay.handles.size()); ++i) {
        if (lay.handles[static_cast<std::size_t>(i)].Contains(pt)) return i;
    }
    return -1;
}

void ColumnResize::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    ColumnResizeState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (rc.Empty()) return;
    const ColumnResizeLayout lay = MeasureColumnResize(rc, st, mx);
    if (lay.columns.empty()) return;

    PaintFrame(g, rc, pal, mx);
    g.PushClip(ClipInset(rc));

    // Faint alternating bands make the column boundaries readable.
    for (int i = 0; i < static_cast<int>(lay.columns.size()); ++i) {
        const Color band = (i % 2 == 0) ? pal.surface : pal.sunken;
        g.FillRect(lay.columns[static_cast<std::size_t>(i)], band);
    }

    for (int i = 0; i < static_cast<int>(lay.handles.size()); ++i) {
        const bool active = (i == st.dragging) || (i == st.hovered);
        const Rect& h = lay.handles[static_cast<std::size_t>(i)];
        g.FillRect(h, active ? pal.primary : pal.split);
        if (active) g.StrokeRect(h, pal.primaryHover, 1);
    }

    g.PopClip();
}

} // namespace widgets
} // namespace ui
} // namespace air
