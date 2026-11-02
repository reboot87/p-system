// ============================================================
// DemoWindow.cpp — AirUI 复用示例
//
// 40 行代码得到一个成品外观的应用窗口：
// 无边框 + DWM 圆角阴影 + 自绘标题栏（应用名/状态胶囊/三键）+
// 侧栏导航 + 底部状态栏 + 页面框架。
//
// 编译：build.bat 的最后一步（产出 build\airui_demo.exe）
// ============================================================
#include <windows.h>
#include "airui/AirUi.h"

using namespace air::ui;

// ------------------------------------------------------------
// 一个最简单的页面：只有绘制
// ------------------------------------------------------------
class HelloPage : public IPage
{
public:
    const wchar_t* Title() const override    { return L"示例页"; }
    const wchar_t* NavGlyph() const override { return L"\xE896"; }  // LightningBolt
    const wchar_t* NavLabel() const override { return L"示例页"; }

    void OnPaint(HDC dc, const RECT& content) override
    {
        const Palette& p = Pal();
        const Metrics& m = Mx();

        // 内容卡片
        RECT card = { content.left, content.top,
                      content.right, content.top + 220 };
        gfx::FillRound(dc, card, p.surface, m.radiusLg);
        gfx::Stroke(dc, card, p.split, m.radiusLg);

        gfx::Text(dc, m_shell->UiFonts().Get(FontRole::Display), p.primary,
                  RECT{ card.left, card.top + 40, card.right, card.top + 90 },
                  L"Hello, AirUI!",
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        gfx::Text(dc, m_shell->UiFonts().Get(FontRole::Body), p.textSub,
                  RECT{ card.left, card.top + 100, card.right, card.top + 130 },
                  L"这个窗口由 airui 静态库直接提供，页面只需实现 IPage。",
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);

        // 主按钮外观（纯绘制示例）
        RECT btn = { card.left + 60, card.top + 150,
                     card.left + 180, card.top + 182 };
        gfx::FillRound(dc, btn, p.primary, m.radiusMd);
        gfx::Text(dc, m_shell->UiFonts().Get(FontRole::Button), p.textInverse,
                  btn, L"主按钮",
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }
};

// 带输入框的页面：演示占位提示 + 命中测试
class InputPage : public IPage
{
public:
    const wchar_t* Title() const override    { return L"输入演示"; }
    const wchar_t* NavGlyph() const override { return L"\xE70F"; }  // Edit
    const wchar_t* NavLabel() const override { return L"输入演示"; }

    void OnAttach(Shell& shell) override
    {
        m_edit = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0, shell.Hwnd(), (HMENU)2001, nullptr, nullptr);
        SendMessageW(m_edit, WM_SETFONT,
                     (WPARAM)shell.UiFonts().Body(), TRUE);
        SetEditMargins(m_edit);
        PlaceholderEdit::Attach(m_edit, L"随便输入点什么…",
                                shell.UiFonts().Body());
    }

    void OnLayout(const RECT& content) override
    {
        m_content = content;
        m_box = { content.left, content.top, content.left + 320,
                  content.top + Mx().inputH };
        // 单行 EDIT 压到一行文字高、垂直居中 → 文字真正上下居中
        MoveWindow(m_edit, m_box.left + 1, m_box.top + 4,
                   318, m_shell->UiFonts().BodyLineHeight(), TRUE);
    }

    void OnPaint(HDC dc, const RECT&) override
    {
        const Palette& p = Pal();
        if (m_box.right > m_box.left) {
            gfx::FillRound(dc, m_box, p.surface, Mx().radiusMd);
            gfx::Stroke(dc, m_box, p.border, Mx().radiusMd);
        }
    }

    bool OnMouseDown(POINT pt) override
    {
        POINT abs = { pt.x + m_content.left, pt.y + m_content.top };
        if (PtInRect(&m_box, abs)) {
            ForwardClickToEdit(m_edit, m_shell->UiFonts().Body(), abs);
            return true;
        }
        return false;
    }

    void OnShow(bool shown) override
    {
        if (m_edit) ShowWindow(m_edit, shown ? SW_SHOW : SW_HIDE);
    }

private:
    HWND m_edit = nullptr;
    RECT m_box = {}, m_content = {};
};

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    Shell shell;
    ShellOptions opt;
    opt.className   = L"AirUiDemoWnd";
    opt.appName     = L"AirUI Demo";
    opt.logoLetter  = L"D";          // logo 内单字（空 = 默认图形）
    opt.width       = 960;
    opt.height      = 640;

    if (!shell.Create(hInst, opt)) return 1;

    shell.AddPage(1, std::make_unique<HelloPage>());
    shell.AddPage(2, std::make_unique<InputPage>());

    shell.SetCaptionBadge(L"演示模式", true);
    shell.SetStatus(L"就绪", true);
    shell.SetStatusKeys({ { L"F1", L"帮助" } });
    shell.SetSidebarInfo(L"关于", {
        { L"框架", L"airui 静态库", false },
        { L"依赖", L"仅 Win32", false },
    });
    shell.SetFooterText(L"AirUI Demo v1.0");

    shell.SwitchPage(1);
    return shell.RunMessageLoop();
}
