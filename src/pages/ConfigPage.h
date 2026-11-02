#pragma once
// ============================================================
// ConfigPage — 机器配置
// 服务与进程 / 操作系统 / 处理器与内存 / 显示 / 磁盘 / 网络 /
// 快捷键 / 功能模块，双栏瀑布式卡片
// ============================================================
#include "airui/AirUi.h"
#include "../core/SysInfo.h"

class App;

class ConfigPage : public air::ui::IPage
{
public:
    explicit ConfigPage(App& app) : m_app(app) {}

    const wchar_t* Title() const override    { return L"机器配置"; }
    const wchar_t* NavGlyph() const override { return L"\xE713"; }
    const wchar_t* NavLabel() const override { return L"机器配置"; }

    void OnAttach(air::ui::Shell& shell) override;
    void OnShow(bool shown) override;
    void OnLayout(const RECT& content) override;
    void OnPaint(HDC dc, const RECT& content) override;
    bool OnTimer(UINT_PTR id) override;

private:
    const air::ui::Fonts& F() const { return m_shell->UiFonts(); }

    struct CardData
    {
        const wchar_t* glyph;
        const wchar_t* title;
        std::wstring   badge;
        bool           badgeOk;
        std::vector<std::wstring> rows;   // "k\tv\tbar"，bar < 0 表示无进度条
    };

    static int CardHeight(int rowCount) { return 44 + rowCount * 26 + 12; }
    void DrawInfoCard(HDC dc, RECT rc, const CardData& c);

    App&        m_app;
    SysInfoData m_sys;
    bool        m_sysLoaded = false;
};
