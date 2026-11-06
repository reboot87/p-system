#include "windows.h"
#include "windowsx.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "airui/AirUi.h"
#include "airui/Components.h"
#include "airui/ShellCore.h"
#include "airui/ControlsCore.h"
#include "airui/OverlayCore.h"
#include "airui/FrameworkCore.h"
#include "airui/ControlsExtra.h"
#include "ExtraGallery.h"
#include "airui/platform/win32/airui/CanvasPainter.h"
#include "airui/platform/win32/airui/Win32Window.h"

using namespace air::ui;

namespace {

// Page focus ids start at 100; layer ids start at 200 so a pointer
// result can be recognised as a layer activation at a glance.
enum { kFocusPageBase = 100, kFocusLayerBase = 200,
       kDropOptionBase = 200, kModalOk = 301, kModalCancel = 302 };

// Platform clock injected into the core animation driver. core only
// sees IClock; the tick source lives entirely in the Win32 backend.
class DemoClock : public IClock
{
public:
    uint64_t NowMs() override { return static_cast<uint64_t>(GetTickCount64()); }
};

// Rectangles shared by painting and focus registration, so a tab
// focus ring can never drift away from the button it belongs to.
struct LayerItemRects {
    Rect rows[3]{};
    Rect ok{};
    Rect cancel{};
    int rowCount = 0;
};

struct Item {
    enum Kind { Section, Button, Checkbox, Radio, Slider, ProgressBar, ToggleSwitch,
                TextField, Segmented, ComboBox, Tag, Rating, Stepper, ProgressRing,
                Badge, Alert, Tabs, Avatar, Skeleton,
                Table, List, Tree, Breadcrumb, Pagination, Toast,
                OverlayTrigger, OverlayHint, Extra } kind = Button;
    RECT rect;
    std::wstring text;
    BtnStyle style = BtnStyle::Primary;
    bool checked = false;
    bool disabled = false;
    bool hover = false;
    bool pressed = false;
    int value = 50; // for Slider
    float progress = 0.5f; // for ProgressBar
    bool toggled = false; // for ToggleSwitch
    std::wstring placeholder; // for TextField
    int segmentedSelected = 0; // for Segmented
    bool tagSelected = false; // for Tag
    int rating = 3; // for Rating
    int stepperValue = 5; // for Stepper
    AlertKind alertKind = AlertKind::Info; // for Alert
    std::wstring message;                 // for Alert
    int tabsSelected = 0;                 // for Tabs
    int lines = 3;                        // for Skeleton
    int ext = -1;                         // index into extragallery::Defs()
};

class GalleryPage : public Page {
public:
    const wchar_t* Title() const override { return L"组件画廊"; }

    void OnAttach(Shell& shell) override {
        InitData();
        extragallery::InitExtraDemo(m_x);
        m_animClock.SetClock(&m_tickClock);
        m_shell = &shell;
        shell.SetCaptionSubtitle(L"简洁示例");
        // 初始化 ShellCore，展示核心状态机与 IPainter 集成
        m_shellCore.SetCaption({L"AirUI Gallery", L"简洁示例", L"", true});
        m_shellCore.SetStatus({L"就绪", true, L""});

        // 导航项与 Build() 中的 section 一一对应，点击可跳转
        struct NavDef { const wchar_t* group; const wchar_t* label; int id; };
        std::vector<NavDef> nav = {
            {L"表单输入", L"Button",       1},
            {L"表单输入", L"Checkbox",     2},
            {L"表单输入", L"Radio",        3},
            {L"表单输入", L"ToggleSwitch", 4},
            {L"表单输入", L"TextField",    5},
            {L"表单输入", L"Segmented",    6},
            {L"表单输入", L"ComboBox",     7},
            {L"数据展示", L"Tag",          8},
            {L"数据展示", L"Rating",       9},
            {L"数据展示", L"Badge",       12},
            {L"数据展示", L"Card",        15},
            {L"数据展示", L"Table",       20},
            {L"数据展示", L"List",        21},
            {L"数据展示", L"Tree",        22},
            {L"数值反馈", L"Stepper",     10},
            {L"数值反馈", L"ProgressRing",11},
            {L"数值反馈", L"Slider",      13},
            {L"数值反馈", L"ProgressBar", 14},
            {L"反馈与布局", L"Alert",     16},
            {L"反馈与布局", L"Tabs",      17},
            {L"反馈与布局", L"Avatar",    18},
            {L"反馈与布局", L"Skeleton",  19},
            {L"反馈与布局", L"Breadcrumb",23},
            {L"反馈与布局", L"Pagination",24},
            {L"反馈与布局", L"Toast",     25},
            {L"反馈与布局", L"Overlay",   26},
        };
        // P0/P1/P2 demos are appended as their own nav groups so the
        // ShellCore nav scroller handles the now much longer list.
        for (int i = 0; i < extragallery::Count(); ++i) {
            const auto& d = extragallery::Defs()[static_cast<size_t>(i)];
            nav.push_back({d.group, d.label, 27 + i});
        }
        std::vector<ShellNavGroup> groups;
        for (const auto& n : nav) {
            if (groups.empty() || groups.back().title != n.group) {
                groups.push_back(ShellNavGroup{});
                groups.back().title = n.group;
            }
            groups.back().items.push_back({n.id, n.label, false, L"", true});
            m_navLabelById[n.id] = n.label;
            m_navIdByLabel[n.label] = n.id;
        }
        m_shellCore.SetNavGroups(std::move(groups));
        m_shellCore.SetActiveNav(1);

        HWND hwnd = shell.Hwnd();
        m_win = std::make_unique<Win32Window>(hwnd);
        m_win->OnMouseMove([](air::ui::Point){ /* 命中测试可接入 ShellCore */ });
        m_win->OnResized([](air::ui::Rect){ /* 布局更新可接入 ShellCore */ });
        shell.SetMessageHook([this, hwnd](HWND,UINT msg,WPARAM wp,LPARAM lp,LRESULT& res){
            if (m_win) m_win->PumpEvent(msg, wp, lp);
            if (msg == WM_KEYDOWN) {
                const int vk = (int)wp;
                if (vk == VK_TAB || vk == VK_ESCAPE || vk == VK_RETURN || vk == VK_SPACE) {
                    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    HandleOverlayKey(vk, shift);
                    res = 0; return true;
                }
                const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (HandleTextEditKey(vk, ctrl)) { res = 0; return true; }
            }
            if (msg == WM_CHAR) {
                const wchar_t ch = static_cast<wchar_t>(wp);
                // 'T' / 'D' are consumed by the theme / DPI shortcuts.
                if (m_x.textbox.edit.hasFocus && ch >= 32 &&
                    ch != L'T' && ch != L'D') {
                    ApplyEdit(m_x.textbox.edit, EditAction::Insert,
                              std::wstring(1, ch), &m_x.clipboard);
                    if (m_shell) m_shell->Invalidate();
                    res = 0; return true;
                }
            }
            if (msg == WM_KEYDOWN && wp == 'T') {
                m_dark = !m_dark;
                SetPalette(m_dark ? DarkPalette() : LightPalette());
                if (m_shell) m_shell->Invalidate();
                res = 0; return true;
            }
            if (msg == WM_KEYDOWN && wp == 'D') {
                static const int kDpis[] = { 96, 120, 144 };
                m_dpiIndex = (m_dpiIndex + 1) % 3;
                m_x.dpi = kDpis[m_dpiIndex];
                ApplyDpi(m_x.dpiBase, m_x.dpi);
                OnLayout(m_lastClient);
                if (m_shell) m_shell->Invalidate();
                SetStatusText(L"DPI 已切换为 " + std::to_wstring(m_x.dpi));
                res = 0; return true;
            }
            if (msg == WM_MOUSEWHEEL) {
                POINT pt = {(short)LOWORD(lp),(short)HIWORD(lp)};
                ScreenToClient(hwnd,&pt);
                int d = GET_WHEEL_DELTA_WPARAM(wp);
                const int step = d/120*30;
                // A wheel over a virtual-scroll demo scrolls that list
                // instead of the page.
                POINT cp = { pt.x - m_content.left,
                             pt.y - (m_content.top - m_scroll) };
                for (const auto& it : m_items) {
                    if (it.kind != Item::Extra || it.ext < 0) continue;
                    if (!PtInRect(&it.rect, cp)) continue;
                    if (extragallery::ExtraWheel(it.ext, -step, m_x, Mx())) {
                        if (m_shell) m_shell->Invalidate();
                        res = 0; return true;
                    }
                    break;
                }
                Rect navRc = m_shellCore.NavRect();
                if (navRc.Contains(Point{pt.x, pt.y})) {
                    // 指针在导航栏：滚动导航项（26 项超出可视高度）
                    m_shellCore.ScrollNav(-step);
                } else {
                    m_scroll -= step;
                    ClampScroll();
                    UpdateActiveNav();
                }
                if (m_shell) m_shell->Invalidate();
                res = 0; return true;
            }
            if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
                POINT pt = {(short)LOWORD(lp),(short)HIWORD(lp)};
                if (msg == WM_LBUTTONUP && GetCapture()==hwnd) ReleaseCapture();

                // 0) Overlay 层优先于页面：模态拦截、点击外部关闭、层内命中消费。
                //    坐标与 Click() 一致，均为内容区坐标（不随滚动改变语义）。
                if (msg == WM_LBUTTONDOWN) {
                    POINT op = { pt.x - m_content.left, pt.y - (m_content.top - m_scroll) };
                    const OverlayHit oh = m_overlayCtl.Stack().Hit(Point{op.x, op.y});
                    const OverlayResult ovr = m_overlayCtl.OnPointer(Point{op.x, op.y});
                    bool consumed = false;
                    switch (ovr.action) {
                    case OverlayAction::Blocked:
                        SetStatusText(L"模态对话框：点击被拦截，按 Esc 关闭");
                        consumed = true;
                        break;
                    case OverlayAction::Dismiss:
                        SetStatusText(L"已关闭弹出层");
                        consumed = true;
                        break;
                    case OverlayAction::FocusChanged:
                        if (ovr.id >= kFocusLayerBase) {
                            ActivateFocus(ovr.id);
                            consumed = true;
                        } else {
                            StatusFocus();
                        }
                        break;
                    default:
                        break;
                    }
                    if (oh.inside) consumed = true;   // 弹出层吞掉点击，不穿透
                    if (ovr.redraw || consumed) { if (m_shell) m_shell->Invalidate(); }
                    if (consumed) { res = 0; return true; }
                }

                // 1) ShellCore 导航栏：悬停高亮 + 点击跳转
                const int navId = m_shellCore.HitTestNav(Point{pt.x, pt.y});
                if (msg == WM_MOUSEMOVE && navId != m_shellCore.NavHover()) {
                    m_shellCore.SetNavHover(navId);
                    if (m_shell) m_shell->Invalidate();
                }
                if (msg == WM_LBUTTONDOWN && navId >= 0) {
                    GotoSection(navId);
                    res = 0; return true;
                }

                // 2) 内容区控件交互
                if (!PtInRect(&m_content, pt)) return false;
                if (msg == WM_LBUTTONDOWN) SetCapture(hwnd);
                Click(pt,msg);
                res = 0; return true;
            }
            return false;
        });
    }

    void ClampScroll() {
        if (m_scroll < 0) m_scroll = 0;
        if (m_scroll > m_max) m_scroll = m_max;
    }

    // 按导航 id 滚动到对应 section
    void GotoSection(int id) {
        auto itId = m_navLabelById.find(id);
        if (itId == m_navLabelById.end()) return;
        auto itTop = m_sectionTop.find(itId->second);
        if (itTop == m_sectionTop.end()) return;
        m_scroll = itTop->second;
        ClampScroll();
        UpdateActiveNav();
        if (m_shell) m_shell->Invalidate();
    }

    // 依据当前滚动位置高亮当前 section 对应的导航项
    void UpdateActiveNav() {
        int active = -1;
        for (const auto& s : m_sections) {          // m_sections 按 y 升序
            if (s.second <= m_scroll + 8) {
                auto it = m_navIdByLabel.find(s.first);
                if (it != m_navIdByLabel.end()) active = it->second;
            }
        }
        if (active < 0 && !m_sections.empty()) {
            auto it = m_navIdByLabel.find(m_sections.front().first);
            if (it != m_navIdByLabel.end()) active = it->second;
        }
        if (active != m_shellCore.ActiveNav()) m_shellCore.SetActiveNav(active);
    }

    void OnLayout(const RECT& client) override {
        m_lastClient = client;
        // ShellCore 负责画 Caption / NavBar / StatusBar，
        // 画廊内容只使用它剩下的 ContentRect，避免互相重叠。
        Rect rc{client.left, client.top, client.right, client.bottom};
        m_shellCore.Layout(rc);
        Rect c = m_shellCore.ContentRect();
        m_content = {c.left, c.top, c.right, c.bottom};
        Build();
        ClampScroll();
        UpdateActiveNav();
        if (m_shell) m_shell->Invalidate();
    }

    void OnPaint(Canvas& g, const RECT& client) override {
        // Drive core animations from the injected platform clock.
        m_x.animPhase = SpinnerPhase(m_animClock.Tick(), 1200);
        const auto& p = Pal();
        g.Fill(client,p.bg);
        HDC dc = g.Native();
        int s = dc ? SaveDC(dc) : 0;
        if (dc) IntersectClipRect(dc,client.left,client.top,client.right,client.bottom);

        // ShellCore 集成：通过 CanvasPainter 绘制 Caption / NavBar / StatusBar
        {
            Rect rc{client.left, client.top, client.right, client.bottom};
            CanvasPainter painter(g);
            m_shellCore.Paint(painter, p, Mx());
        }

        // 画廊条目只在 ContentRect（导航右侧、标题下方、状态栏上方）内绘制
        const RECT region = m_content;
        if (region.right <= region.left || region.bottom <= region.top) {
            if (dc && s) RestoreDC(dc,s);
            return;
        }
        if (dc) IntersectClipRect(dc,region.left,region.top,region.right,region.bottom);

        // 内容区整块白底，与灰底页面、侧边导航形成三层结构
        g.Fill(region, p.surface);

        const int scroll = m_scroll;
        const int ox = region.left;              // 内容区原点
        const int oy = region.top - scroll;      // 条目 y 原点（标题由 ShellCore 承担）

        for (auto& it : m_items) {
            RECT r = it.rect;
            OffsetRect(&r, ox, oy);
            if (r.bottom < region.top || r.top > region.bottom) continue;
            // Hover highlight for all interactive items
            if (it.kind != Item::Section && it.kind != Item::Extra &&
                it.hover && !it.disabled) {
                RECT hl = r;
                hl.top -= 4;
                hl.bottom += 4;
                hl.left -= 4;
                hl.right += 4;
                g.FillRound(hl, Pal().active, 6);
            }
            if (it.kind == Item::Section) {
                // 分组标题：左侧主色强调条 + 标题 + 底部分隔线
                RECT accent = { r.left, r.top + 3, r.left + 4, r.bottom - 3 };
                g.FillRound(accent, p.primary, 2);
                RECT tr = r;
                tr.left += 14;
                g.Text(tr, it.text, FontRole::Section, p.text,
                       DT_SINGLELINE | DT_LEFT | DT_VCENTER);
                g.HLine(r.left, r.right, r.bottom + 8, p.split);
            } else if (it.kind == Item::Button) {
                g.Button(r,it.text,it.style,it.hover,it.pressed,it.disabled);
            } else if (it.kind == Item::Checkbox) {
                g.Checkbox(r,it.text,it.checked,it.hover,it.disabled);
            } else if (it.kind == Item::Radio) {
                g.Radio(r,it.text,it.checked,it.hover,it.disabled);
            } else if (it.kind == Item::Slider) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                SliderState st;
                st.base.hover = it.hover;
                st.base.pressed = it.pressed;
                st.base.disabled = it.disabled;
                st.value = it.value;
                st.min = 0; st.max = 100;
                CanvasPainter painter(g);
                PaintSlider(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::ProgressBar) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                ControlState st;
                st.disabled = it.disabled;
                CanvasPainter painter(g);
                PaintProgressBar(painter, rc, it.progress, st, Pal(), Mx());
            } else if (it.kind == Item::ToggleSwitch) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                ToggleState st;
                st.base.hover = it.hover;
                st.base.pressed = it.pressed;
                st.base.disabled = it.disabled;
                st.on = it.toggled;
                CanvasPainter painter(g);
                PaintToggleSwitch(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::TextField) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                TextFieldState st;
                st.base.disabled = it.disabled;
                st.base.hover = it.hover;
                st.text = L"";
                st.placeholder = it.placeholder.empty() ? L"输入内容..." : it.placeholder;
                CanvasPainter painter(g);
                PaintTextField(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::Segmented) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                SegmentedState st;
                st.base.disabled = it.disabled;
                st.base.hover = it.hover;
                st.items = {L"选项A", L"选项B", L"选项C"};
                st.selected = it.segmentedSelected;
                CanvasPainter painter(g);
                PaintSegmented(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::ComboBox) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                ComboBoxState st;
                st.base.disabled = it.disabled;
                st.base.hover = it.hover;
                st.text = L"请选择";
                CanvasPainter painter(g);
                PaintComboBox(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::Tag) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                TagState st;
                st.base.disabled = it.disabled;
                st.base.hover = it.hover;
                st.text = it.text.empty() ? L"标签" : it.text;
                st.selected = it.tagSelected;
                CanvasPainter painter(g);
                PaintTag(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::Rating) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                RatingState st;
                st.base.disabled = it.disabled;
                st.base.hover = it.hover;
                st.rating = it.rating;
                CanvasPainter painter(g);
                PaintRating(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::Stepper) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                StepperState st;
                st.base.disabled = it.disabled;
                st.base.hover = it.hover;
                st.value = it.stepperValue;
                CanvasPainter painter(g);
                PaintStepper(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::ProgressRing) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                ProgressRingState st;
                st.base.disabled = it.disabled;
                st.progress = it.progress;
                CanvasPainter painter(g);
                PaintProgressRing(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::Badge) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                BadgeState st;
                st.base.disabled = it.disabled;
                st.text = it.text.empty() ? L"99+" : it.text;
                CanvasPainter painter(g);
                PaintBadge(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::Alert) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                AlertState st;
                st.base.disabled = it.disabled;
                st.kind = it.alertKind;
                st.title = it.text;
                st.message = it.message;
                CanvasPainter painter(g);
                PaintAlert(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::Tabs) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                TabsState st;
                st.base.disabled = it.disabled;
                st.tabs = {L"概览", L"详情", L"日志", L"设置"};
                st.selected = it.tabsSelected;
                st.hovered = it.hover ? (it.tabsSelected + 1) % 4 : -1;
                CanvasPainter painter(g);
                PaintTabs(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::Avatar) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                AvatarState st;
                st.base.disabled = it.disabled;
                st.base.hover = it.hover;
                st.text = it.text.empty() ? L"A" : it.text;
                CanvasPainter painter(g);
                PaintAvatar(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::Skeleton) {
                Rect rc{ r.left, r.top, r.right, r.bottom };
                SkeletonState st;
                st.base.disabled = it.disabled;
                st.lines = it.lines;
                CanvasPainter painter(g);
                PaintSkeleton(painter, rc, st, Pal(), Mx());
            } else if (it.kind == Item::Table) {
                CanvasPainter painter(g);
                PaintTable(painter, Rect{r.left, r.top, r.right, r.bottom},
                           m_table, Pal(), Mx());
            } else if (it.kind == Item::List) {
                CanvasPainter painter(g);
                PaintList(painter, Rect{r.left, r.top, r.right, r.bottom},
                          m_list, Pal(), Mx());
            } else if (it.kind == Item::Tree) {
                CanvasPainter painter(g);
                PaintTree(painter, Rect{r.left, r.top, r.right, r.bottom},
                          m_tree, Pal(), Mx());
            } else if (it.kind == Item::Breadcrumb) {
                CanvasPainter painter(g);
                PaintBreadcrumb(painter, Rect{r.left, r.top, r.right, r.bottom},
                                m_crumb, Pal(), Mx());
            } else if (it.kind == Item::Pagination) {
                CanvasPainter painter(g);
                PaintPagination(painter, Rect{r.left, r.top, r.right, r.bottom},
                                m_pager, Pal(), Mx());
            } else if (it.kind == Item::Toast) {
                const int idx = it.value;
                const bool hidden = idx < 0 || idx >= (int)m_toastHidden.size()
                                 || m_toastHidden[idx];
                if (hidden) {
                    g.StrokeDashed(r, p.border, 6);
                    g.Text(r, L"Toast 已关闭 · 点击重新显示", FontRole::Meta,
                           p.textFaint,
                           DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
                } else {
                    ToastState st = m_toasts[idx];
                    st.hoveredClose = (m_toastHoverIndex == idx && m_toastHoverClose);
                    CanvasPainter painter(g);
                    PaintToast(painter, Rect{r.left, r.top, r.right, r.bottom},
                               st, Pal(), Mx());
                }
            } else if (it.kind == Item::OverlayTrigger) {
                ControlState st;
                st.hover = it.hover;
                st.pressed = it.pressed;
                CanvasPainter painter(g);
                PaintButton(painter, Rect{r.left, r.top, r.right, r.bottom},
                            it.text, ButtonStyle::Secondary, st, Pal(), Mx());
            } else if (it.kind == Item::OverlayHint) {
                g.Text(r, it.text, FontRole::Meta, p.textSub,
                       DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
            } else if (it.kind == Item::Extra && it.ext >= 0) {
                CanvasPainter painter(g);
                extragallery::DrawExtraControl(painter,
                    Rect{r.left, r.top, r.right, r.bottom},
                    it.ext, m_x, Pal(), Mx());
            }
        }
        for (auto& c : m_cards) {
            RECT r = c->GetRect();
            OffsetRect(&r, ox, oy);
            if (r.bottom < region.top || r.top > region.bottom) continue;
            RECT old = c->GetRect(); c->SetRect(r); c->Paint(g); c->SetRect(old);
        }

        // Overlay 层与焦点圈画在所有内容之上。Translate 让 core 继续
        // 使用内容坐标，后端负责把 (ox, oy) 偏移加到每一次绘制上。
        if (m_overlayCtl.Stack().Count() > 0 || m_overlayCtl.FocusedId() >= 0) {
            CanvasPainter painter(g);
            painter.Translate(Point{ox, oy});
            const OverlayItem* top = m_overlayCtl.Stack().Top();
            if (top && top->scrim)
                PaintOverlayScrim(painter, OverlayClientRect(), p.bg);
            for (const auto& layer : m_overlayCtl.Stack().Layers())
                PaintOverlayLayer(painter, layer);
            const Rect fr = m_overlayCtl.FocusRect();
            if (!fr.Empty()) PaintFocusRing(painter, fr, p, Mx());
        }
        if (dc && s) RestoreDC(dc,s);

        if (m_max > 0) {
            const int h = region.bottom - region.top;
            const int barX = region.right - 6;
            const int top = region.top;
            const int trackH = region.bottom - top;
            if (trackH > 0) {
                RECT track = { barX, top, barX + 4, region.bottom };
                g.FillRound(track, p.split, 2);
                int thumbH = h * h / (h + m_max);
                if (thumbH < 24) thumbH = 24;
                if (thumbH > trackH) thumbH = trackH;
                int ty = top + (trackH - thumbH) * m_scroll / m_max;
                RECT thumb = { barX, ty, barX + 4, ty + thumbH };
                g.FillRound(thumb, p.textDisabled, 2);
            }
        }
    }

private:
    // --------------------------------------------------------
    // 新控件的数据与交互状态。全部是 GalleryPage 的成员，
    // 因此 OnLayout -> Build() 重建 Item 列表时不会丢失。
    // --------------------------------------------------------
    void InitData() {
        if (m_dataReady) return;
        m_dataReady = true;

        m_table.title = L"最近记录 / Table · 点击表头排序，点击行选中";
        m_table.columns = { L"名称", L"类型", L"状态", L"更新时间" };
        m_table.widths  = { 3, 2, 2, 3 };
        m_table.rows = {
            { L"季度报告.docx",     L"文档", L"已完成", L"2026-11-05 21:14" },
            { L"设计稿_v3.png",     L"图片", L"处理中", L"2026-11-05 20:02" },
            { L"会议纪要.md",       L"文本", L"已完成", L"2026-11-05 18:47" },
            { L"client_setup.exe",  L"文件", L"失败",   L"2026-11-05 17:30" },
            { L"接口说明.pdf",      L"文档", L"已完成", L"2026-11-05 16:08" },
        };
        m_table.selectedRow = 1;

        m_list.items = {
            { L"剪贴板监控", L"自动记录剪贴板内容，支持搜索与备注", L"运行中", L"\xE8C8", false },
            { L"文件监控",   L"监控指定目录的新增 / 修改 / 删除",   L"规划中", L"\xE8B7", false },
            { L"打印监控",   L"记录本机打印任务与文档信息",         L"规划中", L"\xE749", false },
            { L"插件中心",   L"该模块暂不可用",                     L"已停用", L"\xE713", true },
        };
        m_list.selected = 0;

        m_tree.nodes = {
            { L"全部记录", 0, true,  true,  false },
            { L"剪贴板",   1, true,  true,  false },
            { L"文本片段", 2, false, false, false },
            { L"图片",     2, false, false, false },
            { L"文件监控", 1, true,  false, false },
            { L"下载目录", 2, false, false, false },
            { L"系统设置", 0, false, false, false },
            { L"关于",     0, false, false, true  },
        };
        m_tree.selected = 2;

        m_crumb.items = { L"首页", L"数据中心", L"记录管理", L"详情" };

        m_pager.page = 3;
        m_pager.pageCount = 12;

        auto toast = [](AlertKind k, const wchar_t* title, const wchar_t* msg) {
            ToastState s;
            s.kind = k; s.title = title; s.message = msg;
            return s;
        };
        m_toasts = {
            toast(AlertKind::Info,    L"提示",   L"点击卡片切换类型，点击 × 关闭"),
            toast(AlertKind::Success, L"保存成功", L"记录已写入本地数据库"),
            toast(AlertKind::Error,   L"同步失败", L"网络连接已断开，请稍后重试"),
        };
        m_toastHidden.assign(m_toasts.size(), false);
    }

    // Editable TextBox: translate native key codes into core edit
    // actions. Clipboard goes through IClipboard, so core never sees a
    // native clipboard handle.
    bool HandleTextEditKey(int vk, bool ctrl)
    {
        EditAction action = EditAction::None;
        if (ctrl) {
            switch (vk) {
            case 'A': action = EditAction::SelectAll; break;
            case 'C': action = EditAction::Copy;     break;
            case 'X': action = EditAction::Cut;      break;
            case 'V': action = EditAction::Paste;    break;
            default: return false;
            }
        } else {
            switch (vk) {
            case VK_BACK:   action = EditAction::Backspace; break;
            case VK_DELETE: action = EditAction::Delete;    break;
            case VK_LEFT:   action = EditAction::MoveLeft;  break;
            case VK_RIGHT:  action = EditAction::MoveRight; break;
            case VK_HOME:   action = EditAction::Home;      break;
            case VK_END:    action = EditAction::End;       break;
            default: return false;
            }
        }
        if (!m_x.textbox.edit.hasFocus) return false;
        ApplyEdit(m_x.textbox.edit, action, L"", &m_x.clipboard);
        if (m_shell) m_shell->Invalidate();
        return true;
    }

    void SetStatusText(const std::wstring& s) {
        m_shellCore.SetStatus({s, true, L""});
    }

    // --------------------------------------------------------
    // Overlay / focus plumbing. All rectangles are in content
    // space (same as Item::rect); CanvasPainter::Translate maps
    // them to the window when drawing.
    // --------------------------------------------------------
    Rect OverlayClientRect() const {
        return Rect{0, m_scroll, m_content.right - m_content.left,
                    m_scroll + (m_content.bottom - m_content.top)};
    }

    LayerItemRects LayerRectsFor(const OverlayItem& layer) const {
        const auto& mx = Mx();
        LayerItemRects out;
        const Rect c = OverlayContentRect(layer, mx);
        if (c.Empty()) return out;
        if (layer.kind == OverlayKind::Dropdown) {
            out.rowCount = 3;
            for (int i = 0; i < 3; ++i)
                out.rows[i] = Rect(c.left, c.top + i * mx.rowH,
                                  c.right, c.top + (i + 1) * mx.rowH);
        } else if (layer.kind == OverlayKind::Modal) {
            const int bw = 96, bh = 30;
            out.ok = Rect(c.right - bw * 2 - mx.unit, c.bottom - bh,
                          c.right - bw - mx.unit, c.bottom);
            out.cancel = Rect(c.right - bw, c.bottom - bh, c.right, c.bottom);
        }
        return out;
    }

    void PaintOverlayLayer(IPainter& g, const OverlayItem& layer) {
        const auto& p = Pal();
        const auto& mx = Mx();
        PaintOverlayFrame(g, layer, p, mx);
        const Rect c = OverlayContentRect(layer, mx);
        if (c.Empty()) return;
        constexpr TextFormat kText = dt::SingleLine | dt::Left | dt::VCenter |
                                     dt::NoPrefix | dt::EndEllipsis;
        switch (layer.kind) {
        case OverlayKind::Popover:
        case OverlayKind::Tooltip:
            g.TextBlock(c, L"由 OverlayStack 按 z 序绘制；点击外部或按 Esc 关闭。",
                        FontRole::Body, p.textSub, 3);
            break;
        case OverlayKind::Dropdown: {
            const wchar_t* opts[] = {L"复制到剪贴板", L"导出为 CSV", L"查看详情"};
            const LayerItemRects lr = LayerRectsFor(layer);
            for (int i = 0; i < lr.rowCount; ++i) {
                const int fid = kDropOptionBase + i;
                if (i == m_overlayHoverOption ||
                    (m_overlayCtl.ActiveGroup() > 0 && m_overlayCtl.FocusedId() == fid))
                    g.FillRound(lr.rows[i], mx.radiusSm, p.hover);
                g.Text(lr.rows[i], opts[i], FontRole::Body, p.text, kText);
            }
            break;
        }
        case OverlayKind::Modal: {
            Rect msg = c;
            msg.bottom = c.top + mx.rowH * 2;
            g.TextBlock(msg,
                L"模态层拦截所有外部点击；Tab 只在层内循环，关闭后焦点回到打开前的控件。",
                FontRole::Body, p.textSub, 3);
            const LayerItemRects lr = LayerRectsFor(layer);
            ControlState stOk, stCancel;
            stOk.hover = (m_overlayCtl.FocusedId() == kModalOk);
            stCancel.hover = (m_overlayCtl.FocusedId() == kModalCancel);
            PaintButton(g, lr.ok, L"确定", ButtonStyle::Primary, stOk, p, mx);
            PaintButton(g, lr.cancel, L"取消", ButtonStyle::Secondary, stCancel, p, mx);
            break;
        }
        }
    }

    void OpenOverlay(int which, const Rect& anchor) {
        const auto& mx = Mx();
        const Rect client = OverlayClientRect();
        OverlayItem item;
        std::vector<FocusEntry> entries;
        std::wstring label;

        if (which == 0) {
            item.kind = OverlayKind::Popover;
            item.title = L"Tooltip / Popover";
            item.bounds = PlaceOverlay(anchor, Size{260, 110}, client,
                                       OverlayPlacement::Below);
            label = L"Popover";
        } else if (which == 1) {
            item.kind = OverlayKind::Dropdown;
            item.title = L"选择一个操作";
            item.bounds = PlaceOverlay(anchor, Size{240, mx.rowH * 4 + mx.unit * 3},
                                       client, OverlayPlacement::Below);
            item.focusGroup = 1;
            const LayerItemRects lr = LayerRectsFor(item);
            const wchar_t* names[] = {L"复制到剪贴板", L"导出为 CSV", L"查看详情"};
            for (int i = 0; i < lr.rowCount; ++i) {
                FocusEntry e;
                e.id = kDropOptionBase + i;
                e.bounds = lr.rows[i];
                e.tabIndex = i * 10;
                e.name = names[i];
                entries.push_back(e);
            }
            label = L"Dropdown";
        } else {
            item.kind = OverlayKind::Modal;
            item.title = L"模态对话框";
            item.bounds = PlaceOverlay(anchor, Size{360, 210}, client,
                                       OverlayPlacement::Center);
            item.modal = true;
            item.scrim = true;
            item.closeOnOutside = false;
            item.focusGroup = 2;
            const LayerItemRects lr = LayerRectsFor(item);
            FocusEntry ok;
            ok.id = kModalOk; ok.bounds = lr.ok; ok.tabIndex = 10; ok.name = L"确定";
            FocusEntry cancel;
            cancel.id = kModalCancel; cancel.bounds = lr.cancel;
            cancel.tabIndex = 20; cancel.name = L"取消";
            entries.push_back(ok);
            entries.push_back(cancel);
            label = L"Modal";
        }

        item.anchor = anchor;
        m_overlayHoverOption = -1;
        m_overlayCtl.Open(item, std::move(entries));
        SetStatusText(L"已打开 " + label + L" · Tab 切换焦点，Esc 关闭");
        if (m_shell) m_shell->Invalidate();
    }

    void ActivateFocus(int id) {
        if (id >= kFocusLayerBase) {
            const FocusEntry* e = m_overlayCtl.FocusedEntry();
            const std::wstring name = (e && e->id == id) ? e->name : L"选项";
            m_overlayCtl.CloseTop();
            m_overlayHoverOption = -1;
            if (id == kModalCancel) SetStatusText(L"已取消模态对话框");
            else SetStatusText(L"已激活：" + name);
            if (m_shell) m_shell->Invalidate();
            return;
        }
        const auto fit = m_focusToItem.find(id);
        if (fit == m_focusToItem.end() || fit->second >= m_items.size()) return;
        const Item& it = m_items[fit->second];
        if (it.kind == Item::OverlayTrigger) {
            OpenOverlay(it.value, Rect{it.rect.left, it.rect.top,
                                       it.rect.right, it.rect.bottom});
            return;
        }
        // 其它控件复用既有鼠标交互：把矩形中心转成客户区坐标再转发。
        POINT pt = { m_content.left + (it.rect.left + it.rect.right) / 2,
                     m_content.top - m_scroll + (it.rect.top + it.rect.bottom) / 2 };
        Click(pt, WM_LBUTTONDOWN);
    }

    void HandleOverlayKey(int vk, bool shift) {
        const OverlayResult r = m_overlayCtl.OnKey(vk, shift);
        switch (r.action) {
        case OverlayAction::Activate:
            ActivateFocus(r.id);
            break;
        case OverlayAction::FocusChanged:
            EnsureFocusVisible();
            StatusFocus();
            break;
        case OverlayAction::Dismiss:
            m_overlayHoverOption = -1;
            SetStatusText(L"已关闭弹出层");
            break;
        default:
            break;
        }
        if (r.redraw && m_shell) m_shell->Invalidate();
    }

    void EnsureFocusVisible() {
        // 焦点在弹出层内时不要滚动页面，否则弹出层会被带走。
        if (m_overlayCtl.ActiveGroup() != 0) return;
        const Rect fr = m_overlayCtl.FocusRect();
        const int viewH = m_content.bottom - m_content.top;
        if (fr.Empty() || viewH <= 0) return;
        if (fr.top < m_scroll + 8) m_scroll = fr.top - 8;
        else if (fr.bottom > m_scroll + viewH - 8) m_scroll = fr.bottom - viewH + 8;
        ClampScroll();
        UpdateActiveNav();
    }

    void StatusFocus() {
        const FocusEntry* e = m_overlayCtl.FocusedEntry();
        if (e) SetStatusText(L"焦点：" + e->name);
    }

    bool UpdateOverlayHover(const POINT& p) {
        int hov = -1;
        for (const auto& layer : m_overlayCtl.Stack().Layers()) {
            if (layer.kind != OverlayKind::Dropdown) continue;
            const LayerItemRects lr = LayerRectsFor(layer);
            for (int i = 0; i < lr.rowCount; ++i)
                if (lr.rows[i].Contains(Point{p.x, p.y})) hov = i;
        }
        if (hov == m_overlayHoverOption) return false;
        m_overlayHoverOption = hov;
        return true;
    }

    // 计算数据控件的 hover；返回是否有变化（需要重绘）。
    bool UpdateDataHover(const POINT& p) {
        int tableHover = -1, listHover = -1, treeHover = -1, crumbHover = -1;
        int pagerHover = 0, toastHover = -1;
        bool toastClose = false;

        for (const auto& it : m_items) {
            if (PtInRect(&it.rect, p) == FALSE) continue;
            Rect rc{it.rect.left, it.rect.top, it.rect.right, it.rect.bottom};
            Point hit{p.x, p.y};
            switch (it.kind) {
            case Item::Table: {
                const int row = TableRowAt(rc, m_table, Mx(), hit);
                tableHover = row < 0 ? -1 : row;
                break;
            }
            case Item::List:
                listHover = ListRowAt(rc, m_list, Mx(), hit);
                break;
            case Item::Tree:
                treeHover = TreeRowAt(rc, m_tree, Mx(), hit);
                break;
            case Item::Breadcrumb:
                crumbHover = BreadcrumbHit(rc, m_crumb, Mx(), hit);
                break;
            case Item::Pagination:
                pagerHover = PaginationHit(rc, m_pager, Mx(), hit);
                break;
            case Item::Toast:
                toastHover = it.value;
                if (it.value >= 0 && it.value < (int)m_toastHidden.size() &&
                    !m_toastHidden[it.value]) {
                    toastClose = ToastCloseHit(rc, m_toasts[it.value], Mx(), hit);
                }
                break;
            default:
                break;
            }
            break;   // 各控件区域不重叠，命中第一个即可
        }

        bool changed = false;
        if (m_table.hoveredRow != tableHover) { m_table.hoveredRow = tableHover; changed = true; }
        if (m_list.hovered   != listHover)   { m_list.hovered = listHover;     changed = true; }
        if (m_tree.hovered   != treeHover)   { m_tree.hovered = treeHover;     changed = true; }
        if (m_crumb.hovered  != crumbHover)  { m_crumb.hovered = crumbHover;   changed = true; }
        if (m_pager.hovered  != pagerHover)  { m_pager.hovered = pagerHover;   changed = true; }
        if (m_toastHoverIndex != toastHover) { m_toastHoverIndex = toastHover; changed = true; }
        if (m_toastHoverClose != toastClose) { m_toastHoverClose = toastClose; changed = true; }
        return changed;
    }

    void Build() {
        InitData();
        // 保留交互状态：resize 重建布局时，复选/单选的选中状态不丢失
        for (auto& it : m_items) {
            if (it.kind == Item::Checkbox || it.kind == Item::Radio)
                m_checked[it.text] = it.checked;
        }

        m_items.clear(); m_cards.clear();
        m_sections.clear(); m_sectionTop.clear();
        int y = 16;                              // 顶部留白，避免首张卡片贴住标题栏
        const int x = 20;                        // 左右留白，卡片不压住导航栏
        int w = m_content.right - m_content.left;
        if (w < 240) w = 240;
        const int contentW = w - 40;

        auto sec = [&](const wchar_t* t){
            Item it; it.kind=Item::Section; it.text=t; it.rect={x,y,x+contentW,y+22};
            m_items.push_back(it);
            m_sectionTop[t] = y;                 // 供导航跳转
            m_sections.push_back({t, y});        // 供滚动高亮
            y+=60;
        };

        sec(L"Button");
        {
            const wchar_t* texts[] = {L"Primary", L"Secondary", L"Danger", L"Ghost"};
            BtnStyle styles[] = {BtnStyle::Primary, BtnStyle::Secondary, BtnStyle::Danger, BtnStyle::Ghost};
            int bw = 110, bh = 30, gap = 8;
            int cols = contentW / (bw + gap);
            if (cols < 1) cols = 1;
            if (cols > 4) cols = 4;
            int idx = 0;
            for (int i = 0; i < 4; i++) {
                int cx = x + (idx % cols) * (bw + gap);
                int cy = y + (idx / cols) * (bh + gap);
                Item it; it.kind=Item::Button; it.text=texts[i]; it.style=styles[i];
                it.rect={cx, cy, cx+bw, cy+bh}; m_items.push_back(it);
                idx++;
            }
            y += ((4 + cols - 1) / cols) * (bh + gap) + 10;
        }

        sec(L"Checkbox");
        {
            // 窄窗口竖排、宽窗口横排（自适应）
            bool horizontal = contentW >= 360;
            const wchar_t* texts[] = {L"选项A", L"选项B"};
            for (int i = 0; i < 2; i++) {
                Item it; it.kind=Item::Checkbox; it.text=texts[i];
                auto found = m_checked.find(texts[i]);
                it.checked = (found != m_checked.end()) ? found->second : (i == 0);
                if (horizontal) it.rect={x + i*180, y, x+360, y+20};
                else            it.rect={x, y+i*26, x+contentW, y+i*26+20};
                m_items.push_back(it);
            }
            y += horizontal ? 32 : 56;
        }

        sec(L"Radio");
        {
            bool horizontal = contentW >= 360;
            const wchar_t* texts[] = {L"选项1", L"选项2"};
            for (int i = 0; i < 2; i++) {
                Item it; it.kind=Item::Radio; it.text=texts[i];
                auto found = m_checked.find(texts[i]);
                it.checked = (found != m_checked.end()) ? found->second : (i == 0);
                if (horizontal) it.rect={x + i*180, y, x+360, y+20};
                else            it.rect={x, y+i*26, x+contentW, y+i*26+20};
                m_items.push_back(it);
            }
            y += horizontal ? 32 : 56;
        }

        sec(L"ToggleSwitch");
        {
            const wchar_t* labels[] = {L"关闭", L"开启", L"禁用"};
            for (int i = 0; i < 3; i++) {
                Item it; it.kind=Item::ToggleSwitch; it.text=labels[i];
                it.disabled = (i==2);
                it.toggled = (i==1);
                it.rect={x, y, x+contentW, y+28};
                m_items.push_back(it);
                y += 44;
            }
        }

        sec(L"TextField");
        {
            const wchar_t* labels[] = {L"正常", L"禁用"};
            for (int i = 0; i < 2; i++) {
                Item it; it.kind=Item::TextField; it.text=labels[i];
                it.disabled = (i==1);
                it.placeholder = L"请输入内容";
                it.rect={x, y, x+contentW, y+32};
                m_items.push_back(it);
                y += 48;
            }
        }

        sec(L"Segmented");
        {
            const wchar_t* labels[] = {L"正常", L"禁用"};
            for (int i = 0; i < 2; i++) {
                Item it; it.kind=Item::Segmented; it.text=labels[i];
                it.disabled = (i==1);
                it.segmentedSelected = 1;
                it.rect={x, y, x+contentW, y+32};
                m_items.push_back(it);
                y += 48;
            }
        }

        sec(L"ComboBox");
        {
            const wchar_t* labels[] = {L"正常", L"禁用"};
            for (int i = 0; i < 2; i++) {
                Item it; it.kind=Item::ComboBox; it.text=labels[i];
                it.disabled = (i==1);
                it.rect={x, y, x+contentW, y+32};
                m_items.push_back(it);
                y += 48;
            }
        }

        sec(L"Tag");
        {
            const wchar_t* labels[] = {L"正常", L"选中", L"禁用"};
            for (int i = 0; i < 3; i++) {
                Item it; it.kind=Item::Tag; it.text=labels[i];
                it.disabled = (i==2);
                it.tagSelected = (i==1);
                it.rect={x, y, x+200, y+28};
                m_items.push_back(it);
                y += 44;
            }
        }

        sec(L"Rating");
        {
            const wchar_t* labels[] = {L"3星", L"5星", L"禁用"};
            for (int i = 0; i < 3; i++) {
                Item it; it.kind=Item::Rating; it.text=labels[i];
                it.disabled = (i==2);
                it.rating = (i==0)?3:5;
                it.rect={x, y, x+contentW, y+32};
                m_items.push_back(it);
                y += 48;
            }
        }

        sec(L"Stepper");
        {
            const wchar_t* labels[] = {L"正常", L"禁用"};
            for (int i = 0; i < 2; i++) {
                Item it; it.kind=Item::Stepper; it.text=labels[i];
                it.disabled = (i==1);
                it.stepperValue = 5;
                it.rect={x, y, x+contentW, y+36};
                m_items.push_back(it);
                y += 52;
            }
        }

        sec(L"ProgressRing");
        {
            const wchar_t* labels[] = {L"50%", L"禁用"};
            for (int i = 0; i < 2; i++) {
                Item it; it.kind=Item::ProgressRing; it.text=labels[i];
                it.disabled = (i==1);
                it.progress = 0.5f;
                it.rect={x, y, x+contentW, y+28};
                m_items.push_back(it);
                y += 44;
            }
        }

        sec(L"Badge");
        {
            Item it; it.kind=Item::Badge; it.text=L"9";
            it.rect={x, y, x+80, y+28};
            m_items.push_back(it);
            y += 44;
        }

        sec(L"Slider");
        {
            const wchar_t* labels[] = {L"正常", L"禁用"};
            for (int i = 0; i < 2; i++) {
                Item it; it.kind=Item::Slider; it.text=labels[i];
                it.disabled = (i==1);
                it.value = 60;
                it.rect={x, y, x+contentW, y+36};
                m_items.push_back(it);
                y += 46;
            }
        }

        sec(L"ProgressBar");
        {
            const wchar_t* labels[] = {L"25%", L"60%", L"禁用"};
            for (int i = 0; i < 3; i++) {
                Item it; it.kind=Item::ProgressBar; it.text=labels[i];
                it.disabled = (i==2);
                it.progress = (i==0)?0.25f:(i==1)?0.6f:0.6f;
                it.rect={x, y, x+contentW, y+28};
                m_items.push_back(it);
                y += 44;
            }
        }

        sec(L"Card");
        {
            const int minCard = 210;
            const int gap = 12;
            int cols = contentW / (minCard + gap);
            if (cols < 1) cols = 1;
            if (cols > 3) cols = 3;
            int cw = (contentW - gap * (cols - 1)) / cols;
            int rows = (3 + cols - 1) / cols;

            int cy = y;
            for (int i = 0; i < 3; i++) {
                int col = i % cols, row = i / cols;
                Card::Config cc;
                cc.rect = { x + col*(cw+gap), cy + row*162,
                            x + col*(cw+gap) + cw, cy + row*162 + 150 };
                cc.title = L"卡片"; cc.desc = L"示例"; cc.number = L"42"; cc.unit = L"个";
                cc.badge.text = L"正常"; cc.badge.ok = true; cc.linkText = L"更多";
                cc.icon.glyph = L"\xE8C8"; cc.icon.size = 32;
                m_cards.emplace_back(new Card(cc));
            }
            y = cy + rows * 162 + 10;
        }

        sec(L"Alert");
        {
            struct A { AlertKind k; const wchar_t* t; const wchar_t* m; bool dis; };
            const A arr[] = {
                {AlertKind::Info,    L"提示",  L"这是一条信息提示",   false},
                {AlertKind::Success, L"成功",  L"操作已成功完成",     false},
                {AlertKind::Warning, L"警告",  L"磁盘空间不足 10%",  false},
                {AlertKind::Error,   L"错误",  L"网络连接已断开",     false},
                {AlertKind::Info,    L"禁用",  L"不可用状态",         true},
            };
            for (const auto& a : arr) {
                Item it; it.kind = Item::Alert;
                it.alertKind = a.k; it.text = a.t; it.message = a.m;
                it.disabled = a.dis;
                it.rect = {x, y, x + contentW, y + 48};
                m_items.push_back(it);
                y += 60;
            }
        }

        sec(L"Tabs");
        {
            const wchar_t* labels[] = {L"正常", L"禁用"};
            for (int i = 0; i < 2; i++) {
                Item it; it.kind = Item::Tabs; it.text = labels[i];
                it.disabled = (i == 1);
                it.tabsSelected = 1;
                it.rect = {x, y, x + contentW, y + 36};
                m_items.push_back(it);
                y += 52;
            }
        }

        sec(L"Avatar");
        {
            const wchar_t* labels[] = {L"A", L"UI", L"禁"};
            for (int i = 0; i < 3; i++) {
                Item it; it.kind = Item::Avatar; it.text = labels[i];
                it.disabled = (i == 2);
                it.rect = {x + i * 60, y, x + i * 60 + 40, y + 40};
                m_items.push_back(it);
            }
            y += 64;
        }

        sec(L"Skeleton");
        {
            Item it; it.kind = Item::Skeleton; it.lines = 3;
            it.rect = {x, y, x + contentW, y + 72};
            m_items.push_back(it);
            y += 88;
        }

        sec(L"Table");
        {
            const int titleH = (m_table.showTitle && !m_table.title.empty())
                               ? Mx().rowH : 0;
            const int h = titleH + Mx().rowH
                        + (int)m_table.rows.size() * Mx().rowH + 2;
            Item it; it.kind = Item::Table;
            it.rect = {x, y, x + contentW, y + h};
            m_items.push_back(it);
            y += h + 24;
        }

        sec(L"List");
        {
            const int h = (int)m_list.items.size() * ListRowHeight(m_list, Mx()) + 2;
            Item it; it.kind = Item::List;
            it.rect = {x, y, x + contentW, y + h};
            m_items.push_back(it);
            y += h + 24;
        }

        sec(L"Tree");
        {
            const int rows = (int)TreeVisibleNodes(m_tree).size();
            const int h = rows * TreeRowHeight(Mx()) + 2;
            Item it; it.kind = Item::Tree;
            it.rect = {x, y, x + contentW, y + h};
            m_items.push_back(it);
            y += h + 24;
        }

        sec(L"Breadcrumb");
        {
            Item it; it.kind = Item::Breadcrumb;
            it.rect = {x, y, x + contentW, y + 36};
            m_items.push_back(it);
            y += 52;
        }

        sec(L"Pagination");
        {
            Item it; it.kind = Item::Pagination;
            it.rect = {x, y, x + contentW, y + 40};
            m_items.push_back(it);
            y += 56;
        }

        sec(L"Toast");
        {
            for (int i = 0; i < (int)m_toasts.size(); ++i) {
                Item it; it.kind = Item::Toast; it.value = i;
                it.rect = {x, y, x + contentW, y + 52};
                m_items.push_back(it);
                y += 62;
            }
        }

        sec(L"Overlay");
        {
            const wchar_t* labels[] = {L"Tooltip", L"Dropdown", L"Modal"};
            for (int i = 0; i < 3; ++i) {
                Item it; it.kind = Item::OverlayTrigger; it.value = i;
                it.text = labels[i];
                it.rect = {x + i * 150, y, x + i * 150 + 140, y + 32};
                m_items.push_back(it);
            }
            y += 44;
            Item hint; hint.kind = Item::OverlayHint;
            hint.text = L"Tab / Shift+Tab 遍历焦点 · Enter / Space 激活 · Esc 关闭 · 点击外部关闭非模态层";
            hint.rect = {x, y, x + contentW, y + 24};
            m_items.push_back(hint);
            y += 220;                     // 给弹出层留出展示空间
        }

        // P0 / P1 / P2 演示区：每个控件一个 section + 一个导航项。
        // 状态全部存在 m_x，resize 重建 Item 不会丢失交互。
        for (int i = 0; i < extragallery::Count(); ++i) {
            const auto& d = extragallery::Defs()[static_cast<size_t>(i)];
            sec(d.label);
            Item it; it.kind = Item::Extra; it.ext = i;
            it.rect = {x, y, x + contentW, y + d.height};
            m_items.push_back(it);
            y += d.height + 20;
        }

        // 页面焦点域：按 Item 出现顺序注册 Tab 序。id 用 100 + 序号，
        // resize 重建 Item 列表后依然稳定，焦点不会被重置到第一项。
        std::vector<FocusEntry> pageEntries;
        m_focusToItem.clear();
        for (size_t i = 0; i < m_items.size(); ++i) {
            const Item& it = m_items[i];
            const bool focusable =
                it.kind == Item::OverlayTrigger || it.kind == Item::Button ||
                it.kind == Item::Checkbox || it.kind == Item::ToggleSwitch;
            if (!focusable || it.disabled) continue;
            FocusEntry e;
            e.id = kFocusPageBase + (int)pageEntries.size();
            e.bounds = Rect{it.rect.left, it.rect.top, it.rect.right, it.rect.bottom};
            e.tabIndex = (int)pageEntries.size() * 10;
            e.group = 0;
            e.name = it.text;
            pageEntries.push_back(e);
            m_focusToItem[e.id] = i;
        }
        m_overlayCtl.SetPageEntries(std::move(pageEntries));

        int viewH = m_content.bottom - m_content.top;
        int contentH = y;                         // 标题/状态栏由 ShellCore 承担
        m_max = contentH - viewH;
        if (m_max < 0) m_max = 0;
    }

    void Click(POINT pt, UINT msg) {
        // 与 OnPaint 相同的坐标映射：内容区原点 - 滚动量
        POINT p = { pt.x - m_content.left, pt.y - (m_content.top - m_scroll) };

        // 数据/导航控件的 hover 保存在成员里（而非 Item），这样
        // 窗口 resize 重建布局后状态不会丢。先算出目标值再比较标脏。
        bool d = UpdateDataHover(p);
        if (msg == WM_MOUSEMOVE) d = UpdateOverlayHover(p) || d;

        for (auto& it : m_items) {
            bool h = PtInRect(&it.rect,p)!=FALSE;
            // hover for all interactive items
            if (msg==WM_MOUSEMOVE) {
                if (it.hover!=h){it.hover=h;d=true;}
            }
            if (it.kind==Item::Extra && msg==WM_LBUTTONDOWN && h && it.ext>=0) {
                std::wstring status;
                extragallery::ExtraClick(it.ext, Point{p.x,p.y},
                    Rect{it.rect.left,it.rect.top,it.rect.right,it.rect.bottom},
                    m_x, Mx(), status);
                if (!status.empty()) SetStatusText(status);
                d=true;
                continue;
            }
            if (it.kind==Item::Button) {
                if (msg==WM_LBUTTONDOWN && h){it.pressed=true;d=true;}
                if (msg==WM_LBUTTONUP){it.pressed=false;d=true;}
            } else if (it.kind==Item::OverlayTrigger) {
                if (msg==WM_LBUTTONDOWN && h) {
                    it.pressed=true;
                    OpenOverlay(it.value, Rect{it.rect.left,it.rect.top,
                                              it.rect.right,it.rect.bottom});
                    d=true;
                }
                if (msg==WM_LBUTTONUP){it.pressed=false;d=true;}
            } else if (it.kind==Item::Checkbox || it.kind==Item::Radio) {
                if (msg==WM_LBUTTONDOWN && h) {
                    if (it.kind==Item::Checkbox) {
                        it.checked=!it.checked;
                    } else {
                        for(auto& x:m_items) if(x.kind==Item::Radio) x.checked=false;
                        it.checked=true;
                    }
                    m_checked[it.text] = it.checked;
                    if (it.kind==Item::Radio) {
                        for (auto& x : m_items)
                            if (x.kind == Item::Radio) m_checked[x.text] = x.checked;
                    }
                    d=true;
                }
            } else if (it.kind==Item::ToggleSwitch) {
                if (msg==WM_LBUTTONDOWN && h && !it.disabled) {
                    it.toggled = !it.toggled;
                    d=true;
                }
            } else if (it.kind==Item::Slider) {
                if (msg==WM_LBUTTONDOWN && h && !it.disabled) {
                    int w = it.rect.right - it.rect.left;
                    if (w > 0) {
                        float t = float(p.x - it.rect.left) / float(w);
                        if (t < 0.f) t = 0.f;
                        if (t > 1.f) t = 1.f;
                        it.value = int(t * 100);
                        d=true;
                    }
                }
            } else if (it.kind==Item::Segmented) {
                if (msg==WM_LBUTTONDOWN && h && !it.disabled) {
                    int w = it.rect.right - it.rect.left;
                    if (w > 0) {
                        float t = float(p.x - it.rect.left) / float(w);
                        int seg = int(t * 3);
                        if (seg < 0) seg = 0;
                        if (seg > 2) seg = 2;
                        it.segmentedSelected = seg;
                        d=true;
                    }
                }
            } else if (it.kind==Item::Tag) {
                if (msg==WM_LBUTTONDOWN && h && !it.disabled) {
                    it.tagSelected = !it.tagSelected;
                    d=true;
                }
            } else if (it.kind==Item::Rating) {
                if (msg==WM_LBUTTONDOWN && h && !it.disabled) {
                    int w = it.rect.right - it.rect.left;
                    if (w > 0) {
                        float t = float(p.x - it.rect.left) / float(w);
                        int newRating = int(t * 5) + 1;
                        if (newRating < 1) newRating = 1;
                        if (newRating > 5) newRating = 5;
                        it.rating = newRating;
                        d=true;
                    }
                }
            } else if (it.kind==Item::Stepper) {
                if (msg==WM_LBUTTONDOWN && h && !it.disabled) {
                    int btnW = it.rect.bottom - it.rect.top;
                    int leftEdge = it.rect.left;
                    int rightEdge = it.rect.right - btnW;
                    if (p.x < leftEdge + btnW) {
                        it.stepperValue = it.stepperValue > 0 ? it.stepperValue - 1 : 0;
                        d=true;
                    } else if (p.x > rightEdge) {
                        it.stepperValue = it.stepperValue < 100 ? it.stepperValue + 1 : 100;
                        d=true;
                    }
                }
            } else if (it.kind==Item::Tabs) {
                if (msg==WM_LBUTTONDOWN && h && !it.disabled) {
                    int w = it.rect.right - it.rect.left;
                    if (w > 0) {
                        float t = float(p.x - it.rect.left) / float(w);
                        int seg = int(t * 4);
                        if (seg < 0) seg = 0;
                        if (seg > 3) seg = 3;
                        it.tabsSelected = seg;
                        d=true;
                    }
                }
            } else if (it.kind==Item::Table) {
                if (msg==WM_LBUTTONDOWN && h) {
                    Rect rc{it.rect.left,it.rect.top,it.rect.right,it.rect.bottom};
                    const int row = TableRowAt(rc, m_table, Mx(), Point{p.x,p.y});
                    if (row == -1) {
                        const int col = TableColumnAt(rc, m_table, Mx(), Point{p.x,p.y});
                        if (col >= 0) {
                            if (m_table.sortColumn == col) {
                                m_table.sortAscending = !m_table.sortAscending;
                            } else {
                                m_table.sortColumn = col;
                                m_table.sortAscending = true;
                            }
                            auto cell = [](const std::vector<std::wstring>& r, int c) {
                                return c < (int)r.size() ? r[c] : std::wstring();
                            };
                            std::sort(m_table.rows.begin(), m_table.rows.end(),
                                [&](const std::vector<std::wstring>& a,
                                    const std::vector<std::wstring>& b) {
                                    const std::wstring x = cell(a, col);
                                    const std::wstring y = cell(b, col);
                                    return m_table.sortAscending ? x < y : x > y;
                                });
                            SetStatusText(L"Table：按「" + m_table.columns[col] +
                                          (m_table.sortAscending ? L"」升序" : L"」降序"));
                            d=true;
                        }
                    } else if (row >= 0) {
                        m_table.selectedRow = row;
                        SetStatusText(L"Table：已选中第 " + std::to_wstring(row+1) + L" 行");
                        d=true;
                    }
                }
            } else if (it.kind==Item::List) {
                if (msg==WM_LBUTTONDOWN && h) {
                    Rect rc{it.rect.left,it.rect.top,it.rect.right,it.rect.bottom};
                    const int row = ListRowAt(rc, m_list, Mx(), Point{p.x,p.y});
                    if (row >= 0 && !m_list.items[row].disabled) {
                        m_list.selected = row;
                        SetStatusText(L"List：已选中「" + m_list.items[row].title + L"」");
                        d=true;
                    }
                }
            } else if (it.kind==Item::Tree) {
                if (msg==WM_LBUTTONDOWN && h) {
                    Rect rc{it.rect.left,it.rect.top,it.rect.right,it.rect.bottom};
                    Point hit{p.x,p.y};
                    const int toggle = TreeToggleAt(rc, m_tree, Mx(), hit);
                    if (toggle >= 0) {
                        TreeNode& node = m_tree.nodes[toggle];
                        node.expanded = !node.expanded;
                        SetStatusText(std::wstring(L"Tree：") +
                                      (node.expanded ? L"展开「" : L"折叠「") +
                                      node.text + L"」");
                        m_needRebuild = true;   // 可见行数变化，循环结束后重排
                        d=true;
                    } else {
                        const int row = TreeRowAt(rc, m_tree, Mx(), hit);
                        if (row >= 0 && !m_tree.nodes[row].disabled) {
                            m_tree.selected = row;
                            SetStatusText(L"Tree：已选中「" + m_tree.nodes[row].text + L"」");
                            d=true;
                        }
                    }
                }
            } else if (it.kind==Item::Breadcrumb) {
                if (msg==WM_LBUTTONDOWN && h) {
                    Rect rc{it.rect.left,it.rect.top,it.rect.right,it.rect.bottom};
                    const int idx = BreadcrumbHit(rc, m_crumb, Mx(), Point{p.x,p.y});
                    if (idx >= 0) {
                        m_crumb.active = idx;
                        SetStatusText(L"Breadcrumb：跳转到「" + m_crumb.items[idx] + L"」");
                        d=true;
                    }
                }
            } else if (it.kind==Item::Pagination) {
                if (msg==WM_LBUTTONDOWN && h) {
                    Rect rc{it.rect.left,it.rect.top,it.rect.right,it.rect.bottom};
                    const int code = PaginationHit(rc, m_pager, Mx(), Point{p.x,p.y});
                    // 不用 std::max/min：windows.h 的 min/max 宏会与之冲突
                    if (code == -2)      m_pager.page = m_pager.page > 1 ? m_pager.page - 1 : 1;
                    else if (code == -1) m_pager.page = m_pager.page < m_pager.pageCount
                                                        ? m_pager.page + 1 : m_pager.pageCount;
                    else if (code > 0)   m_pager.page = code;
                    if (code != 0) {
                        SetStatusText(L"Pagination：第 " + std::to_wstring(m_pager.page) +
                                      L" / " + std::to_wstring(m_pager.pageCount) + L" 页");
                        d=true;
                    }
                }
            } else if (it.kind==Item::Toast) {
                if (msg==WM_LBUTTONDOWN && h) {
                    const int idx = it.value;
                    if (idx >= 0 && idx < (int)m_toasts.size()) {
                        if (m_toastHidden[idx]) {
                            m_toastHidden[idx] = false;
                            SetStatusText(L"Toast：重新显示");
                        } else {
                            Rect rc{it.rect.left,it.rect.top,it.rect.right,it.rect.bottom};
                            if (ToastCloseHit(rc, m_toasts[idx], Mx(), Point{p.x,p.y})) {
                                m_toastHidden[idx] = true;
                                SetStatusText(L"Toast：已关闭");
                            } else {
                                int k = (int)m_toasts[idx].kind + 1;
                                if (k > (int)AlertKind::Error) k = (int)AlertKind::Info;
                                m_toasts[idx].kind = (AlertKind)k;
                                SetStatusText(L"Toast：切换状态类型");
                            }
                        }
                        d=true;
                    }
                }
            }
        }
        for (auto& c : m_cards) { c->SetHover(PtInRect(&c->GetRect(),p)!=FALSE); d=true; }
        // Build() 会清空 m_items，不能在上面的遍历过程中调用
        if (m_needRebuild) {
            m_needRebuild = false;
            Build();
            ClampScroll();
            UpdateActiveNav();
            d=true;
        }
        if (d && m_shell) m_shell->Invalidate();
    }

    Shell* m_shell=nullptr;
    RECT m_content{};
    std::vector<Item> m_items;
    std::vector<std::unique_ptr<Card>> m_cards;
    std::map<std::wstring, bool> m_checked;   // resize 后保留选中状态
    std::map<std::wstring, int>  m_sectionTop;   // section 标题 -> y
    std::vector<std::pair<std::wstring,int>> m_sections; // 构建顺序 (标题, y)
    std::map<int, std::wstring>      m_navLabelById;   // 导航 id -> 标题
    std::map<std::wstring, int>      m_navIdByLabel;   // 标题 -> 导航 id
    int m_scroll=0, m_max=0;
    bool m_dark=false;
    ShellCore m_shellCore;
    std::unique_ptr<Win32Window> m_win;

    // Table / List / Tree / Breadcrumb / Pagination / Toast 状态
    bool m_dataReady = false;
    TableState       m_table;
    ListState        m_list;
    TreeState        m_tree;
    BreadcrumbState  m_crumb;
    PaginationState  m_pager;
    std::vector<ToastState> m_toasts;
    std::vector<bool>       m_toastHidden;
    int  m_toastHoverIndex = -1;
    bool m_toastHoverClose = false;
    bool m_needRebuild = false;

    // Overlay 层与键盘焦点（core 状态机，resize 后保留）
    OverlayController m_overlayCtl;
    std::map<int, size_t> m_focusToItem;   // focus id -> m_items 下标
    int m_overlayHoverOption = -1;

    // P0 framework / P1 migration / P2 controls demo state. Owned by
    // the page so Build() never resets it.
    extragallery::ExtraDemoState m_x;
    AnimationClock m_animClock;
    DemoClock      m_tickClock;
    int            m_dpiIndex = 0;
    RECT           m_lastClient{};
};
}

int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int) {
    SetProcessDPIAware();
    SetPalette(LightPalette());
    ShellOptions opt;
    opt.className=L"AirUiGallery"; opt.appName=L"AirUI Gallery"; opt.logoLetter=L"A";
    opt.width=900; opt.height=650; opt.sidebar=false; opt.statusBar=false;
    Shell shell;
    if (!shell.Create(inst,opt)) return 1;
    shell.AddPage(1,std::unique_ptr<IPage>(new GalleryPage()));
    shell.SwitchPage(1);
    return shell.RunMessageLoop();
}
