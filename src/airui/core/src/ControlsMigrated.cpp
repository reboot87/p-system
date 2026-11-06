// ============================================================
// AirUI - ControlsMigrated.cpp
// Phase 1: composites that used to live behind a native platform
// header in Components.h / Canvas.cpp are now pure IPainter painting.
//
//   Card, EmptyView, StatusTag, KbdChip, Toolbar, Form,
//   DataGrid, ListData, Custom
//
// Every Measure* / *Hit helper is shared by painting so a backend
// only forwards pointer events. ListData uses the framework's
// virtual range helper so long lists only paint visible rows.
// ============================================================
#include "airui/ControlsExtra.h"

#include <algorithm>
#include <cstddef>
#include <cwchar>
#include <cwctype>
#include <string>
#include <utility>
#include <vector>

namespace air {
namespace ui {

namespace {

constexpr TextFormat kLeftText =
    dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kCenterText =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;

struct SemanticColors
{
    Color accent;
    Color soft;
    Color text;
    Color border;
};

SemanticColors ColorsFor(AlertKind kind, const Palette& pal)
{
    switch (kind) {
    case AlertKind::Success: return { pal.ok, pal.okSoft, pal.okText, pal.okBorder };
    case AlertKind::Warning: return { pal.warn, pal.warnSoft, pal.warnText, pal.warnBorder };
    case AlertKind::Error:   return { pal.error, pal.errorSoft, pal.errorText, pal.errorBorder };
    default:                 return { pal.primary, pal.primarySoft, pal.primaryText, pal.hoverBorder };
    }
}

void PaintFrame(IPainter& g, const Rect& rc, const Palette& pal,
                const Metrics& mx, bool hover)
{
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, hover ? pal.hoverBorder : pal.split, 1);
}

std::wstring Fold(const std::wstring& s)
{
    std::wstring out = s;
    for (auto& ch : out) {
        if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
        else ch = static_cast<wchar_t>(std::towlower(static_cast<std::wint_t>(ch)));
    }
    return out;
}

bool IsNumeric(const std::wstring& s)
{
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == L'-' || s[0] == L'+') i = 1;
    if (i >= s.size()) return false;
    for (; i < s.size(); ++i) {
        const wchar_t c = s[i];
        if ((c < L'0' || c > L'9') && c != L'.') return false;
    }
    return true;
}

int CompareCells(const std::wstring& a, const std::wstring& b)
{
    if (IsNumeric(a) && IsNumeric(b)) {
        const double da = std::wcstod(a.c_str(), nullptr);
        const double db = std::wcstod(b.c_str(), nullptr);
        return da < db ? -1 : (da > db ? 1 : 0);
    }
    return a < b ? -1 : (a > b ? 1 : 0);
}

int RowHeight(bool hasSubtitle, const Metrics& mx)
{
    return hasSubtitle ? mx.rowH + mx.unit * 2 : mx.rowH;
}

} // namespace

// ============================================================
// Card
// ============================================================
CardLayout MeasureCard(const Rect& rc, const CardState& st, const Metrics& mx)
{
    CardLayout L;
    if (rc.Empty()) return L;
    const int pad = mx.unit * 2;
    const int iconSize = st.iconSize > 0 ? st.iconSize : mx.unit * 5;
    L.iconRc = Rect(rc.left + pad, rc.top + pad,
                    rc.left + pad + iconSize, rc.top + pad + iconSize);
    L.titleRc = Rect(rc.left + pad, L.iconRc.bottom + mx.unit,
                     rc.right - pad, L.iconRc.bottom + mx.unit + mx.rowH / 2 + 4);
    L.descRc = Rect(rc.left + pad, L.titleRc.bottom + 2,
                    rc.right - pad, L.titleRc.bottom + 2 + mx.rowH / 2);
    L.numberRc = Rect(rc.left + pad, L.descRc.bottom + mx.unit,
                      rc.right - pad, L.descRc.bottom + mx.unit + mx.rowH);
    L.unitRc = Rect(L.numberRc.left + mx.unit * 6, L.numberRc.top,
                    L.numberRc.right, L.numberRc.bottom);
    L.badgeRc = Rect(rc.right - pad - mx.unit * 10, rc.top + pad,
                     rc.right - pad, rc.top + pad + mx.tagH);
    L.linkRc = Rect(rc.left + pad, rc.bottom - pad - mx.rowH / 2,
                    rc.right - pad, rc.bottom - pad);
    return L;
}

CardHit HitCard(const Rect& rc, const CardState& st,
                const Metrics& mx, Point pt)
{
    if (!rc.Contains(pt)) return CardHit::None;
    const CardLayout L = MeasureCard(rc, st, mx);
    if (!st.linkText.empty() && L.linkRc.Contains(pt)) return CardHit::Link;
    return CardHit::Body;
}

void PaintCard(IPainter& g, const Rect& rc, const CardState& st,
               const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const CardLayout L = MeasureCard(rc, st, mx);
    const bool hover = st.base.hover && st.hoverable && !st.base.disabled;
    g.FillRound(rc, mx.radiusCard, hover ? pal.hover : pal.surface);
    g.StrokeRound(rc, mx.radiusCard,
                  hover ? pal.hoverBorder : pal.split, 1);

    const Color iconBg = st.iconBg ? st.iconBg : pal.primarySoft;
    const Color iconFg = st.iconColor ? st.iconColor : pal.primary;
    const int iconRadius = st.iconRadius > 0 ? st.iconRadius : mx.radiusMd;
    g.FillRound(L.iconRc, iconRadius, st.base.disabled ? pal.sunken : iconBg);
    g.Text(L.iconRc, st.glyph, FontRole::IconLarge,
           st.base.disabled ? pal.textDisabled : iconFg, kCenterText);

    if (!st.badge.empty()) {
        const Color bg = st.badgeOk ? pal.okSoft : pal.sunken;
        const Color fg = st.badgeOk ? pal.okText : pal.textSub;
        g.FillRound(L.badgeRc, mx.radiusPill, bg);
        g.Text(L.badgeRc, st.badge, FontRole::Meta, fg, kCenterText);
    }

    const Color text = st.base.disabled ? pal.textDisabled : pal.text;
    const Color sub  = st.base.disabled ? pal.textDisabled : pal.textFaint;
    g.Text(L.titleRc, st.title, FontRole::Title, text, kLeftText);
    g.Text(L.descRc, st.desc, FontRole::Meta, sub, kLeftText);
    if (!st.number.empty()) {
        g.Text(L.numberRc, st.number, FontRole::Display, text, kLeftText);
        if (!st.unit.empty()) g.Text(L.unitRc, st.unit, FontRole::Meta, sub, kLeftText);
    }
    if (!st.linkText.empty()) {
        g.Text(L.linkRc, st.linkText, FontRole::Button,
               st.base.disabled ? pal.textDisabled : pal.primaryText, kLeftText);
    }
}

// ============================================================
// EmptyView / StatusTag / KbdChip
// ============================================================
void PaintEmptyView(IPainter& g, const Rect& rc, const EmptyViewState& st,
                    const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const int cy = rc.top + rc.Height() / 3;
    const int box = mx.unit * 6;
    const Rect icon(rc.left + rc.Width() / 2 - box / 2, cy - box,
                    rc.left + rc.Width() / 2 + box / 2, cy);
    g.FillRound(icon, mx.radiusLg, pal.sunken);
    g.Text(icon, st.glyph, FontRole::IconLarge, pal.textFaint, kCenterText);
    const int lineH = mx.rowH / 2 + 6;
    g.Text(Rect(rc.left, cy + mx.unit, rc.right, cy + mx.unit + lineH),
           st.title, FontRole::Title, pal.text, kCenterText);
    g.Text(Rect(rc.left, cy + mx.unit + lineH, rc.right,
                cy + mx.unit + lineH * 2),
           st.steps, FontRole::Meta, pal.textSub, kCenterText);
    g.Text(Rect(rc.left, cy + mx.unit + lineH * 2, rc.right,
                cy + mx.unit + lineH * 3),
           st.foot, FontRole::Meta, pal.textFaint, kCenterText);
}

void PaintStatusTagColored(IPainter& g, const Rect& rc,
                           const std::wstring& text, bool showDot,
                           const StatusTagColors& colors, const Metrics& mx)
{
    if (rc.Empty()) return;
    g.FillRound(rc, mx.radiusPill, colors.bg);
    g.StrokeRound(rc, mx.radiusPill, colors.border, 1);
    if (showDot) {
        const int d = (std::max)(4, mx.unit / 2);
        const Rect dot(rc.left + mx.unit, rc.top + rc.Height() / 2 - d / 2,
                       rc.left + mx.unit + d, rc.top + rc.Height() / 2 + d / 2);
        g.FillRound(dot, d / 2, colors.dot);
    }
    const int inset = showDot ? mx.unit * 2 : mx.unit;
    g.Text(Rect(rc.left + inset, rc.top, rc.right - mx.unit, rc.bottom),
           text, FontRole::Meta, colors.text, kLeftText);
}

void PaintStatusTag(IPainter& g, const Rect& rc, const StatusTagState& st,
                    const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const SemanticColors c = ColorsFor(st.kind, pal);
    StatusTagColors colors;
    colors.bg = c.soft;
    colors.border = c.border;
    colors.text = c.text;
    colors.dot = c.accent;
    PaintStatusTagColored(g, rc, st.text, st.dot, colors, mx);
}

int MeasureKbdChip(const KbdChipState& st, const Metrics& mx)
{
    const int keyW = static_cast<int>(st.key.size()) * mx.unit + mx.unit * 2;
    const int labelW = static_cast<int>(st.label.size()) * mx.unit + mx.unit * 2;
    return keyW + labelW + mx.unit * 2;
}

void PaintKbdChip(IPainter& g, const Rect& rc, const KbdChipState& st,
                  const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const int keyW = static_cast<int>(st.key.size()) * mx.unit + mx.unit * 2;
    const Rect key(rc.left, rc.top, rc.left + keyW, rc.bottom);
    g.FillRound(key, mx.radiusSm, pal.chipBg);
    g.StrokeRound(key, mx.radiusSm, pal.chipBorder, 1);
    g.Text(key, st.key, FontRole::Meta, pal.textSub, kCenterText);
    g.Text(Rect(key.right + mx.unit, rc.top, rc.right, rc.bottom),
           st.label, FontRole::Meta, pal.textFaint, kLeftText);
}

// ============================================================
// Toolbar
// ============================================================
ToolbarLayout MeasureToolbar(const Rect& rc, const ToolbarState& st,
                             const Metrics& mx)
{
    ToolbarLayout L;
    if (rc.Empty()) return L;
    L.rowH = rc.Height();
    int x = rc.left + mx.unit;
    for (size_t i = 0; i < st.crumbs.size(); ++i) {
        const int w = static_cast<int>(st.crumbs[i].size()) * mx.unit + mx.unit * 2;
        const Rect crumb(x, rc.top, x + w, rc.bottom);
        L.crumbs.push_back(crumb);
        x = crumb.right + mx.unit * 2;
    }
    const int actionH = rc.Height() - mx.unit;
    int rightX = rc.right - mx.unit;
    for (size_t i = 0; i < st.actions.size(); ++i) {
        const int w = static_cast<int>(st.actions[i].label.size()) * mx.unit + mx.unit * 3;
        const Rect a(rightX - w, rc.top + mx.unit / 2, rightX,
                     rc.top + mx.unit / 2 + actionH);
        L.actions.push_back(a);
        rightX = a.left - mx.unit;
    }
    const int searchLeft = x + mx.unit;
    if (rightX - mx.unit > searchLeft)
        L.search = Rect(searchLeft, rc.top + mx.unit / 2,
                        rightX - mx.unit, rc.bottom - mx.unit / 2);
    (void)actionH;
    return L;
}

ToolbarHit HitToolbar(const Rect& rc, const ToolbarState& st,
                      const Metrics& mx, Point pt)
{
    ToolbarHit out;
    if (!rc.Contains(pt)) return out;
    const ToolbarLayout L = MeasureToolbar(rc, st, mx);
    for (size_t i = 0; i < L.actions.size(); ++i) {
        if (L.actions[i].Contains(pt)) { out.action = static_cast<int>(i); return out; }
    }
    for (size_t i = 0; i < L.crumbs.size(); ++i) {
        if (L.crumbs[i].Contains(pt)) { out.crumb = static_cast<int>(i); return out; }
    }
    if (L.search.Contains(pt)) out.search = true;
    return out;
}

void PaintToolbar(IPainter& g, const Rect& rc, const ToolbarState& st,
                  const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const ToolbarLayout L = MeasureToolbar(rc, st, mx);
    g.FillRound(rc, 0, pal.surface);
    g.FillRect(Rect(rc.left, rc.bottom - 1, rc.right, rc.bottom), pal.split);

    for (size_t i = 0; i < L.crumbs.size(); ++i) {
        if (st.hoveredCrumb == static_cast<int>(i))
            g.FillRound(L.crumbs[i], mx.radiusSm, pal.hover);
        const bool last = (i + 1 == L.crumbs.size());
        g.Text(L.crumbs[i], st.crumbs[i], FontRole::Body,
               last ? pal.text : pal.textSub, kCenterText);
        if (!last) {
            const Rect sep(L.crumbs[i].right, rc.top,
                           L.crumbs[i].right + mx.unit * 2, rc.bottom);
            g.Text(sep, L"\u203A", FontRole::Meta, pal.textFaint, kCenterText);
        }
    }

    if (!L.search.Empty()) {
        g.FillRound(L.search, mx.radiusMd, pal.sunken);
        g.StrokeRound(L.search, mx.radiusMd, pal.border, 1);
        const std::wstring shown = st.searchText.empty()
            ? st.searchPlaceholder : st.searchText;
        g.Text(Rect(L.search.left + mx.unit, L.search.top,
                    L.search.right - mx.unit, L.search.bottom),
               shown, FontRole::Body,
               st.searchText.empty() ? pal.textFaint : pal.text, kLeftText);
    }

    for (size_t i = 0; i < L.actions.size(); ++i) {
        const ToolbarActionState& a = st.actions[i];
        ControlState cs;
        cs.hover = (st.hoveredAction == static_cast<int>(i));
        cs.disabled = a.disabled;
        const ButtonStyle style = a.primary ? ButtonStyle::Primary
                                            : ButtonStyle::Secondary;
        PaintButton(g, L.actions[i], a.label, style, cs, pal, mx);
    }
}

// ============================================================
// Form
// ============================================================
FormLayout MeasureForm(const Rect& rc, const FormState& st, const Metrics& mx)
{
    FormLayout L;
    if (rc.Empty()) return L;
    L.rowH = mx.inputH + mx.unit * 2;
    const int gap = mx.unit * 2;
    int y = rc.top;
    for (size_t i = 0; i < st.fields.size(); ++i) {
        const int inputLeft = rc.left + st.labelWidth + gap;
        L.labels.push_back(Rect(rc.left, y + mx.unit, inputLeft - mx.unit, y + mx.inputH + mx.unit));
        L.inputs.push_back(Rect(inputLeft, y, rc.right, y + mx.inputH));
        L.errors.push_back(Rect(inputLeft, y + mx.inputH, rc.right,
                                y + mx.inputH + mx.unit * 2));
        y += L.rowH;
    }
    L.submit = Rect(rc.right - mx.unit * 14, y, rc.right, y + mx.btnH);
    return L;
}

FormHit HitForm(const Rect& rc, const FormState& st,
                const Metrics& mx, Point pt)
{
    FormHit out;
    if (!rc.Contains(pt)) return out;
    const FormLayout L = MeasureForm(rc, st, mx);
    if (L.submit.Contains(pt)) { out.submit = true; return out; }
    for (size_t i = 0; i < L.inputs.size(); ++i) {
        if (L.inputs[i].Contains(pt)) { out.field = static_cast<int>(i); return out; }
    }
    return out;
}

int ValidateForm(FormState& st)
{
    for (size_t i = 0; i < st.fields.size(); ++i) {
        FormField& f = st.fields[i];
        f.invalid = (f.required && f.value.empty());
        if (f.invalid) return static_cast<int>(i);
    }
    return -1;
}

void PaintForm(IPainter& g, const Rect& rc, const FormState& st,
               const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const FormLayout L = MeasureForm(rc, st, mx);
    for (size_t i = 0; i < st.fields.size(); ++i) {
        const FormField& f = st.fields[i];
        std::wstring label = f.label;
        if (f.required) label = L"* " + label;
        g.Text(L.labels[i], label, FontRole::Body, pal.text, kLeftText);

        const bool focused = (st.focusedField == static_cast<int>(i));
        g.FillRound(L.inputs[i], mx.radiusMd, pal.surface);
        g.StrokeRound(L.inputs[i], mx.radiusMd,
                      f.invalid ? pal.error : (focused ? pal.primary : pal.border), 1);
        const std::wstring shown = f.value.empty() ? f.placeholder : f.value;
        g.Text(Rect(L.inputs[i].left + mx.unit, L.inputs[i].top,
                    L.inputs[i].right - mx.unit, L.inputs[i].bottom),
               shown, FontRole::Body,
               f.value.empty() ? pal.textFaint : pal.text, kLeftText);
        if (f.invalid) {
            g.Text(L.errors[i], f.error.empty() ? L"此项为必填" : f.error,
                   FontRole::Meta, pal.errorText, kLeftText);
        }
    }
    ControlState cs;
    cs.hover = st.hoverSubmit;
    PaintButton(g, L.submit, st.submitText, ButtonStyle::Primary, cs, pal, mx);
}

// ============================================================
// DataGrid
// ============================================================
DataGridLayout MeasureDataGrid(const Rect& rc, const DataGridState& st,
                               const Metrics& mx)
{
    DataGridLayout L;
    if (rc.Empty() || st.columns.empty()) return L;
    const int pad = mx.unit;
    L.headerH = mx.rowH + pad;
    L.headerTop = rc.top;
    L.rowTop = rc.top + L.headerH + pad;
    L.rowH = mx.rowH;
    const int available = rc.bottom - L.rowTop;
    L.visibleRows = available > 0 ? available / (L.rowH + 2) : 0;

    int totalWeight = 0;
    for (const auto& c : st.columns) totalWeight += (std::max)(1, c.weight);
    if (totalWeight < 1) totalWeight = 1;
    const int innerW = rc.Width() - pad * 2;
    int x = rc.left + pad;
    for (const auto& c : st.columns) {
        const int w = static_cast<int>(static_cast<long long>(innerW) *
                                      (std::max)(1, c.weight) / totalWeight);
        L.columns.push_back(Rect(x, rc.top, x + w, rc.bottom));
        x += w;
    }
    for (size_t i = 0; i + 1 < L.columns.size(); ++i) {
        const int edge = L.columns[i].right;
        L.handles.push_back(Rect(edge - mx.unit / 2, rc.top,
                                edge + mx.unit / 2, rc.bottom));
    }
    return L;
}

DataGridHit HitDataGrid(const Rect& rc, const DataGridState& st,
                        const Metrics& mx, Point pt)
{
    DataGridHit out;
    if (!rc.Contains(pt)) return out;
    const DataGridLayout L = MeasureDataGrid(rc, st, mx);
    for (size_t i = 0; i < L.handles.size(); ++i) {
        if (L.handles[i].Contains(pt)) { out.resize = static_cast<int>(i); return out; }
    }
    if (pt.y >= L.headerTop && pt.y < L.headerTop + L.headerH) {
        out.header = true;
        for (size_t i = 0; i < L.columns.size(); ++i) {
            if (pt.x >= L.columns[i].left && pt.x < L.columns[i].right) {
                out.column = static_cast<int>(i);
                break;
            }
        }
        return out;
    }
    if (pt.y >= L.rowTop && L.rowH > 0) {
        out.row = (pt.y - L.rowTop) / (L.rowH + 2);
        for (size_t i = 0; i < L.columns.size(); ++i) {
            if (pt.x >= L.columns[i].left && pt.x < L.columns[i].right) {
                out.column = static_cast<int>(i);
                break;
            }
        }
    }
    return out;
}

std::vector<std::vector<std::wstring>> DataGridVisibleRows(const DataGridState& st)
{
    std::vector<std::vector<std::wstring>> rows;
    const std::wstring needle = Fold(st.filter);
    for (const auto& row : st.rows) {
        if (needle.empty()) { rows.push_back(row); continue; }
        bool match = false;
        for (const auto& cell : row) {
            if (Fold(cell).find(needle) != std::wstring::npos) { match = true; break; }
        }
        if (match) rows.push_back(row);
    }
    if (st.sortColumn >= 0 && st.sortColumn < static_cast<int>(st.columns.size())) {
        const size_t col = static_cast<size_t>(st.sortColumn);
        std::stable_sort(rows.begin(), rows.end(),
            [&](const std::vector<std::wstring>& a,
                const std::vector<std::wstring>& b) {
                const std::wstring av = col < a.size() ? a[col] : L"";
                const std::wstring bv = col < b.size() ? b[col] : L"";
                const int cmp = CompareCells(av, bv);
                return st.sortAscending ? cmp < 0 : cmp > 0;
            });
    }
    return rows;
}

void PaintDataGrid(IPainter& g, const Rect& rc, const DataGridState& st,
                   const Palette& pal, const Metrics& mx)
{
    if (rc.Empty() || st.columns.empty()) return;
    const DataGridLayout L = MeasureDataGrid(rc, st, mx);
    PaintFrame(g, rc, pal, mx, false);
    const Rect header(rc.left, L.headerTop, rc.right, L.headerTop + L.headerH);
    g.FillRound(header, 0, pal.sunken);
    for (size_t i = 0; i < L.columns.size(); ++i) {
        std::wstring text = st.columns[i].header;
        if (static_cast<int>(i) == st.sortColumn)
            text += st.sortAscending ? L" \u25B2" : L" \u25BC";
        g.Text(Rect(L.columns[i].left + mx.unit, header.top,
                    L.columns[i].right - mx.unit, header.bottom),
               text, FontRole::Body, pal.textSub, kLeftText);
    }

    const auto rows = DataGridVisibleRows(st);
    g.PushClip(Rect(rc.left + 1, L.rowTop, rc.right - 1, rc.bottom - 1));
    for (int r = 0; r < (std::min)(L.visibleRows, static_cast<int>(rows.size())); ++r) {
        const int y = L.rowTop + r * (L.rowH + 2);
        const Rect rowRc(rc.left + 1, y, rc.right - 1, y + L.rowH);
        if (r == st.selected) {
            g.FillRect(rowRc, pal.selectFill);
            g.FillRect(Rect(rowRc.left, y, rowRc.left + 3, y + L.rowH), pal.primary);
        } else if (r == st.hovered) {
            g.FillRect(rowRc, pal.hover);
        } else if (st.striped && r % 2 == 1) {
            g.FillRound(rowRc, mx.radiusSm, pal.sunken);
        }
        for (size_t c = 0; c < L.columns.size(); ++c) {
            const std::wstring value = c < rows[static_cast<size_t>(r)].size()
                ? rows[static_cast<size_t>(r)][c] : L"";
            g.Text(Rect(L.columns[c].left + mx.unit, y,
                        L.columns[c].right - mx.unit, y + L.rowH),
                   value, FontRole::Body,
                   r == st.selected ? pal.textInverse : pal.text, kLeftText);
        }
    }
    if (rows.empty()) {
        g.Text(Rect(rc.left, L.rowTop, rc.right, rc.bottom),
               L"暂无数据", FontRole::Meta, pal.textFaint, kCenterText);
    }
    g.PopClip();

    for (size_t i = 0; i < L.handles.size(); ++i) {
        const Rect& h = L.handles[i];
        if (st.resizingColumn == static_cast<int>(i))
            g.FillRect(Rect(h.left + 1, rc.top, h.right - 1, rc.bottom), pal.primarySoft);
    }
}

// ============================================================
// ListData - variable height with virtual scrolling
// ============================================================
int ListDataRowHeight(const ListDataRow& row, const Metrics& mx)
{
    if (row.height > 0) return row.height;
    return RowHeight(!row.subtitle.empty(), mx);
}

std::vector<int> ListDataHeights(const ListDataState& st, const Metrics& mx)
{
    std::vector<int> out;
    out.reserve(st.rows.size());
    for (const auto& row : st.rows) out.push_back(ListDataRowHeight(row, mx));
    return out;
}

int ListDataContentHeight(const ListDataState& st, const Metrics& mx)
{
    int total = 0;
    for (const auto& row : st.rows) total += ListDataRowHeight(row, mx);
    return total;
}

int ListDataRowAt(const Rect& rc, const ListDataState& st,
                  const Metrics& mx, Point pt)
{
    if (!rc.Contains(pt)) return -1;
    const std::vector<int> heights = ListDataHeights(st, mx);
    ScrollState scroll = st.scroll;
    scroll.contentH = ListDataContentHeight(st, mx);
    scroll.viewH = rc.Height();
    scroll.offset = ClampScrollOffset(scroll.offset, scroll);
    const VirtualRange vr = VirtualRangeVariable(scroll, heights, 0);
    int y = rc.top + vr.topPixel;
    for (int i = vr.first; i < vr.last; ++i) {
        const int h = heights[static_cast<size_t>(i)];
        if (pt.y >= y && pt.y < y + h) return i;
        y += h;
    }
    return -1;
}

void PaintListData(IPainter& g, const Rect& rc, const ListDataState& st,
                   const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    PaintFrame(g, rc, pal, mx, false);
    if (st.rows.empty()) {
        g.Text(rc, st.emptyText, FontRole::Meta, pal.textFaint, kCenterText);
        return;
    }

    const std::vector<int> heights = ListDataHeights(st, mx);
    ScrollState scroll = st.scroll;
    scroll.contentH = ListDataContentHeight(st, mx);
    scroll.viewH = rc.Height();
    scroll.offset = ClampScrollOffset(scroll.offset, scroll);
    const VirtualRange vr = VirtualRangeVariable(scroll, heights, 2);

    g.PushClip(Rect(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1));
    int y = rc.top + vr.topPixel;
    for (int i = vr.first; i < vr.last; ++i) {
        const ListDataRow& row = st.rows[static_cast<size_t>(i)];
        const int h = heights[static_cast<size_t>(i)];
        const Rect rowRc(rc.left + 1, y, rc.right - 1, y + h);
        if (i == st.selected) {
            g.FillRect(rowRc, pal.selectFill);
            g.FillRect(Rect(rowRc.left, y, rowRc.left + 3, y + h), pal.primary);
        } else if (i == st.hovered) {
            g.FillRect(rowRc, pal.hover);
        }
        const int pad = mx.unit;
        const int badgeW = row.badge.empty() ? 0 : mx.unit * 8;
        const Rect textRc(rc.left + pad, y, rc.right - pad - badgeW,
                          y + (row.subtitle.empty() ? h : h / 2));
        g.Text(textRc, row.title, FontRole::Body,
               row.disabled ? pal.textDisabled : pal.text, kLeftText);
        if (!row.subtitle.empty()) {
            g.Text(Rect(rc.left + pad, y + h / 2, rc.right - pad - badgeW, y + h),
                   row.subtitle, FontRole::Meta, pal.textFaint, kLeftText);
        }
        if (badgeW > 0) {
            const Rect badge(rc.right - badgeW - pad, y + h / 2 - mx.tagH / 2,
                             rc.right - pad, y + h / 2 + mx.tagH / 2);
            g.FillRound(badge, mx.radiusPill, pal.primarySoft);
            g.Text(badge, row.badge, FontRole::Meta, pal.primaryText, kCenterText);
        }
        g.FillRect(Rect(rowRc.left, y + h - 1, rowRc.right, y + h), pal.split);
        y += h;
    }
    g.PopClip();
    PaintScrollbar(g, rc, scroll, pal, mx);
}

// ============================================================
// Custom (owner-draw placeholder)
// ============================================================
void PaintCustom(IPainter& g, const Rect& rc, const CustomState& st,
                 const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    g.FillRound(rc, mx.radiusMd, st.base.disabled ? pal.sunken : pal.surface);
    g.StrokeDashed(rc, pal.hoverBorder, 1, 4);
    const int step = mx.unit * 3;
    g.PushClip(rc);
    if (st.variant == 0) {
        for (int x = rc.left; x < rc.right; x += step)
            g.FillRect(Rect(x, rc.top, x + 1, rc.bottom), pal.split);
        for (int y = rc.top; y < rc.bottom; y += step)
            g.FillRect(Rect(rc.left, y, rc.right, y + 1), pal.split);
    } else if (st.variant == 1) {
        for (int x = rc.left - rc.Height(); x < rc.right; x += step * 2)
            g.FillRect(Rect(x, rc.top, x + 2, rc.bottom), pal.primarySoft);
    } else {
        for (int x = rc.left; x < rc.right; x += step) {
            const int phase = (x - rc.left) / step;
            const int h = mx.unit + (phase % 4) * mx.unit;
            g.FillRound(Rect(x, rc.bottom - h, x + step / 2, rc.bottom),
                        mx.radiusSm, pal.primarySoft);
        }
    }
    g.PopClip();
    g.Text(rc, st.label, FontRole::Meta,
           st.base.disabled ? pal.textDisabled : pal.textSub, kCenterText);
}

} // namespace ui
} // namespace air
