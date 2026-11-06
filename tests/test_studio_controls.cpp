// ============================================================
// AirUI - test_studio_controls.cpp
// Core-only regression for the fifth wave of controls. No platform
// headers: this test links airui-core alone.
// ============================================================
#include "airui/ControlsStudio.h"

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
                   int) override { ++texts; Note(r); }
    int MeasureText(const std::wstring& s, FontRole) override
    { return static_cast<int>(s.size()) * 8; }
    void PushClip(const Rect& r) override { ++clipDepth; Note(r); }
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
SparklineState MakeSpark()
{
    SparklineState st;
    st.values = {12.4, 13.1, 12.8, 14.6, 15.2, 14.9, 16.8, 17.5,
                 17.1, 18.6, 19.4, 20.2};
    st.change = 8.4;
    return st;
}

WaterfallState MakeWaterfall()
{
    WaterfallState st;
    st.bars = {
        {L"期初", 120.0, true}, {L"新增", 45.0, false},
        {L"退款", -18.0, false}, {L"续费", 30.0, false},
        {L"期末", 177.0, true},
    };
    return st;
}

StackedBarState MakeStacked()
{
    StackedBarState st;
    st.legend = {L"直营", L"渠道", L"线上", L"其他"};
    st.rows = {
        {L"华东", {42, 28, 20, 10}},
        {L"华南", {35, 30, 25, 10}},
        {L"华北", {50, 20, 18, 12}},
        {L"西南", {25, 35, 30, 10}},
    };
    return st;
}

GaugeState MakeGauge()
{
    GaugeState st;
    st.value = 84.0;
    st.warnAt = 80.0;
    st.segments = 24;
    return st;
}

JsonTreeState MakeJson()
{
    JsonTreeState st;
    st.nodes = {
        {L"code", L"200", 1, false, true},
        {L"data", L"{...}", 1, true, true},
        {L"items", L"[3]", 2, true, true},
        {L"id", L"1001", 3, false, true},
        {L"name", L"控件", 3, false, true},
        {L"enabled", L"true", 3, false, true},
        {L"message", L"ok", 1, false, true},
    };
    return st;
}

LogViewerState MakeLog()
{
    LogViewerState st;
    st.filter = L"缓存";
    st.entries = {
        {L"09:12:01", LogLevel::Info, L"app", L"启动完成，加载配置"},
        {L"09:12:03", LogLevel::Debug, L"db", L"连接池初始化 8 个连接"},
        {L"09:13:20", LogLevel::Info, L"cache", L"缓存预热完成"},
        {L"09:14:05", LogLevel::Warning, L"net", L"请求重试 2 次"},
        {L"09:15:41", LogLevel::Error, L"db", L"缓存写入超时"},
        {L"09:16:02", LogLevel::Info, L"app", L"恢复正常运行"},
    };
    return st;
}

CronEditorState MakeCron()
{
    CronEditorState st;
    st.fields = {L"*/5", L"*", L"*", L"*", L"1-5"};
    st.captions = {L"分", L"时", L"日", L"月", L"周"};
    st.description = L"每 5 分钟执行一次，仅工作日生效";
    st.presets = {L"每小时", L"每天", L"每周", L"自定义"};
    st.selectedPreset = 0;
    st.focused = true;
    return st;
}

MentionInputState MakeMention()
{
    MentionInputState st;
    st.text = L"@";
    st.showCandidates = true;
    st.focused = true;
    st.candidates = {
        {L"张伟", L"产品 · 在线"},
        {L"李娜", L"设计 · 忙碌"},
        {L"王强", L"研发 · 在线"},
        {L"赵敏", L"测试 · 离开"},
    };
    return st;
}

OrgChartState MakeOrg()
{
    OrgChartState st;
    st.nodes = {
        {L"周明", L"负责人", -1},
        {L"张伟", L"产品组", 0},
        {L"李娜", L"设计组", 0},
        {L"王强", L"研发组", 0},
        {L"赵敏", L"测试", 3},
        {L"陈晨", L"前端", 3},
    };
    return st;
}

LeaderboardState MakeLeaders()
{
    LeaderboardState st;
    st.rows = {
        {L"张伟", L"提交 48 次", 980, 2},
        {L"李娜", L"提交 41 次", 875, -1},
        {L"王强", L"提交 39 次", 810, 1},
        {L"赵敏", L"提交 32 次", 690, 0},
        {L"陈晨", L"提交 27 次", 605, -2},
    };
    return st;
}

ActivityFeedState MakeFeed()
{
    ActivityFeedState st;
    st.items = {
        {L"张伟", L"合并了", L"控件库 PR #42", L"2 分钟前", AlertKind::Success, true},
        {L"李娜", L"评论了", L"视觉走查", L"18 分钟前", AlertKind::Info, true},
        {L"王强", L"修复了", L"滚动闪烁", L"1 小时前", AlertKind::Warning, false},
        {L"赵敏", L"报告了", L"构建失败", L"昨天", AlertKind::Error, false},
    };
    return st;
}

AudioWaveformState MakeWave()
{
    AudioWaveformState st;
    for (int i = 0; i < 48; ++i) {
        const double v = 0.25 + 0.7 *
            (static_cast<double>((i * 7) % 11) / 10.0);
        st.peaks.push_back(v > 1.0 ? 1.0 : v);
    }
    st.position = 0.42;
    st.duration = 184.0;
    st.playing = true;
    return st;
}

void PaintAll(IPainter& g, const Rect& rc, const Palette& pal,
              const Metrics& mx)
{
    PaintSparkline(g, rc, MakeSpark(), pal, mx);
    PaintWaterfall(g, rc, MakeWaterfall(), pal, mx);
    PaintStackedBar(g, rc, MakeStacked(), pal, mx);
    PaintGauge(g, rc, MakeGauge(), pal, mx);
    PaintJsonTree(g, rc, MakeJson(), pal, mx);
    PaintLogViewer(g, rc, MakeLog(), pal, mx);
    PaintCronEditor(g, rc, MakeCron(), pal, mx);
    PaintMentionInput(g, rc, MakeMention(), pal, mx);
    PaintOrgChart(g, rc, MakeOrg(), pal, mx);
    PaintLeaderboard(g, rc, MakeLeaders(), pal, mx);
    PaintActivityFeed(g, rc, MakeFeed(), pal, mx);
    PaintWaveform(g, rc, MakeWave(), pal, mx);
}

// ------------------------------------------------------------
// Invariants shared by every control
// ------------------------------------------------------------
void TestInvariants()
{
    const Metrics mx{};
    RecordingPainter empty;
    PaintAll(empty, Rect{}, LightPalette(), mx);
    Check(empty.ops == 0, "empty rect produces zero draw calls");
    Check(empty.clipDepth == 0, "empty rect leaves clip stack balanced");

    const Rect rc(20, 30, 700, 260);
    RecordingPainter light;
    PaintAll(light, rc, LightPalette(), mx);
    Check(light.ops > 300, "all 12 controls actually paint");
    Check(light.texts > 100, "all 12 controls draw text");
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

void TestSparkline(const Rect& rc, const Metrics& mx)
{
    const SparklineState st = MakeSpark();
    const SparklineLayout o = MeasureSparkline(rc, st, mx);
    Check(!o.header.Empty() && !o.valueRc.Empty() && !o.plot.Empty(),
          "sparkline has header, value and plot");
    Check(Inside(rc, o.plot), "sparkline plot inside bounds");
    Check(o.points.size() == st.values.size(),
          "sparkline has one marker per sample");
    Check(o.steps.size() + 1 == st.values.size(),
          "sparkline connects every sample");
    Check(o.baseY == o.plot.bottom, "sparkline baseline is the plot bottom");
    for (std::size_t i = 0; i < o.points.size(); ++i) {
        Check(Inside(o.plot, o.points[i]), "sparkline marker inside plot");
        const int hit = SparklinePointAt(rc, st, mx, Center(o.points[i]));
        Check(hit >= 0 && hit < static_cast<int>(st.values.size()),
              "sparkline marker hit resolves a sample");
        if (i > 0) {
            Check(NoOverlap(o.points[i - 1], o.points[i]),
                  "sparkline markers never overlap");
        }
    }
    Check(SparklinePointAt(rc, st, mx, Point{rc.right + 2, rc.top}) == -1,
          "outside point misses the sparkline");
    SparklineState none;
    const SparklineLayout blank = MeasureSparkline(rc, none, mx);
    Check(blank.points.empty() && blank.steps.empty(),
          "empty series produces no geometry");
}

void TestWaterfall(const Rect& rc, const Metrics& mx)
{
    const WaterfallState st = MakeWaterfall();
    const WaterfallLayout o = MeasureWaterfall(rc, st, mx);
    Check(o.bars.size() == st.bars.size(), "waterfall has one bar per stage");
    Check(o.labels.size() == st.bars.size(), "waterfall has one label per stage");
    Check(o.zeroY >= o.plot.top && o.zeroY <= o.plot.bottom,
          "waterfall zero line inside plot");
    for (std::size_t i = 0; i < o.bars.size(); ++i) {
        Check(Inside(o.plot, o.bars[i]), "waterfall bar inside plot");
        Check(WaterfallBarAt(rc, st, mx, Center(o.bars[i])) ==
                  static_cast<int>(i),
              "waterfall bar hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(o.bars[i - 1], o.bars[i]),
                  "waterfall bars never overlap");
        }
    }
    for (std::size_t i = 0; i < o.connectors.size(); ++i) {
        Check(Inside(o.plot, o.connectors[i]),
              "waterfall connector inside plot");
    }
    Check(WaterfallBarAt(rc, st, mx, Point{rc.left - 1, rc.top}) == -1,
          "outside point misses the waterfall");
}

void TestStackedBar(const Rect& rc, const Metrics& mx)
{
    const StackedBarState st = MakeStacked();
    const StackedBarLayout o = MeasureStackedBar(rc, st, mx);
    Check(o.rowRects.size() == st.rows.size(),
          "stacked bar has one rect per row");
    Check(o.segments.size() == st.rows.size(),
          "stacked bar has one segment list per row");
    for (std::size_t i = 0; i < o.segments.size(); ++i) {
        Check(o.segments[i].size() == st.rows[i].values.size(),
              "segment count matches the value count");
        Check(Inside(rc, o.rowRects[i]), "stacked row inside bounds");
        for (std::size_t j = 0; j < o.segments[i].size(); ++j) {
            const Rect& seg = o.segments[i][j];
            if (seg.Empty()) continue;
            Check(Inside(o.body, seg), "stacked segment inside body");
            const StackedBarHit hit = HitStackedBar(rc, st, mx, Center(seg));
            Check(hit.row == static_cast<int>(i) &&
                      hit.seg == static_cast<int>(j),
                  "stacked segment hit resolves row and segment");
            if (j > 0) {
                Check(NoOverlap(o.segments[i][j - 1], seg),
                      "stacked segments never overlap");
            }
        }
    }
    const StackedBarHit outside =
        HitStackedBar(rc, st, mx, Point{rc.left - 1, rc.bottom + 1});
    Check(outside.row == -1 && outside.seg == -1,
          "outside point misses the stacked bar");
}

void TestGauge(const Rect& rc, const Metrics& mx)
{
    const GaugeState st = MakeGauge();
    const GaugeLayout o = MeasureGauge(rc, st, mx);
    Check(static_cast<int>(o.segments.size()) == st.segments,
          "gauge has the requested segment count");
    Check(!o.band.Empty() && !o.threshold.Empty(),
          "gauge has a band and a threshold marker");
    Check(Inside(o.band, o.threshold), "threshold inside the gauge band");
    for (std::size_t i = 0; i < o.segments.size(); ++i) {
        Check(Inside(o.band, o.segments[i]), "gauge segment inside band");
        Check(GaugeSegmentAt(rc, st, mx, Center(o.segments[i])) ==
                  static_cast<int>(i),
              "gauge segment hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(o.segments[i - 1], o.segments[i]),
                  "gauge segments never overlap");
        }
    }
    Check(GaugeSegmentAt(rc, st, mx, Point{rc.right + 1, rc.top}) == -1,
          "outside point misses the gauge");
}

void TestJsonTree(const Rect& rc, const Metrics& mx)
{
    const JsonTreeState st = MakeJson();
    const JsonTreeLayout o = MeasureJsonTree(rc, st, mx);
    Check(!o.rows.empty(), "json tree has rows");
    Check(o.rows.size() == o.toggles.size(),
          "json tree rows and toggles stay aligned");
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        Check(Inside(o.body, o.rows[i]), "json row inside body");
        const JsonTreeHit rowHit =
            HitJsonTree(rc, st, mx, Point{o.rows[i].right - 2,
                                          Center(o.rows[i]).y});
        Check(rowHit.node == static_cast<int>(i) && !rowHit.toggle,
              "json row hit resolves its own node");
        if (!o.toggles[i].Empty()) {
            Check(Inside(o.rows[i], o.toggles[i]),
                  "json toggle inside its row");
            const JsonTreeHit toggleHit =
                HitJsonTree(rc, st, mx, Center(o.toggles[i]));
            Check(toggleHit.node == static_cast<int>(i) && toggleHit.toggle,
                  "json toggle hit is reported separately");
        }
        if (i > 0) {
            Check(NoOverlap(o.rows[i - 1], o.rows[i]),
                  "json rows never overlap");
        }
    }
    const JsonTreeHit outside =
        HitJsonTree(rc, st, mx, Point{rc.left - 1, rc.bottom + 1});
    Check(outside.node == -1 && !outside.toggle,
          "outside point misses the json tree");
}

void TestLogViewer(const Rect& rc, const Metrics& mx)
{
    const LogViewerState st = MakeLog();
    const LogViewerLayout o = MeasureLogViewer(rc, st, mx);
    Check(!o.header.Empty() && !o.filterRc.Empty() && !o.body.Empty(),
          "log viewer has header, filter and body");
    Check(!o.rows.empty(), "log viewer has rows");
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        Check(Inside(o.body, o.rows[i]), "log row inside body");
        Check(LogLineAt(rc, st, mx, Center(o.rows[i])) == static_cast<int>(i),
              "log row hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(o.rows[i - 1], o.rows[i]),
                  "log rows never overlap");
        }
    }
    Check(LogLineAt(rc, st, mx, Point{rc.right + 1, rc.top}) == -1,
          "outside point misses the log viewer");
}

void TestCronEditor(const Rect& rc, const Metrics& mx)
{
    const CronEditorState st = MakeCron();
    const CronEditorLayout o = MeasureCronEditor(rc, st, mx);
    Check(o.fields.size() == st.fields.size(),
          "cron editor has one field per expression part");
    Check(o.presets.size() == st.presets.size(),
          "cron editor has one button per preset");
    for (std::size_t i = 0; i < o.fields.size(); ++i) {
        Check(Inside(o.expr, o.fields[i]), "cron field inside expression area");
        const CronEditorHit hit = HitCronEditor(rc, st, mx, Center(o.fields[i]));
        Check(hit.field == static_cast<int>(i) && hit.preset == -1,
              "cron field hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(o.fields[i - 1], o.fields[i]),
                  "cron fields never overlap");
        }
    }
    for (std::size_t i = 0; i < o.presets.size(); ++i) {
        Check(Inside(o.presetsRc, o.presets[i]), "cron preset inside area");
        const CronEditorHit hit =
            HitCronEditor(rc, st, mx, Center(o.presets[i]));
        Check(hit.preset == static_cast<int>(i) && hit.field == -1,
              "cron preset hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(o.presets[i - 1], o.presets[i]),
                  "cron presets never overlap");
        }
    }
    const CronEditorHit outside =
        HitCronEditor(rc, st, mx, Point{rc.left - 1, rc.bottom + 1});
    Check(outside.field == -1 && outside.preset == -1,
          "outside point misses the cron editor");
}

void TestMentionInput(const Rect& rc, const Metrics& mx)
{
    const MentionInputState st = MakeMention();
    const MentionInputLayout o = MeasureMentionInput(rc, st, mx);
    Check(!o.input.Empty() && !o.list.Empty(),
          "mention input has a field and a candidate list");
    Check(o.rows.size() == st.candidates.size(),
          "mention input has one row per candidate");
    Check(HitMentionInput(rc, st, mx, Center(o.input)).input,
          "mention field hit is reported");
    Check(Inside(rc, o.list), "candidate list inside bounds");
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        Check(Inside(o.list, o.rows[i]), "candidate row inside list");
        const MentionInputHit hit =
            HitMentionInput(rc, st, mx, Center(o.rows[i]));
        Check(hit.candidate == static_cast<int>(i) && !hit.input,
              "candidate hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(o.rows[i - 1], o.rows[i]),
                  "candidate rows never overlap");
        }
    }
    MentionInputState closed = st;
    closed.showCandidates = false;
    const MentionInputLayout folded = MeasureMentionInput(rc, closed, mx);
    Check(folded.list.Empty() && folded.rows.empty(),
          "closed candidate list produces no list geometry");
    const MentionInputHit outside =
        HitMentionInput(rc, st, mx, Point{rc.left - 1, rc.bottom + 1});
    Check(outside.candidate == -1 && !outside.input,
          "outside point misses the mention input");
}

void TestOrgChart(const Rect& rc, const Metrics& mx)
{
    const OrgChartState st = MakeOrg();
    const OrgChartLayout o = MeasureOrgChart(rc, st, mx);
    Check(o.boxes.size() == st.nodes.size(),
          "org chart has one box per node");
    Check(o.depths.size() == st.nodes.size(),
          "org chart has one depth per node");
    Check(o.depths[0] == 0, "org chart root sits at depth zero");
    for (std::size_t i = 0; i < o.boxes.size(); ++i) {
        Check(Inside(rc, o.boxes[i]), "org box inside bounds");
        Check(OrgNodeAt(rc, st, mx, Center(o.boxes[i])) == static_cast<int>(i),
              "org box hit resolves its own node");
        const int parent = st.nodes[i].parent;
        if (parent >= 0) {
            Check(o.depths[i] > o.depths[static_cast<std::size_t>(parent)],
                  "child depth is greater than its parent");
        }
    }
    for (std::size_t i = 0; i < o.links.size(); ++i) {
        Check(Inside(rc, o.links[i]), "org link inside bounds");
    }
    Check(OrgNodeAt(rc, st, mx, Point{rc.right + 1, rc.top}) == -1,
          "outside point misses the org chart");
}

void TestLeaderboard(const Rect& rc, const Metrics& mx)
{
    const LeaderboardState st = MakeLeaders();
    const LeaderboardLayout o = MeasureLeaderboard(rc, st, mx);
    Check(o.rows.size() == st.rows.size(), "leaderboard has one row per member");
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        Check(Inside(o.body, o.rows[i]), "leaderboard row inside body");
        Check(LeaderRowAt(rc, st, mx, Center(o.rows[i])) ==
                  static_cast<int>(i),
              "leaderboard row hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(o.rows[i - 1], o.rows[i]),
                  "leaderboard rows never overlap");
        }
    }
    Check(LeaderRowAt(rc, st, mx, Point{rc.left - 1, rc.top}) == -1,
          "outside point misses the leaderboard");
}

void TestActivityFeed(const Rect& rc, const Metrics& mx)
{
    const ActivityFeedState st = MakeFeed();
    const ActivityFeedLayout o = MeasureActivityFeed(rc, st, mx);
    Check(!o.rows.empty(), "activity feed has rows");
    Check(o.rows.size() == o.avatars.size(),
          "activity feed rows and avatars stay aligned");
    for (std::size_t i = 0; i < o.rows.size(); ++i) {
        Check(Inside(rc, o.rows[i]), "activity row inside bounds");
        Check(Inside(o.rows[i], o.avatars[i]),
              "activity avatar inside its row");
        Check(ActivityItemAt(rc, st, mx, Center(o.rows[i])) ==
                  static_cast<int>(i),
              "activity row hit resolves its own index");
        if (i > 0) {
            Check(NoOverlap(o.rows[i - 1], o.rows[i]),
                  "activity rows never overlap");
        }
    }
    Check(ActivityItemAt(rc, st, mx, Point{rc.right + 1, rc.top}) == -1,
          "outside point misses the activity feed");
}

void TestWaveform(const Rect& rc, const Metrics& mx)
{
    const AudioWaveformState st = MakeWave();
    const WaveformLayout o = MeasureWaveform(rc, st, mx);
    Check(!o.header.Empty() && !o.track.Empty() && !o.playBtn.Empty(),
          "waveform has header, track and play button");
    Check(!o.playhead.Empty() && Inside(o.track, o.playhead),
          "playhead stays inside the track");
    Check(o.bars.size() == st.peaks.size(),
          "waveform has one bar per peak");
    for (std::size_t i = 0; i < o.bars.size(); ++i) {
        Check(Inside(o.track, o.bars[i]), "waveform bar inside track");
        if (i > 0) {
            Check(NoOverlap(o.bars[i - 1], o.bars[i]),
                  "waveform bars never overlap");
        }
    }
    Check(WaveformPlayButtonAt(rc, st, mx, Center(o.playBtn)),
          "play button hit is reported");
    Check(!WaveformPlayButtonAt(rc, st, mx, Point{rc.right + 1, rc.top}),
          "outside point misses the play button");
    Check(WaveformPositionAt(rc, st, mx, Point{o.track.left, o.track.top}) == 0,
          "track start maps to position zero");
    Check(WaveformPositionAt(rc, st, mx,
                             Point{o.track.right - 1, o.track.top}) >= 990,
          "track end maps to the final position");
    Check(WaveformPositionAt(rc, st, mx, Point{rc.right + 500, rc.top}) <= 1000,
          "position stays clamped outside the track");
}

} // namespace

int main()
{
    const Metrics mx{};
    const Rect rc(24, 32, 760, 300);
    TestInvariants();
    TestSparkline(rc, mx);
    TestWaterfall(rc, mx);
    TestStackedBar(rc, mx);
    TestGauge(rc, mx);
    TestJsonTree(rc, mx);
    TestLogViewer(rc, mx);
    TestCronEditor(rc, mx);
    TestMentionInput(rc, mx);
    TestOrgChart(rc, mx);
    TestLeaderboard(rc, mx);
    TestActivityFeed(rc, mx);
    TestWaveform(rc, mx);
    std::printf("test_studio_controls: OK (%d checks)\n", g_checks);
    return 0;
}
