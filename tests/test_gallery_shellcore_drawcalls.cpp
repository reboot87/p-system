#include <iostream>
#include <fstream>
#include "airui/ShellCore.h"
#include "airui/Theme.h"

using namespace air::ui;

int main() {
    ShellCore shell;
    shell.SetCaption({L"AirUI Gallery", L"Test", L"", true});
    shell.SetStatus({L"Ready", true, L""});
    std::vector<ShellNavGroup> groups;
    ShellNavGroup g;
    g.title = L"Test";
    g.items.push_back({1, L"A", false, L"", true});
    groups.push_back(std::move(g));
    shell.SetNavGroups(std::move(groups));

    Rect client{0,0,800,600};
    shell.Layout(client);

    // Use RecordingPainter to capture ops
    struct Op { int type; Rect rect; Color color; std::wstring text; };
    class RecPainter : public IPainter {
    public:
        std::vector<Op> ops;
        void FillRect(const Rect& r, Color c) override { ops.push_back({1, r, c, L""}); }
        void FillRound(const Rect&, int, Color) override {}
        void StrokeRect(const Rect& r, Color c, int) override { ops.push_back({2, r, c, L""}); }
        void StrokeRound(const Rect&, int, Color, int) override {}
        void StrokeDashed(const Rect&, Color, int, int) override {}
        void Text(const Rect& r, const std::wstring& s, FontRole, Color c, TextFormat) override { ops.push_back({3, r, c, s}); }
        void TextBlock(const Rect&, const std::wstring&, FontRole, Color, int) override {}
        int MeasureText(const std::wstring&, FontRole) override { return 0; }
        void PushClip(const Rect&) override {}
        void PopClip() override {}
        void Translate(Point) override {}
    } painter;

    shell.Paint(painter, Pal(), Mx());
    std::cout << "ShellCore draw ops count: " << painter.ops.size() << "\n";
    for (size_t i=0;i<painter.ops.size() && i<10; ++i) {
        std::wcout << L"Op " << i << L": type=" << painter.ops[i].type << L" text=" << painter.ops[i].text << L"\n";
    }
    std::cout << "PASS\n";
    return 0;
}
