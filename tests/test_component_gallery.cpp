// ============================================================
// AirUI 组件渲染预览 —— 生成一张组件画廊 BMP
//
// 绘制逻辑与 airui_gallery.exe 共用 src/gallery/GalleryDraw.cpp，
// 因此这里输出的图与 GUI 中看到的内容保持一致。
// 生成后可用 Pillow / System.Drawing 转成 PNG 查看。
// ============================================================
#include <windows.h>
#include <fstream>
#include <iostream>
#include <string>

#include "airui/AirUi.h"
#include "gallery/GalleryDraw.h"

using namespace air::ui;

static bool SaveBmp(const wchar_t* path, int W, int H, const void* bits)
{
    BITMAPFILEHEADER fh = {};
    BITMAPINFOHEADER ih = {};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + W * H * 4;

    ih.biSize = sizeof(ih);
    ih.biWidth = W;
    ih.biHeight = -H;
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
    f.write(reinterpret_cast<const char*>(&ih), sizeof(ih));
    f.write(reinterpret_cast<const char*>(bits), W * H * 4);
    return f.good();
}

int main()
{
    const int W = 1280;
    const int H = gallery::kGalleryHeight;

    Fonts::Default().Create(96);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits,
                                   nullptr, 0);
    if (!bmp || !bits) {
        std::wcerr << L"CreateDIBSection failed\n";
        return 1;
    }

    HDC mem = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(mem, bmp);

    Canvas g(mem, Fonts::Default());

    // 背景 + 完整画廊（左右各留 32px 页边距，与 GUI 内容区一致）
    g.Fill({ 0, 0, W, H }, Pal().bg);
    gallery::DrawGallery(g, 32, 0, W - 64, false);

    // 32bpp BMP 的 alpha 通道 GDI 不写，手动补 255，避免 PNG 转换后透明
    BYTE* px = static_cast<BYTE*>(bits);
    for (int i = 0; i < W * H; ++i) px[i * 4 + 3] = 255;

    SelectObject(mem, old);
    DeleteDC(mem);

    std::wstring path = L"F:/project/github/p-system/component_gallery.bmp";
    bool ok = SaveBmp(path.c_str(), W, H, bits);
    DeleteObject(bmp);

    if (!ok) {
        std::wcerr << L"SaveBmp failed\n";
        return 1;
    }
    std::wcout << L"Gallery saved: " << path << L"\n";
    std::wcout << L"PASS\n";
    return 0;
}
