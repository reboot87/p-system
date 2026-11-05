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

} // namespace ui
} // namespace air
