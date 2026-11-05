#pragma once
// ============================================================
// AirUI - Win32Window.h
// Minimal IWindow implementation for Win32 backend.
// Phase 1 HAL stub: wraps an existing HWND, forwards events via
// std::function callbacks. Core remains platform-free.
// ============================================================
#include "airui/hal.h"
#include <windows.h>
#include <functional>
#include <string>

namespace air {
namespace ui {

class Win32Window : public IWindow
{
public:
    explicit Win32Window(HWND hwnd) : m_hwnd(hwnd) {}
    ~Win32Window() override = default;

    // IWindow
    void SetTitle(const std::wstring& title) override {
        if (m_hwnd) SetWindowTextW(m_hwnd, title.c_str());
    }
    void SetMinSize(int w, int h) override {
        if (m_hwnd) {
            RECT r{};
            GetClientRect(m_hwnd, &r);
            // store min size for later use — simplified
            (void)w; (void)h;
        }
    }
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
        if (m_hwnd && GetClientRect(m_hwnd, &rc)) {
            return Size{ rc.right - rc.left, rc.bottom - rc.top };
        }
        return Size{0,0};
    }

    // caption / status / nav
    void SetCaptionBadge(const std::wstring& text, bool ok) override { m_captionBadge = text; m_captionOk = ok; }
    void SetCaptionSubtitle(const std::wstring& text) override { m_captionSubtitle = text; }
    void SetStatus(const std::wstring& text, bool ok) override { m_statusText = text; m_statusOk = ok; }
    void SetStatusExtra(const std::wstring& text) override { m_statusExtra = text; }
    void SetNavBadge(int id, const std::wstring& text, bool ok) override { (void)id; (void)text; (void)ok; }

    // event registration
    void OnMouseMove(std::function<void(Point)> cb) override { m_onMouseMove = std::move(cb); }
    void OnMouseDown(std::function<void(Point,int)> cb) override { m_onMouseDown = std::move(cb); }
    void OnMouseUp(std::function<void(Point,int)> cb) override { m_onMouseUp = std::move(cb); }
    void OnWheel(std::function<void(Point,float)> cb) override { m_onWheel = std::move(cb); }
    void OnKeyDown(std::function<void(int,bool,bool)> cb) override { m_onKeyDown = std::move(cb); }
    void OnResized(std::function<void(Rect)> cb) override { m_onResized = std::move(cb); }

    void OnMinimize(std::function<void()> cb) override { m_onMinimize = std::move(cb); }
    void OnMaximizeRestore(std::function<void()> cb) override { m_onMaxRestore = std::move(cb); }
    void OnClose(std::function<void()> cb) override { m_onClose = std::move(cb); }

    // helpers to pump events from Win32 messages
    void PumpEvent(UINT msg, WPARAM wp, LPARAM lp) {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        switch (msg) {
        case WM_MOUSEMOVE:
            if (m_onMouseMove) m_onMouseMove(Point{pt.x, pt.y});
            break;
        case WM_LBUTTONDOWN:
            if (m_onMouseDown) m_onMouseDown(Point{pt.x, pt.y}, 0);
            break;
        case WM_LBUTTONUP:
            if (m_onMouseUp) m_onMouseUp(Point{pt.x, pt.y}, 0);
            break;
        case WM_MOUSEWHEEL:
            if (m_onWheel) m_onWheel(Point{pt.x, pt.y}, (float)GET_WHEEL_DELTA_WPARAM(wp) / 120.f);
            break;
        case WM_KEYDOWN:
            if (m_onKeyDown) m_onKeyDown((int)wp, (GetKeyState(VK_CONTROL) & 0x8000)!=0, (GetKeyState(VK_SHIFT) & 0x8000)!=0);
            break;
        case WM_SIZE:
            if (m_onResized && m_hwnd) {
                RECT rc; GetClientRect(m_hwnd, &rc);
                m_onResized(Rect{rc.left, rc.top, rc.right, rc.bottom});
            }
            break;
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_MINIMIZE && m_onMinimize) m_onMinimize();
            if ((wp & 0xFFF0) == SC_MAXIMIZE && m_onMaxRestore) m_onMaxRestore();
            if ((wp & 0xFFF0) == SC_CLOSE && m_onClose) m_onClose();
            break;
        }
    }

private:
    HWND m_hwnd = nullptr;
    std::wstring m_captionBadge, m_captionSubtitle, m_statusText, m_statusExtra;
    bool m_captionOk = true, m_statusOk = true;

    std::function<void(Point)> m_onMouseMove;
    std::function<void(Point,int)> m_onMouseDown, m_onMouseUp;
    std::function<void(Point,float)> m_onWheel;
    std::function<void(int,bool,bool)> m_onKeyDown;
    std::function<void(Rect)> m_onResized;
    std::function<void()> m_onMinimize, m_onMaxRestore, m_onClose;
};

} // namespace ui
} // namespace air
