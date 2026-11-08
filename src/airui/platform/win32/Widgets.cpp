#include "airui/Widgets.h"
#include "airui/Shell.h"
#include "airui/Gfx.h"
#include <commctrl.h>
#include <cstdarg>
#include <cstdio>

namespace air {
namespace ui {

// List-hover forensics: AIRUI_HOVER_TRACE=1 appends the LISTBOX message and
// draw-item sequence to %LOCALAPPDATA%\AireRecord\list_hover.log, so a
// "the list blinks when the pointer crosses over" report can be turned into
// an actual message trace (which paint ran with which hover index, and how
// much of the list that paint covered). Off by default; costs one cached
// environment lookup.
static bool HoverTraceOn()
{
    static int enabled = -1;
    if (enabled < 0) {
        wchar_t v[8] = {};
        enabled = GetEnvironmentVariableW(L"AIRUI_HOVER_TRACE", v, 8) > 0;
    }
    return enabled > 0;
}

static void HoverLog(const wchar_t* fmt, ...)
{
    if (!HoverTraceOn()) return;

    // Open/append/close per record: the log must stay readable while the
    // client runs, otherwise a held handle blocks the analyst's shell.
    wchar_t path[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 40) return;
    wcscat_s(path, L"\\AireRecord\\list_hover.log");
    FILE* f = nullptr;
    _wfopen_s(&f, path, L"a");
    if (!f) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fwprintf_s(f, L"%02d:%02d:%02d.%03d ", st.wHour, st.wMinute, st.wSecond,
               st.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    vfwprintf_s(f, fmt, ap);
    va_end(ap);
    fwprintf_s(f, L"\n");
    fclose(f);
}

// 子类化 id（占位提示 0xA1 / 按钮 hover 0xB1 已占用）
constexpr UINT_PTR kSubTextBox = 0xC1;
constexpr UINT_PTR kSubListBox = 0xC2;
constexpr UINT_PTR kSubCustom  = 0xC3;

// ============================================================
// Widget 基类
// ============================================================
void Widget::Init(Shell* shell, int id)
{
    m_shell = shell;
    m_id    = id;
}

Widget::~Widget()
{
    if (m_hwnd && IsWindow(m_hwnd))
        DestroyWindow(m_hwnd);
}

void Widget::Show(bool visible)
{
    if (m_hwnd) ::ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
}

bool Widget::Visible() const
{
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void Widget::Move(int x, int y, int w, int h)
{
    if (!m_hwnd) return;
    // Layout re-runs on every page relayout and on every refresh that
    // touches geometry; handing MoveWindow the rect a control already
    // occupies still invalidates it (MoveWindow repaints unconditionally).
    // For EDIT/LISTBOX children that repaint is a visible white flash --
    // an unchanged layout must therefore be a no-op.
    RECT cur = {};
    if (GetWindowRect(m_hwnd, &cur)) {
        HWND parent = GetParent(m_hwnd);
        if (parent) MapWindowPoints(HWND_DESKTOP, parent, (LPPOINT)&cur, 2);
        if (cur.left == x && cur.top == y &&
            cur.right == x + w && cur.bottom == y + h)
            return;
    }
    MoveWindow(m_hwnd, x, y, w, h, TRUE);
}

void Widget::Move(const RECT& rc)
{
    Move(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
}

void Widget::Focus()
{
    if (m_hwnd) ::SetFocus(m_hwnd);
}

RECT Widget::Rect() const
{
    RECT rc = {};
    if (m_hwnd) GetWindowRect(m_hwnd, &rc);
    if (m_shell && m_shell->Hwnd())
        MapWindowPoints(HWND_DESKTOP, m_shell->Hwnd(), (LPPOINT)&rc, 2);
    return rc;
}

// ============================================================
// TextBox
// ============================================================
void TextBox::CreateNative()
{
    HWND parent = m_shell->Hwnd();
    HFONT font  = m_shell->UiFonts().Get(m_desc.fontRole);

    DWORD style = WS_CHILD | WS_TABSTOP |
                  (m_desc.multiline
                       ? ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN
                       : ES_AUTOHSCROLL);
    m_hwnd = CreateWindowExW(0, L"EDIT", L"", style,
                             0, 0, 0, 0, parent,
                             (HMENU)(INT_PTR)m_id, nullptr, nullptr);
    if (!m_hwnd) return;

    SendMessageW(m_hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    SetEditMargins(m_hwnd, 8, 8);
    if (!m_desc.placeholder.empty())
        PlaceholderEdit::Attach(m_hwnd, m_desc.placeholder, font);
    SetWindowSubclass(m_hwnd, &TextBoxProc, kSubTextBox, (DWORD_PTR)this);
}

LRESULT CALLBACK TextBoxProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam, UINT_PTR idSub,
                                    DWORD_PTR ref)
{
    auto* self = (TextBox*)ref;

    switch (msg) {
    case WM_KEYDOWN:
        if (self) {
            bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (self->HandleKey((UINT)wParam, ctrl, shift)) return 0;
        }
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &TextBoxProc, idSub);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

bool TextBox::HandleKey(UINT vk, bool ctrl, bool shift)
{
    return m_keyCb ? m_keyCb(vk, ctrl, shift) : false;
}

std::wstring TextBox::Text() const
{
    return m_hwnd ? GetEditText(m_hwnd) : std::wstring();
}

void TextBox::SetText(const std::wstring& s)
{
    if (m_hwnd) SetWindowTextW(m_hwnd, s.c_str());
}

void TextBox::Clear()
{
    if (m_hwnd) ClearEditText(m_hwnd);
}

void TextBox::CaretToEnd()
{
    if (m_hwnd) MoveCaretToEnd(m_hwnd);
}

void TextBox::SetPlaceholder(const std::wstring& s)
{
    if (m_hwnd) PlaceholderEdit::SetPlaceholder(m_hwnd, s);
}

void TextBox::OnKey(std::function<bool(UINT, bool, bool)> cb)
{
    m_keyCb = std::move(cb);
}

void TextBox::SetRightInset(int px)
{
    if (m_hwnd)
        SendMessageW(m_hwnd, EM_SETMARGINS, EC_RIGHTMARGIN,
                     MAKELPARAM(0, px));
}

void TextBox::ClickAt(POINT parentPt)
{
    if (m_hwnd && m_shell && m_shell->UiFonts().Valid())
        ForwardClickToEdit(m_hwnd, m_shell->UiFonts().Body(), parentPt);
}

// ============================================================
// Button
// ============================================================
void Button::CreateNative()
{
    HWND parent = m_shell->Hwnd();
    m_style = m_desc.style;

    m_hwnd = CreateWindowExW(0, L"BUTTON", m_desc.text.c_str(),
                             WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                             0, 0, 0, 0, parent,
                             (HMENU)(INT_PTR)m_id, nullptr, nullptr);
    if (!m_hwnd) return;

    SendMessageW(m_hwnd, WM_SETFONT,
                 (WPARAM)m_shell->UiFonts().Get(FontRole::Button), TRUE);
    AttachButton(m_hwnd, m_style);
}

void Button::SetText(const std::wstring& s)
{
    if (m_hwnd) SetWindowTextW(m_hwnd, s.c_str());
}

std::wstring Button::Text() const
{
    wchar_t buf[128] = {};
    if (m_hwnd) GetWindowTextW(m_hwnd, buf, 128);
    return buf;
}

void Button::OnClick(std::function<void()> cb)
{
    m_click = std::move(cb);
}

void Button::SetStyle(BtnStyle style)
{
    m_style = style;
}

bool Button::HandleCommand(int code)
{
    if (code == BN_CLICKED && m_click) {
        m_click();
        return true;
    }
    return false;
}

bool Button::HandleDrawItem(const DRAWITEMSTRUCT* dis)
{
    if (!m_shell) return false;
    DrawButton(dis->hDC, dis->rcItem, Text().c_str(),
               m_shell->UiFonts().Get(FontRole::Button),
               m_style,
               IsButtonHover(dis->hwndItem),
               (dis->itemState & ODS_SELECTED) != 0);
    return true;
}

// ============================================================
// ListBox
// ============================================================
void ListBox::CreateNative()
{
    HWND parent = m_shell->Hwnd();
    m_fallbackH = m_desc.fallbackItemHeight;

    // 不要 WS_EX_COMPOSITED：它给的是「本窗口的下级子树双缓冲」，LISTBOX
    // 没有子窗口，等于完全没用；更糟的是父窗口一旦不再合成（见 Shell 的
    // WS_CLIPCHILDREN 修正），带这个扩展样式创建 LISTBOX 会直接失败
    // （CreateWindowEx 返回 NULL 且 GetLastError=0），整块列表就消失了。
    m_hwnd = CreateWindowExW(0, L"LISTBOX", L"",
        WS_CHILD | WS_VSCROLL | WS_TABSTOP |
        LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWVARIABLE,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)m_id, nullptr, nullptr);
    if (!m_hwnd) {
        HoverLog(L"listbox create FAILED gle=%lu", GetLastError());
        return;
    }

    SendMessageW(m_hwnd, WM_SETFONT,
                 (WPARAM)m_shell->UiFonts().Body(), TRUE);
    SetWindowSubclass(m_hwnd, &ListBoxProc, kSubListBox, (DWORD_PTR)this);
}

LRESULT CALLBACK ListBoxProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam, UINT_PTR idSub,
                                    DWORD_PTR ref)
{
    auto* self = (ListBox*)ref;

    switch (msg) {
    case WM_ERASEBKGND: {
        // LISTBOX 画的是原生窗口，不在任何双缓冲里：先擦整片客户区、再
        // 逐行回调 WM_DRAWITEM，中间那段（16 行 × 1~2ms ≈ 20ms）屏幕上
        // 就是一张空白底。点筛选胶囊重建列表时整块卡片区就是这么"闪"
        // 一下的。WS_EX_COMPOSITED 帮不上忙——它给的是"窗口的下级子树
        // 双缓冲"，而 LISTBOX 没有子窗口，等于没过缓冲。
        //
        // 所以只擦「最后一行之后」的那条带：条目区不擦，由每行的卡片自
        // 己覆盖（卡片绘制时已铺满自己的 item rect），于是重建时屏幕上
        // 的像素只会被"就地覆盖"，不会先退化成底色。列表变短时多出来的
        // 下半截仍由这条带清掉。
        HDC dc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        RECT band = rc;
        const int n = (int)SendMessageW(hwnd, LB_GETCOUNT, 0, 0);
        if (n > 0) {
            RECT last = {};
            if (SendMessageW(hwnd, LB_GETITEMRECT, n - 1, (LPARAM)&last) != LB_ERR)
                band.top = last.bottom;
        }
        if (band.top < band.bottom) {
            HoverLog(L"erase band %ld,%ld-%ld,%ld items=%d",
                     (long)band.left, (long)band.top,
                     (long)band.right, (long)band.bottom, n);
            gfx::Fill(dc, band, Pal().bg);
        } else {
            HoverLog(L"erase skipped items=%d", n);
        }
        return 1;
    }
    case WM_LBUTTONDOWN: {
        if (!self) break;
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int item = list::ItemFromPoint(hwnd, pt);
        if (item >= 0 && self->onItemClicked) {
            RECT rc;
            SendMessageW(hwnd, LB_GETITEMRECT, item, (LPARAM)&rc);
            POINT ptIn = { pt.x - rc.left, pt.y - rc.top };
            if (self->onItemClicked(item, ptIn)) return 0;
        }
        break;   // 其余交给列表默认选中逻辑
    }
    case WM_MOUSEMOVE: {
        if (!self) break;
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int item = list::ItemFromPoint(hwnd, pt);
        HoverLog(L"move pt=%d,%d item=%d hover=%d", (int)pt.x, (int)pt.y, item,
                 self->m_hover);
        if (item != self->m_hover) {
            int old = self->m_hover;
            self->m_hover = item;
            if (old >= 0) {
                RECT rc;
                SendMessageW(hwnd, LB_GETITEMRECT, old, (LPARAM)&rc);
                InvalidateRect(hwnd, &rc, FALSE);
            }
            if (item >= 0) {
                RECT rc;
                SendMessageW(hwnd, LB_GETITEMRECT, item, (LPARAM)&rc);
                InvalidateRect(hwnd, &rc, FALSE);
            }
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        break;
    }
    case WM_MOUSELEAVE: {
        if (!self) break;
        int old = self->m_hover;
        self->m_hover = -1;
        RECT rc = {};
        if (old >= 0) {
            SendMessageW(hwnd, LB_GETITEMRECT, old, (LPARAM)&rc);
            InvalidateRect(hwnd, &rc, FALSE);
        }
        HoverLog(L"leave hover=%d invalidated=%ld,%ld-%ld,%ld", old,
                 (long)rc.left, (long)rc.top, (long)rc.right, (long)rc.bottom);
        // 吃掉这条消息，不下传：标准 LISTBOX 自己的 WM_MOUSELEAVE 处理
        // 会整客户区失效一次（整块擦成底色再逐行画回来，约 10ms），鼠标
        // 一离开列表就能看到整列表闪一下。hover 完全由我们自己维护，
        // 这里只失效原先悬停的那一行。
        return 0;
    }
    case WM_PAINT: {
        if (!self) break;
        if (HoverTraceOn()) {
            RECT upd = {};
            GetUpdateRect(hwnd, &upd, FALSE);
            HoverLog(L"paint update=%ld,%ld-%ld,%ld hover=%d",
                     (long)upd.left, (long)upd.top, (long)upd.right,
                     (long)upd.bottom, self->m_hover);
        }
        if (!self->onPaintEmpty) break;
        if (SendMessageW(hwnd, LB_GETCOUNT, 0, 0) != 0) break;
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        gfx::Fill(dc, rc, Pal().bg);
        Canvas g(dc, self->m_shell->UiFonts());
        self->onPaintEmpty(g, rc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CONTEXTMENU: {
        if (!self || !self->onContextMenu) break;
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
        self->onContextMenu(pt, item);
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &ListBoxProc, idSub);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void ListBox::Reset()
{
    if (m_hwnd) SendMessageW(m_hwnd, LB_RESETCONTENT, 0, 0);
    m_hover = -1;
}

int ListBox::AddItem(int height, size_t data)
{
    if (!m_hwnd) return -1;
    int idx = (int)SendMessageW(m_hwnd, LB_ADDSTRING, 0, (LPARAM)L"");
    SendMessageW(m_hwnd, LB_SETITEMDATA, idx, (LPARAM)data);
    SendMessageW(m_hwnd, LB_SETITEMHEIGHT, idx, height);
    return idx;
}

int ListBox::Count()
{
    return m_hwnd ? (int)SendMessageW(m_hwnd, LB_GETCOUNT, 0, 0) : 0;
}

int ListBox::Selection()
{
    return m_hwnd ? (int)SendMessageW(m_hwnd, LB_GETCURSEL, 0, 0) : LB_ERR;
}

void ListBox::SetSelection(int index)
{
    if (m_hwnd) SendMessageW(m_hwnd, LB_SETCURSEL, index, 0);
}

RECT ListBox::ItemRect(int index)
{
    RECT rc = {};
    if (m_hwnd)
        SendMessageW(m_hwnd, LB_GETITEMRECT, index, (LPARAM)&rc);
    return rc;
}

void ListBox::InvalidateItem(int index)
{
    if (!m_hwnd) return;
    RECT rc = ItemRect(index);
    InvalidateRect(m_hwnd, &rc, TRUE);
}

void ListBox::Invalidate()
{
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
}

int ListBox::ItemAtPoint(POINT clientPt)
{
    return m_hwnd ? list::ItemFromPoint(m_hwnd, clientPt) : -1;
}

bool ListBox::HandleCommand(int code)
{
    if (code == LBN_SELCHANGE) {
        if (onSelectChanged) {
            int sel = Selection();
            if (sel != LB_ERR) onSelectChanged(sel);
        }
        return true;
    }
    if (code == LBN_DBLCLK) {
        if (onDoubleClicked) {
            int sel = Selection();
            if (sel != LB_ERR) onDoubleClicked(sel);
        }
        return true;
    }
    return false;
}

bool ListBox::HandleDrawItem(const DRAWITEMSTRUCT* dis)
{
    if (!onPaintItem || !m_shell) return false;
    ListItemDraw it;
    it.index    = (int)dis->itemID;
    it.data     = (size_t)dis->itemData;
    it.rc       = dis->rcItem;
    it.selected = (dis->itemState & ODS_SELECTED) != 0;
    it.hovered  = ((int)dis->itemID == m_hover);
    HoverLog(L"draw idx=%d hover=%d sel=%d rc=%ld,%ld-%ld,%ld",
             it.index, (int)it.hovered, (int)it.selected,
             (long)it.rc.left, (long)it.rc.top, (long)it.rc.right,
             (long)it.rc.bottom);

    // 行底 = 页面灰（卡片浮于其上），与旧实现保持一致
    gfx::Fill(dis->hDC, dis->rcItem, Pal().bg);

    Canvas g(dis->hDC, m_shell->UiFonts());
    onPaintItem(g, it);
    return true;
}

bool ListBox::HandleMeasureItem(MEASUREITEMSTRUCT* mis)
{
    mis->itemHeight = m_fallbackH;
    return true;
}

// ============================================================
// Custom
// ============================================================
void Custom::CreateNative()
{
    HWND parent = m_shell->Hwnd();

    m_hwnd = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
                             0, 0, 0, 0, parent,
                             (HMENU)(INT_PTR)m_id, nullptr, nullptr);
    if (!m_hwnd) return;

    SetWindowSubclass(m_hwnd, &CustomProc, kSubCustom, (DWORD_PTR)this);
}

LRESULT CALLBACK CustomProc(HWND hwnd, UINT msg, WPARAM wParam,
                                   LPARAM lParam, UINT_PTR idSub,
                                   DWORD_PTR ref)
{
    auto* self = (Custom*)ref;

    switch (msg) {
    case WM_NCHITTEST:
        if (self) {
            LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
            return (r == HTCLIENT) ? HTTRANSPARENT : r;   // 鼠标穿透
        }
        break;
    case WM_ERASEBKGND:
        return 1;   // 自绘全覆盖，免闪烁
    case WM_PAINT: {
        if (!self || !self->m_painter) break;
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (self->m_shell) {
            Canvas g(dc, self->m_shell->UiFonts());
            self->m_painter(g, rc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &CustomProc, idSub);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void Custom::OnPaint(std::function<void(Canvas&, const RECT&)> painter)
{
    m_painter = std::move(painter);
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
}

} // namespace ui
} // namespace air
