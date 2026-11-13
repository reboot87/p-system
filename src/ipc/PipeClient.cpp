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

// 系统服务未运行时，提权 airecordd.exe -install（需要 UAC）以安装并
// 启动服务。返回 true 表示提权调用已发起（成功或失败由 UAC 决定）。
bool PipeClient::LaunchService()
{
    if (m_servicePath.empty()) return false;

    // runas 触发 UAC 提权：-install 会注册（如未注册）并启动系统服务。
    HINSTANCE r = ShellExecuteW(nullptr, L"runas", m_servicePath.c_str(),
                                L"-install", nullptr, SW_HIDE);
    return (intptr_t)r > 32;   // >32 表示调用发起成功（UAC 结果另计）
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