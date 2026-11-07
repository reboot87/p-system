// ============================================================
// AirUI - SkiaPlugin.cpp
// The dynamically loadable Skia backend (airui-skia.dll).
//
// The application never links a Skia SDK: it loads this library at
// runtime, checks the ABI, and calls the C functions below. The
// returned opaque handle is an IPainter* owned by this module; the
// plugin's destroy() runs the virtual destructor (which flushes the
// frame to the native surface), so no object ownership crosses a
// module/CRT boundary.
// ============================================================
#include "airui/SkiaPluginAbi.h"
#include "airui/SkiaSurface.h"

#if defined(_WIN32)
#define AIRUI_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define AIRUI_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {

// The handle is the IPainter subobject pointer produced by create().
// SkiaFrame derives from IPainter by non-virtual single inheritance,
// so this downcast is well defined for handles we created ourselves.
air::ui::SkiaFrame* FrameFrom(void* handle)
{
    return static_cast<air::ui::SkiaFrame*>(
        reinterpret_cast<air::ui::IPainter*>(handle));
}

} // namespace

AIRUI_PLUGIN_EXPORT int AIRUI_SKIA_CALL airui_skia_abi()
{
    return AIRUI_SKIA_PLUGIN_ABI;
}

AIRUI_PLUGIN_EXPORT void* AIRUI_SKIA_CALL airui_skia_create(
    void* surface, int l, int t, int r, int b)
{
    using air::ui::IPainter;
    using air::ui::Rect;
    using air::ui::SkiaFrame;

    SkiaFrame* frame = nullptr;
#ifdef _WIN32
    if (surface)
        frame = new SkiaFrame(static_cast<HDC>(surface), Rect(l, t, r, b));
    else
#endif
        frame = new SkiaFrame(r - l, b - t);

    IPainter* painter = static_cast<IPainter*>(frame);
    if (!frame->Valid()) {
        delete painter;      // virtual destructor, safe
        return nullptr;
    }
    return painter;
}

AIRUI_PLUGIN_EXPORT void AIRUI_SKIA_CALL airui_skia_destroy(void* handle)
{
    if (handle) delete reinterpret_cast<air::ui::IPainter*>(handle);
}

AIRUI_PLUGIN_EXPORT const void* AIRUI_SKIA_CALL airui_skia_pixels(void* handle)
{
    return handle ? FrameFrom(handle)->Pixels() : nullptr;
}

AIRUI_PLUGIN_EXPORT uint64_t AIRUI_SKIA_CALL airui_skia_row_bytes(void* handle)
{
    return handle ? static_cast<uint64_t>(FrameFrom(handle)->RowBytes()) : 0u;
}
