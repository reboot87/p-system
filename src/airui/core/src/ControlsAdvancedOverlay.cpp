#include "airui/WidgetControls.h"
// AirUI - ControlsAdvancedOverlay.cpp : platform-free overlay widgets.
// Third wave: Snackbar, CommandPalette, GuideTour.
// All three take the client rect and position themselves inside it;
// scrims use the optional alpha fill so an opaque fallback backend
// still renders something readable.
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

int ClampI(int v, int lo, int hi)
{
    return (std::min)((std::max)(v, lo), hi);
}

bool RectHit(const Rect& r, Point p)
{
    return !r.Empty() && r.Contains(p);
}

Rect InsetR(const Rect& r, int d)
{
    return Rect(r.left + d, r.top + d, r.right - d, r.bottom - d);
}

Color AccentFor(AlertKind kind, const Palette& pal)
{
    switch (kind) {
    case AlertKind::Success: return pal.ok;
    case AlertKind::Warning: return pal.warn;
    case AlertKind::Error:   return pal.error;
    default:                 return pal.primary;
    }
}

std::wstring LowerAscii(const std::wstring& s)
{
    std::wstring out = s;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i] >= L'A' && out[i] <= L'Z') {
            out[i] = static_cast<wchar_t>(out[i] + (L'a' - L'A'));
        }
    }
    return out;
}

// ------------------------------------------------------------
// GuideTour
// ------------------------------------------------------------
Rect ClampAnchor(const Rect& client, const Rect& anchor)
{
    const int l = (std::max)(client.left, anchor.left);
    const int t = (std::max)(client.top, anchor.top);
    const int r = (std::min)(client.right, anchor.right);
    const int b = (std::min)(client.bottom, anchor.bottom);
    return Rect(l, t, (std::max)(l, r), (std::max)(t, b));
}

void PaintSmallButton(IPainter& g, const Rect& rc, const wchar_t* label,
                      bool primary, bool hovered, const Palette& pal,
                      const Metrics& mx)
{
    g.FillRound(rc, mx.radiusSm,
                primary ? pal.primary : hovered ? pal.hover : pal.sunken);
    g.StrokeRound(rc, mx.radiusSm,
                  primary ? pal.primaryDown : pal.border, 1);
    g.Text(rc, label, FontRole::Meta,
           primary ? pal.textInverse : pal.text, kCenter);
}

} // namespace







std::vector<int> CommandPaletteMatches(const CommandPaletteState& st)
{
    std::vector<int> out;
    const std::wstring q = LowerAscii(st.query);
    for (int i = 0; i < static_cast<int>(st.items.size()); ++i) {
        const CommandItem& item = st.items[static_cast<std::size_t>(i)];
        if (q.empty()) {
            out.push_back(i);
            continue;
        }
        const std::wstring hay = LowerAscii(item.title + L" " + item.hint +
                                           L" " + item.group);
        if (hay.find(q) != std::wstring::npos) out.push_back(i);
    }
    return out;
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

SnackbarLayout Snackbar::Measure(const Metrics& mx) const
{
    const Rect& client = Bounds();
    const SnackbarState& st = state;
    (void)mx;
    SnackbarLayout out;
    if (client.Empty()) return out;
    const int pad = mx.unit * 2;
    const int w = (std::min)(client.Width() - pad * 2, mx.unit * 60);
    const int h = mx.rowH + mx.unit * 2;
    if (w <= 0 || h <= 0 || client.Height() <= h + pad) return out;
    const int l = client.left + (client.Width() - w) / 2;
    const int t = client.bottom - pad - h;
    out.card = Rect(l, t, l + w, t + h);
    const Rect inner = InsetR(out.card, mx.unit);
    const int closeW = st.closable ? mx.unit * 4 : 0;
    const int actionW = st.action.empty()
                      ? 0
                      : static_cast<int>(st.action.size()) * mx.fontMeta + mx.unit * 3;
    if (closeW > 0) {
        out.closeRc = Rect(inner.right - closeW, inner.top,
                           inner.right, inner.bottom);
    }
    const int rightEdge = closeW > 0 ? out.closeRc.left : inner.right;
    if (actionW > 0) {
        out.actionRc = Rect(rightEdge - actionW - mx.unit / 2,
                            inner.top,
                            rightEdge - mx.unit / 2,
                            inner.bottom);
    }
    const int textR = out.actionRc.Empty() ? inner.right - closeW
                                           : out.actionRc.left - mx.unit / 2;
    out.textRc = Rect(inner.left, inner.top,
                      (std::max)(inner.left, textR), inner.bottom);
    return out;
}

SnackbarHit Snackbar::HitAt(const Metrics& mx, Point pt) const
{
    const Rect& client = Bounds();
    const SnackbarState& st = state;
    (void)mx;
    (void)pt;
    const SnackbarLayout lay = MeasureSnackbar(client, st, mx);
    if (!RectHit(lay.card, pt)) return SnackbarHit::None;
    if (RectHit(lay.closeRc, pt)) return SnackbarHit::Close;
    if (RectHit(lay.actionRc, pt)) return SnackbarHit::Action;
    return SnackbarHit::None;
}

void Snackbar::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& client = Bounds();
    SnackbarState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (client.Empty()) return;
    const SnackbarLayout lay = MeasureSnackbar(client, st, mx);
    if (lay.card.Empty()) return;
    g.PushClip(client);
    // The bar uses inverted semantic colors so it stays legible in both
    // the light and the dark palette without any hardcoded color.
    g.FillRound(lay.card, mx.radiusMd, pal.text);
    g.FillRect(Rect(lay.card.left, lay.card.top,
                    lay.card.left + (std::max)(2, mx.unit / 3), lay.card.bottom),
               AccentFor(st.kind, pal));
    g.Text(lay.textRc, st.message, FontRole::Body, pal.textInverse, kLeft);
    if (!lay.actionRc.Empty()) {
        if (st.actionHover) {
            g.FillRectAlpha(lay.actionRc, pal.textInverse, 40);
        }
        g.Text(lay.actionRc, st.action, FontRole::Title, pal.textInverse, kCenter);
    }
    if (!lay.closeRc.Empty()) {
        if (st.closeHover) {
            g.FillRectAlpha(lay.closeRc, pal.textInverse, 40);
        }
        g.Text(lay.closeRc, L"\x00D7", FontRole::Meta, pal.textInverse, kCenter);
    }
    g.PopClip();
}

CommandPaletteLayout CommandPalette::Measure(const Metrics& mx) const
{
    const Rect& client = Bounds();
    const CommandPaletteState& st = state;
    (void)mx;
    CommandPaletteLayout out;
    if (client.Empty()) return out;
    const int pad = mx.unit * 2;
    const int w = (std::min)(client.Width() - pad * 2, mx.unit * 72);
    const int h = (std::min)(client.Height() - pad * 2, mx.unit * 52);
    if (w <= mx.unit * 20 || h <= mx.rowH * 4) return out;
    const int l = client.left + (client.Width() - w) / 2;
    const int t = client.top + (client.Height() - h) / 4;
    out.panel = Rect(l, t, l + w, t + h);
    const Rect inner = InsetR(out.panel, mx.unit);
    const int searchH = mx.rowH + mx.unit;
    out.search = Rect(inner.left, inner.top, inner.right, inner.top + searchH);
    out.footer = Rect(inner.left, inner.bottom - mx.rowH,
                      inner.right, inner.bottom);
    out.list = Rect(inner.left, out.search.bottom + mx.unit / 2,
                    inner.right, out.footer.top - mx.unit / 2);
    out.rowH = mx.rowH + mx.unit / 2;
    if (out.list.Empty() || out.rowH <= 0) return out;
    const std::vector<int> matches = CommandPaletteMatches(st);
    for (std::size_t k = 0; k < matches.size(); ++k) {
        const int y = out.list.top + static_cast<int>(k) * out.rowH;
        if (y + out.rowH > out.list.bottom) break;
        out.rows.push_back(Rect(out.list.left, y, out.list.right, y + out.rowH));
    }
    return out;
}

CommandPaletteHit CommandPalette::HitAt(const Metrics& mx, Point pt) const
{
    const Rect& client = Bounds();
    const CommandPaletteState& st = state;
    (void)mx;
    (void)pt;
    CommandPaletteHit h;
    const CommandPaletteLayout lay = MeasureCommandPalette(client, st, mx);
    if (!RectHit(lay.panel, pt)) return h;
    if (RectHit(lay.search, pt)) {
        h.search = true;
        return h;
    }
    if (!RectHit(lay.list, pt) || lay.rowH <= 0) return h;
    const int k = (pt.y - lay.list.top) / lay.rowH;
    if (k < 0 || k >= static_cast<int>(lay.rows.size())) return h;
    const std::vector<int> matches = CommandPaletteMatches(st);
    if (k >= static_cast<int>(matches.size())) return h;
    h.row = matches[static_cast<std::size_t>(k)];
    return h;
}

void CommandPalette::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& client = Bounds();
    CommandPaletteState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (Hovered()) st.base.hover = true;
    if (Focused()) st.base.focused = true;
    if (client.Empty()) return;
    const CommandPaletteLayout lay = MeasureCommandPalette(client, st, mx);
    if (lay.panel.Empty()) return;
    const std::vector<int> matches = CommandPaletteMatches(st);
    g.PushClip(client);
    g.FillRectAlpha(client, pal.sidebar, 130);
    g.FillRound(lay.panel, mx.radiusCard, pal.surface);
    g.StrokeRound(lay.panel, mx.radiusCard, pal.split, 1);
    g.FillRound(lay.search, mx.radiusMd, pal.sunken);
    g.StrokeRound(lay.search, mx.radiusMd, pal.selectRing, 1);
    g.Text(Rect(lay.search.left + mx.unit, lay.search.top,
                lay.search.right - mx.unit, lay.search.bottom),
           st.query.empty() ? st.placeholder : st.query,
           FontRole::Body, st.query.empty() ? pal.textFaint : pal.text, kLeft);
    g.PushClip(lay.list);
    for (std::size_t k = 0; k < lay.rows.size(); ++k) {
        if (k >= matches.size()) break;
        const CommandItem& item = st.items[static_cast<std::size_t>(matches[k])];
        const Rect& row = lay.rows[k];
        const bool active = st.selected == static_cast<int>(k) ||
                            st.hovered == static_cast<int>(k);
        g.FillRound(row, mx.radiusSm, active ? pal.selectFill : pal.surface);
        if (active) {
            g.FillRect(Rect(row.left, row.top,
                            row.left + (std::max)(2, mx.unit / 3), row.bottom),
                       pal.primary);
        }
        const int shortcutW = item.shortcut.empty() ? 0 : mx.unit * 9;
        const int hintW = item.hint.empty() ? 0 : mx.unit * 14;
        const int textR = row.right - mx.unit -
                          (std::max)(shortcutW, 0) - (std::max)(hintW, 0);
        g.Text(Rect(row.left + mx.unit, row.top,
                    (std::max)(row.left + mx.unit, textR), row.bottom),
               item.title, FontRole::Body,
               item.disabled ? pal.textDisabled : pal.text, kLeft);
        if (hintW > 0) {
            g.Text(Rect(textR, row.top,
                        (std::min)(row.right - mx.unit - shortcutW, textR + hintW),
                        row.bottom),
                   item.hint, FontRole::Meta, pal.textFaint, kRight);
        }
        if (shortcutW > 0) {
            g.Text(Rect(row.right - mx.unit - shortcutW, row.top,
                        row.right - mx.unit, row.bottom),
                   item.shortcut, FontRole::Meta, pal.textFaint, kRight);
        }
    }
    g.PopClip();
    g.Text(lay.footer,
           L"方向键选择  ·  Enter 执行  ·  Esc 关闭  ·  共 " +
               std::to_wstring(matches.size()) + L" 项",
           FontRole::Meta, pal.textFaint, kLeft);
    g.PopClip();
}

GuideTourLayout GuideTour::Measure(const Metrics& mx) const
{
    const Rect& client = Bounds();
    const GuideTourState& st = state;
    (void)mx;
    GuideTourLayout out;
    (void)st;
    if (client.Empty()) return out;
    const int pad = mx.unit;
    const int w = (std::min)(client.Width() - pad * 2, mx.unit * 40);
    const int h = (std::min)(client.Height() - pad * 2, mx.unit * 22);
    if (w <= mx.unit * 16 || h <= mx.rowH * 4) return out;
    const Rect a = ClampAnchor(client, anchor);
    int top = a.Empty() ? client.top + pad : a.bottom + mx.unit * 2;
    if (top + h > client.bottom - pad) {
        top = a.Empty() ? client.top + pad : a.top - mx.unit * 2 - h;
    }
    top = ClampI(top, client.top + pad,
                 (std::max)(client.top + pad, client.bottom - pad - h));
    int left = a.Empty() ? client.left + (client.Width() - w) / 2 : a.left;
    left = ClampI(left, client.left + pad,
                  (std::max)(client.left + pad, client.right - pad - w));
    out.card = Rect(left, top, left + w, top + h);
    const Rect inner = InsetR(out.card, mx.unit);
    out.titleRc = Rect(inner.left, inner.top, inner.right, inner.top + mx.rowH);
    const int bodyH = (std::max)(mx.rowH, mx.unit * 5);
    out.bodyRc = Rect(inner.left, out.titleRc.bottom, inner.right,
                      out.titleRc.bottom + bodyH);
    const int btnH = mx.rowH;
    const int btnW = mx.unit * 9;
    const int btnTop = inner.bottom - btnH;
    out.skipRc = Rect(inner.left, btnTop, inner.left + btnW, inner.bottom);
    out.prevRc = Rect(inner.right - btnW * 2 - mx.unit, btnTop,
                      inner.right - btnW - mx.unit, inner.bottom);
    out.nextRc = Rect(inner.right - btnW, btnTop, inner.right, inner.bottom);
    out.dotsRc = Rect(inner.left, btnTop - mx.unit * 2,
                      inner.right, btnTop - mx.unit / 2);
    return out;
}

GuideTourHit GuideTour::HitAt(const Metrics& mx, Point pt) const
{
    const Rect& client = Bounds();
    const GuideTourState& st = state;
    (void)mx;
    (void)pt;
    const GuideTourLayout lay = MeasureGuideTour(client, anchor, st, mx);
    if (!RectHit(lay.card, pt)) return GuideTourHit::None;
    if (RectHit(lay.nextRc, pt)) return GuideTourHit::Next;
    if (RectHit(lay.prevRc, pt)) return GuideTourHit::Prev;
    if (RectHit(lay.skipRc, pt)) return GuideTourHit::Skip;
    return GuideTourHit::None;
}

void GuideTour::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    const Rect& client = Bounds();
    GuideTourState& st = state;
    (void)g;
    (void)pal;
    (void)mx;
    if (client.Empty()) return;
    const GuideTourLayout lay = MeasureGuideTour(client, anchor, st, mx);
    if (lay.card.Empty()) return;
    const Rect a = ClampAnchor(client, anchor);
    // When the card has to be clamped on top of the anchor, highlighting
    // the anchor would leave a stray corner peeking out from under it.
    const bool overlaps = !a.Empty() &&
        !(a.right <= lay.card.left || a.left >= lay.card.right ||
          a.bottom <= lay.card.top || a.top >= lay.card.bottom);
    g.PushClip(client);
    if (a.Empty() || overlaps) {
        g.FillRectAlpha(client, pal.sidebar, 150);
    } else {
        g.FillRectAlpha(Rect(client.left, client.top, client.right, a.top),
                        pal.sidebar, 150);
        g.FillRectAlpha(Rect(client.left, a.bottom, client.right, client.bottom),
                        pal.sidebar, 150);
        g.FillRectAlpha(Rect(client.left, a.top, a.left, a.bottom),
                        pal.sidebar, 150);
        g.FillRectAlpha(Rect(a.right, a.top, client.right, a.bottom),
                        pal.sidebar, 150);
        g.FillRectAlpha(a, pal.primary, 30);
        g.StrokeRect(a, pal.primary, 2);
    }
    g.FillRound(lay.card, mx.radiusCard, pal.surface);
    g.StrokeRound(lay.card, mx.radiusCard, pal.split, 1);
    g.Text(lay.titleRc, st.title, FontRole::Title, pal.text, kLeft);
    g.TextBlock(lay.bodyRc, st.body, FontRole::Meta, pal.textSub, 2);
    const int total = ClampI(st.total, 1, 12);
    const int dot = (std::max)(4, mx.unit);
    const int dotPitch = dot + mx.unit;
    for (int i = 0; i < total; ++i) {
        const int x = lay.dotsRc.left + i * dotPitch;
        if (x + dot > lay.dotsRc.right) break;
        const int y = lay.dotsRc.top + (lay.dotsRc.Height() - dot) / 2;
        g.FillRound(Rect(x, y, x + dot, y + dot), dot / 2,
                    i + 1 == st.step ? pal.primary : pal.split);
    }
    g.Text(Rect(lay.dotsRc.left, lay.dotsRc.top,
                lay.dotsRc.right, lay.dotsRc.bottom),
           std::to_wstring((std::max)(1, st.step)) + L"/" +
               std::to_wstring(total),
           FontRole::Meta, pal.textFaint, kRight);
    PaintSmallButton(g, lay.skipRc, L"跳过", false, st.skipHover, pal, mx);
    PaintSmallButton(g, lay.prevRc, L"上一步", false,
                     st.prevHover && st.step > 1, pal, mx);
    if (st.step <= 1) {
        g.FillRectAlpha(lay.prevRc, pal.sunken, 140);
    }
    const wchar_t* nextLabel = st.step >= total ? L"完成" : L"下一步";
    PaintSmallButton(g, lay.nextRc, nextLabel, true, st.nextHover, pal, mx);
    g.PopClip();
}

} // namespace widgets
} // namespace ui
} // namespace air
