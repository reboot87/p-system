// dpick.c - precise DPI probe against the running airecord:
// 1. find AireRecordWnd, verify it's a Shell window (GWLP_USERDATA != 0)
// 2. SendMessage WM_DPICHANGED dpi
// 3. report client rect before/after (+1.2s)
// 4. also posts a marker text to the status bar via Shell? (no - keep simple)
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    if (argc < 2) { printf("usage: dpick <dpi>\n"); return 1; }
    HWND w = FindWindowW(L"AireRecordWnd", NULL);
    if (!IsWindow(w)) { printf("not found\n"); return 1; }
    LONG_PTR shell = GetWindowLongPtrW(w, GWLP_USERDATA);
    RECT r0; GetClientRect(w, &r0);
    printf("hwnd=%lld shellUserData=%ld client=(%d,%d,%d,%d)\n",
           (long long)w, (long)shell, r0.left, r0.top, r0.right, r0.bottom);
    LRESULT r = SendMessageW(w, WM_USER + 9000, (WPARAM)atoi(argv[1]), 0);
    LRESULT m = SendMessageW(w, WM_USER + 9001, 0, 0);
    printf("marker SendMessage ret=%ld\n", (long)m);
    RECT r1; GetClientRect(w, &r1);
    Sleep(1200);
    RECT r2; GetClientRect(w, &r2);
    printf("SendMessage ret=%ld client_after=%dx%d client_after2=%dx%d\n",
           (long)r, r1.right - r1.left, r1.bottom - r1.top,
           r2.right - r2.left, r2.bottom - r2.top);
    return 0;
}
