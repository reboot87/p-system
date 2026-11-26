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

    // ---- neutral geometry helpers ----
    // The platform backends use these instead of exposing their own rect
    // algebra, so page code never has to reach for a native rectangle API.

    // Translate by (dx, dy).
    constexpr Rect Offset(int32_t dx, int32_t dy) const
    {
        return Rect{ left + dx, top + dy, right + dx, bottom + dy };
    }

    // Grow by (dx, dy) on every side (left/top shrink, right/bottom grow).
    constexpr Rect Inflated(int32_t dx, int32_t dy) const
    {
        return Rect{ left - dx, top - dy, right + dx, bottom + dy };
    }

    // Shared area of two rectangles. Empty when they do not overlap.
    constexpr Rect Intersect(const Rect& r) const
    {
        Rect o;
        o.left   = left   > r.left   ? left   : r.left;
        o.top    = top    > r.top    ? top    : r.top;
        o.right  = right  < r.right  ? right  : r.right;
        o.bottom = bottom < r.bottom ? bottom : r.bottom;
        if (o.right  < o.left)  o.right  = o.left;
        if (o.bottom < o.top)   o.bottom = o.top;
        return o;
    }

    // Bounding box of two rectangles.
    constexpr Rect Union(const Rect& r) const
    {
        Rect o;
        o.left   = left   < r.left   ? left   : r.left;
        o.top    = top    < r.top    ? top    : r.top;
        o.right  = right  > r.right  ? right  : r.right;
        o.bottom = bottom > r.bottom ? bottom : r.bottom;
        return o;
    }
};

// ---------------- color ----------------
using Color = uint32_t;

// Rect 与 Color 的二进制布局是与平台无关的契约的组成部分(见文件头说明)：
// 平台层依赖此布局按字段直接复制，不做逐字段转换。尺寸变了就应视为
// ABI 变更，在这里提前拦住。
static_assert(sizeof(Rect) == 16, "Rect binary layout changed - ABI break");
static_assert(sizeof(Color) == 4, "Color binary layout changed - ABI break");

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
// Mirrors the native text-flag bit layout exactly, so the platform boundary
// can pass the value straight to the native text API without translating
// it. Values are pinned to the Win32 SDK definitions (verified against
// um/winuser.h) because every backend relies on the same bit positions.
constexpr TextFormat Top              = 0x00000;
constexpr TextFormat Left             = 0x00000;
constexpr TextFormat Center           = 0x00001;
constexpr TextFormat Right            = 0x00002;
constexpr TextFormat VCenter          = 0x00004;
constexpr TextFormat Bottom           = 0x00008;
constexpr TextFormat WordBreak        = 0x00010;
constexpr TextFormat SingleLine       = 0x00020;
constexpr TextFormat ExpandTabs       = 0x00040;
constexpr TextFormat TabStop          = 0x00080;
constexpr TextFormat NoClipping       = 0x00100;
constexpr TextFormat ExternalLeading  = 0x00200;
constexpr TextFormat CalcRect         = 0x00400;
constexpr TextFormat NoPrefix         = 0x00800;
constexpr TextFormat EditCtrl         = 0x02000;
constexpr TextFormat PathEllipsis     = 0x04000;
constexpr TextFormat EndEllipsis      = 0x08000;
constexpr TextFormat WordEllipsis     = 0x40000;
constexpr TextFormat NoFullWidthBreak = 0x80000;
} // namespace dt

// ---------------- opaque handles / message words ----------------
// 公开 API 只使用以下平台无关别名携带原生句柄与消息字；具体平台在
// 私有实现处把它们还原成各自的原生 handle 类型。别名保持与指针等宽，
// 因此调用方传入的原生句柄可隐式转换，无需任何跨边界拷贝。
using WindowHandle         = void*;
using FontHandle           = void*;
using DeviceContextHandle  = void*;   // native drawing context handle
using BrushHandle          = void*;
using PenHandle            = void*;
using BitmapHandle         = void*;
using GdiObjectHandle      = void*;
using InstanceHandle       = void*;   // module / application instance
using IconHandle           = void*;
using CursorHandle         = void*;
using MenuHandle           = void*;
using MessageWParam        = uintptr_t;
using MessageLParam        = intptr_t;
using MessageResult        = intptr_t;
// Native sub-class/callback words. Pointer-width so a platform handle
// survives the round trip without truncation on both x86 and x64.
using SubclassId           = uintptr_t;
using SubclassRef          = uintptr_t;

// ---------------- message / drawing flags ----------------
// MessageId is the platform message identifier (UINT). VirtualKey is
// the platform key code. TextFormat already mirrors DT_* bits above.
using MessageId   = uint32_t;
using VirtualKey  = uint32_t;
using MessageBoxFlags = uint32_t;

namespace vkey {
// 中立虚拟键常量。取值沿用各平台通行的键盘扫描码约定，页面代码
// 比较这些常量即可，不必引用任何平台头文件。
constexpr VirtualKey Backspace  = 0x08;
constexpr VirtualKey Tab        = 0x09;
constexpr VirtualKey Return     = 0x0D;
constexpr VirtualKey Shift      = 0x10;
constexpr VirtualKey Control    = 0x11;
constexpr VirtualKey Escape     = 0x1B;
constexpr VirtualKey Space      = 0x20;
constexpr VirtualKey PageUp     = 0x21;
constexpr VirtualKey PageDown   = 0x22;
constexpr VirtualKey End        = 0x23;
constexpr VirtualKey Home       = 0x24;
constexpr VirtualKey Left       = 0x25;
constexpr VirtualKey Up         = 0x26;
constexpr VirtualKey Right      = 0x27;
constexpr VirtualKey Down       = 0x28;
constexpr VirtualKey Delete     = 0x2E;
constexpr VirtualKey F1         = 0x70;
constexpr VirtualKey F2         = 0x71;
constexpr VirtualKey F3         = 0x72;
constexpr VirtualKey F4         = 0x73;
constexpr VirtualKey F5         = 0x74;
constexpr VirtualKey F6         = 0x75;
constexpr VirtualKey F7         = 0x76;
constexpr VirtualKey F8         = 0x77;
constexpr VirtualKey F9         = 0x78;
constexpr VirtualKey F10        = 0x79;
constexpr VirtualKey F11        = 0x7A;
constexpr VirtualKey F12        = 0x7B;
} // namespace vkey

namespace mb {
constexpr MessageBoxFlags Ok          = 0x00000000u;
constexpr MessageBoxFlags OkCancel    = 0x00000001u;
constexpr MessageBoxFlags YesNo       = 0x00000004u;
constexpr MessageBoxFlags IconError   = 0x00000010u;
constexpr MessageBoxFlags IconWarning = 0x00000030u;
constexpr MessageBoxFlags IconInfo    = 0x00000040u;
} // namespace mb

// Neutral drawing/message PODs. Field order and widths mirror the common
// platform structures so the boundary can reinterpret them without copying,
// while public headers stay free of native handle/type names.
struct DrawItemInfo
{
    uint32_t             CtlType    = 0;
    uint32_t             CtlID      = 0;
    uint32_t             itemID     = 0;
    uint32_t             itemAction = 0;
    uint32_t             itemState  = 0;
    WindowHandle         hwndItem   = nullptr;
    DeviceContextHandle  hDC        = nullptr;
    Rect                 rcItem     = {};
    uintptr_t            itemData   = 0;
};

struct MeasureItemInfo
{
    uint32_t  CtlType    = 0;
    uint32_t  CtlID      = 0;
    uint32_t  itemID     = 0;
    uint32_t  itemWidth  = 0;
    uint32_t  itemHeight = 0;
    uintptr_t itemData   = 0;
};

// Neutral message POD. Field order and widths mirror the common platform
// message-struct layout so the boundary can reinterpret it without copying,
// while the public header stays free of native handle/type names.
struct MessageInfo
{
    WindowHandle  hwnd    = nullptr;
    MessageId     message = 0;
    MessageWParam wParam  = 0;
    MessageLParam lParam  = 0;
    uint32_t      time    = 0;
    Point         pt      = {};
};

// 日期时间 POD：字段布局与通用平台时间结构一致，供 i18n 格式化使用。
struct DateTime
{
    uint16_t year         = 0;
    uint16_t month        = 0;
    uint16_t dayOfWeek    = 0;
    uint16_t day          = 0;
    uint16_t hour         = 0;
    uint16_t minute       = 0;
    uint16_t second       = 0;
    uint16_t milliseconds = 0;
};

} // namespace ui
} // namespace air
