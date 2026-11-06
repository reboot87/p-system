// AirUI - ControlsDisplay.cpp : platform-free display controls.
// Pure functions of (rect, state, palette, metrics); rect-only drawing
// (thin filled rects for lines, small rounded rects for dots) because
// IPainter has no path/line/circle primitives.
#include "airui/ControlsExtra.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace air {
namespace ui {
namespace {

// Every Text() call carries dt::NoPrefix + dt::SingleLine + alignment;
// long strings also dt::EndEllipsis; short glyphs/numbers use kTextGlyph.
constexpr TextFormat kTextLeft =
    dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kTextCenter =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kTextRight =
    dt::SingleLine | dt::Right | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kTextGlyph =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix;

int Clamp(int v, int lo, int hi) { return (std::min)((std::max)(v, lo), hi); }
Rect Inset(const Rect& r, int d) { return Rect(r.left + d, r.top + d, r.right - d, r.bottom - d); }
Color BodyText(const ControlState& st, const Palette& pal) {
    return st.disabled ? pal.textDisabled : pal.text;
}
void PaintCardFrame(IPainter& g, const Rect& rc, const Palette& pal, const Metrics& mx) {
    g.FillRound(rc, mx.radiusCard, pal.surface);
    g.StrokeRound(rc, mx.radiusCard, pal.split, 1);
}
struct KindColors { Color accent, soft, text; };
KindColors ColorsFor(AlertKind kind, const Palette& pal) {
    switch (kind) {
    case AlertKind::Success: return { pal.ok,      pal.okSoft,      pal.okText };
    case AlertKind::Warning: return { pal.warn,    pal.warnSoft,    pal.warnText };
    case AlertKind::Error:   return { pal.error,   pal.errorSoft,   pal.errorText };
    default:                 return { pal.primary, pal.primarySoft, pal.primaryText };
    }
}
// A straight line as a chain of small filled rects (no line primitive).
void PaintSegment(IPainter& g, Point a, Point b, int thick, Color c) {
    const int dx = b.x - a.x, dy = b.y - a.y;
    const int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    const int steps = (std::max)((std::max)(adx, ady), 1), half = thick / 2;
    for (int i = 0; i <= steps; ++i) {
        const int x = a.x + dx * i / steps, y = a.y + dy * i / steps;
        g.FillRect(Rect(x - half, y - half, x - half + thick, y - half + thick), c);
    }
}
void PaintArrow(IPainter& g, const Rect& rc, const wchar_t* glyph, bool hovered,
                const ControlState& st, const Palette& pal, const Metrics& mx) {
    const Color fill = st.disabled ? pal.sunken : hovered ? pal.primarySoft : pal.surface;
    g.FillRound(rc, mx.radiusSm, fill);
    g.StrokeRound(rc, mx.radiusSm, st.disabled ? pal.border : pal.split, 1);
    g.Text(rc, glyph, FontRole::Title,
           st.disabled ? pal.textDisabled : pal.primaryText, kTextGlyph);
}

// File-local calendar arithmetic (0 = Sunday). Not the ControlsInput helpers.
bool IsLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); }
int MonthDays(int y, int m) {
    const int t[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    m = Clamp(m, 1, 12);
    return (m == 2 && IsLeap(y)) ? 29 : t[m - 1];
}
int FirstDow(int y, int m) {                 // Zeller, day 1 of month
    y = (std::max)(1, y); m = Clamp(m, 1, 12);
    int yy = y, mm = m;
    if (mm <= 2) { mm += 12; --yy; }
    const int k = yy % 100, j = yy / 100;
    const int h = (1 + (13 * (mm + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return (h + 6) % 7;
}
int DayAtCell(int y, int m, int row, int col) {
    const int day = row * 7 + col - FirstDow(y, m) + 1;
    return (day < 1 || day > MonthDays(y, m)) ? -1 : day;
}
const wchar_t* WeekdayLabel(int i) {
    const wchar_t* l[7] = { L"\u65E5", L"\u4E00", L"\u4E8C", L"\u4E09",
                            L"\u56DB", L"\u4E94", L"\u516D" };
    return l[Clamp(i, 0, 6)];
}

// ---- Steps geometry (shared by StepAt / PaintSteps) ----
struct StepsGeo {
    int n = 0;
    std::vector<Rect> cells, nodes, labels, bars;
};
StepsGeo MeasureSteps(const Rect& rc, const StepsState& st, const Metrics& mx) {
    StepsGeo geo;
    const int n = static_cast<int>(st.steps.size());
    if (n == 0 || rc.Empty()) return geo;
    const int pad = mx.unit, inner = rc.Width() - pad * 2;
    if (inner < n) return geo;
    geo.n = n;
    const int cw = inner / n;
    int nodeSize = (std::min)(mx.rowH, cw - mx.unit);
    if (nodeSize < mx.unit) nodeSize = mx.unit;
    const int nodeTop = rc.top + pad, nodeCY = nodeTop + nodeSize / 2;
    const int barH = (std::max)(2, mx.unit / 4);
    const int labelTop = nodeTop + nodeSize + mx.unit / 2;
    const int labelBottom = rc.bottom - pad;
    for (int i = 0; i < n; ++i) {
        const int colLeft = rc.left + pad + i * cw;
        const int colRight = (i == n - 1) ? (rc.right - pad) : (colLeft + cw);
        const int cx = (colLeft + colRight) / 2, nl = cx - nodeSize / 2;
        geo.cells.push_back(Rect(colLeft, rc.top, colRight, rc.bottom));
        geo.nodes.push_back(Rect(nl, nodeTop, nl + nodeSize, nodeTop + nodeSize));
        geo.labels.push_back(Rect(colLeft, labelTop, colRight,
                                  (std::max)(labelTop, labelBottom)));
    }
    for (int i = 0; i < n - 1; ++i) {
        geo.bars.push_back(Rect(geo.nodes[static_cast<std::size_t>(i)].right,
                                nodeCY - barH / 2,
                                geo.nodes[static_cast<std::size_t>(i + 1)].left,
                                nodeCY - barH / 2 + barH));
    }
    return geo;
}

// ---- Chart geometry (shared by ChartBarAt / PaintChart) ----
struct ChartGeo {
    int n = 0;
    Rect plot{}, titleBand{}, labelBand{};
    int baseline = 0, maxVal = 1, barW = 0;
    std::vector<Rect> slots, labels;
};
ChartGeo MeasureChart(const Rect& rc, const ChartState& st, const Metrics& mx) {
    ChartGeo geo;
    const int n = static_cast<int>(st.values.size());
    if (n == 0 || rc.Empty()) return geo;
    const int pad = mx.unit;
    const int titleH = st.title.empty() ? 0 : mx.rowH;
    geo.titleBand = Rect(rc.left + pad, rc.top + pad, rc.right - pad, rc.top + pad + titleH);
    const int labelH = mx.rowH - pad;
    geo.labelBand = Rect(rc.left + pad, rc.bottom - pad - labelH, rc.right - pad, rc.bottom - pad);
    geo.baseline = geo.labelBand.top - pad / 2;
    geo.plot = Rect(rc.left + pad, geo.titleBand.bottom, rc.right - pad, geo.baseline);
    if (geo.plot.Empty()) return geo;
    for (int v : st.values) geo.maxVal = (std::max)(geo.maxVal, (std::max)(0, v));
    const int slotW = geo.plot.Width() / n;
    if (slotW < 1) return geo;
    geo.barW = (std::max)(2, slotW * 3 / 5);
    for (int i = 0; i < n; ++i) {
        const int x0 = geo.plot.left + i * slotW;
        const int x1 = (i == n - 1) ? geo.plot.right : (x0 + slotW);
        geo.slots.push_back(Rect(x0, geo.plot.top, x1, geo.labelBand.bottom));
        geo.labels.push_back(Rect(x0, geo.labelBand.top, x1, geo.labelBand.bottom));
    }
    geo.n = n;
    return geo;
}

// ---- Carousel geometry (shared by HitCarousel / PaintCarousel) ----
struct CarouselGeo {
    int n = 0, dotSize = 0;
    Rect card{}, prev{}, next{};
    std::vector<Rect> dots;
};
CarouselGeo MeasureCarousel(const Rect& rc, const CarouselState& st, const Metrics& mx) {
    CarouselGeo geo;
    const int n = static_cast<int>(st.slides.size());
    if (n == 0 || rc.Empty()) return geo;
    geo.n = n;
    const int pad = mx.unit, arrowW = mx.rowH, dotBandH = mx.rowH;
    const Rect dotBand(rc.left, rc.bottom - dotBandH, rc.right, rc.bottom);
    const int contentBottom = dotBand.top - pad / 2;
    geo.prev = Rect(rc.left + pad, rc.top + pad, rc.left + pad + arrowW, contentBottom);
    geo.next = Rect(rc.right - pad - arrowW, rc.top + pad, rc.right - pad, contentBottom);
    geo.card = Rect(geo.prev.right + pad / 2, rc.top + pad,
                    geo.next.left - pad / 2, contentBottom);
    if (geo.card.Empty()) { geo.n = 0; return geo; }
    const int dotSize = mx.unit, dotGap = mx.unit;
    geo.dotSize = dotSize;
    const int total = n * dotSize + (n - 1) * dotGap;
    int x = rc.left + (rc.Width() - total) / 2;
    const int y = dotBand.top + (dotBand.Height() - dotSize) / 2;
    for (int i = 0; i < n; ++i) {
        geo.dots.push_back(Rect(x, y, x + dotSize, y + dotSize));
        x += dotSize + dotGap;
    }
    return geo;
}

} // namespace

// ---- Timeline ----
void PaintTimeline(IPainter& g, const Rect& rc, const TimelineState& st,
                   const Palette& pal, const Metrics& mx) {
    if (rc.Empty()) return;
    const int n = static_cast<int>(st.events.size()), eventH = mx.itemH;
    if (n == 0 || eventH <= 0) return;
    const int visible = (std::min)(n, (std::max)(0, rc.Height() / eventH));
    if (visible <= 0) return;
    const int pad = mx.unit, nodeSize = (std::max)(mx.unit, mx.rowH / 3);
    const int railW = (std::max)(2, mx.unit / 4);
    const int railX = rc.left + pad + nodeSize / 2;
    const int textLeft = rc.left + pad + nodeSize + pad, half = eventH / 2;
    g.PushClip(rc);
    g.FillRect(Rect(railX - railW / 2, rc.top + pad + nodeSize / 2,
                    railX - railW / 2 + railW,
                    rc.top + pad + (visible - 1) * eventH + nodeSize / 2 + 1), pal.split);
    for (int i = 0; i < visible; ++i) {
        const TimelineEvent& ev = st.events[static_cast<std::size_t>(i)];
        const KindColors kc = ColorsFor(ev.kind, pal);
        const Color accent = st.base.disabled ? pal.border : kc.accent;
        const int y = rc.top + i * eventH, r = nodeSize / 2;
        const Rect node(railX - r, y + pad, railX - r + nodeSize, y + pad + nodeSize);
        g.FillRound(Inset(node, -3), r + 3, kc.soft);
        g.FillRound(node, r, pal.surface);
        g.StrokeRound(node, r, accent, 1);
        g.FillRound(Inset(node, 3), (std::max)(1, r - 3), accent);
        const int timeW = ev.time.empty() ? 0 : g.MeasureText(ev.time, FontRole::Meta) + pad;
        g.Text(Rect(textLeft, y, rc.right - pad - timeW, y + half), ev.title,
               FontRole::Body, BodyText(st.base, pal), kTextLeft);
        g.Text(Rect(textLeft, y + half, rc.right - pad, y + eventH), ev.desc,
               FontRole::Meta, st.base.disabled ? pal.textDisabled : pal.textSub, kTextLeft);
        if (timeW > 0) {
            g.Text(Rect(rc.right - pad - timeW, y, rc.right - pad, y + half), ev.time,
                   FontRole::Meta, st.base.disabled ? pal.textDisabled : kc.text, kTextRight);
        }
    }
    g.PopClip();
}

// ---- Steps ----
int StepAt(const Rect& rc, const StepsState& st, const Metrics& mx, Point pt) {
    if (rc.Empty() || !rc.Contains(pt)) return -1;
    const StepsGeo geo = MeasureSteps(rc, st, mx);
    for (int i = 0; i < geo.n; ++i) {
        if (geo.cells[static_cast<std::size_t>(i)].Contains(pt)) return i;
    }
    return -1;
}
void PaintSteps(IPainter& g, const Rect& rc, const StepsState& st,
                const Palette& pal, const Metrics& mx) {
    if (rc.Empty()) return;
    const StepsGeo geo = MeasureSteps(rc, st, mx);
    if (geo.n == 0) return;
    const int current = Clamp(st.current, 0, geo.n - 1);
    g.PushClip(rc);
    for (int i = 0; i < geo.n - 1; ++i) {
        const Color c = st.base.disabled ? pal.split
                      : (i < current && !st.failed) ? pal.ok : pal.split;
        g.FillRect(geo.bars[static_cast<std::size_t>(i)], c);
    }
    for (int i = 0; i < geo.n; ++i) {
        const Rect& node = geo.nodes[static_cast<std::size_t>(i)];
        const int r = node.Height() / 2;
        Color fill;
        if (st.base.disabled)          fill = pal.sunken;
        else if (i < current)          fill = pal.ok;
        else if (i == current)         fill = st.failed ? pal.error : pal.primary;
        else                           fill = pal.sunken;
        if (i == st.hovered && !st.base.disabled)
            g.FillRound(Inset(node, -3), r + 3, pal.hover);
        g.FillRound(node, r, fill);
        if (i > current || st.base.disabled) g.StrokeRound(node, r, pal.border, 1);
        const Color numTxt = st.base.disabled ? pal.textDisabled
                           : (i > current ? pal.textSub : pal.textInverse);
        g.Text(node, std::to_wstring(i + 1), FontRole::Meta, numTxt, kTextGlyph);
        const Color lab = st.base.disabled ? pal.textDisabled
                        : (i == current ? pal.primaryText : pal.textSub);
        g.Text(geo.labels[static_cast<std::size_t>(i)],
               st.steps[static_cast<std::size_t>(i)], FontRole::Meta, lab, kTextCenter);
    }
    g.PopClip();
}

// ---- Calendar ----
CalendarLayout MeasureCalendar(const Rect& rc, const CalendarState& st,
                               const Metrics& mx) {
    CalendarLayout out;
    if (rc.Empty()) return out;
    const int pad = mx.unit;
    out.headerH = st.showWeekdays ? mx.rowH : 0;
    const int gl = rc.left + pad, gr = rc.right - pad;
    const int gt = rc.top + pad + out.headerH, gb = rc.bottom - pad;
    if (gr <= gl || gb <= gt) return out;
    out.grid = Rect(gl, gt, gr, gb);
    out.cellW = out.grid.Width() / 7;
    out.cellH = out.grid.Height() / 6;
    return out;
}
int CalendarDayAt(const Rect& rc, const CalendarState& st,
                  const Metrics& mx, Point pt) {
    if (rc.Empty() || !rc.Contains(pt)) return -1;
    const CalendarLayout lay = MeasureCalendar(rc, st, mx);
    if (lay.grid.Empty() || lay.cellW <= 0 || lay.cellH <= 0) return -1;
    if (!lay.grid.Contains(pt)) return -1;
    const int col = (pt.x - lay.grid.left) / lay.cellW;
    const int row = (pt.y - lay.grid.top) / lay.cellH;
    if (col < 0 || col >= 7 || row < 0 || row >= 6) return -1;
    return DayAtCell((std::max)(1, st.year), Clamp(st.month, 1, 12), row, col);
}
void PaintCalendar(IPainter& g, const Rect& rc, const CalendarState& st,
                   const Palette& pal, const Metrics& mx) {
    if (rc.Empty()) return;
    const CalendarLayout lay = MeasureCalendar(rc, st, mx);
    const int pad = mx.unit;
    PaintCardFrame(g, rc, pal, mx);
    g.PushClip(Inset(rc, 1));
    if (st.showWeekdays && lay.headerH > 0) {
        for (int c = 0; c < 7; ++c) {
            const int x = lay.grid.left + c * lay.cellW;
            g.Text(Rect(x, rc.top + pad, x + lay.cellW, rc.top + pad + lay.headerH),
                   WeekdayLabel(c), FontRole::Meta,
                   st.base.disabled ? pal.textDisabled : pal.textFaint, kTextCenter);
        }
    }
    if (!lay.grid.Empty() && lay.cellW > 0 && lay.cellH > 0) {
        const int y = (std::max)(1, st.year), m = Clamp(st.month, 1, 12);
        const int total = MonthDays(y, m);
        const int inset = (std::max)(1, pad / 4), dotR = (std::max)(1, pad / 4);
        for (int row = 0; row < 6; ++row) {
            for (int col = 0; col < 7; ++col) {
                const int day = DayAtCell(y, m, row, col);
                if (day < 1 || day > total) continue;
                const int x0 = lay.grid.left + col * lay.cellW;
                const int y0 = lay.grid.top + row * lay.cellH;
                const Rect cell(x0 + inset, y0 + inset,
                                x0 + lay.cellW - inset, y0 + lay.cellH - inset);
                if (cell.Empty()) continue;
                const bool selected = (day == st.selectedDay);
                const bool hovered = (day == st.hoveredDay && !st.base.disabled);
                const bool marked =
                    std::find(st.marked.begin(), st.marked.end(), day) != st.marked.end();
                if (selected) {
                    g.FillRound(cell, mx.radiusSm, st.base.disabled ? pal.border : pal.primary);
                } else if (hovered) {
                    g.FillRound(cell, mx.radiusSm, pal.hover);
                }
                const Color txt = st.base.disabled ? pal.textDisabled
                                : selected ? pal.textInverse : pal.text;
                Rect numRc = cell;
                if (marked) numRc.bottom -= (dotR * 2 + 2);
                g.Text(numRc, std::to_wstring(day), FontRole::Meta, txt, kTextCenter);
                if (marked) {
                    const int cx = (cell.left + cell.right) / 2;
                    const int cy = cell.bottom - dotR - 1;
                    g.FillRound(Rect(cx - dotR, cy - dotR, cx + dotR, cy + dotR), dotR,
                                selected ? pal.textInverse : pal.primary);
                }
            }
        }
    }
    g.PopClip();
}

// ---- DescriptionList ----
void PaintDescriptionList(IPainter& g, const Rect& rc,
                          const DescriptionListState& st,
                          const Palette& pal, const Metrics& mx) {
    if (rc.Empty()) return;
    const int cols = (std::max)(1, st.columns), pad = mx.pagePad;
    const int n = static_cast<int>(st.items.size());
    const int titleH = st.title.empty() ? 0 : mx.rowH, rowH = mx.rowH + mx.unit;
    const int inner = rc.Width() - pad * 2;
    PaintCardFrame(g, rc, pal, mx);
    g.PushClip(Inset(rc, 1));
    if (titleH > 0) {
        g.Text(Rect(rc.left + pad, rc.top + pad, rc.right - pad, rc.top + pad + titleH),
               st.title, FontRole::Title, BodyText(st.base, pal), kTextLeft);
    }
    if (n > 0 && inner >= cols && rowH > 0) {
        const int slotW = inner / cols, keyW = slotW * 2 / 5;
        const int rows = (n + cols - 1) / cols;
        const int top = rc.top + pad + titleH + (titleH > 0 ? pad / 2 : 0);
        const int available = rc.bottom - pad - top;
        const int visibleRows = available > 0 ? (std::min)(rows, available / rowH) : 0;
        if (visibleRows > 0) {
            for (int c = 1; c < cols; ++c) {
                const int x = rc.left + pad + c * slotW;
                g.StrokeDashed(Rect(x, top, x + 1, top + visibleRows * rowH), pal.split, 1, 2);
            }
            for (int r = 0; r < visibleRows; ++r) {
                const int y = top + r * rowH;
                for (int c = 0; c < cols; ++c) {
                    const int index = r * cols + c;
                    if (index >= n) break;
                    const int x = rc.left + pad + c * slotW;
                    g.Text(Rect(x, y, x + keyW, y + rowH),
                           st.items[static_cast<std::size_t>(index)].first, FontRole::Meta,
                           st.base.disabled ? pal.textDisabled : pal.textFaint, kTextRight);
                    g.Text(Rect(x + keyW, y, x + slotW, y + rowH),
                           st.items[static_cast<std::size_t>(index)].second, FontRole::Body,
                           BodyText(st.base, pal), kTextLeft);
                }
                if (r + 1 < visibleRows) {
                    g.FillRect(Rect(rc.left + pad, y + rowH - 1, rc.right - pad, y + rowH), pal.split);
                }
            }
        }
    }
    g.PopClip();
}

// ---- StatCard ----
StatCardLayout MeasureStatCard(const Rect& rc, const StatCardState& st, const Metrics& mx) {
    StatCardLayout out;
    if (rc.Empty()) return out;
    const int pad = mx.pagePad, innerW = rc.Width() - pad * 2;
    if (innerW < mx.unit * 2) return out;
    const int titleH = mx.rowH, valueH = mx.itemH, deltaH = mx.tagH;
    out.titleRc = Rect(rc.left + pad, rc.top + pad, rc.right - pad, rc.top + pad + titleH);
    const int valueTop = out.titleRc.bottom;
    const int deltaW = (std::max)(mx.rowH * 2,
        static_cast<int>(st.delta.size()) * mx.fontMeta + mx.unit * 2);
    int deltaLeft = rc.right - pad - deltaW;
    if (deltaLeft < rc.left + pad + mx.rowH) deltaLeft = rc.left + pad + mx.rowH;
    const int dy = valueTop + (valueH - deltaH) / 2;
    out.deltaRc = Rect(deltaLeft, dy, rc.right - pad, dy + deltaH);
    out.valueRc = Rect(rc.left + pad, valueTop, out.deltaRc.left - pad / 2, valueTop + valueH);
    out.chartRc = Rect(rc.left + pad, rc.top + pad + titleH + valueH + pad / 2,
                       rc.right - pad, rc.bottom - pad);
    const int nb = static_cast<int>(st.bars.size());
    if (nb > 0 && !out.chartRc.Empty()) {
        out.gap = (std::max)(1, mx.unit / 2);
        out.barW = (out.chartRc.Width() - out.gap * (nb - 1)) / nb;
        if (out.barW < 1) out.barW = 1;
    }
    return out;
}
StatCardHit HitStatCard(const Rect& rc, const StatCardState& st,
                        const Metrics& mx, Point pt) {
    if (rc.Empty() || !rc.Contains(pt)) return StatCardHit::None;
    const StatCardLayout lay = MeasureStatCard(rc, st, mx);
    if (!lay.deltaRc.Empty() && lay.deltaRc.Contains(pt)) return StatCardHit::Delta;
    return StatCardHit::Body;
}
void PaintStatCard(IPainter& g, const Rect& rc, const StatCardState& st,
                   const Palette& pal, const Metrics& mx) {
    if (rc.Empty()) return;
    const StatCardLayout lay = MeasureStatCard(rc, st, mx);
    const bool highlight = st.base.hover || st.base.pressed || st.base.checked;
    g.FillRound(rc, mx.radiusCard, highlight ? pal.primarySoft : pal.surface);
    g.StrokeRound(rc, mx.radiusCard, highlight ? pal.primary : pal.split, 1);
    g.PushClip(Inset(rc, 1));
    g.Text(lay.titleRc, st.title, FontRole::Meta,
           st.base.disabled ? pal.textDisabled : pal.textSub, kTextLeft);
    g.Text(lay.valueRc, st.value, FontRole::Display, BodyText(st.base, pal), kTextLeft);
    if (!lay.deltaRc.Empty() && !st.delta.empty()) {
        Color pillFill = st.positive ? pal.okSoft : pal.errorSoft;
        Color pillText = st.positive ? pal.okText : pal.errorText;
        Color pillStroke = st.positive ? pal.ok : pal.error;
        // Selected-row contrast: on a highlighted background the pill
        // switches to surface + a primary stroke so it stays visible.
        if (highlight) {
            pillFill = pal.surface; pillText = pal.primaryText; pillStroke = pal.primary;
        }
        if (st.base.disabled) {
            pillFill = pal.sunken; pillText = pal.textDisabled; pillStroke = pal.border;
        }
        g.FillRound(lay.deltaRc, mx.radiusPill, pillFill);
        g.StrokeRound(lay.deltaRc, mx.radiusPill, pillStroke, 1);
        g.Text(lay.deltaRc, st.delta, FontRole::Meta, pillText, kTextCenter);
    }
    if (!lay.chartRc.Empty() && lay.barW > 0 && !st.bars.empty()) {
        g.FillRect(Rect(lay.chartRc.left, lay.chartRc.bottom - 1,
                        lay.chartRc.right, lay.chartRc.bottom), pal.split);
        int maxBar = 1;
        for (int v : st.bars) maxBar = (std::max)(maxBar, (std::max)(0, v));
        const int nb = static_cast<int>(st.bars.size()), h = lay.chartRc.Height();
        for (int i = 0; i < nb; ++i) {
            const int x = lay.chartRc.left + i * (lay.barW + lay.gap);
            if (x + lay.barW > lay.chartRc.right) break;
            const int v = (std::max)(0, st.bars[static_cast<std::size_t>(i)]);
            const int bh = h * v / maxBar;
            g.FillRound(Rect(x, lay.chartRc.bottom - bh, x + lay.barW, lay.chartRc.bottom),
                        mx.radiusSm, st.base.disabled ? pal.border : pal.primary);
        }
    }
    g.PopClip();
}

// ---- Chart ----
int ChartBarAt(const Rect& rc, const ChartState& st, const Metrics& mx, Point pt) {
    if (rc.Empty() || !rc.Contains(pt)) return -1;
    const ChartGeo geo = MeasureChart(rc, st, mx);
    for (int i = 0; i < geo.n; ++i) {
        if (geo.slots[static_cast<std::size_t>(i)].Contains(pt)) return i;
    }
    return -1;
}
void PaintChart(IPainter& g, const Rect& rc, const ChartState& st,
                const Palette& pal, const Metrics& mx) {
    if (rc.Empty()) return;
    const ChartGeo geo = MeasureChart(rc, st, mx);
    PaintCardFrame(g, rc, pal, mx);
    g.PushClip(Inset(rc, 1));
    if (!st.title.empty()) {
        g.Text(geo.titleBand, st.title, FontRole::Title, BodyText(st.base, pal), kTextLeft);
    }
    if (geo.n > 0 && !geo.plot.Empty()) {
        const int midY = geo.plot.top + geo.plot.Height() / 2;
        g.StrokeDashed(Rect(geo.plot.left, midY, geo.plot.right, midY + 1), pal.split, 1, 3);
        g.FillRect(Rect(geo.plot.left, geo.baseline, geo.plot.right, geo.baseline + 1), pal.split);
        const Color baseCol = st.base.disabled ? pal.border : pal.primary;
        const Color hoverCol = st.base.disabled ? pal.border : pal.primaryHover;
        const Color okCol = st.base.disabled ? pal.border : pal.ok;
        const Color warnCol = st.base.disabled ? pal.border : pal.warn;
        const int thick = (std::max)(2, mx.unit / 2);
        std::vector<Point> points;
        for (int i = 0; i < geo.n; ++i) {
            const Rect& slot = geo.slots[static_cast<std::size_t>(i)];
            const int cx = (slot.left + slot.right) / 2;
            const int v = (std::max)(0, st.values[static_cast<std::size_t>(i)]);
            const int h = geo.plot.Height() * v / geo.maxVal;
            const int top = geo.baseline - h;
            const bool hov = (i == st.hovered && !st.base.disabled);
            const Rect bar(cx - geo.barW / 2, top, cx - geo.barW / 2 + geo.barW, geo.baseline);
            if (st.kind == ChartKind::Line) {
                points.push_back(Point{cx, top});
            } else if (st.kind == ChartKind::Stacked) {
                const int seg = (std::max)(1, h / 3);
                const int y1 = geo.baseline - seg, y2 = geo.baseline - 2 * seg;
                g.FillRect(Rect(bar.left, y1, bar.right, geo.baseline), baseCol);
                g.FillRect(Rect(bar.left, y2, bar.right, y1), okCol);
                g.FillRect(Rect(bar.left, bar.top, bar.right, y2), warnCol);
                if (hov) g.StrokeRect(bar, pal.primary, 1);
            } else {
                g.FillRound(bar, mx.radiusSm, hov ? hoverCol : baseCol);
            }
            const std::wstring lab = (i < static_cast<int>(st.labels.size()))
                ? st.labels[static_cast<std::size_t>(i)] : L"";
            if (!lab.empty()) {
                g.Text(geo.labels[static_cast<std::size_t>(i)], lab, FontRole::Meta,
                       st.base.disabled ? pal.textDisabled : pal.textFaint, kTextCenter);
            }
        }
        if (st.kind == ChartKind::Line) {
            const Color lineCol = st.base.disabled ? pal.border : pal.primary;
            for (std::size_t i = 0; i + 1 < points.size(); ++i) {
                PaintSegment(g, points[i], points[i + 1], thick, lineCol);
            }
            for (int i = 0; i < geo.n; ++i) {
                const Point p = points[static_cast<std::size_t>(i)];
                const bool hov = (i == st.hovered && !st.base.disabled);
                const int r = hov ? thick + 1 : thick;
                g.FillRound(Rect(p.x - r, p.y - r, p.x + r, p.y + r), r,
                            hov ? hoverCol : baseCol);
            }
        }
    }
    g.PopClip();
}

// ---- Carousel ----
CarouselHitResult HitCarousel(const Rect& rc, const CarouselState& st,
                              const Metrics& mx, Point pt) {
    CarouselHitResult out;
    if (rc.Empty() || !rc.Contains(pt)) return out;
    const CarouselGeo geo = MeasureCarousel(rc, st, mx);
    if (geo.n == 0) return out;
    if (geo.prev.Contains(pt)) { out.code = 1; return out; }
    if (geo.next.Contains(pt)) { out.code = 2; return out; }
    for (int i = 0; i < geo.n; ++i) {
        if (geo.dots[static_cast<std::size_t>(i)].Contains(pt)) {
            out.code = 3; out.index = i; return out;
        }
    }
    return out;
}
void PaintCarousel(IPainter& g, const Rect& rc, const CarouselState& st,
                   const Palette& pal, const Metrics& mx) {
    if (rc.Empty()) return;
    const CarouselGeo geo = MeasureCarousel(rc, st, mx);
    PaintCardFrame(g, rc, pal, mx);
    g.PushClip(Inset(rc, 1));
    if (geo.n > 0) {
        const int cur = Clamp(st.current, 0, geo.n - 1);
        g.FillRound(geo.card, mx.radiusMd, pal.surface);
        g.StrokeRound(geo.card, mx.radiusMd, pal.split, 1);
        g.FillRectAlpha(geo.card, pal.primary, 18);
        g.TextBlock(Inset(geo.card, mx.unit * 2),
                    st.slides[static_cast<std::size_t>(cur)], FontRole::Body,
                    BodyText(st.base, pal), 3);
        PaintArrow(g, geo.prev, L"\u2039", st.hovered == 1, st.base, pal, mx);
        PaintArrow(g, geo.next, L"\u203A", st.hovered == 2, st.base, pal, mx);
        const int r = (std::max)(1, geo.dotSize / 2);
        for (int i = 0; i < geo.n; ++i) {
            const bool active = (i == cur);
            const bool dotHover = (st.hovered >= 3 && st.hovered - 3 == i);
            const Color c = st.base.disabled ? pal.border
                          : active ? pal.primary
                          : dotHover ? pal.primarySoft : pal.sunken;
            const Rect& dot = geo.dots[static_cast<std::size_t>(i)];
            g.FillRound(dot, r, c);
            g.StrokeRound(dot, r, pal.split, 1);
        }
    }
    g.PopClip();
}

} // namespace ui
} // namespace air
