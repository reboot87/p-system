#pragma once
// ============================================================
// AirUI - ControlsAdvanced.h
//
// Third wave of controls. Same contract as ControlsCore.h and
// ControlsExtra.h:
//
//   *State      - plain data, owned by the caller, no handles
//   Measure*    - pure geometry shared by painting and hit testing
//   *At / *Hit  - pure hit testing
//   Paint*      - void PaintXxx(IPainter&, rect, state, pal, mx)
//
// Implementation rules (identical to the other core files):
//   - only IPainter / Palette / Metrics / types.h / std lib
//   - colors come from pal.*, sizes from mx.*, no hardcoded RGB
//   - empty rect => zero draw calls
//   - every Text() carries dt::NoPrefix; long strings also
//     dt::EndEllipsis; TextBlock() for wrapped content
//   - PushClip/PopClip balanced; clip anything that may overflow
//   - no static mutable state, no platform headers
// ============================================================
#include "airui/types.h"
#include "airui/hal.h"
#include "airui/Theme.h"
#include "airui/ControlsCore.h"

#include <string>
#include <vector>

namespace air {
namespace ui {

// ============================================================
// P3 - input / selection
// ============================================================

// ---- TagInput: editable chips with a trailing text caret ----
struct TagInputState
{
    ControlState base = {};
    std::vector<std::wstring> tags;
    std::wstring draft;
    std::wstring placeholder = L"输入后回车添加标签";
    bool hasFocus = false;
    int  hoveredTag = -1;
    bool clearAllHover = false;
};

struct TagChipRect
{
    Rect chip{};
    Rect close{};      // the small x button inside chip
};

struct TagInputLayout
{
    std::vector<TagChipRect> chips;
    Rect input{};      // remaining single-line text area
    Rect clearAll{};   // empty when there is no tag or control is disabled
    bool overflow = false;   // some chips did not fit and are clipped
};

TagInputLayout MeasureTagInput(const Rect& rc, const TagInputState& st,
                               const Metrics& mx);

struct TagInputHit
{
    int  tag = -1;         // index into tags
    bool close = false;    // the x button of that tag
    bool clearAll = false;
    bool input = false;
};

TagInputHit HitTagInput(const Rect& rc, const TagInputState& st,
                        const Metrics& mx, Point pt);
void PaintTagInput(IPainter& g, const Rect& rc, const TagInputState& st,
                   const Palette& pal, const Metrics& mx);

// ---- RangeSlider: two thumbs over one track ----
struct RangeSliderState
{
    ControlState base = {};
    int low = 20;
    int high = 80;
    int min = 0;
    int max = 100;
    int step = 1;
    int hoveredThumb = 0;    // 0 none, 1 low, 2 high
    int draggingThumb = 0;
    bool showTicks = true;
};

struct RangeSliderLayout
{
    Rect track{}, fill{}, lowThumb{}, highThumb{};
};

RangeSliderLayout MeasureRangeSlider(const Rect& rc, const RangeSliderState& st,
                                     const Metrics& mx);

// 1 low thumb, 2 high thumb, -1 track, -2 outside.
int RangeSliderThumbAt(const Rect& rc, const RangeSliderState& st,
                       const Metrics& mx, Point pt);

// Value under a point, clamped to [min, max] and snapped to step.
int RangeSliderValueAt(const Rect& rc, const RangeSliderState& st,
                       const Metrics& mx, Point pt);

void PaintRangeSlider(IPainter& g, const Rect& rc, const RangeSliderState& st,
                      const Palette& pal, const Metrics& mx);

// ---- OtpInput: fixed-length code cells ----
struct OtpInputState
{
    ControlState base = {};
    std::wstring code = L"123";
    int length = 6;
    int focused = 3;         // cell index with the caret
    int hovered = -1;
    bool masked = false;
};

struct OtpInputLayout
{
    std::vector<Rect> cells;
    int cellW = 0;
    int gap = 0;
};

OtpInputLayout MeasureOtpInput(const Rect& rc, const OtpInputState& st,
                               const Metrics& mx);
int OtpCellAt(const Rect& rc, const OtpInputState& st,
              const Metrics& mx, Point pt);
void PaintOtpInput(IPainter& g, const Rect& rc, const OtpInputState& st,
                   const Palette& pal, const Metrics& mx);

// ---- Cascader: linked multi-column selector ----
struct CascaderColumn
{
    std::wstring title;
    std::vector<std::wstring> items;
};

struct CascaderState
{
    ControlState base = {};
    std::vector<CascaderColumn> columns;
    std::vector<int> selected;      // one index per column, -1 = none
    int hoveredColumn = -1;
    int hoveredRow = -1;
};

struct CascaderLayout
{
    std::vector<Rect> columnRects;
    int rowH = 0;
    int headerH = 0;
};

struct CascaderHit
{
    int column = -1;
    int row = -1;
};

CascaderLayout MeasureCascader(const Rect& rc, const CascaderState& st,
                               const Metrics& mx);
CascaderHit HitCascader(const Rect& rc, const CascaderState& st,
                        const Metrics& mx, Point pt);
void PaintCascader(IPainter& g, const Rect& rc, const CascaderState& st,
                   const Palette& pal, const Metrics& mx);

// ============================================================
// P3 - data / display
// ============================================================

// ---- AvatarGroup: overlapping avatars plus a +N bubble ----
struct AvatarItem
{
    std::wstring text = L"A";
    Color tint = 0;      // 0 = derive a semantic color from the palette
};

struct AvatarGroupState
{
    ControlState base = {};
    std::vector<AvatarItem> items;
    int maxVisible = 4;
    int size = 0;        // 0 = derive from metrics
    int hovered = -1;
};

struct AvatarGroupLayout
{
    std::vector<Rect> cells;
    Rect more{};         // empty when every avatar is shown
    int diameter = 0;
};

AvatarGroupLayout MeasureAvatarGroup(const Rect& rc, const AvatarGroupState& st,
                                    const Metrics& mx);
// >= 0 avatar index, -1 none, -2 the +N bubble.
int AvatarGroupAt(const Rect& rc, const AvatarGroupState& st,
                  const Metrics& mx, Point pt);
void PaintAvatarGroup(IPainter& g, const Rect& rc, const AvatarGroupState& st,
                      const Palette& pal, const Metrics& mx);

// ---- Heatmap: weeks x 7 contribution grid ----
struct HeatmapState
{
    ControlState base = {};
    int weeks = 20;
    std::vector<int> values;     // weeks * 7, each 0..4; short => 0
    std::wstring title = L"活跃度";
    int hoveredWeek = -1;
    int hoveredDay = -1;
    bool showLegend = true;
};

struct HeatmapLayout
{
    Rect grid{};
    Rect legend{};
    int cell = 0;
    int gap = 0;
};

struct HeatmapCell
{
    int week = -1;
    int day = -1;
};

HeatmapLayout MeasureHeatmap(const Rect& rc, const HeatmapState& st,
                             const Metrics& mx);
HeatmapCell HeatmapCellAt(const Rect& rc, const HeatmapState& st,
                          const Metrics& mx, Point pt);
void PaintHeatmap(IPainter& g, const Rect& rc, const HeatmapState& st,
                  const Palette& pal, const Metrics& mx);

// ---- CodeBlock: titled code panel with line numbers and copy ----
struct CodeBlockState
{
    ControlState base = {};
    std::wstring title = L"example.cpp";
    std::vector<std::wstring> lines;
    int  highlightLine = -1;
    bool lineNumbers = true;
    bool copyHover = false;
};

struct CodeBlockLayout
{
    Rect header{}, body{}, copy{};
    int lineH = 0;
};

CodeBlockLayout MeasureCodeBlock(const Rect& rc, const CodeBlockState& st,
                                 const Metrics& mx);
bool CodeBlockCopyHit(const Rect& rc, const CodeBlockState& st,
                      const Metrics& mx, Point pt);
void PaintCodeBlock(IPainter& g, const Rect& rc, const CodeBlockState& st,
                    const Palette& pal, const Metrics& mx);

// ---- KanbanBoard: columns of stacked cards ----
struct KanbanCard
{
    std::wstring title;
    std::wstring tag;
    AlertKind kind = AlertKind::Info;
};

struct KanbanColumn
{
    std::wstring title;
    std::vector<KanbanCard> cards;
};

struct KanbanState
{
    ControlState base = {};
    std::vector<KanbanColumn> columns;
    int hoveredColumn = -1;
    int hoveredCard = -1;
};

struct KanbanLayout
{
    std::vector<Rect> columnRects;
    std::vector<std::vector<Rect>> cardRects;
    int cardH = 0;
};

struct KanbanHit
{
    int column = -1;
    int card = -1;
};

KanbanLayout MeasureKanban(const Rect& rc, const KanbanState& st,
                           const Metrics& mx);
KanbanHit HitKanban(const Rect& rc, const KanbanState& st,
                    const Metrics& mx, Point pt);
void PaintKanban(IPainter& g, const Rect& rc, const KanbanState& st,
                 const Palette& pal, const Metrics& mx);

// ---- GanttChart: day header plus task bars ----
struct GanttTask
{
    std::wstring name;
    int start = 0;         // day index, 0-based
    int duration = 1;
    int progress = 0;      // 0..100
    AlertKind kind = AlertKind::Info;
};

struct GanttState
{
    ControlState base = {};
    std::vector<GanttTask> tasks;
    int days = 14;
    int today = 5;
    bool showToday = true;
    int hoveredTask = -1;
};

struct GanttLayout
{
    Rect header{}, rows{}, labelArea{};
    std::vector<Rect> bars;
    int rowH = 0;
    int dayW = 0;
    int labelW = 0;
};

GanttLayout MeasureGantt(const Rect& rc, const GanttState& st,
                         const Metrics& mx);
int GanttTaskAt(const Rect& rc, const GanttState& st,
                const Metrics& mx, Point pt);
void PaintGantt(IPainter& g, const Rect& rc, const GanttState& st,
                const Palette& pal, const Metrics& mx);

// ============================================================
// P3 - feedback / navigation
// ============================================================

// ---- Snackbar: bottom action bar (Toast with an action button) ----
struct SnackbarState
{
    std::wstring message = L"已删除 1 条记录";
    std::wstring action = L"撤销";
    AlertKind kind = AlertKind::Info;
    bool closable = true;
    bool actionHover = false;
    bool closeHover = false;
};

struct SnackbarLayout
{
    Rect card{}, textRc{}, actionRc{}, closeRc{};
};

SnackbarLayout MeasureSnackbar(const Rect& client, const SnackbarState& st,
                               const Metrics& mx);
enum class SnackbarHit { None, Action, Close };
SnackbarHit HitSnackbar(const Rect& client, const SnackbarState& st,
                        const Metrics& mx, Point pt);
void PaintSnackbar(IPainter& g, const Rect& client, const SnackbarState& st,
                   const Palette& pal, const Metrics& mx);

// ---- CommandPalette: search + grouped command list ----
struct CommandItem
{
    std::wstring title;
    std::wstring hint;
    std::wstring shortcut;
    std::wstring group;
    bool disabled = false;
};

struct CommandPaletteState
{
    ControlState base = {};
    std::wstring query;
    std::wstring placeholder = L"输入命令或搜索…";
    std::vector<CommandItem> items;
    int selected = 0;       // index into the *matched* rows, not items
    int hovered = -1;       // ditto
};

struct CommandPaletteLayout
{
    Rect panel{}, search{}, list{}, footer{};
    std::vector<Rect> rows;
    int rowH = 0;
};

struct CommandPaletteHit
{
    int row = -1;         // index into items of the matched row
    bool search = false;
};

// Simple case-insensitive ASCII substring filter; returns indices
// into items. An empty query matches everything.
std::vector<int> CommandPaletteMatches(const CommandPaletteState& st);

CommandPaletteLayout MeasureCommandPalette(const Rect& client,
                                           const CommandPaletteState& st,
                                           const Metrics& mx);
CommandPaletteHit HitCommandPalette(const Rect& client,
                                    const CommandPaletteState& st,
                                    const Metrics& mx, Point pt);
void PaintCommandPalette(IPainter& g, const Rect& client,
                         const CommandPaletteState& st,
                         const Palette& pal, const Metrics& mx);

// ---- GuideTour: anchored coach mark with step navigation ----
struct GuideTourState
{
    std::wstring title = L"欢迎使用";
    std::wstring body = L"这里可以管理全部记录。";
    int step = 1;                 // 1-based
    int total = 3;
    bool prevHover = false;
    bool nextHover = false;
    bool skipHover = false;
};

struct GuideTourLayout
{
    Rect card{}, titleRc{}, bodyRc{};
    Rect prevRc{}, nextRc{}, skipRc{}, dotsRc{};
};

GuideTourLayout MeasureGuideTour(const Rect& client, const Rect& anchor,
                                 const GuideTourState& st, const Metrics& mx);
enum class GuideTourHit { None, Prev, Next, Skip };
GuideTourHit HitGuideTour(const Rect& client, const Rect& anchor,
                          const GuideTourState& st, const Metrics& mx, Point pt);
void PaintGuideTour(IPainter& g, const Rect& client, const Rect& anchor,
                    const GuideTourState& st, const Palette& pal,
                    const Metrics& mx);

} // namespace ui
} // namespace air
