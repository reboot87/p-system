#pragma once
// ============================================================
// F4 — 手动录入保存插件（通过 IPC 请求服务端入库）
// 底部 EDIT + 备注 + BUTTON：点击按钮或 Ctrl+Enter 保存
// ============================================================
#include "../core/Plugin.h"

class ManualEntryPlugin : public IFeaturePlugin
{
public:
    const wchar_t* Name() const override { return L"ManualEntry"; }
    bool OnAttach(App& app) override;
    void OnDetach(App& app) override;
    void OnCommand(App& app, int ctrlId, int notifyCode) override;

    // 读取录入框内容并保存（供 EDIT 子类化回调调用），返回是否成功写入
    static bool SaveFromManual(App& app);
};
