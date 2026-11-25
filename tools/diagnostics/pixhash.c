// pixhash.c - fingerprint the pixels of a screen rectangle (BitBlt +
// FNV-1a 64). usage: pixhash.exe <x> <y> <w> <h>
//   Prints: pixhash <16-hex> <w>x<h>
// Deterministic screen-region hash: the T5 password test uses it to assert
// that the masked box (bullets) and its plaintext twin (real glyphs) with
// the SAME text render DIFFERENTLY - no font-metric assumption needed.
// Hashing the same rect twice must give the same value (stability control).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    HDC scr = 0, mem = 0;
    HBITMAP bmp = 0;
    HGDIOBJ old = 0;
    BITMAPINFO bi;
    unsigned char* buf = 0;
    unsigned long long hsh = 1469598103934665603ULL;
    size_t n = 0, i = 0;
    int x, y, w, h, got;

    if (argc < 5) { printf("usage: pixhash <x> <y> <w> <h>\n"); return 1; }
    x = atoi(argv[1]); y = atoi(argv[2]);
    w = atoi(argv[3]); h = atoi(argv[4]);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) { printf("bad rect\n"); return 1; }

    scr = GetDC(NULL);
    mem = CreateCompatibleDC(scr);
    bmp = CreateCompatibleBitmap(scr, w, h);
    old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, scr, x, y, SRCCOPY);

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;      // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    buf = (unsigned char*)malloc((size_t)w * (size_t)h * 4);
    if (!buf) { printf("alloc failed\n"); return 1; }
    got = GetDIBits(mem, bmp, 0, (UINT)h, buf, &bi, DIB_RGB_COLORS);
    if (got != h) {
        printf("GetDIBits failed (got %d)\n", got);
        free(buf);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
        ReleaseDC(NULL, scr);
        return 1;
    }

    n = (size_t)w * (size_t)h * 4;
    for (i = 0; i < n; ++i) {
        hsh ^= buf[i];
        hsh *= 1099511628211ULL;
    }
    printf("pixhash %016llX %dx%d\n", hsh, w, h);

    free(buf);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(NULL, scr);
    return 0;
}
