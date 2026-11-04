// ============================================================
// AirUI - test_controls_core.cpp (core-only, no platform headers)
// ============================================================
#include "airui/ControlsCore.h"
#include "airui/Theme.h"

#include <cstdio>
#include <vector>
#include <cstdint>

using namespace air::ui;

namespace {

struct Op {
    enum Type {
        FillRound, StrokeRound, Text
    } type = FillRound;
    Rect rect{};
    Color color{};
    int radius = 0;
    std::wstring text;
    FontRole role = FontRole::Body;
    TextFormat fmt = 0;
};

class RecordingPainter : public IPainter {
public:
    std::vector<Op> ops;

    void FillRect(const Rect&, Color) override {}
    void FillRound(const Rect& r, int radius, Color c) override {
        Op op; op.type = Op::FillRound; op.rect = r; op.color = c; op.radius = radius;
        ops.push_back(op);
    }
    void StrokeRect(const Rect&, Color, int) override {}
    void StrokeRound(const Rect& r, int radius, Color c, int) override {
        Op op; op.type = Op::StrokeRound; op.rect = r; op.color = c; op.radius = radius;
        ops.push_back(op);
    }
    void StrokeDashed(const Rect&, Color, int, int) override {}
    void Text(const Rect& r, const std::wstring& s, FontRole role, Color c, TextFormat fmt) override {
        ops.push_back({Op::Text, r, c, 0, s, role, fmt});
    }
    void TextBlock(const Rect&, const std::wstring&, FontRole, Color, int) override {}
    int MeasureText(const std::wstring&, FontRole) override { return 0; }
    void PushClip(const Rect&) override {}
    void PopClip() override {}
    void Translate(Point) override {}
};

void Check(bool ok, const char* msg) {
    if (!ok) { std::printf("FAIL: %s\n", msg); std::exit(1); }
    std::printf("PASS: %s\n", msg);
}

} // namespace

int main()
{
    const Palette& pal = Pal();
    const Metrics& mx = Mx();

    // ---- Button Primary, normal ----
    {
        RecordingPainter p;
        Rect rc{0, 0, 100, 32};
        ControlState st{};
        PaintButton(p, rc, L"Save", ButtonStyle::Primary, st, pal, mx);
        Check(p.ops.size() == 2, "Button Primary normal: 2 ops");
        Check(p.ops[0].type == Op::FillRound && p.ops[0].color == pal.primary,
              "Button Primary normal: fill primary");
        Check(p.ops[1].type == Op::Text && p.ops[1].text == L"Save" && p.ops[1].role == FontRole::Button
              && p.ops[1].color == pal.textInverse,
              "Button Primary normal: text inverse");
    }

    // ---- Button Primary, hover ----
    {
        RecordingPainter p;
        Rect rc{0,0,100,32};
        ControlState st; st.hover = true;
        PaintButton(p, rc, L"Save", ButtonStyle::Primary, st, pal, mx);
        Check(p.ops[0].color == pal.primaryHover, "Button Primary hover: fill hover");
    }

    // ---- Button Primary, pressed ----
    {
        RecordingPainter p;
        Rect rc{0,0,100,32};
        ControlState st; st.pressed = true;
        PaintButton(p, rc, L"Save", ButtonStyle::Primary, st, pal, mx);
        Check(p.ops[0].color == pal.primaryDown, "Button Primary pressed: fill down");
    }

    // ---- Button Primary, disabled ----
    {
        RecordingPainter p;
        Rect rc{0,0,100,32};
        ControlState st; st.disabled = true;
        PaintButton(p, rc, L"Save", ButtonStyle::Primary, st, pal, mx);
        Check(p.ops[0].color == pal.border, "Button Primary disabled: fill border");
    }

    // ---- Button Secondary, hover ----
    {
        RecordingPainter p;
        Rect rc{0,0,100,32};
        ControlState st; st.hover = true;
        PaintButton(p, rc, L"Edit", ButtonStyle::Secondary, st, pal, mx);
        Check(p.ops[0].type == Op::FillRound && p.ops[0].color == pal.active,
              "Button Secondary hover: fill active");
        Check(p.ops.size() >= 2 && p.ops[1].type == Op::StrokeRound,
              "Button Secondary hover: stroke present");
    }

    // ---- Button Danger, hover ----
    {
        RecordingPainter p;
        Rect rc{0,0,100,32};
        ControlState st; st.hover = true;
        PaintButton(p, rc, L"Delete", ButtonStyle::Danger, st, pal, mx);
        Check(p.ops[0].color == pal.errorSoft, "Button Danger hover: fill errorSoft");
    }

    // ---- Button Ghost, hover ----
    {
        RecordingPainter p;
        Rect rc{0,0,100,32};
        ControlState st; st.hover = true;
        PaintButton(p, rc, L"Ghost", ButtonStyle::Ghost, st, pal, mx);
        // Ghost hover creates a fill first, then text. If hover fill is absent we would have just text.
        // Ensure first op is FillRound and color is primarySoft.
        Check(p.ops.size() >= 1 && p.ops[0].type == Op::FillRound && p.ops[0].color == pal.primarySoft,
              "Button Ghost hover: fill primarySoft");
    }

    // ---- Checkbox checked ----
    {
        RecordingPainter p;
        Rect rc{0,0,100,20};
        ControlState st; st.checked = true;
        PaintCheckbox(p, rc, L"Option", st, pal);
        Check(p.ops.size() == 4, "Checkbox checked: 4 ops");
        Check(p.ops[0].type == Op::FillRound && p.ops[0].radius == 3 && p.ops[0].color == pal.surface,
              "Checkbox: box filled surface");
        Check(p.ops[1].type == Op::StrokeRound && p.ops[1].radius == 3 && p.ops[1].color == pal.border,
              "Checkbox: box stroked border");
        Check(p.ops[2].type == Op::Text && p.ops[2].text == L"✓" && p.ops[2].color == pal.primary,
              "Checkbox: checkmark primary");
        Check(p.ops[3].type == Op::Text && p.ops[3].text == L"Option",
              "Checkbox: label painted");
    }

    // ---- Checkbox unchecked ----
    {
        RecordingPainter p;
        Rect rc{0,0,100,20};
        ControlState st{};
        PaintCheckbox(p, rc, L"Option", st, pal);
        // Should not paint a checkmark. The first two ops are box, third is label text.
        bool hasCheck = false;
        for (auto& o : p.ops) if (o.type == Op::Text && o.text == L"\u2713") hasCheck = true;
        Check(!hasCheck, "Checkbox unchecked: no checkmark");
    }

    // ---- Radio checked ----
    {
        RecordingPainter p;
        Rect rc{0,0,100,20};
        ControlState st; st.checked = true;
        PaintRadio(p, rc, L"Choice", st, pal);
        Check(p.ops.size() >= 3, "Radio checked: at least 3 ops");
        Check(p.ops[0].type == Op::FillRound && p.ops[0].radius == 8 && p.ops[0].color == pal.surface,
              "Radio: outer filled surface");
        Check(p.ops[1].type == Op::StrokeRound && p.ops[1].radius == 8 && p.ops[1].color == pal.border,
              "Radio: outer stroked border");
        bool hasDot = false;
        for (auto& o : p.ops) if (o.type == Op::FillRound && o.radius == 4 && o.color == pal.primary) hasDot = true;
        Check(hasDot, "Radio checked: inner dot primary");
    }

    std::printf("PASS\n");
    return 0;
}
