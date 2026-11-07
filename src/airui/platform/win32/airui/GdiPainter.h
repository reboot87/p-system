#pragma once
// ============================================================
// AirUI - GdiPainter.h
// Production IPainter implementation backed by a raw GDI HDC.
//
// This is the HDC-level HAL: used when a raw device context (not
// the Canvas wrapper) is the drawing target, e.g. when the Win32
// Shell renders the platform-free ShellCore. Core stays platform
// free; only this file sees GDI.
//
// All coordinates are logical pixels in the painter's space.
// Color is 0x00BBGGRR (Win32 COLORREF layout) so it passes
// straight through to GDI.
// ============================================================
#include "airui/hal.h"
#include "airui/UiText.h"
#include <windows.h>
#include <string>
#include <stack>

namespace air {
namespace ui {

class GdiPainter : public IPainter
{
public:
    explicit GdiPainter(HDC hdc) : m_dc(hdc) {}
    ~GdiPainter() override {
        for (HFONT f : m_fonts) if (f) DeleteObject(f);
    }

    void FillRect(const Rect& r, Color c) override {
        if (!m_dc) return;
        RECT rc = ToRect(r);
        HBRUSH br = CreateSolidBrush(c);
        ::FillRect(m_dc, &rc, br);
        DeleteObject(br);
    }

    // Real alpha blending via a 1x1 32bpp premultiplied DIB. Falls back
    // to opaque fill when no DC or allocation fails.
    void FillRectAlpha(const Rect& r, Color c, uint8_t alpha) override {
        if (alpha == 0) return;
        if (alpha == 255) { FillRect(r, c); return; }
        HDC dc = m_dc;
        if (!dc) { FillRect(r, c); return; }
        RECT rc = ToRect(r);
        const int w = rc.right - rc.left, h = rc.bottom - rc.top;
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
            const uint32_t a = alpha;
            const uint32_t b = ColorB(c) * a / 255u;
            const uint32_t g = ColorG(c) * a / 255u;
            const uint32_t rr = ColorR(c) * a / 255u;
            *static_cast<uint32_t*>(bits) = (a << 24) | (rr << 16) | (g << 8) | b;
            HGDIOBJ old = SelectObject(mem, bmp);
            BLENDFUNCTION bf{};
            bf.BlendOp = AC_SRC_OVER;
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

    // True rounded rect via a region fill.
    void FillRound(const Rect& r, int radius, Color c) override {
        if (!m_dc) return;
        RECT rc = ToRect(r);
        HBRUSH br = CreateSolidBrush(c);
        HRGN rgn = CreateRoundRectRgn(rc.left, rc.top, rc.right, rc.bottom,
                                      radius * 2, radius * 2);
        if (rgn) { FillRgn(m_dc, rgn, br); DeleteObject(rgn); }
        else { ::FillRect(m_dc, &rc, br); }
        DeleteObject(br);
    }

    void StrokeRect(const Rect& r, Color c, int width) override {
        if (!m_dc) return;
        RECT rc = ToRect(r);
        HPEN pen = CreatePen(PS_SOLID, width, c);
        HGDIOBJ oldPen = SelectObject(m_dc, pen);
        HGDIOBJ oldBrush = SelectObject(m_dc, GetStockObject(NULL_BRUSH));
        ::Rectangle(m_dc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(m_dc, oldPen); SelectObject(m_dc, oldBrush);
        DeleteObject(pen);
    }

    // True rounded stroke.
    void StrokeRound(const Rect& r, int radius, Color c, int width) override {
        if (!m_dc) return;
        RECT rc = ToRect(r);
        HPEN pen = CreatePen(PS_SOLID, width, c);
        HGDIOBJ oldPen = SelectObject(m_dc, pen);
        HGDIOBJ oldBrush = SelectObject(m_dc, GetStockObject(NULL_BRUSH));
        if (radius > 0)
            RoundRect(m_dc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
        else
            ::Rectangle(m_dc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(m_dc, oldPen); SelectObject(m_dc, oldBrush);
        DeleteObject(pen);
    }

    // Dashed stroke (custom dash if dash > 0, else a default dash).
    void StrokeDashed(const Rect& r, Color c, int width, int dash) override {
        if (!m_dc) return;
        RECT rc = ToRect(r);
        HPEN pen = nullptr;
        int dp[2] = { dash > 0 ? dash : 6, dash > 0 ? dash : 4 };
        LOGBRUSH lb{}; lb.lbStyle = BS_SOLID; lb.lbColor = c;
        pen = ExtCreatePen(PS_COSMETIC | PS_USERSTYLE, width, &lb, 2, (DWORD*)dp);
        if (!pen) pen = CreatePen(PS_DASH, width, c);
        HGDIOBJ oldPen = SelectObject(m_dc, pen);
        HGDIOBJ oldBrush = SelectObject(m_dc, GetStockObject(NULL_BRUSH));
        ::Rectangle(m_dc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(m_dc, oldPen); SelectObject(m_dc, oldBrush);
        DeleteObject(pen);
    }

    // Font selected per FontRole so text matches the design system.
    void Text(const Rect& r, const std::wstring& s, FontRole role, Color c, TextFormat fmt) override {
        if (!m_dc || s.empty()) return;
        HFONT f = FontFor(role);
        HGDIOBJ old = f ? SelectObject(m_dc, f) : nullptr;
        SetTextColor(m_dc, c);
        SetBkMode(m_dc, TRANSPARENT);
        RECT rc = ToRect(r);
        DrawTextW(m_dc, s.c_str(), -1, &rc, (UINT)fmt);
        if (f) SelectObject(m_dc, old);
    }

    void TextBlock(const Rect& r, const std::wstring& s, FontRole role, Color c, int maxLines) override {
        (void)maxLines;
        if (!m_dc || s.empty()) return;
        HFONT f = FontFor(role);
        HGDIOBJ old = f ? SelectObject(m_dc, f) : nullptr;
        SetTextColor(m_dc, c);
        SetBkMode(m_dc, TRANSPARENT);
        RECT rc = ToRect(r);
        DrawTextW(m_dc, s.c_str(), -1, &rc, DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (f) SelectObject(m_dc, old);
    }

    int MeasureText(const std::wstring& s, FontRole role) override {
        if (!m_dc || s.empty()) return 0;
        HFONT f = FontFor(role);
        HGDIOBJ old = f ? SelectObject(m_dc, f) : nullptr;
        SIZE sz{};
        GetTextExtentPoint32W(m_dc, s.c_str(), (int)s.size(), &sz);
        if (f) SelectObject(m_dc, old);
        return sz.cx;
    }

    // Save/Restore so nested clips compose correctly (unlike a bare
    // SelectClipRgn(nullptr) which would wipe the whole stack).
    void PushClip(const Rect& r) override {
        if (!m_dc) return;
        RECT rc = ToRect(r);
        m_clipStack.push(SaveDC(m_dc));
        IntersectClipRect(m_dc, rc.left, rc.top, rc.right, rc.bottom);
    }

    void PopClip() override {
        if (m_dc && !m_clipStack.empty()) {
            RestoreDC(m_dc, m_clipStack.top());
            m_clipStack.pop();
        }
    }

    void Translate(Point offset) override {
        // Additive: callers stack translations inside PushClip/PopClip,
        // whose SaveDC/RestoreDC already preserves the viewport origin.
        if (!m_dc) return;
        POINT org{};
        GetViewportOrgEx(m_dc, &org);
        SetViewportOrgEx(m_dc, org.x + offset.x, org.y + offset.y, nullptr);
    }

private:
    static RECT ToRect(const Rect& r) {
        return { r.left, r.top, r.right, r.bottom };
    }

    HFONT FontFor(FontRole role) {
        int i = (int)role;
        if (i < 0 || i >= (int)FontRole::Count) i = 0;
        if (!m_fonts[i]) {
            // Size and family come from UiText.h so the Skia backend
            // renders the exact same box (it converts `cell` to an em
            // size). Do not inline numbers here again.
            const FontSpec spec = RoleFontSpec(role);
            m_fonts[i] = CreateFontW(-spec.cell, 0, 0, 0, spec.weight,
                                     FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH, kFamilyUi);
        }
        return m_fonts[i];
    }

    HDC m_dc = nullptr;
    HFONT m_fonts[(int)FontRole::Count] = {};
    std::stack<int> m_clipStack;
};

} // namespace ui
} // namespace air
