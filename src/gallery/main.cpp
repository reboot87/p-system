#include "windows.h"
#include "windowsx.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "airui/AirUi.h"
#include "airui/Components.h"

using namespace air::ui;

namespace {

struct Item {
    enum Kind { Section, Button, Checkbox, Radio } kind = Button;
    RECT rect;
    std::wstring text;
    BtnStyle style = BtnStyle::Primary;
    bool checked = false;
    bool disabled = false;
    bool hover = false;
    bool pressed = false;
};

class GalleryPage : public Page {
public:
    const wchar_t* Title() const override { return L"组件画廊"; }

    void OnAttach(Shell& shell) override {
        m_shell = &shell;
        shell.SetCaptionSubtitle(L"简洁示例");
        HWND hwnd = shell.Hwnd();
        shell.SetMessageHook([this, hwnd](HWND,UINT msg,WPARAM wp,LPARAM lp,LRESULT& res){
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
        if (m_scroll > m_max) m_scroll = m_max;
        if (m_shell) m_shell->Invalidate();
    }

    void OnPaint(Canvas& g, const RECT& content) override {
        const auto& p = Pal();
        g.Fill(content,p.bg);
        HDC dc = g.Native();
        int s = dc ? SaveDC(dc) : 0;
        if (dc) IntersectClipRect(dc,content.left,content.top,content.right,content.bottom);

        const int scroll = m_scroll;
        const int titleH = 52;                       // 标题占位高度
        const int ox = content.left;                 // 内容区原点
        const int oy = content.top + titleH - scroll; // 条目 y 原点

        g.Text({content.left + 12, content.top + 12,
                content.left + 400, content.top + 40},
               L"AirUI Gallery", FontRole::Display, p.text,
               DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

        for (auto& it : m_items) {
            RECT r = it.rect;
            OffsetRect(&r, ox, oy);
            if (r.bottom < content.top || r.top > content.bottom) continue;
            if (it.kind == Item::Section) {
                g.Text(r,it.text,FontRole::Section,p.text,DT_SINGLELINE|DT_LEFT|DT_VCENTER);
                g.HLine(r.left,r.right,r.bottom,p.split);
            } else if (it.kind == Item::Button) {
                g.Button(r,it.text,it.style,it.hover,it.pressed,it.disabled);
            } else if (it.kind == Item::Checkbox) {
                RECT b={r.left,r.top+4,r.left+16,r.top+20};
                g.FillRound(b,p.surface,3); g.Stroke(b,p.border,3);
                if (it.checked) g.Text(b,L"✓",FontRole::Body,p.primary,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                g.Text({r.left+24,r.top,r.right,r.bottom},it.text,FontRole::Body,p.text,DT_SINGLELINE|DT_LEFT|DT_VCENTER);
            } else if (it.kind == Item::Radio) {
                RECT b={r.left,r.top+4,r.left+16,r.top+20};
                g.FillRound(b,p.surface,8); g.Stroke(b,p.border,8);
                if (it.checked) { RECT d={b.left+4,b.top+4,b.right-4,b.bottom-4}; g.FillRound(d,p.primary,4); }
                g.Text({r.left+24,r.top,r.right,r.bottom},it.text,FontRole::Body,p.text,DT_SINGLELINE|DT_LEFT|DT_VCENTER);
            }
        }
        for (auto& c : m_cards) {
            RECT r = c->GetRect();
            OffsetRect(&r, ox, oy);
            if (r.bottom < content.top || r.top > content.bottom) continue;
            RECT old = c->GetRect(); c->SetRect(r); c->Paint(g); c->SetRect(old);
        }
        if (dc && s) RestoreDC(dc,s);
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

        auto sec = [&](const wchar_t* t){ Item it; it.kind=Item::Section; it.text=t; it.rect={x,y,x+contentW,y+22}; m_items.push_back(it); y+=30; };

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
            if (it.kind==Item::Button) {
                if (msg==WM_MOUSEMOVE && it.hover!=h){it.hover=h;d=true;}
                if (msg==WM_LBUTTONDOWN && h){it.pressed=true;d=true;}
                if (msg==WM_LBUTTONUP){it.pressed=false;d=true;}
            } else if (it.kind==Item::Checkbox || it.kind==Item::Radio) {
                if (msg==WM_MOUSEMOVE && it.hover!=h){it.hover=h;d=true;}
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
