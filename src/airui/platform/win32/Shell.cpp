#include "airui/Shell.h"
#include "airui/OverlayWindow.h"
#include "airui/GdiPainter.h"
#include "airui/PaintProviders.h"
#include "airui/PaintFrame.h"
#include "airui/Gfx.h"
#include "airui/Responsive.h"
#include <commctrl.h>
#include <algorithm>
#include <share.h>

#pragma comment(lib, "comctl32.lib")

namespace air {
namespace ui {

// Opt-in paint tracing (AIRUI_PAINT_TRACE=1). Off by default: one static
// check per call, no file is ever opened. Enable it when a repaint looks
// wrong and you need per-frame timings / page-switch costs.
static bool PaintTraceEnabled()
{
    static int on = -1;
    if (on < 0) {
        wchar_t v[8] = {};
        const DWORD n = GetEnvironmentVariableW(L"AIRUI_PAINT_TRACE", v, 8);
        on = (n > 0 && n < 8 && v[0] != L'0') ? 1 : 0;
    }
    return on != 0;
}

// Microsecond clock for frame timing. GetTickCount64 only steps at the
// system timer tick (15.6 ms by default), so every value it reports here
// was really "somewhere in a 15.6 ms bucket" -- 31 vs 35 ms were the same
// measurement twice. QPC resolves to the TSC and costs ~20 ns.
static long long QpcUs()
{
    static const double freq = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return f.QuadPart > 0 ? (double)f.QuadPart : 1.0;
    }();
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return static_cast<long long>(c.QuadPart * 1000000.0 / freq);
}

// Rolling frame-cost summary (AIRUI_FRAME_PROFILE=1). Values are
// microseconds; keep the switch separate from AIRUI_PAINT_TRACE so a
// profiling run never pays for per-frame log writes.
static bool FrameProfileEnabled()
{
    static int on = -1;
    if (on < 0) {
        wchar_t v[8] = {};
        const DWORD n = GetEnvironmentVariableW(L"AIRUI_FRAME_PROFILE", v, 8);
        on = (n > 0 && n < 8 && v[0] != L'0') ? 1 : 0;
    }
    return on != 0;
}

struct FrameCost
{
    long long n = 0;
    long long dcBuild = 0;    // PaintDC: 32-bit DDB + background fill
    long long frameBegin = 0; // PainterRegistry::Begin (SkiaFrame construct)
    long long draw = 0;       // every gfx::*/IPainter call of the frame
    long long present = 0;    // SkiaFrame::Impl::Present (copy + composite)
    long long flush = 0;      // BitBlt of the back buffer to the screen

    void Dump(const wchar_t* why)
    {
        if (n <= 0) return;
        wchar_t path[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH) == 0) return;
        wcscat_s(path, L"\\AireRecord\\frame_cost.log");
        FILE* f = _wfsopen(path, L"a", _SH_DENYWR);
        if (!f) return;
        fwprintf_s(f, L"%s n=%lld dc=%.2f begin=%.2f draw=%.2f present=%.2f flush=%.2f total=%.2f ms\n",
                   why, n,
                   (double)dcBuild / n / 1000.0,
                   (double)frameBegin / n / 1000.0,
                   (double)draw / n / 1000.0,
                   (double)present / n / 1000.0,
                   (double)flush / n / 1000.0,
                   (double)(dcBuild + frameBegin + draw + present + flush) / n / 1000.0);
        fclose(f);
        n = dcBuild = frameBegin = draw = present = flush = 0;
    }
};

static FrameCost& Cost()
{
    static FrameCost c;
    return c;
}

static void uiDebugLog(const wchar_t* fmt, ...)
{
    if (!PaintTraceEnabled()) return;

    // Open per record with read sharing: a held handle would lock the log
    // for the whole session, and the trace is only useful live -- the
    // analyst reads it while the client is still running.
    wchar_t path[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 40) return;
    wcscat_s(path, L"\\AireRecord\\shell_paint.log");
    FILE* f = _wfsopen(path, L"a", _SH_DENYWR);
    if (!f) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fwprintf_s(f, L"%02d:%02d:%02d.%03d ", st.wHour, st.wMinute, st.wSecond,
               st.wMilliseconds);
    va_list ap; va_start(ap, fmt);
    vfwprintf_s(f, fmt, ap);
    va_end(ap); fwprintf_s(f, L"\n");
    fclose(f);
}

// ============================================================
// IPage::ParentWnd
// ============================================================
HWND IPage::ParentWnd() const
{
    return m_shell ? m_shell->Hwnd() : nullptr;
}

// ============================================================
// 构造 / 析构
// ============================================================
namespace {

// Caption-bar renderer switch button. Its reserved width is also fed to
// ShellCore's caption layout (SetShellGeometry), so the badge and the
// app name never end up underneath it in either chrome path.
const int kPaintBtnW   = 160;
const int kPaintBtnH   = 26;
const int kPaintBtnGap = 12;   // gap between the button and the 3 caption keys

RECT ToWinRect(const Rect& r)
{
    return RECT{ r.left, r.top, r.right, r.bottom };
}

// NavGroup / NavItem are Win32 control types; ShellNavGroup /
// ShellNavItem are the platform-free equivalents owned by ShellCore.
// Keeping the conversion here means the core never sees a Win32 type.
std::vector<ShellNavGroup> ToCoreNavGroups(const std::vector<NavGroup>& src)
{
    std::vector<ShellNavGroup> out;
    out.reserve(src.size());
    for (const auto& g : src) {
        ShellNavGroup cg;
        cg.title = g.title;
        cg.items.reserve(g.items.size());
        for (const auto& it : g.items) {
            ShellNavItem ci;
            ci.id       = it.id;
            ci.label    = it.label;
            ci.disabled = it.disabled;
            ci.badge    = it.badge;
            ci.badgeOk  = it.badgeOk;
            cg.items.push_back(std::move(ci));
        }
        out.push_back(std::move(cg));
    }
    return out;
}

} // namespace

Shell::Shell()
{
    // Core is the single source of truth for caption / status state.
    CaptionState cs;
    cs.badgeOk = true;
    m_shellCore.SetCaption(cs);

    StatusState ss;
    ss.text = L"就绪";
    ss.ok   = true;
    m_shellCore.SetStatus(ss);
}

Shell::~Shell()
{
    m_widgets.clear();
    for (auto& slot : m_pages)
        if (slot.page) slot.page->OnDetach();
    m_pages.clear();
    m_fonts.Destroy();
}

// ============================================================
// 创建
// ============================================================
bool Shell::RegisterWindowClass(HINSTANCE hInst)
{
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &Shell::WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon         = m_opt.icon ? m_opt.icon
                                  : LoadIconW(hInst, MAKEINTRESOURCEW(1));
    if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = wc.hIcon;
    wc.hbrBackground = nullptr;   // 全自绘，禁用系统擦背景
    wc.lpszClassName = m_opt.className.c_str();

    // 已注册（同类名多实例）不算失败
    if (!RegisterClassExW(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;
    return true;
}

bool Shell::Create(HINSTANCE hInstance, const ShellOptions& options)
{
    m_hinst = hInstance;
    m_opt   = options;
    InitPaintBackends();
    if (m_opt.className.empty()) m_opt.className = L"AirUiShellWnd";
    if (m_opt.appName.empty())   m_opt.appName   = L"App";

    CaptionState cs = m_shellCore.Caption();
    cs.title = m_opt.appName;
    m_shellCore.SetCaption(cs);

    // DPI：默认跟随系统
    m_dpi = m_opt.dpi;
    if (m_dpi <= 0) {
        HDC dc = GetDC(nullptr);
        m_dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
        if (dc) ReleaseDC(nullptr, dc);
        if (m_dpi <= 0) m_dpi = 96;
    }
    SetMetrics(Metrics().ForDpi(m_dpi));
    if (m_opt.dark) SetPalette(DarkPalette());

    if (!RegisterWindowClass(hInstance)) {
        uiDebugLog(L"RegisterWindowClass failed, gle=%lu", GetLastError());
        return false;
    }
    if (!m_fonts.Create(m_dpi)) {
        uiDebugLog(L"Fonts.Create failed");
        return false;
    }

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // WS_CLIPCHILDREN：父窗口自绘（双缓冲 BitBlt 覆盖全客户区）时避开子窗口
    // 区域，否则父/子的 WM_PAINT 派发顺序不定，后画者覆盖先画者——
    // 实测会随机吞掉输入框首次显示时的占位提示。
    //
    // 它必须放在 *dwStyle* 里。WS_CLIPCHILDREN 和 WS_EX_COMPOSITED 都是
    // 0x02000000，但分属 style 与 ex-style 两个字段：这里曾经把
    // WS_CLIPCHILDREN 传成第一个参数（dwExStyle），于是窗口实际拿到的是
    // 「合成窗口」而不是「裁剪子窗口」——父窗口每次自绘都会把 LISTBOX 和
    // 各个 EDIT 一起拖下水重画一遍。Skia 一帧 30~47ms，用户看到的就是
    // 「点一下筛选胶囊，文本框和列表一起闪」。
    m_hwnd = CreateWindowExW(0,
                             m_opt.className.c_str(), m_opt.appName.c_str(),
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             m_opt.width, m_opt.height,
                             nullptr, nullptr, hInstance, this);
    if (!m_hwnd) {
        uiDebugLog(L"CreateWindowEx failed, gle=%lu", GetLastError());
        return false;
    }

    ApplyDwmFrame(m_hwnd);
    uiDebugLog(L"Create ok, hwnd=%p", (void*)m_hwnd);
    return true;
}

int Shell::RunMessageLoop()
{
    if (!m_hwnd) return 1;

    // 页面挂载放在窗口创建之后：OnAttach 里可以安全建子控件
    for (auto& slot : m_pages)
        if (slot.page && slot.page->m_shell != this) {
            slot.page->m_shell = this;
            slot.page->OnAttach(*this);
        }

    if (m_pageId < 0 && !m_pages.empty()) SwitchPage(m_pages.front().id);

    // 静默启动：窗口已建好但不显示（页面/子控件仍完成挂载与首帧布局）
    ShowWindow(m_hwnd, m_opt.showOnCreate ? SW_SHOW : SW_HIDE);
    UpdateWindow(m_hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        // 加速键必须在派发之前统一处理：按键消息会投给当前焦点窗口，
        // 而焦点几乎总在页面的 EDIT / LISTBOX 上，主窗口 WndProc
        // 根本收不到 F3/F4。交给应用层判断消息归属，库里不含业务语义。
        if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) &&
            !m_modalActive && m_accel && m_accel(msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    for (auto& slot : m_pages)
        if (slot.page) slot.page->OnDetach();
    return (int)msg.wParam;
}

// ============================================================
// 页面管理
// ============================================================
void Shell::AddPage(int id, std::unique_ptr<IPage> page)
{
    if (!page) return;
    PageSlot slot;
    slot.id   = id;
    slot.page = std::move(page);
    m_pages.push_back(std::move(slot));

    // 窗口已创建时立即挂载
    if (m_hwnd) {
        m_pages.back().page->m_shell = this;
        m_pages.back().page->OnAttach(*this);
    }
}

IPage* Shell::FindPage(int id) const
{
    for (auto& slot : m_pages)
        if (slot.id == id) return slot.page.get();
    return nullptr;
}

IPage* Shell::CurrentPage() const
{
    return FindPage(m_pageId);
}

bool Shell::SwitchPage(int id)
{
    if (id == m_pageId) return CurrentPage() != nullptr;
    IPage* next = FindPage(id);
    if (!next) return false;
    IPage* prev = CurrentPage();

    // Freeze painting for the WHOLE switch, not just the layout call.
    // Previously OnShow(false) ran before the freeze and OnShow(true) ran
    // after it was lifted, so any child-control erase/creation flashed the
    // half-built new page on screen before the real frame arrived -- very
    // visible with the Skia backend, where a frame costs far more than a
    // GDI repaint. With everything inside the freeze only the finished
    // frame is ever presented.
    SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);
    // Lifting the freeze repaints the client area WITH an erase. Suppress
    // that fill for the duration of the switch, otherwise the user sees a
    // flat background frame until the (slow) Skia frame lands. Only safe
    // once something has been painted at least once.
    m_eraseSuppressed = m_paintedOnce;

    if (prev) prev->OnShow(false);

    m_pageId = id;
    m_nav.SetActive(id);
    m_shellCore.SetActiveNav(id);

    LayoutAll();
    next->OnShow(true);

    ::InvalidateRect(m_hwnd, nullptr, FALSE);
    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);

    // WM_SETREDRAW TRUE only clears the freeze flag; it does not repaint
    // by itself. Force one synchronous frame so the switch lands as a
    // single swap instead of waiting for the next idle WM_PAINT.
    const ULONGLONG t0 = GetTickCount64();
    ::RedrawWindow(m_hwnd, nullptr, nullptr,
                   RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE |
                   RDW_ALLCHILDREN);
    m_eraseSuppressed = false;
    uiDebugLog(L"[switch] id=%d sync_ms=%llu", id,
               (unsigned long long)(GetTickCount64() - t0));
    return true;
}

void Shell::CreateNavItems()
{
    if (!m_navGroups.empty()) return;   // 已手工配置

    NavGroup g;
    g.title = L"功能模块";
    for (auto& slot : m_pages) {
        if (!slot.page) continue;
        NavItem it;
        it.id       = slot.id;
        it.glyph    = slot.page->NavGlyph();
        it.label    = slot.page->NavLabel();
        it.disabled = slot.page->NavDisabled();
        g.items.push_back(it);
    }
    if (!g.items.empty()) m_navGroups.push_back(std::move(g));
    m_nav.SetGroups(m_navGroups);
    m_shellCore.SetNavGroups(ToCoreNavGroups(m_navGroups));
}

void Shell::SetNavGroups(std::vector<NavGroup> groups)
{
    m_navGroups = std::move(groups);
    m_nav.SetGroups(m_navGroups);
    m_shellCore.SetNavGroups(ToCoreNavGroups(m_navGroups));
    if (m_hwnd) LayoutAll();
}

// ============================================================
// 导航命令
// ============================================================
void Shell::AddNavCommand(int id, const std::wstring& glyph,
                          const std::wstring& label, bool disabled)
{
    NavItem it;
    it.id       = id;
    it.glyph    = glyph;
    it.label    = label;
    it.disabled = disabled;

    // 追加到最后一个分组；没有任何分组时新建「快捷操作」
    std::vector<NavGroup> groups = m_navGroups;
    if (groups.empty()) {
        NavGroup g;
        g.title = L"快捷操作";
        groups.push_back(std::move(g));
    }
    groups.back().items.push_back(std::move(it));
    SetNavGroups(std::move(groups));
}

void Shell::SetNavBadge(int id, const std::wstring& text, bool ok)
{
    m_nav.SetBadge(id, text, ok);
    m_shellCore.SetNavBadge(id, text, ok);
    if (m_hwnd) Invalidate();
}

void Shell::SetNavCommandHandler(std::function<void(int)> handler)
{
    m_navHandler = std::move(handler);
}

// ============================================================
// 标题栏 / 状态栏 / 侧栏外观
// ============================================================
void Shell::SetCaptionBadge(const std::wstring& text, bool ok)
{
    CaptionState cs = m_shellCore.Caption();
    cs.badge   = text;
    cs.badgeOk = ok;
    m_shellCore.SetCaption(cs);
    if (m_hwnd) InvalidateTitle();
}

void Shell::SetCaptionSubtitle(const std::wstring& text)
{
    CaptionState cs = m_shellCore.Caption();
    cs.subtitle = text;
    m_shellCore.SetCaption(cs);
    if (m_hwnd) InvalidateTitle();
}

void Shell::SetStatus(const std::wstring& text, bool ok)
{
    StatusState ss = m_shellCore.Status();
    // Pages call this on every list refresh, usually with the same string.
    // Repainting the whole client for an unchanged status is pure cost --
    // and on the Skia path every such repaint is a full window frame.
    if (ss.text == text && ss.ok == ok) return;
    ss.text = text;
    ss.ok   = ok;
    m_shellCore.SetStatus(ss);
    if (m_hwnd) Invalidate();
}

void Shell::SetStatusExtra(const std::wstring& text)
{
    StatusState ss = m_shellCore.Status();
    ss.extra = text;
    m_shellCore.SetStatus(ss);
    if (m_hwnd) Invalidate();
}

void Shell::SetStatusKeys(std::vector<std::pair<std::wstring, std::wstring>> keys)
{
    m_statusKeys = std::move(keys);
    if (m_hwnd) Invalidate();
}

void Shell::SetFooterText(const std::wstring& text)
{
    m_footerText = text;
    if (m_hwnd) Invalidate();
}

void Shell::SetSidebarInfo(const std::wstring& title,
                           const std::vector<InfoRow>& rows)
{
    m_infoTitle = title;
    m_infoRows  = rows;
    if (m_hwnd) Invalidate();
}

// ============================================================
// 刷新
// ============================================================
void Shell::Invalidate()
{
    if (m_hwnd) ::InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Shell::InvalidateContent()
{
    if (m_hwnd) ::InvalidateRect(m_hwnd, &m_content, FALSE);
}

void Shell::InvalidateTitle()
{
    if (!m_hwnd) return;
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    rc.bottom = Mx().titleH;
    ::InvalidateRect(m_hwnd, &rc, FALSE);
}

void Shell::InvalidateRegion(const RECT& rcClient)
{
    if (m_hwnd) ::InvalidateRect(m_hwnd, &rcClient, FALSE);
}

void Shell::SetRedraw(bool on)
{
    if (!m_hwnd) return;

    SendMessageW(m_hwnd, WM_SETREDRAW, on ? TRUE : FALSE, 0);
    if (!on) return;

    // 重新打开重绘时要**连子控件一起**失效。
    //
    // 批处理期间（WM_SETREDRAW FALSE）被 Move/Show 的子控件，它们挂起的
    // 那次重绘会丢：父窗口重绘标志为假时子窗口的 WM_PAINT 不会落地，等到
    // 标志恢复，WM_SETREDRAW TRUE 也只让外壳自己重新变脏 —— 而外壳带
    // WS_CLIPCHILDREN，子控件占的矩形本来就排除在它的绘制区之外。
    // 结果就是子控件位置上留着**搬家之前**的旧像素：剪贴板页点「＋ 手动
    // 录入」展开表单卡时，多行录入框里显示的还是上方的列表行，要点一下
    // 输入框或最小化还原才恢复。
    //
    // RDW_ALLCHILDREN 把所有后代一起标脏，把批处理吞掉的那次重绘补回来。
    // 代价是一次整窗失效（本来就是这一帧要做的事），不能省。
    RedrawWindow(m_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

int Shell::ShowMessageBox(const std::wstring& text, const std::wstring& caption,
                          UINT flags)
{
    if (!m_hwnd) return 0;
    // 应用内所有确认框都走自绘 Modal：风格跟设计系统一致，也避免
    // 原生弹窗把无边框窗口的焦点/重绘节奏打乱。服务安装器是命令行
    // 工具，没有 Shell，仍用系统弹窗。
    m_modalActive = true;
    const int rc = RunModalDialog(m_hwnd, m_fonts, caption, text, flags);
    m_modalActive = false;
    return rc;
}

int Shell::ShowPopupMenu(POINT screenPt, const std::vector<MenuItem>& items)
{
    if (!m_hwnd || items.empty()) return 0;

    HMENU menu = CreatePopupMenu();
    for (const auto& it : items) {
        if (it.separator) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        } else {
            UINT f = MF_STRING | (it.disabled ? MF_DISABLED : 0);
            AppendMenuW(menu, f, it.id, it.label.c_str());
        }
    }

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(menu,
                             TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                             screenPt.x, screenPt.y, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
    return cmd;
}

int Shell::MeasureTextWidth(const std::wstring& s, FontRole role)
{
    HDC dc = GetDC(nullptr);
    int w = gfx::MeasureWidth(dc, m_fonts.Get(role), s);
    ReleaseDC(nullptr, dc);
    return w;
}

int Shell::TextLineHeight(FontRole role)
{
    HDC dc = GetDC(nullptr);
    int h = gfx::LineHeight(dc, m_fonts.Get(role));
    ReleaseDC(nullptr, dc);
    return h;
}

// ============================================================
// 控件注册表
// ============================================================
void Shell::RegisterWidget(Widget* w)
{
    if (w) m_widgets.push_back(w);
}

Widget* Shell::WidgetAt(int id)
{
    for (Widget* w : m_widgets)
        if (w && w->Id() == id) return w;
    return nullptr;
}

void Shell::SetMessageHook(
    std::function<bool(HWND, UINT, WPARAM, LPARAM, LRESULT&)> hook)
{
    m_hook = std::move(hook);
}

void Shell::SetAcceleratorHandler(std::function<bool(const MSG&)> handler)
{
    m_accel = std::move(handler);
}

void Shell::SetCloseGuard(std::function<bool()> guard)
{
    m_closeGuard = std::move(guard);
}

// ============================================================
// 布局
// ============================================================
void Shell::LayoutAll()
{
    if (!m_hwnd) return;
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const Metrics& m = Mx();

    int W = rc.right, H = rc.bottom;
    int statusH = m_opt.statusBar ? m.statusH : 0;
    int sideW   = m_opt.sidebar   ? m.sidebarW : 0;

    m_caption = { 0, 0, W, m.titleH };
    m_status  = { 0, H - statusH, W, H };
    m_side    = { 0, m.titleH, sideW, H - statusH };

    // 内容区：排除标题栏 / 侧栏 / 状态栏与页面外边距
    m_content = { sideW + m.pagePad,
                  m.titleH + 14,
                  W - m.pagePad,
                  H - statusH - 12 };
    if (m_content.right  < m_content.left) m_content.right  = m_content.left;
    if (m_content.bottom < m_content.top)  m_content.bottom = m_content.top;

    CreateNavItems();
    if (m_opt.sidebar) m_nav.Layout(m_side);

    // Feed the geometry we actually draw into the platform-free core so
    // ContentRect / HitTestNav never drift from the Win32 pixels.
    m_shellCore.SetShellGeometry(m.titleH, statusH, sideW,
                                 CaptionActionWidth());
    m_shellCore.Layout(Rect{ 0, 0, W, H });

    if (IPage* p = CurrentPage()) p->OnLayout(m_content);
}

// ============================================================
// 运行时绘制后端
// ============================================================
void Shell::InitPaintBackends()
{
    // gdi is always registered; skia is linked in when a SDK was
    // available at build time, otherwise resolved from airui-skia.dll
    // on first use. Neither path is chosen by a preprocessor flag.
    RegisterPlatformPaintProviders();

    // Precedence: ShellOptions.paintBackend (also fed by --paint-backend)
    // > AIRUI_PAINT_BACKEND > legacy skiaChrome > "auto" (= gdi).
    m_paintBackend = ResolvePaintBackend(m_opt.paintBackend,
                                         EnvironmentPaintBackend(),
                                         m_opt.skiaChrome);
}

void Shell::SetPaintBackend(const std::string& id)
{
    m_paintBackend = NormalizeBackend(id);
    Invalidate();   // hot switch: the very next frame uses the new id
}

std::vector<std::string> Shell::AvailablePaintBackends() const
{
    PainterRegistry& registry = PainterRegistry::Instance();
    std::vector<std::string> out;
    for (const std::string& id : registry.Backends())
        if (registry.Available(id)) out.push_back(id);
    return out;
}

// ============================================================
// 标题栏「渲染后端」切换按钮
//
// 按钮属于外壳（backend-owned chrome）：legacy 富渲染与 coreShell/HAL
// 两条路径下都可见、都可点，所以切到 Skia 之后仍然能切回来。它和窗口
// 其它部分一样走 PaintFrame → 当前后端，文字由帧级文字层统一处理。
// G5 金样对比走独立的 ShellCore::Paint 序列，不受按钮影响。
// ============================================================
int Shell::CaptionActionWidth() const
{
    // 三键 + 与三键的间距 + 按钮 + 与状态胶囊的间距
    return Mx().capBtnW * 3 + kPaintBtnGap + kPaintBtnW + 10;
}

RECT Shell::PaintBackendButtonRect() const
{
    RECT rc = {};
    if (!m_hwnd) return rc;
    GetClientRect(m_hwnd, &rc);
    const int right = rc.right - Mx().capBtnW * 3 - kPaintBtnGap;
    const int top   = (Mx().titleH - kPaintBtnH) / 2;
    return RECT{ right - kPaintBtnW, top, right, top + kPaintBtnH };
}

bool Shell::HitTestPaintBackendButton(POINT pt) const
{
    const RECT r = PaintBackendButtonRect();
    return pt.x >= r.left && pt.x < r.right &&
           pt.y >= r.top  && pt.y < r.bottom;
}

void Shell::DrawPaintBackendButton(HDC dc)
{
    const RECT r = PaintBackendButtonRect();
    if (r.right <= r.left) return;

    const Palette& p = Pal();
    const std::vector<std::string> available = AvailablePaintBackends();
    const bool hasSkia = std::find(available.begin(), available.end(), "skia")
                         != available.end();
    const std::string cur = NormalizeBackend(m_paintBackend);
    const bool skiaOn = (cur == "skia");

    std::wstring label = L"渲染: ";
    // "auto" 会由注册表解析成默认后端 gdi —— 按钮显示实际生效的后端，
    // 而不是配置模式，避免用户以为「自动」是第三种渲染器。
    if (skiaOn) label += L"Skia";
    else if (cur == "gdi" || cur == "auto" || cur.empty()) label += L"GDI";
    else label += std::wstring(cur.begin(), cur.end());

    // 悬停用 pal.active：pal.hover(#FAFAFA) 在白色标题栏上等于没变，
    // 这个按钮之前"指上去毫无反应"就是这么来的。
    COLORREF bg = skiaOn ? p.primarySoft
                : m_paintBtnHover ? p.active : p.surface;
    COLORREF border = skiaOn ? p.primary
                    : m_paintBtnHover ? p.hoverBorder : p.border;
    COLORREF fg = skiaOn ? p.primaryText
                : m_paintBtnHover ? p.primaryText : p.textSub;
    if (!hasSkia) {
        // 选了 Skia 却加载不到插件时注册表会静默回退 GDI —— 按钮必须
        // 把这件事说出来，否则「显示 Skia、实际 GDI」会误导。
        if (skiaOn) {
            label += L" · 已回退";
            border = p.errorBorder; fg = p.errorText;
        } else {
            label += L" · 无插件";
            border = p.warnBorder; fg = p.warnText;
        }
    }

    gfx::FillRound(dc, r, bg, 6);
    gfx::Stroke(dc, r, border, 6);
    RECT tr = { r.left + 8, r.top, r.right - 8, r.bottom };
    gfx::Text(dc, m_fonts.Get(FontRole::Meta), fg, tr, label,
              DT_SINGLELINE | DT_CENTER | DT_VCENTER |
              DT_NOPREFIX | DT_END_ELLIPSIS);
}

void Shell::TogglePaintBackend()
{
    const std::vector<std::string> available = AvailablePaintBackends();
    // 只有一个可用后端时什么都不做，按钮标签已写明「无插件」。
    if (available.size() < 2) { Invalidate(); return; }

    const std::string next = NextPaintBackend(m_paintBackend, available);
    // 不再改 coreShell：后端切换只换像素来源，不把界面换成另一套外壳。
    SetPaintBackend(next);          // 归一化 + 下一帧生效
    if (m_paintBackendChanged) {
        const bool backendText =
            (next == "skia") && SkiaPluginSupportsWideText();
        m_paintBackendChanged(next, backendText);
    }
}

void Shell::SetPaintBackendChangedHandler(
    std::function<void(const std::string&, bool)> handler)
{
    m_paintBackendChanged = std::move(handler);
}

// ============================================================
// 绘制
// ============================================================
void Shell::Paint(HDC dc)
{
    const long long tPaint0 = QpcUs();
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    // ---- Open ONE frame for the entire client area -----------------
    // gfx::* and CanvasPainter both route through PaintFrame, so the
    // selected backend draws chrome, pages and core widgets alike --
    // not just a separate HAL chrome path. A plain GDI frame leaves
    // the routing inactive and runs the original code untouched.
    const bool wantSkia = (NormalizeBackend(m_paintBackend) == "skia") &&
                          PainterRegistry::Instance().Available("skia");
    std::unique_ptr<IPainter> backend;
    bool frameActive = false;

    if (wantSkia) {
        PaintTarget target{
            static_cast<void*>(dc),
            Rect{ rc.left, rc.top, rc.right, rc.bottom }
        };
        backend = PainterRegistry::Instance().Begin("skia", target);
        const std::string& err = PainterRegistry::Instance().LastError();
        if (backend && err.empty()) {
            PaintFrame& frame = PaintFrame::Instance();
            frame.Begin(backend.get(), dc, &m_fonts,
                        SkiaPluginSupportsWideText());
            frameActive = frame.Active();
        } else {
            if (!err.empty()) uiDebugLog(L"[paint] %hs", err.c_str());
            backend.reset();       // fell back to gdi: keep GDI rendering
        }
    }

    const long long tBegan = QpcUs();
    gfx::Fill(dc, rc, Pal().bg);


    if (m_opt.coreShell) {
        // HAL chrome still owns state and layout; its IPainter calls now
        // go through the same frame router as everything else.
        if (frameActive) {
            m_shellCore.Paint(PaintFrame::Instance(), Pal(), Mx());
        } else {
            GdiPainter fallback(dc);
            m_shellCore.Paint(fallback, Pal(), Mx());
        }
    } else {
        // Legacy rich chrome (logo, three caption buttons, status pills,
        // key hints). Its state is read back from ShellCore.
        DrawCaption(dc, rc);
        if (m_opt.sidebar) {
            m_nav.Paint(dc, m_fonts);
            DrawSidebarFooter(dc);
        }
    }

    if (IPage* p = CurrentPage()) p->OnPaint(dc, m_content);

    if (!m_opt.coreShell && m_opt.statusBar) DrawStatusBar(dc, rc);

    // 外壳动作区最后画：两条 chrome 路径下都在最上层，且不会被页面
    // 的 HWND 子控件遮挡（标题栏内没有子控件）。
    if (m_opt.coreShell) DrawCaptionButtons(dc, rc);
    DrawPaintBackendButton(dc);

    const long long tDrawn = QpcUs();

    // ---- Close the frame -------------------------------------------
    // Order matters: the backend's destructor composites its raster
    // onto the DC; host-rasterised text must be drawn AFTER that or it
    // would be covered by the composite.
    if (frameActive) {
        PaintFrame& frame = PaintFrame::Instance();
        frame.End();
        backend.reset();       // SkiaFrame::~SkiaFrame -> AlphaBlend
        frame.ReplayText();
        frame.Clear();
    }

    const long long tPresented = QpcUs();

    if (PaintTraceEnabled()) {
        uiDebugLog(L"[paint] page=%d active=%d ms=%.2f (begin=%.2f draw=%.2f present=%.2f)",
                   m_pageId, frameActive ? 1 : 0,
                   (double)(tPresented - tPaint0) / 1000.0,
                   (double)(tBegan - tPaint0) / 1000.0,
                   (double)(tDrawn - tBegan) / 1000.0,
                   (double)(tPresented - tDrawn) / 1000.0);
    }
    if (FrameProfileEnabled()) {
        PaintFrameReportPrimCost();
        FrameCost& c = Cost();
        c.frameBegin += tBegan - tPaint0;
        c.draw       += tDrawn - tBegan;
        c.present    += tPresented - tDrawn;
        if (++c.n >= 30) c.Dump(L"[cost]");
    }
    m_paintedOnce = true;
}

// ------------------------------------------------------------
// 标题栏：Logo + 应用名 + 竖线 + 副标题 + 自定义区 + 状态胶囊 + 三键
// ------------------------------------------------------------
void Shell::DrawCaption(HDC dc, const RECT& rc)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();
    const CaptionState& cs = m_shellCore.Caption();   // core is the state source

    gfx::Fill(dc, m_caption, p.surface);
    gfx::HLine(dc, 0, rc.right, m.titleH - 1, p.split);

    // Logo 26x26
    RECT rcLogo = { 14, 9, 40, 35 };
    gfx::FillRound(dc, rcLogo, p.primary, 7);
    if (!m_opt.logoLetter.empty()) {
        gfx::Text(dc, m_fonts.Get(FontRole::Button), p.textInverse, rcLogo,
                  m_opt.logoLetter,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    } else {
        // 默认图形：白色圆角板 + 两条主色横条（通用文档意象）
        RECT board = { rcLogo.left + 7, rcLogo.top + 6,
                       rcLogo.right - 7, rcLogo.bottom - 6 };
        gfx::FillRound(dc, board, p.textInverse, 2);
        RECT l1 = { board.left + 3, board.top + 4,
                    board.right - 8, board.top + 7 };
        RECT l2 = { board.left + 3, board.top + 10,
                    board.right - 3, board.top + 13 };
        gfx::Fill(dc, l1, p.primary);
        gfx::Fill(dc, l2, p.primary);
    }

    // 应用名。右侧要让出「渲染后端切换按钮 + 状态胶囊」的保留区，
    // 否则切换按钮启用后会把标题/副标题压在下面。
    int rightLimit = rc.right - CaptionActionWidth() - 12;
    int nameRight  = rightLimit - 132;
    if (nameRight < 60) nameRight = 60;
    RECT rcName = { 52, 0, nameRight, m.titleH };
    gfx::Text(dc, m_fonts.Get(FontRole::Title), p.text, rcName, m_opt.appName,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    int nameW = gfx::MeasureWidth(dc, m_fonts.Get(FontRole::Title), m_opt.appName);
    int dx = 52 + nameW + 10;

    gfx::VLine(dc, dx, m.titleH / 2 - 8, m.titleH / 2 + 8, p.split);

    // 副标题
    std::wstring sub = cs.subtitle;
    if (sub.empty()) {
        if (IPage* pg = CurrentPage()) sub = pg->Title();
    }
    RECT rcSub = { dx + 10, 0, nameRight, m.titleH };
    gfx::Text(dc, m_fonts.Get(FontRole::Meta), RGB(143, 149, 158), rcSub, sub,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    // 自定义区（派生类）
    RECT rcExtra = { dx + 10, 0, nameRight, m.titleH };
    OnPaintCaptionExtra(dc, rcExtra);

    // 状态胶囊
    if (!cs.badge.empty()) {
        gfx::StatusTag(dc, rightLimit, m.titleH / 2,
                       cs.badge.c_str(),
                       m_fonts.Get(FontRole::Meta),
                       cs.badgeOk ? p.okSoft : p.errorSoft,
                       cs.badgeOk ? p.okBorder : p.errorBorder,
                       cs.badgeOk ? p.okText : p.errorText,
                       cs.badgeOk ? p.ok : p.error,
                       24);
    }

    // 三键（与 HAL 外壳共用同一份绘制，见 DrawCaptionButtons）
    DrawCaptionButtons(dc, rc);
}

// ------------------------------------------------------------
// 标题栏三键。legacy 由 DrawCaption 调用；HAL 外壳（coreShell）下
// ShellCore 不画三键，但命中测试一直存在 —— 这里补画，避免切到 Skia
// 后「关闭/最小化按钮变成隐形」。
// ------------------------------------------------------------
void Shell::DrawCaptionButtons(HDC dc, const RECT& rc)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();
    const wchar_t* glyphs[3] = {
        L"\xE921",                                     // 最小化
        IsZoomed(m_hwnd) ? L"\xE923" : L"\xE922",      // 还原 / 最大化
        L"\xE8BB"                                      // 关闭
    };
    for (int i = 0; i < 3; ++i) {
        RECT rcBtn = { rc.right - m.capBtnW * (3 - i), 0,
                       rc.right - m.capBtnW * (2 - i), m.titleH };
        if (m_capHover == i + 1) {
            gfx::Fill(dc, rcBtn,
                      (i == 2) ? p.captionCloseHover : p.captionBtnHover);
        }
        COLORREF tx = (i == 2 && m_capHover == 3) ? p.textInverse : p.text;
        gfx::Text(dc, m_fonts.Get(FontRole::Icon), tx, rcBtn, glyphs[i],
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }
}

// PaintFrame now records and replays text for the WHOLE window, so
// the old chrome-only GDI text overlay is no longer needed.

// ------------------------------------------------------------
// 状态栏：左「状态点 + 文本 + 副信息」，右 kbd 键位提示
// ------------------------------------------------------------
void Shell::DrawStatusBar(HDC dc, const RECT& rc)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();
    const StatusState& ss = m_shellCore.Status();   // core is the state source
    (void)rc;

    gfx::Fill(dc, m_status, p.surface);
    gfx::HLine(dc, 0, m_status.right, m_status.top, p.split);

    int cy = m_status.top + m.statusH / 2;

    // 状态点
    gfx::Dot(dc, 17, cy, 6, ss.ok ? p.ok : p.error);

    // 状态文本
    std::wstring svc = ss.text;
    gfx::Text(dc, m_fonts.Get(FontRole::Meta),
              ss.ok ? p.okText : p.errorText,
              RECT{ 26, m_status.top, m_status.right - 400, m_status.bottom },
              svc, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    // 副信息
    if (!ss.extra.empty()) {
        int w = gfx::MeasureWidth(dc, m_fonts.Get(FontRole::Meta), svc);
        int x2 = 26 + w + 18;
        gfx::Text(dc, m_fonts.Get(FontRole::Meta), RGB(143, 149, 158),
                  RECT{ x2, m_status.top, m_status.right - 300, m_status.bottom },
                  ss.extra,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    }

    // 键位提示（从右往左）
    int right = m_status.right - 14;
    for (auto it = m_statusKeys.rbegin(); it != m_statusKeys.rend(); ++it) {
        gfx::KbdChip(dc, right, cy, it->first.c_str(), it->second.c_str(),
                     m_fonts.Get(FontRole::Meta));
        right -= gfx::KbdChipWidth(dc, it->first.c_str(),
                                   it->second.c_str(),
                                   m_fonts.Get(FontRole::Meta)) + 20;
    }
}

// ------------------------------------------------------------
// 侧栏底部：信息卡 + 版本行
// ------------------------------------------------------------
void Shell::DrawSidebarFooter(HDC dc)
{
    if (m_infoRows.empty() && m_footerText.empty()) return;

    const Palette& p = Pal();
    const Metrics& m = Mx();

    int cardBottom = m_side.bottom - 26;
    int cardH = 30 + (int)m_infoRows.size() * 24 + 12;
    int cardTop = cardBottom - cardH;

    // 与导航内容太挤就不画，避免重叠
    if (cardTop < m_side.top + m_nav.ContentHeight() + 16) return;

    if (!m_infoRows.empty()) {
        RECT rcCard = { 10, cardTop, m_side.right - 10, cardBottom };
        gfx::FillRound(dc, rcCard, p.surface, 8);
        gfx::Stroke(dc, rcCard, p.sidebarLine, 8);

        if (!m_infoTitle.empty()) {
            RECT rcCap = { rcCard.left + 12, rcCard.top + 8,
                           rcCard.right - 12, rcCard.top + 26 };
            gfx::Text(dc, m_fonts.Get(FontRole::Meta), RGB(143, 149, 158),
                      rcCap, m_infoTitle,
                      DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
        }
        int y = rcCard.top + 30;
        for (auto& row : m_infoRows) {
            RECT rcK = { rcCard.left + 12, y, rcCard.left + 68, y + 20 };
            gfx::Text(dc, m_fonts.Get(FontRole::Meta), RGB(143, 149, 158),
                      rcK, row.key,
                      DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
            RECT rcV = { rcCard.left + 70, y, rcCard.right - 10, y + 20 };
            gfx::Text(dc, m_fonts.Get(FontRole::Meta),
                      row.ok ? p.okText : p.text, rcV, row.value,
                      DT_SINGLELINE | DT_LEFT | DT_VCENTER |
                      DT_END_ELLIPSIS | DT_NOPREFIX);
            y += 24;
        }
    }

    if (!m_footerText.empty()) {
        RECT rcVer = { 10, cardBottom + 8, m_side.right - 10,
                       cardBottom + 26 };
        gfx::Text(dc, m_fonts.Get(FontRole::Meta), RGB(169, 174, 184),
                  rcVer, m_footerText,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }
}

// ============================================================
// 命中测试
// ============================================================
int Shell::CaptionButtonAt(POINT pt) const
{
    const Metrics& m = Mx();
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    if (pt.y < 0 || pt.y >= m.titleH) return 0;
    if (pt.x >= rc.right - m.capBtnW)     return 3;
    if (pt.x >= rc.right - m.capBtnW * 2) return 2;
    if (pt.x >= rc.right - m.capBtnW * 3) return 1;
    return 0;
}

// ============================================================
// 窗口过程
// ============================================================
LRESULT CALLBACK Shell::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Shell* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lParam;
        self = (Shell*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        if (self) self->m_hwnd = hwnd;
    } else {
        self = (Shell*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);
    return self->HandleMessage(hwnd, msg, wParam, lParam);
}

LRESULT Shell::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 1) 外部钩子（插件 / 快捷键）最优先
    LRESULT hooked = 0;
    if (m_hook && m_hook(hwnd, msg, wParam, lParam, hooked))
        return hooked;

    // 2) 派生类介入
    LRESULT sub = 0;
    if (OnShellMessage(hwnd, msg, wParam, lParam, sub))
        return sub;

    const Metrics& m = Mx();

    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_ERASEBKGND: {
        HDC dc = (HDC)wParam;
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        RECT upd = {};
        GetUpdateRect(hwnd, &upd, FALSE);
        uiDebugLog(L"[erase] page=%d %dx%d suppressed=%d strips=%d "
                   L"update=%ld,%ld-%ld,%ld",
                   m_pageId,
                   (int)(rcClient.right - rcClient.left),
                   (int)(rcClient.bottom - rcClient.top),
                   m_eraseSuppressed ? 1 : 0, m_eraseCount,
                   (long)upd.left, (long)upd.top,
                   (long)upd.right, (long)upd.bottom);
        // Paint() renders the WHOLE client (page, chrome, widgets) into a
        // back buffer and blits it, so an erase can never correct anything
        // an ordinary invalidation asked to repaint -- it only puts a flat
        // background frame on screen while that frame is being built.
        // With Skia a frame costs 31-47 ms at 1120x780, so the erased state
        // spans two or three presents: the user sees the entire window
        // blink every time a control invalidates the client (the filter
        // chips were the loudest case). Erasing is still needed right after
        // a size/show change, where DWM presents the newly exposed area
        // before our first paint can land (a maximized window would flash
        // black) -- and even then only the strips that are new get filled.
        const bool allowed = m_eraseCount > 0;
        if (m_eraseSuppressed || !allowed) return 1;
        // Cache the brush but re-create it when the palette changes:
        // a once-only static brush keeps erasing with the old theme's
        // colour after a light/dark switch, which reads as a flash of
        // the wrong background.
        static HBRUSH   s_bg      = nullptr;
        static COLORREF s_bgColor = CLR_INVALID;
        if (!s_bg || s_bgColor != Pal().bg) {
            if (s_bg) DeleteObject(s_bg);
            s_bgColor = Pal().bg;
            s_bg      = CreateSolidBrush(s_bgColor);
        }
        for (int i = 0; i < m_eraseCount; ++i) {
            RECT r = m_eraseRects[i];
            if (r.right > r.left && r.bottom > r.top) FillRect(dc, &r, s_bg);
        }
        m_eraseCount = 0;
        return 1;
    }

    // ---- 无边框：客户区 = 整个窗口 ----
    case WM_NCCALCSIZE:
        if (!wParam) break;
        if (IsZoomed(hwnd)) {
            auto* r = (RECT*)lParam;
            int fw = GetSystemMetrics(SM_CXSIZEFRAME) +
                     GetSystemMetrics(SM_CXPADDEDBORDER);
            int fh = GetSystemMetrics(SM_CYSIZEFRAME) +
                     GetSystemMetrics(SM_CXPADDEDBORDER);
            r->left += fw; r->top += fh; r->right -= fw; r->bottom -= fh;
        }
        return 0;

    case WM_NCACTIVATE:
        m_active = (wParam != FALSE);
        return DefWindowProcW(hwnd, msg, wParam, -1);

    case WM_NCHITTEST: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        ScreenToClient(hwnd, &pt);

        // 1) 标题栏三键 / 渲染后端按钮优先：按客户区点击处理
        //    （否则标题栏会被判成 HTCAPTION，点击变成拖动窗口）
        if (CaptionButtonAt(pt)) return HTCLIENT;
        if (HitTestPaintBackendButton(pt)) return HTCLIENT;

        // 2) 无边框 + NCCALCSIZE 返回 0 后，DefWindowProc 找不到边缘区；
        //    这里显式提供 6px 边缘热区，拖拽边框即可调整窗口大小。
        if (!IsZoomed(hwnd)) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            const int B = 6;
            bool onLeft   = pt.x >= 0 && pt.x < B;
            bool onRight  = pt.x < rc.right && pt.x >= rc.right - B;
            bool onTop    = pt.y >= 0 && pt.y < B;
            bool onBottom = pt.y < rc.bottom && pt.y >= rc.bottom - B;

            if (onTop && onLeft)     return HTTOPLEFT;
            if (onTop && onRight)    return HTTOPRIGHT;
            if (onBottom && onLeft)  return HTBOTTOMLEFT;
            if (onBottom && onRight) return HTBOTTOMRIGHT;
            if (onLeft)              return HTLEFT;
            if (onRight)             return HTRIGHT;
            if (onTop)               return HTTOP;
            if (onBottom)            return HTBOTTOM;
        }

        LRESULT ht = DefWindowProcW(hwnd, msg, wParam, lParam);
        if (ht != HTCLIENT) return ht;
        if (pt.y >= 0 && pt.y < m.titleH) return HTCAPTION;  // 拖动/双击/吸附
        return HTCLIENT;
    }

    case WM_SETCURSOR: {
        // 指针进了子控件（列表 / 输入框 / 原生按钮）：这些窗口自己收
        // WM_MOUSEMOVE，父窗口一条也收不到，页面里那些自绘热区会一直
        // 停在"悬停"上（从胶囊直接移到列表时，胶囊就一直亮着）。子窗口
        // 的 WM_SETCURSOR 会转给父窗口，这里统一清一次即可。
        if ((HWND)wParam != hwnd && (HWND)wParam != nullptr) {
            if (IPage* p = CurrentPage())
                if (p->OnMouseLeave())
                    ::InvalidateRect(hwnd, &m_content, FALSE);
            break;
        }

        // 自绘按钮 / 卡片不是子窗口，光标只能由页面自己主张。只处理
        // "光标就在本窗口上"的情形（wParam == hwnd）：DefWindowProc 会把
        // 子窗口的 WM_SETCURSOR 转给父窗口，若不加这道判断，搜索框上的
        // I-beam 会被换成手型。
        if ((HWND)wParam == hwnd && LOWORD(lParam) == HTCLIENT) {
            POINT pt = {};
            if (::GetCursorPos(&pt) && ::ScreenToClient(hwnd, &pt)) {
                POINT cp = { pt.x - m_content.left, pt.y - m_content.top };
                IPage* p = CurrentPage();
                if (p && p->HandCursorAt(cp)) {
                    // IDC_HAND 是 MAKEINTRESOURCE(32649)，写死成宽字符版本
                    // 才能直接喂给 LoadCursorW（IDC_HAND 本身是 ANSI 资源名）。
                    static HCURSOR hand =
                        ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32649));
                    ::SetCursor(hand);
                    return TRUE;
                }
            }
        }
        break;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        bool captionDirty = false;   // 标题栏三键 / 渲染后端按钮
        bool navDirty     = false;   // 侧栏导航列表

        int btn = CaptionButtonAt(pt);
        if (btn != m_capHover) {
            m_capHover = btn;
            captionDirty = true;
        }
        bool onPaintBtn = HitTestPaintBackendButton(pt);
        if (onPaintBtn != m_paintBtnHover) {
            m_paintBtnHover = onPaintBtn;
            captionDirty = true;
        }
        bool tracking = (btn != 0) || onPaintBtn;

        if (m_opt.sidebar && pt.x < m_side.right && pt.y >= m.titleH) {
            if (m_nav.OnMouseMove(pt)) navDirty = true;
            tracking = true;
        } else {
            if (m_nav.OnMouseLeave()) navDirty = true;
        }
        // Keep the platform-free hit-test state in step with the mouse.
        m_shellCore.SetNavHover(m_shellCore.HitTestNav(Point{ pt.x, pt.y }));

        POINT cp = { pt.x - m_content.left, pt.y - m_content.top };
        bool contentDirty = false;
        if (IPage* p = CurrentPage())
            contentDirty = p->OnMouseMove(cp);

        if (contentDirty) {
            ::InvalidateRect(hwnd, nullptr, FALSE);
        } else if (captionDirty || navDirty) {
            // 只失效真正变化的那一块。整窗失效会连带把子控件（剪贴板
            // 列表）拖进一次重绘：LISTBOX 的 owner-draw 帧是"先擦成背
            // 景色，再逐行画回来"，十几次这样闪就会看成列表在抖。
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            if (captionDirty) {
                RECT rcCap = { 0, 0, rcClient.right, m.titleH };
                ::InvalidateRect(hwnd, &rcCap, FALSE);
            }
            if (navDirty && m_opt.sidebar) {
                RECT rcNav = { 0, m.titleH, m_side.right, rcClient.bottom };
                ::InvalidateRect(hwnd, &rcNav, FALSE);
            }
        }
        if (tracking) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        bool captionDirty = false, navDirty = false;
        if (m_capHover) { m_capHover = 0; captionDirty = true; }
        if (m_paintBtnHover) { m_paintBtnHover = false; captionDirty = true; }
        if (m_nav.OnMouseLeave()) navDirty = true;
        m_shellCore.SetNavHover(-1);
        bool contentDirty = false;
        if (IPage* p = CurrentPage()) contentDirty = p->OnMouseLeave();
        if (contentDirty) {
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        if (captionDirty) {
            RECT rcCap = { 0, 0, rcClient.right, m.titleH };
            ::InvalidateRect(hwnd, &rcCap, FALSE);
        }
        if (navDirty) {
            RECT rcNav = { 0, m.titleH, m_side.right, rcClient.bottom };
            ::InvalidateRect(hwnd, &rcNav, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };

        // 三键最优先：任何页面都必须响应
        // （曾因页面分支提前 return 导致默认页点三键无反应）
        int btn = CaptionButtonAt(pt);
        if (btn == 1) { PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0); return 0; }
        if (btn == 2) {
            PostMessageW(hwnd, WM_SYSCOMMAND,
                         IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
            return 0;
        }
        if (btn == 3) { PostMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0); return 0; }

        // 渲染后端切换：热切换，下一帧生效；首次点击顺带打开 HAL 外壳
        if (HitTestPaintBackendButton(pt)) {
            TogglePaintBackend();
            return 0;
        }

        // 侧栏导航
        if (m_opt.sidebar && pt.x < m_side.right && pt.y >= m.titleH) {
            int id = m_nav.HitTest(pt);
            if (id >= 0 && !m_nav.IsDisabled(id)) {
                if (FindPage(id)) SwitchPage(id);
                else if (m_navHandler) m_navHandler(id);
            }
            return 0;
        }

        // 页面
        POINT cp = { pt.x - m_content.left, pt.y - m_content.top };
        if (IPage* p = CurrentPage()) p->OnMouseDown(cp);
        return 0;
    }

    case WM_LBUTTONDBLCLK:
        return 0;

    case WM_LBUTTONUP: {
        // 自绘按钮的动作落在抬起：按下只改视觉，指针拖出热区再松手
        // 视为取消（与原生按钮一致）。页面自己决定要不要重绘。
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        POINT cp = { pt.x - m_content.left, pt.y - m_content.top };
        if (IPage* p = CurrentPage()) {
            if (p->OnMouseUp(cp))
                ::InvalidateRect(hwnd, &m_content, FALSE);
        }
        return 0;
    }

    case WM_PAINT: {
        if (PaintTraceEnabled()) {
            RECT upd = {};
            GetUpdateRect(hwnd, &upd, FALSE);
            uiDebugLog(L"[paint-req] page=%d update=%ld,%ld-%ld,%ld", m_pageId,
                       (long)upd.left, (long)upd.top,
                       (long)upd.right, (long)upd.bottom);
        }
        const long long tDc0 = QpcUs();
        // Skia composites a finished, whole-client DIB onto this DC, so a
        // back buffer here would be a second full-size copy of pixels
        // nobody reads: it cost 3.2 ms to create+fill and 1.0 ms to blit
        // out of a 21 ms frame. GDI paints onto the DC directly and does
        // need it, so only the Skia path skips the buffer.
        const bool skiaDirect =
            (NormalizeBackend(m_paintBackend) == "skia") &&
            PainterRegistry::Instance().Available("skia");
        PaintDC pdc(hwnd, !skiaDirect);
        const long long tDc1 = QpcUs();
        if (pdc.dc()) {
            Paint(pdc.dc());
            const long long tDc2 = QpcUs();
            pdc.Flush();
            const long long tDc3 = QpcUs();
            if (FrameProfileEnabled()) {
                FrameCost& c = Cost();
                c.dcBuild += tDc1 - tDc0;
                c.flush   += tDc3 - tDc2;
            }
        }
        // The exposed area is on screen now: stop allowing background
        // erases until the next size/show change asks for one.
        m_eraseCount = 0;
        return 0;
    }

    case WM_SIZE: {
        RECT nrc = {};
        GetClientRect(hwnd, &nrc);
        const int nw = nrc.right - nrc.left;
        const int nh = nrc.bottom - nrc.top;
        const int ow = m_lastClient.cx;
        const int oh = m_lastClient.cy;
        m_lastClient.cx = nw;
        m_lastClient.cy = nh;

        LayoutAll();

        // New pixels were just exposed: let the next erase fill THEM with
        // the theme background instead of leaving them uninitialised until
        // the (possibly slow) first frame lands. Only the strips that are
        // genuinely new are erased -- filling the whole client would wipe
        // content that is still valid, and on maximize (one frame = 140 ms)
        // that reads as the window blinking to a blank surface.
        m_eraseCount = 0;
        if (!m_paintedOnce || ow <= 0 || oh <= 0) {
            m_eraseRects[m_eraseCount++] = nrc;                    // 从没画过
        } else {
            if (nw > ow) {                                        // 右边新露出一条
                m_eraseRects[m_eraseCount].left   = ow;
                m_eraseRects[m_eraseCount].top    = 0;
                m_eraseRects[m_eraseCount].right  = nw;
                m_eraseRects[m_eraseCount].bottom = nh;
                ++m_eraseCount;
            }
            if (nh > oh) {                                        // 下边新露出一条
                m_eraseRects[m_eraseCount].left   = 0;
                m_eraseRects[m_eraseCount].top    = oh;
                m_eraseRects[m_eraseCount].right  = nw;
                m_eraseRects[m_eraseCount].bottom = nh;
                ++m_eraseCount;
            }
        }
        if (m_toastMgr) m_toastMgr->Reposition();
        ::InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOVE:
        // Toast 浮层是 owned popup，不会自动跟着 owner 平移。
        if (m_toastMgr) m_toastMgr->Reposition();
        return 0;

    case WM_SHOWWINDOW:
        // Same exposure problem on the very first show / after a hide.
        if (wParam) {
            RECT rcShow = {};
            GetClientRect(hwnd, &rcShow);
            m_eraseRects[0] = rcShow;
            m_eraseCount    = 1;
            if (rcShow.right <= rcShow.left) m_eraseCount = 0;
        }
        break;

    case WM_GETMINMAXINFO: {
        auto* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = m_opt.minWidth;
        mmi->ptMinTrackSize.y = m_opt.minHeight;
        return 0;
    }

    case WM_SETFOCUS:
        if (IPage* p = CurrentPage()) p->OnSetFocus();
        return 0;

    // ---- 控件配色 ----
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, Pal().text);
        SetBkColor(dc, Pal().surface);
        static HBRUSH   s_br      = nullptr;
        static COLORREF s_brColor = CLR_INVALID;
        if (!s_br || s_brColor != Pal().surface) {
            if (s_br) DeleteObject(s_br);
            s_brColor = Pal().surface;
            s_br      = CreateSolidBrush(s_brColor);
        }
        return (LRESULT)s_br;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wParam;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, Pal().textSub);
        static HBRUSH   s_br      = nullptr;
        static COLORREF s_brColor = CLR_INVALID;
        if (!s_br || s_brColor != Pal().bg) {
            if (s_br) DeleteObject(s_br);
            s_brColor = Pal().bg;
            s_br      = CreateSolidBrush(s_brColor);
        }
        return (LRESULT)s_br;
    }

    case WM_COMMAND: {
        int ctrlId = LOWORD(wParam), code = HIWORD(wParam);
        // 1) 控件自身回调优先（Button::OnClick / ListBox::onSelectChanged…）
        if (Widget* w = WidgetAt(ctrlId))
            if (w->HandleCommand(code)) return 0;
        // 2) 页面（含插件分发）
        if (IPage* p = CurrentPage())
            if (p->OnCommand(ctrlId, code)) return 0;
        break;
    }

    case WM_MEASUREITEM:
        if (Widget* w = WidgetAt((int)((MEASUREITEMSTRUCT*)lParam)->CtlID))
            if (w->HandleMeasureItem((MEASUREITEMSTRUCT*)lParam)) return TRUE;
        if (IPage* p = CurrentPage())
            if (p->OnMeasureItem((MEASUREITEMSTRUCT*)lParam)) return TRUE;
        break;

    case WM_DRAWITEM: {
        auto* dis = (DRAWITEMSTRUCT*)lParam;
        if (Widget* w = WidgetAt((int)dis->CtlID))
            if (w->HandleDrawItem(dis)) return TRUE;
        if (IPage* p = CurrentPage())
            if (p->OnDrawItem(dis)) return TRUE;
        break;
    }

    case WM_TIMER:
        if (IPage* p = CurrentPage())
            if (p->OnTimer((UINT_PTR)wParam)) return 0;
        break;

    case WM_CLOSE:
        if (m_closeGuard && !m_closeGuard()) return 0;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================
// 新增功能实现
// ============================================================
void Shell::SetSidebarWidth(int width)
{
    m_opt.sidebarW = width;
    LayoutAll();
    Invalidate();
}

void Shell::SetBreakpoints(std::vector<Breakpoint> bps)
{
    if (!m_responsive) m_responsive.reset(new ResponsiveManager(this));
    m_responsive->SetBreakpoints(std::move(bps));
}

void Shell::OnSize(int width, int height)
{
    if (m_responsive) m_responsive->OnSize(width, height);
}

void Shell::Tick(uint64_t now)
{
    (void)now;
    // 浮层自带 50ms 定时器；这里保留给外部驱动的调用方（幂等）。
    if (m_toastMgr) m_toastMgr->Tick();
}

::air::ui::ToastManager& Shell::ToastManager()
{
    if (!m_toastMgr) m_toastMgr.reset(new ::air::ui::ToastManager());
    // 浮层绘制要用外壳当前的字体，位置要跟着外壳的客户区走。
    m_toastMgr->m_owner = m_hwnd;
    m_toastMgr->m_fonts = &m_fonts;
    return *m_toastMgr;
}

} // namespace ui
} // namespace air
