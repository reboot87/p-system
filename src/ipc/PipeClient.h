#pragma once
// ============================================================
// PipeClient.h — 客户端同步请求（连接机器级全局管道）
//
// 管道 \\.\pipe\AireRecordIpc 由 Session 0 系统服务独占监听。
// 客户端（airecord.exe 的 UI 与同进程剪贴板代理）连它访问共享的
// 机器级数据库。服务未运行时，客户端确保系统服务就绪：服务已安装
// 则 SCM 静默启动（无 UAC）；仅未安装时才提权 airecordd.exe -install。
// ============================================================
#include <string>

class PipeClient
{
public:
    // 设置服务端 exe 完整路径（用于自动拉起/安装服务）
    void SetServicePath(const std::wstring& exePath) { m_servicePath = exePath; }

    // 连通性测试；retries 为失败时的重试次数
    bool Ping(int retries = 1);

    // 发送一条命令，返回响应文本；失败返回空串
    std::wstring Request(const std::wstring& line);

    // 服务未运行时尝试启动（exe 与客户端同目录）
    bool LaunchService();

    // 确保系统服务已安装并运行（必要时提权 -install），并等待全局
    // 管道就绪。返回 true 表示服务已就绪。
    bool EnsureService();

private:
    std::wstring m_servicePath;
};