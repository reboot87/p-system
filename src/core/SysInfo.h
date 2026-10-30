#pragma once
// ============================================================
// SysInfo.h — 系统与硬件信息采集（纯 Win32 API，不依赖 WMI）
// 客户端"机器配置"页数据源。全部为只读系统查询。
// ============================================================
#include <string>
#include <vector>

struct DiskInfo
{
    std::wstring letter;      // "C:"
    long long    totalMB = 0;
    long long    freeMB  = 0;
};

struct SysInfoData
{
    // ---- 操作系统 ----
    std::wstring osName;        // "Windows 11 专业版 23H2"
    std::wstring osBuild;       // "22631.4317"
    std::wstring arch;          // "x64" / "x86" / "ARM64"
    std::wstring computerName;  // 主机名
    std::wstring userName;      // 当前用户
    std::wstring uptime;        // "3 天 4 小时 12 分"

    // ---- 处理器与内存 ----
    std::wstring cpuName;       // 完整型号
    int          logicalCpu = 0;
    double       cpuUsage   = -1.0;   // 百分比（需两次采样，首次 -1）
    long long    memTotalMB = 0;
    long long    memAvailMB = 0;

    // ---- 显示与图形 ----
    std::wstring gpuName;       // 主显卡
    int          monitorCount = 0;
    std::wstring primaryRes;    // "2560 x 1440"

    // ---- 磁盘 ----
    std::vector<DiskInfo> disks;

    // ---- 网络 ----
    std::wstring netAdapter;    // 首个已连接网卡名
    std::wstring netIp;         // IPv4
};

namespace sysinfo
{
    // 一次性采集（开销毫秒级，可在切页时调用）
    SysInfoData Collect();

    // CPU 占用率：与上次调用做差值；首次调用返回 -1
    double CpuUsage();

    // 服务状态查询（SCM）：0 未安装 1 停止 2 运行中；statusText 返回描述
    int QueryServiceState(const wchar_t* svcName, std::wstring& statusText);
}
