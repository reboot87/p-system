// AirUI - ControlsStudioCharts.cpp : platform-free compact charts.
// Fifth wave: Sparkline, Waterfall, StackedBar, GaugeMeter.
#include "airui/ControlsStudio.h"

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

double ClampD(double v, double lo, double hi)
{
    return (std::min)((std::max)(v, lo), hi);
}

bool Has(const Rect& r, Point p)
{
    return !r.Empty() && r.Contains(p);
}

Rect Inset(const Rect& r, int d)
{
    return Rect(r.left + d, r.top + d, r.right - d, r.bottom - d);
}

std::wstring Whole(double v)
{
    return std::to_wstring(static_cast<int>(v < 0 ? v - 0.5 : v + 0.5));
}

std::wstring SignedPercent(double v)
{
    const int tenths = static_cast<int>(v < 0 ? v * 10.0 - 0.5
                                              : v * 10.0 + 0.5);
    std::wstring s = std::to_wstring(tenths / 10) + L"." +
                     std::to_wstring(tenths < 0 ? -tenths % 10 : tenths % 10) + L"%";
    return v >= 0 ? L"+" + s : s;
}

// A rise is warm, a fall is cool (domestic market convention).
struct TrendColors
{
    Color line, soft, text;
};

TrendColors Trend(double change, const Palette& pal)
{
    if (change >= 0.0) return TrendColors{pal.error, pal.errorSoft, pal.errorText};
    return TrendColors{pal.ok, pal.okSoft, pal.okText};
}

Color SeriesColor(int index, const Palette& pal)
{
    switch (index % 6) {
    case 0:  return pal.primary;
    case 1:  return pal.ok;
    case 2:  return pal.warn;
    case 3:  return pal.error;
    case 4:  return pal.selectRing;
    default: return pal.chipBorder;
    }
}

// ============================================================
// Sparkline
// ============================================================
SparklineLayout GeoSpark(const Rect& rc, const SparklineState& st,
                         const Metrics& mx)
{
    SparklineLayout o;
    if (rc.Empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.rowH * 2) return o;
    o.header = Rect(inner.left, inner.top, inner.right,
                    inner.top + mx.rowH);
    o.valueRc = Rect(inner.left, o.header.bottom, inner.right,
                     o.header.bottom + mx.rowH);
    o.plot = Rect(inner.left, o.valueRc.bottom + mx.unit / 2,
                  inner.right, inner.bottom);
    if (o.plot.Empty() || st.values.empty()) return o;

    double lo = st.values.front();
    double hi = st.values.front();
    for (double v : st.values) {
        lo = (std::min)(lo, v);
        hi = (std::max)(hi, v);
    }
    const double span = (std::max)(1.0, hi - lo);
    const int n = static_cast<int>(st.values.size());
    const int spanX = o.plot.Width();
    const int spanY = o.plot.Height();
    const int stepW = (std::max)(1, spanX / (std::max)(1, n - 1));
    const int thick = (std::max)(2, mx.unit / 4);
    o.baseY = o.plot.bottom;

    int prevX = o.plot.left;
    int prevY = o.plot.bottom -
                static_cast<int>((st.values.front() - lo) / span * (spanY - thick));
    prevY = ClampI(prevY, o.plot.top, o.plot.bottom - thick);
    o.points.push_back(Rect(prevX, prevY,
                            (std::min)(prevX + thick * 2, o.plot.right),
                            (std::min)(prevY + thick * 2, o.plot.bottom)));

    for (int i = 1; i < n; ++i) {
        const int x = (std::min)(o.plot.left + i * stepW, o.plot.right - thick * 2);
        int y = o.plot.bottom -
                static_cast<int>((st.values[static_cast<std::size_t>(i)] - lo) /
                                 span * (spanY - thick));
        y = ClampI(y, o.plot.top, o.plot.bottom - thick);
        const int top = (std::min)(prevY, y);
        const int bottom = (std::max)(prevY, y) + thick;
        o.steps.push_back(Rect(prevX, top, (std::max)(x, prevX + 1),
                               (std::min)(bottom, o.plot.bottom)));
        o.points.push_back(Rect(x, y,
                                (std::min)(x + thick * 2, o.plot.right),
                                (std::min)(y + thick * 2, o.plot.bottom)));
        prevX = x;
        prevY = y;
    }
    return o;
}

} // namespace

SparklineLayout MeasureSparkline(const Rect& rc, const SparklineState& st,
                                 const Metrics& mx)
{
    return GeoSpark(rc, st, mx);
}

int SparklinePointAt(const Rect& rc, const SparklineState& st,
                     const Metrics& mx, Point pt)
{
    const SparklineLayout o = GeoSpark(rc, st, mx);
    if (o.plot.Empty() || !o.plot.Contains(pt)) return -1;
    const int n = static_cast<int>(o.points.size());
    for (int i = 0; i < n; ++i) {
        if (Has(o.points[static_cast<std::size_t>(i)], pt)) return i;
    }
    if (n <= 0 || o.plot.Width() <= 0) return -1;
    const int idx = (pt.x - o.plot.left) * (n - 1) / o.plot.Width();
    return ClampI(idx, 0, n - 1);
}

void PaintSparkline(IPainter& g, const Rect& rc, const SparklineState& st,
                    const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const SparklineLayout o = GeoSpark(rc, st, mx);
    if (o.header.Empty()) return;
    const TrendColors tc = Trend(st.change, pal);
    const bool disabled = st.base.disabled;

    g.Text(o.header, st.title, FontRole::Section, pal.textSub, kLeft);
    g.Text(o.header, SignedPercent(st.change), FontRole::Body,
           disabled ? pal.textDisabled : tc.text, kRight);

    if (!st.values.empty()) {
        const double last = st.values.back();
        g.Text(o.valueRc, st.unit + Whole(last), FontRole::Display,
               disabled ? pal.textDisabled : pal.text, kLeft);
    }

    if (o.plot.Empty()) return;
    g.FillRect(o.plot, pal.sunken);
    g.PushClip(o.plot);
    if (st.showArea) {
        const int n = static_cast<int>(o.points.size());
        for (int i = 0; i < n; ++i) {
            const Rect& p = o.points[static_cast<std::size_t>(i)];
            const Rect area(p.left, (std::min)(p.top, o.baseY - 1),
                            p.right, o.baseY);
            if (!area.Empty()) {
                g.FillRect(area, disabled ? pal.sunken : tc.soft);
            }
        }
    }
    for (std::size_t i = 0; i < o.steps.size(); ++i) {
        if (!o.steps[i].Empty()) {
            g.FillRect(o.steps[i], disabled ? pal.textDisabled : tc.line);
        }
    }
    for (std::size_t i = 0; i < o.points.size(); ++i) {
        const Rect& p = o.points[i];
        if (p.Empty()) continue;
        const int idx = static_cast<int>(i);
        if (idx == st.hovered) {
            g.StrokeRound(p, mx.radiusSm, pal.selectRing, 1);
        } else if (idx + 1 == static_cast<int>(o.points.size())) {
            g.FillRound(p, mx.radiusSm, tc.line);
        }
    }
    g.PopClip();
}

namespace {

// ============================================================
// Waterfall
// ============================================================
struct WaterfallValue
{
    double from = 0.0;
    double to = 0.0;
};

WaterfallLayout GeoWaterfall(const Rect& rc, const WaterfallState& st,
                             const Metrics& mx)
{
    WaterfallLayout o;
    if (rc.Empty() || st.bars.empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.rowH * 3) return o;
    o.titleRc = Rect(inner.left, inner.top, inner.right,
                     inner.top + mx.rowH);
    o.axis = Rect(inner.left, inner.bottom - mx.rowH,
                  inner.right, inner.bottom);
    o.plot = Rect(inner.left, o.titleRc.bottom + mx.unit / 2,
                  inner.right, o.axis.top - mx.unit / 2);
    if (o.plot.Empty()) return o;

    const int n = static_cast<int>(st.bars.size());
    std::vector<WaterfallValue> vals(static_cast<std::size_t>(n));
    double running = 0.0;
    double lo = 0.0;
    double hi = 0.0;
    for (int i = 0; i < n; ++i) {
        const WaterfallBar& b = st.bars[static_cast<std::size_t>(i)];
        if (b.total) {
            vals[static_cast<std::size_t>(i)].from = 0.0;
            vals[static_cast<std::size_t>(i)].to = b.delta;
            running = b.delta;
        } else {
            vals[static_cast<std::size_t>(i)].from = running;
            vals[static_cast<std::size_t>(i)].to = running + b.delta;
            running += b.delta;
        }
        lo = (std::min)(lo, (std::min)(vals[static_cast<std::size_t>(i)].from,
                                       vals[static_cast<std::size_t>(i)].to));
        hi = (std::max)(hi, (std::max)(vals[static_cast<std::size_t>(i)].from,
                                       vals[static_cast<std::size_t>(i)].to));
    }
    const double span = (std::max)(1.0, hi - lo);
    const auto yFor = [&](double v) {
        const double t = (v - lo) / span;
        return o.plot.bottom - static_cast<int>(t * o.plot.Height());
    };
    o.zeroY = ClampI(yFor(0.0), o.plot.top, o.plot.bottom);

    const int slotW = o.plot.Width() / n;
    const int barW = (std::max)(2, slotW * 3 / 5);
    const int thick = (std::max)(1, mx.unit / 8);
    for (int i = 0; i < n; ++i) {
        const int x = o.plot.left + i * slotW + (slotW - barW) / 2;
        const WaterfallValue& v = vals[static_cast<std::size_t>(i)];
        const int y1 = ClampI(yFor(v.from), o.plot.top, o.plot.bottom);
        const int y2 = ClampI(yFor(v.to), o.plot.top, o.plot.bottom);
        const int top = (std::min)(y1, y2);
        const int bottom = (std::max)(y1 + 1, y2 + 1);
        o.bars.push_back(Rect(x, top, (std::min)(x + barW, o.plot.right),
                              ClampI(bottom, top + 1, o.plot.bottom)));
        o.labels.push_back(Rect(o.plot.left + i * slotW, o.axis.top,
                                o.plot.left + (i + 1) * slotW, o.axis.bottom));
        if (i > 0) {
            const Rect prev = o.bars[static_cast<std::size_t>(i - 1)];
            const int linkY = ClampI(yFor(v.from), o.plot.top, o.plot.bottom);
            const int x1 = prev.right;
            const int x2 = x;
            if (x2 > x1) {
                const Rect link(x1, linkY, x2,
                                (std::min)(linkY + thick, o.plot.bottom));
                if (!link.Empty()) o.connectors.push_back(link);
            }
        }
    }
    return o;
}

} // namespace

WaterfallLayout MeasureWaterfall(const Rect& rc, const WaterfallState& st,
                                 const Metrics& mx)
{
    return GeoWaterfall(rc, st, mx);
}

int WaterfallBarAt(const Rect& rc, const WaterfallState& st,
                   const Metrics& mx, Point pt)
{
    const WaterfallLayout o = GeoWaterfall(rc, st, mx);
    if (o.plot.Empty() || !o.plot.Contains(pt)) return -1;
    for (std::size_t i = 0; i < o.bars.size(); ++i) {
        if (Has(o.bars[i], pt)) return static_cast<int>(i);
    }
    if (o.labels.empty()) return -1;
    for (std::size_t i = 0; i < o.labels.size(); ++i) {
        const Rect column(o.labels[i].left, o.plot.top,
                          o.labels[i].right, o.plot.bottom);
        if (Has(column, pt)) return static_cast<int>(i);
    }
    return -1;
}

void PaintWaterfall(IPainter& g, const Rect& rc, const WaterfallState& st,
                    const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const WaterfallLayout o = GeoWaterfall(rc, st, mx);
    if (o.titleRc.Empty()) return;
    const bool disabled = st.base.disabled;
    g.Text(o.titleRc, st.title, FontRole::Section, pal.text, kLeft);
    if (o.plot.Empty()) return;

    g.FillRect(Rect(o.plot.left, o.zeroY, o.plot.right,
                    (std::min)(o.zeroY + 1, o.plot.bottom)), pal.split);
    g.PushClip(o.plot);
    for (std::size_t i = 0; i < o.connectors.size(); ++i) {
        if (!o.connectors[i].Empty()) g.FillRect(o.connectors[i], pal.split);
    }
    for (std::size_t i = 0; i < o.bars.size(); ++i) {
        const Rect& bar = o.bars[i];
        if (bar.Empty()) continue;
        const WaterfallBar& item = st.bars[i];
        Color fill = pal.primary;
        if (!item.total) fill = item.delta >= 0 ? pal.error : pal.ok;
        if (disabled) fill = pal.textDisabled;
        g.FillRound(bar, mx.radiusSm, fill);
        if (static_cast<int>(i) == st.hovered) {
            g.StrokeRound(bar, mx.radiusSm, pal.selectRing, 1);
        }
    }
    g.PopClip();
    for (std::size_t i = 0; i < o.labels.size(); ++i) {
        if (o.labels[i].Empty()) continue;
        g.Text(o.labels[i], st.bars[i].label, FontRole::Meta,
               pal.textFaint, kCenter);
    }
}

namespace {

// ============================================================
// StackedBar
// ============================================================
StackedBarLayout GeoStacked(const Rect& rc, const StackedBarState& st,
                            const Metrics& mx)
{
    StackedBarLayout o;
    if (rc.Empty() || st.rows.empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.rowH * 2) return o;
    o.header = Rect(inner.left, inner.top, inner.right,
                    inner.top + mx.rowH);
    const int legendH = st.legend.empty() ? 0 : mx.tagH + mx.unit / 2;
    o.legendRc = Rect(inner.left, o.header.bottom, inner.right,
                      o.header.bottom + legendH);
    o.body = Rect(inner.left, o.legendRc.bottom + (legendH > 0 ? mx.unit / 2 : 0),
                  inner.right, inner.bottom);
    if (o.body.Empty()) return o;

    const int n = static_cast<int>(st.rows.size());
    const int labelW = (std::min)(mx.unit * 10, o.body.Width() / 3);
    const int rowH = (std::min)(mx.rowH, o.body.Height() / n);
    if (rowH <= 0) return o;
    const int barH = (std::max)(4, rowH - mx.unit);
    const int gap = (std::max)(1, mx.unit / 8);
    for (int i = 0; i < n; ++i) {
        const int top = o.body.top + i * rowH;
        const Rect row(inner.left, top, inner.right, top + rowH);
        o.rowRects.push_back(row);
        o.labels.push_back(Rect(row.left, top, row.left + labelW, top + rowH));
        const int barLeft = row.left + labelW;
        const int barRight = row.right;
        const int barTop = top + (rowH - barH) / 2;
        const int barBottom = barTop + barH;
        const std::vector<double>& vals = st.rows[static_cast<std::size_t>(i)].values;
        double total = 0.0;
        for (double v : vals) total += (std::max)(0.0, v);
        std::vector<Rect> segs;
        const int m = static_cast<int>(vals.size());
        if (total <= 0.0 || barRight <= barLeft) {
            for (int j = 0; j < m; ++j) segs.push_back(Rect{});
            o.segments.push_back(segs);
            continue;
        }
        int x = barLeft;
        for (int j = 0; j < m; ++j) {
            const double v = (std::max)(0.0, vals[static_cast<std::size_t>(j)]);
            int w = static_cast<int>((barRight - barLeft) * v / total);
            if (j == m - 1) w = barRight - x;
            w = ClampI(w, 0, barRight - x);
            const Rect seg(x, barTop, x + w, barBottom);
            segs.push_back(seg);
            x += w + gap;
            if (x >= barRight) {
                for (int k = j + 1; k < m; ++k) segs.push_back(Rect{});
                break;
            }
        }
        if (static_cast<int>(segs.size()) < m) {
            while (static_cast<int>(segs.size()) < m) segs.push_back(Rect{});
        }
        o.segments.push_back(segs);
    }
    return o;
}

} // namespace

StackedBarLayout MeasureStackedBar(const Rect& rc,
                                   const StackedBarState& st,
                                   const Metrics& mx)
{
    return GeoStacked(rc, st, mx);
}

StackedBarHit HitStackedBar(const Rect& rc, const StackedBarState& st,
                            const Metrics& mx, Point pt)
{
    StackedBarHit h;
    const StackedBarLayout o = GeoStacked(rc, st, mx);
    if (o.body.Empty()) return h;
    for (std::size_t i = 0; i < o.segments.size(); ++i) {
        for (std::size_t j = 0; j < o.segments[i].size(); ++j) {
            if (Has(o.segments[i][j], pt)) {
                h.row = static_cast<int>(i);
                h.seg = static_cast<int>(j);
                return h;
            }
        }
    }
    return h;
}

void PaintStackedBar(IPainter& g, const Rect& rc, const StackedBarState& st,
                     const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const StackedBarLayout o = GeoStacked(rc, st, mx);
    if (o.header.Empty()) return;
    g.Text(o.header, st.title, FontRole::Section, pal.text, kLeft);

    if (!o.legendRc.Empty() && !st.legend.empty()) {
        g.PushClip(o.legendRc);
        const int itemW = (std::min)(mx.unit * 14,
                          o.legendRc.Width() /
                              (std::max)(1, static_cast<int>(st.legend.size())));
        for (std::size_t i = 0; i < st.legend.size(); ++i) {
            const int x = o.legendRc.left +
                          static_cast<int>(i) * itemW;
            const Rect dot(x, o.legendRc.top + mx.unit / 4,
                           x + mx.tagH / 2,
                           o.legendRc.top + mx.unit / 4 + mx.tagH / 2);
            const Rect text(dot.right + mx.unit / 2, o.legendRc.top,
                            x + itemW, o.legendRc.bottom);
            if (!dot.Empty()) g.FillRound(dot, mx.radiusSm,
                                          SeriesColor(static_cast<int>(i), pal));
            if (!text.Empty()) {
                g.Text(text, st.legend[i], FontRole::Meta, pal.textSub, kLeft);
            }
        }
        g.PopClip();
    }
    if (o.body.Empty()) return;
    g.PushClip(o.body);
    for (std::size_t i = 0; i < o.rowRects.size(); ++i) {
        if (!o.labels[i].Empty()) {
            g.Text(o.labels[i], st.rows[i].label, FontRole::Body,
                   pal.textSub, kLeft);
        }
        for (std::size_t j = 0; j < o.segments[i].size(); ++j) {
            const Rect& seg = o.segments[i][j];
            if (seg.Empty()) continue;
            const int si = static_cast<int>(j);
            g.FillRect(seg, st.base.disabled ? pal.sunken
                                             : SeriesColor(si, pal));
            if (st.hoveredRow == static_cast<int>(i) &&
                st.hoveredSeg == si) {
                g.StrokeRound(seg, mx.radiusSm, pal.selectRing, 1);
            }
            if (seg.Width() > mx.unit * 6) {
                double total = 0.0;
                for (double v : st.rows[i].values) {
                    total += (std::max)(0.0, v);
                }
                const double part =
                    (std::max)(0.0, st.rows[i].values[j]);
                const int pct = total > 0.0
                    ? static_cast<int>(part / total * 100.0 + 0.5) : 0;
                g.Text(seg, std::to_wstring(pct) + L"%", FontRole::Meta,
                       pal.surface, kCenter);
            }
        }
    }
    g.PopClip();
}

namespace {

// ============================================================
// GaugeMeter
// ============================================================
GaugeLayout GeoGauge(const Rect& rc, const GaugeState& st, const Metrics& mx)
{
    GaugeLayout o;
    if (rc.Empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.rowH * 3) return o;
    o.header = Rect(inner.left, inner.top, inner.right,
                    inner.top + mx.rowH);
    o.valueRc = Rect(inner.left, o.header.bottom, inner.right,
                     o.header.bottom + mx.rowH + mx.unit);
    o.scale = Rect(inner.left, inner.bottom - mx.tagH,
                   inner.right, inner.bottom);
    o.band = Rect(inner.left, o.valueRc.bottom + mx.unit / 2,
                  inner.right, o.scale.top - mx.unit / 2);
    if (o.band.Empty()) return o;

    const int n = ClampI(st.segments, 4, 48);
    const int gap = (std::max)(1, mx.unit / 8);
    const int slot = o.band.Width() / n;
    if (slot <= gap) return o;
    for (int i = 0; i < n; ++i) {
        const int x = o.band.left + i * slot;
        const double d = (std::max)(0.0,
            (static_cast<double>(i) - (n - 1) / 2.0) /
                (std::max)(1.0, n / 2.0));
        const int h = (std::max)(mx.unit,
            o.band.Height() - static_cast<int>(o.band.Height() * 0.35 * d));
        o.segments.push_back(Rect(x + gap / 2, o.band.bottom - h,
                                  x + slot - gap / 2, o.band.bottom));
    }
    const double span = (std::max)(1.0, st.max - st.min);
    const double t = ClampD((st.warnAt - st.min) / span, 0.0, 1.0);
    const int tx = o.band.left + static_cast<int>(t * o.band.Width());
    o.threshold = Rect(ClampI(tx, o.band.left, o.band.right - 1),
                       o.band.top,
                       ClampI(tx + 1, o.band.left + 1, o.band.right),
                       o.band.bottom);
    return o;
}

} // namespace

GaugeLayout MeasureGauge(const Rect& rc, const GaugeState& st,
                         const Metrics& mx)
{
    return GeoGauge(rc, st, mx);
}

int GaugeSegmentAt(const Rect& rc, const GaugeState& st,
                   const Metrics& mx, Point pt)
{
    const GaugeLayout o = GeoGauge(rc, st, mx);
    if (o.band.Empty()) return -1;
    for (std::size_t i = 0; i < o.segments.size(); ++i) {
        if (Has(o.segments[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void PaintGauge(IPainter& g, const Rect& rc, const GaugeState& st,
                const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const GaugeLayout o = GeoGauge(rc, st, mx);
    if (o.header.Empty()) return;
    const double span = (std::max)(1.0, st.max - st.min);
    const double value = ClampD(st.value, st.min, st.max);
    const bool over = value >= st.warnAt;
    const bool disabled = st.base.disabled;

    g.Text(o.header, st.title, FontRole::Section, pal.text, kLeft);
    g.Text(o.valueRc, Whole(value) + L" " + st.unit, FontRole::Display,
           disabled ? pal.textDisabled : (over ? pal.errorText : pal.text),
           kLeft);
    g.Text(o.valueRc, over ? L"已超过阈值" : L"运行正常", FontRole::Meta,
           over ? pal.errorText : pal.okText, kRight);

    if (o.band.Empty()) return;
    g.PushClip(o.band);
    const int active = o.segments.empty() ? 0 :
        ClampI(static_cast<int>((value - st.min) / span *
                                static_cast<double>(o.segments.size()) + 0.5),
               0, static_cast<int>(o.segments.size()));
    for (std::size_t i = 0; i < o.segments.size(); ++i) {
        const Rect& seg = o.segments[i];
        if (seg.Empty()) continue;
        Color fill = pal.sunken;
        if (static_cast<int>(i) < active) {
            fill = over ? pal.error : pal.primary;
        }
        if (disabled) fill = pal.sunken;
        g.FillRound(seg, mx.radiusSm, fill);
        if (static_cast<int>(i) == st.hovered) {
            g.StrokeRound(seg, mx.radiusSm, pal.selectRing, 1);
        }
    }
    if (!o.threshold.Empty()) g.FillRect(o.threshold, pal.warn);
    g.PopClip();
    if (!o.scale.Empty()) {
        g.Text(o.scale, Whole(st.min), FontRole::Meta, pal.textFaint, kLeft);
        g.Text(o.scale, L"阈值 " + Whole(st.warnAt), FontRole::Meta,
               pal.warnText, kCenter);
        g.Text(o.scale, Whole(st.max), FontRole::Meta, pal.textFaint, kRight);
    }
}

} // namespace ui
} // namespace air
