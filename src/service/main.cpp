// ============================================================
// AireRecord 服务端 (airecordd.exe) — SCM 系统服务 + 会话代理
//
// 运行模式（按命令行参数）：
//   -service    作为 Windows 系统服务运行（Session 0，SYSTEM 账户）。
//               职责：独占机器级共享数据库 %PROGRAMDATA%\AireRecord\
//               airecord.db，并监听整机全局命名管道 \\.\pipe\AireRecordIpc
//               处理所有读/写（LIST/SAVE/NOTE/DEL/STATS/CLEAR/COPY）。
//               所有登录会话的 airecord.exe（UI + 同进程剪贴板代理）
//               都连这条全局管道，把剪贴板记录转发给服务入库。
//   -agent      会话剪贴板代理（用户会话内）：托盘常驻、监听本会话
//               剪贴板、把新文本经全局管道转发给服务入库。委托给
//               AgentHost（同进程合并模式共用）。
//   -install    注册系统服务（自动启动）并立即启动，清理旧版自启
//   -uninstall  停止并删除服务，清理旧版自启
//   (无参数)    以会话代理运行（兼容旧版自动拉起逻辑）
//
// 数据契约：机器级共享库由服务独占，不再有"每会话一个代理库"。
// 服务不再需要为每个会话拉起独立代理——每个会话的 airecord.exe
// 本身就是 UI + 剪贴板代理。
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
#include <map>
#include <atomic>
#include "../ipc/Ipc.h"
#include "../ipc/PipeServer.h"
#include "../core/DiagLog.h"
#include "../core/MachineConfig.h"
#include "../core/Database.h"
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

// ============================================================
// 服务持有的共享状态：机器级数据库 + 全局命名管道
// ============================================================
static Database      g_svcDb;
static std::wstring   g_svcDbPathInternal;
static PipeServer     g_svcServer;
static std::atomic<bool> g_stopping{ false };   // 服务停止中：不再拉起会话 UI

// 会话守护共享状态（PING 心跳写入 / WatchThread 巡检，受 g_cs 保护）。
// 定义在此早于 HandleSvcIpc，使其 PING 分支可直接记录心跳。
static CRITICAL_SECTION             g_cs;
static std::map<DWORD, ULONGLONG>   g_sessionSeen;   // sessionId → 最近心跳 GetTickCount64

static const wchar_t* SvcDbPath()
{
    return g_svcDbPathInternal.c_str();
}

// ============================================================
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

// 把文本写入本线程剪贴板（COPY 命令用；OpenClipboard(NULL) 即本线程）。
static bool SetClipboardTextSvc(const std::wstring& text)
{
    if (!OpenClipboard(nullptr)) return false;
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
    return ok;
}

// ============================================================
// 服务端 IPC 命令分发（对共享数据库 g_svcDb 操作）
// ============================================================
static std::wstring HandleSvcIpc(const std::wstring& line)
{
    auto tok = ipc::SplitTabs(line);
    if (tok.empty()) return L"ERR\t空命令";

    if (tok[0] == L"PING") {
        // 心跳可携带会话号：PING\t<sessionId> → 记录该会话最后一次心跳。
        // 无参 PING 保持向后兼容（仅探测连通性，返回 OK）。
        if (tok.size() >= 2 && !tok[1].empty()) {
            DWORD sid = (DWORD)_wtoi(tok[1].c_str());
            EnterCriticalSection(&g_cs);
            g_sessionSeen[sid] = GetTickCount64();
            LeaveCriticalSection(&g_cs);
        }
        return L"OK";
    }

    if (tok[0] == L"LIST") {
        std::wstring filter = tok.size() > 1 ? ipc::UnescapeField(tok[1]) : L"";
        auto v = g_svcDb.Query(filter, 500);
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
        bool ok = g_svcDb.SaveEntry(content, source.c_str(), note);
        if (ok)
            g_svcServer.SendEvent(L"NEWRECORD");   // 广播：所有 UI 实时刷新
        return ok ? L"OK" : L"ERR\t保存失败";
    }

    if (tok[0] == L"NOTE" && tok.size() >= 3) {
        int64_t id = _wtoi64(tok[1].c_str());
        return g_svcDb.UpdateNote(id, ipc::UnescapeField(tok[2]))
                   ? L"OK" : L"ERR\t备注更新失败";
    }

    if (tok[0] == L"DEL" && tok.size() >= 2) {
        int64_t id = _wtoi64(tok[1].c_str());
        bool ok = g_svcDb.DeleteById(id);
        if (ok)
            g_svcServer.SendEvent(L"DATA_CHANGED");   // 广播：所有 UI 实时刷新
        return ok ? L"OK" : L"ERR\t删除失败";
    }

    if (tok[0] == L"UI") {
        // 通知所有 UI 窗口刷新（客户端 300ms 防抖）
        HWND client = FindWindowW(ipc::kUiWindowClass, nullptr);
        if (client) PostMessageW(client, WM_APP + 2, 0, 0);
        return L"OK";
    }

    if (tok[0] == L"STATS") {
        wchar_t buf[96];
        long long bytes = 0;
        HANDLE hf = CreateFileW(SvcDbPath(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER sz;
            if (GetFileSizeEx(hf, &sz)) bytes = sz.QuadPart;
            CloseHandle(hf);
        }
        StringCchPrintfW(buf, 96, L"%lld\t%lld\t%lld",
                         (long long)g_svcDb.Count(),
                         (long long)g_svcDb.CountToday(), bytes);
        return std::wstring(L"OK\t") + buf;
    }

    if (tok[0] == L"CLEAR") {
        bool ok = g_svcDb.ClearAll();
        if (ok)
            g_svcServer.SendEvent(L"DATA_CHANGED");   // 广播：所有 UI 实时刷新
        return ok ? L"OK" : L"ERR\t清空失败";
    }

    if (tok[0] == L"COPY" && tok.size() >= 2) {
        SetClipboardTextSvc(ipc::UnescapeField(tok[1]));
        return L"OK";
    }

    return L"ERR\t未知命令";
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
// 第三部分：SCM 系统服务（Session 0 守护共享库 + 全局管道）
// ============================================================
static SERVICE_STATUS         g_svcStatus = {};
static SERVICE_STATUS_HANDLE  g_svcHandle = nullptr;
static HANDLE                 g_stopEvent = nullptr;

// ============================================================
// 会话守护（watchdog）状态
//
// 服务为每个活动登录会话拉起一个隐藏的 airecord.exe --hidden（UI +
// 同进程剪贴板代理）。UI 持有一条到服务端的持久连接。
// 实时断开检测（主触发）：某会话 UI 的连接断开（ReadFile 返回
// ERROR_BROKEN_PIPE）→ OnClientDisconnect 立即重启该会话 UI，不再依赖
// 心跳轮询。WatchThread 只做兜底：
//   · 新会话登录 → 自动拉起（LaunchMissingAgents）；
//   · 慢心跳超时（kHeartbeatBackstopMs，正常不触发）→ 兜底重启。
// 用户托盘「退出」→ 写入一次性 agent_stopped 标记，本会话不再拉起。
// 共享状态 g_cs / g_sessionSeen 定义在文件前部（早于 HandleSvcIpc 的
// PING 分支），此处只补充其余状态。
// ============================================================
static const ULONGLONG kHeartbeatBackstopMs = 60000;  // 兜底心跳超时（秒级，正常不触发）

struct SessionProc {
    DWORD  sessionId;
    HANDLE process;        // 已拉起的 airecord.exe 进程句柄
};

static std::vector<SessionProc>   g_procs;         // 已拉起的会话 UI 进程
static std::vector<DWORD>         g_stopped;       // 用户主动退出的会话（不再拉起）
static std::wstring               g_clientPath;    // airecord.exe 完整路径（服务同目录）

// 读取并消费一次性「用户主动退出」标记。返回 true 表示标记已设置。
// 语义：AgentHost 托盘「退出」时写 machine [runtime] agent_stopped=1。
// WatchThread 看到某会话 UI 退出时查询它：置位 → 加入 g_stopped 不再
// 拉起；否则视为崩溃 → 重启。
static bool AgentStoppedFlag()
{
    std::wstring v = machine::Get(L"runtime", L"agent_stopped", L"0");
    if (v != L"1") return false;
    machine::Set(L"runtime", L"agent_stopped", L"0");   // 消费掉（一次性）
    return true;
}

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

// 前向声明：ServiceCtrl 的会话注销分支调用（定义在其后）
static void RemoveSessionProcLocked(DWORD sessionId);

static void WINAPI ServiceCtrl(DWORD ctrl, DWORD eventType,
                               LPVOID eventData, LPVOID /*context*/)
{
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
        SetSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
        if (g_stopEvent) SetEvent(g_stopEvent);
        return;
    case SERVICE_CONTROL_SESSIONCHANGE: {
        // 会话登录/注销 → 由 WatchThread 负责拉起/退出，这里只调整
        // g_stopped 状态：登录清除"用户退出"记录（用户又回来了，重新拉起）；
        // 注销直接移除进程句柄（本会话已不在，无需再拉起）。
        if (!eventData) return;
        const auto* ns = static_cast<const WTSSESSION_NOTIFICATION*>(eventData);
        const DWORD sid = ns->dwSessionId;
        EnterCriticalSection(&g_cs);
        if (eventType == WTS_SESSION_LOGON) {
            g_stopped.erase(std::remove(g_stopped.begin(), g_stopped.end(), sid),
                            g_stopped.end());
            g_sessionSeen.erase(sid);
        } else if (eventType == WTS_SESSION_LOGOFF) {
            RemoveSessionProcLocked(sid);
        }
        LeaveCriticalSection(&g_cs);
        diag::Log(L"[svc] sessionchange type=%lu sid=%lu", (DWORD)eventType, sid);
        return;
    }
    case SERVICE_CONTROL_INTERROGATE:
        SetSvcStatus(g_svcStatus.dwCurrentState, NO_ERROR, 0);
        return;
    default:
        break;
    }
}

// ============================================================
// 会话守护（watchdog）实现
// ============================================================

// 计算会话 UI 完整路径 = 服务 exe 同目录的 airecord.exe
static std::wstring ClientExePath()
{
    if (!g_clientPath.empty()) return g_clientPath;
    std::wstring p = ExeDir() + L"\\airecord.exe";
    return p;
}

// 在指定会话（WTS 活动会话）内以该用户的令牌拉起隐藏的 airecord.exe --hidden。
// 成功则登记 g_procs 并重置该会话心跳时刻。
static bool LaunchAgentInSession(DWORD sessionId)
{
    // 会话令牌：SYSTEM 服务经 WTSQueryUserToken 取得对应用户令牌
    HANDLE token = nullptr;
    if (!WTSQueryUserToken(sessionId, &token)) {
        diag::Log(L"[watchdog] WTSQueryUserToken sid=%lu failed gle=%lu",
                  sessionId, GetLastError());
        return false;
    }
    BOOL ok = FALSE;
    void* env = nullptr;
    // 用目标会话的用户环境（避免缺 USERPROFILE / APPDATA 等导致 UI 起不来）
    if (!CreateEnvironmentBlock(&env, token, FALSE)) {
        diag::Log(L"[watchdog] CreateEnvironmentBlock sid=%lu failed gle=%lu",
                  sessionId, GetLastError());
    }
    std::wstring client = ClientExePath();
    std::wstring cmd = L"\"" + client + L"\" --hidden";

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    ok = CreateProcessAsUserW(token, client.c_str(), &cmd[0],
                              nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT,
                              env, nullptr, &si, &pi);
    if (env) DestroyEnvironmentBlock(env);
    CloseHandle(token);
    if (!ok) {
        diag::Log(L"[watchdog] CreateProcessAsUser sid=%lu path=%ls failed gle=%lu",
                  sessionId, client.c_str(), GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);

    EnterCriticalSection(&g_cs);
    g_procs.push_back({ sessionId, pi.hProcess });
    g_sessionSeen[sessionId] = GetTickCount64();
    LeaveCriticalSection(&g_cs);
    diag::Log(L"[watchdog] launched airecord --hidden sid=%lu pid=%lu",
              sessionId, pi.dwProcessId);
    return true;
}

// 枚举 WTS 活动会话，为每个"尚未拉起且用户未主动退出"的会话拉起 UI。
// 若某会话已有最近心跳（哪怕不是本服务实例拉起的，如服务重启后存活的
// 旧 UI），也视为存活而跳过，避免每轮重复拉起同会话的重复实例。
static void LaunchMissingAgents()
{
    WTS_SESSION_INFOW* info = nullptr;
    DWORD count = 0;
    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &info, &count)) {
        diag::Log(L"[watchdog] WTSEnumerateSessions failed gle=%lu", GetLastError());
        return;
    }
    const ULONGLONG now = GetTickCount64();
    for (DWORD i = 0; i < count; ++i) {
        const DWORD sid = info[i].SessionId;
        if (info[i].State != WTSActive) continue;       // 只守护活动会话
        bool already = false, stopped = false, alive = false;
        EnterCriticalSection(&g_cs);
        for (auto& p : g_procs)
            if (p.sessionId == sid) { already = true; break; }
        stopped = std::find(g_stopped.begin(), g_stopped.end(), sid)
                  != g_stopped.end();
        auto seen = g_sessionSeen.find(sid);
        if (seen != g_sessionSeen.end())
            alive = (now - seen->second < kHeartbeatBackstopMs);
        LeaveCriticalSection(&g_cs);
        // 已在 g_procs、用户主动退出、或仍在心跳（存活中）→ 跳过
        if (already || stopped || alive) continue;
        LaunchAgentInSession(sid);
    }
    WTSFreeMemory(info);
}

// 移除某会话的进程记录（不强制结束，仅收尾；注销/超时重启时用）。
static void RemoveSessionProcLocked(DWORD sessionId)
{
    for (auto it = g_procs.begin(); it != g_procs.end(); ++it) {
        if (it->sessionId == sessionId) {
            if (it->process) { CloseHandle(it->process); it->process = nullptr; }
            g_procs.erase(it);
            break;
        }
    }
}

// 重启某会话的 UI：先移除该会话既有进程记录（无论死活），再拉起新的。
// 断开回调与 WatchThread 兜底都会调它；先移除保证并发时不会重复拉起。
static void RelaunchSession(DWORD sessionId)
{
    EnterCriticalSection(&g_cs);
    for (auto it = g_procs.begin(); it != g_procs.end(); ++it) {
        if (it->sessionId == sessionId) {
            if (it->process) { CloseHandle(it->process); it->process = nullptr; }
            g_procs.erase(it);
            break;
        }
    }
    LeaveCriticalSection(&g_cs);
    LaunchAgentInSession(sessionId);
}

// ============================================================
// 实时断开回调（PipeServer 工作线程调用）
//
// 某会话 UI 的持久连接断开（进程死亡/被结束）→ 若该会话用户明确退出
// （agent_stopped 标记）则登记为不再拉起；否则立即重启该会话 UI。
// 这是取代 2s 心跳轮询的**实时**主触发：ReadFile 返回 ERROR_BROKEN_PIPE
// 即触发，无需等待 15s 心跳超时。
// ============================================================
static void OnClientDisconnect(const std::wstring& sessionId)
{
    if (sessionId.empty()) return;
    const DWORD sid = (DWORD)_wtoi(sessionId.c_str());
    diag::Log(L"[svc] client disconnected sid=%lu", sid);

    // 服务停止中（StopAllAgents 结束各会话 UI 也会触发断开）：只收尾，
    // 绝不拉起新 UI。
    if (g_stopping.load()) return;

    if (AgentStoppedFlag()) {
        // 用户主动退出 → 记录该会话，不再拉起
        EnterCriticalSection(&g_cs);
        if (std::find(g_stopped.begin(), g_stopped.end(), sid)
            == g_stopped.end())
            g_stopped.push_back(sid);
        for (auto it = g_procs.begin(); it != g_procs.end(); ++it) {
            if (it->sessionId == sid) {
                if (it->process) { CloseHandle(it->process); it->process = nullptr; }
                g_procs.erase(it);
                break;
            }
        }
        LeaveCriticalSection(&g_cs);
        diag::Log(L"[svc] user exited sid=%lu, not relaunching", sid);
    } else {
        diag::Log(L"[svc] UI disconnected sid=%lu, relaunching (real-time)", sid);
        RelaunchSession(sid);
    }
}

// 兜底巡检循环：每 ~2s 只补拉新登录会话；慢心跳超时（kHeartbeatBackstopMs，
// 正常不触发）作为最后防线。实时重启由 OnClientDisconnect 断开回调驱动。
static DWORD WINAPI WatchThread(LPVOID)
{
    for (;;) {
        // 短超时等待停止事件，服务停止时快速退出
        if (WaitForSingleObject(g_stopEvent, 2000) == WAIT_OBJECT_0)
            break;

        const ULONGLONG now = GetTickCount64();

        // 兜底：某会话连接异常未触发断开回调、且长期无心跳 → 重启
        std::vector<SessionProc> stale;
        EnterCriticalSection(&g_cs);
        for (auto it = g_procs.begin(); it != g_procs.end(); ) {
            auto seen = g_sessionSeen.find(it->sessionId);
            bool timeout = (seen == g_sessionSeen.end()) ||
                           (now - seen->second > kHeartbeatBackstopMs);
            if (timeout) {
                stale.push_back(*it);
                if (it->process) { TerminateProcess(it->process, 1); CloseHandle(it->process); it->process = nullptr; }
                it = g_procs.erase(it);
            } else {
                ++it;
            }
        }
        LeaveCriticalSection(&g_cs);
        for (auto& s : stale) {
            diag::Log(L"[watchdog] heartbeat backstop sid=%lu, relaunching",
                      s.sessionId);
            RelaunchSession(s.sessionId);
        }

        // 补拉新登录会话（断开回调之外的自动拉起路径）
        LaunchMissingAgents();
    }
    return 0;
}

// 停止所有已拉起的会话 UI 进程（服务停止时调用）。
static void StopAllAgents()
{
    EnterCriticalSection(&g_cs);
    for (auto& p : g_procs) {
        if (p.process) {
            TerminateProcess(p.process, 1);
            CloseHandle(p.process);
            p.process = nullptr;
        }
    }
    g_procs.clear();
    LeaveCriticalSection(&g_cs);
}

static void WINAPI ServiceMain(DWORD, LPWSTR*)
{
    g_svcHandle = RegisterServiceCtrlHandlerExW(kSvcName,
                    (LPHANDLER_FUNCTION_EX)&ServiceCtrl, nullptr);
    if (!g_svcHandle) return;

    g_svcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP |
                                     SERVICE_ACCEPT_SESSIONCHANGE;
    SetSvcStatus(SERVICE_START_PENDING, NO_ERROR, 5000);

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    InitializeCriticalSection(&g_cs);

    // 会话 UI 客户端路径 = 本服务 exe 同目录的 airecord.exe
    g_clientPath = ExeDir() + L"\\airecord.exe";

    machine::Set(L"mode", L"run_mode", L"scm");

    // 打开机器级共享数据库（%PROGRAMDATA%\AireRecord\airecord.db，
    // SYSTEM 可创建/读写）。旧配置首启时写入默认值。
    if (!machine::Exists()) {
        machine::Set(L"service", L"pipe_name", L"AireRecordIpc");
        machine::Set(L"service", L"log_level",  L"info");
    }
    g_svcDbPathInternal = machine::ConfigDir() + L"\\airecord.db";
    bool dbOk = g_svcDb.Open(g_svcDbPathInternal);
    diag::Log(L"[svc] open db %ls ok=%d", g_svcDbPathInternal.c_str(), (int)dbOk);

    // 监听整机全局管道，处理所有读/写命令；断开回调驱动会话 UI 实时重启
    g_svcServer.Start(&HandleSvcIpc, &OnClientDisconnect);
    diag::Log(L"[svc] pipe server on %ls", ipc::PipeName());

    // 启动会话守护线程：兜底补拉新会话 / 慢心跳超时重启（详见 WatchThread）。
    // 实时重启由 OnClientDisconnect 断开回调驱动（取代 2s 心跳轮询）。
    HANDLE watch = CreateThread(nullptr, 0, &WatchThread, nullptr, 0, nullptr);

    SetSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);
    WaitForSingleObject(g_stopEvent, INFINITE);

    SetSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
    g_stopping = true;                   // 停止中：断开回调不再拉起会话 UI
    StopAllAgents();                     // 结束所有已拉起的会话 UI
    // 等守护线程在收到停止事件后退出（最多 3s），再回收 g_cs
    if (watch) {
        WaitForSingleObject(watch, 3000);
        CloseHandle(watch);
    }
    g_svcServer.Stop();                // 停止管道服务
    g_svcDbPathInternal.clear();
    g_clientPath.clear();
    DeleteCriticalSection(&g_cs);
    CloseHandle(g_stopEvent);
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
                // 已在运行 → 重启一次，刷新服务状态
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

    // 让普通用户可读写共享库目录（服务以 SYSTEM 独占库；给 Users 权限
    // 只是兜底——万一将来有回落路径需要普通进程直接访问，也能打开）。
    // 仅在此处对目录做一次性 ACL 授权，失败无害。
    {
        std::wstring dir = machine::ConfigDir();
        std::wstring icacls =
            L"icacls \"" + dir + L"\" /grant *S-1-5-32-545:(OI)(CI)M";
        PROCESS_INFORMATION pi = {};
        STARTUPINFOW si = { sizeof(si) };
        if (CreateProcessW(nullptr, &icacls[0], nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 10000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }

    NotifyUi(started, L"AireRecord 安装服务",
        started ? L"系统服务安装成功，已随系统自动启动，后台记录将在每次登录后自动运行。"
                : L"系统服务已注册，但启动失败。");
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

    NotifyUi(true, L"AireRecord 卸载服务",
             L"系统服务已卸载。");
    CoUninitialize();
    return 0;
}

// ============================================================
// 第五部分：会话剪贴板代理（委托给 AgentHost，兼容 -agent / 无参模式）
// ============================================================
static int RunAgent(HINSTANCE hInstance)
{
    // ---- 客户端部署/投放（适配虚拟化桌面、沙箱环境）----
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
    bool served = host.Start(hInstance, nullptr, nullptr);
    if (!served)
        return 0;   // 本会话已有剪贴板监听者（airecord.exe），直接退出
    host.Wait();    // 阻塞直到代理退出（托盘"退出"）
    host.Stop();
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
    return RunAgent(hInstance);            // 默认：会话剪贴板代理
}