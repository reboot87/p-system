// ============================================================
// DemoWindow.cpp — AirUI 复用示例
//
// 一个成品外观的应用窗口：无边框 + DWM 圆角阴影 + 自绘标题栏
// （应用名/状态胶囊/三键）+ 侧栏导航 + 底部状态栏 + 页面框架。
//
// 页面代码全程不接触 HDC / HFONT / HWND / DRAWITEMSTRUCT：
//   · 绘制   → Canvas
//   · 控件   → CreateTextBox / CreateButton（回调驱动）
//   · 弹窗   → ShowMessageBox
//
// 编译：build.bat 的最后一步（产出 build\airui_demo.exe）
// ============================================================
#include <windows.h>
#include "airui/AirUi.h"

using namespace air::ui;

// ------------------------------------------------------------
// 页面一：只有绘制 —— Canvas 让页面只描述「画什么」
// ------------------------------------------------------------
class HelloPage : public Page
{
public:
    const wchar_t* Title() const override    { return L"示例页"; }
    const wchar_t* NavGlyph() const override { return L"\xE896"; }  // LightningBolt
    const wchar_t* NavLabel() const override { return L"示例页"; }

    void OnPaint(Canvas& g, const RECT& content) override
    {
        const Palette& p = Pal();
        const Metrics& m = Mx();

        // 内容卡片
        RECT card = { content.left, content.top,
                      content.right, content.top + 220 };
        g.FillRound(card, p.surface, m.radiusLg);
        g.Stroke(card, p.split, m.radiusLg);

        g.Text(RECT{ card.left, card.top + 40, card.right, card.top + 90 },
               L"Hello, AirUI!", FontRole::Display, p.primary,
               DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        g.Text(RECT{ card.left, card.top + 100, card.right, card.top + 130 },
               L"这个窗口由 airui 静态库直接提供，页面只需继承 Page。",
               FontRole::Body, p.textSub,
               DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);

        // 主按钮外观（纯绘制示例；点击在 OnMouseDown 命中）
        RECT btn = { card.left + 60, card.top + 150,
                     card.left + 180, card.top + 182 };
        g.Button(btn, L"主按钮", BtnStyle::Primary, m_btnHover);
    }

    bool OnMouseDown(POINT pt) override
    {
        POINT abs = { pt.x + m_content.left, pt.y + m_content.top };
        RECT btn = { m_content.left + 60, m_content.top + 150,
                     m_content.left + 180, m_content.top + 182 };
        if (PtInRect(&btn, abs)) {
            ShowMessageBox(L"页面代码里没有任何 Win32 句柄。",
                           L"AirUI Demo");
            return true;
        }
        return false;
    }

    bool OnMouseMove(POINT pt) override
    {
        POINT abs = { pt.x + m_content.left, pt.y + m_content.top };
        RECT btn = { m_content.left + 60, m_content.top + 150,
                     m_content.left + 180, m_content.top + 182 };
        bool hover = PtInRect(&btn, abs) != 0;
        if (hover == m_btnHover) return false;
        m_btnHover = hover;
        return true;
    }

    bool OnMouseLeave() override
    {
        if (!m_btnHover) return false;
        m_btnHover = false;
        return true;
    }

private:
    RECT m_content = {};
    bool m_btnHover = false;
};

// ------------------------------------------------------------
// 页面二：控件 —— TextBox / Button 回调驱动，无 HWND
// ------------------------------------------------------------
class InputPage : public Page
{
public:
    const wchar_t* Title() const override    { return L"输入演示"; }
    const wchar_t* NavGlyph() const override { return L"\xE70F"; }  // Edit
    const wchar_t* NavLabel() const override { return L"输入演示"; }

    void OnAttach(Shell& shell) override
    {
        (void)shell;
        m_edit = &CreateTextBox({ 2001, false, L"随便输入点什么…",
                                  FontRole::Body });
        m_edit->OnKey([this](UINT vk, bool, bool) -> bool {
            if (vk == VK_RETURN) { Greet(); return true; }
            return false;
        });
        m_btn = &CreateButton({ 2002, L"打招呼", BtnStyle::Primary });
        m_btn->OnClick([this] { Greet(); });
    }

    void OnLayout(const RECT& content) override
    {
        m_content = content;
        m_box = { content.left, content.top, content.left + 320,
                  content.top + Mx().inputH };
        // 单行 EDIT 压到一行文字高、垂直居中 → 文字真正上下居中
        m_edit->Move(m_box.left + 1, m_box.top + 4, 318,
                     Fonts().BodyLineHeight());
        m_btn->Move(content.left + 340, content.top, 96, Mx().inputH);
    }

    void OnPaint(Canvas& g, const RECT&) override
    {
        const Palette& p = Pal();
        if (m_box.right > m_box.left) {
            g.FillRound(m_box, p.surface, Mx().radiusMd);
            g.Stroke(m_box, p.border, Mx().radiusMd);
        }
    }

    bool OnMouseDown(POINT pt) override
    {
        POINT abs = { pt.x + m_content.left, pt.y + m_content.top };
        if (PtInRect(&m_box, abs)) {
            m_edit->ClickAt(abs);
            return true;
        }
        return false;
    }

    void OnShow(bool shown) override
    {
        if (m_edit) m_edit->Show(shown);
        if (m_btn)  m_btn->Show(shown);
    }

    void Greet()
    {
        std::wstring name = m_edit->Text();
        ShowMessageBox(name.empty() ? L"你好，陌生人！"
                                    : L"你好，" + name + L"！",
                       L"AirUI Demo");
    }

private:
    TextBox* m_edit = nullptr;
    Button*  m_btn  = nullptr;
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
