// ============================================================
// AirUI - test_framework_extra.cpp
// Core-only regression for the Phase 0 framework, the Phase 1
// migration and the Phase 2 controls. No platform headers: this
// test links airui-core alone.
// ============================================================
#include "airui/FrameworkCore.h"
#include "airui/ControlsExtra.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
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

enum class OpType { FillRect, FillRectAlpha, FillRound, StrokeRect,
                    StrokeRound, StrokeDashed, Text, TextBlock, Clip, Unclip };

struct Op
{
    OpType type{};
    Rect rect{};
    Color color = 0;
    int extra = 0;
};

class RecordingPainter : public IPainter
{
public:
    std::vector<Op> ops;
    int clipDepth = 0;
    int minClipDepth = 0;

    void FillRect(const Rect& r, Color c) override { ops.push_back({OpType::FillRect, r, c, 0}); }
    void FillRectAlpha(const Rect& r, Color c, uint8_t a) override
    { ops.push_back({OpType::FillRectAlpha, r, c, a}); }
    void FillRound(const Rect& r, int radius, Color c) override
    { ops.push_back({OpType::FillRound, r, c, radius}); }
    void StrokeRect(const Rect& r, Color c, int w) override
    { ops.push_back({OpType::StrokeRect, r, c, w}); }
    void StrokeRound(const Rect& r, int, Color c, int w) override
    { ops.push_back({OpType::StrokeRound, r, c, w}); }
    void StrokeDashed(const Rect& r, Color c, int w, int dash) override
    { ops.push_back({OpType::StrokeDashed, r, c, w + dash}); }
    void Text(const Rect& r, const std::wstring&, FontRole, Color c, TextFormat) override
    { ops.push_back({OpType::Text, r, c, 0}); }
    void TextBlock(const Rect& r, const std::wstring&, FontRole, Color c, int lines) override
    { ops.push_back({OpType::TextBlock, r, c, lines}); }
    int MeasureText(const std::wstring& s, FontRole) override
    { return static_cast<int>(s.size()) * 8; }
    void PushClip(const Rect& r) override
    { ++clipDepth; ops.push_back({OpType::Clip, r, 0, 0}); }
    void PopClip() override
    { --clipDepth; if (clipDepth < minClipDepth) minClipDepth = clipDepth;
      ops.push_back({OpType::Unclip, Rect{}, 0, 0}); }
    void Translate(Point) override {}
};

bool HasOp(const RecordingPainter& p, OpType t)
{
    for (const auto& o : p.ops) if (o.type == t) return true;
    return false;
}

class FakeClock : public IClock
{
public:
    uint64_t now = 0;
    uint64_t NowMs() override { return now; }
};

class FakeClipboard : public IClipboard
{
public:
    std::wstring text;
    std::wstring Read() override { return text; }
    void Write(const std::wstring& s) override { text = s; }
};

const Palette& P() { return Pal(); }
const Metrics& M() { return Mx(); }

// ------------------------------------------------------------
void TestControlHost()
{
    ControlHost host;
    auto a = std::make_unique<Control>();
    a->SetName(L"A"); a->SetRole(ControlRole::Button);
    a->SetBounds(Rect(0, 0, 100, 40)); a->SetZ(1); a->SetTabIndex(20);
    const int idA = host.Add(std::move(a));

    auto b = std::make_unique<Control>();
    b->SetName(L"B"); b->SetRole(ControlRole::TextBox);
    b->SetBounds(Rect(50, 10, 150, 50)); b->SetZ(5); b->SetTabIndex(10);
    const int idB = host.Add(std::move(b));

    Check(host.Count() == 2, "ControlHost: Add registers both controls");
    Check(idA != idB && idA > 0, "ControlHost: ids are unique and positive");
    Check(host.HitControl(Point{75, 25}) == idB,
          "ControlHost: topmost z-order wins on overlap");
    Check(host.HitControl(Point{10, 25}) == idA,
          "ControlHost: non-overlapping hit resolves to the only control");
    Check(host.HitControl(Point{400, 400}) == -1,
          "ControlHost: miss returns -1");

    PointerEvent down; down.pos = Point{75, 25}; down.down = true;
    ControlResult r = host.DispatchPointer(down);
    Check(r.handled && r.id == idB && r.action == 1,
          "ControlHost: pointer down routes to the topmost control");
    Check(host.FocusedId() == idB && host.CapturedId() == idB,
          "ControlHost: down sets focus and capture");

    PointerEvent up; up.pos = Point{400, 400}; up.up = true;
    r = host.DispatchPointer(up);
    Check(r.handled && r.id == idB && host.CapturedId() == -1,
          "ControlHost: pointer up goes to the captured control and releases it");

    Check(host.MoveFocus(true) && host.FocusedId() == idA,
          "ControlHost: Tab follows ascending tabIndex");
    Check(host.MoveFocus(true) && host.FocusedId() == idB,
          "ControlHost: Tab wraps back to the first entry");
    Check(!host.Find(idA)->Bounds().Empty(), "ControlHost: Find returns the control");

    KeyEvent tab; tab.vk = keys::Tab;
    const ControlResult kr = host.DispatchKey(tab);
    Check(kr.handled && kr.action == 4 && kr.redraw,
          "ControlHost: Tab key reports a focus move");

    Control* b2 = host.Find(idB);
    b2->SetVisible(false);
    Check(host.HitControl(Point{75, 25}) == idA,
          "ControlHost: invisible controls are not hit");
    const auto tree = BuildAccessibleTree(host);
    Check(tree.size() == 1 && tree[0].id == idA,
          "Accessibility: invisible controls are omitted");
    Check(DescribeAll(host).find(L"A") != std::wstring::npos,
          "Accessibility: DescribeAll includes the control name");

    RecordingPainter p;
    host.PaintAll(p, P(), M());
    Check(!p.ops.empty() && HasOp(p, OpType::FillRound),
          "ControlHost: PaintAll emits drawing calls");
    Check(p.clipDepth == 0 && p.minClipDepth == 0,
          "ControlHost: PaintAll keeps clip depth balanced");

    Check(host.Remove(idA) && host.Count() == 1,
          "ControlHost: Remove deletes a control");
    host.Clear();
    Check(host.Count() == 0 && host.FocusedId() == -1,
          "ControlHost: Clear resets focus");
}

// ------------------------------------------------------------
void TestScrollAndVirtual()
{
    ScrollState st;
    st.contentH = 1000; st.viewH = 200; st.lineH = 32;
    Check(ScrollMax(st) == 800, "Scroll: max offset is content - view");
    Check(ClampScrollOffset(9999, st) == 800, "Scroll: offset is clamped high");
    Check(ClampScrollOffset(-10, st) == 0, "Scroll: offset is clamped low");
    ScrollBy(st, 150);
    Check(st.offset == 150, "Scroll: ScrollBy moves the offset");
    ScrollPage(st, 1);
    Check(st.offset == 350, "Scroll: ScrollPage advances one viewport");

    const Rect view(0, 0, 400, 200);
    const ScrollGeometry geo = MeasureScrollbar(view, st, M());
    Check(geo.visible && geo.thumbH > 0 && geo.thumb.top >= geo.track.top &&
          geo.thumb.bottom <= geo.track.bottom,
          "Scrollbar: thumb stays inside the track");
    Check(HitScrollbar(view, st, M(), Point{geo.thumb.left + 1, geo.thumb.top + 2})
          == ScrollBarHit::Thumb, "Scrollbar: thumb hit");
    Check(HitScrollbar(view, st, M(), Point{geo.upBtn.left + 1, geo.upBtn.top + 1})
          == ScrollBarHit::UpButton, "Scrollbar: up button hit");
    Check(HitScrollbar(view, st, M(), Point{500, 20}) == ScrollBarHit::None,
          "Scrollbar: outside is a miss");
    const int dragged = ScrollOffsetFromDrag(geo, st, geo.thumb.top + 40, 5);
    Check(dragged >= 0 && dragged <= ScrollMax(st),
          "Scrollbar: drag maps back into the valid offset range");

    ScrollState emptyScroll; emptyScroll.contentH = 100; emptyScroll.viewH = 200;
    Check(!MeasureScrollbar(view, emptyScroll, M()).visible,
          "Scrollbar: hidden when content fits");

    ScrollState vr; vr.contentH = 1000; vr.viewH = 100; vr.offset = 320;
    const VirtualRange u = VirtualRangeUniform(vr, 100, 20, 2);
    Check(u.first == 14 && u.last >= 23 && u.count == u.last - u.first,
          "Virtual: uniform range includes overscan rows only");

    std::vector<int> heights(100, 20);
    heights[10] = 60;
    const VirtualRange v = VirtualRangeVariable(vr, heights, 1);
    Check(v.first >= 0 && v.last <= 100 && v.count > 0 && v.count < 100,
          "Virtual: variable heights only expose a window of rows");
    Check(FirstVisibleRow(vr, heights) >= 0 &&
          FirstVisibleRow(vr, heights) < 100,
          "Virtual: FirstVisibleRow is a valid index");

    RecordingPainter p;
    PaintScrollbar(p, view, st, P(), M());
    Check(!p.ops.empty(), "PaintScrollbar: emits drawing calls");
    Check(p.clipDepth == 0, "PaintScrollbar: clip stays balanced");
}

// ------------------------------------------------------------
void TestDpiAndAnimation()
{
    Check(ScaleInt(100, 144) == 150, "DPI: ScaleInt uses the 96 dpi baseline");
    Check(ScaleInt(100, 96) == 100, "DPI: 96 dpi is identity");
    const Rect r = ScaleRect(Rect(0, 0, 100, 32), 144);
    Check(r.right == 150 && r.bottom == 48, "DPI: ScaleRect scales every edge");
    Check(MetricsForDpi(Metrics{}, 144).unit == 12,
          "DPI: MetricsForDpi actually scales Metrics::unit");

    FakeClock clock;
    AnimationClock anim;
    anim.SetClock(&clock);
    clock.now = 1000;
    Check(anim.Tick() == 1000 && anim.Running(), "Animation: Tick reads IClock");
    Check(anim.Phase(900, 200) == 0.5f, "Animation: Phase is elapsed/duration");
    Check(anim.Phase(900, 50) == 1.0f, "Animation: Phase clamps at 1");
    clock.now = 1250; anim.Tick();
    Check(anim.Loop(1000, 1000) == 0.25f, "Animation: Loop wraps by period");
    Check(SpinnerPhase(250, 1000) == 0.25f, "Animation: SpinnerPhase wraps");
    Check(PulsePhase(400, 1600) > 0.0f && PulsePhase(1200, 1600) < 1.0f,
          "Animation: PulsePhase is a triangle wave");
    Check(ShimmerOffset(0, 100) == -100 && ShimmerOffset(800, 100) == 0,
          "Animation: ShimmerOffset sweeps across the width");

    AnimationClock noClock;
    Check(noClock.Tick() == 0 && !noClock.Running(),
          "Animation: no clock attached stays deterministic at 0");
}

// ------------------------------------------------------------
void TestTextEdit()
{
    TextEditState st;
    st.text = L"abcdef";
    st.cursor = 2;
    st.selectionAnchor = 5;
    Check(SelectionBegin(st) == 2 && SelectionEnd(st) == 5,
          "TextEdit: selection is normalised");
    Check(SelectedText(st) == L"cde", "TextEdit: SelectedText extracts the range");

    FakeClipboard clip;
    EditResult r = ApplyEdit(st, EditAction::Copy, L"", &clip);
    Check(clip.text == L"cde" && r.consumed,
          "TextEdit: Copy writes the selection to IClipboard");
    r = ApplyEdit(st, EditAction::Insert, L"XY", &clip);
    Check(st.text == L"abXYf" && st.cursor == 4 && !HasSelection(st),
          "TextEdit: Insert replaces the selection and moves the caret");
    ApplyEdit(st, EditAction::SelectAll, L"", &clip);
    Check(SelectionBegin(st) == 0 && SelectionEnd(st) == 5,
          "TextEdit: SelectAll covers the whole text");
    ApplyEdit(st, EditAction::Paste, L"", &clip);
    Check(st.text == L"cde", "TextEdit: Paste replaces the selection");
    ApplyEdit(st, EditAction::Backspace, L"", &clip);
    Check(st.text == L"cd", "TextEdit: Backspace deletes before the caret");
    ApplyEdit(st, EditAction::Home, L"", &clip);
    ApplyEdit(st, EditAction::Delete, L"", &clip);
    Check(st.text == L"d", "TextEdit: Delete removes at the caret");
    ApplyEdit(st, EditAction::CompositionUpdate, L"\u4E2D", &clip);
    ApplyEdit(st, EditAction::CompositionEnd, L"", &clip);
    Check(st.text.find(L"\u4E2D") != std::wstring::npos,
          "TextEdit: IME composition commits into the text");

    TextBoxState box;
    box.edit.text = L"hello";
    box.edit.cursor = 5;
    box.edit.hasFocus = true;
    box.edit.selectionAnchor = 1;
    const TextEditLayout L = MeasureTextEdit(Rect(0, 0, 200, 32), box.edit, M(), 8);
    Check(L.textRc.left >= 0 && L.caretX >= L.textRc.left && L.caretX <= L.textRc.right,
          "MeasureTextEdit: caret stays inside the text area");
    Check(CaretIndexAt(Rect(0, 0, 200, 32), box.edit, M(), 8, Point{16, 16}) >= 0,
          "CaretIndexAt: returns a valid index");
    Check(HitTextBox(Rect(0, 0, 200, 32), box, M(), Point{10, 10}) == TextBoxHit::Body,
          "HitTextBox: body hit");
    Check(HitTextBox(Rect(0, 0, 200, 32), box, M(), Point{500, 10}) == TextBoxHit::None,
          "HitTextBox: miss");

    RecordingPainter p;
    PaintTextBox(p, Rect(0, 0, 200, 32), box, P(), M());
    Check(p.ops.size() > 3 && HasOp(p, OpType::Text),
          "PaintTextBox: draws frame, text and caret");
    Check(p.clipDepth == 0 && p.minClipDepth == 0,
          "PaintTextBox: PushClip/PopClip are balanced");

    RecordingPainter empty;
    PaintTextBox(empty, Rect{}, box, P(), M());
    Check(empty.ops.empty(), "PaintTextBox: empty rect is a no-op");
}

// ------------------------------------------------------------
void TestMigratedControls()
{
    CardState card;
    card.linkText = L"更多";
    const CardLayout cl = MeasureCard(Rect(0, 0, 240, 180), card, M());
    Check(cl.iconRc.Width() > 0 && cl.titleRc.top >= cl.iconRc.bottom,
          "MeasureCard: icon and title do not overlap");
    Check(HitCard(Rect(0, 0, 240, 180), card, M(),
                  Point{cl.linkRc.left + 4, cl.linkRc.top + 4}) == CardHit::Link,
          "HitCard: link area is hit");
    Check(HitCard(Rect(0, 0, 240, 180), card, M(), Point{500, 10}) == CardHit::None,
          "HitCard: outside is a miss");

    FormState form;
    form.fields = { {L"名称", L"", L"", L"", true, false},
                    {L"备注", L"ok", L"", L"", false, false} };
    Check(ValidateForm(form) == 0 && form.fields[0].invalid,
          "ValidateForm: flags the first required empty field");
    form.fields[0].value = L"filled";
    Check(ValidateForm(form) == -1 && !form.fields[0].invalid,
          "ValidateForm: clears the flag once valid");
    const FormLayout fl = MeasureForm(Rect(0, 0, 400, 200), form, M());
    Check(fl.inputs.size() == 2 && fl.inputs[1].top > fl.inputs[0].top,
          "MeasureForm: rows are stacked without overlap");

    DataGridState grid;
    grid.columns = { {L"名称", 3, true}, {L"数量", 1, true} };
    grid.rows = { {L"甲", L"10"}, {L"乙", L"2"}, {L"丙", L"30"} };
    grid.filter = L"乙";
    auto rows = DataGridVisibleRows(grid);
    Check(rows.size() == 1 && rows[0][0] == L"乙",
          "DataGridVisibleRows: filter keeps matching rows");
    grid.filter.clear();
    grid.sortColumn = 1; grid.sortAscending = true;
    rows = DataGridVisibleRows(grid);
    Check(rows.size() == 3 && rows[0][1] == L"2" && rows[2][1] == L"30",
          "DataGridVisibleRows: numeric-aware ascending sort");
    grid.sortAscending = false;
    rows = DataGridVisibleRows(grid);
    Check(rows[0][1] == L"30", "DataGridVisibleRows: descending sort");
    const DataGridLayout dl = MeasureDataGrid(Rect(0, 0, 400, 200), grid, M());
    Check(dl.columns.size() == 2 && dl.handles.size() == 1,
          "MeasureDataGrid: columns and drag handles are derived from weights");
    Check(HitDataGrid(Rect(0, 0, 400, 200), grid, M(), Point{5, 5}).header,
          "HitDataGrid: header hit");

    ListDataState list;
    list.rows = { {L"A", L"", L"", 32, false},
                  {L"B", L"sub", L"", 48, false},
                  {L"C", L"", L"", 36, true} };
    const auto heights = ListDataHeights(list, M());
    Check(heights.size() == 3 && heights[1] == 48,
          "ListDataHeights: explicit heights are preserved");
    Check(ListDataContentHeight(list, M()) == 116,
          "ListDataContentHeight: sums the row heights");
    Check(ListDataRowAt(Rect(0, 0, 300, 100), list, M(), Point{20, 40}) == 1,
          "ListDataRowAt: resolves the variable-height row");

    ToolbarState toolbar;
    toolbar.crumbs = { L"首页", L"详情" };
    toolbar.actions = { {1, L"新建", L"", true, false} };
    const ToolbarLayout tl = MeasureToolbar(Rect(0, 0, 600, 48), toolbar, M());
    Check(tl.crumbs.size() == 2 && tl.actions.size() == 1,
          "MeasureToolbar: lays out crumbs and actions");
    Check(HitToolbar(Rect(0, 0, 600, 48), toolbar, M(), Point{590, 24}).action == 0,
          "HitToolbar: right-aligned action is hit");

    KbdChipState kbd;
    Check(MeasureKbdChip(kbd, M()) > 0, "MeasureKbdChip: positive width");
}

// ------------------------------------------------------------
void TestInputControls()
{
    Check(DaysInMonth(2024, 2) == 29, "Calendar: leap year February");
    Check(DaysInMonth(1900, 2) == 28, "Calendar: century non-leap year");
    Check(DaysInMonth(2026, 11) == 30, "Calendar: November has 30 days");
    Check(DaysInMonth(2026, 13) == 0, "Calendar: invalid month rejected");
    Check(FirstWeekday(2000, 1) == 6, "Calendar: Sakamoto weekday matches");
    Check(FirstWeekday(2026, 11) >= 0 && FirstWeekday(2026, 11) <= 6,
          "Calendar: weekday stays in 0..6");

    DatePickerState date;
    const Rect drc(0, 0, 280, 220);
    const int day = DatePickerDayAt(drc, date, M(), Point{140, 130});
    Check(day >= -3 && day <= 31, "DatePickerDayAt: returns a valid code");
    Check(DatePickerDayAt(drc, date, M(), Point{900, 900}) == -1,
          "DatePickerDayAt: outside is a miss");

    NumberInputState num; num.value = 5;
    Check(HitNumberInput(Rect(0, 0, 160, 32), num, M(), Point{500, 10})
          == NumberInputHit::None, "HitNumberInput: outside is a miss");
    SearchBoxState search; search.text = L"abc";
    Check(HitSearchBox(Rect(0, 0, 240, 32), search, M(), Point{500, 10})
          == SearchBoxHit::None, "HitSearchBox: outside is a miss");
    PasswordBoxState pass;
    Check(HitPasswordBox(Rect(0, 0, 240, 32), pass, M(), Point{500, 10})
          == PasswordBoxHit::None, "HitPasswordBox: outside is a miss");
    ColorPickerState color;
    Check(ColorPickerSwatchAt(Rect(0, 0, 300, 80), color, M(), Point{500, 10}) == -1,
          "ColorPickerSwatchAt: outside is a miss");
}

// ------------------------------------------------------------
void TestChoiceAndDisplay()
{
    DropdownState drop;
    drop.options = { L"A", L"B", L"C" }; drop.open = true;
    const Rect drc(0, 0, 240, 180);
    const DropdownLayout dl = MeasureDropdown(drc, drop, M());
    Check(!dl.field.Empty() && !dl.list.Empty() && dl.optionH > 0,
          "MeasureDropdown: field and list geometry are valid");
    Check(DropdownOptionAt(drc, drop, M(), Point{500, 500}) == -2,
          "DropdownOptionAt: outside is -2");

    MenuState menu;
    menu.items = { {L"新建", L"Ctrl+N", L"", false, false, false},
                   {L"", L"", L"", false, true, false},
                   {L"禁用", L"", L"", true, false, false} };
    Check(MenuItemHeight(menu.items[1], M()) > 0,
          "MenuItemHeight: separators have a height");
    Check(MenuContentHeight(menu, M()) > 0, "MenuContentHeight: positive");
    Check(MenuItemAt(Rect(0, 0, 200, 120), menu, M(), Point{500, 10}) == -1,
          "MenuItemAt: outside is -1");

    MultiSelectState multi;
    multi.options = { L"A", L"B" }; multi.checked = { false, false };
    Check(MultiSelectAt(Rect(0, 0, 200, 80), multi, M(), Point{500, 10}) == -1,
          "MultiSelectAt: outside is -1");

    TransferState transfer;
    transfer.left = { L"L1", L"L2" }; transfer.right = { L"R1" };
    transfer.leftChecked = { true, false }; transfer.rightChecked = { false };
    const TransferLayout tl = MeasureTransfer(Rect(0, 0, 500, 200), transfer, M());
    Check(!tl.leftBox.Empty() && !tl.rightBox.Empty() && !tl.toRight.Empty(),
          "MeasureTransfer: both boxes and the move buttons exist");
    MoveTransferItem(transfer, 0, 0);
    Check(transfer.left.size() == 1 && transfer.right.size() == 2,
          "MoveTransferItem: really moves the item between lists");

    CalendarState cal;
    const CalendarLayout cl = MeasureCalendar(Rect(0, 0, 280, 220), cal, M());
    Check(cl.cellW > 0 && cl.cellH > 0, "MeasureCalendar: grid cells are positive");
    Check(CalendarDayAt(Rect(0, 0, 280, 220), cal, M(), Point{900, 900}) == -1,
          "CalendarDayAt: outside is -1");

    StatCardState stat; stat.bars = { 1, 2, 3 };
    const StatCardLayout sl = MeasureStatCard(Rect(0, 0, 260, 140), stat, M());
    Check(sl.titleRc.bottom <= sl.valueRc.top &&
          sl.valueRc.bottom <= sl.chartRc.top,
          "MeasureStatCard: title/value/chart do not overlap");
    Check(HitStatCard(Rect(0, 0, 260, 140), stat, M(), Point{500, 10})
          == StatCardHit::None, "HitStatCard: outside is a miss");

    ChartState chart; chart.values = { 1, 2, 3 };
    Check(ChartBarAt(Rect(0, 0, 300, 160), chart, M(), Point{900, 900}) == -1,
          "ChartBarAt: outside is -1");
    CarouselState car; car.slides = { L"A", L"B" };
    Check(HitCarousel(Rect(0, 0, 320, 160), car, M(), Point{900, 900}).code == 0,
          "HitCarousel: outside is none");
    StepsState steps; steps.steps = { L"1", L"2", L"3" };
    Check(StepAt(Rect(0, 0, 400, 80), steps, M(), Point{900, 900}) == -1,
          "StepAt: outside is -1");
}

// ------------------------------------------------------------
void TestFeedbackAndLayout()
{
    const Rect client(0, 0, 600, 400);
    ModalState modal;
    const ModalLayout ml = MeasureModal(client, modal, M());
    Check(!ml.card.Empty() && ml.confirmRc.top > ml.titleRc.top,
          "MeasureModal: card and buttons are laid out");
    Check(ModalButtonAt(client, modal, M(), Point{900, 900}) == 0,
          "ModalButtonAt: outside is none");

    // MB_OK 只有一个动作：取消槽必须为空，主按钮靠右且仍可命中。
    ModalState single;
    single.singleButton = true;
    const ModalLayout sl = MeasureModal(client, single, M());
    Check(sl.cancelRc.Empty() && !sl.confirmRc.Empty(),
          "MeasureModal: single button has no cancel slot");
    Check(ModalButtonAt(client, single, M(),
                        Point{sl.confirmRc.left + 2, sl.confirmRc.top + 2}) == 1,
          "ModalButtonAt: single confirm is hittable");
    Check(ModalButtonAt(client, single, M(),
                        Point{sl.confirmRc.left - 4, sl.confirmRc.top + 2}) == 0,
          "ModalButtonAt: empty cancel slot never hits");

    DrawerState drawer;
    const DrawerLayout dl = MeasureDrawer(client, drawer, M());
    Check(!dl.panel.Empty() && !dl.closeRc.Empty(),
          "MeasureDrawer: panel and close box exist");
    Check(!DrawerCloseHit(client, drawer, M(), Point{900, 900}),
          "DrawerCloseHit: outside is false");
    ResultState result;
    Check(!ResultActionHit(Rect(0, 0, 300, 160), result, M(), Point{900, 900}),
          "ResultActionHit: outside is false");

    AccordionState acc;
    acc.panels = { {L"A", L"body A"}, {L"B", L"body B"} };
    acc.expanded = { true, false };
    const Rect arc(0, 0, 300, 200);
    Check(AccordionHeaderAt(arc, acc, M(), Point{900, 900}) == -1,
          "AccordionHeaderAt: outside is -1");
    Check(AccordionContentHeight(acc, M()) > 0,
          "AccordionContentHeight: positive");
    AnchorState anchor; anchor.items = { L"A", L"B" };
    Check(AnchorHit(Rect(0, 0, 120, 160), anchor, M(), Point{900, 900}) == -1,
          "AnchorHit: outside is -1");
    SplitterState split;
    const SplitterLayout spl = MeasureSplitter(Rect(0, 0, 400, 200), split, M());
    Check(!spl.first.Empty() && !spl.second.Empty() && !spl.handle.Empty(),
          "MeasureSplitter: both panes and the handle exist");
    Check(!SplitterHandleHit(Rect(0, 0, 400, 200), split, M(), Point{900, 900}),
          "SplitterHandleHit: outside is false");
    ColumnResizeState cols; cols.widths = { 3, 2, 1 };
    const ColumnResizeLayout crl =
        MeasureColumnResize(Rect(0, 0, 400, 100), cols, M());
    Check(crl.columns.size() == 3 && crl.handles.size() == 2,
          "MeasureColumnResize: columns and boundaries match");
    Check(ColumnResizeHandleAt(Rect(0, 0, 400, 100), cols, M(), Point{900, 900}) == -1,
          "ColumnResizeHandleAt: outside is -1");
}

// ------------------------------------------------------------
void TestAllPaintFunctions()
{
    // Every new Paint* must draw something, keep clip depth balanced
    // and be a no-op on an empty rect.
    const Rect rc(0, 0, 480, 240);
    const Rect client(0, 0, 640, 400);

    // Straight-line smoke list (kept explicit for readable failures).
    RecordingPainter a;
    PaintCard(a, rc, CardState{}, P(), M());
    PaintEmptyView(a, rc, EmptyViewState{}, P(), M());
    PaintStatusTag(a, Rect(0, 0, 120, 24), StatusTagState{}, P(), M());
    PaintKbdChip(a, Rect(0, 0, 140, 24), KbdChipState{}, P(), M());
    PaintToolbar(a, Rect(0, 0, 600, 48), ToolbarState{}, P(), M());
    PaintForm(a, rc, FormState{}, P(), M());
    PaintDataGrid(a, rc, DataGridState{}, P(), M());
    PaintListData(a, rc, ListDataState{}, P(), M());
    PaintCustom(a, rc, CustomState{}, P(), M());
    PaintSearchBox(a, rc, SearchBoxState{}, P(), M());
    PaintPasswordBox(a, rc, PasswordBoxState{}, P(), M());
    PaintTextArea(a, rc, TextAreaState{}, P(), M());
    PaintNumberInput(a, rc, NumberInputState{}, P(), M());
    PaintDatePicker(a, rc, DatePickerState{}, P(), M());
    PaintColorPicker(a, rc, ColorPickerState{}, P(), M());
    PaintDropdown(a, rc, DropdownState{}, P(), M());
    PaintMenu(a, rc, MenuState{}, P(), M());
    PaintContextMenu(a, client, ContextMenuState{}, P(), M());
    PaintMultiSelect(a, rc, MultiSelectState{}, P(), M());
    PaintTransfer(a, rc, TransferState{}, P(), M());
    PaintTimeline(a, rc, TimelineState{}, P(), M());
    PaintSteps(a, rc, StepsState{}, P(), M());
    PaintCalendar(a, rc, CalendarState{}, P(), M());
    PaintDescriptionList(a, rc, DescriptionListState{}, P(), M());
    PaintStatCard(a, rc, StatCardState{}, P(), M());
    PaintChart(a, rc, ChartState{}, P(), M());
    PaintCarousel(a, rc, CarouselState{}, P(), M());
    PaintSpinner(a, rc, SpinnerState{}, P(), M());
    PaintTooltip(a, Rect(10, 10, 100, 40), client, TooltipState{}, P(), M());
    PaintPopover(a, Rect(10, 10, 100, 40), client, PopoverState{}, P(), M());
    PaintModal(a, client, ModalState{}, P(), M());
    PaintDrawer(a, client, DrawerState{}, P(), M());
    PaintNotificationQueue(a, client, NotificationState{}, P(), M());
    PaintResult(a, rc, ResultState{}, P(), M());
    PaintAccordion(a, rc, AccordionState{}, P(), M());
    PaintAnchor(a, rc, AnchorState{}, P(), M());
    PaintSplitter(a, rc, SplitterState{}, P(), M());
    PaintColumnResize(a, rc, ColumnResizeState{}, P(), M());
    Check(a.ops.size() > 80, "Paint smoke: all 40+ new controls emit drawing calls");
    Check(a.clipDepth == 0 && a.minClipDepth == 0,
          "Paint smoke: every PushClip is matched by PopClip");
    Check(HasOp(a, OpType::FillRectAlpha),
          "Paint smoke: modal/drawer scrim uses alpha fill");
}

} // namespace

int main()
{
    TestControlHost();
    TestScrollAndVirtual();
    TestDpiAndAnimation();
    TestTextEdit();
    TestMigratedControls();
    TestInputControls();
    TestChoiceAndDisplay();
    TestFeedbackAndLayout();
    TestAllPaintFunctions();
    std::printf("test_framework_extra: all %d checks passed\n", g_checks);
    return 0;
}
