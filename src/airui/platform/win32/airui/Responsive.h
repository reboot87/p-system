#pragma once
// ============================================================
// AirUI — Responsive.h
// 响应式断点系统
// ============================================================
#include <windows.h>
#include <functional>
#include <vector>
#include <algorithm>

// 前向声明
namespace air { namespace ui { class Shell; } }

namespace air {
namespace ui {

struct Breakpoint
{
    int maxWidth;
    std::function<void(Shell&)> apply;
    std::function<void(Shell&)> revert;
};

class ResponsiveManager
{
public:
    explicit ResponsiveManager(Shell* shell) : m_shell(shell) {}

    void SetBreakpoints(std::vector<Breakpoint> bps) {
        m_breakpoints = std::move(bps);
        std::sort(m_breakpoints.begin(), m_breakpoints.end(),
            [](const Breakpoint& a, const Breakpoint& b) {
                return a.maxWidth > b.maxWidth;
            });
        m_currentIdx = -1;
    }

    void OnSize(int width, int height) {
        int newIdx = -1;
        for (size_t i = 0; i < m_breakpoints.size(); ++i) {
            if (width <= m_breakpoints[i].maxWidth) {
                newIdx = (int)i;
                break;
            }
        }
        if (newIdx != m_currentIdx) {
            if (m_currentIdx >= 0 && m_currentIdx < (int)m_breakpoints.size()) {
                if (m_breakpoints[m_currentIdx].revert) {
                    m_breakpoints[m_currentIdx].revert(*m_shell);
                }
            }
            if (newIdx >= 0) {
                m_breakpoints[newIdx].apply(*m_shell);
            }
            m_currentIdx = newIdx;
        }
    }

    void ApplyCurrent() {
        if (m_currentIdx >= 0 && m_currentIdx < (int)m_breakpoints.size()) {
            m_breakpoints[m_currentIdx].apply(*m_shell);
        }
    }

    int CurrentBreakpoint() const { return m_currentIdx; }
    const std::vector<Breakpoint>& Breakpoints() const { return m_breakpoints; }

private:
    Shell* m_shell = nullptr;
    std::vector<Breakpoint> m_breakpoints;
    int m_currentIdx = -1;
};

namespace responsive {
std::vector<Breakpoint> StandardBreakpoints(Shell* shell);
void ToggleSidebar(Shell* shell, bool collapse);
}

} // namespace ui
} // namespace air