// ============================================================
// AirUI - test_advanced_controls.cpp
// Core-only regression for the third wave of controls. No platform
// headers: this test links airui-core alone.
// ============================================================
#include "airui/ControlsAdvanced.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace air::ui;

namespace {

int g_checks = 0;

void Check(bool ok, const char* msg)
{
    ++g_checks;
    if (!ok) {
        std::printf("FAIL: %s\n", msg);
        std::exit(1);
    }
}

class RecordingPainter : public IPainter
{
public:
    int ops = 0;
    int texts = 0;
    int alphaFills = 0;
    int clipDepth = 0;
    int minClipDepth = 0;
    std::vector<Rect> rects;

    void Note(const Rect& r)
    {
        ++ops;
        if (!r.Empty()) rects.push_back(r);
    }
    void FillRect(const Rect& r, Color) override { Note(r); }
    void FillRectAlpha(const Rect& r, Color, uint8_t) override
    { ++alphaFills; Note(r); }
    void FillRound(const Rect& r, int, Color) override { Note(r); }
    void StrokeRect(const Rect& r, Color, int) override { Note(r); }
    void StrokeRound(const Rect& r, int, Color, int) override { Note(r); }
    void StrokeDashed(const Rect& r, Color, int, int) override { Note(r); }
    void Text(const Rect& r, const std::wstring&, FontRole, Color,
              TextFormat) override { ++texts; Note(r); }
    void TextBlock(const Rect& r, const std::wstring&, FontRole, Color,
                   int) override { ++texts; Note(r); }
    int MeasureText(const std::wstring& s, FontRole) override
    { return static_cast<int>(s.size()) * 8; }
    void PushClip(const Rect& r) override
    { ++clipDepth; Note(r); }
    void PopClip() override
    { --clipDepth; if (clipDepth < minClipDepth) minClipDepth = clipDepth; }
    void Translate(Point) override {}
};

Point Center(const Rect& r)
{
    return Point{(r.left + r.right) / 2, (r.top + r.bottom) / 2};
}

bool Inside(const Rect& outer, const Rect& inner)
{
    return !inner.Empty() && inner.left >= outer.left &&
           inner.top >= outer.top && inner.right <= outer.right &&
           inner.bottom <= outer.bottom;
}

bool NoOverlap(const Rect& a, const Rect& b)
{
    return a.right <= b.left || b.right <= a.left ||
           a.bottom <= b.top || b.bottom <= a.top;
}

void PaintAll(RecordingPainter& g, const Rect& rc,
              const Palette& pal, const Metrics& mx)
{
    TagInputState tag;
    tag.tags = { L"alpha", L"beta" };
    PaintTagInput(g, rc, tag, pal, mx);

    RangeSliderState range;
    PaintRangeSlider(g, rc, range, pal, mx);

    OtpInputState otp;
    PaintOtpInput(g, rc, otp, pal, mx);

    CascaderState casc;
    casc.columns = {{L"L1", {L"a", L"b"}}, {L"L2", {L"c", L"d"}}};
    PaintCascader(g, rc, casc, pal, mx);

    AvatarGroupState av;
    av.items = {{L"A", 0}, {L"B", 0}, {L"C", 0}};
    PaintAvatarGroup(g, rc, av, pal, mx);

    HeatmapState heat;
    heat.weeks = 8;
    PaintHeatmap(g, rc, heat, pal, mx);

    CodeBlockState code;
    code.lines = { L"int main() {", L"    return 0;", L"}" };
    PaintCodeBlock(g, rc, code, pal, mx);

    KanbanState kanban;
    kanban.columns = {{L"todo", {{L"task", L"P1", AlertKind::Info}}}};
    PaintKanban(g, rc, kanban, pal, mx);

    GanttState gantt;
    gantt.tasks = {{L"one", 0, 2, 50, AlertKind::Info},
                   {L"two", 2, 3, 20, AlertKind::Success}};
    PaintGantt(g, rc, gantt, pal, mx);

    SnackbarState snack;
    PaintSnackbar(g, rc, snack, pal, mx);

    CommandPaletteState cmd;
    cmd.items = {{L"one", L"", L"", L"g", false},
                 {L"two", L"", L"", L"g", false}};
    PaintCommandPalette(g, rc, cmd, pal, mx);

    GuideTourState tour;
    PaintGuideTour(g, rc, Rect(rc.left + 40, rc.top + 20,
                              rc.left + 160, rc.top + 60), tour, pal, mx);
}

} // namespace

int main()
{
    const Palette pal = LightPalette();
    const Metrics mx{};

    // ---- empty rect must never draw ----
    {
        RecordingPainter g;
        PaintAll(g, Rect{}, pal, mx);
        Check(g.ops == 0, "empty rect produces no draw calls");
        Check(g.clipDepth == 0, "empty rect leaves clip balanced");
    }

    // ---- a full pass: clips balanced, content stays in bounds ----
    {
        const Rect rc(0, 0, 700, 220);
        RecordingPainter g;
        PaintAll(g, rc, pal, mx);
        Check(g.ops > 0, "all twelve controls draw something");
        Check(g.texts > 0, "controls render text");
        Check(g.alphaFills > 0, "alpha scrims / heatmap levels are used");
        Check(g.clipDepth == 0, "PushClip/PopClip balanced");
        Check(g.minClipDepth >= 0, "never pops more clips than pushed");
        for (const Rect& r : g.rects) {
            Check(Inside(rc, r), "every draw rect stays inside client");
        }
    }

    // ---- TagInput ----
    {
        const Rect rc(0, 0, 420, 48);
        TagInputState st;
        st.tags = { L"a", L"bb", L"ccc" };
        const TagInputLayout lay = MeasureTagInput(rc, st, mx);
        Check(lay.chips.size() == 3, "TagInput: one rect per tag");
        Check(!lay.input.Empty(), "TagInput: input area available");
        Check(!lay.clearAll.Empty(), "TagInput: clear-all button available");
        Check(Inside(rc, lay.clearAll), "TagInput: clear-all inside bounds");
        for (const TagChipRect& c : lay.chips) {
            Check(Inside(rc, c.chip), "TagInput: chip inside bounds");
            Check(Inside(c.chip, c.close), "TagInput: close button inside chip");
        }
        Check(NoOverlap(lay.chips[0].chip, lay.chips[1].chip),
              "TagInput: chips do not overlap");
        const TagInputHit hClose = HitTagInput(rc, st, mx,
                                               Center(lay.chips[1].close));
        Check(hClose.tag == 1 && hClose.close, "TagInput: close hit");
        const TagInputHit hChip = HitTagInput(rc, st, mx,
              Point{lay.chips[0].chip.left + 2, lay.chips[0].chip.top + 2});
        Check(hChip.tag == 0 && !hChip.close, "TagInput: chip hit");
        Check(HitTagInput(rc, st, mx, Center(lay.clearAll)).clearAll,
              "TagInput: clear-all hit");
        Check(HitTagInput(rc, st, mx, Point{900, 900}).tag == -1,
              "TagInput: outside is not a hit");
        st.tags.assign(20, L"long-tag");
        const TagInputLayout over = MeasureTagInput(rc, st, mx);
        Check(over.overflow, "TagInput: overflow flagged");
        for (const TagChipRect& c : over.chips) {
            Check(Inside(rc, c.chip), "TagInput: clipped chips stay inside");
        }
    }

    // ---- RangeSlider ----
    {
        const Rect rc(0, 0, 420, 60);
        RangeSliderState st;
        const RangeSliderLayout lay = MeasureRangeSlider(rc, st, mx);
        Check(Inside(rc, lay.track), "RangeSlider: track inside bounds");
        Check(Inside(rc, lay.fill), "RangeSlider: fill inside bounds");
        Check(Inside(rc, lay.lowThumb) && Inside(rc, lay.highThumb),
              "RangeSlider: thumbs inside bounds");
        Check(NoOverlap(lay.lowThumb, lay.highThumb),
              "RangeSlider: distinct low/high thumbs");
        Check(lay.fill.left >= lay.track.left && lay.fill.right <= lay.track.right,
              "RangeSlider: fill spans the selected interval");
        Check(RangeSliderThumbAt(rc, st, mx, Center(lay.lowThumb)) == 1,
              "RangeSlider: low thumb hit");
        Check(RangeSliderThumbAt(rc, st, mx, Center(lay.highThumb)) == 2,
              "RangeSlider: high thumb hit");
        Check(RangeSliderThumbAt(rc, st, mx, Point{900, 900}) == -2,
              "RangeSlider: outside is -2");
        Check(RangeSliderValueAt(rc, st, mx,
              Point{lay.track.left, lay.track.top}) == st.min,
              "RangeSlider: left edge maps to min");
        Check(RangeSliderValueAt(rc, st, mx,
              Point{lay.track.right - 1, lay.track.top}) == st.max,
              "RangeSlider: right edge maps to max");
        st.step = 5;
        const int v = RangeSliderValueAt(rc, st, mx,
              Point{lay.track.left + lay.track.Width() / 3, lay.track.top});
        Check(v % 5 == 0 && v >= st.min && v <= st.max,
              "RangeSlider: value snapped to step");
        st.low = 90; st.high = 10;
        const RangeSliderLayout swapped = MeasureRangeSlider(rc, st, mx);
        Check(swapped.lowThumb.left <= swapped.highThumb.left,
              "RangeSlider: reversed bounds normalized");
    }

    // ---- OtpInput ----
    {
        const Rect rc(0, 0, 420, 60);
        OtpInputState st;
        const OtpInputLayout lay = MeasureOtpInput(rc, st, mx);
        Check(lay.cells.size() == 6, "OtpInput: one cell per digit");
        for (const Rect& c : lay.cells) {
            Check(Inside(rc, c), "OtpInput: cell inside bounds");
        }
        Check(NoOverlap(lay.cells[0], lay.cells[1]), "OtpInput: cells do not overlap");
        Check(OtpCellAt(rc, st, mx, Center(lay.cells[2])) == 2,
              "OtpInput: cell hit");
        Check(OtpCellAt(rc, st, mx, Point{900, 900}) == -1,
              "OtpInput: outside is -1");
        st.length = 0;
        Check(MeasureOtpInput(rc, st, mx).cells.empty(),
              "OtpInput: zero length yields no cells");
    }

    // ---- Cascader ----
    {
        const Rect rc(0, 0, 600, 170);
        CascaderState st;
        st.columns = {{L"L1", {L"a", L"b", L"c"}},
                      {L"L2", {L"d", L"e", L"f"}},
                      {L"L3", {L"g", L"h", L"i"}}};
        st.selected = {0, -1, -1};
        const CascaderLayout lay = MeasureCascader(rc, st, mx);
        Check(lay.columnRects.size() == 3, "Cascader: one rect per column");
        for (const Rect& c : lay.columnRects) {
            Check(Inside(rc, c), "Cascader: column inside bounds");
        }
        const Rect& col1 = lay.columnRects[1];
        const Point p{col1.left + mx.unit * 2,
                      col1.top + lay.headerH + lay.rowH + lay.rowH / 2};
        const CascaderHit h = HitCascader(rc, st, mx, p);
        Check(h.column == 1 && h.row == 1, "Cascader: row hit");
        Check(HitCascader(rc, st, mx, Point{col1.left + 2, col1.top + 2}).row == -1,
              "Cascader: header is not a row");
        Check(HitCascader(rc, st, mx, Point{900, 900}).column == -1,
              "Cascader: outside is not a hit");
    }

    // ---- AvatarGroup ----
    {
        const Rect rc(0, 0, 420, 80);
        AvatarGroupState st;
        st.items = {{L"A", 0}, {L"B", 0}, {L"C", 0},
                    {L"D", 0}, {L"E", 0}, {L"F", 0}, {L"G", 0}};
        st.maxVisible = 4;
        const AvatarGroupLayout lay = MeasureAvatarGroup(rc, st, mx);
        Check(lay.cells.size() == 4, "AvatarGroup: maxVisible respected");
        Check(!lay.more.Empty(), "AvatarGroup: +N bubble present");
        Check(Inside(rc, lay.more), "AvatarGroup: bubble inside bounds");
        for (const Rect& c : lay.cells) {
            Check(Inside(rc, c), "AvatarGroup: avatar inside bounds");
        }
        Check(AvatarGroupAt(rc, st, mx, Center(lay.cells[3])) == 3,
              "AvatarGroup: avatar hit");
        Check(AvatarGroupAt(rc, st, mx, Center(lay.more)) == -2,
              "AvatarGroup: bubble hit");
        Check(AvatarGroupAt(rc, st, mx, Point{900, 900}) == -1,
              "AvatarGroup: outside is -1");
    }

    // ---- Heatmap ----
    {
        const Rect rc(0, 0, 600, 180);
        HeatmapState st;
        st.weeks = 10;
        st.values.assign(70, 2);
        const HeatmapLayout lay = MeasureHeatmap(rc, st, mx);
        Check(lay.cell > 0 && !lay.grid.Empty(), "Heatmap: grid measured");
        Check(Inside(rc, lay.grid), "Heatmap: grid inside bounds");
        Check(!lay.legend.Empty(), "Heatmap: legend present");
        const Point first{lay.grid.left + lay.cell / 2,
                          lay.grid.top + lay.cell / 2};
        const HeatmapCell c = HeatmapCellAt(rc, st, mx, first);
        Check(c.week == 0 && c.day == 0, "Heatmap: first cell hit");
        Check(HeatmapCellAt(rc, st, mx, Point{900, 900}).week == -1,
              "Heatmap: outside is -1");
        Check(HeatmapCellAt(rc, st, mx,
              Point{lay.grid.left + lay.cell + 1, lay.grid.top + lay.cell / 2}).week == -1,
              "Heatmap: gap is not a cell");
    }

    // ---- CodeBlock ----
    {
        const Rect rc(0, 0, 500, 180);
        CodeBlockState st;
        st.lines = { L"line 1", L"line 2", L"line 3", L"line 4" };
        st.highlightLine = 1;
        const CodeBlockLayout lay = MeasureCodeBlock(rc, st, mx);
        Check(lay.lineH > 0 && !lay.body.Empty(), "CodeBlock: body measured");
        Check(Inside(rc, lay.header) && Inside(rc, lay.body),
              "CodeBlock: header/body inside bounds");
        Check(Inside(lay.header, lay.copy), "CodeBlock: copy button inside header");
        Check(CodeBlockCopyHit(rc, st, mx, Center(lay.copy)),
              "CodeBlock: copy hit");
        Check(!CodeBlockCopyHit(rc, st, mx, Center(lay.body)),
              "CodeBlock: body is not the copy button");
    }

    // ---- KanbanBoard ----
    {
        const Rect rc(0, 0, 700, 200);
        KanbanState st;
        st.columns = {
            {L"todo", {{L"a", L"P1", AlertKind::Info},
                       {L"b", L"P2", AlertKind::Warning}}},
            {L"doing", {{L"c", L"P0", AlertKind::Error}}},
            {L"done", {{L"d", L"OK", AlertKind::Success}}},
        };
        const KanbanLayout lay = MeasureKanban(rc, st, mx);
        Check(lay.columnRects.size() == 3, "Kanban: one rect per column");
        Check(lay.cardRects.size() == 3, "Kanban: one card list per column");
        for (std::size_t c = 0; c < lay.cardRects.size(); ++c) {
            for (const Rect& card : lay.cardRects[c]) {
                Check(Inside(lay.columnRects[c], card),
                      "Kanban: card inside its column");
            }
        }
        const KanbanHit h = HitKanban(rc, st, mx,
                                     Center(lay.cardRects[1][0]));
        Check(h.column == 1 && h.card == 0, "Kanban: card hit");
        Check(HitKanban(rc, st, mx, Point{900, 900}).column == -1,
              "Kanban: outside is not a hit");
    }

    // ---- GanttChart ----
    {
        const Rect rc(0, 0, 800, 190);
        GanttState st;
        st.days = 14;
        st.tasks = {{L"a", 0, 2, 100, AlertKind::Info},
                    {L"b", 2, 5, 40, AlertKind::Success},
                    {L"c", 8, 3, 10, AlertKind::Warning},
                    {L"d", 11, 2, 0, AlertKind::Error}};
        const GanttLayout lay = MeasureGantt(rc, st, mx);
        Check(lay.bars.size() == 4, "Gantt: one bar per task");
        Check(lay.dayW > 0 && lay.rowH > 0, "Gantt: grid measured");
        for (const Rect& bar : lay.bars) {
            Check(Inside(rc, bar), "Gantt: bar inside bounds");
        }
        Check(lay.bars[1].Width() > lay.bars[0].Width(),
              "Gantt: longer task draws a wider bar");
        const Point p{lay.rows.left + lay.labelW + lay.dayW * 3,
                      lay.rows.top + lay.rowH + lay.rowH / 2};
        Check(GanttTaskAt(rc, st, mx, p) == 1, "Gantt: task row hit");
        Check(GanttTaskAt(rc, st, mx, Point{900, 900}) == -1,
              "Gantt: outside is -1");
    }

    // ---- Snackbar ----
    {
        const Rect client(0, 0, 600, 120);
        SnackbarState st;
        const SnackbarLayout lay = MeasureSnackbar(client, st, mx);
        Check(!lay.card.Empty() && Inside(client, lay.card),
              "Snackbar: card inside client");
        Check(Inside(lay.card, lay.textRc), "Snackbar: text inside card");
        Check(Inside(lay.card, lay.actionRc) && Inside(lay.card, lay.closeRc),
              "Snackbar: action/close inside card");
        Check(HitSnackbar(client, st, mx,
              Center(lay.actionRc)) == SnackbarHit::Action,
              "Snackbar: action hit");
        Check(HitSnackbar(client, st, mx,
              Center(lay.closeRc)) == SnackbarHit::Close,
              "Snackbar: close hit");
        Check(HitSnackbar(client, st, mx, Point{900, 900}) == SnackbarHit::None,
              "Snackbar: outside is None");
        st.closable = false;
        const SnackbarLayout noClose = MeasureSnackbar(client, st, mx);
        Check(noClose.closeRc.Empty(), "Snackbar: no close button when disabled");
        Check(Inside(noClose.card, noClose.actionRc),
              "Snackbar: action stays inside card without close button");
    }

    // ---- CommandPalette ----
    {
        const Rect client(0, 0, 600, 220);
        CommandPaletteState st;
        st.items = {
            {L"切换主题", L"外观", L"Ctrl+T", L"Theme", false},
            {L"主题编辑器", L"外观", L"", L"Theme", false},
            {L"打开设置", L"通用", L"Ctrl+,", L"General", false},
            {L"插件市场", L"扩展", L"", L"Plugin", true},
        };
        st.query = L"Theme";
        const std::vector<int> matches = CommandPaletteMatches(st);
        Check(matches.size() == 2, "CommandPalette: case-insensitive filter");
        Check(matches[0] == 0 && matches[1] == 1,
              "CommandPalette: returns item indices");
        st.query.clear();
        Check(CommandPaletteMatches(st).size() == 4,
              "CommandPalette: empty query matches all");
        st.query = L"zzz-not-found";
        Check(CommandPaletteMatches(st).empty(),
              "CommandPalette: no match returns empty");
        st.query = L"Theme";
        const CommandPaletteLayout lay = MeasureCommandPalette(client, st, mx);
        Check(Inside(client, lay.panel), "CommandPalette: panel inside client");
        Check(Inside(lay.panel, lay.search) && Inside(lay.panel, lay.footer),
              "CommandPalette: search/footer inside panel");
        Check(!lay.rows.empty(), "CommandPalette: rows measured");
        const CommandPaletteHit h = HitCommandPalette(client, st, mx,
                                                     Center(lay.rows[0]));
        Check(h.row == matches[0], "CommandPalette: row maps to item index");
        Check(HitCommandPalette(client, st, mx,
              Center(lay.search)).search, "CommandPalette: search hit");
        Check(HitCommandPalette(client, st, mx, Point{900, 900}).row == -1,
              "CommandPalette: outside is not a hit");
    }

    // ---- GuideTour ----
    {
        const Rect client(0, 0, 600, 220);
        const Rect anchor(60, 30, 220, 80);
        GuideTourState st;
        const GuideTourLayout lay = MeasureGuideTour(client, anchor, st, mx);
        Check(!lay.card.Empty() && Inside(client, lay.card),
              "GuideTour: card inside client");
        Check(Inside(lay.card, lay.titleRc) && Inside(lay.card, lay.bodyRc),
              "GuideTour: title/body inside card");
        Check(Inside(lay.card, lay.prevRc) && Inside(lay.card, lay.nextRc) &&
              Inside(lay.card, lay.skipRc),
              "GuideTour: buttons inside card");
        Check(HitGuideTour(client, anchor, st, mx,
              Center(lay.nextRc)) == GuideTourHit::Next,
              "GuideTour: next hit");
        Check(HitGuideTour(client, anchor, st, mx,
              Center(lay.prevRc)) == GuideTourHit::Prev,
              "GuideTour: prev hit");
        Check(HitGuideTour(client, anchor, st, mx,
              Center(lay.skipRc)) == GuideTourHit::Skip,
              "GuideTour: skip hit");
        Check(HitGuideTour(client, anchor, st, mx,
              Point{900, 900}) == GuideTourHit::None,
              "GuideTour: outside is None");
    }

    std::printf("test_advanced_controls: %d checks passed\n", g_checks);
    return 0;
}
