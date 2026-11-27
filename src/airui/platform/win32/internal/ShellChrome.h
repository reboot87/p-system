#pragma once
// ============================================================
// AirUI - ShellChrome.h (Win32 backend, private)
//
// 外壳标题栏状态胶囊 / 状态栏键位提示沿用的旧 gfx 复合件几何。
//
// 公开的 Canvas::StatusTag / KbdChip 走 core 的 Paint* 版本：圆点按
// Metrics::unit 定位、键位宽度按字符数估算，与外壳用了多年的固定像素
// 几何（圆点在左 14px、直径 8，chip 宽度 = 文本宽 + 12 + 5）不同。
// 外壳像素要求保持不变，所以这里用 IPainter 原语按旧几何重放。
//
// 放在头里而不是 Shell.cpp 的匿名命名空间，是为了让 test_paint_frame
// 能拿同一份代码与 gfx::StatusTag / gfx::KbdChip 做逐像素回归。
// ============================================================
#include <string>

#include "Fonts.h"
#include "airui/Theme.h"
#include "internal/Canvas.h"

namespace air {
namespace ui {

inline void PaintStatusTagLegacy(Canvas& g, int rightX, int centerY,
                                 const std::wstring& s, FontRole role,
                                 Color bg, Color border, Color tx, Color dot,
                                 int height)
{
    const int w = g.MeasureWidth(s, role) + 34;
    const Rect rc{ rightX - w, centerY - height / 2,
                   rightX, centerY + height / 2 };
    g.FillRound(rc, bg, height / 2);
    g.Stroke(rc, border, height / 2);
    g.Dot(rc.left + 14, centerY, 8, dot);
    g.Text(Rect{ rc.left + 26, rc.top, rc.right - 4, rc.bottom }, s, role, tx,
           dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix);
}

inline int KbdChipWidthLegacy(Canvas& g, const std::wstring& key,
                              const std::wstring& label, FontRole role)
{
    return g.MeasureWidth(key, role) + 12 + 5 +
           g.MeasureWidth(label, role);
}

inline void PaintKbdChipLegacy(Canvas& g, int rightX, int centerY,
                               const std::wstring& key,
                               const std::wstring& label, FontRole role)
{
    const int kw = g.MeasureWidth(key, role);
    const int lw = g.MeasureWidth(label, role);
    const int chipW = kw + 12;
    const int x = rightX - (chipW + 5 + lw);
    const Rect rc{ x, centerY - 9, x + chipW, centerY + 9 };
    g.FillRound(rc, Pal().chipBg, 4);
    g.Stroke(rc, Pal().chipBorder, 4);
    g.Text(Rect{ rc.left + 6, rc.top, rc.left + 6 + kw, rc.bottom }, key,
           role, RGB(90, 98, 112),
           dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix);
    g.Text(Rect{ x + chipW + 5, centerY - 10, rightX, centerY + 10 }, label,
           role, Pal().textFaint,
           dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix);
}

} // namespace ui
} // namespace air
