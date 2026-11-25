#include "App.h"
#include "DiagLog.h"
#include "SysInfo.h"
#include "../agent/AgentHost.h"
#include "airui/PaintBackend.h"
#include "airui/PaintProviders.h"
#include "../ipc/Ipc.h"
#include "pages/OverviewPage.h"
#include "pages/ClipsPage.h"
#include "pages/ConfigPage.h"

using air::ui::NavGroup;
using air::ui::NavItem;
using air::ui::ShellOptions;
using namespace arecord;

namespace arecord
{

// 版本单一来源：页脚文案唯一拼接点。版本号来自 CMake 生成的 Version.h
// （project(AireRecord VERSION ...)），无硬编码 "vX.Y"。后缀（服务已连接 /
// 未连接）由调用点追加。
const wchar_t* kAppFooterPrefix = L"AireRecord " AIRUI_VERSION_W L" · ";

// App 析构：AgentHost 是完整类型时才析构（unique_ptr 成员）。
// 后台刷新/重连线程改为可 join：置位停止标志后逐个 join，等待在途的
// Connect()（~1s）/ Request()（1.5s）结束，杜绝 detach 线程在 App 析构后
// 仍访问 this 的悬空（use-after-free）。
App::~App()
{
    m_shuttingDown.store(true);
    if (m_reconnectThread.joinable()) m_reconnectThread.join();
    if (m_refreshThread.joinable())   m_refreshThread.join();
}

// ============================================================
// 后台任务（可 join 线程 + 结果经消息回投）
// ============================================================

// 后台刷新线程把「拉到的数据」封装成一份结果，经 WM_APP+6 回 UI 线程；
// UI 线程在 WM_APP+6 里统一写入 m_stats / m_svcState / m_svcText / m_entries，
// 保证共享状态只有 UI 线程一个写者（消除数据竞争）。
namespace {
struct RefreshResult
{
    ServiceStats         stats;
    int                  svcState = 0;
    std::wstring         svcText;
    std::vector<UiEntry> entries;
    bool                 ok = false;
};

// 纯 IPC：Request(STATS) 解析进局部 ServiceStats，并把 SCM 服务状态查进
// 输出参数。不写任何 App 成员——供后台线程调用。
ServiceStats FetchStats(PipeClient& ipc, int& outSvcState, std::wstring& outSvcText)
{
    ServiceStats st;
    std::wstring resp = ipc.Request(L"STATS");
    if (resp.rfind(L"OK", 0) == 0) {
        auto f = ipc::SplitTabs(resp);
        if (f.size() >= 4) {
            st.total   = _wtoi64(f[1].c_str());
            st.today   = _wtoi64(f[2].c_str());
            st.dbBytes = _wtoi64(f[3].c_str());
        }
    }
    outSvcState = sysinfo::QueryServiceState(L"AireRecord", outSvcText);
    return st;
}
} // namespace

// 离线重连探测（后台线程）：只探测/建立连接，不拉起服务。成功经 WM_APP+7
// 回 UI 线程恢复，失败则由定时器下轮再试。只读 m_ipc，不写共享 UI 成员。
void App::ReconnectProbe(HWND hwnd, bool wasOffline)
{
    if (wasOffline) {
        // 服务曾不可用：探测/建立连接；成功经 WM_APP+7 回 UI 线程恢复，
        // 失败则下轮再试。
        bool ok = m_ipc.Connect();
        m_reconnecting.store(false);
        if (m_shuttingDown.load() || !hwnd) return;   // 退出中：不再投递
        PostMessage(hwnd, WM_APP + 7, ok ? 0 : 1, 0);
    } else {
        // 服务在线：例行心跳，向服务端登记本会话，让服务端心跳兜底
        // （60s 未收到则按会话重启 UI）始终最新，从而不会误拉重复实例。
        // 不做阻塞式 Connect，直接经持久连接发 PING。
        m_ipc.Request(L"PING\t" + std::to_wstring(diag::SessionId()));
        m_reconnecting.store(false);
        if (m_shuttingDown.load() || !hwnd) return;   // 退出中：不再投递
        PostMessage(hwnd, WM_APP + 7, 2, 0);
    }
}

// 后台刷新线程：只做纯 IPC 拉取 + 解析，不写任何 App 成员；结果封装进堆
// 分配的 RefreshResult，经 WM_APP+6 回 UI 线程，由 UI 线程统一应用成员。
void App::RefreshWorker(HWND hwnd, std::wstring filter)
{
    RefreshResult* out = new RefreshResult;
    out->stats    = FetchStats(m_ipc, out->svcState, out->svcText);  // Request(STATS)
    if (m_clips)
        out->entries = m_clips->FetchEntries(filter, &out->ok);      // Request(LIST)

    m_refreshing.store(false);   // 释放防抖锁：本次刷新完成，允许下一次

    if (m_shuttingDown.load() || !hwnd) { delete out; return; }       // 退出中：不投递
    if (!PostMessage(hwnd, WM_APP + 6, 0, reinterpret_cast<LPARAM>(out)))
        delete out;               // PostMessage 失败（窗口已销毁）：必须回收
}

// ============================================================
// 启动
// ============================================================
int App::Run(HINSTANCE hInstance)
{
    ShellOptions opt;
    // 类名保持不变：单实例检测与任务栏行为维持原样
    opt.className = ipc::kUiWindowClass;
    opt.appName   = L"AireRecord";
    opt.width     = 1120;
    opt.height    = 780;
    opt.minWidth  = 780;
    opt.minHeight = 560;
    opt.sidebar   = true;
    opt.statusBar = true;
    // 静默启动或延迟显示时，窗口先以隐藏状态创建
    opt.showOnCreate = !m_startHidden && m_showDelayMs <= 0;
    // 运行时后端选择（--paint-backend / AIRUI_PAINT_BACKEND / 默认 gdi）
    opt.paintBackend = m_paintBackend;
    // 暗色模式（机器级 [ui] dark，默认浅色）。Shell::Create 里按 opt.dark
    // 应用 DarkPalette；配置页「显示与外观」可运行期切换（SetThemeDark）。
    m_dark = machine::Get(L"ui", L"dark", L"0") == L"1";
    opt.dark = m_dark;
    // --core-shell 只切换外壳实现（简化 HAL 外壳），与渲染后端无关；
    // 后端由 PaintFrame 在帧级别覆盖整个客户区。
    opt.coreShell = m_coreShell;

    // 标题栏「渲染」按钮热切换后补一条可观测日志：启动时的 [paint] 行
    // 只反映初始选择，UI 切换必须单独留痕。
    m_shell.SetPaintBackendChangedHandler(
        [this](const std::string& id, bool backendOwnsText) {
            const char* textMode = "gdi";
            if (id == "skia")
                textMode = backendOwnsText ? "backend" : "host";
            diag::Log(L"[paint] UI switch -> %hs wholeWindow=1 coreShell=%d text=%hs",
                      id.c_str(), (int)m_shell.CoreShellEnabled(), textMode);
            const wchar_t* name = (id == "skia") ? L"Skia" : L"GDI";
            std::wstring status = std::wstring(L"渲染后端: ") + name;
            if (id == "skia")
                status += backendOwnsText ? L" · 全 Skia" : L" · 文字 GDI";
            m_shell.SetStatusExtra(status);
            m_shell.Invalidate();
            Toast(std::wstring(L"渲染后端 · ") + name, L"已切换绘制后端");
        });

    if (!m_shell.Create(hInstance, opt)) {
        diag::Log(L"[fatal] shell create failed, gle=%lu", GetLastError());
        return 1;
    }

    // ---- 合并后台剪贴板代理（同进程） ----
    // 代理在独立工作线程 + 自己的隐藏窗口上运行，本进程单进程同时承载
    // UI 主窗口与后台剪贴板记录。若本会话已有其它剪贴板监听者（如残留的
    // airecordd -agent），则 AgentHost 不启动自己的代理，UI 仍走 PipeClient
    // 连全局管道（数据统一由系统服务入库）。
    m_agent = std::make_unique<AgentHost>();
    bool served = m_agent->Start(hInstance, m_shell.Hwnd(), [this]() {
        // 托盘「退出」：停止服务 + 退出 UI（一起退出，符合生命周期契约）。
        ExitAll();
    });
    diag::Log(L"[agent] merged in-process agent serving_pipe=%d", (int)served);

    // ---- 持久连接回调：事件（实时刷新）+ 断开（实时重启服务） ----
    // 服务端广播新记录/数据变更 → 通知主界面刷新（读者线程→UI 线程）。
    m_ipc.SetEventCallback([this](const std::wstring& evt) {
        // 用户主动停服务（sc stop）时，服务端先广播 STOPPING 再关管道：
        // 让 UI 暂停自动重启，避免"停止"与"启动"竞争（SCM 立即报无法停止）。
        if (evt == L"STOPPING") {
            // 用户主动停服务（sc stop）：UI **保持存活**进入离线态，不退出。
            // 剪贴板记录暂存本地，等服务回来（SCM 失败操作 / 用户重新启动）
            // 时自动同步回机器库。不做重启服务的尝试（尊重 sc stop）。
            diag::Log(L"[evt] STOPPING -> UI offline (stays alive)");
            if (m_shell.Hwnd())
                PostMessage(m_shell.Hwnd(), WM_APP + 3, 0, 0);   // 走 ServiceDied 统一离线
            return;
        }
        if (m_shell.Hwnd())
            PostMessage(m_shell.Hwnd(), WM_APP + 2, 0, 0);   // 防抖刷新
    });
    // 服务端连接断开（ReadFile 返回 ERROR_BROKEN_PIPE）→ 实时重启服务。
    // 回调在后台 reader 线程：经 WM_APP+3 转到 UI 线程做恢复动作。
    m_ipc.SetDisconnectCallback([this]() {
        if (m_shell.Hwnd())
            PostMessage(m_shell.Hwnd(), WM_APP + 3, 0, 0);
    });

    // ---- 确保系统服务就绪（必要依赖） ----
    // 机器级共享库归 Session 0 系统服务独占。UI 首次 IPC 前先确保服务
    // 已安装并运行、全局管道就绪；未安装时提权 airecordd.exe -install
    // （UAC）。失败则后续 Request 每次也会尝试，并显示"服务未连接"。
    if (m_ipc.EnsureService()) {
        diag::Log(L"[ipc] service up on %ls", ipc::PipeName());
        RegisterSession();
    } else {
        // 服务启动失败（如未安装且用户拒绝 UAC）：不退出，进入离线态，
        // 后台每 3s 尝试重连；剪贴板记录暂存本地，等服务可用后同步。
        diag::Log(L"[ipc] service NOT up on %ls, entering offline mode", ipc::PipeName());
        SetServiceOffline(true);
        StartReconnectWatch();
    }

    // ---- 关闭策略（机器级 [ui] close_behavior，默认 hide） ----
    //   "hide"：关闭主窗口仅隐藏，后台代理与托盘继续运行（进程存活）。
    //   "exit"：关闭主窗口即退出整个进程（含后台代理）。
    m_shell.SetCloseGuard([this]() -> bool {
        if (m_forceExit) {            // 托盘「退出」：总是真正退出
            m_agent->Stop();
            return true;
        }
        std::wstring policy = machine::Get(L"ui", L"close_behavior", L"hide");
        if (policy == L"exit") {
            m_agent->Stop();          // 停止后台代理线程与托盘
            return true;              // 允许销毁窗口 → 消息循环结束 → 进程退出
        }
        ShowWindow(m_shell.Hwnd(), SW_HIDE);   // 默认：仅隐藏，保持进程存活
        return false;
    });

    // 后端可观测性：把「请求了什么 / 解析成什么 / 哪些真正可用 / 是否
    // 发生回退」写进 client.log，避免「以为在用 Skia，其实回退了 GDI」
    // 无从分辨。AvailablePaintBackends() 会触发插件 DLL 的按需加载。
    {
        using air::ui::PainterRegistry;
        PainterRegistry& reg = PainterRegistry::Instance();
        std::string available;
        for (const std::string& id : m_shell.AvailablePaintBackends()) {
            if (!available.empty()) available += ",";
            available += id;
        }
        const std::string& resolved = m_shell.PaintBackendChoice();

        // Available() 只证明 DLL 能加载；再探测一次真实开帧，才能发现
        // ABI 版本不符、像素缓冲失败等运行时回退。
        std::string probeError;
        if (resolved == "skia") {
            air::ui::PaintTarget probe{ nullptr, air::ui::Rect{ 0, 0, 16, 16 } };
            std::unique_ptr<air::ui::IPainter> frame = reg.Begin(resolved, probe);
            probeError = reg.LastError();
            frame.reset();
        }

        const char* textMode = "gdi";
        if (resolved == "skia")
            textMode = air::ui::SkiaPluginSupportsWideText()
                     ? "backend" : "host";

        diag::Log(L"[paint] wholeWindow=1 coreShell=%d requested=%hs resolved=%hs available=[%hs] skiaDll=%d probe=%hs text=%hs",
                  (int)m_coreShell,
                  m_paintBackend.empty() ? "auto" : m_paintBackend.c_str(),
                  resolved.c_str(),
                  available.c_str(),
                  (int)air::ui::SkiaPluginLoaded(),
                  probeError.empty() ? "ok" : probeError.c_str(),
                  textMode);
    }

    // 插件钩子：所有窗口消息先过插件分发（F3/F4 等）
    m_shell.SetMessageHook(
        [this](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
               LRESULT& result) -> bool {
            // 退出链路留痕：现场"闪一下就消失"时能区分是自己关的窗，
            // 还是外部进程投递 WM_CLOSE / 结束进程（见 client.log）
            if (msg == WM_CLOSE || msg == WM_DESTROY ||
                msg == WM_ENDSESSION || msg == WM_QUERYENDSESSION) {
                POINT cur{}; GetCursorPos(&cur);
                diag::Log(L"[msg] 0x%04X wParam=%llu insend=%d cursor=%ld,%ld",
                          msg, (unsigned long long)wParam,
                          (int)InSendMessage(), cur.x, cur.y);
            }
            // 服务安装器 / SCM 启动失败的提权进程通过 WM_COPYDATA 把
            // 结果送过来：由主界面用自绘 Toast 呈现，服务端自己不弹窗。
            if (msg == WM_COPYDATA) {
                const auto* cds = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
                if (cds && cds->dwData == ipc::kUiToastTag && cds->lpData &&
                    cds->cbData >= sizeof(wchar_t)) {
                    const wchar_t* raw = static_cast<const wchar_t*>(cds->lpData);
                    const size_t cap = (size_t)cds->cbData / sizeof(wchar_t);
                    size_t len = 0;
                    while (len < cap && raw[len]) ++len;
                    const std::wstring payload(raw, len);
                    // 载荷：ok(1/0) \t 标题 \t 正文
                    const size_t p1 = payload.find(L'\t');
                    const size_t p2 = p1 == std::wstring::npos
                        ? std::wstring::npos : payload.find(L'\t', p1 + 1);
                    if (p1 != std::wstring::npos && p2 != std::wstring::npos) {
                        Toast(payload.substr(p1 + 1, p2 - p1 - 1),
                              payload.substr(p2 + 1),
                              payload.compare(0, 1, L"1") == 0);
                    }
                }
                result = TRUE;
                return true;
            }
            // 延迟显示：窗口已隐藏建好，到点再显示（规避启动期的可见窗口检查）
            if (msg == WM_TIMER && wParam == TIMER_PROBE_HIDDEN) {
                KillTimer(hwnd, TIMER_PROBE_HIDDEN);
                if (m_showNoActivate) {
                    // 不抢前台：只让窗口出现，由用户点击激活
                    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                } else {
                    ShowWindow(hwnd, SW_SHOWNORMAL);
                    SetForegroundWindow(hwnd);
                }
                diag::Log(L"[show] deferred %d ms noact=%d visible=%d fg=%d",
                          m_showDelayMs, (int)m_showNoActivate,
                          (int)IsWindowVisible(hwnd),
                          (int)(GetForegroundWindow() == hwnd));
                result = 0;
                return true;
            }
            // 服务端新记录通知 → 防抖后刷新（合并连续多次复制）
            if (msg == WM_APP + 2) {
                SetTimer(hwnd, TIMER_REFRESH_NOTIFY, 300, nullptr);
                result = 0;
                return true;
            }
            // 窗口移动 / 缩放 → 800ms 防抖后持久化位置（连续拖动只存最后一次）。
            // 只「武装」定时器、不消费消息：必须继续交给 Shell 处理（WM_SIZE
            // 要重排布局、WM_MOVE 要重定位 Toast）。最大化 / 最小化不存
            // （IsZoomed/IsIconic 在 SaveWindowRect 内判）。
            if (msg == WM_MOVE || msg == WM_SIZE) {
                if (!m_rectSaveArmed) {
                    m_rectSaveArmed = true;
                    SetTimer(hwnd, TIMER_WINDOW_RECT, 800, nullptr);
                }
            }
            // 窗口位置防抖到期：落盘 [ui] window_rect
            if (msg == WM_TIMER && wParam == TIMER_WINDOW_RECT) {
                KillTimer(hwnd, TIMER_WINDOW_RECT);
                m_rectSaveArmed = false;
                SaveWindowRect();
                result = 0;
                return true;
            }
            // 防抖到期由应用层统一刷新共享数据。不能交给 Shell 按
            // CurrentPage() 分发：以前只有剪贴板页认这个 id，停在总览 /
            // 配置页时定时器空转，看上去就是「复制了但总数不动」，非得
            // 点一下剪贴板页（OnShow 主动拉数据）才补上。
            if (msg == WM_TIMER && wParam == TIMER_REFRESH_NOTIFY) {
                KillTimer(hwnd, TIMER_REFRESH_NOTIFY);
                RefreshData();
                result = 0;
                return true;
            }
            // 服务心跳（30s 兜底）：断开回调已是实时主触发（WM_APP+3），
            // 这里仅作为慢后备，防止断开回调漏触发。
            if (msg == WM_TIMER && wParam == TIMER_SVC_HEARTBEAT) {
                if (!m_ipc.IsConnected() && m_shell.Hwnd())
                    PostMessage(m_shell.Hwnd(), WM_APP + 3, 0, 0);
                result = 0;
                return true;
            }
            // 服务离线重连（3s 兜底）：服务不可用时轮询，连接一恢复就
            // 同步 pending → 退出离线态 → 刷新统计与外壳 → 停定时器。
            if (msg == WM_TIMER && wParam == TIMER_SVC_RECONNECT) {
                KillTimer(hwnd, TIMER_SVC_RECONNECT);
                // 每 3s 触发一轮：服务在线时作例行心跳（发 PING 保持服务端
                // 会话最新，避免 60s 兜底误拉重复实例）；服务离线时才做重连。
                // **不能在 UI 线程直接调 Connect()**：它内部有 10×100ms 的
                // TryConnect 探测循环，离线时每次探测会把 UI 线程卡 ~1s
                // （用户报告"点击隔约 1 秒才有反应"的规律性卡顿正是这个）。
                // 探测放到后台线程，成功后经 WM_APP+7 回 UI 线程做恢复。
                if (m_reconnecting.exchange(true))
                    return true;                  // 上一次探测还在跑：跳过本轮
                // 防抖（m_reconnecting）已保证上一个探测线程结束，join 立即
                // 返回；再用具名可 join 线程持有，析构时等待，杜绝 detach 裸 this。
                if (m_reconnectThread.joinable()) m_reconnectThread.join();
                m_reconnectThread = std::thread(&App::ReconnectProbe, this, hwnd, m_offline.load());
                result = 0;
                return true;
            }
            // 服务恢复探测完成（后台线程投递，UI 线程）：成功→同步+清离线；
            // 失败→继续下轮探测。
            if (msg == WM_APP + 7) {
                if (wParam == 0) {
                    // 服务已恢复：同步暂存 + 重登记 + 退出离线 + 异步刷新
                    if (m_agent) m_agent->FlushPending();
                    RegisterSession();
                    SetServiceOffline(false);
                    RefreshData();               // 后台线程拉数据，UI 不阻塞
                    Toast(L"服务已恢复", L"离线期间暂存的剪贴板记录已同步");
                } else if (wParam == 2) {
                    // 例行心跳完成：仅重新武装 3s 定时器，让下轮心跳继续。
                    // 不弹 Toast、不改离线态、不刷新数据（服务一直在线）。
                    StartReconnectWatch();
                } else {
                    StartReconnectWatch();       // 还没起来 → 下轮继续探测
                }
                result = 0;
                return true;
            }
            // 服务端连接断开（实时，reader 线程投递）→ 重启服务并重连。
            // ServiceDied 只发起异步恢复，不在 UI 线程阻塞轮询（会卡界面）。
            if (msg == WM_APP + 3) {
                ServiceDied();
                result = 0;
                return true;
            }
            // 服务恢复完成（后台线程 EnsureService 结束，回 UI 线程收尾）：
            // 重连、刷新统计、状态栏、Toast。wParam=1 恢复成功，0 失败。
            if (msg == WM_APP + 4) {
                OnServiceRecovered(wParam != 0);
                result = 0;
                return true;
            }
            // 后台刷新线程拉完 STATS + LIST（只做 IPC，不碰 UI），经此消息
            // 回到 UI 线程应用结果：重建剪贴板列表、同步外壳文案、重绘。
            // 页切换/搜索/新记录刷新都不再阻塞 UI。
            if (msg == WM_APP + 6) {
                // 后台线程把 RefreshResult 堆分配后投递回来；UI 线程在此统一
                // 写入 m_stats / m_svcState / m_svcText / m_entries（唯一写者），
                // 再应用界面。PostMessage 失败时后台已 delete out（lParam 为 0）。
                std::unique_ptr<RefreshResult> r(reinterpret_cast<RefreshResult*>(lParam));
                if (r) {
                    m_stats    = r->stats;
                    m_svcState = r->svcState;
                    m_svcText  = std::move(r->svcText);
                    m_entries  = std::move(r->entries);
                    ApplyRefresh();
                }
                result = 0;
                return true;
            }
            // 定时器先给插件（F3 搜索防抖 250ms）
            if (msg == WM_TIMER)
                m_plugins.DispatchTimer(*this, (UINT_PTR)wParam);

            return m_plugins.DispatchWndProc(*this, hwnd, msg, wParam,
                                             lParam, result);
        });

    // 状态栏与侧栏一直标着 F3 / F4，但此前没有任何地方注册过它们。
    // 关键是走消息循环而不是窗口过程：按键消息投给当前焦点窗口，
    // 焦点通常在搜索框 / 录入框 / 列表里，主窗口根本收不到。
    m_shell.SetAcceleratorHandler([this](const MSG& msg) -> bool {
        // 系统组合键（Alt+F4 等）一律交给系统；模态对话框打开时不抢键；
        // 窗口隐藏时交给全局 Ctrl+Shift+V 唤出。
        if (msg.message == WM_SYSKEYDOWN ||
            m_shell.IsModalActive() || !IsWindowVisible(m_shell.Hwnd()))
            return false;
        const HWND main = m_shell.Hwnd();
        if (msg.hwnd != main && !IsChild(main, msg.hwnd)) return false;
        // Alt+F4 / Ctrl+F4 是系统组合，不能吞。
        if ((GetKeyState(VK_CONTROL) & 0x8000) ||
            (GetKeyState(VK_MENU)    & 0x8000) ||
            (GetKeyState(VK_SHIFT)   & 0x8000)) return false;

        switch (msg.wParam) {
        case VK_F3: FocusSearch();     return true;
        case VK_F4: OpenManualEntry(); return true;
        default:                       return false;
        }
    });

    // ---- 页面 ----
    auto overview = std::make_unique<OverviewPage>(*this);
    auto clips    = std::make_unique<ClipsPage>(*this);
    auto config   = std::make_unique<ConfigPage>(*this);
    m_overview = overview.get();
    m_clips    = clips.get();
    m_config   = config.get();

    m_shell.AddPage(0, std::move(overview));
    m_shell.AddPage(1, std::move(clips));
    m_shell.AddPage(2, std::move(config));

    // ---- 高 DPI 动态适配（WM_DPICHANGED → Shell 已重建字体/度量） ----
    // 页面层在此重建「按旧 DPI 像素缓存的几何」：ClipsPage 列表行高
    // （groupH/itemH）是 AddItem 时定死的老 DPI 值，不重建则高 DPI 下
    // 行高偏小、选中/命中错位。ApplyEntries 纯内存重建（ScopedBatch
    // 只提交一帧），UI 线程安全。回调在 WM_DPICHANGED 处理中途触发，
    // 此刻 m_dpi 已更新、Metrics/Fonts 已是新 DPI 的值。
    m_shell.SetDpiChangedHandler([this](int dpi) {
        (void)dpi;
        if (m_clips) m_clips->RebuildListForDpi();
    });

    // ---- 侧栏结构（功能模块 / 快捷操作 / 系统） ----
    {
        std::vector<NavGroup> groups;

        NavGroup mods;
        mods.title = L"功能模块";
        mods.items = {
            { 0,   L"\xE80F", L"工作台总览", L"",       false, false },
            { 1,   L"\xE8C8", L"剪贴板",     L"",       false, false },
            { -11, L"\xE8B7", L"文件监控",   L"规划中", false, true  },
            { -12, L"\xE749", L"打印监控",   L"规划中", false, true  },
        };
        groups.push_back(std::move(mods));

        NavGroup ops;
        ops.title = L"快捷操作";
        ops.items = {
            { 100, L"\xE70F", L"手动录入",       L"F4", false, false },
            { 101, L"\xE74D", L"清空剪贴板记录", L"",   false, false },
        };
        groups.push_back(std::move(ops));

        NavGroup sys;
        sys.title = L"系统";
        sys.items = {
            { 2,  L"\xE713", L"机器配置", L"", false, false },
            { -2, L"\xE72C", L"服务管理", L"", false, false },
        };
        groups.push_back(std::move(sys));

        m_shell.SetNavGroups(std::move(groups));
    }

    // 非页面导航项（手动录入 / 清空 / 服务管理）
    m_shell.SetNavCommandHandler([this](int id) {
        switch (id) {
        case 100:   // 手动录入 → 切到剪贴板页并展开表单卡
            OpenManualEntry();
            break;
        case 101:   // 清空全部记录
            if (m_shell.ShowMessageBox(
                    L"确定清空全部剪贴板记录吗？\n此操作不可恢复。",
                    L"清空记录", MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
                const std::wstring resp = m_ipc.Request(L"CLEAR");
                RefreshData();
                if (resp.rfind(L"OK", 0) == 0)
                    Toast(L"已清空", L"所有剪贴板记录已删除");
                else
                    Toast(L"清空失败", L"服务未响应，请稍后重试", false);
            }
            break;
        case -2:    // 服务管理 → 配置页
            m_shell.SwitchPage(2);
            RefreshData();
            break;
        default:
            break;
        }
    });

    // ---- 状态栏快捷键提示（从右往左排列：F4 / F3 / Ctrl+Shift+V） ----
    m_shell.SetStatusKeys({
        { L"Ctrl+Shift+V", L"呼出 / 隐藏" },
        { L"F3",           L"搜索" },
        { L"F4",           L"手动录入" },
    });

    // ---- 插件 ----
    if (!m_plugins.AttachAll(*this)) {
        diag::Log(L"[fatal] plugin attach failed");
        m_plugins.DetachAll(*this);
        return 2;
    }

    // ---- 首屏 ----
    // SwitchPage(0) 的 OnShow(true) 已走 RefreshData()：拉列表快照 +
    // 统计 + 同步外壳文案 + 标脏，不必再重复三次 IPC。
    m_shell.SwitchPage(0);

    // 窗口位置记忆：在窗口首次显示/绘制前恢复上次的位置与尺寸（避免在
    // 默认位置闪一帧）。放在 RunMessageLoop 之前——那时窗口已创建但尚未
    // ShowWindow/首帧，SetWindowPos 不会引入可见抖动。
    RestoreWindowRect();

    diag::Log(L"[ui] message loop begin, stats.total=%lld, svc=%d hidden=%d delay=%d",
              (long long)m_stats.total, (int)m_svcState,
              (int)m_startHidden, m_showDelayMs);
    if (m_showDelayMs > 0)
        SetTimer(m_shell.Hwnd(), TIMER_PROBE_HIDDEN, (UINT)m_showDelayMs, nullptr);
    // 服务心跳兜底：断开回调（WM_APP+3）已是实时主触发；此 30s 定时器
    // 仅作慢后备。窗口隐藏时消息循环仍在跑，定时器照常触发。
    SetTimer(m_shell.Hwnd(), TIMER_SVC_HEARTBEAT, 30000, nullptr);
    int rc = m_shell.RunMessageLoop();
    diag::Log(L"[ui] message loop end, rc=%d", rc);
    m_plugins.DetachAll(*this);
    return rc;
}

// ============================================================
// 门面
// ============================================================
HWND App::Hwnd()
{
    return m_shell.Hwnd();
}

// 插件属集成层：仍以 HWND 交互（页面内部已完全 Widget 化）
HWND App::List()          { return m_clips ? m_clips->List().Native() : nullptr; }
HWND App::SearchEdit()    { return m_clips ? m_clips->SearchEdit().Native() : nullptr; }
HWND App::ManualEdit()    { return m_clips ? m_clips->ManualEdit().Native() : nullptr; }
HWND App::NoteEdit()      { return m_clips ? m_clips->NoteEdit().Native() : nullptr; }
HWND App::NoteEditMulti() { return m_clips ? m_clips->NoteEditMulti().Native() : nullptr; }

void App::RefreshData()
{
    // 不在 UI 线程做阻塞 IPC：后台线程拉 STATS + LIST，完成后经 WM_APP+6
    // 回 UI 线程应用（ApplyRefresh）。服务连接看似在线实则已死时，Request
    // 会在 m_cv.wait_for(1500ms) 上阻塞——若放 UI 线程，切页就会卡 1.5s 起。
    // 用 m_refreshing 防抖：上一次还在拉则跳过本次，绝不叠发后台线程。
    if (m_refreshing.exchange(true)) return;

    // 在 UI 线程先把筛选文本取出来（不能在后台线程读 m_clips->GetSearchText()，
    // 那是 UI 控件）；按值传进工作线程，后台线程只做纯 IPC、不碰 UI 控件。
    const std::wstring filter = m_clips ? m_clips->GetSearchText() : L"";

    // 防抖已保证上一个刷新线程结束（若有），join 立即返回；再用具名可 join
    // 线程持有，析构时等待，杜绝 detach 裸 this。结果经 WM_APP+6 回投，由
    // UI 线程统一写 m_stats / m_svcState / m_svcText / m_entries。
    if (m_refreshThread.joinable()) m_refreshThread.join();
    m_refreshThread = std::thread(&App::RefreshWorker, this, m_shell.Hwnd(), filter);
}

// WM_APP+6（UI 线程）：把后台线程拉好的 m_stats / m_entries 应用到界面。
void App::ApplyRefresh()
{
    // 只有停在剪贴板页才重建列表控件；总览/配置页读 m_stats / m_entries
    // 直接重绘即可（它们已在后台线程更新）。
    if (m_clips && m_shell.CurrentPageId() == 1)
        m_clips->ApplyEntries();

    SyncServiceUi();        // 标题栏胶囊 / 状态栏 / 侧栏角标
    m_shell.Invalidate();   // 总览 / 配置页纯自绘，数据变了要手动标脏
}

std::wstring App::GetSearchText()
{
    return m_clips ? m_clips->GetSearchText() : std::wstring();
}

bool App::IsEditingNote() const
{
    return m_clips && m_clips->IsEditingNote();
}

void App::SaveNoteEdit()
{
    if (m_clips) m_clips->SaveNoteEdit();
}

void App::CancelNoteEdit()
{
    if (m_clips) m_clips->CancelNoteEdit();
}

void App::ToggleForm(bool open)
{
    if (m_clips) m_clips->ToggleForm(open);
}

void App::Toast(const std::wstring& title, const std::wstring& message, bool ok)
{
    m_shell.ToastManager().Show(title, message,
        ok ? air::ui::ToastKind::Success : air::ui::ToastKind::Error);
}

void App::SetThemeDark(bool dark)
{
    if (m_dark == dark) return;
    m_dark = dark;
    machine::Set(L"ui", L"dark", dark ? L"1" : L"0");

    // 换整套调色板：外壳 / 页面 / 自绘控件都只读 Pal()，换色即全局生效。
    air::ui::SetPalette(dark ? air::ui::DarkPalette() : air::ui::LightPalette());

    m_shell.Invalidate();
    // 子控件（TextBox / ListBox / Custom）是独立子窗口、各自 WM_PAINT 读
    // Pal()；父窗 InvalidateRect(...,FALSE) 不会波及子窗口，需逐个标脏。
    EnumChildWindows(m_shell.Hwnd(),
        [](HWND h, LPARAM) -> BOOL {
            InvalidateRect(h, nullptr, FALSE);
            return TRUE;
        }, 0);
}

void App::RestoreWindowRect()
{
    HWND hwnd = m_shell.Hwnd();
    if (!hwnd) return;

    const std::wstring raw = machine::Get(L"ui", L"window_rect", L"");
    if (raw.empty()) return;   // 首次运行：沿用 Shell 的默认位置

    // 解析 "x,y,w,h"
    int x = 0, y = 0, w = 0, h = 0;
    if (swscanf_s(raw.c_str(), L"%d,%d,%d,%d", &x, &y, &w, &h) != 4) return;
    if (w < 200 || h < 150) return;   // 尺寸异常（损坏）→ 不恢复

    // 校验落点：取窗口当前所在显示器的工作区，把保存的矩形夹进去。
    // 处理「上次停在已拔掉的副屏」——夹回主屏可见区，而不是恢复到屏幕外。
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    RECT wa{};
    if (mon && GetMonitorInfoW(mon, &mi))
        wa = mi.rcWork;
    else {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    }
    if (wa.right - wa.left <= 0) return;

    // 完全落在工作区外 → 居中；否则夹入可见区（保留用户意图的相对位置）
    bool offscreen = (x + w <= wa.left || x >= wa.right ||
                      y + h <= wa.top || y >= wa.bottom);
    if (offscreen) {
        x = wa.left + ((wa.right - wa.left) - w) / 2;
        y = wa.top + ((wa.bottom - wa.top) - h) / 2;
    } else {
        if (x < wa.left) x = wa.left;
        if (y < wa.top)  y = wa.top;
        if (x + w > wa.right) x = wa.right - w;
        if (y + h > wa.bottom) y = wa.bottom - h;
    }
    SetWindowPos(hwnd, nullptr, x, y, w, h,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
}

void App::SaveWindowRect()
{
    HWND hwnd = m_shell.Hwnd();
    if (!hwnd) return;

    // 最大化 / 最小化时 GetWindowRect 返回的是显示器边界 / 任务栏坐标，
    // 不是用户真正摆放的位置——不保存，避免下次启动被"顶"到角落。
    if (IsZoomed(hwnd) || IsIconic(hwnd)) return;

    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) return;
    wchar_t buf[64];
    swprintf_s(buf, 64, L"%d,%d,%d,%d",
               rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
    machine::Set(L"ui", L"window_rect", buf);
}

void App::FocusSearch()
{
    m_shell.SwitchPage(1);
    if (!m_clips) return;
    HWND edit = m_clips->SearchEdit().Native();
    if (!edit) return;
    SetFocus(edit);
    // 已有关键字时全选，按一下 F3 就能直接改 / 直接输新的。
    SendMessageW(edit, EM_SETSEL, 0, (LPARAM)-1);
}

void App::OpenManualEntry()
{
    m_shell.SwitchPage(1);
    if (m_clips) m_clips->OpenForm();   // 内部会聚焦多行录入框
}

std::wstring App::GetWindowTextStr(HWND ctrl)
{
    return air::ui::GetEditText(ctrl);
}

// ============================================================
// 服务状态 / 统计
// ============================================================
void App::SyncServiceUi()
{
    // 离线态：保持红色告警文案（标题栏/状态栏/底栏），不被刷新覆盖。
    if (m_offline.load()) {
        m_shell.SetCaptionBadge(L"服务不可用 · 记录暂存本地", false);
        m_shell.SetStatus(L"服务不可用 · 剪贴板记录暂存本地 · 恢复后自动同步", false);
        m_shell.SetFooterText(L"AireRecord · 服务不可用，记录暂存本地");
        m_shell.SetNavBadge(-2, L"已停止", false);
        m_shell.SetSidebarInfo(L"本机信息", {
            { L"运行模式", L"服务 + 代理", false },
            { L"服务自启", m_svcState == 2 ? L"已启用" : L"未启用", m_svcState == 2 },
            { L"条目", L"—", false },
        });
        return;
    }
    bool ok = (m_stats.total >= 0);   // STATS 有应答即视为服务连通

    // 标题栏胶囊
    std::wstring cap = ok
        ? (L"服务运行中 · 已记录 " +
           (m_stats.total >= 0 ? std::to_wstring((long)m_stats.total) : L"—") +
           L" 条")
        : L"服务未连接";
    m_shell.SetCaptionBadge(cap, ok);

    // 状态栏
    m_shell.SetStatus(ok ? L"服务运行中（airecordd.exe）" : L"服务未连接", ok);
    m_shell.SetStatusExtra(L"启用模块 1 / 3");

    // 侧栏角标
    m_shell.SetNavBadge(1,
        m_stats.total >= 0 ? std::to_wstring((long)m_stats.total) : L"", false);
    m_shell.SetNavBadge(-2,
        m_svcState == 2 ? L"运行中" : L"已停止", m_svcState == 2);

    // 侧栏信息卡
    m_shell.SetSidebarInfo(L"本机信息", {
        { L"运行模式", L"服务 + 代理", false },
        { L"服务自启", m_svcState == 2 ? L"已启用" : L"未启用", m_svcState == 2 },
        { L"条目",
          m_stats.total >= 0 ? std::to_wstring((long)m_stats.total) + L" 条" : L"—",
          false },
    });

    m_shell.SetFooterText(ok ? std::wstring(kAppFooterPrefix) + L"服务已连接"
                             : std::wstring(kAppFooterPrefix) + L"服务未连接");
}

// ============================================================
// 服务断开（WM_APP+3，UI 线程）：服务停止/崩溃 → UI **不退出**，
// 进入离线态（红色告警 + 剪贴板暂存本地 + 每 3s 尝试重连）。
//
// SCM 失败操作负责在服务崩溃时自动重启（Session 0）；STOPPING 事件
// 是用户主动 sc stop，服务不会自动起来，重连直到用户重新启动才成功。
// 两种情况下 UI 都保持存活，记录不丢失。
// ============================================================
void App::ServiceDied()
{
    diag::Log(L"[svc-died] service down, UI offline (stays alive)");
    SetServiceOffline(true);
    StartReconnectWatch();
}

// 进入 / 退出离线态：更新标题栏胶囊、状态栏、底栏与页面横幅；重绘。
void App::SetServiceOffline(bool off)
{
    const bool wasOff = m_offline.exchange(off);
    // 离线态：禁止 Request 自动拉起/重启服务（避免 UI 在服务停机时擅自
    // 重启）。恢复后放开，让首次启动/手动恢复仍能拉起。
    m_ipc.SetAutoStart(!off);
    if (wasOff == off) return;   // 状态未变，避免反复刷新

    if (off) {
        m_shell.SetCaptionBadge(L"服务不可用 · 记录暂存本地", /*ok=*/false);
        m_shell.SetStatus(L"服务不可用 · 剪贴板记录暂存本地 · 恢复后自动同步", false);
        m_shell.SetFooterText(L"AireRecord · 服务不可用，记录暂存本地");
    } else {
        // 服务恢复：恢复标准文案（刷新统计后由 SyncServiceUi 填充具体数字）
        m_shell.SetCaptionBadge(L"服务运行中 · 已记录 — 条", true);
        m_shell.SetStatus(L"服务运行中（airecordd.exe）", true);
        m_shell.SetFooterText(std::wstring(kAppFooterPrefix) + L"服务已连接");
    }
    m_shell.Invalidate();        // 页面重绘横幅
}

// 离线期间每 3s 尝试重连。成功：同步 pending → 重新登记会话 → 退出离线态
// → 刷新统计与外壳 → 停止定时器。
void App::StartReconnectWatch()
{
    if (m_shell.Hwnd())
        SetTimer(m_shell.Hwnd(), TIMER_SVC_RECONNECT, 3000, nullptr);
}

// 统一退出：停止系统服务（best-effort，服务已停则 no-op），再强制退出本 UI。
// 仅由托盘「退出」等用户显式退出调用。
void App::ExitAll()
{
    // 1) 停止服务（SCM ControlService，发起即返回，不等完全停止）。
    //    服务已停/已死时是 no-op（返回 ERROR_SERVICE_NOT_ACTIVE）。
    SC_HANDLE mgr = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (mgr) {
        SC_HANDLE svc = OpenServiceW(mgr, L"AireRecord", SERVICE_STOP);
        if (svc) {
            SERVICE_STATUS st{};
            ControlService(svc, SERVICE_CONTROL_STOP, &st);
            CloseServiceHandle(svc);
        }
        CloseServiceHandle(mgr);
    }
    // 2) 强制退出本 UI（绕过 close_behavior=hide）。
    if (m_agent) m_agent->Stop();
    m_forceExit = true;
    if (m_shell.Hwnd())
        PostMessage(m_shell.Hwnd(), WM_CLOSE, 0, 0);
}

// 服务恢复完成（WM_APP+4，UI 线程）：重连 + 刷新状态 + Toast。
void App::OnServiceRecovered(bool ok)
{
    if (ok) {
        m_suppressRestartUntil.store(0);  // 服务已恢复：解除停止抑制，后续崩溃仍自动重启
        RegisterSession();          // 重新登记本会话（供断开时按会话重启）
        // 数据刷新走后台线程（异步），不在 UI 线程阻塞 IPC。
        RefreshData();
        Toast(L"服务已重启", L"系统服务已恢复，后台记录继续运行");
    } else {
        // 未能恢复（如服务被卸载且用户拒绝 UAC）→ 状态栏如实报告失败
        m_svcState = sysinfo::QueryServiceState(L"AireRecord", m_svcText);
        SyncServiceUi();
        diag::Log(L"[svc-died] recovery failed, svc=%d", m_svcState);
    }
}

// 经持久连接向服务端登记本会话号。连接建立后调用一次；服务端借此把
// 该连接关联到本会话，断开时就能按会话号实时重启本会话 UI。
void App::RegisterSession()
{
    m_ipc.Request(L"PING\t" + std::to_wstring(diag::SessionId()));
}

} // namespace arecord
