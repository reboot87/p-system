// ============================================================
// AireRecord 服务端 (airecordd.exe) — SCM 系统服务 + 会话代理
//
// 运行模式（按命令行参数）：
//   -service    作为 Windows 系统服务运行（Session 0，SYSTEM 账户）
//               职责：随系统启动、监听会话通知、在每个登录用户的
//               会话内拉起"会话代理"、代理崩溃后自动重启（守护）
//   -agent      会话代理（用户会话内）：托盘常驻、剪贴板监听入库、
//               全局热键、命名管道 IPC 服务、SQLite（用户级数据）
//   -install    注册系统服务（自动启动）并立即启动，清理旧版自启
//   -uninstall  停止并删除服务，清理旧版自启，重新拉起代理
//   (无参数)    以会话代理运行（兼容客户端的自动拉起逻辑）
//
// 为什么拆成"服务 + 代理"：Session 0 隔离下无法使用托盘、全局
// 热键与剪贴板监听（都属于交互会话），故由服务代管生命周期，
// 实际功能在用户会话内的代理进程中执行。
// ============================================================
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <taskschd.h>
#include <comdef.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <vector>
#include <algorithm>
#include "../ipc/Ipc.h"
#include "../ipc/PipeServer.h"
#include "../core/Database.h"
#include "../core/DiagLog.h"
#include "../core/MachineConfig.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "userenv.lib")

// 启用 Common Controls v6（视觉样式）
#pragma comment(linker, "/manifestdependency:\"type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static const wchar_t* kSvcName    = L"AireRecord";
static const wchar_t* kSvcWndClass= L"AireRecordSvcWnd";
static const int      kHotkeyId   = 1;          // Ctrl+Shift+V
static const UINT     WM_TRAYICON = WM_APP + 1;

enum TrayCmd {
    CMD_OPEN_UI  = 100,
    CMD_AUTOSTART= 101,
    CMD_EXIT     = 102,
};

static std::wstring g_selfPath;                   // 本 exe 完整路径
static std::wstring g_clientPath;                 // airecord.exe 完整路径

static std::wstring ExeDir()
{
    size_t slash = g_selfPath.find_last_of(L'\\');
    return slash == std::wstring::npos ? L"." : g_selfPath.substr(0, slash);
}

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

// ============================================================
// 第一部分：会话代理（原交互服务端逻辑，运行在用户会话内）
// ============================================================
static HWND         g_hwnd    = nullptr;
static HINSTANCE    g_hInst   = nullptr;
static Database     g_db;
static std::wstring g_dbPath;                      // 数据库文件路径（STATS 用）
static PipeServer   g_server;
static NOTIFYICONDATAW g_nid  = {};
static UINT         s_taskbarCreated = 0;
static std::wstring g_lastInternalCopy;           // 应用内复制过滤（COPY 命令）

// 通知客户端窗口刷新列表（客户端收到后 300ms 防抖）
static void NotifyClientRefresh()
{
    HWND client = FindWindowW(L"AireRecordWnd", nullptr);
    if (client) PostMessageW(client, WM_APP + 2, 0, 0);
}

// ---------- 剪贴板（代理自己的 COPY → 过滤不入库） ----------
static bool SetClipboardTextSvc(const std::wstring& text)
{
    if (!OpenClipboard(g_hwnd)) return false;
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
    if (ok) g_lastInternalCopy = text;
    return ok;
}

static void OnClipboardUpdate()
{
    std::wstring text;
    bool opened = false;
    for (int i = 0; i < 5 && !opened; ++i) {
        if (OpenClipboard(g_hwnd)) opened = true;
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

    if (!g_lastInternalCopy.empty() && text == g_lastInternalCopy) {
        g_lastInternalCopy.clear(); // 本代理自己的 COPY → 过滤
        return;
    }
    if (g_db.SaveEntry(text, L"Clipboard"))
        NotifyClientRefresh();
}

// ---------- 自启状态（查询系统服务，普通权限可查询） ----------
static bool IsServiceInstalled()
{
    SC_HANDLE mgr = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!mgr) return false;
    SC_HANDLE svc = OpenServiceW(mgr, kSvcName, SERVICE_QUERY_STATUS);
    if (svc) CloseServiceHandle(svc);
    CloseServiceHandle(mgr);
    return svc != nullptr;
}

static void LaunchClient()
{
    // 客户端启动参数来自机器级配置 [service] client_args（默认空）
    //   例：--hidden 静默启动（新建可见窗口会被管控策略关闭的机器上使用）
    std::wstring args = machine::Get(L"service", L"client_args", L"");
    // 启动方式：direct（默认，本进程直接拉起）
    //           explorer（由桌面壳 explorer.exe 代为启动）
    // 终端管控/DLP 常把"后台服务进程直接弹窗"判为可疑；改由 explorer 启动后
    // 客户端的父进程是桌面壳，与用户双击打开的行为一致，通常可放行。
    std::wstring via  = machine::Get(L"service", L"launch_via", L"direct");

    HINSTANCE r;
    if (_wcsicmp(via.c_str(), L"explorer") == 0) {
        std::wstring quoted = L"\"" + g_clientPath + L"\"";
        r = ShellExecuteW(g_hwnd, L"open", L"explorer.exe", quoted.c_str(),
                          nullptr, SW_SHOWNORMAL);
    } else {
        r = ShellExecuteW(g_hwnd, L"open", g_clientPath.c_str(),
                          args.empty() ? nullptr : args.c_str(),
                          nullptr, SW_SHOWNORMAL);
    }
    // 客户端"闪一下就没了"时，先确认是没拉起来还是拉起来后退出的
    diag::Log(L"[launch-client] session=%lu via=%ls path=%ls args='%ls' ret=%p",
              diag::SessionId(), via.c_str(), g_clientPath.c_str(),
              args.c_str(), (void*)r);
}

// ---------- 托盘 ----------
static void AddTrayIcon()
{
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(1),
                                    IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    if (!g_nid.hIcon) g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip),
                   L"AireRecord 后台服务（剪贴板记录中）");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void ShowTrayMenu()
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
    SetForegroundWindow(g_hwnd);
    int cmd = TrackPopupMenu(menu,
                             TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, g_hwnd, nullptr);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (cmd) {
    case CMD_OPEN_UI: LaunchClient(); break;
    case CMD_AUTOSTART: {
        bool on = IsServiceInstalled();
        g_db.SetSetting(L"autostart", (!on) ? L"1" : L"0");
        int ret = (int)ShellExecuteW(nullptr, L"runas", g_selfPath.c_str(),
                                     (!on) ? L"-install" : L"-uninstall",
                                     nullptr, SW_HIDE);
        if (ret <= 32)  // 用户拒绝 UAC 或失败 → 回滚偏好
            g_db.SetSetting(L"autostart", on ? L"1" : L"0");
        break;
    }
    case CMD_EXIT:
        SetAgentStoppedFlag(true);   // 服务守护不再自动拉起
        DestroyWindow(g_hwnd);
        break;
    }
}

// ---------- IPC 命令分发 ----------
static std::wstring HandleIpc(const std::wstring& line)
{
    auto tok = ipc::SplitTabs(line);
    if (tok.empty()) return L"ERR\t空命令";

    if (tok[0] == L"PING")
        return L"OK";

    if (tok[0] == L"LIST") {
        std::wstring filter = tok.size() > 1 ? ipc::UnescapeField(tok[1]) : L"";
        auto v = g_db.Query(filter, 500);
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
        return g_db.SaveEntry(content, source.c_str(), note)
                   ? L"OK" : L"ERR\t保存失败";
    }

    if (tok[0] == L"NOTE" && tok.size() >= 3) {
        // 修改任意条目的备注
        int64_t id = _wtoi64(tok[1].c_str());
        return g_db.UpdateNote(id, ipc::UnescapeField(tok[2]))
                   ? L"OK" : L"ERR\t备注更新失败";
    }

    if (tok[0] == L"DEL" && tok.size() >= 2) {
        int64_t id = _wtoi64(tok[1].c_str());
        return g_db.DeleteById(id) ? L"OK" : L"ERR\t删除失败";
    }

    if (tok[0] == L"UI") {
        LaunchClient();   // 唤起客户端主界面（供热键/远程触发）
        return L"OK";
    }

    if (tok[0] == L"STATS") {
        // OK\t<总数>\t<今日新增>\t<库文件字节>
        wchar_t buf[96];
        long long bytes = 0;
        HANDLE hf = CreateFileW(g_dbPath.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER sz;
            if (GetFileSizeEx(hf, &sz)) bytes = sz.QuadPart;
            CloseHandle(hf);
        }
        StringCchPrintfW(buf, 96, L"%lld\t%lld\t%lld",
                         (long long)g_db.Count(),
                         (long long)g_db.CountToday(), bytes);
        return std::wstring(L"OK\t") + buf;
    }

    if (tok[0] == L"CLEAR") {
        return g_db.ClearAll() ? L"OK" : L"ERR\t清空失败";
    }

    if (tok[0] == L"COPY" && tok.size() >= 2) {
        SetClipboardTextSvc(ipc::UnescapeField(tok[1]));
        return L"OK";
    }

    return L"ERR\t未知命令";
}

// ---------- 代理窗口过程 ----------
static LRESULT CALLBACK SvcWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == s_taskbarCreated) { // Explorer 重启后恢复托盘图标
        AddTrayIcon();
        return 0;
    }
    switch (msg) {
    case WM_CREATE:
        AddClipboardFormatListener(hwnd);
        RegisterHotKey(hwnd, kHotkeyId, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'V');
        return 0;

    case WM_CLIPBOARDUPDATE:
        OnClipboardUpdate();
        return 0;

    case WM_HOTKEY:
        if ((int)wParam == kHotkeyId)
            LaunchClient();
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_LBUTTONDBLCLK)
            LaunchClient();
        else if (lParam == WM_RBUTTONUP)
            ShowTrayMenu();
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        UnregisterHotKey(hwnd, kHotkeyId);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// 会话代理主流程
static int RunAgent(HINSTANCE hInstance)
{
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    g_hInst = hInstance;

    // 单实例（会话内）
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\AireRecordAgentMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;

    g_clientPath = ExeDir() + L"\\airecord.exe";

    // ---- 客户端部署/投放（适配虚拟化桌面、沙箱环境）----
    // [service] client_path           客户端实际启动路径（默认 = 代理同目录）
    // [service] provision_client_to   启动时把客户端复制到此目录；
    //                                 值 "desktop" 表示当前用户的桌面
    // 说明：部分环境用户桌面被重定向到会话内虚拟盘（如 P:\Users\xx\DesktopX），
    // 只在该登录会话内可见，服务/其他会话写入无效，故由代理在会话内投放。
    {
        std::wstring provTo = machine::Get(L"service", L"provision_client_to", L"");
        if (provTo == L"desktop") {
            wchar_t desk[MAX_PATH] = {};
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY,
                                           nullptr, 0, desk)))
                provTo = desk;
        }
        if (!provTo.empty()) {
            std::wstring src = ExeDir() + L"\\airecord.exe";
            std::wstring dst = provTo + L"\\airecord.exe";
            CreateDirectoryW(provTo.c_str(), nullptr);
            BOOL ok = CopyFileW(src.c_str(), dst.c_str(), FALSE);
            diag::Log(L"[provision] session=%lu src=%ls dst=%ls ok=%d gle=%lu",
                      diag::SessionId(), src.c_str(), dst.c_str(),
                      (int)ok, ok ? 0 : GetLastError());
        }
        std::wstring cp = machine::Get(L"service", L"client_path", L"");
        if (!cp.empty()) {
            g_clientPath = cp;
            diag::Log(L"[provision] client_path override = %ls", cp.c_str());
        }
    }

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
    g_db.Open(dbPath);
    g_dbPath = dbPath;

    // 机器级配置：首次运行写入默认值（不覆盖服务写入的运行模式）
    if (!machine::Exists()) {
        machine::Set(L"service", L"pipe_name", L"AireRecordIpc");
        machine::Set(L"service", L"log_level",  L"info");
        machine::Set(L"mode",    L"run_mode",   L"agent");
    }

    // 隐藏窗口（托盘/热键/剪贴板消息宿主）
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = &SvcWndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = kSvcWndClass;
    RegisterClassExW(&wc);
    g_hwnd = CreateWindowExW(0, kSvcWndClass, L"AireRecordService",
                             WS_OVERLAPPED, 0, 0, 0, 0,
                             nullptr, nullptr, hInstance, nullptr);

    s_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    AddTrayIcon();

    g_server.Start(&HandleIpc);

    MSG m;
    while (GetMessage(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    g_server.Stop();
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
    CoUninitialize();
    return 0;
}

// ============================================================
// 第二部分：旧版自启清理（计划任务 / HKCU Run 键）
// ============================================================
static void RemoveRunKey()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    RegDeleteValueW(key, L"AireRecord");
    RegCloseKey(key);
}

static void DeleteLogonTask()
{
    ITaskService* svc = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr,
            CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&svc)))
        return;
    if (SUCCEEDED(svc->Connect(_variant_t(), _variant_t(),
                               _variant_t(), _variant_t()))) {
        ITaskFolder* folder = nullptr;
        if (SUCCEEDED(svc->GetFolder(_bstr_t(L"\\"), &folder))) {
            folder->DeleteTask(_bstr_t(L"AireRecord"), 0);
            folder->Release();
        }
    }
    svc->Release();
}

// ============================================================
// 第三部分：SCM 系统服务（Session 0 守护）
// ============================================================
static SERVICE_STATUS         g_svcStatus = {};
static SERVICE_STATUS_HANDLE  g_svcHandle = nullptr;
static HANDLE                 g_stopEvent = nullptr;
static CRITICAL_SECTION       g_cs;

struct SessionProc {
    DWORD  sessionId;
    HANDLE proc;
};

static std::vector<SessionProc> g_procs;   // 会话 → 代理进程
static std::vector<DWORD>       g_stopped; // 用户主动退出代理的会话（不守护重启）

static void SetSvcStatus(DWORD state, DWORD exitCode, DWORD waitHint)
{
    if (!g_svcHandle) return;
    g_svcStatus.dwCurrentState  = state;
    g_svcStatus.dwWin32ExitCode = exitCode;
    g_svcStatus.dwWaitHint      = waitHint;
    g_svcStatus.dwCheckPoint =
        (state == SERVICE_RUNNING || state == SERVICE_STOPPED) ? 0
        : ++g_svcStatus.dwCheckPoint;
    SetServiceStatus(g_svcHandle, &g_svcStatus);
}

// 在指定用户会话内拉起代理（SYSTEM → 用户令牌）
static bool LaunchAgentInSession(DWORD sessionId)
{
    HANDLE token = nullptr;
    if (!WTSQueryUserToken(sessionId, &token))
        return false;

    // 用户环境变量（保证 %APPDATA% 指向目标用户）
    LPVOID env = nullptr;
    CreateEnvironmentBlock(&env, token, FALSE);

    std::wstring cmd = L"\"" + g_selfPath + L"\" -agent";
    STARTUPINFOW si = { sizeof(si) };
    si.lpDesktop = (LPWSTR)L"winsta0\\default";
    PROCESS_INFORMATION pi = {};

    DWORD flags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT
                | CREATE_BREAKAWAY_FROM_JOB;
    BOOL ok = CreateProcessAsUserW(token, g_selfPath.c_str(), &cmd[0],
                                   nullptr, nullptr, FALSE, flags,
                                   env, nullptr, &si, &pi);
    if (!ok) { // 不在作业中时 BREAKAWAY 无效，去掉重试
        flags &= ~CREATE_BREAKAWAY_FROM_JOB;
        ok = CreateProcessAsUserW(token, g_selfPath.c_str(), &cmd[0],
                                  nullptr, nullptr, FALSE, flags,
                                  env, nullptr, &si, &pi);
    }
    if (env) DestroyEnvironmentBlock(env);
    CloseHandle(token);

    if (!ok) return false;
    CloseHandle(pi.hThread);
    EnterCriticalSection(&g_cs);
    g_procs.push_back({ sessionId, pi.hProcess });
    LeaveCriticalSection(&g_cs);
    return true;
}

// 为所有活动会话补拉代理（跳过已有代理、以及用户主动退出的会话）
static void LaunchMissingAgents()
{
    WTS_SESSION_INFOW* sessions = nullptr;
    DWORD count = 0;
    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1,
                               &sessions, &count))
        return;

    EnterCriticalSection(&g_cs);
    for (DWORD i = 0; i < count; ++i) {
        if (sessions[i].State != WTSActive) continue;
        DWORD id = sessions[i].SessionId;
        bool exists = false;
        for (auto& p : g_procs)
            if (p.sessionId == id) { exists = true; break; }
        for (auto s : g_stopped)
            if (s == id) { exists = true; break; }
        if (!exists)
            LaunchAgentInSession(id);
    }
    LeaveCriticalSection(&g_cs);
    WTSFreeMemory(sessions);
}

static void RemoveSessionProc(DWORD sessionId, bool kill)
{
    EnterCriticalSection(&g_cs);
    g_procs.erase(
        std::remove_if(g_procs.begin(), g_procs.end(),
            [&](const SessionProc& p) {
                if (p.sessionId != sessionId) return false;
                if (kill) TerminateProcess(p.proc, 0);
                WaitForSingleObject(p.proc, 3000);
                CloseHandle(p.proc);
                return true;
            }),
        g_procs.end());
    LeaveCriticalSection(&g_cs);
}

static void StopAllAgents()
{
    EnterCriticalSection(&g_cs);
    for (auto& p : g_procs) {
        TerminateProcess(p.proc, 0);
        WaitForSingleObject(p.proc, 3000);
        CloseHandle(p.proc);
    }
    g_procs.clear();
    LeaveCriticalSection(&g_cs);
}

// 守护线程：代理退出 → 读标记区分"崩溃重启"与"用户主动退出"；
// 新会话 → 补拉代理
static DWORD WINAPI WatchThread(LPVOID)
{
    for (;;) {
        DWORD w = WaitForSingleObject(g_stopEvent, 4000);
        if (w == WAIT_OBJECT_0) break;

        // 回收已退出的代理
        std::vector<SessionProc> exited;
        {
            EnterCriticalSection(&g_cs);
            g_procs.erase(
                std::remove_if(g_procs.begin(), g_procs.end(),
                    [&](const SessionProc& p) {
                        if (WaitForSingleObject(p.proc, 0) != WAIT_OBJECT_0)
                            return false;
                        CloseHandle(p.proc);
                        exited.push_back(p);
                        return true;
                    }),
                g_procs.end());
            LeaveCriticalSection(&g_cs);
        }

        for (auto& p : exited) {
            if (AgentStoppedFlag()) {
                SetAgentStoppedFlag(false);       // 消费一次性标记
                EnterCriticalSection(&g_cs);
                g_stopped.push_back(p.sessionId); // 用户退出 → 不再守护
                LeaveCriticalSection(&g_cs);
            } else {
                LaunchAgentInSession(p.sessionId); // 崩溃 → 立即重启
            }
        }
        LaunchMissingAgents();
    }
    return 0;
}

static void WINAPI ServiceCtrl(DWORD ctrl, DWORD eventType,
                               LPVOID eventData, LPVOID /*context*/)
{
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
        SetSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
        if (g_stopEvent) SetEvent(g_stopEvent);
        return;
    case SERVICE_CONTROL_INTERROGATE:
        SetSvcStatus(g_svcStatus.dwCurrentState, NO_ERROR, 0);
        return;
    case SERVICE_CONTROL_SESSIONCHANGE:
        if (eventType == WTS_SESSION_LOGON) {
            DWORD sid = eventData ? *(DWORD*)eventData : 0;
            EnterCriticalSection(&g_cs);   // 新登录视为全新会话
            g_stopped.erase(std::remove(g_stopped.begin(),
                              g_stopped.end(), sid),
                            g_stopped.end());
            LeaveCriticalSection(&g_cs);
            LaunchAgentInSession(sid);
        }
        else if (eventType == WTS_SESSION_LOGOFF)
            RemoveSessionProc(eventData ? *(DWORD*)eventData : 0, false);
        return;
    default:
        break;
    }
}

static void WINAPI ServiceMain(DWORD, LPWSTR*)
{
    g_svcHandle = RegisterServiceCtrlHandlerExW(kSvcName,
                    (LPHANDLER_FUNCTION_EX)&ServiceCtrl, nullptr);
    if (!g_svcHandle) return;

    g_svcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_svcStatus.dwControlsAccepted =
        SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE;
    SetSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    InitializeCriticalSection(&g_cs);

    SetAgentStoppedFlag(false);              // 新一次开机 → 恢复记录
    machine::Set(L"mode", L"run_mode", L"scm");
    LaunchMissingAgents();

    HANDLE watch = CreateThread(nullptr, 0, &WatchThread, nullptr, 0, nullptr);

    SetSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);
    WaitForSingleObject(g_stopEvent, INFINITE);

    SetSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
    StopAllAgents();                         // 服务停止 → 连带停止代理
    if (watch) { WaitForSingleObject(watch, 5000); CloseHandle(watch); }
    CloseHandle(g_stopEvent);
    DeleteCriticalSection(&g_cs);
    SetSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

static int RunAsService()
{
    SERVICE_TABLE_ENTRYW table[] = {
        { (LPWSTR)kSvcName, &ServiceMain },
        { nullptr, nullptr },
    };
    if (!StartServiceCtrlDispatcherW(table)) {
        // 未由 SCM 启动（如手动带 -service 运行）→ 提示
        wchar_t msg[128];
        StringCchPrintfW(msg, 128,
            L"启动服务调度失败（错误码 %u）。\n"
            L"请通过系统服务管理器或 -install 安装后运行。",
            GetLastError());
        MessageBoxW(nullptr, msg, L"AireRecord", MB_ICONERROR);
        return 1;
    }
    return 0;
}

// ============================================================
// 第四部分：服务安装 / 卸载（需管理员）
// ============================================================
static int InstallService()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    SC_HANDLE mgr = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!mgr) {
        wchar_t msg[160];
        StringCchPrintfW(msg, 160,
            L"打开服务管理器失败（错误码 %u）。\n请以管理员身份运行。",
            GetLastError());
        MessageBoxW(nullptr, msg, L"AireRecord 安装服务", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    std::wstring cmd = L"\"" + g_selfPath + L"\" -service";
    SC_HANDLE svc = CreateServiceW(
        mgr, kSvcName, L"AireRecord Clipboard Service",
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        cmd.c_str(),
        nullptr, nullptr, nullptr,
        nullptr,           // LocalSystem
        nullptr);
    if (!svc && GetLastError() == ERROR_SERVICE_EXISTS)
        svc = OpenServiceW(mgr, kSvcName, SERVICE_ALL_ACCESS);

    bool started = false;
    if (svc) {
        SERVICE_STATUS st = {};
        if (QueryServiceStatus(svc, &st)) {
            if (st.dwCurrentState == SERVICE_STOPPED) {
                started = StartServiceW(svc, 0, nullptr) != FALSE;
            } else {
                // 已在运行 → 重启一次，刷新会话代理状态
                ControlService(svc, SERVICE_CONTROL_STOP, &st);
                for (int i = 0; i < 20; ++i) {
                    QueryServiceStatus(svc, &st);
                    if (st.dwCurrentState == SERVICE_STOPPED) break;
                    Sleep(300);
                }
                started = StartServiceW(svc, 0, nullptr) != FALSE;
            }
        }
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(mgr);

    // 清理旧版自启（计划任务 / Run 键），标记运行模式
    DeleteLogonTask();
    RemoveRunKey();
    machine::Set(L"mode", L"run_mode", L"scm");
    SetAgentStoppedFlag(false);

    MessageBoxW(nullptr,
        started ? L"系统服务安装成功，已随系统自动启动。\n"
                  L"后台记录将在每次登录后自动运行。"
                : L"系统服务已注册，但启动失败（可能已有代理在运行）。",
        L"AireRecord 安装服务", MB_ICONINFORMATION);
    CoUninitialize();
    return started ? 0 : 1;
}

static int UninstallService()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    SC_HANDLE mgr = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (mgr) {
        SC_HANDLE svc = OpenServiceW(mgr, kSvcName,
                        SERVICE_ALL_ACCESS);
        if (svc) {
            SERVICE_STATUS st = {};
            ControlService(svc, SERVICE_CONTROL_STOP, &st);
            for (int i = 0; i < 20; ++i) {
                QueryServiceStatus(svc, &st);
                if (st.dwCurrentState == SERVICE_STOPPED) break;
                Sleep(300);
            }
            DeleteService(svc);
            CloseServiceHandle(svc);
        }
        CloseServiceHandle(mgr);
    }
    DeleteLogonTask();
    RemoveRunKey();
    machine::Set(L"mode", L"run_mode", L"manual");

    // 服务停止会连带杀掉代理；用 explorer 中转在用户会话内
    // 重新拉起一个非提升的代理，托盘功能不中断
    std::wstring arg = L"\"" + g_selfPath + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(),
                  nullptr, SW_SHOWNORMAL);

    MessageBoxW(nullptr, L"系统服务已卸载，已恢复为普通后台运行。",
                L"AireRecord 卸载服务", MB_ICONINFORMATION);
    CoUninitialize();
    return 0;
}

// ============================================================
// 入口：按命令行参数分发模式
// ============================================================
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    // 本 exe 路径
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    g_selfPath = self;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring mode = (argc > 1) ? argv[1] : L"";
    if (argv) LocalFree(argv);

    if (mode == L"-service")   return RunAsService();
    if (mode == L"-install")   return InstallService();
    if (mode == L"-uninstall") return UninstallService();
    return RunAgent(hInstance);            // 默认：会话代理
}
