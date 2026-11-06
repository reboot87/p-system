// ============================================================
// AireRecord 客户端 (airecord.exe) — UI 入口
//
// C/S 架构的 Client：所有数据操作走命名管道发给服务端
// (airecordd.exe)。本进程崩溃/关闭不影响后台记录。
// 启动逻辑：单实例 → 自动拉起服务端（未运行时）→ 显示 UI
// ============================================================
#include "core/App.h"
#include "core/DiagLog.h"
#include "plugins/SearchPlugin.h"
#include "plugins/ManualEntryPlugin.h"
#include <memory>

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

    diag::Log(L"[start] session=%lu exe-dir=%ls", diag::SessionId(),
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
    RegisterClientPlugins(app);
    int rc = app.Run(hInstance);
    diag::Log(L"[exit] rc=%d", rc);
    return rc;
}
