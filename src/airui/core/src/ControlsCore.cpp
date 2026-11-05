#include "airui/ControlsCore.h"

namespace air {
namespace ui {

namespace {

// Shared geometry for the option controls.
constexpr int kBoxTop    = 4;    // box offset from the row top
constexpr int kBoxSize   = 16;   // visual box / circle diameter
constexpr int kLabelGap  = 24;   // label starts here
constexpr int kBoxRadius = 3;    // checkbox corner radius
constexpr int kRadioR    = 8;    // radio radius (16 / 2)
constexpr int kDotInset  = 4;    // checked dot inset
constexpr int kDotR      = 4;    // checked dot radius

constexpr TextFormat kButtonText =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix;
constexpr TextFormat kGlyphText =
    dt::SingleLine | dt::Center | dt::VCenter;
constexpr TextFormat kLabelText =
    dt::SingleLine | dt::Left | dt::VCenter;

Rect OptionBox(const Rect& rc)
{
    return Rect(rc.left, rc.top + kBoxTop,
                rc.left + kBoxSize, rc.top + kBoxTop + kBoxSize);
}

Rect OptionLabel(const Rect& rc)
{
    return Rect(rc.left + kLabelGap, rc.top, rc.right, rc.bottom);
}

} // namespace

void PaintButton(IPainter& g,
                 const Rect& rc,
                 const std::wstring& text,
                 ButtonStyle style,
                 const ControlState& st,
                 const Palette& pal,
                 const Metrics& mx)
{
    switch (style) {
    case ButtonStyle::Primary: {
        Color fill = pal.primary;
        if (st.disabled)      fill = pal.border;
        else if (st.pressed)  fill = pal.primaryDown;
        else if (st.hover)    fill = pal.primaryHover;

        g.FillRound(rc, mx.radiusMd, fill);
        g.Text(rc, text, FontRole::Button, pal.textInverse, kButtonText);
        break;
    }
    case ButtonStyle::Secondary: {
        const Color fill = (st.pressed || st.hover) ? pal.active : pal.surface;

        g.FillRound(rc, mx.radiusMd, fill);
        g.StrokeRound(rc, mx.radiusMd,
                      st.hover ? pal.hoverBorder : pal.border, 1);
        g.Text(rc, text, FontRole::Button,
               st.disabled ? pal.textDisabled : pal.textSub, kButtonText);
        break;
    }
    case ButtonStyle::Danger: {
        const Color fill = st.pressed ? pal.errorSoft
                                      : (st.hover ? pal.errorSoft : pal.surface);

        g.FillRound(rc, mx.radiusMd, fill);
        g.StrokeRound(rc, mx.radiusMd,
                      st.disabled ? pal.border : pal.errorBorder, 1);
        g.Text(rc, text, FontRole::Button,
               st.disabled ? pal.textDisabled : pal.errorText, kButtonText);
        break;
    }
    case ButtonStyle::Ghost: {
        if (st.hover) {
            g.FillRound(rc, mx.radiusMd, pal.primarySoft);
        }
        g.Text(rc, text, FontRole::Button,
               st.disabled ? pal.textDisabled : pal.primaryText, kButtonText);
        break;
    }
    }
}

void PaintCheckbox(IPainter& g,
                   const Rect& rc,
                   const std::wstring& text,
                   const ControlState& st,
                   const Palette& pal)
{
    const Rect box = OptionBox(rc);

    g.FillRound(box, kBoxRadius, pal.surface);
    g.StrokeRound(box, kBoxRadius, pal.border, 1);

    if (st.checked) {
        g.Text(box, L"\u2713", FontRole::Body,
               st.disabled ? pal.textDisabled : pal.primary, kGlyphText);
    }
    g.Text(OptionLabel(rc), text, FontRole::Body,
           st.disabled ? pal.textDisabled : pal.text, kLabelText);
}

void PaintRadio(IPainter& g,
                const Rect& rc,
                const std::wstring& text,
                const ControlState& st,
                const Palette& pal)
{
    const Rect box = OptionBox(rc);

    g.FillRound(box, kRadioR, pal.surface);
    g.StrokeRound(box, kRadioR, pal.border, 1);

    if (st.checked) {
        const Rect dot(box.left + kDotInset, box.top + kDotInset,
                       box.right - kDotInset, box.bottom - kDotInset);
        g.FillRound(dot, kDotR,
                    st.disabled ? pal.textDisabled : pal.primary);
    }
    g.Text(OptionLabel(rc), text, FontRole::Body,
           st.disabled ? pal.textDisabled : pal.text, kLabelText);
}

void PaintProgressBar(IPainter& g,
                      const Rect& rc,
                      float progress,
                      const ControlState& st,
                      const Palette& pal,
                      const Metrics& mx)
{
    if (progress < 0.f) progress = 0.f;
    if (progress > 1.f) progress = 1.f;

    Color bg = st.disabled ? pal.surface : pal.sunken;
    g.FillRound(rc, mx.radiusMd, bg);

    int w = static_cast<int>((rc.right - rc.left) * progress);
    if (w > 0) {
        Rect fill{rc.left, rc.top, rc.left + w, rc.bottom};
        Color fillCol = st.disabled ? pal.border : pal.primary;
        g.FillRound(fill, mx.radiusMd, fillCol);
    }
    g.StrokeRound(rc, mx.radiusMd, pal.border, 1);
}

void PaintSlider(IPainter& g,
                 const Rect& rc,
                 const SliderState& st,
                 const Palette& pal,
                 const Metrics& mx)
{
    const int h = rc.bottom - rc.top;
    const int trackH = 4;
    const int trackY = rc.top + (h - trackH) / 2;
    Rect track{rc.left, trackY, rc.right, trackY + trackH};

    Color trackCol = st.base.disabled ? pal.border : pal.sunken;
    g.FillRound(track, trackH/2, trackCol);

    int range = st.max - st.min;
    float t = 0.f;
    if (range > 0) {
        t = static_cast<float>(st.value - st.min) / static_cast<float>(range);
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
    }
    int thumbX = rc.left + static_cast<int>((rc.right - rc.left) * t);
    Rect thumb{thumbX - 6, rc.top, thumbX + 6, rc.bottom};
    if (thumb.left < rc.left) thumb.left = rc.left;
    if (thumb.right > rc.right) thumb.right = rc.right;

    Color thumbCol = st.base.disabled ? pal.border : pal.surface;
    g.FillRound(thumb, 6, thumbCol);
    g.StrokeRound(thumb, 6, pal.border, 1);

    if (st.base.hover || st.base.pressed) {
        g.StrokeRound(track, trackH/2, pal.primary, 1);
    }
}

void PaintToggleSwitch(IPainter& g,
                       const Rect& rc,
                       const ToggleState& st,
                       const Palette& pal,
                       const Metrics& mx)
{
    const int h = rc.bottom - rc.top;
    const int w = rc.right - rc.left;
    const int pad = 2;
    Rect track{rc.left + pad, rc.top + pad, rc.right - pad, rc.bottom - pad};
    Color trackCol;
    if (st.base.disabled) trackCol = pal.border;
    else if (st.on) trackCol = pal.primary;
    else trackCol = pal.sunken;
    g.FillRound(track, h/2, trackCol);
    g.StrokeRound(track, h/2, pal.border, 1);

    int thumbSize = h - 4;
    int thumbX;
    if (st.base.disabled) thumbX = rc.left + 2;
    else if (st.on) thumbX = rc.right - thumbSize - 2;
    else thumbX = rc.left + 2;
    Rect thumb{thumbX, rc.top + 2, thumbX + thumbSize, rc.bottom - 2};
    Color thumbCol = st.base.disabled ? pal.border : pal.surface;
    g.FillRound(thumb, thumbSize/2, thumbCol);
    g.StrokeRound(thumb, thumbSize/2, pal.border, 1);
}

void PaintTextField(IPainter& g,
                    const Rect& rc,
                    const TextFieldState& st,
                    const Palette& pal,
                    const Metrics& mx)
{
    Color bg = st.base.disabled ? pal.sunken : pal.surface;
    g.FillRound(rc, mx.radiusMd, bg);
    Color border = st.base.disabled ? pal.border : (st.hasFocus ? pal.primary : pal.border);
    g.StrokeRound(rc, mx.radiusMd, border, 1);

    Rect inner{rc.left + 8, rc.top + 2, rc.right - 8, rc.bottom - 2};
    const std::wstring& txt = st.text.empty() ? st.placeholder : st.text;
    Color txtCol = st.base.disabled ? pal.textDisabled :
                   st.text.empty() ? pal.textDisabled : pal.text;
    g.Text(inner, txt, FontRole::Body, txtCol, dt::SingleLine | dt::Left | dt::VCenter);
}

void PaintSegmented(IPainter& g,
                    const Rect& rc,
                    const SegmentedState& st,
                    const Palette& pal,
                    const Metrics& mx)
{
    if (st.items.empty()) return;
    int count = static_cast<int>(st.items.size());
    int w = (rc.right - rc.left) / count;
    for (int i = 0; i < count; ++i) {
        Rect seg{rc.left + i * w, rc.top, rc.left + (i + 1) * w, rc.bottom};
        if (i == count - 1) seg.right = rc.right;
        bool selected = (i == st.selected);
        Color bg = st.base.disabled ? pal.sunken :
                   selected ? pal.primary : pal.surface;
        Color br = st.base.disabled ? pal.border : pal.border;
        g.FillRound(seg, mx.radiusMd, bg);
        g.StrokeRound(seg, mx.radiusMd, br, 1);
        Color txtCol = st.base.disabled ? pal.textDisabled :
                       selected ? pal.textInverse : pal.text;
        g.Text(seg, st.items[i], FontRole::Body, txtCol,
               dt::SingleLine | dt::Center | dt::VCenter);
    }
}

void PaintComboBox(IPainter& g,
                   const Rect& rc,
                   const ComboBoxState& st,
                   const Palette& pal,
                   const Metrics& mx)
{
    Color bg = st.base.disabled ? pal.sunken : pal.surface;
    g.FillRound(rc, mx.radiusMd, bg);
    Color border = st.base.disabled ? pal.border : (st.base.hover || st.open ? pal.primary : pal.border);
    g.StrokeRound(rc, mx.radiusMd, border, 1);

    Rect textR{rc.left + 8, rc.top, rc.right - 20, rc.bottom};
    Color txtCol = st.base.disabled ? pal.textDisabled : pal.text;
    g.Text(textR, st.text, FontRole::Body, txtCol, dt::SingleLine | dt::Left | dt::VCenter);

    Rect arrowR{rc.right - 18, rc.top + 4, rc.right - 4, rc.bottom - 4};
    g.Text(arrowR, L"\u25BE", FontRole::Body, txtCol, dt::SingleLine | dt::Center | dt::VCenter);
}

void PaintTag(IPainter& g,
              const Rect& rc,
              const TagState& st,
              const Palette& pal,
              const Metrics& mx)
{
    Color bg = st.base.disabled ? pal.sunken :
               st.selected ? pal.primarySoft : pal.surface;
    g.FillRound(rc, mx.radiusPill, bg);
    Color border = st.base.disabled ? pal.border : pal.border;
    g.StrokeRound(rc, mx.radiusPill, border, 1);
    Color txtCol = st.base.disabled ? pal.textDisabled :
                   st.selected ? pal.primaryText : pal.text;
    g.Text(rc, st.text, FontRole::Body, txtCol, dt::SingleLine | dt::Center | dt::VCenter);
}

void PaintRating(IPainter& g,
                 const Rect& rc,
                 const RatingState& st,
                 const Palette& pal,
                 const Metrics& mx)
{
    int starCount = st.max;
    int w = (rc.right - rc.left) / starCount;
    for (int i = 0; i < starCount; ++i) {
        Rect star{rc.left + i*w, rc.top, rc.left + (i+1)*w, rc.bottom};
        Color col = st.base.disabled ? pal.textDisabled :
                    i < st.rating ? pal.primary : pal.border;
        g.Text(star, L"\u2605", FontRole::Body, col, dt::SingleLine | dt::Center | dt::VCenter);
    }
}

void PaintStepper(IPainter& g,
                  const Rect& rc,
                  const StepperState& st,
                  const Palette& pal,
                  const Metrics& mx)
{
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int btnW = h;
    Rect leftBtn{rc.left, rc.top, rc.left + btnW, rc.bottom};
    Rect mid{rc.left + btnW, rc.top, rc.right - btnW, rc.bottom};
    Rect rightBtn{rc.right - btnW, rc.top, rc.right, rc.bottom};

    Color bg = st.base.disabled ? pal.sunken : pal.surface;
    g.FillRound(rc, mx.radiusMd, bg);
    g.StrokeRound(rc, mx.radiusMd, pal.border, 1);

    // left button
    g.FillRound(leftBtn, mx.radiusMd, st.minusHover ? pal.active : pal.surface);
    g.StrokeRound(leftBtn, mx.radiusMd, pal.border, 1);
    g.Text(leftBtn, L"\u2212", FontRole::Body, st.base.disabled ? pal.textDisabled : pal.text, dt::SingleLine | dt::Center | dt::VCenter);

    // right button
    g.FillRound(rightBtn, mx.radiusMd, st.plusHover ? pal.active : pal.surface);
    g.StrokeRound(rightBtn, mx.radiusMd, pal.border, 1);
    g.Text(rightBtn, L"+", FontRole::Body, st.base.disabled ? pal.textDisabled : pal.text, dt::SingleLine | dt::Center | dt::VCenter);

    // value
    std::wstring val = std::to_wstring(st.value);
    Color txtCol = st.base.disabled ? pal.textDisabled : pal.text;
    g.Text(mid, val, FontRole::Body, txtCol, dt::SingleLine | dt::Center | dt::VCenter);
}

void PaintProgressRing(IPainter& g,
                       const Rect& rc,
                       const ProgressRingState& st,
                       const Palette& pal,
                       const Metrics& mx)
{
    // Simple representation: filled arc via rect progress for now
    Color bg = st.base.disabled ? pal.sunken : pal.sunken;
    g.FillRound(rc, mx.radiusMd, bg);
    int w = static_cast<int>((rc.right - rc.left) * st.progress);
    if (w > 0) {
        Rect fill{rc.left, rc.top, rc.left + w, rc.bottom};
        Color fillCol = st.base.disabled ? pal.border : pal.primary;
        g.FillRound(fill, mx.radiusMd, fillCol);
    }
    g.StrokeRound(rc, mx.radiusMd, pal.border, 1);
}

void PaintBadge(IPainter& g,
                const Rect& rc,
                const BadgeState& st,
                const Palette& pal,
                const Metrics& mx)
{
    Color bg = st.base.disabled ? pal.border : pal.errorText;
    g.FillRound(rc, mx.radiusPill, bg);
    Color txtCol = pal.textInverse;
    g.Text(rc, st.text, FontRole::Meta, txtCol, dt::SingleLine | dt::Center | dt::VCenter);
}

void PaintAlert(IPainter& g,
                const Rect& rc,
                const AlertState& st,
                const Palette& pal,
                const Metrics& mx)
{
    Color bg, accent, txt;
    const wchar_t* icon;
    switch (st.kind) {
    case AlertKind::Success:
        bg = pal.okSoft; accent = pal.ok; txt = pal.okText; icon = L"\u2713"; break;
    case AlertKind::Warning:
        bg = pal.warnSoft; accent = pal.warn; txt = pal.warnText; icon = L"\u26A0"; break;
    case AlertKind::Error:
        bg = pal.errorSoft; accent = pal.error; txt = pal.errorText; icon = L"\u2717"; break;
    default:
        bg = pal.primarySoft; accent = pal.primary; txt = pal.primaryText; icon = L"\u2139"; break;
    }
    if (st.base.disabled) { bg = pal.sunken; accent = pal.border; txt = pal.textDisabled; }

    g.FillRound(rc, mx.radiusMd, bg);
    g.StrokeRound(rc, mx.radiusMd, accent, 1);

    // 左侧强调条
    Rect bar{rc.left, rc.top + 6, rc.left + 4, rc.bottom - 6};
    g.FillRound(bar, 2, accent);

    // 图标
    Rect iconRc{rc.left + 12, rc.top, rc.left + 36, rc.bottom};
    g.Text(iconRc, icon, FontRole::Icon, txt, dt::SingleLine | dt::Center | dt::VCenter);

    // 文本区
    const bool twoLine = !st.message.empty();
    Rect textRc{rc.left + 40, rc.top + 2, rc.right - 10, rc.bottom - 2};
    if (twoLine)
    {
        int half = (textRc.bottom - textRc.top) / 2;
        Rect t{textRc.left, textRc.top, textRc.right, textRc.top + half};
        Rect m{textRc.left, textRc.top + half, textRc.right, textRc.bottom};
        g.Text(t, st.title, FontRole::Body, txt, dt::SingleLine | dt::Left | dt::VCenter);
        g.Text(m, st.message, FontRole::Meta, st.base.disabled ? pal.textDisabled : pal.textSub,
               dt::SingleLine | dt::Left | dt::VCenter);
    }
    else
    {
        g.Text(textRc, st.title, FontRole::Body, txt, dt::SingleLine | dt::Left | dt::VCenter);
    }
}

void PaintTabs(IPainter& g,
               const Rect& rc,
               const TabsState& st,
               const Palette& pal,
               const Metrics& mx)
{
    if (st.tabs.empty()) return;
    const int n = static_cast<int>(st.tabs.size());
    const int w = (rc.right - rc.left) / n;

    // 底部基线
    g.FillRect(Rect{rc.left, rc.bottom - 1, rc.right, rc.bottom}, pal.split);

    for (int i = 0; i < n; ++i)
    {
        Rect tab{rc.left + i * w, rc.top, rc.left + (i + 1) * w, rc.bottom};
        if (i == n - 1) tab.right = rc.right;

        const bool sel = (i == st.selected);
        const bool hov = (i == st.hovered && !sel && !st.base.disabled);
        if (hov) g.FillRound(tab, mx.radiusSm, pal.hover);

        Color txt = st.base.disabled ? pal.textDisabled
                  : sel ? pal.primaryText : pal.textSub;
        g.Text(tab, st.tabs[i], FontRole::Body, txt,
               dt::SingleLine | dt::Center | dt::VCenter);

        if (sel)
        {
            Rect ul{tab.left + 10, rc.bottom - 3, tab.right - 10, rc.bottom};
            g.FillRound(ul, 2, pal.primary);
        }
    }
}

void PaintAvatar(IPainter& g,
                 const Rect& rc,
                 const AvatarState& st,
                 const Palette& pal,
                 const Metrics& mx)
{
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const int d = (w < h ? w : h);
    Rect box{rc.left, rc.top, rc.left + d, rc.top + d};

    const Color bg = st.base.disabled ? pal.sunken : pal.primarySoft;
    g.FillRound(box, d / 2, bg);
    if (!st.base.disabled) g.StrokeRound(box, d / 2, pal.primary, 1);

    const Color txt = st.base.disabled ? pal.textDisabled : pal.primaryText;
    g.Text(box, st.text, FontRole::Body, txt,
           dt::SingleLine | dt::Center | dt::VCenter);
}

void PaintSkeleton(IPainter& g,
                   const Rect& rc,
                   const SkeletonState& st,
                   const Palette& pal,
                   const Metrics& mx)
{
    int lines = st.lines < 1 ? 1 : st.lines;
    const int gap = 8;
    int lineH = (rc.bottom - rc.top - gap * (lines - 1)) / lines;
    if (lineH < 6) lineH = 6;

    const int widths[] = {100, 84, 66};
    const Color fill = st.base.disabled ? pal.split : pal.sunken;
    for (int i = 0; i < lines; ++i)
    {
        const int top = rc.top + i * (lineH + gap);
        if (top + lineH > rc.bottom) break;
        const int lw = (rc.right - rc.left) * widths[i % 3] / 100;
        Rect bar{rc.left, top, rc.left + lw, top + lineH};
        g.FillRound(bar, lineH / 2, fill);
    }
}

} // namespace ui
} // namespace air
