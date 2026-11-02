#pragma once
// ============================================================
// ClipsPage — 剪贴板模块
// 工具条（搜索 + 类型胶囊 + 手动录入）/ 分组卡片列表 / 详情面板
// 列表与表单走 IPC，备注就地编辑
// ============================================================
#include "airui/AirUi.h"
#include "../core/UiEntry.h"

class App;
struct ServiceStats;

class ClipsPage : public air::ui::IPage
{
public:
    explicit ClipsPage(App& app) : m_app(app) {}

    const wchar_t* Title() const override    { return L"剪贴板模块"; }
    const wchar_t* NavGlyph() const override { return L"\xE8C8"; }
    const wchar_t* NavLabel() const override { return L"剪贴板"; }

    // ---- IPage ----
    void OnAttach(air::ui::Shell& shell) override;
    void OnDetach() override;
    void OnShow(bool shown) override;
    void OnLayout(const RECT& content) override;
    void OnPaint(HDC dc, const RECT& content) override;
    bool OnMouseDown(POINT pt) override;
    bool OnMouseMove(POINT pt) override;
    bool OnMouseLeave() override;
    bool OnCommand(int ctrlId, int code) override;
    bool OnTimer(UINT_PTR id) override;
    void OnSetFocus() override;
    bool OnDrawItem(const DRAWITEMSTRUCT* dis) override;
    bool OnMeasureItem(MEASUREITEMSTRUCT* mis) override;

    // ---- 对外（App 门面 / 插件） ----
    HWND List()          const { return m_hList; }
    HWND SearchEdit()    const { return m_hSearch; }
    HWND ManualEdit()    const { return m_hManualEdit; }
    HWND NoteEdit()      const { return m_hNoteEdit; }
    HWND NoteEditMulti() const { return m_hNoteMulti; }

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
    const air::ui::Fonts& F() const { return m_shell->UiFonts(); }

    // 子控件创建与显隐
    void CreateControls();
    void SyncVisibility();
    void Relayout();
    static RECT DetailNoteBox(RECT detail);
    static int  TypeOfEntry(const UiEntry& e);

    // 绘制
    void DrawToolbar(HDC dc);
    void DrawFormCard(HDC dc);
    void DrawDetailPanel(HDC dc);
    void DrawListRow(const DRAWITEMSTRUCT* dis);

    // 交互
    void OnListOp(int entryIdx, int op);          // 0复制 1备注 2删除
    int  DetailButtonAt(POINT absPt) const;       // 0无 1复制 2备注 3删除
    void ShowItemMenu(POINT screenPt, int entryIdx);

    // 子类化
    static LRESULT CALLBACK ListProc(HWND, UINT, WPARAM, LPARAM,
                                     UINT_PTR, DWORD_PTR);
    static LRESULT CALLBACK ChipProc(HWND, UINT, WPARAM, LPARAM,
                                     UINT_PTR, DWORD_PTR);

    App& m_app;

    // 控件
    HWND m_hSearch     = nullptr;
    HWND m_hSearchChip = nullptr;
    HWND m_hList       = nullptr;
    HWND m_hManualEdit = nullptr;
    HWND m_hNoteEdit   = nullptr;
    HWND m_hNoteMulti  = nullptr;
    HWND m_hSaveBtn    = nullptr;

    // 几何（客户区绝对坐标）
    RECT m_content     = {};
    RECT m_rcListCard  = {};
    RECT m_rcFormCard  = {};
    RECT m_rcSearchBox = {};
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
    int  m_hoverItem  = -1;
    bool m_btnHover   = false;
    long long m_editingId = 0;
    int  m_lineH = 21;          // 单行 EDIT 客户区高（= 一行文字高）
    bool m_visible = false;
};
