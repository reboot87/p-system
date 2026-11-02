#pragma once
// ============================================================
// AirUI — Shell.h
// 无边框窗口外壳 + 页面框架
//
// 一行接入：
//     air::ui::Shell shell;
//     shell.Create(hInstance, { .appName = L"MyApp", ... });
//     shell.AddPage(1, std::make_unique<MyPage>());
//     shell.SwitchPage(1);
//     return shell.RunMessageLoop();
//
// 外壳负责：DWM 阴影/圆角、去 NC 区、标题栏自绘（应用名 + 副标题 +
// 状态胶囊 + 三键）、拖动/双击最大化/吸附、最小尺寸、侧栏导航、
// 状态栏、双缓冲绘制。业务只管页面内容。
// ============================================================
#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "Theme.h"
#include "Fonts.h"
#include "Controls.h"
#include "Page.h"

namespace air {
namespace ui {

struct ShellOptions
{
    std::wstring className   = L"AirUiShellWnd";
    std::wstring appName     = L"App";        // 标题栏左侧应用名
    std::wstring logoLetter;                   // logo 内单字（空 = 画默认图形）
    int          width       = 1120;
    int          height      = 780;
    int          minWidth    = 780;
    int          minHeight   = 560;
    bool         sidebar     = true;
    bool         statusBar   = true;
    bool         dark        = false;
    int          dpi         = 0;              // 0 = 跟随系统
    HICON        icon        = nullptr;        // 空 = 使用应用图标
};

class Shell
{
public:
    Shell();
    virtual ~Shell();

    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;

    // ---- 创建与运行 ----
    bool Create(HINSTANCE hInstance, const ShellOptions& options);
    int  RunMessageLoop();
    HWND Hwnd() const { return m_hwnd; }

    // ---- 页面 ----
    void    AddPage(int id, std::unique_ptr<IPage> page);
    bool    SwitchPage(int id);
    int     CurrentPageId() const { return m_pageId; }
    IPage*  CurrentPage() const;
    IPage*  FindPage(int id) const;
    size_t  PageCount() const { return m_pages.size(); }

    // 侧栏里额外的命令项（不切页，触发 SetNavCommandHandler）
    void AddNavCommand(int id, const std::wstring& glyph,
                       const std::wstring& label, bool disabled = false);
    // 完全自定义侧栏结构（分组 / 禁用项 / 顺序）
    void SetNavGroups(std::vector<NavGroup> groups);
    void SetNavBadge(int id, const std::wstring& text, bool ok = false);
    void SetNavCommandHandler(std::function<void(int)> handler);

    // ---- 标题栏 ----
    // 右侧状态胶囊（如「服务运行中 · 已记录 128 条」）
    void SetCaptionBadge(const std::wstring& text, bool ok);
    void SetCaptionSubtitle(const std::wstring& text);   // 覆盖当前页 Title()

    // ---- 状态栏 ----
    void SetStatus(const std::wstring& text, bool ok);
    void SetStatusExtra(const std::wstring& text);       // 左侧副信息
    void SetStatusKeys(std::vector<std::pair<std::wstring, std::wstring>> keys);
    void SetFooterText(const std::wstring& text);        // 侧栏底部版本行

    // ---- 侧栏底部信息卡 ----
    struct InfoRow { std::wstring key; std::wstring value; bool ok = false; };
    void SetSidebarInfo(const std::wstring& title,
                        const std::vector<InfoRow>& rows);

    // ---- 刷新 ----
    void Invalidate();          // 整窗
    void InvalidateContent();   // 仅内容区
    void InvalidateTitle();     // 仅标题栏
    void InvalidateRegion(const RECT& rcClient);  // 客户区局部

    // ---- 批量绘制（WM_SETREDRAW 封装） ----
    void SetRedraw(bool on);

    // ---- 对话框 / 菜单（父窗口由 Shell 承担） ----
    int ShowMessageBox(const std::wstring& text, const std::wstring& caption,
                       UINT flags = MB_OK);
    // 返回被点击的命令 id；0 = 未选择
    int ShowPopupMenu(POINT screenPt, const std::vector<MenuItem>& items);

    // ---- 排版期度量（无 DC 环境，如布局回调里） ----
    int MeasureTextWidth(const std::wstring& s, FontRole role);
    int TextLineHeight(FontRole role);

    // 注意：成员函数不能叫 Fonts()，否则会遮蔽类型名 Fonts
    const ui::Fonts& UiFonts() const { return m_fonts; }
    const RECT&  ContentRect() const { return m_content; }
    const RECT&  SidebarRect() const { return m_side; }

    // 消息钩子：返回 true 表示已处理，Shell 不再继续分发。
    // 插件、全局快捷键、自定义托盘消息都可挂在这里。
    void SetMessageHook(
        std::function<bool(HWND, UINT, WPARAM, LPARAM, LRESULT&)> hook);

    // 子类可介入消息（返回 true 表示已处理）
    virtual bool OnShellMessage(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam, LRESULT& result)
    { (void)hwnd; (void)msg; (void)wParam; (void)lParam; (void)result; return false; }

    // 关闭前确认（返回 false 可阻止关闭）
    void SetCloseGuard(std::function<bool()> guard);

protected:
    // 供派生类画自定义标题栏元素（在标题右侧、状态胶囊左侧）
    virtual void OnPaintCaptionExtra(HDC dc, const RECT& bar) { (void)dc; (void)bar; }

    // ---- 控件注册表（Page 创建控件时登记，消息由此路由） ----
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

    // ---- 数据 ----
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
    std::vector<NavGroup> m_navGroups;   // 侧栏结构（单一数据源）
    std::vector<Widget*>  m_widgets;     // 控件注册表（页面拥有）
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
};

} // namespace ui
} // namespace air
