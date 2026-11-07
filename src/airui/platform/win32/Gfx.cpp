#include "airui/Gfx.h"
#include "airui/PaintFrame.h"
#include "airui/Theme.h"

namespace air {
namespace ui {
namespace gfx {

// ============================================================
// 填充 / 描边
// ============================================================
// When a PaintFrame is active all primitives go to the runtime
// selected backend. With no frame the original GDI code below runs
// unchanged, so the default renderer stays pixel-for-pixel identical.
void Fill(HDC dc, const RECT& rc, COLORREF color)
{
    if (PaintFrame* f = CurrentPaintFrame()) { f->GfxFill(rc, color); return; }
    Brush br(color);
    RECT r = rc;
    ::FillRect(dc, &r, br);
}

void FillRound(HDC dc, const RECT& rc, COLORREF color, int radius)
{
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;
    if (PaintFrame* f = CurrentPaintFrame()) {
        f->GfxFillRound(rc, color, radius); return;
    }
    Brush br(color);
    Pen   pen(0, 1, PS_NULL);
    UseBrush ub(dc, br);
    UsePen   up(dc, pen);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
}

void FillShape(HDC dc, const RECT& rc, COLORREF color, int radius)
{
    if (radius <= 0) Fill(dc, rc, color);
    else             FillRound(dc, rc, color, radius);
}

void Stroke(HDC dc, const RECT& rc, COLORREF color, int radius, int width)
{
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;
    if (PaintFrame* f = CurrentPaintFrame()) {
        f->GfxStroke(rc, color, radius, width); return;
    }
    Pen pen(color, width);
    UsePen   up(dc, pen);
    UseBrush ub(dc, (HBRUSH)GetStockObject(NULL_BRUSH));
    if (radius <= 0) {
        // 直角：Rectangle 的右/下边为排他边界，边框自然落在矩形内侧
        int n = width < 1 ? 1 : width;
        RECT r = { rc.left + n / 2, rc.top + n / 2,
                   rc.right - n / 2, rc.bottom - n / 2 };
        if (r.right <= r.left) r.right = r.left + 1;
        if (r.bottom <= r.top) r.bottom = r.top + 1;
        Rectangle(dc, r.left, r.top, r.right, r.bottom);
    } else {
        // 圆角：GDI 的 RoundRect 描边以边界为中线，right/bottom 各 +1
        // 让 1px 边框完整落在矩形内侧，不侵蚀邻居像素
        RoundRect(dc, rc.left, rc.top, rc.right + 1, rc.bottom + 1,
                  radius * 2, radius * 2);
    }
}

void StrokeDashed(HDC dc, const RECT& rc, COLORREF color, int radius)
{
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;
    if (PaintFrame* f = CurrentPaintFrame()) {
        f->GfxStrokeDashed(rc, color, radius); return;
    }
    Pen pen(color, 1, PS_DASH);
    UsePen   up(dc, pen);
    UseBrush ub(dc, (HBRUSH)GetStockObject(NULL_BRUSH));
    RoundRect(dc, rc.left, rc.top, rc.right + 1, rc.bottom + 1,
              radius * 2, radius * 2);
}

void HLine(HDC dc, int x1, int x2, int y, COLORREF color)
{
    if (PaintFrame* f = CurrentPaintFrame()) {
        f->GfxHLine(x1, x2, y, color); return;
    }
    Brush br(color);
    RECT r = { x1, y, x2, y + 1 };
    ::FillRect(dc, &r, br);
}

void HLineDashed(HDC dc, int x1, int x2, int y, COLORREF color)
{
    if (PaintFrame* f = CurrentPaintFrame()) {
        f->GfxHLineDashed(x1, x2, y, color); return;
    }
    Pen pen(color, 1, PS_DASH);
    UsePen up(dc, pen);
    MoveToEx(dc, x1, y, nullptr);
    LineTo(dc, x2, y);
}

void VLine(HDC dc, int x, int y1, int y2, COLORREF color)
{
    if (PaintFrame* f = CurrentPaintFrame()) {
        f->GfxVLine(x, y1, y2, color); return;
    }
    Brush br(color);
    RECT r = { x, y1, x + 1, y2 };
    ::FillRect(dc, &r, br);
}

void Dot(HDC dc, int cx, int cy, int diameter, COLORREF color)
{
    if (PaintFrame* f = CurrentPaintFrame()) {
        f->GfxDot(cx, cy, diameter, color); return;
    }
    Brush br(color);
    Pen   pen(0, 1, PS_NULL);
    UseBrush ub(dc, br);
    UsePen   up(dc, pen);
    int r = diameter / 2;
    Ellipse(dc, cx - r, cy - r, cx - r + diameter, cy - r + diameter);
}

void ProgressBar(HDC dc, const RECT& rc, int pct, COLORREF track,
                 COLORREF colorNormal, COLORREF colorWarn, COLORREF colorHot)
{
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;
    Fill(dc, rc, track);
    if (pct <= 0) return;
    if (pct > 100) pct = 100;
    COLORREF c = (pct > 70) ? colorHot : (pct > 45) ? colorWarn : colorNormal;
    RECT fill = rc;
    fill.right = rc.left + (int)((rc.right - rc.left) * pct / 100.0);
    if (fill.right > rc.right) fill.right = rc.right;
    Fill(dc, fill, c);
}

// ============================================================
// 文本
// ============================================================
void Text(HDC dc, HFONT font, COLORREF color, const RECT& rc,
          const wchar_t* s, UINT format)
{
    if (!s) return;
    if (PaintFrame* f = CurrentPaintFrame()) {
        f->GfxText(font, color, rc, s, format); return;
    }
    RECT r = rc;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    UseFont uf(dc, font);
    DrawTextW(dc, s, -1, &r, format);
}

void Text(HDC dc, HFONT font, COLORREF color, const RECT& rc,
          const std::wstring& s, UINT format)
{
    Text(dc, font, color, rc, s.c_str(), format);
}

void TextRow(HDC dc, HFONT font, COLORREF color, const RECT& rc,
             const std::wstring& s, UINT extra)
{
    Text(dc, font, color, rc, s,
         DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS | extra);
}

void TextRowRight(HDC dc, HFONT font, COLORREF color, const RECT& rc,
                  const std::wstring& s, UINT extra)
{
    Text(dc, font, color, rc, s,
         DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_END_ELLIPSIS | extra);
}

void TextRowCenter(HDC dc, HFONT font, COLORREF color, const RECT& rc,
                   const std::wstring& s, UINT extra)
{
    Text(dc, font, color, rc, s,
         DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | extra);
}

void TextBlock(HDC dc, HFONT font, COLORREF color, const RECT& rc,
               const std::wstring& s, UINT extra)
{
    Text(dc, font, color, rc, s,
         DT_WORDBREAK | DT_END_ELLIPSIS | DT_LEFT | extra);
}

int MeasureWidth(HDC dc, HFONT font, const wchar_t* s, int len)
{
    if (!s) return 0;
    if (len < 0) len = (int)wcslen(s);
    if (len == 0) return 0;
    UseFont uf(dc, font);
    SIZE sz{};
    GetTextExtentPoint32W(dc, s, len, &sz);
    return sz.cx;
}

int MeasureWidth(HDC dc, HFONT font, const std::wstring& s)
{
    return MeasureWidth(dc, font, s.c_str(), (int)s.size());
}

int LineHeight(HDC dc, HFONT font)
{
    UseFont uf(dc, font);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    return tm.tmHeight + tm.tmExternalLeading;
}

std::wstring OneLine(const std::wstring& s)
{
    std::wstring out = s;
    for (auto& c : out)
        if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
    return out;
}

// ============================================================
// 复合小部件
// ============================================================
int Tag(HDC dc, int x, int y, const wchar_t* text, HFONT font,
        COLORREF bg, COLORREF tx, int height)
{
    int w = MeasureWidth(dc, font, text) + 16;
    RECT rc = { x, y, x + w, y + height };
    FillRound(dc, rc, bg, 2);
    RECT inner = { rc.left + 8, rc.top, rc.right - 8, rc.bottom };
    Text(dc, font, tx, inner, text,
         DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    return rc.right;
}

void PillRight(HDC dc, int rightX, int y, const wchar_t* text, HFONT font,
               COLORREF bg, COLORREF tx, int height)
{
    int w = MeasureWidth(dc, font, text) + 18;
    RECT rc = { rightX - w, y, rightX, y + height };
    FillRound(dc, rc, bg, height / 2);
    RECT inner = { rc.left + 9, rc.top, rc.right - 9, rc.bottom };
    Text(dc, font, tx, inner, text,
         DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
}

void StatusTag(HDC dc, int rightX, int centerY, const wchar_t* text, HFONT font,
               COLORREF bg, COLORREF border, COLORREF tx, COLORREF dot,
               int height)
{
    int tw = MeasureWidth(dc, font, text);
    int w  = tw + 34;
    RECT rc = { rightX - w, centerY - height / 2,
                rightX,     centerY + height / 2 };
    FillRound(dc, rc, bg, height / 2);
    Stroke(dc, rc, border, height / 2);
    Dot(dc, rc.left + 14, centerY, 8, dot);
    RECT inner = { rc.left + 26, rc.top, rc.right - 4, rc.bottom };
    Text(dc, font, tx, inner, text,
         DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
}

int KbdChipWidth(HDC dc, const wchar_t* key, const wchar_t* label, HFONT font)
{
    return MeasureWidth(dc, font, key) + 12 + 5 + MeasureWidth(dc, font, label);
}

void KbdChip(HDC dc, int rightX, int centerY, const wchar_t* key,
             const wchar_t* label, HFONT font)
{
    int kw = MeasureWidth(dc, font, key);
    int lw = MeasureWidth(dc, font, label);
    int chipW = kw + 12;
    int total = chipW + 5 + lw;
    int x = rightX - total;

    RECT rc = { x, centerY - 9, x + chipW, centerY + 9 };
    FillRound(dc, rc, Pal().chipBg, 4);
    Stroke(dc, rc, Pal().chipBorder, 4);
    {
        RECT rk = { rc.left + 6, rc.top, rc.left + 6 + kw, rc.bottom };
        Text(dc, font, RGB(90, 98, 112), rk, key,
             DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    }
    {
        RECT rl = { x + chipW + 5, centerY - 10, rightX, centerY + 10 };
        Text(dc, font, Pal().textFaint, rl, label,
             DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    }
}

void IconTile(HDC dc, const RECT& rc, const wchar_t* glyph, HFONT iconFont,
              COLORREF bg, COLORREF tx, int radius)
{
    FillRound(dc, rc, bg, radius);
    Text(dc, iconFont, tx, rc, glyph,
         DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
}

} // namespace gfx
} // namespace ui
} // namespace air
