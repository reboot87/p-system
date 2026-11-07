#include "airui/Controls.h"
#include "airui/Gfx.h"
#include "airui/Theme.h"
#include <commctrl.h>
#include <dwmapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

namespace air {
namespace ui {

// ============================================================
// 通用小工具
// ============================================================
namespace {

// 窗口属性名（用字符串，静态库场景无需 RegisterWindowMessage）
constexpr const wchar_t* kPropPlaceholder = L"AirUi.Placeholder";
constexpr const wchar_t* kPropBtn         = L"AirUi.Button";
constexpr const wchar_t* kPropComposing   = L"AirUi.ImeComposing";

} // namespace

// ============================================================
// 输入框辅助
// ============================================================
void SetEditMargins(HWND edit, int left, int right)
{
    if (!edit) return;
    SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELPARAM(left, right));
}

void ClearEditText(HWND edit)
{
    if (!edit || !IsWindow(edit)) return;
    SendMessageW(edit, EM_SETSEL, 0, -1);
    SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)L"");
}

void MoveCaretToEnd(HWND edit)
{
    if (!edit || !IsWindow(edit)) return;
    int len = GetWindowTextLengthW(edit);
    SendMessageW(edit, EM_SETSEL, len, len);
}

std::wstring GetEditText(HWND edit)
{
    if (!edit || !IsWindow(edit)) return std::wstring();
    // 跨进程读 EDIT 必须走 WM_GETTEXT；GetWindowTextW 会读到陈旧 caption
    LRESULT len = SendMessageW(edit, WM_GETTEXTLENGTH, 0, 0);
    if (len <= 0) return std::wstring();
    std::wstring s((size_t)len + 1, L'\0');
    SendMessageW(edit, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)&s[0]);
    s.resize((size_t)len);
    return s;
}

void ForwardClickToEdit(HWND edit, HFONT font, POINT ptParent)
{
    if (!edit || !IsWindowVisible(edit)) return;

    RECT er;
    GetWindowRect(edit, &er);
    MapWindowPoints(HWND_DESKTOP, GetParent(edit), (LPPOINT)&er, 2);

    int cx = ptParent.x - er.left;
    int cy = ptParent.y - er.top;
    int ew = er.right - er.left;
    int eh = er.bottom - er.top;
    if (cx < 0) cx = 0;
    if (cx > ew) cx = ew;
    if (cy < 0) cy = 0;
    if (cy > eh) cy = eh;

    // 按字符宽度累加换算命中的字符下标（不依赖 EM_CHARINDEXFROMPOS）
    int len = GetWindowTextLengthW(edit);
    UINT idx = (UINT)len;
    if (len > 0 && font) {
        std::wstring buf = GetEditText(edit);
        HDC dc = GetDC(edit);
        HGDIOBJ old = SelectObject(dc, font);
        int target = cx - 8;   // 左内边距 8px
        int acc = 0;
        for (int i = 0; i < len; ++i) {
            SIZE sz{};
            GetTextExtentPoint32W(dc, buf.c_str() + i, 1, &sz);
            if (target < acc + sz.cx / 2) { idx = (UINT)i; break; }
            acc += sz.cx;
        }
        SelectObject(dc, old);
        ReleaseDC(edit, dc);
    }
    SetFocus(edit);
    SendMessageW(edit, EM_SETSEL, idx, idx);
    SetCaretPos(cx, cy);
}

// ============================================================
// PlaceholderEdit
// ============================================================
namespace {

struct PhData
{
    std::wstring        text;
    HFONT               font = nullptr;
    PlaceholderEdit::KeyCallback key;
};

PhData* PhOf(HWND edit)
{
    return (PhData*)GetPropW(edit, kPropPlaceholder);
}

LRESULT CALLBACK PhProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                        UINT_PTR idSub, DWORD_PTR)
{
    PhData* d = PhOf(hwnd);
    if (!d) return DefSubclassProc(hwnd, msg, wParam, lParam);

    // ---- 业务按键回调（Esc / Ctrl+Enter 等） ----
    if (msg == WM_KEYDOWN && d->key) {
        bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (d->key(hwnd, (UINT)wParam, ctrl, shift))
            return 0;   // 已消费，吞掉默认处理（含提示音）
    }

    // ---- IME 组词态标记 ----
    // 组词期间 GetWindowTextLength 仍为 0，若照常画占位字会压在拼音候选上
    if (msg == WM_IME_STARTCOMPOSITION) {
        SetPropW(hwnd, kPropComposing, (HANDLE)1);
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
    if (msg == WM_IME_ENDCOMPOSITION) {
        RemovePropW(hwnd, kPropComposing);
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return r;
    }

    // 焦点变化必须重绘，否则占位字会停留在错误状态
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    if (msg == WM_PAINT) {
        // 先让系统把编辑框本体（背景/文字/光标）画完，再叠加占位字
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);

        bool composing = GetPropW(hwnd, kPropComposing) != nullptr;
        if (!composing && d->text.size() &&
            GetWindowTextLengthW(hwnd) == 0 && GetFocus() != hwnd)
        {
            HDC dc = GetDC(hwnd);
            RECT rc;
            GetClientRect(hwnd, &rc);
            rc.left += 9;
            rc.right -= 9;
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, Pal().textFaint);
            HGDIOBJ old = SelectObject(dc, d->font ? d->font : GetStockObject(DEFAULT_GUI_FONT));

            bool multiline =
                (GetWindowLongW(hwnd, GWL_STYLE) & ES_MULTILINE) != 0;
            // 真实文字由系统绘制：单行 EDIT 恒定贴客户区顶部，多行首行同样在顶部。
            // 占位字一律 DT_TOP，与真实文字落在同一基线。
            UINT fmt = DT_LEFT | DT_NOPREFIX | DT_TOP;
            if (multiline)
                fmt |= DT_WORDBREAK | DT_EDITCONTROL | DT_NOFULLWIDTHCHARBREAK;
            else
                fmt |= DT_SINGLELINE;
            DrawTextW(dc, d->text.c_str(), (int)d->text.size(), &rc, fmt);

            SelectObject(dc, old);
            ReleaseDC(hwnd, dc);
        }
        return r;
    }

    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, &PhProc, idSub);
        RemovePropW(hwnd, kPropPlaceholder);
        RemovePropW(hwnd, kPropComposing);
        delete d;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

} // namespace

void PlaceholderEdit::Attach(HWND edit, const std::wstring& placeholder, HFONT font)
{
    if (!edit || !IsWindow(edit)) return;
    PhData* d = PhOf(edit);
    if (!d) {
        d = new PhData();
        SetPropW(edit, kPropPlaceholder, (HANDLE)d);
        SetWindowSubclass(edit, &PhProc, 0xA1, 0);
    }
    d->text = placeholder;
    if (font) d->font = font;
}

void PlaceholderEdit::Detach(HWND edit)
{
    if (!edit || !IsWindow(edit)) return;
    PhData* d = PhOf(edit);
    RemovePropW(edit, kPropPlaceholder);
    RemoveWindowSubclass(edit, &PhProc, 0xA1);
    delete d;
    InvalidateRect(edit, nullptr, FALSE);
}

bool PlaceholderEdit::Attached(HWND edit)
{
    return edit && IsWindow(edit) && PhOf(edit) != nullptr;
}

void PlaceholderEdit::SetPlaceholder(HWND edit, const std::wstring& text)
{
    if (PhData* d = PhOf(edit)) {
        d->text = text;
        InvalidateRect(edit, nullptr, FALSE);
    }
}

void PlaceholderEdit::SetFont(HWND edit, HFONT font)
{
    if (PhData* d = PhOf(edit)) {
        d->font = font;
        InvalidateRect(edit, nullptr, FALSE);
    }
}

void PlaceholderEdit::SetKeyCallback(HWND edit, KeyCallback cb)
{
    if (PhData* d = PhOf(edit)) d->key = std::move(cb);
}

// ============================================================
// 按钮
// ============================================================
namespace {

struct BtnState
{
    BtnStyle style   = BtnStyle::Primary;
    bool     hover   = false;
    bool     pressed = false;
};

BtnState* BtnOf(HWND h)
{
    return (BtnState*)GetPropW(h, kPropBtn);
}

LRESULT CALLBACK BtnProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                         UINT_PTR idSub, DWORD_PTR)
{
    BtnState* s = BtnOf(hwnd);
    if (!s) return DefSubclassProc(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_MOUSEMOVE:
        if (!s->hover) {
            s->hover = true;
            InvalidateRect(hwnd, nullptr, TRUE);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        break;
    case WM_MOUSELEAVE:
        if (s->hover || s->pressed) {
            s->hover = s->pressed = false;
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        break;
    case WM_LBUTTONDOWN:
        s->pressed = true;
        InvalidateRect(hwnd, nullptr, TRUE);
        break;
    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
        if (s->pressed) {
            s->pressed = false;
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &BtnProc, idSub);
        RemovePropW(hwnd, kPropBtn);
        delete s;
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

} // namespace

void AttachButton(HWND button, BtnStyle style)
{
    if (!button || !IsWindow(button)) return;
    BtnState* s = BtnOf(button);
    if (!s) {
        s = new BtnState();
        SetPropW(button, kPropBtn, (HANDLE)s);
        SetWindowSubclass(button, &BtnProc, 0xB1, 0);
    }
    s->style = style;
}

void DetachButton(HWND button)
{
    if (!button || !IsWindow(button)) return;
    BtnState* s = BtnOf(button);
    RemovePropW(button, kPropBtn);
    RemoveWindowSubclass(button, &BtnProc, 0xB1);
    delete s;
}

bool IsButtonHover(HWND button)
{
    BtnState* s = BtnOf(button);
    return s && s->hover;
}

bool IsButtonPressed(HWND button)
{
    BtnState* s = BtnOf(button);
    return s && s->pressed;
}

BtnStyle ButtonStyleOf(HWND button)
{
    BtnState* s = BtnOf(button);
    return s ? s->style : BtnStyle::Primary;
}

void DrawButton(HDC dc, const RECT& rc, const wchar_t* text, HFONT font,
                BtnStyle style, bool hover, bool pressed, bool disabled)
{
    const Palette& p = Pal();
    RECT r = rc;

    switch (style) {
    case BtnStyle::Primary: {
        COLORREF fill = p.primary;
        if (disabled)      fill = p.border;
        else if (pressed)  fill = p.primaryDown;
        else if (hover)    fill = p.primaryHover;
        gfx::FillRound(dc, r, fill, Mx().radiusMd);
        gfx::Text(dc, font, p.textInverse, r, text,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        break;
    }
    case BtnStyle::Secondary: {
        COLORREF fill = (pressed || hover) ? p.active : p.surface;
        gfx::FillRound(dc, r, fill, Mx().radiusMd);
        gfx::Stroke(dc, r, hover ? p.hoverBorder : p.border, Mx().radiusMd);
        gfx::Text(dc, font, disabled ? p.textDisabled : p.textSub, r, text,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        break;
    }
    case BtnStyle::Danger: {
        COLORREF fill = pressed ? p.errorSoft : (hover ? p.errorSoft : p.surface);
        gfx::FillRound(dc, r, fill, Mx().radiusMd);
        gfx::Stroke(dc, r, disabled ? p.border : p.errorBorder, Mx().radiusMd);
        gfx::Text(dc, font, disabled ? p.textDisabled : p.errorText, r, text,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        break;
    }
    case BtnStyle::Ghost: {
        if (hover) gfx::FillRound(dc, r, p.primarySoft, Mx().radiusMd);
        gfx::Text(dc, font, disabled ? p.textDisabled : p.primaryText, r, text,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        break;
    }
    }
}

// ============================================================
// hover 跟踪
// ============================================================
namespace hover {

bool OnMouseMove(HWND hwnd, bool& state)
{
    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
    TrackMouseEvent(&tme);
    if (state) return false;
    state = true;
    return true;
}

bool OnMouseLeave(HWND, bool& state)
{
    if (!state) return false;
    state = false;
    return true;
}

} // namespace hover

// ============================================================
// NavBar
// ============================================================
void NavBar::SetGroups(std::vector<NavGroup> groups)
{
    m_groups = std::move(groups);
    RebuildRects();
}

void NavBar::Layout(const RECT& side)
{
    m_side = side;
    RebuildRects();
}

void NavBar::RebuildRects()
{
    m_items.clear();
    m_groupsRect.clear();

    const Metrics& m = Mx();
    const int padX = 8;
    int y = m_side.top + 10;

    for (auto& g : m_groups) {
        if (!g.title.empty()) {
            GroupRect gr;
            gr.title = g.title;
            gr.rc = { m_side.left + 14, y, m_side.right - 8, y + 24 };
            m_groupsRect.push_back(gr);
            y += 24;
        }
        for (auto& it : g.items) {
            ItemRect ir;
            ir.id       = it.id;
            ir.disabled = it.disabled;
            ir.rc = { m_side.left + padX, y, m_side.right - padX, y + m.rowH };
            m_items.push_back(ir);
            y += m.rowH;
        }
        y += 6;
    }
    m_contentH = y - m_side.top;
}

void NavBar::Paint(HDC dc, const Fonts& fonts)
{
    if (m_side.right <= m_side.left) return;

    const Palette& p = Pal();
    const Metrics& m = Mx();

    // 侧栏底色 + 右侧分隔线
    gfx::Fill(dc, m_side, p.sidebar);
    gfx::VLine(dc, m_side.right - 1, m_side.top, m_side.bottom, p.sidebarLine);

    // 分组标题
    for (auto& gr : m_groupsRect) {
        gfx::Text(dc, fonts.Get(FontRole::Meta), RGB(143, 149, 158), gr.rc,
                  gr.title, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    }

    // 条目
    int idx = 0;
    for (auto& g : m_groups) {
        for (auto& it : g.items) {
            if (idx >= (int)m_items.size()) break;
            const ItemRect& ir = m_items[idx++];

            bool active = (it.id == m_active) && !it.disabled;
            bool hot    = (it.id == m_hoverId) && !it.disabled;

            if (active)      gfx::FillRound(dc, ir.rc, p.selectFill, 6);
            else if (hot)    gfx::FillRound(dc, ir.rc, p.active, 6);

            // 图标
            RECT rcIc = { ir.rc.left + 12, ir.rc.top,
                          ir.rc.left + 34, ir.rc.bottom };
            COLORREF icTx = it.disabled ? p.textDisabled
                                      : (active ? p.primary : RGB(90, 98, 112));
            gfx::Text(dc, fonts.Get(FontRole::Icon), icTx, rcIc,
                      it.glyph, DT_SINGLELINE | DT_CENTER | DT_VCENTER |
                                DT_NOPREFIX);

            // 文本（右侧留出角标空间）
            int textRight = ir.rc.right - 10;
            if (!it.badge.empty()) textRight -= 52;
            RECT rcTx = { ir.rc.left + 40, ir.rc.top, textRight, ir.rc.bottom };
            COLORREF txCol = it.disabled ? p.textDisabled
                                        : (active ? p.primaryText : p.text);
            gfx::Text(dc, active ? fonts.Get(FontRole::Title)
                                 : fonts.Get(FontRole::Body),
                      txCol, rcTx, it.label,
                      DT_SINGLELINE | DT_LEFT | DT_VCENTER |
                      DT_END_ELLIPSIS | DT_NOPREFIX);

            // 角标
            if (!it.badge.empty()) {
                int bw = gfx::MeasureWidth(dc, fonts.Get(FontRole::Meta),
                                           it.badge) + 14;
                RECT rcB = { ir.rc.right - 8 - bw, ir.rc.top + 8,
                             ir.rc.right - 8, ir.rc.top + 24 };
                COLORREF bg, tx;
                if (it.disabled) {
                    bg = RGB(239, 241, 244); tx = RGB(143, 149, 158);
                } else if (it.badgeOk) {
                    bg = p.okSoft; tx = p.okText;
                } else if (active) {
                    bg = p.selectRing; tx = p.primaryText;
                } else {
                    bg = RGB(239, 241, 244); tx = RGB(143, 149, 158);
                }
                gfx::FillRound(dc, rcB, bg, 8);
                gfx::Text(dc, fonts.Get(FontRole::Meta), tx, rcB, it.badge,
                          DT_SINGLELINE | DT_CENTER | DT_VCENTER |
                          DT_NOPREFIX);
            }
        }
    }
}

int NavBar::HitTest(POINT pt) const
{
    for (auto& ir : m_items)
        if (PtInRect(&ir.rc, pt)) return ir.id;
    return -1;
}

bool NavBar::OnMouseMove(POINT pt)
{
    int id = HitTest(pt);
    if (id == m_hoverId) return false;
    m_hoverId = id;
    return true;
}

bool NavBar::OnMouseLeave()
{
    if (m_hoverId == -1) return false;
    m_hoverId = -1;
    return true;
}

bool NavBar::IsDisabled(int id) const
{
    for (auto& g : m_groups)
        for (auto& it : g.items)
            if (it.id == id) return it.disabled;
    return true;
}

void NavBar::SetBadge(int id, const std::wstring& text, bool ok)
{
    for (auto& g : m_groups)
        for (auto& it : g.items)
            if (it.id == id) {
                it.badge   = text;
                it.badgeOk = ok;
                return;
            }
}

// ============================================================
// PaintDC
// ============================================================
PaintDC::PaintDC(HWND hwnd) : m_hwnd(hwnd)
{
    m_wdc = BeginPaint(hwnd, &m_ps);
    m_began = true;
    GetClientRect(hwnd, &m_client);

    int w = m_client.right, h = m_client.bottom;
    if (w <= 0 || h <= 0) return;

    m_mem = CreateCompatibleDC(m_wdc);
    m_bmp = CreateCompatibleBitmap(m_wdc, w, h);
    if (m_mem && m_bmp) {
        m_old = SelectObject(m_mem, m_bmp);
        // A Device-Dependent Bitmap arrives with UNDEFINED contents. Any
        // pixel the active backend leaves transparent is AlphaBlended
        // against whatever was in here, so a partially covered frame used
        // to present as black noise for a moment. Pre-fill with the theme
        // background: transparency then degrades to the correct colour
        // instead of garbage.
        RECT rcFill = m_client;
        gfx::Brush bgBrush(Pal().bg);
        FillRect(m_mem, &rcFill, bgBrush);
    }
}

PaintDC::~PaintDC()
{
    if (m_mem) {
        if (m_old) SelectObject(m_mem, m_old);
        if (m_bmp) DeleteObject(m_bmp);
        DeleteDC(m_mem);
    }
    if (m_began) EndPaint(m_hwnd, &m_ps);
}

void PaintDC::Flush()
{
    if (m_flushed || !m_began) return;
    if (m_mem && m_wdc)
        BitBlt(m_wdc, 0, 0, m_client.right, m_client.bottom,
               m_mem, 0, 0, SRCCOPY);
    m_flushed = true;
}

// ============================================================
// 列表工具 / 空状态
// ============================================================
namespace list {

int ItemFromPoint(HWND listbox, POINT clientPt)
{
    DWORD pos = (DWORD)SendMessageW(listbox, LB_ITEMFROMPOINT, 0,
                                    MAKELPARAM(clientPt.x, clientPt.y));
    if (HIWORD(pos)) return -1;   // 点在条目之外
    return (int)LOWORD(pos);
}

} // namespace list

void DrawEmptyState(HDC dc, const RECT& rc, const Fonts& fonts,
                    const EmptyState& state)
{
    const Palette& p = Pal();
    int midY = rc.top + (rc.bottom - rc.top) / 2;

    RECT rcT = { rc.left, midY - 72, rc.right, midY - 40 };
    gfx::Text(dc, fonts.Get(FontRole::Section), p.text, rcT, state.title,
              DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);

    if (!state.steps.empty()) {
        RECT rcS = { rc.left + 24, rcT.bottom + 4, rc.right - 24,
                     rcT.bottom + 94 };
        gfx::Text(dc, fonts.Get(FontRole::Body), p.textSub, rcS, state.steps,
                  DT_CENTER | DT_VCENTER | DT_EXTERNALLEADING | DT_NOPREFIX);
    }

    if (!state.foot.empty()) {
        RECT rcF = { rc.left, midY + 40, rc.right, midY + 60 };
        gfx::Text(dc, fonts.Get(FontRole::Meta), p.textFaint, rcF, state.foot,
                  DT_SINGLELINE | DT_CENTER | DT_NOPREFIX);
    }
}

// ============================================================
// DWM 外壳
// ============================================================
void ApplyDwmFrame(HWND hwnd)
{
    // Win10：1px 扩展边框 → 阴影；Win11：圆角偏好
    MARGINS margins = { 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    INT pref = 2;   // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/,
                          &pref, sizeof(pref));

    // 强制重算 NC 区，确保 WM_NCCALCSIZE 在首次显示前生效
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
}

} // namespace ui
} // namespace air
