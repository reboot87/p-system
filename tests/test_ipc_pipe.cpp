// ============================================================
// test_ipc_pipe.cpp — IPC 管道名回归测试
//
// 背景：机器级共享数据库归 Session 0 的系统服务独占，服务在整机
// 全局命名管道（\\.\pipe\AireRecordIpc）上监听。所有登录会话的
// airecord.exe（UI + 同进程剪贴板代理）都连到这条全局管道，把
// 剪贴板记录转发给服务入库。
//
// 因此管道名必须为整机全局名 \\.\pipe\AireRecordIpc，不带任何
// 会话后缀。任何会话中的 UI/代理算出的名字都一致。
// ============================================================
#include <windows.h>
#include <cstdio>
#include <string>
#include "ipc/Ipc.h"

int wmain()
{
    int fails = 0;
    auto expect = [&](bool ok, const char* what) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok) ++fails;
    };

    const std::wstring name = ipc::PipeName();
    std::wprintf(L"pipe name = %ls\n", name.c_str());

    expect(name.rfind(L"\\\\.\\pipe\\", 0) == 0,
           "pipe name starts with \\\\.\\pipe\\");
    expect(name == L"\\\\.\\pipe\\AireRecordIpc",
           "pipe name is the machine-global name (no session suffix)");
    expect(std::wstring(ipc::PipeName()) == name,
           "pipe name is stable across calls");

    std::printf(fails ? "FAILED (%d)\n" : "ALL PASSED\n", fails);
    return fails ? 1 : 0;
}