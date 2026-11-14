#include "PipeClient.h"
#include "Ipc.h"
#include <shellapi.h>
#include <chrono>
#include <cstdlib>

#pragma comment(lib, "shell32.lib")

namespace {

// 尝试连接全局管道，成功返回句柄，失败返回 INVALID_HANDLE_VALUE
HANDLE TryConnect()
{
    return CreateFileW(ipc::PipeName(), GENERIC_READ | GENERIC_WRITE,
                       0, nullptr, OPEN_EXISTING,
                       FILE_FLAG_OVERLAPPED, nullptr);
}

// 服务已安装但被停止时，用 SCM 静默启动（无需提权/UAC）。
bool StartInstalledService()
{
    SC_HANDLE mgr = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!mgr) return false;
    SC_HANDLE svc = OpenServiceW(mgr, L"AireRecord", SERVICE_START);
    if (svc) {
        const BOOL started = StartServiceW(svc, 0, nullptr) ||
                             GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;
        CloseServiceHandle(svc);
        CloseServiceHandle(mgr);
        return started != FALSE;
    }
    CloseServiceHandle(mgr);
    return false;   // 服务未安装 → 需要提权安装
}

} // namespace

PipeClient::~PipeClient()
{
    Close();
}

void PipeClient::Close()
{
    m_closing = true;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_connected = false;
        if (m_pipe != INVALID_HANDLE_VALUE)
            CancelIoEx(m_pipe, nullptr);   // 唤醒 reader（reader 负责关闭句柄）
        m_cv.notify_all();
    }
    if (m_reader.joinable())
        m_reader.join();
}

// 服务未运行时确保它起来。优先：服务已安装 → SCM 静默启动（无 UAC）；
// 仅当服务从未安装时，才提权 airecordd.exe -install（UAC）。
bool PipeClient::LaunchService()
{
    if (m_servicePath.empty()) return false;

    if (StartInstalledService())
        return true;                            // 已装 → 静默启动成功

    // 服务未安装 → runas 触发 UAC 提权：-install 注册并启动系统服务。
    HINSTANCE r = ShellExecuteW(nullptr, L"runas", m_servicePath.c_str(),
                                L"-install", nullptr, SW_HIDE);
    return (intptr_t)r > 32;   // >32 表示提权调用已发起（UAC 结果另计）
}

// 确保系统服务已安装并运行、全局管道就绪。若管道未就绪，尝试提权
// 安装/启动服务，并轮询等待管道（服务启动需要几秒）。返回 true 表示
// 服务已就绪。
bool PipeClient::EnsureService()
{
    for (int i = 0; i < 30; ++i) {
        HANDLE h = TryConnect();
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            return true;
        }
        // 首轮失败即拉起；若首轮恰逢服务"已运行但管道未就绪"的过渡态
        // （被强杀的服务进程 SCM 尚未回收），后续轮次再重试拉起，否则
        // 会一直连不上而误报失败。
        if (i == 0 || i == 10 || i == 20)
            LaunchService();
        Sleep(300);
    }
    return false;
}

bool PipeClient::Connect()
{
    if (m_connected.load())
        return true;
    // 回收上一个已结束的 reader（连接断开后它已退出）
    if (m_reader.joinable())
        m_reader.join();

    HANDLE h = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 10 && h == INVALID_HANDLE_VALUE; ++i) {
        h = TryConnect();
        if (h == INVALID_HANDLE_VALUE) Sleep(100);
    }
    if (h == INVALID_HANDLE_VALUE)
        return false;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_pipe = h;
        m_connected = true;
        m_closing = false;
    }
    m_reader = std::thread(&PipeClient::ReaderThread, this);
    return true;
}

void PipeClient::ReaderThread()
{
    // 本线程持有打开的句柄并负责最终关闭
    HANDLE h;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        h = m_pipe;
    }
    // 本线程专用的 overlapped 读事件
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    for (;;) {
        std::string tag;
        if (!ipc::PipeReadLine(h, ov, tag)) break;
        std::string payload;
        if (!ipc::PipeReadLine(h, ov, payload)) break;

        std::wstring tagW = ipc::Utf8ToWide(tag);
        if (tagW == L"RSP") {
            // 响应帧：RSP\n<id>\t<escaped-resp>\n → UnescapeField 还原
            std::wstring p = ipc::UnescapeField(ipc::Utf8ToWide(payload));
            size_t tab = p.find(L'\t');
            unsigned long long id = 0;
            if (tab != std::wstring::npos)
                id = _wcstoui64(p.substr(0, tab).c_str(), nullptr, 10);
            std::wstring resp = (tab == std::wstring::npos)
                                ? L"" : p.substr(tab + 1);
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                auto it = m_pending.find(id);
                if (it != m_pending.end()) {
                    it->second = resp;
                    m_pendingDone[id] = true;
                    m_cv.notify_all();
                }
            }
        } else if (tagW == L"EVT") {
            std::wstring evt = ipc::Utf8ToWide(payload);
            if (m_onEvent)
                m_onEvent(evt);
        }
        // REQ 是客户端上行，服务端不应下发；忽略
    }

    if (ov.hEvent) CloseHandle(ov.hEvent);
    HandleDisconnect(h);
}

void PipeClient::HandleDisconnect(HANDLE h)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_pipe == h)
            m_pipe = INVALID_HANDLE_VALUE;
        m_connected = false;
        // 唤醒所有等待者：它们因 !m_connected 而返回（请求失败/重连）
        for (auto& kv : m_pendingDone)
            kv.second = true;
        m_cv.notify_all();
    }
    DisconnectNamedPipe(h);
    CloseHandle(h);
    if (!m_closing.load() && m_onDisconnect)
        m_onDisconnect();   // 实时检测服务死亡
}

std::wstring PipeClient::Request(const std::wstring& line)
{
    const unsigned long long id = m_nextId.fetch_add(1);
    const std::wstring payload = std::to_wstring(id) + L"\t" + line;

    for (int attempt = 0; attempt < 2; ++attempt) {
        // 确保连接就绪（断线时自动重连；首次失败则拉起服务）
        if (!Connect()) {
            if (attempt == 0 && EnsureService()) continue;
            return L"";
        }

        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_pending[id] = std::wstring();
            m_pendingDone[id] = false;

            HANDLE h = m_pipe;
            if (h == INVALID_HANDLE_VALUE) {
                m_connected = false;
                m_pendingDone[id] = true;
                m_cv.notify_all();
                if (attempt == 0) { EnsureService(); continue; }
                return L"";
            }
            std::string req = "REQ\n" + ipc::WideToUtf8(payload) + "\n";
            if (!ipc::PipeWrite(h, req)) {
                // 管道已断 → 标记失效，由断开回调/重连处理
                m_connected = false;
                m_pendingDone[id] = true;
                m_cv.notify_all();
                if (attempt == 0) { EnsureService(); continue; }
                return L"";
            }

            // 等待匹配的 RSP（或连接断开 / 超时）
            m_cv.wait_for(lk, std::chrono::seconds(8), [&] {
                return m_pendingDone[id] || !m_connected.load();
            });

            std::wstring resp = m_pending[id];
            m_pending.erase(id);
            m_pendingDone.erase(id);

            if (m_connected.load())
                return resp;
            // 等待期间连接断开 → 重连重试一次
            if (attempt == 0) { EnsureService(); continue; }
            return resp;
        }
    }
    return L"";
}

bool PipeClient::Ping(int retries)
{
    for (int i = 0; i <= retries; ++i) {
        if (Request(L"PING").rfind(L"OK", 0) == 0)
            return true;
        Sleep(300);
    }
    return false;
}