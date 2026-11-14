#pragma once
// ============================================================
// PipeServer.h — 服务端命名管道（多实例 + 持久双向流 + 事件广播）
//
// 每个客户端（airecord.exe 的 UI 与同进程剪贴板代理）持有一条持久
// 连接。服务为每条连接起一个工作线程，循环读取帧：
//   REQ\n<id>\t<line>\n   → 调 handler(line) 得响应，写回 RSP\n<id>\t<resp>\n
//   断开（ReadFile 返回 ERROR_BROKEN_PIPE / 0 字节）→ 调 onDisconnect(sessionId)
// 服务端用 SendEvent() 向所有已连接客户端广播 EVT\n<evt>\n（事件帧）。
//
// 帧格式：每条帧 = "TAG\nPAYLOAD\n"，TAG ∈ {REQ, RSP, EVT}，PAYLOAD 单行。
// 请求行本身不含换行（命令字段已 EscapeField）；响应帧的 PAYLOAD 是
// 对整个 "<id>\t<resp>" 做 EscapeField 的结果，保证 LIST 等含内嵌换行的
// 响应也能作为一个完整帧安全传输（客户端 UnescapeField 还原）。
//
// 会话号关联：请求 PING\t<session> 会把该会话号登记到本连接，供
// onDisconnect 回调识别是哪个会话的 UI 掉线（用于实时重启该会话 UI）。
// ============================================================
#include <windows.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class PipeServer
{
public:
    // REQ 处理器：收到一条命令行，返回响应文本（保持既有命令契约不变）
    using Handler = std::function<std::wstring(const std::wstring&)>;
    // 断开回调：参数为该连接登记的会话号（来自 PING\t<session>，可为空）
    using DisconnectHandler = std::function<void(const std::wstring&)>;

    bool Start(Handler handler, DisconnectHandler onDisconnect = {});
    void Stop();

    // 向所有已连接客户端广播一个事件帧（EVT\n<evt>\n）
    void SendEvent(const std::wstring& evt);

private:
    struct Connection {
        HANDLE       pipe   = INVALID_HANDLE_VALUE;
        std::thread  worker;
    };

    void AcceptLoop();                 // 接受线程：循环建实例、等连接、派生工作线程
    void Worker(HANDLE pipe);          // 单连接工作线程：读帧循环直至断开

    Handler           m_handler;
    DisconnectHandler m_onDisconnect;
    std::atomic<bool> m_running{ false };

    std::mutex               m_cs;
    std::vector<Connection>  m_conns;        // 受 m_cs 保护
    HANDLE                   m_acceptPipe = INVALID_HANDLE_VALUE; // 受 m_cs 保护
    std::thread              m_acceptThread;
};