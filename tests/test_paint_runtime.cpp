// ============================================================
// test_paint_runtime.cpp
// Windows-only, NO Skia SDK required.
//
// Verifies the promise that runtime backend selection works in the
// official default build: with no Skia DLL deployed, asking for "skia"
// must not fail - it resolves to a painter and falls back to gdi.
// When airui-skia.dll IS present the same assertions run against the
// real plugin provider instead.
// ============================================================
#include "airui/PaintBackend.h"
#include "airui/PaintProviders.h"

#include <cstdio>
#include <string>

using namespace air::ui;

namespace {

int g_fail = 0;

void Check(bool ok, const char* what)
{
    if (!ok) { std::printf("FAIL: %s\n", what); ++g_fail; }
}

} // namespace

int main()
{
    RegisterPlatformPaintProviders();
    PainterRegistry& reg = PainterRegistry::Instance();

    const PaintTarget target{ nullptr, Rect{ 0, 0, 32, 32 } };

    Check(reg.Available("gdi"), "gdi provider is always available");
    Check(reg.Backends().size() == 2u, "gdi + skia ids registered");

    const bool skiaAvailable = reg.Available("skia");
    std::printf("skia backend %s\n",
                skiaAvailable ? "available (linked or DLL loaded)"
                              : "not deployed (fallback path under test)");

    // The point of this test: a runtime request never depends on a
    // preprocessor flag and never leaves the frame without a painter.
    std::unique_ptr<IPainter> p = reg.Begin("skia", target);
    Check(p != nullptr, "runtime skia request yields a painter");
    if (p) {
        p->FillRect(Rect{ 0, 0, 16, 16 }, MakeColor(0x11, 0x22, 0x33));
        p.reset();
    }
    if (!skiaAvailable)
        Check(reg.LastError().find("fell back to gdi") != std::string::npos,
              "missing plugin records a gdi fallback");

    std::unique_ptr<IPainter> g = reg.Begin("gdi", target);
    Check(g != nullptr, "explicit gdi request yields a painter");
    if (g) { g->FillRect(Rect{ 0, 0, 8, 8 }, 0); g.reset(); }

    // A hot default switch must not require recompilation.
    reg.SetDefault("skia");
    std::unique_ptr<IPainter> a = reg.Begin("auto", target);
    Check(a != nullptr, "auto follows the runtime default (skia)");
    a.reset();
    reg.SetDefault("gdi");
    a = reg.Begin("auto", target);
    Check(a != nullptr, "auto follows the runtime default (gdi)");
    a.reset();

    if (g_fail == 0)
        std::printf("test_paint_runtime: runtime selection + fallback OK\n");
    else
        std::printf("test_paint_runtime: %d check(s) failed\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
