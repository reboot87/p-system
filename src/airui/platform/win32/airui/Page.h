#pragma once
// ============================================================
// AirUI — Page.h
// 页面接口：Shell 负责外壳（标题栏 / 侧栏 / 状态栏 / 双缓冲），
// 内容区完全交给页面，页面之间互不依赖。
//
// 页面作者请继承 Page（高级接口）：
//   · 绘制走 Canvas —— 不接触 HDC / HFONT
//   · 控件走 CreateTextBox / CreateButton / CreateListBox
//     —— 不接触 HWND / 消息 / 子类化
//   · 定时器、弹窗、菜单走 StartTimer / ShowMessageBox / ShowPopupMenu
//
// IPage 仍保留为底层接口（库自身与特殊页面用），业务不要直接继承。
// ============================================================
#include <windows.h>
#include <memory>
#include <vector>
#include "airui/Widgets.h"

namespace air {
namespace ui {

class Shell;

// ---- 底层页面接口（库内部使用；业务页面请继承 Page） ----
class IPage
{
public:
    virtual ~IPage() = default;

    // ---- 元信息（侧栏与标题栏据此渲染） ----
    virtual const wchar_t* Title() const = 0;               // 标题栏副标题
    virtual const wchar_t* NavGlyph() const { return L""; }  // Segoe MDL2 字形
    virtual const wchar_t* NavLabel() const { return Title(); }
    virtual bool           NavDisabled() const { return false; }

    // ---- 生命周期 ----
    virtual void OnAttach(Shell& shell) { (void)shell; }
    virtual void OnDetach() {}
    virtual void OnShow(bool shown) { (void)shown; }

    // ---- 布局与绘制 ----
    // content 已排除标题栏、侧栏、状态栏与页面外边距
    virtual void OnLayout(const RECT& content) { (void)content; }
    virtual void OnPaint(HDC dc, const RECT& content) { (void)dc; (void)content; }

    // ---- 输入（坐标相对内容区，即已减去 content.left/top） ----
    virtual bool OnMouseDown(POINT pt) { (void)pt; return false; }
    virtual bool OnMouseMove(POINT pt) { (void)pt; return false; }
    virtual bool OnMouseLeave() { return false; }
    virtual bool OnCommand(int ctrlId, int code) { (void)ctrlId; (void)code; return false; }
    virtual bool OnTimer(UINT_PTR id) { (void)id; return false; }
    virtual void OnSetFocus() {}

    // owner-draw 控件：返回 true 表示页面已绘制
    virtual bool OnDrawItem(const DRAWITEMSTRUCT* dis) { (void)dis; return false; }
    virtual bool OnMeasureItem(MEASUREITEMSTRUCT* mis) { (void)mis; return false; }

    // ---- 页面内控件挂在哪 ----
    HWND ParentWnd() const;

protected:
    Shell* m_shell = nullptr;
    friend class Shell;
};

// ---- 弹出菜单项 ----
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
    // ---- 高级绘制钩子（页面实现这个，而不是 HDC 版本） ----
    virtual void OnPaint(Canvas& g, const RECT& content)
    { (void)g; (void)content; }

    // ---- 控件工厂（所有权归页面；父窗口销毁时自动清理） ----
    TextBox& CreateTextBox(const TextBoxDesc& desc);
    Button&  CreateButton(const ButtonDesc& desc);
    ListBox& CreateListBox(const ListBoxDesc& desc);
    Custom&  CreateCustom(const CustomDesc& desc);

    // ---- 常用操作（Win32 细节由库承担） ----
    void StartTimer(UINT_PTR id, int ms);
    void KillTimer(UINT_PTR id);
    int  ShowMessageBox(const std::wstring& text, const std::wstring& caption,
                        UINT flags = MB_OK);
    // 返回被点击的命令 id；0 = 未选择
    int  ShowPopupMenu(POINT screenPt, const std::vector<MenuItem>& items);
    void InvalidateRegion(const RECT& rcClient);   // 客户区局部重绘
    void BatchDraw(bool begin);                    // WM_SETREDRAW 批量绘制

    const Fonts& Fonts() const;

    // ---- IPage 绘制入口收编：页面无需也无法再触碰 HDC / DRAWITEMSTRUCT ----
    void OnPaint(HDC dc, const RECT& content) final;
    bool OnDrawItem(const DRAWITEMSTRUCT* dis) final;
    bool OnMeasureItem(MEASUREITEMSTRUCT* mis) final;

protected:
    std::vector<std::unique_ptr<Widget>> m_widgets;
};

} // namespace ui
} // namespace air
