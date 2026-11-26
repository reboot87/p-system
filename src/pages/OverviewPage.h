#pragma once
// ============================================================
// OverviewPage — 工作台总览
// 模块卡片墙（剪贴板 / 文件监控 / 打印监控）+ 最近动态
// ============================================================
#include "airui/AirUi.h"

namespace arecord { class App; }
using namespace arecord;

class OverviewPage : public air::ui::Page
{
public:
    explicit OverviewPage(App& app) : m_app(app) {}

    const wchar_t* Title() const override    { return L"工作台总览"; }
    const wchar_t* NavGlyph() const override { return L"\xE80F"; }
    const wchar_t* NavLabel() const override { return L"工作台总览"; }

    void OnShow(bool shown) override;
    void OnLayout(const air::ui::Rect& content) override;
    void OnPaint(air::ui::Canvas& g, const air::ui::Rect& content) override;
    bool OnMouseDown(air::ui::Point pt) override;
    bool OnMouseMove(air::ui::Point pt) override;
    bool OnMouseLeave() override;
    bool HandCursorAt(air::ui::Point pt) override;

private:
    void DrawModuleCard(air::ui::Canvas& g, air::ui::Rect r, int idx,
                        const wchar_t* glyph,
                        air::ui::Color icBg, air::ui::Color icTx,
                        const wchar_t* name, const wchar_t* desc,
                        const std::wstring& number, const wchar_t* unit,
                        const wchar_t* badge, bool off, bool badgeOk,
                        bool hover);
    void DrawOfflineBanner(air::ui::Canvas& g, const air::ui::Rect& banner);

    App&  m_app;
    air::ui::Rect  m_content = {};
    air::ui::Rect  m_rcModCard[3] = {};
    int   m_hoverCard = -1;
    int   m_bannerH = 0;      // 服务离线横幅高度（离线 ~28px，在线 0）
};
