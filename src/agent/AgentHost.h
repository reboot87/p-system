#pragma once
// ============================================================
// AgentHost.h — 会话代理宿主（合并进 airecord.exe 后的后台代理）
//
// 背景：原来剪贴板后台代理是独立的 airecordd.exe -agent 进程。
// 合并后 airecord.exe 单进程同时跑 UI 主窗口与后台剪贴板代理，
// 代理运行在独立工作线程 + 自己的隐藏窗口 + 独立消息循环。
//
// AgentHost 拥有全部代理功能：剪贴板监听、全局热键、托盘、
// 命名管道 IPC 服务（共享同一 Database）、单实例互斥。
//
// 互斥：管道名按会话隔离。启动时创建互斥体 Local\AireRecordAgentMutex。
//   · 创建成功（ERROR_SUCCESS）→ 本实例成为管道主，启动完整代理。
//   · 已存在（ERROR_ALREADY_EXISTS）→ 外部 airecordd.exe -agent
//     （SCM 服务拉起）已在服务本会话管道 → Start 返回 false，
//     UI 走原 PipeClient 直接连外部管道即可。
//
// airecordd.exe 的 -agent 模式也通过本类运行（uiWindow 传 nullptr，
// 此时托盘/热键改用 LaunchClient 唤起独立客户端进程）。
// ============================================================
#include <windows.h>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

class PipeServer;
class Database;

class AgentHost
{
public:
    AgentHost();
    ~AgentHost();

    // 启动后台代理。返回 true 表示本实例已成为管道主并启动了完整的
    // 后台代理（剪贴板监听 / 热键 / 托盘 / PipeServer）。
    // 返回 false 表示外部代理（airecordd -agent）已持有本会话管道，
    // 此时什么都不跑，UI 通过 PipeClient 连外部管道即可。
    //   hInstance     模块实例
    //   uiWindow      主界面窗口（合并模式传入；独立代理传 nullptr）
    //   onExitRequest 托盘「退出」时回调（合并模式由 App 注册为
    //                 强制关闭主界面；独立代理传 nullptr → 结束后台循环）
    bool Start(HINSTANCE hInstance, HWND uiWindow,
               std::function<void()> onExitRequest);

    // 本实例是否正服务本会话的命名管道
    bool IsServingPipe() const { return m_servingPipe.load(); }

    // 阻塞直到后台工作线程退出（独立代理模式：托盘退出后返回）
    void Wait();

    // 请求工作线程消息循环退出，join 线程，释放单实例互斥体，
    // 清理托盘 / 热键 / PipeServer。可安全重复调用。
    void Stop();

private:
    void Run();                       // 后台工作线程入口
    bool HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    std::wstring HandleIpc(const std::wstring& line);
    bool SetClipboardTextSvc(const std::wstring& text);
    void OnClipboardUpdate();
    void AddTrayIcon();
    void ShowTrayMenu();
    void NotifyClientRefresh();
    void NotifyUi(bool ok, const wchar_t* title, const wchar_t* message);
    void LaunchClient();
    void ToggleUiWindow();

    static LRESULT CALLBACK SvcWndProc(HWND, UINT, WPARAM, LPARAM);

    // ---- 从 service/main.cpp 移入的代理全局状态 ----
    HWND         m_hwnd     = nullptr;
    HINSTANCE    m_hInst    = nullptr;
    std::unique_ptr<Database>   m_db;
    std::wstring m_dbPath;
    std::unique_ptr<PipeServer> m_server;
    NOTIFYICONDATAW m_nid   = {};
    UINT         m_taskbarCreated = 0;
    std::wstring m_lastInternalCopy;   // 应用内复制过滤（COPY 命令）
    std::wstring m_selfPath;           // 本 exe 完整路径
    std::wstring m_serviceExe;         // airecordd.exe 完整路径（服务安装用）
    std::wstring m_clientPath;         // airecord.exe 完整路径（独立模式唤起）
    HWND         m_uiWindow  = nullptr;
    std::function<void()> m_onExitRequest;

    HANDLE       m_mutex  = nullptr;
    std::thread  m_thread;
    std::atomic<bool> m_servingPipe{ false };
    std::atomic<bool> m_started   { false };
    DWORD        m_agentThreadId = 0;
};