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
