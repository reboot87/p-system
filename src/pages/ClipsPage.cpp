#include "ClipsPage.h"
#include "core/App.h"
#include "core/Format.h"
#include "../ipc/Ipc.h"

using namespace air::ui;

namespace {

// 列表行内三个操作钮的几何，以「行的左上角」为原点。
//
// 绘制时整体平移到行矩形，命中时直接用行内坐标比较 —— 两边必须是同一
// 个函数：LISTBOX 给页面的点击坐标是行内坐标，而 ItemRect() 是客户区
// 坐标，直接拿客户区的矩形去比行内坐标，除了列表最顶上那一行（top=0），
// 其余每一行的图标都是错位的（点不到，也悬停不上）。
void OpRects(int itemW, RECT out[3])
{
    // 与卡片一致的左右内缩：左 2 / 右 6，顶 3，底 9
    const int right = itemW - 6;
    int bx = right - 12 - 3 * 26 - 2 * 6;
    for (int i = 0; i < 3; ++i) {
        out[i] = RECT{ bx, 3 + 15, bx + 26, 3 + 41 };
        bx += 32;
    }
}

} // namespace

// ============================================================
// 创建与显隐
// ============================================================
void ClipsPage::CreateControls()
{
    // 单行 EDIT 永远把文字画在客户区顶部 → 客户区压到一行文字高才真正居中
    m_lineH = m_shell->TextLineHeight(FontRole::Body);

    // ---- 搜索框 ----
    m_search = &CreateTextBox({ IDC_SEARCH_EDIT, false,
                                L"搜索内容和备注…", FontRole::Body });

    // ---- 搜索框右侧 Ctrl+Shift+V 提示（自绘小块，鼠标穿透到搜索框） ----
    m_searchChip = &CreateCustom({ IDC_SEARCH_CHIP, true });
    m_searchChip->OnPaint([](Canvas& g, const RECT& rc) {
        RECT r = { rc.left + 1, rc.top, rc.right - 1, rc.bottom };
        g.FillRound(r, Pal().chipBg, 4);
        g.Stroke(r, Pal().chipBorder, 4);
        RECT rk = { r.left + 6, r.top, r.right - 4, r.bottom };
        g.Text(rk, L"Ctrl+Shift+V", FontRole::Meta, RGB(110, 116, 125),
               DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    });

    // ---- 记录列表 ----
    m_list = &CreateListBox({ IDC_LIST, 54 });
    m_list->onPaintItem = [this](Canvas& g, const ListItemDraw& it) {
        DrawListRow(g, it);
    };
    m_list->onPaintEmpty = [](Canvas& g, const RECT& rc) {
        EmptyState st;
        st.title = L"暂无记录";
        st.steps = L"①  复制任意文字 —— 自动存入这里\n"
                   L"②  双击任意条目 —— 复制回剪贴板\n"
                   L"③  点击右上角「＋ 手动录入」—— 添加并写备注";
        st.foot  = L"关闭窗口后仍在后台记录 · Ctrl+Shift+V 随时呼出";
        g.EmptyState(rc, st);
    };
    m_list->onSelectChanged = [this](int sel) {
        if (sel < 0 || sel >= (int)m_rows.size() || m_rows[sel].header) return;
        m_selEntry = (int)m_rows[sel].entry;
        // 备注编辑态跟随选中（否则编辑框停留旧记录、保存写错 id）
        SwitchNoteEditTarget(m_selEntry);
        InvalidateRegion(m_rcDetail);
    };
    m_list->onDoubleClicked = [this](int sel) {
        if (sel < 0 || sel >= (int)m_rows.size() || m_rows[sel].header) return;
        const std::vector<UiEntry>& entries = m_app.Entries();
        if (m_rows[sel].entry >= entries.size()) return;
        const std::wstring resp = m_app.Ipc().Request(
            L"COPY\t" + ipc::EscapeField(entries[m_rows[sel].entry].content));
        if (resp.rfind(L"OK", 0) == 0)
            m_app.Toast(L"已复制", L"内容已写入系统剪贴板");
        else
            m_app.Toast(L"复制失败", L"服务未响应，请稍后重试", false);
    };
    m_list->onItemClicked = [this](int item, POINT ptInItem) -> bool {
        if (item < 0 || item >= (int)m_rows.size()) return false;
        if (m_rows[item].header) return true;   // 分组头不可选

        RECT rc = m_list->ItemRect(item);
        RECT rb[3];
        OpRects(rc.right - rc.left, rb);
        for (int op = 0; op < 3; ++op) {
            if (PtInRect(&rb[op], ptInItem)) {
                OnListOp((int)m_rows[item].entry, op);
                return true;
            }
        }
        return false;
    };
    m_list->onContextMenu = [this](POINT screenPt, int item) {
        int entryIdx = -1;
        if (item >= 0 && item < (int)m_rows.size() && !m_rows[item].header) {
            entryIdx = (int)m_rows[item].entry;
            m_list->SetSelection(item);
            InvalidateRegion(m_rcDetail);
        }
        ShowItemMenu(screenPt, entryIdx);
    };
    // 行内三个操作钮只在悬停时出现；指针在同一行里从一个钮移到另一个
    // 钮，LISTBOX 不会重画（它只认"换行"），所以逐个钮的高亮必须靠这条
    // 回调自己失效。
    m_list->onHoverMove = [this](int row, POINT ptInItem) {
        SyncHoverOp(row, ptInItem);
    };

    // ---- 备注（表单卡，单行） ----
    m_noteEdit = &CreateTextBox({ IDC_NOTE_EDIT, false,
                                  L"备注（可选）", FontRole::Body });

    // ---- 备注（详情面板，多行） ----
    // 多行：备注显示区本就折行，单行框无法编辑长备注；
    // 字号与显示态一致（12px），进入编辑时换行位置不跳动。
    m_noteMulti = &CreateTextBox({ IDC_NOTE_MULTI, true,
                                   L"输入备注…", FontRole::Meta });
    m_noteMulti->OnKey([this](UINT vk, bool ctrl, bool) -> bool {
        if (vk == VK_ESCAPE && m_editingId) { CancelNoteEdit(); return true; }
        if (vk == VK_RETURN && ctrl && m_editingId) {
            SaveNoteEdit(); return true;
        }
        return false;
    });

    // ---- 手动录入（多行） ----
    m_manualEdit = &CreateTextBox({ IDC_MANUAL_EDIT, true,
                                    L"输入要保存的内容…", FontRole::Body });
    m_manualEdit->OnKey([this](UINT vk, bool ctrl, bool) -> bool {
        // Ctrl+Enter 由 ManualEntryPlugin 的子类化处理（它后装、先收到消息）
        if (vk == VK_ESCAPE && m_formOpen) { ToggleForm(false); return true; }
        (void)ctrl;
        return false;
    });

    // ---- 主按钮 ----
    m_saveBtn = &CreateButton({ IDC_SAVE_BUTTON, L"保存记录",
                                BtnStyle::Primary });
}

void ClipsPage::OnAttach(Shell& shell)
{
    (void)shell;
    CreateControls();
    SyncVisibility();
}

void ClipsPage::OnDetach()
{
    // 子控件（Widget）随父窗口销毁，无需手工清理
}

void ClipsPage::OnShow(bool shown)
{
    m_visible = shown;

    if (!shown) {
        SyncVisibility();
        // 不要 KillTimer(TIMER_REFRESH_NOTIFY)：那条防抖定时器属于应用层
        // 的共享数据刷新，页面切走时取消它会把挂起的刷新整个丢掉。
        return;
    }

    // 挂起重绘 → 布局 + 显隐一次完成 → 恢复（无双帧闪烁）
    BatchDraw(false);
    Relayout();
    SyncVisibility();
    BatchDraw(true);

    m_app.RefreshData();
}

void ClipsPage::SyncVisibility()
{
    if (!m_search) return;

    bool show = m_visible;
    m_search->Show(show);
    m_searchChip->Show(show);
    m_list->Show(show);

    bool form = show && m_formOpen && !m_editingId;
    bool edit = show && (m_editingId != 0);

    m_manualEdit->Show(form);
    m_noteEdit->Show(form);
    m_noteMulti->Show(edit);
    m_saveBtn->Show(form || edit);
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
    if (!m_search || m_content.right <= m_content.left) return;

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

    m_search->Move(m_rcSearchBox.left + 1,
                   m_rcSearchBox.top + (kSearchBoxH - m_lineH) / 2,
                   searchW - 2, m_lineH);

    // 快捷键提示 chip：右对齐，与搜索框同高居中
    {
        int chipW = m_shell->MeasureTextWidth(L"Ctrl+Shift+V",
                                              FontRole::Meta) + 12;
        int chipH = 18;
        int cx = m_rcSearchBox.right - chipW - 6;
        int cy = m_rcSearchBox.top + (kSearchBoxH - chipH) / 2;
        m_searchChip->Move(cx, cy, chipW, chipH);
        // 右内边距让出 chip 区域，文字不压到标签下
        m_search->SetRightInset(chipW + 10);
    }

    m_rcAddBtn = { content.right - 112, content.top + 1,
                   content.right, content.top + 35 };

    // ---- 类型筛选胶囊 ----
    {
        const wchar_t* labels[4] = { L"全部", L"文本", L"链接", L"手动" };
        int cx = content.left + searchW + 14;
        int cy = content.top + 4;
        for (int i = 0; i < 4; ++i) {
            int w = m_shell->MeasureTextWidth(labels[i], FontRole::Meta);
            m_rcChips[i] = { cx, cy, cx + w + 24, cy + 28 };
            cx = m_rcChips[i].right + 8;
        }
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
    m_list->Move(rcList.left, rcList.top,
                 rcList.right - rcList.left, rcList.bottom - rcList.top);

    // ---- 表单卡内部控件 ----
    if (m_formOpen && !m_editingId) {
        int fx = m_rcFormCard.left + 16;
        int fw = (m_rcFormCard.right - m_rcFormCard.left) - 32;
        int contentY = m_rcFormCard.top + 16 + 22 + 8;
        m_rcManualBox = { fx, contentY, fx + fw, contentY + 88 };
        m_manualEdit->Move(fx, contentY, fw, 88);

        int rowY = contentY + 88 + 12;
        int btnW = 96;
        m_rcNoteBox = { fx, rowY, fx + fw - btnW - 12, rowY + m.inputH };
        m_noteEdit->Move(m_rcNoteBox.left + 1,
                         m_rcNoteBox.top + (m.inputH - m_lineH) / 2,
                         m_rcNoteBox.right - m_rcNoteBox.left - 2,
                         m_lineH);
        m_saveBtn->Move(fx + fw - btnW, rowY, btnW, m.inputH);
        m_rcFormClose = { m_rcFormCard.right - 62, m_rcFormCard.top + 14,
                          m_rcFormCard.right - 16, m_rcFormCard.top + 36 };
        m_rcNoteMulti = {};
    }
    // ---- 备注编辑态：多行框覆盖详情面板备注区 ----
    else if (m_editingId && m_rcDetail.right > m_rcDetail.left) {
        RECT nb = DetailNoteBox(m_rcDetail);
        m_rcNoteMulti = nb;
        // 内缩 4px 给父窗口画的边框留空间
        m_noteMulti->Move(nb.left + 4, nb.top + 4,
                          nb.right - nb.left - 8, nb.bottom - nb.top - 8);
        m_saveBtn->Move(m_rcDetail.left + 16, nb.bottom + 10, 96, 30);
        m_rcFormClose = {};
        m_rcNoteBox = {};
        m_rcManualBox = {};
    } else {
        m_rcNoteBox = {};
        m_rcNoteMulti = {};
        m_rcManualBox = {};
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
    return m_search ? m_search->Text() : std::wstring();
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

// 只更新应用层共享快照，不碰任何控件。总览页的「最近动态」读的就是
// 这份数据，因此停在工作台总览 / 机器配置页时也必须能刷新它。
bool ClipsPage::ReloadEntries(const std::wstring& filter)
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
    return ok;
}

void ClipsPage::RefreshList(const std::wstring& filter)
{
    const bool ok = ReloadEntries(filter);
    const std::vector<UiEntry>& all = m_app.Entries();

    m_shell->SetStatus(ok ? L"记录中" : L"服务未连接", ok);

    if (!m_list) return;

    // ---- 行模型：分组头 + 条目（类型筛选在客户端本地做） ----
    m_rows.clear();
    m_hoverOp    = -1;    // 行下标即将全部作废
    m_hoverOpRow = -1;

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

    // ---- 填充列表（行高：分组头 / 条目卡） ----
    // 整个重建夹在 Begin/EndUpdate 之间：LB_RESETCONTENT 与每次
    // LB_ADDSTRING 都会各自触发一次同步刷新，不夹住的话屏幕上会先出现
    // 「整块擦成底色」再逐行画回来，Skia 一帧 30~50ms 时这段中间态看得
    // 清清楚楚（点筛选胶囊就是最典型的触发点）。
    m_list->BeginUpdate();
    m_list->Reset();
    int selRow = -1, firstEntryRow = -1;
    for (size_t r = 0; r < m_rows.size(); ++r) {
        int idx = m_list->AddItem(m_rows[r].header ? Mx().groupH : Mx().itemH, r);
        if (!m_rows[r].header) {
            if (firstEntryRow < 0) firstEntryRow = idx;
            if ((int)m_rows[r].entry == m_selEntry) selRow = idx;
        }
    }

    // 选中态：优先保持原选中，否则选第一条
    int curSel = m_list->Selection();
    if (selRow >= 0 && curSel != selRow) {
        m_list->SetSelection(selRow);
    } else if (selRow < 0 && firstEntryRow >= 0) {
        m_list->SetSelection(firstEntryRow);
        selRow = firstEntryRow;
    }
    m_list->EndUpdate();
    if (selRow >= 0 && selRow < (int)m_rows.size() && !m_rows[selRow].header)
        m_selEntry = (int)m_rows[selRow].entry;
    else if (firstEntryRow < 0)
        m_selEntry = -1;

    // 选中迁移（条目被删/被筛掉）时同步备注编辑态
    SwitchNoteEditTarget(m_selEntry);

    m_list->Invalidate();
    if (m_visible && m_shell->CurrentPageId() == 1)
        InvalidateRegion(m_rcDetail);
}

// ============================================================
// 绘制
// ============================================================
void ClipsPage::OnPaint(Canvas& g, const RECT& content)
{
    m_content = content;

    DrawToolbar(g);
    if (m_formOpen && !m_editingId) DrawFormCard(g);
    DrawDetailPanel(g);
}

void ClipsPage::DrawToolbar(Canvas& g)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();

    // 搜索框视觉外框（EDIT 只占中间一行，边框必须画在父窗口）
    if (m_rcSearchBox.right > m_rcSearchBox.left) {
        g.FillRound(m_rcSearchBox, p.surface, m.radiusMd);
        g.Stroke(m_rcSearchBox, p.border, m.radiusMd);
    }

    // 类型筛选胶囊
    const wchar_t* chips[4] = { L"全部", L"文本", L"链接", L"手动" };
    for (int i = 0; i < 4; ++i) {
        RECT r = m_rcChips[i];
        const bool on    = (m_typeFilter == i);
        const bool hov   = m_hot.Hovered(kHotChip + i);
        const bool press = m_hot.Pressed(kHotChip + i);

        COLORREF fill, border, txt;
        if (on) {
            // 选中态：实心主色，悬停提亮、按下压深（与主按钮同一套）
            fill   = press ? p.primaryDown : (hov ? p.primaryHover : p.primary);
            border = fill;
            txt    = p.textInverse;
        } else {
            // 未选中：白底 + 描边；悬停给一层主色浅底 + 主色描边，
            // 这是"可点"的信号，同时不会与选中态混淆（选中是实心的）
            fill   = press ? p.active : (hov ? p.primarySoft : p.surface);
            border = press ? p.primary : (hov ? p.hoverBorder : p.border);
            txt    = (hov || press) ? p.primaryText : RGB(78, 89, 105);
        }
        g.FillRound(r, fill, m.radiusPill);
        g.Stroke(r, border, m.radiusPill);
        // 字号必须与 Relayout 里量宽度用的角色一致（Meta）：换成 Button
        // 会让 14px 的字挤进按 12px 量的盒子，选中/悬停时文字还会跳一下。
        g.Text(r, chips[i], FontRole::Meta, txt,
               DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }

    // 主按钮：＋ 手动录入
    {
        RECT b = m_rcAddBtn;
        const bool hov   = m_hot.Hovered(kHotAddBtn);
        const bool press = m_hot.Pressed(kHotAddBtn);
        g.FillRound(b, press ? p.primaryDown
                             : (hov ? p.primaryHover : p.primary),
                    m.radiusMd);
        g.Text(b, L"＋ 手动录入", FontRole::Button, p.textInverse,
               DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }
}

void ClipsPage::DrawFormCard(Canvas& g)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();

    RECT r = m_rcFormCard;
    g.FillRound(r, p.surface, m.radiusLg);
    g.Stroke(r, p.split, m.radiusLg);

    // 左侧主色竖条 + 标题
    RECT bar = { r.left + 16, r.top + 20, r.left + 19, r.top + 34 };
    g.Fill(bar, p.primary);

    RECT rcFt = { r.left + 25, r.top + 16, r.right - 80, r.top + 38 };
    g.Text(rcFt, L"添加记录", FontRole::Section, p.text,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    // 收起：文字按钮没有边框，悬停靠一层浅底 + 文字提亮来表达"可点"
    {
        const bool hov   = m_hot.Hovered(kHotFormClose);
        const bool press = m_hot.Pressed(kHotFormClose);
        if (hov || press) {
            RECT hb = m_rcFormClose;
            InflateRect(&hb, 6, 1);
            g.FillRound(hb, press ? p.chipBg : p.active, m.radiusSm);
        }
        g.Text(m_rcFormClose, L"收起 ▴", FontRole::Meta,
               hov ? p.textSub : p.textFaint,
               DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }

    // 输入框边框（画在控件外框上）
    if (m_rcManualBox.right > m_rcManualBox.left) {
        RECT rr = m_rcManualBox;
        InflateRect(&rr, 1, 1);
        g.Stroke(rr, p.border, m.radiusSm);
    }
    if (m_rcNoteBox.right > m_rcNoteBox.left) {
        RECT nr = m_rcNoteBox;
        InflateRect(&nr, 1, 1);
        g.Stroke(nr, p.border, m.radiusSm);
    }
}

void ClipsPage::DrawDetailPanel(Canvas& g)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();
    RECT d = m_rcDetail;
    if (d.right <= d.left) return;

    g.FillRound(d, p.surface, m.radiusLg);
    g.Stroke(d, p.split, m.radiusLg);

    const std::vector<UiEntry>& entries = m_app.Entries();
    if (m_selEntry < 0 || m_selEntry >= (int)entries.size()) {
        RECT rcE = { d.left + 16, d.top, d.right - 16, d.bottom };
        g.Text(rcE, L"选择左侧条目查看详情", FontRole::Meta, p.textFaint,
               DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        return;
    }
    const UiEntry& e = entries[m_selEntry];

    // "详情"
    RECT rcCap = { d.left + 16, d.top + 14, d.right - 16, d.top + 34 };
    g.Text(rcCap, L"详情", FontRole::Meta, RGB(143, 149, 158),
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
    g.Tag(d.left + 16, d.top + 44, pill, FontRole::Meta,
          p.primarySoft, p.primaryText, m.tagH);

    // 内容框（灰底，多行折行）
    RECT rcBox = { d.left + 16, d.top + 76, d.right - 16, d.top + 172 };
    g.FillRound(rcBox, p.sunken, m.radiusLg);
    g.Stroke(rcBox, p.split, m.radiusLg);
    RECT rcContent = { rcBox.left + 12, rcBox.top + 10,
                       rcBox.right - 12, rcBox.bottom - 10 };
    g.TextBlock(rcContent, e.content, FontRole::Meta, RGB(51, 56, 63));

    // 属性
    RECT rcSec = { d.left + 16, d.top + 186, d.right - 16, d.top + 204 };
    g.Text(rcSec, L"属性", FontRole::Meta, RGB(143, 149, 158),
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
        g.Text(rcK, keys[i], FontRole::Meta, p.textFaint,
               DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
        RECT rcV = { d.left + 88, ry, d.right - 16, ry + 22 };
        g.Text(rcV, vals[i], FontRole::Meta, RGB(78, 89, 105),
               DT_SINGLELINE | DT_LEFT | DT_VCENTER |
               DT_END_ELLIPSIS | DT_NOPREFIX);
        ry += 22;
    }

    // 备注
    RECT rcNs = { d.left + 16, d.top + 308, d.right - 16, d.top + 326 };
    bool noteEditing = (m_editingId != 0);
    g.Text(rcNs,
           noteEditing ? L"备注（编辑中，Esc 取消）" : L"备注（点击编辑）",
           FontRole::Meta, RGB(143, 149, 158),
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    RECT nb = DetailNoteBox(d);
    if (noteEditing) {
        // 编辑态：常规输入框外观（白底 + 实线），不画旧备注，避免与编辑内容重叠
        g.FillRound(nb, p.surface, m.radiusMd);
        g.Stroke(nb, p.border, m.radiusMd);
    } else {
        // 虚线框 = "点我就能写"，悬停时描边提亮，让它看起来确实可以点
        const bool hov = m_hot.Hovered(kHotNoteBox);
        g.FillRound(nb, hov ? p.surface : p.noteBg, m.radiusMd);
        g.StrokeDashed(nb, hov ? p.hoverBorder : p.noteBorder, m.radiusMd);
        RECT rcNote = { nb.left + 10, nb.top + 8, nb.right - 10, nb.bottom - 8 };
        g.TextBlock(rcNote, e.note, FontRole::Meta, RGB(100, 106, 115));
    }

    // 底部操作
    int bw = (d.right - d.left - 32 - 16) / 3;
    int by = d.bottom - 46;
    m_rcDBtn[0] = { d.left + 16, by, d.left + 16 + bw, by + 32 };
    m_rcDBtn[1] = { m_rcDBtn[0].right + 8, by,
                    m_rcDBtn[0].right + 8 + bw, by + 32 };
    m_rcDBtn[2] = { m_rcDBtn[1].right + 8, by, d.right - 16, by + 32 };

    g.Button(m_rcDBtn[0], L"复制内容", BtnStyle::Primary,
             m_hot.Hovered(kHotDBtn + 0), m_hot.Pressed(kHotDBtn + 0));
    g.Button(m_rcDBtn[1], L"备注", BtnStyle::Secondary,
             m_hot.Hovered(kHotDBtn + 1), m_hot.Pressed(kHotDBtn + 1));
    g.Button(m_rcDBtn[2], L"删除", BtnStyle::Danger,
             m_hot.Hovered(kHotDBtn + 2), m_hot.Pressed(kHotDBtn + 2));
}

void ClipsPage::DrawListRow(Canvas& g, const ListItemDraw& item)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();

    RECT rc = item.rc;
    bool selected = item.selected;
    bool hovered  = item.hovered;

    if (item.index < 0 || item.index >= (int)m_rows.size()) return;
    const Row& row = m_rows[item.index];

    // ---- 分组头 ----
    if (row.header) {
        int cy = rc.top + 15;
        int w = g.MeasureWidth(row.label, FontRole::Meta);
        g.Text(RECT{ rc.left + 4, rc.top, rc.right, rc.bottom }, row.label,
               FontRole::Meta, RGB(143, 149, 158),
               DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
        g.HLine(rc.left + 12 + w + 10, rc.right - 4, cy, RGB(233, 236, 241));
        return;
    }

    // ---- 条目卡片 ----
    const std::vector<UiEntry>& entries = m_app.Entries();
    if (row.entry >= entries.size()) return;
    const UiEntry& e = entries[row.entry];

    RECT card = { rc.left + 2, rc.top + 3, rc.right - 6, rc.bottom - 9 };
    g.FillRound(card, p.surface, m.radiusLg);
    g.Stroke(card,
             selected ? p.primary : (hovered ? p.hoverBorder : p.split),
             m.radiusLg);
    if (selected) {
        RECT ring = { card.left - 1, card.top - 1,
                      card.right + 1, card.bottom + 1 };
        g.Stroke(ring, p.selectRing, m.radiusLg + 1);
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
    g.IconTile(ic, glyph, FontRole::IconLarge, icBg, icTx, 8);

    int tx = ic.right + 12;

    // 行1：内容 + 备注标签
    int line1Right = card.right - 12;
    if (hovered) line1Right -= 3 * 26 + 2 * 6 + 6;

    if (!e.note.empty()) {
        std::wstring tagTxt = L"备注: " + e.note;
        if (tagTxt.size() > 30) tagTxt = tagTxt.substr(0, 30) + L"…";
        int tagW = g.MeasureWidth(tagTxt, FontRole::Meta) + 14;
        RECT rcTag = { line1Right - tagW, card.top + 8, line1Right,
                       card.top + 26 };
        g.FillRound(rcTag, p.warnSoft, 4);
        g.Stroke(rcTag, p.warnBorder, 4);
        g.Text(rcTag, tagTxt, FontRole::Meta, p.warnText,
               DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS |
               DT_NOPREFIX);
        line1Right = rcTag.left - 8;
    }

    RECT rcLine1 = { tx, card.top + 9, line1Right, card.top + 30 };
    g.TextRow(rcLine1, gfx::OneLine(e.content), FontRole::Body, p.text);

    // 行2：来源 · 时间 · 字符数
    std::wstring src = (e.source == L"Manual") ? L"手动录入" : e.source;
    std::wstring meta = src + L" · " + fmt::RelTime(e.createdAt) +
                        L" · " + std::to_wstring((long)e.content.size()) +
                        L" 字符";
    RECT rcLine2 = { tx, card.top + 31, card.right - 12, card.top + 52 };
    g.TextRow(rcLine2, meta, FontRole::Meta, p.textFaint);

    // 悬停操作钮：复制 / 备注 / 删除
    if (hovered) {
        const wchar_t* ops[3] = { L"\xE8C8", L"\xE70F", L"\xE74D" };
        RECT rb[3];
        OpRects(rc.right - rc.left, rb);      // 行内坐标，与命中共用
        for (int i = 0; i < 3; ++i) {
            OffsetRect(&rb[i], rc.left, rc.top);   // 平移到客户区再画
            const bool opHover =
                (m_hoverOpRow == item.index && m_hoverOp == i);
            COLORREF bg = p.iconTile;
            COLORREF fg = (i == 2) ? RGB(229, 72, 77) : RGB(90, 98, 112);
            if (opHover) {
                // 删除单独给一层淡红：误点代价高，颜色先提醒
                bg = (i == 2) ? p.errorSoft : p.selectFill;
                fg = (i == 2) ? p.errorText : p.primary;
            }
            g.FillRound(rb[i], bg, 6);
            g.Text(rb[i], ops[i], FontRole::Icon, fg,
                   DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
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

    // 自绘热区：按下只记状态，动作留到抬起时执行。这样才画得出"按下去"
    // 的那一帧，也才有桌面按钮的语义——按住拖出去松手 = 取消。
    if (const int hot = HitHot(abs)) {
        m_hot.Down(hot);
        SetCapture(true);          // 拖出窗口也要收到抬起
        m_shell->InvalidateContent();
        return true;
    }

    // 搜索框 / 备注框：视觉外框高于 EDIT 客户区，留白点击也要能进入输入状态
    if (m_rcSearchBox.right > m_rcSearchBox.left &&
        PtInRect(&m_rcSearchBox, abs)) {
        m_search->ClickAt(abs);
        return true;
    }
    if (m_rcNoteBox.right > m_rcNoteBox.left && PtInRect(&m_rcNoteBox, abs)) {
        m_noteEdit->ClickAt(abs);
        return true;
    }
    return false;
}

bool ClipsPage::OnMouseUp(POINT pt)
{
    if (!m_hot.Pressing()) return false;

    POINT abs = { pt.x + m_content.left, pt.y + m_content.top };
    const int  hit     = HitHot(abs);
    const bool clicked = m_hot.Release(hit);
    SetCapture(false);

    if (clicked) ActivateHot(hit);
    else         m_shell->InvalidateContent();   // 拖走了：只把按下的样子收回来
    return true;
}

bool ClipsPage::OnMouseMove(POINT pt)
{
    if (!m_visible) return false;

    POINT abs = { pt.x + m_content.left, pt.y + m_content.top };
    if (!m_hot.Move(HitHot(abs))) return false;
    // 只失效内容区：悬停变化不该把标题栏和侧栏一起重画。
    m_shell->InvalidateContent();
    return false;    // 已经自己失效过了，不必让 Shell 再整窗失效
}

bool ClipsPage::OnMouseLeave()
{
    if (!m_hot.Leave()) return false;
    m_shell->InvalidateContent();
    return false;
}

bool ClipsPage::HandCursorAt(POINT pt)
{
    if (!m_visible) return false;
    POINT abs = { pt.x + m_content.left, pt.y + m_content.top };
    return HitHot(abs) != kHotNone;
}

// ============================================================
// 自绘热区
// ============================================================
int ClipsPage::HitHot(POINT absPt) const
{
    for (int i = 0; i < 4; ++i)
        if (PtInRect(&m_rcChips[i], absPt)) return kHotChip + i;

    if (PtInRect(&m_rcAddBtn, absPt)) return kHotAddBtn;

    if (m_formOpen && !m_editingId && PtInRect(&m_rcFormClose, absPt))
        return kHotFormClose;

    // 详情面板三键 + 备注框：只在有选中条目、且几何已经算出来时有效
    if (m_selEntry >= 0 && !m_editingId && m_rcDetail.right > m_rcDetail.left &&
        m_rcDBtn[0].right > m_rcDBtn[0].left) {
        for (int i = 0; i < 3; ++i)
            if (PtInRect(&m_rcDBtn[i], absPt)) return kHotDBtn + i;
        const RECT nb = DetailNoteBox(m_rcDetail);
        if (PtInRect(&nb, absPt)) return kHotNoteBox;
    }
    return kHotNone;
}

void ClipsPage::ActivateHot(int hotId)
{
    if (hotId >= kHotChip && hotId < kHotChip + 4) {
        const int f = hotId - kHotChip;
        // 点的就是当前筛选项：行集合不会变，重建只会白闪一次
        if (f == m_typeFilter) return;
        m_typeFilter = f;
        RefreshList(GetSearchText());
        m_shell->InvalidateContent();
        return;
    }
    if (hotId == kHotAddBtn)    { ToggleForm(true);  return; }
    if (hotId == kHotFormClose) { ToggleForm(false); return; }

    if (hotId >= kHotDBtn && hotId < kHotDBtn + 3) {
        if (m_selEntry >= 0) OnListOp(m_selEntry, hotId - kHotDBtn);
        return;
    }
    if (hotId == kHotNoteBox) {
        if (m_selEntry >= 0 && !m_editingId) OnListOp(m_selEntry, 1);
    }
}

// 指针在列表行内的位置 → 哪个操作钮被指着（几何与 onItemClicked 一致）
void ClipsPage::SyncHoverOp(int row, POINT ptInItem)
{
    int op = -1;
    if (row >= 0 && row < (int)m_rows.size() && !m_rows[row].header) {
        RECT rc = m_list->ItemRect(row);
        RECT rb[3];
        OpRects(rc.right - rc.left, rb);
        for (int i = 0; i < 3; ++i)
            if (PtInRect(&rb[i], ptInItem)) { op = i; break; }
    }
    if (op == m_hoverOp && row == m_hoverOpRow) return;

    const int oldRow = m_hoverOpRow;
    m_hoverOp    = op;
    m_hoverOpRow = (op >= 0) ? row : -1;
    if (oldRow >= 0 && oldRow != m_hoverOpRow) m_list->InvalidateItem(oldRow);
    if (m_hoverOpRow >= 0) m_list->InvalidateItem(m_hoverOpRow);
}

void ClipsPage::OnSetFocus()
{
    if (m_visible && m_search) m_search->Focus();
}

bool ClipsPage::OnCommand(int ctrlId, int code)
{
    // 过插件（F3 搜索防抖、保存按钮等由插件处理；
    // 列表选中/双击已由 ListBox 回调消费，不会到这里）
    m_app.Plugins().DispatchCommand(m_app, ctrlId, code);
    return false;
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

    BatchDraw(false);
    SyncVisibility();
    BatchDraw(true);
    m_shell->InvalidateContent();
}

void ClipsPage::OpenForm()
{
    RefreshList(GetSearchText());
    ToggleForm(true);
    if (m_manualEdit) m_manualEdit->Focus();
}

void ClipsPage::StartNoteEdit(long long id, const std::wstring& note)
{
    if (m_formOpen) ToggleForm(false);   // 表单卡让位

    m_editingId = id;
    Relayout();
    m_noteMulti->SetText(note);
    m_noteMulti->CaretToEnd();
    m_saveBtn->SetText(L"保存备注");
    SyncVisibility();
    m_noteMulti->Focus();
    m_shell->InvalidateContent();
}

void ClipsPage::CancelNoteEdit()
{
    if (!m_editingId) return;
    m_editingId = 0;
    m_saveBtn->SetText(L"保存记录");
    m_noteMulti->Clear();
    Relayout();
    SyncVisibility();
    m_shell->InvalidateContent();
}

void ClipsPage::SaveNoteEdit()
{
    if (!m_editingId) return;
    std::wstring note = m_noteMulti->Text();
    const std::wstring resp = m_app.Ipc().Request(
        L"NOTE\t" + std::to_wstring(m_editingId) +
        L"\t" + ipc::EscapeField(note));
    m_editingId = 0;
    m_saveBtn->SetText(L"保存记录");
    m_noteMulti->Clear();
    Relayout();
    SyncVisibility();
    m_app.RefreshData();
    if (resp.rfind(L"OK", 0) == 0)
        m_app.Toast(L"备注已保存", L"这条记录的备注已更新");
    else
        m_app.Toast(L"备注保存失败", L"服务未响应，请稍后重试", false);
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
    std::wstring cur = m_noteMulti->Text();
    for (UiEntry& e : m_app.EntriesMut()) {
        if (e.id == m_editingId) {
            if (cur != e.note) {
                const std::wstring resp = m_app.Ipc().Request(
                    L"NOTE\t" + std::to_wstring(e.id) +
                    L"\t" + ipc::EscapeField(cur));
                // 只有落库成功才改本地缓存，否则切回来会看到一条
                // 「看着存了、其实没存」的假备注。
                if (resp.rfind(L"OK", 0) == 0) e.note = cur;
                else m_app.Toast(L"备注保存失败",
                                 L"切换条目时未能写入上一条备注", false);
            }
            break;
        }
    }
    StartNoteEdit(ne.id, ne.note);
}

// ============================================================
// 条目操作
// ============================================================
void ClipsPage::OnListOp(int entryIdx, int op)
{
    const std::vector<UiEntry>& entries = m_app.Entries();
    if (entryIdx < 0 || entryIdx >= (int)entries.size()) return;
    const UiEntry& e = entries[entryIdx];

    switch (op) {
    case 0: {
        const std::wstring resp =
            m_app.Ipc().Request(L"COPY\t" + ipc::EscapeField(e.content));
        if (resp.rfind(L"OK", 0) == 0)
            m_app.Toast(L"已复制", L"内容已写入系统剪贴板");
        else
            m_app.Toast(L"复制失败", L"服务未响应，请稍后重试", false);
        break;
    }
    case 1:
        StartNoteEdit(e.id, e.note);
        break;
    case 2:
        if (ShowMessageBox(L"确定删除这条记录吗？删除后不可恢复。",
                           L"删除记录", MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
            // 删除的正是正在编辑的条目 → 先退出编辑态
            if (m_editingId == e.id) CancelNoteEdit();
            const std::wstring resp =
                m_app.Ipc().Request(L"DEL\t" + std::to_wstring(e.id));
            m_selEntry = -1;
            m_app.RefreshData();
            if (resp.rfind(L"OK", 0) == 0)
                m_app.Toast(L"已删除", L"记录已从本地数据库移除");
            else
                m_app.Toast(L"删除失败", L"服务未响应，请稍后重试", false);
        }
        break;
    }
}

void ClipsPage::ShowItemMenu(POINT screenPt, int entryIdx)
{
    const std::vector<UiEntry>& entries = m_app.Entries();
    if (entryIdx < 0 || entryIdx >= (int)entries.size()) return;

    enum { CMD_COPY = 1, CMD_EDIT = 2, CMD_DEL = 3 };
    std::vector<MenuItem> items;
    items.push_back({ CMD_COPY, L"复制内容" });
    items.push_back({ CMD_EDIT,
                      entries[entryIdx].note.empty() ? L"添加备注"
                                                     : L"编辑备注" });
    items.push_back({ 0, L"", true, false });
    items.push_back({ CMD_DEL, L"删除记录" });

    int cmd = ShowPopupMenu(screenPt, items);

    switch (cmd) {
    case CMD_COPY: OnListOp(entryIdx, 0); break;
    case CMD_EDIT: OnListOp(entryIdx, 1); break;
    case CMD_DEL:  OnListOp(entryIdx, 2); break;
    }
}
