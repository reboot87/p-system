#pragma once
// ============================================================
// AirUI - GfxPainter.h (Win32 backend)
//
// IPainter implementation backed by the legacy gfx::* GDI helpers.
//
// Why this exists: Canvas used to call gfx::* directly, so the public
// drawing surface was not an IPainter. Canvas now talks only to
// IPainter; when no PaintFrame is active, this adapter is what it talks
// to. It is a thin pass-through on purpose -- every pixel keeps coming
// from the same gfx::* code (PS_DASH pen, Ellipse dot, RoundRect fill),
// so moving Canvas onto the HAL changes nothing on screen.
//
//   Canvas::Painter() -> PaintFrame (backend active) | GfxPainter (GDI)
//   GfxPainter        -> gfx::* -> GDI
//
// The newer GdiPainter is a separate, independent GDI implementation
// used by the platform-free shell core; do not merge the two without a
// pixel comparison, their rounded-rect / dash conventions differ.
// ============================================================
#include <windows.h>

#include <string>
#include <vector>

#include "Fonts.h"
#include "airui/hal.h"
#include "internal/Gfx.h"

namespace air {
namespace ui {

class GfxPainter : public IPainter
{
public:
    GfxPainter(DeviceContextHandle dc, const Fonts& fonts)
        : m_dc(dc), m_fonts(fonts) {}

    // ---- shapes ----
    void FillRect(const Rect& r, Color c) override
    {
        gfx::Fill(m_dc, Win(r), c);
    }

    // Same 1x1 premultiplied-DIB blend GdiPainter uses; kept local so
    // this adapter still works if the two are ever split apart.
    void FillRectAlpha(const Rect& r, Color c, uint8_t alpha) override
    {
        if (alpha == 0) return;
        if (alpha == 255) { FillRect(r, c); return; }
        HDC dc = reinterpret_cast<HDC>(m_dc);
        const RECT rc = Win(r);
        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        if (!dc || w <= 0 || h <= 0) { FillRect(r, c); return; }

        HDC mem = CreateCompatibleDC(dc);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = 1;
        bmi.bmiHeader.biHeight = 1;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits,
                                       nullptr, 0);
        if (bmp && bits) {
            const uint32_t a = alpha;
            const uint32_t b = ColorB(c) * a / 255u;
            const uint32_t g = ColorG(c) * a / 255u;
            const uint32_t rr = ColorR(c) * a / 255u;
            *static_cast<uint32_t*>(bits) = (a << 24) | (rr << 16) | (g << 8) | b;
            HGDIOBJ old = SelectObject(mem, bmp);
            BLENDFUNCTION bf{};
            bf.BlendOp = AC_SRC_OVER;
            bf.BlendFlags = 0;
            bf.SourceConstantAlpha = 255;
            bf.AlphaFormat = AC_SRC_ALPHA;
            AlphaBlend(dc, rc.left, rc.top, w, h, mem, 0, 0, 1, 1, bf);
            SelectObject(mem, old);
        } else {
            FillRect(r, c);
        }
        if (bmp) DeleteObject(bmp);
        DeleteDC(mem);
    }

    void FillRound(const Rect& r, int radius, Color c) override
    {
        gfx::FillRound(m_dc, Win(r), c, radius);
    }

    void StrokeRect(const Rect& r, Color c, int width) override
    {
        gfx::Stroke(m_dc, Win(r), c, 0, width);
    }

    void StrokeRound(const Rect& r, int radius, Color c, int width) override
    {
        gfx::Stroke(m_dc, Win(r), c, radius, width);
    }

    void StrokeDashed(const Rect& r, Color c, int width, int dash) override
    {
        (void)width;
        (void)dash;
        gfx::StrokeDashed(m_dc, Win(r), c, 0);
    }

    // The legacy border was a dashed RoundRect, so keep the radius here:
    // this is the exact call Canvas::StrokeDashed made before the HAL had
    // a rounded variant, which is what keeps native pixels unchanged.
    void StrokeDashedRound(const Rect& r, int radius, Color c, int width,
                           int dash) override
    {
        (void)width;
        (void)dash;
        gfx::StrokeDashed(m_dc, Win(r), c, radius);
    }

    // ---- lines / dot / track ----
    void HLine(int x1, int x2, int y, Color c) override
    {
        const Point o = m_offset;
        gfx::HLine(m_dc, x1 + o.x, x2 + o.x, y + o.y, c);
    }

    void HLineDashed(int x1, int x2, int y, Color c) override
    {
        const Point o = m_offset;
        gfx::HLineDashed(m_dc, x1 + o.x, x2 + o.x, y + o.y, c);
    }

    void VLine(int x, int y1, int y2, Color c) override
    {
        const Point o = m_offset;
        gfx::VLine(m_dc, x + o.x, y1 + o.y, y2 + o.y, c);
    }

    void Dot(int cx, int cy, int diameter, Color c) override
    {
        const Point o = m_offset;
        gfx::Dot(m_dc, cx + o.x, cy + o.y, diameter, c);
    }

    void ProgressBar(const Rect& r, int pct, Color track, Color normal,
                     Color warn, Color hot) override
    {
        gfx::ProgressBar(m_dc, Win(r), pct, track, normal, warn, hot);
    }

    // ---- text ----
    void Text(const Rect& r, const wchar_t* s, size_t len, FontRole role,
              Color c, TextFormat fmt) override
    {
        gfx::Text(m_dc, m_fonts.Get(role), c, Win(r), std::wstring(s, len),
                  static_cast<UINT>(fmt));
    }

    void TextBlock(const Rect& r, const wchar_t* s, size_t len, FontRole role,
                   Color c, int maxLines) override
    {
        (void)maxLines;   // gfx::TextBlock wraps and ellipsises by itself
        gfx::TextBlock(m_dc, m_fonts.Get(role), c, Win(r),
                       std::wstring(s, len));
    }

    int MeasureText(const wchar_t* s, size_t len, FontRole role) override
    {
        return gfx::MeasureWidth(m_dc, m_fonts.Get(role), s,
                                 static_cast<int>(len));
    }

    int LineHeight(FontRole role) override
    {
        return gfx::LineHeight(m_dc, m_fonts.Get(role));
    }

    // ---- clipping ----
    void PushClip(const Rect& r) override
    {
        HDC dc = reinterpret_cast<HDC>(m_dc);
        if (!dc) { m_clipStack.push_back(0); return; }
        const int saved = SaveDC(dc);
        m_clipStack.push_back(saved);
        if (saved) {
            const RECT rc = Win(r);
            IntersectClipRect(dc, rc.left, rc.top, rc.right, rc.bottom);
        }
    }

    void PopClip() override
    {
        if (m_clipStack.empty()) return;
        const int saved = m_clipStack.back();
        m_clipStack.pop_back();
        HDC dc = reinterpret_cast<HDC>(m_dc);
        if (dc && saved) RestoreDC(dc, saved);
    }

    void Translate(Point offset) override
    {
        m_offset.x += offset.x;
        m_offset.y += offset.y;
    }

private:
    RECT Win(const Rect& r) const
    {
        RECT rc = { r.left + m_offset.x, r.top + m_offset.y,
                    r.right + m_offset.x, r.bottom + m_offset.y };
        return rc;
    }

    DeviceContextHandle m_dc;
    const Fonts&        m_fonts;
    Point               m_offset{};
    std::vector<int>    m_clipStack;
};

} // namespace ui
} // namespace air
