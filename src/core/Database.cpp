#include "Database.h"
#include <windows.h>
#include <sqlite3.h>
#include <ctime>
#include <sstream>

// ---------- 编码转换 ----------
std::string Database::WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                        &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring Database::Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

std::wstring Database::NowString()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

Database::~Database()
{
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
}

bool Database::Open(const std::wstring& dbPath)
{
    int rc = sqlite3_open16(dbPath.c_str(), &m_db); // open16 直接支持宽字符路径
    if (rc != SQLITE_OK) {
        if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
        return false;
    }
    sqlite3_busy_timeout(m_db, 1000);

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS entries ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  content    TEXT NOT NULL,"
        "  source     TEXT NOT NULL DEFAULT 'Clipboard',"
        "  created_at TEXT NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_entries_content ON entries(content);"
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key   TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");";
    char* err = nullptr;
    rc = sqlite3_exec(m_db, ddl, nullptr, nullptr, &err);
    if (err) { sqlite3_free(err); }
    if (rc != SQLITE_OK) return false;

    // 旧库迁移：补充 note 列（已存在则忽略错误）
    sqlite3_exec(m_db,
        "ALTER TABLE entries ADD COLUMN note TEXT NOT NULL DEFAULT ''",
        nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    return true;
}

bool Database::SaveEntry(const std::wstring& content, const wchar_t* source,
                         const std::wstring& note)
{
    if (!m_db || content.empty()) return false;

    // 去重：删除同内容旧记录（新记录自然置顶）
    {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, "DELETE FROM entries WHERE content = ?1",
                               -1, &stmt, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_text16(stmt, 1, content.c_str(),
                            (int)(content.size() * sizeof(wchar_t)), SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
            "INSERT INTO entries(content, source, created_at, note) "
            "VALUES (?1, ?2, ?3, ?4)",
            -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    std::wstring now = NowString();
    sqlite3_bind_text16(stmt, 1, content.c_str(),
                        (int)(content.size() * sizeof(wchar_t)), SQLITE_TRANSIENT);
    sqlite3_bind_text16(stmt, 2, source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(stmt, 3, now.c_str(),
                        (int)(now.size() * sizeof(wchar_t)), SQLITE_TRANSIENT);
    sqlite3_bind_text16(stmt, 4, note.c_str(),
                        (int)(note.size() * sizeof(wchar_t)), SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Entry> Database::Query(const std::wstring& filter, int limit)
{
    std::vector<Entry> out;
    if (!m_db) return out;

    const char* sql;
    if (filter.empty()) {
        sql = "SELECT id, content, source, created_at, note FROM entries "
              "ORDER BY id DESC LIMIT ?1";
    } else {
        sql = "SELECT id, content, source, created_at, note FROM entries "
              "WHERE content LIKE ?1 ESCAPE '\\' OR note LIKE ?1 ESCAPE '\\' "
              "ORDER BY id DESC LIMIT ?2";
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return out;

    if (filter.empty()) {
        sqlite3_bind_int(stmt, 1, limit);
    } else {
        // 转义 LIKE 通配符 % 和 _
        std::wstring esc;
        esc.reserve(filter.size() * 2);
        for (wchar_t c : filter) {
            if (c == L'%' || c == L'_' || c == L'\\') esc += L'\\';
            esc += c;
        }
        std::wstring pattern = L"%" + esc + L"%";
        sqlite3_bind_text16(stmt, 1, pattern.c_str(),
                            (int)(pattern.size() * sizeof(wchar_t)), SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, limit);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Entry e;
        e.id        = sqlite3_column_int64(stmt, 0);
        const wchar_t* c = (const wchar_t*)sqlite3_column_text16(stmt, 1);
        if (c) e.content = c;
        const wchar_t* s = (const wchar_t*)sqlite3_column_text16(stmt, 2);
        if (s) e.source = s;
        const wchar_t* t = (const wchar_t*)sqlite3_column_text16(stmt, 3);
        if (t) e.createdAt = t;
        const wchar_t* n = (const wchar_t*)sqlite3_column_text16(stmt, 4);
        if (n) e.note = n;
        out.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return out;
}

bool Database::DeleteById(int64_t id)
{
    if (!m_db) return false;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "DELETE FROM entries WHERE id = ?1",
                           -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::UpdateNote(int64_t id, const std::wstring& note)
{
    if (!m_db) return false;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "UPDATE entries SET note = ?1 WHERE id = ?2",
                           -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text16(stmt, 1, note.c_str(),
                        (int)(note.size() * sizeof(wchar_t)), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ---------- 统计 ----------
static int64_t ScalarCount(sqlite3* db, const char* sql)
{
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    int64_t n = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        n = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return n;
}

int64_t Database::Count()
{
    if (!m_db) return 0;
    return ScalarCount(m_db, "SELECT COUNT(*) FROM entries");
}

int64_t Database::CountToday()
{
    if (!m_db) return 0;
    // created_at 为 "YYYY-MM-DD HH:MM:SS"，用本地日期前缀匹配
    wchar_t today[16];
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf_s(today, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    std::string utf8 = WideToUtf8(today);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
            "SELECT COUNT(*) FROM entries WHERE created_at LIKE ?1 || '%'",
            -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(stmt, 1, utf8.c_str(), (int)utf8.size(),
                      SQLITE_TRANSIENT);
    int64_t n = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        n = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return n;
}

bool Database::ClearAll()
{
    if (!m_db) return false;
    char* err = nullptr;
    bool ok = (sqlite3_exec(m_db, "DELETE FROM entries;", nullptr, nullptr, &err)
               == SQLITE_OK);
    if (err) sqlite3_free(err);
    return ok;
}

std::wstring Database::GetSetting(const std::wstring& key, const std::wstring& def)
{
    if (!m_db) return def;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT value FROM settings WHERE key = ?1",
                           -1, &stmt, nullptr) != SQLITE_OK)
        return def;
    sqlite3_bind_text16(stmt, 1, key.c_str(),
                        (int)(key.size() * sizeof(wchar_t)), SQLITE_TRANSIENT);
    std::wstring out = def;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const wchar_t* v = (const wchar_t*)sqlite3_column_text16(stmt, 0);
        if (v) out = v;
    }
    sqlite3_finalize(stmt);
    return out;
}

void Database::SetSetting(const std::wstring& key, const std::wstring& value)
{
    if (!m_db) return;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
            "INSERT INTO settings(key, value) VALUES (?1, ?2) "
            "ON CONFLICT(key) DO UPDATE SET value = ?2",
            -1, &stmt, nullptr) != SQLITE_OK)
        return;
    sqlite3_bind_text16(stmt, 1, key.c_str(),
                        (int)(key.size() * sizeof(wchar_t)), SQLITE_TRANSIENT);
    sqlite3_bind_text16(stmt, 2, value.c_str(),
                        (int)(value.size() * sizeof(wchar_t)), SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}
