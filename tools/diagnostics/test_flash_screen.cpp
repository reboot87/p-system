// 屏幕级闪屏检测：截取窗口所在屏幕区域（用户真实所见），
// 统计最大化/还原动画期间的近黑像素占比。
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static int ScreenBlackPct(HWND w)
{
    RECT rc; GetWindowRect(w, &rc);
    int W = rc.right - rc.left, H = rc.bottom - rc.top;
    if (W <= 0 || H <= 0) return -1;
    HDC sdc = GetDC(NULL);
    HDC mem = CreateCompatibleDC(sdc);
    HBITMAP bmp = CreateCompatibleBitmap(sdc, W, H);
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, W, H, sdc, rc.left, rc.top, SRCCOPY);
    struct BI { BITMAPINFOHEADER h; DWORD masks[3]; } bi = {};
    bi.h.biSize = sizeof(BITMAPINFOHEADER);
    bi.h.biPlanes = 1; bi.h.biWidth = W; bi.h.biBitCount = 32;
    bi.h.biCompression = BI_RGB; bi.h.biHeight = -H;
    int rowBytes = W * 4;
    BYTE* bits = (BYTE*)malloc(rowBytes * H);
    int black = 0, total = 0;
    if (GetDIBits(mem, bmp, 0, H, bits, (BITMAPINFO*)&bi, DIB_RGB_COLORS) == H) {
        for (int y = 0; y < H; y += 6) {
            const BYTE* row = bits + (size_t)y * rowBytes;
            for (int x = 0; x < W; x += 6) {
                const BYTE* px = row + (size_t)x * 4;
                ++total;
                if (px[0] < 20 && px[1] < 20 && px[2] < 20) ++black;
            }
        }
    }
    free(bits);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); ReleaseDC(NULL, sdc);
    return total ? black * 100 / total : -1;
}

int main(void)
{
    HWND w = FindWindowW(L"AireRecordWnd", NULL);
    if (!w) { printf("no window\n"); return 1; }

    // 先铺一层已知内容（切到总览页），等待稳定
    Sleep(500);

    int worstMax = 0, worstRes = 0;
    for (int round = 0; round < 3; ++round) {
        PostMessageW(w, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        int worst = 0;
        for (int i = 0; i < 30; ++i) {
            int pct = ScreenBlackPct(w);
            if (pct > worst) worst = pct;
            Sleep(12);
        }
        if (worst > worstMax) worstMax = worst;
        printf("round %d MAXIMIZE worst=%d%%\n", round, worst);
        Sleep(500);

        PostMessageW(w, WM_SYSCOMMAND, SC_RESTORE, 0);
        worst = 0;
        for (int i = 0; i < 30; ++i) {
            int pct = ScreenBlackPct(w);
            if (pct > worst) worst = pct;
            Sleep(12);
        }
        if (worst > worstRes) worstRes = worst;
        printf("round %d RESTORE   worst=%d%%\n", round, worst);
        Sleep(500);
    }
    printf("SUMMARY worstMax=%d%% worstRes=%d%%\n", worstMax, worstRes);
    printf(worstMax < 10 && worstRes < 10 ? "PASS\n" : "FAIL\n");
    return 0;
}
