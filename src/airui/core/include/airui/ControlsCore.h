#pragma once
// ============================================================
// AirUI - ControlsCore.h
// Self-drawn control visuals, platform free.
//
// These are the widgets that were already pure Canvas painting
// (no native handles): Button, Checkbox and Radio. The state machine lives
// here; a backend only implements IPainter.
//
//   ButtonState st;
//   st.hover = true;
//   PaintButton(painter, rc, L"Save", ButtonStyle::Primary,
//               st, Pal(), Mx());
//
// Keeping the drawing here means the Win32 backend and any future
// Skia/X11 backend produce identical pixels from the same code.
// ============================================================
#include "airui/types.h"
#include "airui/hal.h"
#include "airui/Theme.h"

#include <string>
#include <vector>

namespace air {
namespace ui {

// The four button variants. Named ButtonStyle in core; the Win32
// backend keeps the historical alias BtnStyle for source compat.
enum class ButtonStyle
{
    Primary,    // 主色填充 + 白字
    Secondary,  // 白底 + 边框 + 深字
    Danger,     // 白底 + 红边 + 红字
    Ghost       // 透明底 + 主色字
};

// Shared interaction state for the self-drawn controls.
struct ControlState
{
    bool hover    = false;
    bool pressed  = false;
    bool focused  = false;
    bool checked  = false;
    bool disabled = false;
};

// ------------------------------------------------------------
// PaintButton - identical layout to the original DrawButton:
// centred single line, no mnemonic prefix, radius from Mx().
// ------------------------------------------------------------
void PaintButton(IPainter& g,
                 const Rect& rc,
                 const std::wstring& text,
                 ButtonStyle style,
                 const ControlState& st,
                 const Palette& pal,
                 const Metrics& mx);

// ------------------------------------------------------------
// PaintCheckbox - 16x16 rounded box at rc.top + 4, label at
// rc.left + 24. Geometry intentionally matches the gallery so the
// migration is pixel-for-pixel.
// ------------------------------------------------------------
void PaintCheckbox(IPainter& g,
                   const Rect& rc,
                   const std::wstring& text,
                   const ControlState& st,
                   const Palette& pal);

// ------------------------------------------------------------
// PaintRadio - 16x16 circle, 8x8 inner dot when checked.
// ------------------------------------------------------------
void PaintRadio(IPainter& g,
                const Rect& rc,
                const std::wstring& text,
                const ControlState& st,
                const Palette& pal);

// ------------------------------------------------------------
// PaintProgressBar - horizontal bar with normal/disabled states.
// ------------------------------------------------------------
void PaintProgressBar(IPainter& g,
                      const Rect& rc,
                      float progress,
                      const ControlState& st,
                      const Palette& pal,
                      const Metrics& mx);

// ------------------------------------------------------------
// PaintSlider - horizontal slider with thumb, hover/press/disabled.
// ------------------------------------------------------------
struct SliderState
{
    ControlState base = {};
    int value = 50;
    int min = 0;
    int max = 100;
};

void PaintSlider(IPainter& g,
                 const Rect& rc,
                 const SliderState& st,
                 const Palette& pal,
                 const Metrics& mx);

// ------------------------------------------------------------
// PaintToggleSwitch - on/off switch with hover/pressed/disabled.
// ------------------------------------------------------------
struct ToggleState
{
    ControlState base = {};
    bool on = false;
};

void PaintToggleSwitch(IPainter& g,
                       const Rect& rc,
                       const ToggleState& st,
                       const Palette& pal,
                       const Metrics& mx);

// ------------------------------------------------------------
// PaintTextField - visual text input with placeholder, focus/disabled.
// ------------------------------------------------------------
struct TextFieldState
{
    ControlState base = {};
    std::wstring text;
    std::wstring placeholder = L"Placeholder";
    bool hasFocus = false;
};

void PaintTextField(IPainter& g,
                    const Rect& rc,
                    const TextFieldState& st,
                    const Palette& pal,
                    const Metrics& mx);

// ------------------------------------------------------------
// PaintSegmentedControl - segmented choice control.
// ------------------------------------------------------------
struct SegmentedState
{
    ControlState base = {};
    std::vector<std::wstring> items;
    int selected = 0;
};

void PaintSegmented(IPainter& g,
                    const Rect& rc,
                    const SegmentedState& st,
                    const Palette& pal,
                    const Metrics& mx);

// ------------------------------------------------------------
// PaintComboBox - visual combobox with arrow.
// ------------------------------------------------------------
struct ComboBoxState
{
    ControlState base = {};
    std::wstring text;
    bool open = false;
};

void PaintComboBox(IPainter& g,
                   const Rect& rc,
                   const ComboBoxState& st,
                   const Palette& pal,
                   const Metrics& mx);

// ------------------------------------------------------------
// PaintTag - chip/tag pill.
// ------------------------------------------------------------
struct TagState
{
    ControlState base = {};
    std::wstring text;
    bool selected = false;
};

void PaintTag(IPainter& g,
              const Rect& rc,
              const TagState& st,
              const Palette& pal,
              const Metrics& mx);

// ------------------------------------------------------------
// PaintRating - star rating.
// ------------------------------------------------------------
struct RatingState
{
    ControlState base = {};
    int rating = 3;   // 0-5
    int max = 5;
};

void PaintRating(IPainter& g,
                 const Rect& rc,
                 const RatingState& st,
                 const Palette& pal,
                 const Metrics& mx);

// ------------------------------------------------------------
// PaintStepper - numeric stepper with +/- buttons.
// ------------------------------------------------------------
struct StepperState
{
    ControlState base = {};
    int value = 0;
    int min = 0;
    int max = 100;
    bool minusHover = false;
    bool plusHover = false;
};

void PaintStepper(IPainter& g,
                  const Rect& rc,
                  const StepperState& st,
                  const Palette& pal,
                  const Metrics& mx);

// ------------------------------------------------------------
// PaintProgressRing - circular progress.
// ------------------------------------------------------------
struct ProgressRingState
{
    ControlState base = {};
    float progress = 0.5f; // 0-1
};

void PaintProgressRing(IPainter& g,
                       const Rect& rc,
                       const ProgressRingState& st,
                       const Palette& pal,
                       const Metrics& mx);

// ------------------------------------------------------------
// PaintBadge - small badge with count.
// ------------------------------------------------------------
struct BadgeState
{
    ControlState base = {};
    std::wstring text = L"99+";
};

void PaintBadge(IPainter& g,
                const Rect& rc,
                const BadgeState& st,
                const Palette& pal,
                const Metrics& mx);

// ------------------------------------------------------------
// PaintAlert - callout banner: left accent bar + icon + text.
// ------------------------------------------------------------
enum class AlertKind { Info, Success, Warning, Error };

struct AlertState
{
    ControlState base = {};
    AlertKind kind = AlertKind::Info;
    std::wstring title;
    std::wstring message;
};

void PaintAlert(IPainter& g,
                const Rect& rc,
                const AlertState& st,
                const Palette& pal,
                const Metrics& mx);

// ------------------------------------------------------------
// PaintTabs - tab strip with active underline.
// ------------------------------------------------------------
struct TabsState
{
    ControlState base = {};
    std::vector<std::wstring> tabs;
    int selected = 0;
    int hovered  = -1;
};

void PaintTabs(IPainter& g,
               const Rect& rc,
               const TabsState& st,
               const Palette& pal,
               const Metrics& mx);

// ------------------------------------------------------------
// PaintAvatar - circular avatar with initials.
// ------------------------------------------------------------
struct AvatarState
{
    ControlState base = {};
    std::wstring text = L"A";
};

void PaintAvatar(IPainter& g,
                 const Rect& rc,
                 const AvatarState& st,
                 const Palette& pal,
                 const Metrics& mx);

// ------------------------------------------------------------
// PaintSkeleton - loading placeholder bars.
// ------------------------------------------------------------
struct SkeletonState
{
    ControlState base = {};
    int lines = 3;
};

void PaintSkeleton(IPainter& g,
                   const Rect& rc,
                   const SkeletonState& st,
                   const Palette& pal,
                   const Metrics& mx);

// ============================================================
// Data & navigation controls (Phase 1)
//
// Table / List / Tree / Breadcrumb / Pagination / Toast all keep
// their geometry in the core, so a backend only has to forward
// pointer events and call the matching Paint* function.  The
// Measure* / *Hit helpers are pure geometry: no platform API,
// no hidden state, identical results on every backend.
// ============================================================

// ------------------------------------------------------------
// PaintTable - titled, column-based data grid.
//
// Columns are evenly spaced unless st.widths supplies relative
// weights. Rows are clipped to rc; the header, zebra striping,
// hover / selection and the sort indicator all come from state.
// ------------------------------------------------------------
struct TableState
{
    ControlState base = {};
    std::wstring title = L"Table";
    bool showTitle = true;
    std::vector<std::wstring> columns;
    std::vector<int> widths;                       // optional relative weights
    std::vector<std::vector<std::wstring>> rows;
    int  selectedRow = -1;
    int  hoveredRow  = -1;
    int  sortColumn  = -1;
    bool sortAscending = true;
    bool striped = true;
};

struct TableLayout
{
    int titleH      = 0;
    int headerTop   = 0;   // absolute y
    int headerH     = 0;
    int rowTop      = 0;   // absolute y of the first data row
    int rowH        = 0;
    int visibleRows = 0;
    std::vector<Rect> columns;                    // x-ranges of each column
};

TableLayout MeasureTable(const Rect& rc, const TableState& st, const Metrics& mx);

// >= 0: data row index, -1: header, -2: outside the table body.
int TableRowAt(const Rect& rc, const TableState& st, const Metrics& mx, Point pt);

// >= 0: column index, -1: outside the column band.
int TableColumnAt(const Rect& rc, const TableState& st, const Metrics& mx, Point pt);

void PaintTable(IPainter& g,
                const Rect& rc,
                const TableState& st,
                const Palette& pal,
                const Metrics& mx);

// ------------------------------------------------------------
// PaintList - icon / title / subtitle / badge rows.
// ------------------------------------------------------------
struct ListItem
{
    std::wstring title;
    std::wstring subtitle;
    std::wstring badge;
    std::wstring glyph;
    bool disabled = false;
};

struct ListState
{
    ControlState base = {};
    std::vector<ListItem> items;
    int selected = -1;
    int hovered  = -1;
    bool dividers = true;
    std::wstring emptyText = L"暂无数据";
};

// Uniform row height: taller when any item carries a subtitle.
int ListRowHeight(const ListState& st, const Metrics& mx);

// >= 0: item index under the point, -1: nothing.
int ListRowAt(const Rect& rc, const ListState& st, const Metrics& mx, Point pt);

void PaintList(IPainter& g,
               const Rect& rc,
               const ListState& st,
               const Palette& pal,
               const Metrics& mx);

// ------------------------------------------------------------
// PaintTree - hierarchical list with disclosure triangles.
// Nodes are stored in display order; a child is only visible
// while all of its ancestors are expanded.
// ------------------------------------------------------------
struct TreeNode
{
    std::wstring text;
    int  depth = 0;
    bool hasChildren = false;
    bool expanded = false;
    bool disabled = false;
};

struct TreeState
{
    ControlState base = {};
    std::vector<TreeNode> nodes;
    int selected = -1;   // index into nodes
    int hovered  = -1;
};

std::vector<int> TreeVisibleNodes(const TreeState& st);

int TreeRowHeight(const Metrics& mx);

// Both return an index into st.nodes, or -1 when nothing is hit.
int TreeRowAt(const Rect& rc, const TreeState& st, const Metrics& mx, Point pt);
int TreeToggleAt(const Rect& rc, const TreeState& st, const Metrics& mx, Point pt);

void PaintTree(IPainter& g,
               const Rect& rc,
               const TreeState& st,
               const Palette& pal,
               const Metrics& mx);

// ------------------------------------------------------------
// PaintBreadcrumb - equally spaced navigation trail.
// ------------------------------------------------------------
struct BreadcrumbState
{
    ControlState base = {};
    std::vector<std::wstring> items;
    int active  = -1;   // -1 = last item
    int hovered = -1;
};

// >= 0: item index, -1: outside.
int BreadcrumbHit(const Rect& rc, const BreadcrumbState& st,
                  const Metrics& mx, Point pt);

void PaintBreadcrumb(IPainter& g,
                     const Rect& rc,
                     const BreadcrumbState& st,
                     const Palette& pal,
                     const Metrics& mx);

// ------------------------------------------------------------
// PaintPagination - windowed pager.
// Hit codes: -2 previous, -1 next, 0 none, > 0 page number.
// ------------------------------------------------------------
struct PaginationSlot
{
    Rect rect;
    int  page = 0;      // -2 prev, -1 next, 0 ellipsis, > 0 page
};

struct PaginationState
{
    ControlState base = {};
    int page = 1;
    int pageCount = 10;
    int hovered = 0;    // hit code of the hovered slot
};

std::vector<PaginationSlot> MeasurePagination(const Rect& rc,
                                             const PaginationState& st,
                                             const Metrics& mx);
int PaginationHit(const Rect& rc, const PaginationState& st,
                  const Metrics& mx, Point pt);

void PaintPagination(IPainter& g,
                     const Rect& rc,
                     const PaginationState& st,
                     const Palette& pal,
                     const Metrics& mx);

// ------------------------------------------------------------
// PaintToast - floating status card with an optional close box.
// ------------------------------------------------------------
struct ToastState
{
    ControlState base = {};
    AlertKind kind = AlertKind::Info;
    std::wstring title;
    std::wstring message;
    bool closable = true;
    bool hoveredClose = false;
};

bool ToastCloseHit(const Rect& rc, const ToastState& st,
                   const Metrics& mx, Point pt);

void PaintToast(IPainter& g,
                const Rect& rc,
                const ToastState& st,
                const Palette& pal,
                const Metrics& mx);

} // namespace ui
} // namespace air
