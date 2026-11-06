#pragma once
// ============================================================
// AirUI - Win32Window.h
// IWindow implementation for the Win32 backend.
//
// Wraps an existing HWND, forwards native messages to the core
// through std::function callbacks. The core stays platform-free;
// this class is the only place that understands Win32 messages.
//
// Phase 1+ : full event injection. Every event the core cares
// about is surfaced here (mouse, buttons, wheel, keyboard, focus,
// DPI, caption chrome) so a backend consumer can register a
// handler and the core never sees a raw MSG.
// ============================================================
#include "airui/hal.h"
#include <windows.h>
#include <functional>
#include <map>
#include <string>

namespace air {
namespace ui {

class Win32Window : public IWindow
{
public:
    explicit Win32Window(HWND hwnd) : m_hwnd(hwnd) {}
    ~Win32Window() override = default;

    // ---- IWindow: chrome / lifecycle ----
    void SetTitle(const std::wstring& title) override {
        if (m_hwnd) SetWindowTextW(m_hwnd, title.c_str());
    }
    void SetMinSize(int w, int h) override {
        // Remembered here, enforced in PumpEvent's WM_GETMINMAXINFO case,
        // so the host only has to forward messages and gets a real limit.
        m_minW = w; m_minH = h;
    }
    void GetMinSize(int& w, int& h) const { w = m_minW; h = m_minH; }

    void Show() override {
        if (m_hwnd) ShowWindow(m_hwnd, SW_SHOW);
    }
    int PumpOne() override {
        MSG msg;
        if (!PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) return 0;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) return 1;
        return 0;
    }
    void RequestRedraw() override {
        if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    Size ClientSize() const override {
        RECT rc{};
        if (m_hwnd && GetClientRect(m_hwnd, &rc))
            return Size{ rc.right - rc.left, rc.bottom - rc.top };
        return Size{ 0, 0 };
    }

    // ---- IWindow: caption / status / nav ----
    void SetCaptionBadge(const std::wstring& text, bool ok) override { m_captionBadge = text; m_captionOk = ok; }
    void SetCaptionSubtitle(const std::wstring& text) override { m_captionSubtitle = text; }
    void SetStatus(const std::wstring& text, bool ok) override { m_statusText = text; m_statusOk = ok; }
    void SetStatusExtra(const std::wstring& text) override { m_statusExtra = text; }
    struct NavBadge { std::wstring text; bool ok = true; };
    void SetNavBadge(int id, const std::wstring& text, bool ok) override {
        m_navBadges[id] = NavBadge{ text, ok };
    }
    // Read back by the rendering layer; nullptr when the id has no badge.
    const NavBadge* FindNavBadge(int id) const {
        auto it = m_navBadges.find(id);
        return it == m_navBadges.end() ? nullptr : &it->second;
    }

    // ---- IWindow: event registration ----
    void OnMouseMove(std::function<void(Point)> cb) override { m_onMouseMove = std::move(cb); }
    void OnMouseDown(std::function<void(Point, int)> cb) override { m_onMouseDown = std::move(cb); }
    void OnMouseUp(std::function<void(Point, int)> cb) override { m_onMouseUp = std::move(cb); }
    void OnMouseEnter(std::function<void()> cb) override { m_onMouseEnter = std::move(cb); }
    void OnMouseLeave(std::function<void()> cb) override { m_onMouseLeave = std::move(cb); }
    void OnWheel(std::function<void(Point, float)> cb) override { m_onWheel = std::move(cb); }
    void OnKeyDown(std::function<void(int, bool, bool)> cb) override { m_onKeyDown = std::move(cb); }
    void OnChar(std::function<void(wchar_t)> cb) override { m_onChar = std::move(cb); }
    void OnFocus(std::function<void()> cb) override { m_onFocus = std::move(cb); }
    void OnBlur(std::function<void()> cb) override { m_onBlur = std::move(cb); }
    void OnDpiChanged(std::function<void(int)> cb) override { m_onDpi = std::move(cb); }
    void OnResized(std::function<void(Rect)> cb) override { m_onResized = std::move(cb); }

    void OnMinimize(std::function<void()> cb) override { m_onMinimize = std::move(cb); }
    void OnMaximizeRestore(std::function<void()> cb) override { m_onMaxRestore = std::move(cb); }
    void OnClose(std::function<void()> cb) override { m_onClose = std::move(cb); }

    // ---- message pump: forward native messages to registered callbacks ----
    // button: 0 = left, 1 = right, 2 = middle.
    void PumpEvent(UINT msg, WPARAM wp, LPARAM lp) {
        const POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        switch (msg) {
        case WM_MOUSEMOVE:
            if (m_onMouseMove) m_onMouseMove(Point{ pt.x, pt.y });
            if (!m_mouseInside) {
                m_mouseInside = true;
                if (m_onMouseEnter) m_onMouseEnter();
                // arm WM_MOUSELEAVE so we get a leave notification
                TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hwnd, 0 };
                TrackMouseEvent(&tme);
            }
            break;
        case WM_MOUSELEAVE:
            m_mouseInside = false;
            if (m_onMouseLeave) m_onMouseLeave();
            break;
        case WM_LBUTTONDOWN:
            if (m_onMouseDown) m_onMouseDown(Point{ pt.x, pt.y }, 0);
            break;
        case WM_RBUTTONDOWN:
            if (m_onMouseDown) m_onMouseDown(Point{ pt.x, pt.y }, 1);
            break;
        case WM_MBUTTONDOWN:
            if (m_onMouseDown) m_onMouseDown(Point{ pt.x, pt.y }, 2);
            break;
        case WM_LBUTTONUP:
            if (m_onMouseUp) m_onMouseUp(Point{ pt.x, pt.y }, 0);
            break;
        case WM_RBUTTONUP:
            if (m_onMouseUp) m_onMouseUp(Point{ pt.x, pt.y }, 1);
            break;
        case WM_MBUTTONUP:
            if (m_onMouseUp) m_onMouseUp(Point{ pt.x, pt.y }, 2);
            break;
        case WM_MOUSEWHEEL:
            // WM_MOUSEWHEEL carries SCREEN coordinates, unlike the button
            // messages. Convert to client space before handing to core.
            if (m_onWheel) {
                POINT screenPt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                if (m_hwnd) ScreenToClient(m_hwnd, &screenPt);
                m_onWheel(Point{ screenPt.x, screenPt.y },
                          (float)GET_WHEEL_DELTA_WPARAM(wp) / 120.f);
            }
            break;
        case WM_KEYDOWN:
            if (m_onKeyDown)
                m_onKeyDown((int)wp, (GetKeyState(VK_CONTROL) & 0x8000) != 0,
                            (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            break;
        case WM_CHAR:
            if (m_onChar) m_onChar((wchar_t)wp);
            break;
        case WM_SETFOCUS:
            if (m_onFocus) m_onFocus();
            break;
        case WM_KILLFOCUS:
            if (m_onBlur) m_onBlur();
            break;
        case WM_DPICHANGED:
            if (m_onDpi) m_onDpi((int)LOWORD(lp));
            break;
        case WM_SIZE:
            if (m_onResized && m_hwnd) {
                RECT rc; GetClientRect(m_hwnd, &rc);
                m_onResized(Rect{ rc.left, rc.top, rc.right, rc.bottom });
            }
            break;
        case WM_GETMINMAXINFO:
            // Turn SetMinSize() into a real window constraint.
            if ((m_minW > 0 || m_minH > 0) && lp) {
                MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lp);
                if (mmi->ptMinTrackSize.x < m_minW) mmi->ptMinTrackSize.x = m_minW;
                if (mmi->ptMinTrackSize.y < m_minH) mmi->ptMinTrackSize.y = m_minH;
            }
            break;
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_MINIMIZE && m_onMinimize) m_onMinimize();
            if (((wp & 0xFFF0) == SC_MAXIMIZE || (wp & 0xFFF0) == SC_RESTORE) && m_onMaxRestore) m_onMaxRestore();
            if ((wp & 0xFFF0) == SC_CLOSE && m_onClose) m_onClose();
            break;
        }
    }

private:
    HWND m_hwnd = nullptr;
    int  m_minW = 0, m_minH = 0;
    bool m_mouseInside = false;
    std::wstring m_captionBadge, m_captionSubtitle, m_statusText, m_statusExtra;
    bool m_captionOk = true, m_statusOk = true;
    std::map<int, NavBadge> m_navBadges;

    std::function<void(Point)> m_onMouseMove;
    std::function<void(Point, int)> m_onMouseDown, m_onMouseUp;
    std::function<void()> m_onMouseEnter, m_onMouseLeave;
    std::function<void(Point, float)> m_onWheel;
    std::function<void(int, bool, bool)> m_onKeyDown;
    std::function<void(wchar_t)> m_onChar;
    std::function<void()> m_onFocus, m_onBlur;
    std::function<void(int)> m_onDpi;
    std::function<void(Rect)> m_onResized;
    std::function<void()> m_onMinimize, m_onMaxRestore, m_onClose;
};

} // namespace ui
} // namespace air
