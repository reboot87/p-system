// 滚动闪烁取证：切到剪贴板页，向列表发 WM_VSCROLL 连续滚动，
// 期间用屏幕 DC 连拍窗口区域帧到 BMP。
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

struct FindCtx { HWND list; };
static BOOL CALLBACK FindList(HWND h, LPARAM lp)
{
    wchar_t cls[64] = L"";
    GetClassNameW(h, cls, 64);
    if (_wcsicmp(cls, L"LISTBOX") == 0) { ((FindCtx*)lp)->list = h; return FALSE; }
    return TRUE;
}

static void Shot(HWND w, const char* path)
{
    RECT rc; GetWindowRect(w, &rc);
    int W = rc.right-rc.left, H = rc.bottom-rc.top;
    HDC sdc = GetDC(NULL);
    HDC mem = CreateCompatibleDC(sdc);
    HBITMAP bmp = CreateCompatibleBitmap(sdc, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, W, H, sdc, rc.left, rc.top, SRCCOPY);
    struct BI { BITMAPINFOHEADER h; DWORD masks[3]; } bi = {};
    bi.h.biSize = sizeof(BITMAPINFOHEADER);
    bi.h.biPlanes = 1; bi.h.biWidth = W; bi.h.biBitCount = 32;
    bi.h.biCompression = BI_RGB; bi.h.biHeight = -H;
    int rowBytes = W*4;
    BYTE* bits = (BYTE*)malloc(rowBytes*H);
    GetDIBits(mem, bmp, 0, H, bits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    BITMAPFILEHEADER fh = {};
    fh.bfType = 0x4D42; fh.bfOffBits = sizeof(fh)+sizeof(BITMAPINFOHEADER);
    fh.bfSize = fh.bfOffBits + rowBytes*H;
    FILE* f = fopen(path, "wb");
    if (f) { fwrite(&fh,1,sizeof(fh),f); fwrite(&bi.h,1,sizeof(bi.h),f); fwrite(bits,1,rowBytes*H,f); fclose(f); }
    free(bits);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); ReleaseDC(NULL, sdc);
}

int main(int argc, char** argv)
{
    const char* tag = argc > 1 ? argv[1] : "new";
    HWND w = FindWindowW(L"AireRecordWnd", NULL);
    if (!w) { printf("no window\n"); return 1; }

    // 切到剪贴板页（侧栏第二项 y~126）
    PostMessageW(w, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(100, 126));
    PostMessageW(w, WM_LBUTTONUP, 0, MAKELPARAM(100, 126));
    Sleep(900);

    FindCtx ctx = {};
    EnumChildWindows(w, FindList, (LPARAM)&ctx);
    if (!ctx.list) { printf("no listbox\n"); return 1; }
    printf("list=%p tag=%s\n", (void*)ctx.list, tag);

    for (int i = 0; i < 18; ++i) {
        PostMessageW(ctx.list, WM_VSCROLL, SB_PAGEDOWN, 0);
        char p[160];
        sprintf(p, "C://Users//Administrator//AppData//Local//Temp//scroll_%s_%02d.bmp", tag, i);
        Shot(w, p);
        Sleep(30);
    }
    printf("done\n");
    return 0;
}
