// ============================================================
// AgentHost.cpp — 会话剪贴板代理实现（合并进 airecord.exe）
//
// 机器级共享数据库归 Session 0 系统服务独占。本类的职责收窄为
// "本会话的剪贴板监听 + 转发"：在独立工作线程的隐藏窗口上监听
// 剪贴板变化，抓到新文本后经全局管道 SAVE 转发给服务入库。
// 不再打开数据库、不再运行 PipeServer、不再分发 IPC 命令。
// ============================================================
#include "AgentHost.h"
#include "../ipc/Ipc.h"
#include "../ipc/PipeClient.h"
#include "../core/DiagLog.h"
#include "../core/MachineConfig.h"
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

static const wchar_t* kSvcWndClass = L"AireRecordAgentSvcWnd";
static const int      kHotkeyId    = 1;          // Ctrl+Shift+V
static const UINT     WM_TRAYICON  = WM_APP + 1;

enum TrayCmd {
    CMD_OPEN_UI   = 100,
    CMD_AUTOSTART = 101,
    CMD_EXIT      = 102,
};

AgentHost::AgentHost() = default;
AgentHost::~AgentHost()
{
    Stop();
}

// ---------- 自启状态（查询系统服务，普通权限可查询） ----------
static bool IsServiceInstalled()
{
    SC_HANDLE mgr = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!mgr) return false;
    SC_HANDLE svc = OpenServiceW(mgr, L"AireRecord", SERVICE_QUERY_STATUS);
    if (svc) CloseServiceHandle(svc);
    CloseServiceHandle(mgr);
    return svc != nullptr;
}

bool AgentHost::Start(HINSTANCE hInstance, HWND uiWindow,
                      std::function<void()> onExitRequest)
{
    m_hInst = hInstance;
    m_uiWindow = uiWindow;
    m_onExitRequest = std::move(onExitRequest);

    // 本 exe 路径
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    m_selfPath = self;
    size_t slash = m_selfPath.find_last_of(L'\\');
    std::wstring dir = slash == std::wstring::npos
        ? L"." : m_selfPath.substr(0, slash);

    // 服务 exe 永远是 airecordd.exe（服务安装/卸载用）
    m_serviceExe = dir + L"\\airecordd.exe";
    m_clientPath = dir + L"\\airecord.exe";

    // 机器级 [service] client_path 覆盖客户端启动路径（独立拉起用）
    std::wstring cp = machine::Get(L"service", L"client_path", L"");
    if (!cp.empty()) {
        m_clientPath = cp;
        diag::Log(L"[provision] client_path override = %ls", cp.c_str());
    }

    // 单实例互斥（会话内）：保证"一个会话只有一个剪贴板监听者"。
    // 已存在 → 本会话已有其它监听者（如残留的 airecordd -agent），
    // 本进程不做监听，UI 只走 PipeClient 连全局管道即可。
    m_mutex = CreateMutexW(nullptr, TRUE, L"Local\\AireRecordAgentMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (m_mutex) { CloseHandle(m_mutex); m_mutex = nullptr; }
        diag::Log(L"[agent-host] another clipboard listener in this session, skip");
        return false;
    }

    m_started = true;

    // 后台工作线程：隐藏窗口 + 消息循环 + 剪贴板监听/热键/托盘 + 转发
    m_thread = std::thread(&AgentHost::Run, this);
    return true;
}

void AgentHost::Wait()
{
    if (m_thread.joinable())
        m_thread.join();
}

void AgentHost::Stop()
{
    if (!m_started.exchange(false))
        return;
    if (m_hwnd)
        PostMessage(m_hwnd, WM_CLOSE, 0, 0);
    if (m_thread.joinable() && GetCurrentThreadId() != m_agentThreadId)
        m_thread.join();
    if (m_mutex) { ReleaseMutex(m_mutex); CloseHandle(m_mutex); m_mutex = nullptr; }
}

// ============================================================
// 后台工作线程
// ============================================================
void AgentHost::Run()
{
    m_agentThreadId = GetCurrentThreadId();
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // 隐藏窗口（托盘/热键/剪贴板消息宿主）
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = &AgentHost::SvcWndProc;
    wc.hInstance     = m_hInst;
    wc.lpszClassName = kSvcWndClass;
    RegisterClassExW(&wc);
    m_hwnd = CreateWindowExW(0, kSvcWndClass, L"AireRecordAgent",
                             WS_OVERLAPPED, 0, 0, 0, 0,
                             nullptr, nullptr, m_hInst, this);

    m_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    diag::Log(L"[agent-host] hwnd=%p session=%lu", (void*)m_hwnd, diag::SessionId());
    AddTrayIcon();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    m_hwnd = nullptr;
    CoUninitialize();
}

// 通知客户端窗口刷新列表（客户端收到后 300ms 防抖）
void AgentHost::NotifyClientRefresh()
{
    HWND client = FindWindowW(ipc::kUiWindowClass, nullptr);
    if (client) PostMessageW(client, WM_APP + 2, 0, 0);
}

void AgentHost::NotifyUi(bool ok, const wchar_t* title, const wchar_t* message)
{
    diag::Log(L"[ui-notify] ok=%d title=%ls msg=%ls",
              (int)ok, title, message);
    HWND client = FindWindowW(ipc::kUiWindowClass, nullptr);
    if (!client) return;

    const std::wstring payload = (ok ? L"1\t" : L"0\t") +
                                 std::wstring(title) + L"\t" + message;
    COPYDATASTRUCT cds{};
    cds.dwData = ipc::kUiToastTag;
    cds.cbData = (DWORD)((payload.size() + 1) * sizeof(wchar_t));
    cds.lpData = const_cast<wchar_t*>(payload.c_str());
    DWORD_PTR ignored = 0;
    SendMessageTimeoutW(client, WM_COPYDATA, 0,
                        reinterpret_cast<LPARAM>(&cds),
                        SMTO_ABORTIFHUNG, 1000, &ignored);
}

// 把一条剪贴板记录经全局管道 SAVE 转发给系统服务。若管道未就绪，
// PipeClient::Request 会尝试拉起/启动系统服务；这里小循环重试几次，
// 仍失败则丢弃（记录日志）。
bool AgentHost::ForwardClipboard(const std::wstring& text)
{
    // 服务端 exe 与本 exe 同目录（airecordd.exe）
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring selfPath = self;
    size_t slash = selfPath.find_last_of(L'\\');
    std::wstring dir = slash == std::wstring::npos
        ? L"." : selfPath.substr(0, slash);
    std::wstring svcExe = dir + L"\\airecordd.exe";

    std::wstring cmd = L"SAVE\tClipboard\t\t" + ipc::EscapeField(text);

    PipeClient client;
    client.SetServicePath(svcExe);
    for (int i = 0; i < 4; ++i) {
        std::wstring resp = client.Request(cmd);
        if (resp.rfind(L"OK", 0) == 0)
            return true;
        Sleep(300);
    }
    diag::Log(L"[agent-host] forward clipboard failed, text_len=%zu",
              text.size());
    return false;
}

void AgentHost::OnClipboardUpdate()
{
    std::wstring text;
    bool opened = false;
    for (int i = 0; i < 5 && !opened; ++i) {
        if (OpenClipboard(m_hwnd)) opened = true;
        else Sleep(20);
    }
    if (!opened) return; // 静默跳过

    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* p = (const wchar_t*)GlobalLock(h);
        if (p) { text = p; GlobalUnlock(h); }
    }
    CloseClipboard();
    if (text.empty()) return;

    if (!m_lastInternalCopy.empty() && text == m_lastInternalCopy) {
        m_lastInternalCopy.clear(); // 本代理自己写入的文本 → 不转发
        return;
    }
    if (ForwardClipboard(text))
        NotifyClientRefresh();
}

// ---------- 托盘 ----------
void AgentHost::AddTrayIcon()
{
    m_nid.cbSize           = sizeof(m_nid);
    m_nid.hWnd             = m_hwnd;
    m_nid.uID              = 1;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = (HICON)LoadImageW(m_hInst, MAKEINTRESOURCEW(1),
                                    IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    if (!m_nid.hIcon) m_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    StringCchCopyW(m_nid.szTip, ARRAYSIZE(m_nid.szTip),
                   L"AireRecord 后台服务（剪贴板记录中）");
    Shell_NotifyIconW(NIM_ADD, &m_nid);
}

void AgentHost::ShowTrayMenu()
{
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, CMD_OPEN_UI, L"打开主界面");
    AppendMenuW(menu, MF_STRING | (IsServiceInstalled() ? MF_CHECKED : 0),
                CMD_AUTOSTART, L"随系统自动启动（系统服务）");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CMD_EXIT, L"退出");

    // TrackPopupMenu 前必须前置窗口，否则菜单不消失
    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(menu,
                             TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (cmd) {
    case CMD_OPEN_UI: ToggleUiWindow(); break;
    case CMD_AUTOSTART: {
        bool on = IsServiceInstalled();
        int ret = (int)ShellExecuteW(nullptr, L"runas", m_serviceExe.c_str(),
                                     (!on) ? L"-install" : L"-uninstall",
                                     nullptr, SW_HIDE);
        if (ret <= 32) {
            NotifyUi(false, L"未能更改自启动",
                     L"系统权限请求被取消或失败，设置未生效。");
        }
        break;
    }
    case CMD_EXIT:
        if (m_onExitRequest) {
            m_onExitRequest();       // 合并模式：交给 UI 强制退出
        } else {
            PostMessage(m_hwnd, WM_CLOSE, 0, 0);  // 结束后台消息循环
        }
        break;
    }
}

// 唤起客户端主界面：合并模式直接显示/隐藏同进程 UI 窗口；
// 独立模式（无 uiWindow）则拉起独立客户端进程。
void AgentHost::LaunchClient()
{
    std::wstring args = machine::Get(L"service", L"client_args", L"");
    std::wstring via  = machine::Get(L"service", L"launch_via", L"direct");

    HINSTANCE r;
    if (_wcsicmp(via.c_str(), L"explorer") == 0) {
        std::wstring quoted = L"\"" + m_clientPath + L"\"";
        r = ShellExecuteW(m_hwnd, L"open", L"explorer.exe", quoted.c_str(),
                          nullptr, SW_SHOWNORMAL);
    } else {
        r = ShellExecuteW(m_hwnd, L"open", m_clientPath.c_str(),
                          args.empty() ? nullptr : args.c_str(),
                          nullptr, SW_SHOWNORMAL);
    }
    diag::Log(L"[launch-client] session=%lu via=%ls path=%ls args='%ls' ret=%p",
              diag::SessionId(), via.c_str(), m_clientPath.c_str(),
              args.c_str(), (void*)r);
}

void AgentHost::ToggleUiWindow()
{
    if (m_uiWindow) {
        if (IsWindowVisible(m_uiWindow)) {
            ShowWindow(m_uiWindow, SW_HIDE);   // 可见 → 隐藏
        } else {
            ShowWindow(m_uiWindow, SW_RESTORE);
            SetForegroundWindow(m_uiWindow);   // 隐藏 → 显示并前置
        }
    } else {
        LaunchClient();                        // 独立模式：拉起客户端进程
    }
}

// ---------- 代理窗口过程 ----------
LRESULT CALLBACK AgentHost::SvcWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AgentHost* self = reinterpret_cast<AgentHost*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<AgentHost*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self && self->HandleWindowMessage(hwnd, msg, wParam, lParam))
        return 0;                       // 已处理
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool AgentHost::HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == m_taskbarCreated) { // Explorer 重启后恢复托盘图标
        AddTrayIcon();
        return true;
    }
    switch (msg) {
    case WM_CREATE:
        AddClipboardFormatListener(hwnd);
        RegisterHotKey(hwnd, kHotkeyId, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'V');
        return true;

    case WM_CLIPBOARDUPDATE:
        OnClipboardUpdate();
        return true;

    case WM_HOTKEY:
        if ((int)wParam == kHotkeyId)
            ToggleUiWindow();
        return true;

    case WM_TRAYICON:
        if (lParam == WM_LBUTTONDBLCLK)
            ToggleUiWindow();
        else if (lParam == WM_RBUTTONUP)
            ShowTrayMenu();
        return true;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return true;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        UnregisterHotKey(hwnd, kHotkeyId);
        PostQuitMessage(0);
        return true;
    }
    return false;
}