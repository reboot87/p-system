#include "airui/Responsive.h"
#include "airui/Shell.h"

namespace air {
namespace ui {
namespace responsive {

std::vector<Breakpoint> StandardBreakpoints(Shell* shell)
{
    return {
        { 1200, [shell](Shell& s){ 
            // 大屏：显示侧栏和状态栏
            s.SetSidebarWidth(208);
            s.SetStatus(L"就绪", true);
            s.SetFooterText(L"AireRecord v1.0");
        }},
        { 900, [shell](Shell& s){ 
            // 中屏：侧栏折叠为图标模式，添加汉堡菜单
            s.SetSidebarWidth(56);
            s.SetNavGroups({}); // 清空导航组，触发重建
            s.AddNavCommand(999, L"\xE700", L"菜单", false); // 汉堡菜单
        }, [shell](Shell& s){
            s.SetSidebarWidth(208);
            s.SetNavGroups({}); // 恢复默认导航
        }},
        { 600, [shell](Shell& s){ 
            // 小屏：隐藏状态栏文字
            s.SetStatus(L"", true);
            s.SetFooterText(L"");
        }, [shell](Shell& s){
            s.SetStatus(L"就绪", true);
            s.SetFooterText(L"AireRecord v1.0");
        }}
    };
}

void ToggleSidebar(Shell* shell, bool collapse)
{
    if (collapse) {
        shell->SetSidebarWidth(56); // 只显示图标
    } else {
        shell->SetSidebarWidth(208);
    }
}

} // namespace responsive
} // namespace ui
} // namespace air