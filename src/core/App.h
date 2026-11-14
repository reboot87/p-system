#pragma once
// ============================================================
// App.h — 客户端应用容器
//
// 结构：
//   App（本文件）  持有 Shell + IPC + 插件，是应用门面，不含任何绘制
//   pages/*        每个页面实现 air::ui::IPage，自带布局/绘制/交互
//
// 这样界面能力全部来自 airui 库，AireRecord 只写业务。
// ============================================================
#include <windows.h>
#include <memory>
#include <string>
#include <vector>

#include "PluginManager.h"
#include "UiEntry.h"
#include "MachineConfig.h"
#include "../ipc/PipeClient.h"
#include "../agent/AgentHost.h"
#include "airui/AirUi.h"

// 控件 ID（页面与插件共用）

class AgentHost;

// 控件 ID（页面与插件共用）
enum {
    IDC_SEARCH_EDIT = 1001,   // 搜索框
    IDC_LIST        = 1002,   // 记录列表（owner-draw 卡片）
    IDC_MANUAL_EDIT = 1003,   // 内容录入框
    IDC_SAVE_BUTTON = 1004,   // 保存按钮
    IDC_NOTE_EDIT   = 1005,   // 备注输入框（表单卡，单行）
    IDC_SEARCH_CHIP = 1006,   // 搜索框右侧快捷键提示（独立置顶子窗口）
    IDC_NOTE_MULTI  = 1007,   // 备注输入框（详情面板，多行）
};

// 定时器 ID
enum {
    TIMER_SEARCH_DEBOUNCE = 1, // 搜索防抖 250ms
    TIMER_REFRESH_NOTIFY  = 2, // 服务端新记录通知 → 300ms 防抖刷新
    TIMER_SYSINFO         = 3, // 配置页 CPU/内存 2s 轮询
    TIMER_SVC_HEARTBEAT   = 8, // 服务心跳 2s（PING\t<sessionId>，检测/拉起服务）
    TIMER_PROBE_HIDDEN    = 9, // 静默启动后的延迟显示（规避启动期窗口检查）
};

// 服务统计（IPC STATS）
struct ServiceStats
{
    long long total   = -1;   // 总条目（-1 = 未知）
    long long today   = 0;
    long long dbBytes = 0;
};

class ClipsPage;
class OverviewPage;
class ConfigPage;

class App
{
public:
    App() = default;
    ~App();   // 需在 .cpp 中定义，以便 unique_ptr<AgentHost> 完整析构

    int Run(HINSTANCE hInstance);

    // 服务端 exe 路径（IPC 自动拉起用）
    void SetServicePath(const std::wstring& path) { m_ipc.SetServicePath(path); }

    // 静默启动：不显示主窗口（用于被策略关闭可见窗口的环境，托盘/热键再唤出）
    void SetStartHidden(bool on) { m_startHidden = on; }
    // 延迟显示：窗口先隐藏创建，ms 毫秒后再显示。
    // 有些终端管控/DLP 会在进程启动后 0.3~0.6s 内关闭"新出现的可见窗口"，
    // 隐藏创建 + 延后显示可绕开该检查（>600ms 稳定通过，默认留 800ms 余量）。
    void SetShowDelayMs(int ms)  { m_showDelayMs = ms; }
    // 显示时不抢焦点（SW_SHOWNOACTIVATE）。部分终端管控会把"非白名单程序
    // 自动抢前台"当成弹窗行为直接关闭窗口；不激活显示可避开。
    void SetShowNoActivate(bool on) { m_showNoActivate = on; }

    // 关闭策略（机器级 [ui] close_behavior）：
    //   "hide"（默认）关闭主窗口仅隐藏，后台剪贴板代理与托盘继续运行；
    //   "exit"          关闭主窗口即退出整个进程（含后台代理）。
    void SetCloseBehavior(const std::wstring& v)
        { machine::Set(L"ui", L"close_behavior", v.c_str()); }

    // 运行时绘制后端（"gdi" / "skia" / "auto"）。透传给 ShellOptions，
    // 不需要重新编译，也不需要本机安装 Skia SDK（插件按需加载）。
    void SetPaintBackend(const std::string& id) { m_paintBackend = id; }

    // 启用 ShellCore / HAL 外壳渲染路径（--core-shell）。注册表只在
    // 这条路径上生效：不打开它，--paint-backend 不会改变主窗口绘制。
    void SetCoreShell(bool on) { m_coreShell = on; }

    // ---- 外壳 ----
    air::ui::Shell& Shell() { return m_shell; }
    HWND Hwnd();

    // ---- 页面 ----
    ClipsPage*    Clips()    { return m_clips; }
    OverviewPage* Overview() { return m_overview; }
    ConfigPage*   Config()   { return m_config; }

    // ---- 数据/服务（页面与插件共用） ----
    PipeClient&    Ipc()     { return m_ipc; }
    PluginManager& Plugins() { return m_plugins; }

    const ServiceStats& Stats() const { return m_stats; }
    void RefreshStats();          // IPC STATS + SCM 服务状态
    int  ServiceState() const { return m_svcState; }
    const std::wstring& ServiceText() const { return m_svcText; }

    // 列表数据（剪贴板页负责拉取，总览页共享同一份快照）
    const std::vector<UiEntry>& Entries() const { return m_entries; }
    std::vector<UiEntry>&       EntriesMut()    { return m_entries; }
    void SetEntries(std::vector<UiEntry> entries) { m_entries = std::move(entries); }

    // 把服务状态同步到标题栏胶囊 / 状态栏 / 侧栏角标
    void SyncServiceUi();

    // 数据变更后的统一刷新：列表快照 + 统计 + 外壳文案 + 重绘。
    // m_entries / m_stats 是跨页面共享的（总览页画总数和「最近动态」），
    // 所以刷新不能挂在某一个页面上；服务端来新记录时用户可能正停在
    // 工作台总览或机器配置页。
    void RefreshData();

    // 服务端连接断开（由断开回调经 UI 线程触发）：服务停止/崩溃 → UI 也退出。
    void ServiceDied();

    // 统一退出：停止服务（best-effort，服务已停则 no-op）+ 强制退出本 UI。
    void ExitAll();

    // 服务恢复完成（后台 EnsureService 结束后回 UI 线程）：重连+刷新+Toast。
    void OnServiceRecovered(bool ok);

    // 经持久连接向服务端登记本会话号（供断开时按会话重启 UI）。
    void RegisterSession();

    // ---- 剪贴板页门面（插件依赖的稳定接口） ----
    HWND List();
    HWND SearchEdit();
    HWND ManualEdit();
    HWND NoteEdit();
    HWND NoteEditMulti();
    void RefreshList(const std::wstring& filter = L"");
    std::wstring GetSearchText();
    bool IsEditingNote() const;
    void SaveNoteEdit();
    void CancelNoteEdit();
    void ToggleForm(bool open);

    // 非阻塞反馈：自绘 Toast（成功 / 失败两种语气）
    void Toast(const std::wstring& title,
               const std::wstring& message = L"", bool ok = true);

    static std::wstring GetWindowTextStr(HWND ctrl);

    // 应用版本（侧栏底部、配置页）
    static const wchar_t* Version() { return L"v1.3"; }

private:
    // 状态栏 / 侧栏上写着的两个功能键
    void FocusSearch();       // F3：切到剪贴板页并聚焦搜索框
    void OpenManualEntry();   // F4：切到剪贴板页并展开录入卡

    air::ui::Shell m_shell;
    PipeClient     m_ipc;
    PluginManager  m_plugins;

    ClipsPage*    m_clips    = nullptr;
    OverviewPage* m_overview = nullptr;
    ConfigPage*   m_config   = nullptr;

    // 合并后台剪贴板代理（AgentHost）。同进程后台线程 + 独立隐藏窗口。
    std::unique_ptr<AgentHost> m_agent;
    bool m_forceExit = false;   // 托盘"退出" → 强制关闭（不受 close_behavior 影响）

    ServiceStats m_stats;
    int          m_svcState = 0;   // 0未装 1停止 2运行
    std::wstring m_svcText;
    std::vector<UiEntry> m_entries;
    std::atomic<bool> m_svcRecovering{ false };   // 服务恢复线程是否进行中
    std::atomic<unsigned long long> m_suppressRestartUntil{ 0 }; // 收到 STOPPING 后暂停自动重启的截止时间
    bool         m_startHidden = false;
    int          m_showDelayMs = 0;
    bool         m_showNoActivate = false;
    std::string  m_paintBackend;   // empty = auto (env / gdi)
    bool         m_coreShell = false;
};
