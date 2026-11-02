#pragma once
// ============================================================
// AirUI — Page.h
// 页面接口：Shell 负责外壳（标题栏 / 侧栏 / 状态栏 / 双缓冲），
// 内容区完全交给页面，页面之间互不依赖。
//
// 一个页面 = 一块自绘内容 + 自己的子控件 + 自己的事件响应。
// 新增功能（文件监控 / 打印监控…）只需再实现一个 IPage。
// ============================================================
#include <windows.h>

namespace air {
namespace ui {

class Shell;

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

} // namespace ui
} // namespace air
