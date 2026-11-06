// ============================================================
// AirUI - ControlsClasses.cpp
//
// Source-compatible free functions. They build a widget object,
// inject the caller-owned state and delegate to its methods.
// The drawing code now lives in widget member functions in the
// Controls*.cpp implementation files.
// ============================================================
#include "airui/WidgetControls.h"

namespace air {
namespace ui {

void PaintButton(IPainter& g, const Rect& rc, const std::wstring& text, ButtonStyle style, const ControlState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Button w;
    w.SetBounds(rc);
    w.state = st;
    w.text = text;
    w.style = style;
    w.Paint(g, pal, mx);
}

void PaintCheckbox(IPainter& g, const Rect& rc, const std::wstring& text, const ControlState& st, const Palette& pal)
{
    widgets::Checkbox w;
    w.SetBounds(rc);
    w.state = st;
    w.text = text;
    w.Paint(g, pal, Metrics{});
}

void PaintRadio(IPainter& g, const Rect& rc, const std::wstring& text, const ControlState& st, const Palette& pal)
{
    widgets::Radio w;
    w.SetBounds(rc);
    w.state = st;
    w.text = text;
    w.Paint(g, pal, Metrics{});
}

void PaintProgressBar(IPainter& g, const Rect& rc, float progress, const ControlState& st, const Palette& pal, const Metrics& mx)
{
    widgets::ProgressBar w;
    w.SetBounds(rc);
    w.state = st;
    w.progress = progress;
    w.Paint(g, pal, mx);
}

void PaintSlider(IPainter& g, const Rect& rc, const SliderState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Slider w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintToggleSwitch(IPainter& g, const Rect& rc, const ToggleState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Toggle w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintTextField(IPainter& g, const Rect& rc, const TextFieldState& st, const Palette& pal, const Metrics& mx)
{
    widgets::TextField w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintSegmented(IPainter& g, const Rect& rc, const SegmentedState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Segmented w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintComboBox(IPainter& g, const Rect& rc, const ComboBoxState& st, const Palette& pal, const Metrics& mx)
{
    widgets::ComboBox w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintTag(IPainter& g, const Rect& rc, const TagState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Tag w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintRating(IPainter& g, const Rect& rc, const RatingState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Rating w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintStepper(IPainter& g, const Rect& rc, const StepperState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Stepper w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintProgressRing(IPainter& g, const Rect& rc, const ProgressRingState& st, const Palette& pal, const Metrics& mx)
{
    widgets::ProgressRing w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintBadge(IPainter& g, const Rect& rc, const BadgeState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Badge w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintAlert(IPainter& g, const Rect& rc, const AlertState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Alert w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintTabs(IPainter& g, const Rect& rc, const TabsState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Tabs w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintAvatar(IPainter& g, const Rect& rc, const AvatarState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Avatar w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintSkeleton(IPainter& g, const Rect& rc, const SkeletonState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Skeleton w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintTable(IPainter& g, const Rect& rc, const TableState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Table w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

TableLayout MeasureTable(const Rect& rc, const TableState& st, const Metrics& mx)
{
    widgets::Table w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int TableRowAt(const Rect& rc, const TableState& st, const Metrics& mx, Point pt)
{
    widgets::Table w;
    w.SetBounds(rc);
    w.state = st;
    return w.RowAt(mx, pt);
}

int TableColumnAt(const Rect& rc, const TableState& st, const Metrics& mx, Point pt)
{
    widgets::Table w;
    w.SetBounds(rc);
    w.state = st;
    return w.ColumnAt(mx, pt);
}

void PaintList(IPainter& g, const Rect& rc, const ListState& st, const Palette& pal, const Metrics& mx)
{
    widgets::List w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

int ListRowAt(const Rect& rc, const ListState& st, const Metrics& mx, Point pt)
{
    widgets::List w;
    w.SetBounds(rc);
    w.state = st;
    return w.RowAt(mx, pt);
}

void PaintTree(IPainter& g, const Rect& rc, const TreeState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Tree w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

int TreeRowAt(const Rect& rc, const TreeState& st, const Metrics& mx, Point pt)
{
    widgets::Tree w;
    w.SetBounds(rc);
    w.state = st;
    return w.RowAt(mx, pt);
}

int TreeToggleAt(const Rect& rc, const TreeState& st, const Metrics& mx, Point pt)
{
    widgets::Tree w;
    w.SetBounds(rc);
    w.state = st;
    return w.ToggleAt(mx, pt);
}

void PaintBreadcrumb(IPainter& g, const Rect& rc, const BreadcrumbState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Breadcrumb w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintPagination(IPainter& g, const Rect& rc, const PaginationState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Pagination w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

std::vector<PaginationSlot> MeasurePagination(const Rect& rc, const PaginationState& st, const Metrics& mx)
{
    widgets::Pagination w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

void PaintToast(IPainter& g, const Rect& rc, const ToastState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Toast w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintCard(IPainter& g, const Rect& rc, const CardState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Card w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

CardLayout MeasureCard(const Rect& rc, const CardState& st, const Metrics& mx)
{
    widgets::Card w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

CardHit HitCard(const Rect& rc, const CardState& st, const Metrics& mx, Point pt)
{
    widgets::Card w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintEmptyView(IPainter& g, const Rect& rc, const EmptyViewState& st, const Palette& pal, const Metrics& mx)
{
    widgets::EmptyView w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintStatusTag(IPainter& g, const Rect& rc, const StatusTagState& st, const Palette& pal, const Metrics& mx)
{
    widgets::StatusTag w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintKbdChip(IPainter& g, const Rect& rc, const KbdChipState& st, const Palette& pal, const Metrics& mx)
{
    widgets::KbdChip w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintToolbar(IPainter& g, const Rect& rc, const ToolbarState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Toolbar w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

ToolbarLayout MeasureToolbar(const Rect& rc, const ToolbarState& st, const Metrics& mx)
{
    widgets::Toolbar w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

ToolbarHit HitToolbar(const Rect& rc, const ToolbarState& st, const Metrics& mx, Point pt)
{
    widgets::Toolbar w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintForm(IPainter& g, const Rect& rc, const FormState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Form w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

FormLayout MeasureForm(const Rect& rc, const FormState& st, const Metrics& mx)
{
    widgets::Form w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

FormHit HitForm(const Rect& rc, const FormState& st, const Metrics& mx, Point pt)
{
    widgets::Form w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintDataGrid(IPainter& g, const Rect& rc, const DataGridState& st, const Palette& pal, const Metrics& mx)
{
    widgets::DataGrid w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

DataGridLayout MeasureDataGrid(const Rect& rc, const DataGridState& st, const Metrics& mx)
{
    widgets::DataGrid w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

DataGridHit HitDataGrid(const Rect& rc, const DataGridState& st, const Metrics& mx, Point pt)
{
    widgets::DataGrid w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintListData(IPainter& g, const Rect& rc, const ListDataState& st, const Palette& pal, const Metrics& mx)
{
    widgets::ListData w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

int ListDataRowAt(const Rect& rc, const ListDataState& st, const Metrics& mx, Point pt)
{
    widgets::ListData w;
    w.SetBounds(rc);
    w.state = st;
    return w.RowAt(mx, pt);
}

void PaintCustom(IPainter& g, const Rect& rc, const CustomState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Custom w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintSearchBox(IPainter& g, const Rect& rc, const SearchBoxState& st, const Palette& pal, const Metrics& mx)
{
    widgets::SearchBox w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

SearchBoxHit HitSearchBox(const Rect& rc, const SearchBoxState& st, const Metrics& mx, Point pt)
{
    widgets::SearchBox w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintPasswordBox(IPainter& g, const Rect& rc, const PasswordBoxState& st, const Palette& pal, const Metrics& mx)
{
    widgets::PasswordBox w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

PasswordBoxHit HitPasswordBox(const Rect& rc, const PasswordBoxState& st, const Metrics& mx, Point pt)
{
    widgets::PasswordBox w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintTextArea(IPainter& g, const Rect& rc, const TextAreaState& st, const Palette& pal, const Metrics& mx)
{
    widgets::TextArea w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintNumberInput(IPainter& g, const Rect& rc, const NumberInputState& st, const Palette& pal, const Metrics& mx)
{
    widgets::NumberInput w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

NumberInputHit HitNumberInput(const Rect& rc, const NumberInputState& st, const Metrics& mx, Point pt)
{
    widgets::NumberInput w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintDatePicker(IPainter& g, const Rect& rc, const DatePickerState& st, const Palette& pal, const Metrics& mx)
{
    widgets::DatePicker w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

int DatePickerDayAt(const Rect& rc, const DatePickerState& st, const Metrics& mx, Point pt)
{
    widgets::DatePicker w;
    w.SetBounds(rc);
    w.state = st;
    return w.DayAt(mx, pt);
}

void PaintColorPicker(IPainter& g, const Rect& rc, const ColorPickerState& st, const Palette& pal, const Metrics& mx)
{
    widgets::ColorPicker w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

int ColorPickerSwatchAt(const Rect& rc, const ColorPickerState& st, const Metrics& mx, Point pt)
{
    widgets::ColorPicker w;
    w.SetBounds(rc);
    w.state = st;
    return w.SwatchAt(mx, pt);
}

void PaintDropdown(IPainter& g, const Rect& rc, const DropdownState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Dropdown w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

DropdownLayout MeasureDropdown(const Rect& rc, const DropdownState& st, const Metrics& mx)
{
    widgets::Dropdown w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int DropdownOptionAt(const Rect& rc, const DropdownState& st, const Metrics& mx, Point pt)
{
    widgets::Dropdown w;
    w.SetBounds(rc);
    w.state = st;
    return w.OptionAt(mx, pt);
}

void PaintMenu(IPainter& g, const Rect& rc, const MenuState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Menu w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

int MenuItemAt(const Rect& rc, const MenuState& st, const Metrics& mx, Point pt)
{
    widgets::Menu w;
    w.SetBounds(rc);
    w.state = st;
    return w.ItemAt(mx, pt);
}

void PaintContextMenu(IPainter& g, const Rect& client, const ContextMenuState& st, const Palette& pal, const Metrics& mx)
{
    widgets::ContextMenu w;
    w.SetBounds(client);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintMultiSelect(IPainter& g, const Rect& rc, const MultiSelectState& st, const Palette& pal, const Metrics& mx)
{
    widgets::MultiSelect w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

int MultiSelectAt(const Rect& rc, const MultiSelectState& st, const Metrics& mx, Point pt)
{
    widgets::MultiSelect w;
    w.SetBounds(rc);
    w.state = st;
    return w.At(mx, pt);
}

void PaintTransfer(IPainter& g, const Rect& rc, const TransferState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Transfer w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

TransferLayout MeasureTransfer(const Rect& rc, const TransferState& st, const Metrics& mx)
{
    widgets::Transfer w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

TransferHit HitTransfer(const Rect& rc, const TransferState& st, const Metrics& mx, Point pt)
{
    widgets::Transfer w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintTimeline(IPainter& g, const Rect& rc, const TimelineState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Timeline w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintSteps(IPainter& g, const Rect& rc, const StepsState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Steps w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

int StepAt(const Rect& rc, const StepsState& st, const Metrics& mx, Point pt)
{
    widgets::Steps w;
    w.SetBounds(rc);
    w.state = st;
    return w.StepAt(mx, pt);
}

void PaintCalendar(IPainter& g, const Rect& rc, const CalendarState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Calendar w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

CalendarLayout MeasureCalendar(const Rect& rc, const CalendarState& st, const Metrics& mx)
{
    widgets::Calendar w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int CalendarDayAt(const Rect& rc, const CalendarState& st, const Metrics& mx, Point pt)
{
    widgets::Calendar w;
    w.SetBounds(rc);
    w.state = st;
    return w.DayAt(mx, pt);
}

void PaintDescriptionList(IPainter& g, const Rect& rc, const DescriptionListState& st, const Palette& pal, const Metrics& mx)
{
    widgets::DescriptionList w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintStatCard(IPainter& g, const Rect& rc, const StatCardState& st, const Palette& pal, const Metrics& mx)
{
    widgets::StatCard w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

StatCardLayout MeasureStatCard(const Rect& rc, const StatCardState& st, const Metrics& mx)
{
    widgets::StatCard w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

StatCardHit HitStatCard(const Rect& rc, const StatCardState& st, const Metrics& mx, Point pt)
{
    widgets::StatCard w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintChart(IPainter& g, const Rect& rc, const ChartState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Chart w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

int ChartBarAt(const Rect& rc, const ChartState& st, const Metrics& mx, Point pt)
{
    widgets::Chart w;
    w.SetBounds(rc);
    w.state = st;
    return w.BarAt(mx, pt);
}

void PaintCarousel(IPainter& g, const Rect& rc, const CarouselState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Carousel w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

CarouselHitResult HitCarousel(const Rect& rc, const CarouselState& st, const Metrics& mx, Point pt)
{
    widgets::Carousel w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintSpinner(IPainter& g, const Rect& rc, const SpinnerState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Spinner w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintTooltip(IPainter& g, const Rect& anchor, const Rect& client, const TooltipState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Tooltip w;
    w.SetBounds(anchor);
    w.state = st;
    w.client = client;
    w.Paint(g, pal, mx);
}

void PaintPopover(IPainter& g, const Rect& anchor, const Rect& client, const PopoverState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Popover w;
    w.SetBounds(anchor);
    w.state = st;
    w.client = client;
    w.Paint(g, pal, mx);
}

void PaintModal(IPainter& g, const Rect& client, const ModalState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Modal w;
    w.SetBounds(client);
    w.state = st;
    w.Paint(g, pal, mx);
}

ModalLayout MeasureModal(const Rect& client, const ModalState& st, const Metrics& mx)
{
    widgets::Modal w;
    w.SetBounds(client);
    w.state = st;
    return w.Measure(mx);
}

int ModalButtonAt(const Rect& client, const ModalState& st, const Metrics& mx, Point pt)
{
    widgets::Modal w;
    w.SetBounds(client);
    w.state = st;
    return w.ButtonAt(mx, pt);
}

void PaintDrawer(IPainter& g, const Rect& client, const DrawerState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Drawer w;
    w.SetBounds(client);
    w.state = st;
    w.Paint(g, pal, mx);
}

DrawerLayout MeasureDrawer(const Rect& client, const DrawerState& st, const Metrics& mx)
{
    widgets::Drawer w;
    w.SetBounds(client);
    w.state = st;
    return w.Measure(mx);
}

void PaintNotificationQueue(IPainter& g, const Rect& client, const NotificationState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Notification w;
    w.SetBounds(client);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintResult(IPainter& g, const Rect& rc, const ResultState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Result w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintAccordion(IPainter& g, const Rect& rc, const AccordionState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Accordion w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

int AccordionHeaderAt(const Rect& rc, const AccordionState& st, const Metrics& mx, Point pt)
{
    widgets::Accordion w;
    w.SetBounds(rc);
    w.state = st;
    return w.HeaderAt(mx, pt);
}

void PaintAnchor(IPainter& g, const Rect& rc, const AnchorState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Anchor w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

void PaintSplitter(IPainter& g, const Rect& rc, const SplitterState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Splitter w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

SplitterLayout MeasureSplitter(const Rect& rc, const SplitterState& st, const Metrics& mx)
{
    widgets::Splitter w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

void PaintColumnResize(IPainter& g, const Rect& rc, const ColumnResizeState& st, const Palette& pal, const Metrics& mx)
{
    widgets::ColumnResize w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

ColumnResizeLayout MeasureColumnResize(const Rect& rc, const ColumnResizeState& st, const Metrics& mx)
{
    widgets::ColumnResize w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int ColumnResizeHandleAt(const Rect& rc, const ColumnResizeState& st, const Metrics& mx, Point pt)
{
    widgets::ColumnResize w;
    w.SetBounds(rc);
    w.state = st;
    return w.HandleAt(mx, pt);
}

void PaintTagInput(IPainter& g, const Rect& rc, const TagInputState& st, const Palette& pal, const Metrics& mx)
{
    widgets::TagInput w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

TagInputLayout MeasureTagInput(const Rect& rc, const TagInputState& st, const Metrics& mx)
{
    widgets::TagInput w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

TagInputHit HitTagInput(const Rect& rc, const TagInputState& st, const Metrics& mx, Point pt)
{
    widgets::TagInput w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintRangeSlider(IPainter& g, const Rect& rc, const RangeSliderState& st, const Palette& pal, const Metrics& mx)
{
    widgets::RangeSlider w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

RangeSliderLayout MeasureRangeSlider(const Rect& rc, const RangeSliderState& st, const Metrics& mx)
{
    widgets::RangeSlider w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int RangeSliderThumbAt(const Rect& rc, const RangeSliderState& st, const Metrics& mx, Point pt)
{
    widgets::RangeSlider w;
    w.SetBounds(rc);
    w.state = st;
    return w.ThumbAt(mx, pt);
}

int RangeSliderValueAt(const Rect& rc, const RangeSliderState& st, const Metrics& mx, Point pt)
{
    widgets::RangeSlider w;
    w.SetBounds(rc);
    w.state = st;
    return w.ValueAt(mx, pt);
}

void PaintOtpInput(IPainter& g, const Rect& rc, const OtpInputState& st, const Palette& pal, const Metrics& mx)
{
    widgets::OtpInput w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

OtpInputLayout MeasureOtpInput(const Rect& rc, const OtpInputState& st, const Metrics& mx)
{
    widgets::OtpInput w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int OtpCellAt(const Rect& rc, const OtpInputState& st, const Metrics& mx, Point pt)
{
    widgets::OtpInput w;
    w.SetBounds(rc);
    w.state = st;
    return w.OtpCellAt(mx, pt);
}

void PaintCascader(IPainter& g, const Rect& rc, const CascaderState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Cascader w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

CascaderLayout MeasureCascader(const Rect& rc, const CascaderState& st, const Metrics& mx)
{
    widgets::Cascader w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

CascaderHit HitCascader(const Rect& rc, const CascaderState& st, const Metrics& mx, Point pt)
{
    widgets::Cascader w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintAvatarGroup(IPainter& g, const Rect& rc, const AvatarGroupState& st, const Palette& pal, const Metrics& mx)
{
    widgets::AvatarGroup w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

AvatarGroupLayout MeasureAvatarGroup(const Rect& rc, const AvatarGroupState& st, const Metrics& mx)
{
    widgets::AvatarGroup w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int AvatarGroupAt(const Rect& rc, const AvatarGroupState& st, const Metrics& mx, Point pt)
{
    widgets::AvatarGroup w;
    w.SetBounds(rc);
    w.state = st;
    return w.At(mx, pt);
}

void PaintHeatmap(IPainter& g, const Rect& rc, const HeatmapState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Heatmap w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

HeatmapLayout MeasureHeatmap(const Rect& rc, const HeatmapState& st, const Metrics& mx)
{
    widgets::Heatmap w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

HeatmapCell HeatmapCellAt(const Rect& rc, const HeatmapState& st, const Metrics& mx, Point pt)
{
    widgets::Heatmap w;
    w.SetBounds(rc);
    w.state = st;
    return w.CellAt(mx, pt);
}

void PaintCodeBlock(IPainter& g, const Rect& rc, const CodeBlockState& st, const Palette& pal, const Metrics& mx)
{
    widgets::CodeBlock w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

CodeBlockLayout MeasureCodeBlock(const Rect& rc, const CodeBlockState& st, const Metrics& mx)
{
    widgets::CodeBlock w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

void PaintKanban(IPainter& g, const Rect& rc, const KanbanState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Kanban w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

KanbanLayout MeasureKanban(const Rect& rc, const KanbanState& st, const Metrics& mx)
{
    widgets::Kanban w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

KanbanHit HitKanban(const Rect& rc, const KanbanState& st, const Metrics& mx, Point pt)
{
    widgets::Kanban w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintGantt(IPainter& g, const Rect& rc, const GanttState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Gantt w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

GanttLayout MeasureGantt(const Rect& rc, const GanttState& st, const Metrics& mx)
{
    widgets::Gantt w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int GanttTaskAt(const Rect& rc, const GanttState& st, const Metrics& mx, Point pt)
{
    widgets::Gantt w;
    w.SetBounds(rc);
    w.state = st;
    return w.TaskAt(mx, pt);
}

void PaintSnackbar(IPainter& g, const Rect& client, const SnackbarState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Snackbar w;
    w.SetBounds(client);
    w.state = st;
    w.Paint(g, pal, mx);
}

SnackbarLayout MeasureSnackbar(const Rect& client, const SnackbarState& st, const Metrics& mx)
{
    widgets::Snackbar w;
    w.SetBounds(client);
    w.state = st;
    return w.Measure(mx);
}

SnackbarHit HitSnackbar(const Rect& client, const SnackbarState& st, const Metrics& mx, Point pt)
{
    widgets::Snackbar w;
    w.SetBounds(client);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintCommandPalette(IPainter& g, const Rect& client, const CommandPaletteState& st, const Palette& pal, const Metrics& mx)
{
    widgets::CommandPalette w;
    w.SetBounds(client);
    w.state = st;
    w.Paint(g, pal, mx);
}

CommandPaletteLayout MeasureCommandPalette(const Rect& client, const CommandPaletteState& st, const Metrics& mx)
{
    widgets::CommandPalette w;
    w.SetBounds(client);
    w.state = st;
    return w.Measure(mx);
}

CommandPaletteHit HitCommandPalette(const Rect& client, const CommandPaletteState& st, const Metrics& mx, Point pt)
{
    widgets::CommandPalette w;
    w.SetBounds(client);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintGuideTour(IPainter& g, const Rect& client, const Rect& anchor, const GuideTourState& st, const Palette& pal, const Metrics& mx)
{
    widgets::GuideTour w;
    w.SetBounds(client);
    w.state = st;
    w.anchor = anchor;
    w.Paint(g, pal, mx);
}

GuideTourLayout MeasureGuideTour(const Rect& client, const Rect& anchor, const GuideTourState& st, const Metrics& mx)
{
    widgets::GuideTour w;
    w.SetBounds(client);
    w.state = st;
    w.anchor = anchor;
    return w.Measure(mx);
}

GuideTourHit HitGuideTour(const Rect& client, const Rect& anchor, const GuideTourState& st, const Metrics& mx, Point pt)
{
    widgets::GuideTour w;
    w.SetBounds(client);
    w.state = st;
    w.anchor = anchor;
    return w.HitAt(mx, pt);
}

void PaintQueryBuilder(IPainter& g, const Rect& rc, const QueryBuilderState& st, const Palette& pal, const Metrics& mx)
{
    widgets::QueryBuilder w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

QueryBuilderLayout MeasureQueryBuilder(const Rect& rc, const QueryBuilderState& st, const Metrics& mx)
{
    widgets::QueryBuilder w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

QueryBuilderHit HitQueryBuilder(const Rect& rc, const QueryBuilderState& st, const Metrics& mx, Point pt)
{
    widgets::QueryBuilder w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintPropertyGrid(IPainter& g, const Rect& rc, const PropertyGridState& st, const Palette& pal, const Metrics& mx)
{
    widgets::PropertyGrid w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

PropertyGridLayout MeasurePropertyGrid(const Rect& rc, const PropertyGridState& st, const Metrics& mx)
{
    widgets::PropertyGrid w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

PropertyGridHit HitPropertyGrid(const Rect& rc, const PropertyGridState& st, const Metrics& mx, Point pt)
{
    widgets::PropertyGrid w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintSchedule(IPainter& g, const Rect& rc, const ScheduleState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Schedule w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

ScheduleLayout MeasureSchedule(const Rect& rc, const ScheduleState& st, const Metrics& mx)
{
    widgets::Schedule w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int ScheduleEventAt(const Rect& rc, const ScheduleState& st, const Metrics& mx, Point pt)
{
    widgets::Schedule w;
    w.SetBounds(rc);
    w.state = st;
    return w.EventAt(mx, pt);
}

void PaintFileDropZone(IPainter& g, const Rect& rc, const FileDropZoneState& st, const Palette& pal, const Metrics& mx)
{
    widgets::FileDropZone w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

FileDropZoneLayout MeasureFileDropZone(const Rect& rc, const FileDropZoneState& st, const Metrics& mx)
{
    widgets::FileDropZone w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

FileDropZoneHit HitFileDropZone(const Rect& rc, const FileDropZoneState& st, const Metrics& mx, Point pt)
{
    widgets::FileDropZone w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintTreeMap(IPainter& g, const Rect& rc, const TreeMapState& st, const Palette& pal, const Metrics& mx)
{
    widgets::TreeMap w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

TreeMapLayout MeasureTreeMap(const Rect& rc, const TreeMapState& st, const Metrics& mx)
{
    widgets::TreeMap w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int TreeMapTileAt(const Rect& rc, const TreeMapState& st, const Metrics& mx, Point pt)
{
    widgets::TreeMap w;
    w.SetBounds(rc);
    w.state = st;
    return w.TileAt(mx, pt);
}

void PaintFunnel(IPainter& g, const Rect& rc, const FunnelState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Funnel w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

FunnelLayout MeasureFunnel(const Rect& rc, const FunnelState& st, const Metrics& mx)
{
    widgets::Funnel w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int FunnelStageAt(const Rect& rc, const FunnelState& st, const Metrics& mx, Point pt)
{
    widgets::Funnel w;
    w.SetBounds(rc);
    w.state = st;
    return w.StageAt(mx, pt);
}

void PaintDiff(IPainter& g, const Rect& rc, const DiffState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Diff w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

DiffLayout MeasureDiff(const Rect& rc, const DiffState& st, const Metrics& mx)
{
    widgets::Diff w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int DiffLineAt(const Rect& rc, const DiffState& st, const Metrics& mx, Point pt)
{
    widgets::Diff w;
    w.SetBounds(rc);
    w.state = st;
    return w.LineAt(mx, pt);
}

void PaintScore(IPainter& g, const Rect& rc, const ScoreState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Score w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

ScoreLayout MeasureScore(const Rect& rc, const ScoreState& st, const Metrics& mx)
{
    widgets::Score w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int ScoreBarAt(const Rect& rc, const ScoreState& st, const Metrics& mx, Point pt)
{
    widgets::Score w;
    w.SetBounds(rc);
    w.state = st;
    return w.BarAt(mx, pt);
}

void PaintChat(IPainter& g, const Rect& rc, const ChatState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Chat w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

ChatLayout MeasureChat(const Rect& rc, const ChatState& st, const Metrics& mx)
{
    widgets::Chat w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int ChatMessageAt(const Rect& rc, const ChatState& st, const Metrics& mx, Point pt)
{
    widgets::Chat w;
    w.SetBounds(rc);
    w.state = st;
    return w.MessageAt(mx, pt);
}

void PaintMultiStatusBar(IPainter& g, const Rect& rc, const MultiStatusBarState& st, const Palette& pal, const Metrics& mx)
{
    widgets::MultiStatusBar w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

MultiStatusBarLayout MeasureMultiStatusBar(const Rect& rc, const MultiStatusBarState& st, const Metrics& mx)
{
    widgets::MultiStatusBar w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

MultiStatusBarHit HitMultiStatusBar(const Rect& rc, const MultiStatusBarState& st, const Metrics& mx, Point pt)
{
    widgets::MultiStatusBar w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintRibbon(IPainter& g, const Rect& rc, const RibbonState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Ribbon w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

RibbonLayout MeasureRibbon(const Rect& rc, const RibbonState& st, const Metrics& mx)
{
    widgets::Ribbon w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

RibbonHit HitRibbon(const Rect& rc, const RibbonState& st, const Metrics& mx, Point pt)
{
    widgets::Ribbon w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintNoticeCenter(IPainter& g, const Rect& rc, const NoticeCenterState& st, const Palette& pal, const Metrics& mx)
{
    widgets::NoticeCenter w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

NoticeCenterLayout MeasureNoticeCenter(const Rect& rc, const NoticeCenterState& st, const Metrics& mx)
{
    widgets::NoticeCenter w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

NoticeCenterHit HitNoticeCenter(const Rect& rc, const NoticeCenterState& st, const Metrics& mx, Point pt)
{
    widgets::NoticeCenter w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintSparkline(IPainter& g, const Rect& rc, const SparklineState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Sparkline w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

SparklineLayout MeasureSparkline(const Rect& rc, const SparklineState& st, const Metrics& mx)
{
    widgets::Sparkline w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int SparklinePointAt(const Rect& rc, const SparklineState& st, const Metrics& mx, Point pt)
{
    widgets::Sparkline w;
    w.SetBounds(rc);
    w.state = st;
    return w.PointAt(mx, pt);
}

void PaintWaterfall(IPainter& g, const Rect& rc, const WaterfallState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Waterfall w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

WaterfallLayout MeasureWaterfall(const Rect& rc, const WaterfallState& st, const Metrics& mx)
{
    widgets::Waterfall w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int WaterfallBarAt(const Rect& rc, const WaterfallState& st, const Metrics& mx, Point pt)
{
    widgets::Waterfall w;
    w.SetBounds(rc);
    w.state = st;
    return w.BarAt(mx, pt);
}

void PaintStackedBar(IPainter& g, const Rect& rc, const StackedBarState& st, const Palette& pal, const Metrics& mx)
{
    widgets::StackedBar w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

StackedBarLayout MeasureStackedBar(const Rect& rc, const StackedBarState& st, const Metrics& mx)
{
    widgets::StackedBar w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

StackedBarHit HitStackedBar(const Rect& rc, const StackedBarState& st, const Metrics& mx, Point pt)
{
    widgets::StackedBar w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintGauge(IPainter& g, const Rect& rc, const GaugeState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Gauge w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

GaugeLayout MeasureGauge(const Rect& rc, const GaugeState& st, const Metrics& mx)
{
    widgets::Gauge w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int GaugeSegmentAt(const Rect& rc, const GaugeState& st, const Metrics& mx, Point pt)
{
    widgets::Gauge w;
    w.SetBounds(rc);
    w.state = st;
    return w.SegmentAt(mx, pt);
}

void PaintJsonTree(IPainter& g, const Rect& rc, const JsonTreeState& st, const Palette& pal, const Metrics& mx)
{
    widgets::JsonTree w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

JsonTreeLayout MeasureJsonTree(const Rect& rc, const JsonTreeState& st, const Metrics& mx)
{
    widgets::JsonTree w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

JsonTreeHit HitJsonTree(const Rect& rc, const JsonTreeState& st, const Metrics& mx, Point pt)
{
    widgets::JsonTree w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintLogViewer(IPainter& g, const Rect& rc, const LogViewerState& st, const Palette& pal, const Metrics& mx)
{
    widgets::LogViewer w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

LogViewerLayout MeasureLogViewer(const Rect& rc, const LogViewerState& st, const Metrics& mx)
{
    widgets::LogViewer w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int LogLineAt(const Rect& rc, const LogViewerState& st, const Metrics& mx, Point pt)
{
    widgets::LogViewer w;
    w.SetBounds(rc);
    w.state = st;
    return w.LogLineAt(mx, pt);
}

void PaintCronEditor(IPainter& g, const Rect& rc, const CronEditorState& st, const Palette& pal, const Metrics& mx)
{
    widgets::CronEditor w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

CronEditorLayout MeasureCronEditor(const Rect& rc, const CronEditorState& st, const Metrics& mx)
{
    widgets::CronEditor w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

CronEditorHit HitCronEditor(const Rect& rc, const CronEditorState& st, const Metrics& mx, Point pt)
{
    widgets::CronEditor w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintMentionInput(IPainter& g, const Rect& rc, const MentionInputState& st, const Palette& pal, const Metrics& mx)
{
    widgets::MentionInput w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

MentionInputLayout MeasureMentionInput(const Rect& rc, const MentionInputState& st, const Metrics& mx)
{
    widgets::MentionInput w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

MentionInputHit HitMentionInput(const Rect& rc, const MentionInputState& st, const Metrics& mx, Point pt)
{
    widgets::MentionInput w;
    w.SetBounds(rc);
    w.state = st;
    return w.HitAt(mx, pt);
}

void PaintOrgChart(IPainter& g, const Rect& rc, const OrgChartState& st, const Palette& pal, const Metrics& mx)
{
    widgets::OrgChart w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

OrgChartLayout MeasureOrgChart(const Rect& rc, const OrgChartState& st, const Metrics& mx)
{
    widgets::OrgChart w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int OrgNodeAt(const Rect& rc, const OrgChartState& st, const Metrics& mx, Point pt)
{
    widgets::OrgChart w;
    w.SetBounds(rc);
    w.state = st;
    return w.OrgNodeAt(mx, pt);
}

void PaintLeaderboard(IPainter& g, const Rect& rc, const LeaderboardState& st, const Palette& pal, const Metrics& mx)
{
    widgets::Leaderboard w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

LeaderboardLayout MeasureLeaderboard(const Rect& rc, const LeaderboardState& st, const Metrics& mx)
{
    widgets::Leaderboard w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int LeaderRowAt(const Rect& rc, const LeaderboardState& st, const Metrics& mx, Point pt)
{
    widgets::Leaderboard w;
    w.SetBounds(rc);
    w.state = st;
    return w.LeaderRowAt(mx, pt);
}

void PaintActivityFeed(IPainter& g, const Rect& rc, const ActivityFeedState& st, const Palette& pal, const Metrics& mx)
{
    widgets::ActivityFeed w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

ActivityFeedLayout MeasureActivityFeed(const Rect& rc, const ActivityFeedState& st, const Metrics& mx)
{
    widgets::ActivityFeed w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int ActivityItemAt(const Rect& rc, const ActivityFeedState& st, const Metrics& mx, Point pt)
{
    widgets::ActivityFeed w;
    w.SetBounds(rc);
    w.state = st;
    return w.ActivityItemAt(mx, pt);
}

void PaintWaveform(IPainter& g, const Rect& rc, const AudioWaveformState& st, const Palette& pal, const Metrics& mx)
{
    widgets::AudioWaveform w;
    w.SetBounds(rc);
    w.state = st;
    w.Paint(g, pal, mx);
}

WaveformLayout MeasureWaveform(const Rect& rc, const AudioWaveformState& st, const Metrics& mx)
{
    widgets::AudioWaveform w;
    w.SetBounds(rc);
    w.state = st;
    return w.Measure(mx);
}

int WaveformPositionAt(const Rect& rc, const AudioWaveformState& st, const Metrics& mx, Point pt)
{
    widgets::AudioWaveform w;
    w.SetBounds(rc);
    w.state = st;
    return w.WaveformPositionAt(mx, pt);
}

bool WaveformPlayButtonAt(const Rect& rc, const AudioWaveformState& st, const Metrics& mx, Point pt)
{
    widgets::AudioWaveform w;
    w.SetBounds(rc);
    w.state = st;
    return w.WaveformPlayButtonAt(mx, pt);
}

} // namespace ui
} // namespace air
