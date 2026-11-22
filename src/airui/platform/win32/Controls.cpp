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
constexpr const wchar_t* kPropBtn         = L"AirUi.Button";

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
        // 与 core 的 widgets::Button::Paint 保持一致：悬停同时改描边和
        // 文字颜色，只换底色在浅色主题下几乎看不出来。
        COLORREF fill = pressed ? p.selectFill : (hover ? p.active : p.surface);
        COLORREF line = pressed ? p.primaryDown
                                : (hover ? p.primary : p.border);
        gfx::FillRound(dc, r, fill, Mx().radiusMd);
        gfx::Stroke(dc, r, disabled ? p.border : line, Mx().radiusMd);
        gfx::Text(dc, font,
                  disabled ? p.textDisabled
                           : ((hover || pressed) ? p.primaryText : p.textSub),
                  r, text,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        break;
    }
    case BtnStyle::Danger: {
        COLORREF fill = pressed ? p.errorSoft : (hover ? p.errorSoft : p.surface);
        COLORREF line = (hover || pressed) ? p.error : p.errorBorder;
        gfx::FillRound(dc, r, fill, Mx().radiusMd);
        gfx::Stroke(dc, r, disabled ? p.border : line, Mx().radiusMd);
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
void PaintDC::BackBuffer::Release()
{
    if (mem) {
        if (old) SelectObject(mem, old);
        old = nullptr;
    }
    if (bmp) { DeleteObject(bmp); bmp = nullptr; }
    if (mem) { DeleteDC(mem); mem = nullptr; }
    w = h = 0;
}

PaintDC::PaintDC(HWND hwnd, bool useBackBuffer, BackBuffer* cache)
    : m_hwnd(hwnd), m_cache(cache)
{
    m_wdc = BeginPaint(hwnd, &m_ps);
    m_began = true;
    GetClientRect(hwnd, &m_client);

    // BeginPaint hands back exactly the region that changed. Honour it:
    // the caller (Shell::Paint) clips to this and we blit only this. A
    // whole-window invalidate leaves rcPaint == client, so the common
    // full-frame case is byte-for-byte what it was.
    m_update = m_client;
    if (m_ps.rcPaint.right > m_ps.rcPaint.left &&
        m_ps.rcPaint.bottom > m_ps.rcPaint.top) {
        m_update.left   = m_ps.rcPaint.left   > m_client.left   ? m_ps.rcPaint.left   : m_client.left;
        m_update.top    = m_ps.rcPaint.top    > m_client.top    ? m_ps.rcPaint.top    : m_client.top;
        m_update.right  = m_ps.rcPaint.right  < m_client.right  ? m_ps.rcPaint.right  : m_client.right;
        m_update.bottom = m_ps.rcPaint.bottom < m_client.bottom ? m_ps.rcPaint.bottom : m_client.bottom;
    }
    if (m_update.right <= m_update.left || m_update.bottom <= m_update.top)
        m_update = m_client;

    int w = m_client.right, h = m_client.bottom;
    if (!useBackBuffer || w <= 0 || h <= 0) return;

    if (m_cache) {
        bool fresh = false;
        // Reuse the window's buffer when the size still matches. Growth
        // and DPI changes take the slow path below.
        if (!(m_cache->mem && m_cache->bmp && m_cache->w == w && m_cache->h == h)) {
            m_cache->Release();
            m_cache->mem = CreateCompatibleDC(m_wdc);
            m_cache->bmp = CreateCompatibleBitmap(m_wdc, w, h);
            if (m_cache->mem && m_cache->bmp) {
                m_cache->old = SelectObject(m_cache->mem, m_cache->bmp);
                m_cache->w = w;
                m_cache->h = h;
                fresh = true;
            } else {
                m_cache->Release();
            }
        }
        m_mem = m_cache->mem;
        m_bmp = m_cache->bmp;
        m_old  = m_cache->old;
        if (fresh && m_mem && m_bmp) {
            // Only a brand-new DDB has undefined contents, so fill the
            // whole buffer once (a later partial paint may present an area
            // this first WM_PAINT never touched). A reused buffer still
            // holds the previous frame, which is a safe backdrop for
            // anything a page leaves transparent -- and skipping this fill
            // removes the last full-area GDI work from every frame.
            RECT all = { 0, 0, w, h };
            gfx::Brush bgBrush(Pal().bg);
            FillRect(m_mem, &all, bgBrush);
        }
        return;
    }

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
        gfx::Brush bgBrush(Pal().bg);
        FillRect(m_mem, &m_update, bgBrush);
    }
}

PaintDC::~PaintDC()
{
    // The cache path hands ownership back to the owner; only the
    // allocate-per-frame path destroys the objects it made here.
    if (!m_cache) {
        if (m_mem) {
            if (m_old) SelectObject(m_mem, m_old);
            if (m_bmp) DeleteObject(m_bmp);
            DeleteDC(m_mem);
        }
    }
    m_mem = nullptr;
    m_bmp = nullptr;
    m_old = nullptr;
    if (m_began) EndPaint(m_hwnd, &m_ps);
}

void PaintDC::Flush()
{
    if (m_flushed || !m_began) return;
    if (m_mem && m_wdc) {
        // Blit only what changed. The BeginPaint DC is itself clipped to
        // the true update region, so a bounding-box copy can never spill
        // onto anything that was still valid.
        const RECT& u = m_update;
        BitBlt(m_wdc, u.left, u.top, u.right - u.left, u.bottom - u.top,
               m_mem, u.left, u.top, SRCCOPY);
    }
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
