#pragma once
// ============================================================
// Plugin.h — 功能插件接口
// 每个功能（F1~F4 及未来的功能）实现一个 IFeaturePlugin，
// 在 main.cpp 中注册进 PluginManager，由核心统一分发事件。
// ============================================================
#include <windows.h>

class App;

class IFeaturePlugin
{
public:
    virtual ~IFeaturePlugin() = default;

    // 插件显示名（日志/调试用）
    virtual const wchar_t* Name() const = 0;

    // 挂载：注册热键/剪贴板监听/子类化控件等。返回 false 表示初始化失败。
    virtual bool OnAttach(App& app) { UNREFERENCED_PARAMETER(app); return true; }

    // 卸载：释放资源
    virtual void OnDetach(App& app) { UNREFERENCED_PARAMETER(app); }

    // 主窗口消息钩子。返回 true 表示已处理，核心不再继续分发。
    virtual bool OnWndProc(App& app, HWND hwnd, UINT msg,
                           WPARAM wParam, LPARAM lParam, LRESULT& result)
    { UNREFERENCED_PARAMETER(app); UNREFERENCED_PARAMETER(hwnd);
      UNREFERENCED_PARAMETER(msg); UNREFERENCED_PARAMETER(wParam);
      UNREFERENCED_PARAMETER(lParam); UNREFERENCED_PARAMETER(result);
      return false; }

    // WM_COMMAND 分发（控件通知）
    virtual void OnCommand(App& app, int ctrlId, int notifyCode)
    { UNREFERENCED_PARAMETER(app); UNREFERENCED_PARAMETER(ctrlId);
      UNREFERENCED_PARAMETER(notifyCode); }

    // WM_TIMER 分发（防抖等场景）
    virtual void OnTimer(App& app, UINT_PTR timerId)
    { UNREFERENCED_PARAMETER(app); UNREFERENCED_PARAMETER(timerId); }
};
