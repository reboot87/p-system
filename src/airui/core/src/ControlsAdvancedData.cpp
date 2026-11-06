#include "airui/WidgetControls.h"
// AirUI - ControlsAdvancedData.cpp : platform-free data views.
// Third wave: AvatarGroup, Heatmap, CodeBlock, KanbanBoard, GanttChart.
// The painter has no path/image primitive, so everything is built from
// filled and stroked rectangles plus text.
#include "airui/ControlsAdvanced.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace air {
namespace ui {
namespace {

constexpr TextFormat kLeft =
    dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kRight =
    dt::SingleLine | dt::Right | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kCenter =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;

int ClampI(int v, int lo, int hi)
{
    return (std::min)((std::max)(v, lo), hi);
}

bool RectHit(const Rect& r, Point p)
{
    return !r.Empty() && r.Contains(p);
}

struct KindColors
{
    Color accent, soft, text;
};

KindColors ColorsFor(AlertKind kind, const Palette& pal)
{
    switch (kind) {
    case AlertKind::Success: return KindColors{pal.ok, pal.okSoft, pal.okText};
    case AlertKind::Warning: return KindColors{pal.warn, pal.warnSoft, pal.warnText};
    case AlertKind::Error:   return KindColors{pal.error, pal.errorSoft, pal.errorText};
    default:                 return KindColors{pal.primary, pal.primarySoft, pal.primaryText};
    }
}

// ------------------------------------------------------------
// AvatarGroup
// ------------------------------------------------------------
struct AvatarGeo
{
    std::vector<Rect> cells;
    Rect more{};
    int diameter = 0;
};

AvatarGeo GeoAvatars(const Rect& rc, const AvatarGroupState& st,
                     const Metrics& mx)
{
    AvatarGeo o;
    if (rc.Empty()) return o;
    const int pad = mx.unit;
    const int d = st.size > 0 ? st.size
                              : (std::min)(rc.Height() - pad * 2,
                                           mx.itemH / 2 + mx.unit);
    if (d <= 0 || rc.Width() <= pad * 2) return o;
    const int total = static_cast<int>(st.items.size());
    if (total <= 0) return o;
    const int maxVisible = (std::max)(0, st.maxVisible);
    int visible = (std::min)(total, maxVisible);
    bool needMore = total > visible;
    int step = d - (std::max)(2, d / 4);
    const int availW = rc.Width() - pad * 2;
    auto WidthNeeded = [&](int count, bool more, int stride) {
        const int slots = count + (more ? 1 : 0);
        return d + ((std::max)(0, slots - 1)) * stride;
    };
    while (visible > 0 && WidthNeeded(visible, needMore, step) > availW) {
        --visible;
        needMore = true;
    }
    if (visible == 0) {
        step = (std::max)(d / 3, 1);
        while (visible < total &&
               WidthNeeded(visible + 1, total > visible + 1, step) <= availW) {
            ++visible;
        }
        needMore = total > visible;
    }
    if (visible == 0 && !needMore) return o;
    o.diameter = d;
    const int startX = rc.left + pad;
    const int top = rc.top + (rc.Height() - d) / 2;
    for (int i = 0; i < visible; ++i) {
        const int x = startX + i * step;
        o.cells.push_back(Rect(x, top, x + d, top + d));
    }
    if (needMore) {
        const int x = startX + visible * step;
        o.more = Rect(x, top, x + d, top + d);
    }
    return o;
}

// ------------------------------------------------------------
// GanttChart
// ------------------------------------------------------------
struct GanttGeo
{
    GanttLayout lay{};
    std::vector<Rect> rowRects;
};

GanttGeo GeoGantt(const Rect& rc, const GanttState& st, const Metrics& mx)
{
    GanttGeo o;
    if (rc.Empty()) return o;
    const int pad = mx.unit;
    const int days = (std::max)(1, st.days);
    const int headerH = mx.rowH;
    if (rc.Height() <= pad * 2 + headerH || rc.Width() <= pad * 3) return o;
    const int labelW = (std::min)((rc.Width() - pad * 2) / 3, mx.unit * 16);
    const int timelineL = rc.left + pad + labelW;
    const int timelineR = rc.right - pad;
    if (timelineR <= timelineL) return o;
    o.lay.dayW = (timelineR - timelineL) / days;
    if (o.lay.dayW < 1) return o;
    o.lay.labelW = labelW;
    o.lay.header = Rect(rc.left + pad, rc.top + pad,
                        rc.right - pad, rc.top + pad + headerH);
    o.lay.rows = Rect(rc.left + pad, o.lay.header.bottom,
                      rc.right - pad, rc.bottom - pad);
    o.lay.labelArea = Rect(o.lay.rows.left, o.lay.rows.top,
                           timelineL, o.lay.rows.bottom);
    if (o.lay.rows.Empty()) return o;
    const int n = static_cast<int>(st.tasks.size());
    o.lay.rowH = (std::max)(mx.unit * 2, mx.rowH);
    if (n > 0 && o.lay.rows.Height() / n < o.lay.rowH) {
        o.lay.rowH = (std::max)(mx.unit * 2, o.lay.rows.Height() / n);
    }
    for (int i = 0; i < n; ++i) {
        const int y = o.lay.rows.top + i * o.lay.rowH;
        const int bottom = (std::min)(y + o.lay.rowH, o.lay.rows.bottom);
        o.rowRects.push_back(Rect(o.lay.rows.left, y, o.lay.rows.right, bottom));
        const GanttTask& t = st.tasks[static_cast<std::size_t>(i)];
        const int s = ClampI(t.start, 0, days - 1);
        const int len = ClampI(t.duration, 1, days - s);
        const int x1 = ClampI(timelineL + s * o.lay.dayW, timelineL, timelineR);
        const int x2 = ClampI(timelineL + (s + len) * o.lay.dayW, x1, timelineR);
        const int inset = (std::max)(1, mx.unit / 4);
        o.lay.bars.push_back(Rect(x1, y + inset, x2,
                                  (std::max)(y + inset + 1, bottom - inset)));
    }
    return o;
}

} // namespace















bool CodeBlockCopyHit(const Rect& rc, const CodeBlockState& st,
                      const Metrics& mx, Point pt)
{
    if (!RectHit(rc, pt)) return false;
    return RectHit(MeasureCodeBlock(rc, st, mx).copy, pt);
}















} // namespace ui
} // namespace air

// ============================================================
// Object layer: widget member functions migrated from the
// free-function drawing kernel above.
// ============================================================
namespace air {
namespace ui {
namespace widgets {

AvatarGroupLayout AvatarGroup::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const AvatarGroupState& st = state;
    (void)mx;
    const AvatarGeo geo = GeoAvatars(rc, st, mx);
    AvatarGroupLayout out;
    out.cells = geo.cells;
    out.more = geo.more;
    out.diameter = geo.diameter;
    return out;
}

int AvatarGroup::At(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const AvatarGroupState& st = state;
    (void)mx;
    (void)pt;
    if (!RectHit(rc, pt)) return -1;
    const AvatarGeo geo = GeoAvatars(rc, st, mx);
    if (RectHit(geo.more, pt)) return -2;
    for (int i = static_cast<int>(geo.cells.size()) - 1; i >= 0; --i) {
        if (RectHit(geo.cells[static_cast<std::size_t>(i)], pt)) return i;
    }
    return -1;
}

void AvatarGroup::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    AvatarGroupState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const AvatarGeo geo = GeoAvatars(rc, st, mx);
    if (geo.diameter <= 0) return;
    const Color tints[] = { pal.primary, pal.ok, pal.warn, pal.error };
    g.PushClip(rc);
    for (std::size_t i = 0; i < geo.cells.size(); ++i) {
        const Rect& cell = geo.cells[i];
        const Color tint = st.items[i].tint != 0
                         ? st.items[i].tint
                         : tints[i % 4];
        g.FillRound(cell, geo.diameter / 2, tint);
        g.StrokeRound(cell, geo.diameter / 2,
                      st.hovered == static_cast<int>(i) ? pal.selectRing : pal.surface,
                      st.hovered == static_cast<int>(i) ? 2 : 1);
        const std::wstring label = st.items[i].text.empty()
                                 ? L"?" : st.items[i].text.substr(0, 1);
        g.Text(cell, label, FontRole::Title, pal.textInverse, kCenter);
    }
    if (!geo.more.Empty()) {
        g.FillRound(geo.more, geo.diameter / 2, pal.sunken);
        g.StrokeRound(geo.more, geo.diameter / 2, pal.border, 1);
        const int extra = static_cast<int>(st.items.size()) -
                          static_cast<int>(geo.cells.size());
        g.Text(geo.more, L"+" + std::to_wstring((std::max)(0, extra)),
               FontRole::Meta, pal.textSub, kCenter);
    }
    g.PopClip();
}

HeatmapLayout Heatmap::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const HeatmapState& st = state;
    (void)mx;
    HeatmapLayout out;
    if (rc.Empty()) return out;
    const int pad = mx.unit;
    const int weeks = ClampI(st.weeks, 1, 60);
    const int gap = (std::max)(1, mx.unit / 4);
    const int titleH = st.title.empty() ? 0 : mx.rowH;
    const int legendH = st.showLegend ? mx.rowH : 0;
    const int availTop = rc.top + pad + titleH;
    const int availBottom = rc.bottom - pad - legendH;
    const int rowsAvail = availBottom - availTop - gap * 6;
    const int colsAvail = rc.Width() - pad * 2 - gap * (weeks - 1);
    if (rowsAvail <= 0 || colsAvail <= 0) return out;
    const int cell = (std::min)(rowsAvail / 7, colsAvail / weeks);
    if (cell < 2) return out;
    out.cell = cell;
    out.gap = gap;
    const int gridW = weeks * cell + gap * (weeks - 1);
    const int gridH = 7 * cell + gap * 6;
    const int gx = rc.left + pad + ((rc.Width() - pad * 2 - gridW) / 2);
    out.grid = Rect((std::max)(rc.left + pad, gx), availTop,
                    (std::max)(rc.left + pad, gx) + gridW, availTop + gridH);
    if (st.showLegend) {
        out.legend = Rect(rc.left + pad, rc.bottom - pad - legendH,
                          rc.right - pad, rc.bottom - pad);
    }
    return out;
}

HeatmapCell Heatmap::CellAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const HeatmapState& st = state;
    (void)mx;
    (void)pt;
    HeatmapCell out;
    const HeatmapLayout lay = MeasureHeatmap(rc, st, mx);
    if (!RectHit(lay.grid, pt) || lay.cell <= 0) return out;
    const int pitch = lay.cell + lay.gap;
    const int week = (pt.x - lay.grid.left) / pitch;
    const int day = (pt.y - lay.grid.top) / pitch;
    if (week < 0 || week >= st.weeks || day < 0 || day >= 7) return out;
    if (pt.x >= lay.grid.left + week * pitch + lay.cell) return out;
    if (pt.y >= lay.grid.top + day * pitch + lay.cell) return out;
    out.week = week;
    out.day = day;
    return out;
}

void Heatmap::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    HeatmapState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const HeatmapLayout lay = MeasureHeatmap(rc, st, mx);
    if (lay.grid.Empty()) return;
    g.PushClip(rc);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    if (!st.title.empty()) {
        g.Text(Rect(rc.left + mx.unit, rc.top + mx.unit / 2,
                    rc.right - mx.unit, rc.top + mx.unit / 2 + mx.rowH),
               st.title, FontRole::Title, pal.text, kLeft);
    }
    const int weeks = ClampI(st.weeks, 1, 60);
    for (int w = 0; w < weeks; ++w) {
        for (int d = 0; d < 7; ++d) {
            const int idx = w * 7 + d;
            const int level = (idx < static_cast<int>(st.values.size()))
                            ? ClampI(st.values[static_cast<std::size_t>(idx)], 0, 4) : 0;
            const int x = lay.grid.left + w * (lay.cell + lay.gap);
            const int y = lay.grid.top + d * (lay.cell + lay.gap);
            const Rect cell(x, y, x + lay.cell, y + lay.cell);
            g.FillRect(cell, pal.sunken);
            if (level > 0) {
                const int alpha = (std::min)(255, 45 + (level - 1) * 55);
                g.FillRectAlpha(cell, pal.primary, static_cast<uint8_t>(alpha));
            }
            if (st.hoveredWeek == w && st.hoveredDay == d) {
                g.StrokeRect(cell, pal.selectRing, 2);
            }
        }
    }
    if (!lay.legend.Empty()) {
        g.Text(Rect(lay.legend.left, lay.legend.top,
                    lay.legend.left + mx.unit * 4, lay.legend.bottom),
               L"少", FontRole::Meta, pal.textFaint, kLeft);
        int lx = lay.legend.left + mx.unit * 5;
        for (int i = 0; i < 5; ++i) {
            const Rect sw(lx, lay.legend.top + lay.legend.Height() / 2 - lay.cell / 2,
                          lx + lay.cell,
                          lay.legend.top + lay.legend.Height() / 2 + lay.cell / 2);
            g.FillRect(sw, pal.sunken);
            if (i > 0) {
                g.FillRectAlpha(sw, pal.primary,
                                static_cast<uint8_t>((std::min)(255, 45 + (i - 1) * 55)));
            }
            lx += lay.cell + (std::max)(2, mx.unit / 2);
        }
        g.Text(Rect(lx, lay.legend.top, (std::min)(lay.legend.right, lx + mx.unit * 4),
                    lay.legend.bottom),
               L"多", FontRole::Meta, pal.textFaint, kLeft);
    }
    g.PopClip();
}

CodeBlockLayout CodeBlock::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const CodeBlockState& st = state;
    (void)mx;
    CodeBlockLayout out;
    (void)st;
    if (rc.Empty()) return out;
    const int pad = mx.unit;
    const int headerH = mx.rowH;
    if (rc.Height() <= pad * 2 + headerH || rc.Width() <= pad * 3) return out;
    out.header = Rect(rc.left + pad, rc.top + pad,
                      rc.right - pad, rc.top + pad + headerH);
    const int copyW = mx.unit * 8;
    out.copy = Rect(out.header.right - copyW - mx.unit / 2,
                    out.header.top + (std::min)(2, headerH / 6),
                    out.header.right - mx.unit / 2,
                    out.header.bottom - (std::min)(2, headerH / 6));
    out.body = Rect(rc.left + pad, out.header.bottom,
                    rc.right - pad, rc.bottom - pad);
    out.lineH = (std::max)(12, mx.fontBody + mx.unit / 2);
    return out;
}

void CodeBlock::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    CodeBlockState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const CodeBlockLayout lay = MeasureCodeBlock(rc, st, mx);
    if (lay.body.Empty()) return;
    g.PushClip(rc);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    g.FillRect(lay.header, pal.sunken);
    g.Text(Rect(lay.header.left + mx.unit, lay.header.top,
                (std::max)(lay.header.left + mx.unit, lay.copy.left - mx.unit),
                lay.header.bottom),
           st.title, FontRole::Title, pal.textSub, kLeft);
    g.FillRound(lay.copy, mx.radiusSm,
                st.copyHover ? pal.hover : pal.surface);
    g.StrokeRound(lay.copy, mx.radiusSm,
                  st.copyHover ? pal.hoverBorder : pal.chipBorder, 1);
    g.Text(lay.copy, L"复制", FontRole::Meta, pal.textSub, kCenter);
    g.PushClip(lay.body);
    const int gutter = st.lineNumbers ? mx.unit * 5 : 0;
    if (gutter > 0) {
        g.FillRect(Rect(lay.body.left, lay.body.top,
                        lay.body.left + gutter, lay.body.bottom), pal.sunken);
    }
    const int count = static_cast<int>(st.lines.size());
    const int maxLines = lay.body.Height() / lay.lineH;
    for (int i = 0; i < count && i < maxLines; ++i) {
        const int y = lay.body.top + i * lay.lineH;
        const Rect line(lay.body.left, y, lay.body.right, y + lay.lineH);
        if (i == st.highlightLine) {
            g.FillRect(line, pal.noteBg);
        }
        if (gutter > 0) {
            g.Text(Rect(line.left, y, line.left + gutter - mx.unit / 2, y + lay.lineH),
                   std::to_wstring(i + 1), FontRole::Meta, pal.textFaint, kRight);
        }
        g.Text(Rect(line.left + gutter + mx.unit, y, line.right - mx.unit / 2,
                    y + lay.lineH),
               st.lines[static_cast<std::size_t>(i)], FontRole::Body,
               pal.text, kLeft);
    }
    g.PopClip();
    g.PopClip();
}

KanbanLayout Kanban::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const KanbanState& st = state;
    (void)mx;
    KanbanLayout out;
    if (rc.Empty() || st.columns.empty()) return out;
    const int n = static_cast<int>(st.columns.size());
    const int pad = mx.unit;
    const int gap = mx.unit;
    const int colW = (rc.Width() - pad * 2 - gap * (n - 1)) / n;
    if (colW < mx.unit * 8 || rc.Height() <= pad * 2 + mx.rowH) return out;
    out.cardH = (std::min)(mx.itemH, mx.rowH * 2);
    const int headH = mx.rowH;
    for (int c = 0; c < n; ++c) {
        const int l = rc.left + pad + c * (colW + gap);
        const Rect col(l, rc.top + pad, l + colW, rc.bottom - pad);
        out.columnRects.push_back(col);
        std::vector<Rect> cards;
        const int cardL = col.left + pad / 2;
        const int cardR = col.right - pad / 2;
        int y = col.top + headH + pad / 2;
        const auto& data = st.columns[static_cast<std::size_t>(c)].cards;
        for (std::size_t k = 0; k < data.size(); ++k) {
            const Rect card(cardL, y, cardR, y + out.cardH);
            if (card.bottom > col.bottom - pad / 2) break;
            cards.push_back(card);
            y += out.cardH + gap;
        }
        out.cardRects.push_back(cards);
    }
    return out;
}

KanbanHit Kanban::HitAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const KanbanState& st = state;
    (void)mx;
    (void)pt;
    KanbanHit h;
    if (!RectHit(rc, pt)) return h;
    const KanbanLayout lay = MeasureKanban(rc, st, mx);
    for (std::size_t c = 0; c < lay.cardRects.size(); ++c) {
        for (std::size_t k = 0; k < lay.cardRects[c].size(); ++k) {
            if (RectHit(lay.cardRects[c][k], pt)) {
                h.column = static_cast<int>(c);
                h.card = static_cast<int>(k);
                return h;
            }
        }
    }
    return h;
}

void Kanban::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    KanbanState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const KanbanLayout lay = MeasureKanban(rc, st, mx);
    if (lay.columnRects.empty()) return;
    g.PushClip(rc);
    for (std::size_t c = 0; c < lay.columnRects.size(); ++c) {
        const Rect& col = lay.columnRects[c];
        g.FillRound(col, mx.radiusMd, pal.sunken);
        g.StrokeRound(col, mx.radiusMd, pal.split, 1);
        const auto& data = st.columns[c];
        const Rect head(col.left + mx.unit / 2, col.top,
                        col.right - mx.unit / 2, col.top + mx.rowH);
        g.Text(head, data.title + L"  " + std::to_wstring(data.cards.size()),
               FontRole::Title, pal.text, kLeft);
        g.PushClip(Rect(col.left, col.top + mx.rowH, col.right, col.bottom));
        const auto& rects = lay.cardRects[c];
        for (std::size_t k = 0; k < rects.size(); ++k) {
            const Rect& card = rects[k];
            const KanbanCard& item = data.cards[k];
            const bool hovered = st.hoveredColumn == static_cast<int>(c) &&
                                 st.hoveredCard == static_cast<int>(k);
            const KindColors kc = ColorsFor(item.kind, pal);
            g.FillRound(card, mx.radiusMd,
                        hovered ? pal.hover : pal.surface);
            g.StrokeRound(card, mx.radiusMd,
                          hovered ? pal.hoverBorder : pal.split, 1);
            const int tagW = item.tag.empty() ? 0 : mx.unit * 8;
            g.Text(Rect(card.left + mx.unit, card.top,
                        (std::max)(card.left + mx.unit, card.right - mx.unit - tagW),
                        card.top + card.Height() / 2),
                   item.title, FontRole::Body, pal.text, kLeft);
            if (tagW > 0) {
                const Rect tag(card.right - mx.unit - tagW,
                               card.top + card.Height() / 2 - mx.tagH / 2,
                               card.right - mx.unit,
                               card.top + card.Height() / 2 + mx.tagH / 2);
                g.FillRound(tag, mx.radiusSm, kc.soft);
                g.Text(tag, item.tag, FontRole::Meta, kc.text, kCenter);
            }
            g.FillRect(Rect(card.left + mx.unit, card.bottom - mx.unit,
                            card.left + mx.unit + card.Width() / 3,
                            card.bottom - mx.unit + (std::max)(2, mx.unit / 4)),
                       kc.accent);
        }
        g.PopClip();
    }
    g.PopClip();
}

GanttLayout Gantt::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const GanttState& st = state;
    (void)mx;
    return GeoGantt(rc, st, mx).lay;
}

int Gantt::TaskAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const GanttState& st = state;
    (void)mx;
    (void)pt;
    if (!RectHit(rc, pt)) return -1;
    const GanttGeo geo = GeoGantt(rc, st, mx);
    if (!RectHit(geo.lay.rows, pt) || geo.lay.rowH <= 0) return -1;
    const int row = (pt.y - geo.lay.rows.top) / geo.lay.rowH;
    if (row < 0 || row >= static_cast<int>(geo.rowRects.size())) return -1;
    return row;
}

void Gantt::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    GanttState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const GanttGeo geo = GeoGantt(rc, st, mx);
    if (geo.lay.rows.Empty()) return;
    const int days = (std::max)(1, st.days);
    const int timelineL = geo.lay.rows.left + geo.lay.labelW;
    g.PushClip(rc);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    g.FillRect(geo.lay.header, pal.sunken);
    g.FillRect(Rect(timelineL - 1, geo.lay.header.top,
                    timelineL, geo.lay.rows.bottom), pal.split);
    const int labelStep = days > 10 ? 2 : 1;
    for (int d = 0; d < days; d += labelStep) {
        const int x = timelineL + d * geo.lay.dayW;
        if (x >= geo.lay.header.right) break;
        g.FillRect(Rect(x, geo.lay.header.bottom - (std::max)(2, mx.unit / 4),
                        x + (std::max)(1, mx.unit / 8), geo.lay.header.bottom),
                   pal.split);
        g.Text(Rect(x, geo.lay.header.top,
                    (std::min)(geo.lay.header.right, x + geo.lay.dayW * labelStep),
                    geo.lay.header.bottom),
               L"D" + std::to_wstring(d + 1), FontRole::Meta, pal.textFaint, kLeft);
    }
    g.PushClip(geo.lay.rows);
    for (int d = 0; d <= days; ++d) {
        const int x = timelineL + d * geo.lay.dayW;
        if (x > geo.lay.rows.right) break;
        g.FillRect(Rect(x, geo.lay.rows.top, x + 1, geo.lay.rows.bottom), pal.split);
    }
    if (st.showToday && st.today >= 0 && st.today < days) {
        const int x = timelineL + st.today * geo.lay.dayW;
        g.FillRect(Rect(x, geo.lay.rows.top, x + (std::max)(2, mx.unit / 4),
                        geo.lay.rows.bottom), pal.warn);
    }
    for (std::size_t i = 0; i < geo.lay.bars.size(); ++i) {
        const Rect& bar = geo.lay.bars[i];
        if (bar.Empty() || bar.top >= geo.lay.rows.bottom) continue;
        const GanttTask& task = st.tasks[i];
        const KindColors kc = ColorsFor(task.kind, pal);
        g.FillRound(bar, mx.radiusSm, kc.soft);
        g.StrokeRound(bar, mx.radiusSm,
                      st.hoveredTask == static_cast<int>(i) ? pal.selectRing : kc.accent,
                      st.hoveredTask == static_cast<int>(i) ? 2 : 1);
        const int done = bar.Width() * ClampI(task.progress, 0, 100) / 100;
        if (done > 0) {
            g.FillRect(Rect(bar.left, bar.top, bar.left + done, bar.bottom), kc.accent);
        }
    }
    g.PopClip();
    g.PushClip(geo.lay.labelArea);
    for (std::size_t i = 0; i < st.tasks.size() && i < geo.rowRects.size(); ++i) {
        const Rect& row = geo.rowRects[i];
        if (row.top >= geo.lay.rows.bottom) break;
        g.Text(Rect(row.left + mx.unit / 2, row.top,
                    geo.lay.labelArea.right - mx.unit / 2, row.bottom),
               st.tasks[i].name, FontRole::Body, pal.text, kLeft);
    }
    g.PopClip();
    g.PopClip();
}

} // namespace widgets
} // namespace ui
} // namespace air
