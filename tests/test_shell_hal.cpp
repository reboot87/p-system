// ============================================================
// AirUI - ShellCore HAL test (platform-free)
//
// Proves Phase 1 item 3: ShellCore owns Caption / NavBar / StatusBar
// state and layout, its hit test is driven by backend-supplied
// geometry, and it can paint through an arbitrary IPainter without
// any windowing dependency.
// ============================================================
#include "airui/ShellCore.h"
#include "airui/hal.h"
#include "airui/Theme.h"

#include <cstdio>
#include <vector>

using namespace air::ui;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what)
{
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("PASS: %s\n", what);
    }
}

bool Same(const Rect& r, int l, int t, int rt, int b)
{
    return r.left == l && r.top == t && r.right == rt && r.bottom == b;
}

// A headless IPainter: proves the core never needs a native HDC.
struct RecordingPainter : IPainter
{
    int fills = 0;
    int strokes = 0;
    int texts = 0;

    void FillRect(const Rect&, Color) override { ++fills; }
    void FillRectAlpha(const Rect&, Color, uint8_t) override { ++fills; }
    void FillRound(const Rect&, int, Color) override { ++fills; }
    void StrokeRect(const Rect&, Color, int) override { ++strokes; }
    void StrokeRound(const Rect&, int, Color, int) override { ++strokes; }
    void StrokeDashed(const Rect&, Color, int, int) override { ++strokes; }
    void Text(const Rect&, const std::wstring&, FontRole, Color, TextFormat) override { ++texts; }
    void TextBlock(const Rect&, const std::wstring&, FontRole, Color, int) override { ++texts; }
    int  MeasureText(const std::wstring&, FontRole) override { return 0; }
    void PushClip(const Rect&) override {}
    void PopClip() override {}
    void Translate(Point) override {}
};

} // namespace

int main()
{
    ShellCore core;

    // ---- core is the single source of truth for caption / status ----
    CaptionState cs;
    cs.title = L"App"; cs.subtitle = L"sub"; cs.badge = L"OK"; cs.badgeOk = true;
    core.SetCaption(cs);

    StatusState ss;
    ss.text = L"ready"; ss.ok = false; ss.extra = L"1286";
    core.SetStatus(ss);

    Check(core.Caption().title == L"App" && core.Caption().badgeOk,
          "caption state round-trip");
    Check(core.Status().text == L"ready" && !core.Status().ok,
          "status state round-trip");

    // ---- nav groups / badges / active item live in core ----
    std::vector<ShellNavGroup> groups(1);
    groups[0].title = L"Group";
    ShellNavItem a; a.id = 11; a.label = L"A";
    ShellNavItem b; b.id = 22; b.label = L"B"; b.disabled = true;
    groups[0].items = { a, b };
    core.SetNavGroups(groups);
    core.SetNavBadge(11, L"3", false);
    core.SetActiveNav(11);

    Check(core.NavGroups().size() == 1 &&
          core.NavGroups()[0].items[0].badge == L"3" &&
          !core.NavGroups()[0].items[0].badgeOk,
          "nav badge stored in core");
    Check(core.ActiveNav() == 11, "active nav stored in core");

    // ---- backend-supplied geometry drives the platform-free layout ----
    core.SetShellGeometry(40, 20, 200);
    core.Layout(Rect{ 0, 0, 800, 600 });

    Check(Same(core.CaptionRect(), 0, 0, 800, 40), "caption rect uses backend geometry");
    Check(Same(core.NavRect(), 0, 40, 200, 580), "nav rect uses backend geometry");
    Check(Same(core.StatusRect(), 0, 580, 800, 600), "status rect uses backend geometry");
    Check(Same(core.ContentRect(), 200, 40, 800, 580), "content rect uses backend geometry");

    // ---- platform-free hit test matches the supplied geometry ----
    const Metrics& mx = Mx();
    const int pad = mx.unit * 2;
    // LayoutNav: y starts at pad, then adds a group-title row (18 + unit),
    // then shifts by navRect.top. So the first item's screen top is:
    const int firstTop = 40 + pad + 18 + mx.unit;
    Check(core.HitTestNav(Point{ pad + 2, firstTop + mx.rowH / 2 }) == 11,
          "hit test resolves the first nav item");
    Check(core.HitTestNav(Point{ 400, 10 }) == -1,
          "hit test outside the nav rect returns -1");
    Check(core.HitTestNav(Point{ 5, 45 }) == -1,
          "hit test in a blank nav area returns -1");

    core.SetNavHover(11);
    Check(core.NavHover() == 11, "nav hover state");

    // ---- core can drive any IPainter, with no windowing backend ----
    RecordingPainter painter;
    core.Paint(painter, Pal(), Mx());
    Check(painter.fills >= 3, "core painted caption / nav / status via IPainter");
    Check(painter.texts >= 1, "core emitted text via IPainter");

    // ---- a backend may have no status bar / sidebar at all ----
    core.SetShellGeometry(40, 0, 200);
    core.Layout(Rect{ 0, 0, 800, 600 });
    Check(core.StatusRect().Empty(), "backend can disable the status bar");

    if (g_failures) std::printf("FAILED: %d\n", g_failures);
    else            std::printf("ALL PASS (%d failures)\n", g_failures);
    return g_failures ? 1 : 0;
}
