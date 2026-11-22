#include "airui/Widgets.h"
#include "airui/Shell.h"
#include "airui/Gfx.h"
#include "airui/Accessibility.h"
#include "airui/PaintBackend.h"
#include <commctrl.h>
#include <imm.h>
#include <cstdarg>
#include <cstdio>
#include <memory>

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

// 局部夹取（core 的 ClampInt 未导出，这里是同一语义的最小副本）
static int ClampV(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

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
// TextBox 辅助：剪贴板 / IME 窗口 / 逐字宽度
// ============================================================
namespace {

// 系统剪贴板的 IClipboard 适配（Copy/Cut/Paste 复用 core 的 EditAction）。
class Win32Clipboard final : public IClipboard
{
public:
    std::wstring Read() override
    {
        std::wstring out;
        if (!OpenClipboard(nullptr)) return out;
        if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
            if (const wchar_t* p = (const wchar_t*)GlobalLock(h)) {
                out = p;
                GlobalUnlock(h);
            }
        }
        CloseClipboard();
        return out;
    }
    void Write(const std::wstring& text) override
    {
        if (!OpenClipboard(nullptr)) return;
        EmptyClipboard();
        const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
        if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (void* p = GlobalLock(h)) {
                memcpy(p, text.c_str(), bytes);
                GlobalUnlock(h);
                SetClipboardData(CF_UNICODETEXT, h);
            } else {
                GlobalFree(h);
            }
        }
        CloseClipboard();
    }
};

// 组词串 + 候选窗跟随插入点（坐标是输入框客户区）。
void SyncImeWindow(HWND hwnd, int caretX, int caretY,
                   int zx, int zy, int zr, int zb)
{
    HIMC imc = ImmGetContext(hwnd);
    if (!imc) return;
    COMPOSITIONFORM cf{};
    cf.dwStyle        = CFS_POINT;
    cf.ptCurrentPos.x = caretX;
    cf.ptCurrentPos.y = caretY;
    ImmSetCompositionWindow(imc, &cf);

    CANDIDATEFORM cand{};
    cand.dwIndex        = 0;
    cand.dwStyle        = CFS_CANDIDATEPOS;
    cand.ptCurrentPos.x = caretX;
    cand.ptCurrentPos.y = caretY;
    cand.rcArea.left    = zx;
    cand.rcArea.top     = zy;
    cand.rcArea.right   = zr;
    cand.rcArea.bottom  = zb;
    ImmSetCandidateWindow(imc, &cand);
    ImmReleaseContext(hwnd, imc);
}

} // namespace

// ============================================================
// TextBox
// ============================================================
void TextBox::CreateNative()
{
    HWND parent = m_shell->Hwnd();

    // 自绘窗口类：所有编辑逻辑（组词/光标/选区/折行）由 TextBoxProc 与
    // core 引擎承担，系统只提供一个收键盘/焦点的宿主。类名 Edit 仍可
    // 被 test_ui 识别？——不能复用（user32 已注册真 Edit，RegisterClass 会
    // 失败），改为 AirUITextBox + 测试端 IsCtrl 别名（表：向后兼容）。
    static const bool s_reg = [] {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.style          = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = DefWindowProcW;
        wc.hInstance      = GetModuleHandleW(nullptr);
        wc.hCursor        = LoadCursorW(nullptr, (LPCWSTR)IDC_IBEAM);
        wc.hbrBackground  = nullptr;
        wc.lpszClassName  = L"AirUITextBox";
        return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }();
    if (!s_reg) return;

    m_hwnd = CreateWindowExW(0, L"AirUITextBox", L"",
                             WS_CHILD | WS_TABSTOP | WS_VISIBLE,
                             0, 0, 0, 0, parent,
                             (HMENU)(INT_PTR)m_id, nullptr, nullptr);
    if (!m_hwnd) return;

    m_font = m_shell ? m_shell->UiFonts().Get(m_desc.fontRole) : nullptr;
    m_edit.hasFocus = false;
    SyncTextLayout();
    SetWindowSubclass(m_hwnd, &TextBoxProc, kSubTextBox, (DWORD_PTR)this);
}

LRESULT CALLBACK TextBoxProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam, UINT_PTR idSub,
                                    DWORD_PTR ref)
{
    auto* self = (TextBox*)ref;

    // ---- 消息级探测（跨 TU 入口：App::FocusSearch 全选、ManualEntry
    //      插件的 GetWindowTextStr/SetWindowTextW、test_ui 的 TypeText）。
    //      页面代码一律 Text()/SetText()/OnKey，不依赖这些。 ----
    if (msg == WM_GETTEXT && self) {
        wchar_t* out = (wchar_t*)lParam;
        const int cap = (int)wParam;
        if (!out || cap <= 0) return 0;
        const int len = (int)self->m_edit.text.size();
        const int n = (len > cap - 1) ? cap - 1 : len;
        if (n > 0) memcpy(out, self->m_edit.text.c_str(), n * sizeof(wchar_t));
        out[n] = L'\0';
        return (LRESULT)n;
    }
    if (msg == WM_GETTEXTLENGTH && self) return (LRESULT)self->m_edit.text.size();
    if (msg == WM_SETTEXT && self) {
        self->SetText(lParam ? std::wstring((const wchar_t*)lParam) : std::wstring());
        return 0;
    }
    if (msg == EM_SETSEL && self) {
        // wParam=0,lParam=-1 是「全选」（FocusSearch 走这个）；直接改
        // 引擎的选区状态，不经过任何原生 EM_SETSEL 语义。
        const int len = (int)self->m_edit.text.size();
        if ((int)(short)wParam == 0 && (short)lParam == -1) {
            self->m_edit.selectionAnchor = 0;
            self->m_edit.cursor = len;
        } else {
            self->m_edit.selectionAnchor = ClampV((int)(short)wParam, 0, len);
            self->m_edit.cursor = ClampV((int)(short)lParam, 0, len);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (msg == EM_SETMARGINS && self) {
        self->m_rightInset = (int)HIWORD(lParam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    switch (msg) {
    case WM_KEYDOWN:
        if (self) {
            const bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (self->HandleKey((UINT)wParam, ctrl, shift)) { self->ApplyPostEdit(); return 0; }
            if (self->DefaultKey((UINT)wParam, ctrl, shift)) { self->ApplyPostEdit(); return 0; }
        }
        break;   // 未消费（Tab 等）→ 默认处理
    case WM_GETOBJECT:
        // 被 SetAccessible* 标注过才接管；否则 EDIT 继续用系统原生
        // provider（其 Text/Value/Selection 支持比薄包装更完整）。
        if (LRESULT acc = a11y::HandleGetObjectIfAnnotated(hwnd, wParam, lParam))
            return acc;
        break;
    case WM_SETFOCUS:
        if (self) {
            self->m_edit.hasFocus = true;
            self->SyncIme();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_KILLFOCUS:
        if (self) {
            self->m_edit.hasFocus = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_CHAR:
        if (self) {
            const wchar_t ch = (wchar_t)wParam;
            // 控制字符（回车/退格/Delete/Ctrl+字母产生的 0x01..0x1F）不
            // 走 WM_CHAR：编辑语义键已由 WM_KEYDOWN 处理，这里再插入会
            // 双写。仅 Tab 例外（多行缩进仍不插，交给焦点系统）。
            if (ch >= L' ' && ch != 0x7F && !self->m_composing &&
                !(GetKeyState(VK_CONTROL) & 0x8000))
                self->InsertChar(ch);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (self && self->m_desc.multiline) {
            const int delta = GET_WHEEL_DELTA_WPARAM(wParam) / 120;
            self->m_scroll = ClampV(self->m_scroll - delta * 30, 0,
                                    self->MaxScroll());
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (self) self->CaretAtClientPoint(POINT{ (short)LOWORD(lParam),
                                                  (short)HIWORD(lParam) });
        return 0;
    case WM_IME_STARTCOMPOSITION:
        if (self) {
            self->m_composing = true;
            self->m_edit.composition.clear();
            self->SyncIme();
        }
        return 0;   // 预绘位置由 SyncIme 布置，无须系统默认参与
    case WM_IME_COMPOSITION:
        if (self && lParam) {
            HIMC imc = ImmGetContext(hwnd);
            if (imc) {
                if (lParam & GCS_RESULTSTR) {
                    const LONG n = ImmGetCompositionStringW(imc, GCS_RESULTSTR,
                                                            nullptr, 0);
                    if (n > 0) {
                        std::wstring result((size_t)n / sizeof(wchar_t), L'\0');
                        ImmGetCompositionStringW(imc, GCS_RESULTSTR, &result[0], n);
                        self->InsertText(result);
                    }
                    self->m_composing = false;
                    self->m_edit.composition.clear();
                    self->ApplyPostEdit();
                    ImmReleaseContext(hwnd, imc);
                    return 0;
                }
                if (lParam & GCS_COMPSTR) {
                    const LONG n = ImmGetCompositionStringW(imc, GCS_COMPSTR,
                                                            nullptr, 0);
                    std::wstring comp((size_t)n / sizeof(wchar_t), L'\0');
                    if (n > 0)
                        ImmGetCompositionStringW(imc, GCS_COMPSTR, &comp[0], n);
                    self->m_edit.composition = std::move(comp);
                    self->m_edit.compositionCursor =
                        (int)self->m_edit.composition.size();
                    self->SyncIme();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    ImmReleaseContext(hwnd, imc);
                    return 0;
                }
                ImmReleaseContext(hwnd, imc);
            }
        }
        break;
    case WM_IME_ENDCOMPOSITION:
        if (self) {
            self->m_composing = false;
            self->m_edit.composition.clear();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (self) self->PaintSelf(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;   // 背景在 WM_PAINT 里整块画（自绘无闪烁）
    case WM_NCDESTROY:
        a11y::ForgetWindow(hwnd);   // 清无障碍属性表（幂等）
        RemoveWindowSubclass(hwnd, &TextBoxProc, idSub);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

bool TextBox::HandleKey(UINT vk, bool ctrl, bool shift)
{
    return m_keyCb ? m_keyCb(vk, ctrl, shift) : false;
}

// ---- 公共接口：全部直接操作 core 引擎（不再走原生 EDIT 消息） ----

std::wstring TextBox::Text() const
{
    return m_edit.text;
}

void TextBox::SetText(const std::wstring& s)
{
    m_edit.text = s;
    m_edit.cursor = (int)s.size();
    m_edit.selectionAnchor = -1;
    m_edit.composition.clear();
    m_edit.compositionCursor = 0;
    m_layW = -1;                  // 强制重建 cum/lines（同长替换也要重算）
    ApplyPostEdit();
}

void TextBox::Clear()
{
    SetText(std::wstring());
}

void TextBox::CaretToEnd()
{
    m_edit.cursor = (int)m_edit.text.size();
    m_edit.selectionAnchor = -1;
    ApplyPostEdit();
}

void TextBox::SetPlaceholder(const std::wstring& s)
{
    m_desc.placeholder = s;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TextBox::OnKey(std::function<bool(UINT, bool, bool)> cb)
{
    m_keyCb = std::move(cb);
}

void TextBox::SetRightInset(int px)
{
    m_rightInset = px;
    m_layW = -1;
    if (m_hwnd) ApplyPostEdit();
}

void TextBox::ClickAt(POINT parentPt)
{
    if (!m_hwnd) return;
    POINT p = parentPt;
    MapWindowPoints(GetParent(m_hwnd), m_hwnd, &p, 1);
    SetFocus(m_hwnd);
    CaretAtClientPoint(p);
}

// ============================================================
// 自绘实现
// ============================================================

// 逐字前缀宽度表：cum[i] = width(text[0..i))。整前缀测量（逐字累加会
// 漂），与 GDI 绘制同源：同一 HFONT + GetTextExtentPoint32W。
std::vector<int> MeasureCum(HDC dc, HFONT font, const std::wstring& s)
{
    std::vector<int> cum;
    cum.reserve(s.size() + 1);
    cum.push_back(0);
    if (!dc || !font || s.empty()) {
        for (size_t i = 0; i < s.size(); ++i) cum.push_back(0);
        return cum;
    }
    HGDIOBJ old = SelectObject(dc, font);
    for (size_t i = 0; i < s.size(); ++i) {
        SIZE sz{};
        if (GetTextExtentPoint32W(dc, s.c_str(), (int)(i + 1), &sz))
            cum.push_back(sz.cx);
        else
            cum.push_back(cum.back());
    }
    SelectObject(dc, old);
    return cum;
}

void TextBox::SyncTextLayout()
{
    if (!m_shell || !m_shell->UiFonts().Valid()) return;
    m_font = m_shell->UiFonts().Get(m_desc.fontRole);

    RECT rc{};
    if (m_hwnd) GetClientRect(m_hwnd, &rc);
    const int availW = (int)(rc.right - rc.left) - Mx().unit * 2 - m_rightInset;
    const int availH = (int)(rc.bottom - rc.top);
    const bool same = (m_layW == availW && m_layH == availH &&
                       (int)m_cum.size() == (int)m_edit.text.size() + 1);
    if (same) return;
    m_layW = availW;
    m_layH = availH;

    HDC dc = GetDC(nullptr);
    m_cum = MeasureCum(dc, m_font, m_edit.text);
    int lineH = 0, ascent = 0;
    if (dc && m_font) {
        HGDIOBJ old = SelectObject(dc, m_font);
        TEXTMETRICW tm{};
        if (GetTextMetricsW(dc, &tm)) {
            lineH  = tm.tmHeight + tm.tmExternalLeading;
            ascent = tm.tmAscent;
        }
        SelectObject(dc, old);
    }
    ReleaseDC(nullptr, dc);
    if (lineH > 0) m_lineH = lineH;
    if (ascent > 0) m_ascent = ascent;

    // 多行折行；单行不折（avail = 无限），由 PaintTextBox 处理横向滚动。
    m_lines = WrapText(m_edit.text, m_cum,
                       m_desc.multiline ? availW : 0x7fffffff, m_lineH);
    m_scroll = ClampV(m_scroll, 0, MaxScroll());
}

int TextBox::MaxScroll() const
{
    if (!m_desc.multiline) return 0;
    const int max = m_lines.contentH - m_layH;
    return max > 0 ? max : 0;
}

void TextBox::ScrollForCaret()
{
    if (!m_desc.multiline) return;
    m_scroll = ScrollToCaret(m_lines, m_cum, m_edit.cursor, m_scroll,
                             m_layH, m_lineH);
}

POINT TextBox::CaretPoint() const
{
    POINT p{ 0, 0 };
    if (!m_hwnd) return p;
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    const int unit = Mx().unit;
    const int n = (int)m_edit.text.size();
    const bool haveCum = ((int)m_cum.size() == n + 1);
    const int caret = ClampV(m_edit.cursor, 0, n);
    if (m_desc.multiline) {
        if (!haveCum || m_lines.begin.empty()) {
            p.x = unit;
            p.y = unit;
            return p;
        }
        const int line = LineOfIndex(m_lines, caret);
        const int lb = m_lines.begin[line];
        p.x = unit + (caret >= lb ? m_cum[caret] - m_cum[lb] : 0);
        p.y = unit + line * m_lineH - m_scroll;
        return p;
    }
    // 单行：文本在框内垂直居中（PaintTextBox 用 DT 单行对齐）。
    p.x = unit + (haveCum ? m_cum[caret] : 0);
    p.y = (int)(rc.bottom - rc.top - m_lineH) / 2;
    if (p.y < 0) p.y = 0;
    return p;
}

void TextBox::SyncIme()
{
    if (!m_hwnd) return;
    const POINT c = CaretPoint();
    // 候选窗区域：整块文本区（caret 行的上下沿）。
    int zx = Mx().unit;
    int zy = c.y;
    int zr = Mx().unit + m_layW;
    int zb = c.y + m_lineH;
    if (zr < zx + 1) zr = zx + 1;
    if (zb < zy + 1) zb = zy + 1;
    SyncImeWindow(m_hwnd, c.x, c.y, zx, zy, zr, zb);
}

void TextBox::ApplyEditResult(EditAction action, const std::wstring& arg,
                              IClipboard* clip)
{
    const EditResult r = air::ui::ApplyEdit(m_edit, action, arg, clip);
    if (r.consumed) ApplyPostEdit();
}

void TextBox::ApplyPostEdit()
{
    m_layW = -1;                 // 文本可能同长替换：一律重建布局
    SyncTextLayout();
    ScrollForCaret();
    SyncIme();
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TextBox::InsertChar(wchar_t ch)
{
    if (ch == L'\r') return;     // 回车由 WM_KEYDOWN/VK_RETURN 处理
    Win32Clipboard clip;
    ApplyEditResult(EditAction::Insert, std::wstring(1, ch), &clip);
}

void TextBox::InsertText(const std::wstring& s)
{
    if (s.empty()) return;
    Win32Clipboard clip;
    ApplyEditResult(EditAction::Insert, s, &clip);
}

void TextBox::CaretAtClientPoint(POINT pt)
{
    SyncTextLayout();
    const int n = (int)m_edit.text.size();
    if (!m_edit.hasFocus) SetFocus(m_hwnd);
    m_edit.hasFocus = true;

    const int unit = Mx().unit;
    if (m_desc.multiline) {
        const int lineH = m_lineH > 0 ? m_lineH : 1;
        int line = (pt.y - unit + m_scroll) / lineH;
        line = ClampV(line, 0, (int)m_lines.begin.size() - 1);
        if (line >= 0 && line < (int)m_lines.begin.size()) {
            m_edit.cursor = IndexAtX(m_edit.text, m_cum, m_lines.begin[line],
                                     m_lines.end[line], pt.x - unit);
        } else {
            m_edit.cursor = n;
        }
    } else {
        m_edit.cursor = IndexAtX(m_edit.text, m_cum, 0, n, pt.x - unit);
    }
    if (GetKeyState(VK_SHIFT) & 0x8000) {
        if (m_edit.selectionAnchor < 0) m_edit.selectionAnchor = m_edit.cursor;
    } else {
        m_edit.selectionAnchor = -1;
    }
    ApplyPostEdit();
}

bool TextBox::DefaultKey(UINT vk, bool ctrl, bool shift)
{
    const int n = (int)m_edit.text.size();
    const int prev = m_edit.cursor;
    auto moveTo = [&](int idx) {
        m_edit.cursor = ClampV(idx, 0, n);
        if (!shift && m_edit.selectionAnchor >= 0)
            m_edit.selectionAnchor = -1;
        return m_edit.cursor != prev;
    };

    if (ctrl) {
        switch (vk) {
        case 'A':
            m_edit.selectionAnchor = 0;
            m_edit.cursor = n;
            return true;
        case 'C': case 'X': {
            Win32Clipboard clip;
            const EditResult r = air::ui::ApplyEdit(
                m_edit, vk == 'C' ? EditAction::Copy : EditAction::Cut, L"", &clip);
            return r.consumed;
        }
        case 'V': {
            Win32Clipboard clip;
            InsertText(clip.Read());
            return true;
        }
        default:
            return false;
        }
    }

    switch (vk) {
    case VK_LEFT: {
        int idx = prev - 1;
        if (HasSelection(m_edit) && !shift) idx = SelectionBegin(m_edit);
        return moveTo(idx);
    }
    case VK_RIGHT: {
        int idx = prev + 1;
        if (HasSelection(m_edit) && !shift) idx = SelectionEnd(m_edit);
        return moveTo(idx);
    }
    case VK_UP:
    case VK_DOWN:
        if (!m_desc.multiline || m_edit.text.empty()) return false;
        return moveTo(MoveVertically(m_edit.text, m_cum, m_lines, m_edit.cursor,
                                     vk == VK_UP ? -1 : +1));
    case VK_HOME:
        if (m_desc.multiline && !m_lines.begin.empty()) {
            const int line = LineOfIndex(m_lines, m_edit.cursor);
            return moveTo(m_lines.begin[line]);
        }
        return moveTo(0);
    case VK_END:
        if (m_desc.multiline && !m_lines.begin.empty()) {
            const int line = LineOfIndex(m_lines, m_edit.cursor);
            return moveTo(m_lines.end[line]);
        }
        return moveTo(n);
    case VK_BACK:
        ApplyEditResult(EditAction::Backspace, L"", nullptr);
        return true;
    case VK_DELETE:
        ApplyEditResult(EditAction::Delete, L"", nullptr);
        return true;
    case VK_RETURN:
        if (m_desc.multiline) {
            InsertText(L"\n");
            return true;
        }
        return false;
    default:
        return false;
    }
}

// 单行直接用 core 的 PaintTextBox（外框/占位/选区/光标同一套设计）；
// 多行用同一组 IPainter 原语按 core 的折行结果逐行直排。
void TextBox::PaintSelf(HDC dc)
{
    if (!m_shell || !dc) return;
    SyncTextLayout();

    RECT cr{};
    GetClientRect(m_hwnd, &cr);
    const ::air::ui::Rect frame(cr.left, cr.top, cr.right, cr.bottom);
    if (frame.Empty()) return;

    PaintTarget target{ dc, frame };
    std::unique_ptr<IPainter> g =
        PainterRegistry::Instance().Begin("gdi", target);
    if (!g) return;
    const Palette& pal = Pal();
    const Metrics& mx = Mx();

    if (!m_desc.multiline) {
        TextBoxState st;
        st.edit = m_edit;
        st.placeholder = m_desc.placeholder;
        st.base.focused = m_edit.hasFocus;
        PaintTextBox(*g, frame, st, pal, mx);
        return;
    }

    // ---- 多行：外框 + 行内选区 + 逐行文本 + 组词串 + 插入符 ----
    g->FillRound(frame, mx.radiusMd, pal.surface);
    g->StrokeRound(frame, mx.radiusMd,
                   m_edit.hasFocus ? pal.primary : pal.border, 1);

    const int unit = mx.unit;
    const ::air::ui::Rect textRc(frame.left + unit, frame.top + unit,
                                 frame.right - unit - m_rightInset,
                                 frame.bottom - unit);
    if (textRc.Empty()) return;
    g->PushClip(textRc);

    const bool composing = !m_edit.composition.empty();
    if (m_edit.text.empty() && !composing) {
        const std::wstring ph = m_desc.placeholder;
        if (!ph.empty())
            g->Text(textRc, ph.c_str(), ph.size(), m_desc.fontRole,
                    pal.textFaint, dt::Left | dt::Top | dt::SingleLine);
        g->PopClip();
        return;
    }

    const int lineH = m_lineH > 0 ? m_lineH : 1;
    if (HasSelection(m_edit)) {
        const int a = SelectionBegin(m_edit), b = SelectionEnd(m_edit);
        for (size_t li = 0; li < m_lines.begin.size(); ++li) {
            const int lb = m_lines.begin[li], le = m_lines.end[li];
            if (le <= a || lb >= b) continue;
            const int s = (std::max)(lb, a), e = (std::min)(le, b);
            if (e <= s) continue;
            const int x0 = textRc.left + (m_cum[s] - m_cum[lb]);
            const int x1 = textRc.left + (m_cum[e] - m_cum[lb]);
            const int y  = textRc.top + (int)li * lineH - m_scroll;
            if (x1 > x0)
                g->FillRect(::air::ui::Rect(x0, y, x1, y + lineH), pal.selectFill);
        }
    }

    const Color ink = m_edit.hasFocus ? pal.text : pal.textSub;
    for (size_t li = 0; li < m_lines.begin.size(); ++li) {
        const int lb = m_lines.begin[li], le = m_lines.end[li];
        if (le <= lb) continue;
        const int y = textRc.top + (int)li * lineH - m_scroll;
        if (y >= textRc.bottom || y + lineH <= textRc.top) continue;
        g->Text(::air::ui::Rect(textRc.left, y, textRc.right, y + lineH),
                m_edit.text.c_str() + lb, (size_t)(le - lb), m_desc.fontRole,
                ink, dt::Left | dt::Top | dt::SingleLine);
    }

    if (composing) {
        const POINT c = CaretPoint();
        const int y = textRc.top + c.y;
        g->Text(::air::ui::Rect(textRc.left + c.x, y, textRc.right, y + lineH),
                m_edit.composition.c_str(), m_edit.composition.size(),
                m_desc.fontRole, pal.primaryText,
                dt::Left | dt::Top | dt::SingleLine);
    }
    if (m_edit.hasFocus && !composing) {
        const POINT c = CaretPoint();
        const int y = textRc.top + c.y;
        if (y + lineH > textRc.top && y < textRc.bottom)
            g->FillRect(::air::ui::Rect(textRc.left + c.x, y,
                                        textRc.left + c.x + 1, y + lineH),
                        pal.primary);
    }
    g->PopClip();
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

    // 自绘窗口：WS_VSCROLL 让 DefWindowProc 画标准滚动条（非客户区），
    // 滚动范围/位置由 SyncScrollBar 维护；条目绘制、命中、选中、键盘与
    // 滚轮全部在本文件实现，不再依赖 LISTBOX 类的行为。
    static const bool s_reg = [] {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = DefWindowProcW;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.hCursor       = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"AirUIListBox";
        return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }();
    if (!s_reg) return;

    m_hwnd = CreateWindowExW(0, L"AirUIListBox", L"",
        WS_CHILD | WS_VSCROLL | WS_TABSTOP | WS_VISIBLE,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)m_id, nullptr, nullptr);
    if (!m_hwnd) {
        HoverLog(L"listbox create FAILED gle=%lu", GetLastError());
        return;
    }

    SetWindowSubclass(m_hwnd, &ListBoxProc, kSubListBox, (DWORD_PTR)this);
}

LRESULT CALLBACK ListBoxProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam, UINT_PTR idSub,
                                    DWORD_PTR ref)
{
    auto* self = (ListBox*)ref;

    // ---- 消息级兼容（test_ui / 外部探针按原生 LB_* 读取） ----
    // 条目模型在本类内；这些消息映射到 m_items/m_sel，语义保持与原生
    // 一致，但不再触发任何原生 LISTBOX 代码。
    if (self) {
        const int n = (int)self->m_items.size();
        switch (msg) {
        case LB_GETCOUNT:
            return n;
        case LB_GETCURSEL:
            return self->m_sel;
        case LB_SETCURSEL:
            self->SetSelection((int)wParam);
            return self->m_sel;
        case LB_GETITEMRECT:
            if ((int)wParam < 0 || (int)wParam >= n) return LB_ERR;
            if (RECT* out = (RECT*)lParam) *out = self->ItemRect((int)wParam);
            return LB_OKAY;
        case LB_GETITEMDATA:
            if ((int)wParam < 0 || (int)wParam >= n) return LB_ERR;
            return (LRESULT)self->m_items[(size_t)wParam].data;
        case LB_SETITEMDATA:
            if ((int)wParam < 0 || (int)wParam >= n) return LB_ERR;
            self->m_items[(size_t)wParam].data = (size_t)lParam;
            return LB_OKAY;
        case LB_SETITEMHEIGHT:
            if ((int)wParam < 0 || (int)wParam >= n) return LB_ERR;
            self->m_items[(size_t)wParam].height = (int)LOWORD(lParam);
            return LB_OKAY;
        case LB_GETITEMHEIGHT:
            if ((int)wParam < 0 || (int)wParam >= n) return LB_ERR;
            return self->m_items[(size_t)wParam].height;
        case LB_ADDSTRING:
            return self->AddItem(self->m_fallbackH, 0);
        case LB_RESETCONTENT:
            self->Reset();
            return 0;
        case LB_ITEMFROMPOINT: {
            const POINT pt = { (LONG)(short)LOWORD(lParam),
                               (LONG)(short)HIWORD(lParam) };
            const int idx = self->ItemAtPoint(pt);
            return idx < 0 ? (LRESULT)(DWORD)-1 : (LRESULT)(DWORD)idx;
        }
        case LB_GETTEXTLEN:
        case LB_GETTEXT:
            return 0;   // 自绘列表不存字符串（文本由 onPaintItem 画）
        default:
            break;
        }
    }

    switch (msg) {
    case WM_SIZE:
        if (self) {
            self->SyncScrollBar();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (self) self->PaintSelf(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;   // 背景由 PaintSelf 整块覆盖（单帧无闪烁）
    case WM_LBUTTONDOWN: {
        if (!self) break;
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        SetFocus(hwnd);
        const int item = self->ItemAtPoint(pt);
        if (item >= 0) {
            if (self->onItemClicked) {
                const RECT rc = self->ItemRect(item);
                POINT ptIn = { pt.x - rc.left, pt.y - rc.top };
                if (self->onItemClicked(item, ptIn)) return 0;
            }
            self->SelectItem(item, true);
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        if (!self) break;
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        const int item = self->ItemAtPoint(pt);
        if (item >= 0) {
            self->SelectItem(item, false);
            SendMessageW(GetParent(hwnd), WM_COMMAND,
                         MAKEWPARAM(GetDlgCtrlID(hwnd), LBN_DBLCLK),
                         (LPARAM)hwnd);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!self) break;
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        const int item = self->ItemAtPoint(pt);
        if (item != self->m_hover) {
            const int old = self->m_hover;
            self->m_hover = item;
            if (old >= 0) self->InvalidateItem(old);
            if (item >= 0) self->InvalidateItem(item);
        }
        if (self->onHoverMove) {
            POINT in = { pt.x, pt.y };
            if (item >= 0) {
                const RECT irc = self->ItemRect(item);
                in.x -= irc.left;
                in.y -= irc.top;
            }
            self->onHoverMove(item, in);
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (!self) break;
        const int old = self->m_hover;
        self->m_hover = -1;
        if (old >= 0) self->InvalidateItem(old);
        if (self->onHoverMove) {
            POINT out = { -1, -1 };
            self->onHoverMove(-1, out);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (!self) break;
        const int notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        self->m_scroll = ClampV(self->m_scroll - notches * self->ScrollStep(),
                                0, self->MaxScroll());
        self->SyncScrollBar();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_VSCROLL: {
        if (!self) break;
        const int page = self->VisibleHeight();
        const int step = self->ScrollStep() / 3 > 0 ? self->ScrollStep() / 3 : 1;
        int s = self->m_scroll;
        switch (LOWORD(wParam)) {
        case SB_LINEUP:    s -= step; break;
        case SB_LINEDOWN:  s += step; break;
        case SB_PAGEUP:    s -= page; break;
        case SB_PAGEDOWN:  s += page; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: s = (int)HIWORD(wParam); break;
        case SB_TOP:       s = 0; break;
        case SB_BOTTOM:    s = self->MaxScroll(); break;
        default: return 0;
        }
        s = ClampV(s, 0, self->MaxScroll());
        if (s != self->m_scroll) {
            self->m_scroll = s;
            self->SyncScrollBar();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (!self) break;
        const int n = (int)self->m_items.size();
        if (n <= 0) return 0;
        int sel = self->m_sel;
        int page = self->VisibleHeight() /
                   (self->m_fallbackH > 0 ? self->m_fallbackH : 1);
        if (page < 1) page = 1;
        switch (wParam) {
        case VK_UP:    sel = (sel <= 0) ? 0 : sel - 1; break;
        case VK_DOWN:  sel = (sel < 0) ? 0 : (sel + 1 < n ? sel + 1 : n - 1); break;
        case VK_HOME:  sel = 0; break;
        case VK_END:   sel = n - 1; break;
        case VK_PRIOR: sel = (sel < 0 ? 0 : sel) - page; if (sel < 0) sel = 0; break;
        case VK_NEXT:  sel = (sel < 0 ? 0 : sel) + page;
                       if (sel > n - 1) sel = n - 1; break;
        default: return 0;   // 其它键交默认处理（Tab/加速键等）
        }
        self->SelectItem(sel, true);
        return 0;
    }
    case WM_CONTEXTMENU: {
        if (!self || !self->onContextMenu) break;
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int item = -1;
        if (pt.x != -1 || pt.y != -1) {
            POINT lpt = pt;
            ScreenToClient(hwnd, &lpt);
            item = self->ItemAtPoint(lpt);
        } else {
            item = self->m_sel;
            if (item < 0) return 0;
            const RECT rc = self->ItemRect(item);
            pt = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
            MapWindowPoints(hwnd, HWND_DESKTOP, (LPPOINT)&pt, 1);
        }
        self->onContextMenu(pt, item);
        return 0;
    }
    case WM_GETOBJECT:
        // 被 SetAccessible* 标注过才接管；否则 LISTBOX 继续用系统原生
        // provider（Item selection / focus 支持比薄包装更完整）。
        if (LRESULT acc = a11y::HandleGetObjectIfAnnotated(hwnd, wParam, lParam))
            return acc;
        break;
    case WM_NCDESTROY:
        a11y::ForgetWindow(hwnd);   // 清无障碍属性表（幂等）
        RemoveWindowSubclass(hwnd, &ListBoxProc, idSub);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void ListBox::Reset()
{
    m_items.clear();
    m_sel = -1;
    m_hover = -1;
    m_scroll = 0;
    if (m_hwnd) {
        SyncScrollBar();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void ListBox::BeginUpdate()
{
    ++m_updateLock;
}

void ListBox::EndUpdate()
{
    if (m_updateLock == 0) return;
    if (--m_updateLock > 0) return;
    if (m_hwnd) {
        SyncScrollBar();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

int ListBox::AddItem(int height, size_t data)
{
    if (height < 1) height = m_fallbackH > 0 ? m_fallbackH : 1;
    m_items.push_back(ListBoxItem{ height, data });
    const int idx = (int)m_items.size() - 1;
    if (m_hwnd && m_updateLock == 0) {
        SyncScrollBar();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    return idx;
}

int ListBox::Count()
{
    return (int)m_items.size();
}

int ListBox::Selection()
{
    return m_sel;
}

void ListBox::SetSelection(int index)
{
    if (index < -1) index = -1;
    if (index >= (int)m_items.size()) index = (int)m_items.size() - 1;
    m_sel = index;
    if (index >= 0) EnsureVisible(index);
    if (m_hwnd) {
        SyncScrollBar();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

RECT ListBox::ItemRect(int index)
{
    RECT rc = {};
    if (!m_hwnd || index < 0 || index >= (int)m_items.size()) return rc;
    RECT cr = {};
    GetClientRect(m_hwnd, &cr);
    int y = -m_scroll;
    for (int i = 0; i < index; ++i) y += m_items[(size_t)i].height;
    rc.left   = 0;
    rc.top    = y;
    rc.right  = cr.right;
    rc.bottom = y + m_items[(size_t)index].height;
    return rc;
}

void ListBox::InvalidateItem(int index)
{
    if (!m_hwnd) return;
    if (index < 0 || index >= (int)m_items.size()) return;
    const RECT rc = ItemRect(index);
    InvalidateRect(m_hwnd, &rc, FALSE);
}

void ListBox::Invalidate()
{
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

int ListBox::ItemAtPoint(POINT clientPt)
{
    if (!m_hwnd) return -1;
    RECT cr = {};
    GetClientRect(m_hwnd, &cr);
    if (clientPt.x < cr.left || clientPt.x >= cr.right ||
        clientPt.y < cr.top  || clientPt.y >= cr.bottom)
        return -1;
    int y = -m_scroll;
    for (size_t i = 0; i < m_items.size(); ++i) {
        const int h = m_items[i].height;
        if (clientPt.y >= y && clientPt.y < y + h) return (int)i;
        y += h;
    }
    return -1;
}

// ---- 自绘内部 ----

int ListBox::ContentHeight() const
{
    int h = 0;
    for (const ListBoxItem& it : m_items) h += it.height;
    return h;
}

int ListBox::VisibleHeight() const
{
    RECT cr = {};
    if (m_hwnd) GetClientRect(m_hwnd, &cr);
    return (int)(cr.bottom - cr.top);
}

int ListBox::ScrollStep() const
{
    const int u = Mx().unit;
    return u > 0 ? u * 8 : 64;      // 一格滚轮 ≈ 一屏的 1/5（96dpi: 64px）
}

int ListBox::MaxScroll() const
{
    const int max = ContentHeight() - VisibleHeight();
    return max > 0 ? max : 0;
}

void ListBox::SyncScrollBar()
{
    if (!m_hwnd) return;
    const int content = ContentHeight();
    const int view    = VisibleHeight();
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = content > 0 ? content - 1 : 0;
    si.nPage  = (UINT)(view > 0 ? view : 1);
    si.nPos   = m_scroll;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

void ListBox::EnsureVisible(int index)
{
    if (index < 0 || index >= (int)m_items.size()) return;
    const int view = VisibleHeight();
    if (view <= 0) return;
    int top = 0;
    for (int i = 0; i < index; ++i) top += m_items[(size_t)i].height;
    const int bottom = top + m_items[(size_t)index].height;
    if (top < m_scroll) m_scroll = top;
    else if (bottom > m_scroll + view) m_scroll = bottom - view;
    m_scroll = ClampV(m_scroll, 0, MaxScroll());
}

void ListBox::SelectItem(int index, bool notify)
{
    if (index < 0 || index >= (int)m_items.size()) return;
    const int old = m_sel;
    if (old == index) return;
    m_sel = index;
    EnsureVisible(index);
    if (m_hwnd) {
        SyncScrollBar();
        if (old >= 0) InvalidateItem(old);
        InvalidateItem(index);
    }
    if (notify && m_hwnd) {
        SendMessageW(GetParent(m_hwnd), WM_COMMAND,
                     MAKEWPARAM(GetDlgCtrlID(m_hwnd), LBN_SELCHANGE),
                     (LPARAM)m_hwnd);
    }
}

void ListBox::PaintItem(Canvas& g, int index, const RECT& rc)
{
    if (index < 0 || index >= (int)m_items.size()) return;
    // 行底 = 页面灰（卡片浮于其上），与旧实现保持一致
    g.Fill(rc, Pal().bg);
    if (!onPaintItem) return;
    ListItemDraw it;
    it.index    = index;
    it.data     = m_items[(size_t)index].data;
    it.rc       = rc;
    it.selected = (index == m_sel);
    it.hovered  = (index == m_hover);
    onPaintItem(g, it);
}

void ListBox::PaintSelf(HDC dc)
{
    if (!m_shell || !dc) return;
    RECT rc = {};
    GetClientRect(m_hwnd, &rc);
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;

    gfx::Fill(dc, rc, Pal().bg);
    Canvas g(dc, m_shell->UiFonts());

    if (m_items.empty()) {
        if (onPaintEmpty) onPaintEmpty(g, rc);
        return;
    }

    m_scroll = ClampV(m_scroll, 0, MaxScroll());
    int y = -m_scroll;
    for (size_t i = 0; i < m_items.size(); ++i) {
        const int h = m_items[i].height;
        RECT row = { 0, y, rc.right, y + h };
        y += h;
        if (row.bottom <= rc.top) continue;
        if (row.top >= rc.bottom) break;
        PaintItem(g, (int)i, row);
    }
}

bool ListBox::HandleCommand(int code)
{
    if (code == LBN_SELCHANGE) {
        if (onSelectChanged && m_sel >= 0) onSelectChanged(m_sel);
        return true;
    }
    if (code == LBN_DBLCLK) {
        if (onDoubleClicked && m_sel >= 0) onDoubleClicked(m_sel);
        return true;
    }
    return false;
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
    case WM_GETOBJECT:
        // 被 SetAccessible* 标注过才接管；STATIC 无原生 a11y provider，
        // 未标注时落回 DefWindowProc 的系统默认对象。
        if (LRESULT acc = a11y::HandleGetObjectIfAnnotated(hwnd, wParam, lParam))
            return acc;
        break;
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
        a11y::ForgetWindow(hwnd);   // 清无障碍属性表（幂等）
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
