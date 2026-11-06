#pragma once
// ============================================================
// AirUI - ControlsEnterprise.h
//
// Fourth wave of controls, focused on business-workbench widgets.
// Same contract as ControlsCore.h / ControlsExtra.h /
// ControlsAdvanced.h:
//
//   *State      - plain data owned by the caller, no handles
//   Measure*    - pure geometry shared by painting and hit testing
//   *Hit / *At  - pure hit testing
//   Paint*      - void PaintXxx(IPainter&, rect, state, pal, mx)
//
// Implementation rules:
//   - only IPainter / Palette / Metrics / types.h / std lib
//   - colors come from pal.*, sizes from mx.*
//   - an empty rect produces zero draw calls
//   - every Text() carries dt::NoPrefix
//   - PushClip/PopClip balanced
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
// P4 - editors / input surfaces
// ============================================================

// ---- QueryBuilder: rows of field / operator / value conditions ----
struct QueryCondition
{
    std::wstring logic = L"AND";   // AND or OR, ignored on the first row
    std::wstring field;
    std::wstring op = L"=";
    std::wstring value;
};

struct QueryBuilderState
{
    ControlState base = {};
    std::vector<QueryCondition> conditions;
    std::wstring title = L"筛选条件";
    int hoveredRow = -1;
    int hoveredCell = -1;     // 0 logic, 1 field, 2 operator, 3 value
    bool addHover = false;
};

struct QueryRowLayout
{
    Rect row{};
    Rect logic{};
    Rect field{};
    Rect op{};
    Rect value{};
    Rect remove{};
};

struct QueryBuilderLayout
{
    Rect header{};
    std::vector<QueryRowLayout> rows;
    Rect add{};
    int rowH = 0;
};

enum class QueryHitKind { None, Logic, Field, Operator, Value, Remove, Add };

struct QueryBuilderHit
{
    QueryHitKind kind = QueryHitKind::None;
    int row = -1;
};

QueryBuilderLayout MeasureQueryBuilder(const Rect& rc,
                                       const QueryBuilderState& st,
                                       const Metrics& mx);
QueryBuilderHit HitQueryBuilder(const Rect& rc, const QueryBuilderState& st,
                                const Metrics& mx, Point pt);
void PaintQueryBuilder(IPainter& g, const Rect& rc,
                       const QueryBuilderState& st,
                       const Palette& pal, const Metrics& mx);

// ---- PropertyGrid: grouped name/value rows --------------------
enum class PropertyKind { Text, Number, Bool, Choice };

struct PropertyItem
{
    std::wstring group;
    std::wstring name;
    std::wstring value;
    PropertyKind kind = PropertyKind::Text;
    bool enabled = true;
};

struct PropertyGridState
{
    ControlState base = {};
    std::vector<PropertyItem> items;
    int selected = -1;        // index into items
    int hovered = -1;
    bool showGroupHeaders = true;
};

struct PropertyRowLayout
{
    bool groupHeader = false;
    int  item = -1;           // index into items, -1 for a group header
    Rect row{};
    Rect name{};
    Rect value{};
};

struct PropertyGridLayout
{
    std::vector<PropertyRowLayout> rows;
    int rowH = 0;
    int groupH = 0;
    int nameW = 0;
};

struct PropertyGridHit
{
    int item = -1;
    int group = -1;           // index into items of the first row in group
    bool onValue = false;
};

PropertyGridLayout MeasurePropertyGrid(const Rect& rc,
                                       const PropertyGridState& st,
                                       const Metrics& mx);
PropertyGridHit HitPropertyGrid(const Rect& rc, const PropertyGridState& st,
                                const Metrics& mx, Point pt);
void PaintPropertyGrid(IPainter& g, const Rect& rc,
                       const PropertyGridState& st,
                       const Palette& pal, const Metrics& mx);

// ---- ScheduleGrid: days x hours with spanning event blocks ----
struct ScheduleEvent
{
    std::wstring title;
    int day = 0;              // 0-based column
    int startHour = 9;
    int endHour = 10;
    AlertKind kind = AlertKind::Info;
};

struct ScheduleState
{
    ControlState base = {};
    std::vector<std::wstring> days;
    std::vector<ScheduleEvent> events;
    int startHour = 9;
    int endHour = 18;
    int today = -1;
    int hovered = -1;
};

struct ScheduleLayout
{
    Rect header{}, timeCol{}, grid{};
    std::vector<Rect> bars;
    int hourH = 0;
    int colW = 0;
    int timeW = 0;
};

ScheduleLayout MeasureSchedule(const Rect& rc, const ScheduleState& st,
                               const Metrics& mx);
int ScheduleEventAt(const Rect& rc, const ScheduleState& st,
                    const Metrics& mx, Point pt);
void PaintSchedule(IPainter& g, const Rect& rc, const ScheduleState& st,
                   const Palette& pal, const Metrics& mx);

// ---- FileDropZone: dashed drop target plus upload rows --------
struct UploadFile
{
    std::wstring name;
    std::wstring note = L"已完成";
    int progress = 100;       // 0..100
    AlertKind kind = AlertKind::Success;
};

struct FileDropZoneState
{
    ControlState base = {};
    std::vector<UploadFile> files;
    std::wstring title = L"拖拽文件到此处";
    std::wstring hint = L"或点击选择 · 单个文件不超过 20 MB";
    bool dragOver = false;
    int hovered = -1;
};

struct FileDropZoneLayout
{
    Rect drop{};
    std::vector<Rect> rows;
    std::vector<Rect> removeButtons;
    int rowH = 0;
};

struct FileDropZoneHit
{
    int file = -1;
    bool remove = false;
    bool drop = false;
};

FileDropZoneLayout MeasureFileDropZone(const Rect& rc,
                                       const FileDropZoneState& st,
                                       const Metrics& mx);
FileDropZoneHit HitFileDropZone(const Rect& rc,
                                const FileDropZoneState& st,
                                const Metrics& mx, Point pt);
void PaintFileDropZone(IPainter& g, const Rect& rc,
                       const FileDropZoneState& st,
                       const Palette& pal, const Metrics& mx);

// ============================================================
// P4 - data views / charts
// ============================================================

// ---- TreeMap: weighted nested rectangles ----------------------
struct TileItem
{
    std::wstring label;
    int weight = 1;
    Color tint = 0;           // 0 = derive a semantic color from the palette
};

struct TreeMapState
{
    ControlState base = {};
    std::vector<TileItem> items;
    int hovered = -1;
};

struct TreeMapLayout
{
    std::vector<Rect> tiles;
};

TreeMapLayout MeasureTreeMap(const Rect& rc, const TreeMapState& st,
                             const Metrics& mx);
int TreeMapTileAt(const Rect& rc, const TreeMapState& st,
                  const Metrics& mx, Point pt);
void PaintTreeMap(IPainter& g, const Rect& rc, const TreeMapState& st,
                  const Palette& pal, const Metrics& mx);

// ---- FunnelChart: centred narrowing stages with rates ---------
struct FunnelStage
{
    std::wstring label;
    int value = 0;
};

struct FunnelState
{
    ControlState base = {};
    std::vector<FunnelStage> stages;
    std::wstring title = L"转化漏斗";
    bool showRate = true;
    int hovered = -1;
};

struct FunnelLayout
{
    Rect titleRc{};
    std::vector<Rect> bars;
    std::vector<Rect> rates;
};

FunnelLayout MeasureFunnel(const Rect& rc, const FunnelState& st,
                           const Metrics& mx);
int FunnelStageAt(const Rect& rc, const FunnelState& st,
                  const Metrics& mx, Point pt);
void PaintFunnel(IPainter& g, const Rect& rc, const FunnelState& st,
                 const Palette& pal, const Metrics& mx);

// ---- DiffView: unified diff with old/new line numbers ---------
enum class DiffKind { Context, Added, Removed };

struct DiffLine
{
    DiffKind kind = DiffKind::Context;
    std::wstring text;
};

struct DiffState
{
    ControlState base = {};
    std::wstring file = L"src/example.cpp";
    std::vector<DiffLine> lines;
    int firstOld = 1;
    int firstNew = 1;
    int hovered = -1;
};

struct DiffLayout
{
    Rect header{}, body{};
    std::vector<Rect> rows;
    int lineH = 0;
    int gutterW = 0;
};

DiffLayout MeasureDiff(const Rect& rc, const DiffState& st,
                       const Metrics& mx);
int DiffLineAt(const Rect& rc, const DiffState& st,
               const Metrics& mx, Point pt);
void PaintDiff(IPainter& g, const Rect& rc, const DiffState& st,
               const Palette& pal, const Metrics& mx);

// ---- ScoreBreakdown: average plus 5 distribution bars ---------
struct ScoreState
{
    ControlState base = {};
    std::wstring title = L"用户评分";
    double average = 4.6;
    int total = 0;
    std::vector<int> counts;   // index 0 = 5 stars, 4 = 1 star
    int hovered = -1;
};

struct ScoreLayout
{
    Rect summary{}, barsArea{};
    std::vector<Rect> bars;
    std::vector<Rect> labels;
    int rowH = 0;
};

ScoreLayout MeasureScore(const Rect& rc, const ScoreState& st,
                         const Metrics& mx);
int ScoreBarAt(const Rect& rc, const ScoreState& st,
               const Metrics& mx, Point pt);
void PaintScore(IPainter& g, const Rect& rc, const ScoreState& st,
                const Palette& pal, const Metrics& mx);

// ============================================================
// P4 - shell / communication
// ============================================================

// ---- ChatThread: alternating message bubbles ------------------
struct ChatMessage
{
    std::wstring author;
    std::wstring text;
    std::wstring time;
    bool outgoing = false;
};

struct ChatState
{
    ControlState base = {};
    std::vector<ChatMessage> messages;
    int hovered = -1;
};

struct ChatLayout
{
    std::vector<Rect> bubbles;
};

ChatLayout MeasureChat(const Rect& rc, const ChatState& st,
                       const Metrics& mx);
int ChatMessageAt(const Rect& rc, const ChatState& st,
                  const Metrics& mx, Point pt);
void PaintChat(IPainter& g, const Rect& rc, const ChatState& st,
               const Palette& pal, const Metrics& mx);

// ---- MultiStatusBar: message plus aligned segments ------------
struct StatusSegment
{
    std::wstring text;
    AlertKind kind = AlertKind::Info;
    bool active = false;
};

struct MultiStatusBarState
{
    ControlState base = {};
    std::wstring message = L"就绪";
    std::wstring trailing = L"UTF-8";
    std::vector<StatusSegment> segments;
    int hovered = -1;
};

struct MultiStatusBarLayout
{
    Rect messageRc{}, trailingRc{};
    std::vector<Rect> segments;
};

struct MultiStatusBarHit
{
    int segment = -1;
    bool message = false;
};

MultiStatusBarLayout MeasureMultiStatusBar(const Rect& rc,
                                           const MultiStatusBarState& st,
                                           const Metrics& mx);
MultiStatusBarHit HitMultiStatusBar(const Rect& rc,
                                    const MultiStatusBarState& st,
                                    const Metrics& mx, Point pt);
void PaintMultiStatusBar(IPainter& g, const Rect& rc,
                         const MultiStatusBarState& st,
                         const Palette& pal, const Metrics& mx);

// ---- RibbonBar: command groups with large/small buttons -------
struct RibbonButton
{
    std::wstring label;
    std::wstring glyph;
    bool large = false;
    bool disabled = false;
};

struct RibbonGroup
{
    std::wstring title;
    std::vector<RibbonButton> buttons;
};

struct RibbonState
{
    ControlState base = {};
    std::vector<RibbonGroup> groups;
    int hoveredGroup = -1;
    int hoveredButton = -1;
};

struct RibbonLayout
{
    std::vector<Rect> groupRects;
    std::vector<std::vector<Rect>> buttons;
};

struct RibbonHit
{
    int group = -1;
    int button = -1;
};

RibbonLayout MeasureRibbon(const Rect& rc, const RibbonState& st,
                           const Metrics& mx);
RibbonHit HitRibbon(const Rect& rc, const RibbonState& st,
                    const Metrics& mx, Point pt);
void PaintRibbon(IPainter& g, const Rect& rc, const RibbonState& st,
                 const Palette& pal, const Metrics& mx);

// ---- NotificationCenter: unread list with read-all action -----
struct NoticeItem
{
    std::wstring title;
    std::wstring body;
    std::wstring time;
    std::wstring group;
    bool unread = false;
    AlertKind kind = AlertKind::Info;
};

struct NoticeCenterState
{
    ControlState base = {};
    std::wstring title = L"通知中心";
    std::vector<NoticeItem> items;
    bool readAllHover = false;
    int hovered = -1;
};

struct NoticeCenterLayout
{
    Rect header{}, readAll{};
    std::vector<Rect> rows;
    int rowH = 0;
};

struct NoticeCenterHit
{
    int item = -1;
    bool readAll = false;
};

NoticeCenterLayout MeasureNoticeCenter(const Rect& rc,
                                       const NoticeCenterState& st,
                                       const Metrics& mx);
NoticeCenterHit HitNoticeCenter(const Rect& rc,
                                const NoticeCenterState& st,
                                const Metrics& mx, Point pt);
// Number of unread notices; pure helper, safe with any state.
int NoticeUnreadCount(const NoticeCenterState& st);
void PaintNoticeCenter(IPainter& g, const Rect& rc,
                       const NoticeCenterState& st,
                       const Palette& pal, const Metrics& mx);

} // namespace ui
} // namespace air
