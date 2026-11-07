#pragma once
// ============================================================
// AirUI - CanvasPainter.h (Win32 backend)
//
// IPainter implementation backed by Canvas/GDI. This is the glue
// that lets platform-free controls in airui/ControlsCore.h make
// their drawing calls on Windows.
//
//   CanvasPainter painter(canvas);
//   PaintButton(painter, ToRect(rc), text, style, st, Pal(), Mx());
// ============================================================
#include "airui/Canvas.h"
#include "airui/PaintFrame.h"
#include "airui/hal.h"

#include <vector>

namespace air {
namespace ui {

// Color/Rect are layout-identical to the Win32 value types, so
// conversion is a plain field copy.
inline Rect ToRect(const RECT& r)
{
    return Rect(r.left, r.top, r.right, r.bottom);
}

inline RECT ToRect(const Rect& r)
{
    RECT rc = { r.left, r.top, r.right, r.bottom };
    return rc;
}

class CanvasPainter : public IPainter
{
public:
    explicit CanvasPainter(Canvas& canvas) : m_canvas(canvas) {}

    // ---- shapes ----
    void FillRect(const Rect& r, Color c) override
    {
        m_canvas.Fill(rect(r), c);
    }

    // Modal scrims need real blending: an opaque fill would erase the
    // page behind the dialog. AlphaBlend from a 1x1 32bpp source is
    // the cheapest GDI path that works on every modern Windows.
    void FillRectAlpha(const Rect& r, Color c, uint8_t alpha) override
    {
        // Shapes drawn straight onto the DC would be overwritten by the
        // backend's end-of-frame composite, so route alpha scrims too.
        if (PaintFrame* f = CurrentPaintFrame()) {
            f->FillRectAlpha(r, c, alpha); return;
        }
        if (alpha == 0) return;
        if (alpha == 255) { FillRect(r, c); return; }
        HDC dc = m_canvas.Native();
        if (!dc) { FillRect(r, c); return; }
        const RECT rc = rect(r);
        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) return;

        HDC mem = CreateCompatibleDC(dc);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = 1;
        bmi.bmiHeader.biHeight = 1;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bmp && bits) {
            // 32bpp DIB is BGRA in memory; AC_SRC_ALPHA needs
            // premultiplied colour channels.
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
        m_canvas.FillRound(rect(r), c, radius);
    }

    void StrokeRect(const Rect& r, Color c, int width) override
    {
        m_canvas.Stroke(rect(r), c, 0, width);
    }

    void StrokeRound(const Rect& r, int radius, Color c, int width) override
    {
        m_canvas.Stroke(rect(r), c, radius, width);
    }

    void StrokeDashed(const Rect& r, Color c, int width, int dash) override
    {
        (void)width;
        (void)dash;
        m_canvas.StrokeDashed(rect(r), c, 0);
    }

    // ---- text ----
    void Text(const Rect& r, const std::wstring& s,
              FontRole role, Color c, TextFormat fmt) override
    {
        m_canvas.Text(rect(r), s, role, c, fmt);
    }

    void TextBlock(const Rect& r, const std::wstring& s,
                   FontRole role, Color c, int maxLines) override
    {
        (void)maxLines;   // Canvas::TextBlock already wraps + ellipsises
        m_canvas.TextBlock(rect(r), s, role, c);
    }

    int MeasureText(const std::wstring& s, FontRole role) override
    {
        return m_canvas.MeasureWidth(s, role);
    }

    // ---- clipping ----
    void PushClip(const Rect& r) override
    {
        if (PaintFrame* f = CurrentPaintFrame()) {
            f->PushClip(ToRect(rect(r))); return;
        }
        HDC dc = m_canvas.Native();
        if (!dc) {
            m_clipStack.push_back(0);
            return;
        }
        m_clipStack.push_back(SaveDC(dc));
        RECT rc = rect(r);
        IntersectClipRect(dc, rc.left, rc.top, rc.right, rc.bottom);
    }

    void PopClip() override
    {
        if (PaintFrame* f = CurrentPaintFrame()) { f->PopClip(); return; }
        if (m_clipStack.empty()) return;
        const int saved = m_clipStack.back();
        m_clipStack.pop_back();

        HDC dc = m_canvas.Native();
        if (dc && saved) RestoreDC(dc, saved);
    }

    // ---- transform ----
    void Translate(Point offset) override
    {
        m_offset.x += offset.x;
        m_offset.y += offset.y;
    }

private:
    RECT rect(const Rect& r) const
    {
        RECT rc = { r.left + m_offset.x, r.top + m_offset.y,
                    r.right + m_offset.x, r.bottom + m_offset.y };
        return rc;
    }

    Canvas&          m_canvas;
    Point            m_offset{};
    std::vector<int> m_clipStack;
};

} // namespace ui
} // namespace air
