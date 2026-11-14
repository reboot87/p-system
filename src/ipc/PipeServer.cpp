#include "PipeServer.h"
#include "Ipc.h"

bool PipeServer::Start(Handler handler, DisconnectHandler onDisconnect)
{
    m_handler      = std::move(handler);
    m_onDisconnect = std::move(onDisconnect);
    m_running = true;
    if (!m_stopEvent) {
        m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);   // manual reset
    }
    m_acceptThread = std::thread(&PipeServer::AcceptLoop, this);
    return true;
}

void PipeServer::Stop()
{
    m_running = false;
    if (m_stopEvent) SetEvent(m_stopEvent);   // 唤醒接受线程（overlapped connect 等待）
    // 唤醒阻塞中的 ConnectNamedPipe / ReadFile（不关闭句柄，由各自的
    // 接受/工作线程负责关闭，避免双重关闭）
    {
        std::lock_guard<std::mutex> lk(m_cs);
        if (m_acceptPipe != INVALID_HANDLE_VALUE)
            CancelIoEx(m_acceptPipe, nullptr);
        for (auto& c : m_conns)
            if (c.pipe != INVALID_HANDLE_VALUE)
                CancelIoEx(c.pipe, nullptr);
    }
    if (m_acceptThread.joinable())
        m_acceptThread.join();
    std::lock_guard<std::mutex> lk(m_cs);
    for (auto& c : m_conns)
        if (c.worker.joinable())
            c.worker.join();
    m_conns.clear();
    if (m_stopEvent) { CloseHandle(m_stopEvent); m_stopEvent = nullptr; }
}

void PipeServer::SendEvent(const std::wstring& evt)
{
    std::string out = "EVT\n" + ipc::WideToUtf8(evt) + "\n";
    std::lock_guard<std::mutex> lk(m_cs);
    for (auto& c : m_conns) {
        if (c.pipe == INVALID_HANDLE_VALUE) continue;
        // 超时写，避免某连接读侧未及时消费而长时间阻塞
        ipc::PipeWriteTimeout(c.pipe, out, 200);
    }
}

void PipeServer::AcceptLoop()
{
    while (m_running) {
        HANDLE pipe = CreateNamedPipeW(
            ipc::PipeName(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,   // 支持并发读写
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,          // 多客户端并发连接
            4096, 65536, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            // 管道名被占用/权限不足：退避后重试（不能直接退出，否则代理
            // 失去 IPC 能力，客户端会静默连到其它实例而丢记录）
            Sleep(300);
            continue;
        }
        {
            std::lock_guard<std::mutex> lk(m_cs);
            m_acceptPipe = pipe;
        }
        // overlapped ConnectNamedPipe：既接受新客户端，也能被 Stop() 的
        // m_stopEvent 唤醒（阻塞式 ConnectNamedPipe 无法被 CancelIoEx 取消，
        // 否则服务停止时 accept 线程 join 会卡死，SCM 报"无法停止服务"）。
        OVERLAPPED cov{};
        cov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        BOOL connected = FALSE;
        HANDLE waiters[2] = { cov.hEvent, m_stopEvent };
        if (ConnectNamedPipe(pipe, &cov)) {
            connected = TRUE;
        } else if (GetLastError() == ERROR_PIPE_CONNECTED) {
            connected = TRUE;
        } else if (GetLastError() == ERROR_IO_PENDING) {
            DWORD w = WaitForMultipleObjects(2, waiters, FALSE, INFINITE);
            if (w == WAIT_OBJECT_0) {
                DWORD nr = 0;
                connected = GetOverlappedResult(pipe, &cov, &nr, FALSE) ? TRUE : FALSE;
            } else {
                connected = FALSE;   // 停止事件或失败 → 放弃本次等待
            }
        }
        if (cov.hEvent) CloseHandle(cov.hEvent);
        {
            std::lock_guard<std::mutex> lk(m_cs);
            m_acceptPipe = INVALID_HANDLE_VALUE;
        }
        if (!m_running) { CloseHandle(pipe); break; }

        if (connected) {
            std::lock_guard<std::mutex> lk(m_cs);
            m_conns.push_back({ pipe, std::thread(&PipeServer::Worker, this, pipe) });
        } else {
            CloseHandle(pipe);
        }
    }
}

void PipeServer::Worker(HANDLE pipe)
{
    std::wstring sessionIdPlaceholder;
    // 本连接专用的 overlapped 读事件
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    for (;;) {
        std::string tag;
        if (!ipc::PipeReadLine(pipe, ov, tag)) break;
        std::string payload;
        if (!ipc::PipeReadLine(pipe, ov, payload)) break;

        std::wstring tagW = ipc::Utf8ToWide(tag);
        if (tagW == L"REQ") {
            std::wstring payloadW = ipc::Utf8ToWide(payload);
            size_t tab = payloadW.find(L'\t');
            std::wstring id = payloadW.substr(0, tab);
            std::wstring line = (tab == std::wstring::npos)
                                ? L"" : payloadW.substr(tab + 1);
            // 会话号关联：PING\t<session> → 登记到本连接，供断开回调识别
            if (line.rfind(L"PING\t", 0) == 0) {
                size_t t = line.find(L'\t');
                if (t != std::wstring::npos)
                    sessionIdPlaceholder = line.substr(t + 1);
            }
            std::wstring resp = m_handler(line);
            // 响应帧：RSP\n<id>\t<escaped-resp>\n（EscapeField 转义内嵌换行）
            std::string out = "RSP\n" +
                ipc::WideToUtf8(ipc::EscapeField(id + L"\t" + resp)) + "\n";
            if (!ipc::PipeWrite(pipe, out))
                break;
        }
        // EVT 是服务端单向广播，客户端不应上行 EVT；其它 TAG 忽略
    }

    if (ov.hEvent) CloseHandle(ov.hEvent);
    // 断开：标记该连接失效（供 Stop 清理），回调通知，回收句柄
    {
        std::lock_guard<std::mutex> lk(m_cs);
        for (auto& c : m_conns)
            if (c.pipe == pipe) { c.pipe = INVALID_HANDLE_VALUE; break; }
    }
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    if (m_onDisconnect)
        m_onDisconnect(sessionIdPlaceholder);
}