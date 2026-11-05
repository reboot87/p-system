// ============================================================
// AirUI - ControlsData.cpp
// Platform-free data & navigation controls.
//
//   Table / List / Tree / Breadcrumb / Pagination / Toast
//
// Every control is a pure function of (rect, state, palette,
// metrics) and talks to the backend only through IPainter. The
// Measure* / *Hit helpers are shared by painting and by pointer
// hit testing, so a backend never has to duplicate the geometry.
// ============================================================
#include "airui/ControlsCore.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace air {
namespace ui {

namespace {

constexpr TextFormat kLeftText =
    dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kCenterText =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix;

int Clamp(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

Rect InflateX(const Rect& r, int dx)
{
    return Rect(r.left + dx, r.top, r.right - dx, r.bottom);
}

struct AlertColors
{
    Color accent;
    Color soft;
    Color text;
};

AlertColors ColorsFor(AlertKind kind, const Palette& pal)
{
    switch (kind) {
    case AlertKind::Success: return { pal.ok,     pal.okSoft,     pal.okText };
    case AlertKind::Warning: return { pal.warn,   pal.warnSoft,   pal.warnText };
    case AlertKind::Error:   return { pal.error,  pal.errorSoft,  pal.errorText };
    default:                 return { pal.primary, pal.primarySoft, pal.primaryText };
    }
}

const wchar_t* GlyphFor(AlertKind kind)
{
    switch (kind) {
    case AlertKind::Success: return L"\u2713";
    case AlertKind::Warning: return L"\u26A0";
    case AlertKind::Error:   return L"\u2717";
    default:                 return L"\u2139";
    }
}

// Shared card frame for the data controls.
void PaintFrame(IPainter& g, const Rect& rc, const Palette& pal, const Metrics& mx)
{
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
}

// A 3px accent bar on the left edge of a selected row.
void PaintSelectionBar(IPainter& g, const Rect& rowRc, const Palette& pal)
{
    g.FillRect(Rect(rowRc.left, rowRc.top, rowRc.left + 3, rowRc.bottom), pal.primary);
}

Color BodyText(const ControlState& st, const Palette& pal)
{
    return st.disabled ? pal.textDisabled : pal.text;
}

} // namespace

// ============================================================
// Table
// ============================================================

TableLayout MeasureTable(const Rect& rc, const TableState& st, const Metrics& mx)
{
    TableLayout out;
    const int pad = mx.unit;
    const int n = static_cast<int>(st.columns.size());
    if (n == 0 || rc.Empty()) return out;

    out.titleH    = (st.showTitle && !st.title.empty()) ? mx.rowH : 0;
    out.headerTop = rc.top + out.titleH;
    out.headerH   = mx.rowH;
    out.rowTop    = out.headerTop + out.headerH;
    out.rowH      = mx.rowH;

    const int available = rc.bottom - out.rowTop;
    out.visibleRows = available > 0 ? available / out.rowH : 0;

    int total = rc.Width() - pad * 2;
    if (total < n) total = n;

    const bool weighted = static_cast<int>(st.widths.size()) == n;
    long long weightSum = 0;
    if (weighted) {
        for (int w : st.widths) weightSum += (w > 0 ? w : 1);
    }

    int x = rc.left + pad;
    for (int i = 0; i < n; ++i) {
        int w = 0;
        if (weighted) {
            const int weight = st.widths[i] > 0 ? st.widths[i] : 1;
            w = static_cast<int>(total * static_cast<long long>(weight) / weightSum);
        } else {
            w = total / n;
        }
        // The last column absorbs rounding error, so the band always
        // ends exactly at rc.right - pad (no spill outside the frame).
        if (i == n - 1) w = rc.right - pad - x;
        if (w < 1) w = 1;
        out.columns.push_back(Rect(x, out.headerTop, x + w, rc.bottom));
        x += w;
    }
    return out;
}

int TableRowAt(const Rect& rc, const TableState& st, const Metrics& mx, Point pt)
{
    const TableLayout lay = MeasureTable(rc, st, mx);
    if (lay.columns.empty()) return -2;
    if (pt.x < rc.left || pt.x >= rc.right || pt.y < lay.headerTop || pt.y >= rc.bottom) return -2;
    if (pt.y < lay.rowTop) return -1;

    const int row = (pt.y - lay.rowTop) / lay.rowH;
    if (row < 0 || row >= lay.visibleRows ||
        row >= static_cast<int>(st.rows.size())) return -2;
    return row;
}

int TableColumnAt(const Rect& rc, const TableState& st, const Metrics& mx, Point pt)
{
    const TableLayout lay = MeasureTable(rc, st, mx);
    if (lay.columns.empty()) return -1;
    if (pt.y < rc.top || pt.y >= rc.bottom) return -1;
    for (std::size_t i = 0; i < lay.columns.size(); ++i) {
        if (pt.x >= lay.columns[i].left && pt.x < lay.columns[i].right) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void PaintTable(IPainter& g,
                const Rect& rc,
                const TableState& st,
                const Palette& pal,
                const Metrics& mx)
{
    const TableLayout lay = MeasureTable(rc, st, mx);
    if (lay.columns.empty()) return;

    const int pad = mx.unit;
    const int cellPad = pad / 2;
    const Color text = BodyText(st.base, pal);

    PaintFrame(g, rc, pal, mx);
    g.PushClip(Rect(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1));

    if (lay.titleH > 0) {
        g.Text(Rect(rc.left + pad, rc.top, rc.right - pad, rc.top + lay.titleH),
               st.title, FontRole::Title, text, kLeftText);
        g.FillRect(Rect(rc.left, lay.headerTop - 1, rc.right, lay.headerTop), pal.split);
    }

    // ---- header ----
    g.FillRect(Rect(rc.left + 1, lay.headerTop, rc.right - 1, lay.rowTop), pal.sunken);
    for (std::size_t i = 0; i < lay.columns.size(); ++i) {
        std::wstring label = st.columns[i];
        if (static_cast<int>(i) == st.sortColumn) {
            label += st.sortAscending ? L" \u25B2" : L" \u25BC";
        }
        const Color c = st.base.disabled ? pal.textDisabled
                      : static_cast<int>(i) == st.sortColumn ? pal.primaryText
                      : pal.textSub;
        g.Text(Rect(lay.columns[i].left + cellPad, lay.headerTop,
                    lay.columns[i].right - cellPad, lay.rowTop),
               label, FontRole::Body, c, kLeftText);
    }
    g.FillRect(Rect(rc.left, lay.rowTop - 1, rc.right, lay.rowTop), pal.split);

    // ---- rows ----
    const int count = std::min(static_cast<int>(st.rows.size()), lay.visibleRows);
    for (int r = 0; r < count; ++r) {
        const int y = lay.rowTop + r * lay.rowH;
        const Rect rowRc(rc.left + 1, y, rc.right - 1, y + lay.rowH);

        if (r == st.selectedRow) {
            g.FillRect(rowRc, pal.selectFill);
            PaintSelectionBar(g, rowRc, pal);
        } else if (r == st.hoveredRow) {
            g.FillRect(rowRc, pal.hover);
        } else if (st.striped && (r % 2 == 1)) {
            g.FillRect(rowRc, pal.sunken);
        }

        for (std::size_t c = 0; c < lay.columns.size(); ++c) {
            const std::wstring value =
                (c < st.rows[static_cast<std::size_t>(r)].size())
                    ? st.rows[static_cast<std::size_t>(r)][c] : L"";
            g.Text(Rect(lay.columns[c].left + cellPad, y,
                        lay.columns[c].right - cellPad, y + lay.rowH),
                   value, FontRole::Body, text, kLeftText);
        }
        g.FillRect(Rect(rc.left, y + lay.rowH - 1, rc.right, y + lay.rowH), pal.split);
    }

    if (st.rows.empty()) {
        g.Text(Rect(rc.left, lay.rowTop, rc.right, rc.bottom),
               L"暂无数据", FontRole::Meta, pal.textFaint, kCenterText);
    }

    g.PopClip();
}

// ============================================================
// List
// ============================================================

int ListRowHeight(const ListState& st, const Metrics& mx)
{
    for (const ListItem& item : st.items) {
        if (!item.subtitle.empty()) return mx.itemH;
    }
    return mx.rowH + 12;
}

int ListRowAt(const Rect& rc, const ListState& st, const Metrics& mx, Point pt)
{
    if (rc.Empty() || st.items.empty()) return -1;
    if (!rc.Contains(pt)) return -1;
    const int rowH = ListRowHeight(st, mx);
    if (rowH <= 0) return -1;
    const int row = (pt.y - rc.top) / rowH;
    if (row < 0 || row >= (rc.bottom - rc.top) / rowH ||
        row >= static_cast<int>(st.items.size())) return -1;
    return row;
}

void PaintList(IPainter& g,
               const Rect& rc,
               const ListState& st,
               const Palette& pal,
               const Metrics& mx)
{
    if (rc.Empty()) return;
    const int pad = mx.unit;
    const int rowH = ListRowHeight(st, mx);

    PaintFrame(g, rc, pal, mx);
    g.PushClip(Rect(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1));

    if (st.items.empty()) {
        g.Text(InflateX(rc, pad), st.emptyText, FontRole::Meta,
               pal.textFaint, kCenterText);
        g.PopClip();
        return;
    }

    const int visible = std::min(static_cast<int>(st.items.size()),
                                (rc.bottom - rc.top) / rowH);
    for (int i = 0; i < visible; ++i) {
        const ListItem& item = st.items[static_cast<std::size_t>(i)];
        const int y = rc.top + i * rowH;
        const Rect rowRc(rc.left + 1, y, rc.right - 1, y + rowH);
        const bool highlighted = (i == st.selected) ||
                                 (i == st.hovered && !item.disabled);

        if (i == st.selected) {
            g.FillRect(rowRc, pal.selectFill);
            PaintSelectionBar(g, rowRc, pal);
        } else if (i == st.hovered && !item.disabled) {
            g.FillRect(rowRc, pal.hover);
        }

        int textLeft = rc.left + pad;
        if (!item.glyph.empty()) {
            const int iconSize = mx.rowH;
            const Rect iconRc(textLeft, y + (rowH - iconSize) / 2,
                              textLeft + iconSize, y + (rowH - iconSize) / 2 + iconSize);
            // selectFill == primarySoft in the built-in themes: on a
            // highlighted row the tile must switch to surface + ring,
            // otherwise it disappears into the row background.
            g.FillRound(iconRc, mx.radiusSm,
                        item.disabled ? pal.sunken
                        : highlighted ? pal.surface : pal.primarySoft);
            if (highlighted) {
                g.StrokeRound(iconRc, mx.radiusSm, pal.primary, 1);
            }
            g.Text(iconRc, item.glyph, FontRole::Icon,
                   item.disabled ? pal.textDisabled : pal.primaryText, kCenterText);
            textLeft += iconSize + pad;
        }

        int textRight = rc.right - pad;
        if (!item.badge.empty()) {
            const int measured = g.MeasureText(item.badge, FontRole::Meta);
            const int badgeW = std::max(44, measured + 16);
            const int badgeH = mx.tagH;
            const Rect badgeRc(rc.right - pad - badgeW,
                               y + (rowH - badgeH) / 2,
                               rc.right - pad,
                               y + (rowH - badgeH) / 2 + badgeH);
            g.FillRound(badgeRc, mx.radiusPill,
                        item.disabled ? pal.sunken
                        : highlighted ? pal.surface : pal.primarySoft);
            if (highlighted) {
                g.StrokeRound(badgeRc, mx.radiusPill, pal.primary, 1);
            }
            g.Text(badgeRc, item.badge, FontRole::Meta,
                   item.disabled ? pal.textDisabled : pal.primaryText, kCenterText);
            textRight = badgeRc.left - pad;
        }

        const Color main = item.disabled ? pal.textDisabled : pal.text;
        if (item.subtitle.empty()) {
            g.Text(Rect(textLeft, y, textRight, y + rowH), item.title,
                   FontRole::Body, main, kLeftText);
        } else {
            const int half = rowH / 2;
            g.Text(Rect(textLeft, y, textRight, y + half), item.title,
                   FontRole::Body, main, kLeftText);
            g.Text(Rect(textLeft, y + half, textRight, y + rowH), item.subtitle,
                   FontRole::Meta, item.disabled ? pal.textDisabled : pal.textFaint,
                   kLeftText);
        }

        if (st.dividers && i + 1 < visible) {
            g.FillRect(Rect(rc.left, y + rowH - 1, rc.right, y + rowH), pal.split);
        }
    }

    g.PopClip();
}

// ============================================================
// Tree
// ============================================================

std::vector<int> TreeVisibleNodes(const TreeState& st)
{
    std::vector<int> visible;
    std::vector<bool> ancestor(8, false);  // indexed by depth
    ancestor[0] = true;                    // root level is always visible

    for (std::size_t i = 0; i < st.nodes.size(); ++i) {
        const TreeNode& node = st.nodes[i];
        const int depth = std::max(0, node.depth);
        if (depth + 2 >= static_cast<int>(ancestor.size())) {
            ancestor.resize(static_cast<std::size_t>(depth) + 3, false);
        }
        if (!ancestor[static_cast<std::size_t>(depth)]) continue;

        visible.push_back(static_cast<int>(i));
        ancestor[static_cast<std::size_t>(depth) + 1] =
            node.hasChildren && node.expanded && !node.disabled;
        for (int d = depth + 2; d < static_cast<int>(ancestor.size()); ++d) {
            ancestor[static_cast<std::size_t>(d)] = false;
        }
    }
    return visible;
}

int TreeRowHeight(const Metrics& mx)
{
    return mx.rowH;
}

int TreeRowAt(const Rect& rc, const TreeState& st, const Metrics& mx, Point pt)
{
    if (rc.Empty()) return -1;
    if (!rc.Contains(pt)) return -1;
    const int rowH = TreeRowHeight(mx);
    if (rowH <= 0) return -1;
    const std::vector<int> visible = TreeVisibleNodes(st);
    const int row = (pt.y - rc.top) / rowH;
    if (row < 0 || row >= (rc.bottom - rc.top) / rowH ||
        row >= static_cast<int>(visible.size())) return -1;
    return visible[static_cast<std::size_t>(row)];
}

int TreeToggleAt(const Rect& rc, const TreeState& st, const Metrics& mx, Point pt)
{
    const int nodeIndex = TreeRowAt(rc, st, mx, pt);
    if (nodeIndex < 0) return -1;
    const TreeNode& node = st.nodes[static_cast<std::size_t>(nodeIndex)];
    if (!node.hasChildren || node.disabled) return -1;

    const int arrowLeft = rc.left + mx.unit + std::max(0, node.depth) * 16;
    if (pt.x >= arrowLeft && pt.x < arrowLeft + 16) return nodeIndex;
    return -1;
}

void PaintTree(IPainter& g,
               const Rect& rc,
               const TreeState& st,
               const Palette& pal,
               const Metrics& mx)
{
    if (rc.Empty()) return;
    const int pad = mx.unit;
    const int rowH = TreeRowHeight(mx);
    const std::vector<int> visible = TreeVisibleNodes(st);

    PaintFrame(g, rc, pal, mx);
    g.PushClip(Rect(rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1));

    const int count = std::min(static_cast<int>(visible.size()),
                               (rc.bottom - rc.top) / rowH);
    for (int r = 0; r < count; ++r) {
        const int index = visible[static_cast<std::size_t>(r)];
        const TreeNode& node = st.nodes[static_cast<std::size_t>(index)];
        const int y = rc.top + r * rowH;
        const Rect rowRc(rc.left + 1, y, rc.right - 1, y + rowH);

        if (index == st.selected) {
            g.FillRect(rowRc, pal.selectFill);
            PaintSelectionBar(g, rowRc, pal);
        } else if (index == st.hovered && !node.disabled) {
            g.FillRect(rowRc, pal.hover);
        }

        const int arrowLeft = rc.left + pad + std::max(0, node.depth) * 16;
        if (node.hasChildren) {
            g.Text(Rect(arrowLeft, y, arrowLeft + 16, y + rowH),
                   node.expanded ? L"\u25BE" : L"\u25B8",
                   FontRole::Meta,
                   node.disabled ? pal.textDisabled : pal.textSub,
                   kCenterText);
        }

        const int textLeft = arrowLeft + 18;
        g.Text(Rect(textLeft, y, rc.right - pad, y + rowH), node.text,
               FontRole::Body,
               node.disabled ? pal.textDisabled : pal.text, kLeftText);
    }

    if (visible.empty()) {
        g.Text(InflateX(rc, pad), L"暂无数据", FontRole::Meta,
               pal.textFaint, kCenterText);
    }

    g.PopClip();
}

// ============================================================
// Breadcrumb
// ============================================================

int BreadcrumbHit(const Rect& rc, const BreadcrumbState& st,
                  const Metrics& mx, Point pt)
{
    const int n = static_cast<int>(st.items.size());
    if (n == 0 || rc.Empty() || !rc.Contains(pt)) return -1;
    const int inner = rc.Width() - mx.unit * 2;
    if (inner <= 0) return -1;
    const int cellW = inner / n;
    if (cellW <= 0) return -1;
    const int left = rc.left + mx.unit;
    // Guard the padding explicitly: C++ integer division truncates
    // toward zero, so a negative offset would otherwise map to 0.
    if (pt.x < left || pt.x >= left + cellW * n) return -1;
    const int index = (pt.x - left) / cellW;
    if (index < 0 || index >= n) return -1;
    return index;
}

void PaintBreadcrumb(IPainter& g,
                     const Rect& rc,
                     const BreadcrumbState& st,
                     const Palette& pal,
                     const Metrics& mx)
{
    const int n = static_cast<int>(st.items.size());
    if (n == 0 || rc.Empty()) return;

    const int pad = mx.unit;
    const int inner = rc.Width() - pad * 2;
    if (inner <= 0) return;
    const int cellW = inner / n;
    if (cellW <= 0) return;
    const int active = st.active < 0 ? n - 1 : Clamp(st.active, 0, n - 1);

    for (int i = 0; i < n; ++i) {
        const int x = rc.left + pad + i * cellW;
        const Rect cell(x, rc.top, x + cellW, rc.bottom);

        if (i == st.hovered && !st.base.disabled) {
            g.FillRound(Rect(cell.left + 2, cell.top + 2,
                             cell.right - 2, cell.bottom - 2),
                        mx.radiusSm, pal.hover);
        }
        if (i > 0) {
            g.Text(Rect(cell.left, rc.top, cell.left + 16, rc.bottom),
                   L"\u203A", FontRole::Meta, pal.textFaint, kCenterText);
        }

        Color c = st.base.disabled ? pal.textDisabled
                : i == active ? pal.primaryText
                : i == st.hovered ? pal.primary
                : pal.textSub;
        g.Text(Rect(cell.left + 16, rc.top, cell.right - 4, rc.bottom),
               st.items[static_cast<std::size_t>(i)], FontRole::Body, c, kLeftText);
    }
}

// ============================================================
// Pagination
// ============================================================

std::vector<PaginationSlot> MeasurePagination(const Rect& rc,
                                             const PaginationState& st,
                                             const Metrics& mx)
{
    std::vector<PaginationSlot> slots;
    const int count = st.pageCount;
    if (count < 1 || rc.Empty()) return slots;

    const int page  = Clamp(st.page, 1, count);
    const int btn   = mx.rowH;
    const int gap   = 4;
    constexpr int kMaxPages = 7;

    int first = 1;
    int last  = count;
    if (count > kMaxPages) {
        first = page - 3;
        if (first < 1) first = 1;
        last = first + kMaxPages - 1;
        if (last > count) {
            last = count;
            first = last - kMaxPages + 1;
        }
    }

    const bool headGap = first > 1;
    const bool tailGap = last < count;
    const int slotCount = 2 + (last - first + 1)
                        + (headGap ? 2 : 0) + (tailGap ? 2 : 0);
    const int totalW = slotCount * btn + (slotCount - 1) * gap;

    int x = rc.left + (rc.Width() - totalW) / 2;
    int y = rc.top + (rc.Height() - btn) / 2;
    if (y < rc.top) y = rc.top;

    auto add = [&](int p) {
        slots.push_back(PaginationSlot{ Rect(x, y, x + btn, y + btn), p });
        x += btn + gap;
    };

    add(-2);                                   // previous
    if (headGap) { add(1); add(0); }           // first page + ellipsis
    for (int p = first; p <= last; ++p) add(p);
    if (tailGap) { add(0); add(count); }       // ellipsis + last page
    add(-1);                                   // next
    return slots;
}

int PaginationHit(const Rect& rc, const PaginationState& st,
                  const Metrics& mx, Point pt)
{
    if (st.pageCount < 1) return 0;
    const std::vector<PaginationSlot> slots = MeasurePagination(rc, st, mx);
    const int page = Clamp(st.page, 1, st.pageCount);
    for (const PaginationSlot& slot : slots) {
        if (!slot.rect.Contains(pt)) continue;
        if (slot.page == 0) return 0;                     // ellipsis
        if (slot.page == -2) return page > 1 ? -2 : 0;   // previous
        if (slot.page == -1) return page < st.pageCount ? -1 : 0;
        return slot.page;
    }
    return 0;
}

void PaintPagination(IPainter& g,
                     const Rect& rc,
                     const PaginationState& st,
                     const Palette& pal,
                     const Metrics& mx)
{
    const std::vector<PaginationSlot> slots = MeasurePagination(rc, st, mx);
    const int page = st.pageCount > 0 ? Clamp(st.page, 1, st.pageCount) : 1;

    for (const PaginationSlot& slot : slots) {
        const int code = slot.page;
        const bool current = (code > 0 && code == page);
        const bool disabled = st.base.disabled
                           || (code == -2 && page <= 1)
                           || (code == -1 && page >= st.pageCount);

        if (code == 0) {
            g.Text(slot.rect, L"\u2026", FontRole::Body,
                   pal.textFaint, kCenterText);
            continue;
        }

        if (current) {
            g.FillRound(slot.rect, mx.radiusSm,
                        disabled ? pal.border : pal.primary);
        } else {
            g.FillRound(slot.rect, mx.radiusSm,
                        st.hovered == code && !disabled ? pal.hover : pal.surface);
            g.StrokeRound(slot.rect, mx.radiusSm, pal.border, 1);
        }

        std::wstring label;
        if (code == -2)      label = L"\u2039";
        else if (code == -1) label = L"\u203A";
        else                 label = std::to_wstring(code);

        const Color c = disabled ? pal.textDisabled
                      : current ? pal.textInverse
                      : pal.textSub;
        g.Text(slot.rect, label, FontRole::Body, c, kCenterText);
    }
}

// ============================================================
// Toast
// ============================================================

bool ToastCloseHit(const Rect& rc, const ToastState& st,
                   const Metrics& mx, Point pt)
{
    if (!st.closable || rc.Empty()) return false;
    const int box = mx.rowH - 8;
    const int left = rc.right - mx.unit - box;
    const int top  = rc.top + (rc.Height() - box) / 2;
    return Rect(left, top, left + box, top + box).Contains(pt);
}

void PaintToast(IPainter& g,
                const Rect& rc,
                const ToastState& st,
                const Palette& pal,
                const Metrics& mx)
{
    if (rc.Empty()) return;
    const int pad = mx.unit;
    const AlertColors colors = ColorsFor(st.kind, pal);
    const Color accent = st.base.disabled ? pal.border : colors.accent;

    PaintFrame(g, rc, pal, mx);
    g.FillRect(Rect(rc.left, rc.top + 6, rc.left + 4, rc.bottom - 6), accent);

    const Color iconColor = st.base.disabled ? pal.textDisabled : colors.text;
    g.Text(Rect(rc.left + pad, rc.top, rc.left + pad + 28, rc.bottom),
           GlyphFor(st.kind), FontRole::Icon, iconColor, kCenterText);

    int textRight = rc.right - pad;
    if (st.closable) {
        const int box = mx.rowH - 8;
        const int left = rc.right - pad - box;
        const int top  = rc.top + (rc.Height() - box) / 2;
        const Rect closeRc(left, top, left + box, top + box);
        if (st.hoveredClose) {
            g.FillRound(closeRc, mx.radiusSm, pal.hover);
        }
        g.Text(closeRc, L"\u00D7", FontRole::Body,
               st.base.disabled ? pal.textDisabled : pal.textSub, kCenterText);
        textRight = closeRc.left - pad;
    }

    const int textLeft = rc.left + pad + 32;
    if (st.message.empty()) {
        g.Text(Rect(textLeft, rc.top, textRight, rc.bottom), st.title,
               FontRole::Body, BodyText(st.base, pal), kLeftText);
    } else {
        const int half = rc.Height() / 2;
        g.Text(Rect(textLeft, rc.top, textRight, rc.top + half), st.title,
               FontRole::Body, BodyText(st.base, pal), kLeftText);
        g.Text(Rect(textLeft, rc.top + half, textRight, rc.bottom), st.message,
               FontRole::Meta, st.base.disabled ? pal.textDisabled : pal.textSub,
               kLeftText);
    }
}

} // namespace ui
} // namespace air
