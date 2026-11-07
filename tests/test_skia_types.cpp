// ============================================================
// test_skia_types.cpp
// Pure-C++ mapping tests for the Skia backend. Requires NO Skia SDK
// and NO platform headers, so it runs in ctest on Windows, Linux and
// macOS CI. This is the guard that keeps the backend's value mapping
// honest even when Skia itself is not installed.
// ============================================================
#include "airui/SkiaTypes.h"
#include "airui/UiText.h"

#include <cstdio>
#include <string>

using namespace air::ui;
using namespace air::ui::skia;

namespace {
int g_fail = 0;

void Check(bool ok, const char* what)
{
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_fail;
    }
}

std::string Hex(const std::string& s)
{
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 0xF]);
    }
    return out;
}
} // namespace

int main()
{
    // core Color is 0x00BBGGRR; Skia is 0xAARRGGBB.
    const Color c = MakeColor(0x12, 0x34, 0x56);
    Check(c == 0x00563412u, "core color layout");
    Check(ToSkColor(c) == 0xFF123456u, "color conversion keeps opaque alpha");
    Check(ToSkColor(c, 0x80) == 0x80123456u, "color conversion applies alpha");
    Check(ToSkColor(c, 0) == 0x00123456u, "color conversion allows zero alpha");

    // Font roles must match GdiPainter's table byte for byte.
    Check(RoleFontSize(FontRole::Body)      == 13, "role size Body");
    Check(RoleFontSize(FontRole::Meta)      == 11, "role size Meta");
    Check(RoleFontSize(FontRole::Title)     == 20, "role size Title");
    Check(RoleFontSize(FontRole::Section)   == 15, "role size Section");
    Check(RoleFontSize(FontRole::Button)    == 13, "role size Button");
    Check(RoleFontSize(FontRole::Icon)      == 13, "role size Icon");
    Check(RoleFontSize(FontRole::IconLarge) == 18, "role size IconLarge");
    Check(RoleFontSize(FontRole::Display)   == 28, "role size Display");
    Check(RoleFontSize(static_cast<FontRole>(999)) == 13,
          "out-of-range role falls back to Body");

    // Wide -> UTF-8, covering 1/2/3/4-byte encodings and both wchar_t
    // layouts (Windows UTF-16 vs Linux/macOS UTF-32).
    Check(WideToUtf8(L"") == "", "empty string");
    Check(WideToUtf8(L"A") == "A", "ASCII");
    Check(Hex(WideToUtf8(L"\u00E9")) == "C3A9", "2-byte UTF-8 (e acute)");
    Check(Hex(WideToUtf8(L"\u4E2D")) == "E4B8AD", "3-byte UTF-8 (CJK)");

    std::wstring emoji;
    constexpr bool kUtf16 = (sizeof(wchar_t) == 2);
    if constexpr (kUtf16) {
        emoji.push_back(static_cast<wchar_t>(0xD83D));
        emoji.push_back(static_cast<wchar_t>(0xDE00));
    } else {
        emoji.push_back(static_cast<wchar_t>(0x1F600));
    }
    Check(Hex(WideToUtf8(emoji)) == "F09F9880", "4-byte UTF-8 / surrogate pair");

    std::wstring mixed = L"a";
    mixed.push_back(static_cast<wchar_t>(0x4E2D));
    Check(Hex(WideToUtf8(mixed)) == "61E4B8AD", "mixed-width string");

    // dt::* alignment bits.
    Check(Horizontal(dt::Left)             == HAlign::Left,   "horizontal left");
    Check(Horizontal(dt::Center)           == HAlign::Center, "horizontal center");
    Check(Horizontal(dt::Right)            == HAlign::Right,  "horizontal right");
    Check(Horizontal(dt::Center | dt::VCenter) == HAlign::Center,
          "horizontal ignores vertical bits");
    Check(Vertical(dt::Top)                == VAlign::Top,    "vertical top");
    Check(Vertical(dt::VCenter)            == VAlign::Middle, "vertical middle");
    Check(Vertical(dt::Bottom)             == VAlign::Bottom, "vertical bottom");

    // Typography contract. Skia sizes by em, GDI by cell height, so the
    // two backends drifted once already (Skia text came out ~33% too big
    // and every nav label was ellipsised). Pin the table and the unit.
    static const int kExpected[(int)FontRole::Count] = {
        13, 11, 20, 15, 13, 13, 18, 28
    };
    for (int i = 0; i < (int)FontRole::Count; ++i) {
        const FontRole role = static_cast<FontRole>(i);
        Check(RoleFontSpec(role).cell == kExpected[i], "role cell height");
        Check(RoleFontSize(role) == kExpected[i],
              "RoleFontSize is a cell height, not an em size");
        Check(RoleFontSpec(role).weight >= 400 &&
              RoleFontSpec(role).weight <= 700, "role weight is a LOGFONT weight");
    }
    const FontSpec oob = RoleFontSpec(static_cast<FontRole>((int)FontRole::Count));
    Check(oob.cell == 13 && oob.weight == 400,
          "out-of-range role falls back to Body");

    if (g_fail == 0) std::printf("test_skia_types: all checks passed\n");
    else             std::printf("test_skia_types: %d check(s) failed\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
