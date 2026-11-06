// AirUI - ControlsEnterpriseData.cpp : platform-free data widgets.
// Fourth wave: TreeMap, Funnel, Diff, ScoreBreakdown.
#include "airui/ControlsEnterprise.h"

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

bool Hit(const Rect& r, Point p)
{
    return !r.Empty() && r.Contains(p);
}

Rect Inset(const Rect& r, int d)
{
    return Rect(r.left + d, r.top + d, r.right - d, r.bottom - d);
}

std::wstring OneDecimal(double v)
{
    const int clamped = ClampI(static_cast<int>(v * 10.0 + 0.5), 0, 100);
    return std::to_wstring(clamped / 10) + L"." +
           std::to_wstring(clamped % 10);
}

struct SemanticColor
{
    Color fill, edge, text;
};

SemanticColor Semantic(int index, const Palette& pal)
{
    switch (index % 5) {
    case 0:  return SemanticColor{pal.primarySoft, pal.primary, pal.primaryText};
    case 1:  return SemanticColor{pal.okSoft, pal.ok, pal.okText};
    case 2:  return SemanticColor{pal.warnSoft, pal.warn, pal.warnText};
    case 3:  return SemanticColor{pal.errorSoft, pal.error, pal.errorText};
    default: return SemanticColor{pal.iconTile, pal.chipBorder, pal.textSub};
    }
}

// ============================================================
// TreeMap - recursive weighted slice-and-dice
// ============================================================
void SliceTiles(const Rect& area, const std::vector<int>& ids,
                const TreeMapState& st, const Metrics& mx,
                std::vector<Rect>& out)
{
    if (area.Empty() || ids.empty()) return;
    if (ids.size() == 1) {
        out[static_cast<std::size_t>(ids[0])] = area;
        return;
    }
    long long total = 0;
    for (int id : ids) {
        total += (std::max)(1, st.items[static_cast<std::size_t>(id)].weight);
    }
    if (total <= 0) return;
    std::size_t cut = 1;
    long long running = 0;
    long long best = total;
    for (std::size_t k = 1; k < ids.size(); ++k) {
        running += (std::max)(1, st.items[static_cast<std::size_t>(ids[k - 1])].weight);
        const long long diff = running * 2 - total;
        if (diff < 0 ? -diff < best : diff < best) {
            best = diff < 0 ? -diff : diff;
            cut = k;
        }
    }
    long long leftWeight = 0;
    for (std::size_t k = 0; k < cut; ++k) {
        leftWeight += (std::max)(1, st.items[static_cast<std::size_t>(ids[k])].weight);
    }
    const std::vector<int> a(ids.begin(), ids.begin() + static_cast<std::ptrdiff_t>(cut));
    const std::vector<int> b(ids.begin() + static_cast<std::ptrdiff_t>(cut), ids.end());
    const bool vertical = area.Width() >= area.Height();
    const int span = vertical ? area.Width() : area.Height();
    int split = static_cast<int>(span * leftWeight / total);
    const int minSpan = (std::max)(mx.unit * 2, 8);
    if (split < minSpan || span - split < minSpan) split = span / 2;
    Rect first = area;
    Rect second = area;
    if (vertical) {
        first.right = area.left + split;
        second.left = area.left + split;
    } else {
        first.bottom = area.top + split;
        second.top = area.top + split;
    }
    SliceTiles(first, a, st, mx, out);
    SliceTiles(second, b, st, mx, out);
}

TreeMapLayout GeoTreeMap(const Rect& rc, const TreeMapState& st,
                         const Metrics& mx)
{
    TreeMapLayout o;
    if (rc.Empty() || st.items.empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty()) return o;
    const std::size_t n = st.items.size();
    o.tiles.assign(n, Rect{});
    std::vector<int> ids;
    for (std::size_t i = 0; i < n; ++i) ids.push_back(static_cast<int>(i));
    SliceTiles(inner, ids, st, mx, o.tiles);
    const int gap = (std::max)(1, mx.unit / 4);
    for (std::size_t i = 0; i < n; ++i) {
        Rect& tile = o.tiles[i];
        if (tile.Empty()) continue;
        const Rect shrunk = Inset(tile, gap);
        tile = shrunk.Empty() ? Rect{} : shrunk;
        if (!inner.Contains(tile)) tile = Rect{};
    }
    return o;
}

// ============================================================
// Funnel
// ============================================================
FunnelLayout GeoFunnel(const Rect& rc, const FunnelState& st,
                       const Metrics& mx)
{
    FunnelLayout o;
    if (rc.Empty() || st.stages.empty()) return o;
    const Rect inner = Inset(rc, mx.unit);
    if (inner.Empty() || inner.Height() <= mx.groupH) return o;
    o.titleRc = Rect(inner.left, inner.top, inner.right,
                     inner.top + mx.groupH);
    const Rect body(o.titleRc.left, o.titleRc.bottom,
                    o.titleRc.right, inner.bottom);
    const int n = static_cast<int>(st.stages.size());
    const int gap = (std::max)(2, mx.unit / 2);
    const int barAreaW = body.Width() * 3 / 4;
    const int rateW = body.Width() - barAreaW - gap;
    if (barAreaW < mx.unit * 6 || rateW < mx.unit * 3) return o;
    const int rowH = (std::min)(mx.rowH + mx.unit / 2, body.Height() / n);
    if (rowH <= gap * 2) return o;
    int maxValue = 0;
    for (const FunnelStage& s : st.stages) maxValue = (std::max)(maxValue, s.value);
    if (maxValue <= 0) maxValue = 1;
    const int usedH = rowH * n;
    const int top = body.top + (std::max)(0, (body.Height() - usedH) / 2);
    for (int i = 0; i < n; ++i) {
        const int y = top + i * rowH;
        const int w = static_cast<int>(
            static_cast<long long>(barAreaW) *
            (std::max)(0, st.stages[static_cast<std::size_t>(i)].value) / maxValue);
        // Keep late, tiny stages wide enough for their label and value.
        const int minW = (std::min)(barAreaW, mx.unit * 14);
        const int width = (std::max)(minW, w);
        const int x = body.left + (barAreaW - width) / 2;
        o.bars.push_back(Rect(x, y + gap / 2, x + width, y + rowH - gap / 2));
        o.rates.push_back(Rect(body.left + barAreaW + gap, y,
                               body.right, y + rowH));
    }
    return o;
}

// ============================================================
// Diff
// ============================================================
DiffLayout GeoDiff(const Rect& rc, const DiffState& st, const Metrics& mx)
{
    DiffLayout o;
    if (rc.Empty() || st.lines.empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() <= mx.groupH) return o;
    o.header = Rect(inner.left, inner.top, inner.right,
                    inner.top + mx.groupH);
    o.body = Rect(inner.left, o.header.bottom, inner.right, inner.bottom);
    o.lineH = (std::min)(mx.rowH * 3 / 4, mx.rowH);
    o.gutterW = mx.unit * 9;
    if (o.gutterW >= o.body.Width() - mx.unit * 4) return o;
    const int n = static_cast<int>(st.lines.size());
    const int visible = (std::min)(n, o.body.Height() / (std::max)(1, o.lineH));
    for (int i = 0; i < visible; ++i) {
        o.rows.push_back(Rect(o.body.left, o.body.top + i * o.lineH,
                              o.body.right, o.body.top + (i + 1) * o.lineH));
    }
    return o;
}

// ============================================================
// Score
// ============================================================
ScoreLayout GeoScore(const Rect& rc, const ScoreState& st, const Metrics& mx)
{
    ScoreLayout o;
    if (rc.Empty() || st.counts.empty()) return o;
    const Rect inner = Inset(rc, mx.unit);
    if (inner.Empty()) return o;
    const int summaryH = (std::min)(mx.itemH, inner.Height() / 2);
    if (summaryH < mx.rowH) return o;
    o.summary = Rect(inner.left, inner.top, inner.right, inner.top + summaryH);
    o.barsArea = Rect(inner.left, o.summary.bottom, inner.right, inner.bottom);
    const int n = (std::min)(5, static_cast<int>(st.counts.size()));
    if (n <= 0 || o.barsArea.Empty()) return o;
    o.rowH = (std::min)(mx.rowH, o.barsArea.Height() / n);
    if (o.rowH <= 0) return o;
    const int labelW = mx.unit * 6;
    const int pctW = mx.unit * 6;
    if (o.barsArea.Width() <= labelW + pctW + mx.unit * 2) return o;
    int maxCount = 0;
    for (int i = 0; i < n; ++i) {
        maxCount = (std::max)(maxCount, st.counts[static_cast<std::size_t>(i)]);
    }
    if (maxCount <= 0) maxCount = 1;
    for (int i = 0; i < n; ++i) {
        const int y = o.barsArea.top + i * o.rowH;
        o.labels.push_back(Rect(o.barsArea.left, y,
                                o.barsArea.left + labelW, y + o.rowH));
        o.bars.push_back(Rect(o.barsArea.left + labelW, y,
                              o.barsArea.right - pctW, y + o.rowH));
    }
    return o;
}

} // namespace

// ============================================================
// TreeMap
// ============================================================
TreeMapLayout MeasureTreeMap(const Rect& rc, const TreeMapState& st,
                             const Metrics& mx)
{
    return GeoTreeMap(rc, st, mx);
}

int TreeMapTileAt(const Rect& rc, const TreeMapState& st,
                  const Metrics& mx, Point pt)
{
    if (rc.Empty() || !rc.Contains(pt)) return -1;
    const TreeMapLayout lay = GeoTreeMap(rc, st, mx);
    for (std::size_t i = 0; i < lay.tiles.size(); ++i) {
        if (Hit(lay.tiles[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void PaintTreeMap(IPainter& g, const Rect& rc, const TreeMapState& st,
                  const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const TreeMapLayout lay = GeoTreeMap(rc, st, mx);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty()) return;
    g.PushClip(inner);
    for (std::size_t i = 0; i < lay.tiles.size(); ++i) {
        const Rect& tile = lay.tiles[i];
        if (tile.Empty()) continue;
        const TileItem& item = st.items[i];
        const SemanticColor sc = Semantic(static_cast<int>(i), pal);
        const bool hot = st.hovered == static_cast<int>(i);
        g.FillRound(tile, mx.radiusSm,
                    hot ? sc.edge : (item.tint != 0 ? item.tint : sc.fill));
        g.StrokeRound(tile, mx.radiusSm, sc.edge, hot ? 2 : 1);
        const Rect textRc = Inset(tile, mx.unit / 2);
        if (textRc.Empty()) continue;
        g.Text(textRc, item.label, FontRole::Body,
               hot ? pal.textInverse : sc.text, kCenter);
        if (textRc.Height() >= mx.rowH) {
            const Rect weightRc(textRc.left, textRc.bottom - mx.rowH / 2,
                                textRc.right, textRc.bottom);
            g.Text(weightRc, std::to_wstring((std::max)(0, item.weight)),
                   FontRole::Meta, hot ? pal.textInverse : sc.text, kCenter);
        }
    }
    g.PopClip();
}

// ============================================================
// Funnel
// ============================================================
FunnelLayout MeasureFunnel(const Rect& rc, const FunnelState& st,
                           const Metrics& mx)
{
    return GeoFunnel(rc, st, mx);
}

int FunnelStageAt(const Rect& rc, const FunnelState& st,
                  const Metrics& mx, Point pt)
{
    if (rc.Empty() || !rc.Contains(pt)) return -1;
    const FunnelLayout lay = GeoFunnel(rc, st, mx);
    for (std::size_t i = 0; i < lay.bars.size(); ++i) {
        if (Hit(lay.bars[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void PaintFunnel(IPainter& g, const Rect& rc, const FunnelState& st,
                 const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const FunnelLayout lay = GeoFunnel(rc, st, mx);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    const Rect inner = Inset(rc, mx.unit);
    if (inner.Empty()) return;
    g.PushClip(inner);
    if (!lay.titleRc.Empty()) {
        g.Text(lay.titleRc, st.title, FontRole::Section, pal.text, kLeft);
    }
    const int first = st.stages.empty() ? 0 : st.stages[0].value;
    for (std::size_t i = 0; i < lay.bars.size(); ++i) {
        const Rect& bar = lay.bars[i];
        if (bar.Empty()) continue;
        const FunnelStage& stage = st.stages[i];
        const SemanticColor sc = Semantic(static_cast<int>(i), pal);
        const bool hot = st.hovered == static_cast<int>(i);
        g.FillRound(bar, mx.radiusSm, hot ? sc.edge : sc.fill);
        g.StrokeRound(bar, mx.radiusSm, sc.edge, 1);
        const Rect labelRc = Inset(bar, mx.unit / 2);
        if (!labelRc.Empty()) {
            g.Text(labelRc, stage.label, FontRole::Body,
                   hot ? pal.textInverse : sc.text, kLeft);
            g.Text(labelRc, std::to_wstring(stage.value), FontRole::Body,
                   hot ? pal.textInverse : sc.text, kRight);
        }
        if (!st.showRate || i >= lay.rates.size()) continue;
        const Rect& rateRc = lay.rates[i];
        if (rateRc.Empty()) continue;
        const int pct = first > 0
                            ? ClampI(static_cast<int>(
                                         static_cast<long long>(stage.value) *
                                         100 / first), 0, 100)
                            : 0;
        g.Text(rateRc, std::to_wstring(pct) + L"%", FontRole::Meta,
               i == 0 ? pal.textFaint : pal.textSub, kLeft);
    }
    g.PopClip();
}

// ============================================================
// Diff
// ============================================================
DiffLayout MeasureDiff(const Rect& rc, const DiffState& st, const Metrics& mx)
{
    return GeoDiff(rc, st, mx);
}

int DiffLineAt(const Rect& rc, const DiffState& st,
               const Metrics& mx, Point pt)
{
    if (rc.Empty() || !rc.Contains(pt)) return -1;
    const DiffLayout lay = GeoDiff(rc, st, mx);
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        if (Hit(lay.rows[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void PaintDiff(IPainter& g, const Rect& rc, const DiffState& st,
               const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const DiffLayout lay = GeoDiff(rc, st, mx);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty()) return;
    g.PushClip(inner);
    if (!lay.header.Empty()) {
        g.FillRect(lay.header, pal.sunken);
        g.Text(Inset(lay.header, mx.unit / 2), st.file, FontRole::Body,
               pal.textSub, kLeft);
        int added = 0;
        int removed = 0;
        for (const DiffLine& line : st.lines) {
            if (line.kind == DiffKind::Added) ++added;
            if (line.kind == DiffKind::Removed) ++removed;
        }
        g.Text(Inset(lay.header, mx.unit / 2),
               L"+" + std::to_wstring(added) + L"  \x2212" +
                   std::to_wstring(removed),
               FontRole::Meta, pal.textFaint, kRight);
    }
    const int numW = mx.unit * 4;
    int oldNo = st.firstOld;
    int newNo = st.firstNew;
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        const DiffLine& line = st.lines[i];
        const bool add = line.kind == DiffKind::Added;
        const bool del = line.kind == DiffKind::Removed;
        const Rect& row = lay.rows[i];
        if (add) g.FillRect(row, pal.okSoft);
        else if (del) g.FillRect(row, pal.errorSoft);
        else if (st.hovered == static_cast<int>(i)) g.FillRect(row, pal.hover);
        const Rect oldRc(row.left, row.top, row.left + numW, row.bottom);
        const Rect newRc(oldRc.right, row.top, oldRc.right + numW, row.bottom);
        const Rect signRc(newRc.right, row.top,
                          (std::min)(newRc.right + mx.unit * 2, row.right),
                          row.bottom);
        const Rect textRc((std::min)(signRc.right, row.right), row.top,
                          row.right, row.bottom);
        const Color numColor = add ? pal.okText
                                   : (del ? pal.errorText : pal.textFaint);
        const Color bodyColor = add ? pal.okText
                                    : (del ? pal.errorText : pal.text);
        if (!add) {
            g.Text(oldRc, std::to_wstring(oldNo), FontRole::Meta,
                   pal.textFaint, kRight);
            ++oldNo;
        }
        if (!del) {
            g.Text(newRc, std::to_wstring(newNo), FontRole::Meta,
                   pal.textFaint, kRight);
            ++newNo;
        }
        g.Text(signRc, add ? L"+" : (del ? L"\x2212" : L" "),
               FontRole::Body, numColor, kCenter);
        if (!textRc.Empty()) {
            g.Text(textRc, line.text, FontRole::Body, bodyColor, kLeft);
        }
    }
    g.PopClip();
}

// ============================================================
// Score
// ============================================================
ScoreLayout MeasureScore(const Rect& rc, const ScoreState& st,
                         const Metrics& mx)
{
    return GeoScore(rc, st, mx);
}

int ScoreBarAt(const Rect& rc, const ScoreState& st,
               const Metrics& mx, Point pt)
{
    if (rc.Empty() || !rc.Contains(pt)) return -1;
    const ScoreLayout lay = GeoScore(rc, st, mx);
    for (std::size_t i = 0; i < lay.bars.size(); ++i) {
        if (Hit(lay.bars[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void PaintScore(IPainter& g, const Rect& rc, const ScoreState& st,
                const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const ScoreLayout lay = GeoScore(rc, st, mx);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    const Rect inner = Inset(rc, mx.unit);
    if (inner.Empty()) return;
    g.PushClip(inner);
    if (!lay.summary.Empty()) {
        const Rect scoreRc(lay.summary.left, lay.summary.top,
                           lay.summary.left + mx.unit * 12,
                           lay.summary.bottom);
        g.Text(scoreRc, OneDecimal(st.average), FontRole::Display,
               pal.primaryText, kLeft);
        const Rect infoRc((std::min)(scoreRc.right, lay.summary.right),
                          lay.summary.top, lay.summary.right,
                          lay.summary.bottom);
        if (!infoRc.Empty()) {
            g.Text(infoRc, st.title, FontRole::Section, pal.text, kLeft);
            g.Text(infoRc, L"共 " + std::to_wstring(st.total) + L" 条评价",
                   FontRole::Meta, pal.textFaint, kRight);
        }
    }
    int maxCount = 1;
    for (std::size_t i = 0; i < lay.bars.size(); ++i) {
        maxCount = (std::max)(maxCount, st.counts[i]);
    }
    for (std::size_t i = 0; i < lay.bars.size(); ++i) {
        const Rect& track = lay.bars[i];
        if (track.Empty()) continue;
        const Rect labelRc = i < lay.labels.size() ? lay.labels[i] : Rect{};
        if (!labelRc.Empty()) {
            g.Text(labelRc, std::to_wstring(5 - static_cast<int>(i)) + L" 星",
                   FontRole::Meta, pal.textSub, kLeft);
        }
        const int barH = (std::max)(2, mx.unit);
        const Rect bar(track.left,
                       track.top + (track.Height() - barH) / 2,
                       track.right,
                       track.top + (track.Height() + barH) / 2);
        g.FillRound(bar, mx.radiusSm, pal.sunken);
        const int w = static_cast<int>(static_cast<long long>(bar.Width()) *
                                       (std::max)(0, st.counts[i]) / maxCount);
        if (w > 0) {
            g.FillRound(Rect(bar.left, bar.top, bar.left + w, bar.bottom),
                        mx.radiusSm,
                        st.hovered == static_cast<int>(i)
                            ? pal.primaryHover : pal.primary);
        }
        const Rect pctRc(track.right, track.top, inner.right, track.bottom);
        if (!pctRc.Empty()) {
            const int pct = st.total > 0
                                ? ClampI(static_cast<int>(
                                             static_cast<long long>(
                                                 st.counts[i]) * 100 /
                                             st.total), 0, 100)
                                : 0;
            g.Text(pctRc, std::to_wstring(pct) + L"%", FontRole::Meta,
                   pal.textFaint, kRight);
        }
    }
    g.PopClip();
}

} // namespace ui
} // namespace air
