#pragma once
// ============================================================
// AirUI — OverlayWindow.h
//
// 自绘浮层窗口骨架。Toast 卡片与模态对话框都用它：一张 32bpp
// 离屏位图接收 Canvas / IPainter 绘制，再按「透明底色 / 遮罩底色」
// 两种规则逐像素补 alpha，最后用 UpdateLayeredWindow 上屏。
//
// 为什么不画在主窗口的 WM_PAINT 里：外壳带 WS_CLIPCHILDREN，
// 右下角那块被列表 / 编辑框等子窗口占着，父窗口画的内容会被裁掉；
// Skia 路径还会整窗合成覆盖。独立浮层既能盖住子控件，也不受
// 后端切换影响。
// ============================================================
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include "airui/Fonts.h"

namespace air {
namespace ui {

enum class OverlayAlphaMode
{
    Transparent,  // 底色全透明（toast 卡片之间的空隙）
    Scrim         // 底色是半透明黑（模态对话框的遮罩）
};

class OverlayWindow
{
public:
    // paint 在 32bpp 离屏 DC 上作画，坐标原点 = 浮层客户区左上角。
    using PaintFn = std::function<void(HDC dc, int width, int height)>;
    // 返回 true 表示消息已处理；false 交给 DefWindowProc。
    using EventFn = std::function<bool(UINT msg, WPARAM wp, LPARAM lp,
                                       LRESULT& result)>;

    OverlayWindow() = default;
    ~OverlayWindow();
    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    // owner 是主窗口：浮层随它最小化 / 保持在它上方，但不抢激活态。
    bool Create(HWND owner, const RECT& screenRect, bool activate);
    void Destroy();
    void Move(const RECT& screenRect);
    void Show(bool visible);

    void SetMode(OverlayAlphaMode mode) { m_mode = mode; }
    void SetOpacity(BYTE alpha)         { m_opacity = alpha; }
    void SetPaint(PaintFn fn)           { m_paint = std::move(fn); }
    void SetEvents(EventFn fn)          { m_events = std::move(fn); }
    void StartTimer(UINT ms);
    void Redraw();

    HWND Native() const { return m_hwnd; }

    // 窗口过程必须是公开静态函数，注册窗口类时要取地址。
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
                                    WPARAM wp, LPARAM lp);

private:
    LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    bool    EnsureSurface(int width, int height);
    void    ReleaseSurface();

    HWND    m_hwnd  = nullptr;
    HDC     m_mem   = nullptr;
    HBITMAP m_dib   = nullptr;
    HGDIOBJ m_old   = nullptr;
    void*   m_bits  = nullptr;
    int     m_w     = 0;
    int     m_h     = 0;
    int     m_x     = 0;
    int     m_y     = 0;
    bool    m_visible = false;
    bool    m_activate = false;

    OverlayAlphaMode m_mode    = OverlayAlphaMode::Transparent;
    BYTE             m_opacity = 255;
    PaintFn m_paint;
    EventFn m_events;
};

// ------------------------------------------------------------
// 自绘模态对话框。用 core 的 PaintModal 画卡片 + 按钮，键盘
// (Enter / Esc / Tab) 与鼠标 (悬停 / 按下 / 抬起) 都在这里闭环。
// 返回 IDOK / IDCANCEL / IDYES / IDNO，语义与 MessageBox 一致。
// ------------------------------------------------------------
int RunModalDialog(HWND owner, const Fonts& fonts,
                   const std::wstring& caption,
                   const std::wstring& text, UINT flags);

// ------------------------------------------------------------
// 自绘上下文菜单项。与 Page.h 的 MenuItem 分开定义，避免浮层骨架
// 反向依赖页面框架；Shell::ShowPopupMenu 负责两者之间的转换。
// ------------------------------------------------------------
struct OverlayMenuItem
{
    int          id        = 0;
    std::wstring label;
    bool         separator = false;
    bool         disabled  = false;
};

// 阻塞式弹出菜单，语义与 TrackPopupMenu 一致：返回被选中的命令 id，
// 取消返回 0。菜单由 core 的 PaintMenu 绘制，命中用同一份 MenuItemAt。
int RunPopupMenu(HWND owner, const Fonts& fonts, POINT anchor,
                 const std::vector<OverlayMenuItem>& items);

} // namespace ui
} // namespace air
