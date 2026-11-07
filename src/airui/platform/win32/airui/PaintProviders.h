#pragma once
// ============================================================
// AirUI - PaintProviders.h
// Win32 side of the runtime painter registry.
//   - "gdi"  is always available.
//   - "skia" is either linked into the process (when a Skia SDK was
//     available at build time) or loaded from airui-skia.dll at
//     runtime. Which one is in use is a runtime decision; the app
//     never needs to be recompiled to enable Skia.
// ============================================================
namespace air {
namespace ui {

// Register gdi + the best available skia provider. Safe to call more
// than once (re-registration replaces the same ids).
void RegisterPlatformPaintProviders();

// Force the DLL-backed skia provider (overrides a linked-in one).
// Used by the plugin integration test and by explicit opt-in.
void RegisterSkiaPluginProvider();

// Diagnostics: true once the plugin library was loaded and its ABI
// matched. Also lets a host retry loading after deploying the DLL.
bool SkiaPluginLoaded();
void SkiaPluginReload();

// Runtime capability probe: renders one CJK glyph into an off-screen
// plugin frame and checks whether any ink was produced. Older plugin
// builds use drawSimpleText() with a typeface that has no CJK glyphs,
// so text vanishes entirely; hosts must then rasterise text themselves.
// Once a plugin ships proper font fallback this returns true and the
// whole frame (including text) is rendered by the backend.
// AIRUI_SKIA_TEXT=host|backend overrides the probe for diagnostics.
bool SkiaPluginSupportsWideText();

} // namespace ui
} // namespace air
