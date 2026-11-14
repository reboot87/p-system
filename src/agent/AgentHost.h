#pragma once
// ============================================================
// AgentHost.h — 会话剪贴板代理（合并进 airecord.exe 的后台代理）
//
// 背景：机器级共享数据库由 Session 0 的系统服务（airecordd.exe
// -service）独占，服务在整机全局命名管道（\\.\pipe\AireRecordIpc）
// 上监听。每个登录会话运行一个 airecord.exe，其同进程 AgentHost
// 只负责：监听本会话剪贴板 → 抓到新文本后经全局管道 SAVE 转发给
// 服务入库。AgentHost 不再拥有数据库、不再运行 PipeServer、不再
// 分发 IPC 命令（LIST/SAVE/NOTE/... 全部由服务处理）。
//
// 互斥：Local\AireRecordAgentMutex 保证"一个会话只有一个剪贴板
// 监听者"。创建成功 → 本实例启动剪贴板监听；已存在（如残留的
// airecordd -agent 仍持有）→ Start 返回 false，本进程不做监听，
// UI 只走 PipeClient 连全局管道即可。
//
// 注意：本类只用于 airecord.exe 合并模式（uiWindow 传入主界面）。
// ============================================================
#include <windows.h>
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include "../ipc/PipeClient.h"

class AgentHost
{
public:
    AgentHost();
    ~AgentHost();

    // 启动后台剪贴板代理。返回 true 表示本实例成为本会话的剪贴板
    // 监听者并启动了后台线程（隐藏窗口 + 热键 + 托盘 + 剪贴板转发）。
    // 返回 false 表示本会话已有其它监听者（互斥体被占用），此时不
    // 启动任何后台线程，UI 直接走 PipeClient 连全局管道即可。
    //   hInstance     模块实例
    //   uiWindow      主界面窗口（合并模式传入）
    //   onExitRequest 托盘「退出」时回调（由 App 注册为强制关闭主界面）
    bool Start(HINSTANCE hInstance, HWND uiWindow,
               std::function<void()> onExitRequest);

    // 本实例是否正运行剪贴板监听
    bool IsServingPipe() const { return m_started.load(); }

    // 阻塞直到后台工作线程退出
    void Wait();

    // 请求工作线程消息循环退出，join 线程，释放单实例互斥体，
    // 清理托盘 / 热键。可安全重复调用。
    void Stop();

private:
    void Run();                       // 后台工作线程入口
    bool HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnClipboardUpdate();
    bool ForwardClipboard(const std::wstring& text);
    void AddTrayIcon();
    void ShowTrayMenu();
    void NotifyClientRefresh();
    void NotifyUi(bool ok, const wchar_t* title, const wchar_t* message);
    void LaunchClient();
    void ToggleUiWindow();

    static LRESULT CALLBACK SvcWndProc(HWND, UINT, WPARAM, LPARAM);

    // ---- 后台代理状态 ----
    HWND         m_hwnd     = nullptr;
    HINSTANCE    m_hInst    = nullptr;
    NOTIFYICONDATAW m_nid   = {};
    UINT         m_taskbarCreated = 0;
    std::wstring m_lastInternalCopy;   // 应用内复制过滤（不转发自己）
    std::wstring m_selfPath;           // 本 exe 完整路径
    std::wstring m_serviceExe;         // airecordd.exe 完整路径（服务安装用）
    std::wstring m_clientPath;         // airecord.exe 完整路径（独立模式唤起）
    HWND         m_uiWindow  = nullptr;
    std::function<void()> m_onExitRequest;

    HANDLE       m_mutex  = nullptr;
    std::thread  m_thread;
    std::atomic<bool> m_started{ false };
    DWORD        m_agentThreadId = 0;
    PipeClient   m_client;              // 持久转发连接（本会话剪贴板→服务入库）
};