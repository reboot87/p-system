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

class App;
struct ServiceStats;

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
    bool OnTimer(UINT_PTR id) override;
    void OnSetFocus() override;

    // ---- 对外（App 门面 / 插件） ----
    air::ui::ListBox& List()          { return *m_list; }
    air::ui::TextBox& SearchEdit()    { return *m_search; }
    air::ui::TextBox& ManualEdit()    { return *m_manualEdit; }
    air::ui::TextBox& NoteEdit()      { return *m_noteEdit; }
    air::ui::TextBox& NoteEditMulti() { return *m_noteMulti; }

    void         RefreshList(const std::wstring& filter = L"");
    std::wstring GetSearchText();

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
    static RECT DetailNoteBox(RECT detail);
    static int  TypeOfEntry(const UiEntry& e);

    // 绘制
    void DrawToolbar(air::ui::Canvas& g);
    void DrawFormCard(air::ui::Canvas& g);
    void DrawDetailPanel(air::ui::Canvas& g);
    void DrawListRow(air::ui::Canvas& g, const air::ui::ListItemDraw& item);

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
    air::ui::TextBox* m_search     = nullptr;
    air::ui::Custom*  m_searchChip = nullptr;
    air::ui::ListBox* m_list       = nullptr;
    air::ui::TextBox* m_manualEdit = nullptr;
    air::ui::TextBox* m_noteEdit   = nullptr;
    air::ui::TextBox* m_noteMulti  = nullptr;
    air::ui::Button*  m_saveBtn    = nullptr;

    // 几何（客户区绝对坐标）
    RECT m_content     = {};
    RECT m_rcListCard  = {};
    RECT m_rcFormCard  = {};
    RECT m_rcSearchBox = {};
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

    int  m_typeFilter = 0;      // 0全部 1文本 2链接 3手动
    bool m_formOpen   = false;
    int  m_selEntry   = -1;
    long long m_editingId = 0;
    int  m_lineH = 0;           // 单行 EDIT 客户区高（= 一行文字高）
    bool m_visible = false;
};
