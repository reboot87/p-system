#include "airui/Canvas.h"
#include "airui/Gfx.h"
#include "airui/Controls.h"

namespace air {
namespace ui {

Canvas::Canvas(HDC dc, const Fonts& fonts) : m_dc(dc), m_fonts(fonts)
{
    if (m_dc) SetBkMode(m_dc, TRANSPARENT);
}

// ---- 形状 ----
void Canvas::Fill(const RECT& rc, COLORREF color)
{
    gfx::Fill(m_dc, rc, color);
}

void Canvas::FillRound(const RECT& rc, COLORREF color, int radius)
{
    gfx::FillRound(m_dc, rc, color, radius);
}

void Canvas::Stroke(const RECT& rc, COLORREF color, int radius, int width)
{
    gfx::Stroke(m_dc, rc, color, radius, width);
}

void Canvas::StrokeDashed(const RECT& rc, COLORREF color, int radius)
{
    gfx::StrokeDashed(m_dc, rc, color, radius);
}

void Canvas::HLine(int x1, int x2, int y, COLORREF color)
{
    gfx::HLine(m_dc, x1, x2, y, color);
}

void Canvas::HLineDashed(int x1, int x2, int y, COLORREF color)
{
    gfx::HLineDashed(m_dc, x1, x2, y, color);
}

void Canvas::VLine(int x, int y1, int y2, COLORREF color)
{
    gfx::VLine(m_dc, x, y1, y2, color);
}

void Canvas::Dot(int cx, int cy, int diameter, COLORREF color)
{
    gfx::Dot(m_dc, cx, cy, diameter, color);
}

void Canvas::ProgressBar(const RECT& rc, int pct, COLORREF track,
                         COLORREF colorNormal, COLORREF colorWarn,
                         COLORREF colorHot)
{
    gfx::ProgressBar(m_dc, rc, pct, track, colorNormal, colorWarn, colorHot);
}

// ---- 文本 ----
void Canvas::Text(const RECT& rc, const std::wstring& s, FontRole role,
                  COLORREF color, UINT format)
{
    gfx::Text(m_dc, m_fonts.Get(role), color, rc, s, format);
}

void Canvas::TextRow(const RECT& rc, const std::wstring& s, FontRole role,
                     COLORREF color, UINT extra)
{
    gfx::TextRow(m_dc, m_fonts.Get(role), color, rc, s, extra);
}

void Canvas::TextRowRight(const RECT& rc, const std::wstring& s, FontRole role,
                          COLORREF color, UINT extra)
{
    gfx::TextRowRight(m_dc, m_fonts.Get(role), color, rc, s, extra);
}

void Canvas::TextRowCenter(const RECT& rc, const std::wstring& s, FontRole role,
                           COLORREF color, UINT extra)
{
    gfx::TextRowCenter(m_dc, m_fonts.Get(role), color, rc, s, extra);
}

void Canvas::TextBlock(const RECT& rc, const std::wstring& s, FontRole role,
                       COLORREF color, UINT extra)
{
    gfx::TextBlock(m_dc, m_fonts.Get(role), color, rc, s, extra);
}

int Canvas::MeasureWidth(const std::wstring& s, FontRole role)
{
    return gfx::MeasureWidth(m_dc, m_fonts.Get(role), s);
}

int Canvas::LineHeight(FontRole role)
{
    return gfx::LineHeight(m_dc, m_fonts.Get(role));
}

// ---- 复合部件 ----
int Canvas::Tag(int x, int y, const std::wstring& s, FontRole role,
                COLORREF bg, COLORREF tx, int height)
{
    return gfx::Tag(m_dc, x, y, s.c_str(), m_fonts.Get(role), bg, tx, height);
}

void Canvas::PillRight(int rightX, int y, const std::wstring& s, FontRole role,
                       COLORREF bg, COLORREF tx, int height)
{
    gfx::PillRight(m_dc, rightX, y, s.c_str(), m_fonts.Get(role), bg, tx,
                   height);
}

void Canvas::StatusTag(int rightX, int centerY, const std::wstring& s,
                       FontRole role, COLORREF bg, COLORREF border,
                       COLORREF tx, COLORREF dot, int height)
{
    gfx::StatusTag(m_dc, rightX, centerY, s.c_str(), m_fonts.Get(role), bg,
                   border, tx, dot, height);
}

void Canvas::KbdChip(int rightX, int centerY, const std::wstring& key,
                     const std::wstring& label)
{
    gfx::KbdChip(m_dc, rightX, centerY, key.c_str(), label.c_str(),
                 m_fonts.Get(FontRole::Meta));
}

int Canvas::KbdChipWidth(const std::wstring& key, const std::wstring& label)
{
    return gfx::KbdChipWidth(m_dc, key.c_str(), label.c_str(),
                             m_fonts.Get(FontRole::Meta));
}

void Canvas::IconTile(const RECT& rc, const wchar_t* glyph, FontRole role,
                      COLORREF bg, COLORREF tx, int radius)
{
    gfx::IconTile(m_dc, rc, glyph, m_fonts.Get(role), bg, tx, radius);
}

void Canvas::EmptyState(const RECT& rc, const air::ui::EmptyState& state)
{
    DrawEmptyState(m_dc, rc, m_fonts, state);
}

void Canvas::Button(const RECT& rc, const std::wstring& text, BtnStyle style,
                    bool hover, bool pressed, bool disabled)
{
    DrawButton(m_dc, rc, text.c_str(), m_fonts.Get(FontRole::Button),
               style, hover, pressed, disabled);
}

} // namespace ui
} // namespace air
