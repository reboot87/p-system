#pragma once
// ============================================================
// AirUI - ExtraGallery.h
// Shared demo content for the P0 framework / P1 migration / P2
// controls. Kept in one header so src/gallery/main.cpp only needs
// four small hooks (nav, layout, paint, pointer/key/wheel).
//
// All state lives in ExtraDemoState, owned by GalleryPage, so a
// resize that rebuilds the Item list never resets any interaction.
//
// Design note: every demo control is described by a single
// `DemoEntry` registered in `Demos()`. One entry bundles the
// metadata (group/label/height) together with its Paint / Click /
// Wheel behaviour. This is a Command + Registry (Factory) pattern:
// adding a new control is a *single* registration instead of
// editing four parallel switch statements (the previous OCP
// violation). `Defs()` and `Count()` are derived from the same
// registry, so there is one source of truth for the demo list.
// ============================================================
#include "airui/ControlsExtra.h"
#include "airui/ControlsAdvanced.h"
#include "airui/ControlsEnterprise.h"
#include "airui/ControlsStudio.h"
#include "airui/FrameworkCore.h"
#include "airui/WidgetControls.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace air {
namespace ui {
namespace extragallery {

struct ExtraDef
{
    const wchar_t* group;
    const wchar_t* label;
    int  height;
};

// Forward declaration so the command signatures below can name it.
struct ExtraDemoState;

// ------------------------------------------------------------
// Command + Registry pattern.
// Each demo control is one DemoEntry: metadata plus the three
// behaviour hooks. `paint` is mandatory; `click`/`wheel` are
// optional (nullptr → falls back to the default handler).
// ------------------------------------------------------------
using DemoPaintFn = std::function<void(IPainter&, const Rect&, ExtraDemoState&,
                                      const Palette&, const Metrics&)>;
using DemoClickFn = std::function<void(int, Point, const Rect&, ExtraDemoState&,
                                      const Metrics&, std::wstring&)>;
using DemoWheelFn = std::function<bool(int, int, ExtraDemoState&, const Metrics&)>;

struct DemoEntry
{
    const wchar_t* group;
    const wchar_t* label;
    int  height;
    DemoPaintFn paint;
    DemoClickFn click;   // optional
    DemoWheelFn wheel;   // optional
};

// Forward declaration; defined after the command implementations.
inline const std::vector<DemoEntry>& Demos();

// ------------------------------------------------------------
// A tiny in-memory clipboard so the editable text demo can use
// Copy / Cut / Paste without a native clipboard dependency.
// ------------------------------------------------------------
class DemoClipboard : public IClipboard
{
public:
    std::wstring Read() override { return m_text; }
    void Write(const std::wstring& text) override { m_text = text; }
private:
    std::wstring m_text;
};

struct ExtraDemoState
{
    // P0
    ControlHost   host;
    bool          hostReady = false;
    TextBoxState  textbox;
    DemoClipboard clipboard;
    Metrics       dpiBase{};
    int           dpi = 96;
    float         animPhase = 0.0f;
    ListDataState virtualList;
    ScrollState   demoScroll;

    // P1
    CardState       card;
    EmptyViewState  emptyView;
    StatusTagState  statusTag;
    KbdChipState    kbd;
    ToolbarState    toolbar;
    FormState       form;
    DataGridState   grid;
    ListDataState   listData;
    CustomState     custom;

    // P2 input
    SearchBoxState   search;
    PasswordBoxState password;
    TextAreaState    textArea;
    NumberInputState number;
    DatePickerState  date;
    ColorPickerState color;

    // P2 choice
    DropdownState    dropdown;
    MenuState        menu;
    ContextMenuState contextMenu;
    MultiSelectState multi;
    TransferState    transfer;

    // P2 display
    TimelineState       timeline;
    StepsState          steps;
    CalendarState       calendar;
    DescriptionListState description;
    StatCardState       stat;
    ChartState          chart;
    CarouselState       carousel;

    // P2 feedback
    SpinnerState      spinner;
    TooltipState      tooltip;
    PopoverState      popover;
    ModalState        modal;
    DrawerState       drawer;
    NotificationState notification;
    ResultState       result;

    // P2 layout
    AccordionState    accordion;
    AnchorState       anchor;
    SplitterState     splitter;
    ColumnResizeState columnResize;

    // P3 advanced
    TagInputState       tagInput;
    RangeSliderState    rangeSlider;
    OtpInputState       otp;
    CascaderState       cascader;
    AvatarGroupState    avatars;
    HeatmapState        heatmap;
    CodeBlockState      codeBlock;
    KanbanState         kanban;
    GanttState          gantt;
    SnackbarState       snackbar;
    CommandPaletteState palette;
    GuideTourState      guide;

    // P4 enterprise
    QueryBuilderState     query;
    PropertyGridState     property;
    ScheduleState         schedule;
    FileDropZoneState     dropZone;
    TreeMapState          treeMap;
    FunnelState           funnel;
    DiffState             diff;
    ScoreState            score;
    ChatState             chat;
    MultiStatusBarState   multiStatus;
    RibbonState           ribbon;
    NoticeCenterState     notice;

    // P5 studio
    SparklineState        spark;
    WaterfallState        waterfall;
    StackedBarState       stacked;
    GaugeState            gauge;
    JsonTreeState         jsonTree;
    LogViewerState        logs;
    CronEditorState       cron;
    MentionInputState     mention;
    OrgChartState         org;
    LeaderboardState      leaders;
    ActivityFeedState     feed;
    AudioWaveformState    wave;

    // --- P6: object layer, each widget owns its own state ---
    widgets::Button       wButton;
    widgets::Checkbox     wCheckbox;
    widgets::TextField    wTextField;
    widgets::Slider       wSlider;
    widgets::Sparkline    wSparkline;
    widgets::Gauge        wGauge;
    widgets::RatingBar    wRatingBar;    // new control (built from scratch)
    widgets::IconButton   wIconBtn;      // inherited control (derives from Button)
};

inline void InitExtraDemo(ExtraDemoState& s)
{
    // --- P0: object/event layer with z-order, now hosting real widgets ---
    if (!s.hostReady) {
        auto a = std::make_unique<widgets::Button>();
        a->SetName(L"保存"); a->text = L"保存";
        a->style = ButtonStyle::Primary;
        a->SetBounds(Rect(12, 12, 132, 44)); a->SetZ(1); a->SetTabIndex(10);
        s.host.Add(std::move(a));
        auto b = std::make_unique<widgets::TextField>();
        b->SetName(L"输入框 B"); b->state.text = L"对象持有的状态";
        b->SetBounds(Rect(80, 28, 220, 60)); b->SetZ(2); b->SetTabIndex(20);
        s.host.Add(std::move(b));
        auto c = std::make_unique<widgets::Button>();
        c->SetName(L"禁用 C"); c->text = L"禁用";
        c->style = ButtonStyle::Danger;
        c->SetBounds(Rect(240, 12, 340, 44)); c->SetZ(0); c->SetTabIndex(30);
        c->SetEnabled(false);
        s.host.Add(std::move(c));
        s.hostReady = true;
    }
    s.dpiBase = Metrics{};
    s.textbox.placeholder = L"点击后输入文字，支持 Home/End/退格/删除/Ctrl+C/V";
    s.textbox.edit.text = L"可编辑文本";
    s.textbox.edit.cursor = 5;
    s.virtualList.rows.clear();
    for (int i = 0; i < 240; ++i) {
        ListDataRow row;
        row.title = L"虚拟行 #" + std::to_wstring(i + 1);
        row.subtitle = L"仅绘制可见区间，滚动查看";
        row.badge = (i % 7 == 0) ? L"标记" : L"";
        row.height = (i % 5 == 0) ? 44 : 32;
        s.virtualList.rows.push_back(row);
    }
    s.virtualList.scroll.contentH = ListDataContentHeight(s.virtualList, Metrics{});
    s.demoScroll.contentH = 900;
    s.demoScroll.viewH = 140;
    s.demoScroll.offset = 120;

    // --- P1 ---
    s.card.title = L"核心卡片 / Card";
    s.card.desc = L"绘制逻辑已从平台层迁入 core";
    s.card.number = L"1,286";
    s.card.unit = L"条";
    s.card.badge = L"已同步";
    s.card.linkText = L"查看详情 ›";
    s.emptyView.title = L"暂无记录";
    s.emptyView.steps = L"当前筛选条件下没有数据";
    s.emptyView.foot = L"清空筛选或新建一条记录";
    s.statusTag.text = L"运行中";
    s.kbd.key = L"Ctrl";
    s.kbd.label = L"+ K 搜索";
    s.toolbar.searchPlaceholder = L"搜索记录…";
    s.toolbar.crumbs = { L"首页", L"数据中心", L"记录" };
    s.toolbar.actions = { {1, L"新建", L"", true, false}, {2, L"导出", L"", false, false} };
    s.form.fields = {
        {L"名称", L"季度报告", L"请输入名称", L"", true, false},
        {L"备注", L"", L"选填", L"", false, false},
        {L"负责人", L"张三", L"请输入", L"", true, false},
    };
    s.grid.columns = { {L"名称", 3, true}, {L"数量", 1, true}, {L"状态", 2, true} };
    s.grid.rows = {
        {L"剪贴板记录", L"128", L"已完成"},
        {L"文件变更",   L"64",  L"处理中"},
        {L"打印任务",   L"12",  L"失败"},
        {L"截图归档",   L"256", L"已完成"},
    };
    s.grid.selected = 1;
    s.listData.rows = {
        {L"第一行", L"可变行高 · 32px", L"", 32, false},
        {L"第二行", L"更高的行 · 48px", L"标记", 48, false},
        {L"第三行", L"禁用态", L"", 40, true},
        {L"第四行", L"可变行高 · 36px", L"", 36, false},
    };
    s.listData.selected = 0;
    s.custom.label = L"Custom / 自绘区域（虚线边框）";

    // --- P2 input ---
    s.search.text = L"季度报告";
    s.password.text = L"airui-core";
    s.textArea.text = L"第一行：平台无关的绘制逻辑。\n第二行：状态与几何都在 core。";
    s.number.value = 12;
    s.color.swatches = {};   // core synthesises from semantic palette colors
    s.date.year = 2026; s.date.month = 11; s.date.day = 6;

    // --- P2 choice ---
    s.dropdown.text = L"请选择类型";
    s.dropdown.options = { L"文档", L"图片", L"视频", L"音频", L"其他" };
    s.dropdown.selected = 1;
    s.dropdown.open = true;
    s.menu.items = {
        {L"新建", L"Ctrl+N", L"", false, false, false},
        {L"打开", L"Ctrl+O", L"", false, false, false},
        {L"", L"", L"", false, true, false},
        {L"自动保存", L"", L"", false, false, true},
        {L"导出（不可用）", L"", L"", true, false, false},
    };
    s.contextMenu.menu = s.menu;
    s.contextMenu.visible = true;
    s.multi.options = { L"剪贴板监控", L"文件监控", L"打印监控", L"插件中心" };
    s.multi.checked = { true, false, true, false };
    s.transfer.left = { L"待选 A", L"待选 B", L"待选 C", L"待选 D" };
    s.transfer.right = { L"已选 1", L"已选 2" };
    s.transfer.leftChecked = { true, false, false, false };
    s.transfer.rightChecked = { false, false };

    // --- P2 display ---
    s.timeline.events = {
        {L"创建记录", L"由剪贴板监控自动创建", L"09:12", AlertKind::Success},
        {L"同步中", L"正在同步到云端", L"09:20", AlertKind::Info},
        {L"校验失败", L"重试后可继续", L"09:31", AlertKind::Error},
    };
    s.steps.steps = { L"选择数据", L"配置规则", L"确认提交", L"完成" };
    s.steps.current = 1;
    s.calendar.year = 2026; s.calendar.month = 11;
    s.calendar.selectedDay = 6;
    s.calendar.marked = { 3, 12, 20, 26 };
    s.description.title = L"记录详情";
    s.description.items = {
        {L"名称", L"季度报告.docx"}, {L"类型", L"文档"},
        {L"大小", L"2.4 MB"}, {L"状态", L"已完成"}
    };
    s.stat.bars = { 20, 34, 28, 46, 40, 62, 55 };
    s.chart.title = L"每周记录量";
    s.chart.values = { 32, 58, 41, 76, 64, 88 };
    s.chart.labels = { L"一", L"二", L"三", L"四", L"五", L"六" };
    s.carousel.slides = { L"概览：全部控件均已接入 core", L"交互：点击切换 / 键盘可达",
                          L"跨平台：状态与几何与后端无关" };

    // --- P2 feedback ---
    s.tooltip.text = L"这是一个自动定位的提示层";
    s.popover.title = L"弹出层标题";
    s.popover.body = L"位置由 PlaceOverlay 计算，越界时自动翻转并夹紧到可视区域。";
    s.modal.body = L"确定要提交这批记录吗？此操作不可撤销。";
    s.modal.focused = 1;
    s.drawer.body = L"抽屉从右侧滑入，遮罩使用半透明填充。";
    s.notification.maxVisible = 3;
    s.notification.items.clear();
    for (int i = 0; i < 3; ++i) {
        ToastState t;
        t.kind = static_cast<AlertKind>(i % 4);
        t.title = L"通知 " + std::to_wstring(i + 1);
        t.message = L"操作已完成，可点击关闭";
        s.notification.items.push_back(t);
    }
    s.result.detail = L"45 个新控件全部接入画廊并可通过导航跳转。";

    // --- P2 layout ---
    s.accordion.panels = {
        {L"基础设置", L"主题、语言与密度设置"},
        {L"高级设置", L"缓存、日志与实验特性"},
        {L"关于", L"版本信息与开源许可"},
    };
    s.accordion.expanded = { true, false, false };
    s.anchor.items = { L"概览", L"数据", L"统计", L"设置" };
    s.columnResize.widths = { 3, 2, 1, 2 };

    // --- P3 input ---
    s.tagInput.tags = { L"airui", L"core", L"自绘" };
    s.tagInput.placeholder = L"输入新标签…";
    s.rangeSlider.low = 20;
    s.rangeSlider.high = 80;
    s.rangeSlider.step = 5;
    s.rangeSlider.showTicks = true;
    s.otp.code = L"826";
    s.otp.length = 6;
    s.otp.focused = 3;
    s.cascader.columns = {
        {L"一级", {L"华东", L"华北", L"华南", L"西南"}},
        {L"二级", {L"上海", L"江苏", L"浙江", L"安徽"}},
        {L"三级", {L"浦东", L"徐汇", L"静安", L"长宁"}},
    };
    s.cascader.selected = { 0, 1, -1 };

    // --- P3 data ---
    s.avatars.items = { {L"李", 0}, {L"王", 0}, {L"张", 0},
                        {L"赵", 0}, {L"陈", 0}, {L"周", 0}, {L"吴", 0} };
    s.avatars.maxVisible = 4;
    s.heatmap.title = L"近 20 周活跃度";
    s.heatmap.weeks = 20;
    s.heatmap.values.clear();
    for (int i = 0; i < 20 * 7; ++i) {
        s.heatmap.values.push_back((i * 7 + i / 9) % 5);
    }
    s.codeBlock.title = L"example.cpp";
    s.codeBlock.lines = {
        L"#include \"airui/ControlsAdvanced.h\"",
        L"",
        L"void Demo(IPainter& g, const Rect& rc) {",
        L"    PaintHeatmap(g, rc, state, Pal(), Mx());",
        L"}",
    };
    s.codeBlock.highlightLine = 2;
    s.kanban.columns = {
        {L"待办", {{L"补齐单元测试", L"P1", AlertKind::Warning},
                   {L"整理设计稿", L"设计", AlertKind::Info}}},
        {L"进行中", {{L"虚拟滚动优化", L"P0", AlertKind::Error},
                     {L"深色主题适配", L"P2", AlertKind::Info}}},
        {L"已完成", {{L"Overlay 焦点陷阱", L"P0", AlertKind::Success},
                     {L"控件平台迁移", L"P1", AlertKind::Success}}},
    };
    s.gantt.days = 14;
    s.gantt.today = 5;
    s.gantt.tasks = {
        {L"需求梳理", 0, 3, 100, AlertKind::Success},
        {L"控件实现", 2, 6, 65, AlertKind::Info},
        {L"视觉走查", 7, 3, 20, AlertKind::Warning},
        {L"发布验证", 10, 4, 0, AlertKind::Info},
    };

    // --- P3 overlay ---
    s.snackbar.message = L"已删除 1 条记录";
    s.snackbar.action = L"撤销";
    s.palette.query = L"theme";
    s.palette.items = {
        {L"切换主题", L"外观", L"Ctrl+T", L"Theme", false},
        {L"主题编辑器", L"外观", L"", L"Theme", false},
        {L"打开设置", L"通用", L"Ctrl+,", L"General", false},
        {L"导出数据", L"通用", L"", L"General", false},
        {L"查看快捷键", L"帮助", L"?", L"Help", false},
        {L"插件市场（未开放）", L"扩展", L"", L"Plugin", true},
    };
    s.guide.title = L"欢迎使用 AirUI";
    s.guide.body = L"这里可以查看全部自绘控件，点击导航可快速跳转，滚轮可浏览长列表。";
    s.guide.step = 1;
    s.guide.total = 3;

    // --- P4 editors ---
    s.query.conditions = {
        {L"AND", L"记录状态", L"=", L"进行中"},
        {L"OR", L"负责人", L"包含", L"张"},
        {L"AND", L"创建时间", L">", L"2026-01-01"},
    };
    s.property.items = {
        {L"基本信息", L"名称", L"季度经营报表", PropertyKind::Text, true},
        {L"基本信息", L"记录行数", L"240", PropertyKind::Number, true},
        {L"基本信息", L"启用缓存", L"true", PropertyKind::Bool, true},
        {L"显示", L"主题", L"浅色", PropertyKind::Choice, true},
        {L"显示", L"只读模式", L"false", PropertyKind::Bool, false},
    };
    s.property.selected = 3;
    s.schedule.days = {L"周一", L"周二", L"周三", L"周四", L"周五"};
    s.schedule.startHour = 9;
    s.schedule.endHour = 17;
    s.schedule.today = 2;
    s.schedule.events = {
        {L"需求评审", 0, 9, 11, AlertKind::Info},
        {L"设计走查", 1, 11, 12, AlertKind::Warning},
        {L"版本发布", 2, 14, 16, AlertKind::Success},
        {L"回归测试", 2, 14, 15, AlertKind::Error},
        {L"项目周会", 4, 16, 17, AlertKind::Info},
    };
    s.dropZone.files = {
        {L"report-q3.xlsx", L"已完成", 100, AlertKind::Success},
        {L"screenshot.png", L"上传中", 62, AlertKind::Info},
        {L"archive.zip", L"失败", 20, AlertKind::Error},
    };

    // --- P4 data ---
    s.treeMap.items = {
        {L"核心模块", 42, 0}, {L"数据层", 26, 0}, {L"界面", 18, 0},
        {L"测试", 9, 0}, {L"文档", 5, 0},
    };
    s.funnel.stages = {
        {L"访问页面", 12000}, {L"注册账号", 4800},
        {L"完成激活", 2100}, {L"付费转化", 640},
    };
    s.diff.file = L"src/airui/core/src/Theme.cpp";
    s.diff.lines = {
        {DiffKind::Context, L"const Palette& Pal() { return current; }"},
        {DiffKind::Removed, L"// legacy hard-coded palette"},
        {DiffKind::Added, L"// semantic palette tokens"},
        {DiffKind::Context, L"void SetPalette(const Palette& p) {"},
        {DiffKind::Added, L"    current = p;"},
        {DiffKind::Context, L"}"},
    };
    s.score.average = 4.6;
    s.score.total = 1280;
    s.score.counts = {820, 280, 110, 50, 20};

    // --- P4 shell ---
    s.chat.messages = {
        {L"李", L"这版控件预览看起来不错，气泡排版没有重叠。", L"09:12", false},
        {L"我", L"好的，我再把深色主题和 144 DPI 缩放跑一遍。", L"09:14", true},
        {L"李", L"收到，记得检查裁剪区域。", L"09:15", false},
    };
    s.multiStatus.message = L"已连接到本地数据库";
    s.multiStatus.trailing = L"UTF-8";
    s.multiStatus.segments = {
        {L"同步正常", AlertKind::Success, true},
        {L"3 个任务", AlertKind::Warning, false},
        {L"只读", AlertKind::Info, false},
    };
    s.ribbon.groups = {
        {L"剪贴板", {{L"粘贴", L"贴", true, false}}},
        {L"编辑", {{L"撤销", L"撤", false, false},
                   {L"重做", L"做", false, false},
                   {L"查找", L"查", false, false}}},
        {L"视图", {{L"布局", L"局", true, false},
                   {L"缩放", L"放", false, false}}},
    };
    s.notice.items = {
        {L"构建完成", L"MSVC Release 全部目标已通过", L"2 分钟前", L"CI", true, AlertKind::Success},
        {L"同步异常", L"凭据缓存写入被拒绝，请检查权限", L"18 分钟前", L"Git", true, AlertKind::Error},
        {L"新增评论", L"控件契约头已审阅", L"1 小时前", L"评审", false, AlertKind::Info},
        {L"磁盘空间", L"剩余空间低于 20%", L"昨天", L"系统", false, AlertKind::Warning},
    };

    // --- P5 charts ---
    s.spark.values = {12.4, 13.1, 12.8, 14.6, 15.2, 14.9, 16.8,
                      17.5, 17.1, 18.6, 19.4, 20.2};
    s.spark.change = 8.4;
    s.waterfall.bars = {
        {L"期初", 120.0, true}, {L"新增", 45.0, false},
        {L"退款", -18.0, false}, {L"续费", 30.0, false},
        {L"期末", 177.0, true},
    };
    s.stacked.legend = {L"直营", L"渠道", L"线上", L"其他"};
    s.stacked.rows = {
        {L"华东", {42, 28, 20, 10}}, {L"华南", {35, 30, 25, 10}},
        {L"华北", {50, 20, 18, 12}}, {L"西南", {25, 35, 30, 10}},
    };
    s.gauge.value = 84.0;
    s.gauge.warnAt = 80.0;
    s.gauge.segments = 24;

    // --- P5 editors ---
    s.jsonTree.root = L"response";
    s.jsonTree.nodes = {
        {L"code", L"200", 1, false, true},
        {L"data", L"{...}", 1, true, true},
        {L"items", L"[3]", 2, true, true},
        {L"id", L"1001", 3, false, true},
        {L"name", L"控件库", 3, false, true},
        {L"enabled", L"true", 3, false, true},
        {L"message", L"ok", 1, false, true},
    };
    s.logs.filter = L"缓存";
    s.logs.entries = {
        {L"09:12:01", LogLevel::Info, L"app", L"启动完成，加载配置"},
        {L"09:12:03", LogLevel::Debug, L"db", L"连接池初始化 8 个连接"},
        {L"09:13:20", LogLevel::Info, L"cache", L"缓存预热完成"},
        {L"09:14:05", LogLevel::Warning, L"net", L"请求重试 2 次"},
        {L"09:15:41", LogLevel::Error, L"db", L"缓存写入超时"},
        {L"09:16:02", LogLevel::Info, L"app", L"恢复正常运行"},
    };
    s.cron.fields = {L"*/5", L"*", L"*", L"*", L"1-5"};
    s.cron.captions = {L"分", L"时", L"日", L"月", L"周"};
    s.cron.description = L"每 5 分钟执行一次，仅工作日生效";
    s.cron.presets = {L"每小时", L"每天", L"每周", L"自定义"};
    s.cron.selectedPreset = 0;
    s.mention.text = L"@";
    s.mention.showCandidates = true;
    s.mention.focused = true;
    s.mention.candidates = {
        {L"张伟", L"产品 · 在线"}, {L"李娜", L"设计 · 忙碌"},
        {L"王强", L"研发 · 在线"}, {L"赵敏", L"测试 · 离开"},
    };

    // --- P5 collaboration ---
    s.org.nodes = {
        {L"周明", L"负责人", -1}, {L"张伟", L"产品组", 0},
        {L"李娜", L"设计组", 0}, {L"王强", L"研发组", 0},
        {L"赵敏", L"测试", 3}, {L"陈晨", L"前端", 3},
    };
    s.leaders.rows = {
        {L"张伟", L"提交 48 次", 980, 2}, {L"李娜", L"提交 41 次", 875, -1},
        {L"王强", L"提交 39 次", 810, 1}, {L"赵敏", L"提交 32 次", 690, 0},
        {L"陈晨", L"提交 27 次", 605, -2},
    };
    s.feed.items = {
        {L"张伟", L"合并了", L"控件库 PR #42", L"2 分钟前", AlertKind::Success, true},
        {L"李娜", L"评论了", L"视觉走查", L"18 分钟前", AlertKind::Info, true},
        {L"王强", L"修复了", L"滚动闪烁", L"1 小时前", AlertKind::Warning, false},
        {L"赵敏", L"报告了", L"构建失败", L"昨天", AlertKind::Error, false},
    };
    s.wave.peaks.clear();
    for (int i = 0; i < 48; ++i) {
        const double v = 0.25 + 0.7 *
            (static_cast<double>((i * 7) % 11) / 10.0);
        s.wave.peaks.push_back(v > 1.0 ? 1.0 : v);
    }
    s.wave.position = 0.42;
    s.wave.duration = 184.0;
    s.wave.playing = true;

    // --- P6: widgets own their state; no caller-held XxxState ---
    s.wButton.text = L"类化按钮";
    s.wButton.style = ButtonStyle::Primary;
    s.wButton.SetName(L"保存当前设置");
    s.wCheckbox.text = L"启用自动同步";
    s.wCheckbox.state.checked = true;
    s.wTextField.state.text = L"状态由对象持有";
    s.wSlider.state.value = 65;
    s.wSparkline.state.title = L"对象化走势";
    s.wSparkline.state.values = {12, 15, 13, 18, 17, 22, 20, 26, 24, 30};
    s.wSparkline.state.change = 12.5;
    s.wGauge.state.title = L"资源占用";
    s.wGauge.state.value = 62;
    s.wRatingBar.value = 3; s.wRatingBar.max = 5; s.wRatingBar.SetName(L"对象化评分");
    s.wIconBtn.text = L"保存"; s.wIconBtn.icon = L"★";
    s.wIconBtn.style = ButtonStyle::Primary;
}

inline Rect GuideAnchorRect(const Rect& rc, const Metrics& mx)
{
    return Rect(rc.left + mx.unit * 4, rc.top + mx.unit * 2,
                rc.left + mx.unit * 18, rc.top + mx.unit * 6);
}

// ============================================================
// Demo command implementations.
// Each block below is one control's Paint / Click / Wheel, lifted
// verbatim from the previous switch statements. Keeping them as
// named commands makes the registry (Demos()) read as a flat,
// declarative table and removes the four-way duplication.
// ============================================================

// --- Paint commands (simple cases: forward to the core PaintXxx) ---
#define DEMO_PAINT(Name, Member, Fn)                                            \
    inline void DemoPaint_##Name(IPainter& g, const Rect& rc, ExtraDemoState& s, \
                                const Palette& pal, const Metrics& mx) {        \
        Fn(g, rc, s.Member, pal, mx);                                           \
    }
DEMO_PAINT(TextBox, textbox, PaintTextBox)
DEMO_PAINT(Card, card, PaintCard)
DEMO_PAINT(EmptyView, emptyView, PaintEmptyView)
DEMO_PAINT(Toolbar, toolbar, PaintToolbar)
DEMO_PAINT(Form, form, PaintForm)
DEMO_PAINT(DataGrid, grid, PaintDataGrid)
DEMO_PAINT(Custom, custom, PaintCustom)
DEMO_PAINT(SearchBox, search, PaintSearchBox)
DEMO_PAINT(PasswordBox, password, PaintPasswordBox)
DEMO_PAINT(TextArea, textArea, PaintTextArea)
DEMO_PAINT(NumberInput, number, PaintNumberInput)
DEMO_PAINT(DatePicker, date, PaintDatePicker)
DEMO_PAINT(ColorPicker, color, PaintColorPicker)
DEMO_PAINT(Dropdown, dropdown, PaintDropdown)
DEMO_PAINT(Menu, menu, PaintMenu)
DEMO_PAINT(MultiSelect, multi, PaintMultiSelect)
DEMO_PAINT(Transfer, transfer, PaintTransfer)
DEMO_PAINT(Timeline, timeline, PaintTimeline)
DEMO_PAINT(Steps, steps, PaintSteps)
DEMO_PAINT(Calendar, calendar, PaintCalendar)
DEMO_PAINT(DescriptionList, description, PaintDescriptionList)
DEMO_PAINT(StatCard, stat, PaintStatCard)
DEMO_PAINT(Chart, chart, PaintChart)
DEMO_PAINT(Carousel, carousel, PaintCarousel)
DEMO_PAINT(Modal, modal, PaintModal)
DEMO_PAINT(Drawer, drawer, PaintDrawer)
DEMO_PAINT(Notification, notification, PaintNotificationQueue)
DEMO_PAINT(Result, result, PaintResult)
DEMO_PAINT(Accordion, accordion, PaintAccordion)
DEMO_PAINT(Anchor, anchor, PaintAnchor)
DEMO_PAINT(Splitter, splitter, PaintSplitter)
DEMO_PAINT(ColumnResize, columnResize, PaintColumnResize)
DEMO_PAINT(TagInput, tagInput, PaintTagInput)
DEMO_PAINT(RangeSlider, rangeSlider, PaintRangeSlider)
DEMO_PAINT(OtpInput, otp, PaintOtpInput)
DEMO_PAINT(Cascader, cascader, PaintCascader)
DEMO_PAINT(AvatarGroup, avatars, PaintAvatarGroup)
DEMO_PAINT(Heatmap, heatmap, PaintHeatmap)
DEMO_PAINT(CodeBlock, codeBlock, PaintCodeBlock)
DEMO_PAINT(Kanban, kanban, PaintKanban)
DEMO_PAINT(Gantt, gantt, PaintGantt)
DEMO_PAINT(Snackbar, snackbar, PaintSnackbar)
DEMO_PAINT(CommandPalette, palette, PaintCommandPalette)
DEMO_PAINT(QueryBuilder, query, PaintQueryBuilder)
DEMO_PAINT(PropertyGrid, property, PaintPropertyGrid)
DEMO_PAINT(Schedule, schedule, PaintSchedule)
DEMO_PAINT(FileDropZone, dropZone, PaintFileDropZone)
DEMO_PAINT(TreeMap, treeMap, PaintTreeMap)
DEMO_PAINT(Funnel, funnel, PaintFunnel)
DEMO_PAINT(Diff, diff, PaintDiff)
DEMO_PAINT(Score, score, PaintScore)
DEMO_PAINT(Chat, chat, PaintChat)
DEMO_PAINT(MultiStatusBar, multiStatus, PaintMultiStatusBar)
DEMO_PAINT(Ribbon, ribbon, PaintRibbon)
DEMO_PAINT(NoticeCenter, notice, PaintNoticeCenter)
DEMO_PAINT(Sparkline, spark, PaintSparkline)
DEMO_PAINT(Waterfall, waterfall, PaintWaterfall)
DEMO_PAINT(StackedBar, stacked, PaintStackedBar)
DEMO_PAINT(Gauge, gauge, PaintGauge)
DEMO_PAINT(JsonTree, jsonTree, PaintJsonTree)
DEMO_PAINT(LogViewer, logs, PaintLogViewer)
DEMO_PAINT(CronEditor, cron, PaintCronEditor)
DEMO_PAINT(MentionInput, mention, PaintMentionInput)
DEMO_PAINT(OrgChart, org, PaintOrgChart)
DEMO_PAINT(Leaderboard, leaders, PaintLeaderboard)
DEMO_PAINT(ActivityFeed, feed, PaintActivityFeed)
DEMO_PAINT(Waveform, wave, PaintWaveform)
#undef DEMO_PAINT

// --- Paint commands (special cases) ---
inline void DemoPaint_Framework(IPainter& g, const Rect& rc, ExtraDemoState& s,
                                const Palette& pal, const Metrics& mx)
{
    g.Translate(Point{rc.left, rc.top});
    s.host.PaintAll(g, pal, mx);
    g.Translate(Point{-rc.left, -rc.top});
    g.Text(Rect(rc.left, rc.bottom - 28, rc.right, rc.bottom),
           L"ControlHost：z-order 命中 / 事件路由 / Tab 焦点 · 点击重叠区试试",
           FontRole::Meta, pal.textSub,
           dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis);
}

inline void DemoPaint_ScrollVirtual(IPainter& g, const Rect& rc, ExtraDemoState& s,
                                    const Palette& pal, const Metrics& mx)
{
    s.virtualList.scroll.viewH = rc.Height();
    s.virtualList.scroll.contentH = ListDataContentHeight(s.virtualList, mx);
    PaintListData(g, rc, s.virtualList, pal, mx);
}

inline void DemoPaint_DpiScaling(IPainter& g, const Rect& rc, ExtraDemoState& s,
                                 const Palette& pal, const Metrics& mx)
{
    const Rect scaled = ScaleRect(Rect(0, 0, 100, 32), s.dpi);
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    g.Text(Rect(rc.left + mx.unit, rc.top, rc.right - mx.unit, rc.bottom),
           L"DPI " + std::to_wstring(s.dpi) + L" · unit=" +
           std::to_wstring(MetricsForDpi(s.dpiBase, s.dpi).unit) +
           L" · 100x32 → " + std::to_wstring(scaled.Width()) + L"x" +
           std::to_wstring(scaled.Height()) + L"（按 D 键切换 96/120/144）",
           FontRole::Body, pal.text,
           dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis);
}

inline void DemoPaint_Animation(IPainter& g, const Rect& rc, ExtraDemoState& s,
                                const Palette& pal, const Metrics& mx)
{
    SpinnerState sp = s.spinner;
    sp.phase = s.animPhase;
    PaintSpinner(g, Rect(rc.left, rc.top, rc.left + mx.unit * 20, rc.bottom), sp, pal, mx);
    ControlState cs;
    PaintProgressBar(g, Rect(rc.left + mx.unit * 22, rc.top + rc.Height() / 2 - 4,
                             rc.right - mx.unit, rc.top + rc.Height() / 2 + 4),
                     s.animPhase, cs, pal, mx);
}

inline void DemoPaint_Accessibility(IPainter& g, const Rect& rc, ExtraDemoState& s,
                                    const Palette& pal, const Metrics& mx)
{
    g.FillRound(rc, mx.radiusMd, pal.sunken);
    g.TextBlock(rc, L"无障碍树：\n" + DescribeAll(s.host),
                FontRole::Meta, pal.textSub, 4);
}

inline void DemoPaint_StatusTag(IPainter& g, const Rect& rc, ExtraDemoState& s,
                                const Palette& pal, const Metrics& mx)
{
    const int w = rc.Width() / 4;
    const AlertKind kinds[] = { AlertKind::Info, AlertKind::Success,
                                AlertKind::Warning, AlertKind::Error };
    for (int i = 0; i < 4; ++i) {
        StatusTagState st = s.statusTag;
        st.kind = kinds[i];
        st.text = kinds[i] == AlertKind::Info ? L"信息"
                : kinds[i] == AlertKind::Success ? L"成功"
                : kinds[i] == AlertKind::Warning ? L"警告" : L"错误";
        PaintStatusTag(g, Rect(rc.left + i * w + mx.unit, rc.top + mx.unit,
                               rc.left + (i + 1) * w - mx.unit,
                               rc.bottom - mx.unit), st, pal, mx);
    }
}

inline void DemoPaint_KbdChip(IPainter& g, const Rect& rc, ExtraDemoState& s,
                              const Palette& pal, const Metrics& mx)
{
    const int w = MeasureKbdChip(s.kbd, mx);
    PaintKbdChip(g, Rect(rc.right - w - mx.unit * 2, rc.top + mx.unit,
                         rc.right - mx.unit * 2, rc.bottom - mx.unit),
                 s.kbd, pal, mx);
    KbdChipState alt; alt.key = L"Alt"; alt.label = L"+ F4 关闭";
    const int w2 = MeasureKbdChip(alt, mx);
    PaintKbdChip(g, Rect(rc.left + mx.unit, rc.top + mx.unit,
                         rc.left + mx.unit + w2, rc.bottom - mx.unit),
                 alt, pal, mx);
}

inline void DemoPaint_ListData(IPainter& g, const Rect& rc, ExtraDemoState& s,
                               const Palette& pal, const Metrics& mx)
{
    s.listData.scroll.viewH = rc.Height();
    s.listData.scroll.contentH = ListDataContentHeight(s.listData, mx);
    PaintListData(g, rc, s.listData, pal, mx);
}

inline void DemoPaint_ContextMenu(IPainter& g, const Rect& rc, ExtraDemoState& s,
                                  const Palette& pal, const Metrics& mx)
{
    s.contextMenu.anchor = Point{rc.left + mx.unit * 8, rc.top + mx.unit * 2};
    PaintContextMenu(g, rc, s.contextMenu, pal, mx);
}

inline void DemoPaint_Spinner(IPainter& g, const Rect& rc, ExtraDemoState& s,
                              const Palette& pal, const Metrics& mx)
{
    SpinnerState sp = s.spinner;
    sp.phase = s.animPhase;
    PaintSpinner(g, rc, sp, pal, mx);
}

inline void DemoPaint_Tooltip(IPainter& g, const Rect& rc, ExtraDemoState& s,
                              const Palette& pal, const Metrics& mx)
{
    PaintTooltip(g, Rect(rc.left + mx.unit * 4, rc.top + mx.unit,
                         rc.left + mx.unit * 18, rc.top + mx.unit * 5),
                 rc, s.tooltip, pal, mx);
}

inline void DemoPaint_Popover(IPainter& g, const Rect& rc, ExtraDemoState& s,
                              const Palette& pal, const Metrics& mx)
{
    PaintPopover(g, Rect(rc.left + mx.unit * 4, rc.top,
                         rc.left + mx.unit * 18, rc.top + mx.unit * 4),
                 rc, s.popover, pal, mx);
}

inline void DemoPaint_Scrollbar(IPainter& g, const Rect& rc, ExtraDemoState& s,
                                const Palette& pal, const Metrics& mx)
{
    g.FillRound(rc, mx.radiusMd, pal.surface);
    g.StrokeRound(rc, mx.radiusMd, pal.split, 1);
    s.demoScroll.viewH = rc.Height();
    PaintScrollbar(g, rc, s.demoScroll, pal, mx);
    g.Text(Rect(rc.left + mx.unit, rc.top, rc.right - mx.unit * 3, rc.bottom),
           L"自绘滚动条 · offset=" + std::to_wstring(s.demoScroll.offset) +
           L" / max=" + std::to_wstring(ScrollMax(s.demoScroll)) +
           L"（在此区域滚轮）",
           FontRole::Meta, pal.textSub,
           dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis);
}

inline void DemoPaint_GuideTour(IPainter& g, const Rect& rc, ExtraDemoState& s,
                                const Palette& pal, const Metrics& mx)
{
    PaintGuideTour(g, rc, GuideAnchorRect(rc, mx), s.guide, pal, mx);
}

inline void DemoPaint_WidgetClasses(IPainter& g, const Rect& rc, ExtraDemoState& s,
                                    const Palette& pal, const Metrics& mx)
{
    const int n = 8;
    const int gap = mx.unit;
    const int cols = 3;
    const int rows = (n + cols - 1) / cols;               // 3 rows for 8 tiles
    const int cw = (rc.Width() - gap * (cols - 1)) / cols;
    const int ch = (rc.Height() - gap * (rows - 1)) / rows;
    if (cw <= 0 || ch <= 0) return;
    Control* items[8] = {&s.wButton, &s.wCheckbox, &s.wTextField,
                         &s.wSlider, &s.wSparkline, &s.wGauge,
                         &s.wRatingBar, &s.wIconBtn};
    const wchar_t* names[8] = {
        L"widgets::Button", L"widgets::Checkbox", L"widgets::TextField",
        L"widgets::Slider", L"widgets::Sparkline", L"widgets::Gauge",
        L"widgets::RatingBar", L"widgets::IconButton"};
    const TextFormat tf = dt::SingleLine | dt::Left | dt::VCenter |
                          dt::NoPrefix | dt::EndEllipsis;
    for (int i = 0; i < n; ++i) {
        const int col = i % cols;
        const int row = i / cols;
        const Rect tile(rc.left + col * (cw + gap),
                        rc.top + row * (ch + gap),
                        rc.left + col * (cw + gap) + cw,
                        rc.top + row * (ch + gap) + ch);
        g.FillRound(tile, mx.radiusMd, pal.surface);
        g.StrokeRound(tile, mx.radiusMd, pal.border, 1);
        const int labelH = (std::min)(mx.rowH, ch / 3);
        g.Text(Rect(tile.left + gap, tile.top + 2,
                    tile.right - gap, tile.top + labelH),
               names[i], FontRole::Meta, pal.textSub, tf);
        const Rect body(tile.left + gap, tile.top + labelH,
                        tile.right - gap, tile.bottom - gap);
        items[i]->SetBounds(body);
        items[i]->Paint(g, pal, mx);
    }
}

// --- Click commands ---
inline void DemoClick_Framework(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                const Metrics&, std::wstring& status)
{
    const Point local{pt.x - rc.left, pt.y - rc.top};
    PointerEvent ev; ev.pos = local; ev.down = true; ev.button = 0;
    const ControlResult r = s.host.DispatchPointer(ev);
    status = r.handled ? L"ControlHost：命中控件 #" + std::to_wstring(r.id)
                       : L"ControlHost：空白区域";
}

inline void DemoClick_VirtualList(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                  const Metrics& mx, std::wstring& status)
{
    ListDataState& target = s.virtualList;
    const int row = ListDataRowAt(rc, target, mx, pt);
    if (row >= 0) {
        target.selected = row;
        status = L"ListData：选中第 " + std::to_wstring(row + 1) + L" 行";
    }
}

inline void DemoClick_TextBox(int, Point pt, const Rect& rc, ExtraDemoState& s,
                              const Metrics& mx, std::wstring& status)
{
    const int charW = 8;
    s.textbox.edit.cursor = CaretIndexAt(rc, s.textbox.edit, mx, charW, pt);
    s.textbox.edit.selectionAnchor = -1;
    s.textbox.edit.hasFocus = true;
    status = L"TextBox：光标定位，可用键盘编辑";
}

inline void DemoClick_Toolbar(int, Point pt, const Rect& rc, ExtraDemoState& s,
                              const Metrics& mx, std::wstring& status)
{
    const ToolbarHit h = HitToolbar(rc, s.toolbar, mx, pt);
    if (h.action >= 0) status = L"Toolbar：点击动作 #" + std::to_wstring(h.action + 1);
    else if (h.crumb >= 0) status = L"Toolbar：面包屑 " + s.toolbar.crumbs[static_cast<size_t>(h.crumb)];
    else if (h.search) status = L"Toolbar：搜索框";
}

inline void DemoClick_Form(int, Point pt, const Rect& rc, ExtraDemoState& s,
                           const Metrics& mx, std::wstring& status)
{
    const FormHit h = HitForm(rc, s.form, mx, pt);
    if (h.field >= 0) {
        s.form.focusedField = h.field;
        status = L"Form：聚焦字段 " + s.form.fields[static_cast<size_t>(h.field)].label;
    } else if (h.submit) {
        const int bad = ValidateForm(s.form);
        status = bad < 0 ? L"Form：校验通过，已提交"
                         : L"Form：字段 " + s.form.fields[static_cast<size_t>(bad)].label + L" 必填";
    }
}

inline void DemoClick_DataGrid(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const DataGridHit h = HitDataGrid(rc, s.grid, mx, pt);
    if (h.resize >= 0) {
        s.grid.resizingColumn = h.resize;
        status = L"DataGrid：拖拽列宽 #" + std::to_wstring(h.resize + 1);
    } else if (h.header && h.column >= 0) {
        if (s.grid.sortColumn == h.column) s.grid.sortAscending = !s.grid.sortAscending;
        else { s.grid.sortColumn = h.column; s.grid.sortAscending = true; }
        status = L"DataGrid：按列 " + s.grid.columns[static_cast<size_t>(h.column)].header + L" 排序";
    } else if (h.row >= 0) {
        s.grid.selected = h.row;
        status = L"DataGrid：选中第 " + std::to_wstring(h.row + 1) + L" 行";
    }
}

inline void DemoClick_ListData(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    ListDataState& target = s.listData;
    const int row = ListDataRowAt(rc, target, mx, pt);
    if (row >= 0) {
        target.selected = row;
        status = L"ListData：选中第 " + std::to_wstring(row + 1) + L" 行";
    }
}

inline void DemoClick_SearchBox(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                const Metrics& mx, std::wstring& status)
{
    if (HitSearchBox(rc, s.search, mx, pt) == SearchBoxHit::Clear) {
        s.search.text.clear();
        status = L"SearchBox：已清空";
    }
}

inline void DemoClick_PasswordBox(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                  const Metrics& mx, std::wstring& status)
{
    if (HitPasswordBox(rc, s.password, mx, pt) == PasswordBoxHit::Reveal) {
        s.password.revealed = !s.password.revealed;
        status = s.password.revealed ? L"PasswordBox：已显示" : L"PasswordBox：已隐藏";
    }
}

inline void DemoClick_NumberInput(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                  const Metrics& mx, std::wstring& status)
{
    const NumberInputHit h = HitNumberInput(rc, s.number, mx, pt);
    if (h == NumberInputHit::Plus)  s.number.value = (std::min)(s.number.max, s.number.value + s.number.step);
    if (h == NumberInputHit::Minus) s.number.value = (std::max)(s.number.min, s.number.value - s.number.step);
    status = L"NumberInput：value=" + std::to_wstring(s.number.value);
}

inline void DemoClick_DatePicker(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                 const Metrics& mx, std::wstring& status)
{
    const int day = DatePickerDayAt(rc, s.date, mx, pt);
    if (day > 0) { s.date.day = day; status = L"DatePicker：选择 " + std::to_wstring(day) + L" 日"; }
    else if (day == -2 || day == -3) {
        s.date.month += day == -2 ? -1 : 1;
        if (s.date.month < 1) { s.date.month = 12; --s.date.year; }
        if (s.date.month > 12) { s.date.month = 1; ++s.date.year; }
        status = L"DatePicker：切换月份";
    }
}

inline void DemoClick_ColorPicker(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                  const Metrics& mx, std::wstring& status)
{
    const int i = ColorPickerSwatchAt(rc, s.color, mx, pt);
    if (i >= 0) { s.color.selected = i; status = L"ColorPicker：选择色块 #" + std::to_wstring(i + 1); }
}

inline void DemoClick_Dropdown(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const int o = DropdownOptionAt(rc, s.dropdown, mx, pt);
    if (o >= 0) {
        s.dropdown.selected = o;
        s.dropdown.text = s.dropdown.options[static_cast<size_t>(o)];
        status = L"Dropdown：选择 " + s.dropdown.text;
    } else if (o == -1) {
        s.dropdown.open = !s.dropdown.open;
        status = s.dropdown.open ? L"Dropdown：展开" : L"Dropdown：收起";
    }
}

inline void DemoClick_Menu(int, Point pt, const Rect& rc, ExtraDemoState& s,
                           const Metrics& mx, std::wstring& status)
{
    const int i = MenuItemAt(rc, s.menu, mx, pt);
    if (i >= 0) {
        s.menu.selected = i;
        status = L"Menu：选择 " + s.menu.items[static_cast<size_t>(i)].text;
    }
}

inline void DemoClick_MultiSelect(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                  const Metrics& mx, std::wstring& status)
{
    const int i = MultiSelectAt(rc, s.multi, mx, pt);
    if (i >= 0 && i < static_cast<int>(s.multi.checked.size())) {
        s.multi.checked[static_cast<size_t>(i)] = !s.multi.checked[static_cast<size_t>(i)];
        status = L"MultiSelect：切换 " + s.multi.options[static_cast<size_t>(i)];
    }
}

inline void DemoClick_Transfer(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const TransferHit h = HitTransfer(rc, s.transfer, mx, pt);
    if (h.button != 0) {
        const int row = h.button == 1 ? 0 : 0;
        MoveTransferItem(s.transfer, h.button == 1 ? 0 : 1, row);
        status = L"Transfer：移动条目";
    } else if (h.row >= 0) {
        if (h.side == 0 && h.row < static_cast<int>(s.transfer.leftChecked.size()))
            s.transfer.leftChecked[static_cast<size_t>(h.row)] = !s.transfer.leftChecked[static_cast<size_t>(h.row)];
        if (h.side == 1 && h.row < static_cast<int>(s.transfer.rightChecked.size()))
            s.transfer.rightChecked[static_cast<size_t>(h.row)] = !s.transfer.rightChecked[static_cast<size_t>(h.row)];
        status = L"Transfer：勾选条目";
    }
}

inline void DemoClick_Steps(int, Point pt, const Rect& rc, ExtraDemoState& s,
                            const Metrics& mx, std::wstring& status)
{
    const int i = StepAt(rc, s.steps, mx, pt);
    if (i >= 0) { s.steps.current = i; status = L"Steps：跳到第 " + std::to_wstring(i + 1) + L" 步"; }
}

inline void DemoClick_Calendar(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const int d = CalendarDayAt(rc, s.calendar, mx, pt);
    if (d > 0) { s.calendar.selectedDay = d; status = L"Calendar：选择 " + std::to_wstring(d) + L" 日"; }
}

inline void DemoClick_Chart(int, Point pt, const Rect& rc, ExtraDemoState& s,
                            const Metrics& mx, std::wstring& status)
{
    const int i = ChartBarAt(rc, s.chart, mx, pt);
    if (i >= 0) { s.chart.hovered = i; status = L"Chart：柱 #" + std::to_wstring(i + 1); }
}

inline void DemoClick_Carousel(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const CarouselHitResult h = HitCarousel(rc, s.carousel, mx, pt);
    const int n = static_cast<int>(s.carousel.slides.size());
    if (h.code == 1) s.carousel.current = (s.carousel.current - 1 + n) % n;
    else if (h.code == 2) s.carousel.current = (s.carousel.current + 1) % n;
    else if (h.code == 3 && h.index >= 0) s.carousel.current = h.index % n;
    if (h.code != 0) status = L"Carousel：第 " + std::to_wstring(s.carousel.current + 1) + L" 页";
}

inline void DemoClick_Modal(int, Point pt, const Rect& rc, ExtraDemoState& s,
                            const Metrics& mx, std::wstring& status)
{
    const int b = ModalButtonAt(rc, s.modal, mx, pt);
    if (b > 0) { s.modal.focused = b; status = b == 1 ? L"Modal：确定" : L"Modal：取消"; }
}

inline void DemoClick_Result(int, Point pt, const Rect& rc, ExtraDemoState& s,
                             const Metrics& mx, std::wstring& status)
{
    if (ResultActionHit(rc, s.result, mx, pt))
        status = L"Result：点击返回首页";
}

inline void DemoClick_Accordion(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                const Metrics& mx, std::wstring& status)
{
    const int i = AccordionHeaderAt(rc, s.accordion, mx, pt);
    if (i >= 0) {
        if (i >= static_cast<int>(s.accordion.expanded.size()))
            s.accordion.expanded.resize(static_cast<size_t>(i + 1), false);
        s.accordion.expanded[static_cast<size_t>(i)] = !s.accordion.expanded[static_cast<size_t>(i)];
        status = L"Accordion：展开/折叠 " + s.accordion.panels[static_cast<size_t>(i)].first;
    }
}

inline void DemoClick_Anchor(int, Point pt, const Rect& rc, ExtraDemoState& s,
                             const Metrics& mx, std::wstring& status)
{
    const int i = AnchorHit(rc, s.anchor, mx, pt);
    if (i >= 0) { s.anchor.active = i; status = L"Anchor：跳转 " + s.anchor.items[static_cast<size_t>(i)]; }
}

inline void DemoClick_Splitter(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    if (SplitterHandleHit(rc, s.splitter, mx, pt)) {
        s.splitter.dragging = !s.splitter.dragging;
        status = s.splitter.dragging ? L"Splitter：开始拖拽" : L"Splitter：结束拖拽";
    }
}

inline void DemoClick_ColumnResize(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                   const Metrics& mx, std::wstring& status)
{
    const int i = ColumnResizeHandleAt(rc, s.columnResize, mx, pt);
    if (i >= 0) {
        s.columnResize.dragging = i;
        status = L"ColumnResize：拖拽边界 #" + std::to_wstring(i + 1);
    }
}

inline void DemoClick_Scrollbar(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                const Metrics& mx, std::wstring& status)
{
    const ScrollBarHit h = HitScrollbar(rc, s.demoScroll, mx, pt);
    if (h == ScrollBarHit::TrackAbove) ScrollPage(s.demoScroll, -1);
    else if (h == ScrollBarHit::TrackBelow) ScrollPage(s.demoScroll, 1);
    else if (h == ScrollBarHit::UpButton) ScrollBy(s.demoScroll, -s.demoScroll.lineH);
    else if (h == ScrollBarHit::DownButton) ScrollBy(s.demoScroll, s.demoScroll.lineH);
    status = L"Scrollbar：offset=" + std::to_wstring(s.demoScroll.offset);
}

inline void DemoClick_TagInput(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const TagInputHit h = HitTagInput(rc, s.tagInput, mx, pt);
    if (h.close && h.tag >= 0 && h.tag < static_cast<int>(s.tagInput.tags.size())) {
        const std::wstring removed = s.tagInput.tags[static_cast<size_t>(h.tag)];
        s.tagInput.tags.erase(s.tagInput.tags.begin() + h.tag);
        s.tagInput.hoveredTag = -1;
        status = L"TagInput：移除标签 " + removed;
    } else if (h.clearAll) {
        s.tagInput.tags.clear();
        status = L"TagInput：已清空全部标签";
    } else if (h.input) {
        s.tagInput.hasFocus = true;
        status = L"TagInput：输入区已聚焦";
    }
}

inline void DemoClick_RangeSlider(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                  const Metrics& mx, std::wstring& status)
{
    const int thumb = RangeSliderThumbAt(rc, s.rangeSlider, mx, pt);
    if (thumb == 1 || thumb == 2) {
        s.rangeSlider.draggingThumb = thumb;
        s.rangeSlider.hoveredThumb = thumb;
        status = thumb == 1 ? L"RangeSlider：拖拽下限" : L"RangeSlider：拖拽上限";
    } else if (thumb != -2) {
        const int v = RangeSliderValueAt(rc, s.rangeSlider, mx, pt);
        int dLow = v - s.rangeSlider.low;  if (dLow < 0) dLow = -dLow;
        int dHigh = v - s.rangeSlider.high; if (dHigh < 0) dHigh = -dHigh;
        if (dLow <= dHigh) s.rangeSlider.low = (std::min)(v, s.rangeSlider.high);
        else s.rangeSlider.high = (std::max)(v, s.rangeSlider.low);
        status = L"RangeSlider：区间 " + std::to_wstring(s.rangeSlider.low) +
                 L" – " + std::to_wstring(s.rangeSlider.high);
    }
}

inline void DemoClick_OtpInput(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const int cell = OtpCellAt(rc, s.otp, mx, pt);
    if (cell >= 0) {
        s.otp.focused = cell;
        status = L"OtpInput：聚焦第 " + std::to_wstring(cell + 1) + L" 格";
    }
}

inline void DemoClick_Cascader(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const CascaderHit h = HitCascader(rc, s.cascader, mx, pt);
    if (h.column >= 0 && h.row >= 0) {
        if (static_cast<int>(s.cascader.selected.size()) < h.column + 1)
            s.cascader.selected.resize(static_cast<size_t>(h.column + 1), -1);
        s.cascader.selected[static_cast<size_t>(h.column)] = h.row;
        for (int c = h.column + 1; c < static_cast<int>(s.cascader.selected.size()); ++c)
            s.cascader.selected[static_cast<size_t>(c)] = -1;
        status = L"Cascader：选择 " +
                 s.cascader.columns[static_cast<size_t>(h.column)]
                     .items[static_cast<size_t>(h.row)];
    }
}

inline void DemoClick_AvatarGroup(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                  const Metrics& mx, std::wstring& status)
{
    const int a = AvatarGroupAt(rc, s.avatars, mx, pt);
    if (a >= 0) {
        s.avatars.hovered = a;
        status = L"AvatarGroup：成员 " + s.avatars.items[static_cast<size_t>(a)].text;
    } else if (a == -2) {
        status = L"AvatarGroup：展开更多成员";
    }
}

inline void DemoClick_Heatmap(int, Point pt, const Rect& rc, ExtraDemoState& s,
                              const Metrics& mx, std::wstring& status)
{
    const HeatmapCell c = HeatmapCellAt(rc, s.heatmap, mx, pt);
    if (c.week >= 0) {
        s.heatmap.hoveredWeek = c.week;
        s.heatmap.hoveredDay = c.day;
        const int idx = c.week * 7 + c.day;
        const int level = (idx < static_cast<int>(s.heatmap.values.size()))
                        ? s.heatmap.values[static_cast<size_t>(idx)] : 0;
        status = L"Heatmap：第 " + std::to_wstring(c.week + 1) +
                 L" 周 · 强度 " + std::to_wstring(level);
    }
}

inline void DemoClick_CodeBlock(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                const Metrics& mx, std::wstring& status)
{
    if (CodeBlockCopyHit(rc, s.codeBlock, mx, pt)) {
        s.codeBlock.copyHover = true;
        status = L"CodeBlock：已复制代码";
    }
}

inline void DemoClick_Kanban(int, Point pt, const Rect& rc, ExtraDemoState& s,
                             const Metrics& mx, std::wstring& status)
{
    const KanbanHit h = HitKanban(rc, s.kanban, mx, pt);
    if (h.column >= 0 && h.card >= 0) {
        s.kanban.hoveredColumn = h.column;
        s.kanban.hoveredCard = h.card;
        status = L"KanbanBoard：选中卡片 " +
                 s.kanban.columns[static_cast<size_t>(h.column)]
                     .cards[static_cast<size_t>(h.card)].title;
    }
}

inline void DemoClick_Gantt(int, Point pt, const Rect& rc, ExtraDemoState& s,
                            const Metrics& mx, std::wstring& status)
{
    const int t = GanttTaskAt(rc, s.gantt, mx, pt);
    if (t >= 0) {
        s.gantt.hoveredTask = t;
        status = L"GanttChart：任务 " + s.gantt.tasks[static_cast<size_t>(t)].name;
    }
}

inline void DemoClick_Snackbar(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const SnackbarHit h = HitSnackbar(rc, s.snackbar, mx, pt);
    if (h == SnackbarHit::Action) {
        s.snackbar.actionHover = true;
        status = L"Snackbar：已撤销删除";
    } else if (h == SnackbarHit::Close) {
        s.snackbar.closeHover = true;
        status = L"Snackbar：关闭提示条";
    }
}

inline void DemoClick_CommandPalette(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                     const Metrics& mx, std::wstring& status)
{
    const CommandPaletteHit h = HitCommandPalette(rc, s.palette, mx, pt);
    if (h.row >= 0 && h.row < static_cast<int>(s.palette.items.size())) {
        const std::vector<int> matches = CommandPaletteMatches(s.palette);
        for (size_t k = 0; k < matches.size(); ++k) {
            if (matches[k] == h.row) s.palette.selected = static_cast<int>(k);
        }
        status = L"CommandPalette：执行 " + s.palette.items[static_cast<size_t>(h.row)].title;
    } else if (h.search) {
        status = L"CommandPalette：搜索框（可输入过滤）";
    }
}

inline void DemoClick_GuideTour(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                const Metrics& mx, std::wstring& status)
{
    const GuideTourHit h = HitGuideTour(rc, GuideAnchorRect(rc, mx), s.guide, mx, pt);
    if (h == GuideTourHit::Next) {
        s.guide.step = (std::min)(s.guide.total, s.guide.step + 1);
        status = L"GuideTour：第 " + std::to_wstring(s.guide.step) + L" 步";
    } else if (h == GuideTourHit::Prev) {
        s.guide.step = (std::max)(1, s.guide.step - 1);
        status = L"GuideTour：返回第 " + std::to_wstring(s.guide.step) + L" 步";
    } else if (h == GuideTourHit::Skip) {
        s.guide.step = 1;
        status = L"GuideTour：已跳过引导";
    }
}

inline void DemoClick_QueryBuilder(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                   const Metrics& mx, std::wstring& status)
{
    const QueryBuilderHit h = HitQueryBuilder(rc, s.query, mx, pt);
    s.query.hoveredRow = h.row;
    s.query.addHover = h.kind == QueryHitKind::Add;
    switch (h.kind) {
    case QueryHitKind::Logic: {
        QueryCondition& c = s.query.conditions[static_cast<size_t>(h.row)];
        c.logic = (c.logic == L"AND") ? L"OR" : L"AND";
        status = L"QueryBuilder：第 " + std::to_wstring(h.row + 1) +
                 L" 条切换为 " + c.logic;
        break;
    }
    case QueryHitKind::Field:
        s.query.hoveredCell = 1;
        status = L"QueryBuilder：编辑字段";
        break;
    case QueryHitKind::Operator:
        s.query.hoveredCell = 2;
        status = L"QueryBuilder：切换操作符";
        break;
    case QueryHitKind::Value:
        s.query.hoveredCell = 3;
        status = L"QueryBuilder：编辑取值";
        break;
    case QueryHitKind::Remove:
        if (h.row >= 0 && h.row < static_cast<int>(s.query.conditions.size())) {
            s.query.conditions.erase(s.query.conditions.begin() + h.row);
            status = L"QueryBuilder：已删除第 " + std::to_wstring(h.row + 1) + L" 条";
        }
        break;
    case QueryHitKind::Add:
        s.query.conditions.push_back({L"AND", L"新字段", L"=", L""});
        status = L"QueryBuilder：新增一条条件";
        break;
    default:
        status = L"QueryBuilder：点击条件行进行编辑";
        break;
    }
}

inline void DemoClick_PropertyGrid(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                   const Metrics& mx, std::wstring& status)
{
    const PropertyGridHit h = HitPropertyGrid(rc, s.property, mx, pt);
    if (h.item >= 0 && h.item < static_cast<int>(s.property.items.size())) {
        PropertyItem& item = s.property.items[static_cast<size_t>(h.item)];
        s.property.selected = h.item;
        s.property.hovered = h.item;
        if (h.onValue && item.enabled && item.kind == PropertyKind::Bool) {
            item.value = (item.value == L"true") ? L"false" : L"true";
            status = L"PropertyGrid：" + item.name + L" → " + item.value;
        } else if (h.onValue && item.enabled && item.kind == PropertyKind::Choice) {
            item.value = (item.value == L"浅色") ? L"深色" : L"浅色";
            status = L"PropertyGrid：" + item.name + L" → " + item.value;
        } else {
            status = L"PropertyGrid：选中 " + item.name;
        }
    }
}

inline void DemoClick_Schedule(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const int hit = ScheduleEventAt(rc, s.schedule, mx, pt);
    s.schedule.hovered = hit;
    if (hit >= 0 && hit < static_cast<int>(s.schedule.events.size())) {
        const ScheduleEvent& ev = s.schedule.events[static_cast<size_t>(hit)];
        status = L"ScheduleGrid：" + ev.title + L" · " +
                 std::to_wstring(ev.startHour) + L":00–" +
                 std::to_wstring(ev.endHour) + L":00";
    }
}

inline void DemoClick_FileDropZone(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                   const Metrics& mx, std::wstring& status)
{
    const FileDropZoneHit h = HitFileDropZone(rc, s.dropZone, mx, pt);
    s.dropZone.hovered = h.file;
    if (h.remove && h.file >= 0 && h.file < static_cast<int>(s.dropZone.files.size())) {
        s.dropZone.files.erase(s.dropZone.files.begin() + h.file);
        status = L"FileDropZone：已移除文件";
    } else if (h.drop) {
        s.dropZone.dragOver = !s.dropZone.dragOver;
        status = s.dropZone.dragOver ? L"FileDropZone：拖拽悬停态"
                                     : L"FileDropZone：取消拖拽态";
    } else if (h.file >= 0 && h.file < static_cast<int>(s.dropZone.files.size())) {
        UploadFile& f = s.dropZone.files[static_cast<size_t>(h.file)];
        f.progress = (f.progress >= 100) ? 0 : (std::min)(100, f.progress + 20);
        status = L"FileDropZone：" + f.name + L" 进度 " +
                 std::to_wstring(f.progress) + L"%";
    }
}

inline void DemoClick_TreeMap(int, Point pt, const Rect& rc, ExtraDemoState& s,
                              const Metrics& mx, std::wstring& status)
{
    const int hit = TreeMapTileAt(rc, s.treeMap, mx, pt);
    s.treeMap.hovered = hit;
    if (hit >= 0 && hit < static_cast<int>(s.treeMap.items.size())) {
        const TileItem& t = s.treeMap.items[static_cast<size_t>(hit)];
        status = L"TreeMap：" + t.label + L" · 权重 " + std::to_wstring(t.weight);
    }
}

inline void DemoClick_Funnel(int, Point pt, const Rect& rc, ExtraDemoState& s,
                             const Metrics& mx, std::wstring& status)
{
    const int hit = FunnelStageAt(rc, s.funnel, mx, pt);
    s.funnel.hovered = hit;
    if (hit >= 0 && hit < static_cast<int>(s.funnel.stages.size())) {
        const FunnelStage& f = s.funnel.stages[static_cast<size_t>(hit)];
        status = L"FunnelChart：" + f.label + L" · " + std::to_wstring(f.value) + L" 人";
    }
}

inline void DemoClick_Diff(int, Point pt, const Rect& rc, ExtraDemoState& s,
                           const Metrics& mx, std::wstring& status)
{
    const int hit = DiffLineAt(rc, s.diff, mx, pt);
    s.diff.hovered = hit;
    if (hit >= 0 && hit < static_cast<int>(s.diff.lines.size())) {
        const DiffLine& line = s.diff.lines[static_cast<size_t>(hit)];
        const wchar_t* kind = line.kind == DiffKind::Added ? L"新增"
                              : line.kind == DiffKind::Removed ? L"删除" : L"上下文";
        status = std::wstring(L"DiffView：第 ") + std::to_wstring(hit + 1) +
                 L" 行 · " + kind;
    }
}

inline void DemoClick_Score(int, Point pt, const Rect& rc, ExtraDemoState& s,
                            const Metrics& mx, std::wstring& status)
{
    const int hit = ScoreBarAt(rc, s.score, mx, pt);
    s.score.hovered = hit;
    if (hit >= 0 && hit < static_cast<int>(s.score.counts.size())) {
        status = L"ScoreBreakdown：" + std::to_wstring(5 - hit) +
                 L" 星 · " + std::to_wstring(s.score.counts[static_cast<size_t>(hit)]) +
                 L" 条";
    }
}

inline void DemoClick_Chat(int, Point pt, const Rect& rc, ExtraDemoState& s,
                           const Metrics& mx, std::wstring& status)
{
    const int hit = ChatMessageAt(rc, s.chat, mx, pt);
    s.chat.hovered = hit;
    if (hit >= 0 && hit < static_cast<int>(s.chat.messages.size())) {
        const ChatMessage& m = s.chat.messages[static_cast<size_t>(hit)];
        status = L"ChatThread：" + m.author + L" · " + m.time;
    }
}

inline void DemoClick_MultiStatusBar(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                     const Metrics& mx, std::wstring& status)
{
    const MultiStatusBarHit h = HitMultiStatusBar(rc, s.multiStatus, mx, pt);
    s.multiStatus.hovered = h.segment;
    if (h.segment >= 0 && h.segment < static_cast<int>(s.multiStatus.segments.size())) {
        StatusSegment& seg = s.multiStatus.segments[static_cast<size_t>(h.segment)];
        seg.active = !seg.active;
        status = L"MultiStatusBar：" + seg.text + (seg.active ? L" 已激活" : L" 已取消");
    }
}

inline void DemoClick_Ribbon(int, Point pt, const Rect& rc, ExtraDemoState& s,
                             const Metrics& mx, std::wstring& status)
{
    const RibbonHit h = HitRibbon(rc, s.ribbon, mx, pt);
    s.ribbon.hoveredGroup = h.group;
    s.ribbon.hoveredButton = h.button;
    if (h.group >= 0 && h.group < static_cast<int>(s.ribbon.groups.size()) &&
        h.button >= 0 && h.button < static_cast<int>(s.ribbon.groups[static_cast<size_t>(h.group)].buttons.size())) {
        const RibbonButton& b =
            s.ribbon.groups[static_cast<size_t>(h.group)].buttons[static_cast<size_t>(h.button)];
        status = std::wstring(L"RibbonBar：") + b.label +
                 (b.disabled ? L"（禁用）" : L" 已执行");
    }
}

inline void DemoClick_NoticeCenter(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                   const Metrics& mx, std::wstring& status)
{
    const NoticeCenterHit h = HitNoticeCenter(rc, s.notice, mx, pt);
    s.notice.readAllHover = h.readAll;
    s.notice.hovered = h.item;
    if (h.readAll) {
        for (NoticeItem& item : s.notice.items) item.unread = false;
        status = L"NoticeCenter：全部标为已读";
    } else if (h.item >= 0 && h.item < static_cast<int>(s.notice.items.size())) {
        NoticeItem& item = s.notice.items[static_cast<size_t>(h.item)];
        item.unread = false;
        status = L"NoticeCenter：" + item.title + L" 已读 · 剩余 " +
                 std::to_wstring(NoticeUnreadCount(s.notice)) + L" 条未读";
    }
}

inline void DemoClick_Sparkline(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                const Metrics& mx, std::wstring& status)
{
    const int i = SparklinePointAt(rc, s.spark, mx, pt);
    s.spark.hovered = i;
    status = i >= 0 && i < static_cast<int>(s.spark.values.size())
                 ? L"Sparkline：第 " + std::to_wstring(i + 1) +
                   L" 个采样点，数值 " +
                   std::to_wstring(static_cast<int>(s.spark.values[static_cast<size_t>(i)]))
                 : L"Sparkline：点在绘图区外";
}

inline void DemoClick_Waterfall(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                const Metrics& mx, std::wstring& status)
{
    const int i = WaterfallBarAt(rc, s.waterfall, mx, pt);
    s.waterfall.hovered = i;
    status = i >= 0 && i < static_cast<int>(s.waterfall.bars.size())
                 ? L"WaterfallChart：" + s.waterfall.bars[static_cast<size_t>(i)].label +
                   L" · 变化 " + std::to_wstring(static_cast<int>(
                       s.waterfall.bars[static_cast<size_t>(i)].delta))
                 : L"WaterfallChart：点在绘图区外";
}

inline void DemoClick_StackedBar(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                 const Metrics& mx, std::wstring& status)
{
    const StackedBarHit h = HitStackedBar(rc, s.stacked, mx, pt);
    s.stacked.hoveredRow = h.row;
    s.stacked.hoveredSeg = h.seg;
    status = (h.row >= 0 && h.seg >= 0 && h.row < static_cast<int>(s.stacked.rows.size()))
                 ? L"StackedBar：" + s.stacked.rows[static_cast<size_t>(h.row)].label +
                   L" · " + s.stacked.legend[static_cast<size_t>(
                       (std::min)(h.seg, static_cast<int>(s.stacked.legend.size()) - 1))]
                 : L"StackedBar：点在条形区外";
}

inline void DemoClick_Gauge(int, Point pt, const Rect& rc, ExtraDemoState& s,
                            const Metrics& mx, std::wstring& status)
{
    const int i = GaugeSegmentAt(rc, s.gauge, mx, pt);
    s.gauge.hovered = i;
    status = i >= 0 ? L"GaugeMeter：第 " + std::to_wstring(i + 1) +
                   L" 段，当前 " + std::to_wstring(static_cast<int>(s.gauge.value)) + L"%"
                 : L"GaugeMeter：点在刻度区外";
}

inline void DemoClick_JsonTree(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const JsonTreeHit h = HitJsonTree(rc, s.jsonTree, mx, pt);
    s.jsonTree.hovered = h.node;
    if (h.node < 0 || h.node >= static_cast<int>(s.jsonTree.nodes.size())) {
        status = L"JsonTree：点在节点区外";
        return;
    }
    JsonNode& node = s.jsonTree.nodes[static_cast<size_t>(h.node)];
    if (h.toggle && node.expandable) {
        node.expanded = !node.expanded;
        status = std::wstring(L"JsonTree：") + node.key +
                 (node.expanded ? L" 已展开" : L" 已折叠");
    } else {
        s.jsonTree.selected = h.node;
        status = L"JsonTree：选中 " + node.key + L" = " + node.value;
    }
}

inline void DemoClick_LogViewer(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                const Metrics& mx, std::wstring& status)
{
    const int i = LogLineAt(rc, s.logs, mx, pt);
    s.logs.selected = i;
    s.logs.hovered = i;
    status = i >= 0 && i < static_cast<int>(s.logs.entries.size())
                 ? L"LogViewer：" + s.logs.entries[static_cast<size_t>(i)].text
                 : L"LogViewer：点在日志区外";
}

inline void DemoClick_CronEditor(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                 const Metrics& mx, std::wstring& status)
{
    const CronEditorHit h = HitCronEditor(rc, s.cron, mx, pt);
    s.cron.hoveredField = h.field;
    s.cron.hoveredPreset = h.preset;
    if (h.preset >= 0 && h.preset < static_cast<int>(s.cron.presets.size())) {
        s.cron.selectedPreset = h.preset;
        s.cron.focused = false;
        switch (h.preset) {
        case 0: s.cron.fields = {L"0", L"*", L"*", L"*", L"*"};
                s.cron.description = L"每小时整点执行一次"; break;
        case 1: s.cron.fields = {L"0", L"9", L"*", L"*", L"*"};
                s.cron.description = L"每天上午 09:00 执行一次"; break;
        case 2: s.cron.fields = {L"0", L"9", L"*", L"*", L"1"};
                s.cron.description = L"每周一上午 09:00 执行一次"; break;
        default: s.cron.description = L"自定义表达式，可点击字段编辑"; break;
        }
        status = L"CronEditor：已应用预设「" +
                 s.cron.presets[static_cast<size_t>(h.preset)] + L"」";
    } else if (h.field >= 0) {
        s.cron.focused = true;
        status = L"CronEditor：选中第 " + std::to_wstring(h.field + 1) + L" 段";
    }
}

inline void DemoClick_MentionInput(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                   const Metrics& mx, std::wstring& status)
{
    const MentionInputHit h = HitMentionInput(rc, s.mention, mx, pt);
    s.mention.hovered = h.candidate;
    if (h.candidate >= 0 && h.candidate < static_cast<int>(s.mention.candidates.size())) {
        const MentionCandidate& c =
            s.mention.candidates[static_cast<size_t>(h.candidate)];
        s.mention.text = L"@" + c.name + L" ";
        s.mention.selected = h.candidate;
        s.mention.showCandidates = false;
        status = L"MentionInput：已提及 " + c.name;
    } else if (h.input) {
        s.mention.showCandidates = true;
        s.mention.focused = true;
        status = L"MentionInput：候选项已展开";
    }
}

inline void DemoClick_OrgChart(int, Point pt, const Rect& rc, ExtraDemoState& s,
                               const Metrics& mx, std::wstring& status)
{
    const int i = OrgNodeAt(rc, s.org, mx, pt);
    s.org.selected = i;
    s.org.hovered = i;
    status = i >= 0 && i < static_cast<int>(s.org.nodes.size())
                 ? L"OrgChart：选中 " + s.org.nodes[static_cast<size_t>(i)].name +
                   L" · " + s.org.nodes[static_cast<size_t>(i)].role
                 : L"OrgChart：点在节点外";
}

inline void DemoClick_Leaderboard(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                  const Metrics& mx, std::wstring& status)
{
    const int i = LeaderRowAt(rc, s.leaders, mx, pt);
    s.leaders.selected = i;
    s.leaders.hovered = i;
    status = i >= 0 && i < static_cast<int>(s.leaders.rows.size())
                 ? L"Leaderboard：第 " + std::to_wstring(i + 1) + L" 名 " +
                   s.leaders.rows[static_cast<size_t>(i)].name
                 : L"Leaderboard：点在榜单外";
}

inline void DemoClick_ActivityFeed(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                   const Metrics& mx, std::wstring& status)
{
    const int i = ActivityItemAt(rc, s.feed, mx, pt);
    s.feed.hovered = i;
    if (i >= 0 && i < static_cast<int>(s.feed.items.size())) {
        ActivityItem& item = s.feed.items[static_cast<size_t>(i)];
        item.unread = false;
        status = L"ActivityFeed：" + item.actor + L" " + item.action +
                 L" " + item.target + L"（已读）";
    }
}

inline void DemoClick_AudioWaveform(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                    const Metrics& mx, std::wstring& status)
{
    if (WaveformPlayButtonAt(rc, s.wave, mx, pt)) {
        s.wave.playing = !s.wave.playing;
        status = s.wave.playing ? L"AudioWaveform：开始播放"
                                : L"AudioWaveform：已暂停";
    } else {
        const int milli = WaveformPositionAt(rc, s.wave, mx, pt);
        s.wave.position = static_cast<double>(milli) / 1000.0;
        status = L"AudioWaveform：跳转到 " +
                 std::to_wstring(static_cast<int>(s.wave.duration * s.wave.position)) +
                 L" 秒";
    }
}

inline void DemoClick_WidgetClasses(int, Point pt, const Rect& rc, ExtraDemoState& s,
                                    const Metrics& mx, std::wstring& status)
{
    const int n = 8;
    const int gap = mx.unit;
    const int cols = 3;
    const int rows = (n + cols - 1) / cols;               // 3 rows for 8 tiles
    const int cw = (rc.Width() - gap * (cols - 1)) / cols;
    const int ch = (rc.Height() - gap * (rows - 1)) / rows;
    if (cw <= 0 || ch <= 0) return;
    Control* items[8] = {&s.wButton, &s.wCheckbox, &s.wTextField,
                         &s.wSlider, &s.wSparkline, &s.wGauge,
                         &s.wRatingBar, &s.wIconBtn};
    for (int i = 0; i < n; ++i) {
        const int col = i % cols;
        const int row = i / cols;
        const Rect tile(rc.left + col * (cw + gap),
                        rc.top + row * (ch + gap),
                        rc.left + col * (cw + gap) + cw,
                        rc.top + row * (ch + gap) + ch);
        if (!tile.Contains(pt)) continue;
        PointerEvent ev; ev.pos = pt; ev.down = true;
        const ControlResult r = items[i]->OnPointer(ev);
        switch (i) {
        case 0:
            s.wButton.style = s.wButton.style == ButtonStyle::Primary
                                  ? ButtonStyle::Secondary : ButtonStyle::Primary;
            status = L"widgets::Button：切换按钮样式";
            break;
        case 1:
            s.wCheckbox.state.checked = !s.wCheckbox.state.checked;
            status = s.wCheckbox.state.checked ? L"widgets::Checkbox：已勾选"
                                               : L"widgets::Checkbox：已取消";
            break;
        case 2:
            s.wTextField.state.hasFocus = !s.wTextField.state.hasFocus;
            status = L"widgets::TextField：切换焦点外观";
            break;
        case 3:
            s.wSlider.state.value = (s.wSlider.state.value + 13) % 101;
            status = L"widgets::Slider：value = " +
                     std::to_wstring(s.wSlider.state.value);
            break;
        case 4:
            s.wSparkline.state.hovered =
                (s.wSparkline.state.hovered + 1) %
                static_cast<int>(s.wSparkline.state.values.size());
            status = L"widgets::Sparkline：切换高亮数据点";
            break;
        case 6:
            // widgets::RatingBar: OnPointer already mapped the click to a star
            status = L"widgets::RatingBar：value = " +
                     std::to_wstring(s.wRatingBar.value);
            break;
        case 7:
            s.wIconBtn.style = s.wIconBtn.style == ButtonStyle::Primary
                                  ? ButtonStyle::Secondary : ButtonStyle::Primary;
            status = L"widgets::IconButton：切换按钮样式";
            break;
        default:
            s.wGauge.state.value = s.wGauge.state.value + 9 > 100
                                       ? 10 : s.wGauge.state.value + 9;
            status = L"widgets::Gauge：value = " +
                     std::to_wstring(static_cast<int>(s.wGauge.state.value));
            break;
        }
        status += r.handled ? L"（事件已处理）" : L"（未处理）";
        break;
    }
}

// --- Wheel commands ---
inline bool DemoWheel_VirtualList(int, int delta, ExtraDemoState& s, const Metrics& mx)
{
    s.virtualList.scroll.contentH = ListDataContentHeight(s.virtualList, mx);
    ScrollBy(s.virtualList.scroll, delta);
    return true;
}

inline bool DemoWheel_ListData(int, int delta, ExtraDemoState& s, const Metrics& mx)
{
    s.listData.scroll.contentH = ListDataContentHeight(s.listData, mx);
    ScrollBy(s.listData.scroll, delta);
    return true;
}

inline bool DemoWheel_DemoScroll(int, int delta, ExtraDemoState& s, const Metrics&)
{
    ScrollBy(s.demoScroll, delta);
    return true;
}

// ============================================================
// The registry: one entry per demo control. This is the single
// source of truth consumed by Defs() / Count() and the three
// dispatch hooks below. To add a control you edit *this* list
// only — no more parallel switch statements.
// ============================================================
inline const std::vector<DemoEntry>& Demos()
{
    static const std::vector<DemoEntry> demos = {
        // ---------------- P0 ----------------
        { L"P0 框架", L"Framework",      120, DemoPaint_Framework,  DemoClick_Framework,  nullptr },
        { L"P0 框架", L"ScrollVirtual",  190, DemoPaint_ScrollVirtual, DemoClick_VirtualList, DemoWheel_VirtualList },
        { L"P0 框架", L"DpiScaling",      76, DemoPaint_DpiScaling,  nullptr, nullptr },
        { L"P0 框架", L"Animation",       86, DemoPaint_Animation,   nullptr, nullptr },
        { L"P0 框架", L"Accessibility",  100, DemoPaint_Accessibility, nullptr, nullptr },
        { L"P0 框架", L"TextBox",         80, DemoPaint_TextBox,     DemoClick_TextBox, nullptr },
        // ---------------- P1 ----------------
        { L"P1 迁移", L"Card",           170, DemoPaint_Card,        nullptr, nullptr },
        { L"P1 迁移", L"EmptyView",      140, DemoPaint_EmptyView,   nullptr, nullptr },
        { L"P1 迁移", L"StatusTag",       70, DemoPaint_StatusTag,   nullptr, nullptr },
        { L"P1 迁移", L"KbdChip",         70, DemoPaint_KbdChip,     nullptr, nullptr },
        { L"P1 迁移", L"Toolbar",         76, DemoPaint_Toolbar,     DemoClick_Toolbar, nullptr },
        { L"P1 迁移", L"Form",           210, DemoPaint_Form,        DemoClick_Form, nullptr },
        { L"P1 迁移", L"DataGrid",       190, DemoPaint_DataGrid,    DemoClick_DataGrid, nullptr },
        { L"P1 迁移", L"ListData",       170, DemoPaint_ListData,    DemoClick_ListData, DemoWheel_ListData },
        { L"P1 迁移", L"Custom",         110, DemoPaint_Custom,      nullptr, nullptr },
        // ---------------- P2 input ----------------
        { L"P2 输入", L"SearchBox",       70, DemoPaint_SearchBox,   DemoClick_SearchBox, nullptr },
        { L"P2 输入", L"PasswordBox",     70, DemoPaint_PasswordBox, DemoClick_PasswordBox, nullptr },
        { L"P2 输入", L"TextArea",       110, DemoPaint_TextArea,    nullptr, nullptr },
        { L"P2 输入", L"NumberInput",     70, DemoPaint_NumberInput, DemoClick_NumberInput, nullptr },
        { L"P2 输入", L"DatePicker",     230, DemoPaint_DatePicker,  DemoClick_DatePicker, nullptr },
        { L"P2 输入", L"ColorPicker",    110, DemoPaint_ColorPicker, DemoClick_ColorPicker, nullptr },
        // ---------------- P2 choice ----------------
        { L"P2 选择", L"Dropdown",       180, DemoPaint_Dropdown,    DemoClick_Dropdown, nullptr },
        { L"P2 选择", L"Menu",           200, DemoPaint_Menu,        DemoClick_Menu, nullptr },
        { L"P2 选择", L"ContextMenu",    170, DemoPaint_ContextMenu, nullptr, nullptr },
        { L"P2 选择", L"MultiSelect",    150, DemoPaint_MultiSelect, DemoClick_MultiSelect, nullptr },
        { L"P2 选择", L"Transfer",       200, DemoPaint_Transfer,    DemoClick_Transfer, nullptr },
        // ---------------- P2 display ----------------
        { L"P2 展示", L"Timeline",       180, DemoPaint_Timeline,    nullptr, nullptr },
        { L"P2 展示", L"Steps",           90, DemoPaint_Steps,       DemoClick_Steps, nullptr },
        { L"P2 展示", L"Calendar",       230, DemoPaint_Calendar,    DemoClick_Calendar, nullptr },
        { L"P2 展示", L"DescriptionList",130, DemoPaint_DescriptionList, nullptr, nullptr },
        { L"P2 展示", L"StatCard",       150, DemoPaint_StatCard,    nullptr, nullptr },
        { L"P2 展示", L"Chart",          170, DemoPaint_Chart,       DemoClick_Chart, nullptr },
        { L"P2 展示", L"Carousel",       160, DemoPaint_Carousel,    DemoClick_Carousel, nullptr },
        // ---------------- P2 feedback ----------------
        { L"P2 反馈", L"Spinner",         90, DemoPaint_Spinner,     nullptr, nullptr },
        { L"P2 反馈", L"Tooltip",        110, DemoPaint_Tooltip,     nullptr, nullptr },
        { L"P2 反馈", L"Popover",        150, DemoPaint_Popover,     nullptr, nullptr },
        { L"P2 反馈", L"Modal",          210, DemoPaint_Modal,       DemoClick_Modal, nullptr },
        { L"P2 反馈", L"Drawer",         190, DemoPaint_Drawer,      nullptr, nullptr },
        { L"P2 反馈", L"Notification",   190, DemoPaint_Notification,nullptr, nullptr },
        { L"P2 反馈", L"Result",         160, DemoPaint_Result,      DemoClick_Result, nullptr },
        // ---------------- P2 layout ----------------
        { L"P2 布局", L"Accordion",      180, DemoPaint_Accordion,   DemoClick_Accordion, nullptr },
        { L"P2 布局", L"Anchor",         150, DemoPaint_Anchor,      DemoClick_Anchor, nullptr },
        { L"P2 布局", L"Splitter",       140, DemoPaint_Splitter,    DemoClick_Splitter, nullptr },
        { L"P2 布局", L"ColumnResize",   110, DemoPaint_ColumnResize,DemoClick_ColumnResize, nullptr },
        { L"P2 布局", L"Scrollbar",      150, DemoPaint_Scrollbar,   DemoClick_Scrollbar, DemoWheel_DemoScroll },
        // ---------------- P3 input ----------------
        { L"P3 输入", L"TagInput",        90, DemoPaint_TagInput,    DemoClick_TagInput, nullptr },
        { L"P3 输入", L"RangeSlider",     90, DemoPaint_RangeSlider, DemoClick_RangeSlider, nullptr },
        { L"P3 输入", L"OtpInput",        90, DemoPaint_OtpInput,    DemoClick_OtpInput, nullptr },
        { L"P3 输入", L"Cascader",       170, DemoPaint_Cascader,    DemoClick_Cascader, nullptr },
        // ---------------- P3 data ----------------
        { L"P3 数据", L"AvatarGroup",     90, DemoPaint_AvatarGroup, DemoClick_AvatarGroup, nullptr },
        { L"P3 数据", L"Heatmap",        180, DemoPaint_Heatmap,     DemoClick_Heatmap, nullptr },
        { L"P3 数据", L"CodeBlock",      170, DemoPaint_CodeBlock,   DemoClick_CodeBlock, nullptr },
        { L"P3 数据", L"KanbanBoard",    200, DemoPaint_Kanban,      DemoClick_Kanban, nullptr },
        { L"P3 数据", L"GanttChart",     190, DemoPaint_Gantt,       DemoClick_Gantt, nullptr },
        // ---------------- P3 overlay ----------------
        { L"P3 浮层", L"Snackbar",       110, DemoPaint_Snackbar,    DemoClick_Snackbar, nullptr },
        { L"P3 浮层", L"CommandPalette", 210, DemoPaint_CommandPalette, DemoClick_CommandPalette, nullptr },
        { L"P3 浮层", L"GuideTour",      220, DemoPaint_GuideTour,   DemoClick_GuideTour, nullptr },
        // ---------------- P4 editors ----------------
        { L"P4 编辑", L"QueryBuilder",   200, DemoPaint_QueryBuilder,DemoClick_QueryBuilder, nullptr },
        { L"P4 编辑", L"PropertyGrid",   240, DemoPaint_PropertyGrid,DemoClick_PropertyGrid, nullptr },
        { L"P4 编辑", L"ScheduleGrid",  230, DemoPaint_Schedule,     DemoClick_Schedule, nullptr },
        { L"P4 编辑", L"FileDropZone",  190, DemoPaint_FileDropZone, DemoClick_FileDropZone, nullptr },
        // ---------------- P4 data ----------------
        { L"P4 数据", L"TreeMap",        180, DemoPaint_TreeMap,     DemoClick_TreeMap, nullptr },
        { L"P4 数据", L"FunnelChart",    190, DemoPaint_Funnel,      DemoClick_Funnel, nullptr },
        { L"P4 数据", L"DiffView",       190, DemoPaint_Diff,        DemoClick_Diff, nullptr },
        { L"P4 数据", L"ScoreBreakdown", 240, DemoPaint_Score,       DemoClick_Score, nullptr },
        // ---------------- P4 shell ----------------
        { L"P4 外壳", L"ChatThread",     280, DemoPaint_Chat,        DemoClick_Chat, nullptr },
        { L"P4 外壳", L"MultiStatusBar",  60, DemoPaint_MultiStatusBar, DemoClick_MultiStatusBar, nullptr },
        { L"P4 外壳", L"RibbonBar",      120, DemoPaint_Ribbon,      DemoClick_Ribbon, nullptr },
        { L"P4 外壳", L"NoticeCenter",   270, DemoPaint_NoticeCenter, DemoClick_NoticeCenter, nullptr },
        // ---------------- P5 charts ----------------
        { L"P5 图表", L"Sparkline",      150, DemoPaint_Sparkline,   DemoClick_Sparkline, nullptr },
        { L"P5 图表", L"WaterfallChart", 210, DemoPaint_Waterfall,   DemoClick_Waterfall, nullptr },
        { L"P5 图表", L"StackedBar",     190, DemoPaint_StackedBar,  DemoClick_StackedBar, nullptr },
        { L"P5 图表", L"GaugeMeter",     160, DemoPaint_Gauge,       DemoClick_Gauge, nullptr },
        // ---------------- P5 editors ----------------
        { L"P5 编辑", L"JsonTree",       230, DemoPaint_JsonTree,    DemoClick_JsonTree, nullptr },
        { L"P5 编辑", L"LogViewer",      230, DemoPaint_LogViewer,   DemoClick_LogViewer, nullptr },
        { L"P5 编辑", L"CronEditor",     190, DemoPaint_CronEditor,  DemoClick_CronEditor, nullptr },
        { L"P5 编辑", L"MentionInput",   210, DemoPaint_MentionInput,DemoClick_MentionInput, nullptr },
        // ---------------- P5 collaboration ----------------
        { L"P5 协同", L"OrgChart",       240, DemoPaint_OrgChart,    DemoClick_OrgChart, nullptr },
        { L"P5 协同", L"Leaderboard",    240, DemoPaint_Leaderboard, DemoClick_Leaderboard, nullptr },
        { L"P5 协同", L"ActivityFeed",   230, DemoPaint_ActivityFeed,DemoClick_ActivityFeed, nullptr },
        { L"P5 协同", L"AudioWaveform",  160, DemoPaint_Waveform,    DemoClick_AudioWaveform, nullptr },
        // ---------------- P6 ----------------
        // Both new controls are shown here as real ControlHost instances
        // (see the P6 widget members + DemoPaint/Click_WidgetClasses).
        { L"P6 类体系", L"WidgetClasses", 250, DemoPaint_WidgetClasses, DemoClick_WidgetClasses, nullptr },
    };
    return demos;
}

inline const std::vector<ExtraDef>& Defs()
{
    static std::vector<ExtraDef> defs;
    if (defs.empty()) {
        for (const auto& e : Demos())
            defs.push_back({e.group, e.label, e.height});
    }
    return defs;
}

inline int Count() { return static_cast<int>(Demos().size()); }

// ============================================================
// Dispatch hooks. These are now trivial table lookups over the
// registry — the previous switch(idx) logic lives inside the
// individual command functions above.
// ============================================================
inline void DrawExtraControl(IPainter& g, const Rect& rc, int idx,
                             ExtraDemoState& s, const Palette& pal,
                             const Metrics& mx)
{
    if (rc.Empty()) return;
    const auto& v = Demos();
    if (idx < 0 || idx >= static_cast<int>(v.size())) return;
    if (v[idx].paint) v[idx].paint(g, rc, s, pal, mx);
}

inline void ExtraClick(int idx, Point pt, const Rect& rc, ExtraDemoState& s,
                       const Metrics& mx, std::wstring& status)
{
    const auto& v = Demos();
    if (idx < 0 || idx >= static_cast<int>(v.size())) return;
    if (v[idx].click) {
        v[idx].click(idx, pt, rc, s, mx, status);
    } else {
        const ExtraDef& d = Defs()[static_cast<size_t>(idx)];
        status = std::wstring(L"已点击 ") + d.label;
    }
}

inline bool ExtraWheel(int idx, int delta, ExtraDemoState& s,
                       const Metrics& mx)
{
    const auto& v = Demos();
    if (idx < 0 || idx >= static_cast<int>(v.size()) || !v[idx].wheel) return false;
    return v[idx].wheel(idx, delta, s, mx);
}

} // namespace extragallery
} // namespace ui
} // namespace air
