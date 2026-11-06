// AirUI - ControlsAdvancedInput.cpp : platform-free input controls.
// Third wave: TagInput, RangeSlider, OtpInput, Cascader.
// Every Measure* helper is the single source of geometry for both the
// painter and the hit test; no text measurement is available there, so
// text widths are estimated from the character count and font metrics.
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
constexpr TextFormat kGlyph =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix;

int ClampI(int v, int lo, int hi)
{
    return (std::min)((std::max)(v, lo), hi);
}

bool Hit(const Rect& r, Point p)
{
    return !r.Empty() && r.Contains(p);
}

// Estimate a label width without a painter. CJK glyphs are wider than
// latin ones, so the body font size is used as a conservative unit.
int TextW(const std::wstring& s, const Metrics& mx, FontRole role)
{
    const int unit = role == FontRole::Meta ? mx.fontMeta : mx.fontBody;
    return static_cast<int>(s.size()) * unit;
}

// ------------------------------------------------------------
// TagInput
// ------------------------------------------------------------
void PaintChip(IPainter& g, const TagChipRect& chipRect,
               const std::wstring& text, bool hovered, bool disabled,
               const Palette& pal, const Metrics& mx)
{
    g.FillRound(chipRect.chip, mx.radiusSm,
                disabled ? pal.sunken : pal.primarySoft);
    const Rect label(chipRect.chip.left + mx.unit,
                     chipRect.chip.top,
                     chipRect.close.left,
                     chipRect.chip.bottom);
    if (!label.Empty()) {
        g.Text(label, text, FontRole::Meta,
               disabled ? pal.textDisabled : pal.primaryText, kLeft);
    }
    if (hovered && !disabled) {
        g.FillRound(chipRect.close, mx.radiusSm, pal.surface);
    }
    g.Text(chipRect.close, L"\x00D7", FontRole::Meta,
           disabled ? pal.textDisabled : pal.primaryText, kGlyph);
}

// ------------------------------------------------------------
// RangeSlider
// ------------------------------------------------------------
struct RangeGeo
{
    Rect track{}, fill{}, lowThumb{}, highThumb{};
    int min = 0;
    int max = 0;
    int low = 0;
    int high = 0;
};

RangeGeo GeoRange(const Rect& rc, const RangeSliderState& st, const Metrics& mx)
{
    RangeGeo o;
    if (rc.Empty()) return o;
    const int mn = st.min;
    const int mxv = (std::max)(st.min + 1, st.max);
    const int lo = ClampI((std::min)(st.low, st.high), mn, mxv);
    const int hi = ClampI((std::max)(st.low, st.high), mn, mxv);
    const int thumbW = (std::max)(mx.unit * 2, 8);
    const int thumbH = (std::min)(mx.rowH, rc.Height());
    if (thumbH <= 0) return o;
    const int pad = mx.unit;
    const int left = rc.left + pad + thumbW / 2;
    const int right = rc.right - pad - thumbW / 2;
    if (right <= left) return o;
    const int trackH = (std::max)(4, mx.unit / 2);
    const int cy = rc.top + (rc.Height() - trackH) / 2;
    o.track = Rect(left, cy, right, cy + trackH);
    const int span = mxv - mn;
    const int xl = left + static_cast<int>(static_cast<long long>(lo - mn) *
                                           (right - left) / span);
    const int xh = left + static_cast<int>(static_cast<long long>(hi - mn) *
                                           (right - left) / span);
    o.fill = Rect((std::min)(xl, xh), cy, (std::max)(xl, xh), cy + trackH);
    const int thumbTop = cy + trackH / 2 - thumbH / 2;
    o.lowThumb = Rect(xl - thumbW / 2, thumbTop,
                      xl - thumbW / 2 + thumbW, thumbTop + thumbH);
    o.highThumb = Rect(xh - thumbW / 2, thumbTop,
                       xh - thumbW / 2 + thumbW, thumbTop + thumbH);
    o.min = mn;
    o.max = mxv;
    o.low = lo;
    o.high = hi;
    return o;
}

void PaintThumb(IPainter& g, const Rect& r, bool active, bool disabled,
                const Palette& pal, const Metrics& mx)
{
    g.FillRound(r, mx.radiusSm,
                disabled ? pal.sunken : active ? pal.primary : pal.surface);
    g.StrokeRound(r, mx.radiusSm,
                  disabled ? pal.border : active ? pal.primaryDown : pal.primary,
                  active ? 2 : 1);
}

// ------------------------------------------------------------
// Cascader
// ------------------------------------------------------------
Rect CascaderRows(const Rect& column, const CascaderLayout& lay)
{
    return Rect(column.left, column.top + lay.headerH,
                column.right, column.bottom);
}

} // namespace

TagInputLayout MeasureTagInput(const Rect& rc, const TagInputState& st,
                               const Metrics& mx)
{
    TagInputLayout out;
    if (rc.Empty()) return out;
    const int pad = mx.unit;
    const int innerL = rc.left + pad;
    const int innerR = rc.right - pad;
    const int innerT = rc.top + pad;
    const int innerB = rc.bottom - pad;
    if (innerR <= innerL || innerB <= innerT) return out;
    const int chipH = (std::min)(mx.tagH + 4, innerB - innerT);
    if (chipH <= 0) return out;
    const int chipT = innerT + (innerB - innerT - chipH) / 2;
    const int chipB = chipT + chipH;

    const bool hasClear = !st.tags.empty() && !st.base.disabled;
    const int clearW = hasClear ? mx.unit * 3 : 0;
    if (hasClear) {
        out.clearAll = Rect(innerR - clearW, chipT, innerR, chipB);
    }
    const int limitR = hasClear ? out.clearAll.left - pad / 2 : innerR;
    if (limitR <= innerL) return out;

    const int closeW = mx.unit * 2;
    const int gap = (std::max)(2, pad / 2);
    int x = innerL;
    for (std::size_t i = 0; i < st.tags.size(); ++i) {
        const int w = TextW(st.tags[i], mx, FontRole::Meta) + closeW + pad * 2;
        if (x + w > limitR) {
            out.overflow = true;
            break;
        }
        const Rect chip(x, chipT, x + w, chipB);
        const Rect close(chip.right - closeW - pad / 2,
                         chipT + (std::min)(2, chipH / 4),
                         chip.right - pad / 2,
                         chipB - (std::min)(2, chipH / 4));
        out.chips.push_back(TagChipRect{chip, close});
        x += w + gap;
    }
    const int inputL = out.chips.empty() ? innerL : x;
    if (limitR - inputL > mx.unit * 2) {
        out.input = Rect(inputL, innerT, limitR, innerB);
    } else if (!out.chips.empty()) {
        out.overflow = true;
    }
    return out;
}

TagInputHit HitTagInput(const Rect& rc, const TagInputState& st,
                        const Metrics& mx, Point pt)
{
    TagInputHit h;
    if (!Hit(rc, pt)) return h;
    const TagInputLayout lay = MeasureTagInput(rc, st, mx);
    for (std::size_t i = 0; i < lay.chips.size(); ++i) {
        if (Hit(lay.chips[i].close, pt)) {
            h.tag = static_cast<int>(i);
            h.close = true;
            return h;
        }
    }
    for (std::size_t i = 0; i < lay.chips.size(); ++i) {
        if (Hit(lay.chips[i].chip, pt)) {
            h.tag = static_cast<int>(i);
            return h;
        }
    }
    if (Hit(lay.clearAll, pt)) {
        h.clearAll = true;
        return h;
    }
    if (Hit(lay.input, pt)) h.input = true;
    return h;
}

void PaintTagInput(IPainter& g, const Rect& rc, const TagInputState& st,
                   const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const TagInputLayout lay = MeasureTagInput(rc, st, mx);
    g.PushClip(rc);
    g.FillRound(rc, mx.radiusMd,
                st.base.disabled ? pal.sunken : pal.surface);
    const Color edge = st.base.disabled ? pal.border
                     : st.hasFocus ? pal.selectRing
                     : st.base.hover ? pal.hoverBorder : pal.border;
    g.StrokeRound(rc, mx.radiusMd, edge, st.hasFocus ? 2 : 1);
    for (std::size_t i = 0; i < lay.chips.size(); ++i) {
        PaintChip(g, lay.chips[i], st.tags[i],
                  st.hoveredTag == static_cast<int>(i),
                  st.base.disabled, pal, mx);
    }
    if (!lay.input.Empty()) {
        if (!st.draft.empty()) {
            g.Text(lay.input, st.draft, FontRole::Body,
                   st.base.disabled ? pal.textDisabled : pal.text, kLeft);
        } else if (!st.placeholder.empty()) {
            g.Text(lay.input, st.placeholder, FontRole::Meta,
                   pal.textFaint, kLeft);
        }
    }
    if (!lay.clearAll.Empty()) {
        g.FillRound(lay.clearAll, mx.radiusSm,
                    st.clearAllHover ? pal.hover : pal.sunken);
        g.Text(lay.clearAll, L"\x00D7", FontRole::Meta,
               pal.textSub, kGlyph);
    }
    g.PopClip();
}

RangeSliderLayout MeasureRangeSlider(const Rect& rc,
                                     const RangeSliderState& st,
                                     const Metrics& mx)
{
    const RangeGeo geo = GeoRange(rc, st, mx);
    RangeSliderLayout out;
    out.track = geo.track;
    out.fill = geo.fill;
    out.lowThumb = geo.lowThumb;
    out.highThumb = geo.highThumb;
    return out;
}

int RangeSliderThumbAt(const Rect& rc, const RangeSliderState& st,
                       const Metrics& mx, Point pt)
{
    if (!Hit(rc, pt)) return -2;
    const RangeGeo geo = GeoRange(rc, st, mx);
    if (geo.track.Empty()) return -2;
    if (Hit(geo.highThumb, pt)) return 2;
    if (Hit(geo.lowThumb, pt)) return 1;
    const Rect band(geo.track.left, geo.lowThumb.top,
                    geo.track.right, geo.lowThumb.bottom);
    if (Hit(band, pt)) return -1;
    return -2;
}

int RangeSliderValueAt(const Rect& rc, const RangeSliderState& st,
                       const Metrics& mx, Point pt)
{
    const RangeGeo geo = GeoRange(rc, st, mx);
    if (geo.track.Empty()) {
        return ClampI(st.low, st.min, (std::max)(st.min + 1, st.max));
    }
    // Round to nearest so dragging all the way to the right edge can
    // actually reach max (integer truncation would stop at max-1).
    const int w = (std::max)(1, geo.track.Width());
    const long long num = static_cast<long long>(pt.x - geo.track.left) *
                          (geo.max - geo.min) + w / 2;
    const int raw = geo.min + static_cast<int>(num / w);
    int v = ClampI(raw, geo.min, geo.max);
    const int step = (std::max)(1, st.step);
    v = geo.min + ((v - geo.min) / step) * step;
    return ClampI(v, geo.min, geo.max);
}

void PaintRangeSlider(IPainter& g, const Rect& rc,
                      const RangeSliderState& st,
                      const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const RangeGeo geo = GeoRange(rc, st, mx);
    if (geo.track.Empty()) return;
    const bool disabled = st.base.disabled;
    g.PushClip(rc);
    g.FillRound(geo.track, mx.radiusSm, pal.sunken);
    g.StrokeRound(geo.track, mx.radiusSm,
                  disabled ? pal.border : pal.split, 1);
    if (!geo.fill.Empty()) {
        g.FillRect(geo.fill, disabled ? pal.border : pal.primary);
    }
    if (st.showTicks) {
        const int tickW = (std::max)(1, mx.unit / 8);
        const int tickH = (std::max)(2, mx.unit / 2);
        for (int i = 1; i < 10; ++i) {
            const int x = geo.track.left + geo.track.Width() * i / 10;
            g.FillRect(Rect(x, geo.track.bottom + 1,
                            x + tickW, geo.track.bottom + 1 + tickH),
                       disabled ? pal.split : pal.border);
        }
    }
    const bool lowActive = st.hoveredThumb == 1 || st.draggingThumb == 1;
    const bool highActive = st.hoveredThumb == 2 || st.draggingThumb == 2;
    PaintThumb(g, geo.lowThumb, lowActive, disabled, pal, mx);
    PaintThumb(g, geo.highThumb, highActive, disabled, pal, mx);
    g.PopClip();
}

OtpInputLayout MeasureOtpInput(const Rect& rc, const OtpInputState& st,
                               const Metrics& mx)
{
    OtpInputLayout out;
    if (rc.Empty()) return out;
    const int n = st.length;
    if (n <= 0) return out;
    const int pad = mx.unit;
    out.gap = mx.unit;
    const int availW = rc.Width() - pad * 2 - out.gap * (n - 1);
    if (availW <= 0) return out;
    out.cellW = (std::min)(mx.rowH + mx.unit, availW / n);
    if (out.cellW < mx.unit * 2) return out;
    const int totalW = out.cellW * n + out.gap * (n - 1);
    const int startX = rc.left + (rc.Width() - totalW) / 2;
    const int cellH = (std::min)(mx.inputH + mx.unit * 2,
                                 rc.Height() - pad * 2);
    if (cellH <= 0) return out;
    const int top = rc.top + (rc.Height() - cellH) / 2;
    for (int i = 0; i < n; ++i) {
        const int x = startX + i * (out.cellW + out.gap);
        out.cells.push_back(Rect(x, top, x + out.cellW, top + cellH));
    }
    return out;
}

int OtpCellAt(const Rect& rc, const OtpInputState& st,
              const Metrics& mx, Point pt)
{
    if (!Hit(rc, pt)) return -1;
    const OtpInputLayout lay = MeasureOtpInput(rc, st, mx);
    for (std::size_t i = 0; i < lay.cells.size(); ++i) {
        if (Hit(lay.cells[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void PaintOtpInput(IPainter& g, const Rect& rc, const OtpInputState& st,
                   const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const OtpInputLayout lay = MeasureOtpInput(rc, st, mx);
    if (lay.cells.empty()) return;
    g.PushClip(rc);
    for (std::size_t i = 0; i < lay.cells.size(); ++i) {
        const Rect& cell = lay.cells[i];
        const bool focused = !st.base.disabled && st.focused == static_cast<int>(i);
        const bool hovered = !st.base.disabled && st.hovered == static_cast<int>(i);
        g.FillRound(cell, mx.radiusMd,
                    st.base.disabled ? pal.sunken
                    : hovered ? pal.hover : pal.surface);
        g.StrokeRound(cell, mx.radiusMd,
                      st.base.disabled ? pal.border
                      : focused ? pal.selectRing
                      : hovered ? pal.hoverBorder : pal.border,
                      focused ? 2 : 1);
        const wchar_t ch = (static_cast<int>(i) < static_cast<int>(st.code.size()))
                         ? st.code[static_cast<std::size_t>(i)] : L'\0';
        if (ch != L'\0') {
            const std::wstring s = st.masked ? L"\x2022" : std::wstring(1, ch);
            g.Text(cell, s, FontRole::Title,
                   st.base.disabled ? pal.textDisabled : pal.text, kCenter);
        } else if (focused) {
            const int caretW = (std::max)(1, mx.unit / 4);
            const int caretH = cell.Height() / 2;
            const int cx = (cell.left + cell.right) / 2;
            g.FillRect(Rect(cx - caretW / 2, cell.top + (cell.Height() - caretH) / 2,
                            cx - caretW / 2 + caretW,
                            cell.top + (cell.Height() + caretH) / 2),
                       pal.primary);
        }
    }
    g.PopClip();
}

CascaderLayout MeasureCascader(const Rect& rc, const CascaderState& st,
                               const Metrics& mx)
{
    CascaderLayout out;
    if (rc.Empty() || st.columns.empty()) return out;
    const int n = static_cast<int>(st.columns.size());
    const int pad = mx.unit;
    const int innerW = rc.Width() - pad * 2;
    if (innerW < n || rc.Height() <= pad * 2) return out;
    const int colW = innerW / n;
    out.headerH = (std::min)(mx.rowH, (rc.Height() - pad * 2) / 2);
    out.rowH = mx.rowH;
    if (out.headerH <= 0 || out.rowH <= 0) return out;
    for (int i = 0; i < n; ++i) {
        const int l = rc.left + pad + i * colW;
        const int r = (i == n - 1) ? rc.right - pad : l + colW;
        out.columnRects.push_back(Rect(l, rc.top + pad, r, rc.bottom - pad));
    }
    return out;
}

CascaderHit HitCascader(const Rect& rc, const CascaderState& st,
                        const Metrics& mx, Point pt)
{
    CascaderHit h;
    if (!Hit(rc, pt)) return h;
    const CascaderLayout lay = MeasureCascader(rc, st, mx);
    for (std::size_t i = 0; i < lay.columnRects.size(); ++i) {
        const Rect rows = CascaderRows(lay.columnRects[i], lay);
        if (!Hit(rows, pt)) continue;
        const int row = (pt.y - rows.top) / lay.rowH;
        if (row < 0 || row >= static_cast<int>(st.columns[i].items.size())) {
            return h;
        }
        h.column = static_cast<int>(i);
        h.row = row;
        return h;
    }
    return h;
}

void PaintCascader(IPainter& g, const Rect& rc, const CascaderState& st,
                   const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const CascaderLayout lay = MeasureCascader(rc, st, mx);
    if (lay.columnRects.empty()) return;
    g.PushClip(rc);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    for (std::size_t ci = 0; ci < lay.columnRects.size(); ++ci) {
        const Rect& col = lay.columnRects[ci];
        if (ci > 0) {
            g.FillRect(Rect(col.left - 1, col.top, col.left, col.bottom), pal.split);
        }
        const Rect head(col.left, col.top, col.right, col.top + lay.headerH);
        g.FillRect(head, pal.sunken);
        g.Text(Rect(head.left + mx.unit / 2, head.top,
                    head.right - mx.unit / 2, head.bottom),
               st.columns[ci].title, FontRole::Title, pal.textSub, kLeft);
        const Rect rows = CascaderRows(col, lay);
        if (rows.Empty()) continue;
        g.PushClip(rows);
        const CascaderColumn& data = st.columns[ci];
        const int selected = (static_cast<int>(ci) < static_cast<int>(st.selected.size()))
                           ? st.selected[ci] : -1;
        for (int r = 0; r < static_cast<int>(data.items.size()); ++r) {
            const int y = rows.top + r * lay.rowH;
            if (y >= rows.bottom) break;
            const Rect row(col.left, y, col.right,
                           (std::min)(y + lay.rowH, rows.bottom));
            const bool hovered = st.hoveredColumn == static_cast<int>(ci) &&
                                 st.hoveredRow == r;
            const Color fill = selected == r ? pal.selectFill
                             : hovered ? pal.hover : pal.surface;
            g.FillRect(row, fill);
            if (selected == r) {
                g.FillRect(Rect(row.left, row.top, row.left + (std::max)(2, mx.unit / 3),
                                row.bottom), pal.primary);
            }
            const bool hasNext = ci + 1 < lay.columnRects.size();
            const int tail = hasNext ? mx.unit * 3 : mx.unit / 2;
            g.Text(Rect(row.left + mx.unit, row.top,
                        (std::max)(row.left + mx.unit, row.right - tail), row.bottom),
                   data.items[static_cast<std::size_t>(r)], FontRole::Body,
                   st.base.disabled ? pal.textDisabled
                   : selected == r ? pal.primaryText : pal.text,
                   kLeft);
            if (hasNext) {
                g.Text(Rect(row.right - tail, row.top, row.right - mx.unit / 2, row.bottom),
                       L"\x203A", FontRole::Title, pal.textFaint, kRight);
            }
        }
        g.PopClip();
    }
    g.PopClip();
}

} // namespace ui
} // namespace air
