#include "PipeServer.h"
#include "Ipc.h"

bool PipeServer::Start(Handler handler)
{
    m_handler = std::move(handler);
    m_running = true;
    m_thread = std::thread(&PipeServer::Loop, this);
    return true;
}

void PipeServer::Stop()
{
    m_running = false;
    if (m_pipe != INVALID_HANDLE_VALUE) {
        // 唤醒阻塞中的 ConnectNamedPipe / ReadFile
        CancelIoEx(m_pipe, nullptr);
        DisconnectNamedPipe(m_pipe);
        CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
    }
    if (m_thread.joinable())
        m_thread.join();
}

void PipeServer::Loop()
{
    while (m_running) {
        m_pipe = CreateNamedPipeW(
            ipc::PipeName(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,              // 单实例：一次服务一个客户端足够
            4096, 65536, 0, nullptr);
        if (m_pipe == INVALID_HANDLE_VALUE) {
            // 管道名被占用/权限不足时不能直接退出：一旦退出，本会话的
            // 代理就彻底失去 IPC 能力，客户端会静默连到其它实例（旧版本
            // 多会话抢同一全局管道名时，正是这样丢的记录）。退避后重试。
            Sleep(300);
            continue;
        }

        BOOL connected = ConnectNamedPipe(m_pipe, nullptr)
                         ? TRUE
                         : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!m_running) { CloseHandle(m_pipe); m_pipe = INVALID_HANDLE_VALUE; break; }

        if (connected) {
            // 读取一行命令（以 '\n' 结束）
            std::string req;
            char buf[1024];
            DWORD n = 0;
            while (req.find('\n') == std::string::npos &&
                   ReadFile(m_pipe, buf, sizeof(buf), &n, nullptr) && n > 0)
                req.append(buf, n);

            size_t pos = req.find('\n');
            if (pos != std::string::npos) req.resize(pos);

            std::wstring resp = m_handler(ipc::Utf8ToWide(req));
            std::string out = ipc::WideToUtf8(resp);
            DWORD written = 0;
            WriteFile(m_pipe, out.data(), (DWORD)out.size(), &written, nullptr);
            FlushFileBuffers(m_pipe);
        }

        DisconnectNamedPipe(m_pipe);
        CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
    }
}
