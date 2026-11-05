// ============================================================
// AirUI - test_overlay_core.cpp (core-only, no platform headers)
// Covers overlay placement / z-order / modal blocking, tab order
// and focus trapping with focus restore, plus the drawing helpers.
// ============================================================
#include "airui/OverlayCore.h"
#include "airui/Theme.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace air::ui;

namespace {

enum class OpType { FillRect, FillRound, StrokeRect, StrokeRound, Text };

struct Op {
    OpType type = OpType::FillRect;
    Rect rc{};
    Color color{};
    std::wstring text;
};

class RecordingPainter : public IPainter {
public:
    std::vector<Op> ops;
    int clipDepth = 0;
    int lastAlpha = -1;

    void FillRectAlpha(const Rect& r, Color c, uint8_t alpha) override {
        lastAlpha = (int)alpha;
        FillRect(r, c);
    }
    void FillRect(const Rect& r, Color c) override {
        ops.push_back({OpType::FillRect, r, c, L""});
    }
    void FillRound(const Rect& r, int, Color c) override {
        ops.push_back({OpType::FillRound, r, c, L""});
    }
    void StrokeRect(const Rect& r, Color c, int) override {
        ops.push_back({OpType::StrokeRect, r, c, L""});
    }
    void StrokeRound(const Rect& r, int, Color c, int) override {
        ops.push_back({OpType::StrokeRound, r, c, L""});
    }
    void StrokeDashed(const Rect&, Color, int, int) override {}
    void Text(const Rect& r, const std::wstring& s, FontRole, Color c, TextFormat) override {
        ops.push_back({OpType::Text, r, c, s});
    }
    void TextBlock(const Rect&, const std::wstring&, FontRole, Color, int) override {}
    int MeasureText(const std::wstring&, FontRole) override { return 40; }
    void PushClip(const Rect&) override { ++clipDepth; }
    void PopClip() override { --clipDepth; }
    void Translate(Point) override {}
};

void Check(bool ok, const char* msg) {
    if (!ok) { std::printf("FAIL: %s\n", msg); std::exit(1); }
    std::printf("PASS: %s\n", msg);
}

bool SameRect(const Rect& a, const Rect& b) {
    return a.left == b.left && a.top == b.top &&
           a.right == b.right && a.bottom == b.bottom;
}

bool HasOp(const RecordingPainter& p, OpType type, Color color) {
    for (const auto& o : p.ops) if (o.type == type && o.color == color) return true;
    return false;
}

bool HasText(const RecordingPainter& p, const std::wstring& s) {
    for (const auto& o : p.ops)
        if (o.type == OpType::Text && o.text.find(s) != std::wstring::npos) return true;
    return false;
}

OverlayItem Layer(Rect bounds, int focusGroup = 0, bool modal = false,
                  bool closeOnOutside = true)
{
    OverlayItem o;
    o.bounds = bounds;
    o.focusGroup = focusGroup;
    o.modal = modal;
    o.closeOnOutside = closeOnOutside;
    return o;
}

FocusEntry Entry(int id, Rect bounds, int tabIndex, const wchar_t* name,
                 bool enabled = true)
{
    FocusEntry e;
    e.id = id;
    e.bounds = bounds;
    e.tabIndex = tabIndex;
    e.name = name;
    e.enabled = enabled;
    return e;
}

void TestPlacement() {
    const Rect client(0, 0, 600, 400);
    const Rect anchor(100, 100, 200, 130);

    const Rect below = PlaceOverlay(anchor, Size{160, 100}, client,
                                    OverlayPlacement::Below);
    Check(below.top == anchor.bottom && below.left == anchor.left,
          "PlaceOverlay: Below sits under the anchor");

    const Rect nearBottom(100, 350, 200, 380);
    const Rect flipped = PlaceOverlay(nearBottom, Size{160, 100}, client,
                                      OverlayPlacement::Below);
    Check(flipped.bottom <= nearBottom.top,
          "PlaceOverlay: flips above when the bottom has no room");

    const Rect nearRight(520, 100, 590, 130);
    const Rect leftFlip = PlaceOverlay(nearRight, Size{160, 100}, client,
                                       OverlayPlacement::Right);
    Check(leftFlip.right <= nearRight.left,
          "PlaceOverlay: flips left when the right has no room");

    const Rect clamped = PlaceOverlay(Rect(-50, -50, -10, -10), Size{160, 100},
                                      client, OverlayPlacement::Below);
    Check(clamped.left >= client.left && clamped.top >= client.top &&
          clamped.right <= client.right && clamped.bottom <= client.bottom,
          "PlaceOverlay: clamps an off-screen anchor into the client");

    const Rect centre = PlaceOverlay(Rect{}, Size{200, 100}, client,
                                     OverlayPlacement::Center);
    Check(centre.left == 200 && centre.top == 150,
          "PlaceOverlay: Center ignores the anchor");

    Check(PlaceOverlay(anchor, Size{0, 10}, client, OverlayPlacement::Below).Empty() &&
          PlaceOverlay(anchor, Size{10, 10}, Rect{}, OverlayPlacement::Below).Empty(),
          "PlaceOverlay: zero size or empty client is a no-op");
}

void TestStack() {
    OverlayStack s;
    const int a = s.Open(Layer(Rect(10, 10, 110, 110)));
    const int b = s.Open(Layer(Rect(50, 50, 150, 150)));
    Check(a != b && s.Count() == 2 && s.Layers()[0].id == a &&
          s.Layers()[1].z > s.Layers()[0].z,
          "OverlayStack: assigns distinct ids and ascending z");

    const OverlayHit overlap = s.Hit(Point{60, 60});
    Check(overlap.inside && overlap.id == b,
          "OverlayStack: top-most layer wins an overlapping hit");
    Check(s.Hit(Point{300, 300}).outsideTop,
          "OverlayStack: reports hits outside every layer");

    const int m = s.Open(Layer(Rect(200, 200, 300, 300), 0, true));
    const OverlayHit blocked = s.Hit(Point{60, 60});
    Check(blocked.blocked && blocked.id == m,
          "OverlayStack: modal top layer blocks hits on lower layers");
    const OverlayHit inside = s.Hit(Point{250, 250});
    Check(inside.inside && !inside.blocked && inside.id == m,
          "OverlayStack: modal layer still receives hits inside");

    Check(s.CloseTop() && s.TopId() == b, "OverlayStack: CloseTop removes the top");
    Check(s.Close(a) && s.Count() == 1 && !s.Close(a),
          "OverlayStack: Close by id, second close is a no-op");
    s.Clear();
    Check(s.Count() == 0 && s.Hit(Point{1, 1}).outsideTop,
          "OverlayStack: Clear leaves no layers");
}

void TestFocusScope() {
    FocusScope f;
    f.SetEntries({
        Entry(1, Rect(0, 0, 10, 10),   20, L"c"),
        Entry(2, Rect(20, 0, 30, 10),  10, L"a"),
        Entry(3, Rect(40, 0, 50, 10),  30, L"disabled", false),
    });
    Check(f.Entries()[0].id == 2, "FocusScope: stable sort by tabIndex");
    Check(f.First(0) == 2 && f.Last(0) == 1,
          "FocusScope: First/Last ignore disabled entries");

    Check(f.SetFocus(2), "FocusScope: SetFocus accepts an enabled entry");
    Check(f.Next(0, false) == 1, "FocusScope: Tab skips disabled entries");
    Check(f.Next(0, true) == 1, "FocusScope: Shift+Tab wraps to the last enabled");
    f.SetFocus(1);
    Check(f.Next(0, false) == 2, "FocusScope: Tab cycles back to the first");
    Check(!f.SetFocus(3), "FocusScope: disabled entries cannot take focus");
    Check(f.FocusAt(Point{25, 5}, 0) == 2, "FocusScope: FocusAt hits the rect");
    Check(SameRect(f.FocusRect(), Rect(0, 0, 10, 10)),
          "FocusScope: FocusRect follows the focused entry");

    f.SetFocus(2);
    f.SetEnabled(2, false);
    Check(f.Focused() == -1, "FocusScope: disabling the focused entry clears focus");
    Check(f.Next(0, false) == 1, "FocusScope: Next starts from first when unfocused");
    f.UpdateBounds(1, Rect(5, 5, 15, 15));
    f.SetFocus(1);
    Check(SameRect(f.FocusRect(), Rect(5, 5, 15, 15)),
          "FocusScope: UpdateBounds moves the focus ring with the control");
}

void TestController() {
    const std::vector<FocusEntry> page = {
        Entry(100, Rect(0, 0, 80, 30),   10, L"Button A"),
        Entry(101, Rect(100, 0, 180, 30), 20, L"Button B"),
    };
    const std::vector<FocusEntry> options = {
        Entry(200, Rect(210, 50, 390, 75),  10, L"option 1"),
        Entry(201, Rect(210, 80, 390, 105), 20, L"option 2"),
    };
    OverlayItem dropdown = Layer(Rect(200, 40, 400, 160), 1);

    OverlayController ctl;
    ctl.SetPageEntries(page);
    Check(ctl.FocusedId() == 100, "Controller: initial focus is the first page entry");

    const OverlayResult clickB = ctl.OnPointer(Point{140, 15});
    Check(clickB.action == OverlayAction::FocusChanged && ctl.FocusedId() == 101,
          "Controller: clicking a control focuses it");

    ctl.Open(dropdown, options);
    Check(ctl.ActiveGroup() == 1 && ctl.FocusedId() == 200,
          "Controller: opening a layer traps focus inside it");
    Check(ctl.OnKey(keys::Tab, false).id == 201 &&
          ctl.OnKey(keys::Tab, false).id == 200,
          "Controller: Tab cycles inside the layer only");
    Check(ctl.OnKey(keys::Tab, true).id == 201,
          "Controller: Shift+Tab cycles backwards inside the layer");

    const OverlayResult inside = ctl.OnPointer(Point{230, 90});
    Check(inside.action == OverlayAction::FocusChanged && inside.id == 201,
          "Controller: clicks inside a layer move the layer focus");
    const OverlayResult enter = ctl.OnKey(keys::Return, false);
    Check(enter.action == OverlayAction::Activate && enter.id == 201,
          "Controller: Enter activates the focused entry");

    const OverlayResult esc = ctl.OnKey(keys::Escape, false);
    Check(esc.action == OverlayAction::Dismiss && ctl.Stack().Count() == 0 &&
          ctl.FocusedId() == 101,
          "Controller: Escape closes the layer and restores page focus");

    ctl.Open(dropdown, options);
    const OverlayResult outside = ctl.OnPointer(Point{10, 10});
    Check(outside.action == OverlayAction::Dismiss && ctl.Stack().Count() == 0,
          "Controller: outside click dismisses a non-modal layer");

    OverlayItem modal = Layer(Rect(150, 80, 450, 280), 2, true, false);
    const std::vector<FocusEntry> buttons = {
        Entry(301, Rect(200, 230, 280, 260), 10, L"OK"),
    };
    ctl.Open(modal, buttons);
    const OverlayResult blocked = ctl.OnPointer(Point{10, 10});
    Check(blocked.action == OverlayAction::Blocked && ctl.Stack().Count() == 1,
          "Controller: modal layer swallows outside clicks");
    Check(ctl.OnPointer(Point{240, 245}).id == 301,
          "Controller: modal layer still accepts focus inside");

    // Simulate Build() re-registering page entries after a resize.
    ctl.SetPageEntries(page);
    Check(ctl.ActiveGroup() == 2 && ctl.FocusedId() == 301,
          "Controller: re-registering page entries keeps the focus trap");
    Check(ctl.OnKey(keys::Escape, false).action == OverlayAction::Dismiss &&
          ctl.Stack().Count() == 0 && ctl.ActiveGroup() == 0,
          "Controller: Escape closes the modal and returns to the page group");

    Check(ctl.OnKey(65, false).action == OverlayAction::None,
          "Controller: unrelated keys are ignored");
}

void TestDrawing() {
    const Palette pal = LightPalette();
    const Metrics mx{};
    RecordingPainter g;

    OverlayItem item = Layer(Rect(10, 10, 210, 110), 0, true);
    item.title = L"Dialog";
    PaintOverlayFrame(g, item, pal, mx);
    Check(HasOp(g, OpType::FillRound, pal.surface) &&
          HasOp(g, OpType::StrokeRound, pal.primary) &&
          HasOp(g, OpType::FillRect, pal.split) &&
          HasText(g, L"Dialog"),
          "PaintOverlayFrame: card, modal border, title and separator");

    const Rect content = OverlayContentRect(item, mx);
    Check(content.left > item.bounds.left && content.top > item.bounds.top &&
          content.bottom < item.bounds.bottom && content.right < item.bounds.right,
          "OverlayContentRect: inset and below the title");
    Check(OverlayContentRect(OverlayItem{}, mx).Empty(),
          "OverlayContentRect: empty layer is a no-op");

    PaintOverlayScrim(g, Rect(0, 0, 100, 100), pal.bg);
    Check(HasOp(g, OpType::FillRect, pal.bg) && g.lastAlpha == 96,
          "PaintOverlayScrim: fills with the default scrim opacity");
    PaintOverlayScrim(g, Rect{}, pal.bg);
    Check(g.ops.size() == 5, "PaintOverlayScrim: empty area is a no-op");

    PaintFocusRing(g, Rect(5, 5, 50, 25), pal, mx);
    Check(HasOp(g, OpType::StrokeRound, pal.selectRing),
          "PaintFocusRing: draws the selection ring");
    PaintFocusRing(g, Rect{}, pal, mx);
    Check(g.ops.size() == 6, "PaintFocusRing: empty rect is a no-op");

    PaintOverlayScrim(g, Rect(0, 0, 10, 10), pal.bg, 200);
    Check(g.lastAlpha == 200, "PaintOverlayScrim: forwards a custom opacity");
    PaintOverlayScrim(g, Rect(0, 0, 10, 10), pal.bg, 0);
    Check(g.ops.size() == 7, "PaintOverlayScrim: zero opacity is a no-op");
}

} // namespace

int main() {
    TestPlacement();
    TestStack();
    TestFocusScope();
    TestController();
    TestDrawing();
    std::printf("test_overlay_core: all checks passed\n");
    return 0;
}
