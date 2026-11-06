// ============================================================
// AirUI - ControlsInput.cpp
// Platform-free P2 input controls: SearchBox, PasswordBox,
// TextArea, NumberInput, DatePicker, ColorPicker. Hit and paint
// share geometry through file-local Measure* helpers; all drawing
// goes through IPainter with pal.* colors and mx.* sizes. An empty
// rect produces no draw calls.
// ============================================================
#include "airui/ControlsExtra.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace air {
namespace ui {
namespace {

// Text formats: all carry dt::NoPrefix; left/ellipsis variants also
// truncate, single-line text always has an alignment.
constexpr TextFormat kLeftText =
    dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kCenterText =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix;
constexpr TextFormat kCenterEllipsis =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;

constexpr int kDefaultSwatchCount = 12;

int ClampInt(int v, int lo, int hi)
{
    return (std::min)(hi, (std::max)(lo, v));
}

Color BodyText(const ControlState& st, const Palette& pal)
{
    return st.disabled ? pal.textDisabled : pal.text;
}

Color FrameBorder(const ControlState& st, bool focus, const Palette& pal)
{
    if (st.disabled) return pal.border;
    if (focus || st.focused) return pal.primary;
    if (st.hover) return pal.hoverBorder;
    return pal.border;
}

void PaintFieldFrame(IPainter& g, const Rect& rc, const ControlState& st,
                     bool focus, const Palette& pal, const Metrics& mx)
{
    g.FillRound(rc, mx.radiusMd, st.disabled ? pal.sunken : pal.surface);
    g.StrokeRound(rc, mx.radiusMd, FrameBorder(st, focus, pal), 1);
}

// ---- shared single-line input geometry ----
struct FieldLayout
{
    Rect frame{}, glyph{}, text{}, action{};
    bool hasGlyph = false;
    bool hasAction = false;
};

FieldLayout MeasureInputField(const Rect& rc, const Metrics& mx,
                              bool glyph, bool action)
{
    FieldLayout out;
    if (rc.Empty()) return out;
    const int pad = mx.unit;
    int sq = rc.Height() - pad;
    const int cap = rc.Width() / 3;
    if (sq > cap) sq = cap;
    if (sq < 1) sq = 1;
    const int vpad = pad / 2;
    out.frame = rc;
    out.hasGlyph = glyph;
    out.hasAction = action;
    if (glyph)
        out.glyph = Rect(rc.left + pad, rc.top + vpad,
                         rc.left + pad + sq, rc.bottom - vpad);
    if (action)
        out.action = Rect(rc.right - pad - sq, rc.top + vpad,
                          rc.right - pad, rc.bottom - vpad);
    const int textLeft = glyph ? out.glyph.right + vpad : rc.left + pad;
    const int textRight = action ? out.action.left - vpad : rc.right - pad;
    out.text = Rect(textLeft, rc.top,
                    (std::max)(textLeft, textRight), rc.bottom);
    return out;
}

FieldLayout MeasureSearchBox(const Rect& rc, const SearchBoxState& st,
                             const Metrics& mx)
{
    return MeasureInputField(rc, mx, true, !st.text.empty());
}

FieldLayout MeasurePasswordBox(const Rect& rc, const Metrics& mx)
{
    return MeasureInputField(rc, mx, false, true);
}

// ---- number input geometry ----
struct NumberLayout
{
    Rect frame{}, minus{}, plus{}, value{};
};

NumberLayout MeasureNumberInput(const Rect& rc, const Metrics& mx)
{
    NumberLayout out;
    if (rc.Empty()) return out;
    const int pad = mx.unit / 2;
    int btnW = rc.Height() - pad;
    const int cap = rc.Width() / 3;
    if (btnW > cap) btnW = cap;
    if (btnW < 1) btnW = 1;
    out.frame = rc;
    out.minus = Rect(rc.left, rc.top, rc.left + btnW, rc.bottom);
    out.plus  = Rect(rc.right - btnW, rc.top, rc.right, rc.bottom);
    out.value = Rect(rc.left + btnW, rc.top,
                     (std::max)(rc.left + btnW, rc.right - btnW), rc.bottom);
    return out;
}

// ---- date picker geometry ----
struct DateLayout
{
    Rect frame{}, header{}, prev{}, next{}, weekdays{}, grid{};
    int  cellW = 0;
    int  cellH = 0;
};

DateLayout MeasureDatePicker(const Rect& rc, const Metrics& mx)
{
    DateLayout out;
    if (rc.Empty()) return out;
    const int pad = mx.unit;
    const int headerH = mx.rowH;
    const int weekdayH = mx.rowH;
    const int gridH = rc.Height() - pad * 2 - headerH - weekdayH;
    const int innerW = rc.Width() - pad * 2;
    if (gridH < 6 || innerW < 7) return out;
    out.cellW = innerW / 7;
    out.cellH = gridH / 6;
    if (out.cellW < 1 || out.cellH < 1) return out;
    const int top = rc.top + pad;
    out.frame = rc;
    out.header = Rect(rc.left + pad, top, rc.right - pad, top + headerH);
    int nav = headerH;
    const int navCap = out.header.Width() / 3;
    if (nav > navCap) nav = navCap;
    if (nav < 1) nav = 1;
    out.prev = Rect(out.header.left, out.header.top,
                    out.header.left + nav, out.header.bottom);
    out.next = Rect(out.header.right - nav, out.header.top,
                    out.header.right, out.header.bottom);
    const int wkTop = top + headerH;
    out.weekdays = Rect(rc.left + pad, wkTop,
                        rc.right - pad, wkTop + weekdayH);
    const int gridTop = wkTop + weekdayH;
    out.grid = Rect(rc.left + pad, gridTop,
                    rc.left + pad + out.cellW * 7, gridTop + out.cellH * 6);
    return out;
}

Rect DateCell(const DateLayout& lay, int index)
{
    const int x = lay.grid.left + (index % 7) * lay.cellW;
    const int y = lay.grid.top + (index / 7) * lay.cellH;
    return Rect(x, y, x + lay.cellW, y + lay.cellH);
}

// ---- color picker geometry ----
struct ColorLayout
{
    Rect frame{};
    std::vector<Rect> cells;
};

int SwatchCount(const ColorPickerState& st)
{
    return st.swatches.empty()
         ? kDefaultSwatchCount : static_cast<int>(st.swatches.size());
}

std::vector<Color> EffectiveSwatches(const ColorPickerState& st,
                                     const Palette& pal)
{
    if (!st.swatches.empty()) return st.swatches;
    return { pal.primary, pal.primaryHover, pal.primarySoft, pal.primaryText,
             pal.ok, pal.okSoft, pal.warn, pal.warnSoft,
             pal.error, pal.errorSoft, pal.sunken, pal.border };
}

ColorLayout MeasureColorPicker(const Rect& rc, const ColorPickerState& st,
                               const Metrics& mx)
{
    ColorLayout out;
    if (rc.Empty()) return out;
    const int n = SwatchCount(st);
    if (n < 1) return out;
    const int pad = mx.unit;
    const int gap = (std::max)(1, mx.unit / 2);
    const int innerW = rc.Width() - pad * 2;
    const int innerH = rc.Height() - pad * 2;
    if (innerW < 1 || innerH < 1) return out;
    int cols = innerW / (mx.rowH + gap);
    cols = ClampInt(cols, 1, n);
    const int rows = (n + cols - 1) / cols;
    const int slotW = innerW / cols;
    const int slotH = (std::max)(1, innerH / rows);
    int side = (std::min)(slotW, slotH) - gap;
    if (side < 1) side = 1;
    out.frame = rc;
    out.cells.reserve(static_cast<std::size_t>(n));
    const int x0 = rc.left + pad;
    const int y0 = rc.top + pad;
    for (int i = 0; i < n; ++i) {
        const int sx = x0 + (i % cols) * slotW + (slotW - side) / 2;
        const int sy = y0 + (i / cols) * slotH + (slotH - side) / 2;
        out.cells.push_back(Rect(sx, sy, sx + side, sy + side));
    }
    return out;
}

} // namespace

// ---- SearchBox ----
SearchBoxHit HitSearchBox(const Rect& rc, const SearchBoxState& st,
                          const Metrics& mx, Point pt)
{
    if (rc.Empty() || !rc.Contains(pt)) return SearchBoxHit::None;
    const FieldLayout lay = MeasureSearchBox(rc, st, mx);
    if (lay.hasAction && lay.action.Contains(pt)) return SearchBoxHit::Clear;
    return SearchBoxHit::Body;
}

void PaintSearchBox(IPainter& g, const Rect& rc, const SearchBoxState& st,
                    const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const FieldLayout lay = MeasureSearchBox(rc, st, mx);
    PaintFieldFrame(g, rc, st.base, st.hasFocus, pal, mx);
    g.PushClip(Rect(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1));
    g.Text(lay.glyph, L"\xE721", FontRole::Icon,
           st.base.disabled ? pal.textDisabled : pal.textSub, kCenterText);
    const bool empty = st.text.empty();
    const std::wstring& shown = empty ? st.placeholder : st.text;
    const Color textCol = st.base.disabled ? pal.textDisabled
                        : empty ? pal.textFaint : pal.text;
    g.Text(lay.text, shown, FontRole::Body, textCol, kLeftText);
    if (lay.hasAction) {
        if (st.clearHover && !st.base.disabled)
            g.FillRound(lay.action, mx.radiusSm, pal.hover);
        g.Text(lay.action, L"\u00D7", FontRole::Body,
               st.base.disabled ? pal.textDisabled : pal.textSub, kCenterText);
    }
    g.PopClip();
}

// ---- PasswordBox ----
PasswordBoxHit HitPasswordBox(const Rect& rc, const PasswordBoxState& st,
                              const Metrics& mx, Point pt)
{
    (void)st;
    if (rc.Empty() || !rc.Contains(pt)) return PasswordBoxHit::None;
    const FieldLayout lay = MeasurePasswordBox(rc, mx);
    if (lay.action.Contains(pt)) return PasswordBoxHit::Reveal;
    return PasswordBoxHit::Body;
}

void PaintPasswordBox(IPainter& g, const Rect& rc, const PasswordBoxState& st,
                      const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const FieldLayout lay = MeasurePasswordBox(rc, mx);
    PaintFieldFrame(g, rc, st.base, st.hasFocus, pal, mx);
    g.PushClip(Rect(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1));
    const bool empty = st.text.empty();
    std::wstring shown;
    if (empty) shown = st.placeholder;
    else if (st.revealed) shown = st.text;
    else shown.assign(st.text.size(), L'\u2022');
    const Color textCol = st.base.disabled ? pal.textDisabled
                        : empty ? pal.textFaint : pal.text;
    g.Text(lay.text, shown, FontRole::Body, textCol, kLeftText);
    if (st.toggleHover && !st.base.disabled)
        g.FillRound(lay.action, mx.radiusSm, pal.hover);
    const Color revealCol = st.base.disabled ? pal.textDisabled
                          : st.revealed ? pal.primaryText : pal.textSub;
    g.Text(lay.action, L"\xE890", FontRole::Icon, revealCol, kCenterText);
    g.PopClip();
}

// ---- TextArea ----
void PaintTextArea(IPainter& g, const Rect& rc, const TextAreaState& st,
                   const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    PaintFieldFrame(g, rc, st.base, st.hasFocus, pal, mx);
    const int pad = mx.unit;
    const Rect inner(rc.left + pad, rc.top + pad / 2,
                     rc.right - pad, rc.bottom - pad / 2);
    if (inner.Empty()) return;
    g.PushClip(inner);
    if (st.text.empty())
        g.Text(inner, st.placeholder, FontRole::Body,
               st.base.disabled ? pal.textDisabled : pal.textFaint, kLeftText);
    else
        g.TextBlock(inner, st.text, FontRole::Body,
                    BodyText(st.base, pal), (std::max)(1, st.maxLines));
    g.PopClip();
}

// ---- NumberInput ----
NumberInputHit HitNumberInput(const Rect& rc, const NumberInputState& st,
                              const Metrics& mx, Point pt)
{
    (void)st;
    if (rc.Empty() || !rc.Contains(pt)) return NumberInputHit::None;
    const NumberLayout lay = MeasureNumberInput(rc, mx);
    if (lay.minus.Contains(pt)) return NumberInputHit::Minus;
    if (lay.plus.Contains(pt)) return NumberInputHit::Plus;
    return NumberInputHit::Body;
}

void PaintNumberInput(IPainter& g, const Rect& rc, const NumberInputState& st,
                      const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const NumberLayout lay = MeasureNumberInput(rc, mx);
    PaintFieldFrame(g, rc, st.base, st.base.focused, pal, mx);
    g.PushClip(Rect(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1));
    const Color glyphCol = st.base.disabled ? pal.textDisabled : pal.textSub;
    if (st.minusHover && !st.base.disabled)
        g.FillRound(lay.minus, mx.radiusSm, pal.hover);
    g.Text(lay.minus, L"\u2212", FontRole::Body, glyphCol, kCenterText);
    if (st.plusHover && !st.base.disabled)
        g.FillRound(lay.plus, mx.radiusSm, pal.hover);
    g.Text(lay.plus, L"+", FontRole::Body, glyphCol, kCenterText);
    g.Text(lay.value, std::to_wstring(st.value), FontRole::Body,
           BodyText(st.base, pal), kCenterEllipsis);
    g.FillRect(Rect(lay.minus.right, rc.top,
                    lay.minus.right + 1, rc.bottom), pal.split);
    g.FillRect(Rect(lay.plus.left - 1, rc.top,
                    lay.plus.left, rc.bottom), pal.split);
    g.PopClip();
}

// ---- calendar arithmetic (platform free) ----
int DaysInMonth(int year, int month)
{
    if (month < 1 || month > 12) return 0;
    const int days[] = {31, 28, 31, 30, 31, 30,
                        31, 31, 30, 31, 30, 31};
    int d = days[month - 1];
    if (month == 2 &&
        ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)))
        d = 29;
    return d;
}

int FirstWeekday(int year, int month)
{
    if (month < 1 || month > 12) return 0;
    const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = year;
    if (month < 3) y -= 1;
    const int w = (y + y / 4 - y / 100 + y / 400 + t[month - 1] + 1) % 7;
    return (w + 7) % 7;
}

// ---- DatePicker ----
int DatePickerDayAt(const Rect& rc, const DatePickerState& st,
                    const Metrics& mx, Point pt)
{
    if (rc.Empty() || !rc.Contains(pt)) return -1;
    const DateLayout lay = MeasureDatePicker(rc, mx);
    if (lay.prev.Contains(pt)) return -2;
    if (lay.next.Contains(pt)) return -3;
    if (lay.cellW < 1 || lay.cellH < 1 || !lay.grid.Contains(pt)) return -1;
    const int col = (pt.x - lay.grid.left) / lay.cellW;
    const int row = (pt.y - lay.grid.top) / lay.cellH;
    if (col < 0 || col >= 7 || row < 0 || row >= 6) return -1;
    const int index = row * 7 + col;
    const int day = index - FirstWeekday(st.year, st.month) + 1;
    const int dim = DaysInMonth(st.year, st.month);
    if (day < 1 || day > dim) return -1;
    return day;
}

void PaintDatePicker(IPainter& g, const Rect& rc, const DatePickerState& st,
                     const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const DateLayout lay = MeasureDatePicker(rc, mx);
    g.FillRound(rc, mx.radiusMd, st.base.disabled ? pal.sunken : pal.surface);
    g.StrokeRound(rc, mx.radiusMd, FrameBorder(st.base, false, pal), 1);
    if (lay.cellW < 1 || lay.cellH < 1) return;
    g.PushClip(Rect(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1));
    const std::wstring title = std::to_wstring(st.year) + L"\u5E74"
                             + std::to_wstring(st.month) + L"\u6708";
    g.Text(lay.header, title, FontRole::Body,
           st.base.disabled ? pal.textDisabled : pal.text, kCenterEllipsis);
    const Color navCol = st.base.disabled ? pal.textDisabled : pal.textSub;
    if (st.hoveredNav == 1 && !st.base.disabled)
        g.FillRound(lay.prev, mx.radiusSm, pal.hover);
    g.Text(lay.prev, L"\u2039", FontRole::Body, navCol, kCenterText);
    if (st.hoveredNav == 2 && !st.base.disabled)
        g.FillRound(lay.next, mx.radiusSm, pal.hover);
    g.Text(lay.next, L"\u203A", FontRole::Body, navCol, kCenterText);
    const wchar_t* week[7] = { L"\u65E5", L"\u4E00", L"\u4E8C", L"\u4E09",
                               L"\u56DB", L"\u4E94", L"\u516D" };
    for (int c = 0; c < 7; ++c) {
        const Rect cell(lay.grid.left + c * lay.cellW, lay.weekdays.top,
                        lay.grid.left + (c + 1) * lay.cellW,
                        lay.weekdays.bottom);
        g.Text(cell, week[c], FontRole::Meta, pal.textFaint, kCenterText);
    }
    // st.day doubles as the "today" marker: core carries no clock.
    const int first = FirstWeekday(st.year, st.month);
    const int dim = DaysInMonth(st.year, st.month);
    for (int i = 0; i < 42; ++i) {
        const int day = i - first + 1;
        if (day < 1 || day > dim) continue;
        const Rect cell = DateCell(lay, i);
        const int inset = (cell.Width() > 6 && cell.Height() > 6) ? 1 : 0;
        const Rect box(cell.left + inset, cell.top + inset,
                       cell.right - inset, cell.bottom - inset);
        const bool sel = (day == st.day);
        const bool hov = (day == st.hoveredDay && !st.base.disabled);
        const bool today = (day == st.day && !st.base.disabled);
        if (sel)
            g.FillRound(box, mx.radiusSm,
                        st.base.disabled ? pal.border : pal.primary);
        else if (hov)
            g.FillRound(box, mx.radiusSm, pal.hover);
        if (today)
            g.StrokeRound(box, mx.radiusSm, pal.selectRing, 1);
        const Color col = st.base.disabled ? pal.textDisabled
                        : sel ? pal.textInverse
                        : hov ? pal.primaryText : pal.text;
        g.Text(cell, std::to_wstring(day), FontRole::Body, col, kCenterText);
    }
    g.PopClip();
}

// ---- ColorPicker ----
int ColorPickerSwatchAt(const Rect& rc, const ColorPickerState& st,
                        const Metrics& mx, Point pt)
{
    if (rc.Empty() || !rc.Contains(pt)) return -1;
    const ColorLayout lay = MeasureColorPicker(rc, st, mx);
    for (std::size_t i = 0; i < lay.cells.size(); ++i)
        if (lay.cells[i].Contains(pt)) return static_cast<int>(i);
    return -1;
}

void PaintColorPicker(IPainter& g, const Rect& rc, const ColorPickerState& st,
                      const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const ColorLayout lay = MeasureColorPicker(rc, st, mx);
    g.FillRound(rc, mx.radiusMd, st.base.disabled ? pal.sunken : pal.surface);
    g.StrokeRound(rc, mx.radiusMd, FrameBorder(st.base, false, pal), 1);
    if (lay.cells.empty()) return;
    const std::vector<Color> sw = EffectiveSwatches(st, pal);
    g.PushClip(Rect(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1));
    const std::size_t count = (std::min)(lay.cells.size(), sw.size());
    for (std::size_t i = 0; i < count; ++i) {
        const Rect& cell = lay.cells[i];
        const bool sel = (static_cast<int>(i) == st.selected);
        const bool hov = (static_cast<int>(i) == st.hovered);
        g.FillRound(cell, mx.radiusSm, sw[i]);
        g.StrokeRound(cell, mx.radiusSm, pal.split, 1);
        if (sel && !st.base.disabled) {
            g.StrokeRound(cell, mx.radiusSm, pal.primary, 2);
            g.Text(cell, L"\u2713", FontRole::Body,
                   pal.textInverse, kCenterText);
        } else if (hov && !st.base.disabled) {
            g.StrokeRound(cell, mx.radiusSm, pal.selectRing, 2);
        }
    }
    g.PopClip();
}

} // namespace ui
} // namespace air
