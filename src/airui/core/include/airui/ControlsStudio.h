#pragma once
// ============================================================
// AirUI - ControlsStudio.h
//
// Fifth wave of controls: small charts, specialist editors and
// collaboration surfaces. Same contract as the other control
// headers:
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
//
// Market convention used by the finance-flavoured charts: a rise
// is drawn in the warm semantic color, a fall in the cool one.
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
// P5 - compact charts
// ============================================================

// ---- Sparkline: headline value plus a stepped micro trend ----
struct SparklineState
{
    ControlState base = {};
    std::wstring title = L"今日走势";
    std::wstring unit = L"¥";
    std::vector<double> values;
    double change = 0.0;       // percent; positive means a rise
    int hovered = -1;
    bool showArea = true;
};

struct SparklineLayout
{
    Rect header{}, plot{}, valueRc{};
    std::vector<Rect> steps;      // thin blocks tracing the series
    std::vector<Rect> points;     // one marker per sample
    int baseY = 0;
};

SparklineLayout MeasureSparkline(const Rect& rc, const SparklineState& st,
                                 const Metrics& mx);
int SparklinePointAt(const Rect& rc, const SparklineState& st,
                     const Metrics& mx, Point pt);
void PaintSparkline(IPainter& g, const Rect& rc,
                    const SparklineState& st,
                    const Palette& pal, const Metrics& mx);

// ---- WaterfallChart: bridge bars for incremental changes ----
struct WaterfallBar
{
    std::wstring label;
    double delta = 0.0;
    bool total = false;          // total bars start from the axis
};

struct WaterfallState
{
    ControlState base = {};
    std::wstring title = L"营收桥接";
    std::vector<WaterfallBar> bars;
    int hovered = -1;
};

struct WaterfallLayout
{
    Rect titleRc{}, plot{}, axis{};
    std::vector<Rect> bars;
    std::vector<Rect> connectors;
    std::vector<Rect> labels;
    int zeroY = 0;
};

WaterfallLayout MeasureWaterfall(const Rect& rc, const WaterfallState& st,
                                 const Metrics& mx);
int WaterfallBarAt(const Rect& rc, const WaterfallState& st,
                   const Metrics& mx, Point pt);
void PaintWaterfall(IPainter& g, const Rect& rc, const WaterfallState& st,
                    const Palette& pal, const Metrics& mx);

// ---- StackedBar: proportional rows with a shared legend ----
struct StackedRow
{
    std::wstring label;
    std::vector<double> values;
};

struct StackedBarState
{
    ControlState base = {};
    std::wstring title = L"渠道构成";
    std::vector<std::wstring> legend;
    std::vector<StackedRow> rows;
    int hoveredRow = -1;
    int hoveredSeg = -1;
};

struct StackedBarLayout
{
    Rect header{}, legendRc{}, body{};
    std::vector<Rect> rowRects;
    std::vector<Rect> labels;
    std::vector<std::vector<Rect>> segments;
};

struct StackedBarHit
{
    int row = -1;
    int seg = -1;
};

StackedBarLayout MeasureStackedBar(const Rect& rc,
                                   const StackedBarState& st,
                                   const Metrics& mx);
StackedBarHit HitStackedBar(const Rect& rc, const StackedBarState& st,
                            const Metrics& mx, Point pt);
void PaintStackedBar(IPainter& g, const Rect& rc,
                     const StackedBarState& st,
                     const Palette& pal, const Metrics& mx);

// ---- GaugeMeter: band of segments with a threshold marker ----
struct GaugeState
{
    ControlState base = {};
    std::wstring title = L"资源占用";
    std::wstring unit = L"%";
    double value = 62.0;
    double min = 0.0;
    double max = 100.0;
    double warnAt = 80.0;
    int segments = 24;
    int hovered = -1;
};

struct GaugeLayout
{
    Rect header{}, valueRc{}, band{}, scale{}, threshold{};
    std::vector<Rect> segments;
};

GaugeLayout MeasureGauge(const Rect& rc, const GaugeState& st,
                         const Metrics& mx);
int GaugeSegmentAt(const Rect& rc, const GaugeState& st,
                   const Metrics& mx, Point pt);
void PaintGauge(IPainter& g, const Rect& rc, const GaugeState& st,
                const Palette& pal, const Metrics& mx);

// ============================================================
// P5 - specialist editors
// ============================================================

// ---- JsonTree: flat pre-order nodes with fold toggles ----
struct JsonNode
{
    std::wstring key;
    std::wstring value;
    int depth = 0;
    bool expandable = false;
    bool expanded = true;
};

struct JsonTreeState
{
    ControlState base = {};
    std::wstring root = L"response";
    std::vector<JsonNode> nodes;
    int selected = -1;
    int hovered = -1;
};

struct JsonTreeLayout
{
    Rect header{}, body{};
    std::vector<Rect> rows;
    std::vector<Rect> toggles;
    int rowH = 0;
};

struct JsonTreeHit
{
    int node = -1;
    bool toggle = false;
};

JsonTreeLayout MeasureJsonTree(const Rect& rc, const JsonTreeState& st,
                               const Metrics& mx);
JsonTreeHit HitJsonTree(const Rect& rc, const JsonTreeState& st,
                        const Metrics& mx, Point pt);
void PaintJsonTree(IPainter& g, const Rect& rc, const JsonTreeState& st,
                   const Palette& pal, const Metrics& mx);

// ---- LogViewer: timestamped level-tagged log rows ----
enum class LogLevel { Debug, Info, Warning, Error };

struct LogEntry
{
    std::wstring time;
    LogLevel level = LogLevel::Info;
    std::wstring source;
    std::wstring text;
};

struct LogViewerState
{
    ControlState base = {};
    std::wstring title = L"运行日志";
    std::wstring filter;
    std::vector<LogEntry> entries;
    int selected = -1;
    int hovered = -1;
};

struct LogViewerLayout
{
    Rect header{}, filterRc{}, body{};
    std::vector<Rect> rows;
    int rowH = 0;
};

LogViewerLayout MeasureLogViewer(const Rect& rc, const LogViewerState& st,
                                 const Metrics& mx);
int LogLineAt(const Rect& rc, const LogViewerState& st,
              const Metrics& mx, Point pt);
void PaintLogViewer(IPainter& g, const Rect& rc, const LogViewerState& st,
                    const Palette& pal, const Metrics& mx);

// ---- CronEditor: five expression fields plus presets ----
struct CronEditorState
{
    ControlState base = {};
    std::wstring title = L"执行计划";
    std::vector<std::wstring> fields;       // minute hour day month weekday
    std::vector<std::wstring> captions;
    std::wstring description;
    std::vector<std::wstring> presets;
    int selectedPreset = -1;
    int hoveredField = -1;
    int hoveredPreset = -1;
    bool focused = false;
};

struct CronEditorLayout
{
    Rect header{}, expr{}, descriptionRc{}, presetsRc{};
    std::vector<Rect> fields;
    std::vector<Rect> presets;
};

struct CronEditorHit
{
    int field = -1;
    int preset = -1;
};

CronEditorLayout MeasureCronEditor(const Rect& rc,
                                   const CronEditorState& st,
                                   const Metrics& mx);
CronEditorHit HitCronEditor(const Rect& rc, const CronEditorState& st,
                            const Metrics& mx, Point pt);
void PaintCronEditor(IPainter& g, const Rect& rc,
                     const CronEditorState& st,
                     const Palette& pal, const Metrics& mx);

// ---- MentionInput: text field with a candidate popover ----
struct MentionCandidate
{
    std::wstring name;
    std::wstring note;
};

struct MentionInputState
{
    ControlState base = {};
    std::wstring text;
    std::wstring placeholder = L"输入 @ 提及同事";
    std::vector<MentionCandidate> candidates;
    int selected = 0;
    int hovered = -1;
    bool focused = false;
    bool showCandidates = false;
};

struct MentionInputLayout
{
    Rect input{}, list{};
    std::vector<Rect> rows;
};

struct MentionInputHit
{
    bool input = false;
    int candidate = -1;
};

MentionInputLayout MeasureMentionInput(const Rect& rc,
                                       const MentionInputState& st,
                                       const Metrics& mx);
MentionInputHit HitMentionInput(const Rect& rc,
                                const MentionInputState& st,
                                const Metrics& mx, Point pt);
void PaintMentionInput(IPainter& g, const Rect& rc,
                       const MentionInputState& st,
                       const Palette& pal, const Metrics& mx);

// ============================================================
// P5 - collaboration surfaces
// ============================================================

// ---- OrgChart: parent-linked node boxes ----
struct OrgNode
{
    std::wstring name;
    std::wstring role;
    int parent = -1;             // index into nodes, -1 for the root
};

struct OrgChartState
{
    ControlState base = {};
    std::vector<OrgNode> nodes;
    int selected = -1;
    int hovered = -1;
};

struct OrgChartLayout
{
    std::vector<Rect> boxes;
    std::vector<Rect> links;
    std::vector<int> depths;
};

OrgChartLayout MeasureOrgChart(const Rect& rc, const OrgChartState& st,
                               const Metrics& mx);
int OrgNodeAt(const Rect& rc, const OrgChartState& st,
              const Metrics& mx, Point pt);
void PaintOrgChart(IPainter& g, const Rect& rc, const OrgChartState& st,
                   const Palette& pal, const Metrics& mx);

// ---- Leaderboard: ranked rows with movement indicators ----
struct LeaderRow
{
    std::wstring name;
    std::wstring note;
    double score = 0.0;
    int delta = 0;               // rank change, positive means up
};

struct LeaderboardState
{
    ControlState base = {};
    std::wstring title = L"本周贡献榜";
    std::vector<LeaderRow> rows;
    int selected = -1;
    int hovered = -1;
};

struct LeaderboardLayout
{
    Rect header{}, body{};
    std::vector<Rect> rows;
    int rowH = 0;
};

LeaderboardLayout MeasureLeaderboard(const Rect& rc,
                                     const LeaderboardState& st,
                                     const Metrics& mx);
int LeaderRowAt(const Rect& rc, const LeaderboardState& st,
                const Metrics& mx, Point pt);
void PaintLeaderboard(IPainter& g, const Rect& rc,
                      const LeaderboardState& st,
                      const Palette& pal, const Metrics& mx);

// ---- ActivityFeed: avatar + actor + action + timestamp ----
struct ActivityItem
{
    std::wstring actor;
    std::wstring action;
    std::wstring target;
    std::wstring time;
    AlertKind kind = AlertKind::Info;
    bool unread = false;
};

struct ActivityFeedState
{
    ControlState base = {};
    std::wstring title = L"团队动态";
    std::vector<ActivityItem> items;
    int hovered = -1;
};

struct ActivityFeedLayout
{
    Rect header{};
    std::vector<Rect> rows;
    std::vector<Rect> avatars;
    int rowH = 0;
};

ActivityFeedLayout MeasureActivityFeed(const Rect& rc,
                                       const ActivityFeedState& st,
                                       const Metrics& mx);
int ActivityItemAt(const Rect& rc, const ActivityFeedState& st,
                   const Metrics& mx, Point pt);
void PaintActivityFeed(IPainter& g, const Rect& rc,
                       const ActivityFeedState& st,
                       const Palette& pal, const Metrics& mx);

// ---- AudioWaveform: peak bars, playhead and time labels ----
struct AudioWaveformState
{
    ControlState base = {};
    std::wstring title = L"会议录音";
    std::vector<double> peaks;   // 0..1
    double position = 0.35;      // 0..1
    double duration = 180.0;     // seconds
    bool playing = false;
    int hovered = -1;
};

struct WaveformLayout
{
    Rect header{}, track{}, timeRc{}, playBtn{};
    std::vector<Rect> bars;
    Rect playhead{};
};

WaveformLayout MeasureWaveform(const Rect& rc,
                               const AudioWaveformState& st,
                               const Metrics& mx);
int WaveformPositionAt(const Rect& rc, const AudioWaveformState& st,
                       const Metrics& mx, Point pt);  // 0..1000, clamped
bool WaveformPlayButtonAt(const Rect& rc, const AudioWaveformState& st,
                          const Metrics& mx, Point pt);
void PaintWaveform(IPainter& g, const Rect& rc,
                   const AudioWaveformState& st,
                   const Palette& pal, const Metrics& mx);

} // namespace ui
} // namespace air
