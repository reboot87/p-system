#pragma once
// ============================================================
// AirUI - ControlsExtra.h
//
// Phase 1 migration (platform-bound composites moved into core) plus
// Phase 2 new controls. Everything follows the same contract as
// ControlsCore.h:
//
//   *State      - plain data, no handles, no statics
//   Measure*    - pure geometry shared by painting and hit testing
//   *At / *Hit  - pure hit testing, returns -1 / an enum / a struct
//   Paint*      - void PaintXxx(IPainter&, rect, state, pal, mx)
//
// Rules for every implementation file:
//   - only IPainter / Palette / Metrics / types.h / std lib
//   - colors from pal.*, sizes from mx.*, never hardcoded RGB
//   - empty rect => no draw calls at all
//   - all Text() calls carry dt::NoPrefix (and EndEllipsis for long
//     strings); use TextBlock() for wrapping
//   - PushClip/PopClip must be balanced; clip before drawing content
//     that may overflow
//   - no static mutable state, no platform headers
// ============================================================
#include "airui/types.h"
#include "airui/hal.h"
#include "airui/Theme.h"
#include "airui/ControlsCore.h"
#include "airui/FrameworkCore.h"
#include "airui/OverlayCore.h"

#include <string>
#include <utility>
#include <vector>

namespace air {
namespace ui {

// ============================================================
// P1 - migrated platform composites
// ============================================================

// ---- Card (was Components.h Card::Paint) ----
struct CardState
{
    ControlState base = {};
    std::wstring glyph = L"\xE8C8";
    std::wstring title = L"卡片标题";
    std::wstring desc  = L"卡片描述信息";
    std::wstring number;
    std::wstring unit;
    std::wstring badge;
    std::wstring linkText;
    bool badgeOk = true;
    bool hoverable = true;
    // Optional overrides for legacy callers that supplied their own
    // icon tile: 0 means "derive from the theme".
    int   iconSize = 0;
    int   iconRadius = 0;
    Color iconBg = 0;
    Color iconColor = 0;
};

struct CardLayout
{
    Rect iconRc{}, titleRc{}, descRc{}, numberRc{}, unitRc{}, badgeRc{}, linkRc{};
};

CardLayout MeasureCard(const Rect& rc, const CardState& st, const Metrics& mx);

enum class CardHit { None, Body, Link };
CardHit HitCard(const Rect& rc, const CardState& st,
                const Metrics& mx, Point pt);

void PaintCard(IPainter& g, const Rect& rc, const CardState& st,
               const Palette& pal, const Metrics& mx);

// ---- EmptyView (was Canvas::EmptyState) ----
struct EmptyViewState
{
    std::wstring glyph = L"\xE9F5";
    std::wstring title = L"暂无数据";
    std::wstring steps = L"这里还没有任何记录";
    std::wstring foot  = L"试试调整筛选条件";
};

void PaintEmptyView(IPainter& g, const Rect& rc, const EmptyViewState& st,
                    const Palette& pal, const Metrics& mx);

// ---- StatusTag (was Canvas::StatusTag) ----
struct StatusTagState
{
    std::wstring text = L"运行中";
    AlertKind kind = AlertKind::Success;
    bool dot = true;
};

void PaintStatusTag(IPainter& g, const Rect& rc, const StatusTagState& st,
                    const Palette& pal, const Metrics& mx);

// Explicit-colour variant used by legacy platform callers that used
// to pass raw colours. PaintStatusTag() resolves the semantic colors
// and forwards here, so both paths share one drawing implementation.
struct StatusTagColors
{
    Color bg = 0;
    Color border = 0;
    Color text = 0;
    Color dot = 0;
};

void PaintStatusTagColored(IPainter& g, const Rect& rc,
                           const std::wstring& text, bool showDot,
                           const StatusTagColors& colors, const Metrics& mx);

// ---- KbdChip (was Canvas::KbdChip) ----
struct KbdChipState
{
    std::wstring key = L"Ctrl";
    std::wstring label = L"K";
};

int  MeasureKbdChip(const KbdChipState& st, const Metrics& mx);
void PaintKbdChip(IPainter& g, const Rect& rc, const KbdChipState& st,
                  const Palette& pal, const Metrics& mx);

// ---- Toolbar (was Components.h Toolbar::Paint) ----
struct ToolbarActionState
{
    int id = 0;
    std::wstring label;
    std::wstring glyph;
    bool primary = false;
    bool disabled = false;
};

struct ToolbarState
{
    ControlState base = {};
    std::wstring searchText;
    std::wstring searchPlaceholder = L"搜索…";
    std::vector<ToolbarActionState> actions;
    std::vector<std::wstring> crumbs;
    int hoveredAction = -1;
    int hoveredCrumb  = -1;
};

struct ToolbarLayout
{
    std::vector<Rect> crumbs;
    std::vector<Rect> actions;
    Rect search{};
    int  rowH = 0;
};

ToolbarLayout MeasureToolbar(const Rect& rc, const ToolbarState& st,
                             const Metrics& mx);

struct ToolbarHit
{
    int  action = -1;     // index into actions
    int  crumb  = -1;     // index into crumbs
    bool search = false;
};

ToolbarHit HitToolbar(const Rect& rc, const ToolbarState& st,
                      const Metrics& mx, Point pt);

void PaintToolbar(IPainter& g, const Rect& rc, const ToolbarState& st,
                  const Palette& pal, const Metrics& mx);

// ---- Form (was Components.h Form::Arrange / Validate) ----
struct FormField
{
    std::wstring label;
    std::wstring value;
    std::wstring placeholder;
    std::wstring error;
    bool required = false;
    bool invalid  = false;
};

struct FormState
{
    ControlState base = {};
    std::vector<FormField> fields;
    int  labelWidth = 120;
    int  focusedField = -1;
    std::wstring submitText = L"提交";
    bool hoverSubmit = false;
};

struct FormLayout
{
    std::vector<Rect> labels;
    std::vector<Rect> inputs;
    std::vector<Rect> errors;
    Rect submit{};
    int  rowH = 0;
};

FormLayout MeasureForm(const Rect& rc, const FormState& st, const Metrics& mx);

struct FormHit
{
    int  field = -1;
    bool submit = false;
};

FormHit HitForm(const Rect& rc, const FormState& st,
                const Metrics& mx, Point pt);

// Validates required fields and clears/sets invalid flags. Returns the
// index of the first failing field, or -1 when the form is valid.
int ValidateForm(FormState& st);

void PaintForm(IPainter& g, const Rect& rc, const FormState& st,
               const Palette& pal, const Metrics& mx);

// ---- DataGrid (was Components.h DataGrid<T>) ----
struct DataGridColumn
{
    std::wstring header;
    int  weight = 1;
    bool sortable = true;
};

struct DataGridState
{
    ControlState base = {};
    std::vector<DataGridColumn> columns;
    std::vector<std::vector<std::wstring>> rows;
    int  sortColumn = -1;
    bool sortAscending = true;
    int  selected = -1;
    int  hovered  = -1;
    bool striped = true;
    std::wstring filter;
    int  resizingColumn = -1;
};

struct DataGridLayout
{
    int headerTop = 0;
    int headerH   = 0;
    int rowTop    = 0;
    int rowH      = 0;
    int visibleRows = 0;
    std::vector<Rect> columns;
    std::vector<Rect> handles;
};

DataGridLayout MeasureDataGrid(const Rect& rc, const DataGridState& st,
                               const Metrics& mx);

struct DataGridHit
{
    int  row = -1;
    int  column = -1;
    bool header = false;
    int  resize = -1;     // column index whose drag handle was hit
};

DataGridHit HitDataGrid(const Rect& rc, const DataGridState& st,
                        const Metrics& mx, Point pt);

// Applies the text filter and the selected sort to a copy of rows.
std::vector<std::vector<std::wstring>> DataGridVisibleRows(const DataGridState& st);

void PaintDataGrid(IPainter& g, const Rect& rc, const DataGridState& st,
                   const Palette& pal, const Metrics& mx);

// ---- ListData (variable row height + virtual scrolling) ----
struct ListDataRow
{
    std::wstring title;
    std::wstring subtitle;
    std::wstring badge;
    int  height = 0;      // 0 = derive from content
    bool disabled = false;
};

struct ListDataState
{
    ControlState base = {};
    std::vector<ListDataRow> rows;
    int  selected = -1;
    int  hovered  = -1;
    ScrollState scroll;
    std::wstring emptyText = L"暂无数据";
};

int  ListDataRowHeight(const ListDataRow& row, const Metrics& mx);
std::vector<int> ListDataHeights(const ListDataState& st, const Metrics& mx);
int  ListDataContentHeight(const ListDataState& st, const Metrics& mx);

// Returns the absolute row index (including rows scrolled out of view).
int  ListDataRowAt(const Rect& rc, const ListDataState& st,
                   const Metrics& mx, Point pt);

void PaintListData(IPainter& g, const Rect& rc, const ListDataState& st,
                   const Palette& pal, const Metrics& mx);

// ---- Custom (was the STATIC owner-draw placeholder) ----
struct CustomState
{
    ControlState base = {};
    std::wstring label = L"自定义自绘区域";
    int  variant = 0;      // 0 grid, 1 hatch, 2 wave (rect based)
};

void PaintCustom(IPainter& g, const Rect& rc, const CustomState& st,
                 const Palette& pal, const Metrics& mx);

// ============================================================
// P2 - input controls
// ============================================================
struct SearchBoxState
{
    ControlState base = {};
    std::wstring text;
    std::wstring placeholder = L"搜索…";
    bool hasFocus = false;
    bool clearHover = false;
};

enum class SearchBoxHit { None, Body, Clear };
SearchBoxHit HitSearchBox(const Rect& rc, const SearchBoxState& st,
                          const Metrics& mx, Point pt);
void PaintSearchBox(IPainter& g, const Rect& rc, const SearchBoxState& st,
                    const Palette& pal, const Metrics& mx);

struct PasswordBoxState
{
    ControlState base = {};
    std::wstring text;
    std::wstring placeholder = L"请输入密码";
    bool revealed = false;
    bool hasFocus = false;
    bool toggleHover = false;
};

enum class PasswordBoxHit { None, Body, Reveal };
PasswordBoxHit HitPasswordBox(const Rect& rc, const PasswordBoxState& st,
                              const Metrics& mx, Point pt);
void PaintPasswordBox(IPainter& g, const Rect& rc, const PasswordBoxState& st,
                      const Palette& pal, const Metrics& mx);

struct TextAreaState
{
    ControlState base = {};
    std::wstring text;
    std::wstring placeholder = L"请输入多行文本…";
    bool hasFocus = false;
    int  maxLines = 3;
};

void PaintTextArea(IPainter& g, const Rect& rc, const TextAreaState& st,
                   const Palette& pal, const Metrics& mx);

struct NumberInputState
{
    ControlState base = {};
    int value = 0;
    int min = 0;
    int max = 100;
    int step = 1;
    bool minusHover = false;
    bool plusHover  = false;
};

enum class NumberInputHit { None, Minus, Plus, Body };
NumberInputHit HitNumberInput(const Rect& rc, const NumberInputState& st,
                              const Metrics& mx, Point pt);
void PaintNumberInput(IPainter& g, const Rect& rc, const NumberInputState& st,
                      const Palette& pal, const Metrics& mx);

struct DatePickerState
{
    ControlState base = {};
    int year  = 2026;
    int month = 11;        // 1..12
    int day   = 6;
    bool open = true;
    int hoveredDay = -1;
    int hoveredNav = 0;    // 0 none, 1 previous month, 2 next month
};

// Days in the given month, platform-free calendar arithmetic.
int  DaysInMonth(int year, int month);
int  FirstWeekday(int year, int month);   // 0 = Sunday

// 1..31 day, -1 none, -2 previous month, -3 next month.
int  DatePickerDayAt(const Rect& rc, const DatePickerState& st,
                     const Metrics& mx, Point pt);
void PaintDatePicker(IPainter& g, const Rect& rc, const DatePickerState& st,
                     const Palette& pal, const Metrics& mx);

struct ColorPickerState
{
    ControlState base = {};
    std::vector<Color> swatches;
    int selected = 0;
    int hovered  = -1;
    bool open = true;
};

int  ColorPickerSwatchAt(const Rect& rc, const ColorPickerState& st,
                         const Metrics& mx, Point pt);
void PaintColorPicker(IPainter& g, const Rect& rc, const ColorPickerState& st,
                      const Palette& pal, const Metrics& mx);

// ============================================================
// P2 - selection controls
// ============================================================
struct DropdownState
{
    ControlState base = {};
    std::wstring text;
    std::wstring placeholder = L"请选择";
    std::vector<std::wstring> options;
    int  selected = -1;
    int  hovered  = -1;
    bool open = false;
};

struct DropdownLayout
{
    Rect field{};
    Rect list{};
    int  optionH = 0;
};

DropdownLayout MeasureDropdown(const Rect& rc, const DropdownState& st,
                               const Metrics& mx);

// >= 0 option index, -1 field, -2 outside.
int  DropdownOptionAt(const Rect& rc, const DropdownState& st,
                      const Metrics& mx, Point pt);
void PaintDropdown(IPainter& g, const Rect& rc, const DropdownState& st,
                   const Palette& pal, const Metrics& mx);

struct MenuItemState
{
    std::wstring text;
    std::wstring shortcut;
    std::wstring glyph;
    bool disabled = false;
    bool separator = false;
    bool checked = false;
};

struct MenuState
{
    ControlState base = {};
    std::vector<MenuItemState> items;
    int hovered  = -1;
    int selected = -1;
};

int  MenuItemHeight(const MenuItemState& item, const Metrics& mx);
int  MenuContentHeight(const MenuState& st, const Metrics& mx);
int  MenuItemAt(const Rect& rc, const MenuState& st,
                const Metrics& mx, Point pt);
void PaintMenu(IPainter& g, const Rect& rc, const MenuState& st,
               const Palette& pal, const Metrics& mx);

struct ContextMenuState
{
    MenuState menu;
    Point anchor{};
    bool visible = true;
};

void PaintContextMenu(IPainter& g, const Rect& client,
                      const ContextMenuState& st,
                      const Palette& pal, const Metrics& mx);

struct MultiSelectState
{
    ControlState base = {};
    std::vector<std::wstring> options;
    std::vector<bool> checked;
    int hovered = -1;
};

int  MultiSelectAt(const Rect& rc, const MultiSelectState& st,
                   const Metrics& mx, Point pt);
void PaintMultiSelect(IPainter& g, const Rect& rc, const MultiSelectState& st,
                      const Palette& pal, const Metrics& mx);

struct TransferState
{
    ControlState base = {};
    std::vector<std::wstring> left;
    std::vector<std::wstring> right;
    std::vector<bool> leftChecked;
    std::vector<bool> rightChecked;
    int leftHover  = -1;
    int rightHover = -1;
};

struct TransferLayout
{
    Rect leftBox{}, rightBox{};
    Rect toRight{}, toLeft{};
    int  rowH = 0;
};

TransferLayout MeasureTransfer(const Rect& rc, const TransferState& st,
                               const Metrics& mx);

struct TransferHit
{
    int side = -1;      // 0 left, 1 right
    int row  = -1;
    int button = 0;     // 1 move right, 2 move left
};

TransferHit HitTransfer(const Rect& rc, const TransferState& st,
                        const Metrics& mx, Point pt);

void MoveTransferItem(TransferState& st, int side, int row);

void PaintTransfer(IPainter& g, const Rect& rc, const TransferState& st,
                   const Palette& pal, const Metrics& mx);

// ============================================================
// P2 - display controls
// ============================================================
struct TimelineEvent
{
    std::wstring title;
    std::wstring desc;
    std::wstring time;
    AlertKind kind = AlertKind::Info;
};

struct TimelineState
{
    ControlState base = {};
    std::vector<TimelineEvent> events;
};

void PaintTimeline(IPainter& g, const Rect& rc, const TimelineState& st,
                   const Palette& pal, const Metrics& mx);

struct StepsState
{
    ControlState base = {};
    std::vector<std::wstring> steps;
    int current = 1;
    bool failed = false;
    int hovered = -1;
};

int  StepAt(const Rect& rc, const StepsState& st,
            const Metrics& mx, Point pt);
void PaintSteps(IPainter& g, const Rect& rc, const StepsState& st,
                const Palette& pal, const Metrics& mx);

struct CalendarState
{
    ControlState base = {};
    int year = 2026;
    int month = 11;
    int selectedDay = 6;
    int hoveredDay = -1;
    std::vector<int> marked;
    bool showWeekdays = true;
};

struct CalendarLayout
{
    Rect grid{};
    int cellW = 0;
    int cellH = 0;
    int headerH = 0;
};

CalendarLayout MeasureCalendar(const Rect& rc, const CalendarState& st,
                               const Metrics& mx);
int  CalendarDayAt(const Rect& rc, const CalendarState& st,
                   const Metrics& mx, Point pt);
void PaintCalendar(IPainter& g, const Rect& rc, const CalendarState& st,
                   const Palette& pal, const Metrics& mx);

struct DescriptionListState
{
    ControlState base = {};
    std::wstring title;
    std::vector<std::pair<std::wstring, std::wstring>> items;
    int columns = 2;
};

void PaintDescriptionList(IPainter& g, const Rect& rc,
                          const DescriptionListState& st,
                          const Palette& pal, const Metrics& mx);

struct StatCardState
{
    ControlState base = {};
    std::wstring title = L"营收";
    std::wstring value = L"¥ 128,400";
    std::wstring delta = L"+12.5%";
    bool positive = true;
    std::vector<int> bars;
};

struct StatCardLayout
{
    Rect titleRc{}, valueRc{}, deltaRc{}, chartRc{};
    int barW = 0;
    int gap = 0;
};

StatCardLayout MeasureStatCard(const Rect& rc, const StatCardState& st,
                               const Metrics& mx);

enum class StatCardHit { None, Body, Delta };
StatCardHit HitStatCard(const Rect& rc, const StatCardState& st,
                        const Metrics& mx, Point pt);
void PaintStatCard(IPainter& g, const Rect& rc, const StatCardState& st,
                   const Palette& pal, const Metrics& mx);

enum class ChartKind { Bar, Stacked, Line };
struct ChartState
{
    ControlState base = {};
    ChartKind kind = ChartKind::Bar;
    std::wstring title;
    std::vector<int> values;
    std::vector<std::wstring> labels;
    int hovered = -1;
};

int  ChartBarAt(const Rect& rc, const ChartState& st,
                const Metrics& mx, Point pt);
void PaintChart(IPainter& g, const Rect& rc, const ChartState& st,
                const Palette& pal, const Metrics& mx);

struct CarouselState
{
    ControlState base = {};
    std::vector<std::wstring> slides;
    int current = 0;
    int hovered = -1;
};

struct CarouselHitResult
{
    int code = 0;      // 0 none, 1 previous, 2 next, 3 dot
    int index = -1;    // dot index when code == 3
};

CarouselHitResult HitCarousel(const Rect& rc, const CarouselState& st,
                              const Metrics& mx, Point pt);
void PaintCarousel(IPainter& g, const Rect& rc, const CarouselState& st,
                   const Palette& pal, const Metrics& mx);

// ============================================================
// P2 - feedback / overlay controls
// ============================================================
struct SpinnerState
{
    ControlState base = {};
    float phase = 0;               // 0..1 from SpinnerPhase(nowMs)
    int  dots = 8;
    std::wstring label = L"加载中…";
};

void PaintSpinner(IPainter& g, const Rect& rc, const SpinnerState& st,
                  const Palette& pal, const Metrics& mx);

struct TooltipState
{
    std::wstring text = L"提示信息";
    OverlayPlacement placement = OverlayPlacement::Below;
};

// Both tooltips/popovers place themselves next to anchor inside client.
void PaintTooltip(IPainter& g, const Rect& anchor, const Rect& client,
                  const TooltipState& st, const Palette& pal, const Metrics& mx);

struct PopoverState
{
    std::wstring title = L"标题";
    std::wstring body  = L"这里是弹出层的说明内容。";
};

void PaintPopover(IPainter& g, const Rect& anchor, const Rect& client,
                  const PopoverState& st, const Palette& pal, const Metrics& mx);

struct ModalState
{
    std::wstring title = L"对话框标题";
    std::wstring body  = L"确定要执行这个操作吗？";
    std::wstring confirmText = L"确定";
    std::wstring cancelText  = L"取消";
    int  focused = 0;      // 0 none, 1 confirm, 2 cancel (keyboard focus)
    int  hovered = 0;      // pointer target; mirrors focused for highlighting
    int  pressed = 0;      // button held down (visual only, action on release)
    bool singleButton = false;  // one centred action, no cancel slot
    bool scrim = true;
};

struct ModalLayout
{
    Rect card{}, titleRc{}, bodyRc{}, confirmRc{}, cancelRc{};
};

ModalLayout MeasureModal(const Rect& client, const ModalState& st,
                         const Metrics& mx);
int  ModalButtonAt(const Rect& client, const ModalState& st,
                   const Metrics& mx, Point pt);   // 0 none, 1 confirm, 2 cancel
void PaintModal(IPainter& g, const Rect& client, const ModalState& st,
                const Palette& pal, const Metrics& mx);

struct DrawerState
{
    std::wstring title = L"抽屉标题";
    std::wstring body  = L"抽屉内容区域。";
    int  width = 320;
    bool right = true;
    bool scrim = true;
};

struct DrawerLayout
{
    Rect panel{}, titleRc{}, bodyRc{}, closeRc{};
};

DrawerLayout MeasureDrawer(const Rect& client, const DrawerState& st,
                           const Metrics& mx);
bool DrawerCloseHit(const Rect& client, const DrawerState& st,
                    const Metrics& mx, Point pt);
void PaintDrawer(IPainter& g, const Rect& client, const DrawerState& st,
                 const Palette& pal, const Metrics& mx);

struct NotificationState
{
    std::vector<ToastState> items;
    int maxVisible = 3;
};

void PaintNotificationQueue(IPainter& g, const Rect& client,
                            const NotificationState& st,
                            const Palette& pal, const Metrics& mx);

struct ResultState
{
    AlertKind kind = AlertKind::Success;
    std::wstring title = L"操作成功";
    std::wstring detail = L"提交已完成，可返回列表继续操作。";
    std::wstring actionText = L"返回首页";
    bool hoverAction = false;
};

bool ResultActionHit(const Rect& rc, const ResultState& st,
                     const Metrics& mx, Point pt);
void PaintResult(IPainter& g, const Rect& rc, const ResultState& st,
                 const Palette& pal, const Metrics& mx);

// ============================================================
// P2 - layout controls
// ============================================================
struct AccordionState
{
    ControlState base = {};
    std::vector<std::pair<std::wstring, std::wstring>> panels;
    std::vector<bool> expanded;
    int hovered = -1;
};

// >= 0 header index, -1 outside.
int  AccordionHeaderAt(const Rect& rc, const AccordionState& st,
                       const Metrics& mx, Point pt);
int  AccordionContentHeight(const AccordionState& st, const Metrics& mx);
void PaintAccordion(IPainter& g, const Rect& rc, const AccordionState& st,
                    const Palette& pal, const Metrics& mx);

struct AnchorState
{
    ControlState base = {};
    std::vector<std::wstring> items;
    int active = 0;
    int hovered = -1;
};

int  AnchorHit(const Rect& rc, const AnchorState& st,
               const Metrics& mx, Point pt);
void PaintAnchor(IPainter& g, const Rect& rc, const AnchorState& st,
                 const Palette& pal, const Metrics& mx);

struct SplitterState
{
    bool vertical = true;   // true = left/right split
    int  position = 0;      // 0 = middle
    bool dragging = false;
    bool hovered  = false;
};

struct SplitterLayout
{
    Rect first{}, handle{}, second{};
};

SplitterLayout MeasureSplitter(const Rect& rc, const SplitterState& st,
                               const Metrics& mx);
bool SplitterHandleHit(const Rect& rc, const SplitterState& st,
                       const Metrics& mx, Point pt);
void PaintSplitter(IPainter& g, const Rect& rc, const SplitterState& st,
                   const Palette& pal, const Metrics& mx);

struct ColumnResizeState
{
    std::vector<int> widths;   // relative weights or pixel widths
    int dragging = -1;
    int hovered  = -1;
};

struct ColumnResizeLayout
{
    std::vector<Rect> columns;
    std::vector<Rect> handles;
};

ColumnResizeLayout MeasureColumnResize(const Rect& rc,
                                       const ColumnResizeState& st,
                                       const Metrics& mx);
int  ColumnResizeHandleAt(const Rect& rc, const ColumnResizeState& st,
                          const Metrics& mx, Point pt);
void PaintColumnResize(IPainter& g, const Rect& rc,
                       const ColumnResizeState& st,
                       const Palette& pal, const Metrics& mx);

} // namespace ui
} // namespace air
