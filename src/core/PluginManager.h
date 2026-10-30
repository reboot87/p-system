#pragma once
// ============================================================
// PluginManager.h — 插件注册与事件分发
// ============================================================
#include <memory>
#include <vector>
#include "Plugin.h"

class PluginManager
{
public:
    // 注册插件（按注册顺序分发）
    void Register(std::unique_ptr<IFeaturePlugin> plugin);

    bool AttachAll(App& app);
    void DetachAll(App& app);

    // 事件分发
    bool DispatchWndProc(App& app, HWND hwnd, UINT msg,
                         WPARAM wParam, LPARAM lParam, LRESULT& result);
    void DispatchCommand(App& app, int ctrlId, int notifyCode);
    void DispatchTimer(App& app, UINT_PTR timerId);

    size_t Count() const { return m_plugins.size(); }

private:
    std::vector<std::unique_ptr<IFeaturePlugin>> m_plugins;
};
