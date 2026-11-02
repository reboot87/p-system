#pragma once
// ============================================================
// OverviewPage — 工作台总览
// 模块卡片墙（剪贴板 / 文件监控 / 打印监控）+ 最近动态
// ============================================================
#include "airui/AirUi.h"

class App;

class OverviewPage : public air::ui::Page
{
public:
    explicit OverviewPage(App& app) : m_app(app) {}

    const wchar_t* Title() const override    { return L"工作台总览"; }
    const wchar_t* NavGlyph() const override { return L"\xE80F"; }
    const wchar_t* NavLabel() const override { return L"工作台总览"; }

    void OnShow(bool shown) override;
    void OnLayout(const RECT& content) override;
    void OnPaint(air::ui::Canvas& g, const RECT& content) override;
    bool OnMouseDown(POINT pt) override;
    bool OnMouseMove(POINT pt) override;
    bool OnMouseLeave() override;

private:
    void DrawModuleCard(air::ui::Canvas& g, RECT r, int idx,
                        const wchar_t* glyph,
                        COLORREF icBg, COLORREF icTx,
                        const wchar_t* name, const wchar_t* desc,
                        const std::wstring& number, const wchar_t* unit,
                        const wchar_t* badge, bool off, bool badgeOk);

    App&  m_app;
    RECT  m_content = {};
    RECT  m_rcModCard[3] = {};
    int   m_hoverCard = -1;
};
