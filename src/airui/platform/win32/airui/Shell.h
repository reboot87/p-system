#pragma once
// ============================================================
// AirUI — Shell.h
// 无边框窗口外壳 + 页面框架
// ============================================================
#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "airui/Theme.h"
#include "airui/Fonts.h"
#include "airui/Controls.h"
#include "airui/Page.h"

// 前向声明
namespace air { namespace ui { 
    class ResponsiveManager; 
    struct Breakpoint; 
    class ListBox;
    class TextBox;
    class Button;
    class Custom;
} }

// ------------------------------------------------------------
// ToastManager —— 轻量级消息提示（定义在 Shell.h 避免循环依赖）
// ------------------------------------------------------------
namespace air {
namespace ui {

class ToastManager
{
public:
    struct Toast {
        std::wstring text;
        bool ok = true;
        int durationMs = 3000;
        std::function<void()> onDismiss;
    };

    void Show(const Toast& toast);
    void Paint(Canvas& g, const RECT& viewport);
    void Tick();

private:
    struct InternalToast : Toast {
        uint64_t startTime = 0;
    };
    std::vector<InternalToast> m_toasts;
    std::function<void()> m_invalidateCb;
    void InvalidateAll() { if (m_invalidateCb) m_invalidateCb(); }
    friend class Shell;
};

} // namespace ui
} // namespace air

namespace air {
namespace ui {

struct ShellOptions
{
    std::wstring className   = L"AirUiShellWnd";
    std::wstring appName     = L"App";
    std::wstring logoLetter;
    int          width       = 1120;
    int          height      = 780;
    int          minWidth    = 780;
    int          minHeight   = 560;
    bool         sidebar     = true;
    int          sidebarW    = 208;
    bool         statusBar   = true;
    // 创建后是否立刻显示。false = 静默启动（热键/托盘唤出前不显示窗口）
    bool         showOnCreate= true;
    bool         dark        = false;
    int          dpi         = 0;
    HICON        icon        = nullptr;
};

class Shell
{
public:
    Shell();
    virtual ~Shell();

    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;

    bool Create(HINSTANCE hInstance, const ShellOptions& options);
    int  RunMessageLoop();
    HWND Hwnd() const { return m_hwnd; }

    void    AddPage(int id, std::unique_ptr<IPage> page);
    bool    SwitchPage(int id);
    int     CurrentPageId() const { return m_pageId; }
    IPage*  CurrentPage() const;
    IPage*  FindPage(int id) const;
    size_t  PageCount() const { return m_pages.size(); }

    void AddNavCommand(int id, const std::wstring& glyph,
                       const std::wstring& label, bool disabled = false);
    void SetNavGroups(std::vector<NavGroup> groups);
    void SetNavBadge(int id, const std::wstring& text, bool ok = false);
    void SetNavCommandHandler(std::function<void(int)> handler);

    void SetCaptionBadge(const std::wstring& text, bool ok);
    void SetCaptionSubtitle(const std::wstring& text);

    void SetStatus(const std::wstring& text, bool ok);
    void SetStatusExtra(const std::wstring& text);
    void SetStatusKeys(std::vector<std::pair<std::wstring, std::wstring>> keys);
    void SetFooterText(const std::wstring& text);

    struct InfoRow { std::wstring key; std::wstring value; bool ok = false; };
    void SetSidebarInfo(const std::wstring& title,
                        const std::vector<InfoRow>& rows);

    void Invalidate();
    void InvalidateContent();
    void InvalidateTitle();
    void InvalidateRegion(const RECT& rcClient);
    void SetRedraw(bool on);

    int ShowMessageBox(const std::wstring& text, const std::wstring& caption,
                       UINT flags = MB_OK);
    int ShowPopupMenu(POINT screenPt, const std::vector<MenuItem>& items);

    int MeasureTextWidth(const std::wstring& s, FontRole role);
    int TextLineHeight(FontRole role);

    void SetSidebarWidth(int width);
    int  SidebarWidth() const { return m_opt.sidebarW; }

    void SetBreakpoints(std::vector<Breakpoint> bps);
    void OnSize(int width, int height);
    void Tick(uint64_t now);

    ToastManager& ToastManager();

    const ui::Fonts& UiFonts() const { return m_fonts; }
    const RECT&  ContentRect() const { return m_content; }
    const RECT&  SidebarRect() const { return m_side; }

    void SetMessageHook(
        std::function<bool(HWND, UINT, WPARAM, LPARAM, LRESULT&)> hook);

    virtual bool OnShellMessage(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam, LRESULT& result)
    { (void)hwnd; (void)msg; (void)wParam; (void)lParam; (void)result; return false; }

    void SetCloseGuard(std::function<bool()> guard);

protected:
    virtual void OnPaintCaptionExtra(HDC dc, const RECT& bar) { (void)dc; (void)bar; }

    friend class Page;
    void    RegisterWidget(Widget* w);
    Widget* WidgetAt(int id);

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);

    bool RegisterWindowClass(HINSTANCE);
    void CreateNavItems();
    void LayoutAll();
    void Paint(HDC dc);

    void DrawCaption(HDC dc, const RECT& rc);
    void DrawStatusBar(HDC dc, const RECT& rc);
    void DrawSidebarFooter(HDC dc);

    int  CaptionButtonAt(POINT clientPt) const;

    struct PageSlot
    {
        int                   id = 0;
        std::unique_ptr<IPage> page;
    };

    HINSTANCE     m_hinst = nullptr;
    HWND          m_hwnd  = nullptr;
    ShellOptions  m_opt;
    Fonts         m_fonts;
    NavBar        m_nav;

    std::vector<PageSlot> m_pages;
    std::vector<NavGroup> m_navGroups;
    std::vector<Widget*>  m_widgets;
    int           m_pageId = -1;

    RECT          m_content = {};
    RECT          m_side    = {};
    RECT          m_status  = {};
    RECT          m_caption = {};

    int           m_dpi     = 96;
    int           m_capHover = 0;
    bool          m_active  = true;

    std::wstring  m_captionBadge;
    bool          m_captionBadgeOk = true;
    std::wstring  m_captionSubtitle;

    std::wstring  m_statusText = L"就绪";
    std::wstring  m_statusExtra;
    bool          m_statusOk   = true;
    std::vector<std::pair<std::wstring, std::wstring>> m_statusKeys;
    std::wstring  m_footerText;

    std::wstring  m_infoTitle;
    std::vector<InfoRow> m_infoRows;

    std::function<bool(HWND, UINT, WPARAM, LPARAM, LRESULT&)> m_hook;
    std::function<void(int)> m_navHandler;
    std::function<bool()>    m_closeGuard;

    std::unique_ptr<ResponsiveManager> m_responsive;
    std::unique_ptr< ::air::ui::ToastManager> m_toastMgr;
};

} // namespace ui
} // namespace air