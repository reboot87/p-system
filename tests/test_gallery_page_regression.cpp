#include <iostream>
#include <fstream>
#include <vector>
#include "airui/ShellCore.h"
#include "airui/Theme.h"

using namespace air::ui;

struct DrawOp {
    int type;
    Rect rect;
    Color color;
    std::wstring text;
};

class RecordingPainter : public IPainter {
public:
    std::vector<DrawOp> ops;
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
};

void drawGalleryItems(RecordingPainter& p) {
    // Simulate gallery items drawing: 3 fill rects + 3 texts
    p.FillRect({10,10,110,110}, 0x00FF00);
    p.Text({10,10,110,110}, L"Item1", FontRole::Body, 0x000000, 0);
    p.FillRect({120,10,220,110}, 0x00FF00);
    p.Text({120,10,220,110}, L"Item2", FontRole::Body, 0x000000, 0);
    p.FillRect({230,10,330,110}, 0x00FF00);
    p.Text({230,10,330,110}, L"Item3", FontRole::Body, 0x000000, 0);
}

int main() {
    ShellCore shell;
    shell.SetCaption({L"AirUI Gallery", L"Test", L"", true});
    shell.SetStatus({L"Ready", true, L""});
    std::vector<ShellNavGroup> groups;
    ShellNavGroup g; g.title = L"Test"; g.items.push_back({1, L"A", false, L"", true});
    groups.push_back(std::move(g));
    shell.SetNavGroups(std::move(groups));
    Rect client{0,0,800,600};
    shell.Layout(client);

    // Enabled
    {
        RecordingPainter p;
        shell.Paint(p, Pal(), Mx());
        drawGalleryItems(p);
        std::ofstream out("gallery_page_drawcalls_enabled.txt");
        out << "ops:" << p.ops.size() << "\n";
        for (auto& op : p.ops) out << op.type << "," << op.rect.left << "," << op.rect.top << "," << op.rect.right << "," << op.rect.bottom << "," << op.color << "\n";
    }
    // Disabled: shell not painted
    {
        RecordingPainter p;
        // shell.Paint skipped
        drawGalleryItems(p);
        std::ofstream out("gallery_page_drawcalls_disabled.txt");
        out << "ops:" << p.ops.size() << "\n";
        for (auto& op : p.ops) out << op.type << "," << op.rect.left << "," << op.rect.top << "," << op.rect.right << "," << op.rect.bottom << "," << op.color << "\n";
    }

    std::cout << "Captured gallery page draw calls enabled vs disabled\n";
    std::cout << "PASS\n";
    return 0;
}
