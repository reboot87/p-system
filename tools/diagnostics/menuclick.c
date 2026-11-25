// menuclick.c - find the self-drawn popup menu (AirUiOverlayWnd) and
// click one of its items via PostMessage (no real mouse needed).
// usage: menuclick <itemIndex> <pixelY>
//   itemIndex: item to click (0-based, includes separators)
//   pixelY:    click point in the menu's CLIENT y coordinate. The edit
//              menu mixes row heights (item rows 36px, the separator
//              row 6px), so an even height/rows split is wrong once the
//              menu grows; pass the measured y instead. For the current
//              5-item + separator edit menu (536px tall):
//                  row 0 undo    y=18   row 3 copy    y=126
//                  row 1 redo    y=54   row 4 cut     y=162
//                  row 2 (sep)   y=90   row 5 paste   y=198
// The menu handles WM_LBUTTONDOWN/UP with client coords, so posting
// them to the menu window drives its own hit-testing (MenuItemAt uses
// the real per-row geometry). More reliable than synthesized mouse
// events and does not depend on focus.
#include <windows.h>
#include <stdio.h>

static const wchar_t* kClass = L"AirUiOverlayWnd";
static HWND g_menu = NULL;
static int  g_cand = 0;

static BOOL CALLBACK enumCb(HWND h, LPARAM lp)
{
    (void)lp;
    wchar_t cls[64] = {0};
    GetClassNameW(h, cls, 64);
    if (wcscmp(cls, kClass) == 0 && IsWindowVisible(h)) {
        RECT r; GetWindowRect(h, &r);
        int w = r.right - r.left, hh = r.bottom - r.top;
        if (w > 50 && w < 500 && hh > 30 && hh < 400) {
            g_menu = h; g_cand++;   // keep the last candidate (topmost)
        }
    }
    return TRUE;
}

int main(int argc, char** argv)
{
    if (argc < 3) { printf("usage: menuclick <item> <pixelY>\n"); return 1; }
    int item = atoi(argv[1]), py = atoi(argv[2]);
    EnumWindows(enumCb, 0);
    if (!g_menu) { printf("NO_MENU (cand=%d)\n", g_cand); return 2; }
    RECT r; GetWindowRect(g_menu, &r);
    int w = r.right - r.left, hh = r.bottom - r.top;
    if (py < 0 || py >= hh) py = hh / 2;
    int cx = w / 2;
    printf("menu hwnd=%p rect=(%d,%d,%d,%d) %dx%d click item %d at client (%d,%d)\n",
           (void*)g_menu, r.left, r.top, r.right, r.bottom, w, hh, item, cx, py);
    LPARAM lp = MAKELPARAM(cx, py);
    // PostMessage（不是 SendMessage）：菜单的模态循环在 rclick 进程里
    // 阻塞于 GetMessage，SendMessage 会在本线程执行 wndproc、只改本
    // 线程看不到的 done 标志，循环线程永远醒不来。PostMessage 把事件
    // 投进队列，由模态循环线程自己按序派发 DOWN->UP。
    PostMessageW(g_menu, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    PostMessageW(g_menu, WM_LBUTTONUP,   0, lp);
    printf("posted down+up to item %d\n", item);
    // 等菜单窗口销毁（最多 3 秒）
    for (int i = 0; i < 300; ++i) {
        if (!IsWindow(g_menu)) { printf("menu closed\n"); return 0; }
        Sleep(10);
    }
    printf("menu STILL OPEN after 3s\n");
    return 3;
}
