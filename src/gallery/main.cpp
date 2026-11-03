// ============================================================
// AirUI Gallery — 清晰布局的可视化组件画廊
// ============================================================
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "airui/AirUi.h"
#include "airui/Components.h"
#include "gallery/GalleryDraw.h"

using namespace air::ui;

namespace {
constexpr int kScrollStep = 72;

enum class CompType { Button, Checkbox, Radio, Text, Label, Card };

struct CompInstance {
    CompType type = CompType::Button;
    RECT rect{};
    std::wstring text;
    BtnStyle btnStyle = BtnStyle::Primary;
    bool hover = false;
    bool pressed = false;
    bool disabled = false;
    bool checked = false;
};

class GalleryPage : public Page
{
public:
    const wchar_t* Title() const override { return L"组件画廊"; }

    void OnAttach(Shell& shell) override
    {
        m_shell = &shell;
        m_content = shell.ContentRect();

        shell.SetCaptionSubtitle(L"Button · Checkbox · Radio · Text · Label · Card");
        shell.SetStatusKeys({
            { L"滚轮", L"滚动" },
            { L"T", L"切换主题" },
            { L"Home/End", L"顶部/底部" },
        });

        shell.SetMessageHook([this](HWND, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result)->bool {
            if (msg == WM_MOUSEWHEEL) {
                int steps = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
                if (steps == 0) steps = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1;
                ScrollBy(-steps * kScrollStep);
                result = 0; return true;
            }
            if (msg == WM_KEYDOWN) {
                if (wParam == 'T') { ToggleTheme(); result = 0; return true; }
                if (wParam == VK_HOME) { SetScroll(0); result = 0; return true; }
                if (wParam == VK_END) { SetScroll(MaxScroll()); result = 0; return true; }
                if (wParam == VK_PRIOR) { ScrollBy(-(ViewHeight()-40)); result = 0; return true; }
                if (wParam == VK_NEXT) { ScrollBy(ViewHeight()-40); result = 0; return true; }
                if (wParam == VK_UP) { ScrollBy(-40); result = 0; return true; }
                if (wParam == VK_DOWN) { ScrollBy(40); result = 0; return true; }
            }
            if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) {
                HandleHit(msg, wParam, lParam);
                result = 0; return true;
            }
            return false;
        });

        BuildComponents();
        UpdateStatus();
    }

    void OnLayout(const RECT& content) override
    {
        m_content = content;
        m_scroll = std::min(m_scroll, MaxScroll());
    }

    void OnPaint(Canvas& g, const RECT& content) override
    {
        const Palette& p = Pal();
        g.Fill(content, p.bg);

        // 标题
        RECT header = { content.left+24, content.top+24, content.right-24, content.top+64 };
        g.Text(header, L"AirUI 组件画廊", FontRole::Display, p.text, DT_SINGLELINE|DT_LEFT|DT_VCENTER);

        int y = content.top + 80;
        int scrollOffset = m_scroll;
        // 绘制所有实例
        for (auto& c : m_comps) {
            RECT rc = c.rect;
            rc.top -= scrollOffset;
            rc.bottom -= scrollOffset;
            if (rc.bottom < content.top || rc.top > content.bottom) continue;
            PaintComponent(g, c, rc);
        }
        // 绘制卡片
        for (auto& card : m_cards) {
            RECT rc = card->GetRect();
            rc.top -= scrollOffset;
            rc.bottom -= scrollOffset;
            if (rc.bottom < content.top || rc.top > content.bottom) continue;
            // 临时调整卡片位置
            RECT old = card->GetRect();
            card->SetRect(rc);
            card->Paint(g);
            card->SetRect(old);
        }

        DrawScrollBar(g, content);
    }

private:
    void BuildComponents()
    {
        m_comps.clear();
        m_cards.clear();
        int y = 120;
        const int left = 40;
        const int gap = 16;

        auto section = [&](const wchar_t* title){
            CompInstance c; c.type = CompType::Label; c.text = title;
            c.rect = { left, y, left+600, y+28 };
            m_comps.push_back(c);
            y += 36;
        };
        auto btn = [&](const wchar_t* txt, BtnStyle s){
            CompInstance c; c.type = CompType::Button; c.text = txt; c.btnStyle = s;
            c.rect = { left, y, left+180, y+36 };
            m_comps.push_back(c);
            y += 52;
        };
        auto check = [&](const wchar_t* txt, bool checked=false, bool disabled=false){
            CompInstance c; c.type = CompType::Checkbox; c.text = txt; c.checked = checked; c.disabled = disabled;
            c.rect = { left, y, left+260, y+24 };
            m_comps.push_back(c);
            y += 32;
        };
        auto radio = [&](const wchar_t* txt, bool checked=false){
            CompInstance c; c.type = CompType::Radio; c.text = txt; c.checked = checked;
            c.rect = { left, y, left+260, y+24 };
            m_comps.push_back(c);
            y += 32;
        };
        auto text = [&](const wchar_t* txt){
            CompInstance c; c.type = CompType::Text; c.text = txt;
            c.rect = { left, y, left+900, y+24 };
            m_comps.push_back(c);
            y += 36;
        };

        section(L"按钮 / Buttons");
        btn(L"主按钮", BtnStyle::Primary);
        btn(L"次按钮", BtnStyle::Secondary);
        btn(L"危险操作", BtnStyle::Danger);
        btn(L"幽灵按钮", BtnStyle::Ghost);

        section(L"复选框 / Checkbox");
        check(L"选项 A", true);
        check(L"选项 B", false);
        check(L"禁用项", false, true);

        section(L"单选框 / Radio");
        radio(L"红色", false);
        radio(L"绿色", true);
        radio(L"蓝色", false);

        section(L"文本与标签 / Text & Label");
        text(L"正文示例：AirUI 提供企业级 Win32 自绘组件库，支持主题切换、滚动、交互状态。");
        CompInstance lbl; lbl.type = CompType::Label; lbl.text = L"状态标签"; lbl.rect = { left, y, left+200, y+26 }; m_comps.push_back(lbl); y+=40;

        // 卡片示例
        section(L"卡片 / Card");
        int cardY = y;
        const int cardW = 340; const int cardH = 180;
        for (int i=0;i<3;i++){
            Card::Config cfg;
            cfg.rect = { left + i*(cardW+24), cardY, left + i*(cardW+24)+cardW, cardY+cardH };
            cfg.title = L"示例卡片";
            cfg.desc = L"这是一个可交互的卡片组件";
            cfg.number = L"128";
            cfg.unit = L"条记录";
            cfg.badge.text = L"运行中";
            cfg.badge.ok = true;
            cfg.linkText = L"进入 →";
            cfg.icon.glyph = L"\xE8C8";
            cfg.icon.bg = Pal().primarySoft;
            cfg.icon.color = Pal().primary;
            auto card = std::make_unique<Card>(cfg);
            m_cards.push_back(std::move(card));
        }
        m_totalHeight = cardY + cardH + 200;
    }

    void PaintComponent(Canvas& g, const CompInstance& c, const RECT& rc)
    {
        const Palette& p = Pal();
        switch (c.type) {
        case CompType::Button:
            g.Button(rc, c.text, c.btnStyle, c.hover, c.pressed, c.disabled);
            break;
        case CompType::Checkbox: {
            RECT box = { rc.left, rc.top+3, rc.left+18, rc.top+18 };
            g.FillRound(box, p.surface, 4);
            g.Stroke(box, p.border, 4);
            if (c.checked && !c.disabled) {
                RECT chk = { box.left+4, box.top+4, box.right-4, box.bottom-4 };
                g.FillRound(chk, p.primary, 2);
            }
            g.Text({rc.left+26, rc.top, rc.right, rc.bottom}, c.text, FontRole::Body, c.disabled? p.textDisabled : p.text, DT_SINGLELINE|DT_LEFT|DT_VCENTER);
            break;
        }
        case CompType::Radio: {
            RECT box = { rc.left, rc.top+3, rc.left+18, rc.top+18 };
            g.FillRound(box, p.surface, 9);
            g.Stroke(box, p.border, 9);
            if (c.checked) {
                RECT inner = { box.left+5, box.top+5, box.right-5, box.bottom-5 };
                g.FillRound(inner, p.primary, 4);
            }
            g.Text({rc.left+26, rc.top, rc.right, rc.bottom}, c.text, FontRole::Body, p.text, DT_SINGLELINE|DT_LEFT|DT_VCENTER);
            break;
        }
        case CompType::Text:
            g.Text(rc, c.text, FontRole::Body, p.text, DT_SINGLELINE|DT_LEFT|DT_VCENTER|DT_END_ELLIPSIS);
            break;
        case CompType::Label:
            g.Text(rc, c.text, FontRole::Section, p.text, DT_SINGLELINE|DT_LEFT|DT_VCENTER);
            break;
        default: break;
        }
    }

    void HandleHit(UINT msg, WPARAM, LPARAM lParam)
    {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        pt.y += m_scroll;
        bool needInvalidate = false;
        for (auto& c : m_comps) {
            bool inside = PtInRect(&c.rect, pt);
            if (c.type == CompType::Button && !c.disabled) {
                if (msg == WM_MOUSEMOVE) { if (c.hover != inside) { c.hover = inside; needInvalidate = true; } }
                else if (msg == WM_LBUTTONDOWN) { if (inside) { c.pressed = true; needInvalidate = true; } }
                else if (msg == WM_LBUTTONUP) { c.pressed = false; needInvalidate = true; }
            } else if ((c.type == CompType::Checkbox || c.type == CompType::Radio) && !c.disabled) {
                if (msg == WM_LBUTTONDOWN && inside) {
                    if (c.type == CompType::Checkbox) c.checked = !c.checked;
                    else {
                        for (auto& r : m_comps) if (r.type == CompType::Radio) r.checked = false;
                        c.checked = true;
                    }
                    needInvalidate = true;
                }
            }
        }
        for (auto& card : m_cards) {
            RECT rc = card->GetRect();
            bool inside = PtInRect(&rc, pt);
            card->SetHover(inside);
        }
        if (needInvalidate && m_shell) m_shell->Invalidate();
    }

    int ViewHeight() const { int h = m_content.bottom - m_content.top; return h>0?h:0; }
    int MaxScroll() const { return std::max(0, m_totalHeight - ViewHeight()); }
    void SetScroll(int v){ int clamped = std::max(0, std::min(v, MaxScroll())); if(clamped!=m_scroll){ m_scroll=clamped; if(m_shell) m_shell->Invalidate(); UpdateStatus(); } }
    void ScrollBy(int d){ SetScroll(m_scroll+d); }
    void ToggleTheme(){ m_dark=!m_dark; SetPalette(m_dark?DarkPalette():LightPalette()); if(m_shell) m_shell->Invalidate(); UpdateStatus(); }
    void UpdateStatus()
    {
        if(!m_shell) return;
        int maxS = MaxScroll();
        int pct = maxS>0 ? (m_scroll*100+maxS/2)/maxS : 0;
        wchar_t extra[64]; swprintf_s(extra, L"%s %d%%", m_dark?L"深色":L"浅色", pct);
        m_shell->SetStatus(L"组件画廊", true);
        m_shell->SetStatusExtra(extra);
    }
    void DrawScrollBar(Canvas& g, const RECT& content)
    {
        int viewH = content.bottom - content.top;
        int maxS = MaxScroll();
        if(viewH<=0||maxS<=0) return;
        const Palette& p = Pal();
        int x = content.right - 8;
        RECT track = { x, content.top, x+6, content.bottom };
        g.FillRound(track, p.split, 3);
        int thumbH = std::max(40, viewH*viewH/m_totalHeight);
        int thumbY = content.top + (viewH-thumbH)*m_scroll/maxS;
        RECT thumb = { x, thumbY, x+6, thumbY+thumbH };
        g.FillRound(thumb, p.textDisabled, 3);
    }

    std::vector<CompInstance> m_comps;
    std::vector<std::unique_ptr<Card>> m_cards;
    RECT m_content{};
    int m_scroll = 0;
    int m_totalHeight = 2000;
    bool m_dark = false;
    Shell* m_shell = nullptr;
};
} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    SetProcessDPIAware();
    SetPalette(LightPalette());

    ShellOptions opt;
    opt.className = L"AirUiGalleryWnd";
    opt.appName = L"AirUI Gallery";
    opt.logoLetter = L"A";
    opt.width = 1280; opt.height = 900;
    opt.minWidth = 960; opt.minHeight = 640;
    opt.sidebar = false;
    opt.statusBar = true;
    opt.dark = false;

    Shell shell;
    if (!shell.Create(hInstance, opt)) { MessageBoxW(nullptr, L"创建失败", L"AirUI Gallery", MB_ICONERROR); return 1; }
    shell.AddPage(1, std::make_unique<GalleryPage>());
    shell.SwitchPage(1);
    return shell.RunMessageLoop();
}
