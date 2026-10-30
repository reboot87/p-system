#pragma once
// ============================================================
// Database.h — SQLite 封装（UTF-16 与 Win32 无缝衔接）
// 表结构：entries(id, content, source, created_at, note)
// ============================================================
#include <string>
#include <vector>
#include <cstdint>

struct sqlite3;

struct Entry
{
    int64_t      id = 0;
    std::wstring content;
    std::wstring source;     // "Clipboard" 或 "Manual"
    std::wstring createdAt;  // 本地时间字符串
    std::wstring note;       // 备注
};

class Database
{
public:
    ~Database();

    // 打开（不存在则建库建表；旧库自动补 note 列）
    bool Open(const std::wstring& dbPath);

    // 保存条目：内容去重（已存在同内容则删旧插新，保持置顶）。
    // note 为可选备注。返回 true 表示新写入。
    bool SaveEntry(const std::wstring& content, const wchar_t* source,
                   const std::wstring& note = L"");

    // 关键字查询（LIKE %filter%，已转义 % 和 _）；filter 为空则取最近条目
    std::vector<Entry> Query(const std::wstring& filter, int limit = 500);

    // 按 id 删除
    bool DeleteById(int64_t id);

    // 更新指定条目的备注（任意来源条目均可回改）
    bool UpdateNote(int64_t id, const std::wstring& note);

    // 统计：总条目数 / 今日新增条目数
    int64_t Count();
    int64_t CountToday();

    // 清空全部记录（机器配置页"清空剪贴板记录"）
    bool ClearAll();

    // 简单 KV 设置（如 autostart 偏好）
    std::wstring GetSetting(const std::wstring& key, const std::wstring& def = L"");
    void SetSetting(const std::wstring& key, const std::wstring& value);

private:
    sqlite3* m_db = nullptr;

    static std::string  WideToUtf8(const std::wstring& w);
    static std::wstring Utf8ToWide(const std::string& s);
    static std::wstring NowString();
};
