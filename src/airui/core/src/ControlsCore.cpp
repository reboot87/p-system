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

} // namespace ui
} // namespace air
