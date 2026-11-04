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
    p.bg          = MakeColor(240, 242, 245);   // #F0F2F5 bg-layout
    p.surface     = MakeColor(255, 255, 255);
    p.sunken      = MakeColor(247, 248, 250);   // 灰底内容框
    p.sidebar     = MakeColor(251, 252, 253);
    p.sidebarLine = MakeColor(233, 236, 241);
    p.split       = MakeColor(240, 240, 240);   // #F0F0F0 border-split
    p.border      = MakeColor(217, 217, 217);   // #D9D9D9 border-base

    // ---- 文本 ----
    p.text         = MakeColor( 31,  31,  31);  // 88%
    p.textSub      = MakeColor( 89,  89,  89);  // 65%
    p.textFaint    = MakeColor(140, 140, 140);  // 45%
    p.textDisabled = MakeColor(169, 174, 184);
    p.textInverse  = MakeColor(255, 255, 255);

    // ---- 交互态 ----
    p.hover        = MakeColor(250, 250, 250);
    p.active       = MakeColor(242, 244, 247);
    p.selectFill   = MakeColor(230, 244, 255);  // #E6F4FF
    p.selectRing   = MakeColor(214, 230, 255);
    p.hoverBorder  = MakeColor(198, 212, 236);

    // ---- 品牌 ----
    p.primary      = MakeColor( 22, 119, 255);  // #1677FF
    p.primaryHover = MakeColor( 64, 150, 255);  // #4096FF
    p.primaryDown  = MakeColor(  9,  88, 217);  // #0958D9
    p.primarySoft  = MakeColor(230, 244, 255);
    p.primaryText  = MakeColor( 22, 104, 220);

    // ---- 成功 ----
    p.ok       = MakeColor( 82, 196,  26);      // #52C41A
    p.okSoft   = MakeColor(246, 255, 237);
    p.okText   = MakeColor( 56, 158,  13);
    p.okBorder = MakeColor(183, 235, 143);

    // ---- 警告 ----
    p.warn       = MakeColor(250, 173,  20);
    p.warnSoft   = MakeColor(255, 251, 230);
    p.warnText   = MakeColor(212, 136,   6);
    p.warnBorder = MakeColor(255, 229, 143);

    // ---- 错误 ----
    p.error       = MakeColor(255,  77,  79);
    p.errorSoft   = MakeColor(255, 241, 240);
    p.errorText   = MakeColor(207,  19,  34);
    p.errorBorder = MakeColor(255, 204, 199);

    // ---- 组件专用 ----
    p.noteBg           = MakeColor(255, 254, 245);
    p.noteBorder       = MakeColor(212, 217, 225);
    p.iconTile         = MakeColor(242, 243, 245);
    p.iconTileHover    = MakeColor(242, 243, 245);
    p.chipBg           = MakeColor(237, 239, 242);
    p.chipBorder       = MakeColor(214, 218, 224);
    p.captionBtnHover  = MakeColor(229, 229, 229);
    p.captionCloseHover= MakeColor(232,  17,  35);
    p.scrollThumb      = MakeColor(205, 210, 218);
    return p;
}

// 深色（预留：结构已就绪，未启用）
Palette DarkPalette()
{
    Palette p{};

    p.bg          = MakeColor( 24,  26,  30);
    p.surface     = MakeColor( 33,  36,  41);
    p.sunken      = MakeColor( 27,  30,  35);
    p.sidebar     = MakeColor( 28,  31,  36);
    p.sidebarLine = MakeColor( 44,  48,  55);
    p.split       = MakeColor( 44,  48,  55);
    p.border      = MakeColor( 60,  65,  74);

    p.text         = MakeColor(232, 235, 240);
    p.textSub      = MakeColor(176, 182, 192);
    p.textFaint    = MakeColor(130, 137, 148);
    p.textDisabled = MakeColor( 95, 101, 112);
    p.textInverse  = MakeColor(255, 255, 255);

    p.hover        = MakeColor( 41,  45,  52);
    p.active       = MakeColor( 48,  53,  61);
    p.selectFill   = MakeColor( 23,  50,  84);
    p.selectRing   = MakeColor( 33,  72, 118);
    p.hoverBorder  = MakeColor( 72,  95, 128);

    p.primary      = MakeColor( 64, 150, 255);
    p.primaryHover = MakeColor( 96, 170, 255);
    p.primaryDown  = MakeColor( 40, 122, 220);
    p.primarySoft  = MakeColor( 23,  50,  84);
    p.primaryText  = MakeColor(120, 180, 255);

    p.ok       = MakeColor( 82, 196,  26);
    p.okSoft   = MakeColor( 30,  48,  26);
    p.okText   = MakeColor(120, 200,  90);
    p.okBorder = MakeColor( 48,  86,  36);

    p.warn       = MakeColor(250, 173,  20);
    p.warnSoft   = MakeColor( 56,  44,  20);
    p.warnText   = MakeColor(240, 190,  80);
    p.warnBorder = MakeColor( 92,  70,  26);

    p.error       = MakeColor(255,  99, 100);
    p.errorSoft   = MakeColor( 56,  30,  32);
    p.errorText   = MakeColor(255, 130, 130);
    p.errorBorder = MakeColor( 96,  44,  46);

    p.noteBg           = MakeColor( 46,  42,  28);
    p.noteBorder       = MakeColor( 70,  65,  46);
    p.iconTile         = MakeColor( 45,  49,  56);
    p.iconTileHover    = MakeColor( 52,  57,  65);
    p.chipBg           = MakeColor( 45,  49,  56);
    p.chipBorder       = MakeColor( 60,  65,  74);
    p.captionBtnHover  = MakeColor( 52,  57,  65);
    p.captionCloseHover= MakeColor(196,  22,  28);
    p.scrollThumb      = MakeColor( 78,  84,  94);
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
