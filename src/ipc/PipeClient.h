#pragma once
// ============================================================
// PipeClient.h — 客户端同步请求（自动拉起服务端）
// ============================================================
#include <string>

class PipeClient
{
public:
    // 设置服务端 exe 完整路径（用于自动拉起）
    void SetServicePath(const std::wstring& exePath) { m_servicePath = exePath; }

    // 连通性测试；retries 为失败时的重试次数
    bool Ping(int retries = 1);

    // 发送一条命令，返回响应文本；失败返回空串
    std::wstring Request(const std::wstring& line);

    // 服务端未运行时尝试启动（exe 与客户端同目录）
    bool LaunchService();

private:
    std::wstring m_servicePath;
};
