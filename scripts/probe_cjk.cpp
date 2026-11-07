// Probe: does DrawText(DT_CALCRECT) apply GDI font linking, and what
// advance does GDI use for CJK at a given Segoe UI cell height?
//
// Build: g++ -municode -O1 -o build/probe_cjk.exe scripts/probe_cjk.cpp -lgdi32 -luser32
// (pure Win32, no Skia; run it to see per-role numbers.)
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <cwchar>
#include <cstdio>

static int CalcWidth(HDC dc, const wchar_t* text, int n)
{
    RECT rc = { 0, 0, 4000, 400 };
    DrawTextW(dc, text, n, &rc, DT_CALCRECT | DT_SINGLELINE | DT_LEFT | DT_TOP | DT_NOPREFIX);
    return rc.right - rc.left;
}

int main(int argc, char** argv)
{
    const wchar_t* family = L"Microsoft YaHei UI";
    if (argc > 1) {
        static wchar_t buf[64];
        MultiByteToWideChar(CP_ACP, 0, argv[1], -1, buf, 64);
        family = buf;
    }
    printf("family=%ls\n", family);
    SetProcessDPIAware();
    HDC screen = GetDC(NULL);
    HDC dc = CreateCompatibleDC(screen);
    printf("screen dpi=%d logpx=%d\n", GetDeviceCaps(screen, LOGPIXELSY),
           GetDeviceCaps(screen, LOGPIXELSY));

    const int cells[] = { 12, 13, 14, 22 };
    const UINT outPrecs[] = { OUT_DEFAULT_PRECIS, OUT_TT_PRECIS };
    for (int op = 0; op < 2; ++op) {
    const UINT outPrec = outPrecs[op];
    printf("-- outPrec=%s\n", op ? "TT" : "DEFAULT");
    const int weights[] = { 400, 600 };
    const wchar_t* cjk = L"\u5DE5\u4F5C\u53F0\u603B\u89C8\u526A\u8D34\u677F"; // 8 chars
    const wchar_t* one = L"\u5DE5";
    const wchar_t* latin = L"AireRecord 0123456789";

    for (int w = 0; w < 2; ++w) {
        for (int c = 0; c < 6; ++c) {
            HFONT f = CreateFontW(-cells[c], 0, 0, 0, weights[w], FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, outPrec, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HGDIOBJ old = SelectObject(dc, f);
            TEXTMETRICW tm{};
            GetTextMetricsW(dc, &tm);
            SIZE e8{}, e1{}, el{};
            GetTextExtentPoint32W(dc, cjk, 8, &e8);
            GetTextExtentPoint32W(dc, one, 1, &e1);
            GetTextExtentPoint32W(dc, latin, (int)wcslen(latin), &el);
            const int d8 = CalcWidth(dc, cjk, 8);
            const int d1 = CalcWidth(dc, one, 1);
            printf("cell=%2d w=%3d tmH=%2d asc=%2d desc=%2d | extent: cjk8=%3d cjk1=%2d latin=%3d"
                   " | drawtext: cjk8=%3d cjk1=%2d | adv/char=%.2f (draw) %.2f (ext)\n",
                   cells[c], weights[w], tm.tmHeight, tm.tmAscent, tm.tmDescent,
                   (int)e8.cx, (int)e1.cx, (int)el.cx, d8, d1,
                   (d8 - d1) / 7.0, (e8.cx - e1.cx) / 7.0);
            SelectObject(dc, old);
            DeleteObject(f);
        }
    }
    }
    DeleteDC(dc);
    ReleaseDC(NULL, screen);
    return 0;
}
