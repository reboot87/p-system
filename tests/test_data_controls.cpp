// ============================================================
// AirUI - test_data_controls.cpp (core-only, no platform headers)
// Covers Table / List / Tree / Breadcrumb / Pagination / Toast:
// painting calls, geometry helpers and pointer hit testing.
// ============================================================
#include "airui/ControlsCore.h"
#include "airui/Theme.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace air::ui;

namespace {

enum class OpType { FillRect, FillRound, StrokeRect, StrokeRound, Text };

struct Op {
    OpType type = OpType::FillRect;
    Rect rect{};
    Color color{};
    std::wstring text;
    FontRole role = FontRole::Body;
};

class RecordingPainter : public IPainter {
public:
    std::vector<Op> ops;
    int clipDepth = 0;
    int maxClipDepth = 0;

    void FillRect(const Rect& r, Color c) override {
        ops.push_back({OpType::FillRect, r, c, L"", FontRole::Body});
    }
    void FillRound(const Rect& r, int, Color c) override {
        ops.push_back({OpType::FillRound, r, c, L"", FontRole::Body});
    }
    void StrokeRect(const Rect& r, Color c, int) override {
        ops.push_back({OpType::StrokeRect, r, c, L"", FontRole::Body});
    }
    void StrokeRound(const Rect& r, int, Color c, int) override {
        ops.push_back({OpType::StrokeRound, r, c, L"", FontRole::Body});
    }
    void StrokeDashed(const Rect&, Color, int, int) override {}
    void Text(const Rect& r, const std::wstring& s, FontRole role, Color c, TextFormat) override {
        ops.push_back({OpType::Text, r, c, s, role});
    }
    void TextBlock(const Rect&, const std::wstring&, FontRole, Color, int) override {}
    int MeasureText(const std::wstring&, FontRole) override { return 40; }
    void PushClip(const Rect&) override {
        ++clipDepth;
        if (clipDepth > maxClipDepth) maxClipDepth = clipDepth;
    }
    void PopClip() override { --clipDepth; }
    void Translate(Point) override {}
};

void Check(bool ok, const char* msg) {
    if (!ok) { std::printf("FAIL: %s\n", msg); std::exit(1); }
    std::printf("PASS: %s\n", msg);
}

bool HasText(const RecordingPainter& p, const std::wstring& s) {
    return std::any_of(p.ops.begin(), p.ops.end(), [&](const Op& o) {
        return o.type == OpType::Text && o.text.find(s) != std::wstring::npos;
    });
}

bool HasFill(const RecordingPainter& p, OpType type, Color c) {
    return std::any_of(p.ops.begin(), p.ops.end(), [&](const Op& o) {
        return o.type == type && o.color == c;
    });
}

TableState SampleTable() {
    TableState st;
    st.title = L"Records";
    st.columns = { L"Name", L"Type", L"State" };
    st.widths  = { 3, 1, 1 };
    st.rows = {
        { L"alpha", L"doc", L"ok" },
        { L"beta",  L"img", L"ok" },
        { L"gamma", L"txt", L"err" },
    };
    st.selectedRow = 1;
    st.sortColumn = 0;
    return st;
}

} // namespace

int main()
{
    const Palette& pal = Pal();
    const Metrics& mx = Mx();
    const Rect rc{10, 20, 410, 220};

    // ---------------- Table geometry ----------------
    {
        TableState st = SampleTable();
        const TableLayout lay = MeasureTable(rc, st, mx);
        Check(lay.columns.size() == 3, "Table: 3 measured columns");
        Check(lay.columns.front().left == rc.left + mx.unit,
              "Table: first column starts after padding");
        Check(lay.columns.back().right == rc.right - mx.unit,
              "Table: last column ends before right padding (no spill)");
        Check(lay.columns[0].Width() > lay.columns[1].Width(),
              "Table: weighted column is wider than 1x column");
        Check(lay.visibleRows >= 3 && lay.visibleRows * lay.rowH <= rc.bottom - lay.rowTop,
              "Table: only fully visible rows are counted");

        Check(TableRowAt(rc, st, mx, Point{rc.left + 20, lay.headerTop + 4}) == -1,
              "Table: header hit reports -1");
        Check(TableRowAt(rc, st, mx, Point{rc.left + 20, lay.rowTop + 4}) == 0,
              "Table: first row hit reports 0");
        Check(TableRowAt(rc, st, mx, Point{rc.left + 20, lay.rowTop + lay.rowH + 4}) == 1,
              "Table: second row hit reports 1");
        Check(TableRowAt(rc, st, mx, Point{rc.left + 20, rc.top + 1}) == -2,
              "Table: title area is outside the table body");
        Check(TableRowAt(rc, st, mx, Point{rc.left + 20, rc.bottom + 5}) == -2,
              "Table: below the table is outside");
        Check(TableColumnAt(rc, st, mx, Point{lay.columns[1].left + 2, lay.rowTop + 4}) == 1,
              "Table: column hit reports 1");
        Check(TableColumnAt(rc, st, mx, Point{rc.left + 1, lay.rowTop + 4}) == -1,
              "Table: outside column band reports -1");
    }

    // ---------------- Table painting ----------------
    {
        TableState st = SampleTable();
        RecordingPainter p;
        PaintTable(p, rc, st, pal, mx);
        Check(p.clipDepth == 0 && p.maxClipDepth == 1,
              "Table: clip is pushed and popped exactly once");
        Check(HasText(p, L"Records"), "Table: title is painted");
        Check(HasText(p, L"Name") && HasText(p, L"\u25B2"),
              "Table: header text and ascending sort arrow are painted");
        Check(HasText(p, L"alpha") && HasText(p, L"gamma"),
              "Table: first and last row values are painted");
        Check(HasFill(p, OpType::FillRect, pal.selectFill),
              "Table: selected row uses selectFill");
        Check(HasFill(p, OpType::FillRect, pal.primary),
              "Table: selected row has the accent bar");

        RecordingPainter empty;
        TableState noRows;
        noRows.columns = { L"A" };
        PaintTable(empty, rc, noRows, pal, mx);
        Check(HasText(empty, L"暂无数据"), "Table: empty state is painted");

        RecordingPainter noCols;
        TableState st2;
        PaintTable(noCols, rc, st2, pal, mx);
        Check(noCols.ops.empty(), "Table: no columns draws nothing");
    }

    // ---------------- List ----------------
    {
        ListState st;
        st.items = {
            { L"Clipboard", L"auto capture", L"On", L"\xE8C8", false },
            { L"Files",     L"",             L"",   L"",       false },
            { L"Printer",   L"disabled",     L"Off", L"",      true },
        };
        st.selected = 1;
        Check(ListRowHeight(st, mx) == mx.itemH,
              "List: subtitles switch to the tall row height");

        const Rect lrc{0, 0, 300, 260};
        Check(ListRowAt(lrc, st, mx, Point{20, 5}) == 0, "List: first row hit");
        Check(ListRowAt(lrc, st, mx, Point{20, ListRowHeight(st, mx) + 5}) == 1,
              "List: second row hit");
        Check(ListRowAt(lrc, st, mx, Point{20, 259}) == -1,
              "List: partially visible trailing row is not hit");
        Check(ListRowAt(lrc, st, mx, Point{400, 5}) == -1, "List: outside hit");

        RecordingPainter p;
        PaintList(p, lrc, st, pal, mx);
        Check(p.clipDepth == 0 && p.maxClipDepth == 1, "List: clip balanced");
        Check(HasText(p, L"Clipboard") && HasText(p, L"auto capture"),
              "List: title and subtitle are painted");
        Check(HasText(p, L"On"), "List: badge is painted");
        Check(HasFill(p, OpType::FillRect, pal.selectFill),
              "List: selected row uses selectFill");

        RecordingPainter empty;
        ListState st2;
        PaintList(empty, lrc, st2, pal, mx);
        Check(HasText(empty, L"暂无数据"), "List: empty state is painted");
    }

    // ---------------- Tree ----------------
    {
        TreeState st;
        st.nodes = {
            { L"root",  0, true,  true,  false },
            { L"child", 1, true,  true,  false },
            { L"leaf",  2, false, false, false },
            { L"other", 1, true,  false, false },
            { L"hidden",2, false, false, false },
            { L"tail",  0, false, false, false },
        };
        std::vector<int> visible = TreeVisibleNodes(st);
        Check(visible.size() == 5, "Tree: collapsed branch hides its children");
        Check(std::find(visible.begin(), visible.end(), 4) == visible.end(),
              "Tree: node under collapsed parent is not visible");

        st.nodes[3].expanded = true;
        visible = TreeVisibleNodes(st);
        Check(visible.size() == 6, "Tree: expanding a branch reveals its child");

        const Rect trc{0, 0, 260, 200};
        Check(TreeRowAt(trc, st, mx, Point{30, 5}) == 0, "Tree: first row is root");
        Check(TreeRowAt(trc, st, mx, Point{30, TreeRowHeight(mx) + 5}) == 1,
              "Tree: second row maps to the node index");
        Check(TreeRowAt(trc, st, mx, Point{30, 199}) == -1,
              "Tree: partially visible row is not hit");
        Check(TreeToggleAt(trc, st, mx, Point{mx.unit + 2, 5}) == 0,
              "Tree: disclosure arrow hit reports the node index");
        Check(TreeToggleAt(trc, st, mx, Point{100, 5}) == -1,
              "Tree: row body is not a toggle hit");

        RecordingPainter p;
        PaintTree(p, trc, st, pal, mx);
        Check(p.clipDepth == 0 && p.maxClipDepth == 1, "Tree: clip balanced");
        Check(HasText(p, L"root") && HasText(p, L"\u25BE"),
              "Tree: node text and expanded arrow are painted");
        Check(!HasText(p, L"missing"), "Tree: no stray text");
    }

    // ---------------- Breadcrumb ----------------
    {
        BreadcrumbState st;
        st.items = { L"Home", L"Data", L"Detail" };
        const Rect brc{0, 0, 308, 32};
        Check(BreadcrumbHit(brc, st, mx, Point{mx.unit + 2, 10}) == 0,
              "Breadcrumb: first cell hit");
        Check(BreadcrumbHit(brc, st, mx, Point{brc.right - mx.unit - 2, 10}) == 2,
              "Breadcrumb: last cell hit");
        Check(BreadcrumbHit(brc, st, mx, Point{2, 10}) == -1,
              "Breadcrumb: left padding is not a hit");

        RecordingPainter p;
        PaintBreadcrumb(p, brc, st, pal, mx);
        Check(HasText(p, L"Home") && HasText(p, L"Detail"),
              "Breadcrumb: all items are painted");
        Check(HasText(p, L"\u203A"), "Breadcrumb: separators are painted");
    }

    // ---------------- Pagination ----------------
    {
        PaginationState st;
        st.page = 3;
        st.pageCount = 12;
        const Rect prc{0, 0, 600, 40};
        const std::vector<PaginationSlot> slots = MeasurePagination(prc, st, mx);
        // page 3 / 12 -> window 1..7, tail ellipsis + last page,
        // plus prev/next: 1 + 7 + 2 + 1 = 11 slots.
        Check(slots.size() == 11,
              "Pagination: prev + 7 pages + tail gap + next");
        Check(slots.front().page == -2 && slots.back().page == -1,
              "Pagination: first/last slots are prev/next");

        const PaginationSlot* target = nullptr;
        for (const auto& s : slots) if (s.page == 5) target = &s;
        Check(target != nullptr, "Pagination: page 5 is in the window");
        if (target) {
            Check(PaginationHit(prc, st, mx, Point{target->rect.left + 4, target->rect.top + 4}) == 5,
                  "Pagination: page slot hit returns its page number");
        }

        const PaginationSlot& prev = slots.front();
        Check(PaginationHit(prc, st, mx, Point{prev.rect.left + 4, prev.rect.top + 4}) == -2,
              "Pagination: prev hit when not on first page");

        PaginationState first;
        first.page = 1;
        first.pageCount = 12;
        const auto firstSlots = MeasurePagination(prc, first, mx);
        Check(PaginationHit(prc, first, mx,
                            Point{firstSlots.front().rect.left + 4,
                                  firstSlots.front().rect.top + 4}) == 0,
              "Pagination: prev is disabled on the first page");
        Check(PaginationHit(prc, st, mx, Point{prc.right - 1, prc.top + 1}) == 0,
              "Pagination: empty area is not a hit");

        RecordingPainter p;
        PaintPagination(p, prc, st, pal, mx);
        Check(HasFill(p, OpType::FillRound, pal.primary),
              "Pagination: current page is filled with primary");
        Check(HasText(p, L"3") && HasText(p, L"\u2026"),
              "Pagination: page number and ellipsis are painted");
    }

    // ---------------- Toast ----------------
    {
        ToastState st;
        st.kind = AlertKind::Success;
        st.title = L"Saved";
        st.message = L"Record written";
        const Rect trc{0, 0, 320, 56};
        Check(ToastCloseHit(trc, st, mx, Point{trc.right - mx.unit - 4, trc.top + 28}),
              "Toast: close box hit");
        Check(!ToastCloseHit(trc, st, mx, Point{50, 28}),
              "Toast: body is not a close hit");
        st.closable = false;
        Check(!ToastCloseHit(trc, st, mx, Point{trc.right - mx.unit - 4, trc.top + 28}),
              "Toast: no close hit when not closable");
        st.closable = true;

        RecordingPainter p;
        PaintToast(p, trc, st, pal, mx);
        Check(HasText(p, L"Saved") && HasText(p, L"Record written"),
              "Toast: title and message are painted");
        Check(HasText(p, L"\u2713"), "Toast: success glyph is painted");
        Check(HasText(p, L"\u00D7"), "Toast: close glyph is painted");
        Check(HasFill(p, OpType::FillRect, pal.ok),
              "Toast: success accent bar uses ok color");
    }

    std::printf("PASS\n");
    return 0;
}
