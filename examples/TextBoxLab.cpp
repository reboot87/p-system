// ============================================================
// TextBoxLab.cpp — 自绘 TextBox 独立测试壳
//
// 排查「光标不闪 / 位置不对」的最小复现环境：一个窗口放四个
// 自绘 TextBox（单行 / 单行长内容横向滚动 / 多行中文 / 多行
// 长内容纵向滚动），底部一行闪烁相位探针（按系统
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

        StartTimer(1, 200);
    }

    // 顺序级联：标签 → 控件 → 间距，逐行往下排（④ 铺满剩余高度）
    void OnLayout(const RECT& content) override
    {
        m_content = content;
        const int x = content.left;
        const int w = (int)(content.right - content.left);
        const int lineH = Fonts().BodyLineHeight();
        TextBox* boxes[4] = { m_single, m_singleLong, m_multi, m_multiLong };
        int y = content.top;
        for (int i = 0; i < 4; ++i) {
            m_rows[i].labelY = y;
            y += kLabelH + 4;
            m_rows[i].h = (i < 3) ? (i == 2 ? lineH * 4 + 12 : lineH)
                                  : (content.bottom - y - 40);
            if (m_rows[i].h < lineH) m_rows[i].h = lineH;
            m_rows[i].top = y;
            y += m_rows[i].h + kGap;
            boxes[i]->Move(x, m_rows[i].top, w, m_rows[i].h);
        }
    }

    void OnPaint(Canvas& g, const RECT&) override
    {
        const Palette& p = Pal();
        const std::wstring labels[4] = {
            L"① 单行（聚焦后光标应闪烁；键入后光标紧跟文字）",
            L"② 单行长内容（横向滚动：光标跟进出界自动偏移）",
            L"③ 多行（短，可换行/键入）",
            L"④ 多行长内容（60 行；键入深处 caret 行自动滚入视口）",
        };
        for (int i = 0; i < 4; ++i)
            Label(g, labels[i], m_content.top + m_rows[i].labelY);
        Label(g, m_probe.label, m_content.bottom - 32);   // 底部探针行
    }

    bool OnTimer(UINT_PTR id) override
    {
        if (id != 1) return false;
        m_probe.Tick();
        InvalidateRegion(RECT{ m_content.left, m_content.bottom - 40,
                               m_content.right, m_content.bottom });
        return false;
    }

    void OnShow(bool shown) override
    {
        if (m_single)     m_single->Show(shown);
        if (m_singleLong) m_singleLong->Show(shown);
        if (m_multi)      m_multi->Show(shown);
        if (m_multiLong)  m_multiLong->Show(shown);
    }

private:
    static constexpr int kLabelH = 16;   // 行标签高
    static constexpr int kGap    = 10;   // 控件→下一组标签的间距

    struct Row { int labelY = 0; int top = 0; int h = 0; };

    void Label(Canvas& g, const std::wstring& s, int top) const
    {
        const Palette& p = Pal();
        g.Text(RECT{ m_content.left, top, m_content.right, top + kLabelH },
               s, FontRole::Meta, p.textSub,
               DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    }

    TextBox* m_single     = nullptr;
    TextBox* m_singleLong = nullptr;
    TextBox* m_multi      = nullptr;
    TextBox* m_multiLong  = nullptr;
    BlinkProbe m_probe;
    RECT m_content = {};
    Row  m_rows[4];
};

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    Shell shell;
    ShellOptions opt;
    opt.className  = L"AirUiTextBoxLab";
    opt.appName    = L"TextBox Lab";
    opt.logoLetter = L"T";
    opt.width      = 760;
    opt.height     = 640;

    if (!shell.Create(hInst, opt)) return 1;

    shell.AddPage(1, std::make_unique<LabPage>());
    shell.SetCaptionBadge(L"自绘测试", true);
    shell.SetStatus(L"聚焦任一输入框：光标应闪烁（周期=系统 GetCaretBlinkTime）", true);
    shell.SetFooterText(L"AirUI TextBoxLab — 自绘 TextBox 排查壳");
    shell.SwitchPage(1);
    return shell.RunMessageLoop();
}
