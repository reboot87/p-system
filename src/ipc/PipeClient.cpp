#include "PipeClient.h"
#include "Ipc.h"
#include <windows.h>

bool PipeClient::Ping(int retries)
{
    for (int i = 0; i <= retries; ++i) {
        HANDLE h = CreateFileW(ipc::PipeName(), GENERIC_READ | GENERIC_WRITE,
                               0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            // 发送 PING
            std::string req = ipc::WideToUtf8(L"PING") + "\n";
            DWORD written = 0;
            WriteFile(h, req.data(), (DWORD)req.size(), &written, nullptr);

            // 读响应
            std::string resp;
            char buf[256];
            DWORD n = 0;
            while (ReadFile(h, buf, sizeof(buf), &n, nullptr) && n > 0)
                resp.append(buf, n);
            CloseHandle(h);
            if (resp.find("OK") == 0)
                return true;
        }
        if (i < retries)
            Sleep(300);
    }
    return false;
}

bool PipeClient::LaunchService()
{
    if (m_servicePath.empty()) return false;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    std::wstring cmd = L"\"" + m_servicePath + L"\"";
    BOOL ok = CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                             0, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return ok != FALSE;
}

std::wstring PipeClient::Request(const std::wstring& line)
{
    HANDLE h = CreateFileW(ipc::PipeName(), GENERIC_READ | GENERIC_WRITE,
                           0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        // 服务未运行 → 尝试拉起并等待
        if (!LaunchService())
            return L"";
        bool up = false;
        for (int i = 0; i < 10 && !up; ++i) {
            Sleep(300);
            h = CreateFileW(ipc::PipeName(), GENERIC_READ | GENERIC_WRITE,
                            0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) up = true;
        }
        if (!up) return L"";
    }

    std::string req = ipc::WideToUtf8(line) + "\n";
    DWORD written = 0;
    if (!WriteFile(h, req.data(), (DWORD)req.size(), &written, nullptr)) {
        CloseHandle(h);
        return L"";
    }

    std::string resp;
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(h, buf, sizeof(buf), &n, nullptr) && n > 0)
        resp.append(buf, n);
    CloseHandle(h);
    return ipc::Utf8ToWide(resp);
}
