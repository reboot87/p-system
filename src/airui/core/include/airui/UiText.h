#pragma once
// ============================================================
// AirUI - UiText.h
// The single source of truth for UI typography.
//
// Every painter backend must size and style text from this table.
// Before it existed, GdiPainter and SkiaPainter each carried their
// own numbers, and they drifted: GDI drew a 13 px *cell* while Skia
// called SkFont::setSize(13) -- but setSize takes an *em* size, and
// for Segoe UI the cell is ~1.33x the em. Skia text therefore came
// out ~33% too large: headings collided with the title bar and every
// nav row got clipped to an ellipsis.
//
// Keep this header platform-free: it is consumed by the Win32 GDI
// backend, the Skia plugin (which may compile without <windows.h>),
// and the syntax-gate mock.
// ============================================================
#include "airui/hal.h"

namespace air {
namespace ui {

// `cell` is the logical character height in pixels at 96 DPI: GDI's
// LOGFONT.lfHeight is exactly this (ascent + descent), so a backend
// that renders by em must convert, not copy, the number.
struct FontSpec
{
    int cell   = 13;
    int weight = 400;   // 400 = FW_NORMAL, 600 = FW_SEMIBOLD
};

// Families. wchar_t literals are valid on every target we build for
// (wchar_t is 2 bytes on Windows, and the Skia plugin's TCHAR is
// either wchar_t or char -- the face-name copy narrows ASCII safely).
inline constexpr const wchar_t* kFamilyUi    = L"Segoe UI";
inline constexpr const wchar_t* kFamilyCjk   = L"Microsoft YaHei UI";
inline constexpr const wchar_t* kFamilyIcon  = L"Segoe MDL2 Assets";

// Header-inline on purpose: the Skia plugin links its own small library
// and must not pull in core just for eight integers.
inline FontSpec RoleFontSpec(FontRole role) noexcept
{
    // Order matches FontRole. These are the values the GDI backend has
    // always used, so matching them changes nothing on screen in GDI
    // mode and makes Skia produce the identical layout.
    static const int kCell[static_cast<int>(FontRole::Count)] = {
        13,  // Body
        11,  // Meta
        20,  // Title
        15,  // Section
        13,  // Button
        13,  // Icon
        18,  // IconLarge
        28,  // Display
    };
    int i = static_cast<int>(role);
    if (i < 0 || i >= static_cast<int>(FontRole::Count)) i = 0;
    return FontSpec{ kCell[i], 400 };
}

} // namespace ui
} // namespace air
