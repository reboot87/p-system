// ============================================================
// AirUI - test_shell_core.cpp (core-only, no platform headers)
// ============================================================
#include "airui/ShellCore.h"
#include "airui/Theme.h"

#include <cstdio>
#include <vector>

using namespace air::ui;

namespace {

struct Op {
    enum Type { FillRect, StrokeRect, Text } type;
    Rect rect;
    Color color = 0;
    std::wstring text;
    FontRole role = FontRole::Body;
};

class RecordingPainter : public IPainter {
public:
    std::vector<Op> ops;
    void FillRect(const Rect& r, Color c) override {
        Op op; op.type = Op::FillRect; op.rect = r; op.color = c; ops.push_back(op);
    }
    void FillRound(const Rect&, int, Color) override {}
    void StrokeRect(const Rect& r, Color c, int) override {
        Op op; op.type = Op::StrokeRect; op.rect = r; op.color = c; ops.push_back(op);
    }
    void StrokeRound(const Rect&, int, Color, int) override {}
    void StrokeDashed(const Rect&, Color, int, int) override {}
    void Text(const Rect& r, const std::wstring& s, FontRole role, Color c, TextFormat) override {
        Op op; op.type = Op::Text; op.rect = r; op.color = c; op.text = s; op.role = role; ops.push_back(op);
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

    ShellCore shell;
    shell.SetCaption({L"Title", L"Subtitle", L"OK", true});
    shell.SetStatus({L"Ready", true, L"Extra"});

    std::vector<NavGroup> groups;
    NavGroup g;
    g.title = L"Main";
    g.items.push_back({1, L"Home", false, L"3", true});
    g.items.push_back({2, L"Settings", false, L"", true});
    groups.push_back(std::move(g));
    shell.SetNavGroups(std::move(groups));
    shell.SetActiveNav(1);

    Rect client{0,0,800,600};
    shell.Layout(client);

    Check(shell.CaptionRect().Height() > 0, "Caption rect non-empty");
    Check(shell.NavRect().Width() > 0, "Nav rect non-empty");
    Check(shell.StatusRect().Height() > 0, "Status rect non-empty");
    Check(shell.ContentRect().Width() > 0, "Content rect non-empty");

    RecordingPainter p;
    shell.Paint(p, pal, mx);

    bool hasCaptionText = false;
    bool hasStatusText = false;
    bool hasNavText = false;
    for (auto& op : p.ops) {
        if (op.type == Op::Text) {
            if (op.text == L"Title") hasCaptionText = true;
            if (op.text == L"Ready") hasStatusText = true;
            if (op.text == L"Home") hasNavText = true;
        }
    }
    Check(hasCaptionText, "ShellCore paints caption title");
    Check(hasStatusText, "ShellCore paints status text");
    Check(hasNavText, "ShellCore paints nav item");

    std::printf("PASS\n");
    return 0;
}
