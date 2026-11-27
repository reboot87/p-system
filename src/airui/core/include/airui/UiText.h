#pragma once
// ============================================================
// AirUI - UiText.h
// The single source of truth for UI typography.
//
// Every painter backend must size and style text from this table.
// Before it existed, GdiPainter and SkiaPainter each carried their
// own numbers, and they drifted: GDI drew a 13 px *cell* while Skia
// called SkFont::setSize(13) -- but setSize takes an *em* size, and
// for the UI family the cell is ~1.33x the em. Skia text therefore
// came out ~33% too large: headings collided with the title bar and
// every nav row got clipped to an ellipsis.
//
// The numbers mirror air::ui::Metrics (Theme.h) -- fontBody 14,
// fontMeta 12, fontTitle 13, fontSection 14, fontButton 14,
// fontIcon 11, fontIconLg 16, fontDisplay 22 -- and the family
// mirrors Fonts::m_family, because that is what the Win32 backend
// actually selects. A theme that overrides Metrics is NOT reflected
// here yet (the Skia plugin cannot reach Mx()), so keep the defaults
// in sync if the design system moves.
//
// Keep this header platform-free: it is consumed by the Win32 GDI
// backend, the Skia plugin (which may compile without native SDK headers),
// and the syntax-gate mock.
// ============================================================
#include "airui/hal.h"

namespace air {
namespace ui {

// `cell` is the logical character height in pixels at 96 DPI: the total
// ascent + descent of the typeface, so a backend whose text API measures
// by em must convert, not copy, the number.
struct FontSpec
{
    int cell   = 13;
    int weight = 400;   // 400 = regular, 600 = semibold
};

// Families. wchar_t literals are valid on every target we build for
// (wchar_t is 2 bytes on Windows, and the Skia plugin's TCHAR is
// either wchar_t or char -- the face-name copy narrows ASCII safely).
// The UI family is Microsoft YaHei UI, not Segoe UI: that is what
// Fonts::m_family selects, and it is why GDI needs no font linking
// for Chinese. Latin and CJK therefore share one em.
inline constexpr const wchar_t* kFamilyUi    = L"Microsoft YaHei UI";
inline constexpr const wchar_t* kFamilyCjk   = L"Microsoft YaHei UI";
inline constexpr const wchar_t* kFamilyIcon  = L"Segoe MDL2 Assets";

// Symbols the UI family does not carry: U+2713 CHECK MARK (the tick in
// a checkbox), U+2715 BALLOT X, arrows, dingbats. GDI resolves those
// through its own fallback chain, and a backend that picks faces itself
// -- the Skia plugin -- has to name the same families explicitly, or
// the glyph is dropped and the control renders an empty box.
//
// Measured on Windows (advance + ink box, GDI against each candidate):
//   * the Dingbats block (U+2700..U+27BF: check, cross, ballot X)
//     comes from Segoe UI Symbol -- GDI's symbol font;
//   * the other symbols the UI family lacks (U+25BE, U+25B8, U+26A0,
//     U+2139) come from Yu Gothic UI, the tail of GDI's font-link
//     chain for the UI family;
//   * Segoe UI Symbol covers everything the UI family lacks, so it is
//     the last named stop before the system default.
inline constexpr const wchar_t* kFamilySymbol = L"Segoe UI Symbol";
inline constexpr const wchar_t* kFamilyLinked = L"Yu Gothic UI";

// Header-inline on purpose: the Skia plugin links its own small library
// and must not pull in core just for eight integers.
inline FontSpec RoleFontSpec(FontRole role) noexcept
{
    // Order matches FontRole; values are Metrics' defaults, which the
    // Win32 backend's Fonts collection uses.
    static const int kCell[static_cast<int>(FontRole::Count)] = {
        14,  // Body     (fontBody)
        12,  // Meta     (fontMeta)
        13,  // Title    (fontTitle)
        14,  // Section  (fontSection)
        14,  // Button   (fontButton)
        11,  // Icon     (fontIcon)
        16,  // IconLarge(fontIconLg)
        22,  // Display  (fontDisplay)
    };
    // Headings and buttons are semibold in the design system (see
    // Fonts.cpp). Without the weight the Skia backend drew the window
    // title noticeably lighter than GDI, which reads as "different
    // font" even when the size matches.
    static const int kWeight[static_cast<int>(FontRole::Count)] = {
        400,  // Body
        400,  // Meta
        600,  // Title
        600,  // Section
        600,  // Button
        400,  // Icon
        400,  // IconLarge
        600,  // Display
    };
    int i = static_cast<int>(role);
    if (i < 0 || i >= static_cast<int>(FontRole::Count)) i = 0;
    return FontSpec{ kCell[i], kWeight[i] };
}

} // namespace ui
} // namespace air
