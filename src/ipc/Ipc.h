#pragma once
// ============================================================
// Ipc.h — 命名管道协议与编码工具（服务端/客户端共用）
//
// 传输：UTF-8，命令行以 '\n' 结尾，字段以 '\t' 分隔。
// 内容字段先 EscapeField（\\ \t \n \r 转义），保证原始
// \n 只作为命令/记录分隔符出现。
//
// 协议（帧）：每条帧 = "TAG\nPAYLOAD\n"，TAG ∈ {REQ, RSP, EVT}。
//   REQ\n<id>\t<line>\n    客户端→服务端请求（id 匹配响应）
//   RSP\n<id>\t<resp>\n    服务端→客户端响应（resp 先 EscapeField 转义，
//                          以便含内嵌换行的 LIST 等响应完整走一帧）
//   EVT\n<evt>\n           服务端→客户端事件广播（新记录/数据变更）
//
// 命名管道以 FILE_FLAG_OVERLAPPED 打开，支持同一句柄上 reader 线程的
// 阻塞读 与 其它线程的写 并发进行（持久双向流的必要前提）。
// ============================================================
#include <windows.h>
#include <string>
#include <vector>

namespace ipc
{
    // 主界面窗口类名 / WM_COPYDATA 标记。服务安装器（提升进程）靠它
    // 把安装结果交给客户端的自绘 Toast；服务端自身在 Session 0，
    // 不能弹任何系统对话框（会阻塞服务线程且用户永远看不到）。
    const wchar_t* const kUiWindowClass = L"AireRecordWnd";
    const unsigned long  kUiToastTag    = 0x41525431ul;  // 'ART1'

    // 管道全名（从机器级配置 service.ini [service] pipe_name 读取，
    // 默认 AireRecordIpc；首次调用后缓存）。管道是整机全局的：由
    // Session 0 的系统服务独占监听，所有会话的 UI/代理都连到它，
    // 以访问共享的机器级数据库。名字不带会话后缀。
    const wchar_t* PipeName();

    std::string  WideToUtf8(const std::wstring& w);
    std::wstring Utf8ToWide(const std::string& s);

    std::wstring EscapeField(const std::wstring& s);
    std::wstring UnescapeField(const std::wstring& s);

    // 按 '\t' 切分
    std::vector<std::wstring> SplitTabs(const std::wstring& s);
    // 按 '\n' 切分
    std::vector<std::wstring> SplitLines(const std::wstring& s);

    // ---- 命名管道 overlapped I/O 帮助（句柄须以 FILE_FLAG_OVERLAPPED 打开） ----
    // 用 OVERLAPPED 读一行（'\n' 结尾，不包含换行）。返回 true 表示读到一行；
    // 断开（ERROR_BROKEN_PIPE / 0 字节）或取消（ERROR_OPERATION_ABORTED）返回 false。
    // ov.hEvent 由调用方创建（手动重置事件），供阻塞读等待。
    bool PipeReadLine(HANDLE pipe, OVERLAPPED& ov, std::string& out);
    // 用 OVERLAPPED 同步写整块数据（内部创建事件并等待完成）。返回 true 表示写成功。
    bool PipeWrite(HANDLE pipe, const std::string& data);
    // 用 OVERLAPPED 写整块数据，最多等待 timeoutMs 毫秒（SendEvent 广播用，
    // 避免因某连接读侧未及时消费而长时间阻塞）。返回 true 表示写完成。
    bool PipeWriteTimeout(HANDLE pipe, const std::string& data, DWORD timeoutMs);
}
