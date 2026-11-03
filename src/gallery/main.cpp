// ============================================================
// AirUI Gallery — 可视化组件画廊
//
// 这是 airui_gallery.exe 的入口：一个真实 Win32 窗口，用 AirUI
// 的 Shell / Page / Canvas 绘制完整的组件画廊，支持：
//   · 鼠标滚轮 / 方向键 / PageUp / PageDown / Home / End 滚动
//   · 按 T 在浅色与深色主题之间切换
//   · 状态栏显示当前滚动位置与快捷键
// ============================================================
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <memory>
#include <string>

#include "airui/AirUi.h"
#include "GalleryDraw.h"

using namespace air::ui;

namespace {

constexpr int kScrollStep = 72;

class GalleryPage : public Page
{
public:
    const wchar_t* Title() const override { return L"组件画廊"; }

    void OnAttach(Shell& shell) override
    {
        m_shell = &shell;
        m_content = shell.ContentRect();

        shell.SetCaptionSubtitle(L"Canvas · Gfx · Components");
        shell.SetStatusKeys({
            { L"滚轮", L"滚动" },
            { L"T", L"切换主题" },
            { L"Home / End", L"顶部 / 底部" },
        });

        shell.SetMessageHook(
            [this](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                   LRESULT& result) -> bool {
                (void)hwnd;
                (void)lParam;

                if (msg == WM_MOUSEWHEEL) {
                    int steps = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
                    if (steps == 0) {
                        steps = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1;
                    }
                    ScrollBy(-steps * kScrollStep);
                    result = 0;
                    return true;
                }

                if (msg == WM_KEYDOWN) {
                    switch (wParam) {
                    case 'T':
                        ToggleTheme();
                        result = 0;
                        return true;
                    case VK_HOME:
                        SetScroll(0);
                        result = 0;
                        return true;
                    case VK_END:
                        SetScroll(MaxScroll());
                        result = 0;
                        return true;
                    case VK_PRIOR:
                        ScrollBy(-(ViewHeight() - 40));
                        result = 0;
                        return true;
                    case VK_NEXT:
                        ScrollBy(ViewHeight() - 40);
                        result = 0;
                        return true;
                    case VK_UP:
                        ScrollBy(-40);
                        result = 0;
                        return true;
                    case VK_DOWN:
                        ScrollBy(40);
                        result = 0;
                        return true;
                    default:
                        break;
                    }
                }
                return false;
            });

        UpdateStatus();
    }

    void OnLayout(const RECT& content) override
    {
        m_content = content;
        m_scroll = std::min(m_scroll, MaxScroll());
    }

    void OnShow(bool shown) override
    {
        if (shown) UpdateStatus();
    }

    void OnPaint(Canvas& g, const RECT& content) override
    {
        const Palette& p = Pal();
        g.Fill(content, p.bg);

        // 内容整体上移 m_scroll 像素，并裁剪到内容区，模拟滚动。
        HDC dc = g.Native();
        int saved = 0;
        if (dc) {
            saved = SaveDC(dc);
            IntersectClipRect(dc, content.left, content.top,
                              content.right, content.bottom);
        }

        gallery::DrawGallery(g, content.left, content.top - m_scroll,
                             content.right - content.left, m_dark);

        if (dc && saved) RestoreDC(dc, saved);

        DrawScrollBar(g, content);
    }

private:
    int ViewHeight() const
    {
        int h = static_cast<int>(m_content.bottom - m_content.top);
        return h > 0 ? h : 0;
    }

    int MaxScroll() const
    {
        return std::max(0, gallery::kGalleryHeight - ViewHeight());
    }

    void SetScroll(int value)
    {
        int clamped = std::max(0, std::min(value, MaxScroll()));
        if (clamped == m_scroll) return;
        m_scroll = clamped;
        if (m_shell) m_shell->Invalidate();
        UpdateStatus();
    }

    void ScrollBy(int delta) { SetScroll(m_scroll + delta); }

    void ToggleTheme()
    {
        m_dark = !m_dark;
        SetPalette(m_dark ? DarkPalette() : LightPalette());
        if (m_shell) m_shell->Invalidate();
        UpdateStatus();
    }

    void UpdateStatus()
    {
        if (!m_shell) return;

        int maxScroll = MaxScroll();
        int percent = maxScroll > 0
                          ? (m_scroll * 100 + maxScroll / 2) / maxScroll
                          : 0;

        wchar_t extra[96] = {};
        swprintf_s(extra, L"%s · %d%% · 总高 %d px",
                   m_dark ? L"深色主题" : L"浅色主题",
                   percent, gallery::kGalleryHeight);

        m_shell->SetStatus(L"组件画廊", true);
        m_shell->SetStatusExtra(extra);
        m_shell->SetCaptionBadge(m_dark ? L"Dark" : L"Light", true);
    }

    void DrawScrollBar(Canvas& g, const RECT& content)
    {
        int viewH = content.bottom - content.top;
        int maxScroll = MaxScroll();
        if (viewH <= 0 || maxScroll <= 0) return;

        const Palette& p = Pal();
        const int trackW = 6;
        const int x = content.right - trackW - 2;

        RECT track = { x, content.top, x + trackW, content.bottom };
        g.FillRound(track, p.split, trackW / 2);

        int thumbH = std::max(40, viewH * viewH / gallery::kGalleryHeight);
        thumbH = std::min(thumbH, viewH);
        int travel = viewH - thumbH;
        int thumbY = content.top + travel * m_scroll / maxScroll;

        RECT thumb = { x, thumbY, x + trackW, thumbY + thumbH };
        g.FillRound(thumb, p.textDisabled, trackW / 2);
    }

    RECT  m_content = {};
    int   m_scroll  = 0;
    bool  m_dark    = false;
};

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    SetProcessDPIAware();

    // 进程可能被反复启动，先恢复默认浅色主题，避免受上一次调色板影响。
    SetPalette(LightPalette());

    ShellOptions opt;
    opt.className  = L"AirUiGalleryWnd";
    opt.appName    = L"AirUI Gallery";
    opt.logoLetter = L"A";
    opt.width      = 1280;
    opt.height     = 920;
    opt.minWidth   = 960;
    opt.minHeight  = 640;
    opt.sidebar    = false;
    opt.statusBar  = true;
    opt.dark       = false;

    Shell shell;
    if (!shell.Create(hInstance, opt)) {
        MessageBoxW(nullptr, L"创建 AirUI Gallery 窗口失败。",
                    L"AirUI Gallery", MB_ICONERROR | MB_OK);
        return 1;
    }

    shell.AddPage(1, std::make_unique<GalleryPage>());
    shell.SwitchPage(1);

    return shell.RunMessageLoop();
}
