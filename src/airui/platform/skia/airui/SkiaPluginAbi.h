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
// The generic versioned description struct (AiruiPluginDesc with
// size / abi / caps / painterIfaceRev) lives in src/pluginapi and is
// the contract negotiated through airui_skia_describe. Bump the C ABI
// number and the painter interface revision together whenever the
// cross-module IPainter vtable changes; the loader refuses a DLL that
// does not match instead of crashing.
// ============================================================
#include "pluginapi/airui_plugin_abi.h"
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define AIRUI_SKIA_CALL __cdecl
#define AIRUI_SKIA_PLUGIN_DLL L"airui-skia.dll"
#define AIRUI_SKIA_PLUGIN_ENV L"AIRUI_SKIA_DLL"
#else
#define AIRUI_SKIA_CALL
#define AIRUI_SKIA_PLUGIN_DLL L"libairui-skia.so"
#define AIRUI_SKIA_PLUGIN_ENV L"AIRUI_SKIA_DLL"
#endif

// Exported symbol names (runtime dynamic-loader lookup).
#define AIRUI_SKIA_FN_DESCRIBE "airui_skia_describe"
#define AIRUI_SKIA_FN_CREATE    "airui_skia_create"
#define AIRUI_SKIA_FN_DESTROY   "airui_skia_destroy"
#define AIRUI_SKIA_FN_PIXELS    "airui_skia_pixels"
#define AIRUI_SKIA_FN_ROW_BYTES "airui_skia_row_bytes"

extern "C" {

// Returns a pointer to the plugin's static AiruiPluginDesc, negotiated
// by the host (abi / size / painterIfaceRev must all match).
typedef AiruiPluginDescribeFn AiruiSkiaDescribeFn;

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
