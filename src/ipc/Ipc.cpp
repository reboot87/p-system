#include "Ipc.h"
#include <windows.h>
#include "../core/MachineConfig.h"

namespace ipc
{
const wchar_t* PipeName()
{
    static std::wstring name = [] {
        std::wstring p = machine::Get(L"service", L"pipe_name",
                                      L"AireRecordIpc");

        // 会话内隔离：服务为每个 Active 会话（控制台 + 每个 RDP 会话）
        // 各拉起一个代理，每个代理有自己的数据库。若用整机全局管道名，
        // 多个会话会抢同一个名字——先创建者独占，后创建者静默失去 IPC，
        // 客户端于是连到别的会话的代理上：读到别人的库（看起来像记录
        // 全丢），且手动录入/删除会写进别人的库。
        // 追加会话 ID 后，客户端只与本会话的代理通信。
        DWORD sid = 0;
        if (!ProcessIdToSessionId(GetCurrentProcessId(), &sid)) sid = 0;
        return L"\\\\.\\pipe\\" + p + L"_s" + std::to_wstring(sid);
    }();
    return name.c_str();
}

std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                        &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

std::wstring EscapeField(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size() + 8);
    for (wchar_t c : s) {
        switch (c) {
        case L'\\': out += L"\\\\"; break;
        case L'\t': out += L"\\t";  break;
        case L'\n': out += L"\\n";  break;
        case L'\r': out += L"\\r";  break;
        default:    out += c;
        }
    }
    return out;
}

std::wstring UnescapeField(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            wchar_t c = s[++i];
            switch (c) {
            case L'\\': out += L'\\'; break;
            case L't':  out += L'\t'; break;
            case L'n':  out += L'\n'; break;
            case L'r':  out += L'\r'; break;
            default:    out += c;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

std::vector<std::wstring> SplitTabs(const std::wstring& s)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    for (wchar_t c : s) {
        if (c == L'\t') { out.push_back(std::move(cur)); cur.clear(); }
        else            cur += c;
    }
    out.push_back(std::move(cur));
    return out;
}

std::vector<std::wstring> SplitLines(const std::wstring& s)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    for (wchar_t c : s) {
        if (c == L'\n') { out.push_back(std::move(cur)); cur.clear(); }
        else            cur += c;
    }
    if (!cur.empty()) out.push_back(std::move(cur));
    return out;
}
}
