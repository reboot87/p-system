// boxread.c - read geometry of the ClipsPage controls in airecord:
//  - AirUITextBox children (client height; id 1001 = search)
//  - AirUIListBox first 2 rows (LB_GETITEMRECT shim)
// usage: boxread <shellHwndDec>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static HWND g_w = 0;

static int CALLBACK CbBox(HWND h, LPARAM l)
{
    wchar_t cls[64];
    GetClassNameW(h, cls, 64);
    if (lstrcmpiW(cls, L"AirUITextBox") != 0) return TRUE;
    RECT r; GetClientRect(h, &r);
    printf("  box id=%d w=%d h=%d\n", GetDlgCtrlID(h),
           r.right - r.left, r.bottom - r.top);
    return TRUE;
}

static int CALLBACK CbList(HWND h, LPARAM l)
{
    wchar_t cls[64];
    GetClassNameW(h, cls, 64);
    if (lstrcmpiW(cls, L"AirUIListBox") != 0) return TRUE;
    RECT r = {0,0,0,0};
    if (SendMessageW(h, LB_GETITEMRECT, 0, (LPARAM)&r) == LB_OKAY)
        printf("  list row0 h=%d\n", r.bottom - r.top);
    RECT r1 = {0,0,0,0};
    if (SendMessageW(h, LB_GETITEMRECT, 1, (LPARAM)&r1) == LB_OKAY)
        printf("  list row1 h=%d\n", r1.bottom - r1.top);
    return FALSE;
}

int main(int argc, char** argv)
{
    if (argc < 2) { printf("usage: boxread <hwnd>\n"); return 1; }
    g_w = (HWND)(intptr_t)atol(argv[1]);
    if (!IsWindow(g_w)) { printf("not a window\n"); return 1; }
    printf("boxread:\n");
    EnumChildWindows(g_w, CbBox, 0);
    EnumChildWindows(g_w, CbList, 0);
    return 0;
}
