#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include "SysInfo.h"
#include <stdio.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace {

std::wstring ReadRegString(HKEY root, const wchar_t* sub,
                           const wchar_t* value)
{
    std::wstring out;
    DWORD type = 0, cb = 0;
    if (RegGetValueW(root, sub, value, RRF_RT_REG_SZ,
                     &type, nullptr, &cb) != ERROR_SUCCESS || !cb)
        return out;
    out.resize(cb / sizeof(wchar_t));
    if (RegGetValueW(root, sub, value, RRF_RT_REG_SZ,
                     &type, &out[0], &cb) != ERROR_SUCCESS)
        return L"";
    while (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

} // namespace

// ---------- CPU 占用率（两次采样差值） ----------
double sysinfo::CpuUsage()
{
    static unsigned long long s_prevIdle = 0, s_prevKernel = 0;
    FILETIME idleNow = {}, kernelNow = {}, userNow = {};
    if (!GetSystemTimes(&idleNow, &kernelNow, &userNow))
        return -1.0;

    auto to64 = [](const FILETIME& ft) {
        ULARGE_INTEGER u;
        u.LowPart  = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        return u.QuadPart;
    };

    unsigned long long idle64   = to64(idleNow);
    unsigned long long kernel64 = to64(kernelNow);

    if (!s_prevKernel && !s_prevIdle) {
        s_prevIdle   = idle64;
        s_prevKernel = kernel64;
        return -1.0;   // 首次无基准
    }

    unsigned long long idleDelta   = idle64   - s_prevIdle;
    unsigned long long kernelDelta = kernel64 - s_prevKernel;
    s_prevIdle   = idle64;
    s_prevKernel = kernel64;

    if (!kernelDelta) return -1.0;
    double usage = 100.0 * (double)(kernelDelta - idleDelta) / (double)kernelDelta;
    if (usage < 0)   usage = 0;
    if (usage > 100) usage = 100;
    return usage;
}

// ---------- 一次性采集 ----------
SysInfoData sysinfo::Collect()
{
    SysInfoData d;

    // ---- 操作系统（注册表 CurrentVersion + Win11 判定） ----
    std::wstring product = ReadRegString(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"ProductName");                       // Win11 上仍为 "Windows 10 ..."
    std::wstring display = ReadRegString(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"DisplayVersion");                    // 23H2
    std::wstring build = ReadRegString(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"CurrentBuild");
    std::wstring ubr = ReadRegString(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"UBR");

    // ProductName 校正：Build >= 22000 实为 Windows 11
    std::wstring edition = product;
    std::wstring verLabel = L"Windows 10";
    if (!build.empty() && _wtoi(build.c_str()) >= 22000)
        verLabel = L"Windows 11";
    size_t pos = edition.find(L"Windows 10");
    if (pos != std::wstring::npos)
        edition.replace(pos, 10, verLabel);

    d.osName = edition;
    if (!display.empty()) d.osName += L" " + display;
    d.osBuild = build;
    if (!ubr.empty())  d.osBuild += L"." + ubr;

    SYSTEM_INFO si = {};
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64: d.arch = L"x64";    break;
    case PROCESSOR_ARCHITECTURE_ARM64: d.arch = L"ARM64";  break;
    case PROCESSOR_ARCHITECTURE_INTEL: d.arch = L"x86";    break;
    default:                           d.arch = L"未知";    break;
    }

    wchar_t nameBuf[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD nameLen = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(nameBuf, &nameLen);
    d.computerName = nameBuf;

    wchar_t userBuf[256] = {};
    DWORD userLen = 256;
    GetUserNameW(userBuf, &userLen);
    d.userName = userBuf;

    // 开机时长
    unsigned long long ms = GetTickCount64();
    unsigned long long min = ms / 60000ULL;
    if (min < 60) {
        d.uptime = std::to_wstring((long)min) + L" 分钟";
    } else if (min < 1440) {
        d.uptime = std::to_wstring((long)(min / 60)) + L" 小时 " +
                   std::to_wstring((long)(min % 60)) + L" 分";
    } else {
        d.uptime = std::to_wstring((long)(min / 1440)) + L" 天 " +
                   std::to_wstring((long)((min % 1440) / 60)) + L" 小时";
    }

    // ---- CPU（注册表型号 + 逻辑处理器数） ----
    d.cpuName = ReadRegString(
        HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        L"ProcessorNameString");
    d.logicalCpu = (int)si.dwNumberOfProcessors;
    d.cpuUsage   = CpuUsage();

    // ---- 内存 ----
    MEMORYSTATUSEX mstx = { sizeof(mstx) };
    if (GlobalMemoryStatusEx(&mstx)) {
        d.memTotalMB = (long long)(mstx.ullTotalPhys / (1024 * 1024));
        d.memAvailMB = (long long)(mstx.ullAvailPhys / (1024 * 1024));
    }

    // ---- 显卡（首个主显示适配器） ----
    DISPLAY_DEVICEW dd = { sizeof(dd) };
    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i) {
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
            d.gpuName = dd.DeviceString;
            break;
        }
        if (!d.gpuName.empty()) break;
        if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
            d.gpuName = dd.DeviceString;
    }

    // ---- 显示器 ----
    d.primaryRes = std::to_wstring(GetSystemMetrics(SM_CXSCREEN)) + L" x " +
                   std::to_wstring(GetSystemMetrics(SM_CYSCREEN));
    d.monitorCount = GetSystemMetrics(SM_CMONITORS);

    // ---- 固定磁盘 ----
    wchar_t drives[512] = {};
    if (GetLogicalDriveStringsW(511, drives)) {
        for (wchar_t* p = drives; *p; p += wcslen(p) + 1) {
            if (GetDriveTypeW(p) != DRIVE_FIXED) continue;
            ULARGE_INTEGER total = {}, free = {};
            if (!GetDiskFreeSpaceExW(p, nullptr, &total, &free)) continue;
            DiskInfo di;
            di.letter   = p;
            di.totalMB  = (long long)(total.QuadPart / (1024 * 1024));
            di.freeMB   = (long long)(free.QuadPart  / (1024 * 1024));
            d.disks.push_back(di);
        }
    }

    // ---- 网络（首个 Up 状态的 IPv4 适配器） ----
    ULONG bufLen = 16 * 1024;
    std::vector<char> buf(bufLen);
    auto* aa = (IP_ADAPTER_ADDRESSES*)buf.data();
    if (GetAdaptersAddresses(AF_INET,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
            GAA_FLAG_SKIP_DNS_SERVER,
            nullptr, aa, &bufLen) == ERROR_SUCCESS) {
        for (auto* a = aa; a; a = a->Next) {
            if (a->OperStatus != IfOperStatusUp) continue;
            if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (a->FirstUnicastAddress &&
                a->FirstUnicastAddress->Address.lpSockaddr->sa_family == AF_INET) {
                d.netAdapter = a->FriendlyName;
                auto* sa = (sockaddr_in*)a->FirstUnicastAddress->Address.lpSockaddr;
                unsigned long ip = ntohl(sa->sin_addr.S_un.S_addr);
                wchar_t ipBuf[32];
                swprintf_s(ipBuf, L"%u.%u.%u.%u",
                           (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                           (ip >> 8) & 0xFF, ip & 0xFF);
                d.netIp = ipBuf;
                break;
            }
        }
    }

    return d;
}

// ---------- SCM 服务状态查询 ----------
int sysinfo::QueryServiceState(const wchar_t* svcName, std::wstring& statusText)
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) { statusText = L"无法连接服务管理器"; return 0; }
    SC_HANDLE svc = OpenServiceW(scm, svcName, SERVICE_QUERY_STATUS);
    if (!svc) {
        CloseServiceHandle(scm);
        statusText = L"未安装";
        return 0;
    }
    SERVICE_STATUS ss = {};
    int state = 0;
    if (QueryServiceStatus(svc, &ss)) {
        state = (ss.dwCurrentState == SERVICE_RUNNING) ? 2 : 1;
        statusText = (state == 2) ? L"Running" : L"已停止";
    } else {
        statusText = L"状态未知";
    }
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return state;
}
