#pragma once
// ============================================================
// PluginManager.h — 插件注册与事件分发
// ============================================================
#include <memory>
#include <vector>
#include "Plugin.h"

namespace arecord
{

class PluginManager
{
public:
    // 注册插件（按注册顺序分发）
    void Register(std::unique_ptr<IFeaturePlugin> plugin);

    bool AttachAll(App& app);
    void DetachAll(App& app);

    // 事件分发
    bool DispatchWndProc(App& app, air::ui::WindowHandle hwnd,
                         air::ui::MessageId msg,
                         air::ui::MessageWParam wParam,
                         air::ui::MessageLParam lParam,
                         air::ui::MessageResult& result);
    void DispatchCommand(App& app, int ctrlId, int notifyCode);
    void DispatchTimer(App& app, uintptr_t timerId);

    size_t Count() const { return m_plugins.size(); }

private:
    std::vector<std::unique_ptr<IFeaturePlugin>> m_plugins;
};

} // namespace arecord
