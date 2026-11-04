#pragma once
// ============================================================
// AirUI - types.h
// Platform-free base types for the core library.
//
// core/ may only include this header plus the C++ standard
// library. No platform headers, no handle types.
//
//   Rect   - left/top/right/bottom rectangle (ABI-compatible
//            with the Win32 rectangle layout, converted at the
//            platform boundary).
//   Color  - 0x00BBGGRR, identical to the Win32 color
//            layout, so the Win32 backend can pass it straight
//            through and Skia/Qt backends can convert.
//   dt::*  - text-format flags mirroring the Win32 DT_* bits.
// ============================================================
#include <cstdint>
#include <cstddef>

namespace air {
namespace ui {

// ---------------- geometry ----------------
struct Point
{
    int32_t x = 0;
    int32_t y = 0;
};

struct Size
{
    int32_t w = 0;
    int32_t h = 0;
};

struct Rect
{
    int32_t left   = 0;
    int32_t top    = 0;
    int32_t right  = 0;
    int32_t bottom = 0;

    constexpr Rect() = default;
    constexpr Rect(int32_t l, int32_t t, int32_t r, int32_t b)
        : left(l), top(t), right(r), bottom(b) {}

    constexpr int32_t Width()  const { return right - left; }
    constexpr int32_t Height() const { return bottom - top; }
    constexpr Point   Location() const { return { left, top }; }
    constexpr Size    SizeOf()   const { return { right - left, bottom - top }; }

    constexpr bool Contains(Point p) const
    {
        return p.x >= left && p.x < right && p.y >= top && p.y < bottom;
    }
    constexpr bool Contains(const Rect& r) const
    {
        return r.left >= left && r.top >= top && r.right <= right && r.bottom <= bottom;
    }
    constexpr bool Empty() const { return right <= left || bottom <= top; }
};

// ---------------- color ----------------
using Color = uint32_t;

constexpr Color MakeColor(uint32_t r, uint32_t g, uint32_t b) noexcept
{
    return ((b << 16) | (g << 8) | r) & 0x00FFFFFFu;
}

constexpr Color MakeArgb(uint32_t a, uint32_t r, uint32_t g, uint32_t b) noexcept
{
    return ((a & 0xFFu) << 24) | MakeColor(r, g, b);
}

constexpr uint32_t ColorR(Color c) noexcept { return c & 0xFFu; }
constexpr uint32_t ColorG(Color c) noexcept { return (c >> 8) & 0xFFu; }
constexpr uint32_t ColorB(Color c) noexcept { return (c >> 16) & 0xFFu; }
constexpr uint32_t ColorA(Color c) noexcept { return (c >> 24) & 0xFFu; }

// ---------------- text formatting ----------------
using TextFormat = uint32_t;

namespace dt {
constexpr TextFormat Top              = 0x0000;
constexpr TextFormat Left             = 0x0000;
constexpr TextFormat Center           = 0x0001;
constexpr TextFormat Right            = 0x0002;
constexpr TextFormat VCenter          = 0x0004;
constexpr TextFormat Bottom           = 0x0008;
constexpr TextFormat WordBreak        = 0x0010;
constexpr TextFormat SingleLine       = 0x0020;
constexpr TextFormat ExpandTabs       = 0x0040;
constexpr TextFormat TabStop          = 0x0080;
constexpr TextFormat NoClipping       = 0x0100;
constexpr TextFormat NoPrefix         = 0x0800;
constexpr TextFormat EditCtrl         = 0x2000;
constexpr TextFormat PathEllipsis     = 0x4000;
constexpr TextFormat EndEllipsis      = 0x8000;
constexpr TextFormat NoFullWidthBreak = 0x80000;
} // namespace dt

} // namespace ui
} // namespace air
