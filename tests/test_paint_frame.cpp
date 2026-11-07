// ============================================================
// test_paint_frame.cpp
// Proves that a frame switch routes EVERY gfx primitive -- including
// the ones pages and core widgets use -- to the selected backend,
// while a plain frame keeps the original GDI path untouched.
// ============================================================
#include <windows.h>

#include "airui/Gfx.h"
#include "airui/PaintFrame.h"
#include "airui/hal.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace air::ui;

static int g_failures = 0;

static void Check(bool ok, const char* what)
{
    std::printf("%s : %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_failures;
}

// ------------------------------------------------------------
// A backend that only records which IPainter calls it received.
// ------------------------------------------------------------
struct RecordingPainter : IPainter
{
    std::vector<std::string> ops;

    int Count(const char* id) const
    {
        int n = 0;
        for (const std::string& s : ops) if (s == id) ++n;
        return n;
    }

    void FillRect(const Rect&, Color) override { ops.push_back("fill"); }
    void FillRectAlpha(const Rect&, Color, uint8_t) override
    { ops.push_back("fillAlpha"); }
    void FillRound(const Rect&, int, Color) override
    { ops.push_back("fillRound"); }
    void StrokeRect(const Rect&, Color, int) override
    { ops.push_back("strokeRect"); }
    void StrokeRound(const Rect&, int, Color, int) override
    { ops.push_back("strokeRound"); }
    void StrokeDashed(const Rect&, Color, int, int) override
    { ops.push_back("strokeDashed"); }
    void Text(const Rect&, const std::wstring&, FontRole, Color, TextFormat) override
    { ops.push_back("text"); }
    void TextBlock(const Rect&, const std::wstring&, FontRole, Color, int) override
    { ops.push_back("textBlock"); }
    int  MeasureText(const std::wstring&, FontRole) override { return 42; }
    void PushClip(const Rect&) override { ops.push_back("pushClip"); }
    void PopClip() override             { ops.push_back("popClip"); }
    void Translate(Point) override      { ops.push_back("translate"); }
};

// ------------------------------------------------------------
// 32bpp top-down DIB so we can assert what actually hit the DC.
// ------------------------------------------------------------
struct Surface
{
    static const int W = 64;
    static const int H = 32;

    HDC      dc   = nullptr;
    HBITMAP  bmp  = nullptr;
    void*    bits = nullptr;
    HGDIOBJ  old  = nullptr;

    Surface()
    {
        HDC screen = GetDC(nullptr);
        dc = CreateCompatibleDC(screen);
        ReleaseDC(nullptr, screen);

        BITMAPINFO bmi{};
        BITMAPINFOHEADER& bh = bmi.bmiHeader;
        bh.biSize        = sizeof(BITMAPINFOHEADER);
        bh.biWidth       = W;
        bh.biHeight      = -H;              // top-down
        bh.biPlanes      = 1;
        bh.biBitCount    = 32;
        bh.biCompression = BI_RGB;
        bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        old = SelectObject(dc, bmp);
        ClearWhite();
    }

    ~Surface()
    {
        SelectObject(dc, old);
        DeleteObject(bmp);
        DeleteDC(dc);
    }

    void ClearWhite()
    {
        HBRUSH br = CreateSolidBrush(RGB(255, 255, 255));
        RECT rc = { 0, 0, W, H };
        ::FillRect(dc, &rc, br);
        DeleteObject(br);
    }

    uint32_t At(int x, int y) const
    {
        // A 32bpp BI_RGB DIB stores B,G,R,A in memory, while COLORREF
        // packs R,G,B. Convert so callers can compare against RGB().
        const uint32_t v =
            static_cast<const uint32_t*>(bits)[y * W + x] & 0x00FFFFFFu;
        return RGB((v >> 16) & 0xFFu, (v >> 8) & 0xFFu, v & 0xFFu);
    }

    bool HasInk() const
    {
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (At(x, y) != RGB(255, 255, 255)) return true;
        return false;
    }
};

int main()
{
    Surface s;
    Check(s.bits != nullptr && s.dc != nullptr, "32bpp DIB surface created");
    Check(CurrentPaintFrame() == nullptr, "no frame active by default");

    // ---------- 1. Plain frame: original GDI path ----------
    gfx::Fill(s.dc, RECT{ 0, 0, 64, 32 }, RGB(255, 0, 0));
    Check(s.At(8, 8) == RGB(255, 0, 0),
          "gfx::Fill draws straight to the DC with no frame");
    s.ClearWhite();

    // ---------- 2. Active frame: whole window routes to backend ----------
    RecordingPainter mock;
    PaintFrame& frame = PaintFrame::Instance();
    frame.Begin(&mock, s.dc, nullptr, false);   // host-rasterised text
    Check(frame.Active() && CurrentPaintFrame() == &frame,
          "frame activated and visible to gfx::*");

    gfx::Fill(s.dc, RECT{ 0, 0, 64, 32 }, RGB(0, 0, 255));
    Check(s.At(8, 8) == RGB(255, 255, 255),
          "with a frame, gfx::Fill no longer touches the DC");
    Check(mock.Count("fill") == 1, "FillRect reached the backend");

    gfx::FillRound(s.dc, RECT{ 2, 2, 40, 24 }, RGB(1, 2, 3), 6);
    gfx::Stroke(s.dc, RECT{ 2, 2, 40, 24 }, RGB(1, 2, 3), 0, 1);
    gfx::Stroke(s.dc, RECT{ 2, 2, 40, 24 }, RGB(1, 2, 3), 6, 1);
    gfx::StrokeDashed(s.dc, RECT{ 2, 2, 40, 24 }, RGB(1, 2, 3), 6);
    gfx::HLine(s.dc, 0, 40, 10, RGB(1, 2, 3));
    gfx::VLine(s.dc, 20, 0, 30, RGB(1, 2, 3));
    gfx::Dot(s.dc, 30, 15, 8, RGB(1, 2, 3));
    Check(mock.Count("fill") == 3, "rect/line fills reached the backend");
    Check(mock.Count("fillRound") == 2, "round fills reached the backend");
    Check(mock.Count("strokeRect") == 1, "square stroke reached the backend");
    Check(mock.Count("strokeRound") == 1, "round stroke reached the backend");
    Check(mock.Count("strokeDashed") == 1, "dashed stroke reached the backend");
    Check(mock.ops.size() == 8, "lines and dots became ordinary fills");

    // Composite helper (used by pages) must follow without changes.
    gfx::StatusTag(s.dc, 60, 15, L"\u72B6\u6001",
                   (HFONT)GetStockObject(DEFAULT_GUI_FONT),
                   RGB(240, 240, 240), RGB(1, 2, 3), RGB(2, 3, 4), RGB(3, 4, 5));
    Check(mock.Count("fillRound") >= 4 && mock.ops.size() > 8,
          "composite gfx widgets route through the frame too");

    // Text is recorded for host rasterisation, not sent to a backend
    // that cannot draw CJK glyphs.
    gfx::Text(s.dc, (HFONT)GetStockObject(DEFAULT_GUI_FONT), RGB(0, 0, 0),
              RECT{ 2, 2, 60, 28 }, L"\u4E2D\u6587\u6E32\u67D3",
              DT_SINGLELINE | DT_LEFT | DT_TOP | DT_NOPREFIX);
    Check(mock.Count("text") == 0,
          "host-text mode keeps text away from the backend");
    frame.Text(Rect(2, 2, 60, 28), L"\u4E2D\u6587", FontRole::Body,
               MakeColor(0, 0, 0), dt::Top | dt::Left | dt::SingleLine);
    Check(mock.Count("text") == 0,
          "IPainter::Text uses the same host-text policy");
    frame.FillRectAlpha(Rect(0, 0, 64, 32), MakeColor(0, 0, 0), 128);
    frame.PushClip(Rect(0, 0, 32, 32));
    frame.PopClip();
    Check(mock.Count("fillAlpha") == 1 &&
          mock.Count("pushClip") == 1 && mock.Count("popClip") == 1,
          "alpha scrims and clips also route to the backend");

    frame.End();
    Check(CurrentPaintFrame() == nullptr, "frame deactivated");
    frame.ReplayText();
    Check(s.HasInk(), "host text rasterised on the DC after the composite");
    frame.Clear();

    // ---------- 3. Capable backend: pure backend text ----------
    s.ClearWhite();
    mock.ops.clear();
    frame.Begin(&mock, s.dc, nullptr, true);
    gfx::Text(s.dc, (HFONT)GetStockObject(DEFAULT_GUI_FONT), RGB(0, 0, 0),
              RECT{ 2, 2, 60, 28 }, L"\u4E2D\u6587",
              DT_SINGLELINE | DT_LEFT | DT_TOP | DT_NOPREFIX);
    Check(mock.Count("text") == 1,
          "a CJK-capable backend receives text directly");
    frame.End();
    frame.ReplayText();
    Check(!s.HasInk(), "no host overlay when the backend owns text");
    frame.Clear();

    // ---------- 4. Back to GDI ----------
    gfx::Fill(s.dc, RECT{ 0, 0, 64, 32 }, RGB(0, 255, 0));
    Check(s.At(8, 8) == RGB(0, 255, 0),
          "GDI path fully restored after the frame closes");

    std::printf("\n%s (%d failure(s))\n",
                g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
