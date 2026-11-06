// 抓还原动画期间的帧到 BMP，观察黑区形态
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static void Shot(HWND w, const char* path)
{
    RECT rc; GetClientRect(w, &rc);
    int W = rc.right, H = rc.bottom;
    HDC wdc = GetDC(w);
    HDC mem = CreateCompatibleDC(wdc);
    HBITMAP bmp = CreateCompatibleBitmap(wdc, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);
    PrintWindow(w, mem, 3);
    struct BI { BITMAPINFOHEADER h; DWORD masks[3]; } bi = {};
    bi.h.biSize = sizeof(BITMAPINFOHEADER);
    bi.h.biPlanes = 1; bi.h.biWidth = W; bi.h.biBitCount = 32;
    bi.h.biCompression = BI_RGB; bi.h.biHeight = -H;
    int rowBytes = W * 4;
    BYTE* bits = (BYTE*)malloc(rowBytes * H);
    GetDIBits(wdc, bmp, 0, H, bits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    BITMAPFILEHEADER fh = {};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(BITMAPINFOHEADER);
    fh.bfSize = fh.bfOffBits + rowBytes * H;
    FILE* f = fopen(path, "wb");
    if (f) { fwrite(&fh,1,sizeof(fh),f); fwrite(&bi.h,1,sizeof(bi.h),f); fwrite(bits,1,rowBytes*H,f); fclose(f); }
    free(bits);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); ReleaseDC(w, wdc);
    printf("shot %s %dx%d\n", path, W, H);
}

int main(void)
{
    HWND w = FindWindowW(L"AireRecordWnd", NULL);
    if (!w) { printf("no window\n"); return 1; }
    if (!IsZoomed(w)) { PostMessageW(w, WM_SYSCOMMAND, SC_MAXIMIZE, 0); Sleep(1200); }
    PostMessageW(w, WM_SYSCOMMAND, SC_RESTORE, 0);
    for (int i = 0; i < 8; ++i) {
        char path[128];
        sprintf(path, "C://Users//Administrator//AppData//Local//Temp//restore_%d.bmp", i);
        Shot(w, path);
        Sleep(40);
    }
    printf("done zoomed=%d\n", (int)IsZoomed(w));
    return 0;
}
