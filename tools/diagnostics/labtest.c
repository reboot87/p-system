// labtest.c - TextBoxLab cross-process driver (manual diagnostic)
//
// Requires a running airui_textbox_lab.exe (window class AirUiTextBoxLab).
// All commands address the boxes by dialog control id (3001..3004).
//
// Usage:
//   labtest.exe info                       list boxes + system caret blink time
//   labtest.exe rect                       screen rects of boxes 3001..3004
//   labtest.exe type  <id> "text"          WM_CHAR per char (simulate typing)
//   labtest.exe key   <id> <vkHex>         press a key (vk hex, e.g. 08 = Back)
//   labtest.exe focus <id> <x> <y>         click client (x,y) (left down+up)
//   labtest.exe clear <id>                 WM_SETTEXT "" (deterministic reset)
//   labtest.exe setsel <id> <start> <end>  EM_SETSEL (end may be -1 = to end)
//   labtest.exe getsel <id>                EM_GETSEL -> "sel=[begin,end)"
//   labtest.exe getraw                     WM_GETTEXTLENGTH/WM_GETTEXT per box
//   labtest.exe dblclick <id> <x> <y>      WM_LBUTTONDBLCLK at client (x,y)
//   labtest.exe rclick  <id> <x> <y>       WM_RBUTTONUP  at client (x,y)
//                                         BLOCKS while the edit menu is open
//                                         (run it in the background)
//   labtest.exe drag    <id> <x1> <x2> <y> left-button down at x1, move to
//                                         x2 in 10 steps, up (drag-select)
//
// Notes:
// - GetWindowTextW returns empty across processes here, so read text via
//   getraw (WM_GETTEXT) and selection via getsel (EM_GETSEL).
// - The proc sends real WM_* messages to the child boxes; the menu opened by
//   rclick is a blocking modal loop inside RunPopupMenu, so `rclick` does not
//   return until the menu closes (pair it with menuclick.exe).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static HWND g_shell = 0;
static HWND g_found = 0;

static BOOL CALLBACK CbList(HWND h, LPARAM l)
{
    (void)l;
    wchar_t cls[64] = {};
    GetClassNameW(h, cls, 64);
    if (lstrcmpiW(cls, L"AirUITextBox") != 0) return TRUE;
    RECT rc = {};
    GetClientRect(h, &rc);
    wchar_t txt[256] = {};
    GetWindowTextW(h, txt, 256);
    printf("  id=%d vis=%d client=%dx%d text=[%ls]\n",
           GetDlgCtrlID(h), IsWindowVisible(h) ? 1 : 0,
           rc.right - rc.left, rc.bottom - rc.top, txt);
    return TRUE;
}

static BOOL CALLBACK CbFind(HWND h, LPARAM l)
{
    wchar_t cls[64] = {};
    GetClassNameW(h, cls, 64);
    if (lstrcmpiW(cls, L"AirUITextBox") != 0) return TRUE;
    if (GetDlgCtrlID(h) == (int)l) { g_found = h; return FALSE; }
    return TRUE;
}

static int NeedBox(const char* what, int argc, char** argv)
{
    if (argc < 3) { printf("usage: %s <id> ...\n", what); return 0; }
    g_found = 0;
    EnumChildWindows(g_shell, CbFind, (LPARAM)atoi(argv[2]));
    if (!g_found) { printf("box id=%s not found\n", argv[2]); return 0; }
    return 1;
}

int main(int argc, char** argv)
{
    g_shell = FindWindowW(L"AirUiTextBoxLab", NULL);
    if (!g_shell) { printf("no lab window\n"); return 1; }
    ShowWindow(g_shell, SW_RESTORE);
    SetForegroundWindow(g_shell);
    printf("shell=%p caretBlinkTime=%lu\n", (void*)g_shell,
           (unsigned long)GetCaretBlinkTime());

    if (argc >= 2 && !strcmp(argv[1], "info")) {
        EnumChildWindows(g_shell, CbList, 0);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "rect")) {
        for (int i = 3001; i <= 3004; ++i) {
            g_found = 0;
            EnumChildWindows(g_shell, CbFind, (LPARAM)i);
            if (!g_found) continue;
            RECT rc = {};
            GetWindowRect(g_found, &rc);
            printf("  id=%d screen=(%d,%d,%d,%d) %dx%d\n", i,
                   rc.left, rc.top, rc.right, rc.bottom,
                   rc.right - rc.left, rc.bottom - rc.top);
        }
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "clear")) {
        if (!NeedBox("clear", argc, argv)) return 1;
        // WM_SETTEXT with "" is the deterministic clear: the box has a
        // standard shim that resets text/cursor/selection and rebuilds the
        // layout. (Earlier EM_SETSEL select-all + VK_BACK sent back-to-back
        // cross-process could silently drop the Backspace - harness timing
        // artifact, not a product defect.)
        SetFocus(g_found);
        SendMessageW(g_found, WM_SETTEXT, 0, (LPARAM)L"");
        printf("cleared\n");
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "setsel")) {
        // setsel <id> <start> <end>  -> EM_SETSEL (end may be -1 for "to end")
        if (!NeedBox("setsel", argc, argv)) return 1;
        if (argc < 5) { printf("usage: setsel <id> <start> <end>\n"); return 1; }
        int start = atoi(argv[3]), end = atoi(argv[4]);
        SetFocus(g_found);
        SendMessageW(g_found, EM_SETSEL, (WPARAM)(unsigned short)start, (LPARAM)(short)end);
        printf("setsel %d..%d\n", start, end);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "getsel")) {
        if (!NeedBox("getsel", argc, argv)) return 1;
        short sel[2] = { 0, 0 };
        SendMessageW(g_found, EM_GETSEL, 0, (LPARAM)sel);
        printf("sel=[%d,%d)\n", sel[0], sel[1]);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "dblclick")) {
        if (!NeedBox("dblclick", argc, argv)) return 1;
        if (argc < 5) { printf("usage: dblclick <id> <x> <y>\n"); return 1; }
        int x = atoi(argv[3]), y = atoi(argv[4]);
        SetFocus(g_found);
        LPARAM lp = MAKELPARAM(x, y);
        SendMessageW(g_found, WM_LBUTTONDBLCLK, MK_LBUTTON, lp);
        printf("dblclick at (%d,%d)\n", x, y);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "rclick")) {
        if (!NeedBox("rclick", argc, argv)) return 1;
        if (argc < 5) { printf("usage: rclick <id> <x> <y>\n"); return 1; }
        int x = atoi(argv[3]), y = atoi(argv[4]);
        SetFocus(g_found);
        LPARAM lp = MAKELPARAM(x, y);
        SendMessageW(g_found, WM_RBUTTONUP, 0, lp);
        printf("rclick at (%d,%d)\n", x, y);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "drag")) {
        // down at x1, move through a few steps to x2, up
        if (!NeedBox("drag", argc, argv)) return 1;
        if (argc < 6) { printf("usage: drag <id> <x1> <x2> <y>\n"); return 1; }
        int x1 = atoi(argv[3]), x2 = atoi(argv[4]), y = atoi(argv[5]);
        SetFocus(g_found);
        SendMessageW(g_found, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x1, y));
        int steps = 10;
        for (int i = 1; i <= steps; i++) {
            int xx = x1 + (x2 - x1) * i / steps;
            SendMessageW(g_found, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(xx, y));
        }
        SendMessageW(g_found, WM_LBUTTONUP, 0, MAKELPARAM(x2, y));
        printf("drag %d..%d at y=%d\n", x1, x2, y);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "getraw")) {
        // raw WM_GETTEXTLENGTH / WM_GETTEXT per box (bypass GetWindowTextW,
        // which returns empty across processes)
        int i = 0;
        HWND h;
        for (i = 3001; i <= 3004; ++i) {
            g_found = 0;
            EnumChildWindows(g_shell, CbFind, (LPARAM)i);
            if (!g_found) { printf("  id=%d missing\n", i); continue; }
            int len = (int)SendMessageW(g_found, WM_GETTEXTLENGTH, 0, 0);
            wchar_t buf[512] = {};
            int got = (int)SendMessageW(g_found, WM_GETTEXT, 512, (LPARAM)buf);
            printf("  id=%d GETTEXTLENGTH=%d GETTEXT got=%d [%ls]\n",
                   i, len, got, buf);
        }
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "focus")) {
        if (!NeedBox("focus", argc, argv)) return 1;
        if (argc < 5) { printf("usage: focus <id> <x> <y>\n"); return 1; }
        int x = atoi(argv[3]), y = atoi(argv[4]);
        SetFocus(g_found);
        LPARAM lp = MAKELPARAM(x, y);
        SendMessageW(g_found, WM_LBUTTONDOWN, MK_LBUTTON, lp);
        SendMessageW(g_found, WM_LBUTTONUP, 0, lp);
        printf("focus at (%d,%d)\n", x, y);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "type")) {
        if (!NeedBox("type", argc, argv)) return 1;
        if (argc < 4) { printf("usage: type <id> <text>\n"); return 1; }
        SetFocus(g_found);
        const char* s = argv[3];
        for (; *s; ++s)
            SendMessageW(g_found, WM_CHAR, (WPARAM)(unsigned char)*s, 1);
        wchar_t buf[512] = {};
        int n = GetWindowTextW(g_found, buf, 512);
        printf("typed, text now=[%ls] len=%d\n", buf, n);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "key")) {
        if (!NeedBox("key", argc, argv)) return 1;
        if (argc < 4) { printf("usage: key <id> <vkHex>\n"); return 1; }
        UINT vk = (UINT)strtoul(argv[3], NULL, 16);
        SetFocus(g_found);
        SendMessageW(g_found, WM_KEYDOWN, vk, 0);
        SendMessageW(g_found, WM_KEYUP, vk, 0);
        printf("key 0x%02X sent\n", vk);
        return 0;
    }
    printf("usage: labtest.exe info | rect | type <id> <text> | key <id> <vkHex>\n"
           "             | clear <id> | setsel <id> <a> <b> | getsel <id>\n"
           "             | getraw | dblclick <id> <x> <y> | rclick <id> <x> <y>\n"
           "             | drag <id> <x1> <x2> <y>\n");
    return 1;
}
