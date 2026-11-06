#include "airui/WidgetControls.h"
// AirUI - ControlsStudioShell.cpp : platform-free collaboration widgets.
// Fifth wave: OrgChart, Leaderboard, ActivityFeed, AudioWaveform.
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

std::wstring Clock(double seconds)
{
    const int total = (std::max)(0, static_cast<int>(seconds + 0.5));
    const int m = total / 60;
    const int s = total % 60;
    const std::wstring ss = s < 10 ? L"0" + std::to_wstring(s)
                                   : std::to_wstring(s);
    return std::to_wstring(m) + L":" + ss;
}

std::wstring Whole(double v)
{
    return std::to_wstring(static_cast<int>(v < 0 ? v - 0.5 : v + 0.5));
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

// Positive movement is warm, negative is cool (local convention).
Color DeltaColor(int delta, const Palette& pal)
{
    if (delta > 0) return pal.errorText;
    if (delta < 0) return pal.okText;
    return pal.textFaint;
}

std::wstring DeltaText(int delta)
{
    if (delta > 0) return L"+" + std::to_wstring(delta);
    if (delta < 0) return std::to_wstring(delta);
    return L"-";
}

// ============================================================
// OrgChart
// ============================================================
OrgChartLayout GeoOrg(const Rect& rc, const OrgChartState& st,
                      const Metrics& mx)
{
    OrgChartLayout o;
    if (rc.Empty() || st.nodes.empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.rowH * 2) return o;
    const int n = static_cast<int>(st.nodes.size());
    o.depths.assign(static_cast<std::size_t>(n), 0);
    // Relax parent links so node order in the vector does not matter.
    for (int pass = 0; pass < n; ++pass) {
        for (int i = 0; i < n; ++i) {
            const int p = st.nodes[static_cast<std::size_t>(i)].parent;
            if (p >= 0 && p < n && p != i) {
                o.depths[static_cast<std::size_t>(i)] =
                    ClampI(o.depths[static_cast<std::size_t>(p)] + 1, 0, 4);
            }
        }
    }
    int maxDepth = 0;
    for (int d : o.depths) maxDepth = (std::max)(maxDepth, d);
    const int levels = maxDepth + 1;
    const int levelH = inner.Height() / levels;
    if (levelH <= 0) return o;

    int maxPerLevel = 1;
    for (int d = 0; d <= maxDepth; ++d) {
        int count = 0;
        for (int i = 0; i < n; ++i) {
            if (o.depths[static_cast<std::size_t>(i)] == d) ++count;
        }
        maxPerLevel = (std::max)(maxPerLevel, count);
    }
    const int slotW = inner.Width() / maxPerLevel;
    const int boxW = (std::max)(mx.unit * 4, (std::min)(mx.unit * 22,
                                    slotW - mx.unit));
    const int boxH = (std::max)(mx.rowH, (std::min)(mx.itemH,
                                    levelH - mx.unit * 2));
    if (boxH <= 0) return o;
    o.boxes.assign(static_cast<std::size_t>(n), Rect{});

    for (int d = 0; d <= maxDepth; ++d) {
        std::vector<int> ids;
        for (int i = 0; i < n; ++i) {
            if (o.depths[static_cast<std::size_t>(i)] == d) ids.push_back(i);
        }
        if (ids.empty()) continue;
        const int count = static_cast<int>(ids.size());
        const int avail = inner.Width() / count;
        const int levelTop = inner.top + d * levelH;
        const int y = levelTop + (std::max)(0, (levelH - boxH) / 2);
        for (int k = 0; k < count; ++k) {
            const int cx = inner.left + k * avail + avail / 2;
            const int x = ClampI(cx - boxW / 2, inner.left,
                                 (std::max)(inner.left, inner.right - boxW));
            o.boxes[static_cast<std::size_t>(ids[static_cast<std::size_t>(k)])] =
                Rect(x, y, (std::min)(x + boxW, inner.right),
                     (std::min)(y + boxH, inner.bottom));
        }
    }

    const int thick = (std::max)(1, mx.unit / 8);
    for (int i = 0; i < n; ++i) {
        const int p = st.nodes[static_cast<std::size_t>(i)].parent;
        if (p < 0 || p >= n || p == i) continue;
        const Rect& child = o.boxes[static_cast<std::size_t>(i)];
        const Rect& parent = o.boxes[static_cast<std::size_t>(p)];
        if (child.Empty() || parent.Empty()) continue;
        if (child.top <= parent.bottom) continue;
        const int midY = (parent.bottom + child.top) / 2;
        const int px = (parent.left + parent.right) / 2;
        const int cx = (child.left + child.right) / 2;
        o.links.push_back(Rect(px, parent.bottom, px + thick,
                               (std::min)(midY + thick, child.top)));
        const int x1 = (std::min)(px, cx);
        const int x2 = (std::max)(px, cx);
        if (x2 > x1) {
            o.links.push_back(Rect(x1, midY, x2 + thick,
                                   (std::min)(midY + thick, child.top)));
        }
        o.links.push_back(Rect(cx, midY, cx + thick,
                               (std::min)(child.top, inner.bottom)));
    }
    return o;
}

} // namespace







namespace {

// ============================================================
// Leaderboard
// ============================================================
LeaderboardLayout GeoLeaders(const Rect& rc, const LeaderboardState& st,
                             const Metrics& mx)
{
    LeaderboardLayout o;
    if (rc.Empty() || st.rows.empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.rowH * 2) return o;
    o.header = Rect(inner.left, inner.top, inner.right,
                    inner.top + mx.rowH);
    o.body = Rect(inner.left, o.header.bottom, inner.right, inner.bottom);
    if (o.body.Empty()) return o;
    o.rowH = (std::min)(mx.itemH,
                        o.body.Height() /
                            (std::max)(1, static_cast<int>(st.rows.size())));
    if (o.rowH <= 0) o.rowH = mx.rowH;
    for (std::size_t i = 0; i < st.rows.size(); ++i) {
        const int y = o.body.top + static_cast<int>(i) * o.rowH;
        if (y + o.rowH > o.body.bottom) break;
        o.rows.push_back(Rect(o.body.left, y, o.body.right, y + o.rowH));
    }
    return o;
}

} // namespace







namespace {

// ============================================================
// ActivityFeed
// ============================================================
ActivityFeedLayout GeoFeed(const Rect& rc, const ActivityFeedState& st,
                           const Metrics& mx)
{
    ActivityFeedLayout o;
    if (rc.Empty() || st.items.empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.rowH * 2) return o;
    o.header = Rect(inner.left, inner.top, inner.right,
                    inner.top + mx.rowH);
    const Rect body(inner.left, o.header.bottom, inner.right, inner.bottom);
    if (body.Empty()) return o;
    o.rowH = (std::min)(mx.itemH,
                        body.Height() /
                            (std::max)(1, static_cast<int>(st.items.size())));
    if (o.rowH <= 0) o.rowH = mx.rowH;
    const int avatarW = (std::min)(mx.rowH, o.rowH - mx.unit / 2);
    for (std::size_t i = 0; i < st.items.size(); ++i) {
        const int y = body.top + static_cast<int>(i) * o.rowH;
        if (y + o.rowH > body.bottom) break;
        o.rows.push_back(Rect(body.left, y, body.right, y + o.rowH));
        o.avatars.push_back(Rect(body.left + mx.unit / 2,
                                 y + (o.rowH - avatarW) / 2,
                                 body.left + mx.unit / 2 + avatarW,
                                 y + (o.rowH - avatarW) / 2 + avatarW));
    }
    return o;
}

} // namespace







namespace {

// ============================================================
// AudioWaveform
// ============================================================
WaveformLayout GeoWave(const Rect& rc, const AudioWaveformState& st,
                       const Metrics& mx)
{
    WaveformLayout o;
    if (rc.Empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() < mx.rowH * 3) return o;
    o.header = Rect(inner.left, inner.top, inner.right,
                    inner.top + mx.rowH);
    o.timeRc = Rect(inner.left, inner.bottom - mx.tagH,
                    inner.right, inner.bottom);
    const int btnW = mx.rowH;
    const int btnTop = o.header.bottom + mx.unit;
    o.playBtn = Rect(inner.left, btnTop,
                     inner.left + btnW,
                     (std::min)(btnTop + mx.rowH, o.timeRc.top - mx.unit));
    o.track = Rect(o.playBtn.right + mx.unit / 2,
                   o.header.bottom + mx.unit / 2,
                   inner.right, o.timeRc.top - mx.unit / 2);
    if (o.track.Empty()) return o;

    const int n = static_cast<int>(st.peaks.size());
    if (n > 0) {
        const int gap = (std::max)(1, mx.unit / 8);
        const int slot = o.track.Width() / n;
        if (slot > gap) {
            const int centerY = (o.track.top + o.track.bottom) / 2;
            for (int i = 0; i < n; ++i) {
                const double peak = ClampD(st.peaks[static_cast<std::size_t>(i)],
                                           0.05, 1.0);
                const int half = (std::max)(1,
                    static_cast<int>(o.track.Height() / 2 * peak));
                const int x = o.track.left + i * slot;
                o.bars.push_back(Rect(x + gap / 2,
                                      ClampI(centerY - half, o.track.top,
                                             o.track.bottom - 1),
                                      x + slot - gap / 2,
                                      ClampI(centerY + half, o.track.top + 1,
                                             o.track.bottom)));
            }
        }
    }
    const double t = ClampD(st.position, 0.0, 1.0);
    const int px = o.track.left + static_cast<int>(t * o.track.Width());
    o.playhead = Rect(ClampI(px, o.track.left, o.track.right - 1),
                      o.track.top,
                      ClampI(px + (std::max)(1, mx.unit / 4),
                             o.track.left + 1, o.track.right),
                      o.track.bottom);
    return o;
}

} // namespace









} // namespace ui
} // namespace air

// ============================================================
// Object layer: widget member functions migrated from the
// free-function drawing kernel above.
// ============================================================
namespace air {
namespace ui {
namespace widgets {

OrgChartLayout OrgChart::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const OrgChartState& st = state;
    (void)mx;
    return GeoOrg(rc, st, mx);
}

int OrgChart::OrgNodeAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const OrgChartState& st = state;
    (void)mx;
    (void)pt;
    const OrgChartLayout o = GeoOrg(rc, st, mx);
    for (std::size_t i = 0; i < o.boxes.size(); ++i) {
        if (Has(o.boxes[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void OrgChart::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    OrgChartState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const OrgChartLayout o = GeoOrg(rc, st, mx);
    if (o.boxes.empty()) return;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty()) return;
    g.PushClip(inner);
    for (std::size_t i = 0; i < o.links.size(); ++i) {
        if (!o.links[i].Empty()) g.FillRect(o.links[i], pal.border);
    }
    for (std::size_t i = 0; i < o.boxes.size(); ++i) {
        const Rect& box = o.boxes[i];
        if (box.Empty()) continue;
        const int idx = static_cast<int>(i);
        const bool root = st.nodes[i].parent < 0;
        g.FillRound(box, mx.radiusMd,
                    root ? pal.primarySoft : pal.surface);
        g.StrokeRound(box, mx.radiusMd,
                      idx == st.selected ? pal.selectRing
                          : (idx == st.hovered ? pal.hoverBorder : pal.split), 1);
        const int nameH = (std::max)(mx.rowH / 2, box.Height() / 2);
        const Rect nameRc(box.left + mx.unit / 2, box.top,
                          box.right - mx.unit / 2,
                          (std::min)(box.top + nameH, box.bottom));
        const Rect roleRc(box.left + mx.unit / 2, nameRc.bottom,
                          box.right - mx.unit / 2, box.bottom);
        if (!nameRc.Empty()) {
            g.Text(nameRc, st.nodes[i].name, FontRole::Body,
                   root ? pal.primaryText : pal.text, kCenter);
        }
        if (!roleRc.Empty()) {
            g.Text(roleRc, st.nodes[i].role, FontRole::Meta,
                   pal.textFaint, kCenter);
        }
    }
    g.PopClip();
}

LeaderboardLayout Leaderboard::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const LeaderboardState& st = state;
    (void)mx;
    return GeoLeaders(rc, st, mx);
}

int Leaderboard::LeaderRowAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const LeaderboardState& st = state;
    (void)mx;
    (void)pt;
    const LeaderboardLayout o = GeoLeaders(rc, st, mx);
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        if (Has(o.rows[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void Leaderboard::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    LeaderboardState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const LeaderboardLayout o = GeoLeaders(rc, st, mx);
    if (o.header.Empty()) return;
    g.Text(o.header, st.title, FontRole::Section, pal.text, kLeft);
    g.Text(o.header, std::to_wstring(st.rows.size()) + L" 位成员",
           FontRole::Meta, pal.textFaint, kRight);
    if (o.body.Empty()) return;

    double best = 1.0;
    for (const LeaderRow& r : st.rows) best = (std::max)(best, r.score);
    g.PushClip(o.body);
    const int rankW = mx.rowH;
    const int scoreW = mx.unit * 10;
    const int deltaW = mx.unit * 5;
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        const Rect& row = o.rows[i];
        if (row.Empty()) continue;
        const LeaderRow& item = st.rows[i];
        const int idx = static_cast<int>(i);
        if (idx == st.selected) {
            g.FillRect(row, pal.selectFill);
        } else if (idx == st.hovered) {
            g.FillRect(row, pal.hover);
        }
        const Rect rankRc(row.left + mx.unit / 2, row.top + mx.unit / 4,
                          row.left + mx.unit / 2 + rankW,
                          (std::min)(row.bottom - mx.unit / 4, row.bottom));
        if (!rankRc.Empty()) {
            const Color fill = idx < 3 ? pal.primarySoft : pal.sunken;
            g.FillRound(rankRc, mx.radiusSm, fill);
            g.Text(rankRc, std::to_wstring(idx + 1), FontRole::Body,
                   idx < 3 ? pal.primaryText : pal.textSub, kCenter);
        }
        const int textLeft = rankRc.right + mx.unit / 2;
        const int textRight = (std::max)(textLeft,
                                         row.right - scoreW - deltaW);
        const Rect nameRc(textLeft, row.top, textRight,
                          row.top + o.rowH / 2);
        const Rect noteRc(textLeft, nameRc.bottom, textRight,
                          (std::max)(nameRc.bottom, row.bottom - mx.unit));
        if (!nameRc.Empty()) {
            g.Text(nameRc, item.name, FontRole::Body, pal.text, kLeft);
        }
        if (!noteRc.Empty()) {
            g.Text(noteRc, item.note, FontRole::Meta, pal.textFaint, kLeft);
        }
        const Rect scoreRc(row.right - scoreW - deltaW, row.top,
                           row.right - deltaW, row.bottom);
        const Rect deltaRc(row.right - deltaW, row.top,
                           row.right, row.bottom);
        if (!scoreRc.Empty()) {
            g.Text(scoreRc, Whole(item.score), FontRole::Body,
                   pal.text, kRight);
        }
        if (!deltaRc.Empty()) {
            g.Text(deltaRc, DeltaText(item.delta), FontRole::Meta,
                   DeltaColor(item.delta, pal), kRight);
        }
        const int barW = static_cast<int>(
            (std::max)(0.0, (std::min)(1.0, item.score / best)) *
            (std::max)(0, nameRc.Width()));
        const int barH = (std::max)(3, mx.unit / 3);
        const Rect track(noteRc.left, row.bottom - barH - 1,
                         noteRc.right, row.bottom - 1);
        if (!track.Empty()) {
            g.FillRect(track, pal.split);
            const Rect fill(track.left, track.top,
                            (std::min)(track.left + barW, track.right),
                            track.bottom);
            if (!fill.Empty()) g.FillRect(fill, pal.primary);
        }
    }
    g.PopClip();
}

ActivityFeedLayout ActivityFeed::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const ActivityFeedState& st = state;
    (void)mx;
    return GeoFeed(rc, st, mx);
}

int ActivityFeed::ActivityItemAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const ActivityFeedState& st = state;
    (void)mx;
    (void)pt;
    const ActivityFeedLayout o = GeoFeed(rc, st, mx);
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        if (Has(o.rows[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void ActivityFeed::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    ActivityFeedState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const ActivityFeedLayout o = GeoFeed(rc, st, mx);
    if (o.header.Empty()) return;
    int unread = 0;
    for (const ActivityItem& item : st.items) {
        if (item.unread) ++unread;
    }
    g.Text(o.header, st.title, FontRole::Section, pal.text, kLeft);
    g.Text(o.header, std::to_wstring(unread) + L" 条未读",
           FontRole::Meta, pal.textFaint, kRight);
    if (o.rows.empty()) return;

    const Rect inner = Inset(rc, mx.unit / 2);
    g.PushClip(Rect(inner.left, o.header.bottom, inner.right, inner.bottom));
    const int timeW = mx.unit * 10;
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        const Rect& row = o.rows[i];
        if (row.Empty()) continue;
        const ActivityItem& item = st.items[i];
        const KindColors kc = ColorsFor(item.kind, pal);
        if (static_cast<int>(i) == st.hovered) {
            g.FillRound(row, mx.radiusSm, pal.hover);
        }
        const Rect& av = o.avatars[i];
        if (!av.Empty()) {
            g.FillRound(av, mx.radiusSm, kc.soft);
            g.Text(av,
                   item.actor.empty() ? L"?" : std::wstring(1, item.actor[0]),
                   FontRole::Body, kc.text, kCenter);
        }
        const int textLeft = av.right + mx.unit / 2;
        const Rect titleRc(textLeft, row.top,
                           (std::max)(textLeft, row.right - timeW),
                           row.top + o.rowH / 2);
        const Rect detailRc(textLeft, titleRc.bottom,
                            (std::max)(textLeft, row.right - timeW),
                            row.bottom);
        const std::wstring line = item.actor + L" " + item.action + L" " +
                                  item.target;
        if (!titleRc.Empty()) {
            g.Text(titleRc, line, FontRole::Body,
                   item.unread ? pal.text : pal.textSub, kLeft);
        }
        if (!detailRc.Empty()) {
            g.Text(detailRc, item.kind == AlertKind::Error
                                 ? L"需要关注 · " + item.action
                                 : L"活动记录 · " + item.action,
                   FontRole::Meta, pal.textFaint, kLeft);
        }
        const Rect timeRc(row.right - timeW, row.top, row.right, row.bottom);
        if (!timeRc.Empty()) {
            g.Text(timeRc, item.time, FontRole::Meta, pal.textFaint, kRight);
        }
        if (item.unread && !titleRc.Empty()) {
            const int dot = (std::max)(3, mx.unit / 2);
            g.FillRound(Rect(row.right - mx.unit, row.top + dot,
                             row.right - mx.unit + dot,
                             row.top + dot * 2),
                        mx.radiusSm, pal.error);
        }
        g.FillRect(Rect(row.left, row.bottom - 1, row.right, row.bottom),
                   pal.split);
    }
    g.PopClip();
}

WaveformLayout AudioWaveform::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const AudioWaveformState& st = state;
    (void)mx;
    return GeoWave(rc, st, mx);
}

int AudioWaveform::WaveformPositionAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const AudioWaveformState& st = state;
    (void)mx;
    (void)pt;
    const WaveformLayout o = GeoWave(rc, st, mx);
    if (o.track.Empty() || o.track.Width() <= 0) return 0;
    const int x = ClampI(pt.x, o.track.left, o.track.right - 1);
    const int milli = (x - o.track.left) * 1000 / o.track.Width();
    return ClampI(milli, 0, 1000);
}

bool AudioWaveform::WaveformPlayButtonAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const AudioWaveformState& st = state;
    (void)mx;
    (void)pt;
    const WaveformLayout o = GeoWave(rc, st, mx);
    return Has(o.playBtn, pt);
}

void AudioWaveform::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    AudioWaveformState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const WaveformLayout o = GeoWave(rc, st, mx);
    if (o.header.Empty()) return;
    const double pos = ClampD(st.position, 0.0, 1.0);
    g.Text(o.header, st.title, FontRole::Section, pal.text, kLeft);
    g.Text(o.header, Clock(st.duration), FontRole::Meta,
           pal.textFaint, kRight);

    if (!o.playBtn.Empty()) {
        g.FillRound(o.playBtn, mx.radiusMd, pal.primary);
        g.Text(o.playBtn, st.playing ? L"停" : L"播", FontRole::Body,
               pal.textInverse, kCenter);
    }
    if (o.track.Empty()) return;
    g.FillRect(o.track, pal.sunken);
    g.PushClip(o.track);
    const int playedX = o.track.left +
                        static_cast<int>(pos * o.track.Width());
    for (std::size_t i = 0; i < o.bars.size(); ++i) {
        const Rect& bar = o.bars[i];
        if (bar.Empty()) continue;
        const Color fill = bar.left <= playedX ? pal.primary : pal.chipBorder;
        g.FillRound(bar, mx.radiusSm,
                    st.base.disabled ? pal.textDisabled : fill);
        if (static_cast<int>(i) == st.hovered) {
            g.StrokeRound(bar, mx.radiusSm, pal.selectRing, 1);
        }
    }
    if (!o.playhead.Empty()) g.FillRect(o.playhead, pal.error);
    g.PopClip();
    if (!o.timeRc.Empty()) {
        g.Text(o.timeRc, Clock(st.duration * pos), FontRole::Meta,
               pal.primaryText, kLeft);
        g.Text(o.timeRc, st.playing ? L"播放中" : L"已暂停",
               FontRole::Meta, pal.textSub, kCenter);
        g.Text(o.timeRc, Clock(st.duration), FontRole::Meta,
               pal.textFaint, kRight);
    }
}

} // namespace widgets
} // namespace ui
} // namespace air
