#pragma once
// ============================================================
// AirUI - PaintFrame.h (Win32 backend)
//
// One frame's drawing router. When a frame is active, EVERY drawing
// primitive in the application -- chrome, pages, core widgets and
// Canvas helpers -- is routed through this object, because they all
// bottom out in gfx::* (Gfx.cpp) or CanvasPainter (IPainter).
//
//   gfx::* / CanvasPainter  ->  PaintFrame  ->  IPainter (GDI/Skia)
//
// This is what makes the renderer switch cover the WHOLE window
// instead of just a separate "core shell" chrome path. A frame is
// opened and closed by Shell::Paint(); when no frame is active the
// original GDI code path runs untouched (pixel-for-pixel no-regression).
//
// Text policy is decided at runtime from the backend's real capability:
//   * capable backend  -> Text() goes to the backend (pure Skia text)
//   * legacy plugin    -> Text() is recorded and rasterised by the host
//                         AFTER the backend frame was composited, so
//                         CJK / MDL2 glyphs stay readable.
// ============================================================
#include <windows.h>

#include "airui/hal.h"

#include <string>
#include <vector>

namespace air {
namespace ui {

class Fonts;

class PaintFrame : public IPainter
{
public:
    static PaintFrame& Instance();

    bool Active() const { return m_active; }

    // backend : the real painter for this frame (owned by the caller;
    //           its destruction flushes the frame).
    // dc      : the frame's target device context (double-buffer DC).
    // fonts   : supplies HFONTs for host-side text rasterisation.
    // backendOwnsText : true when the backend can rasterise the app's
    //           wide text (CJK + icon glyphs); detected at runtime.
    void Begin(IPainter* backend, HDC dc, const Fonts* fonts,
               bool backendOwnsText);

    // Deactivate the router. Recorded text ops survive until ReplayText().
    void End();

    // MUST be called after the backend painter was destroyed (its
    // destructor composites the Skia raster onto the DC). Drawing text
    // before that would be overwritten by the composite.
    void ReplayText();

    void Clear();

    // ---- gfx::* shims: take native HFONT/COLORREF/RECT/UINT ----
    void GfxFill(const RECT& rc, COLORREF color);
    void GfxFillRound(const RECT& rc, COLORREF color, int radius);
    void GfxStroke(const RECT& rc, COLORREF color, int radius, int width);
    void GfxStrokeDashed(const RECT& rc, COLORREF color, int radius);
    void GfxHLine(int x1, int x2, int y, COLORREF color);
    void GfxHLineDashed(int x1, int x2, int y, COLORREF color);
    void GfxVLine(int x, int y1, int y2, COLORREF color);
    void GfxDot(int cx, int cy, int diameter, COLORREF color);
    void GfxText(HFONT font, COLORREF color, const RECT& rc,
                 const wchar_t* s, UINT format);

    // ---- IPainter (used by CanvasPainter / ShellCore) ----
    void FillRect(const Rect& r, Color c) override;
    void FillRectAlpha(const Rect& r, Color c, uint8_t alpha) override;
    void FillRound(const Rect& r, int radius, Color c) override;
    void StrokeRect(const Rect& r, Color c, int width) override;
    void StrokeRound(const Rect& r, int radius, Color c, int width) override;
    void StrokeDashed(const Rect& r, Color c, int width, int dash) override;
    void Text(const Rect& r, const std::wstring& s,
              FontRole role, Color c, TextFormat fmt) override;
    void TextBlock(const Rect& r, const std::wstring& s,
                   FontRole role, Color c, int maxLines) override;
    int  MeasureText(const std::wstring& s, FontRole role) override;
    void PushClip(const Rect& r) override;
    void PopClip() override;
    void Translate(Point offset) override;

    PaintFrame(const PaintFrame&) = delete;
    PaintFrame& operator=(const PaintFrame&) = delete;

private:
    PaintFrame() = default;

    struct TextOp
    {
        RECT        rc{};
        HFONT       font   = nullptr;
        COLORREF    color  = 0;
        UINT        format = 0;
        std::wstring text;
        bool        clipped = false;
        Rect        clip{};
    };

    Rect  Shift(const Rect& r) const;
    Rect  CurrentClip() const;
    HFONT RoleFont(FontRole role) const;
    FontRole FontRoleOf(HFONT font) const;
    void  RecordText(HFONT font, COLORREF color, const RECT& rc,
                     const std::wstring& s, UINT format);

    IPainter*          m_backend = nullptr;
    HDC                m_dc      = nullptr;
    const Fonts*       m_fonts   = nullptr;
    bool               m_backendOwnsText = false;
    bool               m_active  = false;
    Point              m_offset{};
    std::vector<Rect>  m_clips;
    std::vector<TextOp> m_textOps;
};

// nullptr when the current frame is a plain GDI frame.
PaintFrame* CurrentPaintFrame();

} // namespace ui
} // namespace air
