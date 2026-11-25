// listtest.c - ListBox multi-select driver (3b), cross-process vs the lab.
//
// Requires a running airui_textbox_lab.exe with the ListBox page (--listbox).
// Boxes: 3201 (multi), 3202 (single), 3203 (native EDIT mirror of state).
//
// Usage (all cross-process; wParam carries the MK_* modifier bits):
//   listtest.exe rect <shellHwndDec>            screen rects of AirUIListBox
//                                               children + client row rects 0..5
//   listtest.exe click <shellHwndDec> <id> <x> <y> [modsHex]
//                                               posts WM_LBUTTONDOWN+UP to the
//                                               list (id 3201/3202) with
//                                               CLIENT coords (x,y) — the
//                                               ListBoxProc hit test reads
//                                               client coords (lParam is
//                                               client-relative for mouse msgs).
//                                               modsHex: MK_CONTROL=0x8,
//                                               MK_SHIFT=0x4 (matches the
//                                               handler's wParam read).
//   listtest.exe selstate <shellHwndDec>        prints (wprintf):
//                                               mirror=<3203 text>
//                                               id=3201 LB_CURSEL=<p> count=<n> sel=[...]
//                                               id=3202 LB_CURSEL=<p> sel=[...]
//
// 3b: LB_GETSEL/LB_GETSELCOUNT shims in ListBoxProc (see list_lab_suite.ps1).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static HWND g_shell = 0;
static HWND g_lists[8] = {0};

static int CALLBACK CbList(HWND h, LPARAM l)
{
    wchar_t cls[64];
    GetClassNameW(h, cls, 64);
    if (lstrcmpiW(cls, L"AirUIListBox") != 0) return TRUE;
    const int id = (int)GetDlgCtrlID(h);
    if (id >= 3200 && id <= 3207 && g_lists[id - 3200] == 0)
        g_lists[id - 3200] = h;
    return TRUE;
}

static int CALLBACK CbMirror(HWND h, LPARAM l)
{
    wchar_t cls[64];
    GetClassNameW(h, cls, 64);
    if (lstrcmpiW(cls, L"EDIT") != 0) return TRUE;
    wchar_t txt[256] = {};
    GetWindowTextW(h, txt, 255);
    wprintf(L"  mirror id=%d text=[%ls]\n", GetDlgCtrlID(h), txt);
    return TRUE;
}

static void DumpList(HWND h)
{
    if (!h || !IsWindow(h)) return;
    RECT rc;
    GetWindowRect(h, &rc);
    const int id = GetDlgCtrlID(h);
    const int selcount = (int)SendMessageW(h, LB_GETSELCOUNT, 0, 0);
    const int sel      = (int)SendMessageW(h, LB_GETCURSEL, 0, 0);
    wprintf(L"  id=%d LB_CURSEL=%d selcount=%d sel=[", id, sel, selcount);
    for (int i = 0; i < 12; ++i)
        if (SendMessageW(h, LB_GETSEL, i, 0) == 0) wprintf(L"%d,", i);
    wprintf(L"]\n");
    for (int i = 0; i < 6; ++i) {
        RECT r = {0, 0, 0, 0};
        if (SendMessageW(h, LB_GETITEMRECT, i, (LPARAM)&r) == LB_OKAY)
            wprintf(L"  list id=%d row%d client=(%d,%d,%d,%d) screen=(%d,%d,%d,%d)\n",
                    id, i, r.left, r.top, r.right, r.bottom,
                    r.left + rc.left, r.top + rc.top,
                    r.right + rc.left, r.bottom + rc.top);
    }
}

int main(int argc, char** argv)
{
    if (argc < 3) { wprintf(L"usage: listtest rect|click|selstate <shellHwnd> [args]\n"); return 1; }
    g_shell = (HWND)(intptr_t)atoi(argv[2]);
    if (!IsWindow(g_shell)) { wprintf(L"not a window\n"); return 1; }
    EnumChildWindows(g_shell, CbList, 0);
    const char* cmd = argv[1];
    if (!strcmp(cmd, "rect")) {
        EnumChildWindows(g_shell, CbMirror, 0);
        for (int i = 0; i < 8; ++i) DumpList(g_lists[i]);
        return 0;
    }
    if (!strcmp(cmd, "selstate")) {
        EnumChildWindows(g_shell, CbMirror, 0);
        for (int i = 0; i < 8; ++i) DumpList(g_lists[i]);
        return 0;
    }
    if (!strcmp(cmd, "click")) {
        if (argc < 6) { wprintf(L"usage: click <shell> <id3201|3202> <x> <y> [modsHex]\n"); return 1; }
        const int id = atoi(argv[3]);
        HWND h = g_lists[id - 3200];
        if (!h) { wprintf(L"list id=%d not found\n", id); return 1; }
        const LPARAM lp = MAKELPARAM((int)atoi(argv[4]), (int)atoi(argv[5]));
        const WPARAM mods = (argc >= 7)
            ? (WPARAM)(unsigned)strtoul(argv[6], NULL, 16) : 0;
        wprintf(L"click id=%d client=(%d,%d) mods=0x%X -> down=%d up=%d\n",
                id, LOWORD(lp), HIWORD(lp), (unsigned)mods,
                (int)PostMessageW(h, WM_LBUTTONDOWN, mods, lp),
                (int)PostMessageW(h, WM_LBUTTONUP, mods, lp));
        return 0;
    }
    wprintf(L"unknown cmd %s\n", L"");
    return 1;
}
