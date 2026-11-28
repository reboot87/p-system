// test_app_smoke.cpp — airecord.exe 的进程级冒烟测试（进 ctest）
//
// 只验证"真实客户端能起来"这一层：进程存活 → 主窗口建出来 → 消息循环
// 在跑（WM_NULL 有应答）→ 自绘控件已创建。不做 IPC 断言，所以不需要
// 服务端、数据库或真实的剪贴板数据。
//
// 为什么要 --no-service：正常启动会 EnsureService()，服务未安装时用
// ShellExecuteW("runas") 弹 UAC 并注册系统服务。在 CI / 无人值守环境里
// 这既跑不过也不该发生，所以测试固定带 --no-service 走离线态。
//
// 为什么不发 WM_CLOSE 收尾：默认关闭策略是 hide（只隐藏窗口、进程存活），
// 发 WM_CLOSE 不会退出。冒烟只关心"起来且能响应"，收尾用 TerminateProcess。
#include "ipc/Ipc.h"

#include <windows.h>
#include <stdio.h>
#include <string>

namespace
{

struct WindowSearch
{
    DWORD pid;
    HWND  found;
};

BOOL CALLBACK FindByPid(HWND hwnd, LPARAM lp)
{
    auto* search = reinterpret_cast<WindowSearch*>(lp);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != search->pid)
        return TRUE;   // 别的进程的窗口，继续找

    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, 64);
    if (_wcsicmp(cls, ipc::kUiWindowClass) == 0)
    {
        search->found = hwnd;
        return FALSE;  // 找到了，停止枚举
    }
    return TRUE;
}

BOOL CALLBACK CountChild(HWND, LPARAM lp)
{
    ++*reinterpret_cast<int*>(lp);
    return TRUE;
}

// 终止子进程并等它真正消失；返回 false 表示 5s 内仍未结束。
bool TerminateAndWait(PROCESS_INFORMATION& pi)
{
    if (!TerminateProcess(pi.hProcess, 0))
    {
        // 进程可能已经自己退出，这里不算错误；真正的判据是下面的 wait。
        std::printf("test_app_smoke: note (TerminateProcess gle=%lu)\n", GetLastError());
    }
    return WaitForSingleObject(pi.hProcess, 5000) == WAIT_OBJECT_0;
}

// 自绘控件在 Shell::Create 里逐个建出来，主窗口会先于子控件出现；
// 刚找到窗口就数一次子控件必然有竞态，所以轮询到非 0 或超时。
int WaitForChildren(HWND hwnd, int timeoutMs)
{
    const ULONGLONG start = GetTickCount64();
    while (GetTickCount64() - start < static_cast<ULONGLONG>(timeoutMs))
    {
        int n = 0;
        EnumChildWindows(hwnd, CountChild, reinterpret_cast<LPARAM>(&n));
        if (n > 0)
            return n;
        Sleep(50);
    }
    return 0;
}

} // namespace

int main()
{
    // 1) 找同目录的 airecord.exe（满足"测的是本次构建产物"）。
    wchar_t self[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, self, MAX_PATH))
    {
        std::printf("test_app_smoke: FAILED (GetModuleFileNameW gle=%lu)\n", GetLastError());
        return 1;
    }
    std::wstring dir(self);
    const size_t slash = dir.find_last_of(L'\\');
    if (slash != std::wstring::npos)
        dir.resize(slash);
    const std::wstring exe = dir + L"\\airecord.exe";

    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        std::printf("test_app_smoke: SKIP (airecord.exe not built next to the test: %ls)\n",
                    exe.c_str());
        return 0;
    }

    // 2) 单实例互斥：已有 AireRecord 窗口时，新进程会立刻退出，此时枚举到的
    //    是"别人的窗口"，结果不可信（AGENTS.md 要求验证前先停服务/清进程）。
    if (HWND other = FindWindowW(ipc::kUiWindowClass, nullptr))
    {
        std::printf("test_app_smoke: FAILED (an AireRecord window is already running, hwnd=%p)\n"
                    "  Stop the installed client first: Stop-Service AireRecord; "
                    "stop-process -name airecord,airecordd\n",
                    static_cast<void*>(other));
        return 1;
    }

    // 3) 启动本次构建的客户端（隐藏窗口 + 不抢焦点 + 不碰服务）。
    std::wstring cmd = L"\"" + exe + L"\" --hidden --no-activate --no-service";
    std::wstring cmdBuf = cmd;   // CreateProcessW 可能原地改写，保留可写副本
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (!CreateProcessW(exe.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, dir.c_str(), &si, &pi))
    {
        std::printf("test_app_smoke: FAILED (CreateProcessW gle=%lu)\n", GetLastError());
        return 1;
    }

    int rc = 0;

    // 4) 等主窗口出现（冷启动 + 杀软扫描留足 20s）。
    const ULONGLONG start = GetTickCount64();
    HWND hwnd = nullptr;
    DWORD exitCode = STILL_ACTIVE;

    while (GetTickCount64() - start < 20000)
    {
        if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
            break;   // 进程提前退出，后面统一报错

        WindowSearch search{ pi.dwProcessId, nullptr };
        EnumWindows(FindByPid, reinterpret_cast<LPARAM>(&search));
        if (search.found)
        {
            hwnd = search.found;
            break;
        }
        Sleep(100);
    }

    if (!hwnd)
    {
        if (exitCode != STILL_ACTIVE)
            std::printf("test_app_smoke: FAILED (client exited early, rc=%lu)\n", exitCode);
        else
            std::printf("test_app_smoke: FAILED (no AireRecordWnd within 20s, pid=%lu)\n",
                        pi.dwProcessId);
        rc = 1;
    }

    // 5) 消息循环必须应答：窗口建出来但 UI 线程卡死同样是回归。
    bool responsive = false;
    int  children = 0;
    if (rc == 0)
    {
        const ULONGLONG probeStart = GetTickCount64();
        while (GetTickCount64() - probeStart < 10000)
        {
            DWORD_PTR ignored = 0;
            if (SendMessageTimeoutW(hwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 1000, &ignored))
            {
                responsive = true;
                break;
            }
            Sleep(100);
        }

        if (!responsive)
        {
            std::printf("test_app_smoke: FAILED (window did not answer WM_NULL within 10s)\n");
            rc = 1;
        }
        else
        {
            children = WaitForChildren(hwnd, 10000);
            if (children <= 0)
            {
                // 自绘控件都是真实子窗口（AirUITextBox / AirUIListBox 等），
                // 一个都没有说明 Shell::Create 之后的控件装配断了。
                std::printf("test_app_smoke: FAILED (no child controls within 10s, pid=%lu)\n",
                            pi.dwProcessId);
                rc = 1;
            }
        }
    }

    // 6) 收尾：先确认还活着，再终止；任何路径都不留僵尸进程。
    if (exitCode == STILL_ACTIVE && !TerminateAndWait(pi))
    {
        std::printf("test_app_smoke: FAILED (process %lu did not exit after TerminateProcess)\n",
                    pi.dwProcessId);
        rc = 1;
    }

    // 句柄 signaled 不等于 PID 真的消失（可能已有同名进程接管），
    // 用 PID 再确认一次，避免留下后台实例干扰后续用例。
    {
        HANDLE still = OpenProcess(SYNCHRONIZE, FALSE, pi.dwProcessId);
        if (still)
        {
            if (WaitForSingleObject(still, 0) == WAIT_TIMEOUT)
            {
                std::printf("test_app_smoke: FAILED (pid %lu is still alive after shutdown)\n",
                            pi.dwProcessId);
                rc = 1;
            }
            CloseHandle(still);
        }
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (rc == 0)
    {
        std::printf("test_app_smoke: airecord.exe --hidden --no-activate --no-service\n");
        std::printf("  window    : class=%ls hwnd=%p\n",
                    ipc::kUiWindowClass, static_cast<void*>(hwnd));
        std::printf("  children  : %d\n", children);
        std::printf("  responsive: yes\n");
        std::printf("test_app_smoke: PASSED (process up, window created, messages answered)\n");
    }
    return rc;
}
