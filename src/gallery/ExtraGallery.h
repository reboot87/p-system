#pragma once
// ============================================================
// AirUI - ExtraGallery.h
// Shared demo content for the P0 framework / P1 migration / P2
// controls. Kept in one header so src/gallery/main.cpp only needs
// four small hooks (nav, layout, paint, pointer/key/wheel).
//
// All state lives in ExtraDemoState, owned by GalleryPage, so a
// resize that rebuilds the Item list never resets any interaction.
// ============================================================
#include "airui/ControlsExtra.h"
#include "airui/ControlsAdvanced.h"
#include "airui/ControlsEnterprise.h"
#include "airui/FrameworkCore.h"

#include <algorithm>
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

inline const std::vector<ExtraDef>& Defs()
{
    static const std::vector<ExtraDef> defs = {
        {L"P0 框架", L"Framework",      120},
        {L"P0 框架", L"ScrollVirtual",  190},
        {L"P0 框架", L"DpiScaling",      76},
        {L"P0 框架", L"Animation",       86},
        {L"P0 框架", L"Accessibility",  100},
        {L"P0 框架", L"TextBox",         80},
        {L"P1 迁移", L"Card",           170},
        {L"P1 迁移", L"EmptyView",      140},
        {L"P1 迁移", L"StatusTag",       70},
        {L"P1 迁移", L"KbdChip",         70},
        {L"P1 迁移", L"Toolbar",         76},
        {L"P1 迁移", L"Form",           210},
        {L"P1 迁移", L"DataGrid",       190},
        {L"P1 迁移", L"ListData",       170},
        {L"P1 迁移", L"Custom",         110},
        {L"P2 输入", L"SearchBox",       70},
        {L"P2 输入", L"PasswordBox",     70},
        {L"P2 输入", L"TextArea",       110},
        {L"P2 输入", L"NumberInput",     70},
        {L"P2 输入", L"DatePicker",     230},
        {L"P2 输入", L"ColorPicker",    110},
        {L"P2 选择", L"Dropdown",       180},
        {L"P2 选择", L"Menu",           200},
        {L"P2 选择", L"ContextMenu",    170},
        {L"P2 选择", L"MultiSelect",    150},
        {L"P2 选择", L"Transfer",       200},
        {L"P2 展示", L"Timeline",       180},
        {L"P2 展示", L"Steps",           90},
        {L"P2 展示", L"Calendar",       230},
        {L"P2 展示", L"DescriptionList",130},
        {L"P2 展示", L"StatCard",       150},
        {L"P2 展示", L"Chart",          170},
        {L"P2 展示", L"Carousel",       160},
        {L"P2 反馈", L"Spinner",         90},
        {L"P2 反馈", L"Tooltip",        110},
        {L"P2 反馈", L"Popover",        150},
        {L"P2 反馈", L"Modal",          210},
        {L"P2 反馈", L"Drawer",         190},
        {L"P2 反馈", L"Notification",   190},
        {L"P2 反馈", L"Result",         160},
        {L"P2 布局", L"Accordion",      180},
        {L"P2 布局", L"Anchor",         150},
        {L"P2 布局", L"Splitter",       140},
        {L"P2 布局", L"ColumnResize",   110},
        {L"P2 布局", L"Scrollbar",      150},
        {L"P3 输入", L"TagInput",        90},
        {L"P3 输入", L"RangeSlider",     90},
        {L"P3 输入", L"OtpInput",        90},
        {L"P3 输入", L"Cascader",       170},
        {L"P3 数据", L"AvatarGroup",     90},
        {L"P3 数据", L"Heatmap",        180},
        {L"P3 数据", L"CodeBlock",      170},
        {L"P3 数据", L"KanbanBoard",    200},
        {L"P3 数据", L"GanttChart",     190},
        {L"P3 浮层", L"Snackbar",       110},
        {L"P3 浮层", L"CommandPalette", 210},
        {L"P3 浮层", L"GuideTour",      220},
        {L"P4 编辑", L"QueryBuilder",   200},
        {L"P4 编辑", L"PropertyGrid",   240},
        {L"P4 编辑", L"ScheduleGrid",  230},
        {L"P4 编辑", L"FileDropZone",  190},
        {L"P4 数据", L"TreeMap",        180},
        {L"P4 数据", L"FunnelChart",    190},
        {L"P4 数据", L"DiffView",       190},
        {L"P4 数据", L"ScoreBreakdown", 240},
        {L"P4 外壳", L"ChatThread",     280},
        {L"P4 外壳", L"MultiStatusBar",  60},
        {L"P4 外壳", L"RibbonBar",      120},
        {L"P4 外壳", L"NoticeCenter",   270},
    };
    return defs;
}

inline int Count() { return static_cast<int>(Defs().size()); }

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
};

inline void InitExtraDemo(ExtraDemoState& s)
{
    // --- P0: object/event layer with z-order ---
    if (!s.hostReady) {
        auto a = std::make_unique<Control>();
        a->SetName(L"按钮 A"); a->SetRole(ControlRole::Button);
        a->SetBounds(Rect(12, 12, 132, 44)); a->SetZ(1); a->SetTabIndex(10);
        s.host.Add(std::move(a));
        auto b = std::make_unique<Control>();
        b->SetName(L"输入框 B"); b->SetRole(ControlRole::TextBox);
        b->SetBounds(Rect(80, 28, 220, 60)); b->SetZ(2); b->SetTabIndex(20);
        s.host.Add(std::move(b));
        auto c = std::make_unique<Control>();
        c->SetName(L"禁用 C"); c->SetRole(ControlRole::Button);
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
}

inline Rect GuideAnchorRect(const Rect& rc, const Metrics& mx)
{
    return Rect(rc.left + mx.unit * 4, rc.top + mx.unit * 2,
                rc.left + mx.unit * 18, rc.top + mx.unit * 6);
}

inline void DrawExtraControl(IPainter& g, const Rect& rc, int idx,
                             ExtraDemoState& s, const Palette& pal,
                             const Metrics& mx)
{
    if (rc.Empty()) return;
    switch (idx) {
    // ---------------- P0 ----------------
    case 0: {
        g.Translate(Point{rc.left, rc.top});
        s.host.PaintAll(g, pal, mx);
        g.Translate(Point{-rc.left, -rc.top});
        g.Text(Rect(rc.left, rc.bottom - 28, rc.right, rc.bottom),
               L"ControlHost：z-order 命中 / 事件路由 / Tab 焦点 · 点击重叠区试试",
               FontRole::Meta, pal.textSub,
               dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis);
        break;
    }
    case 1: {
        s.virtualList.scroll.viewH = rc.Height();
        s.virtualList.scroll.contentH = ListDataContentHeight(s.virtualList, mx);
        PaintListData(g, rc, s.virtualList, pal, mx);
        break;
    }
    case 2: {
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
        break;
    }
    case 3: {
        SpinnerState sp = s.spinner;
        sp.phase = s.animPhase;
        PaintSpinner(g, Rect(rc.left, rc.top, rc.left + mx.unit * 20, rc.bottom), sp, pal, mx);
        ControlState cs;
        PaintProgressBar(g, Rect(rc.left + mx.unit * 22, rc.top + rc.Height() / 2 - 4,
                                 rc.right - mx.unit, rc.top + rc.Height() / 2 + 4),
                         s.animPhase, cs, pal, mx);
        break;
    }
    case 4: {
        g.FillRound(rc, mx.radiusMd, pal.sunken);
        g.TextBlock(rc, L"无障碍树：\n" + DescribeAll(s.host),
                    FontRole::Meta, pal.textSub, 4);
        break;
    }
    case 5:
        PaintTextBox(g, rc, s.textbox, pal, mx);
        break;

    // ---------------- P1 ----------------
    case 6:  PaintCard(g, rc, s.card, pal, mx); break;
    case 7:  PaintEmptyView(g, rc, s.emptyView, pal, mx); break;
    case 8: {
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
        break;
    }
    case 9: {
        const int w = MeasureKbdChip(s.kbd, mx);
        PaintKbdChip(g, Rect(rc.right - w - mx.unit * 2, rc.top + mx.unit,
                             rc.right - mx.unit * 2, rc.bottom - mx.unit),
                     s.kbd, pal, mx);
        KbdChipState alt; alt.key = L"Alt"; alt.label = L"+ F4 关闭";
        const int w2 = MeasureKbdChip(alt, mx);
        PaintKbdChip(g, Rect(rc.left + mx.unit, rc.top + mx.unit,
                             rc.left + mx.unit + w2, rc.bottom - mx.unit),
                     alt, pal, mx);
        break;
    }
    case 10: PaintToolbar(g, rc, s.toolbar, pal, mx); break;
    case 11: PaintForm(g, rc, s.form, pal, mx); break;
    case 12: PaintDataGrid(g, rc, s.grid, pal, mx); break;
    case 13:
        s.listData.scroll.viewH = rc.Height();
        s.listData.scroll.contentH = ListDataContentHeight(s.listData, mx);
        PaintListData(g, rc, s.listData, pal, mx);
        break;
    case 14: PaintCustom(g, rc, s.custom, pal, mx); break;

    // ---------------- P2 input ----------------
    case 15: PaintSearchBox(g, rc, s.search, pal, mx); break;
    case 16: PaintPasswordBox(g, rc, s.password, pal, mx); break;
    case 17: PaintTextArea(g, rc, s.textArea, pal, mx); break;
    case 18: PaintNumberInput(g, rc, s.number, pal, mx); break;
    case 19: PaintDatePicker(g, rc, s.date, pal, mx); break;
    case 20: PaintColorPicker(g, rc, s.color, pal, mx); break;

    // ---------------- P2 choice ----------------
    case 21: PaintDropdown(g, rc, s.dropdown, pal, mx); break;
    case 22: PaintMenu(g, rc, s.menu, pal, mx); break;
    case 23: {
        s.contextMenu.anchor = Point{rc.left + mx.unit * 8, rc.top + mx.unit * 2};
        PaintContextMenu(g, rc, s.contextMenu, pal, mx);
        break;
    }
    case 24: PaintMultiSelect(g, rc, s.multi, pal, mx); break;
    case 25: PaintTransfer(g, rc, s.transfer, pal, mx); break;

    // ---------------- P2 display ----------------
    case 26: PaintTimeline(g, rc, s.timeline, pal, mx); break;
    case 27: PaintSteps(g, rc, s.steps, pal, mx); break;
    case 28: PaintCalendar(g, rc, s.calendar, pal, mx); break;
    case 29: PaintDescriptionList(g, rc, s.description, pal, mx); break;
    case 30: PaintStatCard(g, rc, s.stat, pal, mx); break;
    case 31: PaintChart(g, rc, s.chart, pal, mx); break;
    case 32: PaintCarousel(g, rc, s.carousel, pal, mx); break;

    // ---------------- P2 feedback ----------------
    case 33: {
        SpinnerState sp = s.spinner;
        sp.phase = s.animPhase;
        PaintSpinner(g, rc, sp, pal, mx);
        break;
    }
    case 34:
        PaintTooltip(g, Rect(rc.left + mx.unit * 4, rc.top + mx.unit,
                             rc.left + mx.unit * 18, rc.top + mx.unit * 5),
                     rc, s.tooltip, pal, mx);
        break;
    case 35:
        PaintPopover(g, Rect(rc.left + mx.unit * 4, rc.top,
                             rc.left + mx.unit * 18, rc.top + mx.unit * 4),
                     rc, s.popover, pal, mx);
        break;
    case 36: PaintModal(g, rc, s.modal, pal, mx); break;
    case 37: PaintDrawer(g, rc, s.drawer, pal, mx); break;
    case 38: PaintNotificationQueue(g, rc, s.notification, pal, mx); break;
    case 39: PaintResult(g, rc, s.result, pal, mx); break;

    // ---------------- P2 layout ----------------
    case 40: PaintAccordion(g, rc, s.accordion, pal, mx); break;
    case 41: PaintAnchor(g, rc, s.anchor, pal, mx); break;
    case 42: PaintSplitter(g, rc, s.splitter, pal, mx); break;
    case 43: PaintColumnResize(g, rc, s.columnResize, pal, mx); break;
    case 44: {
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
        break;
    }
    // ---------------- P3 input ----------------
    case 45: PaintTagInput(g, rc, s.tagInput, pal, mx); break;
    case 46: PaintRangeSlider(g, rc, s.rangeSlider, pal, mx); break;
    case 47: PaintOtpInput(g, rc, s.otp, pal, mx); break;
    case 48: PaintCascader(g, rc, s.cascader, pal, mx); break;

    // ---------------- P3 data ----------------
    case 49: PaintAvatarGroup(g, rc, s.avatars, pal, mx); break;
    case 50: PaintHeatmap(g, rc, s.heatmap, pal, mx); break;
    case 51: PaintCodeBlock(g, rc, s.codeBlock, pal, mx); break;
    case 52: PaintKanban(g, rc, s.kanban, pal, mx); break;
    case 53: PaintGantt(g, rc, s.gantt, pal, mx); break;

    // ---------------- P3 overlay ----------------
    case 54: PaintSnackbar(g, rc, s.snackbar, pal, mx); break;
    case 55: PaintCommandPalette(g, rc, s.palette, pal, mx); break;
    case 56:
        PaintGuideTour(g, rc, GuideAnchorRect(rc, mx), s.guide, pal, mx);
        break;

    // ---------------- P4 editors ----------------
    case 57: PaintQueryBuilder(g, rc, s.query, pal, mx); break;
    case 58: PaintPropertyGrid(g, rc, s.property, pal, mx); break;
    case 59: PaintSchedule(g, rc, s.schedule, pal, mx); break;
    case 60: PaintFileDropZone(g, rc, s.dropZone, pal, mx); break;

    // ---------------- P4 data ----------------
    case 61: PaintTreeMap(g, rc, s.treeMap, pal, mx); break;
    case 62: PaintFunnel(g, rc, s.funnel, pal, mx); break;
    case 63: PaintDiff(g, rc, s.diff, pal, mx); break;
    case 64: PaintScore(g, rc, s.score, pal, mx); break;

    // ---------------- P4 shell ----------------
    case 65: PaintChat(g, rc, s.chat, pal, mx); break;
    case 66: PaintMultiStatusBar(g, rc, s.multiStatus, pal, mx); break;
    case 67: PaintRibbon(g, rc, s.ribbon, pal, mx); break;
    case 68: PaintNoticeCenter(g, rc, s.notice, pal, mx); break;
    default: break;
    }
}

inline void ExtraClick(int idx, Point pt, const Rect& rc, ExtraDemoState& s,
                       const Metrics& mx, std::wstring& status)
{
    switch (idx) {
    case 0: {
        const Point local{pt.x - rc.left, pt.y - rc.top};
        PointerEvent ev; ev.pos = local; ev.down = true; ev.button = 0;
        const ControlResult r = s.host.DispatchPointer(ev);
        status = r.handled ? L"ControlHost：命中控件 #" + std::to_wstring(r.id)
                           : L"ControlHost：空白区域";
        break;
    }
    case 1: case 13: {
        ListDataState& target = idx == 1 ? s.virtualList : s.listData;
        const int row = ListDataRowAt(rc, target, mx, pt);
        if (row >= 0) {
            target.selected = row;
            status = L"ListData：选中第 " + std::to_wstring(row + 1) + L" 行";
        }
        break;
    }
    case 5: {
        const int charW = 8;
        s.textbox.edit.cursor = CaretIndexAt(rc, s.textbox.edit, mx, charW, pt);
        s.textbox.edit.selectionAnchor = -1;
        s.textbox.edit.hasFocus = true;
        status = L"TextBox：光标定位，可用键盘编辑";
        break;
    }
    case 10: {
        const ToolbarHit h = HitToolbar(rc, s.toolbar, mx, pt);
        if (h.action >= 0) status = L"Toolbar：点击动作 #" + std::to_wstring(h.action + 1);
        else if (h.crumb >= 0) status = L"Toolbar：面包屑 " + s.toolbar.crumbs[static_cast<size_t>(h.crumb)];
        else if (h.search) status = L"Toolbar：搜索框";
        break;
    }
    case 11: {
        const FormHit h = HitForm(rc, s.form, mx, pt);
        if (h.field >= 0) {
            s.form.focusedField = h.field;
            status = L"Form：聚焦字段 " + s.form.fields[static_cast<size_t>(h.field)].label;
        } else if (h.submit) {
            const int bad = ValidateForm(s.form);
            status = bad < 0 ? L"Form：校验通过，已提交"
                             : L"Form：字段 " + s.form.fields[static_cast<size_t>(bad)].label + L" 必填";
        }
        break;
    }
    case 12: {
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
        break;
    }
    case 15:
        if (HitSearchBox(rc, s.search, mx, pt) == SearchBoxHit::Clear) {
            s.search.text.clear();
            status = L"SearchBox：已清空";
        }
        break;
    case 16:
        if (HitPasswordBox(rc, s.password, mx, pt) == PasswordBoxHit::Reveal) {
            s.password.revealed = !s.password.revealed;
            status = s.password.revealed ? L"PasswordBox：已显示" : L"PasswordBox：已隐藏";
        }
        break;
    case 18: {
        const NumberInputHit h = HitNumberInput(rc, s.number, mx, pt);
        if (h == NumberInputHit::Plus)  s.number.value = (std::min)(s.number.max, s.number.value + s.number.step);
        if (h == NumberInputHit::Minus) s.number.value = (std::max)(s.number.min, s.number.value - s.number.step);
        status = L"NumberInput：value=" + std::to_wstring(s.number.value);
        break;
    }
    case 19: {
        const int day = DatePickerDayAt(rc, s.date, mx, pt);
        if (day > 0) { s.date.day = day; status = L"DatePicker：选择 " + std::to_wstring(day) + L" 日"; }
        else if (day == -2 || day == -3) {
            s.date.month += day == -2 ? -1 : 1;
            if (s.date.month < 1) { s.date.month = 12; --s.date.year; }
            if (s.date.month > 12) { s.date.month = 1; ++s.date.year; }
            status = L"DatePicker：切换月份";
        }
        break;
    }
    case 20: {
        const int i = ColorPickerSwatchAt(rc, s.color, mx, pt);
        if (i >= 0) { s.color.selected = i; status = L"ColorPicker：选择色块 #" + std::to_wstring(i + 1); }
        break;
    }
    case 21: {
        const int o = DropdownOptionAt(rc, s.dropdown, mx, pt);
        if (o >= 0) {
            s.dropdown.selected = o;
            s.dropdown.text = s.dropdown.options[static_cast<size_t>(o)];
            status = L"Dropdown：选择 " + s.dropdown.text;
        } else if (o == -1) {
            s.dropdown.open = !s.dropdown.open;
            status = s.dropdown.open ? L"Dropdown：展开" : L"Dropdown：收起";
        }
        break;
    }
    case 22: {
        const int i = MenuItemAt(rc, s.menu, mx, pt);
        if (i >= 0) {
            s.menu.selected = i;
            status = L"Menu：选择 " + s.menu.items[static_cast<size_t>(i)].text;
        }
        break;
    }
    case 24: {
        const int i = MultiSelectAt(rc, s.multi, mx, pt);
        if (i >= 0 && i < static_cast<int>(s.multi.checked.size())) {
            s.multi.checked[static_cast<size_t>(i)] = !s.multi.checked[static_cast<size_t>(i)];
            status = L"MultiSelect：切换 " + s.multi.options[static_cast<size_t>(i)];
        }
        break;
    }
    case 25: {
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
        break;
    }
    case 27: {
        const int i = StepAt(rc, s.steps, mx, pt);
        if (i >= 0) { s.steps.current = i; status = L"Steps：跳到第 " + std::to_wstring(i + 1) + L" 步"; }
        break;
    }
    case 28: {
        const int d = CalendarDayAt(rc, s.calendar, mx, pt);
        if (d > 0) { s.calendar.selectedDay = d; status = L"Calendar：选择 " + std::to_wstring(d) + L" 日"; }
        break;
    }
    case 31: {
        const int i = ChartBarAt(rc, s.chart, mx, pt);
        if (i >= 0) { s.chart.hovered = i; status = L"Chart：柱 #" + std::to_wstring(i + 1); }
        break;
    }
    case 32: {
        const CarouselHitResult h = HitCarousel(rc, s.carousel, mx, pt);
        const int n = static_cast<int>(s.carousel.slides.size());
        if (h.code == 1) s.carousel.current = (s.carousel.current - 1 + n) % n;
        else if (h.code == 2) s.carousel.current = (s.carousel.current + 1) % n;
        else if (h.code == 3 && h.index >= 0) s.carousel.current = h.index % n;
        if (h.code != 0) status = L"Carousel：第 " + std::to_wstring(s.carousel.current + 1) + L" 页";
        break;
    }
    case 36: {
        const int b = ModalButtonAt(rc, s.modal, mx, pt);
        if (b > 0) { s.modal.focused = b; status = b == 1 ? L"Modal：确定" : L"Modal：取消"; }
        break;
    }
    case 39:
        if (ResultActionHit(rc, s.result, mx, pt))
            status = L"Result：点击返回首页";
        break;
    case 40: {
        const int i = AccordionHeaderAt(rc, s.accordion, mx, pt);
        if (i >= 0) {
            if (i >= static_cast<int>(s.accordion.expanded.size()))
                s.accordion.expanded.resize(static_cast<size_t>(i + 1), false);
            s.accordion.expanded[static_cast<size_t>(i)] = !s.accordion.expanded[static_cast<size_t>(i)];
            status = L"Accordion：展开/折叠 " + s.accordion.panels[static_cast<size_t>(i)].first;
        }
        break;
    }
    case 41: {
        const int i = AnchorHit(rc, s.anchor, mx, pt);
        if (i >= 0) { s.anchor.active = i; status = L"Anchor：跳转 " + s.anchor.items[static_cast<size_t>(i)]; }
        break;
    }
    case 42:
        if (SplitterHandleHit(rc, s.splitter, mx, pt)) {
            s.splitter.dragging = !s.splitter.dragging;
            status = s.splitter.dragging ? L"Splitter：开始拖拽" : L"Splitter：结束拖拽";
        }
        break;
    case 43: {
        const int i = ColumnResizeHandleAt(rc, s.columnResize, mx, pt);
        if (i >= 0) {
            s.columnResize.dragging = i;
            status = L"ColumnResize：拖拽边界 #" + std::to_wstring(i + 1);
        }
        break;
    }
    case 44: {
        const ScrollBarHit h = HitScrollbar(rc, s.demoScroll, mx, pt);
        if (h == ScrollBarHit::TrackAbove) ScrollPage(s.demoScroll, -1);
        else if (h == ScrollBarHit::TrackBelow) ScrollPage(s.demoScroll, 1);
        else if (h == ScrollBarHit::UpButton) ScrollBy(s.demoScroll, -s.demoScroll.lineH);
        else if (h == ScrollBarHit::DownButton) ScrollBy(s.demoScroll, s.demoScroll.lineH);
        status = L"Scrollbar：offset=" + std::to_wstring(s.demoScroll.offset);
        break;
    }
    // ---------------- P3 input ----------------
    case 45: {
        const TagInputHit h = HitTagInput(rc, s.tagInput, mx, pt);
        if (h.close && h.tag >= 0 &&
            h.tag < static_cast<int>(s.tagInput.tags.size())) {
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
        break;
    }
    case 46: {
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
        break;
    }
    case 47: {
        const int cell = OtpCellAt(rc, s.otp, mx, pt);
        if (cell >= 0) {
            s.otp.focused = cell;
            status = L"OtpInput：聚焦第 " + std::to_wstring(cell + 1) + L" 格";
        }
        break;
    }
    case 48: {
        const CascaderHit h = HitCascader(rc, s.cascader, mx, pt);
        if (h.column >= 0 && h.row >= 0) {
            if (static_cast<int>(s.cascader.selected.size()) < h.column + 1)
                s.cascader.selected.resize(static_cast<size_t>(h.column + 1), -1);
            s.cascader.selected[static_cast<size_t>(h.column)] = h.row;
            for (int c = h.column + 1;
                 c < static_cast<int>(s.cascader.selected.size()); ++c)
                s.cascader.selected[static_cast<size_t>(c)] = -1;
            status = L"Cascader：选择 " +
                     s.cascader.columns[static_cast<size_t>(h.column)]
                         .items[static_cast<size_t>(h.row)];
        }
        break;
    }

    // ---------------- P3 data ----------------
    case 49: {
        const int a = AvatarGroupAt(rc, s.avatars, mx, pt);
        if (a >= 0) {
            s.avatars.hovered = a;
            status = L"AvatarGroup：成员 " +
                     s.avatars.items[static_cast<size_t>(a)].text;
        } else if (a == -2) {
            status = L"AvatarGroup：展开更多成员";
        }
        break;
    }
    case 50: {
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
        break;
    }
    case 51:
        if (CodeBlockCopyHit(rc, s.codeBlock, mx, pt)) {
            s.codeBlock.copyHover = true;
            status = L"CodeBlock：已复制代码";
        }
        break;
    case 52: {
        const KanbanHit h = HitKanban(rc, s.kanban, mx, pt);
        if (h.column >= 0 && h.card >= 0) {
            s.kanban.hoveredColumn = h.column;
            s.kanban.hoveredCard = h.card;
            status = L"KanbanBoard：选中卡片 " +
                     s.kanban.columns[static_cast<size_t>(h.column)]
                         .cards[static_cast<size_t>(h.card)].title;
        }
        break;
    }
    case 53: {
        const int t = GanttTaskAt(rc, s.gantt, mx, pt);
        if (t >= 0) {
            s.gantt.hoveredTask = t;
            status = L"GanttChart：任务 " +
                     s.gantt.tasks[static_cast<size_t>(t)].name;
        }
        break;
    }

    // ---------------- P3 overlay ----------------
    case 54: {
        const SnackbarHit h = HitSnackbar(rc, s.snackbar, mx, pt);
        if (h == SnackbarHit::Action) {
            s.snackbar.actionHover = true;
            status = L"Snackbar：已撤销删除";
        } else if (h == SnackbarHit::Close) {
            s.snackbar.closeHover = true;
            status = L"Snackbar：关闭提示条";
        }
        break;
    }
    case 55: {
        const CommandPaletteHit h = HitCommandPalette(rc, s.palette, mx, pt);
        if (h.row >= 0 && h.row < static_cast<int>(s.palette.items.size())) {
            const std::vector<int> matches = CommandPaletteMatches(s.palette);
            for (size_t k = 0; k < matches.size(); ++k) {
                if (matches[k] == h.row) s.palette.selected = static_cast<int>(k);
            }
            status = L"CommandPalette：执行 " +
                     s.palette.items[static_cast<size_t>(h.row)].title;
        } else if (h.search) {
            status = L"CommandPalette：搜索框（可输入过滤）";
        }
        break;
    }
    case 56: {
        const GuideTourHit h = HitGuideTour(rc, GuideAnchorRect(rc, mx),
                                            s.guide, mx, pt);
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
        break;
    }
    // ---------------- P4 editors ----------------
    case 57: {
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
                status = L"QueryBuilder：已删除第 " +
                         std::to_wstring(h.row + 1) + L" 条";
            }
            break;
        case QueryHitKind::Add:
            s.query.conditions.push_back(
                {L"AND", L"新字段", L"=", L""});
            status = L"QueryBuilder：新增一条条件";
            break;
        default:
            status = L"QueryBuilder：点击条件行进行编辑";
            break;
        }
        break;
    }
    case 58: {
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
        break;
    }
    case 59: {
        const int hit = ScheduleEventAt(rc, s.schedule, mx, pt);
        s.schedule.hovered = hit;
        if (hit >= 0 && hit < static_cast<int>(s.schedule.events.size())) {
            const ScheduleEvent& ev = s.schedule.events[static_cast<size_t>(hit)];
            status = L"ScheduleGrid：" + ev.title + L" · " +
                     std::to_wstring(ev.startHour) + L":00–" +
                     std::to_wstring(ev.endHour) + L":00";
        }
        break;
    }
    case 60: {
        const FileDropZoneHit h = HitFileDropZone(rc, s.dropZone, mx, pt);
        s.dropZone.hovered = h.file;
        if (h.remove && h.file >= 0 &&
            h.file < static_cast<int>(s.dropZone.files.size())) {
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
        break;
    }

    // ---------------- P4 data ----------------
    case 61: {
        const int hit = TreeMapTileAt(rc, s.treeMap, mx, pt);
        s.treeMap.hovered = hit;
        if (hit >= 0 && hit < static_cast<int>(s.treeMap.items.size())) {
            const TileItem& t = s.treeMap.items[static_cast<size_t>(hit)];
            status = L"TreeMap：" + t.label + L" · 权重 " +
                     std::to_wstring(t.weight);
        }
        break;
    }
    case 62: {
        const int hit = FunnelStageAt(rc, s.funnel, mx, pt);
        s.funnel.hovered = hit;
        if (hit >= 0 && hit < static_cast<int>(s.funnel.stages.size())) {
            const FunnelStage& f = s.funnel.stages[static_cast<size_t>(hit)];
            status = L"FunnelChart：" + f.label + L" · " +
                     std::to_wstring(f.value) + L" 人";
        }
        break;
    }
    case 63: {
        const int hit = DiffLineAt(rc, s.diff, mx, pt);
        s.diff.hovered = hit;
        if (hit >= 0 && hit < static_cast<int>(s.diff.lines.size())) {
            const DiffLine& line = s.diff.lines[static_cast<size_t>(hit)];
            const wchar_t* kind = line.kind == DiffKind::Added ? L"新增"
                                  : line.kind == DiffKind::Removed ? L"删除"
                                                                   : L"上下文";
            status = std::wstring(L"DiffView：第 ") +
                     std::to_wstring(hit + 1) + L" 行 · " + kind;
        }
        break;
    }
    case 64: {
        const int hit = ScoreBarAt(rc, s.score, mx, pt);
        s.score.hovered = hit;
        if (hit >= 0 && hit < static_cast<int>(s.score.counts.size())) {
            status = L"ScoreBreakdown：" + std::to_wstring(5 - hit) +
                     L" 星 · " + std::to_wstring(s.score.counts[static_cast<size_t>(hit)]) +
                     L" 条";
        }
        break;
    }

    // ---------------- P4 shell ----------------
    case 65: {
        const int hit = ChatMessageAt(rc, s.chat, mx, pt);
        s.chat.hovered = hit;
        if (hit >= 0 && hit < static_cast<int>(s.chat.messages.size())) {
            const ChatMessage& m = s.chat.messages[static_cast<size_t>(hit)];
            status = L"ChatThread：" + m.author + L" · " + m.time;
        }
        break;
    }
    case 66: {
        const MultiStatusBarHit h = HitMultiStatusBar(rc, s.multiStatus, mx, pt);
        s.multiStatus.hovered = h.segment;
        if (h.segment >= 0 &&
            h.segment < static_cast<int>(s.multiStatus.segments.size())) {
            StatusSegment& seg = s.multiStatus.segments[static_cast<size_t>(h.segment)];
            seg.active = !seg.active;
            status = L"MultiStatusBar：" + seg.text +
                     (seg.active ? L" 已激活" : L" 已取消");
        }
        break;
    }
    case 67: {
        const RibbonHit h = HitRibbon(rc, s.ribbon, mx, pt);
        s.ribbon.hoveredGroup = h.group;
        s.ribbon.hoveredButton = h.button;
        if (h.group >= 0 && h.group < static_cast<int>(s.ribbon.groups.size()) &&
            h.button >= 0 &&
            h.button < static_cast<int>(s.ribbon.groups[static_cast<size_t>(h.group)].buttons.size())) {
            const RibbonButton& b =
                s.ribbon.groups[static_cast<size_t>(h.group)].buttons[static_cast<size_t>(h.button)];
            status = std::wstring(L"RibbonBar：") + b.label +
                     (b.disabled ? L"（禁用）" : L" 已执行");
        }
        break;
    }
    case 68: {
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
        break;
    }
    default: {
        const ExtraDef& d = Defs()[static_cast<size_t>(idx)];
        status = std::wstring(L"已点击 ") + d.label;
        break;
    }
    }
}

inline bool ExtraWheel(int idx, int delta, ExtraDemoState& s,
                       const Metrics& mx)
{
    if (idx == 1 || idx == 13) {
        ListDataState& target = idx == 1 ? s.virtualList : s.listData;
        target.scroll.contentH = ListDataContentHeight(target, mx);
        ScrollBy(target.scroll, delta);
        return true;
    }
    if (idx == 44) {
        ScrollBy(s.demoScroll, delta);
        return true;
    }
    return false;
}

} // namespace extragallery
} // namespace ui
} // namespace air
