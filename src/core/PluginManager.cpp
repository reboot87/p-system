#include "PluginManager.h"

void PluginManager::Register(std::unique_ptr<IFeaturePlugin> plugin)
{
    m_plugins.push_back(std::move(plugin));
}

bool PluginManager::AttachAll(App& app)
{
    for (auto& p : m_plugins) {
        if (!p->OnAttach(app))
            return false; // 任一插件挂载失败即启动失败
    }
    return true;
}

void PluginManager::DetachAll(App& app)
{
    // 逆序卸载，保证依赖关系
    for (auto it = m_plugins.rbegin(); it != m_plugins.rend(); ++it)
        (*it)->OnDetach(app);
}

bool PluginManager::DispatchWndProc(App& app, HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam, LRESULT& result)
{
    for (auto& p : m_plugins) {
        if (p->OnWndProc(app, hwnd, msg, wParam, lParam, result))
            return true;
    }
    return false;
}

void PluginManager::DispatchCommand(App& app, int ctrlId, int notifyCode)
{
    for (auto& p : m_plugins)
        p->OnCommand(app, ctrlId, notifyCode);
}

void PluginManager::DispatchTimer(App& app, UINT_PTR timerId)
{
    for (auto& p : m_plugins)
        p->OnTimer(app, timerId);
}
