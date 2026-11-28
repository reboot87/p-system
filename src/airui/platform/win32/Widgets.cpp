#include "internal/Widgets.h"
#include "internal/Controls.h"
#include "internal/Shell.h"
#include "internal/OverlayWindow.h"   // RunPopupMenu（TextBox 右键菜单）
#include "internal/Gfx.h"
#include "Accessibility.h"
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
// TextBox 插入符闪烁定时器 id（自绘窗口没有系统 caret，闪烁自管）
constexpr UINT_PTR kCaretBlinkTimerId = 0x5A7C;

// 局部夹取（core 的 ClampInt 未导出，这里是同一语义的最小副本）
static int ClampV(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

namespace {
HWND HwndOfWidget(WindowHandle h) { return reinterpret_cast<HWND>(h); }
HDC DcOfWidget(DeviceContextHandle h) { return reinterpret_cast<HDC>(h); }
POINT ToWinPoint(const Point& p) { return POINT{ p.x, p.y }; }
Point FromWinPoint(const POINT& p) { return Point{ p.x, p.y }; }
RECT ToWinRect(const Rect& r) { return RECT{ r.left, r.top, r.right, r.bottom }; }
Rect FromWinRect(const RECT& r) { return Rect{ r.left, r.top, r.right, r.bottom }; }
} // namespace

// 注册给系统的回调必须在 x86 上使用系统约定（被调方清栈）：公开头里的
// *Proc 要保持平台中立（公开头不能出现平台类型/宏），只能声明成默认约定。
// 若直接把它强转成 SUBCLASSPROC，Win32 下每次派发都会漏掉 6 个参数的栈
// 清理，栈指针逐次漂移，最终以堆损坏（0xC0000374）的形式炸掉——x64 只有
// 一种约定，所以这个问题只在 x86 暴露。故在 .cpp 侧加薄桥接。
namespace {
LRESULT CALLBACK TextBoxProcBridge(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                   UINT_PTR idSub, DWORD_PTR ref)
{
    return TextBoxProc(hwnd, msg, wParam, lParam, idSub, ref);
}
LRESULT CALLBACK ListBoxProcBridge(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                   UINT_PTR idSub, DWORD_PTR ref)
{
    return ListBoxProc(hwnd, msg, wParam, lParam, idSub, ref);
}
LRESULT CALLBACK CustomProcBridge(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                  UINT_PTR idSub, DWORD_PTR ref)
{
    return CustomProc(hwnd, msg, wParam, lParam, idSub, ref);
}
} // namespace

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
    if (m_hwnd && IsWindow(HwndOfWidget(m_hwnd)))
        DestroyWindow(HwndOfWidget(m_hwnd));
}

void Widget::Show(bool visible)
{
    if (m_hwnd) ::ShowWindow(HwndOfWidget(m_hwnd), visible ? SW_SHOW : SW_HIDE);
}

bool Widget::Visible() const
{
    return m_hwnd && IsWindowVisible(HwndOfWidget(m_hwnd));
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
    if (GetWindowRect(HwndOfWidget(m_hwnd), &cur)) {
        HWND parent = GetParent(HwndOfWidget(m_hwnd));
        if (parent) MapWindowPoints(HWND_DESKTOP, parent, (LPPOINT)&cur, 2);
        if (cur.left == x && cur.top == y &&
            cur.right == x + w && cur.bottom == y + h)
            return;
    }
    MoveWindow(HwndOfWidget(m_hwnd), x, y, w, h, TRUE);
}

void Widget::Move(const Rect& rc)
{
    Move(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
}

void Widget::Focus()
{
    if (m_hwnd) ::SetFocus(HwndOfWidget(m_hwnd));
}

Rect Widget::Bounds() const
{
    HWND hwnd = reinterpret_cast<HWND>(m_hwnd);
    RECT rc = {};
    if (hwnd) GetWindowRect(HwndOfWidget(hwnd), &rc);
    if (m_shell && m_shell->Hwnd())
        MapWindowPoints(HWND_DESKTOP, HwndOfWidget(m_shell->Hwnd()), (LPPOINT)&rc, 2);
    return Rect{ rc.left, rc.top, rc.right, rc.bottom };
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
    HWND parent = HwndOfWidget(m_shell->Hwnd());

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

    m_font = m_shell ? static_cast<HFONT>(m_shell->UiFonts().Get(m_desc.fontRole)) : nullptr;
    m_edit.hasFocus = false;
    m_edit.maxLength = (m_desc.maxLength > 0) ? m_desc.maxLength : 0;
    // 键入/粘贴等走 core 引擎的路径由 InsertAtCursor 按 m_edit.maxLength
    // 强制；这里把 desc 的声明同步进引擎（desc 是页面侧的唯一声明点）。
    SyncTextLayout();
    SetWindowSubclass(HwndOfWidget(m_hwnd), &TextBoxProcBridge, kSubTextBox, (DWORD_PTR)this);
}

MessageResult TextBoxProc(WindowHandle hwndH, MessageId msg, MessageWParam wParam,
                                    MessageLParam lParam, SubclassId idSub,
                                    SubclassRef ref)
{
    HWND hwnd = reinterpret_cast<HWND>(hwndH);
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
        InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
        return 0;
    }
    if (msg == EM_SETMARGINS && self) {
        self->m_rightInset = (int)HIWORD(lParam);
        InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
        return 0;
    }
    if (msg == EM_GETSEL && self) {
        // 标准 EDIT 探测：返回 [min,max) 选区（无选区 = caret, caret）
        short* sp = (short*)lParam;
        if (sp) {
            sp[0] = (short)SelectionBegin(self->m_edit);
            sp[1] = (short)SelectionEnd(self->m_edit);
        }
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
    case WM_CONTEXTMENU: {
        // 键盘 Shift+F10：wParam=0，lParam=-1 → 锚定当前 caret
        // （与 ListBoxProc 的 WM_CONTEXTMENU 同款处理）
        if (!self) break;
        POINT sp;
        if (wParam == 0 && lParam == (LPARAM)-1) {
            const Point c = self->CaretPoint();
            sp = { c.x + Mx().unit, c.y + Mx().unit };
            ClientToScreen(hwnd, &sp);
        } else {
            sp = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            ScreenToClient(hwnd, &sp);
            self->CaretAtClientPoint(FromWinPoint(sp));
            ClientToScreen(hwnd, &sp);
        }
        self->ShowEditMenu(FromWinPoint(sp));
        return 0;
    }
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
            self->RestartCaretBlink();
            InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
        }
        return 0;
    case WM_KILLFOCUS:
        if (self) {
            self->m_edit.hasFocus = false;
            self->StopCaretBlink();
            InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
        }
        return 0;
    case WM_TIMER:
        if (self && wParam == kCaretBlinkTimerId) {
            self->m_caretOn = !self->m_caretOn;
            InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
            return 0;
        }
        break;   // 别的 id 交给默认
    case WM_SYSCOLORCHANGE:
        // 系统 caret 闪烁周期可能被用户/IME 改过，重新对齐
        if (self) self->RestartCaretBlink();
        break;
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
            InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (self) self->BeginDrag(Point{ (short)LOWORD(lParam),
                                         (short)HIWORD(lParam) });
        return 0;
    case WM_LBUTTONDBLCLK:
        if (self) self->SelectWordAt(Point{ (short)LOWORD(lParam),
                                            (short)HIWORD(lParam) });
        return 0;
    case WM_LBUTTONUP:
        if (self && self->m_dragging) {
            self->m_dragging = false;
            ReleaseCapture();
        }
        return 0;
    case WM_MOUSEMOVE:
        if (self && self->m_dragging)
            self->DragTo(Point{ (short)LOWORD(lParam),
                                (short)HIWORD(lParam) });
        return 0;
    case WM_RBUTTONUP:
        // 先定位 caret（原生 EDIT 语义），再弹菜单（lParam 已是客户区坐标）
        if (self) {
            const POINT cp = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            self->CaretAtClientPoint(FromWinPoint(cp));
            POINT sp = cp;
            ClientToScreen(hwnd, &sp);
            self->ShowEditMenu(FromWinPoint(sp));
        }
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
                    InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
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
            InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
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
        if (self) self->StopCaretBlink();
        RemoveWindowSubclass(HwndOfWidget(hwnd), &TextBoxProcBridge, idSub);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

bool TextBox::HandleKey(VirtualKey vk, bool ctrl, bool shift)
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
    // 程序化设文本同样受 maxLength 约束（与键入/粘贴同口径，UTF-16 单元）：
    // 否则 SetText 成为绕过上限的旁路。截断到前 maxLen 单元。
    std::wstring v = s;
    if (m_edit.maxLength > 0 && (int)v.size() > m_edit.maxLength)
        v = v.substr(0, (size_t)m_edit.maxLength);
    m_edit.text = v;
    m_edit.cursor = (int)v.size();
    m_edit.selectionAnchor = -1;
    m_edit.composition.clear();
    m_edit.compositionCursor = 0;
    m_undoStack.clear();          // 程序化重置：历史作废（原生 EDIT 语义）
    m_redoStack.clear();
    m_lastNotified = v;           // EN_CHANGE 基线 = 新文本（SetText 自身不发）
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
    if (m_hwnd) InvalidateRect(HwndOfWidget(m_hwnd), nullptr, FALSE);
}

void TextBox::SetMaxLength(int maxLen)
{
    const int v = (maxLen > 0) ? maxLen : 0;
    m_desc.maxLength = v;
    m_edit.maxLength = v;
    // 若当前文本已超限，截断并重建（光标夹取到新长度内）。
    if (v > 0 && (int)m_edit.text.size() > v) {
        m_edit.text = m_edit.text.substr(0, (size_t)v);
        m_edit.cursor = ClampV(m_edit.cursor, 0, v);
        m_edit.selectionAnchor = (m_edit.selectionAnchor > v) ? v
                                   : ((m_edit.selectionAnchor < 0) ? -1 : m_edit.selectionAnchor);
        m_layW = -1;
    }
    if (m_hwnd) ApplyPostEdit();
}

void TextBox::SetPassword(bool on)
{
    if (m_desc.password == on) return;
    m_desc.password = on;
    // 掩码与真实文本等长，但字形宽度不同（• vs 原字符）→ 必须重建
    // m_cum（SyncTextLayout 的判据用 m_laidShown 对比显示串，会触发重建）。
    m_layW = -1;
    if (m_hwnd) ApplyPostEdit();
}

void TextBox::OnKey(std::function<bool(VirtualKey, bool, bool)> cb)
{
    m_keyCb = std::move(cb);
}

void TextBox::SetRightInset(int px)
{
    m_rightInset = px;
    m_layW = -1;
    if (m_hwnd) ApplyPostEdit();
}

void TextBox::ClickAt(Point parentPt)
{
    if (!m_hwnd) return;
    POINT p = ToWinPoint(parentPt);
    MapWindowPoints(GetParent(HwndOfWidget(m_hwnd)), HwndOfWidget(m_hwnd), &p, 1);
    SetFocus(HwndOfWidget(m_hwnd));
    CaretAtClientPoint(FromWinPoint(p));
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

// 密码掩码：core DisplayText 的 1:1 等长映射（每个 UTF-16 单元 → •）。
// 等长是关键——m_cum/折行/光标/选区全部按掩码几何算，与真实文本一一对应，
// 无需第二套布局；真实文本只存在于 m_edit.text（读取/编辑/EN_CHANGE）。
const std::wstring& TextBox::ShownText() const
{
    if (m_desc.password)
        m_shown = ::air::ui::DisplayText(m_edit, true);
    else
        m_shown = m_edit.text;
    return m_shown;
}

void TextBox::SyncTextLayout()
{
    if (!m_shell || !m_shell->UiFonts().Valid()) return;
    m_font = static_cast<HFONT>(m_shell->UiFonts().Get(m_desc.fontRole));

    RECT rc{};
    if (m_hwnd) GetClientRect(HwndOfWidget(m_hwnd), &rc);
    const int availW = (int)(rc.right - rc.left) - Mx().unit * 2 - m_rightInset;
    const int availH = (int)(rc.bottom - rc.top);
    // 判据用「上次布局所用显示串」而非仅长度：等长换文本、密码开/关
    // （掩码与真实串等长）都会让显示串变化，必须重建 m_cum，否则光标
    // /选区读到的是旧字形宽度。字体 DPI 同样入键：DPI 变化时客户区
    // 尺寸可能不变（多行框填满固定区）但字形变宽，旧判据会误命中
    // 缓存、光标/选区沿用上 DPI 的宽度表。
    const std::wstring& shown = ShownText();
    const int fontDpi = m_shell->UiFonts().Dpi();
    const bool same = (m_layW == availW && m_layH == availH &&
                       m_laidShown == shown && m_layFontDpi == fontDpi);
    if (same) return;
    m_layW = availW;
    m_layH = availH;
    m_layFontDpi = fontDpi;

    HDC dc = GetDC(nullptr);
    // 按**显示串**测宽/折行（password 时是等长掩码 •；与真实文本 1:1，
    // 下标语义不变）。宽度表必须和绘制用的字形同源，否则光标/选区错位。
    m_cum = MeasureCum(dc, reinterpret_cast<HFONT>(m_font), shown);
    m_laidShown = shown;
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

    // 多行折行；单行不折（avail = 无限），横向滚动由 PaintSelf 单行分支处理。
    m_lines = WrapText(shown, m_cum,
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

// ============================================================
// 鼠标交互：点击 / 拖选 / 双击选词 / 右键菜单
// 坐标约定：HitIndex/BeginDrag/DragTo 收客户区坐标（含 unit 内边距）；
// 内部换算成文本下标走 core 的同一套几何（IndexAtX/LineOfIndex）。
// ============================================================

// 客户区点 → 全局文本下标（点击/拖选/双击/右键共用）
int TextBox::HitIndex(Point pt)
{
    SyncTextLayout();
    // 命中按显示串（与 m_cum/m_lines 同源）；掩码与真实文本 1:1，
    // 返回的下标对两者等价。
    const std::wstring& shown = ShownText();
    const int n = (int)shown.size();
    const int unit = Mx().unit;
    if (m_desc.multiline) {
        if (m_lines.begin.empty() || n == 0) return 0;
        const int lineH = m_lineH > 0 ? m_lineH : 1;
        int line = (pt.y - unit + m_scroll) / lineH;
        line = ClampV(line, 0, (int)m_lines.begin.size() - 1);
        return IndexAtX(shown, m_cum, m_lines.begin[line],
                        m_lines.end[line], pt.x - unit);
    }
    return IndexAtX(shown, m_cum, 0, n, pt.x - unit);
}

void TextBox::BeginDrag(Point pt)
{
    if (!m_hwnd) return;
    SetFocus(HwndOfWidget(m_hwnd));
    if (!m_edit.hasFocus) m_edit.hasFocus = true;
    m_dragging = true;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    // 点击定位本身：无 Shift → 清选区放 caret；有 Shift → 从锚点扩选
    m_edit.cursor = HitIndex(pt);
    if (shift) {
        if (m_edit.selectionAnchor < 0) m_edit.selectionAnchor = m_edit.cursor;
    } else {
        m_edit.selectionAnchor = -1;
    }
    // 拖选锚点：Shift+下击扩展保留原锚点；普通下击锚点 = 下击点。
    // 必须在 cursor 定位之后取，否则拿到的是旧光标位置（T2 回归源）。
    m_dragAnchor = shift ? m_edit.selectionAnchor : m_edit.cursor;
    SetCapture(HwndOfWidget(m_hwnd));
    ApplyPostEdit();
}

void TextBox::DragTo(Point pt)
{
    // 拖出窗口也要有 WM_MOUSEMOVE：SetCapture 后由本窗口收。
    // 多行：光标拖到上/下边缘附近自动滚屏（原生 EDIT 同款）
    if (m_desc.multiline && m_hwnd) {
        RECT rc{};
        GetClientRect(HwndOfWidget(m_hwnd), &rc);
        const int edge = 12;
        int dy = 0;
        if (pt.y < edge)                      dy = -3;
        else if (pt.y > (rc.bottom - rc.top) - edge) dy = +3;
        if (dy != 0) {
            m_scroll = ClampV(m_scroll + dy, 0, MaxScroll());
            InvalidateRect(HwndOfWidget(m_hwnd), nullptr, FALSE);
        }
    }
    const int idx = HitIndex(pt);
    if (m_dragAnchor < 0) m_dragAnchor = m_edit.cursor;   // 首次：锚 = 下击点
    m_edit.cursor = idx;
    m_edit.selectionAnchor = m_dragAnchor;
    ApplyPostEdit();
}

// 双击选词：ASCII 词 = 连续字母数字；CJK 字符各自成词（与
// 系统 EDIT 的"中文双击选一个词"相比我们选单字，是保守但确定的
// 行为，不引入分词依赖）。选区 = 词的首尾，caret 落到词尾。
void TextBox::SelectWordAt(Point pt)
{
    const int n = (int)m_edit.text.size();
    if (n == 0) return;
    const int unit = Mx().unit;
    int at = HitIndex(pt);
    if (at > n) at = n;
    const std::wstring& s = m_edit.text;
    auto isWordChar = [](wchar_t c) {
        return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
               (c >= L'0' && c <= L'9');
    };
    if (isWordChar(s[(size_t)at])) {
        // ASCII 词：向前/向后扩到非词字符（at 可能指在词尾，从词尾
        // 回退后再向前扫）
        int beg = at;
        while (beg > 0 && isWordChar(s[(size_t)(beg - 1)])) --beg;
        int end = at;
        while (end < n && isWordChar(s[(size_t)end])) ++end;
        m_edit.selectionAnchor = beg;
        m_edit.cursor = end;
    } else {
        // CJK/标点：选点击的那个字符（at 落在两个字符之间且前一字符
        // 存在时视为点击前一字符）
        int i = (at < n) ? at : at - 1;
        m_edit.selectionAnchor = i;
        m_edit.cursor = i + 1;
    }
    ApplyPostEdit();
}

// 右键 / Shift+F10 菜单：复制 / 剪切 / 粘贴（disabled 态按有无选区/剪贴板）
void TextBox::ShowEditMenu(Point screenPt)
{
    if (!m_hwnd || !m_shell) return;
    const bool hasSel = HasSelection(m_edit);
    std::vector< ::air::ui::OverlayMenuItem> items;
    // 撤销/重做（无历史时置灰，点击无效）；随后是原生 EDIT 的
    // 复制/剪切/粘贴。分隔符把两组操作分开（行高不同，menuclick
    // 按像素定位而不是均分行数）。
    items.push_back({ 1, L"撤销",   false, !CanUndo() });
    items.push_back({ 2, L"重做",   false, !CanRedo() });
    items.push_back({ 0, L"",       true,  false });
    items.push_back({ 3, L"复制",   false, !hasSel });
    items.push_back({ 4, L"剪切",   false, !hasSel });
    items.push_back({ 5, L"粘贴",   false, false });
    const int choice = ::air::ui::RunPopupMenu(HwndOfWidget(m_hwnd), m_shell->UiFonts(), ToWinPoint(screenPt), items);
    switch (choice) {
    case 1: Undo();  break;
    case 2: Redo();  break;
    case 3: {
        Win32Clipboard clip;
        ApplyEditResult(EditAction::Copy, L"", &clip);
        break;
    }
    case 4: {
        Win32Clipboard clip;
        ApplyEditResult(EditAction::Cut, L"", &clip);
        break;
    }
    case 5: {
        Win32Clipboard clip;
        InsertText(clip.Read());
        break;
    }
    default:
        break;
    }
}

// 返回插入点在**文本区**（PaintSelf 的 textRc）内的相对坐标：
// 单行 —— x 沿文本，y 垂直居中（文本区 = 客户区减 unit 内边距）；
// 多行 —— x 行内相对，y 已减去 m_scroll（与 PaintSelf 的行 y 同一坐标）。
// 之前这里直接返回客户区坐标（含 unit），PaintSelf 再 +textRc（又含
// unit）→ 双重计入，光标/组词串整体右偏+下偏一个 unit。
Point TextBox::CaretPoint() const
{
    Point p{ 0, 0 };
    if (!m_hwnd) return p;
    RECT rc{};
    GetClientRect(HwndOfWidget(m_hwnd), &rc);
    const int n = (int)m_edit.text.size();
    const bool haveCum = ((int)m_cum.size() == n + 1);
    const int caret = ClampV(m_edit.cursor, 0, n);
    if (m_desc.multiline) {
        if (!haveCum || m_lines.begin.empty()) return p;
        const int line = LineOfIndex(m_lines, caret);
        const int lb = m_lines.begin[line];
        p.x = (caret >= lb ? m_cum[caret] - m_cum[lb] : 0);
        p.y = line * m_lineH - m_scroll;
        return p;
    }
    // 单行：文本在文本区内垂直居中（PaintSelf 用 DT 单行 VCenter 对齐）。
    p.x = (haveCum ? m_cum[caret] : 0);
    p.y = (int)((rc.bottom - rc.top) - m_lineH) / 2;
    if (p.y < 0) p.y = 0;
    return p;
}

void TextBox::SyncIme()
{
    if (!m_hwnd) return;
    const Point c = CaretPoint();
    // ImmSetCompositionWindow/ImmSetCandidateWindow 的坐标是**窗口客户区**：
    // 文本区相对坐标 + unit 内边距还原。
    const int unit = Mx().unit;
    const int cx = c.x + unit, cy = c.y + unit;
    // 候选窗区域：整块文本区（caret 行的上下沿）。
    int zx = unit;
    int zy = cy;
    int zr = unit + m_layW;
    int zb = cy + m_lineH;
    if (zr < zx + 1) zr = zx + 1;
    if (zb < zy + 1) zb = zy + 1;
    SyncImeWindow(HwndOfWidget(m_hwnd), cx, cy, zx, zy, zr, zb);
}

void TextBox::ApplyEditResult(EditAction action, const std::wstring& arg,
                              IClipboard* clip)
{
    // 快照判据 = 「该动作能不能改文本」，与参数无关（Insert 传空串、
    // Cut 无选区都不会真正变化，多存一个快照无害）。粘贴/IME 结果/
    // 选区删除都会改变内容，因此也纳入。移动/全选/复制不动文本。
    const bool mayEdit = (action == EditAction::Insert ||
                          action == EditAction::Backspace ||
                          action == EditAction::Delete ||
                          action == EditAction::Cut ||
                          action == EditAction::Paste);
    if (mayEdit) {
        m_undoStack.push_back(m_edit);
        m_redoStack.clear();       // 新编辑使 redo 链作废
    }
    const EditResult r = air::ui::ApplyEdit(m_edit, action, arg, clip);
    if (r.consumed) ApplyPostEdit();
}

void TextBox::Undo()
{
    if (m_undoStack.empty()) return;
    m_redoStack.push_back(m_edit);
    m_edit = m_undoStack.back();
    m_undoStack.pop_back();
    // 快照里带着当时的 m_edit.maxLength：若运行期 SetMaxLength 改过
    // 上限，还原后必须用 m_desc 的当前值回写，否则上限会被旧快照带回。
    m_edit.maxLength = (m_desc.maxLength > 0) ? m_desc.maxLength : 0;
    m_layW = -1;                   // 快照文本/光标都变了：重建 cum/lines
    ApplyPostEdit();
}

void TextBox::Redo()
{
    if (m_redoStack.empty()) return;
    m_undoStack.push_back(m_edit);
    m_edit = m_redoStack.back();
    m_redoStack.pop_back();
    m_edit.maxLength = (m_desc.maxLength > 0) ? m_desc.maxLength : 0;
    m_layW = -1;
    ApplyPostEdit();
}

void TextBox::ApplyPostEdit()
{
    m_layW = -1;                 // 文本可能同长替换：一律重建布局
    SyncTextLayout();
    ScrollForCaret();
    SyncIme();
    RestartCaretBlink();        // 光标一移动就回「亮」相位（不闪半帧）
    // 文本内容变化 → EN_CHANGE（原生 EDIT 语义，SearchPlugin 的 250ms
    // 防抖过滤全靠它）。所有编辑路径（WM_CHAR / WM_KEYDOWN / WM_SETTEXT /
    // 粘贴 / 菜单操作）都汇聚到本函数；纯光标/选区移动内容未变，不发，
    // 避免白跑一次防抖查询。
    if (m_edit.text != m_lastNotified) {
        m_lastNotified = m_edit.text;
        if (m_hwnd && m_hwnd != GetParent(HwndOfWidget(m_hwnd)))
            SendMessageW(GetParent(HwndOfWidget(m_hwnd)), WM_COMMAND,
                         MAKEWPARAM(GetDlgCtrlID(HwndOfWidget(m_hwnd)), EN_CHANGE),
                         (LPARAM)HwndOfWidget(m_hwnd));
    }
    if (m_hwnd) InvalidateRect(HwndOfWidget(m_hwnd), nullptr, FALSE);
}

// ---- 插入符闪烁 ----
// 原生 EDIT 的闪烁由系统 caret 负责；自绘窗口里 caret 是我们画的，
// 闪烁也要自己驱动。周期/相位同步系统 GetCaretBlinkTime()（用户改
// 系统设置或切换 IME 时系统会发 WM_SYSCOLORCHANGE，跟着对齐）。
void TextBox::RestartCaretBlink()
{
    m_caretOn = true;
    if (!m_hwnd || !m_edit.hasFocus) return;
    KillTimer(HwndOfWidget(m_hwnd), kCaretBlinkTimerId);
    const UINT ms = GetCaretBlinkTime();
    if (ms != INFINITE) SetTimer(HwndOfWidget(m_hwnd), kCaretBlinkTimerId, ms, nullptr);
}

void TextBox::StopCaretBlink()
{
    if (m_hwnd) KillTimer(HwndOfWidget(m_hwnd), kCaretBlinkTimerId);
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

void TextBox::CaretAtClientPoint(Point pt)
{
    SyncTextLayout();
    const int n = (int)m_edit.text.size();
    if (!m_edit.hasFocus) SetFocus(HwndOfWidget(m_hwnd));
    m_edit.hasFocus = true;

    const int unit = Mx().unit;
    // 先捕获原选区，再动 cursor：若等 IndexAtX 覆盖了 cursor 再查
    // SelectionEnd，区间已被收缩，点击点会误判在选区外（T3 回归源）。
    const bool hadSel = HasSelection(m_edit);
    const int selBegin0 = hadSel ? SelectionBegin(m_edit) : 0;
    const int selEnd0   = hadSel ? SelectionEnd(m_edit) : 0;
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
    // 落在已有选区内 → 保留选区（原生 EDIT 语义：这正是右键菜单
    // 复制/剪切 的工作方式）；选区外 → 定位 caret 并清选区。
    // 用捕获的原选区判定，不读已被 cursor 移动改写的 SelectionEnd。
    if (hadSel && m_edit.cursor >= selBegin0 && m_edit.cursor < selEnd0) {
        m_edit.cursor = selEnd0;
        m_edit.selectionAnchor = selBegin0;
        ApplyPostEdit();
        return;
    }
    if (GetKeyState(VK_SHIFT) & 0x8000) {
        if (m_edit.selectionAnchor < 0) m_edit.selectionAnchor = m_edit.cursor;
    } else {
        m_edit.selectionAnchor = -1;
    }
    ApplyPostEdit();
}

bool TextBox::DefaultKey(VirtualKey vk, bool ctrl, bool shift)
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
        case 'Z':
            // Ctrl+Z 撤销；Ctrl+Shift+Z 是标准 IDE 的重做约定（与
            // Ctrl+Y 并存）。Shift 从 WM_KEYDOWN 的 GetKeyState 传入。
            if (shift) { Redo(); } else { Undo(); }
            return true;
        case 'Y':
            Redo();
            return true;
        case 'C': case 'X': {
            Win32Clipboard clip;
            // 不再直调 core ApplyEdit：统一走 ApplyEditResult，快照语义
            // 与菜单复制/剪切一致（Ctrl+X 因此可撤销）。
            ApplyEditResult(vk == 'C' ? EditAction::Copy : EditAction::Cut,
                            L"", &clip);
            return true;
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

// 单行与多行共用 m_cum（逐字 GDI 实测前缀宽度表）做光标/选区/文本几何，
// 三者和绘制同源；单行不折行、超出时横向滚动（caret 跟进出界自动偏移）。
void TextBox::PaintSelf(DeviceContextHandle dc)
{
    if (!m_shell || !dc) return;
    SyncTextLayout();

    RECT cr{};
    GetClientRect(HwndOfWidget(m_hwnd), &cr);
    const ::air::ui::Rect frame(cr.left, cr.top, cr.right, cr.bottom);
    if (frame.Empty()) return;

    PaintTarget target{ dc, frame };
    std::unique_ptr<IPainter> g =
        PainterRegistry::Instance().Begin("gdi", target);
    if (!g) return;
    const Palette& pal = Pal();
    const Metrics& mx = Mx();
    const int unit = mx.unit;
    const Color ink = m_edit.hasFocus ? pal.text : pal.textSub;
    const bool composing = !m_edit.composition.empty();
    const bool showCaret = m_edit.hasFocus && m_caretOn && !composing;

    const ::air::ui::Rect textRc(frame.left + unit, frame.top + unit,
                                 frame.right - unit - m_rightInset,
                                 frame.bottom - unit);
    if (textRc.Empty()) return;
    const std::wstring& shown = ShownText();   // password → 掩码；绘制只认它
    const bool haveCum = ((int)m_cum.size() == (int)shown.size() + 1);
    g->FillRound(frame, mx.radiusMd, pal.surface);
    g->StrokeRound(frame, mx.radiusMd,
                   m_edit.hasFocus ? pal.primary : pal.border, 1);
    g->PushClip(textRc);

    if (!m_desc.multiline) {
        // ---- 单行 ----
        // 不再走 core PaintTextBox：它的 caret/选区几何是等宽模型
        // （cursor × charW("0")），比例字体下光标位置随内容漂移，且
        // 宽度测量（IPainter::MeasureText）与 GDI 绘制字形不同源。
        // 这里与多行共用 m_cum（逐字 GDI 实测前缀宽度表），光标、选区、
        // 文本三者和绘制同源。
        if (m_edit.text.empty() && !composing) {
            const std::wstring ph = m_desc.placeholder;
            if (!ph.empty())
                g->Text(textRc, ph.c_str(), ph.size(), m_desc.fontRole,
                        pal.textFaint, dt::Left | dt::VCenter | dt::SingleLine);
        } else {
            const int n = (int)m_edit.text.size();
            const int caret = ClampV(m_edit.cursor, 0, n);
            const int cx0 = haveCum ? m_cum[caret] : 0;
            // 横向滚动：caret 出右界左移、出左界右移（EDIT 的同款语义）
            int scrollX = 0;
            const int avail = (int)(textRc.right - textRc.left);
            if (cx0 > avail - unit) scrollX = cx0 - (avail - unit);
            const int totalW = (haveCum && n > 0) ? m_cum[n] : 0;
            if (scrollX > totalW - unit) scrollX = (totalW > unit) ? totalW - unit : 0;
            if (HasSelection(m_edit)) {
                const int a = SelectionBegin(m_edit), b = SelectionEnd(m_edit);
                if (haveCum && b > a) {
                    const int x0 = (std::max)(0, m_cum[a] - scrollX);
                    const int x1 = m_cum[b] - scrollX;
                    if (x1 > x0)
                        g->FillRect(::air::ui::Rect(textRc.left + x0, textRc.top,
                                                    textRc.left + x1, textRc.bottom),
                                    pal.selectFill);
                }
            }
            g->Text(::air::ui::Rect(textRc.left - scrollX, textRc.top,
                                    textRc.left - scrollX + totalW + avail,
                                    textRc.bottom),
                    shown.c_str(), shown.size(), m_desc.fontRole, ink,
                    dt::Left | dt::VCenter | dt::SingleLine);
            if (composing) {
                const int x = textRc.left + cx0 - scrollX;
                g->Text(::air::ui::Rect(x, textRc.top, x + avail, textRc.bottom),
                        m_edit.composition.c_str(), m_edit.composition.size(),
                        m_desc.fontRole, pal.primaryText,
                        dt::Left | dt::VCenter | dt::SingleLine);
            }
        }
        if (showCaret) {
            // 空框（占位字）与有文本同一几何：caret = textRc.left + cum[cursor]
            const int cx0 = haveCum ? m_cum[ClampV(m_edit.cursor, 0, (int)m_edit.text.size())] : 0;
            // 窄框保护：textRc 上下各留 2px 后不足 8px 时，caret 居中保底 8px
            // （单行框高 ≈ 一行文字高，textRc 只有几个 px，直接 inset 会变空矩形）
            int cy0 = textRc.top + 2, cy1 = textRc.bottom - 2;
            if (cy1 - cy0 < 8) {
                const int mid = (textRc.top + textRc.bottom) / 2;
                cy0 = mid - 4; cy1 = mid + 4;
            }
            g->FillRect(::air::ui::Rect(textRc.left + cx0, cy0,
                                        textRc.left + cx0 + 1, cy1),
                        pal.primary);
        }
        g->PopClip();
        return;
    }

    // ---- 多行：行内选区 + 逐行文本 + 组词串 + 插入符 ----
    if (m_edit.text.empty() && !composing) {
        const std::wstring ph = m_desc.placeholder;
        if (!ph.empty())
            g->Text(textRc, ph.c_str(), ph.size(), m_desc.fontRole,
                    pal.textFaint, dt::Left | dt::Top | dt::SingleLine);
    } else {
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

        for (size_t li = 0; li < m_lines.begin.size(); ++li) {
            const int lb = m_lines.begin[li], le = m_lines.end[li];
            if (le <= lb) continue;
            const int y = textRc.top + (int)li * lineH - m_scroll;
            if (y >= textRc.bottom || y + lineH <= textRc.top) continue;
            g->Text(::air::ui::Rect(textRc.left, y, textRc.right, y + lineH),
                    shown.c_str() + lb, (size_t)(le - lb), m_desc.fontRole,
                    ink, dt::Left | dt::Top | dt::SingleLine);
        }
    }

    // CaretPoint() 返回 textRc 相对坐标：textRc.left/top 只加一次。
    // 空框（占位字）与有文本同一几何：caret = textRc.left + cum[cursor]。
    const int lineH = m_lineH > 0 ? m_lineH : 1;
    if (composing) {
        const Point c = CaretPoint();
        const int y = textRc.top + c.y;
        g->Text(::air::ui::Rect(textRc.left + c.x, y, textRc.right, y + lineH),
                m_edit.composition.c_str(), m_edit.composition.size(),
                m_desc.fontRole, pal.primaryText,
                dt::Left | dt::Top | dt::SingleLine);
    }
    if (showCaret) {
        const Point c = CaretPoint();
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
    HWND parent = HwndOfWidget(m_shell->Hwnd());
    m_style = m_desc.style;

    m_hwnd = CreateWindowExW(0, L"BUTTON", m_desc.text.c_str(),
                             WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                             0, 0, 0, 0, parent,
                             (HMENU)(INT_PTR)m_id, nullptr, nullptr);
    if (!m_hwnd) return;

    SendMessageW(HwndOfWidget(m_hwnd), WM_SETFONT,
                 (WPARAM)m_shell->UiFonts().Get(FontRole::Button), TRUE);
    AttachButton(HwndOfWidget(m_hwnd), m_style);
}

void Button::SetText(const std::wstring& s)
{
    if (m_hwnd) SetWindowTextW(HwndOfWidget(m_hwnd), s.c_str());
}

std::wstring Button::Text() const
{
    wchar_t buf[128] = {};
    if (m_hwnd) GetWindowTextW(HwndOfWidget(m_hwnd), buf, 128);
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

bool Button::HandleDrawItem(const DrawItemInfo* dis)
{
    if (!m_shell) return false;
    DrawButton(DcOfWidget(dis->hDC), ToWinRect(dis->rcItem), Text().c_str(),
               m_shell->UiFonts(), FontRole::Button,
               m_style,
               IsButtonHover(HwndOfWidget(dis->hwndItem)),
               (dis->itemState & ODS_SELECTED) != 0);
    return true;
}

// ============================================================
// ListBox
// ============================================================
void ListBox::CreateNative()
{
    HWND parent = HwndOfWidget(m_shell->Hwnd());
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

    SetWindowSubclass(HwndOfWidget(m_hwnd), &ListBoxProcBridge, kSubListBox, (DWORD_PTR)this);
}

MessageResult ListBoxProc(WindowHandle hwndH, MessageId msg, MessageWParam wParam,
                                    MessageLParam lParam, SubclassId idSub,
                                    SubclassRef ref)
{
    HWND hwnd = reinterpret_cast<HWND>(hwndH);
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
        case LB_GETSELCOUNT:
            // 原生语义：单选无项 = -1；多选 = 已选条数
            if (self->m_desc.multiSelect)
                return (LRESULT)self->MultiSelectionCount();
            return (self->m_sel >= 0) ? 0 : -1;
        case LB_GETSEL:
            // 原生语义：已选 = 0，未选/无效下标 = -1（多选集合成员判定）
            if (self->m_desc.multiSelect)
                return self->IsSelected((int)wParam) ? 0 : -1;
            return ((int)wParam >= 0 && (int)wParam < n &&
                    (int)wParam == self->m_sel) ? 0 : -1;
        case LB_SETSEL: {
            // 简化语义：wParam=1 加区间，0 移除区间，-1 移除全部；
            // lParam = (start,end) 通过 LOWORD/HIWORD 传递
            const int sel = (int)wParam;
            if (sel == -1) { self->ClearSelection(); return 0; }
            self->SetSelectionRange((int)(short)LOWORD(lParam),
                                    (int)(short)HIWORD(lParam), sel != 0);
            return 0;
        }
        case LB_GETITEMRECT:
            if ((int)wParam < 0 || (int)wParam >= n) return LB_ERR;
            if (RECT* out = (RECT*)lParam) *out = ToWinRect(self->ItemRect((int)wParam));
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
            const int idx = self->ItemAtPoint(FromWinPoint(pt));
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
            InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
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
        SetFocus(HwndOfWidget(hwnd));
        const int item = self->ItemAtPoint(FromWinPoint(pt));
        if (item >= 0) {
            if (self->onItemClicked) {
                const Rect rc = self->ItemRect(item);
                Point ptIn = { pt.x - rc.left, pt.y - rc.top };
                if (self->onItemClicked(item, ptIn)) return 0;
            }
            if (self->m_desc.multiSelect)
                self->OnMultiClick(item,
                                   (wParam & MK_CONTROL) != 0,
                                   (wParam & MK_SHIFT) != 0);
            else
                self->SelectItem(item, true);
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        if (!self) break;
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        const int item = self->ItemAtPoint(FromWinPoint(pt));
        if (item >= 0) {
            self->SelectItem(item, false);
            SendMessageW(GetParent(HwndOfWidget(hwnd)), WM_COMMAND,
                         MAKEWPARAM(GetDlgCtrlID(hwnd), LBN_DBLCLK),
                         (LPARAM)hwnd);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!self) break;
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        const int item = self->ItemAtPoint(FromWinPoint(pt));
        if (item != self->m_hover) {
            const int old = self->m_hover;
            self->m_hover = item;
            if (old >= 0) self->InvalidateItem(old);
            if (item >= 0) self->InvalidateItem(item);
        }
        if (self->onHoverMove) {
            Point in = { pt.x, pt.y };
            if (item >= 0) {
                const Rect irc = self->ItemRect(item);
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
            Point out = { -1, -1 };
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
        InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
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
            InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
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
            item = self->ItemAtPoint(FromWinPoint(lpt));
        } else {
            item = self->m_sel;
            if (item < 0) return 0;
            const Rect rc = self->ItemRect(item);
            pt = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
            MapWindowPoints(HwndOfWidget(hwnd), HWND_DESKTOP, (LPPOINT)&pt, 1);
        }
        self->onContextMenu(FromWinPoint(pt), item);
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
        RemoveWindowSubclass(HwndOfWidget(hwnd), &ListBoxProcBridge, idSub);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void ListBox::Reset()
{
    m_items.clear();
    m_sel = -1;
    m_selSet.clear();
    m_selAnchor = -1;
    m_hover = -1;
    m_scroll = 0;
    if (m_hwnd) {
        SyncScrollBar();
        InvalidateRect(HwndOfWidget(m_hwnd), nullptr, FALSE);
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
        InvalidateRect(HwndOfWidget(m_hwnd), nullptr, FALSE);
    }
}

int ListBox::AddItem(int height, size_t data)
{
    if (height < 1) height = m_fallbackH > 0 ? m_fallbackH : 1;
    m_items.push_back(ListBoxItem{ height, data });
    if (m_desc.multiSelect)
        m_selSet.resize(m_items.size(), false);
    const int idx = (int)m_items.size() - 1;
    if (m_hwnd && m_updateLock == 0) {
        SyncScrollBar();
        InvalidateRect(HwndOfWidget(m_hwnd), nullptr, FALSE);
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
    if (m_desc.multiSelect && (int)m_selSet.size() == (int)m_items.size() && index >= 0) {
        // SetSelection 保持"原生单选语义"：清空集合、锚点=该行
        for (size_t i = 0; i < m_selSet.size(); ++i) m_selSet[i] = false;
        m_selSet[(size_t)index] = true;
        m_selAnchor = index;
    }
    if (index >= 0) EnsureVisible(index);
    if (m_hwnd) {
        SyncScrollBar();
        InvalidateRect(HwndOfWidget(m_hwnd), nullptr, FALSE);
    }
}

Rect ListBox::ItemRect(int index)
{
    RECT rc = {};
    if (!m_hwnd || index < 0 || index >= (int)m_items.size()) return FromWinRect(rc);
    RECT cr = {};
    GetClientRect(HwndOfWidget(m_hwnd), &cr);
    int y = -m_scroll;
    for (int i = 0; i < index; ++i) y += m_items[(size_t)i].height;
    rc.left   = 0;
    rc.top    = y;
    rc.right  = cr.right;
    rc.bottom = y + m_items[(size_t)index].height;
    return FromWinRect(rc);
}

void ListBox::InvalidateItem(int index)
{
    if (!m_hwnd) return;
    if (index < 0 || index >= (int)m_items.size()) return;
    const Rect rc = ItemRect(index);
    RECT native = ToWinRect(rc);
        InvalidateRect(HwndOfWidget(m_hwnd), &native, FALSE);
}

void ListBox::Invalidate()
{
    if (m_hwnd) InvalidateRect(HwndOfWidget(m_hwnd), nullptr, FALSE);
}

int ListBox::ItemAtPoint(Point clientPt)
{
    if (!m_hwnd) return -1;
    RECT cr = {};
    GetClientRect(HwndOfWidget(m_hwnd), &cr);
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
    if (m_hwnd) GetClientRect(HwndOfWidget(m_hwnd), &cr);
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
    SetScrollInfo(HwndOfWidget(m_hwnd), SB_VERT, &si, TRUE);
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
        SendMessageW(GetParent(HwndOfWidget(m_hwnd)), WM_COMMAND,
                     MAKEWPARAM(GetDlgCtrlID(HwndOfWidget(m_hwnd)), LBN_SELCHANGE),
                     (LPARAM)HwndOfWidget(m_hwnd));
    }
}

// ---- 多选（多区）----
// 语义（Explorer 风格）：普通点击 = 单选（清空集合、锚点=该行）；
// Ctrl+点击 = 切换该行（锚点保持）；Shift+点击 = 加选锚点到该行的区间。
// 修饰键取 wParam 上的 MK_CONTROL/MK_SHIFT，真实鼠标与跨进程 Posted
// WM_LBUTTONDOWN 都带这两位（GetKeyState 在跨进程测试中不可靠）。
void ListBox::OnMultiClick(int item, bool ctrl, bool shift)
{
    const int n = (int)m_items.size();
    if (item < 0 || item >= n) return;
    bool changed = false;
    if (ctrl) {
        m_selSet[(size_t)item] = !m_selSet[(size_t)item];
        m_sel = item;
        if (m_selAnchor < 0) m_selAnchor = item;
        changed = true;
    } else if (shift && m_selAnchor >= 0) {
        const int a = m_selAnchor, b = item;
        const int lo = (a < b) ? a : b, hi = (a < b) ? b : a;
        for (int i = lo; i <= hi; ++i) m_selSet[(size_t)i] = true;
        m_sel = item;
        changed = true;
    } else {
        for (int i = 0; i < n; ++i) m_selSet[(size_t)i] = false;
        if (item < n) m_selSet[(size_t)item] = true;
        m_sel = item;
        m_selAnchor = item;
        changed = true;
    }
    if (!changed) return;
    EnsureVisible(item);
    if (m_hwnd) {
        SyncScrollBar();
        Invalidate();
        SendMessageW(GetParent(HwndOfWidget(m_hwnd)), WM_COMMAND,
                     MAKEWPARAM(GetDlgCtrlID(HwndOfWidget(m_hwnd)), LBN_SELCHANGE),
                     (LPARAM)HwndOfWidget(m_hwnd));
    }
}

void ListBox::SetMultiSelect(bool on)
{
    if (m_desc.multiSelect == on) return;
    m_desc.multiSelect = on;
    if (on && (int)m_selSet.size() != (int)m_items.size()) {
        std::vector<bool> s((size_t)m_items.size(), false);
        if (m_sel >= 0 && m_sel < (int)s.size()) s[(size_t)m_sel] = true;
        m_selSet = std::move(s);
    }
    m_selAnchor = -1;
    if (m_hwnd) { SyncScrollBar(); Invalidate(); }
}

bool ListBox::IsSelected(int index) const
{
    if (index < 0 || index >= (int)m_items.size()) return false;
    if (!m_desc.multiSelect) return index == m_sel;
    if ((int)m_selSet.size() == (int)m_items.size()) return m_selSet[(size_t)index];
    return index == m_sel;
}

int ListBox::MultiSelectionCount() const
{
    if (m_desc.multiSelect && (int)m_selSet.size() == (int)m_items.size()) {
        int c = 0;
        for (bool b : m_selSet) if (b) ++c;
        return c;
    }
    return (m_sel >= 0) ? 1 : 0;
}

std::vector<int> ListBox::SelectedItems() const
{
    std::vector<int> out;
    if (m_desc.multiSelect && (int)m_selSet.size() == (int)m_items.size()) {
        for (size_t i = 0; i < m_selSet.size(); ++i)
            if (m_selSet[i]) out.push_back((int)i);
    } else if (m_sel >= 0 && m_sel < (int)m_items.size()) {
        out.push_back(m_sel);
    }
    return out;
}

void ListBox::ClearSelection()
{
    if (!m_desc.multiSelect) {
        if (m_sel >= 0) {
            const int old = m_sel;
            m_sel = -1;
            if (m_hwnd) {
                InvalidateItem(old);
                SendMessageW(GetParent(HwndOfWidget(m_hwnd)), WM_COMMAND,
                             MAKEWPARAM(GetDlgCtrlID(HwndOfWidget(m_hwnd)), LBN_SELCHANGE),
                             (LPARAM)HwndOfWidget(m_hwnd));
            }
        }
        return;
    }
    if (std::find(m_selSet.begin(), m_selSet.end(), true) != m_selSet.end()) {
        for (size_t i = 0; i < m_selSet.size(); ++i) m_selSet[i] = false;
        if (m_hwnd) Invalidate();
    }
}

void ListBox::SetSelectionRange(int start, int end, bool add)
{
    if ((int)m_selSet.size() != (int)m_items.size()) return;
    const int n = (int)m_items.size();
    if (start < 0 || end < 0 || start >= n) return;
    if (end >= n) end = n - 1;
    if (start > end) { const int t = start; start = end; end = t; }
    for (int i = start; i <= end; ++i) m_selSet[(size_t)i] = add;
    if (m_hwnd) {
        SyncScrollBar();
        Invalidate();
        SendMessageW(GetParent(HwndOfWidget(m_hwnd)), WM_COMMAND,
                     MAKEWPARAM(GetDlgCtrlID(HwndOfWidget(m_hwnd)), LBN_SELCHANGE),
                     (LPARAM)HwndOfWidget(m_hwnd));
    }
}

void ListBox::PaintItem(Canvas& g, int index, const Rect& rc)
{
    if (index < 0 || index >= (int)m_items.size()) return;
    // 行底 = 页面灰（卡片浮于其上），与旧实现保持一致
    g.Fill(rc, Pal().bg);
    if (!onPaintItem) return;
    ListItemDraw it;
    it.index         = index;
    it.data          = m_items[(size_t)index].data;
    it.rc            = rc;
    it.selected      = IsSelected(index);
    it.multiSelected = it.selected;
    it.hovered       = (index == m_hover);
    onPaintItem(g, it);
}

void ListBox::PaintSelf(DeviceContextHandle dc)
{
    if (!m_shell || !dc) return;
    RECT rc = {};
    GetClientRect(HwndOfWidget(m_hwnd), &rc);
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;

    Canvas g(dc, m_shell->UiFonts());
    g.Fill(FromWinRect(rc), Pal().bg);

    if (m_items.empty()) {
        if (onPaintEmpty) onPaintEmpty(g, FromWinRect(rc));
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
        PaintItem(g, (int)i, FromWinRect(row));
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
    HWND parent = HwndOfWidget(m_shell->Hwnd());

    m_hwnd = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
                             0, 0, 0, 0, parent,
                             (HMENU)(INT_PTR)m_id, nullptr, nullptr);
    if (!m_hwnd) return;

    SetWindowSubclass(HwndOfWidget(m_hwnd), &CustomProcBridge, kSubCustom, (DWORD_PTR)this);
}

MessageResult CustomProc(WindowHandle hwndH, MessageId msg, MessageWParam wParam,
                                    MessageLParam lParam, SubclassId idSub,
                                    SubclassRef ref)
{
    HWND hwnd = reinterpret_cast<HWND>(hwndH);
    auto* self = (Custom*)ref;

    switch (msg) {
    case WM_GETOBJECT:
        // 被 SetAccessible* 标注过才接管；STATIC 无原生 a11y provider，
        // 未标注时落回 DefWindowProc 的系统默认对象。
        if (LRESULT acc = a11y::HandleGetObjectIfAnnotated(hwnd, wParam, lParam))
            return acc;
        break;
    case WM_NCHITTEST:
        if (self && self->m_desc.mouseTransparent) {
            LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
            return (r == HTCLIENT) ? HTTRANSPARENT : r;   // 鼠标穿透（chip）
        }
        break;
    case WM_ERASEBKGND:
        return 1;   // 自绘全覆盖，免闪烁
    case WM_PAINT: {
        if (!self || !self->m_painter) break;
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(HwndOfWidget(hwnd), &rc);
        if (self->m_shell) {
            Canvas g(dc, self->m_shell->UiFonts());
            self->m_painter(g, FromWinRect(rc));
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    // ---- 可交互变体（设过 OnClick 才走；纯装饰直接落 DefSubclassProc）----
    case WM_MOUSEMOVE:
        if (self && self->m_click && !self->m_press) {
            if (!self->m_hover) {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
            self->m_hover = true;
            InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
        }
        return 0;
    case WM_MOUSELEAVE:
        if (self && self->m_click && self->m_hover) {
            self->m_hover = false;
            InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (self && self->m_click) {
            self->m_press = true;
            SetCapture(hwnd);
            InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP: {
        if (self && self->m_click && self->m_press) {
            self->m_press = false;
            ReleaseCapture();
            InvalidateRect(HwndOfWidget(hwnd), nullptr, FALSE);
            // 点在框外松手 = 取消（桌面按钮语义）
            RECT rc; GetClientRect(HwndOfWidget(hwnd), &rc);
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            if (PtInRect(&rc, pt)) {
                auto cb = self->m_click;
                if (cb) cb();
            }
        }
        return 0;
    }
    case WM_NCDESTROY:
        a11y::ForgetWindow(hwnd);   // 清无障碍属性表（幂等）
        RemoveWindowSubclass(HwndOfWidget(hwnd), &CustomProcBridge, idSub);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void Custom::OnClick(std::function<void()> cb)
{
    m_click = std::move(cb);
    // 设了点击回调 = 交互控件，关闭穿透（创建时的 mouseTransparent 仅
    // 适用于纯装饰用途）；回调置空则恢复穿透。
    m_desc.mouseTransparent = !m_click;
    if (m_hwnd) InvalidateRect(HwndOfWidget(m_hwnd), nullptr, TRUE);
}

void Custom::OnPaint(std::function<void(Canvas&, const Rect&)> painter)
{
    m_painter = std::move(painter);
    if (m_hwnd) InvalidateRect(HwndOfWidget(m_hwnd), nullptr, TRUE);
}

} // namespace ui
} // namespace air
