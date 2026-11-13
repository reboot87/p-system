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
// 会话代理逻辑已抽取到 AgentHost（src/agent）：代理运行在独立
// 工作线程 + 自己的隐藏窗口 + 消息循环，既可作为 airecord.exe
// 合并模式的同进程后台，也可作为本 exe 的 -agent 独立模式。
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
#include "../core/DiagLog.h"
#include "../core/MachineConfig.h"
#include "../agent/AgentHost.h"

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

static std::wstring g_selfPath;                   // 本 exe 完整路径

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
// 第一部分：会话代理
//
// 一次性结果交给主界面的自绘 Toast。服务端 / 提权进程绝不弹系统
// MessageBox：Session 0 没有交互桌面，弹窗会卡住服务线程而且用户
// 永远看不到；安装器是提升进程，风格也应该跟客户端保持一致。
// 提升 → 普通完整性方向发 WM_COPYDATA 是 UIPI 允许的。
// ============================================================
static void NotifyUi(bool ok, const wchar_t* title, const wchar_t* message)
{
    diag::Log(L"[ui-notify] ok=%d title=%ls msg=%ls",
              (int)ok, title, message);
    HWND client = FindWindowW(ipc::kUiWindowClass, nullptr);
    if (!client) return;   // 主界面没开：日志 + 进程返回码即结果

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

// 会话代理主流程：委托给 AgentHost（独立模式，无同进程 UI 窗口）。
// 代理在 AgentHost 的后台线程上运行自己的隐藏窗口与消息循环。
static int RunAgent(HINSTANCE hInstance)
{
    // ---- 客户端部署/投放（适配虚拟化桌面、沙箱环境）----
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
    }

    AgentHost host;
    // 独立代理：uiWindow 传 nullptr（托盘/热键唤起独立客户端），
    // 无 onExitRequest（托盘退出直接结束后台消息循环）。
    bool served = host.Start(hInstance, nullptr, nullptr);
    if (!served)
        return 0;   // 外部代理已在服务本会话管道，直接退出
    host.Wait();    // 阻塞直到代理退出（托盘"退出"）
    host.Stop();
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
        NotifyUi(false, L"服务启动失败", msg);
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
        NotifyUi(false, L"安装服务失败", msg);
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

    NotifyUi(started, L"AireRecord 安装服务",
        started ? L"系统服务安装成功，已随系统自动启动，后台记录将在每次登录后自动运行。"
                : L"系统服务已注册，但启动失败（可能已有代理在运行）。");
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

    NotifyUi(true, L"AireRecord 卸载服务",
             L"系统服务已卸载，已恢复为普通后台运行。");
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