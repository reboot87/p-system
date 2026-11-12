// ============================================================
// test_paint_compare.cpp
// Make the GDI / Skia difference MEASURABLE, and keep it from drifting.
//
// Both backends receive the exact same call sequence on the exact same
// 32bpp BGRA DIB, so any pixel difference is caused by the rasteriser,
// not by the layout or the draw calls. The sequence goes through
// PaintFrame -- the router Shell::Paint opens for a real frame -- so text
// follows the production policy (backend text when the plugin can
// rasterise CJK, host replay otherwise) instead of disappearing into a
// bare backend that has no font table.
//
// What this fixes over a hand-run screenshot diff:
//   * the scene covers EVERY IPainter primitive (fill, alpha fill,
//     round fill, rect/round/dashed strokes, text, text block, nested
//     clip, translate), so a backend losing one of them is visible;
//   * it reports SSIM (luma, 8x8 windows) next to the raw differing
//     pixel ratio, so the number means something perceptually;
//   * it asserts backend-independent properties instead of exact
//     pixels: each backend must be deterministic, and per region the
//     two must agree on how much is inked and on the mean colour;
//   * it runs in the default CMake + VS2017 build with no Skia SDK:
//     without airui-skia.dll it prints the GDI baseline and skips the
//     cross-backend checks. The skia-windows CI job (which has the
//     DLL) exercises the full gate.
// ============================================================
#include "airui/PaintBackend.h"
#include "airui/PaintProviders.h"

#include <windows.h>

#include "airui/Fonts.h"
#include "airui/PaintFrame.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace air::ui;

namespace {

constexpr int    kW = 320;
constexpr int    kH = 240;
constexpr size_t kBytes = static_cast<size_t>(kW) * kH * 4u;

using Image = std::vector<uint8_t>;   // top-down BGRA, 4 bytes per pixel

const Color kBg    = MakeColor(0xF2, 0xF2, 0xF2);
const Color kBlue  = MakeColor(0x2D, 0x6C, 0xDF);
const Color kGreen = MakeColor(0x2E, 0xA0, 0x6A);
const Color kRed   = MakeColor(0xE2, 0x4A, 0x4A);
const Color kInk   = MakeColor(0x33, 0x33, 0x33);

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

// Rounded rect, rounded stroke, two alpha levels, a dashed border, both
// text entry points, a nested clip that must not bleed, and a translate
// that must land where the caller asked.
void DrawScene(IPainter& p)
{
    p.FillRect(Rect{ 0, 0, kW, kH }, kBg);

    p.FillRound(Rect{ 12, 12, 108, 64 }, 12, kBlue);
    p.StrokeRound(Rect{ 112, 12, 208, 64 }, 10, kInk, 2);
    p.FillRectAlpha(Rect{ 212, 12, 308, 64 }, kRed, 128);
    p.FillRectAlpha(Rect{ 212, 72, 308, 100 }, kRed, 64);

    p.StrokeRect(Rect{ 12, 72, 208, 100 }, kInk, 1);
    p.StrokeDashed(Rect{ 12, 108, 208, 152 }, kInk, 1, 6);

    p.Text(Rect{ 12, 158, 208, 182 }, L"Skia vs GDI 123",
           FontRole::Body, kInk, dt::SingleLine | dt::VCenter);
    p.TextBlock(Rect{ 12, 186, 208, 232 },
                L"The quick brown fox jumps over the lazy dog. "
                L"Second line wraps here, third one is clipped.",
                FontRole::Meta, kInk, 2);

    // Clip: the fill is deliberately much larger than the clip.
    p.PushClip(Rect{ 212, 108, 308, 176 });
    p.FillRect(Rect{ 200, 96, 320, 188 }, kGreen);
    p.PopClip();

    // Translate is additive; PushClip/PopClip restores the viewport origin.
    p.PushClip(Rect{ 0, 0, kW, kH });
    p.Translate(Point{ 4, 4 });
    p.FillRect(Rect{ 212, 190, 300, 226 }, kBlue);
    p.PopClip();
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

struct Diff
{
    size_t   pixels = 0;     // |dB|+|dG|+|dR| > 12
    size_t   total  = 0;
    uint64_t sum    = 0;

    double Pct()  const { return total ? 100.0 * pixels / total : 0.0; }
    double Mean() const { return total ? static_cast<double>(sum) / total : 0.0; }
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

// How much of a region is covered by anything other than the background.
// Backend-independent, so the two can be compared without caring about
// anti-aliasing: a dropped primitive changes this a lot, a different
// edge filter barely moves it.
size_t InkedPixels(const Image& img, const Rect& r)
{
    const uint8_t bgB = static_cast<uint8_t>(ColorB(kBg));
    const uint8_t bgG = static_cast<uint8_t>(ColorG(kBg));
    const uint8_t bgR = static_cast<uint8_t>(ColorR(kBg));
    size_t n = 0;
    for (int y = r.top; y < r.bottom; ++y) {
        for (int x = r.left; x < r.right; ++x) {
            const size_t i = (static_cast<size_t>(y) * kW + x) * 4u;
            const int db = img[i + 0] > bgB ? img[i + 0] - bgB : bgB - img[i + 0];
            const int dg = img[i + 1] > bgG ? img[i + 1] - bgG : bgG - img[i + 1];
            const int dr = img[i + 2] > bgR ? img[i + 2] - bgR : bgR - img[i + 2];
            if (db + dg + dr > 12) ++n;
        }
    }
    return n;
}

// Mean luma over a region, 0..255.
double MeanLuma(const Image& img, const Rect& r)
{
    double sum = 0.0;
    size_t n = 0;
    for (int y = r.top; y < r.bottom; ++y) {
        for (int x = r.left; x < r.right; ++x) {
            const size_t i = (static_cast<size_t>(y) * kW + x) * 4u;
            sum += 0.114 * img[i + 0] + 0.587 * img[i + 1] + 0.299 * img[i + 2];
            ++n;
        }
    }
    return n ? sum / n : 0.0;
}

// SSIM on the luma channel with 8x8 windows, averaged over the frame.
// Standard constants for 8-bit data; higher is closer to identical.
double SSIM(const Image& a, const Image& b)
{
    constexpr int kWin = 8;
    constexpr double C1 = 6.5025;    // (0.01 * 255)^2
    constexpr double C2 = 58.5225;   // (0.03 * 255)^2

    auto luma = [](const Image& img, int x, int y) {
        const size_t i = (static_cast<size_t>(y) * kW + x) * 4u;
        return 0.114 * img[i + 0] + 0.587 * img[i + 1] + 0.299 * img[i + 2];
    };

    double total = 0.0;
    int    windows = 0;
    for (int wy = 0; wy + kWin <= kH; wy += kWin) {
        for (int wx = 0; wx + kWin <= kW; wx += kWin) {
            double ma = 0.0, mb = 0.0;
            for (int y = 0; y < kWin; ++y)
                for (int x = 0; x < kWin; ++x) {
                    ma += luma(a, wx + x, wy + y);
                    mb += luma(b, wx + x, wy + y);
                }
            const double n = kWin * kWin;
            ma /= n; mb /= n;

            double va = 0.0, vb = 0.0, cov = 0.0;
            for (int y = 0; y < kWin; ++y)
                for (int x = 0; x < kWin; ++x) {
                    const double da = luma(a, wx + x, wy + y) - ma;
                    const double db = luma(b, wx + x, wy + y) - mb;
                    va += da * da;
                    vb += db * db;
                    cov += da * db;
                }
            va /= n - 1.0; vb /= n - 1.0; cov /= n - 1.0;

            const double num = (2.0 * ma * mb + C1) * (2.0 * cov + C2);
            const double den = (ma * ma + mb * mb + C1) * (va + vb + C2);
            total += (den != 0.0) ? num / den : 1.0;
            ++windows;
        }
    }
    return windows ? total / windows : 0.0;
}

struct Region { const char* name; Rect r; };

// Backend-independent acceptance limits. They are deliberately about
// structure (is the same primitive there, in the same place, at the same
// weight), not about anti-aliasing, which legitimately differs.
constexpr double kMaxMeanLumaDelta = 12.0;   // 0..255
constexpr double kMaxInkRatioDelta = 0.25;   // relative coverage
constexpr double kMinSSIM           = 0.80;
constexpr double kMaxDiffPct        = 12.0;

bool Check(bool ok, const char* what)
{
    if (!ok) std::printf("    FAILED: %s\n", what);
    return ok;
}

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
    std::printf("test_paint_compare: GDI vs Skia raster, %dx%d 32bpp BGRA\n", kW, kH);    std::printf("  providers        : %s\n", available.c_str());
    std::printf("  airui-skia.dll   : %s\n",
                SkiaPluginLoaded() ? "loaded" : "not loaded / static provider");

    // Host-phase text needs the app's real HFONTs; the singleton creates
    // them lazily on first use. backendOwnsText mirrors what Shell::Paint
    // passes when it opens a frame for the Skia backend.
    Fonts& fonts = Fonts::Default();
    const bool backendOwnsText = SkiaPluginSupportsWideText();
    std::printf("  skia text        : %s\n",
                backendOwnsText ? "backend" : "host replay");

    auto render = [&](const char* id) -> Image {
        surface.Clear();
        PaintTarget target{ surface.dc, Rect{ 0, 0, kW, kH } };
        std::unique_ptr<IPainter> painter = reg.Begin(id, target);
        if (!painter) {
            std::printf("  %-6s           : FAILED to open a frame (%s)\n",
                        id, reg.LastError().c_str());
            return {};
        }

        // Drive the scene through PaintFrame, the router the application
        // actually uses, instead of handing the raw IPainter to the scene.
        // That matters for text: a bare backend has no font table, so the
        // Skia plugin silently drops Text() (it measured 0 % ink) and the
        // text regions were not comparable at all. With a frame open, text
        // follows the production policy -- backend text when the plugin
        // can rasterise CJK, host replay on the target DC otherwise.
        PaintFrame& frame = PaintFrame::Instance();
        frame.Begin(painter.get(), surface.dc, &fonts,
                    backendOwnsText && !std::strcmp(id, "skia"));
        DrawScene(frame);
        frame.End();
        painter.reset();      // the backend composites onto surface.dc here
        frame.ReplayText();   // host text on top of the composite
        frame.Clear();
        return surface.Capture();
    };

    const Image gdi = render("gdi");
    const std::string gdiError = reg.LastError();
    if (gdi.empty()) {
        std::printf("test_paint_compare: FAILED - gdi produced no raster\n");
        return 1;
    }

    bool ok = true;

    // Determinism: the same backend must produce the same pixels twice.
    // This catches leaked painter state (viewport origin, clip, colour)
    // between frames, which earlier showed up as drifting anti-aliasing.
    const Image gdi2 = render("gdi");
    std::printf("  gdi  checksum    : 0x%08X (repeat 0x%08X)\n",
                Checksum(gdi), Checksum(gdi2));
    ok = Check(Checksum(gdi) == Checksum(gdi2),
               "gdi raster is deterministic") && ok;

    Image skia;
    std::string skiaError;
    if (skiaReady) {
        skia = render("skia");
        skiaError = reg.LastError();
        if (!skia.empty()) {
            const Image skia2 = render("skia");
            ok = Check(Checksum(skia) == Checksum(skia2),
                       "skia raster is deterministic") && ok;
        }
    }
    if (!skiaError.empty())
        std::printf("  skia fallback    : %s\n", skiaError.c_str());
    if (!gdiError.empty() && gdiError.find("fell back") == std::string::npos)
        std::printf("  gdi note         : %s\n", gdiError.c_str());

    if (skia.empty()) {
        std::printf("  comparison       : SKIPPED (deploy airui-skia.dll + its runtime DLLs next to this exe)\n");
        std::printf("test_paint_compare: GDI baseline %s\n", ok ? "OK" : "FAILED");
        return ok ? 0 : 1;
    }

    static const Region regions[] = {
        { "whole frame   ", Rect{ 0, 0, kW, kH } },
        { "round fill    ", Rect{ 12, 12, 108, 64 } },
        { "round stroke  ", Rect{ 112, 12, 208, 64 } },
        { "alpha 50%     ", Rect{ 212, 12, 308, 64 } },
        { "alpha 25%     ", Rect{ 212, 72, 308, 100 } },
        { "rect stroke   ", Rect{ 12, 72, 208, 100 } },
        { "dashed border ", Rect{ 12, 108, 208, 152 } },
        { "text body     ", Rect{ 12, 158, 208, 182 } },
        { "text block    ", Rect{ 12, 186, 208, 232 } },
        { "clip          ", Rect{ 212, 106, 310, 178 } },
        { "translate     ", Rect{ 216, 194, 304, 230 } },
    };

    std::printf("  agreement (structure, not anti-aliasing):\n");
    std::printf("    %-14s %8s %8s %8s %8s\n",
                "region", "diff%", "inkA%", "inkB%", "luma d");
    for (const Region& r : regions) {
        const Diff d = CompareRegion(gdi, skia, r.r);
        const double area = static_cast<double>(d.total ? d.total : 1);
        const double inA = 100.0 * InkedPixels(gdi, r.r) / area;
        const double inB = 100.0 * InkedPixels(skia, r.r) / area;
        const double lumaA = MeanLuma(gdi, r.r);
        const double lumaB = MeanLuma(skia, r.r);
        const double lumaD = std::fabs(lumaA - lumaB);

        std::printf("    %-14s %7.1f%% %7.1f%% %7.1f%% %8.1f\n",
                    r.name, d.Pct(), inA, inB, lumaD);

        const double inkRatio = (inA > inB ? inA : inB) >= 2.0
                              ? std::fabs(inA - inB) / (inA > inB ? inA : inB)
                              : 0.0;
        char label[96];
        std::snprintf(label, sizeof(label), "%s ink agrees", r.name);
        ok = Check(inkRatio <= kMaxInkRatioDelta, label) && ok;
        std::snprintf(label, sizeof(label), "%s luma agrees", r.name);
        ok = Check(lumaD <= kMaxMeanLumaDelta, label) && ok;
    }

    const Diff whole = CompareRegion(gdi, skia, Rect{ 0, 0, kW, kH });
    const double ssim = SSIM(gdi, skia);
    std::printf("  whole frame      : %.1f%% differing pixels, SSIM %.4f\n",
                whole.Pct(), ssim);
    ok = Check(whole.Pct() <= kMaxDiffPct, "differing pixels within budget") && ok;
    ok = Check(ssim >= kMinSSIM, "SSIM above floor") && ok;

    if (Checksum(gdi) == Checksum(skia))
        std::printf("  WARNING: identical rasters - is Skia really painting?\n");

    std::printf("test_paint_compare: GDI + Skia raster %s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
