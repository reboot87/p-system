// ============================================================
// test_skia_plugin.cpp
// Real-SDK check that Skia can be chosen at RUNTIME from a DLL,
// with no Skia linkage in the caller's backend-selection code.
//
// Built only on Windows with AIRUI_WITH_SKIA=ON (the CI job builds
// airui-skia.dll from the vcpkg Skia SDK). It deliberately forces the
// DLL-backed provider even though this build also has a linked-in
// Skia backend, then drives the painter across the module boundary.
// ============================================================
#include "airui/PaintBackend.h"
#include "airui/PaintProviders.h"
#include "airui/SkiaPluginAbi.h"

#include <windows.h>

#include <cstdio>
#include <string>

using namespace air::ui;

namespace {

int g_fail = 0;

void Check(bool ok, const char* what)
{
    if (!ok) { std::printf("FAIL: %s\n", what); ++g_fail; }
}

template <typename Fn>
Fn Resolve(HMODULE module, const char* name)
{
    return reinterpret_cast<Fn>(GetProcAddress(module, name));
}

} // namespace

int main()
{
    // 1. Runtime registry: gdi always, skia replaced by the plugin.
    RegisterPlatformPaintProviders();
    RegisterSkiaPluginProvider();
    PainterRegistry& reg = PainterRegistry::Instance();

    Check(reg.Backends().size() == 2u, "gdi + skia registered");
    Check(reg.Available("skia"), "runtime skia backend available");
    Check(SkiaPluginLoaded(), "airui-skia.dll was loaded dynamically");
    Check(reg.Available("gdi"), "gdi backend still available");

    // 2. Select skia at runtime and drive it through the vtable.
    PaintTarget target{ nullptr, Rect{ 0, 0, 64, 48 } };
    std::unique_ptr<IPainter> painter = reg.Begin("skia", target);
    Check(painter != nullptr, "runtime-selected skia produced a painter");
    if (painter) {
        painter->FillRect(Rect{ 0, 0, 64, 48 }, MakeColor(0xDD, 0x00, 0x00));
        painter->FillRound(Rect{ 2, 2, 30, 20 }, 4, MakeColor(0xFF, 0xFF, 0xFF));
        painter->Text(Rect{ 4, 4, 60, 24 }, L"airui plugin",
                      FontRole::Body, MakeColor(0x11, 0x22, 0x33),
                      dt::SingleLine);
        painter.reset();   // flushes the frame inside the DLL
    }

    // 3. Runtime fallback when the DLL-backed backend cannot open a
    //    frame (zero-sized target): gdi must still produce a painter.
    PaintTarget broken{ nullptr, Rect{ 0, 0, 0, 0 } };
    std::unique_ptr<IPainter> fallback = reg.Begin("skia", broken);
    Check(fallback != nullptr, "zero-sized skia frame falls back to gdi");
    fallback.reset();

    // 4. Exercise the raw C ABI directly: create / draw / raster / destroy.
    HMODULE dll = LoadLibraryW(AIRUI_SKIA_PLUGIN_DLL);
    Check(dll != nullptr, "plugin DLL loadable next to the exe");
    if (dll) {
        AiruiSkiaAbiFn       abi      = Resolve<AiruiSkiaAbiFn>(dll, AIRUI_SKIA_FN_ABI);
        AiruiSkiaCreateFn    create   = Resolve<AiruiSkiaCreateFn>(dll, AIRUI_SKIA_FN_CREATE);
        AiruiSkiaDestroyFn   destroy  = Resolve<AiruiSkiaDestroyFn>(dll, AIRUI_SKIA_FN_DESTROY);
        AiruiSkiaPixelsFn    pixels   = Resolve<AiruiSkiaPixelsFn>(dll, AIRUI_SKIA_FN_PIXELS);
        AiruiSkiaRowBytesFn  rowBytes = Resolve<AiruiSkiaRowBytesFn>(dll, AIRUI_SKIA_FN_ROW_BYTES);

        Check(abi && create && destroy && pixels && rowBytes,
              "all plugin C ABI entry points exported");
        if (abi && create && destroy && pixels && rowBytes) {
            Check(abi() == AIRUI_SKIA_PLUGIN_ABI, "plugin ABI version matches");
            void* handle = create(nullptr, 0, 0, 32, 32);
            Check(handle != nullptr, "plugin created an off-screen frame");
            if (handle) {
                IPainter* raw = reinterpret_cast<IPainter*>(handle);
                raw->FillRect(Rect{ 0, 0, 32, 32 }, MakeColor(0xDD, 0x00, 0x00));

                const unsigned char* px =
                    static_cast<const unsigned char*>(pixels(handle));
                Check(px != nullptr, "raster pixels available through C ABI");
                Check(rowBytes(handle) >= 32u * 4u, "row stride is sane");
                if (px) {
                    const size_t i = 16u * static_cast<size_t>(rowBytes(handle)) + 16u * 4u;
                    // core 0x000000DD -> BGRA bytes 00 00 DD FF
                    Check(px[i + 0] == 0x00, "pixel blue channel");
                    Check(px[i + 1] == 0x00, "pixel green channel");
                    Check(px[i + 2] == 0xDD, "pixel red channel");
                    Check(px[i + 3] == 0xFF, "pixel alpha channel");
                }
                destroy(handle);
            }
        }
        FreeLibrary(dll);
    }

    if (g_fail == 0)
        std::printf("test_skia_plugin: runtime DLL backend + C ABI + raster OK\n");
    else
        std::printf("test_skia_plugin: %d check(s) failed\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
