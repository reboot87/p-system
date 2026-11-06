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
