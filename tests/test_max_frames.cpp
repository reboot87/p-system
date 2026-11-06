// 抓首次最大化动画期间的屏幕帧
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
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
    printf("shot %s\n", path);
}
int main(void)
{
    HWND w = FindWindowW(L"AireRecordWnd", NULL);
    if (!w) { printf("no window\n"); return 1; }
    PostMessageW(w, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    for (int i = 0; i < 10; ++i) {
        char p[128]; sprintf(p, "C://Users//Administrator//AppData//Local//Temp//maxnew_%d.bmp", i);
        Shot(w, p);
        Sleep(12);
    }
    return 0;
}
