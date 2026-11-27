#pragma once
// ============================================================
// AirUI - airui_plugin_abi.h
// Stable C ABI shared by the application host and every render
// backend plugin (e.g. airui-skia.dll).
//
// Only plain C function pointer shapes cross the module boundary,
// so STL containers and C++ classes are banned from this header.
// The single capability/version negotiation entry point returns a
// versioned description struct; the host validates size, abi and
// painterIfaceRev before constructing any painter, and rejects a
// plugin whose vtable layout differs even when the C ABI number
// happens to match.
//
// Bump AIRUI_PLUGIN_ABI whenever a function shape changes, and
// bump AIRUI_PAINTER_IFACE_REV whenever the cross-module IPainter
// vtable layout (the virtual function set) changes.
// ============================================================
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define AIRUI_CALL __cdecl
#else
#define AIRUI_CALL
#endif

#define AIRUI_PLUGIN_ABI 2

enum
{
    AIRUI_CAP_WIDE_TEXT   = 1u << 0,   // backend rasterises CJK / icon glyphs
    AIRUI_CAP_SUBPIXEL_AA = 1u << 1,
    AIRUI_CAP_OFFSCREEN   = 1u << 2,
    // The backend resolves the symbols the UI family does not carry
    // (U+2713 check mark, U+2715 ballot X, U+25BE, ...) through the
    // same font-link chain GDI uses, so the glyphs come out identical
    // instead of as the .notdef box. A plugin that does not set this
    // bit still draws text, but its symbols differ from GDI's.
    AIRUI_CAP_SYMBOL_FALLBACK = 1u << 3,
};

// Revision of the IPainter vtable that crosses the module boundary.
// This is independent of the C ABI number: it lets the host reject a
// plugin whose C++ virtual layout differs without waiting for the C ABI
// number to move.
// Rev 3 appended HLine / HLineDashed / VLine / Dot / ProgressBar /
// LineHeight to IPainter (the page-facing Canvas no longer goes through
// the native gfx helpers).
#define AIRUI_PAINTER_IFACE_REV 3

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AiruiPluginDesc
{
    uint32_t size;             // sizeof(AiruiPluginDesc), for forward compat
    uint32_t abi;              // AIRUI_PLUGIN_ABI
    uint32_t caps;             // AIRUI_CAP_* bitmask
    uint32_t painterIfaceRev;  // AIRUI_PAINTER_IFACE_REV
} AiruiPluginDesc;

// Returns a pointer to the plugin's static description.
typedef const AiruiPluginDesc* (AIRUI_CALL *AiruiPluginDescribeFn)();

#ifdef __cplusplus
} // extern "C"
#endif