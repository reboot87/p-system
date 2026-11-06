#pragma once
// ============================================================
// AirUI - SkiaTypes.h
// Pure C++ value mapping between the platform-free core types and
// the Skia rendering backend. This file has NO Skia dependency and
// NO platform headers, so it can be compiled and unit-tested on
// every OS without a Skia SDK (see tests/test_skia_types.cpp).
// ============================================================
#include "airui/hal.h"

#include <cstdint>
#include <string>

namespace air {
namespace ui {
namespace skia {

// core Color is 0x00BBGGRR; Skia's 32-bit color is 0xAARRGGBB.
constexpr uint32_t ToSkColor(Color c, uint8_t alpha = 255) noexcept
{
    return (static_cast<uint32_t>(alpha) << 24)
         | (ColorR(c) << 16) | (ColorG(c) << 8) | ColorB(c);
}

// Keep the logical font sizes identical to GdiPainter's role table so
// text metrics do not drift between the two Windows backends.
int RoleFontSize(FontRole role) noexcept;

// wchar_t is UTF-16 on Windows and UTF-32 on Linux/macOS. Skia wants
// UTF-8, so both layouts are handled here (surrogate pairs included).
std::string WideToUtf8(const std::wstring& s);

enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Middle, Bottom };

// dt::* flags mirror the Win32 DT_* bits; translate them into the
// alignment the Skia text shaper applies.
HAlign Horizontal(TextFormat fmt) noexcept;
VAlign Vertical(TextFormat fmt) noexcept;

} // namespace skia
} // namespace ui
} // namespace air
