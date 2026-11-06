// ============================================================
// AirUI - ControlsFeedback.cpp
// Platform-free feedback / overlay controls.
//
//   Spinner / Tooltip / Popover / Modal / Drawer /
//   NotificationQueue / Result
//
// Every control is a pure function of (rect, state, palette,
// metrics) and reaches the backend only through IPainter. The
// Measure* helpers hold the geometry shared by painting and by
// pointer hit testing, so a backend never duplicates it.
// ============================================================
#include "airui/ControlsExtra.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace air {
namespace ui {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Every single-line Text() call carries NoPrefix plus an alignment;
// long strings also carry EndEllipsis so they never spill their rect.
constexpr TextFormat kLeftText =
    dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kCenterText =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kGlyphText =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix;

// Semantic palette derived from an AlertKind.
struct SemanticColors
{
    Color accent;
    Color soft;
    Color text;
};

SemanticColors ColorsFor(AlertKind kind, const Palette& pal)
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

// Shared framed card: surface fill with a split border.
void PaintFrame(IPainter& g, const Rect& rc, const Palette& pal, const Metrics& mx)
{
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
}

// Clip a rectangle into a bounding rect. The result may be empty;
// callers must test before drawing.
Rect ClampRect(const Rect& r, const Rect& bounds)
{
    if (bounds.Empty() || r.Empty()) return Rect{};
    Rect out = r;
    out.left   = (std::max)(out.left, bounds.left);
    out.top    = (std::max)(out.top, bounds.top);
    out.right  = (std::min)(out.right, bounds.right);
    out.bottom = (std::min)(out.bottom, bounds.bottom);
    if (out.right < out.left)   out.right  = out.left;
    if (out.bottom < out.top)   out.bottom = out.top;
    return out;
}

// A focused modal button is reported through ControlState.hover so
// PaintButton highlights it in addition to the focus ring.
ControlState FocusedButton(int which, int focused)
{
    ControlState cs;
    cs.focused = (focused == which);
    cs.hover   = (focused == which);
    return cs;
}

// Shared geometry for ResultActionHit and PaintResult: the two must
// always agree on the action rectangle.
struct ResultGeom
{
    Rect glyph{}, title{}, detail{}, action{};
};

ResultGeom MeasureResult(const Rect& rc, const ResultState& st, const Metrics& mx)
{
    ResultGeom gm;
    if (rc.Empty()) return gm;
    const int pad = mx.unit;
    if (rc.Width() <= pad * 2 || rc.Height() <= pad * 2) return gm;

    const int textW = rc.Width() - pad * 2;
    int glyphSize = mx.rowH * 2;
    const int titleH = mx.rowH;
    int detailH = mx.rowH;
    const int btnH = mx.btnH;
    int btnW = (std::max)(mx.unit * 14,
                          static_cast<int>(st.actionText.size()) * mx.unit + mx.unit * 4);
    btnW = (std::min)(btnW, textW);

    // The gallery gives Result a compact box. Shrink the decorative
    // glyph and gaps first, but never compress the action button:
    // a clipped button paints its label over the detail text.
    int gap = pad;
    const int avail = rc.Height() - pad * 2;
    int totalH = glyphSize + gap + titleH + detailH + gap + btnH;
    if (totalH > avail) {
        glyphSize = (std::max)(mx.rowH / 2, glyphSize - (totalH - avail));
        gap = (std::max)(2, gap / 2);
        totalH = glyphSize + gap + titleH + detailH + gap + btnH;
        if (totalH > avail) {
            detailH = (std::max)(mx.rowH / 2, detailH - (totalH - avail));
            totalH = glyphSize + gap + titleH + detailH + gap + btnH;
        }
    }
    int y = rc.top + (rc.Height() - totalH) / 2;
    if (y < rc.top + pad) y = rc.top + pad;
    const int cx = rc.left + rc.Width() / 2;

    gm.glyph = Rect(cx - glyphSize / 2, y,
                    cx - glyphSize / 2 + glyphSize, y + glyphSize);
    y += glyphSize + gap;
    gm.title = Rect(cx - textW / 2, y, cx + textW / 2, y + titleH);
    y += titleH;
    gm.detail = Rect(cx - textW / 2, y, cx + textW / 2, y + detailH);
    y += detailH + gap;
    gm.action = Rect(cx - btnW / 2, y, cx + btnW / 2, y + btnH);
    if (gm.action.bottom > rc.bottom - pad) {
        const int shift = gm.action.bottom - (rc.bottom - pad);
        gm.action.top -= shift;
        gm.action.bottom -= shift;
    }
    gm.action = ClampRect(gm.action, rc);
    return gm;
}

} // namespace

// ============================================================
// Spinner
// ============================================================

void PaintSpinner(IPainter& g, const Rect& rc, const SpinnerState& st,
                  const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;

    int n = st.dots;
    if (n < 1) n = 1;
    const int dot = (std::max)(6, mx.unit);
    const int labelH = st.label.empty() ? 0 : mx.rowH;
    const int areaH = rc.Height() - labelH;
    const int cx = rc.left + rc.Width() / 2;
    const int cy = rc.top + (areaH > 0 ? areaH : rc.Height()) / 2;
    const int span = (std::min)(rc.Width(), areaH > 0 ? areaH : rc.Height());
    const int R = (std::max)(mx.unit * 3, span / 2 - dot);

    float ph = st.phase;
    if (ph < 0.f) ph = 0.f;
    if (ph > 1.f) ph = 1.f;
    int active = static_cast<int>(ph * n);
    if (active >= n) active = n - 1;

    // Every dot is drawn in the accent colour; the alpha falloff is
    // what creates the rotation. Using semantic pale colours instead
    // made the trailing dots disappear on a light surface.
    const Color dotColor = st.base.disabled ? pal.border : pal.primary;

    for (int i = 0; i < n; ++i) {
        const double ang = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(n);
        const int px = cx + static_cast<int>(static_cast<double>(R) * std::cos(ang));
        const int py = cy + static_cast<int>(static_cast<double>(R) * std::sin(ang));
        const Rect dotRc(px - dot / 2, py - dot / 2,
                         px - dot / 2 + dot, py - dot / 2 + dot);

        int d = i - active;
        if (d < 0) d = -d;
        const int dist = (std::min)(d, n - d);
        const uint8_t alpha = dist == 0 ? 255
                            : dist == 1 ? 205
                            : dist == 2 ? 155
                            : 105;
        g.FillRectAlpha(dotRc, dotColor, alpha);
    }

    if (!st.label.empty()) {
        const Rect labelRc(rc.left, rc.top + (areaH > 0 ? areaH : 0),
                           rc.right, rc.bottom);
        if (!labelRc.Empty()) {
            const Color c = st.base.disabled ? pal.textDisabled : pal.textSub;
            g.Text(labelRc, st.label, FontRole::Meta, c, kCenterText);
        }
    }
}

// ============================================================
// Tooltip / Popover
// ============================================================

void PaintTooltip(IPainter& g, const Rect& anchor, const Rect& client,
                  const TooltipState& st, const Palette& pal, const Metrics& mx)
{
    if (client.Empty()) return;
    // Size the tooltip to its text so long hints are not ellipsised.
    const int measured = g.MeasureText(st.text, FontRole::Body);
    const int w = (std::min)(client.Width(),
                             (std::max)(mx.unit * 16, measured + mx.unit * 4));
    const int h = mx.rowH + mx.unit * 2;

    Rect card = PlaceOverlay(anchor, Size{w, h}, client, st.placement);
    card = ClampRect(card, client);
    if (card.Empty()) return;

    PaintFrame(g, card, pal, mx);
    const Rect textRc(card.left + mx.unit, card.top,
                      card.right - mx.unit, card.bottom);
    if (!textRc.Empty())
        g.Text(textRc, st.text, FontRole::Body, pal.text, kLeftText);
}

void PaintPopover(IPainter& g, const Rect& anchor, const Rect& client,
                  const PopoverState& st, const Palette& pal, const Metrics& mx)
{
    if (client.Empty()) return;
    const int w = (std::min)(client.Width(), mx.unit * 34);
    const int h = mx.rowH * 4 + mx.unit * 2;

    Rect card = PlaceOverlay(anchor, Size{w, h}, client, OverlayPlacement::Below);
    card = ClampRect(card, client);
    if (card.Empty()) return;

    PaintFrame(g, card, pal, mx);
    g.PushClip(Rect(card.left + 1, card.top + 1,
                    card.right - 1, card.bottom - 1));

    const int pad = mx.unit;
    const int left = card.left + pad;
    const int right = card.right - pad;
    const Rect titleRc(left, card.top + pad, right, card.top + pad + mx.rowH);
    if (!titleRc.Empty())
        g.Text(titleRc, st.title, FontRole::Title, pal.text, kLeftText);
    g.FillRect(Rect(left, titleRc.bottom, right, titleRc.bottom + 1), pal.split);

    const Rect bodyRc(left, titleRc.bottom + pad / 2, right, card.bottom - pad);
    if (!bodyRc.Empty())
        g.TextBlock(bodyRc, st.body, FontRole::Body, pal.textSub, 3);

    g.PopClip();
}

// ============================================================
// Modal
// ============================================================

ModalLayout MeasureModal(const Rect& client, const ModalState& st, const Metrics& mx)
{
    ModalLayout out;
    if (client.Empty()) return out;
    const int pad = mx.unit;
    const int availW = client.Width() - pad * 2;
    const int availH = client.Height() - pad * 2;
    if (availW <= 0 || availH <= 0) return out;

    int cardW = (std::min)(mx.unit * 45, availW);
    int cardH = mx.rowH * 4 + mx.unit * 4;
    if (!st.body.empty()) cardH += mx.rowH * 2;
    cardH = (std::min)(cardH, availH);
    if (cardW <= pad * 2 || cardH <= mx.rowH * 2) return out;

    const int cx = client.left + (client.Width() - cardW) / 2;
    const int cy = client.top + (client.Height() - cardH) / 2;
    out.card = Rect(cx, cy, cx + cardW, cy + cardH);

    const int left = out.card.left + pad;
    const int right = out.card.right - pad;
    const int titleTop = out.card.top + pad;
    out.titleRc = Rect(left, titleTop, right, titleTop + mx.rowH);

    const int btnH = mx.btnH;
    const int btnW = mx.unit * 12;
    int btnY = out.card.bottom - pad - btnH;
    if (btnY < out.titleRc.bottom) btnY = out.titleRc.bottom;
    const int gap = mx.unit;
    out.cancelRc = Rect(right - btnW, btnY, right, btnY + btnH);
    out.confirmRc = Rect(out.cancelRc.left - gap - btnW, btnY,
                         out.cancelRc.left - gap, btnY + btnH);

    const int bodyTop = out.titleRc.bottom + pad / 2;
    const int bodyBottom = btnY - pad;
    if (bodyBottom > bodyTop)
        out.bodyRc = Rect(left, bodyTop, right, bodyBottom);
    return out;
}

int ModalButtonAt(const Rect& client, const ModalState& st,
                  const Metrics& mx, Point pt)
{
    const ModalLayout lay = MeasureModal(client, st, mx);
    if (lay.card.Empty()) return 0;
    if (lay.confirmRc.Contains(pt)) return 1;
    if (lay.cancelRc.Contains(pt)) return 2;
    return 0;
}

void PaintModal(IPainter& g, const Rect& client, const ModalState& st,
                const Palette& pal, const Metrics& mx)
{
    if (client.Empty()) return;
    const ModalLayout lay = MeasureModal(client, st, mx);
    if (lay.card.Empty()) return;

    // Alpha-blended scrim must be the first draw call, never an
    // opaque full-client fill.
    if (st.scrim) PaintOverlayScrim(g, client, pal.bg, 96);

    PaintFrame(g, lay.card, pal, mx);
    g.PushClip(Rect(lay.card.left + 1, lay.card.top + 1,
                    lay.card.right - 1, lay.card.bottom - 1));

    if (!lay.titleRc.Empty())
        g.Text(lay.titleRc, st.title, FontRole::Title, pal.text, kLeftText);
    g.FillRect(Rect(lay.card.left + mx.unit, lay.titleRc.bottom,
                    lay.card.right - mx.unit, lay.titleRc.bottom + 1), pal.split);
    if (!lay.bodyRc.Empty())
        g.TextBlock(lay.bodyRc, st.body, FontRole::Body, pal.textSub, 3);

    PaintButton(g, lay.confirmRc, st.confirmText, ButtonStyle::Primary,
                FocusedButton(1, st.focused), pal, mx);
    PaintButton(g, lay.cancelRc, st.cancelText, ButtonStyle::Secondary,
                FocusedButton(2, st.focused), pal, mx);

    if (st.focused == 1)
        PaintFocusRing(g, lay.confirmRc, pal, mx);
    else if (st.focused == 2)
        PaintFocusRing(g, lay.cancelRc, pal, mx);

    g.PopClip();
}

// ============================================================
// Drawer
// ============================================================

DrawerLayout MeasureDrawer(const Rect& client, const DrawerState& st, const Metrics& mx)
{
    DrawerLayout out;
    if (client.Empty()) return out;
    const int pad = mx.unit;
    const int w = (std::min)((std::max)(st.width, pad * 4), client.Width());
    if (w <= pad * 2) return out;

    out.panel = st.right
        ? Rect(client.right - w, client.top, client.right, client.bottom)
        : Rect(client.left, client.top, client.left + w, client.bottom);

    const int left = out.panel.left + pad;
    const int right = out.panel.right - pad;
    const int box = mx.rowH - pad;
    out.closeRc = Rect(right - box, out.panel.top + pad,
                       right, out.panel.top + pad + box);

    const int titleRight = out.closeRc.left - pad;
    if (titleRight > left)
        out.titleRc = Rect(left, out.panel.top + pad,
                           titleRight, out.panel.top + pad + mx.rowH);

    const int bodyTop = out.panel.top + pad + mx.rowH + pad;
    const int bodyBottom = out.panel.bottom - pad;
    if (bodyBottom > bodyTop)
        out.bodyRc = Rect(left, bodyTop, right, bodyBottom);
    return out;
}

bool DrawerCloseHit(const Rect& client, const DrawerState& st,
                    const Metrics& mx, Point pt)
{
    const DrawerLayout lay = MeasureDrawer(client, st, mx);
    if (lay.panel.Empty()) return false;
    return lay.closeRc.Contains(pt);
}

void PaintDrawer(IPainter& g, const Rect& client, const DrawerState& st,
                 const Palette& pal, const Metrics& mx)
{
    if (client.Empty()) return;
    const DrawerLayout lay = MeasureDrawer(client, st, mx);
    if (lay.panel.Empty()) return;

    if (st.scrim) PaintOverlayScrim(g, client, pal.bg, 96);

    g.FillRect(lay.panel, pal.surface);
    const Rect edge = st.right
        ? Rect(lay.panel.left, lay.panel.top, lay.panel.left + 1, lay.panel.bottom)
        : Rect(lay.panel.right - 1, lay.panel.top, lay.panel.right, lay.panel.bottom);
    g.FillRect(edge, pal.split);

    g.PushClip(Rect(lay.panel.left + 1, lay.panel.top + 1,
                    lay.panel.right - 1, lay.panel.bottom - 1));

    if (!lay.titleRc.Empty())
        g.Text(lay.titleRc, st.title, FontRole::Title, pal.text, kLeftText);
    const int headerBottom = lay.panel.top + mx.unit + mx.rowH;
    g.FillRect(Rect(lay.panel.left + mx.unit, headerBottom,
                    lay.panel.right - mx.unit, headerBottom + 1), pal.split);
    if (!lay.bodyRc.Empty())
        g.TextBlock(lay.bodyRc, st.body, FontRole::Body, pal.textSub, 4);

    if (!lay.closeRc.Empty()) {
        g.FillRound(lay.closeRc, mx.radiusSm, pal.sunken);
        g.StrokeRound(lay.closeRc, mx.radiusSm, pal.split, 1);
        g.Text(lay.closeRc, L"\u00D7", FontRole::Body, pal.textSub, kGlyphText);
    }

    g.PopClip();
}

// ============================================================
// Notification queue
// ============================================================

void PaintNotificationQueue(IPainter& g, const Rect& client,
                            const NotificationState& st,
                            const Palette& pal, const Metrics& mx)
{
    if (client.Empty()) return;
    const int count = (std::min)((std::max)(st.maxVisible, 0),
                                 static_cast<int>(st.items.size()));
    if (count <= 0) return;

    const int pad = mx.unit;
    const int w = (std::min)(mx.unit * 36, client.Width() - pad * 2);
    if (w <= 0) return;
    const int h = mx.rowH * 2;
    const int right = client.right - pad;
    const int left = right - w;

    g.PushClip(client);
    int y = client.top + pad;
    for (int i = 0; i < count; ++i) {
        const Rect toastRc(left, y, right, y + h);
        if (toastRc.bottom > client.bottom) break;
        if (!toastRc.Empty())
            PaintToast(g, toastRc, st.items[static_cast<std::size_t>(i)], pal, mx);
        y += h + pad;
    }
    g.PopClip();
}

// ============================================================
// Result
// ============================================================

bool ResultActionHit(const Rect& rc, const ResultState& st,
                     const Metrics& mx, Point pt)
{
    const ResultGeom gm = MeasureResult(rc, st, mx);
    if (gm.action.Empty()) return false;
    return gm.action.Contains(pt);
}

void PaintResult(IPainter& g, const Rect& rc, const ResultState& st,
                 const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const ResultGeom gm = MeasureResult(rc, st, mx);
    if (gm.glyph.Empty()) return;

    const SemanticColors colors = ColorsFor(st.kind, pal);

    g.PushClip(rc);

    g.FillRound(gm.glyph, mx.radiusCard, colors.soft);
    g.StrokeRound(gm.glyph, mx.radiusCard, colors.accent, 1);
    g.Text(gm.glyph, GlyphFor(st.kind), FontRole::Display,
           colors.accent, kGlyphText);

    if (!gm.title.Empty())
        g.Text(gm.title, st.title, FontRole::Title, pal.text, kCenterText);
    if (!gm.detail.Empty())
        g.Text(gm.detail, st.detail, FontRole::Body, pal.textSub, kCenterText);

    ControlState btn;
    btn.hover = st.hoverAction;
    if (!gm.action.Empty())
        PaintButton(g, gm.action, st.actionText, ButtonStyle::Primary, btn, pal, mx);

    g.PopClip();
}

} // namespace ui
} // namespace air
