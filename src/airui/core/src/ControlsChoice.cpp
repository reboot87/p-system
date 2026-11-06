// ============================================================
// AirUI - ControlsChoice.cpp
// Platform-free selection controls:
//   Dropdown / Menu / ContextMenu / MultiSelect / Transfer
//
// Every control is a pure function of (rect, state, palette,
// metrics) and reaches the backend only through IPainter. The
// Measure* helpers are shared by painting and by pointer hit
// testing, so geometry is defined in exactly one place.
// ============================================================
#include "airui/ControlsExtra.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace air {
namespace ui {

namespace {

// All text is single line and prefix free; long strings get an
// ellipsis. Colors always come from the semantic palette.
constexpr TextFormat kLeftEllip =
    dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kRightEllip =
    dt::SingleLine | dt::Right | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kCenterText =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix;

// ---- shared menu geometry -------------------------------------------------
// One helper owns every size the menu needs: row heights (separators
// are thin), column widths and paddings. MenuItemHeight,
// MenuContentHeight, MenuItemAt, PaintMenu and PaintContextMenu all
// derive their geometry from it, so hit testing and painting agree.
struct MenuGeom
{
    int pad = 0;
    int gap = 0;
    int glyphW = 0;
    int rowH = 0;
    int sepH = 0;
    int shortcutW = 0;

    int RowHeight(const MenuItemState& item) const
    {
        return item.separator ? sepH : rowH;
    }
};

MenuGeom MakeMenuGeom(const Metrics& mx)
{
    MenuGeom g;
    g.pad = mx.unit;
    g.gap = mx.unit;
    g.glyphW = (std::max)(8, mx.rowH - mx.unit);
    g.rowH = mx.rowH + mx.unit / 2;
    g.sepH = (std::max)(4, mx.unit - 2);
    g.shortcutW = mx.rowH * 2;
    return g;
}

// ---- shared MultiSelect row geometry --------------------------------------
int MultiSelectRowHeight(const Metrics& mx)
{
    return mx.rowH + mx.unit;
}

Rect MultiSelectRowRect(const Rect& rc, int row, const Metrics& mx)
{
    const int h = MultiSelectRowHeight(mx);
    const int top = rc.top + 1 + row * h;
    return Rect(rc.left + 1, top, rc.right - 1, top + h);
}

int MultiSelectVisibleRows(const Rect& rc, int itemCount, const Metrics& mx)
{
    const int rowH = MultiSelectRowHeight(mx);
    if (rowH <= 0 || rc.Empty() || itemCount <= 0) return 0;
    const int innerH = static_cast<int>(rc.Height()) - 2;
    if (innerH <= 0) return 0;
    return (std::min)(itemCount, innerH / rowH);
}

bool RowChecked(const std::vector<bool>& v, int row)
{
    return row >= 0 && row < static_cast<int>(v.size()) &&
           v[static_cast<std::size_t>(row)];
}

// ---- shared Transfer row geometry -----------------------------------------
Rect TransferRowRect(const Rect& box, int row, int rowH)
{
    const int top = box.top + row * rowH;
    return Rect(box.left + 1, top, box.right - 1, top + rowH);
}

int TransferVisibleRows(const Rect& box, int itemCount, int rowH)
{
    if (rowH <= 0 || box.Empty() || itemCount <= 0) return 0;
    const int innerH = static_cast<int>(box.Height()) - 2;
    if (innerH <= 0) return 0;
    return (std::min)(itemCount, innerH / rowH);
}

void PaintTransferBox(IPainter& g, const Rect& box,
                      const std::vector<std::wstring>& items,
                      const std::vector<bool>& checked,
                      int hovered, bool disabled,
                      const Palette& pal, const Metrics& mx, int rowH)
{
    if (box.Empty()) return;
    g.FillRound(box, mx.radiusMd, disabled ? pal.sunken : pal.surface);
    g.StrokeRound(box, mx.radiusMd, pal.split, 1);

    const Rect inner(box.left + 1, box.top + 1, box.right - 1, box.bottom - 1);
    if (inner.Empty()) return;
    if (items.empty()) {
        g.Text(inner, L"暂无数据", FontRole::Meta, pal.textFaint, kCenterText);
        return;
    }

    const int visible = TransferVisibleRows(box, static_cast<int>(items.size()), rowH);
    g.PushClip(inner);
    for (int i = 0; i < visible; ++i) {
        const Rect rowRc = TransferRowRect(box, i, rowH);
        if (i == hovered) g.FillRect(rowRc, pal.hover);

        ControlState cs;
        cs.checked = RowChecked(checked, i);
        cs.disabled = disabled;
        cs.hover = (i == hovered);
        PaintCheckbox(g, rowRc, items[static_cast<std::size_t>(i)], cs, pal);

        if (i + 1 < visible) {
            g.FillRect(Rect(rowRc.left, rowRc.bottom - 1,
                            rowRc.right, rowRc.bottom), pal.split);
        }
    }
    g.PopClip();
}

void PaintTransferButton(IPainter& g, const Rect& rc, const wchar_t* glyph,
                         bool enabled, const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    g.FillRound(rc, mx.radiusSm, enabled ? pal.surface : pal.sunken);
    g.StrokeRound(rc, mx.radiusSm, pal.border, 1);
    g.Text(rc, glyph, FontRole::Body,
           enabled ? pal.primaryText : pal.textDisabled, kCenterText);
}

} // namespace

// ============================================================
// Dropdown
// ============================================================

DropdownLayout MeasureDropdown(const Rect& rc, const DropdownState& st,
                               const Metrics& mx)
{
    DropdownLayout out;
    if (rc.Empty()) return out;

    const int fieldH = (std::min)(static_cast<int>(rc.Height()), mx.inputH);
    if (fieldH <= 0) return out;
    out.optionH = mx.rowH;
    out.field = Rect(rc.left, rc.top, rc.right, rc.top + fieldH);

    // The option list is an overlay: it starts at the field bottom and
    // keeps its full height so hit testing and painting see identical
    // rectangles even when it extends below the control rect.
    if (st.open && !st.options.empty()) {
        const int n = static_cast<int>(st.options.size());
        const int top = out.field.bottom;
        out.list = Rect(rc.left, top, rc.right, top + out.optionH * n);
    }
    return out;
}

int DropdownOptionAt(const Rect& rc, const DropdownState& st,
                     const Metrics& mx, Point pt)
{
    const DropdownLayout lay = MeasureDropdown(rc, st, mx);
    if (lay.field.Empty()) return -2;
    if (lay.field.Contains(pt)) return -1;
    if (!lay.list.Empty() && lay.list.Contains(pt)) {
        const int row = (pt.y - lay.list.top) / lay.optionH;
        if (row >= 0 && row < static_cast<int>(st.options.size())) return row;
    }
    return -2;
}

void PaintDropdown(IPainter& g, const Rect& rc, const DropdownState& st,
                   const Palette& pal, const Metrics& mx)
{
    const DropdownLayout lay = MeasureDropdown(rc, st, mx);
    if (lay.field.Empty()) return;

    const int pad = mx.unit;
    const Color bg = st.base.disabled ? pal.sunken : pal.surface;
    g.FillRound(lay.field, mx.radiusMd, bg);
    const Color border = st.base.disabled ? pal.border
                       : (st.base.hover || st.base.focused || st.open)
                             ? pal.primary : pal.border;
    g.StrokeRound(lay.field, mx.radiusMd, border, 1);

    const int selected = st.selected;
    const bool hasSelection = selected >= 0 &&
        selected < static_cast<int>(st.options.size());
    const std::wstring& label = !st.text.empty() ? st.text
        : hasSelection ? st.options[static_cast<std::size_t>(selected)]
                       : st.placeholder;
    const Color txt = st.base.disabled ? pal.textDisabled
        : (!st.text.empty() || hasSelection) ? pal.text : pal.textFaint;
    g.Text(Rect(lay.field.left + pad, lay.field.top,
                lay.field.right - pad - mx.rowH, lay.field.bottom),
           label, FontRole::Body, txt, kLeftEllip);
    g.Text(Rect(lay.field.right - mx.rowH, lay.field.top,
                lay.field.right, lay.field.bottom),
           L"\u25BE", FontRole::Body,
           st.base.disabled ? pal.textDisabled : pal.textSub, kCenterText);

    if (lay.list.Empty()) return;

    g.FillRound(lay.list, mx.radiusMd, pal.surface);
    g.StrokeRound(lay.list, mx.radiusMd, pal.split, 1);
    const Rect inner(lay.list.left + 1, lay.list.top + 1,
                     lay.list.right - 1, lay.list.bottom - 1);
    if (inner.Empty()) return;
    g.PushClip(inner);
    const int n = static_cast<int>(st.options.size());
    for (int i = 0; i < n; ++i) {
        const int y = lay.list.top + i * lay.optionH;
        const Rect rowRc(lay.list.left + 1, y, lay.list.right - 1, y + lay.optionH);

        if (i == st.selected) g.FillRect(rowRc, pal.selectFill);
        if (i == st.hovered)  g.FillRect(rowRc, pal.hover);
        if (i == st.selected)
            g.FillRect(Rect(rowRc.left, rowRc.top,
                            rowRc.left + 3, rowRc.bottom), pal.primary);

        const Color c = (i == st.selected || i == st.hovered)
                            ? pal.primaryText : pal.text;
        g.Text(Rect(rowRc.left + pad, y, rowRc.right - pad, y + lay.optionH),
               st.options[static_cast<std::size_t>(i)],
               FontRole::Body, c, kLeftEllip);
    }
    g.PopClip();
}

// ============================================================
// Menu
// ============================================================

int MenuItemHeight(const MenuItemState& item, const Metrics& mx)
{
    return MakeMenuGeom(mx).RowHeight(item);
}

int MenuContentHeight(const MenuState& st, const Metrics& mx)
{
    const MenuGeom gm = MakeMenuGeom(mx);
    int h = 0;
    for (const MenuItemState& item : st.items) h += gm.RowHeight(item);
    return h;
}

int MenuItemAt(const Rect& rc, const MenuState& st,
               const Metrics& mx, Point pt)
{
    if (rc.Empty() || st.items.empty()) return -1;
    if (!rc.Contains(pt)) return -1;
    const MenuGeom gm = MakeMenuGeom(mx);
    int y = rc.top;
    const int n = static_cast<int>(st.items.size());
    for (int i = 0; i < n; ++i) {
        const MenuItemState& item = st.items[static_cast<std::size_t>(i)];
        const int h = gm.RowHeight(item);
        if (pt.y >= y && pt.y < y + h) return item.separator ? -1 : i;
        y += h;
    }
    return -1;
}

void PaintMenu(IPainter& g, const Rect& rc, const MenuState& st,
               const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const MenuGeom gm = MakeMenuGeom(mx);

    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);

    const Rect inner(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1);
    if (inner.Empty()) return;
    g.PushClip(inner);

    const int glyphLeft = rc.left + gm.pad;
    const int glyphRight = glyphLeft + gm.glyphW;
    const int textLeft = glyphRight + gm.gap;
    const int scRight = rc.right - gm.pad;
    const int scLeft = (std::max)(textLeft + 2 * gm.gap, scRight - gm.shortcutW);
    const int textRight = (std::max)(textLeft + 1, scLeft - gm.gap);

    int y = rc.top;
    const int n = static_cast<int>(st.items.size());
    for (int i = 0; i < n; ++i) {
        const MenuItemState& item = st.items[static_cast<std::size_t>(i)];
        const int h = gm.RowHeight(item);
        const Rect rowRc(rc.left + 1, y, rc.right - 1, y + h);
        const bool off = item.disabled || st.base.disabled;

        if (item.separator) {
            const int mid = y + h / 2;
            g.FillRect(Rect(rc.left + gm.pad, mid,
                            rc.right - gm.pad, mid + 1), pal.split);
            y += h;
            if (y >= rc.bottom) break;
            continue;
        }

        if (!off && i == st.selected) {
            g.FillRect(rowRc, pal.selectFill);
            g.FillRect(Rect(rowRc.left, rowRc.top,
                            rowRc.left + 3, rowRc.bottom), pal.primary);
        }
        if (!off && i == st.hovered) g.FillRect(rowRc, pal.hover);

        const Color main = off ? pal.textDisabled : pal.text;
        const Color meta = off ? pal.textDisabled : pal.textFaint;

        if (item.checked) {
            g.Text(Rect(glyphLeft, y, glyphRight, y + h), L"\u2713",
                   FontRole::Body, off ? pal.textDisabled : pal.primary,
                   kCenterText);
        } else if (!item.glyph.empty()) {
            g.Text(Rect(glyphLeft, y, glyphRight, y + h), item.glyph,
                   FontRole::Icon, meta, kCenterText);
        }

        g.Text(Rect(textLeft, y, textRight, y + h), item.text,
               FontRole::Body, main, kLeftEllip);

        if (!item.shortcut.empty() && scLeft < scRight) {
            g.Text(Rect(scLeft, y, scRight, y + h), item.shortcut,
                   FontRole::Meta, meta, kRightEllip);
        }

        y += h;
        if (y >= rc.bottom) break;
    }

    g.PopClip();
}

void PaintContextMenu(IPainter& g, const Rect& client,
                      const ContextMenuState& st,
                      const Palette& pal, const Metrics& mx)
{
    if (!st.visible || client.Empty()) return;
    if (st.menu.items.empty()) return;

    const MenuGeom gm = MakeMenuGeom(mx);
    int textW = 0;
    for (const MenuItemState& item : st.menu.items) {
        if (item.separator) continue;
        textW = (std::max)(textW, g.MeasureText(item.text, FontRole::Body));
    }

    int w = gm.pad * 2 + gm.glyphW + gm.gap + textW + gm.gap + gm.shortcutW;
    w = (std::max)(w, gm.rowH * 6);
    w = (std::min)(w, static_cast<int>(client.Width()));
    const int h = (std::min)(MenuContentHeight(st.menu, mx),
                             static_cast<int>(client.Height()));
    if (w <= 0 || h <= 0) return;

    const Rect anchor(st.anchor.x, st.anchor.y,
                      st.anchor.x + 1, st.anchor.y + 1);
    // PlaceOverlay flips Below to Above when needed and clamps the
    // resulting menu rectangle into client.
    const Rect placed = PlaceOverlay(anchor, Size{w, h}, client,
                                     OverlayPlacement::Below);
    if (placed.Empty()) return;
    PaintMenu(g, placed, st.menu, pal, mx);
}

// ============================================================
// MultiSelect
// ============================================================

int MultiSelectAt(const Rect& rc, const MultiSelectState& st,
                  const Metrics& mx, Point pt)
{
    if (rc.Empty() || st.options.empty()) return -1;
    if (!rc.Contains(pt)) return -1;
    const int rowH = MultiSelectRowHeight(mx);
    if (rowH <= 0) return -1;
    const int visible = MultiSelectVisibleRows(
        rc, static_cast<int>(st.options.size()), mx);
    const int row = (pt.y - (rc.top + 1)) / rowH;
    if (row < 0 || row >= visible) return -1;
    if (!MultiSelectRowRect(rc, row, mx).Contains(pt)) return -1;
    return row;
}

void PaintMultiSelect(IPainter& g, const Rect& rc,
                      const MultiSelectState& st,
                      const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;

    g.FillRound(rc, mx.radiusMd, st.base.disabled ? pal.sunken : pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);

    const Rect inner(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1);
    if (inner.Empty()) return;
    if (st.options.empty()) {
        g.Text(inner, L"暂无数据", FontRole::Meta, pal.textFaint, kCenterText);
        return;
    }

    const int visible = MultiSelectVisibleRows(
        rc, static_cast<int>(st.options.size()), mx);
    g.PushClip(inner);
    for (int i = 0; i < visible; ++i) {
        const Rect rowRc = MultiSelectRowRect(rc, i, mx);
        if (i == st.hovered) g.FillRect(rowRc, pal.hover);

        ControlState cs;
        cs.checked = RowChecked(st.checked, i);
        cs.disabled = st.base.disabled;
        cs.hover = (i == st.hovered);
        PaintCheckbox(g, rowRc, st.options[static_cast<std::size_t>(i)], cs, pal);

        if (i + 1 < visible) {
            g.FillRect(Rect(rowRc.left, rowRc.bottom - 1,
                            rowRc.right, rowRc.bottom), pal.split);
        }
    }
    g.PopClip();
}

// ============================================================
// Transfer
// ============================================================

TransferLayout MeasureTransfer(const Rect& rc, const TransferState& st,
                               const Metrics& mx)
{
    (void)st;
    TransferLayout out;
    if (rc.Empty()) return out;

    const int gap = mx.unit;
    const int btnW = mx.btnH;
    const int midW = btnW + gap * 2;
    const int boxW = (static_cast<int>(rc.Width()) - midW) / 2;
    const int rowH = mx.rowH + mx.unit;
    if (boxW <= 0 || rowH <= 0) return out;
    out.rowH = rowH;

    out.leftBox = Rect(rc.left, rc.top, rc.left + boxW, rc.bottom);
    const int midLeft = rc.left + boxW;
    out.rightBox = Rect(midLeft + midW, rc.top, rc.right, rc.bottom);

    const int btnH = mx.btnH;
    const int totalBtnH = btnH * 2 + gap;
    const int by = rc.top + (static_cast<int>(rc.Height()) - totalBtnH) / 2;
    const int bx = midLeft + gap;
    out.toRight = Rect(bx, by, bx + btnW, by + btnH);
    out.toLeft = Rect(bx, by + btnH + gap, bx + btnW, by + btnH + gap + btnH);
    return out;
}

TransferHit HitTransfer(const Rect& rc, const TransferState& st,
                        const Metrics& mx, Point pt)
{
    TransferHit out;
    const TransferLayout lay = MeasureTransfer(rc, st, mx);
    if (lay.rowH <= 0) return out;

    if (lay.toRight.Contains(pt)) { out.button = 1; return out; }
    if (lay.toLeft.Contains(pt))  { out.button = 2; return out; }

    if (lay.leftBox.Contains(pt)) {
        const int row = (pt.y - lay.leftBox.top) / lay.rowH;
        const int visible = TransferVisibleRows(
            lay.leftBox, static_cast<int>(st.left.size()), lay.rowH);
        if (row >= 0 && row < visible &&
            TransferRowRect(lay.leftBox, row, lay.rowH).Contains(pt)) {
            out.side = 0;
            out.row = row;
        }
        return out;
    }
    if (lay.rightBox.Contains(pt)) {
        const int row = (pt.y - lay.rightBox.top) / lay.rowH;
        const int visible = TransferVisibleRows(
            lay.rightBox, static_cast<int>(st.right.size()), lay.rowH);
        if (row >= 0 && row < visible &&
            TransferRowRect(lay.rightBox, row, lay.rowH).Contains(pt)) {
            out.side = 1;
            out.row = row;
        }
        return out;
    }
    return out;
}

void MoveTransferItem(TransferState& st, int side, int row)
{
    const bool l2r = (side == 0);
    const bool r2l = (side == 1);
    if ((!l2r && !r2l) || row < 0) return;

    std::vector<std::wstring>& src = l2r ? st.left : st.right;
    std::vector<std::wstring>& dst = l2r ? st.right : st.left;
    std::vector<bool>& srcChk      = l2r ? st.leftChecked : st.rightChecked;
    std::vector<bool>& dstChk      = l2r ? st.rightChecked : st.leftChecked;
    if (row >= static_cast<int>(src.size())) return;

    // Keep both checked vectors aligned with their item lists, then
    // move one item together with its checked flag to the other side.
    srcChk.resize(src.size(), false);
    dstChk.resize(dst.size(), false);

    const bool chk = srcChk[static_cast<std::size_t>(row)];
    const std::wstring item = src[static_cast<std::size_t>(row)];
    src.erase(src.begin() + row);
    srcChk.erase(srcChk.begin() + row);
    dst.push_back(item);
    dstChk.push_back(chk);
}

void PaintTransfer(IPainter& g, const Rect& rc, const TransferState& st,
                   const Palette& pal, const Metrics& mx)
{
    const TransferLayout lay = MeasureTransfer(rc, st, mx);
    if (lay.rowH <= 0) return;

    const bool off = st.base.disabled;
    PaintTransferBox(g, lay.leftBox, st.left, st.leftChecked,
                     st.leftHover, off, pal, mx, lay.rowH);
    PaintTransferBox(g, lay.rightBox, st.right, st.rightChecked,
                     st.rightHover, off, pal, mx, lay.rowH);

    bool anyLeft = false;
    for (bool b : st.leftChecked) anyLeft = anyLeft || b;
    bool anyRight = false;
    for (bool b : st.rightChecked) anyRight = anyRight || b;

    PaintTransferButton(g, lay.toRight, L"\u2192", !off && anyLeft, pal, mx);
    PaintTransferButton(g, lay.toLeft,  L"\u2190", !off && anyRight, pal, mx);
}

} // namespace ui
} // namespace air
