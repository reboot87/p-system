#pragma once
// ============================================================
// AirUI - ControlsCore.h
// Self-drawn control visuals, platform free.
//
// These are the widgets that were already pure Canvas painting
// (no native handles): Button, Checkbox and Radio. The state machine lives
// here; a backend only implements IPainter.
//
//   ButtonState st;
//   st.hover = true;
//   PaintButton(painter, rc, L"Save", ButtonStyle::Primary,
//               st, Pal(), Mx());
//
// Keeping the drawing here means the Win32 backend and any future
// Skia/X11 backend produce identical pixels from the same code.
// ============================================================
#include "airui/types.h"
#include "airui/hal.h"
#include "airui/Theme.h"

#include <string>

namespace air {
namespace ui {

// The four button variants. Named ButtonStyle in core; the Win32
// backend keeps the historical alias BtnStyle for source compat.
enum class ButtonStyle
{
    Primary,    // 主色填充 + 白字
    Secondary,  // 白底 + 边框 + 深字
    Danger,     // 白底 + 红边 + 红字
    Ghost       // 透明底 + 主色字
};

// Shared interaction state for the self-drawn controls.
struct ControlState
{
    bool hover    = false;
    bool pressed  = false;
    bool focused  = false;
    bool checked  = false;
    bool disabled = false;
};

// ------------------------------------------------------------
// PaintButton - identical layout to the original DrawButton:
// centred single line, no mnemonic prefix, radius from Mx().
// ------------------------------------------------------------
void PaintButton(IPainter& g,
                 const Rect& rc,
                 const std::wstring& text,
                 ButtonStyle style,
                 const ControlState& st,
                 const Palette& pal,
                 const Metrics& mx);

// ------------------------------------------------------------
// PaintCheckbox - 16x16 rounded box at rc.top + 4, label at
// rc.left + 24. Geometry intentionally matches the gallery so the
// migration is pixel-for-pixel.
// ------------------------------------------------------------
void PaintCheckbox(IPainter& g,
                   const Rect& rc,
                   const std::wstring& text,
                   const ControlState& st,
                   const Palette& pal);

// ------------------------------------------------------------
// PaintRadio - 16x16 circle, 8x8 inner dot when checked.
// ------------------------------------------------------------
void PaintRadio(IPainter& g,
                const Rect& rc,
                const std::wstring& text,
                const ControlState& st,
                const Palette& pal);

// ------------------------------------------------------------
// PaintProgressBar - horizontal bar with normal/disabled states.
// ------------------------------------------------------------
void PaintProgressBar(IPainter& g,
                      const Rect& rc,
                      float progress,
                      const ControlState& st,
                      const Palette& pal,
                      const Metrics& mx);

// ------------------------------------------------------------
// PaintSlider - horizontal slider with thumb, hover/press/disabled.
// ------------------------------------------------------------
struct SliderState
{
    ControlState base = {};
    int value = 50;
    int min = 0;
    int max = 100;
};

void PaintSlider(IPainter& g,
                 const Rect& rc,
                 const SliderState& st,
                 const Palette& pal,
                 const Metrics& mx);

} // namespace ui
} // namespace air
