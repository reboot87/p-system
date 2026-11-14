#pragma once
// ============================================================
// PipeClient.h — 客户端持久连接 + 后台 reader（实时断开/事件）
//
// 客户端（airecord.exe 的 UI 与同进程剪贴板代理）持有一条到服务端的
// 持久连接，不再每次 Request 重连。后台 reader 线程循环读帧：
//   RSP\n<id>\t<resp>\n  → 派发给匹配的等待中的 Request（id 匹配）
//   EVT\n<evt>\n         → 调事件回调（可唤醒 UI 刷新）
//   断开（ERROR_BROKEN_PIPE / 0 字节）→ 调断开回调（实时检测服务死亡）
// 请求为同步阻塞式：Request(line) 发 REQ 帧后等待匹配 RSP，超时返回空。
// ============================================================
#include <windows.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

class PipeClient
{
public:
    PipeClient() = default;
    ~PipeClient();

    // 设置服务端 exe 完整路径（用于自动拉起/安装服务）
    void SetServicePath(const std::wstring& exePath) { m_servicePath = exePath; }

    // 连通性测试；retries 为失败时的重试次数
    bool Ping(int retries = 1);

    // 发送一条命令（REQ 帧），返回响应文本；失败返回空串
    std::wstring Request(const std::wstring& line);

    // 服务未运行时尝试启动（exe 与客户端同目录）
    bool LaunchService();

    // 确保系统服务已安装并运行（必要时提权 -install），并等待全局
    // 管道就绪。返回 true 表示服务已就绪。
    bool EnsureService();

    // 事件回调：收到 EVT 帧时在后台 reader 线程调用
    void SetEventCallback(std::function<void(const std::wstring&)> cb)
        { m_onEvent = std::move(cb); }
    // 断开回调：连接断开时在后台 reader 线程调用（实时检测服务死亡）
    void SetDisconnectCallback(std::function<void()> cb)
        { m_onDisconnect = std::move(cb); }

    // 是否已建立持久连接
    bool IsConnected() const { return m_connected.load(); }

    // 关闭连接并回收 reader 线程（析构/停止时调用）
    void Close();

private:
    void ReaderThread();                 // 后台读帧线程
    void HandleDisconnect(HANDLE h);     // 断开清理（reader 线程）
    bool Connect();                      // 打开管道 + 派生 reader

    std::wstring m_servicePath;

    std::mutex                     m_mutex;                  // 保护 m_pipe / 待响应映射
    std::condition_variable        m_cv;
    HANDLE                         m_pipe = INVALID_HANDLE_VALUE; // 受 m_mutex 保护
    std::thread                    m_reader;
    std::atomic<unsigned long long> m_nextId{ 1 };
    std::map<unsigned long long, std::wstring> m_pending;      // id → 响应
    std::map<unsigned long long, bool>         m_pendingDone;  // id → 已完成
    std::atomic<bool> m_connected{ false };
    std::atomic<bool> m_closing{ false };

    std::function<void(const std::wstring&)> m_onEvent;
    std::function<void()> m_onDisconnect;
};