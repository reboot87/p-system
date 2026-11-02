#pragma once
// ============================================================
// UiEntry.h — 客户端列表条目模型（IPC 解析结果）
// ============================================================
#include <string>

struct UiEntry
{
    long long    id = 0;
    std::wstring source;      // 来源应用（Manual = 手动录入）
    std::wstring createdAt;   // "YYYY-MM-DD HH:MM:SS"
    std::wstring note;
    std::wstring content;
};
