// ============================================================
// AirUI - ShellCore.cpp
// Platform-free shell state for Caption / NavBar / StatusBar.
// ============================================================
#include "airui/ShellCore.h"

namespace air {
namespace ui {

void ShellCore::SetNavGroups(std::vector<ShellNavGroup> groups)
{
    m_groups = std::move(groups);
    m_navItemRects.clear();
    m_navItemIds.clear();
}

void ShellCore::SetActiveNav(int id)
{
    m_activeNav = id;
}

void ShellCore::SetNavBadge(int id, const std::wstring& text, bool ok)
{
    for (auto& g : m_groups)
    {
        for (auto& it : g.items)
        {
            if (it.id == id)
            {
                it.badge = text;
                it.badgeOk = ok;
                return;
            }
        }
    }
}

void ShellCore::Layout(const Rect& client)
{
    const int captionH = 36;
    const int statusH  = 24;
    const int navW     = 220;

    m_captionRect = { client.left, client.top, client.right, client.top + captionH };
    m_navRect     = { client.left, m_captionRect.bottom, client.left + navW, client.bottom - statusH };
    m_statusRect  = { client.left, client.bottom - statusH, client.right, client.bottom };
    m_contentRect = { m_navRect.right, m_captionRect.bottom, client.right, client.bottom - statusH };

    LayoutNav(Mx());
}

void ShellCore::LayoutNav(const Metrics& mx)
{
    m_navItemRects.clear();
    m_navItemIds.clear();
    if (m_navRect.Empty()) return;

    const int pad   = mx.unit * 2;
    const int itemH = mx.rowH;
    int y = m_navRect.top + pad;
    for (const auto& g : m_groups)
    {
        if (!g.title.empty()) y += 18 + mx.unit;
        for (const auto& it : g.items)
        {
            m_navItemRects.push_back(
                Rect{ m_navRect.left + pad, y, m_navRect.right - mx.unit, y + itemH });
            m_navItemIds.push_back(it.id);
            y += itemH + mx.unit;
        }
        y += mx.unit * 2;
    }
}

int ShellCore::HitTestNav(const Point& p) const
{
    for (size_t i = 0; i < m_navItemRects.size(); ++i)
        if (m_navItemRects[i].Contains(p)) return m_navItemIds[i];
    return -1;
}

void ShellCore::Paint(IPainter& painter, const Palette& pal, const Metrics& mx)
{
    PaintCaption(painter, pal, mx);
    PaintNav(painter, pal, mx);
    PaintStatus(painter, pal, mx);
}

void ShellCore::PaintCaption(IPainter& painter, const Palette& pal, const Metrics& mx)
{
    if (m_captionRect.Empty()) return;
    painter.FillRect(m_captionRect, pal.surface);
    painter.StrokeRect(m_captionRect, pal.border, 1);

    const int pad = mx.unit * 2;
    Rect titleRc = m_captionRect;
    titleRc.left += pad;
    painter.Text(titleRc, m_caption.title, FontRole::Title, pal.text, 0);

    if (!m_caption.subtitle.empty())
    {
        Rect subRc = m_captionRect;
        subRc.left += pad;
        subRc.top  += 18;
        painter.Text(subRc, m_caption.subtitle, FontRole::Meta, pal.textSub, 0);
    }

    if (!m_caption.badge.empty())
    {
        Rect badgeRc = m_captionRect;
        badgeRc.right -= pad;
        badgeRc.left  = badgeRc.right - 120;
        painter.Text(badgeRc, m_caption.badge, FontRole::Meta, m_caption.badgeOk ? pal.ok : pal.error, 0);
    }
}

void ShellCore::PaintStatus(IPainter& painter, const Palette& pal, const Metrics& mx)
{
    if (m_statusRect.Empty()) return;
    painter.FillRect(m_statusRect, pal.sunken);
    painter.StrokeRect(m_statusRect, pal.border, 1);

    const int pad = mx.unit * 2;
    Rect txtRc = m_statusRect;
    txtRc.left += pad;
    painter.Text(txtRc, m_status.text, FontRole::Meta, m_status.ok ? pal.text : pal.error, 0);

    if (!m_status.extra.empty())
    {
        Rect extRc = m_statusRect;
        extRc.right -= pad;
        extRc.left  = extRc.right - 200;
        painter.Text(extRc, m_status.extra, FontRole::Meta, pal.textSub, 0);
    }
}

void ShellCore::PaintNav(IPainter& painter, const Palette& pal, const Metrics& mx)
{
    if (m_navRect.Empty()) return;
    if (m_navItemRects.empty() && !m_groups.empty()) LayoutNav(mx);

    painter.FillRect(m_navRect, pal.sidebar);
    painter.StrokeRect(m_navRect, pal.border, 1);

    const int pad = mx.unit * 2;
    const int itemH = mx.rowH;
    int y = m_navRect.top + pad;
    size_t idx = 0;
    for (const auto& g : m_groups)
    {
        if (!g.title.empty())
        {
            Rect grc{ m_navRect.left + mx.unit, y, m_navRect.right - mx.unit, y + 18 };
            painter.Text(grc, g.title, FontRole::Meta, pal.textSub, 0);
            y += 18 + mx.unit;
        }
        for (const auto& it : g.items)
        {
            Rect irc = (idx < m_navItemRects.size())
                     ? m_navItemRects[idx]
                     : Rect{ m_navRect.left + pad, y, m_navRect.right - mx.unit, y + itemH };
            ++idx;

            const bool active = (it.id == m_activeNav);
            const bool hover  = (it.id == m_hoverNav && !active);
            if (active || hover)
            {
                Color bg = active ? pal.primarySoft : pal.hover;
                painter.FillRect(irc, bg);
            }
            if (active)                       // 选中项左侧主色强调条
            {
                Rect bar{ irc.left, irc.top + 6, irc.left + 3, irc.bottom - 6 };
                painter.FillRect(bar, pal.primary);
            }

            Color fg = it.disabled ? pal.textDisabled
                     : active ? pal.primaryText : pal.text;
            Rect trc = irc;
            trc.left += mx.unit;
            painter.Text(trc, it.label, FontRole::Body, fg, 0);

            if (!it.badge.empty())
            {
                Rect brc = irc;
                brc.left = brc.right - 80;
                painter.Text(brc, it.badge, FontRole::Meta, it.badgeOk ? pal.ok : pal.error, 0);
            }
            y += itemH + mx.unit;
        }
        y += mx.unit * 2;
    }
}

} // namespace ui
} // namespace air
