#pragma once
// ============================================================
// Ipc.h — 命名管道协议与编码工具（服务端/客户端共用）
//
// 传输：UTF-8，命令行以 '\n' 结尾，字段以 '\t' 分隔。
// 内容字段先 EscapeField（\\ \t \n \r 转义），保证原始
// \n 只作为命令/记录分隔符出现。
//
// 协议：
//   请求  PING
//   响应  OK
//
//   请求  LIST\t<filter>                      filter 已转义，可为空
//   响应  OK\n<id>\t<source>\t<time>\t<note>\t<content>\n...\nEND
//
//   请求  SAVE\t<source>\t<note>\t<content>   note/content 已转义
//   响应  OK | ERR\t<msg>
//
//   请求  COPY\t<content>                      服务端复制（自动过滤自记录）
//   响应  OK
// ============================================================
#include <string>
#include <vector>

namespace ipc
{
    // 管道全名（从机器级配置 service.ini [service] pipe_name 读取，
    // 默认 AireRecordIpc；首次调用后缓存，服务端/客户端读到同一值）
    const wchar_t* PipeName();

    std::string  WideToUtf8(const std::wstring& w);
    std::wstring Utf8ToWide(const std::string& s);

    std::wstring EscapeField(const std::wstring& s);
    std::wstring UnescapeField(const std::wstring& s);

    // 按 '\t' 切分
    std::vector<std::wstring> SplitTabs(const std::wstring& s);
    // 按 '\n' 切分
    std::vector<std::wstring> SplitLines(const std::wstring& s);
}
