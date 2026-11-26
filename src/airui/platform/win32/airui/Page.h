#pragma once
// ============================================================
// AirUI — Page.h
// 页面接口：Shell 负责外壳（标题栏 / 侧栏 / 状态栏 / 双缓冲），
// 内容区完全交给页面，页面之间互不依赖。
//
// Page authors should inherit Page (high-level API):
//   · painting goes through Canvas - no native handles
//   · 控件走 CreateTextBox / CreateButton / CreateListBox
//     - no native window / messages / subclassing
//   · timers, dialogs and menus go through StartTimer / ShowMessageBox / ShowPopupMenu
//
// IPage stays as the low-level interface for the library itself.
// ============================================================
#include <memory>
#include <vector>
#include "airui/Widgets.h"

namespace air {
namespace ui {

class Shell;

// ---- low-level page interface (internal; page authors inherit Page) ----
class IPage
{
public:
    virtual ~IPage() = default;

    // ---- metadata (sidebar and title bar render from this) ----
    virtual const wchar_t* Title() const = 0;               // 标题栏副标题
    virtual const wchar_t* NavGlyph() const { return L""; }  // Segoe MDL2 字形
    virtual const wchar_t* NavLabel() const { return Title(); }
    virtual bool           NavDisabled() const { return false; }

    // ---- lifecycle ----
    virtual void OnAttach(Shell& shell) { (void)shell; }
    virtual void OnDetach() {}
    virtual void OnShow(bool shown) { (void)shown; }

    // ---- layout and painting ----
    // content excludes title bar, sidebar, status bar and page margins
    virtual void OnLayout(const Rect& content) { (void)content; }
    virtual void OnPaint(DeviceContextHandle dc, const Rect& content) { (void)dc; (void)content; }

    // ---- input (coords relative to content origin) ----
    virtual bool OnMouseDown(Point pt) { (void)pt; return false; }
    // self-drawn buttons have no native click: press changes visuals, action on release
    // (drag out then release = cancel). Returns true to request repaint.
    virtual bool OnMouseUp(Point pt) { (void)pt; return false; }
    virtual bool OnMouseMove(Point pt) { (void)pt; return false; }
    virtual bool OnMouseLeave() { return false; }
    // return true over a clickable self-drawn element to show a hand cursor.
    // native controls manage their own cursor.
    virtual bool HandCursorAt(Point pt) { (void)pt; return false; }
    virtual bool OnCommand(int ctrlId, int code) { (void)ctrlId; (void)code; return false; }
    virtual bool OnTimer(uintptr_t id) { (void)id; return false; }
    virtual void OnSetFocus() {}

    // owner-drawn control: return true if the page painted it
    virtual bool OnDrawItem(const DrawItemInfo* dis) { (void)dis; return false; }
    virtual bool OnMeasureItem(MeasureItemInfo* mis) { (void)mis; return false; }

    // ---- which window hosts page controls ----
    WindowHandle ParentWnd() const;

protected:
    Shell* m_shell = nullptr;
    friend class Shell;
};

// ---- popup menu item ----
struct MenuItem
{
    int          id    = 0;
    std::wstring label;
    bool         separator = false;
    bool         disabled  = false;
};

// ============================================================
// Page —— 业务页面基类（推荐）
// ============================================================
class Page : public IPage
{
public:
    // ---- high-level paint hook (implement this instead of the low-level one) ----
    virtual void OnPaint(Canvas& g, const Rect& content)
    { (void)g; (void)content; }

    // ---- control factories (owned by the page, cleaned up with the parent) ----
    TextBox& CreateTextBox(const TextBoxDesc& desc);
    Button&  CreateButton(const ButtonDesc& desc);
    ListBox& CreateListBox(const ListBoxDesc& desc);
    Custom&  CreateCustom(const CustomDesc& desc);

    // ---- common operations (native details handled by the library) ----
    void StartTimer(uintptr_t id, int ms);
    void KillTimer(uintptr_t id);
    int  ShowMessageBox(const std::wstring& text, const std::wstring& caption,
                        MessageBoxFlags flags = mb::Ok);
    // returns the clicked command id; 0 = none
    int  ShowPopupMenu(Point screenPt, const std::vector<MenuItem>& items);
    void InvalidateRegion(const Rect& rcClient);   // client-area partial repaint
    void BatchDraw(bool begin);                    // batched redraw

    // 自绘热区按住时的鼠标捕获：指针拖出窗口也要能收到抬起消息，
    // 否则"拖出去松手"会变成一次点击。成对调用。
    void SetCapture(bool on);

    const Fonts& Fonts() const;

    // ---- low-level paint hooks are finalized here; pages never touch native structures ----
    void OnPaint(DeviceContextHandle dc, const Rect& content) final;
    bool OnDrawItem(const DrawItemInfo* dis) final;
    bool OnMeasureItem(MeasureItemInfo* mis) final;

protected:
    std::vector<std::unique_ptr<Widget>> m_widgets;
};

} // namespace ui
} // namespace air
