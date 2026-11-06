#include "airui/WidgetControls.h"
// AirUI - ControlsEnterpriseShell.cpp : platform-free shell widgets.
// Fourth wave: Chat, MultiStatusBar, Ribbon, NotificationCenter.
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

bool RectHit(const Rect& r, Point p)
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
// Chat
// ============================================================
ChatLayout GeoChat(const Rect& rc, const ChatState& st, const Metrics& mx)
{
    ChatLayout o;
    if (rc.Empty() || st.messages.empty()) return o;
    const Rect inner = Inset(rc, mx.unit);
    if (inner.Empty()) return o;
    const int avatarW = mx.rowH;
    const int side = mx.unit / 2;
    const int maxW = inner.Width() * 2 / 3;
    if (maxW < mx.unit * 8) return o;
    const int metaH = mx.tagH;
    const int gap = mx.unit / 2;
    int y = inner.top;
    for (const ChatMessage& msg : st.messages) {
        const int chars = static_cast<int>(msg.text.size());
        // CJK glyphs are about fontBody wide, not fontMeta; estimate
        // with the body size so wrapped lines do not end in one char.
        const int bubbleW = ClampI(chars * mx.fontBody + mx.unit * 2,
                                   mx.unit * 8, maxW);
        const int perLine = (std::max)(1, (bubbleW - mx.unit * 2) / (std::max)(1, mx.fontBody));
        const int lines = ClampI((chars + perLine - 1) / perLine, 1, 3);
        const int bubbleH = ClampI(lines * mx.rowH / 2 + mx.unit * 2,
                                   mx.rowH, mx.itemH * 2);
        const int fullH = bubbleH + metaH;
        if (y + fullH > inner.bottom) break;
        const int bx = msg.outgoing
                           ? inner.right - avatarW - side - bubbleW
                           : inner.left + avatarW + side;
        o.bubbles.push_back(Rect(bx, y, bx + bubbleW, y + fullH));
        y += fullH + gap;
    }
    return o;
}

// ============================================================
// MultiStatusBar
// ============================================================
Rect StatusBand(const Rect& rc, const Metrics& mx)
{
    if (rc.Empty()) return Rect{};
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty()) return Rect{};
    const int h = (std::min)(mx.statusH, inner.Height());
    if (h <= 0) return Rect{};
    const int top = inner.top + (inner.Height() - h) / 2;
    return Rect(inner.left, top, inner.right, top + h);
}

MultiStatusBarLayout GeoStatus(const Rect& rc,
                               const MultiStatusBarState& st,
                               const Metrics& mx)
{
    MultiStatusBarLayout o;
    if (rc.Empty()) return o;
    const Rect band = StatusBand(rc, mx);
    if (band.Empty()) return o;
    const int pad = mx.unit;
    const int trailingW = static_cast<int>(st.trailing.size()) * mx.fontMeta + pad * 2;
    if (trailingW + mx.unit * 8 >= band.Width()) return o;
    o.trailingRc = Rect(band.right - trailingW, band.top,
                        band.right, band.bottom);
    int x = o.trailingRc.left - pad / 2;
    for (std::size_t i = 0; i < st.segments.size(); ++i) {
        const StatusSegment& seg = st.segments[i];
        const int w = static_cast<int>(seg.text.size()) * mx.fontMeta + pad * 2;
        if (x - w < band.left + mx.unit * 12) break;
        o.segments.push_back(Rect(x - w, band.top, x, band.bottom));
        x -= w + pad / 2;
    }
    o.messageRc = Rect(band.left + pad, band.top,
                       (std::max)(band.left + pad, x - pad / 2), band.bottom);
    return o;
}

// ============================================================
// Ribbon
// ============================================================
RibbonLayout GeoRibbon(const Rect& rc, const RibbonState& st,
                       const Metrics& mx)
{
    RibbonLayout o;
    if (rc.Empty() || st.groups.empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty()) return o;
    const int gap = (std::max)(2, mx.unit / 2);
    const int titleH = mx.tagH;
    const int contentH = inner.Height() - titleH - gap;
    if (contentH < mx.rowH) return o;
    const int largeW = mx.unit * 9;
    const int smallW = mx.unit * 12;
    const int contentTop = inner.top;
    const int contentBottom = contentTop + contentH;
    int x = inner.left;
    for (const RibbonGroup& group : st.groups) {
        int largeCount = 0;
        int smallCount = 0;
        for (const RibbonButton& b : group.buttons) {
            if (b.large) ++largeCount;
            else ++smallCount;
        }
        // Large buttons take full-height columns; small buttons share
        // one trailing column and stack vertically (classic ribbon).
        const int width = gap * 2 + largeCount * (largeW + gap) +
                          (smallCount > 0 ? smallW + gap : 0);
        if (x + width > inner.right) break;
        o.groupRects.push_back(Rect(x, inner.top, x + width, inner.bottom));
        const int smallH = smallCount > 0 ? contentH / smallCount : contentH;
        const int smallX = x + gap + largeCount * (largeW + gap);
        std::vector<Rect> rects;
        int bx = x + gap;
        int smallIndex = 0;
        for (const RibbonButton& b : group.buttons) {
            if (b.large) {
                rects.push_back(Rect(bx, contentTop,
                                     bx + largeW, contentBottom));
                bx += largeW + gap;
            } else {
                const int y = contentTop + smallIndex * smallH;
                rects.push_back(Rect(smallX, y, smallX + smallW,
                                     (std::min)(y + smallH - 1,
                                                contentBottom)));
                ++smallIndex;
            }
        }
        o.buttons.push_back(rects);
        x += width;
    }
    return o;
}

// ============================================================
// NotificationCenter
// ============================================================
NoticeCenterLayout GeoNotice(const Rect& rc, const NoticeCenterState& st,
                             const Metrics& mx)
{
    NoticeCenterLayout o;
    if (rc.Empty()) return o;
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty() || inner.Height() <= mx.groupH) return o;
    o.header = Rect(inner.left, inner.top, inner.right,
                    inner.top + mx.groupH);
    const int readAllW = mx.unit * 12;
    if (o.header.Width() <= readAllW + mx.unit * 10) return o;
    o.readAll = Rect(o.header.right - readAllW, o.header.top,
                     o.header.right, o.header.bottom);
    o.rowH = mx.itemH - mx.unit;
    int y = o.header.bottom;
    const int total = static_cast<int>(st.items.size());
    for (int i = 0; i < total && y + o.rowH <= inner.bottom; ++i) {
        o.rows.push_back(Rect(inner.left, y, inner.right, y + o.rowH));
        y += o.rowH;
    }
    return o;
}

} // namespace

// ============================================================
// Chat
// ============================================================






// ============================================================
// MultiStatusBar
// ============================================================






// ============================================================
// Ribbon
// ============================================================






// ============================================================
// NotificationCenter
// ============================================================




int NoticeUnreadCount(const NoticeCenterState& st)
{
    int n = 0;
    for (const NoticeItem& item : st.items) {
        if (item.unread) ++n;
    }
    return n;
}



} // namespace ui
} // namespace air

// ============================================================
// Object layer: widget member functions migrated from the
// free-function drawing kernel above.
// ============================================================
namespace air {
namespace ui {
namespace widgets {

ChatLayout Chat::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const ChatState& st = state;
    (void)mx;
    return GeoChat(rc, st, mx);
}

int Chat::MessageAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const ChatState& st = state;
    (void)mx;
    (void)pt;
    if (rc.Empty() || !rc.Contains(pt)) return -1;
    const ChatLayout lay = GeoChat(rc, st, mx);
    for (std::size_t i = 0; i < lay.bubbles.size(); ++i) {
        if (RectHit(lay.bubbles[i], pt)) return static_cast<int>(i);
    }
    return -1;
}

void Chat::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    ChatState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const ChatLayout lay = GeoChat(rc, st, mx);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    const Rect inner = Inset(rc, mx.unit);
    if (inner.Empty()) return;
    g.PushClip(inner);
    const int avatarW = mx.rowH;
    const int metaH = mx.tagH;
    for (std::size_t i = 0; i < lay.bubbles.size(); ++i) {
        const Rect& row = lay.bubbles[i];
        const ChatMessage& msg = st.messages[i];
        if (row.Empty()) continue;
        const Rect bubble(row.left, row.top, row.right,
                          row.bottom - metaH);
        const Rect timeRc(row.left, bubble.bottom, row.right, row.bottom);
        const Rect avatar = msg.outgoing
                                ? Rect(inner.right - avatarW, row.top,
                                       inner.right, row.top + avatarW)
                                : Rect(inner.left, row.top,
                                       inner.left + avatarW, row.top + avatarW);
        const bool hot = st.hovered == static_cast<int>(i);
        g.FillRound(avatar, mx.radiusSm,
                    msg.outgoing ? pal.primarySoft : pal.iconTile);
        if (!msg.author.empty()) {
            g.Text(avatar, msg.author.substr(0, 1), FontRole::Body,
                   msg.outgoing ? pal.primaryText : pal.textSub, kCenter);
        }
        g.FillRound(bubble, mx.radiusMd,
                    msg.outgoing ? pal.primarySoft : pal.sunken);
        g.StrokeRound(bubble, mx.radiusMd,
                      hot ? pal.hoverBorder
                          : (msg.outgoing ? pal.primary : pal.split), 1);
        const Rect textRc = Inset(bubble, mx.unit / 2);
        if (!textRc.Empty()) {
            g.TextBlock(textRc, msg.text, FontRole::Body, pal.text, 3);
        }
        if (!timeRc.Empty()) {
            g.Text(timeRc, msg.time, FontRole::Meta, pal.textFaint,
                   msg.outgoing ? kRight : kLeft);
        }
    }
    g.PopClip();
}

MultiStatusBarLayout MultiStatusBar::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const MultiStatusBarState& st = state;
    (void)mx;
    return GeoStatus(rc, st, mx);
}

MultiStatusBarHit MultiStatusBar::HitAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const MultiStatusBarState& st = state;
    (void)mx;
    (void)pt;
    MultiStatusBarHit out;
    if (rc.Empty() || !rc.Contains(pt)) return out;
    const MultiStatusBarLayout lay = GeoStatus(rc, st, mx);
    for (std::size_t i = 0; i < lay.segments.size(); ++i) {
        if (RectHit(lay.segments[i], pt)) {
            out.segment = static_cast<int>(i);
            return out;
        }
    }
    out.message = RectHit(lay.messageRc, pt);
    return out;
}

void MultiStatusBar::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    MultiStatusBarState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const MultiStatusBarLayout lay = GeoStatus(rc, st, mx);
    const Rect band = StatusBand(rc, mx);
    if (band.Empty()) return;
    g.FillRect(band, pal.sunken);
    g.FillRect(Rect(band.left, band.top, band.right, band.top + 1), pal.split);
    g.PushClip(band);
    if (!lay.messageRc.Empty()) {
        g.Text(Inset(lay.messageRc, mx.unit / 4), st.message,
               FontRole::Meta, pal.textSub, kLeft);
    }
    if (!lay.trailingRc.Empty()) {
        g.Text(Inset(lay.trailingRc, mx.unit / 4), st.trailing,
               FontRole::Meta, pal.textFaint, kRight);
    }
    for (std::size_t i = 0; i < lay.segments.size(); ++i) {
        const Rect& seg = lay.segments[i];
        if (seg.Empty()) continue;
        const StatusSegment& item = st.segments[i];
        const KindColors kc = ColorsFor(item.kind, pal);
        if (item.active || st.hovered == static_cast<int>(i)) {
            g.FillRound(seg, mx.radiusSm, pal.surface);
        }
        const int dot = (std::min)(mx.tagH / 2, seg.Height() / 2);
        const Rect dotRc(seg.left + mx.unit / 2,
                         seg.top + (seg.Height() - dot) / 2,
                         seg.left + mx.unit / 2 + dot,
                         seg.top + (seg.Height() + dot) / 2);
        g.FillRound(dotRc, mx.radiusSm, kc.accent);
        const Rect label(seg.left + mx.unit / 2 + dot + mx.unit / 4,
                         seg.top, seg.right, seg.bottom);
        if (!label.Empty()) {
            g.Text(label, item.text, FontRole::Meta, pal.textSub, kLeft);
        }
    }
    g.PopClip();
}

RibbonLayout Ribbon::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const RibbonState& st = state;
    (void)mx;
    return GeoRibbon(rc, st, mx);
}

RibbonHit Ribbon::HitAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const RibbonState& st = state;
    (void)mx;
    (void)pt;
    RibbonHit out;
    if (rc.Empty() || !rc.Contains(pt)) return out;
    const RibbonLayout lay = GeoRibbon(rc, st, mx);
    for (std::size_t gi = 0; gi < lay.groupRects.size(); ++gi) {
        if (!lay.groupRects[gi].Contains(pt)) continue;
        out.group = static_cast<int>(gi);
        if (gi < lay.buttons.size()) {
            for (std::size_t bi = 0; bi < lay.buttons[gi].size(); ++bi) {
                if (RectHit(lay.buttons[gi][bi], pt)) {
                    out.button = static_cast<int>(bi);
                    break;
                }
            }
        }
        return out;
    }
    return out;
}

void Ribbon::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    RibbonState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const RibbonLayout lay = GeoRibbon(rc, st, mx);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty()) return;
    g.PushClip(inner);
    const int titleH = mx.tagH;
    for (std::size_t gi = 0; gi < lay.groupRects.size(); ++gi) {
        const Rect& groupRc = lay.groupRects[gi];
        const RibbonGroup& group = st.groups[gi];
        if (gi > 0) {
            g.FillRect(Rect(groupRc.left, groupRc.top + mx.unit / 2,
                            groupRc.left + 1, groupRc.bottom - titleH),
                       pal.split);
        }
        const Rect titleRc(groupRc.left, groupRc.bottom - titleH,
                           groupRc.right, groupRc.bottom);
        g.Text(titleRc, group.title, FontRole::Meta, pal.textFaint, kCenter);
        if (gi >= lay.buttons.size()) continue;
        for (std::size_t bi = 0; bi < lay.buttons[gi].size(); ++bi) {
            const Rect& btn = lay.buttons[gi][bi];
            if (btn.Empty() || bi >= group.buttons.size()) continue;
            const RibbonButton& item = group.buttons[bi];
            const bool hot = st.hoveredGroup == static_cast<int>(gi) &&
                             st.hoveredButton == static_cast<int>(bi);
            if (hot && !item.disabled) g.FillRound(btn, mx.radiusSm, pal.hover);
            else if (item.large) g.FillRound(btn, mx.radiusSm, pal.sunken);
            if (item.large) {
                g.StrokeRound(btn, mx.radiusSm, pal.split, 1);
                const int glyphH = (std::min)(mx.rowH, btn.Height() / 2);
                const Rect glyphRc(btn.left, btn.top, btn.right,
                                   btn.top + glyphH);
                const Rect labelRc(btn.left, glyphRc.bottom, btn.right,
                                   btn.bottom);
                g.Text(glyphRc, item.glyph, FontRole::Body,
                       item.disabled ? pal.textDisabled : pal.primaryText,
                       kCenter);
                g.Text(labelRc, item.label, FontRole::Meta,
                       item.disabled ? pal.textDisabled : pal.text, kCenter);
            } else {
                const int box = (std::min)(mx.rowH - mx.unit, btn.Height());
                const Rect glyphRc(btn.left + mx.unit / 2,
                                   btn.top + (btn.Height() - box) / 2,
                                   btn.left + mx.unit / 2 + box,
                                   btn.top + (btn.Height() + box) / 2);
                g.FillRound(glyphRc, mx.radiusSm, pal.iconTile);
                g.Text(glyphRc, item.glyph, FontRole::Meta,
                       item.disabled ? pal.textDisabled : pal.primaryText,
                       kCenter);
                const Rect labelRc(glyphRc.right + mx.unit / 2, btn.top,
                                   btn.right, btn.bottom);
                if (!labelRc.Empty()) {
                    g.Text(labelRc, item.label, FontRole::Meta,
                           item.disabled ? pal.textDisabled : pal.text, kLeft);
                }
            }
        }
    }
    g.PopClip();
}

NoticeCenterLayout NoticeCenter::Measure(const Metrics& mx) const
{
    const Rect& rc = Bounds();
    const NoticeCenterState& st = state;
    (void)mx;
    return GeoNotice(rc, st, mx);
}

NoticeCenterHit NoticeCenter::HitAt(const Metrics& mx, Point pt) const
{
    const Rect& rc = Bounds();
    const NoticeCenterState& st = state;
    (void)mx;
    (void)pt;
    NoticeCenterHit out;
    if (rc.Empty() || !rc.Contains(pt)) return out;
    const NoticeCenterLayout lay = GeoNotice(rc, st, mx);
    if (RectHit(lay.readAll, pt)) { out.readAll = true; return out; }
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        if (RectHit(lay.rows[i], pt)) {
            out.item = static_cast<int>(i);
            return out;
        }
    }
    return out;
}

void NoticeCenter::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& rc = Bounds();
    NoticeCenterState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (rc.Empty()) return;
    const NoticeCenterLayout lay = GeoNotice(rc, st, mx);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    const Rect inner = Inset(rc, mx.unit / 2);
    if (inner.Empty()) return;
    g.PushClip(inner);
    if (!lay.header.Empty()) {
        g.FillRect(lay.header, pal.sunken);
        const Rect titleRc(lay.header.left + mx.unit, lay.header.top,
                           (std::max)(lay.header.left + mx.unit,
                                      lay.readAll.left - mx.unit),
                           lay.header.bottom);
        g.Text(titleRc,
               st.title + L"  ·  " +
                   std::to_wstring(NoticeUnreadCount(st)) + L" 条未读",
               FontRole::Section, pal.text, kLeft);
        if (!lay.readAll.Empty()) {
            g.FillRound(Inset(lay.readAll, mx.unit / 4), mx.radiusSm,
                        st.readAllHover ? pal.primarySoft : pal.surface);
            g.Text(lay.readAll, L"全部已读", FontRole::Meta,
                   st.readAllHover ? pal.primaryText : pal.textSub, kCenter);
        }
    }
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        const Rect& row = lay.rows[i];
        const NoticeItem& item = st.items[i];
        if (st.hovered == static_cast<int>(i)) g.FillRect(row, pal.hover);
        if (item.unread) g.FillRect(Rect(row.left, row.top, row.right,
                                         row.bottom), pal.primarySoft);
        g.FillRect(Rect(row.left, row.bottom - 1, row.right, row.bottom),
                   pal.split);
        const KindColors kc = ColorsFor(item.kind, pal);
        const int dot = (std::max)(2, mx.unit);
        const Rect dotRc(row.left + mx.unit,
                         row.top + mx.unit,
                         row.left + mx.unit + dot,
                         row.top + mx.unit + dot);
        g.FillRound(dotRc, mx.radiusSm,
                    item.unread ? kc.accent : pal.split);
        const int left = dotRc.right + mx.unit;
        const int timeW = mx.unit * 8;
        const Rect timeRc((std::max)(left, row.right - timeW - mx.unit),
                          row.top, row.right - mx.unit,
                          row.top + mx.rowH / 2);
        const Rect titleRc(left, row.top,
                           (std::max)(left, timeRc.left - mx.unit / 2),
                           row.top + mx.rowH / 2);
        const Rect bodyRc(left, titleRc.bottom,
                          row.right - mx.unit, row.bottom);
        if (!titleRc.Empty()) {
            g.Text(titleRc, item.title, FontRole::Body, pal.text, kLeft);
        }
        if (!timeRc.Empty()) {
            g.Text(timeRc, item.time, FontRole::Meta, pal.textFaint, kRight);
        }
        if (!bodyRc.Empty()) {
            g.Text(bodyRc, item.body, FontRole::Meta, pal.textSub, kLeft);
            if (!item.group.empty()) {
                g.Text(bodyRc, item.group, FontRole::Meta, kc.text, kRight);
            }
        }
    }
    g.PopClip();
}

} // namespace widgets
} // namespace ui
} // namespace air
