// boxrect.c - print screen rects of all AirUITextBox children
// Usage: boxrect.exe <shellHwndDec>
#include <windows.h>
#include <stdio.h>

static BOOL CALLBACK Cb(HWND h, LPARAM l)
{
    (void)l;
    wchar_t cls[64] = {};
    GetClassNameW(h, cls, 64);
    if (lstrcmpiW(cls, L"AirUITextBox") != 0) return TRUE;
    RECT cr = {}, wr = {};
    GetClientRect(h, &cr);
    GetWindowRect(h, &wr);
    wchar_t txt[128] = {};
    SendMessageW(h, WM_GETTEXT, 128, (LPARAM)txt);
    printf("id=%d screen=(%d,%d,%d,%d) %dx%d text=[%ls]\n",
           GetDlgCtrlID(h), wr.left, wr.top, wr.right, wr.bottom,
           wr.right - wr.left, wr.bottom - wr.top, txt);
    return TRUE;
}

int main(int argc, char** argv)
{
    if (argc < 2) { printf("usage\n"); return 1; }
    HWND w = (HWND)(intptr_t)atoi(argv[1]);
    if (!IsWindow(w)) { printf("not a window\n"); return 1; }
    EnumChildWindows(w, Cb, 0);
    return 0;
}
