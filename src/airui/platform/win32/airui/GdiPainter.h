#pragma once
// ============================================================
// AirUI - GdiPainter.h
// Minimal IPainter implementation backed by a GDI HDC.
// Phase 1 HAL stub for Win32 backend.
// ============================================================
#include "airui/hal.h"
#include <windows.h>
#include <string>

namespace air {
namespace ui {

class GdiPainter : public IPainter
{
public:
    explicit GdiPainter(HDC hdc) : m_dc(hdc) {}

    void FillRect(const Rect& r, Color c) override {
        if (!m_dc) return;
        RECT rc = {r.left, r.top, r.right, r.bottom};
        HBRUSH br = CreateSolidBrush(c);
        FillRect(m_dc, &rc, br);
        DeleteObject(br);
    }

    void FillRound(const Rect& r, int radius, Color c) override {
        // Simple fallback: fill rect
        FillRect(r, c);
        (void)radius;
    }

    void StrokeRect(const Rect& r, Color c, int width) override {
        if (!m_dc) return;
        RECT rc = {r.left, r.top, r.right, r.bottom};
        HPEN pen = CreatePen(PS_SOLID, width, c);
        HGDIOBJ oldPen = SelectObject(m_dc, pen);
        HGDIOBJ oldBrush = SelectObject(m_dc, GetStockObject(NULL_BRUSH));
        Rectangle(m_dc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(m_dc, oldPen);
        SelectObject(m_dc, oldBrush);
        DeleteObject(pen);
    }

    void StrokeRound(const Rect& r, int radius, Color c, int width) override {
        StrokeRect(r, c, width);
        (void)radius;
    }

    void StrokeDashed(const Rect& r, Color c, int width, int dash) override {
        StrokeRect(r, c, width);
        (void)dash;
    }

    void Text(const Rect& r, const std::wstring& s, FontRole role, Color c, TextFormat fmt) override {
        if (!m_dc || s.empty()) return;
        SetTextColor(m_dc, c);
        SetBkMode(m_dc, TRANSPARENT);
        RECT rc = {r.left, r.top, r.right, r.bottom};
        DrawTextW(m_dc, s.c_str(), -1, &rc, (UINT)fmt);
        (void)role;
    }

    void TextBlock(const Rect& r, const std::wstring& s, FontRole role, Color c, int maxLines) override {
        Text(r, s, role, c, DT_WORDBREAK);
        (void)maxLines;
    }

    int MeasureText(const std::wstring& s, FontRole role) override {
        (void)role;
        if (!m_dc || s.empty()) return 0;
        SIZE sz{};
        GetTextExtentPoint32W(m_dc, s.c_str(), (int)s.size(), &sz);
        return sz.cx;
    }

    void PushClip(const Rect& r) override {
        if (!m_dc) return;
        RECT rc = {r.left, r.top, r.right, r.bottom};
        IntersectClipRect(m_dc, rc.left, rc.top, rc.right, rc.bottom);
    }

    void PopClip() override {
        if (!m_dc) return;
        SelectClipRgn(m_dc, nullptr);
    }

    void Translate(Point offset) override {
        if (m_dc) SetViewportOrgEx(m_dc, offset.x, offset.y, nullptr);
    }

private:
    HDC m_dc = nullptr;
};

} // namespace ui
} // namespace air
