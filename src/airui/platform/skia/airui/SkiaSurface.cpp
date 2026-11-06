// ============================================================
// AirUI - SkiaSurface.cpp
// Raster Skia frame + optional Win32 HDC presentation.
// ============================================================
#include "airui/SkiaSurface.h"
#include "airui/SkiaApi.h"
#include "airui/SkiaPainter.h"

#include <cstring>

namespace air {
namespace ui {

struct SkiaFrame::Impl
{
    int  w = 0;
    int  h = 0;
    SkBitmap bitmap;
    std::unique_ptr<SkCanvas>      canvas;
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

    Impl(int width, int height) : w(width), h(height)
    {
        if (w <= 0 || h <= 0) return;
        const SkImageInfo info = SkImageInfo::Make(
            w, h, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        if (!bitmap.allocPixels(info)) return;
        canvas.reset(new SkCanvas(bitmap));
        canvas->clear(SK_ColorTRANSPARENT);
        painter.reset(new SkiaPainter(canvas.get()));
    }

#ifdef _WIN32
    bool Attach(HDC target, const Rect& area)
    {
        dc = target;
        bounds = area;
        if (!dc || !canvas) return false;

        BITMAPINFO bmi{};
        BITMAPINFOHEADER& bh = bmi.bmiHeader;
        bh.biSize        = sizeof(BITMAPINFOHEADER);
        bh.biWidth       = w;
        bh.biHeight      = -h;             // negative = top-down, like Skia
        bh.biPlanes      = 1;
        bh.biBitCount    = 32;
        bh.biCompression = BI_RGB;

        mem = CreateCompatibleDC(dc);
        dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!mem || !dib || !bits) return false;
        oldBmp = SelectObject(mem, dib);
        return true;
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
        if (!dc || !bits) { presented = true; return; }

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

        BLENDFUNCTION bf{};
        bf.BlendOp             = AC_SRC_OVER;
        bf.BlendFlags          = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat         = AC_SRC_ALPHA;   // Skia is premultiplied
        AlphaBlend(dc, bounds.left, bounds.top, w, h,
                   mem, 0, 0, w, h, bf);
#endif
        presented = true;
    }
};

// ------------------------------------------------------------
// Construction
// ------------------------------------------------------------
#ifdef _WIN32
SkiaFrame::SkiaFrame(HDC dc, const Rect& bounds)
    : m(new Impl(bounds.Width(), bounds.Height()))
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
SkCanvas* SkiaFrame::Canvas() const   { return m ? m->canvas.get() : nullptr; }
const void* SkiaFrame::Pixels() const { return m ? m->bitmap.getPixels() : nullptr; }
size_t    SkiaFrame::RowBytes() const { return m ? m->bitmap.rowBytes() : 0u; }
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
