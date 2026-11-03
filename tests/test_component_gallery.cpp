// ============================================================
// AirUI 组件渲染预览 —— 生成一张组件画廊 BMP
// 生成后可用 System.Drawing 转成 PNG 查看。
// ============================================================
#include <windows.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>

#include "airui/AirUi.h"
#include "airui/Components.h"

using namespace air::ui;

static std::wstring ColorToHex(COLORREF c)
{
    wchar_t buf[16] = {};
    swprintf_s(buf, L"#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    return buf;
}

static void Section(Canvas& g, int x, int y, int right,
                    const wchar_t* title, const wchar_t* desc)
{
    const Palette& p = Pal();
    g.Text({x, y, right, y + 28}, title, FontRole::Section, p.text,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    if (desc && *desc) {
        int titleW = g.MeasureWidth(title, FontRole::Section);
        int dx = x + (titleW + 24 > 220 ? titleW + 24 : 220);
        g.Text({dx, y, right, y + 28}, desc, FontRole::Meta, p.textFaint,
               DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS |
               DT_NOPREFIX);
    }
    g.HLine(x, right, y + 32, p.split);
}

static COLORREF ContrastText(COLORREF c)
{
    int lum = (GetRValue(c) * 299 + GetGValue(c) * 587 + GetBValue(c) * 114) / 1000;
    return lum > 140 ? RGB(31, 35, 41) : RGB(255, 255, 255);
}

static void Swatch(Canvas& g, const RECT& rc, COLORREF color,
                   const std::wstring& name)
{
    const Palette& p = Pal();
    COLORREF tx = ContrastText(color);
    g.FillRound(rc, color, Mx().radiusMd);
    g.Stroke(rc, p.border, Mx().radiusMd);
    g.Text({rc.left + 8, rc.top + 6, rc.right - 8, rc.top + 24},
           name, FontRole::Meta, tx,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS |
           DT_NOPREFIX);
    g.Text({rc.left + 8, rc.bottom - 26, rc.right - 8, rc.bottom - 6},
           ColorToHex(color), FontRole::Meta, tx,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
}

static void ButtonMatrix(Canvas& g, int x, int y)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();
    const wchar_t* colNames[] = { L"Primary", L"Secondary", L"Danger", L"Ghost" };
    const wchar_t* colText[] = { L"主按钮", L"次按钮", L"危险操作", L"幽灵按钮" };
    BtnStyle styles[] = { BtnStyle::Primary, BtnStyle::Secondary,
                          BtnStyle::Danger, BtnStyle::Ghost };
    const wchar_t* rowNames[] = { L"Normal", L"Hover", L"Pressed", L"Disabled" };

    int labelW = 88;
    int bw = 150;
    int bh = 36;
    int gapX = 18;
    int rowH = 46;

    for (int c = 0; c < 4; ++c) {
        int cx = x + labelW + c * (bw + gapX);
        g.Text({cx, y - 22, cx + bw, y},
               colNames[c], FontRole::Meta, p.textFaint,
               DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    }

    for (int r = 0; r < 4; ++r) {
        int ry = y + r * rowH;
        g.Text({x, ry, x + labelW - 12, ry + bh},
               rowNames[r], FontRole::Meta, p.textSub,
               DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
        for (int c = 0; c < 4; ++c) {
            int cx = x + labelW + c * (bw + gapX);
            RECT rc = { cx, ry, cx + bw, ry + bh };
            g.Button(rc, colText[c], styles[c],
                     r == 1, r == 2, r == 3);
        }
    }
}

static void PrimitiveSection(Canvas& g, int x, int y, int right)
{
    const Palette& p = Pal();
    const Metrics& m = Mx();

    // 标签
    g.Text({x, y, x + 88, y + 24}, L"Tag", FontRole::Meta, p.textFaint,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    int tx = x + 96;
    tx = g.Tag(tx, y + 1, L"剪贴板", FontRole::Meta,
               p.primarySoft, p.primaryText, m.tagH) + 8;
    tx = g.Tag(tx, y + 1, L"已完成", FontRole::Meta,
               p.okSoft, p.okText, m.tagH) + 8;
    tx = g.Tag(tx, y + 1, L"待处理", FontRole::Meta,
               p.warnSoft, p.warnText, m.tagH) + 8;
    tx = g.Tag(tx, y + 1, L"失败", FontRole::Meta,
               p.errorSoft, p.errorText, m.tagH) + 8;
    g.Tag(tx, y + 1, L"备注", FontRole::Meta,
          p.noteBg, p.textSub, m.tagH);

    // 胶囊 / 状态胶囊
    int y2 = y + 46;
    g.Text({x, y2 - 12, x + 88, y2 + 12}, L"Pill", FontRole::Meta, p.textFaint,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    g.PillRight(x + 420, y2, L"运行中", FontRole::Meta,
                p.okSoft, p.okText, m.tagH);
    g.PillRight(x + 570, y2, L"规划中", FontRole::Meta,
                RGB(239, 241, 244), RGB(143, 149, 158), m.tagH);

    g.Text({x, y2 + 34, x + 88, y2 + 58}, L"Status", FontRole::Meta, p.textFaint,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    g.StatusTag(x + 420, y2 + 46, L"服务正常", FontRole::Meta,
                p.okSoft, p.okBorder, p.okText, p.ok, m.statusTagH);
    g.StatusTag(x + 570, y2 + 46, L"服务异常", FontRole::Meta,
                p.errorSoft, p.errorBorder, p.errorText, p.error,
                m.statusTagH);

    // 键位提示
    int y3 = y + 112;
    g.Text({x, y3 - 12, x + 88, y3 + 12}, L"Kbd", FontRole::Meta, p.textFaint,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    g.KbdChip(x + 570, y3, L"Ctrl", L"搜索");
    g.KbdChip(x + 420, y3, L"Enter", L"确认");

    // 图标块
    int y4 = y + 160;
    g.Text({x, y4 - 12, x + 88, y4 + 12}, L"IconTile", FontRole::Meta,
           p.textFaint,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    const wchar_t* glyphs[] = { L"\xE8C8", L"\xE8B7", L"\xE749", L"\xE70F", L"\xE713" };
    COLORREF iconBg[] = { p.primarySoft, RGB(230,244,241), RGB(243,238,255),
                          p.okSoft, p.warnSoft };
    COLORREF iconTx[] = { p.primary, RGB(15,163,127), RGB(123,97,255),
                          p.okText, p.warnText };
    for (int i = 0; i < 5; ++i) {
        RECT rc = { x + 96 + i * 56, y4 - 8, x + 136 + i * 56, y4 + 32 };
        g.IconTile(rc, glyphs[i], FontRole::IconLarge,
                   iconBg[i], iconTx[i], m.radiusMd);
    }

    // 进度条
    int y5 = y + 220;
    g.Text({x, y5 - 12, x + 88, y5 + 12}, L"Progress", FontRole::Meta,
           p.textFaint,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    int pcts[] = { 24, 64, 92 };
    COLORREF cols[] = { p.primary, p.warn, p.error };
    for (int i = 0; i < 3; ++i) {
        int px = x + 96 + i * 176;
        RECT rc = { px, y5 - 4, px + 150, y5 + 4 };
        g.ProgressBar(rc, pcts[i], p.sunken, cols[i], p.warn, p.error);
        wchar_t buf[16];
        swprintf_s(buf, L"%d%%", pcts[i]);
        g.Text({px, y5 - 30, px + 150, y5 - 10}, buf, FontRole::Meta,
               p.textSub,
               DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    }
}

static void TypographyPanel(Canvas& g, const RECT& rc)
{
    const Palette& p = Pal();
    g.FillRound(rc, p.surface, Mx().radiusLg);
    g.Stroke(rc, p.split, Mx().radiusLg);

    int x = rc.left + 16;
    int right = rc.right - 16;
    int y = rc.top + 14;

    g.Text({x, y, right, y + 24}, L"Typography", FontRole::Section, p.text,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    y += 34;

    g.Text({x, y, right, y + 30}, L"1,286", FontRole::Display, p.text,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    g.Text({x + 88, y + 14, right, y + 30}, L"条记录 · Display / 22", FontRole::Meta,
           p.textFaint, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    y += 40;

    g.Text({x, y, right, y + 24}, L"区块标题 Section / 14 semibold",
           FontRole::Section, p.text,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    y += 28;

    g.Text({x, y, right, y + 22}, L"小标题 Title / 13 semibold",
           FontRole::Title, p.text,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    y += 26;

    g.Text({x, y, right, y + 22},
           L"正文 Body：自动记录剪贴板内容，支持搜索与备注。",
           FontRole::Body, p.text,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS |
           DT_NOPREFIX);
    y += 26;

    g.Text({x, y, right, y + 20},
           L"元信息 Meta：2026-11-03 20:30 · 来源 剪贴板",
           FontRole::Meta, p.textFaint,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS |
           DT_NOPREFIX);

    // 模拟列表行
    y += 34;
    RECT row = { x, y, right, y + 56 };
    g.FillRound(row, p.hover, Mx().radiusMd);
    RECT ic = { row.left + 12, row.top + 12, row.left + 44, row.top + 44 };
    g.IconTile(ic, L"\xE8C8", FontRole::Icon, p.iconTile, p.primary, 6);
    g.TextRow({row.left + 56, row.top, row.right - 160, row.bottom},
              L"复制的一段示例文本，用于展示列表条目的排版层级",
              FontRole::Body, p.text);
    g.Tag(row.right - 140, row.top + 18, L"剪贴板", FontRole::Meta,
          p.primarySoft, p.primaryText, Mx().tagH);
}

static void EmptyPanel(Canvas& g, const RECT& rc)
{
    const Palette& p = Pal();
    g.FillRound(rc, p.surface, Mx().radiusLg);
    g.Stroke(rc, p.split, Mx().radiusLg);

    EmptyState st;
    st.title = L"暂无数据";
    st.steps = L"复制任意文字后，记录会自动出现在这里\n也可以点击右上角「手动录入」添加一条";
    st.foot  = L"支持搜索、备注与批量清理";
    g.EmptyState(rc, st);
}

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
    const int H = 1480;

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

    const Palette& p = Pal();
    const Metrics& m = Mx();
    Canvas g(mem, Fonts::Default());

    // 背景
    g.Fill({0, 0, W, H}, p.bg);

    int left = 32;
    int right = W - 32;

    // ---- 页头 ----
    g.Text({left, 22, right, 58}, L"AirUI 组件渲染预览",
           FontRole::Display, p.text,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    g.Text({left, 58, right, 82},
           L"企业级 Win32 自绘组件库 · Theme / Canvas / Gfx / Components",
           FontRole::Meta, p.textSub,
           DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    g.StatusTag(right, 52, L"Light · 96 DPI", FontRole::Meta,
                p.okSoft, p.okBorder, p.okText, p.ok, m.statusTagH);
    g.HLine(left, right, 94, p.split);

    // ---- 1. 调色板 ----
    Section(g, left, 110, right, L"调色板 / Palette",
            L"语义化设计令牌，换肤只改这一层");

    struct SwatchDef { const wchar_t* name; COLORREF color; };
    SwatchDef sw1[] = {
        { L"bg",          p.bg },
        { L"surface",     p.surface },
        { L"sunken",      p.sunken },
        { L"border",      p.border },
        { L"primary",     p.primary },
        { L"primarySoft", p.primarySoft },
        { L"okSoft",      p.okSoft },
        { L"warnSoft",    p.warnSoft },
        { L"errorSoft",   p.errorSoft },
    };
    SwatchDef sw2[] = {
        { L"text",         p.text },
        { L"textSub",      p.textSub },
        { L"textFaint",    p.textFaint },
        { L"textDisabled", p.textDisabled },
        { L"selectFill",   p.selectFill },
        { L"selectRing",   p.selectRing },
        { L"noteBg",       p.noteBg },
        { L"iconTile",     p.iconTile },
        { L"chipBg",       p.chipBg },
    };

    int sw = 124;
    int sh = 52;
    int gap = 12;
    for (int i = 0; i < 9; ++i) {
        int sx = left + i * (sw + gap);
        Swatch(g, {sx, 154, sx + sw, 154 + sh}, sw1[i].color, sw1[i].name);
        Swatch(g, {sx, 216, sx + sw, 216 + sh}, sw2[i].color, sw2[i].name);
    }

    // ---- 2. 按钮 ----
    Section(g, left, 300, right, L"按钮 / Buttons",
            L"Primary · Secondary · Danger · Ghost —— 含 hover / pressed / disabled");
    ButtonMatrix(g, left, 352);

    // ---- 3. 基础组件 ----
    Section(g, left, 534, right, L"基础组件 / Primitives",
            L"Tag · Pill · StatusTag · KbdChip · IconTile · ProgressBar");
    PrimitiveSection(g, left, 572, right);

    // ---- 4. 卡片 ----
    Section(g, left, 830, right, L"卡片 / Card",
            L"图标 + 标题 + 描述 + 大数字 + 状态胶囊 + 底部入口");

    int cardW = (right - left - 32) / 3;
    int cardH = 188;
    int cardY = 874;
    auto makeCard = [&](int idx, const wchar_t* glyph, COLORREF icBg,
                        COLORREF icTx, const wchar_t* title,
                        const wchar_t* desc, const wchar_t* number,
                        const wchar_t* unit, const wchar_t* badgeText,
                        bool badgeOk, bool disabled) {
        Card::Config cfg;
        cfg.rect = { left + idx * (cardW + 16), cardY,
                     left + idx * (cardW + 16) + cardW, cardY + cardH };
        cfg.icon.glyph = glyph;
        cfg.icon.bg = icBg;
        cfg.icon.color = icTx;
        cfg.icon.size = 40;
        cfg.icon.radius = 10;
        cfg.title = title;
        cfg.desc = desc;
        cfg.number = number;
        cfg.unit = unit;
        cfg.badge.text = badgeText;
        cfg.badge.ok = badgeOk;
        cfg.linkText = disabled ? L"敬请期待" : L"进入模块 →";
        cfg.disabled = disabled;
        Card card(cfg);
        card.Paint(g);
    };

    makeCard(0, L"\xE8C8", p.primarySoft, p.primary,
             L"剪贴板监控", L"自动记录剪贴板内容，支持搜索与备注",
             L"1,286", L"条记录", L"运行中", true, false);
    makeCard(1, L"\xE8B7", RGB(230,244,241), RGB(15,163,127),
             L"文件监控", L"监控指定目录的新增 / 修改 / 删除",
             L"—", L"未启用", L"规划中", false, true);
    makeCard(2, L"\xE749", RGB(243,238,255), RGB(123,97,255),
             L"打印监控", L"记录本机打印任务与文档信息",
             L"—", L"未启用", L"规划中", false, true);

    // ---- 5. 空状态 + 排版 ----
    Section(g, left, 1102, right, L"空状态与排版 / Empty & Typography",
            L"EmptyState · 字体层级 · 列表行组合");
    EmptyPanel(g, {left, 1146, left + 588, 1438});
    TypographyPanel(g, {left + 612, 1146, right, 1438});

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
