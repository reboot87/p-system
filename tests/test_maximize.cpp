// test_maximize.cpp — 最大化动画期间的帧检测：捕捉黑屏闪烁
// 用法：先启动 airecord.exe，再运行本程序。
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static int g_black = 0, g_total = 0;

static int AnalyzeFrame(HWND w)
{
    RECT rc; GetClientRect(w, &rc);
    int W = rc.right, H = rc.bottom;
    if (W <= 0 || H <= 0) return -1;

    HDC wdc = GetDC(w);
    HDC mem = CreateCompatibleDC(wdc);
    HBITMAP bmp = CreateCompatibleBitmap(wdc, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);
    PrintWindow(w, mem, 3 /* PW_CLIENTONLY | PW_RENDERFULLCONTENT */);

    struct BI { BITMAPINFOHEADER h; DWORD masks[3]; } bi = {};
    bi.h.biSize = sizeof(BITMAPINFOHEADER);
    bi.h.biPlanes = 1;
    bi.h.biWidth = W;
    bi.h.biBitCount = 32;
    bi.h.biCompression = BI_RGB;
    bi.h.biHeight = -H;

    int rowBytes = W * 4;
    BYTE* bits = (BYTE*)malloc(rowBytes * H);
    int got = GetDIBits(wdc, bmp, 0, H, bits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    int black = 0, total = 0;
    if (got == H) {
        for (int y = 0; y < H; y += 8) {
            const BYTE* row = bits + (size_t)y * rowBytes;
            for (int x = 0; x < W; x += 8) {
                const BYTE* px = row + (size_t)x * 4;  // BGX
                ++total;
                if (px[0] < 20 && px[1] < 20 && px[2] < 20) ++black;
            }
        }
    }
    free(bits);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(w, wdc);
    g_black += black; g_total += total;
    return total ? (black * 100 / total) : -1;
}

int main(void)
{
    HWND w = FindWindowW(L"AireRecordWnd", NULL);
    if (!w) { printf("FAIL: no AireRecordWnd\n"); return 1; }
    printf("window found, maximized=%d\n", (int)IsZoomed(w));

    for (int round = 0; round < 3; ++round) {
        // 最大化，动画期间连拍
        PostMessageW(w, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        int maxPct = 0;
        for (int i = 0; i < 25; ++i) {
            int pct = AnalyzeFrame(w);
            if (pct > maxPct) maxPct = pct;
            Sleep(15);
        }
        printf("round %d: MAXIMIZE worst-frame black%% = %d, zoomed=%d\n",
               round, maxPct, (int)IsZoomed(w));
        Sleep(400);

        // 还原，动画期间连拍
        PostMessageW(w, WM_SYSCOMMAND, SC_RESTORE, 0);
        maxPct = 0;
        for (int i = 0; i < 25; ++i) {
            int pct = AnalyzeFrame(w);
            if (pct > maxPct) maxPct = pct;
            Sleep(15);
        }
        printf("round %d: RESTORE   worst-frame black%% = %d, zoomed=%d\n",
               round, maxPct, (int)IsZoomed(w));
        Sleep(400);
    }

    printf(g_black * 100 / (g_total ? g_total : 1) < 2 ? "PASS\n" : "FAIL\n");
    return 0;
}
