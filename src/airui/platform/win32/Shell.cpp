#include "airui/Shell.h"
#include "airui/Gfx.h"
#include "airui/Responsive.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace air {
namespace ui {

// 临时诊断（发布前移除）
#if 0
static void uiDebugLog(const wchar_t* fmt, ...)
{
    FILE* f = nullptr;
    _wfopen_s(&f, L"C:\\Users\\Administrator\\AppData\\Local\\Temp\\airui_dbg.txt", L"a");
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfwprintf_s(f, fmt, ap); fwprintf_s(f, L"\n");
    va_end(ap); fclose(f);
}
#else
static inline void uiDebugLog(const wchar_t*, ...) {}
#endif

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
Shell::Shell() = default;

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
    if (m_opt.className.empty()) m_opt.className = L"AirUiShellWnd";
    if (m_opt.appName.empty())   m_opt.appName   = L"App";

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
    // 实测会随机吞掉输入框首次显示时的占位提示
    m_hwnd = CreateWindowExW(WS_CLIPCHILDREN,
                             m_opt.className.c_str(), m_opt.appName.c_str(),
                             WS_OVERLAPPEDWINDOW,
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

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
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

    if (IPage* prev = CurrentPage()) {
        prev->OnShow(false);
        InvalidateContent();
    }

    m_pageId = id;
    m_nav.SetActive(id);

    // 挂起重绘：布局 → 显示 → 一次性上屏，避免切页闪白
    SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);
    LayoutAll();
    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);

    next->OnShow(true);
    Invalidate();
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
}

void Shell::SetNavGroups(std::vector<NavGroup> groups)
{
    m_navGroups = std::move(groups);
    m_nav.SetGroups(m_navGroups);
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
    m_captionBadge   = text;
    m_captionBadgeOk = ok;
    if (m_hwnd) InvalidateTitle();
}

void Shell::SetCaptionSubtitle(const std::wstring& text)
{
    m_captionSubtitle = text;
    if (m_hwnd) InvalidateTitle();
}

void Shell::SetStatus(const std::wstring& text, bool ok)
{
    m_statusText = text;
    m_statusOk   = ok;
    if (m_hwnd) Invalidate();
}

void Shell::SetStatusExtra(const std::wstring& text)
{
    m_statusExtra = text;
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
    if (m_hwnd) SendMessageW(m_hwnd, WM_SETREDRAW, on ? TRUE : FALSE, 0);
}

int Shell::ShowMessageBox(const std::wstring& text, const std::wstring& caption,
                          UINT flags)
{
    return MessageBoxW(m_hwnd, text.c_str(), caption.c_str(), flags);
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

    if (IPage* p = CurrentPage()) p->OnLayout(m_content);
}

// ============================================================
// 绘制
// ============================================================
void Shell::Paint(HDC dc)
{
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    gfx::Fill(dc, rc, Pal().bg);
    DrawCaption(dc, rc);

    if (m_opt.sidebar) {
        m_nav.Paint(dc, m_fonts);
        DrawSidebarFooter(dc);
    }

    if (IPage* p = CurrentPage()) p->OnPaint(dc, m_content);

    if (m_opt.statusBar) DrawStatusBar(dc, rc);
}

// ------------------------------------------------------------
// 标题栏：Logo + 应用名 + 竖线 + 副标题 + 自定义区 + 状态胶囊 + 三键
// ------------------------------------------------------------
void Shell::DrawCaption(HDC dc, const RECT& rc)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();

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

    // 应用名
    int nameRight = rc.right - m.capBtnW * 3 - 24;
    RECT rcName = { 52, 0, nameRight, m.titleH };
    gfx::Text(dc, m_fonts.Get(FontRole::Title), p.text, rcName, m_opt.appName,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    int nameW = gfx::MeasureWidth(dc, m_fonts.Get(FontRole::Title), m_opt.appName);
    int dx = 52 + nameW + 10;

    gfx::VLine(dc, dx, m.titleH / 2 - 8, m.titleH / 2 + 8, p.split);

    // 副标题
    std::wstring sub = m_captionSubtitle;
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
    int rightLimit = rc.right - m.capBtnW * 3 - 12;
    if (!m_captionBadge.empty()) {
        gfx::StatusTag(dc, rightLimit, m.titleH / 2,
                       m_captionBadge.c_str(),
                       m_fonts.Get(FontRole::Meta),
                       m_captionBadgeOk ? p.okSoft : p.errorSoft,
                       m_captionBadgeOk ? p.okBorder : p.errorBorder,
                       m_captionBadgeOk ? p.okText : p.errorText,
                       m_captionBadgeOk ? p.ok : p.error,
                       24);
    }

    // 三键
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

// ------------------------------------------------------------
// 状态栏：左「状态点 + 文本 + 副信息」，右 kbd 键位提示
// ------------------------------------------------------------
void Shell::DrawStatusBar(HDC dc, const RECT& rc)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();
    (void)rc;

    gfx::Fill(dc, m_status, p.surface);
    gfx::HLine(dc, 0, m_status.right, m_status.top, p.split);

    int cy = m_status.top + m.statusH / 2;

    // 状态点
    gfx::Dot(dc, 17, cy, 6, m_statusOk ? p.ok : p.error);

    // 状态文本
    std::wstring svc = m_statusText;
    gfx::Text(dc, m_fonts.Get(FontRole::Meta),
              m_statusOk ? p.okText : p.errorText,
              RECT{ 26, m_status.top, m_status.right - 400, m_status.bottom },
              svc, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    // 副信息
    if (!m_statusExtra.empty()) {
        int w = gfx::MeasureWidth(dc, m_fonts.Get(FontRole::Meta), svc);
        int x2 = 26 + w + 18;
        gfx::Text(dc, m_fonts.Get(FontRole::Meta), RGB(143, 149, 158),
                  RECT{ x2, m_status.top, m_status.right - 300, m_status.bottom },
                  m_statusExtra,
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

    case WM_ERASEBKGND:
        return 1;   // 全自绘，不擦背景（消除切页闪白）

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

        // 1) 标题栏三键优先：按客户区点击处理
        if (CaptionButtonAt(pt)) return HTCLIENT;

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

    case WM_MOUSEMOVE: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        bool needRepaint = false;

        int btn = CaptionButtonAt(pt);
        if (btn != m_capHover) {
            m_capHover = btn;
            needRepaint = true;
        }
        bool tracking = (btn != 0);

        if (m_opt.sidebar && pt.x < m_side.right && pt.y >= m.titleH) {
            if (m_nav.OnMouseMove(pt)) needRepaint = true;
            tracking = true;
        } else {
            if (m_nav.OnMouseLeave()) needRepaint = true;
        }

        POINT cp = { pt.x - m_content.left, pt.y - m_content.top };
        if (IPage* p = CurrentPage())
            if (p->OnMouseMove(cp)) needRepaint = true;

        if (needRepaint) ::InvalidateRect(hwnd, nullptr, FALSE);
        if (tracking) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        bool dirty = false;
        if (m_capHover) { m_capHover = 0; dirty = true; }
        if (m_nav.OnMouseLeave()) dirty = true;
        if (IPage* p = CurrentPage())
            if (p->OnMouseLeave()) dirty = true;
        if (dirty) ::InvalidateRect(hwnd, nullptr, FALSE);
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

    case WM_PAINT: {
        PaintDC pdc(hwnd);
        if (pdc.dc()) {
            Paint(pdc.dc());
            pdc.Flush();
        }
        return 0;
    }

    case WM_SIZE:
        LayoutAll();
        ::InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

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
        static HBRUSH s_br = nullptr;
        if (!s_br) s_br = CreateSolidBrush(Pal().surface);
        return (LRESULT)s_br;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wParam;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, Pal().textSub);
        static HBRUSH s_br = nullptr;
        if (!s_br) s_br = CreateSolidBrush(Pal().bg);
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
    if (m_toastMgr) m_toastMgr->Tick();
}

::air::ui::ToastManager& Shell::ToastManager()
{
    if (!m_toastMgr) m_toastMgr.reset(new ::air::ui::ToastManager());
    m_toastMgr->m_invalidateCb = [this] { Invalidate(); };
    return *m_toastMgr;
}

// ============================================================
// ToastManager 实现
// ============================================================
void ToastManager::Show(const Toast& toast)
{
    InternalToast t;
    t.text = toast.text;
    t.ok = toast.ok;
    t.durationMs = toast.durationMs;
    t.onDismiss = toast.onDismiss;
    t.startTime = GetTickCount64();
    m_toasts.push_back(std::move(t));
    InvalidateAll();
}

void ToastManager::Paint(Canvas& g, const RECT& viewport)
{
    const auto& p = Pal();
    const auto& m = Mx();
    int y = viewport.bottom - m.unit;
    for (auto it = m_toasts.rbegin(); it != m_toasts.rend(); ++it) {
        auto& t = *it;
        int w = g.MeasureWidth(t.text, FontRole::Body) + 32;
        RECT r = { viewport.right - w - m.unit, y - 48, viewport.right - m.unit, y };
        COLORREF bg = t.ok ? p.okSoft : p.errorSoft;
        COLORREF tx = t.ok ? p.okText : p.errorText;
        g.FillRound(r, bg, m.radiusMd);
        g.Stroke(r, t.ok ? p.okBorder : p.errorBorder, m.radiusMd);
        g.TextRowCenter(r, t.text, FontRole::Body, tx);
        y -= 56;
    }
}

void ToastManager::Tick()
{
    auto now = GetTickCount64();
    bool changed = false;
    for (auto it = m_toasts.begin(); it != m_toasts.end(); ) {
        if (now - it->startTime > (uint64_t)it->durationMs) {
            if (it->onDismiss) it->onDismiss();
            it = m_toasts.erase(it);
            changed = true;
        } else ++it;
    }
    if (changed) InvalidateAll();
}

} // namespace ui
} // namespace air
