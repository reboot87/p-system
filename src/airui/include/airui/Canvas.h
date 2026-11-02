#pragma once
// ============================================================
// AirUI — Canvas.h
// 画布：页面绘制的高级入口。封装 HDC / HFONT / SetBkMode 等
// GDI 细节，页面代码只需要「画什么、画在哪、什么颜色」。
//
//   void MyPage::OnPaint(Canvas& g, const RECT& content) {
//       g.FillRound(rc, Pal().surface, Mx().radiusLg);
//       g.Text(rcTitle, L"标题", FontRole::Section, Pal().text);
//   }
//
// 字体通过 FontRole 语义化选取（由 Shell 按当前 DPI 创建），
// 颜色来自 Theme 调色板（COLORREF 是纯值类型，不是 GDI 句柄）。
// ============================================================
#include <windows.h>
#include <string>
#include "Controls.h"
#include "Fonts.h"
#include "Theme.h"

namespace air {
namespace ui {

struct EmptyState;

class Canvas
{
public:
    // 库内构造（Shell / Widget 在绘制时机传入）；页面只管收着用
    Canvas(HDC dc, const Fonts& fonts);

    // 逃生舱：极少数需要原生 GDI 的场景（一般用不到）
    HDC Native() const { return m_dc; }

    // ---- 形状 ----
    void Fill(const RECT& rc, COLORREF color);
    void FillRound(const RECT& rc, COLORREF color, int radius);
    // radius 为 0 时退化为直角矩形
    void Stroke(const RECT& rc, COLORREF color, int radius, int width = 1);
    // 虚线描边（「可点击但未激活」的提示框）
    void StrokeDashed(const RECT& rc, COLORREF color, int radius);
    void HLine(int x1, int x2, int y, COLORREF color);
    void HLineDashed(int x1, int x2, int y, COLORREF color);
    void VLine(int x, int y1, int y2, COLORREF color);
    void Dot(int cx, int cy, int diameter, COLORREF color);
    // 进度条（track 底槽；fill 按阈值在 normal/warn/hot 间变色）
    void ProgressBar(const RECT& rc, int pct, COLORREF track,
                     COLORREF colorNormal, COLORREF colorWarn, COLORREF colorHot);

    // ---- 文本（按角色选字体） ----
    void Text(const RECT& rc, const std::wstring& s, FontRole role,
              COLORREF color,
              UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    // 单行左对齐 + 垂直居中 + 尾部省略（最高频组合）
    void TextRow(const RECT& rc, const std::wstring& s, FontRole role,
                 COLORREF color, UINT extra = 0);
    void TextRowRight(const RECT& rc, const std::wstring& s, FontRole role,
                      COLORREF color, UINT extra = 0);
    void TextRowCenter(const RECT& rc, const std::wstring& s, FontRole role,
                       COLORREF color, UINT extra = 0);
    // 多行折行 + 尾部省略
    void TextBlock(const RECT& rc, const std::wstring& s, FontRole role,
                   COLORREF color, UINT extra = 0);

    int MeasureWidth(const std::wstring& s, FontRole role);
    int LineHeight(FontRole role);

    // ---- 复合部件 ----
    // 行内标签：浅底深字，圆角 2px。返回右边界 x。
    int  Tag(int x, int y, const std::wstring& s, FontRole role,
             COLORREF bg, COLORREF tx, int height = 20);
    // 右对齐胶囊（卡片右上角状态）
    void PillRight(int rightX, int y, const std::wstring& s, FontRole role,
                   COLORREF bg, COLORREF tx, int height = 20);
    // 状态胶囊：浅底 + 描边 + 同色圆点 + 深字
    void StatusTag(int rightX, int centerY, const std::wstring& s,
                   FontRole role, COLORREF bg, COLORREF border,
                   COLORREF tx, COLORREF dot, int height = 24);
    // 键位提示 chip（右对齐到 rightX）
    void KbdChip(int rightX, int centerY, const std::wstring& key,
                 const std::wstring& label);
    int  KbdChipWidth(const std::wstring& key, const std::wstring& label);
    // 图标块：圆角方块 + 居中字形（Segoe MDL2）
    void IconTile(const RECT& rc, const wchar_t* glyph, FontRole role,
                  COLORREF bg, COLORREF tx, int radius = 8);
    // 列表空状态（三段式引导）
    void EmptyState(const RECT& rc, const air::ui::EmptyState& state);
    // 按钮（纯视觉；点击由页面在 OnMouseDown 里命中）
    void Button(const RECT& rc, const std::wstring& text, BtnStyle style,
                bool hover = false, bool pressed = false, bool disabled = false);

private:
    HDC          m_dc;
    const Fonts& m_fonts;
};

} // namespace ui
} // namespace air
