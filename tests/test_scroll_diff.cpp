// 滚动帧间抖动分析：连续帧像素差百分比（闪烁=额外中间态 → 更高 churn）
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static BYTE* LoadFrame(const char* path, int* W, int* H)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    BITMAPFILEHEADER fh; BITMAPINFOHEADER ih;
    fread(&fh, 1, sizeof(fh), f);
    fread(&ih, 1, sizeof(ih), f);
    *W = ih.biWidth; *H = ih.biHeight < 0 ? -ih.biHeight : ih.biHeight;
    int rowBytes = ((*W * 32 / 8) + 3) & ~3;
    BYTE* bits = (BYTE*)malloc(rowBytes * *H);
    fread(bits, 1, rowBytes * *H, f);
    fclose(f);
    return bits;
}

int main(int argc, char** argv)
{
    const char* tag = argc > 1 ? argv[1] : "new";
    int W = 0, H = 0, prevW = 0, prevH = 0;
    BYTE* prev = NULL;
    double sum = 0; int pairs = 0;
    for (int i = 0; i < 18; ++i) {
        char p[160];
        sprintf(p, "C://Users//Administrator//AppData//Local//Temp//scroll_%s_%02d.bmp", tag, i);
        BYTE* cur = LoadFrame(p, &W, &H);
        if (!cur) continue;
        if (prev && W == prevW && H == prevH) {
            int rowBytes = ((W * 32 / 8) + 3) & ~3;
            long changed = 0, total = 0;
            for (int y = 0; y < H; y += 4) {
                const BYTE* r1 = prev + (size_t)y * rowBytes;
                const BYTE* r2 = cur + (size_t)y * rowBytes;
                for (int x = 0; x < W; x += 4) {
                    const BYTE* a = r1 + (size_t)x * 4;
                    const BYTE* b = r2 + (size_t)x * 4;
                    ++total;
                    if (abs(a[0]-b[0]) > 25 || abs(a[1]-b[1]) > 25 || abs(a[2]-b[2]) > 25)
                        ++changed;
                }
            }
            double pct = total ? changed * 100.0 / total : 0;
            sum += pct; ++pairs;
            printf("pair %02d-%02d diff=%.1f%%\n", i-1, i, pct);
        }
        free(prev);
        prev = cur; prevW = W; prevH = H;
    }
    printf("TAG=%s avg-diff=%.2f%% over %d pairs\n", tag, pairs ? sum/pairs : 0, pairs);
    return 0;
}
