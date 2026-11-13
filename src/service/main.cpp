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

    if (tok[0] == L"PING")
        return L"OK";

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
        return g_svcDb.SaveEntry(content, source.c_str(), note)
                   ? L"OK" : L"ERR\t保存失败";
    }

    if (tok[0] == L"NOTE" && tok.size() >= 3) {
        int64_t id = _wtoi64(tok[1].c_str());
        return g_svcDb.UpdateNote(id, ipc::UnescapeField(tok[2]))
                   ? L"OK" : L"ERR\t备注更新失败";
    }

    if (tok[0] == L"DEL" && tok.size() >= 2) {
        int64_t id = _wtoi64(tok[1].c_str());
        return g_svcDb.DeleteById(id) ? L"OK" : L"ERR\t删除失败";
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
        return g_svcDb.ClearAll() ? L"OK" : L"ERR\t清空失败";
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
static CRITICAL_SECTION       g_cs;

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

static void WINAPI ServiceCtrl(DWORD ctrl, DWORD /*eventType*/,
                               LPVOID /*eventData*/, LPVOID /*context*/)
{
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
        SetSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
        if (g_stopEvent) SetEvent(g_stopEvent);
        return;
    case SERVICE_CONTROL_INTERROGATE:
        SetSvcStatus(g_svcStatus.dwCurrentState, NO_ERROR, 0);
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
    g_svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    SetSvcStatus(SERVICE_START_PENDING, NO_ERROR, 5000);

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    InitializeCriticalSection(&g_cs);

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

    // 监听整机全局管道，处理所有读/写命令
    g_svcServer.Start(&HandleSvcIpc);
    diag::Log(L"[svc] pipe server on %ls", ipc::PipeName());

    SetSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);
    WaitForSingleObject(g_stopEvent, INFINITE);

    SetSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
    g_svcServer.Stop();                    // 停止管道服务
    g_svcDbPathInternal.clear();
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