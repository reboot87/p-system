#include "PipeClient.h"
#include "Ipc.h"
#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

// 尝试连接全局管道，成功返回句柄，失败返回 INVALID_HANDLE_VALUE
static HANDLE TryConnect()
{
    return CreateFileW(ipc::PipeName(), GENERIC_READ | GENERIC_WRITE,
                       0, nullptr, OPEN_EXISTING, 0, nullptr);
}

bool PipeClient::Ping(int retries)
{
    for (int i = 0; i <= retries; ++i) {
        HANDLE h = TryConnect();
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

// 服务已安装但被停止时，用 SCM 静默启动（无需提权/UAC）。
static bool StartInstalledService()
{
    SC_HANDLE mgr = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!mgr) return false;
    SC_HANDLE svc = OpenServiceW(mgr, L"AireRecord", SERVICE_START);
    if (svc) {
        const BOOL started = StartServiceW(svc, 0, nullptr) ||
                             GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;
        CloseServiceHandle(svc);
        CloseServiceHandle(mgr);
        return started != FALSE;
    }
    CloseServiceHandle(mgr);
    return false;   // 服务未安装 → 需要提权安装
}

// 服务未运行时确保它起来。优先：服务已安装 → SCM 静默启动（无 UAC）；
// 仅当服务从未安装时，才提权 airecordd.exe -install（UAC）。
// 返回 true 表示"已发起启动（已安装场景）或提权安装（未安装场景）"。
bool PipeClient::LaunchService()
{
    if (m_servicePath.empty()) return false;

    if (StartInstalledService())
        return true;                            // 已装 → 静默启动成功

    // 服务未安装 → runas 触发 UAC 提权：-install 注册并启动系统服务。
    HINSTANCE r = ShellExecuteW(nullptr, L"runas", m_servicePath.c_str(),
                                L"-install", nullptr, SW_HIDE);
    return (intptr_t)r > 32;   // >32 表示提权调用已发起（UAC 结果另计）
}

// 确保系统服务已安装并运行、全局管道就绪。若管道未就绪，尝试提权
// 安装/启动服务，并轮询等待管道（服务启动需要几秒）。返回 true 表示
// 服务已就绪。
bool PipeClient::EnsureService()
{
    for (int i = 0; i < 20; ++i) {
        HANDLE h = TryConnect();
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            return true;
        }
        if (i == 0)
            LaunchService();       // 第一次就绪失败 → 提权拉起服务
        Sleep(300);
    }
    return false;
}

std::wstring PipeClient::Request(const std::wstring& line)
{
    HANDLE h = TryConnect();
    if (h == INVALID_HANDLE_VALUE) {
        // 服务未运行 → 尝试确保服务就绪并等待
        if (!EnsureService())
            return L"";
        h = TryConnect();
        if (h == INVALID_HANDLE_VALUE)
            return L"";
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