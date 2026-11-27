#pragma once
// ============================================================
// AirUI — Shell.h
// 无边框窗口外壳 + 页面框架
// ============================================================
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <cstdint>
#include "airui/Theme.h"
#include "Fonts.h"
#include "airui/types.h"
#include "Controls.h"
#include "Page.h"
#include "airui/ShellCore.h"
#include "airui/PaintBackend.h"

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
// ToastManager —— 自绘消息提示（定义在 Shell.h 避免循环依赖）
//
// 卡片由 core 的 PaintToast 画在独立分层窗口上：主窗口会把子窗口区域裁掉，
// 右下角被子控件占着，画在父窗口里会被裁掉。
// 浮层自己带 50ms 定时器做淡入淡出 / 自动消失。
// ------------------------------------------------------------
namespace air {
namespace ui {

enum class ToastKind { Info, Success, Warning, Error };

class OverlayWindow;

class ToastManager
{
public:
    struct Toast {
        std::wstring title;
        std::wstring message;
        ToastKind kind = ToastKind::Success;
        int  durationMs = 3200;
        bool closable   = true;
        std::function<void()> onDismiss;
    };

    ~ToastManager();

    void Show(const Toast& toast);
    void Show(const std::wstring& title, const std::wstring& message,
              ToastKind kind = ToastKind::Success);
    void Tick();
    void CloseAll();
    void Reposition();

private:
    struct InternalToast : Toast {
        uint64_t startTime = 0;
    };

    Rect ComputeRect() const;
    Rect CardRect(int index) const;
    bool OwnerVisible() const;
    void AdvanceClock(uint64_t now, bool ownerVisible);
    void Resume();
    void EnsureLayer();
    void Paint(DeviceContextHandle dc, int width, int height);
    void HitTest(Point pt, int& index, bool& onClose) const;
    bool OnEvent(MessageId msg, MessageWParam wp, MessageLParam lp, MessageResult& res);

    std::vector<InternalToast> m_toasts;
    std::unique_ptr<OverlayWindow> m_layer;
    WindowHandle        m_owner  = nullptr;
    const Fonts* m_fonts = nullptr;
    int  m_hoverIndex  = -1;
    bool m_hoverClose  = false;
    int  m_lastOpacity = 0;
    // Toast 的寿命走「显示时钟」：只有主窗口可见时它才推进。窗口
    // 隐藏 / 最小化期间时钟冻结，恢复后无需逐条补偿 startTime，
    // 也就不会有负年龄、淡入被反复推回 0 或定时器误判过期。
    uint64_t m_visibleClock = 0;
    uint64_t m_lastTick     = 0;
    bool     m_wasVisible   = true;
    bool     m_suspended    = false;

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
    IconHandle        icon        = nullptr;
    // Phase 1 HAL route: let the platform-free ShellCore draw the chrome
    // through the native painter instead of the legacy rich renderer. Default off
    // so the existing caption (logo / three buttons) never regresses.
    bool         coreShell   = false;
    // Runtime painter backend for the ShellCore chrome: "gdi", "skia",
    // "auto" (read AIRUI_PAINT_BACKEND, else gdi), or any registered
    // provider id. Empty = "auto". Switching needs no rebuild: the compositing backend is
    // linked in when a SDK was present, otherwise loaded from
    // airui-skia.dll at runtime. Unavailable backends fall back to gdi.
    std::string  paintBackend;
    // Deprecated compatibility alias: when paintBackend is empty,
    // true resolves to "skia" (the old compile-time skiaChrome flag).
    bool         skiaChrome  = false;
};

class Shell
{
public:
    Shell();
    virtual ~Shell();

    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;

    bool Create(InstanceHandle hInstance, const ShellOptions& options);
    int  RunMessageLoop();
    WindowHandle Hwnd() const { return m_hwnd; }

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
    void InvalidateRegion(const Rect& rcClient);
    void SetRedraw(bool on);

    // Region the current msg-PAINT is repainting, in client coordinates.
    // Equals the whole client for a full repaint and for any backend that
    // composites the full client (the compositing backend); narrower for a partial the backend
    // repaint. A page may use it to skip work that would only be clipped.
    const Rect& PaintClip() const { return m_paintClip; }

    int ShowMessageBox(const std::wstring& text, const std::wstring& caption,
                       MessageBoxFlags flags = mb::Ok);
    int ShowPopupMenu(Point screenPt, const std::vector<MenuItem>& items);

    int MeasureTextWidth(const std::wstring& s, FontRole role);
    int TextLineHeight(FontRole role);

    void SetSidebarWidth(int width);
    int  SidebarWidth() const { return m_opt.sidebarW; }

    void SetBreakpoints(std::vector<Breakpoint> bps);
    void OnSize(int width, int height);
    void Tick(uint64_t now);

    ToastManager& ToastManager();

    const ui::Fonts& UiFonts() const { return m_fonts; }
    const Rect&  ContentRect() const { return m_content; }
    const Rect&  SidebarRect() const { return m_side; }

    // Runtime painter backend selection (no rebuild required).
    // SetPaintBackend() takes effect on the next painted frame.
    void        SetPaintBackend(const std::string& id);
    const std::string& PaintBackendChoice() const { return m_paintBackend; }
    std::vector<std::string> AvailablePaintBackends() const;

    // UI action behind the caption-bar "renderer" button: cycle to the
    // next *available* backend. The whole window (chrome, pages and
    // widgets) switches on the next frame; the chrome mode itself is
    // never changed, so the UI does not turn into a different layout.
    void TogglePaintBackend();
    bool CoreShellEnabled() const { return m_opt.coreShell; }

    // Observability hook: (selected backend id, backend owns text).
    // The app layer uses it to append a [paint] line to client.log.
    void SetPaintBackendChangedHandler(
        std::function<void(const std::string&, bool)> handler);

    // DPI change hook: called (on the UI thread) after the shell has
    // rebuilt fonts + metrics in response to msg-DPICHANGED.  The app
    // layer uses it to rebuild page content whose geometry was cached
    // in pixels at the old DPI (e.g. ListBox row heights).
    void SetDpiChangedHandler(std::function<void(int dpi)> handler);
    int  Dpi() const { return m_dpi; }

    // Platform-free shell state / layout / hit test. Core owns state,
    // the native backend owns pixels (see ShellCore.h).
    ShellCore&       Core()       { return m_shellCore; }
    const ShellCore& Core() const { return m_shellCore; }

    void SetMessageHook(
        std::function<bool(WindowHandle, MessageId, MessageWParam, MessageLParam, MessageResult&)> hook);

    // 功能键等加速键：在消息循环里、翻译按键消息之前过一遍。
    // 焦点常落在 edit control / list control 上，主窗口收不到按键消息，
    // 只挂在窗口过程上是「按了没反应」最常见的原因。返回 true 吞掉。
    void SetAcceleratorHandler(std::function<bool(const MessageInfo&)> handler);
    bool IsModalActive() const { return m_modalActive; }

    virtual bool OnShellMessage(WindowHandle hwnd, MessageId msg, MessageWParam wParam,
                                MessageLParam lParam, MessageResult& result)
    { (void)hwnd; (void)msg; (void)wParam; (void)lParam; (void)result; return false; }

    void SetCloseGuard(std::function<bool()> guard);

protected:
    virtual void OnPaintCaptionExtra(DeviceContextHandle dc, const Rect& bar) { (void)dc; (void)bar; }

    friend class Page;
    void    RegisterWidget(Widget* w);
    Widget* WidgetAt(int id);

private:
    static MessageResult WndProc(WindowHandle, MessageId, MessageWParam, MessageLParam);
    MessageResult HandleMessage(WindowHandle, MessageId, MessageWParam, MessageLParam);

    bool RegisterWindowClass(InstanceHandle);
    void CreateNavItems();
    void LayoutAll();
    void ApplyDpiChange(int newDpi);
    void Paint(DeviceContextHandle dc);
    void InitPaintBackends();

    void DrawCaption(DeviceContextHandle dc, const Rect& rc);
    void DrawCaptionButtons(DeviceContextHandle dc, const Rect& rc);
    void DrawStatusBar(DeviceContextHandle dc, const Rect& rc);
    void DrawSidebarFooter(DeviceContextHandle dc);

    int  CaptionButtonAt(Point clientPt) const;

    // Caption-bar renderer switch button (drawn on top of both chrome
    // paths so it stays visible after switching to the compositing backend).
    int  CaptionActionWidth() const;
    Rect PaintBackendButtonRect() const;
    bool HitTestPaintBackendButton(Point clientPt) const;
    void DrawPaintBackendButton(DeviceContextHandle dc);

    struct PageSlot
    {
        int                   id = 0;
        std::unique_ptr<IPage> page;
    };

    InstanceHandle     m_hinst = nullptr;
    WindowHandle          m_hwnd  = nullptr;
    ShellOptions  m_opt;
    std::string   m_paintBackend;   // resolved runtime choice ("" = auto)
    Fonts         m_fonts;
    NavBar        m_nav;
    ShellCore     m_shellCore;   // platform-free state, layout, hit test

    std::vector<PageSlot> m_pages;
    std::vector<NavGroup> m_navGroups;
    std::vector<Widget*>  m_widgets;
    int           m_pageId = -1;

    Rect          m_content = {};
    Rect          m_side    = {};
    Rect          m_status  = {};
    Rect          m_caption = {};

    // Repaint region of the msg-PAINT currently being assembled, client
    // coordinates. Full client for a whole-window invalidate / full-client
    // compositing (the compositing backend); narrower for partial the backend repaints. Pages may use
    // PaintClip() to skip work that will only be clipped away.
    Rect          m_paintClip = {};

    // Window back buffer reused across msg-PAINTs (the backend path only).
    BackBuffer m_backBuffer;

    int           m_dpi     = 96;
    // 96-DPI baseline metrics (the theme's raw values). msg-DPICHANGED
    // rescales this baseline to the new DPI via ForDpi(); re-deriving
    // from Mx() (already scaled) would compound the scaling.
    Metrics       m_baseMetrics = Metrics();
    std::function<void(int)> m_dpiChanged;
    int           m_capHover = 0;
    bool          m_paintBtnHover = false;
    bool          m_active  = true;
    // Set while a page switch is being assembled. Lifting msg-SETREDRAW
    // makes Windows repaint the whole client area WITH an erase first,
    // which painted a full-window background frame a whole the compositing backend frame
    // (46-63 ms) before the real content arrived -- the visible "flash".
    bool          m_eraseSuppressed = false;
    // Erasing the client is only useful for areas the window manager exposes *before* the
    // next paint can land (size / show changes). For an ordinary
    // invalidation the erase just puts a flat background frame on screen
    // while the frame is being built -- 31-47 ms with the the compositing backend backend,
    // which reads as the whole window blinking. Paint() always renders
    // every pixel from a back buffer, so the erase is dropped there.
    //
    // The erase is also limited to the strips that actually became new
    // (right edge + bottom edge of a grown client). Filling the whole
    // client instead threw a flat background over content that was still
    // valid -- on maximize the msg-Size frame alone runs 140 ms, so the
    // window visibly blinked to a blank surface.
    Rect          m_eraseRects[2] = {};
    int           m_eraseCount    = 0;
    Size          m_lastClient    = { 0, 0 };
    // First paint of a window has nothing on screen yet; erasing then is
    // what keeps the exposed area from showing as uninitialised black.
    bool          m_paintedOnce = false;
    std::function<void(const std::string&, bool)> m_paintBackendChanged;

    // Caption / Status state lives in m_shellCore (the single source of
    // truth). Only platform-only extras (key hints) stay here.
    std::vector<std::pair<std::wstring, std::wstring>> m_statusKeys;
    std::wstring  m_footerText;

    std::wstring  m_infoTitle;
    std::vector<InfoRow> m_infoRows;

    std::function<bool(WindowHandle, MessageId, MessageWParam, MessageLParam, MessageResult&)> m_hook;
    std::function<bool(const MessageInfo&)> m_accel;
    std::function<void(int)> m_navHandler;
    std::function<bool()>    m_closeGuard;
    bool         m_modalActive = false;

    std::unique_ptr<ResponsiveManager> m_responsive;
    std::unique_ptr< ::air::ui::ToastManager> m_toastMgr;
};

} // namespace ui
} // namespace air