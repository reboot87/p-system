#pragma once
// ============================================================
// AirUI — RaII.h
// 一批极小的 RAII 作用域守卫，把「改完后必须还原」的窗口状态
// 绑到对象生命周期上：中途 return / 抛异常 / break 也会自动还原，
// 不会把主窗口永久禁掉或永久冻结住。
// ============================================================
#include <windows.h>

namespace air {
namespace ui {

// 临时禁用 owner 输入，析构时按原状态恢复（本来就没启用就不动）。
// 只在自己确实禁了的情况下恢复，避免把上层模态已禁用/重新启用的
// 状态搞错。
class ScopedWindowDisable {
public:
    explicit ScopedWindowDisable(HWND h)
        : m_h(h), m_was(h && IsWindowEnabled(h) != FALSE) {
        if (m_was) EnableWindow(h, FALSE);
    }
    ~ScopedWindowDisable() {
        if (m_was && IsWindow(m_h)) EnableWindow(m_h, TRUE);
    }
    ScopedWindowDisable(const ScopedWindowDisable&) = delete;
    ScopedWindowDisable& operator=(const ScopedWindowDisable&) = delete;

private:
    HWND m_h;
    bool m_was;
};

// 临时冻结窗口重绘（WM_SETREDRAW FALSE），析构时恢复并连子控件一起
// 失效，把冻结期间吞掉的那次重绘补回来。
class ScopedRedrawFreeze {
public:
    explicit ScopedRedrawFreeze(HWND h) : m_h(h) {
        if (m_h) SendMessageW(m_h, WM_SETREDRAW, FALSE, 0);
    }
    ~ScopedRedrawFreeze() {
        if (m_h) {
            SendMessageW(m_h, WM_SETREDRAW, TRUE, 0);
            RedrawWindow(m_h, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
    }
    ScopedRedrawFreeze(const ScopedRedrawFreeze&) = delete;
    ScopedRedrawFreeze& operator=(const ScopedRedrawFreeze&) = delete;

private:
    HWND m_h;
};

} // namespace ui
} // namespace air