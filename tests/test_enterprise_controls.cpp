// ============================================================
// AirUI - test_enterprise_controls.cpp
// Core-only regression for the fourth wave of controls. No platform
// headers: this test links airui-core alone.
// ============================================================
#include "airui/ControlsEnterprise.h"

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
    int blocks = 0;
    int clipDepth = 0;
    int minClipDepth = 0;
    std::vector<Rect> rects;

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
                   int) override { ++blocks; ++texts; Note(r); }
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
    return a.Empty() || b.Empty() ||
           a.right <= b.left || b.right <= a.left ||
           a.bottom <= b.top || b.bottom <= a.top;
}

void CheckInsideAll(const std::vector<Rect>& rects, const Rect& rc,
                    const char* tag)
{
    for (std::size_t i = 0; i < rects.size(); ++i) {
        Check(Inside(rc, rects[i]), tag);
    }
}

// ------------------------------------------------------------
// Well-populated demo states shared by all paint checks
// ------------------------------------------------------------
QueryBuilderState MakeQuery()
{
    QueryBuilderState st;
    st.conditions = {
        {L"AND", L"状态", L"=", L"进行中"},
        {L"OR", L"负责人", L"包含", L"张"},
        {L"AND", L"创建时间", L">", L"2026-01-01"},
    };
    return st;
}

PropertyGridState MakeProperty()
{
    PropertyGridState st;
    st.items = {
        {L"常规", L"名称", L"季度报表", PropertyKind::Text, true},
        {L"常规", L"行数", L"240", PropertyKind::Number, true},
        {L"常规", L"启用缓存", L"true", PropertyKind::Bool, true},
        {L"外观", L"主题", L"浅色", PropertyKind::Choice, true},
        {L"外观", L"只读模式", L"false", PropertyKind::Bool, false},
    };
    return st;
}

ScheduleState MakeSchedule()
{
    ScheduleState st;
    st.days = {L"周一", L"周二", L"周三", L"周四", L"周五"};
    st.startHour = 9;
    st.endHour = 17;
    st.today = 2;
    st.events = {
        {L"需求评审", 0, 9, 11, AlertKind::Info},
        {L"设计走查", 1, 11, 12, AlertKind::Warning},
        {L"发布", 2, 14, 16, AlertKind::Success},
        {L"回归测试", 2, 14, 15, AlertKind::Error},
        {L"周会", 4, 16, 17, AlertKind::Info},
    };
    return st;
}

FileDropZoneState MakeDrop()
{
    FileDropZoneState st;
    st.files = {
        {L"report-q3.xlsx", L"已完成", 100, AlertKind::Success},
        {L"screenshot.png", L"上传中", 62, AlertKind::Info},
        {L"archive.zip", L"失败", 20, AlertKind::Error},
    };
    return st;
}

TreeMapState MakeTiles()
{
    TreeMapState st;
    st.items = {
        {L"核心模块", 42, 0}, {L"数据层", 26, 0},
        {L"界面", 18, 0}, {L"测试", 9, 0}, {L"文档", 5, 0},
    };
    return st;
}

FunnelState MakeFunnel()
{
    FunnelState st;
    st.stages = {{L"访问", 12000}, {L"注册", 4800},
                 {L"激活", 2100}, {L"付费", 640}};
    return st;
}

DiffState MakeDiff()
{
    DiffState st;
    st.file = L"src/airui/core/src/Theme.cpp";
    st.lines = {
        {DiffKind::Context, L"const Palette& Pal() { return current; }"},
        {DiffKind::Removed, L"// old palette"},
        {DiffKind::Added, L"// semantic palette"},
        {DiffKind::Context, L"void SetPalette(const Palette& p) {"},
        {DiffKind::Added, L"    current = p;"},
        {DiffKind::Context, L"}"},
    };
    return st;
}

ScoreState MakeScore()
{
    ScoreState st;
    st.average = 4.6;
    st.total = 1280;
    st.counts = {820, 280, 110, 50, 20};
    return st;
}

ChatState MakeChat()
{
    ChatState st;
    st.messages = {
        {L"李", L"这版控件预览看起来不错，气泡排版没有重叠。", L"09:12", false},
        {L"我", L"好的，我再把深色主题和 DPI 缩放跑一遍。", L"09:14", true},
        {L"李", L"收到。", L"09:15", false},
    };
    return st;
}

MultiStatusBarState MakeStatus()
{
    MultiStatusBarState st;
    st.message = L"已连接到本地数据库";
    st.trailing = L"UTF-8";
    st.segments = {{L"同步正常", AlertKind::Success, true},
                   {L"3 个任务", AlertKind::Warning, false},
                   {L"只读", AlertKind::Info, false}};
    return st;
}

RibbonState MakeRibbon()
{
    RibbonState st;
    st.groups = {
        {L"剪贴板", {{L"粘贴", L"▤", true, false}}},
        {L"编辑", {{L"撤销", L"↶", false, false},
                   {L"重做", L"↷", false, false},
                   {L"查找", L"⌕", false, false}}},
        {L"视图", {{L"布局", L"▦", true, false},
                   {L"缩放", L"＋", false, false}}},
    };
    return st;
}

NoticeCenterState MakeNotice()
{
    NoticeCenterState st;
    st.items = {
        {L"构建完成", L"MSVC Release 全部目标已通过", L"2 分钟前", L"CI", true, AlertKind::Success},
        {L"同步异常", L"凭据缓存写入被拒绝，请检查权限", L"18 分钟前", L"Git", true, AlertKind::Error},
        {L"新增评论", L"控件契约头已审阅", L"1 小时前", L"评审", false, AlertKind::Info},
        {L"磁盘空间", L"剩余空间低于 20%", L"昨天", L"系统", false, AlertKind::Warning},
    };
    return st;
}

void PaintAll(RecordingPainter& g, const Rect& rc,
              const Palette& pal, const Metrics& mx)
{
    PaintQueryBuilder(g, rc, MakeQuery(), pal, mx);
    PaintPropertyGrid(g, rc, MakeProperty(), pal, mx);
    PaintSchedule(g, rc, MakeSchedule(), pal, mx);
    PaintFileDropZone(g, rc, MakeDrop(), pal, mx);
    PaintTreeMap(g, rc, MakeTiles(), pal, mx);
    PaintFunnel(g, rc, MakeFunnel(), pal, mx);
    PaintDiff(g, rc, MakeDiff(), pal, mx);
    PaintScore(g, rc, MakeScore(), pal, mx);
    PaintChat(g, rc, MakeChat(), pal, mx);
    PaintMultiStatusBar(g, rc, MakeStatus(), pal, mx);
    PaintRibbon(g, rc, MakeRibbon(), pal, mx);
    PaintNoticeCenter(g, rc, MakeNotice(), pal, mx);
}

// ------------------------------------------------------------
// Global invariants
// ------------------------------------------------------------
void TestInvariants()
{
    RecordingPainter empty;
    const Palette pal = LightPalette();
    const Metrics mx{};
    PaintAll(empty, Rect{}, pal, mx);
    Check(empty.ops == 0, "empty rect produces zero draw calls");
    Check(empty.clipDepth == 0, "empty rect leaves clip stack balanced");

    const Rect rc(20, 30, 700, 260);
    RecordingPainter light;
    PaintAll(light, rc, LightPalette(), mx);
    Check(light.ops > 300, "all 12 controls actually paint");
    Check(light.texts > 100, "all 12 controls draw text");
    Check(light.blocks > 0, "chat uses wrapped text blocks");
    Check(light.clipDepth == 0, "clip stack balanced after light pass");
    Check(light.minClipDepth >= 0, "no unmatched PopClip in light pass");
    CheckInsideAll(light.rects, rc, "light theme rect outside bounds");

    RecordingPainter dark;
    PaintAll(dark, rc, DarkPalette(), mx);
    Check(dark.clipDepth == 0, "clip stack balanced after dark pass");
    Check(dark.minClipDepth >= 0, "no unmatched PopClip in dark pass");
    CheckInsideAll(dark.rects, rc, "dark theme rect outside bounds");

    const Rect small(0, 0, 96, 44);
    RecordingPainter tiny;
    PaintAll(tiny, small, LightPalette(), mx);
    Check(tiny.clipDepth == 0, "clip stack balanced on a tiny rect");
    CheckInsideAll(tiny.rects, small, "tiny rect draws outside bounds");

    RecordingPainter hidpi;
    PaintAll(hidpi, rc, LightPalette(), mx.ForDpi(144));
    Check(hidpi.clipDepth == 0, "clip stack balanced at 144 dpi");
    CheckInsideAll(hidpi.rects, rc, "144 dpi draws outside bounds");
}

// ------------------------------------------------------------
// Editors
// ------------------------------------------------------------
void TestQueryBuilder(const Rect& rc, const Metrics& mx)
{
    const QueryBuilderState st = MakeQuery();
    const QueryBuilderLayout lay = MeasureQueryBuilder(rc, st, mx);
    Check(!lay.rows.empty(), "query builder has rows");
    Check(!lay.add.Empty(), "query builder has an add button");
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        const QueryRowLayout& r = lay.rows[i];
        Check(Inside(rc, r.row), "query row inside bounds");
        const Rect parts[5] = {r.logic, r.field, r.op, r.value, r.remove};
        for (int k = 0; k < 5; ++k) {
            Check(Inside(r.row, parts[k]), "query cell inside its row");
            for (int j = k + 1; j < 5; ++j) {
                Check(NoOverlap(parts[k], parts[j]),
                      "query cells never overlap");
            }
        }
        Check(!Inside(r.row, lay.add) || NoOverlap(r.row, lay.add),
              "add button does not overlap a row");
    }
    const QueryBuilderHit fieldHit =
        HitQueryBuilder(rc, st, mx, Center(lay.rows[0].field));
    Check(fieldHit.kind == QueryHitKind::Field && fieldHit.row == 0,
          "hit resolves the field cell");
    const QueryBuilderHit logicHit =
        HitQueryBuilder(rc, st, mx, Center(lay.rows[1].logic));
    Check(logicHit.kind == QueryHitKind::Logic, "hit resolves the logic chip");
    const QueryBuilderHit firstLogic =
        HitQueryBuilder(rc, st, mx, Center(lay.rows[0].logic));
    Check(firstLogic.kind == QueryHitKind::None,
          "first row WHERE marker is not an editable logic cell");
    const QueryBuilderHit removeHit =
        HitQueryBuilder(rc, st, mx, Center(lay.rows[2].remove));
    Check(removeHit.kind == QueryHitKind::Remove && removeHit.row == 2,
          "hit resolves the remove button");
    const QueryBuilderHit addHit =
        HitQueryBuilder(rc, st, mx, Center(lay.add));
    Check(addHit.kind == QueryHitKind::Add, "hit resolves the add button");
    const QueryBuilderHit outside =
        HitQueryBuilder(rc, st, mx, Point{rc.right + 10, rc.top + 10});
    Check(outside.kind == QueryHitKind::None && outside.row == -1,
          "outside point misses the query builder");
    const QueryBuilderLayout none = MeasureQueryBuilder(Rect{}, st, mx);
    Check(none.rows.empty() && none.add.Empty(),
          "empty rect yields empty query layout");
}

void TestPropertyGrid(const Rect& rc, const Metrics& mx)
{
    const PropertyGridState st = MakeProperty();
    const PropertyGridLayout lay = MeasurePropertyGrid(rc, st, mx);
    Check(!lay.rows.empty(), "property grid has rows");
    bool sawHeader = false;
    int itemRows = 0;
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        const PropertyRowLayout& r = lay.rows[i];
        Check(Inside(rc, r.row), "property row inside bounds");
        if (i > 0) {
            Check(NoOverlap(lay.rows[i - 1].row, r.row),
                  "property rows never overlap");
        }
        if (r.groupHeader) { sawHeader = true; continue; }
        ++itemRows;
        Check(Inside(r.row, r.name) && Inside(r.row, r.value),
              "name and value inside their row");
        Check(NoOverlap(r.name, r.value), "name and value never overlap");
    }
    Check(sawHeader, "property grid emits group headers");
    Check(itemRows > 0, "property grid emits item rows");
    const PropertyGridHit valueHit =
        HitPropertyGrid(rc, st, mx, Center(lay.rows[1].value));
    Check(valueHit.item >= 0 && valueHit.onValue,
          "hit resolves a value cell");
    const PropertyGridHit nameHit =
        HitPropertyGrid(rc, st, mx, Center(lay.rows[1].name));
    Check(nameHit.item >= 0 && !nameHit.onValue,
          "hit resolves a name cell");
    const PropertyGridHit groupHit =
        HitPropertyGrid(rc, st, mx, Center(lay.rows[0].row));
    Check(groupHit.item == -1 && groupHit.group >= 0,
          "hit resolves a group header");
    const PropertyGridHit outside =
        HitPropertyGrid(rc, st, mx, Point{rc.left - 5, rc.top - 5});
    Check(outside.item == -1 && outside.group == -1 && !outside.onValue,
          "outside point misses the property grid");
}

void TestSchedule(const Rect& rc, const Metrics& mx)
{
    const ScheduleState st = MakeSchedule();
    const ScheduleLayout lay = MeasureSchedule(rc, st, mx);
    Check(!lay.grid.Empty() && !lay.header.Empty(),
          "schedule has a header and a grid");
    Check(Inside(rc, lay.grid) && Inside(rc, lay.header) &&
          Inside(rc, lay.timeCol), "schedule areas inside bounds");
    Check(lay.bars.size() == st.events.size(),
          "one bar rect per event, including invalid ones");
    int valid = 0;
    for (std::size_t i = 0; i < lay.bars.size(); ++i) {
        const Rect& bar = lay.bars[i];
        if (bar.Empty()) continue;
        ++valid;
        Check(Inside(lay.grid, bar), "event bar inside the grid");
        Check(ScheduleEventAt(rc, st, mx, Center(bar)) ==
                  static_cast<int>(i),
              "event bar hit resolves its own index");
    }
    Check(valid >= 4, "most events have visible bars");
    // Two overlapping events on the same day must be side by side.
    Check(NoOverlap(lay.bars[2], lay.bars[3]),
          "overlapping events are split into non-overlapping slots");
    Check(ScheduleEventAt(rc, st, mx,
                          Point{rc.left + 1, rc.top + 1}) == -1,
          "time gutter is not an event hit");
    Check(ScheduleEventAt(rc, st, mx,
                          Point{rc.right + 1, rc.bottom + 1}) == -1,
          "outside point misses the schedule");
    const ScheduleLayout none = MeasureSchedule(Rect{}, st, mx);
    Check(none.grid.Empty() && none.bars.empty(),
          "empty rect yields empty schedule layout");
}

void TestDropZone(const Rect& rc, const Metrics& mx)
{
    const FileDropZoneState st = MakeDrop();
    const FileDropZoneLayout lay = MeasureFileDropZone(rc, st, mx);
    Check(!lay.drop.Empty(), "drop zone has a target");
    Check(lay.rows.size() == lay.removeButtons.size(),
          "one remove button per visible row");
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        Check(Inside(rc, lay.rows[i]), "upload row inside bounds");
        Check(NoOverlap(lay.drop, lay.rows[i]),
              "upload rows never overlap the drop target");
        Check(Inside(lay.rows[i], lay.removeButtons[i]),
              "remove button inside its row");
        if (i > 0) {
            Check(NoOverlap(lay.rows[i - 1], lay.rows[i]),
                  "upload rows never overlap");
        }
    }
    const FileDropZoneHit dropHit =
        HitFileDropZone(rc, st, mx, Center(lay.drop));
    Check(dropHit.drop && dropHit.file == -1,
          "hit resolves the drop target");
    const FileDropZoneHit removeHit =
        HitFileDropZone(rc, st, mx, Center(lay.removeButtons[0]));
    Check(removeHit.file == 0 && removeHit.remove,
          "hit resolves a remove button");
    const FileDropZoneHit rowHit =
        HitFileDropZone(rc, st, mx,
                        Point{lay.rows[0].left + 2, Center(lay.rows[0]).y});
    Check(rowHit.file == 0 && !rowHit.remove,
          "hit resolves an upload row");
    const FileDropZoneHit outside =
        HitFileDropZone(rc, st, mx, Point{rc.right + 3, rc.top});
    Check(outside.file == -1 && !outside.drop && !outside.remove,
          "outside point misses the drop zone");
}

// ------------------------------------------------------------
// Data views
// ------------------------------------------------------------
void TestTreeMap(const Rect& rc, const Metrics& mx)
{
    const TreeMapState st = MakeTiles();
    const TreeMapLayout lay = MeasureTreeMap(rc, st, mx);
    Check(lay.tiles.size() == st.items.size(), "one tile per item");
    int filled = 0;
    for (std::size_t i = 0; i < lay.tiles.size(); ++i) {
        const Rect& tile = lay.tiles[i];
        if (tile.Empty()) continue;
        ++filled;
        Check(Inside(rc, tile), "tile inside bounds");
        Check(TreeMapTileAt(rc, st, mx, Center(tile)) ==
                  static_cast<int>(i),
              "tile hit resolves its own index");
        for (std::size_t j = i + 1; j < lay.tiles.size(); ++j) {
            Check(NoOverlap(tile, lay.tiles[j]), "tiles never overlap");
        }
    }
    Check(filled == static_cast<int>(st.items.size()),
          "every item received a tile");
    // The heaviest tile must be larger than the lightest one.
    Check(lay.tiles[0].Width() * lay.tiles[0].Height() >
              lay.tiles[4].Width() * lay.tiles[4].Height(),
          "tile area follows the weights");
    Check(TreeMapTileAt(rc, st, mx, Point{rc.right + 1, rc.bottom + 1}) == -1,
          "outside point misses the tree map");
    const TreeMapLayout none = MeasureTreeMap(Rect{}, st, mx);
    Check(none.tiles.empty(), "empty rect yields no tiles");
}

void TestFunnel(const Rect& rc, const Metrics& mx)
{
    const FunnelState st = MakeFunnel();
    const FunnelLayout lay = MeasureFunnel(rc, st, mx);
    Check(lay.bars.size() == st.stages.size(), "one bar per funnel stage");
    Check(lay.rates.size() == st.stages.size(), "one rate cell per stage");
    for (std::size_t i = 0; i < lay.bars.size(); ++i) {
        Check(Inside(rc, lay.bars[i]), "funnel bar inside bounds");
        Check(Inside(rc, lay.rates[i]), "funnel rate inside bounds");
        Check(NoOverlap(lay.bars[i], lay.rates[i]),
              "funnel bar and rate never overlap");
        Check(FunnelStageAt(rc, st, mx, Center(lay.bars[i])) ==
                  static_cast<int>(i),
              "funnel bar hit resolves its own index");
        if (i > 0) {
            Check(lay.bars[i].Width() <= lay.bars[i - 1].Width(),
                  "funnel narrows monotonically");
            Check(NoOverlap(lay.bars[i - 1], lay.bars[i]),
                  "funnel bars never overlap");
        }
    }
    Check(FunnelStageAt(rc, st, mx, Point{rc.right + 1, rc.top}) == -1,
          "outside point misses the funnel");
    const FunnelLayout none = MeasureFunnel(Rect{}, st, mx);
    Check(none.bars.empty(), "empty rect yields no funnel bars");
}

void TestDiff(const Rect& rc, const Metrics& mx)
{
    const DiffState st = MakeDiff();
    const DiffLayout lay = MeasureDiff(rc, st, mx);
    Check(!lay.rows.empty(), "diff has visible rows");
    Check(Inside(rc, lay.header) && Inside(rc, lay.body),
          "diff header and body inside bounds");
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        Check(Inside(lay.body, lay.rows[i]), "diff row inside the body");
        Check(DiffLineAt(rc, st, mx, Center(lay.rows[i])) ==
                  static_cast<int>(i),
              "diff row hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(lay.rows[i - 1], lay.rows[i]),
                  "diff rows never overlap");
        }
    }
    Check(DiffLineAt(rc, st, mx, Point{rc.left - 1, rc.top}) == -1,
          "outside point misses the diff");
    const DiffLayout none = MeasureDiff(Rect{}, st, mx);
    Check(none.rows.empty(), "empty rect yields no diff rows");
}

void TestScore(const Rect& rc, const Metrics& mx)
{
    const ScoreState st = MakeScore();
    const ScoreLayout lay = MeasureScore(rc, st, mx);
    Check(lay.bars.size() == 5 && lay.labels.size() == 5,
          "score has five distribution rows");
    for (std::size_t i = 0; i < lay.bars.size(); ++i) {
        Check(Inside(rc, lay.bars[i]), "score bar inside bounds");
        Check(Inside(rc, lay.labels[i]), "score label inside bounds");
        Check(NoOverlap(lay.bars[i], lay.labels[i]),
              "score label and bar never overlap");
        Check(ScoreBarAt(rc, st, mx, Center(lay.bars[i])) ==
                  static_cast<int>(i),
              "score bar hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(lay.bars[i - 1], lay.bars[i]),
                  "score bars never overlap");
        }
    }
    Check(!lay.summary.Empty() && Inside(rc, lay.summary),
          "score summary inside bounds");
    Check(ScoreBarAt(rc, st, mx, Point{rc.right + 1, rc.bottom + 1}) == -1,
          "outside point misses the score widget");
    const ScoreLayout none = MeasureScore(Rect{}, st, mx);
    Check(none.bars.empty(), "empty rect yields no score bars");
}

// ------------------------------------------------------------
// Shell / communication
// ------------------------------------------------------------
void TestChat(const Rect& rc, const Metrics& mx)
{
    const ChatState st = MakeChat();
    const ChatLayout lay = MeasureChat(rc, st, mx);
    Check(lay.bubbles.size() == 3, "all three messages are laid out");
    for (std::size_t i = 0; i < lay.bubbles.size(); ++i) {
        Check(Inside(rc, lay.bubbles[i]), "chat bubble inside bounds");
        Check(ChatMessageAt(rc, st, mx, Center(lay.bubbles[i])) ==
                  static_cast<int>(i),
              "chat bubble hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(lay.bubbles[i - 1], lay.bubbles[i]),
                  "chat bubbles never overlap");
        }
    }
    Check(lay.bubbles[1].left > lay.bubbles[0].left,
          "outgoing bubbles are right aligned");
    Check(ChatMessageAt(rc, st, mx, Point{rc.right + 1, rc.top}) == -1,
          "outside point misses the chat");
    const ChatLayout none = MeasureChat(Rect{}, st, mx);
    Check(none.bubbles.empty(), "empty rect yields no chat bubbles");
}

void TestStatusBar(const Rect& rc, const Metrics& mx)
{
    const MultiStatusBarState st = MakeStatus();
    const MultiStatusBarLayout lay = MeasureMultiStatusBar(rc, st, mx);
    Check(!lay.messageRc.Empty() && !lay.trailingRc.Empty(),
          "status bar has a message and a trailing area");
    Check(Inside(rc, lay.messageRc) && Inside(rc, lay.trailingRc),
          "status bar areas inside bounds");
    Check(NoOverlap(lay.messageRc, lay.trailingRc),
          "message and trailing never overlap");
    Check(!lay.segments.empty(), "status bar has segments");
    for (std::size_t i = 0; i < lay.segments.size(); ++i) {
        Check(Inside(rc, lay.segments[i]), "segment inside bounds");
        Check(NoOverlap(lay.messageRc, lay.segments[i]),
              "segment never overlaps the message");
        Check(NoOverlap(lay.trailingRc, lay.segments[i]),
              "segment never overlaps the trailing text");
        const MultiStatusBarHit hit =
            HitMultiStatusBar(rc, st, mx, Center(lay.segments[i]));
        Check(hit.segment == static_cast<int>(i) && !hit.message,
              "segment hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(lay.segments[i - 1], lay.segments[i]),
                  "segments never overlap");
            Check(lay.segments[i].right <= lay.segments[i - 1].left,
                  "segments are packed from the right");
        }
    }
    const MultiStatusBarHit msgHit =
        HitMultiStatusBar(rc, st, mx, Center(lay.messageRc));
    Check(msgHit.message && msgHit.segment == -1,
          "message hit is reported separately");
    const MultiStatusBarHit outside =
        HitMultiStatusBar(rc, st, mx, Point{rc.right + 1, rc.bottom + 1});
    Check(outside.segment == -1 && !outside.message,
          "outside point misses the status bar");
}

void TestRibbon(const Rect& rc, const Metrics& mx)
{
    const RibbonState st = MakeRibbon();
    const RibbonLayout lay = MeasureRibbon(rc, st, mx);
    Check(!lay.groupRects.empty(), "ribbon has groups");
    Check(lay.buttons.size() == lay.groupRects.size(),
          "one button list per group");
    for (std::size_t gi = 0; gi < lay.groupRects.size(); ++gi) {
        Check(Inside(rc, lay.groupRects[gi]), "ribbon group inside bounds");
        if (gi > 0) {
            Check(NoOverlap(lay.groupRects[gi - 1], lay.groupRects[gi]),
                  "ribbon groups never overlap");
        }
        for (std::size_t bi = 0; bi < lay.buttons[gi].size(); ++bi) {
            const Rect& btn = lay.buttons[gi][bi];
            Check(Inside(lay.groupRects[gi], btn),
                  "ribbon button inside its group");
            const RibbonHit hit = HitRibbon(rc, st, mx, Center(btn));
            Check(hit.group == static_cast<int>(gi) &&
                      hit.button == static_cast<int>(bi),
                  "ribbon button hit resolves group and button");
            for (std::size_t bj = bi + 1; bj < lay.buttons[gi].size(); ++bj) {
                Check(NoOverlap(btn, lay.buttons[gi][bj]),
                      "ribbon buttons never overlap");
            }
        }
    }
    const RibbonHit outside =
        HitRibbon(rc, st, mx, Point{rc.right + 1, rc.top});
    Check(outside.group == -1 && outside.button == -1,
          "outside point misses the ribbon");
}

void TestNoticeCenter(const Rect& rc, const Metrics& mx)
{
    const NoticeCenterState st = MakeNotice();
    Check(NoticeUnreadCount(st) == 2, "unread counter is a pure helper");
    const NoticeCenterLayout lay = MeasureNoticeCenter(rc, st, mx);
    Check(!lay.header.Empty() && !lay.readAll.Empty(),
          "notice center has a header and a read-all button");
    Check(!lay.rows.empty(), "notice center has rows");
    for (std::size_t i = 0; i < lay.rows.size(); ++i) {
        Check(Inside(rc, lay.rows[i]), "notice row inside bounds");
        Check(NoOverlap(lay.header, lay.rows[i]),
              "notice rows never overlap the header");
        const NoticeCenterHit hit =
            HitNoticeCenter(rc, st, mx, Center(lay.rows[i]));
        Check(hit.item == static_cast<int>(i) && !hit.readAll,
              "notice row hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(lay.rows[i - 1], lay.rows[i]),
                  "notice rows never overlap");
        }
    }
    const NoticeCenterHit readAll =
        HitNoticeCenter(rc, st, mx, Center(lay.readAll));
    Check(readAll.readAll && readAll.item == -1,
          "read-all hit is reported separately");
    const NoticeCenterHit outside =
        HitNoticeCenter(rc, st, mx, Point{rc.left - 1, rc.bottom + 1});
    Check(outside.item == -1 && !outside.readAll,
          "outside point misses the notice center");
}

} // namespace

int main()
{
    const Metrics mx{};
    const Rect rc(24, 32, 760, 300);
    TestInvariants();
    TestQueryBuilder(rc, mx);
    TestPropertyGrid(rc, mx);
    TestSchedule(rc, mx);
    TestDropZone(rc, mx);
    TestTreeMap(rc, mx);
    TestFunnel(rc, mx);
    TestDiff(rc, mx);
    TestScore(rc, mx);
    TestChat(rc, mx);
    TestStatusBar(rc, mx);
    TestRibbon(rc, mx);
    TestNoticeCenter(rc, mx);
    std::printf("test_enterprise_controls: OK (%d checks)\n", g_checks);
    return 0;
}
