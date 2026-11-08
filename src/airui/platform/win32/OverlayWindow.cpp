#include "airui/OverlayWindow.h"
#include "airui/Shell.h"
#include "airui/Canvas.h"
#include "airui/CanvasPainter.h"
#include "airui/ControlsCore.h"
#include "airui/ControlsExtra.h"
#include "airui/Interaction.h"
#include "airui/Theme.h"

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
    if (!m_hwnd || m_visible == visible) return;
    m_visible = visible;
    ShowWindow(m_hwnd, visible
        ? (m_activate ? SW_SHOW : SW_SHOWNOACTIVATE)
        : SW_HIDE);
    if (visible) Redraw();
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
    return r;
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
    InternalToast t;
    static_cast<Toast&>(t) = toast;
    t.startTime = GetTickCount64();
    m_toasts.push_back(std::move(t));
    while (m_toasts.size() > 3) {
        if (m_toasts.front().onDismiss) m_toasts.front().onDismiss();
        m_toasts.erase(m_toasts.begin());
    }

    EnsureLayer();
    if (!m_layer) return;
    m_layer->Show(true);
    m_layer->SetOpacity(0);
    m_layer->Move(ComputeRect());
    m_layer->Redraw();
    m_hoverIndex = -1;
    m_hoverClose = false;
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
    bool removed = false;
    for (auto it = m_toasts.begin(); it != m_toasts.end(); ) {
        if (now - it->startTime > (uint64_t)(std::max)(1, it->durationMs)) {
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
    const int64_t age = (int64_t)(now - last.startTime);
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
    if (m_layer) m_layer->Show(false);
}

void ToastManager::Reposition()
{
    if (m_layer && !m_toasts.empty()) {
        m_layer->Move(ComputeRect());
        m_layer->Redraw();
    }
}

// ============================================================
// 自绘模态对话框
// ============================================================
int RunModalDialog(HWND owner, const Fonts& fonts,
                   const std::wstring& caption,
                   const std::wstring& text, UINT flags)
{
    if (!owner) return IDCANCEL;

    RECT client{};
    GetClientRect(owner, &client);
    POINT origin{ 0, 0 };
    ClientToScreen(owner, &origin);
    RECT screen = { origin.x, origin.y,
                    origin.x + client.right, origin.y + client.bottom };

    OverlayWindow layer;
    if (!layer.Create(owner, screen, true)) return IDCANCEL;
    layer.SetMode(OverlayAlphaMode::Scrim);

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
        return ModalButtonAt(Rect(0, 0, client.right, client.bottom),
                             st, Mx(), Point{ pt.x, pt.y });
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

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    layer.Destroy();
    return result;
}

} // namespace ui
} // namespace air
