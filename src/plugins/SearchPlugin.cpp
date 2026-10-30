#include "SearchPlugin.h"
#include "../core/App.h"

// F3 搜索防抖：输入变化时重启 250ms 定时器
void SearchPlugin::OnCommand(App& app, int ctrlId, int notifyCode)
{
    if (ctrlId != IDC_SEARCH_EDIT || notifyCode != EN_CHANGE)
        return;

    HWND hwnd = app.Hwnd();
    // 防抖：每次输入先停掉旧定时器再起新的
    KillTimer(hwnd, TIMER_SEARCH_DEBOUNCE);
    SetTimer(hwnd, TIMER_SEARCH_DEBOUNCE, 250, nullptr);
}

// 定时器触发：执行真正的查询并刷新列表
void SearchPlugin::OnTimer(App& app, UINT_PTR timerId)
{
    if (timerId != TIMER_SEARCH_DEBOUNCE)
        return;

    KillTimer(app.Hwnd(), TIMER_SEARCH_DEBOUNCE); // 一次性：触发即停
    app.RefreshList(app.GetSearchText());
}

void SearchPlugin::OnDetach(App& app)
{
    KillTimer(app.Hwnd(), TIMER_SEARCH_DEBOUNCE);
}
