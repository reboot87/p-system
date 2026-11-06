#include "airui/Components.h"
#include "airui/CanvasPainter.h"
#include "airui/ControlsExtra.h"
#include <algorithm>

namespace air {
namespace ui {

// ------------------------------------------------------------
// Card 实现 —— 绘制逻辑已迁入 core 的 PaintCard。
// 平台层只负责把旧 Config 翻译成 CardState，再转发给 IPainter；
// 这样 Skia / X11 后端可以用同一份几何与配色画出完全一致的卡片。
// ------------------------------------------------------------
void Card::Paint(Canvas& g)
{
    CardState st;
    st.base.disabled = m_cfg.disabled;
    st.base.hover    = m_hover;
    st.hoverable     = m_cfg.hoverable;
    st.glyph         = m_cfg.icon.glyph;
    st.title         = m_cfg.title;
    st.desc          = m_cfg.desc;
    st.number        = m_cfg.number;
    st.unit          = m_cfg.unit;
    st.badge         = m_cfg.badge.text;
    st.badgeOk       = m_cfg.badge.ok;
    st.linkText      = m_cfg.linkText;
    st.iconSize      = m_cfg.icon.size;
    st.iconRadius    = m_cfg.icon.radius;
    st.iconBg        = m_cfg.icon.bg;
    st.iconColor     = m_cfg.icon.color;

    CanvasPainter painter(g);
    PaintCard(painter, ToRect(m_cfg.rect), st, Pal(), Mx());
}

} // namespace ui
} // namespace air
