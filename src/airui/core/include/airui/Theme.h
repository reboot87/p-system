#pragma once
// ============================================================
// AirUI — 轻量 Win32 自绘 UI 库
// Theme.h — 设计令牌（配色 + 度量）
//
// 设计基准：Ant Design 5 / 华为云控制台 的企业级 B 端规范。
//   · 页面底 bg-layout，卡片白底 + 1px 浅边框 + 8~12px 圆角
//   · 品牌主色覆盖率 < 5%（只用于主按钮/选中态/关键图标）
//   · 状态一律「浅底 + 同色点 + 深字」，不用大色块
//
// 用法：
//     air::ui::Pal().primary        取当前配色
//     air::ui::Mx().titleH          取当前度量
//     air::ui::SetPalette(darkPal)  运行期换肤（可选）
// ============================================================
#include "airui/types.h"

namespace air {
namespace ui {

// ------------------------------------------------------------
// 配色。所有字段均为语义名，不出现具体色相，
// 便于整体换肤（浅色 / 深色 / 品牌定制）。
// ------------------------------------------------------------
struct Palette
{
    // ---- 底层表面 ----
    Color bg;            // 页面底板（卡片之间的灰）
    Color surface;       // 卡片 / 容器 / 输入框底色
    Color sunken;        // 下沉区块（灰底内容框）
    Color sidebar;       // 侧栏底色
    Color sidebarLine;   // 侧栏右侧分隔线
    Color split;         // 卡片描边 / 细分隔线
    Color border;        // 输入框、次级按钮描边

    // ---- 文本 ----
    Color text;          // 主文本
    Color textSub;       // 次要文本（正文说明、取值）
    Color textFaint;     // 辅助文本（标签、元信息）
    Color textDisabled;  // 禁用
    Color textInverse;   // 深底上的文字（主按钮）

    // ---- 交互态 ----
    Color hover;         // 悬停填充
    Color active;        // 按下填充
    Color selectFill;    // 选中填充
    Color selectRing;    // 选中外环
    Color hoverBorder;   // 悬停描边

    // ---- 品牌 ----
    Color primary;
    Color primaryHover;
    Color primaryDown;
    Color primarySoft;   // 主色浅底（图标块 / 信息标签）
    Color primaryText;   // 主色文字（浅底上的深主色）

    // ---- 状态：成功 ----
    Color ok;
    Color okSoft;
    Color okText;
    Color okBorder;
    // ---- 状态：警告 ----
    Color warn;
    Color warnSoft;
    Color warnText;
    Color warnBorder;
    // ---- 状态：错误 ----
    Color error;
    Color errorSoft;
    Color errorText;
    Color errorBorder;

    // ---- 组件专用 ----
    Color noteBg;        // 备注区（淡黄提示底）
    Color noteBorder;
    Color iconTile;      // 列表项图标块底
    Color iconTileHover; // 悬停操作钮底
    Color chipBg;        // 键位提示 chip 底
    Color chipBorder;
    Color captionBtnHover;
    Color captionCloseHover;
    Color scrollThumb;
};

// ------------------------------------------------------------
// 度量。全部为 96 DPI 下的逻辑像素；
// 实际使用请经 Scaled() 按 DPI 缩放（见 Metrics::ForDpi）。
// ------------------------------------------------------------
struct Metrics
{
    // ---- 外壳 ----
    int titleH      = 44;   // 自绘标题栏高
    int capBtnW     = 46;   // 标题栏单键宽
    int sidebarW    = 208;  // 侧栏宽（0 = 无侧栏）
    int statusH     = 28;   // 底部状态栏高（0 = 无）

    // ---- 间距（8px 栅格） ----
    int unit        = 8;
    int pagePad     = 16;   // 内容区外边距
    int cardGap     = 12;   // 卡片间距

    // ---- 圆角 ----
    int radiusSm    = 4;    // 标签、chip
    int radiusMd    = 8;    // 输入框、按钮、小卡片
    int radiusLg    = 10;   // 内容卡片
    int radiusCard  = 12;   // 模块大卡
    int radiusPill  = 13;   // 胶囊（高度的一半左右）

    // ---- 控件尺寸 ----
    int rowH        = 32;   // 导航行 / 常规行高
    int inputH      = 32;   // 单行输入框视觉高
    int searchH     = 36;   // 搜索框视觉高（含 chip）
    int btnH        = 32;   // 主按钮高
    int tagH        = 20;   // 行内标签高
    int statusTagH  = 22;   // 状态标签高
    int itemH       = 64;   // 列表条目卡行高
    int groupH      = 30;   // 列表分组头行高

    // ---- 字体（负值 = 字符高度，见 Fonts.h） ----
    int fontBody    = 14;
    int fontMeta    = 12;
    int fontTitle   = 13;
    int fontSection = 14;
    int fontButton  = 14;
    int fontIcon    = 11;
    int fontIconLg  = 16;
    int fontDisplay = 22;

    // 按 DPI 等比缩放（保留 8px 栅格的最小值 1）
    Metrics ForDpi(int dpi) const;
};

// ------------------------------------------------------------
// 取用
// ------------------------------------------------------------
const Palette& Pal();
const Metrics& Mx();

void SetPalette(const Palette& p);
void SetMetrics(const Metrics& m);

// 预置配色
Palette LightPalette();   // 默认：企业蓝 #1677FF
Palette DarkPalette();    // 深色（预留）

} // namespace ui
} // namespace air
