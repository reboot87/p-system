#pragma once
// ============================================================
// AirUI — Gfx.h
// 绘制基元：圆角卡片、描边、分隔线、文本、标签、胶囊、键位 chip、进度条
//
// 约定：
//   · 所有函数无状态，不改动调用方的 GDI 选择对象（内部用 RAII 恢复）
//   · 坐标为逻辑像素，调用方负责 DPI 换算
//   · 描边一律画在矩形内侧 1px（right/bottom 各 +1 补偿）
// ============================================================
#include <windows.h>
#include <string>

namespace air {
namespace ui {
namespace gfx {

// ------------------------------------------------------------
// GDI 对象 RAII —— 杜绝 CreateXxx 忘记 DeleteObject 造成的泄漏
// ------------------------------------------------------------
class Brush
{
public:
    explicit Brush(COLORREF color) : m_h(CreateSolidBrush(color)) {}
    ~Brush() { if (m_h) DeleteObject(m_h); }
    Brush(const Brush&) = delete;
    Brush& operator=(const Brush&) = delete;
    operator HBRUSH() const { return m_h; }
private:
    HBRUSH m_h;
};

class Pen
{
public:
    Pen(COLORREF color, int width = 1, int style = PS_SOLID)
        : m_h(CreatePen(style, width, color)) {}
    ~Pen() { if (m_h) DeleteObject(m_h); }
    Pen(const Pen&) = delete;
    Pen& operator=(const Pen&) = delete;
    operator HPEN() const { return m_h; }
private:
    HPEN m_h;
};

// 把画刷/画笔临时选入 DC，析构自动换回
class UseBrush
{
public:
    UseBrush(HDC dc, HBRUSH br) : m_dc(dc), m_old((HBRUSH)SelectObject(dc, br)) {}
    ~UseBrush() { SelectObject(m_dc, m_old); }
    UseBrush(const UseBrush&) = delete;
    UseBrush& operator=(const UseBrush&) = delete;
private:
    HDC   m_dc;
    HBRUSH m_old;
};

class UsePen
{
public:
    UsePen(HDC dc, HPEN pen) : m_dc(dc), m_old((HPEN)SelectObject(dc, pen)) {}
    ~UsePen() { SelectObject(m_dc, m_old); }
    UsePen(const UseBrush&) = delete;
    UsePen& operator=(const UsePen&) = delete;
private:
    HDC  m_dc;
    HPEN m_old;
};

class UseFont
{
public:
    UseFont(HDC dc, HFONT f) : m_dc(dc), m_old((HFONT)SelectObject(dc, f)) {}
    ~UseFont() { SelectObject(m_dc, m_old); }
    UseFont(const UseFont&) = delete;
    UseFont& operator=(const UseFont&) = delete;
private:
    HDC   m_dc;
    HFONT m_old;
};

// ------------------------------------------------------------
// 填充 / 描边
// ------------------------------------------------------------
void Fill(HDC dc, const RECT& rc, COLORREF color);
void FillRound(HDC dc, const RECT& rc, COLORREF color, int radius);
// radius 为 0 时退化为直角矩形
void FillShape(HDC dc, const RECT& rc, COLORREF color, int radius);

// 1px 实线描边（画在矩形内侧）
void Stroke(HDC dc, const RECT& rc, COLORREF color, int radius, int width = 1);
// 1px 虚线描边（用于「可点击但未激活」的提示框）
void StrokeDashed(HDC dc, const RECT& rc, COLORREF color, int radius);

void HLine(HDC dc, int x1, int x2, int y, COLORREF color);
void HLineDashed(HDC dc, int x1, int x2, int y, COLORREF color);
void VLine(HDC dc, int x, int y1, int y2, COLORREF color);
void Dot(HDC dc, int cx, int cy, int diameter, COLORREF color);

// 进度条（track 为底槽颜色，fill 由 pct 决定长度并按阈值变色）
void ProgressBar(HDC dc, const RECT& rc, int pct, COLORREF track,
                 COLORREF colorNormal, COLORREF colorWarn, COLORREF colorHot);

// ------------------------------------------------------------
// 文本
// ------------------------------------------------------------
// DrawText 的薄封装：明确设置透明背景 + 字体 + 颜色
void Text(HDC dc, HFONT font, COLORREF color, const RECT& rc,
          const wchar_t* s, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE);
void Text(HDC dc, HFONT font, COLORREF color, const RECT& rc,
          const std::wstring& s, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE);

// 单行左对齐 + 垂直居中 + 尾部省略（最高频组合）
void TextRow(HDC dc, HFONT font, COLORREF color, const RECT& rc,
             const std::wstring& s, UINT extra = 0);
// 单行右对齐
void TextRowRight(HDC dc, HFONT font, COLORREF color, const RECT& rc,
                  const std::wstring& s, UINT extra = 0);
// 单行居中
void TextRowCenter(HDC dc, HFONT font, COLORREF color, const RECT& rc,
                   const std::wstring& s, UINT extra = 0);
// 多行折行 + 尾部省略
void TextBlock(HDC dc, HFONT font, COLORREF color, const RECT& rc,
               const std::wstring& s, UINT extra = 0);

int  MeasureWidth(HDC dc, HFONT font, const wchar_t* s, int len = -1);
int  MeasureWidth(HDC dc, HFONT font, const std::wstring& s);
int  LineHeight(HDC dc, HFONT font);

// 把字符串里的换行替换为空格（列表单行展示用）
std::wstring OneLine(const std::wstring& s);

// ------------------------------------------------------------
// 复合小部件（纯绘制，位置由调用方给出）
// ------------------------------------------------------------
// 行内标签：浅底 + 深字，圆角 2px，高 tagH。返回右边界 x。
int Tag(HDC dc, int x, int y, const wchar_t* text, HFONT font,
        COLORREF bg, COLORREF tx, int height = 20);

// 右对齐胶囊（模块卡右上角状态）：圆角 = 高/2
void PillRight(HDC dc, int rightX, int y, const wchar_t* text, HFONT font,
               COLORREF bg, COLORREF tx, int height = 20);

// 状态胶囊：浅底 + 描边 + 同色圆点 + 深字（标题栏服务状态）
void StatusTag(HDC dc, int rightX, int centerY, const wchar_t* text, HFONT font,
               COLORREF bg, COLORREF border, COLORREF tx, COLORREF dot,
               int height = 24);

// 键位提示 chip（底 + 描边 + 按键文字 + 说明文字），右对齐到 rightX
void KbdChip(HDC dc, int rightX, int centerY, const wchar_t* key,
             const wchar_t* label, HFONT font);
int  KbdChipWidth(HDC dc, const wchar_t* key, const wchar_t* label, HFONT font);

// 图标块：圆角方块 + 居中字形（Segoe MDL2）
void IconTile(HDC dc, const RECT& rc, const wchar_t* glyph, HFONT iconFont,
              COLORREF bg, COLORREF tx, int radius = 8);

} // namespace gfx
} // namespace ui
} // namespace air
