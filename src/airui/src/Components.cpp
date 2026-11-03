#include "airui/Components.h"
#include <algorithm>

namespace air {
namespace ui {

// ------------------------------------------------------------
// Card 实现
// ------------------------------------------------------------
void Card::Paint(Canvas& g)
{
    const auto& p = Pal();
    const auto& m = Mx();
    const RECT& r = m_cfg.rect;

    // 背景
    if (m_hover && m_cfg.hoverable) {
        g.FillRound(r, p.hover, m.radiusCard);
        g.Stroke(r, p.hoverBorder, m.radiusCard);
    } else {
        g.FillRound(r, p.surface, m.radiusCard);
        g.Stroke(r, p.split, m.radiusCard);
    }

    // 图标块
    RECT ic = { r.left + 16, r.top + 16, r.left + 16 + m_cfg.icon.size, r.top + 16 + m_cfg.icon.size };
    g.IconTile(ic, m_cfg.icon.glyph.c_str(), FontRole::IconLarge,
               m_cfg.icon.bg ? m_cfg.icon.bg : p.primarySoft,
               m_cfg.icon.color ? m_cfg.icon.color : p.primary,
               m_cfg.icon.radius);

    // 右上角 Badge
    if (!m_cfg.badge.text.empty()) {
        g.PillRight(r.right - 16, r.top + 16 + m_cfg.icon.size / 2,
                    m_cfg.badge.text, FontRole::Meta,
                    m_cfg.badge.ok ? p.okSoft : RGB(239, 241, 244),
                    m_cfg.badge.ok ? p.okText : RGB(143, 149, 158),
                    m.tagH);
    }

    // 文本行高（不能拿 m.titleH —— 那是自绘标题栏高度，不是文字行高）
    const int titleLineH   = g.LineHeight(FontRole::Title);
    const int metaLineH    = g.LineHeight(FontRole::Meta);
    const int displayLineH = g.LineHeight(FontRole::Display);
    const int buttonLineH  = g.LineHeight(FontRole::Button);

    // 标题
    RECT rcTitle = { r.left + 16, ic.bottom + 8, r.right - 16,
                     ic.bottom + 8 + titleLineH };
    g.Text(rcTitle, m_cfg.title, FontRole::Title, m_cfg.disabled ? p.textFaint : p.text,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    // 描述
    RECT rcDesc = { r.left + 16, rcTitle.bottom + 4, r.right - 16,
                    rcTitle.bottom + 4 + metaLineH };
    g.Text(rcDesc, m_cfg.desc, FontRole::Meta, p.textFaint,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    // 大数字
    RECT rcNum = { r.left + 16, rcDesc.bottom + 8, r.right - 16,
                   rcDesc.bottom + 8 + displayLineH };
    g.Text(rcNum, m_cfg.number, FontRole::Display, m_cfg.disabled ? p.textFaint : p.text,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    // 单位
    if (!m_cfg.unit.empty()) {
        int numW = g.MeasureWidth(m_cfg.number, FontRole::Display);
        RECT rcUnit = { r.left + 16 + numW + 8, rcNum.top, r.right - 16, rcNum.bottom };
        g.Text(rcUnit, m_cfg.unit, FontRole::Meta, p.textFaint,
               DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    // 底部链接
    if (!m_cfg.linkText.empty()) {
        RECT rcLink = { r.left + 16, r.bottom - 16 - buttonLineH,
                        r.right - 16, r.bottom - 16 };
        g.Text(rcLink, m_cfg.linkText, FontRole::Button,
               m_cfg.disabled ? RGB(169, 174, 184) : p.primaryText,
               DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    }
}

} // namespace ui
} // namespace air