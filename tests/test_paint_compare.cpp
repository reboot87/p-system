// ============================================================
// test_paint_compare.cpp
// Make the GDI / Skia difference MEASURABLE.
//
// Both backends receive the exact same IPainter call sequence on the
// exact same 32bpp BGRA DIB, so any pixel difference is caused by the
// rasteriser, not by the layout or the draw calls. The comparison is
// split by primitive (round corners, alpha rect, dashed border, text)
// so the report also says WHERE the two backends diverge.
//
// Runs in the default CMake + VS2017 build with no Skia SDK: when
// airui-skia.dll is not deployed it prints the GDI baseline and skips
// the comparison. The skia-windows CI job (which has the DLL) produces
// the full GDI-vs-Skia report.
// ============================================================
#include "airui/PaintBackend.h"
#include "airui/PaintProviders.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace air::ui;

namespace {

constexpr int    kW = 240;
constexpr int    kH = 120;
constexpr size_t kBytes = static_cast<size_t>(kW) * kH * 4u;

using Image = std::vector<uint8_t>;   // top-down BGRA, 4 bytes per pixel

const Color kBg   = MakeColor(0xF2, 0xF2, 0xF2);
const Color kBlue = MakeColor(0x2D, 0x6C, 0xDF);
const Color kRed  = MakeColor(0xE2, 0x4A, 0x4A);
const Color kInk  = MakeColor(0x33, 0x33, 0x33);

struct Surface
{
    HDC           dc   = nullptr;
    HBITMAP       bmp  = nullptr;
    HGDIOBJ       old  = nullptr;
    unsigned char* bits = nullptr;

    bool Init()
    {
        BITMAPINFO bmi{};
        BITMAPINFOHEADER& bh = bmi.bmiHeader;
        bh.biSize        = sizeof(BITMAPINFOHEADER);
        bh.biWidth       = kW;
        bh.biHeight      = -kH;            // top-down, same layout as Skia
        bh.biPlanes      = 1;
        bh.biBitCount    = 32;
        bh.biCompression = BI_RGB;

        dc = CreateCompatibleDC(nullptr);
        if (!dc) return false;
        bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS,
                               reinterpret_cast<void**>(&bits), nullptr, 0);
        if (!bmp || !bits) return false;
        old = SelectObject(dc, bmp);
        return true;
    }

    void Clear() { std::memset(bits, 0xFF, kBytes); }   // opaque white

    Image Capture() const
    {
        Image out(kBytes);
        std::memcpy(out.data(), bits, kBytes);
        return out;
    }

    ~Surface()
    {
        if (dc && old) SelectObject(dc, old);
        if (bmp) DeleteObject(bmp);
        if (dc)  DeleteDC(dc);
    }
};

// Every backend receives this identical call sequence.
void DrawScene(IPainter& p)
{
    p.FillRect(Rect{ 0, 0, kW, kH }, kBg);
    p.FillRound(Rect{ 12, 12, 108, 64 }, 12, kBlue);
    p.FillRectAlpha(Rect{ 120, 12, 228, 44 }, kRed, 128);
    p.StrokeDashed(Rect{ 12, 78, 228, 110 }, kInk, 1, 6);
    p.Text(Rect{ 120, 52, 228, 76 }, L"Skia vs GDI 123",
           FontRole::Body, kInk, dt::SingleLine);
}

uint32_t Checksum(const Image& img)
{
    uint32_t h = 2166136261u;
    for (uint8_t b : img) { h ^= b; h *= 16777619u; }
    return h;
}

inline int Delta(const uint8_t* a, const uint8_t* b)
{
    return static_cast<int>(a[0] > b[0] ? a[0] - b[0] : b[0] - a[0])
         + static_cast<int>(a[1] > b[1] ? a[1] - b[1] : b[1] - a[1])
         + static_cast<int>(a[2] > b[2] ? a[2] - b[2] : b[2] - a[2]);
}

inline int Delta3(const uint8_t* a, uint8_t b, uint8_t g, uint8_t r)
{
    const uint8_t ref[3] = { b, g, r };
    return Delta(a, ref);
}

struct Diff
{
    size_t   pixels = 0;
    size_t   total  = 0;
    uint64_t sum    = 0;

    double Pct()   const { return total ? 100.0 * pixels / total : 0.0; }
    double Mean()  const { return total ? static_cast<double>(sum) / total : 0.0; }
};

Diff CompareRegion(const Image& a, const Image& b, const Rect& r)
{
    Diff d;
    for (int y = r.top; y < r.bottom; ++y) {
        for (int x = r.left; x < r.right; ++x) {
            const size_t i = (static_cast<size_t>(y) * kW + x) * 4u;
            const int delta = Delta(&a[i], &b[i]);
            d.sum += static_cast<uint64_t>(delta);
            if (delta > 12) ++d.pixels;
            ++d.total;
        }
    }
    return d;
}

// Pixels that are neither the flat background nor the pure foreground:
// these are anti-aliased edges / alpha blends. GDI regions are hard
// edged (CreateRoundRectRgn), Skia's paint enables anti-aliasing.
size_t IntermediatePixels(const Image& img, const Rect& r, Color fg)
{
    const uint8_t bgB = static_cast<uint8_t>(ColorB(kBg));
    const uint8_t bgG = static_cast<uint8_t>(ColorG(kBg));
    const uint8_t bgR = static_cast<uint8_t>(ColorR(kBg));
    const uint8_t fgB = static_cast<uint8_t>(ColorB(fg));
    const uint8_t fgG = static_cast<uint8_t>(ColorG(fg));
    const uint8_t fgR = static_cast<uint8_t>(ColorR(fg));

    size_t n = 0;
    for (int y = r.top; y < r.bottom; ++y) {
        for (int x = r.left; x < r.right; ++x) {
            const size_t i = (static_cast<size_t>(y) * kW + x) * 4u;
            const int dbg = Delta3(&img[i], bgB, bgG, bgR);
            const int dfg = Delta3(&img[i], fgB, fgG, fgR);
            if (dbg > 12 && dfg > 12) ++n;
        }
    }
    return n;
}

struct Region { const char* name; Rect r; };

} // namespace

int main()
{
    Surface surface;
    if (!surface.Init()) {
        std::printf("test_paint_compare: FAILED to create a 32bpp DIB\n");
        return 1;
    }

    RegisterPlatformPaintProviders();
    PainterRegistry& reg = PainterRegistry::Instance();

    // A build with a linked-in Skia backend has a static provider; a
    // default build has the DLL-backed one. Force the DLL route when the
    // current providers cannot supply "skia", so this test measures the
    // same runtime path in both build flavours.
    if (!reg.Available("skia")) {
        RegisterSkiaPluginProvider();
    }

    std::string available;
    for (const std::string& id : reg.Backends()) {
        if (!available.empty()) available += ",";
        available += id + (reg.Available(id) ? "(ready)" : "(missing)");
    }

    const bool skiaReady = reg.Available("skia");
    std::printf("test_paint_compare: GDI vs Skia raster, %dx%d 32bpp BGRA\n", kW, kH);
    std::printf("  providers        : %s\n", available.c_str());
    std::printf("  airui-skia.dll   : %s\n",
                SkiaPluginLoaded() ? "loaded" : "not loaded / static provider");

    auto render = [&](const char* id) -> Image {
        surface.Clear();
        PaintTarget target{ surface.dc, Rect{ 0, 0, kW, kH } };
        std::unique_ptr<IPainter> painter = reg.Begin(id, target);
        if (!painter) {
            std::printf("  %-6s           : FAILED to open a frame (%s)\n",
                        id, reg.LastError().c_str());
            return {};
        }
        DrawScene(*painter);
        painter.reset();     // Skia composites to the HDC here
        return surface.Capture();
    };

    const Image gdi = render("gdi");
    const std::string gdiError = reg.LastError();
    if (gdi.empty()) {
        std::printf("test_paint_compare: FAILED - gdi produced no raster\n");
        return 1;
    }

    Image skia;
    std::string skiaError;
    if (skiaReady) {
        skia = render("skia");
        skiaError = reg.LastError();
    }

    const Rect round{ 12, 12, 108, 64 };
    std::printf("  gdi  checksum    : 0x%08X   round-edge intermediates: %zu\n",
                Checksum(gdi), IntermediatePixels(gdi, round, kBlue));

    if (!skia.empty()) {
        std::printf("  skia checksum    : 0x%08X   round-edge intermediates: %zu\n",
                    Checksum(skia), IntermediatePixels(skia, round, kBlue));
    }
    if (!skiaError.empty())
        std::printf("  skia fallback    : %s\n", skiaError.c_str());
    if (!gdiError.empty() && gdiError.find("fell back") == std::string::npos)
        std::printf("  gdi note         : %s\n", gdiError.c_str());

    if (skia.empty()) {
        std::printf("  comparison       : SKIPPED (deploy airui-skia.dll + its runtime DLLs next to this exe)\n");
        std::printf("test_paint_compare: GDI baseline OK\n");
        return 0;
    }

    static const Region regions[] = {
        { "whole frame   ", Rect{ 0, 0, kW, kH } },
        { "round corners ", round },
        { "alpha rect    ", Rect{ 120, 12, 228, 44 } },
        { "dashed border ", Rect{ 12, 78, 228, 110 } },
        { "text          ", Rect{ 120, 52, 228, 76 } },
    };

    std::printf("  differing pixels by region (|dB|+|dG|+|dR| > 12):\n");
    for (const Region& r : regions) {
        const Diff d = CompareRegion(gdi, skia, r.r);
        std::printf("    %s %5zu / %5zu px  (%5.1f%%)  mean delta %5.1f\n",
                    r.name, d.pixels, d.total, d.Pct(), d.Mean());
    }

    if (Checksum(gdi) == Checksum(skia))
        std::printf("  WARNING: identical rasters - is Skia really painting?\n");

    std::printf("test_paint_compare: GDI + Skia raster comparison OK\n");
    return 0;
}
