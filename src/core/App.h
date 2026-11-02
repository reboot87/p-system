#pragma once
// ============================================================
// App.h — 客户端 UI 宿主（C/S 架构的 Client）
// 纯界面层：列表/搜索/录入通过命名管道与服务端通信，
// 本进程崩溃或关闭不影响后台记录。
//
// 视觉：Apple HIG 白卡片 + 中性灰蓝（ui-ux-pro-max 引擎推导）
// ============================================================
#include <windows.h>
#include <string>
#include <vector>
#include "PluginManager.h"
#include "../ipc/PipeClient.h"
#include "SysInfo.h"

// 数据条目（由 IPC 解析而来，与服务端 entries 表对应）
struct UiEntry
{
    long long    id = 0;
    std::wstring source;
    std::wstring createdAt;
    std::wstring note;
    std::wstring content;
};

// ---- 工作台页面 ----
enum WorkPage {
    PageOverview = 0,   // 工作台总览（默认页：模块卡片 + 最近动态）
    PageClips    = 1,   // 剪贴板模块（记录列表 + 录入）
    PageConfig   = 2,   // 机器配置（系统 + 硬件采集）
};

// 控件 ID（插件与核心共用）
enum {
    IDC_SEARCH_EDIT = 1001,   // F3 搜索框
    IDC_LIST        = 1002,   // 记录列表（owner-draw 双行卡片）
    IDC_MANUAL_EDIT = 1003,   // F4 内容录入框
    IDC_SAVE_BUTTON = 1004,   // F4 保存按钮（自绘强调色）
    IDC_NOTE_EDIT   = 1005,   // 备注输入框
    IDC_SEARCH_CHIP = 1006,   // 搜索框右侧 Ctrl+Shift+V 快捷键提示（独立置顶子窗口）
};

// 定时器 ID
enum {
    TIMER_SEARCH_DEBOUNCE = 1, // F3 搜索防抖 250ms
    TIMER_REFRESH_NOTIFY  = 2, // 服务端新记录通知 → 300ms 防抖刷新
    TIMER_SYSINFO         = 3, // 配置页动态数据（CPU/内存）2s 刷新
};

// 服务统计（IPC STATS）
struct ServiceStats
{
    long long total     = -1;   // 总条目（-1 = 未知）
    long long today     = 0;    // 今日新增
    long long dbBytes   = 0;    // 库文件大小
};

class App
{
public:
    int Run(HINSTANCE hInstance);

    // 设置服务端 exe 路径（用于 IPC 自动拉起）
    void SetServicePath(const std::wstring& path) { m_ipc.SetServicePath(path); }

    HWND     Hwnd() const { return m_hwnd; }
    HWND     List() const { return m_hList; }
    HWND     SearchEdit() const { return m_hSearch; }
    HWND     ManualEdit() const { return m_hManualEdit; }
    HWND     NoteEdit() const { return m_hNoteEdit; }
    PipeClient& Ipc() { return m_ipc; }
    PluginManager& Plugins() { return m_plugins; }

    // 刷新列表：filter 为空显示最近记录，否则按关键字/备注查询（走 IPC）
    void RefreshList(const std::wstring& filter = L"");

    // 读取搜索框当前关键字
    std::wstring GetSearchText();

    // ---- 页面切换（工作台导航） ----
    void SwitchPage(WorkPage page);
    WorkPage Page() const { return m_page; }

    // 服务统计（总览/配置页展示）
    const ServiceStats& Stats() const { return m_stats; }
    void RefreshStats();

    // ---- 备注编辑（右键菜单 / 详情面板入口 → 编辑态） ----
    bool   IsEditingNote() const { return m_editingId != 0; }
    void   StartNoteEdit(long long id, const std::wstring& note);
    void   SaveNoteEdit();      // 保存当前编辑的备注并退出编辑态
    void   CancelNoteEdit();    // 放弃编辑（Esc）

    // ---- 剪贴板模块页：手动录入表单卡显隐（＋手动录入 / F4） ----
    void   ToggleForm(bool open);

    // 右键菜单：复制内容 / 编辑备注 / 删除记录
    void   ShowItemMenu(POINT screenPt, int item);

    // 读取任意控件文本（插件共用）
    static std::wstring GetWindowTextStr(HWND ctrl);

    // Owner-draw 数据
    const std::vector<UiEntry>& Items() const { return m_entries; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // 列表/保存按钮子类化（hover 跟踪、空状态绘制）
    static LRESULT CALLBACK ListProc(HWND, UINT, WPARAM, LPARAM,
                                     UINT_PTR, DWORD_PTR);
    static LRESULT CALLBACK SaveBtnProc(HWND, UINT, WPARAM, LPARAM,
                                        UINT_PTR, DWORD_PTR);
    // 输入框子类化：Esc 收起表单 / 取消备注编辑；搜索框占位提示
    static LRESULT CALLBACK EditPlaceholderProc(HWND, UINT, WPARAM, LPARAM,
                                                UINT_PTR, DWORD_PTR);
    // 搜索框右侧 Ctrl+Shift+V 快捷键提示：独立置顶子窗口，避免被编辑框 hover/焦点重绘覆盖
    static LRESULT CALLBACK SearchChipProc(HWND, UINT, WPARAM, LPARAM,
                                          UINT_PTR, DWORD_PTR);

    bool CreateMainWindow(HINSTANCE hInstance);
    void CreateUI(HWND hwnd);
    void Layout();
    void ApplyRoundedRegion(HWND ctrl, int radius);
    void UpdateFont(HWND ctrl);

    void DrawListItem(const DRAWITEMSTRUCT* dis);
    void DrawSaveButton(const DRAWITEMSTRUCT* dis);
    void DrawTitleBar(HDC dc, const RECT& rc);
    void SetStatusText(const std::wstring& text, bool ok);

    // ---- 工作台绘制 ----
    void DrawSidebar(HDC dc, const RECT& rc);
    void DrawOverviewPage(HDC dc, const RECT& rc);
    void DrawConfigPage(HDC dc, const RECT& rc);
    void DrawStatusBar(HDC dc, const RECT& rc);
    // 剪贴板模块页：工具条（搜索+筛选胶囊+主按钮）/ 表单卡 / 详情面板
    void DrawClipsPage(HDC dc, const RECT& rc);
    void DrawDetailPanel(HDC dc);
    // 配置卡片渲染器：rows 中 "k|v|bar" 三段式（bar 为 0-100 进度，<0 无条）
    void DrawInfoCard(HDC dc, RECT rc, const wchar_t* icon,
                      const wchar_t* title, const wchar_t* badge,
                      bool badgeOk,
                      const std::vector<std::wstring>& rows);
    static int CardHeight(int rowCount);

    // ---- 剪贴板模块页数据/几何 ----
    struct ListRow { bool header; std::wstring label; size_t entry; };
    static int TypeOfEntry(const UiEntry& e);   // 1文本 2链接 3手动
    int  DetailButtonAt(POINT clientPt) const;  // 0无 1复制 2备注 3删除
    void OnListOp(int entryIdx, int op);        // 0复制 1备注 2删除

    // ---- 侧栏导航 ----
    struct NavItem { int id; const wchar_t* icon; const wchar_t* label;
                     const wchar_t* badge; bool disabled; };
    int  NavItemAt(POINT clientPt) const;    // 返回导航项索引，-1 无
    void OnNavClick(int index);

    // ---- 自绘标题栏（无边框窗口壳） ----
    int  CaptionButtonAt(POINT clientPt) const;   // 0无 1最小化 2最大化 3关闭
    static std::wstring FormatRelTime(const std::wstring& createdAt);

    HWND           m_hwnd        = nullptr;
    HWND           m_hSearch     = nullptr;
    HWND           m_hList       = nullptr;
    HWND           m_hManualEdit = nullptr;
    HWND           m_hNoteEdit   = nullptr;
    HWND           m_hSaveBtn    = nullptr;
    HWND           m_hSearchChip = nullptr;   // 搜索框右侧 Ctrl+Shift+V 快捷键提示（置顶子窗口）
    HFONT          m_font     = nullptr;  // 正文 14
    HFONT          m_fontMeta = nullptr;  // 元信息 12
    HFONT          m_fontTitle= nullptr;  // 应用名 13 semibold
    HFONT          m_fontSection = nullptr; // 卡片标题 14 semibold
    HFONT          m_fontBtn  = nullptr;  // 按钮 14 semibold
    HFONT          m_fontIcon = nullptr;  // 标题栏图标字体（Segoe MDL2）
    HFONT          m_fontIconBig = nullptr; // 总览/卡片大图标 16
    HFONT          m_fontBig  = nullptr;  // 总览大数字 22 semibold
    HBRUSH         m_brBg     = nullptr;
    HBRUSH         m_brInput  = nullptr;
    HBRUSH         m_brWhite  = nullptr;
    RECT           m_rcListCard = {};     // 记录列表卡片区域
    RECT           m_rcFormCard = {};     // 添加记录卡片区域
    RECT           m_rcSide      = {};    // 侧栏区域
    bool           m_btnHover = false;
    int            m_hoverItem = -1;      // 列表 hover 项
    int            m_capHover  = 0;       // 标题栏按钮 hover（1/2/3）
    int            m_navHover  = -1;      // 侧栏导航 hover 项
    long long      m_editingId = 0;       // 正在编辑备注的条目 id

    // ---- 工作台状态 ----
    WorkPage       m_page    = PageOverview;
    ServiceStats   m_stats;
    SysInfoData    m_sys;
    bool           m_sysLoaded = false;

    // ---- 剪贴板模块页（原型三区） ----
    std::vector<ListRow> m_rows;       // 列表行（分组头/条目）
    int    m_typeFilter = 0;           // 0全部 1文本 2链接 3手动
    bool   m_formOpen   = false;       // 手动录入表单卡展开
    int    m_selEntry   = -1;          // 详情面板展示的条目下标
    int    m_svcState   = 0;           // SCM 状态缓存（0未装 1停止 2运行）
    std::wstring m_svcText;
    RECT   m_rcChips[4]  = {};         // 类型筛选胶囊（命中测试）
    RECT   m_rcAddBtn    = {};         // ＋手动录入 主按钮
    RECT   m_rcFormClose = {};         // 表单卡收起按钮
    RECT   m_rcDetail    = {};         // 详情面板区域
    RECT   m_rcDBtn[3]   = {};         // 详情按钮：复制/备注/删除
    RECT   m_rcModCard[3]= {};         // 总览模块卡（点击进入模块）

    std::wstring   m_statusText = L"连接中…";
    bool           m_serviceOk  = false;

    std::vector<UiEntry> m_entries;       // 当前列表数据（owner-draw 用）
    PipeClient     m_ipc;
    PluginManager  m_plugins;
};
