// ============================================================
// test_font_lab.cpp
// WHY does the same font look different in the two backends?
//
// test_paint_compare answers "how much" (a percentage per region).
// This tool answers "what, exactly", by decomposing the difference:
//
//   * the same LOGFONT is rendered three times on the same 32bpp DIB:
//       ct  GDI, CLEARTYPE_QUALITY   (the shipping Win32 path)
//       aa  GDI, ANTIALIASED_QUALITY (control: GDI's own grayscale
//                                     rasteriser, same size/hinting)
//       sk  Skia, through airui-skia.dll (the shipping Skia path)
//   * an ink map in ASCII, one character per device pixel, so stem
//     widths, subpixel phases and hinting snapping are visible;
//   * per-channel coverage digits for the densest row, which shows how
//     each engine distributes energy over the R/G/B subpixels;
//   * coverage totals, histogram, colour fringe and ink bbox -- the
//     numbers behind "it looks bolder / softer / shifted";
//   * the mean L1 raster difference of ct-vs-sk and aa-vs-sk. If the
//     aa control sits far closer to Skia than ct does, the glyph
//     outlines agree and the residual gap is ClearType's compositing,
//     not the font, the size or the hinting.
//   * GDI's own metrics for the LOGFONT (cell, ascent, advances), the
//     ground truth Skia is supposed to reproduce.
//
// Usage:
//   test_font_lab               all samples
//   test_font_lab 2             one role (FontRole 0..7)
//   test_font_lab --raw         also dump raw RGB of the densest row
//
// Without airui-skia.dll it prints the two GDI columns only.
// ============================================================
#include "airui/PaintBackend.h"
#include "airui/PaintProviders.h"
#include "airui/UiText.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace air::ui;

namespace {

constexpr int    kW = 320;
constexpr int    kH = 40;          // one band per sample
constexpr size_t kBytes = static_cast<size_t>(kW) * kH * 4u;

using Image = std::vector<uint8_t>;    // top-down BGRA

// Pure black on pure white: coverage can be read straight off the
// luminance, so a difference in coverage is never confused with a
// difference in the ink colour.
const Color kBg  = MakeColor(0xFF, 0xFF, 0xFF);
const Color kInk = MakeColor(0x00, 0x00, 0x00);

struct Surface
{
    HDC            dc   = nullptr;
    HBITMAP        bmp  = nullptr;
    HGDIOBJ        old  = nullptr;
    unsigned char* bits = nullptr;

    bool Init()
    {
        BITMAPINFO bmi{};
        BITMAPINFOHEADER& bh = bmi.bmiHeader;
        bh.biSize        = sizeof(BITMAPINFOHEADER);
        bh.biWidth       = kW;
        bh.biHeight      = -kH;
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

    void Clear() { std::memset(bits, 0xFF, kBytes); }

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

// ------------------------------------------------------------
// Ink measurement
// ------------------------------------------------------------
struct Ink
{
    int    x0 = 0, y0 = 0, x1 = -1, y1 = -1;   // inclusive bbox
    int    count  = 0;
    double sum    = 0.0;                       // total coverage, in pixels
    double peak   = 0.0;
    double fringe = 0.0;                       // mean channel spread on ink
    double hist[10] = {};

    int  W() const { return x1 - x0 + 1; }
    int  H() const { return y1 - y0 + 1; }
    bool Empty() const { return count == 0; }
};

// Per-pixel coverage averaged over the three subpixels. On white with
// black ink that is (255 - value) / 255 per channel.
inline double Cov(const uint8_t* p)
{
    return ((255.0 - p[0]) + (255.0 - p[1]) + (255.0 - p[2])) / (3.0 * 255.0);
}

Ink Measure(const Image& img)
{
    Ink k;
    k.x0 = kW; k.y0 = kH;
    double fringeSum = 0.0;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const uint8_t* p = &img[(static_cast<size_t>(y) * kW + x) * 4u];
            const double c = Cov(p);
            if (c <= 0.02) continue;
            ++k.count;
            k.sum += c;
            if (c > k.peak) k.peak = c;
            int lo = p[0], hi = p[0];
            for (int i = 1; i < 3; ++i) {
                if (p[i] < lo) lo = p[i];
                if (p[i] > hi) hi = p[i];
            }
            fringeSum += (hi - lo);
            int bin = static_cast<int>(c * 10.0);
            if (bin > 9) bin = 9;
            k.hist[bin] += 1.0;
            if (x < k.x0) k.x0 = x;
            if (x > k.x1) k.x1 = x;
            if (y < k.y0) k.y0 = y;
            if (y > k.y1) k.y1 = y;
        }
    }
    if (k.count > 0) {
        k.fringe = fringeSum / k.count;
        for (int i = 0; i < 10; ++i) k.hist[i] /= k.count;
    } else {
        k.x0 = 0; k.y0 = 0; k.x1 = -1; k.y1 = -1;
    }
    return k;
}

// Mean absolute per-channel difference over a rect, and how many pixels
// differ by more than 24 in any channel.
struct L1 { double mean = 0.0; int hot = 0; int total = 0; };

L1 RasterDiff(const Image& a, const Image& b, int x0, int y0, int x1, int y1)
{
    L1 d;
    long long sum = 0;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const size_t i = (static_cast<size_t>(y) * kW + x) * 4u;
            int worst = 0;
            for (int c = 0; c < 3; ++c) {
                const int dv = std::abs(static_cast<int>(a[i + c]) -
                                        static_cast<int>(b[i + c]));
                sum += dv;
                if (dv > worst) worst = dv;
            }
            if (worst > 24) ++d.hot;
            ++d.total;
        }
    }
    d.mean = d.total ? static_cast<double>(sum) / (3.0 * d.total) : 0.0;
    return d;
}

// Coverage as a ramp. ' ' = bare background, '@' = solid ink.
char Shade(double c)
{
    static const char* ramp = " .:-=+*#%@";
    int i = static_cast<int>(c * 9.999);
    if (i < 0) i = 0;
    if (i > 9) i = 9;
    return ramp[i];
}

std::string ShadeRow(const Image& img, int y, int x0, int x1)
{
    std::string s;
    for (int x = x0; x <= x1; ++x)
        s += Shade(Cov(&img[(static_cast<size_t>(y) * kW + x) * 4u]));
    return s;
}

// Per-channel coverage as a digit: 0 = background, 9 = full ink. Three
// rows (B/G/R) of this show the LCD subpixel split directly; three
// identical rows mean the engine is drawing grayscale text.
std::string ChanRow(const Image& img, int y, int x0, int x1, int chan)
{
    std::string s;
    for (int x = x0; x <= x1; ++x) {
        const uint8_t* p = &img[(static_cast<size_t>(y) * kW + x) * 4u];
        int v = static_cast<int>((255 - p[chan]) * 10.0 / 256.0);
        if (v < 0) v = 0;
        if (v > 9) v = 9;
        s += static_cast<char>('0' + v);
    }
    return s;
}

// Row carrying the most ink: the densest body row, so the channel dump
// lands on real stems instead of on an ascender tip.
int DensestRow(const Image& img, const Ink& k)
{
    if (k.Empty()) return 0;
    int best = k.y0; double bestSum = -1.0;
    for (int y = k.y0; y <= k.y1; ++y) {
        double s = 0.0;
        for (int x = k.x0; x <= k.x1; ++x)
            s += Cov(&img[(static_cast<size_t>(y) * kW + x) * 4u]);
        if (s > bestSum) { bestSum = s; best = y; }
    }
    return best;
}

// Widths of the runs that are more than half covered on one row: the
// most legible proxy for "is this text bolder than that one".
std::string StemProfile(const Image& img, int y, const Ink& k)
{
    if (k.Empty()) return std::string();
    std::string runs;
    int  cur = 0;
    bool inRun = false;
    for (int x = k.x0; x <= k.x1; ++x) {
        const bool ink = Cov(&img[(static_cast<size_t>(y) * kW + x) * 4u]) > 0.5;
        if (ink) { ++cur; inRun = true; }
        else if (inRun) {
            runs += std::to_string(cur);
            runs += ' ';
            cur = 0;
            inRun = false;
        }
    }
    if (inRun) runs += std::to_string(cur);
    return runs;
}

// ------------------------------------------------------------
// GDI rendering + ground truth
// ------------------------------------------------------------
HFONT MakeGdiFont(FontRole role, BYTE quality)
{
    const FontSpec spec = RoleFontSpec(role);
    const bool isIcon = (role == FontRole::Icon || role == FontRole::IconLarge);
    return CreateFontW(-spec.cell, 0, 0, 0, spec.weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       quality, DEFAULT_PITCH,
                       isIcon ? kFamilyIcon : kFamilyUi);
}

// DrawTextW with an explicitly chosen quality: the control column that
// separates "ClearType" from "this rasteriser's outlines".
void DrawGdiQuality(HDC dc, const std::wstring& s, FontRole role, BYTE quality)
{
    HFONT hf = MakeGdiFont(role, quality);
    HGDIOBJ old = hf ? SelectObject(dc, hf) : nullptr;
    SetTextColor(dc, kInk);
    SetBkMode(dc, TRANSPARENT);
    RECT rc{ 2, 0, kW - 2, kH };
    DrawTextW(dc, s.c_str(), -1, &rc, dt::SingleLine);
    if (hf) { SelectObject(dc, old); DeleteObject(hf); }
}

void PrintGdiMetrics(HDC dc, FontRole role, const std::wstring& sample)
{
    HFONT hf = MakeGdiFont(role, CLEARTYPE_QUALITY);
    if (!hf) return;
    HGDIOBJ old = SelectObject(dc, hf);

    LOGFONTW req{};
    GetObjectW(hf, sizeof(req), &req);

    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);

    wchar_t face[64] = {};
    GetTextFaceW(dc, 64, face);

    std::printf("  gdi LOGFONT      : %ls height=%d weight=%d quality=%d\n",
                face, req.lfHeight, req.lfWeight, req.lfQuality);
    std::printf("  gdi TEXTMETRIC   : height=%d ascent=%d descent=%d "
                "internalLead=%d ave=%d\n",
                tm.tmHeight, tm.tmAscent, tm.tmDescent, tm.tmInternalLeading,
                tm.tmAveCharWidth);

    std::printf("  gdi advances     :");
    for (wchar_t ch : sample) {
        int w = 0;
        if (!GetCharWidth32W(dc, ch, ch, &w)) w = -1;
        std::printf(" %04X:%d", static_cast<unsigned>(ch), w);
    }
    std::printf("\n  gdi abc (a,b,c)  :");
    for (wchar_t ch : sample) {
        ABC abc{};
        if (GetCharABCWidthsW(dc, ch, ch, &abc))
            std::printf(" %04X:%d/%u/%d", static_cast<unsigned>(ch),
                        abc.abcA, abc.abcB, abc.abcC);
        else
            std::printf(" %04X:-", static_cast<unsigned>(ch));
    }
    SIZE sz{};
    if (GetTextExtentPoint32W(dc, sample.c_str(), static_cast<int>(sample.size()), &sz))
        std::printf("   extent=%d\n", sz.cx);
    else
        std::printf("\n");

    SelectObject(dc, old);
    DeleteObject(hf);
}

struct Sample
{
    FontRole       role;
    const char*    name;
    const wchar_t* text;
};

// Small, deliberately chosen strings: a Latin word with a round letter,
// hanzi pairs, one icon glyph. Anything wider than ~40 px stops fitting
// a 120-column terminal once three backends sit side by side.
const Sample kSamples[] = {
    { FontRole::Body,    "Body   Aire", L"Aire" },
    { FontRole::Body,    "Body   CJK ", L"\u5DE5\u4F5C" },   // 工作
    { FontRole::Title,   "Title  CJK ", L"\u8BBE\u7F6E" },   // 设置
    { FontRole::Icon,    "Icon   gear", L"\uE713" },
    { FontRole::Display, "Display Ag ", L"Ag" },
};

void PrintInkLine(const char* tag, const Ink& k, const Ink* ref)
{
    std::printf("  %-4s ink        : x=%d..%d y=%d..%d w=%d h=%d n=%d "
                "sum=%.2f peak=%.2f fringe=%.2f",
                tag, k.x0, k.x1, k.y0, k.y1, k.W(), k.H(), k.count,
                k.sum, k.peak, k.fringe);
    if (ref && !ref->Empty())
        std::printf("  ink%+.1f%%", 100.0 * (k.sum - ref->sum) / ref->sum);
    std::printf("\n");
}

} // namespace

int main(int argc, char** argv)
{
    bool wantRaw = false;
    int  onlyRole = -1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--raw") wantRaw = true;
        else if (a.size() == 1 && a[0] >= '0' && a[0] <= '7')
            onlyRole = a[0] - '0';
    }

    Surface surface;
    if (!surface.Init()) {
        std::printf("test_font_lab: FAILED to create a 32bpp DIB\n");
        return 1;
    }

    RegisterPlatformPaintProviders();
    PainterRegistry& reg = PainterRegistry::Instance();
    if (!reg.Available("skia")) RegisterSkiaPluginProvider();
    const bool hasSkia = reg.Available("skia");

    std::printf("test_font_lab: one LOGFONT, three rasterisers (%dx%d BGRA)\n", kW, kH);
    std::printf("  columns        : ct = GDI ClearType (shipping), "
                "aa = GDI grayscale (control), sk = Skia\n");
    std::printf("  airui-skia.dll : %s\n\n",
                hasSkia ? (SkiaPluginLoaded() ? "loaded" : "static provider")
                        : "NOT FOUND (gdi columns only)");

    auto renderGdi = [&](const Sample& s, BYTE quality, bool viaPainter) -> Image {
        surface.Clear();
        if (viaPainter) {
            PaintTarget target{ surface.dc, Rect{ 0, 0, kW, kH } };
            std::unique_ptr<IPainter> p = reg.Begin("gdi", target);
            if (!p) return {};
            p->Text(Rect{ 2, 0, kW - 2, kH }, s.text, s.role, kInk, dt::SingleLine);
            p.reset();
        } else {
            DrawGdiQuality(surface.dc, s.text, s.role, quality);
        }
        return surface.Capture();
    };

    auto renderSkia = [&](const Sample& s) -> Image {
        surface.Clear();
        PaintTarget target{ surface.dc, Rect{ 0, 0, kW, kH } };
        std::unique_ptr<IPainter> p = reg.Begin("skia", target);
        if (!p) return {};
        p->Text(Rect{ 2, 0, kW - 2, kH }, s.text, s.role, kInk, dt::SingleLine);
        p.reset();
        return surface.Capture();
    };

    for (const Sample& s : kSamples) {
        if (onlyRole >= 0 && static_cast<int>(s.role) != onlyRole) continue;

        const Image ct = renderGdi(s, CLEARTYPE_QUALITY, true);
        const Image aa = renderGdi(s, ANTIALIASED_QUALITY, false);
        const Image sk = hasSkia ? renderSkia(s) : Image();
        if (ct.empty() || aa.empty()) { std::printf("gdi produced no raster\n"); continue; }

        const Ink kct = Measure(ct);
        const Ink kaa = Measure(aa);
        const Ink ksk = sk.empty() ? Ink() : Measure(sk);

        std::printf("=========================================================\n");
        std::printf("%s  role=%d cell=%d  text=\"%ls\"\n", s.name,
                    static_cast<int>(s.role), RoleFontSpec(s.role).cell, s.text);
        PrintInkLine("ct", kct, nullptr);
        PrintInkLine("aa", kaa, &kct);
        if (!sk.empty()) PrintInkLine("sk", ksk, &kct);
        if (!sk.empty()) {
            std::printf("  sk vs ct       : dLeft=%+d dTop=%+d dW=%+d dH=%+d\n",
                        ksk.x0 - kct.x0, ksk.y0 - kct.y0,
                        ksk.W() - kct.W(), ksk.H() - kct.H());
        }

        std::printf("  coverage hist  : %% of ink pixels in each tenth of coverage\n");
        auto hist = [](const char* tag, const Ink& k) {
            std::printf("    %-4s ", tag);
            for (int i = 0; i < 10; ++i) std::printf("%5.1f", k.hist[i] * 100.0);
            std::printf("\n");
        };
        hist("ct", kct);
        hist("aa", kaa);
        if (!sk.empty()) hist("sk", ksk);

        // Union bbox (plus one pixel of margin) so all columns share a
        // coordinate system and a shift is visible as a shift.
        int lo = ct.empty() ? 0 : kct.x0;
        int hi = ct.empty() ? 0 : kct.x1;
        int top = ct.empty() ? 0 : kct.y0;
        int bot = ct.empty() ? 0 : kct.y1;
        if (!kaa.Empty()) {
            lo = (std::min)(lo, kaa.x0); hi = (std::max)(hi, kaa.x1);
            top = (std::min)(top, kaa.y0); bot = (std::max)(bot, kaa.y1);
        }
        if (!sk.empty() && !ksk.Empty()) {
            lo = (std::min)(lo, ksk.x0); hi = (std::max)(hi, ksk.x1);
            top = (std::min)(top, ksk.y0); bot = (std::max)(bot, ksk.y1);
        }
        lo = (std::max)(0, lo - 1); hi = (std::min)(kW - 1, hi + 1);
        top = (std::max)(0, top - 1); bot = (std::min)(kH - 1, bot + 1);
        if (hi < lo) hi = lo;
        if (bot < top) bot = top;

        std::printf("  ink map  x=%d..%d  |ct|aa|sk|\n", lo, hi);
        for (int y = top; y <= bot; ++y) {
            std::printf("   %02d |%s|%s|", y,
                        ShadeRow(ct, y, lo, hi).c_str(),
                        ShadeRow(aa, y, lo, hi).c_str());
            if (!sk.empty()) std::printf("%s|", ShadeRow(sk, y, lo, hi).c_str());
            std::printf("\n");
        }

        // Subpixel split on the densest row of each engine.
        const int yct = DensestRow(ct, kct);
        std::printf("  subpixel row ct y=%d (B/G/R tenths; identical rows = grayscale)\n", yct);
        for (int ch = 0; ch < 3; ++ch)
            std::printf("    %s |%s|\n", ch == 0 ? "B" : ch == 1 ? "G" : "R",
                        ChanRow(ct, yct, lo, hi, ch).c_str());
        if (!sk.empty() && !ksk.Empty()) {
            const int ysk = DensestRow(sk, ksk);
            std::printf("  subpixel row sk y=%d\n", ysk);
            for (int ch = 0; ch < 3; ++ch)
                std::printf("    %s |%s|\n", ch == 0 ? "B" : ch == 1 ? "G" : "R",
                            ChanRow(sk, ysk, lo, hi, ch).c_str());
        }
        std::printf("  stems >50%%     : ct [%s] aa [%s]",
                    StemProfile(ct, yct, kct).c_str(),
                    StemProfile(aa, DensestRow(aa, kaa), kaa).c_str());
        if (!sk.empty() && !ksk.Empty())
            std::printf(" sk [%s]", StemProfile(sk, DensestRow(sk, ksk), ksk).c_str());
        std::printf("\n");

        if (!sk.empty()) {
            const L1 ctSk = RasterDiff(ct, sk, lo, top, hi, bot);
            const L1 aaSk = RasterDiff(aa, sk, lo, top, hi, bot);
            std::printf("  raster diff    : ct-vs-sk |d|=%5.2f (%4.1f%% hot)   "
                        "aa-vs-sk |d|=%5.2f (%4.1f%% hot)\n",
                        ctSk.mean, ctSk.total ? 100.0 * ctSk.hot / ctSk.total : 0.0,
                        aaSk.mean, aaSk.total ? 100.0 * aaSk.hot / aaSk.total : 0.0);
        }

        if (wantRaw && !sk.empty()) {
            const int ysk = DensestRow(sk, ksk);
            std::printf("  raw ct   y=%d :", yct);
            for (int x = lo; x <= hi; ++x) {
                const uint8_t* p = &ct[(static_cast<size_t>(yct) * kW + x) * 4u];
                std::printf(" %d,%d,%d", p[2], p[1], p[0]);
            }
            std::printf("\n  raw sk   y=%d :", ysk);
            for (int x = lo; x <= hi; ++x) {
                const uint8_t* p = &sk[(static_cast<size_t>(ysk) * kW + x) * 4u];
                std::printf(" %d,%d,%d", p[2], p[1], p[0]);
            }
            std::printf("\n");
        }

        PrintGdiMetrics(surface.dc, s.role, s.text);
        std::printf("\n");
    }

    return 0;
}
