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

        // 整机全局管道：由 Session 0 中的系统服务（airecordd.exe -service）
        // 独占持有，作为所有会话共享的机器级数据库的唯一访问入口。
        // 每个登录会话只运行一个 airecord.exe（UI + 同进程剪贴板代理），
        // 它把剪贴板记录通过这里转发给服务入库。因此名字不再带 _s<会话ID>
        // 后缀——全局唯一，所有会话的 UI/代理都连到同一个服务。
        // 保留基础名 AireRecordIpc，使旧配置仍能解析到同一管道。
        return L"\\\\.\\pipe\\" + p;
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
