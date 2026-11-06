// ============================================================
// AireRecord 客户端 (airecord.exe) — UI 入口
//
// C/S 架构的 Client：所有数据操作走命名管道发给服务端
// (airecordd.exe)。本进程崩溃/关闭不影响后台记录。
// 启动逻辑：单实例 → 自动拉起服务端（未运行时）→ 显示 UI
// ============================================================
#include "core/App.h"
#include "core/DiagLog.h"
#include "core/MachineConfig.h"
#include "plugins/SearchPlugin.h"
#include "plugins/ManualEntryPlugin.h"
#include <memory>
#include <shellapi.h>
#include <stdlib.h>

// 启用 Common Controls v6（视觉样式 + 输入框 cue banner 占位文字）
#pragma comment(linker, "/manifestdependency:\"type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// 客户端内置插件：界面层功能在此追加
static void RegisterClientPlugins(App& app)
{
    app.Plugins().Register(std::make_unique<SearchPlugin>());      // F3 关键字自动查询（250ms 防抖）
    app.Plugins().Register(std::make_unique<ManualEntryPlugin>()); // F4 手动录入保存
}

static std::wstring ExeDir()
{
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring dir = self;
    size_t slash = dir.find_last_of(L'\\');
    dir.resize(slash == std::wstring::npos ? 0 : slash);
    return dir;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

    // ---- 启动参数 ----
    //   --hidden            静默启动：窗口建好但不显示（由热键/托盘唤出）
    //   --show-after=<ms>   延迟显示：先隐藏建窗，ms 后再显示
    //                       不传则读机器级配置 [service] defer_show_ms（默认 0）
    //
    // 为什么要延迟显示：部分终端管控/DLP 会在进程启动后约 0.3~0.6s 检查
    // "新出现的可见窗口"并投递 WM_CLOSE 后强杀；隐藏建窗、>600ms 再显示
    // 可稳定通过（实测 300ms 被杀、600ms 起存活）。
    bool startHidden = false;
    int  showAfterMs = -1;
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i < argc; ++i) {
            if (_wcsicmp(argv[i], L"--hidden") == 0)
                startHidden = true;
            else if (_wcsnicmp(argv[i], L"--show-after=", 13) == 0)
                showAfterMs = _wtoi(argv[i] + 13);
        }
        if (argv) LocalFree(argv);
    }
    if (showAfterMs < 0)   // 机器级配置兜底（同一台机器所有会话统一）
        showAfterMs = _wtoi(machine::Get(L"service", L"defer_show_ms", L"0").c_str());

    diag::Log(L"[start] session=%lu hidden=%d delay=%d exe-dir=%ls",
              diag::SessionId(), (int)startHidden, showAfterMs,
              ExeDir().c_str());

    // 未处理异常：先落盘再交给系统（现场"闪一下就没了"时靠这条定位）
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* ep) -> LONG {
        if (ep && ep->ExceptionRecord)
            diag::Log(L"[crash] code=0x%08lX addr=%p",
                      ep->ExceptionRecord->ExceptionCode,
                      ep->ExceptionRecord->ExceptionAddress);
        return EXCEPTION_CONTINUE_SEARCH;
    });

    // 单实例：已运行则激活已有窗口
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\AireRecordUiMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND exist = FindWindowW(L"AireRecordWnd", nullptr);
        diag::Log(L"[second-instance] existing=%p", (void*)exist);
        if (exist) {
            ShowWindow(exist, SW_RESTORE);
            SetForegroundWindow(exist);
        }
        if (mutex) CloseHandle(mutex);
        return 0;
    }

    App app;
    app.SetServicePath(ExeDir() + L"\\airecordd.exe");
    app.SetStartHidden(startHidden);
    app.SetShowDelayMs(showAfterMs);
    RegisterClientPlugins(app);
    int rc = app.Run(hInstance);
    diag::Log(L"[exit] rc=%d", rc);
    return rc;
}
