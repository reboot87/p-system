#include "ClipsPage.h"
#include "core/App.h"
#include "core/Format.h"
#include "../ipc/Ipc.h"
#include <commctrl.h>

using namespace air::ui;

// ============================================================
// 创建与显隐
// ============================================================
void ClipsPage::CreateControls()
{
    HWND parent = m_shell->Hwnd();
    HFONT body = F().Body();
    HFONT meta = F().Meta();

    // 单行 EDIT 永远把文字画在客户区顶部 → 客户区压到一行文字高才真正居中
    {
        HDC dc = GetDC(parent);
        m_lineH = gfx::LineHeight(dc, body);
        ReleaseDC(parent, dc);
    }

    // ---- 搜索框 ----
    m_hSearch = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, parent, (HMENU)IDC_SEARCH_EDIT, nullptr, nullptr);
    SendMessageW(m_hSearch, WM_SETFONT, (WPARAM)body, TRUE);
    SetEditMargins(m_hSearch, 8, 8);
    PlaceholderEdit::Attach(m_hSearch, L"搜索内容和备注…", body);

    // ---- 搜索框右侧 Ctrl+Shift+V 提示（独立置顶子窗口，避免被 EDIT 重绘覆盖） ----
    m_hSearchChip = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD, 0, 0, 0, 0, parent, (HMENU)IDC_SEARCH_CHIP, nullptr, nullptr);
    SetWindowSubclass(m_hSearchChip, &ClipsPage::ChipProc, 5, (DWORD_PTR)this);
    SetWindowPos(m_hSearchChip, m_hSearch, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE);

    // ---- 记录列表 ----
    m_hList = CreateWindowExW(0, L"LISTBOX", L"",
        WS_CHILD | WS_VSCROLL | WS_TABSTOP |
        LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWVARIABLE,
        0, 0, 0, 0, parent, (HMENU)IDC_LIST, nullptr, nullptr);
    SendMessageW(m_hList, WM_SETFONT, (WPARAM)body, TRUE);
    SendMessageW(m_hList, LB_SETITEMHEIGHT, 0, 54);
    SetWindowSubclass(m_hList, &ClipsPage::ListProc, 1, (DWORD_PTR)this);

    // ---- 备注（表单卡，单行） ----
    m_hNoteEdit = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, parent, (HMENU)IDC_NOTE_EDIT, nullptr, nullptr);
    SendMessageW(m_hNoteEdit, WM_SETFONT, (WPARAM)body, TRUE);
    SetEditMargins(m_hNoteEdit, 8, 8);
    PlaceholderEdit::Attach(m_hNoteEdit, L"备注（可选）", body);

    // ---- 备注（详情面板，多行） ----
    // 多行：备注显示区本就折行，单行框无法编辑长备注；
    // 字号与显示态一致（12px），进入编辑时换行位置不跳动。
    m_hNoteMulti = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        0, 0, 0, 0, parent, (HMENU)IDC_NOTE_MULTI, nullptr, nullptr);
    SendMessageW(m_hNoteMulti, WM_SETFONT, (WPARAM)meta, TRUE);
    SetEditMargins(m_hNoteMulti, 8, 8);
    PlaceholderEdit::Attach(m_hNoteMulti, L"输入备注…", meta);
    PlaceholderEdit::SetKeyCallback(m_hNoteMulti,
        [this](HWND, UINT vk, bool ctrl, bool) -> bool {
            if (vk == VK_ESCAPE && m_editingId) { CancelNoteEdit(); return true; }
            if (vk == VK_RETURN && ctrl && m_editingId) {
                SaveNoteEdit(); return true;
            }
            return false;
        });

    // ---- 手动录入（多行） ----
    m_hManualEdit = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        0, 0, 0, 0, parent, (HMENU)IDC_MANUAL_EDIT, nullptr, nullptr);
    SendMessageW(m_hManualEdit, WM_SETFONT, (WPARAM)body, TRUE);
    SetEditMargins(m_hManualEdit, 8, 8);
    PlaceholderEdit::Attach(m_hManualEdit, L"输入要保存的内容…", body);
    PlaceholderEdit::SetKeyCallback(m_hManualEdit,
        [this](HWND, UINT vk, bool ctrl, bool) -> bool {
            // Ctrl+Enter 由 ManualEntryPlugin 的子类化处理（它后装、先收到消息）
            if (vk == VK_ESCAPE && m_formOpen) { ToggleForm(false); return true; }
            (void)ctrl;
            return false;
        });

    // ---- 主按钮 ----
    m_hSaveBtn = CreateWindowExW(0, L"BUTTON", L"保存记录",
        WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 0, 0, parent, (HMENU)IDC_SAVE_BUTTON, nullptr, nullptr);
    SendMessageW(m_hSaveBtn, WM_SETFONT, (WPARAM)F().Get(FontRole::Button), TRUE);
    AttachButton(m_hSaveBtn, BtnStyle::Primary);
}

void ClipsPage::OnAttach(Shell& shell)
{
    (void)shell;
    CreateControls();
    SyncVisibility();
}

void ClipsPage::OnDetach()
{
    // 子控件随父窗口销毁；这里只清掉按钮的窗口属性，避免残留指针
    if (m_hSaveBtn) DetachButton(m_hSaveBtn);
}

void ClipsPage::OnShow(bool shown)
{
    m_visible = shown;

    if (!shown) {
        SyncVisibility();
        KillTimer(m_shell->Hwnd(), TIMER_REFRESH_NOTIFY);
        return;
    }

    // 挂起重绘 → 布局 + 显隐一次完成 → 恢复（无双帧闪烁）
    HWND h = m_shell->Hwnd();
    SendMessageW(h, WM_SETREDRAW, FALSE, 0);
    Relayout();
    SyncVisibility();
    SendMessageW(h, WM_SETREDRAW, TRUE, 0);

    RefreshList(GetSearchText());
    m_app.RefreshStats();
    m_app.SyncServiceUi();
}

void ClipsPage::SyncVisibility()
{
    if (!m_hSearch) return;

    bool show = m_visible;
    ::ShowWindow(m_hSearch,     show ? SW_SHOW : SW_HIDE);
    ::ShowWindow(m_hSearchChip, show ? SW_SHOW : SW_HIDE);
    ::ShowWindow(m_hList,       show ? SW_SHOW : SW_HIDE);

    bool form = show && m_formOpen && !m_editingId;
    bool edit = show && (m_editingId != 0);

    ::ShowWindow(m_hManualEdit, form ? SW_SHOW : SW_HIDE);
    ::ShowWindow(m_hNoteEdit,   form ? SW_SHOW : SW_HIDE);
    ::ShowWindow(m_hNoteMulti,  edit ? SW_SHOW : SW_HIDE);
    ::ShowWindow(m_hSaveBtn,    (form || edit) ? SW_SHOW : SW_HIDE);
}

void ClipsPage::OnLayout(const RECT& content)
{
    m_content = content;
    Relayout();
}

// ============================================================
// 布局（8px 栅格）
// ============================================================
void ClipsPage::Relayout()
{
    if (!m_hSearch || m_content.right <= m_content.left) return;

    const Metrics& m = Mx();
    RECT content = m_content;

    // ---- 工具条：搜索框 ----
    int searchW = 320;
    if (searchW > content.right - content.left - 420)
        searchW = content.right - content.left - 420;
    if (searchW < 160) searchW = 160;

    const int kSearchBoxH = m.searchH;
    m_rcSearchBox = { content.left, content.top,
                      content.left + searchW, content.top + kSearchBoxH };

    MoveWindow(m_hSearch,
               m_rcSearchBox.left + 1,
               m_rcSearchBox.top + (kSearchBoxH - m_lineH) / 2,
               searchW - 2, m_lineH, TRUE);

    // 快捷键提示 chip：右对齐，与搜索框同高居中
    {
        HDC dc = GetDC(m_shell->Hwnd());
        const wchar_t* key = L"Ctrl+Shift+V";
        int chipW = gfx::MeasureWidth(dc, F().Meta(), key) + 12;
        ReleaseDC(m_shell->Hwnd(), dc);

        int chipH = 18;
        int cx = m_rcSearchBox.right - chipW - 6;
        int cy = m_rcSearchBox.top + (kSearchBoxH - chipH) / 2;
        MoveWindow(m_hSearchChip, cx, cy, chipW, chipH, TRUE);
        // 右内边距让出 chip 区域，文字不压到标签下
        SendMessageW(m_hSearch, EM_SETMARGINS, EC_RIGHTMARGIN,
                     MAKELPARAM(0, chipW + 10));
    }

    m_rcAddBtn = { content.right - 112, content.top + 1,
                   content.right, content.top + 35 };

    // ---- 类型筛选胶囊 ----
    {
        HDC dc = GetDC(m_shell->Hwnd());
        const wchar_t* labels[4] = { L"全部", L"文本", L"链接", L"手动" };
        int cx = content.left + searchW + 14;
        int cy = content.top + 4;
        for (int i = 0; i < 4; ++i) {
            int w = gfx::MeasureWidth(dc, F().Meta(), labels[i]);
            m_rcChips[i] = { cx, cy, cx + w + 24, cy + 28 };
            cx = m_rcChips[i].right + 8;
        }
        ReleaseDC(m_shell->Hwnd(), dc);
    }

    // ---- 详情面板 / 列表区 ----
    const int kDetailW = 300;
    m_rcDetail = { content.right - kDetailW, content.top + 48,
                   content.right, content.bottom };
    m_rcListCard = { content.left, content.top + 48,
                     m_rcDetail.left - 14, content.bottom };

    // ---- 表单卡 ----
    const int FormH = 194;
    if (m_formOpen || m_editingId) {
        m_rcFormCard = { m_rcListCard.left, m_rcListCard.bottom - FormH,
                         m_rcListCard.right, m_rcListCard.bottom };
    } else {
        m_rcFormCard = m_rcListCard;
    }

    RECT rcList = m_rcListCard;
    if (m_formOpen) rcList.bottom = m_rcFormCard.top - 10;
    MoveWindow(m_hList, rcList.left, rcList.top,
               rcList.right - rcList.left, rcList.bottom - rcList.top, TRUE);

    // ---- 表单卡内部控件 ----
    if (m_formOpen && !m_editingId) {
        int fx = m_rcFormCard.left + 16;
        int fw = (m_rcFormCard.right - m_rcFormCard.left) - 32;
        int contentY = m_rcFormCard.top + 16 + 22 + 8;
        MoveWindow(m_hManualEdit, fx, contentY, fw, 88, TRUE);

        int rowY = contentY + 88 + 12;
        int btnW = 96;
        m_rcNoteBox = { fx, rowY, fx + fw - btnW - 12, rowY + m.inputH };
        MoveWindow(m_hNoteEdit,
                   m_rcNoteBox.left + 1,
                   m_rcNoteBox.top + (m.inputH - m_lineH) / 2,
                   m_rcNoteBox.right - m_rcNoteBox.left - 2,
                   m_lineH, TRUE);
        MoveWindow(m_hSaveBtn, fx + fw - btnW, rowY, btnW, m.inputH, TRUE);
        m_rcFormClose = { m_rcFormCard.right - 62, m_rcFormCard.top + 14,
                          m_rcFormCard.right - 16, m_rcFormCard.top + 36 };
        m_rcNoteMulti = {};
    }
    // ---- 备注编辑态：多行框覆盖详情面板备注区 ----
    else if (m_editingId && m_rcDetail.right > m_rcDetail.left) {
        RECT nb = DetailNoteBox(m_rcDetail);
        m_rcNoteMulti = nb;
        // 内缩 4px 给父窗口画的边框留空间
        MoveWindow(m_hNoteMulti, nb.left + 4, nb.top + 4,
                   nb.right - nb.left - 8, nb.bottom - nb.top - 8, TRUE);
        MoveWindow(m_hSaveBtn, m_rcDetail.left + 16, nb.bottom + 10,
                   96, 30, TRUE);
        m_rcFormClose = {};
        m_rcNoteBox = {};
    } else {
        m_rcNoteBox = {};
        m_rcNoteMulti = {};
    }
}

// 详情面板「备注」框几何（布局与绘制共用）
RECT ClipsPage::DetailNoteBox(RECT d)
{
    int y = d.top + 14;      // "详情"
    y += 20 + 10;            // 类型胶囊
    y += 22 + 10;            // 内容框
    y += 96 + 14;            // "属性"
    y += 18 + 4;             // 属性行
    y += 4 * 22 + 12;        // "备注"
    y += 18 + 8;             // 备注框
    return { d.left + 16, y, d.right - 16, y + 64 };
}

// ============================================================
// 数据
// ============================================================
std::wstring ClipsPage::GetSearchText()
{
    return m_hSearch ? GetEditText(m_hSearch) : std::wstring();
}

int ClipsPage::TypeOfEntry(const UiEntry& e)
{
    if (e.source == L"Manual") return 3;
    const std::wstring& c = e.content;
    auto starts = [&c](const wchar_t* p) {
        size_t n = wcslen(p);
        return c.size() >= n && _wcsnicmp(c.c_str(), p, n) == 0;
    };
    if (starts(L"http://") || starts(L"https://") ||
        starts(L"www.") || starts(L"ftp://"))
        return 2;
    return 1;
}

void ClipsPage::RefreshList(const std::wstring& filter)
{
    std::vector<UiEntry> entries;

    std::wstring resp = m_app.Ipc().Request(L"LIST\t" + ipc::EscapeField(filter));

    bool ok = false;
    if (!resp.empty()) {
        auto lines = ipc::SplitLines(resp);
        if (!lines.empty() && lines[0] == L"OK") {
            ok = true;
            for (size_t i = 1; i < lines.size(); ++i) {
                if (lines[i] == L"END") break;
                auto f = ipc::SplitTabs(lines[i]);
                if (f.size() < 5) continue;
                UiEntry e;
                e.id        = _wtoi64(f[0].c_str());
                e.source    = f[1];
                e.createdAt = f[2];
                e.note      = ipc::UnescapeField(f[3]);
                e.content   = ipc::UnescapeField(f[4]);
                entries.push_back(std::move(e));
            }
        }
    }
    m_app.SetEntries(std::move(entries));
    const std::vector<UiEntry>& all = m_app.Entries();

    m_shell->SetStatus(ok ? L"记录中" : L"服务未连接", ok);

    if (!m_hList) return;

    // ---- 行模型：分组头 + 条目（类型筛选在客户端本地做） ----
    m_rows.clear();
    m_hoverItem = -1;

    SYSTEMTIME now = {}, yst = {};
    GetLocalTime(&now);
    {
        FILETIME ft = {};
        SystemTimeToFileTime(&now, &ft);
        ULARGE_INTEGER u = { ft.dwLowDateTime, ft.dwHighDateTime };
        u.QuadPart -= 86400ULL * 10000000ULL;
        ft.dwLowDateTime = u.LowPart;
        ft.dwHighDateTime = u.HighPart;
        FileTimeToSystemTime(&ft, &yst);
    }
    wchar_t today[16], yest[16];
    swprintf_s(today, L"%04d-%02d-%02d", now.wYear, now.wMonth, now.wDay);
    swprintf_s(yest,  L"%04d-%02d-%02d", yst.wYear, yst.wMonth, yst.wDay);

    auto groupOf = [&](const std::wstring& ts) -> const wchar_t* {
        if (wcsncmp(ts.c_str(), today, 10) == 0) return L"today";
        if (wcsncmp(ts.c_str(), yest, 10) == 0)  return L"yest";
        return L"older";
    };

    for (size_t i = 0; i < all.size(); ++i) {
        const UiEntry& e = all[i];
        if (m_typeFilter && TypeOfEntry(e) != m_typeFilter) continue;

        const wchar_t* want = groupOf(e.createdAt);
        bool needHead = m_rows.empty() || m_rows.back().header;
        if (!needHead) {
            const std::wstring& prev = all[m_rows.back().entry].createdAt;
            needHead = (wcscmp(groupOf(prev), want) != 0);
        }
        if (needHead) {
            if (wcscmp(want, L"today") == 0)      m_rows.push_back({ true, L"今天", 0 });
            else if (wcscmp(want, L"yest") == 0)  m_rows.push_back({ true, L"昨天", 0 });
            else                                  m_rows.push_back({ true, L"更早", 0 });
        }
        m_rows.push_back({ false, L"", i });
    }

    // ---- 填充列表（VARIABLE owner-draw：分组头 / 条目卡） ----
    SendMessageW(m_hList, LB_RESETCONTENT, 0, 0);
    int selRow = -1, firstEntryRow = -1;
    for (size_t r = 0; r < m_rows.size(); ++r) {
        int idx = (int)SendMessageW(m_hList, LB_ADDSTRING, 0, (LPARAM)L"");
        SendMessageW(m_hList, LB_SETITEMDATA, idx, (LPARAM)r);
        SendMessageW(m_hList, LB_SETITEMHEIGHT, idx,
                     m_rows[r].header ? Mx().groupH : Mx().itemH);
        if (!m_rows[r].header) {
            if (firstEntryRow < 0) firstEntryRow = idx;
            if ((int)m_rows[r].entry == m_selEntry) selRow = idx;
        }
    }

    // 选中态：优先保持原选中，否则选第一条
    int curSel = (int)SendMessageW(m_hList, LB_GETCURSEL, 0, 0);
    if (selRow >= 0 && curSel != selRow) {
        SendMessageW(m_hList, LB_SETCURSEL, selRow, 0);
    } else if (selRow < 0 && firstEntryRow >= 0) {
        SendMessageW(m_hList, LB_SETCURSEL, firstEntryRow, 0);
        selRow = firstEntryRow;
    }
    if (selRow >= 0 && selRow < (int)m_rows.size() && !m_rows[selRow].header)
        m_selEntry = (int)m_rows[selRow].entry;
    else if (firstEntryRow < 0)
        m_selEntry = -1;

    // 选中迁移（条目被删/被筛掉）时同步备注编辑态
    SwitchNoteEditTarget(m_selEntry);

    InvalidateRect(m_hList, nullptr, TRUE);
    if (m_visible && m_shell->CurrentPageId() == 1) {
        RECT rcD = m_rcDetail;
        InvalidateRect(m_shell->Hwnd(), &rcD, FALSE);
    }
}

// ============================================================
// 绘制
// ============================================================
void ClipsPage::OnPaint(HDC dc, const RECT& content)
{
    m_content = content;
    SetBkMode(dc, TRANSPARENT);

    DrawToolbar(dc);
    if (m_formOpen && !m_editingId) DrawFormCard(dc);
    DrawDetailPanel(dc);
}

void ClipsPage::DrawToolbar(HDC dc)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();

    // 搜索框视觉外框（EDIT 只占中间一行，边框必须画在父窗口）
    if (m_rcSearchBox.right > m_rcSearchBox.left) {
        gfx::FillRound(dc, m_rcSearchBox, p.surface, m.radiusMd);
        gfx::Stroke(dc, m_rcSearchBox, p.border, m.radiusMd);
    }

    // 类型筛选胶囊
    const wchar_t* chips[4] = { L"全部", L"文本", L"链接", L"手动" };
    for (int i = 0; i < 4; ++i) {
        RECT r = m_rcChips[i];
        bool on = (m_typeFilter == i);
        gfx::FillRound(dc, r, on ? p.primary : p.surface, m.radiusPill);
        gfx::Stroke(dc, r, on ? p.primary : p.border, m.radiusPill);
        gfx::Text(dc, on ? F().Get(FontRole::Button) : F().Get(FontRole::Meta),
                  on ? p.textInverse : RGB(78, 89, 105), r, chips[i],
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }

    // 主按钮：＋ 手动录入
    {
        RECT b = m_rcAddBtn;
        gfx::FillRound(dc, b, p.primary, m.radiusMd);
        gfx::Text(dc, F().Get(FontRole::Button), p.textInverse, b,
                  L"＋ 手动录入",
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }
}

void ClipsPage::DrawFormCard(HDC dc)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();

    RECT r = m_rcFormCard;
    gfx::FillRound(dc, r, p.surface, m.radiusLg);
    gfx::Stroke(dc, r, p.split, m.radiusLg);

    // 左侧主色竖条 + 标题
    RECT bar = { r.left + 16, r.top + 20, r.left + 19, r.top + 34 };
    gfx::Fill(dc, bar, p.primary);

    RECT rcFt = { r.left + 25, r.top + 16, r.right - 80, r.top + 38 };
    gfx::Text(dc, F().Get(FontRole::Section), p.text, rcFt, L"添加记录",
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    gfx::Text(dc, F().Get(FontRole::Meta), p.textFaint, m_rcFormClose,
              L"收起 ▴",
              DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);

    // 输入框边框（画在控件外框上）
    auto BorderOf = [&](HWND ctrl, COLORREF color) {
        if (!ctrl) return;
        RECT rr;
        GetWindowRect(ctrl, &rr);
        MapWindowPoints(HWND_DESKTOP, m_shell->Hwnd(), (LPPOINT)&rr, 2);
        InflateRect(&rr, 1, 1);
        gfx::Stroke(dc, rr, color, m.radiusSm);
    };
    BorderOf(m_hManualEdit, p.border);
    if (m_rcNoteBox.right > m_rcNoteBox.left) {
        RECT nr = m_rcNoteBox;
        InflateRect(&nr, 1, 1);
        gfx::Stroke(dc, nr, p.border, m.radiusSm);
    }
}

void ClipsPage::DrawDetailPanel(HDC dc)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();
    RECT d = m_rcDetail;
    if (d.right <= d.left) return;

    gfx::FillRound(dc, d, p.surface, m.radiusLg);
    gfx::Stroke(dc, d, p.split, m.radiusLg);
    SetBkMode(dc, TRANSPARENT);

    const std::vector<UiEntry>& entries = m_app.Entries();
    if (m_selEntry < 0 || m_selEntry >= (int)entries.size()) {
        RECT rcE = { d.left + 16, d.top, d.right - 16, d.bottom };
        gfx::Text(dc, F().Get(FontRole::Meta), p.textFaint, rcE,
                  L"选择左侧条目查看详情",
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        return;
    }
    const UiEntry& e = entries[m_selEntry];

    // "详情"
    RECT rcCap = { d.left + 16, d.top + 14, d.right - 16, d.top + 34 };
    gfx::Text(dc, F().Get(FontRole::Meta), RGB(143, 149, 158), rcCap, L"详情",
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    // 类型胶囊
    int t = TypeOfEntry(e);
    bool isFile = (t == 1) && e.content.size() >= 3 &&
                  e.content[1] == L':' && e.content[2] == L'\\';
    const wchar_t* typeName =
        (e.source == L"Manual") ? L"手动录入"
      : (t == 2) ? L"链接" : (isFile ? L"文件" : L"文本");
    std::wstring pill = std::wstring(typeName) +
                        (e.source == L"Manual" ? L" · 手动添加"
                                               : L" · 来自剪贴板");
    gfx::Tag(dc, d.left + 16, d.top + 44, pill.c_str(), F().Get(FontRole::Meta),
             p.primarySoft, p.primaryText, m.tagH);

    // 内容框（灰底，多行折行）
    RECT rcBox = { d.left + 16, d.top + 76, d.right - 16, d.top + 172 };
    gfx::FillRound(dc, rcBox, p.sunken, m.radiusLg);
    gfx::Stroke(dc, rcBox, p.split, m.radiusLg);
    RECT rcContent = { rcBox.left + 12, rcBox.top + 10,
                       rcBox.right - 12, rcBox.bottom - 10 };
    gfx::TextBlock(dc, F().Get(FontRole::Meta), RGB(51, 56, 63), rcContent,
                   e.content);

    // 属性
    RECT rcSec = { d.left + 16, d.top + 186, d.right - 16, d.top + 204 };
    gfx::Text(dc, F().Get(FontRole::Meta), RGB(143, 149, 158), rcSec, L"属性",
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    std::wstring src = (e.source == L"Manual") ? L"手动录入" : e.source;
    const wchar_t* keys[4] = { L"来源应用", L"记录时间", L"内容类型", L"字符数" };
    std::wstring vals[4] = {
        src, e.createdAt, typeName,
        std::to_wstring((long)e.content.size()),
    };
    int ry = d.top + 208;
    for (int i = 0; i < 4; ++i) {
        RECT rcK = { d.left + 16, ry, d.left + 84, ry + 22 };
        gfx::Text(dc, F().Get(FontRole::Meta), p.textFaint, rcK, keys[i],
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
        RECT rcV = { d.left + 88, ry, d.right - 16, ry + 22 };
        gfx::Text(dc, F().Get(FontRole::Meta), RGB(78, 89, 105), rcV, vals[i],
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER |
                  DT_END_ELLIPSIS | DT_NOPREFIX);
        ry += 22;
    }

    // 备注
    RECT rcNs = { d.left + 16, d.top + 308, d.right - 16, d.top + 326 };
    bool noteEditing = (m_editingId != 0);
    gfx::Text(dc, F().Get(FontRole::Meta), RGB(143, 149, 158), rcNs,
              noteEditing ? L"备注（编辑中，Esc 取消）" : L"备注（点击编辑）",
              DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    RECT nb = DetailNoteBox(d);
    if (noteEditing) {
        // 编辑态：常规输入框外观（白底 + 实线），不画旧备注，避免与编辑内容重叠
        gfx::FillRound(dc, nb, p.surface, m.radiusMd);
        gfx::Stroke(dc, nb, p.border, m.radiusMd);
    } else {
        gfx::FillRound(dc, nb, p.noteBg, m.radiusMd);
        gfx::StrokeDashed(dc, nb, p.noteBorder, m.radiusMd);
        RECT rcNote = { nb.left + 10, nb.top + 8, nb.right - 10, nb.bottom - 8 };
        gfx::TextBlock(dc, F().Get(FontRole::Meta), RGB(100, 106, 115),
                       rcNote, e.note);
    }

    // 底部操作
    int bw = (d.right - d.left - 32 - 16) / 3;
    int by = d.bottom - 46;
    m_rcDBtn[0] = { d.left + 16, by, d.left + 16 + bw, by + 32 };
    m_rcDBtn[1] = { m_rcDBtn[0].right + 8, by,
                    m_rcDBtn[0].right + 8 + bw, by + 32 };
    m_rcDBtn[2] = { m_rcDBtn[1].right + 8, by, d.right - 16, by + 32 };

    DrawButton(dc, m_rcDBtn[0], L"复制内容", F().Get(FontRole::Button),
               BtnStyle::Primary, false, false);
    DrawButton(dc, m_rcDBtn[1], L"备注", F().Get(FontRole::Button),
               BtnStyle::Secondary, false, false);
    DrawButton(dc, m_rcDBtn[2], L"删除", F().Get(FontRole::Button),
               BtnStyle::Danger, false, false);
}

void ClipsPage::DrawListRow(const DRAWITEMSTRUCT* dis)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();

    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    bool selected = (dis->itemState & ODS_SELECTED) != 0;
    bool hovered  = ((int)dis->itemID == m_hoverItem);

    // 行底 = 页面灰，卡片浮于其上
    gfx::Fill(dc, rc, p.bg);

    if (dis->itemID >= m_rows.size()) return;
    const Row& row = m_rows[dis->itemID];
    SetBkMode(dc, TRANSPARENT);

    // ---- 分组头 ----
    if (row.header) {
        int cy = rc.top + 15;
        int w = gfx::MeasureWidth(dc, F().Meta(), row.label);
        gfx::Text(dc, F().Get(FontRole::Meta), RGB(143, 149, 158),
                  RECT{ rc.left + 4, rc.top, rc.right, rc.bottom }, row.label,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
        gfx::HLine(dc, rc.left + 12 + w + 10, rc.right - 4, cy,
                   RGB(233, 236, 241));
        return;
    }

    // ---- 条目卡片 ----
    const std::vector<UiEntry>& entries = m_app.Entries();
    if (row.entry >= entries.size()) return;
    const UiEntry& e = entries[row.entry];

    RECT card = { rc.left + 2, rc.top + 3, rc.right - 6, rc.bottom - 9 };
    gfx::FillRound(dc, card, p.surface, m.radiusLg);
    gfx::Stroke(dc, card,
                selected ? p.primary : (hovered ? p.hoverBorder : p.split),
                m.radiusLg);
    if (selected) {
        RECT ring = { card.left - 1, card.top - 1,
                      card.right + 1, card.bottom + 1 };
        gfx::Stroke(dc, ring, p.selectRing, m.radiusLg + 1);
    }

    // 类型图标块 34px
    int t = TypeOfEntry(e);
    bool isFile = (t == 1) &&
        ((e.content.size() >= 3 && e.content[1] == L':' && e.content[2] == L'\\') ||
         (e.content.compare(0, 2, L"\\\\") == 0));
    const wchar_t* glyph;
    COLORREF icBg, icTx;
    if (e.source == L"Manual") {
        glyph = L"\xE70F"; icBg = RGB(243, 238, 255); icTx = RGB(123, 97, 255);
    } else if (t == 2) {
        glyph = L"\xE71B"; icBg = RGB(255, 243, 224); icTx = RGB(232, 137, 12);
    } else if (isFile) {
        glyph = L"\xE8B7"; icBg = RGB(230, 244, 241); icTx = RGB(15, 163, 127);
    } else {
        glyph = L"\xE8A5"; icBg = p.primarySoft; icTx = p.primary;
    }

    RECT ic = { card.left + 10, card.top + 11, card.left + 44, card.top + 45 };
    gfx::IconTile(dc, ic, glyph, F().Get(FontRole::IconLarge), icBg, icTx, 8);

    int tx = ic.right + 12;

    // 行1：内容 + 备注标签
    int line1Right = card.right - 12;
    if (hovered) line1Right -= 3 * 26 + 2 * 6 + 6;

    if (!e.note.empty()) {
        std::wstring tagTxt = L"备注: " + e.note;
        if (tagTxt.size() > 30) tagTxt = tagTxt.substr(0, 30) + L"…";
        int tagW = gfx::MeasureWidth(dc, F().Meta(), tagTxt) + 14;
        RECT rcTag = { line1Right - tagW, card.top + 8, line1Right,
                       card.top + 26 };
        gfx::FillRound(dc, rcTag, p.warnSoft, 4);
        gfx::Stroke(dc, rcTag, p.warnBorder, 4);
        gfx::Text(dc, F().Get(FontRole::Meta), p.warnText, rcTag, tagTxt,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS |
                  DT_NOPREFIX);
        line1Right = rcTag.left - 8;
    }

    RECT rcLine1 = { tx, card.top + 9, line1Right, card.top + 30 };
    gfx::TextRow(dc, F().Body(), p.text, rcLine1, gfx::OneLine(e.content));

    // 行2：来源 · 时间 · 字符数
    std::wstring src = (e.source == L"Manual") ? L"手动录入" : e.source;
    std::wstring meta = src + L" · " + fmt::RelTime(e.createdAt) +
                        L" · " + std::to_wstring((long)e.content.size()) +
                        L" 字符";
    RECT rcLine2 = { tx, card.top + 31, card.right - 12, card.top + 52 };
    gfx::TextRow(dc, F().Get(FontRole::Meta), p.textFaint, rcLine2, meta);

    // 悬停操作钮：复制 / 备注 / 删除
    if (hovered) {
        const wchar_t* ops[3] = { L"\xE8C8", L"\xE70F", L"\xE74D" };
        int bx = card.right - 12 - 3 * 26 - 2 * 6;
        for (int i = 0; i < 3; ++i) {
            RECT rb = { bx, card.top + 15, bx + 26, card.top + 41 };
            gfx::FillRound(dc, rb, p.iconTile, 6);
            gfx::Text(dc, F().Get(FontRole::Icon),
                      (i == 2) ? RGB(229, 72, 77) : RGB(90, 98, 112), rb,
                      ops[i],
                      DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
            bx += 32;
        }
    }
}

// ============================================================
// 交互
// ============================================================
bool ClipsPage::OnMouseDown(POINT pt)
{
    if (!m_visible) return false;

    // Shell 传相对内容区坐标；页面几何是客户区绝对坐标
    POINT abs = { pt.x + m_content.left, pt.y + m_content.top };

    // 搜索框 / 备注框：视觉外框高于 EDIT 客户区，留白点击也要能进入输入状态
    if (m_rcSearchBox.right > m_rcSearchBox.left &&
        PtInRect(&m_rcSearchBox, abs)) {
        ForwardClickToEdit(m_hSearch, F().Body(), abs);
        return true;
    }
    if (m_rcNoteBox.right > m_rcNoteBox.left && PtInRect(&m_rcNoteBox, abs)) {
        ForwardClickToEdit(m_hNoteEdit, F().Body(), abs);
        return true;
    }

    // 类型胶囊
    for (int i = 0; i < 4; ++i) {
        if (PtInRect(&m_rcChips[i], abs)) {
            m_typeFilter = i;
            RefreshList(GetSearchText());
            m_shell->InvalidateContent();
            return true;
        }
    }

    if (PtInRect(&m_rcAddBtn, abs)) { ToggleForm(true); return true; }

    if (m_formOpen && PtInRect(&m_rcFormClose, abs)) {
        ToggleForm(false);
        return true;
    }

    int db = DetailButtonAt(abs);
    if (db) {
        if (m_selEntry >= 0) OnListOp(m_selEntry, db - 1);
        return true;
    }

    // 备注框：点击就地编辑（编辑态下多行框已覆盖该区，由 EDIT 自身接收点击）
    if (m_selEntry >= 0 && !m_editingId) {
        RECT nb = DetailNoteBox(m_rcDetail);
        if (PtInRect(&nb, abs)) {
            OnListOp(m_selEntry, 1);
            return true;
        }
    }
    return false;
}

bool ClipsPage::OnMouseMove(POINT pt)
{
    (void)pt;
    return false;
}

bool ClipsPage::OnMouseLeave()
{
    return false;
}

void ClipsPage::OnSetFocus()
{
    if (m_visible && m_hSearch) SetFocus(m_hSearch);
}

bool ClipsPage::OnCommand(int ctrlId, int code)
{
    // 先过插件（F3 搜索防抖等由插件处理）
    m_app.Plugins().DispatchCommand(m_app, ctrlId, code);

    if (ctrlId == IDC_LIST && code == LBN_SELCHANGE) {
        int sel = (int)SendMessageW(m_hList, LB_GETCURSEL, 0, 0);
        if (sel != LB_ERR && sel < (int)m_rows.size() && !m_rows[sel].header) {
            m_selEntry = (int)m_rows[sel].entry;
            // 备注编辑态跟随选中（否则编辑框停留旧记录、保存写错 id）
            SwitchNoteEditTarget(m_selEntry);
            RECT rcD = m_rcDetail;
            InvalidateRect(m_shell->Hwnd(), &rcD, FALSE);
        }
        return true;
    }

    if (ctrlId == IDC_LIST && code == LBN_DBLCLK) {
        int sel = (int)SendMessageW(m_hList, LB_GETCURSEL, 0, 0);
        if (sel != LB_ERR && sel < (int)m_rows.size() && !m_rows[sel].header) {
            const std::vector<UiEntry>& entries = m_app.Entries();
            if (m_rows[sel].entry < entries.size())
                m_app.Ipc().Request(
                    L"COPY\t" + ipc::EscapeField(entries[m_rows[sel].entry].content));
        }
        return true;
    }
    return false;
}

bool ClipsPage::OnTimer(UINT_PTR id)
{
    if (id != TIMER_REFRESH_NOTIFY) return false;
    KillTimer(m_shell->Hwnd(), TIMER_REFRESH_NOTIFY);
    RefreshList(GetSearchText());
    m_app.RefreshStats();
    m_app.SyncServiceUi();
    return true;
}

bool ClipsPage::OnDrawItem(const DRAWITEMSTRUCT* dis)
{
    if (dis->CtlID == IDC_LIST) {
        DrawListRow(dis);
        return true;
    }
    if (dis->CtlID == IDC_SAVE_BUTTON) {
        wchar_t label[32] = {};
        GetWindowTextW(dis->hwndItem, label, 32);
        DrawButton(dis->hDC, dis->rcItem, label,
                   F().Get(FontRole::Button),
                   ButtonStyleOf(dis->hwndItem),
                   IsButtonHover(dis->hwndItem),
                   (dis->itemState & ODS_SELECTED) != 0);
        return true;
    }
    return false;
}

bool ClipsPage::OnMeasureItem(MEASUREITEMSTRUCT* mis)
{
    if (mis->CtlID != IDC_LIST) return false;
    mis->itemHeight = 54;
    return true;
}

// ============================================================
// 备注编辑流
// ============================================================
void ClipsPage::ToggleForm(bool open)
{
    if (m_formOpen == open) return;
    if (open && m_editingId) return;   // 备注编辑态优先

    m_formOpen = open;
    Relayout();

    SendMessageW(m_shell->Hwnd(), WM_SETREDRAW, FALSE, 0);
    SyncVisibility();
    SendMessageW(m_shell->Hwnd(), WM_SETREDRAW, TRUE, 0);
    m_shell->InvalidateContent();
}

void ClipsPage::OpenForm()
{
    RefreshList(GetSearchText());
    ToggleForm(true);
    if (m_hManualEdit) SetFocus(m_hManualEdit);
}

void ClipsPage::StartNoteEdit(long long id, const std::wstring& note)
{
    if (m_formOpen) ToggleForm(false);   // 表单卡让位

    m_editingId = id;
    Relayout();
    SetWindowTextW(m_hNoteMulti, note.c_str());
    MoveCaretToEnd(m_hNoteMulti);
    SetWindowTextW(m_hSaveBtn, L"保存备注");
    SyncVisibility();
    SetFocus(m_hNoteMulti);
    m_shell->InvalidateContent();
}

void ClipsPage::CancelNoteEdit()
{
    if (!m_editingId) return;
    m_editingId = 0;
    SetWindowTextW(m_hSaveBtn, L"保存记录");
    ClearEditText(m_hNoteMulti);
    Relayout();
    SyncVisibility();
    m_shell->InvalidateContent();
}

void ClipsPage::SaveNoteEdit()
{
    if (!m_editingId) return;
    std::wstring note = GetEditText(m_hNoteMulti);
    m_app.Ipc().Request(L"NOTE\t" + std::to_wstring(m_editingId) +
                        L"\t" + ipc::EscapeField(note));
    m_editingId = 0;
    SetWindowTextW(m_hSaveBtn, L"保存记录");
    ClearEditText(m_hNoteMulti);
    Relayout();
    SyncVisibility();
    RefreshList(GetSearchText());
    m_app.RefreshStats();
    m_app.SyncServiceUi();
}

void ClipsPage::SwitchNoteEditTarget(int newEntryIdx)
{
    if (!m_editingId) return;

    const std::vector<UiEntry>& entries = m_app.Entries();
    if (newEntryIdx < 0 || newEntryIdx >= (int)entries.size()) {
        CancelNoteEdit();
        return;
    }
    const UiEntry& ne = entries[newEntryIdx];
    if (ne.id == m_editingId) return;

    // 先落库原条目（按 id 查找），并同步本地缓存
    std::wstring cur = GetEditText(m_hNoteMulti);
    for (UiEntry& e : m_app.EntriesMut()) {
        if (e.id == m_editingId) {
            if (cur != e.note) {
                m_app.Ipc().Request(L"NOTE\t" + std::to_wstring(e.id) +
                                    L"\t" + ipc::EscapeField(cur));
                e.note = cur;
            }
            break;
        }
    }
    StartNoteEdit(ne.id, ne.note);
}

// ============================================================
// 条目操作
// ============================================================
int ClipsPage::DetailButtonAt(POINT absPt) const
{
    for (int i = 0; i < 3; ++i)
        if (PtInRect(&m_rcDBtn[i], absPt)) return i + 1;
    return 0;
}

void ClipsPage::OnListOp(int entryIdx, int op)
{
    const std::vector<UiEntry>& entries = m_app.Entries();
    if (entryIdx < 0 || entryIdx >= (int)entries.size()) return;
    const UiEntry& e = entries[entryIdx];

    switch (op) {
    case 0:
        m_app.Ipc().Request(L"COPY\t" + ipc::EscapeField(e.content));
        break;
    case 1:
        StartNoteEdit(e.id, e.note);
        break;
    case 2:
        if (MessageBoxW(m_shell->Hwnd(),
                L"确定删除这条记录吗？删除后不可恢复。",
                L"删除记录", MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
            // 删除的正是正在编辑的条目 → 先退出编辑态
            if (m_editingId == e.id) CancelNoteEdit();
            m_app.Ipc().Request(L"DEL\t" + std::to_wstring(e.id));
            m_selEntry = -1;
            RefreshList(GetSearchText());
            m_app.RefreshStats();
            m_app.SyncServiceUi();
        }
        break;
    }
}

void ClipsPage::ShowItemMenu(POINT screenPt, int entryIdx)
{
    const std::vector<UiEntry>& entries = m_app.Entries();
    if (entryIdx < 0 || entryIdx >= (int)entries.size()) return;

    enum { CMD_COPY = 1, CMD_EDIT = 2, CMD_DEL = 3 };
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, CMD_COPY, L"复制内容");
    AppendMenuW(menu, MF_STRING, CMD_EDIT,
                entries[entryIdx].note.empty() ? L"添加备注" : L"编辑备注");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CMD_DEL, L"删除记录");

    SetForegroundWindow(m_shell->Hwnd());
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                             screenPt.x, screenPt.y, 0, m_shell->Hwnd(), nullptr);
    PostMessageW(m_shell->Hwnd(), WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (cmd) {
    case CMD_COPY: OnListOp(entryIdx, 0); break;
    case CMD_EDIT: OnListOp(entryIdx, 1); break;
    case CMD_DEL:  OnListOp(entryIdx, 2); break;
    }
}

// ============================================================
// 子类化：列表（hover / 悬停钮 / 空状态 / 右键菜单）
// ============================================================
LRESULT CALLBACK ClipsPage::ListProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam, UINT_PTR idSub,
                                     DWORD_PTR ref)
{
    auto* self = (ClipsPage*)ref;

    switch (msg) {
    case WM_LBUTTONDOWN: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int item = list::ItemFromPoint(hwnd, pt);
        if (item >= 0 && item < (int)self->m_rows.size()) {
            if (self->m_rows[item].header) return 0;   // 分组头不可选

            RECT rc, card;
            SendMessageW(hwnd, LB_GETITEMRECT, item, (LPARAM)&rc);
            card = { rc.left + 2, rc.top + 3, rc.right - 6, rc.bottom - 9 };
            int bx = card.right - 12 - 3 * 26 - 2 * 6;
            for (int op = 0; op < 3; ++op) {
                RECT rb = { bx, card.top + 15, bx + 26, card.top + 41 };
                if (PtInRect(&rb, pt)) {
                    self->OnListOp((int)self->m_rows[item].entry, op);
                    return 0;
                }
                bx += 32;
            }
        }
        break;   // 其余交给列表默认选中逻辑
    }
    case WM_MOUSEMOVE: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int item = list::ItemFromPoint(hwnd, pt);
        if (item != self->m_hoverItem) {
            int old = self->m_hoverItem;
            self->m_hoverItem = item;
            if (old >= 0) {
                RECT rc;
                SendMessageW(hwnd, LB_GETITEMRECT, old, (LPARAM)&rc);
                InvalidateRect(hwnd, &rc, TRUE);
            }
            if (item >= 0) {
                RECT rc;
                SendMessageW(hwnd, LB_GETITEMRECT, item, (LPARAM)&rc);
                InvalidateRect(hwnd, &rc, TRUE);
            }
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        break;
    }
    case WM_MOUSELEAVE: {
        int old = self->m_hoverItem;
        self->m_hoverItem = -1;
        if (old >= 0) {
            RECT rc;
            SendMessageW(hwnd, LB_GETITEMRECT, old, (LPARAM)&rc);
            InvalidateRect(hwnd, &rc, TRUE);
        }
        break;
    }
    case WM_PAINT: {
        // 空状态：三步引导
        if (SendMessageW(hwnd, LB_GETCOUNT, 0, 0) == 0) {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            gfx::Fill(dc, rc, Pal().bg);

            EmptyState st;
            st.title = L"暂无记录";
            st.steps = L"①  复制任意文字 —— 自动存入这里\n"
                       L"②  双击任意条目 —— 复制回剪贴板\n"
                       L"③  点击右上角「＋ 手动录入」—— 添加并写备注";
            st.foot  = L"关闭窗口后仍在后台记录 · Ctrl+Shift+V 随时呼出";
            DrawEmptyState(dc, rc, self->F(), st);
            EndPaint(hwnd, &ps);
            return 0;
        }
        break;
    }
    case WM_CONTEXTMENU: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int item = -1;
        if (pt.x != -1 || pt.y != -1) {
            POINT lpt = pt;
            ScreenToClient(hwnd, &lpt);
            item = list::ItemFromPoint(hwnd, lpt);
        } else {
            item = (int)SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
            if (item == LB_ERR) return 0;
            RECT rc;
            SendMessageW(hwnd, LB_GETITEMRECT, item, (LPARAM)&rc);
            MapWindowPoints(hwnd, HWND_DESKTOP, (LPPOINT)&rc, 2);
            pt = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
        }

        int entryIdx = -1;
        if (item >= 0 && item < (int)self->m_rows.size() &&
            !self->m_rows[item].header) {
            entryIdx = (int)self->m_rows[item].entry;
            SendMessageW(hwnd, LB_SETCURSEL, item, 0);
            RECT rcD = self->m_rcDetail;
            InvalidateRect(self->m_shell->Hwnd(), &rcD, FALSE);
        }
        self->ShowItemMenu(pt, entryIdx);
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &ClipsPage::ListProc, idSub);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ============================================================
// 子类化：搜索框右侧快捷键提示（独立置顶子窗口）
// ============================================================
LRESULT CALLBACK ClipsPage::ChipProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam, UINT_PTR idSub,
                                     DWORD_PTR ref)
{
    if (msg == WM_NCHITTEST) {
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
        return (r == HTCLIENT) ? HTTRANSPARENT : r;   // 鼠标穿透到 EDIT
    }
    if (msg == WM_ERASEBKGND) return 1;               // 自绘全覆盖，免闪烁

    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        auto* self = (ClipsPage*)ref;
        RECT r = { rc.left + 1, rc.top, rc.right - 1, rc.bottom };
        gfx::FillRound(dc, r, Pal().chipBg, 4);
        gfx::Stroke(dc, r, Pal().chipBorder, 4);

        const wchar_t* key = L"Ctrl+Shift+V";
        RECT rk = { r.left + 6, r.top, r.right - 4, r.bottom };
        gfx::Text(dc, self->F().Meta(), RGB(110, 116, 125), rk, key,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}
