#pragma once
// ============================================================
// AirUI — Controls.h
// 通用控件与窗口绘制辅助
//
//   DrawButton       主/次/危险按钮的统一外观
//   AttachButton     OWNERDRAW 按钮的 hover / pressed 跟踪
//   NavBar           侧栏导航（分组 + 条目 + 角标 + hover）
//   PaintDC          双缓冲 WM_PAINT（整体绘入内存位图后一次上屏）
//   EmptyState       列表空状态的三段式引导
// ============================================================
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include "airui/Theme.h"
#include "airui/ControlsCore.h"
#include "airui/Fonts.h"
#include "airui/Controls.h"   // BtnStyle / EmptyState / GetEditText (public)

namespace air {
namespace ui {

// ============================================================
// 原生 EDIT 工具（自绘 TextBox 仍走 WM_GETTEXT 兼容路径；
// 这里的低层函数保留给集成层，页面不再使用）
// ============================================================
// 输入框 8px 文字内边距（企业级输入规范）
void SetEditMargins(HWND edit, int left = 8, int right = 8);
// 清空 EDIT（EM_SETSEL + EM_REPLACESEL，多行单行均可靠）
void ClearEditText(HWND edit);
// 光标移到末尾（编辑已有内容时续写）
void MoveCaretToEnd(HWND edit);
// GetEditText / BtnStyle now come from the public airui/Controls.h.

// ============================================================
// 按钮
// ============================================================

void DrawButton(HDC dc, const RECT& rc, const wchar_t* text, FontHandle font,
                BtnStyle style, bool hover, bool pressed, bool disabled = false);

// OWNERDRAW 按钮的 hover / pressed 跟踪。
// 用法：AttachButton(hBtn, BtnStyle::Primary)；
// 父窗口 WM_DRAWITEM 里查 IsButtonHover / IsButtonPressed 后调 DrawButton。
void AttachButton(HWND button, BtnStyle style);
void DetachButton(HWND button);
bool IsButtonHover(HWND button);
bool IsButtonPressed(HWND button);
BtnStyle ButtonStyleOf(HWND button);

// ============================================================
// 鼠标悬停跟踪（控件级）
// ============================================================
namespace hover {
// 在 WM_MOUSEMOVE / WM_MOUSELEAVE 中调用；返回 true 表示状态变化需重绘
bool OnMouseMove(HWND hwnd, bool& state);
bool OnMouseLeave(HWND hwnd, bool& state);
} // namespace hover

// NavBar now comes from the public airui/Controls.h.

// ============================================================
// 双缓冲绘制
// ============================================================
class PaintDC
{
public:
    // Persistent back buffer. Creating a full-size compatible bitmap and
    // DC on every WM_PAINT cost 0.4-2.7 ms of a 3-8 ms GDI frame, and a
    // DIB's contents are reusable: the next frame overwrites the region
    // it presents anyway. The owner keeps one of these for the window's
    // lifetime and hands it in; PaintDC grows/releases it on resize.
    using BackBuffer = air::ui::BackBuffer;

    // useBackBuffer=false paints straight onto the BeginPaint DC. That is
    // only correct for a backend which has its own back buffer and
    // blits the finished client area (the Skia path); for GDI it would
    // flicker, so the default stays buffered.
    // cache == nullptr keeps the old allocate-per-frame behaviour.
    explicit PaintDC(HWND hwnd, bool useBackBuffer = true,
                     BackBuffer* cache = nullptr);
    ~PaintDC();

    PaintDC(const PaintDC&) = delete;
    PaintDC& operator=(const PaintDC&) = delete;

    HDC         dc() const { return m_mem ? m_mem : m_wdc; }
    const RECT& client() const { return m_client; }
    int         width()  const { return m_client.right; }
    int         height() const { return m_client.bottom; }

    // The rectangle Windows actually asked to repaint (BeginPaint's
    // rcPaint, clamped to the client). Equals client() after a whole-window
    // invalidate. The GDI back-buffer path clips its drawing to this and
    // only blits this, so a hover that dirties one row no longer pays for
    // a full-window frame.
    const RECT& update() const { return m_update; }
    bool        backBuffer() const { return m_mem != nullptr; }

    // 把内存位图整体上屏（只能调用一次；析构时若未调用会自动补一次空 EndPaint）
    void Flush();

private:
    HWND        m_hwnd = nullptr;
    PAINTSTRUCT m_ps   = {};
    HDC         m_wdc  = nullptr;
    HDC         m_mem  = nullptr;
    HBITMAP     m_bmp  = nullptr;
    HGDIOBJ     m_old  = nullptr;
    RECT        m_client = {};
    RECT        m_update = {};
    BackBuffer* m_cache  = nullptr;   // when non-null, m_mem/m_bmp belong to it
    bool        m_began = false;
    bool        m_flushed = false;
};

// ============================================================
// 列表工具
// ============================================================
namespace list {
// 客户区坐标 → 列表项下标；-1 = 空白/越界
int ItemFromPoint(HWND listbox, POINT clientPt);
} // namespace list

// EmptyState now comes from the public airui/Controls.h.
void DrawEmptyState(HDC dc, const RECT& rc, const Fonts& fonts,
                    const EmptyState& state);

// ============================================================
// 窗口外壳辅助
// ============================================================
// 给无边框窗口加 DWM 阴影 + Win11 圆角（Win10 上静默失败）
void ApplyDwmFrame(HWND hwnd);

} // namespace ui
} // namespace air
