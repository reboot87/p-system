#pragma once
// ============================================================
// F3 — 关键字自动查询插件
// 搜索框 EN_CHANGE → SetTimer 250ms 防抖 → SQLite LIKE 查询
// → LB_RESETCONTENT / LB_ADDSTRING 刷新列表
// ============================================================
#include "../core/Plugin.h"

class SearchPlugin : public IFeaturePlugin
{
public:
    const wchar_t* Name() const override { return L"SearchDebounce"; }
    void OnDetach(App& app) override;
    void OnCommand(App& app, int ctrlId, int notifyCode) override;
    void OnTimer(App& app, UINT_PTR timerId) override;
};
