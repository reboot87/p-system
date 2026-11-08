// ============================================================
// AirUI - SkiaSurface.cpp
// Raster Skia frame + optional Win32 HDC presentation.
// ============================================================
#include "airui/SkiaSurface.h"
#include "airui/SkiaApi.h"
#include "airui/SkiaPainter.h"

#include <cstring>
#include <string>

namespace air {
namespace ui {

namespace {

#if defined(_WIN32) && !defined(AIRUI_SKIA_MOCK)
// One desktop setting, read once: these change only when the user edits
// the ClearType wizard, so re-reading them per frame would be waste.
std::wstring DesktopString(const wchar_t* name)
{
    wchar_t buf[32] = {};
    DWORD   bytes   = sizeof(buf) - sizeof(wchar_t);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", name,
                     RRF_RT_REG_SZ, nullptr, buf, &bytes) != ERROR_SUCCESS)
        return std::wstring();
    return std::wstring(buf);
}
#endif

// Subpixel text is only produced when the device reports its pixel
// geometry. With the default (unknown) props Skia silently degrades to
// greyscale antialiasing, which is why Skia text looked softer and
// muddier next to the GDI backend's ClearType. Windows stores the
// monitor's stripe order in the desktop settings; follow it so the
// fringes land on the same subpixels ClearType uses.
//
// If the user has turned ClearType off, GDI draws greyscale text instead,
// so this returns unknown on purpose: matching GDI means matching whatever
// it is currently doing, not always being subpixel.
SkPixelGeometry LcdPixelGeometry()
{
    // Cached: a SkiaFrame is created for every paint, so reading the
    // registry here unconditionally would put three RegGetValue calls on
    // the per-frame path.
    static const SkPixelGeometry cached = []() -> SkPixelGeometry {
#if defined(_WIN32) && !defined(AIRUI_SKIA_MOCK)
        // FontSmoothing: "0" off, "1" standard greyscale, "2" on.
        // FontSmoothingType: 1 = standard, 2 = ClearType.
        const std::wstring smoothing = DesktopString(L"FontSmoothing");
        const std::wstring type      = DesktopString(L"FontSmoothingType");
        if (smoothing.empty() || smoothing[0] != L'2') return kUnknown_SkPixelGeometry;
        if (!type.empty() && type[0] != L'2')          return kUnknown_SkPixelGeometry;

        // FontSmoothingOrientation: "1" = RGB stripe order (the default),
        // "2" = BGR.
        const std::wstring order = DesktopString(L"FontSmoothingOrientation");
        return (!order.empty() && order[0] == L'2') ? kBGR_H_SkPixelGeometry
                                                   : kRGB_H_SkPixelGeometry;
#else
        return kUnknown_SkPixelGeometry;
#endif
    }();
    return cached;
}

// Whether the frame declares itself opaque. Default yes (see Impl);
// AIRUI_SKIA_ALPHA=premul restores the old premultiplied frame so the
// two can be compared on one build (tests/test_font_lab measures the
// colour fringe either way).
bool OpaqueTarget()
{
#if defined(_WIN32) && !defined(AIRUI_SKIA_MOCK)
    static const bool opaque = [] {
        wchar_t v[16] = {};
        if (GetEnvironmentVariableW(L"AIRUI_SKIA_ALPHA", v, 16) == 0) return true;
        return v[0] == L'o' || v[0] == L'O';
    }();
    return opaque;
#else
    return true;
#endif
}
} // namespace

struct SkiaFrame::Impl
{
    int  w = 0;
    int  h = 0;
    SkBitmap bitmap;
    // Owning surface; the canvas below is a borrowed pointer into it.
    // Skia needs a surface (not a bare SkCanvas over pixels) to carry
    // the surface properties that enable LCD text.
    sk_sp<SkSurface> surface;
    SkCanvas*        canvas = nullptr;
    std::unique_ptr<SkiaPainter>   painter;
    bool presented = false;

#ifdef _WIN32
    HDC      dc     = nullptr;
    HDC      mem    = nullptr;
    HBITMAP  dib    = nullptr;
    HGDIOBJ  oldBmp = nullptr;
    void*    bits   = nullptr;
    Rect     bounds{};
#endif
    // Pixels the surface draws into, when it does not own them (the
    // windowed case: the DIB). null for an off-screen frame.
    void* directPixels = nullptr;

    Impl(int width, int height, void* nativeDc = nullptr) : w(width), h(height)
    {
        if (w <= 0 || h <= 0) return;
        // Opaque on purpose. Skia's raster device only considers LCD
        // (subpixel) text for an OPAQUE target -- a premultiplied frame
        // silently drops back to greyscale AA for every draw, however
        // the font is configured. Measured (tests/test_font_lab): with
        // kPremul the text raster had exactly zero colour fringe while
        // GDI's ClearType text had 60-107 per pixel, i.e. Skia was not
        // doing subpixel at all. The shell fills the whole client area
        // before drawing (Shell::Paint -> gfx::Fill), so declaring the
        // frame opaque states what is already true.
        const SkAlphaType alpha = OpaqueTarget() ? kOpaque_SkAlphaType
                                                 : kPremul_SkAlphaType;
        const SkImageInfo info = SkImageInfo::Make(
            w, h, kBGRA_8888_SkColorType, alpha);

#ifdef _WIN32
        // Create the target DIB first, so the surface can draw straight
        // into the pixels that will be handed to the window. The old
        // order allocated a Skia-owned bitmap, drew into it, then copied
        // the whole frame into the DIB on present -- 8 MB of memcpy per
        // frame at 1920x1040, measured at ~1.5 ms.
        if (nativeDc) PrepareDib(static_cast<HDC>(nativeDc));
#endif

        void*  pixels   = directPixels;
        size_t rowBytes = directPixels ? static_cast<size_t>(w) * 4u : 0u;
        if (!pixels) {
            // Off-screen frame, or the DIB could not be created.
            // allocPixels returns void in current Skia; a null pixel
            // buffer is the failure signal (the old bool-returning
            // overload is gone).
            bitmap.allocPixels(info);
            pixels   = bitmap.getPixels();
            rowBytes = bitmap.rowBytes();
            if (!pixels) return;
        }

        const SkSurfaceProps props(0, LcdPixelGeometry());
        // Skia m130 moved the raster surface factories out of SkSurface
        // into the SkSurfaces namespace; WrapPixels draws straight into
        // the buffer we hand it, so there is no extra copy on present.
        surface = SkSurfaces::WrapPixels(info, pixels, rowBytes, &props);
        if (!surface) return;
        canvas = surface->getCanvas();
        if (!canvas) return;
        // An opaque frame cannot hold transparent pixels: 0x00000000
        // would be premultiplied black. Start from opaque black so an
        // unpainted gap composites as background rather than as
        // "leave the target untouched".
        canvas->clear(OpaqueTarget() ? SK_ColorBLACK : SK_ColorTRANSPARENT);
        // The target HDC (when there is one) is handed to the painter so
        // text can be calibrated against GDI's own metrics once. An
        // off-screen frame falls back to the screen DC.
        painter.reset(new SkiaPainter(canvas, nativeDc));
    }

#ifdef _WIN32
    // Memory DC + top-down 32bpp DIB the frame composites onto. Called
    // from the constructor, before the surface is created, so the surface
    // can wrap these pixels instead of its own allocation.
    bool PrepareDib(HDC target)
    {
        if (!target || w <= 0 || h <= 0) return false;

        BITMAPINFO bmi{};
        BITMAPINFOHEADER& bh = bmi.bmiHeader;
        bh.biSize        = sizeof(BITMAPINFOHEADER);
        bh.biWidth       = w;
        bh.biHeight      = -h;             // negative = top-down, like Skia
        bh.biPlanes      = 1;
        bh.biBitCount    = 32;
        bh.biCompression = BI_RGB;

        mem = CreateCompatibleDC(target);
        dib = CreateDIBSection(target, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!mem || !dib || !bits) {
            // Leave the members clean so Present falls back to copying
            // the Skia-owned bitmap instead of dereferencing a null DIB.
            if (dib) { DeleteObject(dib); dib = nullptr; }
            if (mem) { DeleteDC(mem);       mem = nullptr; }
            bits = nullptr;
            return false;
        }
        oldBmp = SelectObject(mem, dib);
        directPixels = bits;
        return true;
    }

    bool Attach(HDC target, const Rect& area)
    {
        dc = target;
        bounds = area;
        return dc != nullptr && canvas != nullptr;
    }

    ~Impl()
    {
        if (mem && oldBmp) SelectObject(mem, oldBmp);
        if (dib) DeleteObject(dib);
        if (mem) DeleteDC(mem);
    }
#else
    ~Impl() = default;
#endif

    void Present()
    {
        if (presented || !canvas || !painter) return;
#ifdef _WIN32
        if (!dc || !bits || !mem) { presented = true; return; }

        if (!directPixels) {
            // The surface owns its pixels (DIB creation failed): copy the
            // frame into the DIB before handing it to the window.
            const unsigned char* src =
                static_cast<const unsigned char*>(bitmap.getPixels());
            if (!src) { presented = true; return; }
            const size_t srcStride = bitmap.rowBytes();
            const size_t dstStride = static_cast<size_t>(w) * 4u;
            unsigned char* dst = static_cast<unsigned char*>(bits);
            for (int y = 0; y < h; ++y)
                std::memcpy(dst + static_cast<size_t>(y) * dstStride,
                            src + static_cast<size_t>(y) * srcStride,
                            dstStride);
        }

        // The frame is opaque, so every source alpha is 255 and blending
        // cannot change a pixel: AlphaBlend with AC_SRC_ALPHA would still
        // run a per-pixel blend over the whole 8 MB surface. A plain
        // BitBlt is the same result for a straight copy. The premultiplied
        // comparison build (AIRUI_SKIA_ALPHA=premul) keeps blending.
        if (OpaqueTarget()) {
            BitBlt(dc, bounds.left, bounds.top, w, h, mem, 0, 0, SRCCOPY);
        } else {
            BLENDFUNCTION bf{};
            bf.BlendOp             = AC_SRC_OVER;
            bf.BlendFlags          = 0;
            bf.SourceConstantAlpha = 255;
            bf.AlphaFormat         = AC_SRC_ALPHA;   // Skia is premultiplied
            AlphaBlend(dc, bounds.left, bounds.top, w, h,
                       mem, 0, 0, w, h, bf);
        }
#endif
        presented = true;
    }
};

// ------------------------------------------------------------
// Construction
// ------------------------------------------------------------
#ifdef _WIN32
SkiaFrame::SkiaFrame(HDC dc, const Rect& bounds)
    : m(new Impl(bounds.Width(), bounds.Height(), static_cast<void*>(dc)))
{
    if (m->canvas) m->Attach(dc, bounds);
}
#endif

SkiaFrame::SkiaFrame(int width, int height)
    : m(new Impl(width, height)) {}

SkiaFrame::~SkiaFrame()
{
    Present();   // composites to the HDC, or is a no-op off-screen
}

// ------------------------------------------------------------
// Frame state
// ------------------------------------------------------------
bool      SkiaFrame::Valid() const    { return m && m->painter != nullptr; }
void      SkiaFrame::Present()        { if (m) m->Present(); }
SkCanvas* SkiaFrame::Canvas() const   { return m ? m->canvas : nullptr; }
const void* SkiaFrame::Pixels() const
{
    if (!m) return nullptr;
    return m->directPixels ? m->directPixels : m->bitmap.getPixels();
}
size_t    SkiaFrame::RowBytes() const
{
    if (!m) return 0u;
    return m->directPixels ? static_cast<size_t>(m->w) * 4u
                           : m->bitmap.rowBytes();
}
int       SkiaFrame::Width()  const   { return m ? m->w : 0; }
int       SkiaFrame::Height() const   { return m ? m->h : 0; }

// ------------------------------------------------------------
// IPainter delegation
// ------------------------------------------------------------
#define AIRUI_SKIA_FORWARD(call) do { if (m && m->painter) m->painter->call; } while (0)

void SkiaFrame::FillRect(const Rect& r, Color c)
{ AIRUI_SKIA_FORWARD(FillRect(r, c)); }

void SkiaFrame::FillRectAlpha(const Rect& r, Color c, uint8_t alpha)
{ AIRUI_SKIA_FORWARD(FillRectAlpha(r, c, alpha)); }

void SkiaFrame::FillRound(const Rect& r, int radius, Color c)
{ AIRUI_SKIA_FORWARD(FillRound(r, radius, c)); }

void SkiaFrame::StrokeRect(const Rect& r, Color c, int width)
{ AIRUI_SKIA_FORWARD(StrokeRect(r, c, width)); }

void SkiaFrame::StrokeRound(const Rect& r, int radius, Color c, int width)
{ AIRUI_SKIA_FORWARD(StrokeRound(r, radius, c, width)); }

void SkiaFrame::StrokeDashed(const Rect& r, Color c, int width, int dash)
{ AIRUI_SKIA_FORWARD(StrokeDashed(r, c, width, dash)); }

void SkiaFrame::Text(const Rect& r, const std::wstring& s,
                     FontRole role, Color c, TextFormat fmt)
{ AIRUI_SKIA_FORWARD(Text(r, s, role, c, fmt)); }

void SkiaFrame::TextBlock(const Rect& r, const std::wstring& s,
                          FontRole role, Color c, int maxLines)
{ AIRUI_SKIA_FORWARD(TextBlock(r, s, role, c, maxLines)); }

int SkiaFrame::MeasureText(const std::wstring& s, FontRole role)
{
    return (m && m->painter) ? m->painter->MeasureText(s, role) : 0;
}

void SkiaFrame::PushClip(const Rect& r)   { AIRUI_SKIA_FORWARD(PushClip(r)); }
void SkiaFrame::PopClip()                 { AIRUI_SKIA_FORWARD(PopClip()); }
void SkiaFrame::Translate(Point offset)   { AIRUI_SKIA_FORWARD(Translate(offset)); }

#undef AIRUI_SKIA_FORWARD

// ------------------------------------------------------------
// Backend factory
// ------------------------------------------------------------
#ifdef _WIN32
std::unique_ptr<IPainter> CreateSkiaBackend(HDC dc, const Rect& bounds)
{
    return std::unique_ptr<IPainter>(new SkiaFrame(dc, bounds));
}
#else
std::unique_ptr<IPainter> CreateSkiaBackend(int width, int height)
{
    return std::unique_ptr<IPainter>(new SkiaFrame(width, height));
}
#endif

} // namespace ui
} // namespace air
