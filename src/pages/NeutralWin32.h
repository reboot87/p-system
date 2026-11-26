#pragma once
// App-layer adapters: the public AirUI API is platform-neutral, while a few
// page escape hatches still talk to Win32 directly. Keep every cast here so
// page code never spreads native handle conversions across the codebase.
#include <windows.h>
#include "airui/AirUi.h"

namespace arecord {
namespace detail {

inline HWND Hwnd(air::ui::WindowHandle h)
{
    return reinterpret_cast<HWND>(h);
}

inline HDC Dc(air::ui::DeviceContextHandle h)
{
    return reinterpret_cast<HDC>(h);
}

inline RECT NativeRect(const air::ui::Rect& r)
{
    return RECT{ r.left, r.top, r.right, r.bottom };
}

inline air::ui::Rect NeutralRect(const RECT& r)
{
    return air::ui::Rect{ r.left, r.top, r.right, r.bottom };
}

inline POINT NativePoint(const air::ui::Point& p)
{
    return POINT{ p.x, p.y };
}

inline bool Hit(const air::ui::Rect& r, const air::ui::Point& p)
{
    RECT native = NativeRect(r);
    POINT pt = NativePoint(p);
    return PtInRect(&native, pt) != FALSE;
}

inline void Inflate(air::ui::Rect& r, int dx, int dy)
{
    RECT native = NativeRect(r);
    InflateRect(&native, dx, dy);
    r = NeutralRect(native);
}

inline void Offset(air::ui::Rect& r, int dx, int dy)
{
    RECT native = NativeRect(r);
    OffsetRect(&native, dx, dy);
    r = NeutralRect(native);
}

} // namespace detail
} // namespace arecord
