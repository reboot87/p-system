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
#include "airui/platform/win32/airui/CanvasPainter.h"
#include "airui/platform/win32/airui/Win32Window.h"

using namespace air::ui;

namespace {

struct Item {
    enum Kind { Section, Button, Checkbox, Radio, Slider, ProgressBar, ToggleSwitch, TextField, Segmented, ComboBox, Tag, Rating, Stepper, ProgressRing, Badge } kind = Button;
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
};

class GalleryPage : public Page {
public:
    const wchar_t* Title() const override { return L"组件画廊"; }

    void OnAttach(Shell& shell) override {
        m_shell = &shell;
        shell.SetCaptionSubtitle(L"简洁示例");
        // 初始化 ShellCore，展示核心状态机与 IPainter 集成
        m_shellCore.SetCaption({L"AirUI Gallery", L"简洁示例", L"", true});
        m_shellCore.SetStatus({L"就绪", true, L""});
        std::vector<ShellNavGroup> groups;
        ShellNavGroup g;
        g.title = L"组件";
        g.items.push_back({1, L"Button", false, L"", true});
        g.items.push_back({2, L"Checkbox", false, L"", true});
        g.items.push_back({3, L"Radio", false, L"", true});
        groups.push_back(std::move(g));
        m_shellCore.SetNavGroups(std::move(groups));
        m_shellCore.SetActiveNav(1);
        HWND hwnd = shell.Hwnd();
        m_win = std::make_unique<Win32Window>(hwnd);
        // Win32Window 事件回调可扩展到 ShellCore 状态更新
        m_win->OnMouseMove([this](air::ui::Point pt){ /* 命中测试可接入 ShellCore */ });
        m_win->OnResized([this](air::ui::Rect rc){ /* 布局更新可接入 ShellCore */ });
        shell.SetMessageHook([this, hwnd](HWND,UINT msg,WPARAM wp,LPARAM lp,LRESULT& res){
            if (m_win) m_win->PumpEvent(msg, wp, lp);
            if (msg == WM_KEYDOWN && wp == 'T') {
                m_dark = !m_dark;
                SetPalette(m_dark ? DarkPalette() : LightPalette());
                if (m_shell) m_shell->Invalidate();
                res = 0; return true;
            }
            if (msg == WM_MOUSEWHEEL) {
                POINT pt = {(short)LOWORD(lp),(short)HIWORD(lp)};
                ScreenToClient(hwnd,&pt);
                if (!PtInRect(&m_content, pt)) return false;
                int d = GET_WHEEL_DELTA_WPARAM(wp);
                m_scroll -= d/120*30;
                if (m_scroll < 0) m_scroll = 0;
                if (m_scroll > m_max) m_scroll = m_max;
                if (m_shell) m_shell->Invalidate();
                res = 0; return true;
            }
            if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
                POINT pt = {(short)LOWORD(lp),(short)HIWORD(lp)};
                if (msg == WM_LBUTTONUP && GetCapture()==hwnd) ReleaseCapture();
                if (!PtInRect(&m_content, pt)) return false;
                if (msg == WM_LBUTTONDOWN) SetCapture(hwnd);
                Click(pt,msg);
                res = 0; return true;
            }
            return false;
        });
    }

    void OnLayout(const RECT& content) override {
        m_content = content;
        Build();
        // 布局 ShellCore 到内容区
        Rect rc{content.left, content.top, content.right, content.bottom};
        m_shellCore.Layout(rc);
        if (m_scroll > m_max) m_scroll = m_max;
        if (m_shell) m_shell->Invalidate();
    }

    void OnPaint(Canvas& g, const RECT& content) override {
        const auto& p = Pal();
        g.Fill(content,p.bg);
        HDC dc = g.Native();
        int s = dc ? SaveDC(dc) : 0;
        if (dc) IntersectClipRect(dc,content.left,content.top,content.right,content.bottom);

        // ShellCore 集成：通过 CanvasPainter 将核心状态机绘制到画廊
        {
            Rect rc{content.left, content.top, content.right, content.bottom};
            CanvasPainter painter(g);
            m_shellCore.Paint(painter, p, Mx());
        }

        const int scroll = m_scroll;
        const int titleH = 52;                       // 标题占位高度
        const int ox = content.left;                 // 内容区原点
        const int oy = content.top + titleH - scroll; // 条目 y 原点

        // 原标题文本由 ShellCore.Caption 绘制，去掉重复绘制
        // g.Text({content.left + 12, content.top + 12,
        //         content.left + 400, content.top + 40},
        //        L"AirUI Gallery", FontRole::Display, p.text,
        //        DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

        for (auto& it : m_items) {
            RECT r = it.rect;
            OffsetRect(&r, ox, oy);
            if (r.bottom < content.top || r.top > content.bottom) continue;
            // Hover highlight for all interactive items
            if (it.kind != Item::Section && it.hover && !it.disabled) {
                RECT hl = r;
                hl.top -= 4;
                hl.bottom += 4;
                hl.left -= 4;
                hl.right += 4;
                g.FillRound(hl, Pal().active, 6);
            }
            if (it.kind == Item::Section) {
                // Section header with card background and accent
                RECT bg = r;
                bg.top -= 10;
                bg.bottom += 14;
                bg.left -= 16;
                bg.right += 16;
                g.FillRound(bg, Pal().surface, 10);
                // accent bar
                RECT accent = {bg.left + 4, bg.top + 8, bg.left + 8, bg.bottom - 8};
                g.FillRound(accent, Pal().primary, 2);
                g.Text(r,it.text,FontRole::Section,p.text,DT_SINGLELINE|DT_LEFT|DT_VCENTER);
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
            }
        }
        for (auto& c : m_cards) {
            RECT r = c->GetRect();
            OffsetRect(&r, ox, oy);
            if (r.bottom < content.top || r.top > content.bottom) continue;
            RECT old = c->GetRect(); c->SetRect(r); c->Paint(g); c->SetRect(old);
        }
        if (dc && s) RestoreDC(dc,s);

        if (m_max > 0) {
            const int h = content.bottom - content.top;
            const int barX = content.right - 6;
            const int top = content.top + titleH;
            const int trackH = content.bottom - top;
            if (trackH > 0) {
                RECT track = { barX, top, barX + 4, content.bottom };
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
    void Build() {
        // 保留交互状态：resize 重建布局时，复选/单选的选中状态不丢失
        for (auto& it : m_items) {
            if (it.kind == Item::Checkbox || it.kind == Item::Radio)
                m_checked[it.text] = it.checked;
        }

        m_items.clear(); m_cards.clear();
        int y = 0;
        const int x = 12;
        int w = m_content.right - m_content.left;
        if (w < 200) w = 200;
        const int contentW = w - 24;

        auto sec = [&](const wchar_t* t){ Item it; it.kind=Item::Section; it.text=t; it.rect={x,y,x+contentW,y+22}; m_items.push_back(it); y+=60; };

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

        const int titleH = 52;
        int viewH = m_content.bottom - m_content.top;
        int contentH = y + titleH;
        m_max = contentH - viewH;
        if (m_max < 0) m_max = 0;
    }

    void Click(POINT pt, UINT msg) {
        // 与 OnPaint 相同的坐标映射：内容区原点 + 标题占位 - 滚动量
        const int titleH = 52;
        POINT p = { pt.x - m_content.left, pt.y - (m_content.top + titleH - m_scroll) };
        bool d=false;
        for (auto& it : m_items) {
            bool h = PtInRect(&it.rect,p)!=FALSE;
            // hover for all interactive items
            if (msg==WM_MOUSEMOVE) {
                if (it.hover!=h){it.hover=h;d=true;}
            }
            if (it.kind==Item::Button) {
                if (msg==WM_LBUTTONDOWN && h){it.pressed=true;d=true;}
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
            }
        }
        for (auto& c : m_cards) { c->SetHover(PtInRect(&c->GetRect(),p)!=FALSE); d=true; }
        if (d && m_shell) m_shell->Invalidate();
    }

    Shell* m_shell=nullptr;
    RECT m_content{};
    std::vector<Item> m_items;
    std::vector<std::unique_ptr<Card>> m_cards;
    std::map<std::wstring, bool> m_checked;   // resize 后保留选中状态
    int m_scroll=0, m_max=0;
    bool m_dark=false;
    ShellCore m_shellCore;
    std::unique_ptr<Win32Window> m_win;
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
