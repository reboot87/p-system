#include "OverviewPage.h"
#include "core/App.h"
#include "core/Format.h"
#include "core/UiEntry.h"

using namespace air::ui;

void OverviewPage::OnShow(bool shown)
{
    if (shown) m_app.RefreshStats();
}

void OverviewPage::OnLayout(const RECT& content)
{
    m_content = content;

    int x0 = content.left;
    int x1 = content.right;
    int W  = x1 - x0;
    int cardW = (W - 24) / 3;
    int cardH = 188;
    int y = content.top;

    m_rcModCard[0] = { x0, y, x0 + cardW, y + cardH };
    m_rcModCard[1] = { x0 + cardW + 12, y, x0 + cardW * 2 + 12, y + cardH };
    m_rcModCard[2] = { x0 + cardW * 2 + 24, y, x1, y + cardH };
}

void OverviewPage::DrawModuleCard(HDC dc, RECT r, int idx,
                                  const wchar_t* glyph,
                                  COLORREF icBg, COLORREF icTx,
                                  const wchar_t* name, const wchar_t* desc,
                                  const std::wstring& number, const wchar_t* unit,
                                  const wchar_t* badge, bool off, bool badgeOk)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();
    m_rcModCard[idx] = r;

    gfx::FillRound(dc, r, p.surface, m.radiusCard);
    gfx::Stroke(dc, r, p.split, m.radiusCard);

    // 图标块 40px
    RECT ic = { r.left + 16, r.top + 16, r.left + 56, r.top + 56 };
    gfx::IconTile(dc, ic, glyph, F().Get(FontRole::IconLarge), icBg, icTx, 10);

    // 右上角状态
    gfx::PillRight(dc, r.right - 16, r.top + 16, badge,
                   F().Get(FontRole::Meta),
                   badgeOk ? p.okSoft : RGB(239, 241, 244),
                   badgeOk ? p.okText : RGB(143, 149, 158));

    // 名称 + 描述
    RECT rcName = { r.left + 16, r.top + 64, r.right - 16, r.top + 86 };
    gfx::Text(dc, F().Get(FontRole::Title), off ? p.textFaint : p.text,
              rcName, name, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    RECT rcDesc = { r.left + 16, r.top + 88, r.right - 16, r.top + 108 };
    gfx::Text(dc, F().Get(FontRole::Meta), p.textFaint, rcDesc, desc,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS |
              DT_NOPREFIX);

    // 大数字 + 单位
    RECT rcNum = { r.left + 16, r.top + 114, r.right - 16, r.top + 148 };
    gfx::Text(dc, F().Get(FontRole::Display), off ? p.textFaint : p.text,
              rcNum, number, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    int numW = gfx::MeasureWidth(dc, F().Get(FontRole::Display), number);
    RECT rcUnit = { r.left + 16 + numW + 8, r.top + 114, r.right - 16,
                    r.top + 148 };
    gfx::Text(dc, F().Get(FontRole::Meta), p.textFaint, rcUnit, unit,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS |
              DT_NOPREFIX);

    // 底部入口
    RECT rcLink = { r.left + 16, r.top + 154, r.right - 16, r.top + 176 };
    gfx::Text(dc, F().Get(FontRole::Button),
              off ? RGB(169, 174, 184) : p.primaryText, rcLink,
              off ? L"敬请期待" : L"进入模块 →",
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
}

void OverviewPage::OnPaint(HDC dc, const RECT& content)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();
    m_content = content;

    int x0 = content.left;
    int x1 = content.right;
    int W  = x1 - x0;
    int cardW = (W - 24) / 3;
    int cardH = 188;
    int y  = content.top;

    // 卡片矩形（绝对坐标，同时供命中测试使用）
    m_rcModCard[0] = { x0, y, x0 + cardW, y + cardH };
    m_rcModCard[1] = { x0 + cardW + 12, y, x0 + cardW * 2 + 12, y + cardH };
    m_rcModCard[2] = { x0 + cardW * 2 + 24, y, x1, y + cardH };

    const ServiceStats& st = m_app.Stats();

    std::wstring clipNum =
        st.total >= 0 ? std::to_wstring((long)st.total) : L"—";
    std::wstring clipUnit = st.today > 0
        ? L"条记录 · 今日 +" + std::to_wstring((long)st.today)
        : L"条记录";

    DrawModuleCard(dc, m_rcModCard[0], 0,
                   L"\xE8C8", p.primarySoft, p.primary,
                   L"剪贴板监控", L"自动记录剪贴板内容，支持搜索与备注",
                   clipNum, clipUnit.c_str(), L"运行中", false, true);

    DrawModuleCard(dc, m_rcModCard[1], 1,
                   L"\xE8B7", RGB(230, 244, 241), RGB(15, 163, 127),
                   L"文件监控", L"监控指定目录的新增 / 修改 / 删除",
                   L"—", L"未启用", L"规划中", true, false);

    DrawModuleCard(dc, m_rcModCard[2], 2,
                   L"\xE749", RGB(243, 238, 255), RGB(123, 97, 255),
                   L"打印监控", L"记录本机打印任务与文档信息",
                   L"—", L"未启用", L"规划中", true, false);

    // ---- 最近动态 ----
    int y2 = y + cardH + 16;
    RECT rcAct = { x0, y2, x1, content.bottom };
    gfx::FillRound(dc, rcAct, p.surface, m.radiusLg);
    gfx::Stroke(dc, rcAct, p.split, m.radiusLg);

    RECT rcHead = { x0 + 16, y2 + 12, x1 - 16, y2 + 32 };
    gfx::Text(dc, F().Get(FontRole::Section), p.text, rcHead,
              L"最近动态 · 全部模块",
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    const std::vector<UiEntry>& entries = m_app.Entries();
    int ry = y2 + 42;
    int rows = 0;
    for (size_t i = 0; i < entries.size() && rows < 7; ++i, ++rows) {
        const UiEntry& e = entries[i];
        int rowBottom = ry + 34;
        if (rowBottom > rcAct.bottom - 4) break;

        // 图标块 26px
        RECT ic = { x0 + 16, ry + 4, x0 + 42, ry + 30 };
        bool manual = (e.source == L"Manual");
        gfx::IconTile(dc, ic, manual ? L"\xE70F" : L"\xE8C8",
                      F().Get(FontRole::Icon), p.iconTile,
                      manual ? RGB(123, 97, 255) : p.primary, 6);

        // 内容
        RECT rcTxt = { x0 + 52, ry, x1 - 230, ry + 34 };
        gfx::TextRow(dc, F().Get(FontRole::Body), p.text, rcTxt,
                     gfx::OneLine(e.content));

        // 模块标签
        int px = gfx::Tag(dc, x1 - 220, ry + 7, L"剪贴板",
                          F().Get(FontRole::Meta),
                          p.primarySoft, p.primaryText, m.tagH);

        // 时间
        RECT rcTime = { px + 12, ry, x1 - 16, ry + 34 };
        gfx::Text(dc, F().Get(FontRole::Meta), RGB(169, 174, 184), rcTime,
                  fmt::RelTime(e.createdAt),
                  DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX);

        // 虚线分隔
        gfx::HLineDashed(dc, x0 + 16, x1 - 16, rowBottom, RGB(224, 228, 233));
        ry = rowBottom;
    }

    if (!rows) {
        RECT rcEmpty = { x0 + 16, ry + 8, x1 - 16, rcAct.bottom - 8 };
        gfx::Text(dc, F().Get(FontRole::Meta), p.textFaint, rcEmpty,
                  L"暂无记录 —— 复制任意文字将自动出现在这里",
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }
}

bool OverviewPage::OnMouseDown(POINT pt)
{
    // Shell 传的是相对内容区坐标；卡片矩形是客户区绝对坐标
    POINT abs = { pt.x + m_content.left, pt.y + m_content.top };
    if (PtInRect(&m_rcModCard[0], abs)) {
        m_shell->SwitchPage(1);
        return true;
    }
    return true;   // 总览页吞掉内容区点击，避免穿透到别处
}

bool OverviewPage::OnMouseMove(POINT pt)
{
    POINT abs = { pt.x + m_content.left, pt.y + m_content.top };
    int card = -1;
    for (int i = 0; i < 3; ++i)
        if (PtInRect(&m_rcModCard[i], abs)) { card = i; break; }
    if (card == m_hoverCard) return false;
    m_hoverCard = card;
    return true;
}

bool OverviewPage::OnMouseLeave()
{
    if (m_hoverCard == -1) return false;
    m_hoverCard = -1;
    return true;
}
