#include "App.h"
#include "DiagLog.h"
#include "SysInfo.h"
#include "airui/PaintBackend.h"
#include "airui/PaintProviders.h"
#include "../ipc/Ipc.h"
#include "pages/OverviewPage.h"
#include "pages/ClipsPage.h"
#include "pages/ConfigPage.h"

using air::ui::NavGroup;
using air::ui::NavItem;
using air::ui::ShellOptions;

// ============================================================
// 启动
// ============================================================
int App::Run(HINSTANCE hInstance)
{
    ShellOptions opt;
    // 类名保持不变：单实例检测与任务栏行为维持原样
    opt.className = L"AireRecordWnd";
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
    // 注册表只在 ShellCore/HAL 外壳路径上生效；--core-shell 打开它。
    opt.coreShell = m_coreShell;

    if (!m_shell.Create(hInstance, opt)) {
        diag::Log(L"[fatal] shell create failed, gle=%lu", GetLastError());
        return 1;
    }

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

        diag::Log(L"[paint] coreShell=%d requested=%hs resolved=%hs available=[%hs] skiaDll=%d probe=%hs",
                  (int)m_coreShell,
                  m_paintBackend.empty() ? "auto" : m_paintBackend.c_str(),
                  resolved.c_str(),
                  available.c_str(),
                  (int)air::ui::SkiaPluginLoaded(),
                  probeError.empty() ? "ok" : probeError.c_str());
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
            // 定时器先给插件（F3 搜索防抖 250ms）
            if (msg == WM_TIMER)
                m_plugins.DispatchTimer(*this, (UINT_PTR)wParam);

            return m_plugins.DispatchWndProc(*this, hwnd, msg, wParam,
                                             lParam, result);
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
            m_shell.SwitchPage(1);
            if (m_clips) m_clips->OpenForm();
            break;
        case 101:   // 清空全部记录
            if (MessageBoxW(m_shell.Hwnd(),
                    L"确定清空全部剪贴板记录吗？\n此操作不可恢复。",
                    L"清空记录", MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
                m_ipc.Request(L"CLEAR");
                if (m_clips) m_clips->RefreshList(GetSearchText());
                RefreshStats();
                SyncServiceUi();
            }
            break;
        case -2:    // 服务管理 → 配置页
            m_shell.SwitchPage(2);
            RefreshStats();
            SyncServiceUi();
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
    m_shell.SwitchPage(0);
    RefreshList();
    RefreshStats();
    SyncServiceUi();

    diag::Log(L"[ui] message loop begin, stats.total=%lld, svc=%d hidden=%d delay=%d",
              (long long)m_stats.total, (int)m_svcState,
              (int)m_startHidden, m_showDelayMs);
    if (m_showDelayMs > 0)
        SetTimer(m_shell.Hwnd(), TIMER_PROBE_HIDDEN, (UINT)m_showDelayMs, nullptr);
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

void App::RefreshList(const std::wstring& filter)
{
    if (m_clips) m_clips->RefreshList(filter);
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

std::wstring App::GetWindowTextStr(HWND ctrl)
{
    return air::ui::GetEditText(ctrl);
}

// ============================================================
// 服务状态 / 统计
// ============================================================
void App::RefreshStats()
{
    std::wstring resp = m_ipc.Request(L"STATS");
    if (resp.rfind(L"OK", 0) == 0) {
        auto f = ipc::SplitTabs(resp);
        if (f.size() >= 4) {
            m_stats.total   = _wtoi64(f[1].c_str());
            m_stats.today   = _wtoi64(f[2].c_str());
            m_stats.dbBytes = _wtoi64(f[3].c_str());
        }
    }
    m_svcState = sysinfo::QueryServiceState(L"AireRecord", m_svcText);
}

void App::SyncServiceUi()
{
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

    m_shell.SetFooterText(ok ? L"AireRecord v1.3.1 · 服务已连接"
                             : L"AireRecord v1.3.1 · 服务未连接");
}
