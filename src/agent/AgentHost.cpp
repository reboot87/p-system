// ============================================================
// AgentHost.cpp — 会话代理宿主实现
//
// 从 src/service/main.cpp 的 RunAgent / SvcWndProc / HandleIpc /
// OnClipboardUpdate / SetClipboardTextSvc / AddTrayIcon / ShowTrayMenu
// 等移入。代理在独立工作线程上跑自己的隐藏窗口与消息循环，
// 与 UI 主窗口（airecord.exe）同进程共存。
// ============================================================
#include "AgentHost.h"
#include "../ipc/Ipc.h"
#include "../ipc/PipeServer.h"
#include "../core/Database.h"
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

// 代理退出标记：代理被用户主动退出时置 1，服务守护不重启；
// 服务启动 / 安装时清 0（重启电脑后恢复记录）
static bool AgentStoppedFlag()
{
    return machine::Get(L"runtime", L"agent_stopped", L"0") == L"1";
}
static void SetAgentStoppedFlag(bool on)
{
    machine::Set(L"runtime", L"agent_stopped", on ? L"1" : L"0");
}

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

    // 服务 exe 永远是 airecordd.exe（服务安装/卸载用）；独立代理即本进程
    m_serviceExe = dir + L"\\airecordd.exe";
    m_clientPath = dir + L"\\airecord.exe";

    // 机器级 [service] client_path 覆盖客户端启动路径（独立代理拉起用）
    std::wstring cp = machine::Get(L"service", L"client_path", L"");
    if (!cp.empty()) {
        m_clientPath = cp;
        diag::Log(L"[provision] client_path override = %ls", cp.c_str());
    }

    // 单实例互斥（会话内）：已存在 → 外部代理（airecordd -agent）已服务管道
    m_mutex = CreateMutexW(nullptr, TRUE, L"Local\\AireRecordAgentMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (m_mutex) { CloseHandle(m_mutex); m_mutex = nullptr; }
        diag::Log(L"[agent-host] external agent owns pipe, skip in-process agent");
        return false;   // 外部代理已服务本会话管道
    }

    m_servingPipe = true;
    m_started = true;

    // 后台工作线程：隐藏窗口 + 消息循环 + 剪贴板/热键/托盘 + PipeServer
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

    // 用户级数据库（会话代理的数据归属，见数据分层契约）
    std::wstring dbPath;
    wchar_t* env = nullptr;
    size_t envLen = 0;
    if (!_wdupenv_s(&env, &envLen, L"APPDATA") && env) {
        dbPath = std::wstring(env) + L"\\AireRecord";
        CreateDirectoryW(dbPath.c_str(), nullptr);
        dbPath += L"\\airecord.db";
        free(env);
    }
    m_db = std::make_unique<Database>();
    m_db->Open(dbPath);
    m_dbPath = dbPath;

    // 机器级配置：首次运行写入默认值（不覆盖服务写入的运行模式）
    if (!machine::Exists()) {
        machine::Set(L"service", L"pipe_name", L"AireRecordIpc");
        machine::Set(L"service", L"log_level",  L"info");
        machine::Set(L"mode",    L"run_mode",   L"agent");
    }

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
    diag::Log(L"[agent-host] hwnd=%p", (void*)m_hwnd);
    AddTrayIcon();

    m_server = std::make_unique<PipeServer>();
    m_server->Start([this](const std::wstring& line) { return HandleIpc(line); });

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (m_server) m_server->Stop();
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

// ---------- 剪贴板（代理自己的 COPY → 过滤不入库） ----------
bool AgentHost::SetClipboardTextSvc(const std::wstring& text)
{
    if (!OpenClipboard(m_hwnd)) return false;
    bool ok = false;
    if (EmptyClipboard()) {
        SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (h) {
            void* p = GlobalLock(h);
            if (p) {
                memcpy(p, text.c_str(), bytes);
                GlobalUnlock(h);
                ok = SetClipboardData(CF_UNICODETEXT, h) != nullptr;
            }
            if (!ok) GlobalFree(h);
        }
    }
    CloseClipboard();
    if (ok) m_lastInternalCopy = text;
    return ok;
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
        m_lastInternalCopy.clear(); // 本代理自己的 COPY → 过滤
        return;
    }
    if (m_db && m_db->SaveEntry(text, L"Clipboard"))
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
        if (m_db) m_db->SetSetting(L"autostart", (!on) ? L"1" : L"0");
        int ret = (int)ShellExecuteW(nullptr, L"runas", m_serviceExe.c_str(),
                                     (!on) ? L"-install" : L"-uninstall",
                                     nullptr, SW_HIDE);
        if (ret <= 32) {
            if (m_db) m_db->SetSetting(L"autostart", on ? L"1" : L"0");
            NotifyUi(false, L"未能更改自启动",
                     L"系统权限请求被取消或失败，设置已回滚。");
        }
        break;
    }
    case CMD_EXIT:
        SetAgentStoppedFlag(true);   // 服务守护不再自动拉起
        if (m_onExitRequest) {
            m_onExitRequest();       // 合并模式：交给 UI 强制退出
        } else {
            PostMessage(m_hwnd, WM_CLOSE, 0, 0);  // 独立代理：结束本消息循环
        }
        break;
    }
}

// 唤起客户端主界面：合并模式直接显示/隐藏同进程 UI 窗口；
// 独立代理（无 uiWindow）则拉起独立客户端进程。
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
        LaunchClient();                        // 独立代理：拉起客户端进程
    }
}

// ---------- IPC 命令分发 ----------
std::wstring AgentHost::HandleIpc(const std::wstring& line)
{
    auto tok = ipc::SplitTabs(line);
    if (tok.empty()) return L"ERR\t空命令";

    if (tok[0] == L"PING")
        return L"OK";

    if (tok[0] == L"LIST") {
        std::wstring filter = tok.size() > 1 ? ipc::UnescapeField(tok[1]) : L"";
        auto v = m_db->Query(filter, 500);
        std::wstring out = L"OK";
        wchar_t idbuf[32];
        for (auto& e : v) {
            StringCchPrintfW(idbuf, 32, L"%lld", (long long)e.id);
            out += L"\n";
            out += idbuf;
            out += L"\t" + e.source;
            out += L"\t" + e.createdAt;
            out += L"\t" + ipc::EscapeField(e.note);
            out += L"\t" + ipc::EscapeField(e.content);
        }
        out += L"\nEND";
        return out;
    }

    if (tok[0] == L"SAVE" && tok.size() >= 4) {
        std::wstring source  = tok[1];
        std::wstring note    = ipc::UnescapeField(tok[2]);
        std::wstring content = ipc::UnescapeField(tok[3]);
        return m_db->SaveEntry(content, source.c_str(), note)
                   ? L"OK" : L"ERR\t保存失败";
    }

    if (tok[0] == L"NOTE" && tok.size() >= 3) {
        int64_t id = _wtoi64(tok[1].c_str());
        return m_db->UpdateNote(id, ipc::UnescapeField(tok[2]))
                   ? L"OK" : L"ERR\t备注更新失败";
    }

    if (tok[0] == L"DEL" && tok.size() >= 2) {
        int64_t id = _wtoi64(tok[1].c_str());
        return m_db->DeleteById(id) ? L"OK" : L"ERR\t删除失败";
    }

    if (tok[0] == L"UI") {
        ToggleUiWindow();   // 唤起主界面（合并模式显示/隐藏同进程窗口）
        return L"OK";
    }

    if (tok[0] == L"STATS") {
        wchar_t buf[96];
        long long bytes = 0;
        HANDLE hf = CreateFileW(m_dbPath.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER sz;
            if (GetFileSizeEx(hf, &sz)) bytes = sz.QuadPart;
            CloseHandle(hf);
        }
        StringCchPrintfW(buf, 96, L"%lld\t%lld\t%lld",
                         (long long)m_db->Count(),
                         (long long)m_db->CountToday(), bytes);
        return std::wstring(L"OK\t") + buf;
    }

    if (tok[0] == L"CLEAR") {
        return m_db->ClearAll() ? L"OK" : L"ERR\t清空失败";
    }

    if (tok[0] == L"COPY" && tok.size() >= 2) {
        SetClipboardTextSvc(ipc::UnescapeField(tok[1]));
        return L"OK";
    }

    return L"ERR\t未知命令";
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