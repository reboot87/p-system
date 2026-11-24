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
#include <atomic>
#include <thread>

#include "PluginManager.h"
#include "UiEntry.h"
#include "MachineConfig.h"
#include "../ipc/PipeClient.h"
#include "../agent/AgentHost.h"
#include "airui/AirUi.h"

// 版本单一来源：CMake `project(AireRecord VERSION ...)` 生成的版本头。
#include "airui/Version.h"

// 页面类保持在全局作用域（它们继承 air::ui::Page，属于业务层而非框架层）。
class ClipsPage;
class OverviewPage;
class ConfigPage;

namespace arecord
{

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
    IDC_SEARCH_CLEAR = 1008,  // 搜索框「×」清空（独立置顶子窗口，文本非空时显示）
};

// 定时器 ID
enum {
    TIMER_SEARCH_DEBOUNCE = 1, // 搜索防抖 250ms
    TIMER_REFRESH_NOTIFY  = 2, // 服务端新记录通知 → 300ms 防抖刷新
    TIMER_SYSINFO         = 3, // 配置页 CPU/内存 2s 轮询
    TIMER_SVC_HEARTBEAT   = 8, // 服务心跳 2s（PING\t<sessionId>，检测/拉起服务）
    TIMER_PROBE_HIDDEN    = 9, // 静默启动后的延迟显示（规避启动期窗口检查）
    TIMER_SVC_RECONNECT   = 10, // 服务不可用时每 3s 尝试重连（离线恢复）
    TIMER_WINDOW_RECT     = 11, // 窗口移动/缩放后 800ms 防抖 → 持久化位置
};

// 服务统计（IPC STATS）
struct ServiceStats
{
    long long total   = -1;   // 总条目（-1 = 未知）
    long long today   = 0;
    long long dbBytes = 0;
};

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

    // 暗色模式（机器级 [ui] dark）。运行期切换：换整套 Palette 后全量重绘
    // （外壳 / 页面 / 自绘控件都读 Pal()，所以换色即全局生效），并持久化。
    void SetThemeDark(bool dark);
    bool IsDark() const { return m_dark; }

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

    // 后台刷新线程完成后，在 UI 线程把 m_stats / m_entries 应用到界面
    // （重建剪贴板列表、同步外壳文案、重绘）。由 WM_APP+6 触发。
    void ApplyRefresh();

    // 服务端连接断开（由断开回调经 UI 线程触发）：服务停止/崩溃 → 进入离线态。
    void ServiceDied();

    // 服务离线（STOPPING / 断开 / 启动失败）：UI 保持存活，红色告警 + 定期重连。
    void SetServiceOffline(bool off);
    bool IsServiceOffline() const { return m_offline.load(); }

    // 离线时每 3s 尝试重连，成功则同步 pending、清除告警、刷新统计。
    void StartReconnectWatch();

    // 统一退出：停止服务（best-effort，服务已停则 no-op）+ 强制退出本 UI。
    // 仅由托盘「退出」等用户显式退出调用。
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
    std::wstring GetSearchText();
    bool IsEditingNote() const;
    void SaveNoteEdit();
    void CancelNoteEdit();
    void ToggleForm(bool open);

    // 非阻塞反馈：自绘 Toast（成功 / 失败两种语气）
    void Toast(const std::wstring& title,
               const std::wstring& message = L"", bool ok = true);

    // 窗口位置记忆：启动时按 [ui] window_rect 恢复（需有活动显示器命中），
    // WM_MOVE/WM_SIZE 经 800ms 防抖后持久化。最大化/最小化不保存。
    void RestoreWindowRect();
    void SaveWindowRect();

    static std::wstring GetWindowTextStr(HWND ctrl);

    // 应用版本（侧栏底部、配置页）—— 单一来源：生成的 Version.h
    static const wchar_t* Version() { return AIRUI_VERSION_W; }

private:
    // 状态栏 / 侧栏上写着的两个功能键
    void FocusSearch();       // F3：切到剪贴板页并聚焦搜索框
    void OpenManualEntry();   // F4：切到剪贴板页并展开录入卡

    // 后台任务入口（可 join 线程，析构等待，杜绝 detach 线程裸 this 逃逸）
    void ReconnectProbe(HWND hwnd, bool wasOffline);   // 离线重连探测 / 在线例行心跳
    void RefreshWorker(HWND hwnd, std::wstring filter);  // 异步数据刷新

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
    std::atomic<bool> m_reconnecting{ false };    // 离线重连探测线程是否在跑
    std::atomic<bool> m_refreshing{ false };      // 后台数据刷新是否进行中（防抖）
    std::atomic<bool> m_offline{ false };         // 服务不可用：UI 进入离线态（暂存+重连）
    // 可 join 的后台任务线程：启动新线程前 join 前一个（防抖已保证其结束），
    // 析构时置 m_shuttingDown 后 join，杜绝 detach 线程悬空 / 裸 this 逃逸。
    std::thread         m_reconnectThread;
    std::thread         m_refreshThread;
    std::atomic<bool>   m_shuttingDown{ false };  // 退出标志：后台线程据此不再投递
    std::atomic<unsigned long long> m_suppressRestartUntil{ 0 }; // 收到 STOPPING 后暂停自动重启的截止时间
    bool         m_startHidden = false;
    int          m_showDelayMs = 0;
    bool         m_showNoActivate = false;
    std::string  m_paintBackend;   // empty = auto (env / gdi)
    bool         m_dark = false;   // 暗色模式（启动时从 [ui] dark 读取）
    bool         m_rectSaveArmed = false;  // 窗口位置防抖定时器已武装
    bool         m_coreShell = false;
};

} // namespace arecord
