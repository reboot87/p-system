#pragma once
// ============================================================
// ConfigPage — 机器配置
// 服务与进程 / 操作系统 / 处理器与内存 / 显示 / 磁盘 / 网络 /
// 快捷键 / 功能模块，双栏瀑布式卡片
// ============================================================
#include "airui/AirUi.h"
#include "airui/Interaction.h"
#include "../core/SysInfo.h"

namespace arecord { class App; }
using namespace arecord;

class ConfigPage : public air::ui::Page
{
public:
    explicit ConfigPage(App& app) : m_app(app) {}

    const wchar_t* Title() const override    { return L"机器配置"; }
    const wchar_t* NavGlyph() const override { return L"\xE713"; }
    const wchar_t* NavLabel() const override { return L"机器配置"; }

    void OnShow(bool shown) override;
    void OnLayout(const RECT& content) override;
    void OnPaint(air::ui::Canvas& g, const RECT& content) override;
    bool OnTimer(UINT_PTR id) override;
    bool OnMouseDown(POINT pt) override;
    bool OnMouseUp(POINT pt) override;
    bool OnMouseMove(POINT pt) override;
    bool OnMouseLeave() override;
    bool HandCursorAt(POINT pt) override;

private:
    // 输入回调收到的是**内容区相对坐标**（Shell 已减去 content 原点），
    // 而卡片/热区几何是客户区绝对坐标——命中前统一换算（同 ClipsPage）。
    RECT        m_content = {};
    struct CardData
    {
        const wchar_t* glyph;
        const wchar_t* title;
        std::wstring   badge;
        bool           badgeOk;
        std::vector<std::wstring> rows;   // "k\tv\tbar"，bar < 0 表示无进度条
    };

    static int CardHeight(int rowCount) { return 44 + rowCount * 26 + 12; }
    void DrawInfoCard(air::ui::Canvas& g, RECT rc, const CardData& c);
    void DrawDarkToggle(air::ui::Canvas& g);
    bool HitDarkToggle(POINT pt) const;

    App&        m_app;
    SysInfoData m_sys;
    bool        m_sysLoaded = false;

    // 「暗色模式」开关：Canvas 自绘 + 热区（无 HWND，悬停/按下态自管）
    RECT        m_rcDarkToggle = {};
    RECT        m_rcThemeCard  = {};   // 「显示与外观」卡矩形（OnPaint 内算）
    air::ui::HotState m_darkHot;
    static constexpr int kHotDarkToggle = 200;
};
