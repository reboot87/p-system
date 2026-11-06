// AirUI - ControlsStudioEditors.cpp : platform-free specialist editors.
// Fifth wave: JsonTree, LogViewer, CronEditor, MentionInput.
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

bool Has(const Rect& r, Point p)
{
    return !r.Empty() && r.Contains(p);
}

Rect Inset(const Rect& r, int d)
{
    return Rect(r.left + d, r.top + d, r.right - d, r.bottom - d);
}

struct LevelColors
{
    Color soft, text, edge;
    const wchar_t* label;
};

LevelColors ColorsForLevel(LogLevel level, const Palette& pal)
{
    switch (level) {
    case LogLevel::Debug:
        return LevelColors{pal.sunken, pal.textFaint, pal.split, L"DEBUG"};
    case LogLevel::Warning:
        return LevelColors{pal.warnSoft, pal.warnText, pal.warnBorder, L"WARN"};
    case LogLevel::Error:
        return LevelColors{pal.errorSoft, pal.errorText, pal.errorBorder, L"ERROR"};
    default:
        return LevelColors{pal.primarySoft, pal.primaryText, pal.primary, L"INFO"};
    }
}

// ============================================================
// JsonTree
// ============================================================
JsonTreeLayout GeoJson(const Rect& rc, const JsonTreeState& st,
                       const Metrics& mx)
{
    JsonTreeLayout o;
    if (rc.Empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.rowH * 2) return o;
    o.header = Rect(inner.left, inner.top, inner.right,
                    inner.top + mx.rowH);
    o.body = Rect(inner.left, o.header.bottom, inner.right, inner.bottom);
    if (o.body.Empty() || st.nodes.empty()) return o;
    o.rowH = (std::min)(mx.rowH,
                        o.body.Height() /
                            (std::max)(1, static_cast<int>(st.nodes.size())));
    if (o.rowH <= 0) o.rowH = mx.rowH;
    const int indent = mx.unit * 2;
    const int boxW = mx.tagH;
    for (std::size_t i = 0; i < st.nodes.size(); ++i) {
        const int y = o.body.top + static_cast<int>(i) * o.rowH;
        if (y + o.rowH > o.body.bottom) break;
        const JsonNode& node = st.nodes[i];
        const int depth = ClampI(node.depth, 0, 8);
        const int x = o.body.left + depth * indent;
        o.rows.push_back(Rect(o.body.left, y, o.body.right, y + o.rowH));
        if (node.expandable && x + boxW < o.body.right) {
            o.toggles.push_back(Rect(x, y + (o.rowH - boxW) / 2,
                                     x + boxW,
                                     y + (o.rowH - boxW) / 2 + boxW));
        } else {
            o.toggles.push_back(Rect{});
        }
    }
    return o;
}

} // namespace

JsonTreeLayout MeasureJsonTree(const Rect& rc, const JsonTreeState& st,
                               const Metrics& mx)
{
    return GeoJson(rc, st, mx);
}

JsonTreeHit HitJsonTree(const Rect& rc, const JsonTreeState& st,
                        const Metrics& mx, Point pt)
{
    JsonTreeHit h;
    const JsonTreeLayout o = GeoJson(rc, st, mx);
    if (o.body.Empty()) return h;
    for (std::size_t i = 0; i < o.toggles.size(); ++i) {
        if (Has(o.toggles[i], pt)) {
            h.node = static_cast<int>(i);
            h.toggle = true;
            return h;
        }
    }
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        if (Has(o.rows[i], pt)) {
            h.node = static_cast<int>(i);
            return h;
        }
    }
    return h;
}

void PaintJsonTree(IPainter& g, const Rect& rc, const JsonTreeState& st,
                   const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const JsonTreeLayout o = GeoJson(rc, st, mx);
    if (o.header.Empty()) return;
    g.Text(o.header, st.root, FontRole::Section, pal.text, kLeft);
    g.Text(o.header, std::to_wstring(st.nodes.size()) + L" 个节点",
           FontRole::Meta, pal.textFaint, kRight);
    if (o.body.Empty()) return;

    g.PushClip(o.body);
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        const Rect& row = o.rows[i];
        if (row.Empty()) continue;
        const int idx = static_cast<int>(i);
        if (idx == st.selected) {
            g.FillRect(row, pal.selectFill);
        } else if (idx == st.hovered) {
            g.FillRect(row, pal.hover);
        }
        const JsonNode& node = st.nodes[i];
        const Rect& toggle = o.toggles[i];
        const int textLeft = toggle.Empty()
                                 ? row.left + ClampI(node.depth, 0, 8) * mx.unit * 2
                                 : toggle.right + mx.unit / 2;
        if (!toggle.Empty()) {
            g.FillRound(toggle, mx.radiusSm, pal.iconTile);
            g.Text(toggle, node.expanded ? L"-" : L"+", FontRole::Meta,
                   pal.primaryText, kCenter);
        }
        const int valueW = (std::min)(mx.unit * 12, row.Width() / 2);
        const Rect keyRc(textLeft, row.top,
                         (std::max)(textLeft, row.right - valueW), row.bottom);
        const Rect valRc(row.right - valueW, row.top, row.right, row.bottom);
        if (!keyRc.Empty()) {
            g.Text(keyRc, node.key, FontRole::Body,
                   node.expandable ? pal.primaryText : pal.text, kLeft);
        }
        if (!valRc.Empty() && !node.value.empty()) {
            g.Text(valRc, node.value, FontRole::Meta, pal.textSub, kRight);
        }
        g.FillRect(Rect(row.left, row.bottom - 1, row.right, row.bottom),
                   pal.split);
    }
    g.PopClip();
}

namespace {

// ============================================================
// LogViewer
// ============================================================
LogViewerLayout GeoLog(const Rect& rc, const LogViewerState& st,
                       const Metrics& mx)
{
    LogViewerLayout o;
    if (rc.Empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.rowH * 2) return o;
    o.header = Rect(inner.left, inner.top, inner.right,
                    inner.top + mx.rowH);
    o.filterRc = Rect(o.header.right - mx.unit * 18, o.header.top,
                      o.header.right, o.header.bottom);
    o.body = Rect(inner.left, o.header.bottom, inner.right, inner.bottom);
    if (o.body.Empty() || st.entries.empty()) return o;
    o.rowH = (std::min)(mx.rowH,
                        o.body.Height() /
                            (std::max)(1, static_cast<int>(st.entries.size())));
    if (o.rowH <= 0) o.rowH = mx.rowH;
    for (std::size_t i = 0; i < st.entries.size(); ++i) {
        const int y = o.body.top + static_cast<int>(i) * o.rowH;
        if (y + o.rowH > o.body.bottom) break;
        o.rows.push_back(Rect(o.body.left, y, o.body.right, y + o.rowH));
    }
    return o;
}

} // namespace

LogViewerLayout MeasureLogViewer(const Rect& rc, const LogViewerState& st,
                                 const Metrics& mx)
{
    return GeoLog(rc, st, mx);
}

int LogLineAt(const Rect& rc, const LogViewerState& st,
              const Metrics& mx, Point pt)
{
    const LogViewerLayout o = GeoLog(rc, st, mx);
    if (o.body.Empty()) return -1;
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        if (Has(o.rows[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void PaintLogViewer(IPainter& g, const Rect& rc, const LogViewerState& st,
                    const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const LogViewerLayout o = GeoLog(rc, st, mx);
    if (o.header.Empty()) return;
    const Rect titleRc(o.header.left, o.header.top,
                       o.filterRc.left, o.header.bottom);
    if (!titleRc.Empty()) {
        g.Text(titleRc, st.title, FontRole::Section, pal.text, kLeft);
    }
    if (!o.filterRc.Empty()) {
        g.FillRound(o.filterRc, mx.radiusSm, pal.sunken);
        g.Text(Inset(o.filterRc, mx.unit / 2),
               st.filter.empty() ? L"全部级别" : L"过滤：" + st.filter,
               FontRole::Meta, pal.textSub, kLeft);
    }
    if (o.body.Empty()) return;

    g.FillRect(o.body, pal.sunken);
    g.PushClip(o.body);
    const int timeW = mx.unit * 9;
    const int levelW = mx.unit * 8;
    const int srcW = mx.unit * 10;
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        const Rect& row = o.rows[i];
        if (row.Empty()) continue;
        const LogEntry& e = st.entries[i];
        const LevelColors lc = ColorsForLevel(e.level, pal);
        const bool matched = !st.filter.empty() &&
                             e.text.find(st.filter) != std::wstring::npos;
        if (static_cast<int>(i) == st.selected) {
            g.FillRect(row, pal.selectFill);
        } else if (matched) {
            g.FillRect(row, pal.noteBg);
        } else if (static_cast<int>(i) == st.hovered) {
            g.FillRect(row, pal.hover);
        }
        int x = row.left + mx.unit / 2;
        const Rect timeRc(x, row.top, (std::min)(x + timeW, row.right),
                          row.bottom);
        if (!timeRc.Empty()) {
            g.Text(timeRc, e.time, FontRole::Meta, pal.textFaint, kLeft);
        }
        x = (std::min)(x + timeW, row.right);
        const Rect levelRc(x, row.top + mx.unit / 4,
                           (std::min)(x + levelW - mx.unit / 2, row.right),
                           (std::min)(row.bottom - mx.unit / 4, row.bottom));
        if (!levelRc.Empty()) {
            g.FillRound(levelRc, mx.radiusSm, lc.soft);
            g.Text(levelRc, lc.label, FontRole::Meta, lc.text, kCenter);
        }
        x = (std::min)(x + levelW, row.right);
        const Rect srcRc(x, row.top, (std::min)(x + srcW, row.right),
                         row.bottom);
        if (!srcRc.Empty()) {
            g.Text(srcRc, e.source, FontRole::Meta, pal.textSub, kLeft);
        }
        x = (std::min)(x + srcW, row.right);
        const Rect textRc(x, row.top, row.right, row.bottom);
        if (!textRc.Empty()) {
            g.Text(textRc, e.text, FontRole::Body, pal.text, kLeft);
        }
    }
    g.PopClip();
}

namespace {

// ============================================================
// CronEditor
// ============================================================
CronEditorLayout GeoCron(const Rect& rc, const CronEditorState& st,
                         const Metrics& mx)
{
    CronEditorLayout o;
    if (rc.Empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.rowH * 3) return o;
    o.header = Rect(inner.left, inner.top, inner.right,
                    inner.top + mx.rowH);
    const int fieldH = mx.inputH + mx.tagH;
    o.expr = Rect(inner.left, o.header.bottom + mx.unit / 2,
                  inner.right, o.header.bottom + mx.unit / 2 + fieldH);
    o.descriptionRc = Rect(inner.left, o.expr.bottom + mx.unit / 2,
                           inner.right,
                           o.expr.bottom + mx.unit / 2 + mx.rowH);
    const int presetH = mx.btnH;
    o.presetsRc = Rect(inner.left, inner.bottom - presetH,
                       inner.right, inner.bottom);
    if (o.expr.Empty() || o.descriptionRc.Empty()) return o;

    const int n = (std::max)(1, static_cast<int>(st.fields.size()));
    const int gap = mx.unit / 2;
    const int slot = (o.expr.Width() - gap * (n - 1)) / n;
    if (slot <= 0) return o;
    for (int i = 0; i < n; ++i) {
        const int x = o.expr.left + i * (slot + gap);
        o.fields.push_back(Rect(x, o.expr.top,
                                (std::min)(x + slot, o.expr.right),
                                o.expr.bottom));
    }
    const int m = static_cast<int>(st.presets.size());
    if (m > 0 && !o.presetsRc.Empty()) {
        const int pslot = (o.presetsRc.Width() - gap * (m - 1)) / m;
        if (pslot > 0) {
            for (int i = 0; i < m; ++i) {
                const int x = o.presetsRc.left + i * (pslot + gap);
                o.presets.push_back(Rect(x, o.presetsRc.top,
                                         (std::min)(x + pslot, o.presetsRc.right),
                                         o.presetsRc.bottom));
            }
        }
    }
    return o;
}

} // namespace

CronEditorLayout MeasureCronEditor(const Rect& rc,
                                   const CronEditorState& st,
                                   const Metrics& mx)
{
    return GeoCron(rc, st, mx);
}

CronEditorHit HitCronEditor(const Rect& rc, const CronEditorState& st,
                            const Metrics& mx, Point pt)
{
    CronEditorHit h;
    const CronEditorLayout o = GeoCron(rc, st, mx);
    for (std::size_t i = 0; i < o.presets.size(); ++i) {
        if (Has(o.presets[i], pt)) {
            h.preset = static_cast<int>(i);
            return h;
        }
    }
    for (std::size_t i = 0; i < o.fields.size(); ++i) {
        if (Has(o.fields[i], pt)) {
            h.field = static_cast<int>(i);
            return h;
        }
    }
    return h;
}

void PaintCronEditor(IPainter& g, const Rect& rc, const CronEditorState& st,
                     const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const CronEditorLayout o = GeoCron(rc, st, mx);
    if (o.header.Empty()) return;
    g.Text(o.header, st.title, FontRole::Section, pal.text, kLeft);
    g.Text(o.header, st.focused ? L"编辑中" : L"已保存", FontRole::Meta,
           st.focused ? pal.primaryText : pal.textFaint, kRight);

    const std::vector<std::wstring> defaultCaptions =
        {L"分", L"时", L"日", L"月", L"周"};
    for (std::size_t i = 0; i < o.fields.size(); ++i) {
        const Rect& f = o.fields[i];
        if (f.Empty()) continue;
        const int idx = static_cast<int>(i);
        g.FillRound(f, mx.radiusMd, pal.surface);
        g.StrokeRound(f, mx.radiusMd,
                      idx == st.hoveredField ? pal.hoverBorder
                                             : (st.focused ? pal.selectRing
                                                           : pal.border), 1);
        const Rect valueRc(f.left, f.top, f.right,
                           f.bottom - mx.tagH);
        const Rect capRc(f.left, f.bottom - mx.tagH, f.right, f.bottom);
        if (!valueRc.Empty()) {
            const std::wstring& v = i < st.fields.size() ? st.fields[i] : L"*";
            g.Text(valueRc, v, FontRole::Body, pal.text, kCenter);
        }
        if (!capRc.Empty()) {
            const std::wstring& cap =
                i < st.captions.size() ? st.captions[i]
                    : (i < defaultCaptions.size() ? defaultCaptions[i] : L"");
            g.Text(capRc, cap, FontRole::Meta, pal.textFaint, kCenter);
        }
    }
    if (!o.descriptionRc.Empty()) {
        g.Text(o.descriptionRc, st.description, FontRole::Meta,
               pal.textSub, kLeft);
    }
    for (std::size_t i = 0; i < o.presets.size(); ++i) {
        const Rect& p = o.presets[i];
        if (p.Empty()) continue;
        const int idx = static_cast<int>(i);
        const bool chosen = idx == st.selectedPreset;
        const bool hover = idx == st.hoveredPreset;
        g.FillRound(p, mx.radiusSm,
                    chosen ? pal.primary : (hover ? pal.hover : pal.sunken));
        g.StrokeRound(p, mx.radiusSm,
                      chosen ? pal.primary : pal.split, 1);
        g.Text(p, st.presets[i], FontRole::Meta,
               chosen ? pal.textInverse : pal.textSub, kCenter);
    }
}

namespace {

// ============================================================
// MentionInput
// ============================================================
MentionInputLayout GeoMention(const Rect& rc, const MentionInputState& st,
                              const Metrics& mx)
{
    MentionInputLayout o;
    if (rc.Empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.inputH + mx.rowH) return o;
    o.input = Rect(inner.left, inner.top, inner.right,
                   inner.top + mx.inputH);
    if (!st.showCandidates || st.candidates.empty() || inner.Empty()) return o;
    o.list = Rect(inner.left + mx.unit * 2, o.input.bottom + mx.unit / 2,
                  inner.right, inner.bottom);
    if (o.list.Empty()) return o;
    const int rowH = (std::min)(mx.rowH,
                                o.list.Height() /
                                    (std::max)(1, static_cast<int>(st.candidates.size())));
    if (rowH <= 0) return o;
    for (std::size_t i = 0; i < st.candidates.size(); ++i) {
        const int y = o.list.top + static_cast<int>(i) * rowH;
        if (y + rowH > o.list.bottom) break;
        o.rows.push_back(Rect(o.list.left + mx.unit / 2, y,
                              o.list.right - mx.unit / 2, y + rowH));
    }
    return o;
}

} // namespace

MentionInputLayout MeasureMentionInput(const Rect& rc,
                                       const MentionInputState& st,
                                       const Metrics& mx)
{
    return GeoMention(rc, st, mx);
}

MentionInputHit HitMentionInput(const Rect& rc, const MentionInputState& st,
                                const Metrics& mx, Point pt)
{
    MentionInputHit h;
    const MentionInputLayout o = GeoMention(rc, st, mx);
    if (Has(o.input, pt)) {
        h.input = true;
        return h;
    }
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        if (Has(o.rows[i], pt)) {
            h.candidate = static_cast<int>(i);
            return h;
        }
    }
    return h;
}

void PaintMentionInput(IPainter& g, const Rect& rc,
                       const MentionInputState& st,
                       const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const MentionInputLayout o = GeoMention(rc, st, mx);
    if (o.input.Empty()) return;
    const bool disabled = st.base.disabled;
    g.FillRound(o.input, mx.radiusMd,
                disabled ? pal.sunken : pal.surface);
    g.StrokeRound(o.input, mx.radiusMd,
                  st.focused ? pal.selectRing : pal.border, 1);
    const Rect textRc(o.input.left + mx.unit, o.input.top,
                      o.input.right - mx.unit * 3, o.input.bottom);
    if (!textRc.Empty()) {
        if (st.text.empty()) {
            g.Text(textRc, st.placeholder, FontRole::Body,
                   pal.textFaint, kLeft);
        } else {
            g.Text(textRc, st.text, FontRole::Body, pal.text, kLeft);
        }
    }
    if (st.focused) {
        const int caretX = (std::min)(
            textRc.right,
            textRc.left + static_cast<int>(st.text.size()) * mx.fontBody);
        g.FillRect(Rect(caretX, o.input.top + mx.unit / 2,
                        (std::min)(caretX + 1, o.input.right),
                        (std::min)(o.input.bottom - mx.unit / 2, o.input.bottom)),
                   pal.primary);
    }
    const Rect atRc(o.input.right - mx.unit * 3, o.input.top,
                    o.input.right, o.input.bottom);
    if (!atRc.Empty()) {
        g.Text(atRc, L"@", FontRole::Body, pal.primaryText, kCenter);
    }
    if (o.list.Empty()) return;

    g.FillRound(o.list, mx.radiusMd, pal.surface);
    g.StrokeRound(o.list, mx.radiusMd, pal.split, 1);
    g.PushClip(o.list);
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        const Rect& row = o.rows[i];
        if (row.Empty()) continue;
        const int idx = static_cast<int>(i);
        if (idx == st.selected || idx == st.hovered) {
            g.FillRound(row, mx.radiusSm,
                        idx == st.selected ? pal.selectFill : pal.hover);
        }
        const MentionCandidate& c = st.candidates[i];
        const int noteW = (std::min)(mx.unit * 12, row.Width() / 2);
        const Rect nameRc(row.left + mx.unit / 2, row.top,
                          row.right - noteW, row.bottom);
        const Rect noteRc(row.right - noteW, row.top,
                          row.right - mx.unit / 2, row.bottom);
        if (!nameRc.Empty()) {
            g.Text(nameRc, L"@" + c.name, FontRole::Body,
                   pal.text, kLeft);
        }
        if (!noteRc.Empty()) {
            g.Text(noteRc, c.note, FontRole::Meta, pal.textFaint, kRight);
        }
    }
    g.PopClip();
}

} // namespace ui
} // namespace air
