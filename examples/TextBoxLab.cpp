// ============================================================
// TextBoxLab.cpp — 自绘 TextBox 独立测试壳
//
// 排查「光标不闪 / 位置不对」的最小复现环境：一个窗口放七个
// 自绘 TextBox（①单行 / ②单行长内容横向滚动 / ③多行短 / ④多行
// 长内容纵向滚动 / ⑤maxLength=5 / ⑥password 掩码 / ⑦⑥ 的明文
// 孪生，后三个供跨进程 T5 功能验证），底部一行闪烁相位探针（按系统
// GetCaretBlinkTime() 的一半翻相位，与控件自身闪烁定时器同周期
// 反相位；连拍截图应看到 亮/灭/亮/灭 交替，光标常亮则四帧全亮）。
//
// 编译：cmake 目标 airui_textbox_lab（产出 build\airui_textbox_lab.exe）
// 验证步骤（人工）：
//   1. 点任一框聚焦 → 光标出现并每 ≈ GetCaretBlinkTime() 亮灭一次
//   2. 键入英文/中文 → 光标紧跟最后一字（不漂移、不整体偏移）
//   3. 点击文本中间 → 光标落到点击处（比例字体按逐字宽度定位）
//   4. 左右/Home/End/Ctrl+左右 → 光标逐字符/逐词/到两端
//   5. 多行长框里键入到深处 → caret 行自动滚进视口
//   6. 选中一段文字（Shift+方向/拖选）→ 选区高亮与光标同一几何
// ============================================================
#include <windows.h>
#include "airui/AirUi.h"

using namespace air::ui;

// ListLabPage 子控件 id（3b 第 3 项列表多选用）
static const int IDC_LBL_MULTI  = 3201;
static const int IDC_LBL_SINGLE = 3202;
static const int IDC_LBL_MIRROR = 3203;

// 光标闪烁相位探针：周期 = GetCaretBlinkTime()（系统值，默认 530ms），
// 每半个周期翻一次相位。
struct BlinkProbe
{
    std::wstring label = L"caret-blink=on";
    ULONGLONG nextTick = 0;
    bool phase = true;

    void Tick()
    {
        const UINT ms = GetCaretBlinkTime();
        if (ms == INFINITE) { label = L"caret-blink=off(system)"; return; }
        const ULONGLONG half = ms / 2;
        const ULONGLONG now = GetTickCount64();
        if (nextTick == 0) nextTick = now + half;
        while (now >= nextTick) {
            phase = !phase;
            nextTick += half;
        }
        label = phase ? L"caret-blink=on" : L"caret-blink=off";
    }
};

class LabPage : public Page
{
public:
    const wchar_t* Title() const override    { return L"TextBox 自绘实验室"; }
    const wchar_t* NavGlyph() const override { return L"\xE70F"; }
    const wchar_t* NavLabel() const override { return L"TextBox 实验室"; }

    void OnAttach(Shell& shell) override
    {
        (void)shell;
        m_single = &CreateTextBox({ 3001, false,
                                    L"单行：试试英文 abc 与中文 输入…",
                                    FontRole::Body });
        m_singleLong = &CreateTextBox({ 3002, false, L"", FontRole::Body });
        m_singleLong->SetText(L"横向滚动测试：the quick brown fox jumps over the lazy dog "
                              L"0123456789 0123456789 0123456789 0123456789 "
                              L"甲乙丙丁戊己庚辛壬癸 "
                              L"the quick brown fox jumps over the lazy dog");
        m_multi = &CreateTextBox({ 3003, true, L"多行：\n第二行\n第三行",
                                   FontRole::Body });
        m_multiLong = &CreateTextBox({ 3004, true, L"", FontRole::Body });
        std::wstring longText;
        for (int i = 1; i <= 60; ++i) {
            longText += L"第 " + std::to_wstring(i) +
                        L" 行：自绘光标位置与闪烁测试 — line " +
                        std::to_wstring(i) + L" of 60\r\n";
        }
        m_multiLong->SetText(longText);
        m_multiLong->CaretToEnd();

        // ⑤⑥⑦ 功能验证框：maxLength / password / password 明文孪生（跨进程 T5 用）。
        // 放在 3004 之后，前四个框位置不变（T1-T4 依赖 3001 的坐标）。
        // ⑥ password 与 ⑦ 同文本明文孪生：T5 用「两框像素不同」证明
        // ⑥ 渲染的是掩码而非明文（不依赖字体度量假设）。
        TextBoxDesc dMax; dMax.id = 3005; dMax.multiline = false;
        dMax.placeholder = L"maxLen=5：键入超过 5 字符被截断…"; dMax.maxLength = 5;
        m_maxLen = &CreateTextBox(dMax);
        TextBoxDesc dPwd; dPwd.id = 3006; dPwd.multiline = false;
        dPwd.placeholder = L"password：显示 • 掩码…"; dPwd.password = true;
        m_password = &CreateTextBox(dPwd);
        TextBoxDesc dPlain; dPlain.id = 3007; dPlain.multiline = false;
        dPlain.placeholder = L"同文本明文（3006 的孪生，对照用）";
        m_plainTwin = &CreateTextBox(dPlain);

        StartTimer(1, 200);
    }

    // 顺序级联：标签 → 控件 → 间距，逐行往下排（④ 给足高度看滚动，
    // ⑤⑥⑦ 功能验证框固定行高，铺满由 ④ 承担而非最后那个框）
    void OnLayout(const Rect& content) override
    {
        m_content = content;
        const int x = content.left;
        const int w = (int)(content.right - content.left);
        const int lineH = Fonts().BodyLineHeight();
        TextBox* boxes[7] = { m_single, m_singleLong, m_multi, m_multiLong,
                              m_maxLen, m_password, m_plainTwin };
        int y = content.top;
        for (int i = 0; i < 7; ++i) {
            m_rows[i].labelY = y;
            y += kLabelH + 4;
            switch (i) {
            case 2:  m_rows[i].h = lineH * 4 + 12; break;   // ③ 多行短
            case 3:  m_rows[i].h = lineH * 6 + 12; break;   // ④ 多行长
            default: m_rows[i].h = lineH; break;            // 单行
            }
            if (m_rows[i].h < lineH) m_rows[i].h = lineH;
            m_rows[i].top = y;
            y += m_rows[i].h + kGap;
            boxes[i]->Move(x, m_rows[i].top, w, m_rows[i].h);
        }
    }

    void OnPaint(Canvas& g, const Rect&) override
    {
        const Palette& p = Pal();
        const std::wstring labels[7] = {
            L"① 单行（聚焦后光标应闪烁；键入后光标紧跟文字）",
            L"② 单行长内容（横向滚动：光标跟进出界自动偏移）",
            L"③ 多行（短，可换行/键入）",
            L"④ 多行长内容（60 行；键入深处 caret 行自动滚入视口）",
            L"⑤ maxLength=5（键入/粘贴超 5 字符被截断；WM_GETTEXT 读回真实值）",
            L"⑥ password（绘制 • 掩码；WM_GETTEXT 读回真实明文）",
            L"⑦ 与 ⑥ 同文本的明文框（对照：⑥ 应渲染成掩码而非此处的明文）",
        };
        for (int i = 0; i < 7; ++i)
            Label(g, labels[i], m_content.top + m_rows[i].labelY);
        Label(g, m_probe.label, m_content.bottom - 32);   // 底部探针行
    }

    bool OnTimer(uintptr_t id) override
    {
        if (id != 1) return false;
        m_probe.Tick();
        InvalidateRegion(Rect{ m_content.left, m_content.bottom - 40,
                               m_content.right, m_content.bottom });
        return false;
    }

    void OnShow(bool shown) override
    {
        if (m_single)     m_single->Show(shown);
        if (m_singleLong) m_singleLong->Show(shown);
        if (m_multi)      m_multi->Show(shown);
        if (m_multiLong)  m_multiLong->Show(shown);
        if (m_maxLen)     m_maxLen->Show(shown);
        if (m_password)   m_password->Show(shown);
        if (m_plainTwin)  m_plainTwin->Show(shown);
    }

private:
    static constexpr int kLabelH = 16;   // 行标签高
    static constexpr int kGap    = 10;   // 控件→下一组标签的间距

    struct Row { int labelY = 0; int top = 0; int h = 0; };

    void Label(Canvas& g, const std::wstring& s, int top) const
    {
        const Palette& p = Pal();
        g.Text(Rect{ m_content.left, top, m_content.right, top + kLabelH },
               s, FontRole::Meta, p.textSub,
               dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix);
    }

    TextBox* m_single     = nullptr;
    TextBox* m_singleLong = nullptr;
    TextBox* m_multi      = nullptr;
    TextBox* m_multiLong  = nullptr;
    TextBox* m_maxLen     = nullptr;   // ⑤ maxLength=5
    TextBox* m_password   = nullptr;   // ⑥ password 掩码
    TextBox* m_plainTwin  = nullptr;   // ⑦ ⑥ 的明文孪生（像素对照）
    BlinkProbe m_probe;
    Rect m_content = {};
    Row  m_rows[7];
};

// ============================================================
// ListLabPage — 第 3 项列表多选实验页（跨进程驱动：listtest.c + list_lab_suite.ps1）
// ------------------------------------------------------------
// 左侧列表(3201, SetMultiSelect(true))+ 映射 native EDIT(3203, 跨进程可读
// GetWindowText)；右侧列表(3202, 单选)。Row 文字 = 行下标,选中行画标记。
// ============================================================
class ListLabPage : public Page
{
public:
    const wchar_t* Title() const override { return L"ListBox 多选实验"; }
    const wchar_t* NavGlyph() const override { return L"\xE8FD"; }
    const wchar_t* NavLabel() const override { return L"ListBox 多选"; }

    void OnAttach(Shell& shell) override
    {
        (void)shell;
        m_list = &CreateListBox({ IDC_LBL_MULTI, 48 });
        m_list->SetMultiSelect(true);
        for (int i = 0; i < 12; ++i) m_list->AddItem(48, (size_t)(0xB000 + i));
        m_list->onPaintItem = [this](Canvas& g, const ListItemDraw& it) { DrawRow(g, it); };
        m_list->onSelectChanged = [this](int sel) { (void)sel; UpdateMirror(); };

        m_single = &CreateListBox({ IDC_LBL_SINGLE, 48 });
        for (int i = 0; i < 12; ++i) m_single->AddItem(48, (size_t)(0xC000 + i));
        m_single->onPaintItem = [this](Canvas& g, const ListItemDraw& it) { DrawRow(g, it); };
        m_single->onSelectChanged = [this](int sel) { (void)sel; UpdateMirror(); };

        m_mirror = CreateWindowExW(0, L"EDIT", L"multi=[] single=-1",
                                   WS_CHILD | ES_READONLY | ES_AUTOHSCROLL,
                                   0, 0, 0, 0, reinterpret_cast<HWND>(ParentWnd()),
                                   (HMENU)(INT_PTR)IDC_LBL_MIRROR,
                                   GetModuleHandleW(nullptr), nullptr);
        UpdateMirror();
    }

    void OnLayout(const Rect& content) override
    {
        const int gap = 14;
        const int w = (int)(content.right - content.left);
        const int leftW = w * 3 / 5;
        m_rcMulti  = { content.left, content.top + 24,
                       content.left + leftW, content.bottom };
        m_rcMirror = { content.left, content.bottom - 26,
                       content.left + leftW, content.bottom - 6 };
        m_rcSingle = { content.left + leftW + gap, content.top + 24,
                       content.right, content.bottom };
        if (m_list)   m_list->Move(m_rcMulti.left, m_rcMulti.top,
                                   m_rcMulti.right - m_rcMulti.left,
                                   m_rcMulti.bottom - m_rcMulti.top);
        if (m_mirror)
            SetWindowPos(m_mirror, nullptr, m_rcMirror.left, m_rcMirror.top,
                         m_rcMirror.right - m_rcMirror.left,
                         m_rcMirror.bottom - m_rcMirror.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        if (m_single) m_single->Move(m_rcSingle.left, m_rcSingle.top,
                                     m_rcSingle.right - m_rcSingle.left,
                                     m_rcSingle.bottom - m_rcSingle.top);
        UpdateMirror();
    }

private:
    void DrawRow(Canvas& g, const ListItemDraw& it)
    {
        const Palette& p = Pal();
        const int h = it.rc.bottom - it.rc.top;
        if (it.selected)
            g.FillRound(it.rc, p.primarySoft, 0);
        else if (it.hovered)
            g.FillRound(it.rc, p.hover, 0);
        g.Text(Rect{ it.rc.left + 10, it.rc.top + 2, it.rc.left + 40, it.rc.bottom },
               std::to_wstring(it.index), FontRole::Body,
               p.text, dt::SingleLine | dt::VCenter | dt::NoPrefix);
        if (it.selected) {
            const int gy = it.rc.top + h / 2 - 8;
            g.Text(Rect{ it.rc.left + 10, gy, it.rc.left + 40, gy + 16 },
                   L"\xE91B", FontRole::Icon, p.primary,
                   dt::SingleLine | dt::VCenter | dt::NoPrefix);
        }
    }

    // 映射文本（原生 EDIT）：multi=[3,5,] 形式；single=<主选下标或 -1>。
    void UpdateMirror()
    {
        if (!m_mirror) return;
        std::wstring s = L"multi=[";
        for (int i : m_list ? m_list->SelectedItems() : std::vector<int>())
            s += std::to_wstring(i) + L",";
        s += L"] single=" +
             std::to_wstring(m_single ? m_single->Selection() : -1);
        SendMessageW(m_mirror, WM_SETTEXT, 0, (LPARAM)s.c_str());
    }

    ListBox* m_list   = nullptr;   // 3201
    ListBox* m_single = nullptr;   // 3202
    HWND     m_mirror = nullptr;   // 3203
    Rect     m_rcMulti  = {};
    Rect     m_rcMirror = {};
    Rect     m_rcSingle = {};
};

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int)
{
    Shell shell;
    ShellOptions opt;
    opt.className  = L"AirUiTextBoxLab";
    opt.appName    = L"TextBox Lab";
    opt.logoLetter = L"T";
    opt.width      = 760;
    opt.height     = 760;

    if (!shell.Create(hInst, opt)) return 1;

    shell.AddPage(1, std::make_unique<LabPage>());
    shell.AddPage(2, std::make_unique<ListLabPage>());
    // --listbox 从 ListBox 页开始（3b 列表多选跨进程套件使用；默认行为不变）
    const bool listBoxPage = lpCmdLine && wcsstr(lpCmdLine, L"--listbox");
    shell.SwitchPage(listBoxPage ? 2 : 1);
    shell.SetCaptionBadge(L"自绘测试", true);
    shell.SetStatus(L"聚焦任一输入框：光标应闪烁（周期=系统 GetCaretBlinkTime）", true);
    shell.SetFooterText(L"AirUI TextBoxLab — 自绘 TextBox 排查壳");
    return shell.RunMessageLoop();
}
