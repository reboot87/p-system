// ============================================================
// AirUI - SkiaTypes.cpp (pure C++, no Skia SDK required)
// ============================================================
#include "airui/SkiaTypes.h"

namespace air {
namespace ui {
namespace skia {

namespace {

void AppendUtf8(std::string& out, uint32_t cp)
{
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

} // namespace

int RoleFontSize(FontRole role) noexcept
{
    // Must stay byte-for-byte in sync with GdiPainter::FontFor().
    static const int kSize[static_cast<int>(FontRole::Count)] = {
        13, 11, 20, 15, 13, 13, 18, 28
    };
    int i = static_cast<int>(role);
    if (i < 0 || i >= static_cast<int>(FontRole::Count)) i = 0;
    return kSize[i];
}

std::string WideToUtf8(const std::wstring& s)
{
    std::string out;
    out.reserve(s.size() + s.size() / 2 + 1);
    constexpr bool kUtf16 = (sizeof(wchar_t) == 2);
    size_t i = 0;
    while (i < s.size()) {
        uint32_t cp = 0;
        if constexpr (kUtf16) {
            const uint32_t u = static_cast<uint32_t>(
                static_cast<uint16_t>(s[i++]));
            if (u >= 0xD800u && u <= 0xDBFFu && i < s.size()) {
                const uint32_t lo = static_cast<uint32_t>(
                    static_cast<uint16_t>(s[i]));
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    ++i;
                    cp = 0x10000u + ((u - 0xD800u) << 10) + (lo - 0xDC00u);
                } else {
                    cp = 0xFFFDu;                 // lone high surrogate
                }
            } else if (u >= 0xD800u && u <= 0xDFFFu) {
                cp = 0xFFFDu;                     // stray low surrogate
            } else {
                cp = u;
            }
        } else {
            cp = static_cast<uint32_t>(s[i++]);
        }
        AppendUtf8(out, cp);
    }
    return out;
}

HAlign Horizontal(TextFormat fmt) noexcept
{
    if (fmt & dt::Right)  return HAlign::Right;
    if (fmt & dt::Center) return HAlign::Center;
    return HAlign::Left;
}

VAlign Vertical(TextFormat fmt) noexcept
{
    if (fmt & dt::VCenter) return VAlign::Middle;
    if (fmt & dt::Bottom)  return VAlign::Bottom;
    return VAlign::Top;
}

} // namespace skia
} // namespace ui
} // namespace air
