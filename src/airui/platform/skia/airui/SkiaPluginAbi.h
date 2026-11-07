#pragma once
// ============================================================
// AirUI - SkiaPluginAbi.h
// Stable C ABI between the application and airui-skia.dll.
//
// This header is shared by BOTH sides and must stay free of Skia
// headers, STL containers and C++ classes: only plain C function
// pointer shapes cross the module boundary. The opaque handle is an
// IPainter* created and destroyed INSIDE the plugin, so ownership
// never crosses a CRT boundary.
//
// Bump AIRUI_SKIA_PLUGIN_ABI whenever a function shape changes;
// the loader refuses a DLL with a different ABI instead of crashing.
// ============================================================
#include <stddef.h>
#include <stdint.h>

#define AIRUI_SKIA_PLUGIN_ABI 1

#ifdef _WIN32
#define AIRUI_SKIA_CALL __cdecl
#define AIRUI_SKIA_PLUGIN_DLL L"airui-skia.dll"
#define AIRUI_SKIA_PLUGIN_ENV L"AIRUI_SKIA_DLL"
#else
#define AIRUI_SKIA_CALL
#define AIRUI_SKIA_PLUGIN_DLL L"libairui-skia.so"
#define AIRUI_SKIA_PLUGIN_ENV L"AIRUI_SKIA_DLL"
#endif

// Exported symbol names (GetProcAddress / dlsym).
#define AIRUI_SKIA_FN_ABI       "airui_skia_abi"
#define AIRUI_SKIA_FN_CREATE    "airui_skia_create"
#define AIRUI_SKIA_FN_DESTROY   "airui_skia_destroy"
#define AIRUI_SKIA_FN_PIXELS    "airui_skia_pixels"
#define AIRUI_SKIA_FN_ROW_BYTES "airui_skia_row_bytes"

extern "C" {

// Returns AIRUI_SKIA_PLUGIN_ABI.
typedef int (AIRUI_SKIA_CALL *AiruiSkiaAbiFn)();

// surface: opaque native drawing surface, or nullptr for an off-screen
// frame. (l,t,r,b): frame bounds in logical pixels.
// Returns an opaque painter handle, or nullptr on failure.
typedef void* (AIRUI_SKIA_CALL *AiruiSkiaCreateFn)(void*, int, int, int, int);

// Destroys the painter inside the plugin (flushes the frame).
typedef void (AIRUI_SKIA_CALL *AiruiSkiaDestroyFn)(void*);

// Raw premultiplied BGRA pixels / stride of an off-screen frame.
typedef const void* (AIRUI_SKIA_CALL *AiruiSkiaPixelsFn)(void*);
typedef uint64_t (AIRUI_SKIA_CALL *AiruiSkiaRowBytesFn)(void*);

} // extern "C"
