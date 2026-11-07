// ============================================================
// test_paint_backend.cpp
// Runtime painter backend selection (no Skia SDK, no platform headers).
// Verifies that the choice of backend happens at runtime:
//   - explicit id, "auto" -> default, and hot default switching;
//   - graceful fallback to gdi when a backend is missing, unavailable
//     or fails to open a frame;
//   - the precedence used by Shell when resolving the choice.
// ============================================================
#include "airui/PaintBackend.h"

#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace air::ui;

namespace {

int g_fail = 0;

void Check(bool ok, const char* what)
{
    if (!ok) { std::printf("FAIL: %s\n", what); ++g_fail; }
}

class TagPainter : public IPainter
{
public:
    void FillRect(const Rect&, Color) override { ++calls; }
    void FillRectAlpha(const Rect&, Color, uint8_t) override { ++calls; }
    void FillRound(const Rect&, int, Color) override { ++calls; }
    void StrokeRect(const Rect&, Color, int) override { ++calls; }
    void StrokeRound(const Rect&, int, Color, int) override { ++calls; }
    void StrokeDashed(const Rect&, Color, int, int) override { ++calls; }
    void Text(const Rect&, const std::wstring&, FontRole, Color,
              TextFormat) override { ++calls; }
    void TextBlock(const Rect&, const std::wstring&, FontRole, Color,
                   int) override { ++calls; }
    int  MeasureText(const std::wstring&, FontRole) override { return 0; }
    void PushClip(const Rect&) override { ++calls; }
    void PopClip() override { ++calls; }
    void Translate(Point) override { ++calls; }

    int calls = 0;
};

class FakeProvider : public IPainterProvider
{
public:
    FakeProvider(std::string id, bool available, bool opens)
        : m_id(std::move(id)), m_available(available), m_opens(opens) {}

    std::string Id() const override { return m_id; }
    bool Available() const override { return m_available; }
    std::unique_ptr<IPainter> Begin(const PaintTarget&) override
    {
        if (!m_opens) return nullptr;
        ++opened;
        return std::unique_ptr<IPainter>(new TagPainter());
    }

    void SetAvailable(bool on) { m_available = on; }
    void SetOpens(bool on)     { m_opens = on; }
    int  opened = 0;

private:
    std::string m_id;
    bool m_available;
    bool m_opens;
};

bool Has(const std::vector<std::string>& ids, const std::string& id)
{
    for (const std::string& s : ids) if (s == id) return true;
    return false;
}

} // namespace

int main()
{
    // --- pure resolution helpers (platform-free) ---
    Check(NormalizeBackend("  SKIA  ") == "skia", "NormalizeBackend trims/lowercases");
    Check(NormalizeBackend("") == "auto", "empty request means auto");
    Check(ResolvePaintBackend("skia", "gdi") == "skia",
          "explicit request beats environment");
    Check(ResolvePaintBackend("", "skia") == "skia",
          "environment wins when no explicit request");
    Check(ResolvePaintBackend("", "auto", true) == "skia",
          "legacy skia preference resolves to skia");
    Check(ResolvePaintBackend("", "auto") == "auto",
          "nothing set resolves to auto");

    PainterRegistry& reg = PainterRegistry::Instance();
    reg.Reset();

    FakeProvider* gdi  = new FakeProvider("gdi", true, true);
    FakeProvider* skia = new FakeProvider("skia", true, true);
    reg.Register(std::unique_ptr<IPainterProvider>(gdi));
    reg.Register(std::unique_ptr<IPainterProvider>(skia));

    const std::vector<std::string> ids = reg.Backends();
    Check(ids.size() == 2u && Has(ids, "gdi") && Has(ids, "skia"),
          "both providers registered");
    Check(reg.Available("gdi") && reg.Available("skia"),
          "both providers available");
    Check(reg.Default() == "gdi", "default backend is gdi");

    PaintTarget target{ nullptr, Rect{ 0, 0, 64, 48 } };

    // --- explicit runtime selection ---
    std::unique_ptr<IPainter> p = reg.Begin("skia", target);
    Check(p != nullptr, "explicit skia produced a painter");
    Check(skia->opened == 1 && gdi->opened == 0, "skia provider was used");
    p->FillRect(Rect{ 0, 0, 8, 8 }, 0);
    p.reset();   // destruction flushes the frame

    // --- auto follows the default ---
    p = reg.Begin("auto", target);
    Check(p != nullptr, "auto produced a painter");
    Check(gdi->opened == 1, "auto used the default gdi backend");
    p.reset();

    // --- unknown id falls back to gdi ---
    p = reg.Begin("not-installed", target);
    Check(p != nullptr, "unknown backend still yields a painter");
    Check(gdi->opened == 2, "unknown backend fell back to gdi");
    Check(reg.LastError().find("not registered") != std::string::npos,
          "LastError explains missing backend");
    Check(reg.LastError().find("fell back to gdi") != std::string::npos,
          "LastError records the fallback");
    p.reset();

    // --- unavailable backend falls back ---
    skia->SetAvailable(false);
    Check(!reg.Available("skia"), "skia reports unavailable");
    p = reg.Begin("skia", target);
    Check(p != nullptr && gdi->opened == 3, "unavailable skia fell back to gdi");
    Check(reg.LastError().find("unavailable") != std::string::npos,
          "LastError explains unavailability");
    p.reset();

    // --- a provider that cannot open a frame falls back too ---
    skia->SetAvailable(true);
    skia->SetOpens(false);
    p = reg.Begin("skia", target);
    Check(p != nullptr && gdi->opened == 4, "failed frame fell back to gdi");
    Check(reg.LastError().find("failed to open") != std::string::npos,
          "LastError explains frame failure");
    p.reset();

    // --- hot switch: change the default at runtime, same binary ---
    skia->SetOpens(true);
    reg.SetDefault("skia");
    p = reg.Begin("auto", target);
    Check(p != nullptr && skia->opened == 2, "runtime default switch to skia");
    p.reset();
    reg.SetDefault("gdi");
    p = reg.Begin("auto", target);
    Check(p != nullptr && gdi->opened == 5, "runtime default switch back to gdi");
    p.reset();

    // --- re-registration replaces a provider with the same id ---
    FakeProvider* replacement = new FakeProvider("skia", true, true);
    reg.Register(std::unique_ptr<IPainterProvider>(replacement));
    Check(reg.Backends().size() == 2u, "re-registration does not duplicate ids");
    p = reg.Begin("skia", target);
    Check(p != nullptr && replacement->opened == 1,
          "replacement provider is the one being used");
    p.reset();

    reg.Reset();
    Check(reg.Backends().empty(), "Reset clears the registry");

    if (g_fail == 0)
        std::printf("test_paint_backend: runtime selection + fallback OK\n");
    else
        std::printf("test_paint_backend: %d check(s) failed\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
