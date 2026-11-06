#pragma once
// ============================================================
// DiagLog.h — 轻量诊断日志（客户端启动/退出/异常）
//
// 为什么需要：客户端"闪一下就消失"在现场无法复现时，只能靠
// 进程自己的退出轨迹判断是崩溃、被关闭，还是单实例分支提前返回。
// 日志落在 %LOCALAPPDATA%\AireRecord\client.log（按用户隔离），
// 超过 256KB 自动清空重写，避免无限增长。
// ============================================================
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <string>

namespace diag
{
    inline const std::wstring& LogPath()
    {
        static std::wstring path = [] {
            std::wstring dir;
            wchar_t* env = nullptr;
            size_t len = 0;
            if (!_wdupenv_s(&env, &len, L"LOCALAPPDATA") && env) {
                dir = env;
                free(env);
            }
            if (dir.empty()) {
                wchar_t tmp[MAX_PATH] = {};
                GetTempPathW(MAX_PATH, tmp);
                dir = tmp;
            }
            dir += L"\\AireRecord";
            CreateDirectoryW(dir.c_str(), nullptr);
            return dir + L"\\client.log";
        }();
        return path;
    }

    inline void Log(const wchar_t* fmt, ...)
    {
        const std::wstring& path = LogPath();

        // 超过 256KB 直接重写（只保留最近的启动轨迹）
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad) &&
            (fad.nFileSizeHigh > 0 ||
             fad.nFileSizeLow > 256 * 1024)) {
            DeleteFileW(path.c_str());
        }

        FILE* f = nullptr;
        _wfopen_s(&f, path.c_str(), L"a, ccs=UTF-8");
        if (!f) return;

        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf_s(f, L"%04d-%02d-%02d %02d:%02d:%02d.%03d [pid %lu] ",
                   st.wYear, st.wMonth, st.wDay,
                   st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                   GetCurrentProcessId());
        va_list ap;
        va_start(ap, fmt);
        vfwprintf_s(f, fmt, ap);
        va_end(ap);
        fwprintf_s(f, L"\n");
        fclose(f);
    }

    // 当前进程所在会话（多会话环境下区分控制台/RDP）
    inline DWORD SessionId()
    {
        DWORD sid = 0;
        ProcessIdToSessionId(GetCurrentProcessId(), &sid);
        return sid;
    }
}
