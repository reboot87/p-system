#include "airui/Theme.h"

namespace air {
namespace ui {

// ============================================================
// 预置配色
// ============================================================

// 默认浅色：企业级 B 端（对标 Ant Design 5 默认主题）
Palette LightPalette()
{
    Palette p{};

    // ---- 底层表面 ----
    p.bg          = RGB(240, 242, 245);   // #F0F2F5 bg-layout
    p.surface     = RGB(255, 255, 255);
    p.sunken      = RGB(247, 248, 250);   // 灰底内容框
    p.sidebar     = RGB(251, 252, 253);
    p.sidebarLine = RGB(233, 236, 241);
    p.split       = RGB(240, 240, 240);   // #F0F0F0 border-split
    p.border      = RGB(217, 217, 217);   // #D9D9D9 border-base

    // ---- 文本 ----
    p.text         = RGB( 31,  31,  31);  // 88%
    p.textSub      = RGB( 89,  89,  89);  // 65%
    p.textFaint    = RGB(140, 140, 140);  // 45%
    p.textDisabled = RGB(169, 174, 184);
    p.textInverse  = RGB(255, 255, 255);

    // ---- 交互态 ----
    p.hover        = RGB(250, 250, 250);
    p.active       = RGB(242, 244, 247);
    p.selectFill   = RGB(230, 244, 255);  // #E6F4FF
    p.selectRing   = RGB(214, 230, 255);
    p.hoverBorder  = RGB(198, 212, 236);

    // ---- 品牌 ----
    p.primary      = RGB( 22, 119, 255);  // #1677FF
    p.primaryHover = RGB( 64, 150, 255);  // #4096FF
    p.primaryDown  = RGB(  9,  88, 217);  // #0958D9
    p.primarySoft  = RGB(230, 244, 255);
    p.primaryText  = RGB( 22, 104, 220);

    // ---- 成功 ----
    p.ok       = RGB( 82, 196,  26);      // #52C41A
    p.okSoft   = RGB(246, 255, 237);
    p.okText   = RGB( 56, 158,  13);
    p.okBorder = RGB(183, 235, 143);

    // ---- 警告 ----
    p.warn       = RGB(250, 173,  20);
    p.warnSoft   = RGB(255, 251, 230);
    p.warnText   = RGB(212, 136,   6);
    p.warnBorder = RGB(255, 229, 143);

    // ---- 错误 ----
    p.error       = RGB(255,  77,  79);
    p.errorSoft   = RGB(255, 241, 240);
    p.errorText   = RGB(207,  19,  34);
    p.errorBorder = RGB(255, 204, 199);

    // ---- 组件专用 ----
    p.noteBg           = RGB(255, 254, 245);
    p.noteBorder       = RGB(212, 217, 225);
    p.iconTile         = RGB(242, 243, 245);
    p.iconTileHover    = RGB(242, 243, 245);
    p.chipBg           = RGB(237, 239, 242);
    p.chipBorder       = RGB(214, 218, 224);
    p.captionBtnHover  = RGB(229, 229, 229);
    p.captionCloseHover= RGB(232,  17,  35);
    p.scrollThumb      = RGB(205, 210, 218);
    return p;
}

// 深色（预留：结构已就绪，未启用）
Palette DarkPalette()
{
    Palette p{};

    p.bg          = RGB( 24,  26,  30);
    p.surface     = RGB( 33,  36,  41);
    p.sunken      = RGB( 27,  30,  35);
    p.sidebar     = RGB( 28,  31,  36);
    p.sidebarLine = RGB( 44,  48,  55);
    p.split       = RGB( 44,  48,  55);
    p.border      = RGB( 60,  65,  74);

    p.text         = RGB(232, 235, 240);
    p.textSub      = RGB(176, 182, 192);
    p.textFaint    = RGB(130, 137, 148);
    p.textDisabled = RGB( 95, 101, 112);
    p.textInverse  = RGB(255, 255, 255);

    p.hover        = RGB( 41,  45,  52);
    p.active       = RGB( 48,  53,  61);
    p.selectFill   = RGB( 23,  50,  84);
    p.selectRing   = RGB( 33,  72, 118);
    p.hoverBorder  = RGB( 72,  95, 128);

    p.primary      = RGB( 64, 150, 255);
    p.primaryHover = RGB( 96, 170, 255);
    p.primaryDown  = RGB( 40, 122, 220);
    p.primarySoft  = RGB( 23,  50,  84);
    p.primaryText  = RGB(120, 180, 255);

    p.ok       = RGB( 82, 196,  26);
    p.okSoft   = RGB( 30,  48,  26);
    p.okText   = RGB(120, 200,  90);
    p.okBorder = RGB( 48,  86,  36);

    p.warn       = RGB(250, 173,  20);
    p.warnSoft   = RGB( 56,  44,  20);
    p.warnText   = RGB(240, 190,  80);
    p.warnBorder = RGB( 92,  70,  26);

    p.error       = RGB(255,  99, 100);
    p.errorSoft   = RGB( 56,  30,  32);
    p.errorText   = RGB(255, 130, 130);
    p.errorBorder = RGB( 96,  44,  46);

    p.noteBg           = RGB( 46,  42,  28);
    p.noteBorder       = RGB( 70,  65,  46);
    p.iconTile         = RGB( 45,  49,  56);
    p.iconTileHover    = RGB( 52,  57,  65);
    p.chipBg           = RGB( 45,  49,  56);
    p.chipBorder       = RGB( 60,  65,  74);
    p.captionBtnHover  = RGB( 52,  57,  65);
    p.captionCloseHover= RGB(196,  22,  28);
    p.scrollThumb      = RGB( 78,  84,  94);
    return p;
}

// ============================================================
// 当前生效的令牌（单 UI 线程访问，无需加锁）
// ============================================================
namespace {
Palette g_palette = LightPalette();
Metrics g_metrics = Metrics();
}

const Palette& Pal() { return g_palette; }
const Metrics& Mx() { return g_metrics; }

void SetPalette(const Palette& p) { g_palette = p; }
void SetMetrics(const Metrics& m) { g_metrics = m; }

// ------------------------------------------------------------
// DPI 缩放：按比例放大所有度量（字体尺寸由 Fonts 单独缩放）
// ------------------------------------------------------------
Metrics Metrics::ForDpi(int dpi) const
{
    if (dpi <= 0) dpi = 96;
    Metrics m = *this;
    auto s = [dpi](int v) {
        int r = (int)(v * dpi / 96.0 + 0.5);
        return r < 1 ? 1 : r;
    };
    m.titleH       = s(titleH);
    m.capBtnW      = s(capBtnW);
    m.sidebarW     = s(sidebarW);
    m.statusH      = s(statusH);
    m.unit         = s(unit);
    m.pagePad      = s(pagePad);
    m.cardGap      = s(cardGap);
    m.radiusSm     = s(radiusSm);
    m.radiusMd     = s(radiusMd);
    m.radiusLg     = s(radiusLg);
    m.radiusCard   = s(radiusCard);
    m.radiusPill   = s(radiusPill);
    m.rowH         = s(rowH);
    m.inputH       = s(inputH);
    m.searchH      = s(searchH);
    m.btnH         = s(btnH);
    m.tagH         = s(tagH);
    m.statusTagH   = s(statusTagH);
    m.itemH        = s(itemH);
    m.groupH       = s(groupH);
    m.fontBody     = s(fontBody);
    m.fontMeta     = s(fontMeta);
    m.fontTitle    = s(fontTitle);
    m.fontSection  = s(fontSection);
    m.fontButton   = s(fontButton);
    m.fontIcon     = s(fontIcon);
    m.fontIconLg   = s(fontIconLg);
    m.fontDisplay  = s(fontDisplay);
    return m;
}

} // namespace ui
} // namespace air
