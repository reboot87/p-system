// ============================================================
// AirUI - test_control_classes.cpp
// Core-only regression for the object-oriented control layer.
// Verifies every generated widget class, the source-compatible
// free-function kernel, and ControlHost polymorphism.
// ============================================================
#include "airui/WidgetControls.h"

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

class RecordingPainter : public IPainter
{
public:
    int ops = 0;
    int texts = 0;
    int clipDepth = 0;
    int minClipDepth = 0;
    std::vector<Rect> rects;

    void Reset()
    {
        ops = 0; texts = 0; clipDepth = 0; minClipDepth = 0;
        rects.clear();
    }
    void Note(const Rect& r)
    {
        ++ops;
        if (!r.Empty()) rects.push_back(r);
    }
    void FillRect(const Rect& r, Color) override { Note(r); }
    void FillRectAlpha(const Rect& r, Color, uint8_t) override { Note(r); }
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
    void PushClip(const Rect& r) override { ++clipDepth; Note(r); }
    void PopClip() override
    { --clipDepth; if (clipDepth < minClipDepth) minClipDepth = clipDepth; }
    void Translate(Point) override {}
};

const Rect kRc(10, 10, 410, 210);

bool Inside(const Rect& outer, const Rect& inner)
{
    return !inner.Empty() && inner.left >= outer.left &&
           inner.top >= outer.top && inner.right <= outer.right &&
           inner.bottom <= outer.bottom;
}

void CheckInsideAll(const std::vector<Rect>& rects, const Rect& rc,
                    const char* tag)
{
    for (std::size_t i = 0; i < rects.size(); ++i)
        Check(Inside(rc, rects[i]), tag);
}

template <class W, class Fn>
void Parity(Fn fn, const char* tag)
{
    W w;
    w.SetBounds(kRc);
    RecordingPainter a, b;
    a.Reset();
    w.Paint(a, LightPalette(), Metrics{});
    b.Reset();
    fn(b, kRc, w.state, LightPalette(), Metrics{});
    Check(a.ops == b.ops && a.texts == b.texts, tag);
    Check(a.clipDepth == 0 && b.clipDepth == 0, tag);
    CheckInsideAll(a.rects, kRc, tag);
    CheckInsideAll(b.rects, kRc, tag);
}

struct Config
{
    const char* tag;
    Rect rc;
    Palette pal;
    Metrics mx;
};

} // namespace

int main()
{
    std::vector<std::unique_ptr<Control>> all;
    all.push_back(std::make_unique<widgets::Button>());
    all.push_back(std::make_unique<widgets::Checkbox>());
    all.push_back(std::make_unique<widgets::Radio>());
    all.push_back(std::make_unique<widgets::ProgressBar>());
    all.push_back(std::make_unique<widgets::Slider>());
    all.push_back(std::make_unique<widgets::Toggle>());
    all.push_back(std::make_unique<widgets::TextField>());
    all.push_back(std::make_unique<widgets::Segmented>());
    all.push_back(std::make_unique<widgets::ComboBox>());
    all.push_back(std::make_unique<widgets::Tag>());
    all.push_back(std::make_unique<widgets::Rating>());
    all.push_back(std::make_unique<widgets::Stepper>());
    all.push_back(std::make_unique<widgets::ProgressRing>());
    all.push_back(std::make_unique<widgets::Badge>());
    all.push_back(std::make_unique<widgets::Alert>());
    all.push_back(std::make_unique<widgets::Tabs>());
    all.push_back(std::make_unique<widgets::Avatar>());
    all.push_back(std::make_unique<widgets::Skeleton>());
    all.push_back(std::make_unique<widgets::Table>());
    all.push_back(std::make_unique<widgets::List>());
    all.push_back(std::make_unique<widgets::Tree>());
    all.push_back(std::make_unique<widgets::Breadcrumb>());
    all.push_back(std::make_unique<widgets::Pagination>());
    all.push_back(std::make_unique<widgets::Toast>());
    all.push_back(std::make_unique<widgets::Card>());
    all.push_back(std::make_unique<widgets::EmptyView>());
    all.push_back(std::make_unique<widgets::StatusTag>());
    all.push_back(std::make_unique<widgets::KbdChip>());
    all.push_back(std::make_unique<widgets::Toolbar>());
    all.push_back(std::make_unique<widgets::Form>());
    all.push_back(std::make_unique<widgets::DataGrid>());
    all.push_back(std::make_unique<widgets::ListData>());
    all.push_back(std::make_unique<widgets::Custom>());
    all.push_back(std::make_unique<widgets::SearchBox>());
    all.push_back(std::make_unique<widgets::PasswordBox>());
    all.push_back(std::make_unique<widgets::TextArea>());
    all.push_back(std::make_unique<widgets::NumberInput>());
    all.push_back(std::make_unique<widgets::DatePicker>());
    all.push_back(std::make_unique<widgets::ColorPicker>());
    all.push_back(std::make_unique<widgets::Dropdown>());
    all.push_back(std::make_unique<widgets::Menu>());
    all.push_back(std::make_unique<widgets::ContextMenu>());
    all.push_back(std::make_unique<widgets::MultiSelect>());
    all.push_back(std::make_unique<widgets::Transfer>());
    all.push_back(std::make_unique<widgets::Timeline>());
    all.push_back(std::make_unique<widgets::Steps>());
    all.push_back(std::make_unique<widgets::Calendar>());
    all.push_back(std::make_unique<widgets::DescriptionList>());
    all.push_back(std::make_unique<widgets::StatCard>());
    all.push_back(std::make_unique<widgets::Chart>());
    all.push_back(std::make_unique<widgets::Carousel>());
    all.push_back(std::make_unique<widgets::Spinner>());
    all.push_back(std::make_unique<widgets::Tooltip>());
    all.push_back(std::make_unique<widgets::Popover>());
    all.push_back(std::make_unique<widgets::Modal>());
    all.push_back(std::make_unique<widgets::Drawer>());
    all.push_back(std::make_unique<widgets::Notification>());
    all.push_back(std::make_unique<widgets::Result>());
    all.push_back(std::make_unique<widgets::Accordion>());
    all.push_back(std::make_unique<widgets::Anchor>());
    all.push_back(std::make_unique<widgets::Splitter>());
    all.push_back(std::make_unique<widgets::ColumnResize>());
    all.push_back(std::make_unique<widgets::TagInput>());
    all.push_back(std::make_unique<widgets::RangeSlider>());
    all.push_back(std::make_unique<widgets::OtpInput>());
    all.push_back(std::make_unique<widgets::Cascader>());
    all.push_back(std::make_unique<widgets::AvatarGroup>());
    all.push_back(std::make_unique<widgets::Heatmap>());
    all.push_back(std::make_unique<widgets::CodeBlock>());
    all.push_back(std::make_unique<widgets::Kanban>());
    all.push_back(std::make_unique<widgets::Gantt>());
    all.push_back(std::make_unique<widgets::Snackbar>());
    all.push_back(std::make_unique<widgets::CommandPalette>());
    all.push_back(std::make_unique<widgets::GuideTour>());
    all.push_back(std::make_unique<widgets::QueryBuilder>());
    all.push_back(std::make_unique<widgets::PropertyGrid>());
    all.push_back(std::make_unique<widgets::Schedule>());
    all.push_back(std::make_unique<widgets::FileDropZone>());
    all.push_back(std::make_unique<widgets::TreeMap>());
    all.push_back(std::make_unique<widgets::Funnel>());
    all.push_back(std::make_unique<widgets::Diff>());
    all.push_back(std::make_unique<widgets::Score>());
    all.push_back(std::make_unique<widgets::Chat>());
    all.push_back(std::make_unique<widgets::MultiStatusBar>());
    all.push_back(std::make_unique<widgets::Ribbon>());
    all.push_back(std::make_unique<widgets::NoticeCenter>());
    all.push_back(std::make_unique<widgets::Sparkline>());
    all.push_back(std::make_unique<widgets::Waterfall>());
    all.push_back(std::make_unique<widgets::StackedBar>());
    all.push_back(std::make_unique<widgets::Gauge>());
    all.push_back(std::make_unique<widgets::JsonTree>());
    all.push_back(std::make_unique<widgets::LogViewer>());
    all.push_back(std::make_unique<widgets::CronEditor>());
    all.push_back(std::make_unique<widgets::MentionInput>());
    all.push_back(std::make_unique<widgets::OrgChart>());
    all.push_back(std::make_unique<widgets::Leaderboard>());
    all.push_back(std::make_unique<widgets::ActivityFeed>());
    all.push_back(std::make_unique<widgets::AudioWaveform>());
    Check(all.size() == 98, "every widget class registered");

    const std::vector<Config> configs = {
        {"light",  kRc,                 LightPalette(), Metrics{}},
        {"dark",   kRc,                 DarkPalette(),  Metrics{}},
        {"hidpi",  kRc,                 LightPalette(), Metrics{}.ForDpi(144)},
    };
    for (const Config& cfg : configs) {
        for (std::size_t i = 0; i < all.size(); ++i) {
            Control* w = all[i].get();
            w->SetBounds(cfg.rc);
            w->SetName(L"widget");
            const std::string tag = std::string(cfg.tag) + " #" + std::to_string(i);
            RecordingPainter g;
            g.Reset();
            w->Paint(g, cfg.pal, cfg.mx);
            Check(g.clipDepth == 0 && g.minClipDepth == 0, tag.c_str());
            CheckInsideAll(g.rects, cfg.rc, tag.c_str());
            Check(!w->AccessibleText().empty(), tag.c_str());
        }
    }

    // Small rects: some first-wave controls may paint outside their bounds
    // (the outer canvas clips them). The object layer must still keep every
    // PushClip paired and never crash at degenerate sizes.
    for (std::size_t i = 0; i < all.size(); ++i) {
        all[i]->SetBounds(Rect(10, 10, 120, 80));
        RecordingPainter g;
        g.Reset();
        all[i]->Paint(g, LightPalette(), Metrics{});
        Check(g.clipDepth == 0 && g.minClipDepth == 0,
              "tiny rect leaves clip balanced");
    }

    for (std::size_t i = 0; i < all.size(); ++i) {
        all[i]->SetBounds(Rect{});
        RecordingPainter g;
        g.Reset();
        all[i]->Paint(g, LightPalette(), Metrics{});
        Check(g.rects.empty(), "empty rect produces no visible geometry");
        Check(g.clipDepth == 0, "empty rect leaves clip balanced");
    }

    // ---- free-function kernel parity ----
    Parity<widgets::Slider>(
        [](IPainter& g, const Rect& r, const SliderState& st,
           const Palette& p, const Metrics& m) {
            PaintSlider(g, r, st, p, m);
        }, "Slider kernel parity");
    Parity<widgets::Toggle>(
        [](IPainter& g, const Rect& r, const ToggleState& st,
           const Palette& p, const Metrics& m) {
            PaintToggleSwitch(g, r, st, p, m);
        }, "Toggle kernel parity");
    Parity<widgets::TextField>(
        [](IPainter& g, const Rect& r, const TextFieldState& st,
           const Palette& p, const Metrics& m) {
            PaintTextField(g, r, st, p, m);
        }, "TextField kernel parity");
    Parity<widgets::Segmented>(
        [](IPainter& g, const Rect& r, const SegmentedState& st,
           const Palette& p, const Metrics& m) {
            PaintSegmented(g, r, st, p, m);
        }, "Segmented kernel parity");
    Parity<widgets::ComboBox>(
        [](IPainter& g, const Rect& r, const ComboBoxState& st,
           const Palette& p, const Metrics& m) {
            PaintComboBox(g, r, st, p, m);
        }, "ComboBox kernel parity");
    Parity<widgets::Tag>(
        [](IPainter& g, const Rect& r, const TagState& st,
           const Palette& p, const Metrics& m) {
            PaintTag(g, r, st, p, m);
        }, "Tag kernel parity");
    Parity<widgets::Rating>(
        [](IPainter& g, const Rect& r, const RatingState& st,
           const Palette& p, const Metrics& m) {
            PaintRating(g, r, st, p, m);
        }, "Rating kernel parity");
    Parity<widgets::Stepper>(
        [](IPainter& g, const Rect& r, const StepperState& st,
           const Palette& p, const Metrics& m) {
            PaintStepper(g, r, st, p, m);
        }, "Stepper kernel parity");
    Parity<widgets::ProgressRing>(
        [](IPainter& g, const Rect& r, const ProgressRingState& st,
           const Palette& p, const Metrics& m) {
            PaintProgressRing(g, r, st, p, m);
        }, "ProgressRing kernel parity");
    Parity<widgets::Badge>(
        [](IPainter& g, const Rect& r, const BadgeState& st,
           const Palette& p, const Metrics& m) {
            PaintBadge(g, r, st, p, m);
        }, "Badge kernel parity");
    Parity<widgets::Alert>(
        [](IPainter& g, const Rect& r, const AlertState& st,
           const Palette& p, const Metrics& m) {
            PaintAlert(g, r, st, p, m);
        }, "Alert kernel parity");
    Parity<widgets::Tabs>(
        [](IPainter& g, const Rect& r, const TabsState& st,
           const Palette& p, const Metrics& m) {
            PaintTabs(g, r, st, p, m);
        }, "Tabs kernel parity");
    Parity<widgets::Avatar>(
        [](IPainter& g, const Rect& r, const AvatarState& st,
           const Palette& p, const Metrics& m) {
            PaintAvatar(g, r, st, p, m);
        }, "Avatar kernel parity");
    Parity<widgets::Skeleton>(
        [](IPainter& g, const Rect& r, const SkeletonState& st,
           const Palette& p, const Metrics& m) {
            PaintSkeleton(g, r, st, p, m);
        }, "Skeleton kernel parity");
    Parity<widgets::Table>(
        [](IPainter& g, const Rect& r, const TableState& st,
           const Palette& p, const Metrics& m) {
            PaintTable(g, r, st, p, m);
        }, "Table kernel parity");
    Parity<widgets::List>(
        [](IPainter& g, const Rect& r, const ListState& st,
           const Palette& p, const Metrics& m) {
            PaintList(g, r, st, p, m);
        }, "List kernel parity");
    Parity<widgets::Tree>(
        [](IPainter& g, const Rect& r, const TreeState& st,
           const Palette& p, const Metrics& m) {
            PaintTree(g, r, st, p, m);
        }, "Tree kernel parity");
    Parity<widgets::Breadcrumb>(
        [](IPainter& g, const Rect& r, const BreadcrumbState& st,
           const Palette& p, const Metrics& m) {
            PaintBreadcrumb(g, r, st, p, m);
        }, "Breadcrumb kernel parity");
    Parity<widgets::Pagination>(
        [](IPainter& g, const Rect& r, const PaginationState& st,
           const Palette& p, const Metrics& m) {
            PaintPagination(g, r, st, p, m);
        }, "Pagination kernel parity");
    Parity<widgets::Toast>(
        [](IPainter& g, const Rect& r, const ToastState& st,
           const Palette& p, const Metrics& m) {
            PaintToast(g, r, st, p, m);
        }, "Toast kernel parity");
    Parity<widgets::Card>(
        [](IPainter& g, const Rect& r, const CardState& st,
           const Palette& p, const Metrics& m) {
            PaintCard(g, r, st, p, m);
        }, "Card kernel parity");
    Parity<widgets::EmptyView>(
        [](IPainter& g, const Rect& r, const EmptyViewState& st,
           const Palette& p, const Metrics& m) {
            PaintEmptyView(g, r, st, p, m);
        }, "EmptyView kernel parity");
    Parity<widgets::StatusTag>(
        [](IPainter& g, const Rect& r, const StatusTagState& st,
           const Palette& p, const Metrics& m) {
            PaintStatusTag(g, r, st, p, m);
        }, "StatusTag kernel parity");
    Parity<widgets::KbdChip>(
        [](IPainter& g, const Rect& r, const KbdChipState& st,
           const Palette& p, const Metrics& m) {
            PaintKbdChip(g, r, st, p, m);
        }, "KbdChip kernel parity");
    Parity<widgets::Toolbar>(
        [](IPainter& g, const Rect& r, const ToolbarState& st,
           const Palette& p, const Metrics& m) {
            PaintToolbar(g, r, st, p, m);
        }, "Toolbar kernel parity");
    Parity<widgets::Form>(
        [](IPainter& g, const Rect& r, const FormState& st,
           const Palette& p, const Metrics& m) {
            PaintForm(g, r, st, p, m);
        }, "Form kernel parity");
    Parity<widgets::DataGrid>(
        [](IPainter& g, const Rect& r, const DataGridState& st,
           const Palette& p, const Metrics& m) {
            PaintDataGrid(g, r, st, p, m);
        }, "DataGrid kernel parity");
    Parity<widgets::ListData>(
        [](IPainter& g, const Rect& r, const ListDataState& st,
           const Palette& p, const Metrics& m) {
            PaintListData(g, r, st, p, m);
        }, "ListData kernel parity");
    Parity<widgets::Custom>(
        [](IPainter& g, const Rect& r, const CustomState& st,
           const Palette& p, const Metrics& m) {
            PaintCustom(g, r, st, p, m);
        }, "Custom kernel parity");
    Parity<widgets::SearchBox>(
        [](IPainter& g, const Rect& r, const SearchBoxState& st,
           const Palette& p, const Metrics& m) {
            PaintSearchBox(g, r, st, p, m);
        }, "SearchBox kernel parity");
    Parity<widgets::PasswordBox>(
        [](IPainter& g, const Rect& r, const PasswordBoxState& st,
           const Palette& p, const Metrics& m) {
            PaintPasswordBox(g, r, st, p, m);
        }, "PasswordBox kernel parity");
    Parity<widgets::TextArea>(
        [](IPainter& g, const Rect& r, const TextAreaState& st,
           const Palette& p, const Metrics& m) {
            PaintTextArea(g, r, st, p, m);
        }, "TextArea kernel parity");
    Parity<widgets::NumberInput>(
        [](IPainter& g, const Rect& r, const NumberInputState& st,
           const Palette& p, const Metrics& m) {
            PaintNumberInput(g, r, st, p, m);
        }, "NumberInput kernel parity");
    Parity<widgets::DatePicker>(
        [](IPainter& g, const Rect& r, const DatePickerState& st,
           const Palette& p, const Metrics& m) {
            PaintDatePicker(g, r, st, p, m);
        }, "DatePicker kernel parity");
    Parity<widgets::ColorPicker>(
        [](IPainter& g, const Rect& r, const ColorPickerState& st,
           const Palette& p, const Metrics& m) {
            PaintColorPicker(g, r, st, p, m);
        }, "ColorPicker kernel parity");
    Parity<widgets::Dropdown>(
        [](IPainter& g, const Rect& r, const DropdownState& st,
           const Palette& p, const Metrics& m) {
            PaintDropdown(g, r, st, p, m);
        }, "Dropdown kernel parity");
    Parity<widgets::Menu>(
        [](IPainter& g, const Rect& r, const MenuState& st,
           const Palette& p, const Metrics& m) {
            PaintMenu(g, r, st, p, m);
        }, "Menu kernel parity");
    Parity<widgets::ContextMenu>(
        [](IPainter& g, const Rect& r, const ContextMenuState& st,
           const Palette& p, const Metrics& m) {
            PaintContextMenu(g, r, st, p, m);
        }, "ContextMenu kernel parity");
    Parity<widgets::MultiSelect>(
        [](IPainter& g, const Rect& r, const MultiSelectState& st,
           const Palette& p, const Metrics& m) {
            PaintMultiSelect(g, r, st, p, m);
        }, "MultiSelect kernel parity");
    Parity<widgets::Transfer>(
        [](IPainter& g, const Rect& r, const TransferState& st,
           const Palette& p, const Metrics& m) {
            PaintTransfer(g, r, st, p, m);
        }, "Transfer kernel parity");
    Parity<widgets::Timeline>(
        [](IPainter& g, const Rect& r, const TimelineState& st,
           const Palette& p, const Metrics& m) {
            PaintTimeline(g, r, st, p, m);
        }, "Timeline kernel parity");
    Parity<widgets::Steps>(
        [](IPainter& g, const Rect& r, const StepsState& st,
           const Palette& p, const Metrics& m) {
            PaintSteps(g, r, st, p, m);
        }, "Steps kernel parity");
    Parity<widgets::Calendar>(
        [](IPainter& g, const Rect& r, const CalendarState& st,
           const Palette& p, const Metrics& m) {
            PaintCalendar(g, r, st, p, m);
        }, "Calendar kernel parity");
    Parity<widgets::DescriptionList>(
        [](IPainter& g, const Rect& r, const DescriptionListState& st,
           const Palette& p, const Metrics& m) {
            PaintDescriptionList(g, r, st, p, m);
        }, "DescriptionList kernel parity");
    Parity<widgets::StatCard>(
        [](IPainter& g, const Rect& r, const StatCardState& st,
           const Palette& p, const Metrics& m) {
            PaintStatCard(g, r, st, p, m);
        }, "StatCard kernel parity");
    Parity<widgets::Chart>(
        [](IPainter& g, const Rect& r, const ChartState& st,
           const Palette& p, const Metrics& m) {
            PaintChart(g, r, st, p, m);
        }, "Chart kernel parity");
    Parity<widgets::Carousel>(
        [](IPainter& g, const Rect& r, const CarouselState& st,
           const Palette& p, const Metrics& m) {
            PaintCarousel(g, r, st, p, m);
        }, "Carousel kernel parity");
    Parity<widgets::Spinner>(
        [](IPainter& g, const Rect& r, const SpinnerState& st,
           const Palette& p, const Metrics& m) {
            PaintSpinner(g, r, st, p, m);
        }, "Spinner kernel parity");
    Parity<widgets::Modal>(
        [](IPainter& g, const Rect& r, const ModalState& st,
           const Palette& p, const Metrics& m) {
            PaintModal(g, r, st, p, m);
        }, "Modal kernel parity");
    Parity<widgets::Drawer>(
        [](IPainter& g, const Rect& r, const DrawerState& st,
           const Palette& p, const Metrics& m) {
            PaintDrawer(g, r, st, p, m);
        }, "Drawer kernel parity");
    Parity<widgets::Notification>(
        [](IPainter& g, const Rect& r, const NotificationState& st,
           const Palette& p, const Metrics& m) {
            PaintNotificationQueue(g, r, st, p, m);
        }, "Notification kernel parity");
    Parity<widgets::Result>(
        [](IPainter& g, const Rect& r, const ResultState& st,
           const Palette& p, const Metrics& m) {
            PaintResult(g, r, st, p, m);
        }, "Result kernel parity");
    Parity<widgets::Accordion>(
        [](IPainter& g, const Rect& r, const AccordionState& st,
           const Palette& p, const Metrics& m) {
            PaintAccordion(g, r, st, p, m);
        }, "Accordion kernel parity");
    Parity<widgets::Anchor>(
        [](IPainter& g, const Rect& r, const AnchorState& st,
           const Palette& p, const Metrics& m) {
            PaintAnchor(g, r, st, p, m);
        }, "Anchor kernel parity");
    Parity<widgets::Splitter>(
        [](IPainter& g, const Rect& r, const SplitterState& st,
           const Palette& p, const Metrics& m) {
            PaintSplitter(g, r, st, p, m);
        }, "Splitter kernel parity");
    Parity<widgets::ColumnResize>(
        [](IPainter& g, const Rect& r, const ColumnResizeState& st,
           const Palette& p, const Metrics& m) {
            PaintColumnResize(g, r, st, p, m);
        }, "ColumnResize kernel parity");
    Parity<widgets::TagInput>(
        [](IPainter& g, const Rect& r, const TagInputState& st,
           const Palette& p, const Metrics& m) {
            PaintTagInput(g, r, st, p, m);
        }, "TagInput kernel parity");
    Parity<widgets::RangeSlider>(
        [](IPainter& g, const Rect& r, const RangeSliderState& st,
           const Palette& p, const Metrics& m) {
            PaintRangeSlider(g, r, st, p, m);
        }, "RangeSlider kernel parity");
    Parity<widgets::OtpInput>(
        [](IPainter& g, const Rect& r, const OtpInputState& st,
           const Palette& p, const Metrics& m) {
            PaintOtpInput(g, r, st, p, m);
        }, "OtpInput kernel parity");
    Parity<widgets::Cascader>(
        [](IPainter& g, const Rect& r, const CascaderState& st,
           const Palette& p, const Metrics& m) {
            PaintCascader(g, r, st, p, m);
        }, "Cascader kernel parity");
    Parity<widgets::AvatarGroup>(
        [](IPainter& g, const Rect& r, const AvatarGroupState& st,
           const Palette& p, const Metrics& m) {
            PaintAvatarGroup(g, r, st, p, m);
        }, "AvatarGroup kernel parity");
    Parity<widgets::Heatmap>(
        [](IPainter& g, const Rect& r, const HeatmapState& st,
           const Palette& p, const Metrics& m) {
            PaintHeatmap(g, r, st, p, m);
        }, "Heatmap kernel parity");
    Parity<widgets::CodeBlock>(
        [](IPainter& g, const Rect& r, const CodeBlockState& st,
           const Palette& p, const Metrics& m) {
            PaintCodeBlock(g, r, st, p, m);
        }, "CodeBlock kernel parity");
    Parity<widgets::Kanban>(
        [](IPainter& g, const Rect& r, const KanbanState& st,
           const Palette& p, const Metrics& m) {
            PaintKanban(g, r, st, p, m);
        }, "Kanban kernel parity");
    Parity<widgets::Gantt>(
        [](IPainter& g, const Rect& r, const GanttState& st,
           const Palette& p, const Metrics& m) {
            PaintGantt(g, r, st, p, m);
        }, "Gantt kernel parity");
    Parity<widgets::Snackbar>(
        [](IPainter& g, const Rect& r, const SnackbarState& st,
           const Palette& p, const Metrics& m) {
            PaintSnackbar(g, r, st, p, m);
        }, "Snackbar kernel parity");
    Parity<widgets::CommandPalette>(
        [](IPainter& g, const Rect& r, const CommandPaletteState& st,
           const Palette& p, const Metrics& m) {
            PaintCommandPalette(g, r, st, p, m);
        }, "CommandPalette kernel parity");
    Parity<widgets::QueryBuilder>(
        [](IPainter& g, const Rect& r, const QueryBuilderState& st,
           const Palette& p, const Metrics& m) {
            PaintQueryBuilder(g, r, st, p, m);
        }, "QueryBuilder kernel parity");
    Parity<widgets::PropertyGrid>(
        [](IPainter& g, const Rect& r, const PropertyGridState& st,
           const Palette& p, const Metrics& m) {
            PaintPropertyGrid(g, r, st, p, m);
        }, "PropertyGrid kernel parity");
    Parity<widgets::Schedule>(
        [](IPainter& g, const Rect& r, const ScheduleState& st,
           const Palette& p, const Metrics& m) {
            PaintSchedule(g, r, st, p, m);
        }, "Schedule kernel parity");
    Parity<widgets::FileDropZone>(
        [](IPainter& g, const Rect& r, const FileDropZoneState& st,
           const Palette& p, const Metrics& m) {
            PaintFileDropZone(g, r, st, p, m);
        }, "FileDropZone kernel parity");
    Parity<widgets::TreeMap>(
        [](IPainter& g, const Rect& r, const TreeMapState& st,
           const Palette& p, const Metrics& m) {
            PaintTreeMap(g, r, st, p, m);
        }, "TreeMap kernel parity");
    Parity<widgets::Funnel>(
        [](IPainter& g, const Rect& r, const FunnelState& st,
           const Palette& p, const Metrics& m) {
            PaintFunnel(g, r, st, p, m);
        }, "Funnel kernel parity");
    Parity<widgets::Diff>(
        [](IPainter& g, const Rect& r, const DiffState& st,
           const Palette& p, const Metrics& m) {
            PaintDiff(g, r, st, p, m);
        }, "Diff kernel parity");
    Parity<widgets::Score>(
        [](IPainter& g, const Rect& r, const ScoreState& st,
           const Palette& p, const Metrics& m) {
            PaintScore(g, r, st, p, m);
        }, "Score kernel parity");
    Parity<widgets::Chat>(
        [](IPainter& g, const Rect& r, const ChatState& st,
           const Palette& p, const Metrics& m) {
            PaintChat(g, r, st, p, m);
        }, "Chat kernel parity");
    Parity<widgets::MultiStatusBar>(
        [](IPainter& g, const Rect& r, const MultiStatusBarState& st,
           const Palette& p, const Metrics& m) {
            PaintMultiStatusBar(g, r, st, p, m);
        }, "MultiStatusBar kernel parity");
    Parity<widgets::Ribbon>(
        [](IPainter& g, const Rect& r, const RibbonState& st,
           const Palette& p, const Metrics& m) {
            PaintRibbon(g, r, st, p, m);
        }, "Ribbon kernel parity");
    Parity<widgets::NoticeCenter>(
        [](IPainter& g, const Rect& r, const NoticeCenterState& st,
           const Palette& p, const Metrics& m) {
            PaintNoticeCenter(g, r, st, p, m);
        }, "NoticeCenter kernel parity");
    Parity<widgets::Sparkline>(
        [](IPainter& g, const Rect& r, const SparklineState& st,
           const Palette& p, const Metrics& m) {
            PaintSparkline(g, r, st, p, m);
        }, "Sparkline kernel parity");
    Parity<widgets::Waterfall>(
        [](IPainter& g, const Rect& r, const WaterfallState& st,
           const Palette& p, const Metrics& m) {
            PaintWaterfall(g, r, st, p, m);
        }, "Waterfall kernel parity");
    Parity<widgets::StackedBar>(
        [](IPainter& g, const Rect& r, const StackedBarState& st,
           const Palette& p, const Metrics& m) {
            PaintStackedBar(g, r, st, p, m);
        }, "StackedBar kernel parity");
    Parity<widgets::Gauge>(
        [](IPainter& g, const Rect& r, const GaugeState& st,
           const Palette& p, const Metrics& m) {
            PaintGauge(g, r, st, p, m);
        }, "Gauge kernel parity");
    Parity<widgets::JsonTree>(
        [](IPainter& g, const Rect& r, const JsonTreeState& st,
           const Palette& p, const Metrics& m) {
            PaintJsonTree(g, r, st, p, m);
        }, "JsonTree kernel parity");
    Parity<widgets::LogViewer>(
        [](IPainter& g, const Rect& r, const LogViewerState& st,
           const Palette& p, const Metrics& m) {
            PaintLogViewer(g, r, st, p, m);
        }, "LogViewer kernel parity");
    Parity<widgets::CronEditor>(
        [](IPainter& g, const Rect& r, const CronEditorState& st,
           const Palette& p, const Metrics& m) {
            PaintCronEditor(g, r, st, p, m);
        }, "CronEditor kernel parity");
    Parity<widgets::MentionInput>(
        [](IPainter& g, const Rect& r, const MentionInputState& st,
           const Palette& p, const Metrics& m) {
            PaintMentionInput(g, r, st, p, m);
        }, "MentionInput kernel parity");
    Parity<widgets::OrgChart>(
        [](IPainter& g, const Rect& r, const OrgChartState& st,
           const Palette& p, const Metrics& m) {
            PaintOrgChart(g, r, st, p, m);
        }, "OrgChart kernel parity");
    Parity<widgets::Leaderboard>(
        [](IPainter& g, const Rect& r, const LeaderboardState& st,
           const Palette& p, const Metrics& m) {
            PaintLeaderboard(g, r, st, p, m);
        }, "Leaderboard kernel parity");
    Parity<widgets::ActivityFeed>(
        [](IPainter& g, const Rect& r, const ActivityFeedState& st,
           const Palette& p, const Metrics& m) {
            PaintActivityFeed(g, r, st, p, m);
        }, "ActivityFeed kernel parity");
    Parity<widgets::AudioWaveform>(
        [](IPainter& g, const Rect& r, const AudioWaveformState& st,
           const Palette& p, const Metrics& m) {
            PaintWaveform(g, r, st, p, m);
        }, "AudioWaveform kernel parity");

    // ---- polymorphism through ControlHost ----
    {
        ControlHost host;
        auto b = std::make_unique<widgets::Button>();
        b->SetBounds(Rect(0, 0, 100, 32));
        b->text = L"保存";
        b->SetTabIndex(1);
        const int id1 = host.Add(std::move(b));
        auto t = std::make_unique<widgets::TextField>();
        t->SetBounds(Rect(20, 10, 160, 42));
        t->state.text = L"输入";
        t->SetTabIndex(2);
        const int id2 = host.Add(std::move(t));
        Check(host.Count() == 2, "ControlHost owns widget objects");
        Check(id1 != id2 && id1 > 0 && id2 > 0, "widget ids are unique");
        Check(host.HitControl(Point{5, 5}) == id1, "hit resolves to topmost widget");
        PointerEvent ev;
        ev.pos = Point{5, 5}; ev.down = true;
        const ControlResult r = host.DispatchPointer(ev);
        Check(r.handled && r.id == id1, "pointer routed polymorphically");
        Check(host.MoveFocus(true), "tab focus across widget objects");
        Check(!BuildAccessibleTree(host).empty(), "accessible tree from widgets");
        RecordingPainter g;
        g.Reset();
        host.PaintAll(g, LightPalette(), Metrics{});
        Check(g.ops > 0, "PaintAll draws hosted widgets");
    }

    // ---- a widget owns its state and event behaviour ----
    {
        widgets::Button b;
        b.SetBounds(Rect(0, 0, 120, 36));
        b.text = L"保存";
        b.style = ButtonStyle::Primary;
        b.SetName(L"保存按钮");
        Check(b.AccessibleText() == L"保存按钮", "AccessibleText uses Name");
        Check(b.Role() == ControlRole::Button, "constructor sets role");
        PointerEvent ev;
        ev.pos = Point{10, 10}; ev.down = true;
        const ControlResult r = b.OnPointer(ev);
        Check(r.handled && r.redraw, "OnPointer handled");
        Check(b.state.pressed, "OnPointer writes owned state");
        KeyEvent k;
        k.vk = keys::Return;
        Check(b.OnKey(k).handled, "OnKey handled");
        b.state.hover = true;
        RecordingPainter g;
        g.Reset();
        b.Paint(g, LightPalette(), Metrics{});
        Check(g.ops > 0, "widget paints from owned state");
    }

    std::printf("test_control_classes: OK (%d checks)\n", g_checks);
    return 0;
}
