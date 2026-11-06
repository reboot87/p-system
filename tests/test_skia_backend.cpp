// ============================================================
// test_skia_backend.cpp
// Real second-backend check for G5 (docs/cross-platform.md).
//
// Only built when AIRUI_WITH_SKIA=ON. It:
//   1. records the platform-free golden with core's PaintRecorder;
//   2. replays the SAME scene through the Skia raster backend while
//      recording every IPainter call the backend actually receives;
//   3. asserts SequenceEquals / Diff == empty (call-sequence parity);
//   4. checks the Skia raster output really produced BGRA pixels.
//
// This is the assertion that was impossible before a second backend
// existed. Pixel-level GDI-vs-Skia screenshot comparison is a later
// step (cross-platform.md section 6).
// ============================================================
#include "airui/PaintRecorder.h"
#include "airui/SkiaSurface.h"

#include <cstdio>
#include <string>

using namespace air::ui;

namespace {
int g_fail = 0;

void Check(bool ok, const char* what)
{
    if (!ok) { std::printf("FAIL: %s\n", what); ++g_fail; }
}

// Forwards every drawing call to a real backend while also recording
// it. MeasureText is a query, not a draw call, so it is not recorded
// (keeps both sequences strictly comparable).
class TeePainter : public IPainter
{
public:
    TeePainter(IPainter& real, PaintRecorder& rec) : m_real(real), m_rec(rec) {}

    void FillRect(const Rect& r, Color c) override
    { m_rec.FillRect(r, c); m_real.FillRect(r, c); }
    void FillRectAlpha(const Rect& r, Color c, uint8_t a) override
    { m_rec.FillRectAlpha(r, c, a); m_real.FillRectAlpha(r, c, a); }
    void FillRound(const Rect& r, int rad, Color c) override
    { m_rec.FillRound(r, rad, c); m_real.FillRound(r, rad, c); }
    void StrokeRect(const Rect& r, Color c, int w) override
    { m_rec.StrokeRect(r, c, w); m_real.StrokeRect(r, c, w); }
    void StrokeRound(const Rect& r, int rad, Color c, int w) override
    { m_rec.StrokeRound(r, rad, c, w); m_real.StrokeRound(r, rad, c, w); }
    void StrokeDashed(const Rect& r, Color c, int w, int d) override
    { m_rec.StrokeDashed(r, c, w, d); m_real.StrokeDashed(r, c, w, d); }
    void Text(const Rect& r, const std::wstring& s,
              FontRole role, Color c, TextFormat fmt) override
    { m_rec.Text(r, s, role, c, fmt); m_real.Text(r, s, role, c, fmt); }
    void TextBlock(const Rect& r, const std::wstring& s,
                   FontRole role, Color c, int lines) override
    { m_rec.TextBlock(r, s, role, c, lines); m_real.TextBlock(r, s, role, c, lines); }
    int MeasureText(const std::wstring& s, FontRole role) override
    { return m_real.MeasureText(s, role); }
    void PushClip(const Rect& r) override
    { m_rec.PushClip(r); m_real.PushClip(r); }
    void PopClip() override
    { m_rec.PopClip(); m_real.PopClip(); }
    void Translate(Point o) override
    { m_rec.Translate(o); m_real.Translate(o); }

private:
    IPainter&      m_real;
    PaintRecorder& m_rec;
};

// One deterministic scene covering every drawing primitive.
void DrawScene(IPainter& p)
{
    const Color bg     = MakeColor(0xF3, 0xF4, 0xF6);
    const Color white  = MakeColor(0xFF, 0xFF, 0xFF);
    const Color border = MakeColor(0xCC, 0xCC, 0xCC);
    const Color ink    = MakeColor(0x11, 0x22, 0x33);

    p.FillRect(Rect{ 0, 0, 200, 100 }, bg);
    p.FillRectAlpha(Rect{ 0, 0, 200, 100 }, MakeColor(0, 0, 0), 40);
    p.FillRound(Rect{ 10, 10, 90, 40 }, 6, white);
    p.StrokeRect(Rect{ 10, 10, 90, 40 }, border, 1);
    p.StrokeRound(Rect{ 12, 12, 88, 38 }, 6, ink, 2);
    p.StrokeDashed(Rect{ 5, 5, 195, 95 }, border, 1, 4);
    p.PushClip(Rect{ 0, 0, 200, 50 });
    p.Text(Rect{ 16, 14, 180, 36 }, L"AirUI Skia", FontRole::Title,
           ink, dt::VCenter | dt::SingleLine);
    p.PopClip();
    p.TextBlock(Rect{ 16, 48, 180, 96 }, L"cross backend golden",
                FontRole::Body, ink, 2);
    p.Translate(Point{ 2, 3 });
    p.FillRect(Rect{ 0, 0, 4, 4 }, ink);
}
} // namespace

int main()
{
    // 1. Platform-free golden (identical on Windows / Linux / macOS).
    PaintRecorder golden;
    DrawScene(golden);
    Check(golden.Ops().size() == 12u, "golden has 12 draw calls");

    // 2. Same scene, executed by the real Skia backend.
    SkiaFrame frame(200, 100);
    Check(frame.Valid(), "Skia frame is valid");
    Check(frame.Canvas() != nullptr, "Skia canvas created");
    Check(frame.Width() == 200 && frame.Height() == 100, "frame size");

    PaintRecorder observed;
    TeePainter tee(frame, observed);
    DrawScene(tee);

    // 3. G5: the backend received exactly the golden call sequence.
    Check(observed.Ops().size() == golden.Ops().size(), "call count parity");
    Check(golden.FirstDiff(observed) == static_cast<size_t>(-1),
          "FirstDiff reports equality");
    Check(golden.SequenceEquals(observed), "SequenceEquals parity");
    const std::string diff = golden.Diff(observed);
    if (!diff.empty()) std::printf("  diff: %s\n", diff.c_str());
    Check(diff.empty(), "Diff is empty (G5 gate)");

    // 4. Skia actually rasterised pixels (BGRA, premultiplied).
    Check(frame.Pixels() != nullptr, "raster pixels available");
    Check(frame.RowBytes() >= 200u * 4u, "row stride is sane");

    SkiaFrame solid(32, 32);
    solid.FillRect(Rect{ 0, 0, 32, 32 }, MakeColor(0xDD, 0x00, 0x00));
    const unsigned char* px =
        static_cast<const unsigned char*>(solid.Pixels());
    if (px) {
        const size_t i = 16u * solid.RowBytes() + 16u * 4u;
        // core 0x000000DD -> Skia 0xFFDD0000 -> BGRA bytes 00 00 DD FF
        Check(px[i + 0] == 0x00, "pixel blue channel");
        Check(px[i + 1] == 0x00, "pixel green channel");
        Check(px[i + 2] == 0xDD, "pixel red channel");
        Check(px[i + 3] == 0xFF, "pixel alpha channel");
    }

    if (g_fail == 0) std::printf("test_skia_backend: G5 parity + raster OK\n");
    else             std::printf("test_skia_backend: %d check(s) failed\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
