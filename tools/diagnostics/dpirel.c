// dpirel.c - 3a 高 DPI 动态适配的跨进程探针
//
// 本机两块屏都是 96 DPI，无法真实跨屏触发 WM_DPICHANGED。探针改从外部
// PostMessage(WM_DPICHANGED, newDpi, 0) 驱动 Shell 的处理路径（lParam=0：
// 跨进程不能传指针，Shell 的 handler 对「无建议矩形」走原地重排分支）。
// 然后读回可观测结果断言 handler 真的跑了：
//   - 子控件（AirUITextBox）客户区高度：搜索框 = Metrics().searchH，
//     96→150 时 36→54；备注框 inputH 32→48（Metrics 经 ForDpi 重建后
//     页面 OnLayout → Move 的结果）
//   - 列表首行高：LB_GETITEMRECT（subclassed shim），行高 = Mx().itemH，
//     96→150 时 64→96（RebuildListForDpi 重建列表的结果）
//   - GetDpiForWindow 只反映真实显示器 DPI（恒 96），打印参考、不断言
//
// Usage:
//   dpirel.exe find                 print decimal hwnd of AireRecordWnd
//   dpirel.exe <hwndDec>            report win rect + dpi + child heights
//   dpirel.exe <hwndDec> <newDpi>   post WM_DPICHANGED(newDpi,0), wait, report
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static HWND g_w = 0;

// GetDpiForWindow 在 shcore（Win8+）。动态加载避免驱动目标多挂一个库。
static UINT WindowDpi(HWND h)
{
    static UINT (WINAPI *p)(HWND) = NULL;
    if (!p) {
        HMODULE m = LoadLibraryW(L"shcore.dll");
        if (m) p = (UINT (WINAPI *)(HWND))GetProcAddress(m, "GetDpiForWindow");
    }
    return p ? p(h) : 0;
}

static int CALLBACK CbBox(HWND h, LPARAM l)
{
    wchar_t cls[64];
    GetClassNameW(h, cls, 64);
    if (lstrcmpiW(cls, L"AirUITextBox") == 0) {
        RECT r;
        GetClientRect(h, &r);
        printf("  box id=%d w=%d h=%d\n", GetDlgCtrlID(h),
               r.right - r.left, r.bottom - r.top);
    }
    return TRUE;
}

static int g_rowH0 = -1, g_rowH1 = -1;
static int CALLBACK CbList(HWND h, LPARAM l)
{
    wchar_t cls[64];
    GetClassNameW(h, cls, 64);
    if (lstrcmpiW(cls, L"AirUIListBox") == 0) {
        RECT r = {0, 0, 0, 0};
        if (SendMessageW(h, LB_GETITEMRECT, 0, (LPARAM)&r) == LB_OKAY)
            g_rowH0 = r.bottom - r.top;
        if (SendMessageW(h, LB_GETITEMRECT, 1, (LPARAM)&r) == LB_OKAY)
            g_rowH1 = r.bottom - r.top;
        return FALSE;
    }
    return TRUE;
}

static void Report(const char* tag)
{
    RECT wr = {0, 0, 0, 0};
    UINT dpi = 0;
    GetWindowRect(g_w, &wr);
    dpi = WindowDpi(g_w);
    g_rowH0 = -1; g_rowH1 = -1;
    EnumChildWindows(g_w, CbBox, 0);
    EnumChildWindows(g_w, CbList, 0);
    printf("%s win=(%d,%d,%d,%d) monitorDpi=%u row0H=%d row1H=%d\n",
           tag, wr.left, wr.top, wr.right, wr.bottom, dpi, g_rowH0, g_rowH1);
}

int main(int argc, char** argv)
{
    if (argc >= 2 && !strcmp(argv[1], "find")) {
        HWND w = FindWindowW(L"AireRecordWnd", NULL);
        if (!w) { printf("not found\n"); return 1; }
        printf("%ld\n", (long)w);
        return 0;
    }
    if (argc < 2) { printf("usage: dpirel find | dpirel <hwnd> [newDpi|f3]\n"); return 1; }
    g_w = (HWND)(intptr_t)atoi(argv[1]);
    if (!IsWindow(g_w)) { printf("not a window\n"); return 1; }
    if (argc >= 3) {
        if (!strcmp(argv[2], "f3")) {
            // F3 加速键走 Shell 消息循环的 m_accel（读投递的 MSG 本身），
            // 跨进程 PostMessage 可达（不同于 GetKeyState 键盘态）。
            PostMessageW(g_w, WM_KEYDOWN, VK_F3, 0);
            PostMessageW(g_w, WM_KEYUP, VK_F3, 0);
            Sleep(800);   // SwitchPage(1) → ClipsPage OnLayout + 列表填充
        } else {
            const UINT dpi = (UINT)atoi(argv[2]);
            // 用 SendMessage 而非 PostMessage 注入：本 Windows 构建会拒绝
            // 把 WM_DPICHANGED 投递进消息队列（PostMessage 返回失败 GLE
            // 1159，进程内自投也如此——它是窗口管理器独占投递的消息），
            // 但 SendMessage 能直达 WndProc。生产环境里该消息由窗口管理器
            // 直接派发，不经队列，所以这条路径等价于真实投递。
            // lParam=0：跨进程不能传指针，走 handler 的「无建议矩形」分支
            // （原地 LayoutAll，不改窗口大小）。
            LRESULT r = SendMessageW(g_w, WM_DPICHANGED, (WPARAM)dpi, 0);
            printf("SendMessage(WM_DPICHANGED %u) ret=%ld\n", dpi, (long)r);
            Sleep(300);   // 让失效/重排落地再读几何
        }
    }
    Report("probe");
    return 0;
}
