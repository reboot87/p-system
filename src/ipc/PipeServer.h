#pragma once
// ============================================================
// PipeServer.h — 服务端命名管道（独立线程，单实例循环服务）
// ============================================================
#include <windows.h>
#include <atomic>
#include <functional>
#include <string>
#include <thread>

class PipeServer
{
public:
    // handler 收到一条命令行，返回响应文本
    using Handler = std::function<std::wstring(const std::wstring&)>;

    bool Start(Handler handler);
    void Stop();

private:
    void Loop();

    Handler              m_handler;
    std::atomic<bool>    m_running{ false };
    HANDLE               m_pipe = INVALID_HANDLE_VALUE;
    std::thread          m_thread;
};
