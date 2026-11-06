// AirUI - ControlsEnterpriseEdit.cpp : platform-free workbench editors.
// Fourth wave: QueryBuilder, PropertyGrid, Schedule, FileDropZone.
#include "airui/ControlsEnterprise.h"

#include <algorithm>
#include <cmath>
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
constexpr TextFormat kPlain =
    dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix;

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

// ============================================================
// QueryBuilder
// ============================================================
QueryBuilderLayout GeoQuery(const Rect& rc, const QueryBuilderState& st,
                            const Metrics& mx)
{
    QueryBuilderLayout o;
    if (rc.Empty()) return o;
    const int pad = mx.unit;
    const Rect inner = Inset(rc, pad);
    if (inner.Empty() || inner.Height() < mx.groupH + mx.inputH) return o;
    const int headerH = (std::min)(mx.groupH, inner.Height() / 3);
    const int rowH = mx.inputH;
    const int gap = (std::max)(2, mx.unit / 2);
    const int addH = mx.btnH;
    o.rowH = rowH;
    o.header = Rect(inner.left, inner.top, inner.right, inner.top + headerH);
    int y = o.header.bottom + gap;
    const int limit = inner.bottom - addH - gap;
    const int total = static_cast<int>(st.conditions.size());
    const int logicW = mx.unit * 7;
    const int opW = mx.unit * 9;
    const int removeW = mx.rowH;
    const int fixedW = logicW + opW + removeW + gap * 4;
    if (inner.Width() <= fixedW + mx.unit * 4) return o;
    for (int i = 0; i < total && y + rowH <= limit; ++i) {
        QueryRowLayout r;
        r.row = Rect(inner.left, y, inner.right, y + rowH);
        int x = inner.left;
        r.logic = Rect(x, y, x + logicW, y + rowH);
        x += logicW + gap;
        const int pairW = inner.right - x - opW - removeW - gap * 3;
        if (pairW < mx.unit * 4) break;
        const int fieldW = (pairW - gap) / 2;
        const int valueW = pairW - gap - fieldW;
        r.field = Rect(x, y, x + fieldW, y + rowH);
        x += fieldW + gap;
        r.op = Rect(x, y, x + opW, y + rowH);
        x += opW + gap;
        r.value = Rect(x, y, x + valueW, y + rowH);
        x += valueW + gap;
        r.remove = Rect(x, y, x + removeW, y + rowH);
        o.rows.push_back(r);
        y += rowH + gap;
    }
    if (y + addH <= inner.bottom) {
        const int w = (std::min)(inner.Width(), mx.unit * 18);
        o.add = Rect(inner.left, y, inner.left + w, y + addH);
    }
    return o;
}

// ============================================================
// PropertyGrid
// ============================================================
PropertyGridLayout GeoProperty(const Rect& rc, const PropertyGridState& st,
                               const Metrics& mx)
{
    PropertyGridLayout o;
    if (rc.Empty()) return o;
    const int pad = mx.unit / 2;
    const Rect inner = Inset(rc, pad);
    if (inner.Empty()) return o;
    o.rowH = mx.rowH;
    o.groupH = mx.groupH;
    o.nameW = (std::min)(inner.Width() * 2 / 5, mx.unit * 18);
    if (o.nameW < mx.unit * 4 || inner.Width() <= o.nameW + mx.unit * 4) return o;
    const int total = static_cast<int>(st.items.size());
    int y = inner.top;
    std::wstring lastGroup;
    bool haveGroup = false;
    for (int i = 0; i < total; ++i) {
        const PropertyItem& item = st.items[static_cast<std::size_t>(i)];
        const bool newGroup = st.showGroupHeaders &&
                              (!haveGroup || item.group != lastGroup);
        if (newGroup) {
            if (y + o.groupH > inner.bottom) break;
            PropertyRowLayout h;
            h.groupHeader = true;
            h.item = -1;
            h.row = Rect(inner.left, y, inner.right, y + o.groupH);
            o.rows.push_back(h);
            y += o.groupH;
            lastGroup = item.group;
            haveGroup = true;
        }
        if (y + o.rowH > inner.bottom) break;
        PropertyRowLayout r;
        r.item = i;
        r.row = Rect(inner.left, y, inner.right, y + o.rowH);
        r.name = Rect(inner.left, y, inner.left + o.nameW, y + o.rowH);
        r.value = Rect(r.name.right, y, inner.right, y + o.rowH);
        o.rows.push_back(r);
        y += o.rowH;
        if (!st.showGroupHeaders) {
            lastGroup = item.group;
            haveGroup = true;
        }
    }
    return o;
}

// ============================================================
// Schedule
// ============================================================
ScheduleLayout GeoSchedule(const Rect& rc, const ScheduleState& st,
                           const Metrics& mx)
{
    ScheduleLayout o;
    if (rc.Empty() || st.days.empty()) return o;
    const int pad = mx.unit / 2;
    const Rect inner = Inset(rc, pad);
    const int headerH = mx.groupH;
    const int days = static_cast<int>(st.days.size());
    const int hours = ClampI(st.endHour - st.startHour, 1, 24);
    o.timeW = mx.unit * 6;
    if (inner.Empty() || inner.Height() <= headerH + hours) return o;
    o.hourH = (inner.Height() - headerH) / hours;
    if (o.hourH <= 0) return o;
    const int gridH = o.hourH * hours;
    o.colW = (inner.Width() - o.timeW) / days;
    if (o.colW < mx.unit * 2) return o;
    const int gridW = o.colW * days;
    o.timeCol = Rect(inner.left, inner.top + headerH,
                     inner.left + o.timeW, inner.top + headerH + gridH);
    o.header = Rect(o.timeCol.right, inner.top,
                    o.timeCol.right + gridW, inner.top + headerH);
    o.grid = Rect(o.header.left, o.header.bottom,
                  o.header.right, o.header.bottom + gridH);

    const int n = static_cast<int>(st.events.size());
    o.bars.assign(static_cast<std::size_t>(n), Rect{});
    for (int i = 0; i < n; ++i) {
        const ScheduleEvent& ev = st.events[static_cast<std::size_t>(i)];
        if (ev.day < 0 || ev.day >= days) continue;
        const int s = ClampI(ev.startHour, st.startHour, st.endHour);
        const int e = ClampI(ev.endHour, st.startHour, st.endHour);
        if (e <= s) continue;
        // Split overlapping events of the same day into side-by-side slots.
        std::vector<int> peers;
        for (int j = 0; j < n; ++j) {
            const ScheduleEvent& other = st.events[static_cast<std::size_t>(j)];
            if (other.day < 0 || other.day >= days || other.day != ev.day) continue;
            const int os = ClampI(other.startHour, st.startHour, st.endHour);
            const int oe = ClampI(other.endHour, st.startHour, st.endHour);
            if (oe <= os) continue;
            if (os < e && s < oe) peers.push_back(j);
        }
        int slot = 0;
        for (std::size_t k = 0; k < peers.size(); ++k) {
            if (peers[k] == i) { slot = static_cast<int>(k); break; }
        }
        const int slots = (std::max)(1, static_cast<int>(peers.size()));
        const int slotW = o.colW / slots;
        if (slotW <= 2) continue;
        const int inset = (std::max)(1, mx.unit / 4);
        const int x = o.grid.left + ev.day * o.colW + slot * slotW;
        const int top = o.grid.top + (s - st.startHour) * o.hourH;
        const int bottom = o.grid.top + (e - st.startHour) * o.hourH;
        Rect bar(x + inset, top + inset, x + slotW - inset,
                 bottom - inset);
        if (bar.Empty()) {
            bar = Rect(x + 1, top, x + slotW - 1, bottom);
        }
        bar.left = ClampI(bar.left, o.grid.left, o.grid.right);
        bar.right = ClampI(bar.right, o.grid.left, o.grid.right);
        bar.top = ClampI(bar.top, o.grid.top, o.grid.bottom);
        bar.bottom = ClampI(bar.bottom, o.grid.top, o.grid.bottom);
        if (!bar.Empty()) o.bars[static_cast<std::size_t>(i)] = bar;
    }
    return o;
}

// ============================================================
// FileDropZone
// ============================================================
FileDropZoneLayout GeoDrop(const Rect& rc, const FileDropZoneState& st,
                           const Metrics& mx)
{
    FileDropZoneLayout o;
    if (rc.Empty()) return o;
    const int pad = mx.unit;
    const Rect inner = Inset(rc, pad);
    if (inner.Empty()) return o;
    o.rowH = mx.rowH;
    const int dropH = (std::min)(mx.itemH, (std::max)(mx.rowH, inner.Height() / 2));
    if (dropH > inner.Height()) return o;
    o.drop = Rect(inner.left, inner.top, inner.right, inner.top + dropH);
    const int gap = (std::max)(1, mx.unit / 2);
    int y = o.drop.bottom + gap;
    const int removeW = mx.rowH;
    const int total = static_cast<int>(st.files.size());
    for (int i = 0; i < total && y + o.rowH <= inner.bottom; ++i) {
        o.rows.push_back(Rect(inner.left, y, inner.right, y + o.rowH));
        o.removeButtons.push_back(Rect(inner.right - removeW, y,
                                       inner.right, y + o.rowH));
        y += o.rowH + gap;
    }
    return o;
}

} // namespace

// ============================================================
// QueryBuilder - public entry points
// ============================================================
QueryBuilderLayout MeasureQueryBuilder(const Rect& rc,
                                       const QueryBuilderState& st,
                                       const Metrics& mx)
{
    return GeoQuery(rc, st, mx);
}

QueryBuilderHit HitQueryBuilder(const Rect& rc, const QueryBuilderState& st,
                                const Metrics& mx, Point pt)
{
    QueryBuilderHit out;
    if (rc.Empty() || !rc.Contains(pt)) return out;
    const QueryBuilderLayout lay = GeoQuery(rc, st, mx);
    if (!lay.add.Empty() && lay.add.Contains(pt)) {
        out.kind = QueryHitKind::Add;
        return out;
    }
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        const QueryRowLayout& r = lay.rows[i];
        if (!r.row.Contains(pt)) continue;
        out.row = static_cast<int>(i);
        if (Hit(r.remove, pt)) out.kind = QueryHitKind::Remove;
        else if (Hit(r.value, pt)) out.kind = QueryHitKind::Value;
        else if (Hit(r.op, pt)) out.kind = QueryHitKind::Operator;
        else if (Hit(r.field, pt)) out.kind = QueryHitKind::Field;
        else if (i > 0 && Hit(r.logic, pt)) out.kind = QueryHitKind::Logic;
        return out;
    }
    return out;
}

void PaintQueryBuilder(IPainter& g, const Rect& rc,
                       const QueryBuilderState& st,
                       const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const QueryBuilderLayout lay = GeoQuery(rc, st, mx);
    const bool disabled = st.base.disabled;
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    const Rect inner = Inset(rc, mx.unit);
    if (inner.Empty() || lay.header.Empty()) return;
    g.PushClip(inner);
    g.Text(lay.header, st.title, FontRole::Section, pal.text, kPlain);
    const Rect countRc(lay.header.left, lay.header.top,
                       lay.header.right, lay.header.bottom);
    g.Text(countRc, L"共 " + std::to_wstring(st.conditions.size()) + L" 条",
           FontRole::Meta, pal.textFaint, kRight);
    const Color textColor = disabled ? pal.textDisabled : pal.text;
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        const QueryRowLayout& r = lay.rows[i];
        const QueryCondition& c = st.conditions[i];
        const bool rowHover = st.hoveredRow == static_cast<int>(i);
        if (rowHover) g.FillRect(r.row, pal.hover);
        // logic chip / WHERE marker
        const bool where = i == 0;
        g.FillRound(r.logic, mx.radiusSm,
                    where ? pal.sunken : pal.chipBg);
        g.Text(r.logic, where ? L"WHERE" : c.logic,
               FontRole::Meta,
               where ? pal.textFaint : pal.primaryText, kCenter);
        const Rect cells[3] = {r.field, r.op, r.value};
        const std::wstring texts[3] = {c.field, c.op, c.value};
        for (int k = 0; k < 3; ++k) {
            const bool hot = rowHover && st.hoveredCell == k + 1;
            g.FillRound(cells[k], mx.radiusSm, pal.sunken);
            g.StrokeRound(cells[k], mx.radiusSm,
                          hot ? pal.hoverBorder : pal.border, 1);
            const Rect textRc = Inset(cells[k], mx.unit / 2);
            if (!textRc.Empty()) {
                const bool blank = texts[k].empty() && k == 2;
                g.Text(textRc, blank ? L"输入值" : texts[k], FontRole::Body,
                       blank ? pal.textFaint : textColor, kLeft);
            }
        }
        g.FillRound(r.remove, mx.radiusSm,
                    rowHover ? pal.iconTileHover : pal.sunken);
        g.Text(r.remove, L"\x00D7", FontRole::Body, pal.textSub, kCenter);
    }
    if (!lay.add.Empty()) {
        g.FillRound(lay.add, mx.radiusSm,
                    st.addHover ? pal.primarySoft : pal.chipBg);
        g.StrokeRound(lay.add, mx.radiusSm, pal.chipBorder, 1);
        g.Text(lay.add, L"+ 添加条件", FontRole::Button,
               disabled ? pal.textDisabled : pal.primaryText, kCenter);
    }
    g.PopClip();
}

// ============================================================
// PropertyGrid - public entry points
// ============================================================
PropertyGridLayout MeasurePropertyGrid(const Rect& rc,
                                       const PropertyGridState& st,
                                       const Metrics& mx)
{
    return GeoProperty(rc, st, mx);
}

PropertyGridHit HitPropertyGrid(const Rect& rc, const PropertyGridState& st,
                                const Metrics& mx, Point pt)
{
    PropertyGridHit out;
    if (rc.Empty() || !rc.Contains(pt)) return out;
    const PropertyGridLayout lay = GeoProperty(rc, st, mx);
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        const PropertyRowLayout& r = lay.rows[i];
        if (!r.row.Contains(pt)) continue;
        if (r.groupHeader) {
            for (std::size_t j = i + 1; j < lay.rows.size(); ++j) {
                if (!lay.rows[j].groupHeader) {
                    out.group = lay.rows[j].item;
                    break;
                }
            }
        } else {
            out.item = r.item;
            out.onValue = Hit(r.value, pt);
        }
        return out;
    }
    return out;
}

void PaintPropertyGrid(IPainter& g, const Rect& rc,
                       const PropertyGridState& st,
                       const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const PropertyGridLayout lay = GeoProperty(rc, st, mx);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty()) return;
    g.PushClip(inner);
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        const PropertyRowLayout& r = lay.rows[i];
        if (r.groupHeader) {
            g.FillRect(r.row, pal.sunken);
            std::wstring label;
            for (std::size_t j = i + 1; j < lay.rows.size(); ++j) {
                if (lay.rows[j].groupHeader) break;
                if (lay.rows[j].item >= 0) {
                    label = st.items[static_cast<std::size_t>(lay.rows[j].item)].group;
                    break;
                }
            }
            g.Text(Inset(r.row, mx.unit / 2), label, FontRole::Meta,
                   pal.textSub, kPlain);
            continue;
        }
        const PropertyItem& item = st.items[static_cast<std::size_t>((std::max)(0, r.item))];
        const bool selected = r.item == st.selected;
        const bool hovered = r.item == st.hovered;
        if (selected) g.FillRect(r.row, pal.selectFill);
        else if (hovered) g.FillRect(r.row, pal.hover);
        g.FillRect(Rect(r.row.left, r.row.bottom - 1, r.row.right, r.row.bottom),
                   pal.split);
        g.FillRect(Rect(r.name.right, r.row.top, r.name.right + 1, r.row.bottom),
                   pal.split);
        const Color labelColor = item.enabled ? pal.textSub : pal.textDisabled;
        g.Text(Inset(r.name, mx.unit / 2), item.name, FontRole::Body,
               labelColor, kLeft);
        const Rect valueRc = Inset(r.value, mx.unit / 2);
        if (valueRc.Empty()) continue;
        if (item.kind == PropertyKind::Bool) {
            const int box = (std::min)(mx.tagH, valueRc.Height());
            const Rect check(valueRc.left, valueRc.top + (valueRc.Height() - box) / 2,
                             valueRc.left + box,
                             valueRc.top + (valueRc.Height() + box) / 2);
            g.FillRound(check, mx.radiusSm,
                        item.value == L"true" ? pal.primary : pal.sunken);
            g.StrokeRound(check, mx.radiusSm, pal.border, 1);
            g.Text(Rect(check.right + mx.unit / 2, valueRc.top,
                        valueRc.right, valueRc.bottom),
                   item.value == L"true" ? L"已启用" : L"已关闭",
                   FontRole::Meta, labelColor, kLeft);
        } else if (item.kind == PropertyKind::Choice) {
            g.Text(valueRc, item.value, FontRole::Body, labelColor, kLeft);
            const Rect arrow(valueRc.right - mx.unit * 2, valueRc.top,
                             valueRc.right, valueRc.bottom);
            g.Text(arrow, L"\x203A", FontRole::Body, pal.textFaint, kRight);
        } else {
            g.Text(valueRc, item.value, FontRole::Body,
                   item.enabled ? pal.text : pal.textDisabled,
                   item.kind == PropertyKind::Number ? kRight : kLeft);
        }
    }
    g.PopClip();
}

// ============================================================
// Schedule - public entry points
// ============================================================
ScheduleLayout MeasureSchedule(const Rect& rc, const ScheduleState& st,
                               const Metrics& mx)
{
    return GeoSchedule(rc, st, mx);
}

int ScheduleEventAt(const Rect& rc, const ScheduleState& st,
                    const Metrics& mx, Point pt)
{
    if (rc.Empty() || !rc.Contains(pt)) return -1;
    const ScheduleLayout lay = GeoSchedule(rc, st, mx);
    for (std::size_t i = 0; i < lay.bars.size(); ++i) {
        if (Hit(lay.bars[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void PaintSchedule(IPainter& g, const Rect& rc, const ScheduleState& st,
                   const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const ScheduleLayout lay = GeoSchedule(rc, st, mx);
    if (lay.grid.Empty()) return;
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    const Rect inner = Inset(rc, mx.unit / 2);
    g.PushClip(inner);
    const int days = static_cast<int>(st.days.size());
    const int hours = ClampI(st.endHour - st.startHour, 1, 24);
    // today column wash
    if (st.today >= 0 && st.today < days) {
        const int x = lay.grid.left + st.today * lay.colW;
        g.FillRect(Rect(x, lay.grid.top, x + lay.colW, lay.grid.bottom),
                   pal.primarySoft);
    }
    // hour rules and labels
    for (int h = 0; h <= hours; ++h) {
        const int y = lay.grid.top + h * lay.hourH;
        g.FillRect(Rect(lay.grid.left, y, lay.grid.right,
                        (std::min)(y + 1, lay.grid.bottom)), pal.split);
        if (h < hours) {
            const Rect label(lay.timeCol.left, y,
                             lay.timeCol.right,
                             (std::min)(y + lay.hourH, lay.timeCol.bottom));
            g.Text(Inset(label, mx.unit / 4),
                   std::to_wstring(st.startHour + h) + L":00",
                   FontRole::Meta, pal.textFaint, kRight);
        }
    }
    // day columns
    for (int d = 0; d < days; ++d) {
        const int x = lay.grid.left + d * lay.colW;
        if (d > 0) {
            g.FillRect(Rect(x, lay.grid.top, x + 1, lay.grid.bottom), pal.split);
        }
        const Rect cell(x, lay.header.top, x + lay.colW, lay.header.bottom);
        g.Text(Inset(cell, mx.unit / 4), st.days[static_cast<std::size_t>(d)],
               FontRole::Body,
               d == st.today ? pal.primaryText : pal.textSub, kCenter);
    }
    // event blocks
    for (std::size_t i = 0; i < lay.bars.size(); ++i) {
        const Rect& bar = lay.bars[i];
        if (bar.Empty()) continue;
        const ScheduleEvent& ev = st.events[i];
        const KindColors kc = ColorsFor(ev.kind, pal);
        const bool hot = st.hovered == static_cast<int>(i);
        g.FillRound(bar, mx.radiusSm, hot ? kc.accent : kc.soft);
        g.StrokeRound(bar, mx.radiusSm, kc.accent, 1);
        g.FillRect(Rect(bar.left, bar.top, bar.left + (std::max)(2, mx.unit / 4),
                        bar.bottom), kc.accent);
        const Rect textRc(bar.left + mx.unit / 2, bar.top,
                          bar.right - mx.unit / 4, bar.bottom);
        if (!textRc.Empty()) {
            g.Text(textRc, ev.title, FontRole::Meta,
                   hot ? pal.textInverse : kc.text, kLeft);
        }
    }
    g.PopClip();
}

// ============================================================
// FileDropZone - public entry points
// ============================================================
FileDropZoneLayout MeasureFileDropZone(const Rect& rc,
                                       const FileDropZoneState& st,
                                       const Metrics& mx)
{
    return GeoDrop(rc, st, mx);
}

FileDropZoneHit HitFileDropZone(const Rect& rc, const FileDropZoneState& st,
                                const Metrics& mx, Point pt)
{
    FileDropZoneHit out;
    if (rc.Empty() || !rc.Contains(pt)) return out;
    const FileDropZoneLayout lay = GeoDrop(rc, st, mx);
    if (Hit(lay.drop, pt)) { out.drop = true; return out; }
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        if (!lay.rows[i].Contains(pt)) continue;
        out.file = static_cast<int>(i);
        out.remove = i < lay.removeButtons.size() &&
                     Hit(lay.removeButtons[i], pt);
        return out;
    }
    return out;
}

void PaintFileDropZone(IPainter& g, const Rect& rc,
                       const FileDropZoneState& st,
                       const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const FileDropZoneLayout lay = GeoDrop(rc, st, mx);
    if (lay.drop.Empty()) return;
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    const Rect inner = Inset(rc, mx.unit);
    if (inner.Empty()) return;
    g.PushClip(inner);
    if (st.dragOver) g.FillRound(lay.drop, mx.radiusMd, pal.primarySoft);
    g.StrokeDashed(lay.drop, st.dragOver ? pal.primary : pal.border,
                   1, (std::max)(2, mx.unit / 2));
    const int titleH = (std::min)(mx.rowH, lay.drop.Height() / 2);
    const Rect titleRc(lay.drop.left + mx.unit, lay.drop.top,
                       lay.drop.right - mx.unit, lay.drop.top + titleH);
    const Rect hintRc(lay.drop.left + mx.unit, titleRc.bottom,
                      lay.drop.right - mx.unit, lay.drop.bottom);
    g.Text(titleRc, st.title, FontRole::Button,
           st.dragOver ? pal.primaryText : pal.text, kCenter);
    g.Text(hintRc, st.hint, FontRole::Meta, pal.textFaint, kCenter);
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        const Rect& row = lay.rows[i];
        const UploadFile& f = st.files[i];
        if (st.hovered == static_cast<int>(i)) g.FillRect(row, pal.hover);
        const Rect removeRc = i < lay.removeButtons.size()
                                  ? lay.removeButtons[i] : Rect{};
        const int noteW = mx.unit * 8;
        const int right = removeRc.Empty() ? row.right : removeRc.left;
        const Rect noteRc((std::max)(row.left, right - noteW - mx.unit),
                          row.top, right - mx.unit, row.bottom);
        const Rect nameRc(row.left + mx.unit / 2, row.top,
                          (std::max)(row.left + mx.unit / 2,
                                     noteRc.left - mx.unit),
                          row.top + (std::min)(mx.rowH / 2, row.Height()));
        const Rect track(row.left + mx.unit / 2, nameRc.bottom,
                         (std::max)(row.left + mx.unit / 2,
                                    noteRc.left - mx.unit),
                         (std::min)(row.bottom - 1, nameRc.bottom + mx.unit / 2));
        if (!nameRc.Empty()) {
            g.Text(nameRc, f.name, FontRole::Body, pal.text, kLeft);
        }
        const KindColors kc = ColorsFor(f.kind, pal);
        if (!noteRc.Empty()) {
            g.Text(noteRc, f.note, FontRole::Meta, kc.text, kRight);
        }
        if (!track.Empty()) {
            g.FillRect(track, pal.sunken);
            const int pct = ClampI(f.progress, 0, 100);
            const int filled = static_cast<int>(
                static_cast<long long>(track.Width()) * pct / 100);
            if (filled > 0) {
                g.FillRect(Rect(track.left, track.top,
                                track.left + filled, track.bottom), kc.accent);
            }
        }
        if (!removeRc.Empty()) {
            g.Text(removeRc, L"\x00D7", FontRole::Body, pal.textFaint, kCenter);
        }
    }
    g.PopClip();
}

} // namespace ui
} // namespace air
