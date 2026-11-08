#include "airui/OverlayWindow.h"
#include "airui/Shell.h"
#include "airui/Canvas.h"
#include "airui/CanvasPainter.h"
#include "airui/ControlsCore.h"
#include "airui/ControlsExtra.h"
#include "airui/Interaction.h"
#include "airui/Theme.h"
#include "airui/Gfx.h"

#include <algorithm>
#include <vector>

namespace air {
namespace ui {

namespace {

const wchar_t* const kOverlayClass = L"AirUiOverlayWnd";

// 32bpp DIB 在内存里是 BGRA。底色用纯洋红作「这里应当透明」的标记，
// GDI 不写 alpha 通道，所以上屏前要按像素把 alpha 补齐。
const uint32_t kKeyPixel = 0x00FF00FFu;   // B=FF G=00 R=FF A=00

bool RegisterOverlayClass()
{
    static const bool registered = [] {
        WNDCLASSW wc{};
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = &OverlayWindow::WndProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kOverlayClass;
        return RegisterClassW(&wc) != 0;
    }();
    return registered;
}

POINT PointFromLParam(LPARAM lp)
{
    POINT pt = { (int)(short)LOWORD(lp), (int)(short)HIWORD(lp) };
    return pt;
}

// IDC_ARROW / IDC_HAND 是 MAKEINTRESOURCEA，直接塞给 LoadCursorW
// 会报 LPSTR → LPCWSTR。用宽字符资源 id 取系统光标。
HCURSOR ArrowCursor() { return LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); }
HCURSOR HandCursor()  { return LoadCursorW(nullptr, MAKEINTRESOURCEW(32649)); }

AlertKind ToAlertKind(ToastKind kind)
{
    switch (kind) {
    case ToastKind::Success: return AlertKind::Success;
    case ToastKind::Warning: return AlertKind::Warning;
    case ToastKind::Error:   return AlertKind::Error;
    default:                 return AlertKind::Info;
    }
}

// 浮层坐标都是屏幕物理像素。窗口可能跨屏、贴边或最大化（GetWindowRect
// 会带出屏幕外的隐形缩放边框），所以一律约束到所属显示器的工作区 /
// 全屏矩形，避免卡片或遮罩跑出可视范围。
RECT MonitorBounds(HWND owner, bool workArea)
{
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    HMONITOR mon = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfoW(mon, &mi)) {
        return { 0, 0, GetSystemMetrics(SM_CXSCREEN),
                       GetSystemMetrics(SM_CYSCREEN) };
    }
    return workArea ? mi.rcWork : mi.rcMonitor;
}

void ClampInside(RECT& r, const RECT& area, int pad)
{
    const int w = r.right - r.left;
    const int h = r.bottom - r.top;
    if (w <= area.right - area.left - 2 * pad) {
        if (r.left < area.left + pad) { r.left = area.left + pad; r.right = r.left + w; }
        if (r.right > area.right - pad) { r.right = area.right - pad; r.left = r.right - w; }
    }
    if (h <= area.bottom - area.top - 2 * pad) {
        if (r.top < area.top + pad) { r.top = area.top + pad; r.bottom = r.top + h; }
        if (r.bottom > area.bottom - pad) { r.bottom = area.bottom - pad; r.top = r.bottom - h; }
    }
}

// PaintMenu 用 IPainter::MeasureText(FontRole::Body)，底层就是
// gfx::MeasureWidth。这里用同一套度量算菜单宽度，避免文字被截断。
int MeasureMenuLabel(HDC dc, const Fonts& fonts, const std::wstring& s)
{
    return gfx::MeasureWidth(dc, fonts.Get(FontRole::Body), s);
}

// Toast 卡片几何：固定宽、两行高、右下角堆叠。绘制与命中共用这份
// 计算，避免「看着点得到、实际点不到」。
const int kToastW    = 320;
const int kToastH    = 64;
const int kToastGap  = 8;
const int kToastPad  = 16;
const int kStatusBar = 36;
// 详情面板底部「复制内容 / 备注 / 删除」三个按钮和列表行内操作钮
// 都在这一带。Toast 落在它们上方，短暂出现时不会挡住可点的东西。
const int kActionLane = 52;

} // namespace

// ============================================================
// OverlayWindow
// ============================================================
OverlayWindow::~OverlayWindow()
{
    Destroy();
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg,
                                        WPARAM wp, LPARAM lp)
{
    OverlayWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<OverlayWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        // 此刻 CreateWindowEx 还没返回，成员的窗口句柄仍是空的。
        // 必须在这里直接认领，否则 DefWindowProc 拿到空句柄会创建失败。
        return TRUE;
    }
    self = reinterpret_cast<OverlayWindow*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->Handle(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT OverlayWindow::Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT result = 0;
    if (m_events && m_events(msg, wp, lp, result)) return result;
    switch (msg) {
    case WM_TIMER:
    case WM_MOUSEMOVE:
        return 0;
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

bool OverlayWindow::Create(HWND owner, const RECT& screenRect, bool activate)
{
    if (!RegisterOverlayClass()) return false;
    DWORD ex = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    // Toast 不该抢走编辑框的焦点；模态对话框需要键盘，必须能激活。
    if (!activate) ex |= WS_EX_NOACTIVATE;

    const int w = (std::max)(1, (int)(screenRect.right - screenRect.left));
    const int h = (std::max)(1, (int)(screenRect.bottom - screenRect.top));
    m_x = screenRect.left;
    m_y = screenRect.top;
    m_activate = activate;
    m_hwnd = CreateWindowExW(ex, kOverlayClass, L"", WS_POPUP,
                             m_x, m_y, w, h,
                             owner, nullptr, GetModuleHandleW(nullptr), this);
    if (!m_hwnd) return false;
    m_visible = false;
    return true;
}

void OverlayWindow::Destroy()
{
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    ReleaseSurface();
    m_visible = false;
}

void OverlayWindow::Move(const RECT& screenRect)
{
    if (!m_hwnd) return;
    m_x = screenRect.left;
    m_y = screenRect.top;
    const int w = (std::max)(1, (int)(screenRect.right - screenRect.left));
    const int h = (std::max)(1, (int)(screenRect.bottom - screenRect.top));
    SetWindowPos(m_hwnd, nullptr, m_x, m_y, w, h,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void OverlayWindow::Show(bool visible)
{
    if (!m_hwnd) return;
    // owner 隐藏 / 最小化时 Windows 会自动把 owned popup 一起藏起来，
    // 但不会通知我们。若只信 m_visible 这个缓存，恢复后 Show(true)
    // 会被短路，Toast 从此再也不出现。以真实可见性为准。
    const bool reallyShown = IsWindowVisible(m_hwnd) != FALSE;
    m_visible = visible;
    if (!visible) {
        if (reallyShown) ShowWindow(m_hwnd, SW_HIDE);
        return;
    }
    if (!reallyShown) {
        ShowWindow(m_hwnd, m_activate ? SW_SHOW : SW_SHOWNOACTIVATE);
    }
    Redraw();
}

void OverlayWindow::StartTimer(UINT ms)
{
    if (m_hwnd) SetTimer(m_hwnd, 1, ms, nullptr);
}

bool OverlayWindow::EnsureSurface(int width, int height)
{
    if (m_dib && m_w == width && m_h == height) return true;
    ReleaseSurface();

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;   // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    m_dib = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS,
                             &m_bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!m_dib || !m_bits) return false;

    m_mem = CreateCompatibleDC(nullptr);
    m_old = SelectObject(m_mem, m_dib);
    m_w = width;
    m_h = height;
    return true;
}

void OverlayWindow::ReleaseSurface()
{
    if (m_mem) {
        if (m_old) SelectObject(m_mem, m_old);
        DeleteDC(m_mem);
    }
    if (m_dib) DeleteObject(m_dib);
    m_mem = nullptr;
    m_dib = nullptr;
    m_old = nullptr;
    m_bits = nullptr;
    m_w = m_h = 0;
}

void OverlayWindow::Redraw()
{
    if (!m_hwnd || !m_visible) return;
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0 || !EnsureSurface(w, h)) return;

    uint32_t* px = static_cast<uint32_t*>(m_bits);
    std::fill_n(px, (size_t)w * (size_t)h, kKeyPixel);

    if (m_paint) m_paint(m_mem, w, h);

    // GDI 只写颜色通道。透明标记 → 全透明 / 半透明遮罩；
    // 其余像素 → 不透明。AC_SRC_ALPHA 要求预乘，遮罩是纯黑，
    // 不透明像素 alpha=255，所以两种情形都天然满足。
    const uint32_t scrim = (90u << 24);
    for (size_t i = 0, n = (size_t)w * (size_t)h; i < n; ++i) {
        if ((px[i] & 0x00FFFFFFu) == (kKeyPixel & 0x00FFFFFFu))
            px[i] = (m_mode == OverlayAlphaMode::Scrim) ? scrim : 0u;
        else
            px[i] = 0xFF000000u | (px[i] & 0x00FFFFFFu);
    }

    POINT src{ 0, 0 };
    POINT dst{ m_x, m_y };
    SIZE  size{ w, h };
    BLENDFUNCTION bf{};
    bf.BlendOp             = AC_SRC_OVER;
    bf.SourceConstantAlpha = m_opacity;
    bf.AlphaFormat         = AC_SRC_ALPHA;
    UpdateLayeredWindow(m_hwnd, nullptr, &dst, &size,
                        m_mem, &src, 0, &bf, ULW_ALPHA);
}

// ============================================================
// ToastManager
// ============================================================
ToastManager::~ToastManager() = default;

RECT ToastManager::ComputeRect() const
{
    RECT client{};
    GetClientRect(m_owner, &client);
    POINT origin{ 0, 0 };
    ClientToScreen(m_owner, &origin);

    const int n = (int)m_toasts.size();
    const int totalH = n > 0 ? n * kToastH + (n - 1) * kToastGap : 1;
    int x = client.right - kToastPad - kToastW;
    int y = client.bottom - kStatusBar - kToastPad - kActionLane - totalH;
    if (x < kToastPad) x = kToastPad;
    if (y < kToastPad) y = kToastPad;

    RECT r = { origin.x + x, origin.y + y,
               origin.x + x + kToastW, origin.y + y + totalH };
    // 主窗口可能贴着屏幕右缘 / 下缘，甚至跨屏：把整叠卡片约束进
    // 所属显示器的工作区（排除任务栏），保证始终完整可见。
    ClampInside(r, MonitorBounds(m_owner, true), kToastPad);
    return r;
}

bool ToastManager::OwnerVisible() const
{
    return m_owner && IsWindowVisible(m_owner) && !IsIconic(m_owner);
}

void ToastManager::AdvanceClock(uint64_t now, bool ownerVisible)
{
    if (m_lastTick) {
        uint64_t dt = now - m_lastTick;
        // 系统休眠 / 调试断点后的大跳变不该让一屏 Toast 瞬间过期。
        if (dt > 1000) dt = 50;
        if (m_wasVisible && ownerVisible) m_visibleClock += dt;
    }
    m_lastTick   = now;
    m_wasVisible = ownerVisible;
}

void ToastManager::Resume()
{
    if (!m_suspended || !m_layer) return;
    m_suspended   = false;
    m_lastOpacity = -1;      // 强制下一帧重算淡入，不沿用挂起前的透明度
    m_layer->Show(true);
    m_layer->Move(ComputeRect());
    m_layer->Redraw();
}

RECT ToastManager::CardRect(int index) const
{
    const int top = index * (kToastH + kToastGap);
    RECT r = { 0, top, kToastW, top + kToastH };
    return r;
}

void ToastManager::EnsureLayer()
{
    if (m_layer || !m_owner || !m_fonts) return;
    std::unique_ptr<OverlayWindow> layer(new OverlayWindow());
    if (!layer->Create(m_owner, ComputeRect(), false)) return;
    layer->SetMode(OverlayAlphaMode::Transparent);
    layer->SetPaint([this](HDC dc, int w, int h) { Paint(dc, w, h); });
    layer->SetEvents([this](UINT msg, WPARAM wp, LPARAM lp, LRESULT& res) {
        return OnEvent(msg, wp, lp, res);
    });
    layer->StartTimer(50);
    m_layer = std::move(layer);
    m_lastOpacity = 0;
}

void ToastManager::Show(const Toast& toast)
{
    if (!m_owner || !m_fonts) return;
    // 自绘模态框会 EnableWindow(owner, FALSE)，但主窗口的定时器仍在
    // 跑。此时弹出 Toast 会浮在遮罩/对话框上，且抢不到焦点关不掉。
    // 这些都是即时操作反馈，模态期间直接丢弃比事后补一条过期提示好。
    if (!IsWindowEnabled(m_owner)) return;

    const bool vis = OwnerVisible();
    AdvanceClock(GetTickCount64(), vis);
    InternalToast t;
    static_cast<Toast&>(t) = toast;
    t.startTime = m_visibleClock;   // 显示时钟：隐藏期间不会流逝
    m_toasts.push_back(std::move(t));
    while (m_toasts.size() > 3) {
        if (m_toasts.front().onDismiss) m_toasts.front().onDismiss();
        m_toasts.erase(m_toasts.begin());
    }

    EnsureLayer();
    if (!m_layer) return;
    // 主窗口被隐藏（Ctrl+Shift+V 收起）或最小化时，只入队不显示；
    // 窗口回来后由显示时钟自然开始计时并淡入。
    if (!vis) {
        m_suspended = true;
        m_layer->Show(false);
        return;
    }
    m_suspended = false;
    m_layer->Show(true);
    m_layer->SetOpacity(0);
    m_layer->Move(ComputeRect());
    m_layer->Redraw();
    m_lastOpacity = 0;
    m_hoverIndex  = -1;
    m_hoverClose  = false;
}

void ToastManager::Show(const std::wstring& title,
                        const std::wstring& message, ToastKind kind)
{
    Toast t;
    t.title   = title;
    t.message = message;
    t.kind    = kind;
    Show(t);
}

void ToastManager::Paint(HDC dc, int width, int height)
{
    (void)width;
    (void)height;
    if (!m_fonts) return;
    Canvas canvas(dc, *m_fonts);
    CanvasPainter painter(canvas);

    for (size_t i = 0; i < m_toasts.size(); ++i) {
        const RECT rc = CardRect((int)i);
        ToastState st;
        st.kind          = ToAlertKind(m_toasts[i].kind);
        st.title         = m_toasts[i].title;
        st.message       = m_toasts[i].message;
        st.closable      = m_toasts[i].closable;
        st.hoveredClose  = ((int)i == m_hoverIndex && m_hoverClose);
        st.base.hover    = ((int)i == m_hoverIndex);
        PaintToast(painter, Rect(rc.left, rc.top, rc.right, rc.bottom),
                   st, Pal(), Mx());
    }
}

void ToastManager::HitTest(POINT pt, int& index, bool& onClose) const
{
    index = -1;
    onClose = false;
    for (size_t i = 0; i < m_toasts.size(); ++i) {
        const RECT rc = CardRect((int)i);
        if (pt.x < rc.left || pt.x >= rc.right ||
            pt.y < rc.top  || pt.y >= rc.bottom) continue;
        index = (int)i;
        ToastState st;
        st.closable = m_toasts[i].closable;
        onClose = ToastCloseHit(Rect(rc.left, rc.top, rc.right, rc.bottom),
                                st, Mx(), Point{ pt.x, pt.y });
        return;
    }
}

bool ToastManager::OnEvent(UINT msg, WPARAM wp, LPARAM lp, LRESULT& res)
{
    (void)wp;
    switch (msg) {
    case WM_TIMER:
        Tick();
        res = 0;
        return true;

    case WM_MOUSEMOVE: {
        int idx = -1;
        bool close = false;
        HitTest(PointFromLParam(lp), idx, close);
        if (idx != m_hoverIndex || close != m_hoverClose) {
            m_hoverIndex = idx;
            m_hoverClose = close;
            if (m_layer) m_layer->Redraw();
        }
        TRACKMOUSEEVENT tme{};
        tme.cbSize      = sizeof(tme);
        tme.dwFlags     = TME_LEAVE;
        tme.hwndTrack   = m_layer ? m_layer->Native() : nullptr;
        TrackMouseEvent(&tme);
        res = 0;
        return true;
    }

    case WM_MOUSELEAVE:
        if (m_hoverIndex != -1 || m_hoverClose) {
            m_hoverIndex = -1;
            m_hoverClose = false;
            if (m_layer) m_layer->Redraw();
        }
        res = 0;
        return true;

    case WM_LBUTTONUP: {
        int idx = -1;
        bool close = false;
        HitTest(PointFromLParam(lp), idx, close);
        if (close && idx >= 0 && idx < (int)m_toasts.size()) {
            if (m_toasts[idx].onDismiss) m_toasts[idx].onDismiss();
            m_toasts.erase(m_toasts.begin() + idx);
            m_hoverIndex = -1;
            m_hoverClose = false;
            if (m_layer) {
                if (m_toasts.empty()) m_layer->Show(false);
                else { m_layer->Move(ComputeRect()); m_layer->Redraw(); }
            }
        }
        res = 0;
        return true;
    }

    case WM_SETCURSOR:
        SetCursor((m_hoverIndex >= 0 && m_hoverClose)
                      ? HandCursor() : ArrowCursor());
        res = TRUE;
        return true;

    case WM_NCHITTEST:
        res = HTCLIENT;
        return true;

    default:
        return false;
    }
}

void ToastManager::Tick()
{
    if (!m_layer) return;
    const uint64_t now = GetTickCount64();
    const bool vis = OwnerVisible();
    AdvanceClock(now, vis);

    if (!vis) {
        // 窗口不在屏幕上：藏起浮层，显示时钟随之冻结。
        if (!m_suspended) { m_suspended = true; m_layer->Show(false); }
        return;
    }
    if (m_suspended) Resume();   // 继续往下重算淡入，不另起一帧

    bool removed = false;
    for (auto it = m_toasts.begin(); it != m_toasts.end(); ) {
        if (m_visibleClock - it->startTime >
            (uint64_t)(std::max)(1, it->durationMs)) {
            if (it->onDismiss) it->onDismiss();
            it = m_toasts.erase(it);
            removed = true;
        } else ++it;
    }

    if (m_toasts.empty()) {
        m_layer->Show(false);
        return;
    }

    // 主窗口被拖动 / 缩放后，浮层要跟着走。
    RECT want = ComputeRect();
    RECT cur{};
    GetWindowRect(m_layer->Native(), &cur);
    if (want.left != cur.left || want.top != cur.top ||
        want.right != cur.right || want.bottom != cur.bottom) {
        m_layer->Move(want);
        removed = true;
    }

    // 淡入 150ms / 淡出 250ms，以最新一条为准（通常只有一条）。
    const InternalToast& last = m_toasts.back();
    const int64_t age = (int64_t)(m_visibleClock - last.startTime);
    const int64_t dur = (int64_t)(std::max)(1, last.durationMs);
    int fade = 255;
    if (age < 150) fade = (int)(age * 255 / 150);
    else if (age > dur - 250) fade = (int)((dur - age) * 255 / 250);
    fade = (std::max)(0, (std::min)(255, fade));

    if (fade != m_lastOpacity) {
        m_layer->SetOpacity((BYTE)fade);
        removed = true;
        m_lastOpacity = fade;
    }
    if (removed) m_layer->Redraw();
}

void ToastManager::CloseAll()
{
    for (auto& t : m_toasts)
        if (t.onDismiss) t.onDismiss();
    m_toasts.clear();
    m_suspended  = false;
    m_lastOpacity = 0;
    if (m_layer) m_layer->Show(false);
}

void ToastManager::Reposition()
{
    if (!m_layer || m_toasts.empty()) return;
    const bool vis = OwnerVisible();
    AdvanceClock(GetTickCount64(), vis);
    if (!vis) {
        if (!m_suspended) { m_suspended = true; m_layer->Show(false); }
        return;
    }
    // WM_SIZE / WM_SHOWWINDOW 一到就恢复，不必等 50ms 的浮层定时器。
    if (m_suspended) Resume();
    m_layer->Move(ComputeRect());
    m_layer->Redraw();
}

// ============================================================
// 自绘模态对话框
// ============================================================
int RunModalDialog(HWND owner, const Fonts& fonts,
                   const std::wstring& caption,
                   const std::wstring& text, UINT flags)
{
    if (!owner) return IDCANCEL;

    // 遮罩要盖住整个窗口（含标题栏和系统按钮），不能只盖客户区。
    // 最大化时 GetWindowRect 会带出屏幕外的隐形缩放边框，用所属
    // 显示器的全屏矩形裁一下，卡片才不会被推偏。
    RECT screen{};
    GetWindowRect(owner, &screen);
    const RECT mon = MonitorBounds(owner, false);
    screen.left   = (std::max)(screen.left,   mon.left);
    screen.top    = (std::max)(screen.top,    mon.top);
    screen.right  = (std::min)(screen.right,  mon.right);
    screen.bottom = (std::min)(screen.bottom, mon.bottom);
    if (screen.right <= screen.left || screen.bottom <= screen.top)
        return IDCANCEL;
    // 命中与绘制共用浮层自身尺寸：PaintModal 的坐标系原点就是浮层
    // 客户区左上角，不再依赖主窗口的 client rect。
    const Rect layerRc(0, 0, screen.right - screen.left,
                             screen.bottom - screen.top);

    OverlayWindow layer;
    if (!layer.Create(owner, screen, true)) return IDCANCEL;
    layer.SetMode(OverlayAlphaMode::Scrim);
    const bool ownerWasEnabled = IsWindowEnabled(owner) != FALSE;

    const UINT kind = flags & 0x0F;
    const bool yesNo   = (kind == MB_YESNO || kind == MB_YESNOCANCEL);
    const bool single  = (kind == MB_OK);

    ModalState st;
    st.title        = caption;
    st.body         = text;
    st.confirmText  = yesNo ? L"是" : L"确定";
    st.cancelText   = yesNo ? L"否" : L"取消";
    st.singleButton = single;
    st.scrim        = false;   // 遮罩由浮层的透明底色负责
    st.focused      = 1;       // Enter 默认执行主按钮

    HotState hot;
    int  result = IDCANCEL;
    bool done   = false;

    auto finish = [&](int button) {
        if (single)               result = IDOK;
        else if (yesNo)           result = (button == 1) ? IDYES : IDNO;
        else                      result = (button == 1) ? IDOK : IDCANCEL;
        done = true;
    };

    auto hitAt = [&](LPARAM lp) {
        const POINT pt = PointFromLParam(lp);
        return ModalButtonAt(layerRc, st, Mx(), Point{ pt.x, pt.y });
    };

    layer.SetPaint([&](HDC dc, int w, int h) {
        Canvas canvas(dc, fonts);
        CanvasPainter painter(canvas);
        ModalState draw = st;
        draw.hovered = hot.HoverId();
        draw.pressed = (hot.PressId() == hot.HoverId()) ? hot.PressId() : 0;
        PaintModal(painter, Rect(0, 0, w, h), draw, Pal(), Mx());
    });

    layer.SetEvents([&](UINT msg, WPARAM wp, LPARAM lp, LRESULT& res) -> bool {
        switch (msg) {
        case WM_MOUSEMOVE: {
            if (hot.Move(hitAt(lp))) layer.Redraw();
            TRACKMOUSEEVENT tme{};
            tme.cbSize    = sizeof(tme);
            tme.dwFlags   = TME_LEAVE;
            tme.hwndTrack = layer.Native();
            TrackMouseEvent(&tme);
            res = 0;
            return true;
        }
        case WM_MOUSELEAVE:
            if (hot.Leave()) layer.Redraw();
            res = 0;
            return true;

        case WM_LBUTTONDOWN:
            if (hot.Down(hitAt(lp))) layer.Redraw();
            res = 0;
            return true;

        case WM_LBUTTONUP: {
            const int hit = hitAt(lp);
            if (hot.Release(hit)) finish(hit);
            else layer.Redraw();
            res = 0;
            return true;
        }

        case WM_KEYDOWN:
            switch (wp) {
            case VK_RETURN:
            case VK_SPACE:
                finish(st.focused ? st.focused : 1);
                break;
            case VK_ESCAPE:
                finish(single ? 1 : 2);
                break;
            case VK_TAB:
            case VK_LEFT:
            case VK_RIGHT:
                if (!single) {
                    st.focused = (st.focused == 1) ? 2 : 1;
                    layer.Redraw();
                }
                break;
            default:
                return false;
            }
            res = 0;
            return true;

        case WM_SETCURSOR:
            SetCursor(hot.HoverId() ? HandCursor() : ArrowCursor());
            res = TRUE;
            return true;

        case WM_CLOSE:
            finish(single ? 1 : 2);
            res = 0;
            return true;

        case WM_NCHITTEST:
            res = HTCLIENT;
            return true;

        default:
            return false;
        }
    });

    EnableWindow(owner, FALSE);
    layer.Show(true);
    SetForegroundWindow(layer.Native());
    SetActiveWindow(layer.Native());
    SetFocus(layer.Native());

    // 局部消息循环：主窗口的定时器等消息照常分发，但输入被禁用，
    // 遮罩又盖在最上层，点击不会漏到下面的控件。
    MSG msg{};
    while (!done) {
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0) {                 // WM_QUIT 要还给主循环
            PostQuitMessage((int)msg.wParam);
            break;
        }
        if (got < 0) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 只在原本启用时恢复：若调用方（或上一层模态）已经禁用了 owner，
    // 无条件 TRUE 会把不该放开的输入放开。窗口被隐藏 / 最小化时也
    // 不要硬抢前台，否则会把用户当前正在用的程序挤掉。
    if (ownerWasEnabled) EnableWindow(owner, TRUE);
    if (IsWindowVisible(owner) && !IsIconic(owner))
        SetForegroundWindow(owner);
    layer.Destroy();
    return result;
}

// ============================================================
// 自绘上下文菜单
// ============================================================
int RunPopupMenu(HWND owner, const Fonts& fonts, POINT anchor,
                 const std::vector<OverlayMenuItem>& items)
{
    if (!owner || items.empty()) return 0;

    MenuState st;
    for (const auto& src : items) {
        MenuItemState mi;
        mi.text      = src.label;
        mi.separator = src.separator;
        mi.disabled  = src.disabled;
        st.items.push_back(std::move(mi));
    }

    // 宽度公式与 core 的 PaintContextMenu / Menu::Paint 保持一致：
    // pad*2 + 图标列 + gap + 文本 + gap + 快捷键列，再加 1px 描边。
    const Metrics& mx = Mx();
    const int pad       = mx.unit;
    const int gap       = mx.unit;
    const int glyphW    = (std::max)(8, mx.rowH - mx.unit);
    const int shortcutW = mx.rowH * 2;

    HDC dc = GetDC(nullptr);
    int textW = 0;
    for (const auto& mi : st.items)
        if (!mi.separator)
            textW = (std::max)(textW, MeasureMenuLabel(dc, fonts, mi.text));
    ReleaseDC(nullptr, dc);

    int w = pad * 2 + glyphW + gap + textW + gap + shortcutW + 2;
    w = (std::max)(w, mx.rowH * 6);
    const int h = MenuContentHeight(st, mx) + 2;

    // 右下空间不够就向左 / 向上翻转，最后约束进显示器工作区。
    const RECT area = MonitorBounds(owner, true);
    int x = anchor.x;
    int y = anchor.y;
    if (x + w > area.right - 2)  x = anchor.x - w;
    if (y + h > area.bottom - 2) y = anchor.y - h;
    RECT screen = { x, y, x + w, y + h };
    ClampInside(screen, area, 2);

    OverlayWindow layer;
    if (!layer.Create(owner, screen, true)) return 0;
    layer.SetMode(OverlayAlphaMode::Transparent);

    const Rect rc(0, 0, w, h);
    int  hovered = -1;
    int  result  = 0;
    bool done    = false;

    // -2 = 点在浮层外（取消），-1 = 浮层内但不可选，>=0 = 项下标。
    auto hitAt = [&](LPARAM lp) -> int {
        const POINT pt = PointFromLParam(lp);
        if (pt.x < 0 || pt.y < 0 || pt.x >= w || pt.y >= h) return -2;
        const int idx = MenuItemAt(rc, st, mx, Point{ pt.x, pt.y });
        if (idx < 0 || st.items[(size_t)idx].separator ||
            st.items[(size_t)idx].disabled) return -1;
        return idx;
    };

    layer.SetPaint([&](HDC paintDc, int pw, int ph) {
        Canvas canvas(paintDc, fonts);
        CanvasPainter painter(canvas);
        MenuState draw = st;
        draw.hovered  = hovered;
        draw.selected = hovered;
        PaintMenu(painter, Rect(0, 0, pw, ph), draw, Pal(), mx);
    });

    auto moveSelection = [&](int dir) {
        const int n = (int)st.items.size();
        int next = hovered;
        for (int i = 0; i < n; ++i) {
            next += dir;
            if (next < 0)  next = n - 1;
            if (next >= n) next = 0;
            if (!st.items[(size_t)next].separator &&
                !st.items[(size_t)next].disabled) break;
        }
        if (next != hovered) { hovered = next; layer.Redraw(); }
    };

    layer.SetEvents([&](UINT msg, WPARAM wp, LPARAM lp, LRESULT& res) -> bool {
        switch (msg) {
        case WM_MOUSEMOVE: {
            const int hit = hitAt(lp);
            const int nh  = (hit >= 0) ? hit : -1;
            if (nh != hovered) { hovered = nh; layer.Redraw(); }
            TRACKMOUSEEVENT tme{};
            tme.cbSize    = sizeof(tme);
            tme.dwFlags   = TME_LEAVE;
            tme.hwndTrack = layer.Native();
            TrackMouseEvent(&tme);
            res = 0;
            return true;
        }
        case WM_MOUSELEAVE:
            if (hovered != -1) { hovered = -1; layer.Redraw(); }
            res = 0;
            return true;

        case WM_LBUTTONDOWN:
            if (hitAt(lp) == -2) { result = 0; done = true; }
            res = 0;
            return true;

        case WM_LBUTTONUP: {
            const int hit = hitAt(lp);
            if (hit >= 0) result = items[(size_t)hit].id;
            done = true;
            res = 0;
            return true;
        }

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
            if (hitAt(lp) == -2) { result = 0; done = true; }
            res = 0;
            return true;

        case WM_KEYDOWN:
            switch (wp) {
            case VK_ESCAPE:
                result = 0; done = true; break;
            case VK_RETURN:
            case VK_SPACE:
                if (hovered >= 0) result = items[(size_t)hovered].id;
                done = true; break;
            case VK_UP:   moveSelection(-1); break;
            case VK_DOWN: moveSelection(1);  break;
            default: return false;
            }
            res = 0;
            return true;

        case WM_SETCURSOR:
            SetCursor(hovered >= 0 ? HandCursor() : ArrowCursor());
            res = TRUE;
            return true;

        case WM_CANCELMODE:
            result = 0; done = true;
            res = 0;
            return true;

        case WM_NCHITTEST:
            res = HTCLIENT;
            return true;

        default:
            return false;
        }
    });

    const HWND prevFocus = GetFocus();
    layer.Show(true);
    SetForegroundWindow(layer.Native());
    SetActiveWindow(layer.Native());
    SetFocus(layer.Native());
    SetCapture(layer.Native());

    // 与 TrackPopupMenu 一样是阻塞式：拿到命令 id 或取消才返回。
    // 捕获鼠标保证点在菜单外也只会关掉菜单，不会漏给下面的控件。
    MSG msg{};
    while (!done) {
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0) { PostQuitMessage((int)msg.wParam); break; }
        if (got < 0) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ReleaseCapture();
    if (prevFocus && IsWindow(prevFocus)) SetFocus(prevFocus);
    layer.Destroy();
    return result;
}

} // namespace ui
} // namespace air
