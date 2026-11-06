// ============================================================
// test_ipc_pipe.cpp — IPC 管道名回归测试
//
// 背景：SCM 服务会为每个 Active 会话（控制台 + 每个 RDP 会话）
// 各拉起一个 -agent 代理，各自持有本用户的数据库。管道名若为
// 整机全局（\\.\pipe\AireRecordIpc），多会话就会抢同一个名字：
// 先创建者独占，后者静默失去 IPC，客户端于是连到别的会话的
// 代理上——表现为"记录全丢"（其实读到了别人的库），手动录入/
// 删除也会写进别人的库。
//
// 因此管道名必须带会话后缀 _s<sessionId>。
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

    DWORD sid = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sid);
    const std::wstring suffix = L"_s" + std::to_wstring(sid);

    expect(name.rfind(L"\\\\.\\pipe\\", 0) == 0,
           "pipe name starts with \\\\.\\pipe\\");
    expect(name.size() > suffix.size() &&
           name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0,
           "pipe name ends with session suffix _s<sessionId>");
    expect(std::wstring(ipc::PipeName()) == name,
           "pipe name is stable across calls");
    expect(name != L"\\\\.\\pipe\\AireRecordIpc",
           "pipe name is not the bare global name");

    std::printf(fails ? "FAILED (%d)\n" : "ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
