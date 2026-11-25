#pragma once
// ============================================================
// ClipsPage — 剪贴板模块
// 工具条（搜索 + 类型胶囊 + 手动录入）/ 分组卡片列表 / 详情面板
// 列表与表单走 IPC，备注就地编辑
//
// 全程使用 airui 高级 API：Canvas 绘制、Widget 控件，
// 不接触 HDC / HWND / DRAWITEMSTRUCT。
// ============================================================
#include "airui/AirUi.h"
#include "airui/Interaction.h"
#include "../core/UiEntry.h"

namespace arecord { class App; }
using namespace arecord;

class ClipsPage : public air::ui::Page
{
public:
    explicit ClipsPage(App& app) : m_app(app) {}

    const wchar_t* Title() const override    { return L"剪贴板模块"; }
    const wchar_t* NavGlyph() const override { return L"\xE8C8"; }
    const wchar_t* NavLabel() const override { return L"剪贴板"; }

    // ---- Page ----
    void OnAttach(air::ui::Shell& shell) override;
    void OnDetach() override;
    void OnShow(bool shown) override;
    void OnLayout(const RECT& content) override;
    void OnPaint(air::ui::Canvas& g, const RECT& content) override;
    bool OnMouseDown(POINT pt) override;
    bool OnMouseUp(POINT pt) override;
    bool OnMouseMove(POINT pt) override;
    bool OnMouseLeave() override;
    bool HandCursorAt(POINT pt) override;
    bool OnCommand(int ctrlId, int code) override;
    void OnSetFocus() override;

    // ---- 对外（App 门面 / 插件） ----
    air::ui::ListBox& List()          { return *m_list; }
    air::ui::TextBox& SearchEdit()    { return *m_search; }
    air::ui::TextBox& ManualEdit()    { return *m_manualEdit; }
    air::ui::TextBox& NoteEdit()      { return *m_noteEdit; }
    air::ui::TextBox& NoteEditMulti() { return *m_noteMulti; }

    // 只拉取共享快照（m_app.Entries()），不碰列表控件。总览页的
    // 「最近动态」也读这份数据，因此停在任何页面都要能刷新它。
    // 纯 IPC：请求 LIST 并解析成本地 vector<UiEntry>，不写任何共享成员——
    // m_entries 只由 UI 线程写入（WM_APP+6 / RefreshList）。后台刷新线程调用。
    std::vector<UiEntry> FetchEntries(const std::wstring& filter, bool* outOk = nullptr);
    void                 RefreshList(const std::wstring& filter = L"");
    void                 ApplyEntries();   // 仅从 m_app.Entries() 重建列表，不 Request（UI 线程）
    // DPI 变化后强制重建列表（不重取数据）：行高（groupH/itemH）是
    // AddItem 时按当时 Mx() 定死的像素值，DPI 变了必须重排。重置数据
    // 指纹（ApplyEntries 的早退判据）后走纯内存重建，无 IPC、离线安全。
    void                 RebuildListForDpi();
    std::wstring         GetSearchText();

    bool IsEditingNote() const { return m_editingId != 0; }
    void StartNoteEdit(long long id, const std::wstring& note);
    void SaveNoteEdit();
    void CancelNoteEdit();
    void SwitchNoteEditTarget(int newEntryIdx);

    void ToggleForm(bool open);
    void OpenForm();          // 切页后展开表单卡并聚焦录入框

private:
    // 子控件创建与显隐
    void CreateControls();
    void SyncVisibility();
    void Relayout();
    // 「×」清空按钮的显隐 + 搜索框右内边距（搜索文本非空时显示）
    void SyncSearchClearButton();
    static RECT DetailNoteBox(RECT detail);
    static int  TypeOfEntry(const UiEntry& e);

    // 绘制
    void DrawToolbar(air::ui::Canvas& g);
    void DrawFormCard(air::ui::Canvas& g);
    void DrawDetailPanel(air::ui::Canvas& g);
    void DrawListRow(air::ui::Canvas& g, const air::ui::ListItemDraw& item);
    void DrawOfflineBanner(air::ui::Canvas& g, const RECT& banner);

    // 交互
    void OnListOp(int entryIdx, int op);          // 0复制 1备注 2删除
    void ShowItemMenu(POINT screenPt, int entryIdx);

    // ---- 自绘热区 ----
    // 工具条胶囊、＋按钮、详情面板三键、备注框都是 Canvas 画的，没有
    // HWND，Windows 不会替我们记悬停/按下：状态在这里维护。
    enum HotId : int
    {
        kHotNone      = 0,
        kHotChip      = 100,   // +0..3
        kHotAddBtn    = 110,
        kHotFormClose = 111,
        kHotDBtn      = 120,   // +0..2
        kHotNoteBox   = 130,
    };
    air::ui::HotState m_hot;
    int  m_hoverOp    = -1;   // 列表行内操作钮：0复制 1备注 2删除
    int  m_hoverOpRow = -1;
    int  HitHot(POINT absPt) const;
    void ActivateHot(int hotId);
    void SyncHoverOp(int row, POINT ptInItem);

    App& m_app;

    // 控件（Widget 包装，HWND 在库内）
    air::ui::TextBox* m_search      = nullptr;
    air::ui::Custom*  m_searchChip  = nullptr;
    air::ui::Custom*  m_searchClear = nullptr;
    air::ui::ListBox* m_list        = nullptr;
    air::ui::TextBox* m_manualEdit = nullptr;
    air::ui::TextBox* m_noteEdit   = nullptr;
    air::ui::TextBox* m_noteMulti  = nullptr;
    air::ui::Button*  m_saveBtn    = nullptr;

    // 几何（客户区绝对坐标）
    RECT m_content     = {};
    RECT m_rcListCard  = {};
    RECT m_rcFormCard  = {};
    RECT m_rcSearchBox = {};
    RECT m_rcSearchClear = {};
    RECT m_rcManualBox = {};
    RECT m_rcNoteBox   = {};
    RECT m_rcNoteMulti = {};
    RECT m_rcChips[4]  = {};
    RECT m_rcAddBtn    = {};
    RECT m_rcFormClose = {};
    RECT m_rcDetail    = {};
    RECT m_rcDBtn[3]   = {};

    // 行模型（分组头 / 条目）
    struct Row { bool header; std::wstring label; size_t entry; };
    std::vector<Row> m_rows;

    // 上次已应用到 ListBox 的条目指纹（数量 + 首条 id + 末条 id）。
    // 用于跳过"数据未变却反复重建列表"：切页回来时 ListBox 已是现成的，
    // 无需 Reset + 逐行 AddItem（大列表时这段是 UI 线程卡顿主因）。
    long long m_appliedFp = -1;

    int  m_typeFilter = 0;      // 0全部 1文本 2链接 3手动
    bool m_formOpen   = false;
    int  m_selEntry   = -1;
    long long m_editingId = 0;
    bool m_visible = false;
    int  m_bannerH = 0;      // 服务离线横幅高度（离线时 ~28px，在线 0）
};
