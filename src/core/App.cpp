#include "App.h"
#include "Plugin.h"
#include "../ipc/Ipc.h"
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <dwmapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")

static const wchar_t* kWndClass = L"AireRecordWnd";
static const wchar_t* kWndTitle = L"AireRecord — 工作台";
static const int      kTitleH   = 44;   // 自绘标题栏高度
static const int      kCapBtnW  = 46;   // 标题栏按钮宽（最小化/最大化/关闭）
static const int      kSideW    = 208;  // 左侧导航栏宽
static const int      kStatusH  = 28;   // 底部状态栏高
static const int      kDetailW  = 300;  // 剪贴板页右侧详情面板宽

// ============================================================
// 设计 Token — 企业级 B 端规范（对标 Ant Design 5 / 华为云控制台）
// 依据 enterprise-ui-optimizer design-tokens.md：
//   bg-layout #F0F2F5 / 卡片 8px 圆角 1px #F0F0F0 边框 /
//   主色 #1677FF（覆盖率 <5%）/ 状态标签浅底+深字+同色点
// ============================================================
namespace theme
{
    // 中性灰阶
    constexpr COLORREF Bg        = RGB(240, 242, 245); // bg-layout 页面底
    constexpr COLORREF Surface   = RGB(255, 255, 255); // bg-container 卡片
    constexpr COLORREF Split     = RGB(240, 240, 240); // border-split
    constexpr COLORREF BorderIn  = RGB(217, 217, 217); // border-base 输入框
    constexpr COLORREF HoverFill = RGB(250, 250, 250); // 表格行 hover
    constexpr COLORREF Text      = RGB(31, 31, 31);    // text-primary 88%
    constexpr COLORREF TextSub   = RGB(89, 89, 89);    // text-secondary 65%
    constexpr COLORREF TextFaint = RGB(140, 140, 140); // text-tertiary 45%
    // 品牌主色
    constexpr COLORREF Primary      = RGB(22, 119, 255);  // #1677FF
    constexpr COLORREF PrimaryHover = RGB(64, 150, 255);  // #4096FF
    constexpr COLORREF PrimaryDown  = RGB(9, 88, 217);    // #0958D9
    constexpr COLORREF SelFill      = RGB(230, 244, 255); // 选中行 #E6F4FF
    // 状态标签（StatusTag 规范）
    constexpr COLORREF TagInfoBg = RGB(230, 244, 255);    // 信息/剪贴
    constexpr COLORREF TagInfoTx = RGB(9, 88, 217);
    constexpr COLORREF TagOkBg   = RGB(246, 255, 237);    // 成功/手动
    constexpr COLORREF TagOkTx   = RGB(56, 158, 13);
    constexpr COLORREF OkDot     = RGB(82, 196, 26);      // #52C41A
    constexpr COLORREF ErrTagBg  = RGB(255, 241, 240);    // 错误/未连接
    constexpr COLORREF ErrTagTx  = RGB(207, 19, 34);
    constexpr COLORREF ErrDot    = RGB(255, 77, 79);
}

// ---------- Owner-draw 工具（先于使用处定义） ----------
static void FillRoundRect(HDC dc, RECT rc, COLORREF color, int radius)
{
    HBRUSH br = CreateSolidBrush(color);
    HBRUSH old = (HBRUSH)SelectObject(dc, br);
    HPEN pen = CreatePen(PS_NULL, 0, 0);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, old);
    DeleteObject(pen);
    DeleteObject(br);
}

static void DrawRoundRectOutline(HDC dc, RECT rc, COLORREF color, int radius)
{
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, rc.left, rc.top, rc.right + 1, rc.bottom + 1, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBr);
    DeleteObject(pen);
}

// 状态标签（StatusTag：浅底 + 同色点 + 深字，圆角 2px，高 22）
static void DrawStatusTag(HDC dc, int rightX, int centerY, const wchar_t* text,
                          COLORREF bg, COLORREF tx, COLORREF dot, HFONT font)
{
    HFONT old = (HFONT)SelectObject(dc, font);
    SIZE sz;
    GetTextExtentPoint32W(dc, text, (int)wcslen(text), &sz);
    int h = 22, w = sz.cx + 18 + 8;
    RECT rc = { rightX - w, centerY - h / 2, rightX, centerY + h / 2 };
    FillRoundRect(dc, rc, bg, 2);
    HBRUSH br = CreateSolidBrush(dot);
    HPEN oldPen = (HPEN)SelectObject(dc, GetStockObject(NULL_PEN));
    HBRUSH oldBr = (HBRUSH)SelectObject(dc, br);
    int dy = centerY - 3;
    Ellipse(dc, rc.left + 8, dy, rc.left + 14, dy + 6);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBr);
    DeleteObject(br);
    SetTextColor(dc, tx);
    TextOutW(dc, rc.left + 18, centerY - sz.cy / 2, text, (int)wcslen(text));
    SelectObject(dc, old);
}

// 列表内来源标签（高 20，圆角 2px），返回右边界 x
static int DrawTag(HDC dc, int x, int y, const wchar_t* text,
                   COLORREF bg, COLORREF tx, HFONT font)
{
    HFONT old = (HFONT)SelectObject(dc, font);
    SIZE sz;
    GetTextExtentPoint32W(dc, text, (int)wcslen(text), &sz);
    RECT rc = { x, y, x + sz.cx + 16, y + 20 };
    FillRoundRect(dc, rc, bg, 2);
    SetTextColor(dc, tx);
    TextOutW(dc, x + 8, y + (20 - sz.cy) / 2, text, (int)wcslen(text));
    SelectObject(dc, old);
    return rc.right;
}

int App::Run(HINSTANCE hInstance)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    if (!CreateMainWindow(hInstance))
        return 1;

    if (!m_plugins.AttachAll(*this)) {
        m_plugins.DetachAll(*this);
        DestroyWindow(m_hwnd);
        return 2;
    }
    RefreshList();
    RefreshStats();

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    m_plugins.DetachAll(*this);
    return (int)msg.wParam;
}

bool App::CreateMainWindow(HINSTANCE hInstance)
{
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &App::WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = wc.hIcon;
    wc.hbrBackground = CreateSolidBrush(theme::Bg);
    wc.lpszClassName = kWndClass;
    if (!RegisterClassExW(&wc)) return false;

    m_hwnd = CreateWindowExW(0, kWndClass, kWndTitle,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1120, 780,
                             nullptr, nullptr, hInstance, this);
    if (!m_hwnd) return false;

    // 无边框壳：DWM 阴影 + Win11 圆角（Win10 上调用失败无副作用）
    MARGINS m = { 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(m_hwnd, &m);
    INT pref = 2;  // DWMWCP_ROUND
    DwmSetWindowAttribute(m_hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/,
                          &pref, sizeof(pref));
    // 强制重算 NC 区：确保 WM_NCCALCSIZE(返回 0) 在首次显示前生效
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
    return true;
}

void App::CreateUI(HWND hwnd)
{
    // 搜索框（位于列表卡片标题行右侧，白底细边框）
    m_hSearch = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, hwnd, (HMENU)IDC_SEARCH_EDIT, nullptr, nullptr);
    SetWindowSubclass(m_hSearch, &App::EditPlaceholderProc, 3, (DWORD_PTR)this);

    // 搜索框右侧 Ctrl+Shift+V 快捷键提示：做成独立置顶子窗口，
    // 始终绘制在搜索框之上，彻底避免被编辑框 hover/焦点重绘覆盖。
    m_hSearchChip = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd,
        (HMENU)IDC_SEARCH_CHIP, nullptr, nullptr);
    SetWindowSubclass(m_hSearchChip, &App::SearchChipProc, 5, (DWORD_PTR)this);
    // 置于搜索框之上（z-order），并使其对鼠标透明，点击/悬停穿透到编辑框
    SetWindowPos(m_hSearchChip, m_hSearch, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE);

    // 记录列表（表格化：行高 54，hover/选中在 owner-draw 中绘制）
    m_hList = CreateWindowExW(0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP |
        LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWVARIABLE,
        0, 0, 0, 0, hwnd, (HMENU)IDC_LIST, nullptr, nullptr);
    SendMessageW(m_hList, LB_SETITEMHEIGHT, 0, 54);
    SetWindowSubclass(m_hList, &App::ListProc, 1, (DWORD_PTR)this);

    // 添加记录表单
    m_hNoteEdit = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, hwnd, (HMENU)IDC_NOTE_EDIT, nullptr, nullptr);
    SetWindowSubclass(m_hNoteEdit, &App::EditPlaceholderProc, 4, (DWORD_PTR)this);

    m_hManualEdit = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE |
        ES_AUTOVSCROLL | ES_WANTRETURN,
        0, 0, 0, 0, hwnd, (HMENU)IDC_MANUAL_EDIT, nullptr, nullptr);
    // 多行 EDIT 不支持 cue banner，用子类化自绘占位提示
    SetWindowSubclass(m_hManualEdit, &App::EditPlaceholderProc, 2, (DWORD_PTR)this);

    // 主按钮（Primary：主色填充）
    m_hSaveBtn = CreateWindowExW(0, L"BUTTON", L"保存记录",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 0, 0, hwnd, (HMENU)IDC_SAVE_BUTTON, nullptr, nullptr);
    SetWindowSubclass(m_hSaveBtn, &App::SaveBtnProc, 1, (DWORD_PTR)this);

    m_font        = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Microsoft YaHei UI");
    m_fontMeta    = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Microsoft YaHei UI");
    m_fontTitle   = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Microsoft YaHei UI");
    m_fontSection = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Microsoft YaHei UI");
    m_fontBtn     = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Microsoft YaHei UI");
    m_fontIcon    = CreateFontW(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Segoe MDL2 Assets");
    m_fontIconBig = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Segoe MDL2 Assets");
    m_fontBig     = CreateFontW(-22, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Microsoft YaHei UI");
    // 单行输入框内文字行高：Windows 单行 EDIT 永远把文字画在客户区顶部，
    // 因此把 EDIT 客户区压到恰好一行文字高，再垂直居中放进自绘圆角框，
    // 才能实现真正的上下居中（视觉样式/清单无法改变该行为）。
    {
        HDC dc = GetDC(hwnd);
        HFONT old = (HFONT)SelectObject(dc, m_font);
        TEXTMETRICW tm{};
        GetTextMetricsW(dc, &tm);
        SelectObject(dc, old);
        ReleaseDC(hwnd, dc);
        m_lineH = tm.tmHeight + tm.tmExternalLeading;
    }
    UpdateFont(m_hSearch);
    UpdateFont(m_hList);
    UpdateFont(m_hManualEdit);
    UpdateFont(m_hNoteEdit);
    UpdateFont(m_hSaveBtn);

    // 统一 8px 文字内边距（企业级输入规范），并修复 cue 文字贴边
    auto SetMargins = [](HWND edit) {
        SendMessageW(edit, EM_SETMARGINS,
                     EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
    };
    SetMargins(m_hSearch);
    SetMargins(m_hNoteEdit);
    SetMargins(m_hManualEdit);

    m_brBg    = CreateSolidBrush(theme::Bg);
    m_brInput = CreateSolidBrush(theme::Surface);
    m_brWhite = CreateSolidBrush(theme::Surface);

    // 工作台默认页：总览（剪贴板页控件随 SwitchPage 显隐）
    SwitchPage(PageOverview);
}

void App::UpdateFont(HWND ctrl)
{
    SendMessageW(ctrl, WM_SETFONT, (WPARAM)m_font, TRUE);
}

void App::ApplyRoundedRegion(HWND ctrl, int radius)
{
    RECT rc;
    GetWindowRect(ctrl, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, radius, radius);
    SetWindowRgn(ctrl, rgn, TRUE);
}

// 详情面板"备注"框几何（Layout 控件定位与 DrawDetailPanel 绘制共用）
static RECT DetailNoteBox(RECT d)
{
    int y = d.top + 14;      // "详情" 标题
    y += 20 + 10;            // → 类型胶囊 top (44)
    y += 22 + 10;            // → 内容框 top (76)
    y += 96 + 14;            // → "属性" 标题 top (186)
    y += 18 + 4;             // → 属性行 top (208)
    y += 4 * 22 + 12;        // → "备注" 标题 top (308)
    y += 18 + 8;             // → 备注框 top (334)
    return { d.left + 16, y, d.right - 16, y + 64 };
}

// ---------- 布局（8px 栅格：页面边距 16，卡片间距 12~16） ----------
void App::Layout()
{
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    const int M = 16;

    m_rcSide = { 0, kTitleH, kSideW, H - kStatusH };

    // 内容区（状态栏之上）
    RECT content = { kSideW + M, kTitleH + 14, W - M, H - kStatusH - 12 };

    // ---- 工具条：搜索框 + 类型胶囊 + ＋手动录入 ----
    int searchW = 320;
    if (searchW > content.right - content.left - 420)
        searchW = content.right - content.left - 420;
    if (searchW < 160) searchW = 160;

    // 搜索框 = 父窗口绘制的圆角输入框（白底 + 边框），EDIT 只占其中一行文字高并垂直居中。
    // Windows 单行 EDIT 永远把文字画在客户区顶部（视觉样式亦不改变），
    // 压到恰好一行高后，文字才真正上下居中。
    const int kSearchBoxH = 36;
    m_rcSearchBox = { content.left, content.top,
                      content.left + searchW, content.top + kSearchBoxH };
    MoveWindow(m_hSearch,
               m_rcSearchBox.left + 1,
               m_rcSearchBox.top + (kSearchBoxH - m_lineH) / 2,
               searchW - 2, m_lineH, TRUE);

    // 搜索框右侧 Ctrl+Shift+V 快捷键提示：与输入框同高、右对齐，留出 6px 边距
    {
        HDC dc = GetDC(m_hwnd);
        HFONT old = (HFONT)SelectObject(dc, m_fontMeta);
        const wchar_t* key = L"Ctrl+Shift+V";
        SIZE sz;
        GetTextExtentPoint32W(dc, key, 12, &sz);
        SelectObject(dc, old);
        ReleaseDC(m_hwnd, dc);
        int chipW = sz.cx + 12;
        int chipH = 18;
        int cx = m_rcSearchBox.right - chipW - 6;
        int cy = m_rcSearchBox.top + (kSearchBoxH - chipH) / 2;
        if (m_hSearchChip)
            MoveWindow(m_hSearchChip, cx, cy, chipW, chipH, TRUE);
        // 编辑框右内边距让出 chip 区域，避免输入文字/占位字压到标签下
        SendMessageW(m_hSearch, EM_SETMARGINS, EC_RIGHTMARGIN,
                     MAKELPARAM(0, chipW + 10));
    }

    m_rcAddBtn = { content.right - 112, content.top + 1,
                   content.right, content.top + 35 };

    // 筛选胶囊几何（按文字实测宽度）
    {
        HDC dc = GetDC(m_hwnd);
        HFONT old = (HFONT)SelectObject(dc, m_fontMeta);
        const wchar_t* labels[4] = { L"全部", L"文本", L"链接", L"手动" };
        int cx = content.left + searchW + 14;
        int cy = content.top + 4;              // 胶囊高 28，垂直居中于 36 工具条
        for (int i = 0; i < 4; ++i) {
            SIZE sz;
            GetTextExtentPoint32W(dc, labels[i], 2, &sz);
            m_rcChips[i] = { cx, cy, cx + sz.cx + 24, cy + 28 };
            cx = m_rcChips[i].right + 8;
        }
        SelectObject(dc, old);
        ReleaseDC(m_hwnd, dc);
    }

    // ---- 详情面板（右）与列表区（左） ----
    m_rcDetail = { content.right - kDetailW, content.top + 48,
                   content.right, content.bottom };
    m_rcListCard = { content.left, content.top + 48,
                     m_rcDetail.left - 14, content.bottom };

    // ---- 表单卡（展开时占列表列底部） ----
    const int FormH = 194;
    if (m_formOpen || m_editingId) {
        m_rcFormCard = { m_rcListCard.left, m_rcListCard.bottom - FormH,
                         m_rcListCard.right, m_rcListCard.bottom };
    } else {
        m_rcFormCard = m_rcListCard;   // 不展开时不参与命中
    }

    RECT rcList = m_rcListCard;
    if (m_formOpen) rcList.bottom = m_rcFormCard.top - 10;
    MoveWindow(m_hList, rcList.left, rcList.top,
               rcList.right - rcList.left, rcList.bottom - rcList.top, TRUE);

    // ---- 表单卡内部控件 ----
    if (m_formOpen && !m_editingId) {
        int fx = m_rcFormCard.left + 16;
        int fw = (m_rcFormCard.right - m_rcFormCard.left) - 32;
        int contentY = m_rcFormCard.top + 16 + 22 + 8;
        MoveWindow(m_hManualEdit, fx, contentY, fw, 88, TRUE);
        int rowY = contentY + 88 + 12;
        int btnW = 96;
        // 备注框：视觉框 32px 高，内部 EDIT 压到一行文字高并垂直居中（文字真正居中）
        m_rcNoteBox = { fx, rowY, fx + fw - btnW - 12, rowY + 32 };
        MoveWindow(m_hNoteEdit,
                   m_rcNoteBox.left + 1,
                   m_rcNoteBox.top + (32 - m_lineH) / 2,
                   m_rcNoteBox.right - m_rcNoteBox.left - 2, m_lineH, TRUE);
        MoveWindow(m_hSaveBtn, fx + fw - btnW, rowY, btnW, 32, TRUE);
        m_rcFormClose = { m_rcFormCard.right - 62, m_rcFormCard.top + 14,
                          m_rcFormCard.right - 16, m_rcFormCard.top + 36 };
    }
    // ---- 备注编辑态：备注框/按钮覆盖到详情面板"备注"区 ----
    else if (m_editingId && m_rcDetail.right > m_rcDetail.left) {
        RECT nb = DetailNoteBox(m_rcDetail);
        // 备注编辑态同样是单行 EDIT：客户区压到一行文字高并垂直居中，文字真正居中
        MoveWindow(m_hNoteEdit,
                   nb.left, nb.top + (nb.bottom - nb.top - m_lineH) / 2,
                   nb.right - nb.left, m_lineH, TRUE);
        MoveWindow(m_hSaveBtn, m_rcDetail.left + 16,
                   nb.bottom + 10, 96, 30, TRUE);
        m_rcFormClose = {};
    }
    else {
        // 无表单 / 无备注编辑态：备注框不可见，清空视觉框避免误命中
        m_rcNoteBox = {};
    }
}

std::wstring App::GetWindowTextStr(HWND ctrl)
{
    int len = GetWindowTextLengthW(ctrl);
    if (len <= 0) return L"";
    std::wstring s(len + 1, L'\0');
    GetWindowTextW(ctrl, &s[0], len + 1);
    s.resize(len);
    return s;
}

std::wstring App::GetSearchText()
{
    return m_hSearch ? GetWindowTextStr(m_hSearch) : std::wstring();
}

void App::SetStatusText(const std::wstring& text, bool ok)
{
    m_statusText = text;
    m_serviceOk  = ok;
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    rc.bottom = kTitleH;
    InvalidateRect(m_hwnd, &rc, TRUE);
}

// ============================================================
// 工作台：页面切换 / 导航 / 统计
// ============================================================

// ---- 侧栏导航定义（图标：Segoe MDL2 Assets） ----
struct NavDef { int id; const wchar_t* glyph; const wchar_t* label;
                bool disabled; };
static const NavDef kNav[] = {
    { 0,   L"\xE80F", L"工作台总览",   false },   // E80F Home
    { 1,   L"\xE8C8", L"剪贴板",      false },   // E8C8 Copy
    { -1,  L"\xE8B7", L"文件监控",    true  },   // E8B7 Folder（规划中）
    { -1,  L"\xE749", L"打印监控",    true  },   // E749 Print（规划中）
    { 100, L"\xE70F", L"手动录入",    false },   // E70F Edit
    { 101, L"\xE74D", L"清空剪贴板记录", false },// E74D Delete
    { 2,   L"\xE713", L"机器配置",    false },   // E713 Settings
    { -2,  L"\xE72C", L"服务管理",    false },   // E72C Refresh（→ 配置页）
};
static const int kNavCount = (int)(sizeof(kNav) / sizeof(kNav[0]));
static const int kNavGroupCount[3] = { 4, 2, 2 };  // 功能模块/快捷操作/系统
static const wchar_t* kNavGroupName[3] =
    { L"功能模块", L"快捷操作", L"系统" };

// 导航几何：标签行 24 + 组间距 6 + 条目行 32
static void NavLayout(int* groupY, int* itemY)
{
    int y = kTitleH + 10;
    int idx = 0;
    for (int g = 0; g < 3; ++g) {
        if (groupY) groupY[g] = y;
        y += 24;
        for (int i = 0; i < kNavGroupCount[g]; ++i, ++idx) {
            if (itemY) itemY[idx] = y;
            y += 32;
        }
        y += 6;
    }
}

void App::SwitchPage(WorkPage page)
{
    m_page = page;
    bool clips = (page == PageClips);

    // 挂起重绘 → 子控件显隐一次完成 → 恢复重绘并整体重画（无双帧闪烁）
    SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);
    // 搜索框/列表：仅剪贴板页显示
    ShowWindow(m_hSearch, clips ? SW_SHOW : SW_HIDE);
    ShowWindow(m_hSearchChip, clips ? SW_SHOW : SW_HIDE);
    ShowWindow(m_hList,  clips ? SW_SHOW : SW_HIDE);
    // 录入/备注/按钮：表单卡展开或备注编辑态时显示（位置由 Layout 决定）
    bool editCtrls = clips && (m_formOpen || m_editingId);
    HWND ctrls[] = { m_hManualEdit, m_hNoteEdit, m_hSaveBtn };
    for (HWND h : ctrls)
        if (h) ShowWindow(h, editCtrls ? SW_SHOW : SW_HIDE);
    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);

    if (page == PageConfig) {
        m_sys = sysinfo::Collect();     // 本地采集（毫秒级）
        m_sysLoaded = true;
        SetTimer(m_hwnd, TIMER_SYSINFO, 2000, nullptr);
    } else {
        KillTimer(m_hwnd, TIMER_SYSINFO);
    }
    Layout();
    InvalidateRect(m_hwnd, nullptr, FALSE);   // FALSE：不擦背景，避免闪烁
}

// ---------- 手动录入表单卡：展开 / 收起 ----------
void App::ToggleForm(bool open)
{
    if (m_formOpen == open) return;
    if (open && m_editingId) return;   // 备注编辑态优先，避免控件位置冲突
    m_formOpen = open;
    Layout();
    SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);
    bool showCtrls = (m_page == PageClips) && (m_formOpen || m_editingId);
    HWND ctrls[] = { m_hManualEdit, m_hNoteEdit, m_hSaveBtn };
    for (HWND h : ctrls)
        if (h) ShowWindow(h, showCtrls ? SW_SHOW : SW_HIDE);
    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwnd, nullptr, FALSE);
    if (open && m_hManualEdit) {
        // 不自动聚焦录入框：让占位提示“输入要保存的内容…”在展开时自然可见，
        // 用户点击进入后才隐藏（聚焦态一律不画占位字），避免输入时透出。
        RECT rc = m_rcFormCard;
        InvalidateRect(m_hwnd, &rc, FALSE);
    }
}

// ---------- 条目类型判定（客户端本地） ----------
// 1=文本 2=链接 3=手动（文件路径按文本处理，卡片图标单独识别）
int App::TypeOfEntry(const UiEntry& e)
{
    if (e.source == L"Manual") return 3;
    const std::wstring& c = e.content;
    auto starts = [&c](const wchar_t* p) {
        size_t n = wcslen(p);
        return c.size() >= n && _wcsnicmp(c.c_str(), p, n) == 0;
    };
    if (starts(L"http://") || starts(L"https://") ||
        starts(L"www.") || starts(L"ftp://"))
        return 2;
    return 1;
}

void App::RefreshStats()
{
    std::wstring resp = m_ipc.Request(L"STATS");
    if (resp.rfind(L"OK", 0) == 0) {
        auto f = ipc::SplitTabs(resp);
        if (f.size() >= 4) {
            m_stats.total   = _wtoi64(f[1].c_str());
            m_stats.today   = _wtoi64(f[2].c_str());
            m_stats.dbBytes = _wtoi64(f[3].c_str());
        }
    }
    // SCM 服务状态（侧栏"服务管理"角标 / 标题栏状态胶囊）
    m_svcState = sysinfo::QueryServiceState(L"AireRecord", m_svcText);
}

int App::NavItemAt(POINT pt) const
{
    if (pt.x < 0 || pt.x >= kSideW || pt.y < kTitleH) return -1;
    int itemY[16] = {};
    NavLayout(nullptr, itemY);
    for (int i = 0; i < kNavCount; ++i)
        if (pt.y >= itemY[i] && pt.y < itemY[i] + 32)
            return i;
    return -1;
}

void App::OnNavClick(int index)
{
    if (index < 0 || index >= kNavCount) return;
    const NavDef& n = kNav[index];
    if (n.disabled) return;

    if (n.id == 100) {                 // 手动录入 → 展开表单卡并聚焦
        SwitchPage(PageClips);
        RefreshList(GetSearchText());
        ToggleForm(true);
        return;
    }
    if (n.id == -2) {                  // 服务管理 → 配置页（服务与进程卡）
        SwitchPage(PageConfig);
        RefreshStats();
        return;
    }
    if (n.id == 101) {                 // 清空剪贴板记录
        if (MessageBoxW(m_hwnd,
                L"确定清空全部剪贴板记录吗？\n此操作不可恢复。",
                L"清空记录", MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
            m_ipc.Request(L"CLEAR");
            RefreshList(GetSearchText());
            RefreshStats();
        }
        return;
    }
    SwitchPage((WorkPage)n.id);
    RefreshList(GetSearchText());
    RefreshStats();
}

// ---- 侧栏绘制 ----
void App::DrawSidebar(HDC dc, const RECT& rcClient)
{
    SetBkMode(dc, TRANSPARENT);

    // 侧栏底色 + 右侧分隔线
    HBRUSH brSide = CreateSolidBrush(RGB(251, 252, 253));
    FillRect(dc, &m_rcSide, brSide);
    DeleteObject(brSide);
    RECT rcLine = { m_rcSide.right - 1, m_rcSide.top,
                    m_rcSide.right, m_rcSide.bottom };
    HBRUSH brLine = CreateSolidBrush(RGB(233, 236, 241));
    FillRect(dc, &rcLine, brLine);
    DeleteObject(brLine);

    int groupY[3] = {}, itemY[16] = {};
    NavLayout(groupY, itemY);

    // 组标签
    SelectObject(dc, m_fontMeta);
    for (int g = 0; g < 3; ++g) {
        RECT rc = { 14, groupY[g], kSideW - 8, groupY[g] + 20 };
        SetTextColor(dc, RGB(143, 149, 158));
        DrawTextW(dc, kNavGroupName[g], -1, &rc,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    }

    // 导航条目
    for (int i = 0; i < kNavCount; ++i) {
        const NavDef& n = kNav[i];
        RECT rc = { 8, itemY[i], kSideW - 8, itemY[i] + 32 };

        bool active = false;
        if (n.id == (int)m_page) active = true;

        if (active)      FillRoundRect(dc, rc, theme::SelFill, 6);
        else if (i == m_navHover && !n.disabled)
                         FillRoundRect(dc, rc, RGB(242, 244, 247), 6);

        int tx = rc.left + 12;
        // 图标
        SelectObject(dc, m_fontIcon);
        SetTextColor(dc, n.disabled ? RGB(169, 174, 184)
                                    : (active ? theme::Primary
                                              : RGB(90, 98, 112)));
        RECT rcIc = { tx, rc.top, tx + 22, rc.bottom };
        DrawTextW(dc, n.glyph, -1, &rcIc,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        tx += 28;

        // 文本
        SelectObject(dc, active ? m_fontTitle : m_font);
        SetTextColor(dc, n.disabled ? RGB(169, 174, 184)
                                    : (active ? RGB(22, 104, 220)
                                              : theme::Text));
        RECT rcTx = { tx, rc.top, rc.right - 46, rc.bottom };
        DrawTextW(dc, n.label, -1, &rcTx,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

        // 右侧角标
        std::wstring badge;
        if (n.disabled)         badge = L"规划中";
        else if (n.id == 1 && m_stats.total >= 0)
                                badge = std::to_wstring((long)m_stats.total);
        else if (n.id == 100)   badge = L"F4";
        else if (n.id == -2)    badge = (m_svcState == 2) ? L"运行中"
                                                          : L"已停止";
        if (!badge.empty()) {
            SelectObject(dc, m_fontMeta);
            SIZE sz;
            GetTextExtentPoint32W(dc, badge.c_str(),
                                  (int)badge.size(), &sz);
            int bw = sz.cx + 14;
            RECT rcB = { rc.right - 8 - bw, rc.top + 8,
                         rc.right - 8, rc.top + 24 };
            bool green = (n.id == -2 && m_svcState == 2);
            if (n.disabled) {
                FillRoundRect(dc, rcB, RGB(239, 241, 244), 8);
                SetTextColor(dc, RGB(143, 149, 158));
            } else if (green) {
                FillRoundRect(dc, rcB, theme::TagOkBg, 8);
                SetTextColor(dc, theme::TagOkTx);
            } else if (active) {
                FillRoundRect(dc, rcB, RGB(214, 230, 255), 8);
                SetTextColor(dc, RGB(22, 104, 220));
            } else {
                FillRoundRect(dc, rcB, RGB(239, 241, 244), 8);
                SetTextColor(dc, RGB(143, 149, 158));
            }
            DrawTextW(dc, badge.c_str(), -1, &rcB,
                      DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        }
    }

    // 底部"本机信息"卡
    int cardH = 104;
    int cardTop = rcClient.bottom - kStatusH - cardH - 26;
    if (cardTop > itemY[kNavCount - 1] + 40) {
        RECT rcCard = { 10, cardTop, kSideW - 10, cardTop + cardH };
        FillRoundRect(dc, rcCard, theme::Surface, 8);
        DrawRoundRectOutline(dc, rcCard, RGB(233, 236, 241), 8);

        RECT rcCap = { rcCard.left + 12, rcCard.top + 8,
                       rcCard.right - 12, rcCard.top + 26 };
        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, RGB(143, 149, 158));
        DrawTextW(dc, L"本机信息", -1, &rcCap,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER);

        const wchar_t* keys[3] = { L"运行模式", L"服务自启", L"条目" };
        std::wstring vals[3] = {
            L"服务 + 代理", L"已启用",
            m_stats.total >= 0
                ? std::to_wstring((long)m_stats.total) + L" 条"
                : L"—",
        };
        for (int r = 0; r < 3; ++r) {
            int y = rcCard.top + 30 + r * 24;
            RECT rcK = { rcCard.left + 12, y, rcCard.left + 68, y + 20 };
            SetTextColor(dc, RGB(143, 149, 158));
            SelectObject(dc, m_fontMeta);
            DrawTextW(dc, keys[r], -1, &rcK,
                      DT_SINGLELINE | DT_LEFT | DT_VCENTER);
            RECT rcV = { rcCard.left + 70, y, rcCard.right - 10, y + 20 };
            SetTextColor(dc, (r == 1) ? RGB(56, 158, 13) : theme::Text);
            SelectObject(dc, m_fontMeta);
            DrawTextW(dc, vals[r].c_str(), -1, &rcV,
                      DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        }

        // 版本行
        RECT rcVer = { 10, cardTop + cardH + 8, kSideW - 10,
                       cardTop + cardH + 26 };
        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, RGB(169, 174, 184));
        DrawTextW(dc, m_serviceOk ? L"AireRecord v1.2 · 服务已连接"
                                  : L"AireRecord v1.2 · 服务未连接",
                  -1, &rcVer, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
}

// ============================================================
// 总览页（默认页）：模块卡片墙 + 最近动态
// ============================================================

static std::wstring FmtGB(long long mb)
{
    wchar_t b[32];
    if (mb >= 1024) swprintf_s(b, 32, L"%.1f GB", mb / 1024.0);
    else            swprintf_s(b, 32, L"%d MB", (int)mb);
    return b;
}

static std::wstring FmtBytes(long long bytes)
{
    if (bytes <= 0) return L"—";
    return FmtGB(bytes / (1024 * 1024));
}

// 右上角状态小胶囊（ok=绿 灰=规划中）
static void DrawPill(HDC dc, int right, int y, const wchar_t* text,
                     COLORREF bg, COLORREF tx, HFONT font)
{
    SelectObject(dc, font);
    SIZE sz;
    GetTextExtentPoint32W(dc, text, (int)wcslen(text), &sz);
    RECT rc = { right - sz.cx - 18, y, right, y + 20 };
    FillRoundRect(dc, rc, bg, 9);
    SetTextColor(dc, tx);
    TextOutW(dc, rc.left + 9, y + 3, text, (int)wcslen(text));
}

void App::DrawOverviewPage(HDC dc, const RECT& rcClient)
{
    int x0 = kSideW + 16, x1 = rcClient.right - 16;
    int W = x1 - x0;
    int cardW = (W - 24) / 3;
    int cardH = 188;
    int y = kTitleH + 14;

    auto ModuleCard = [&](RECT r, int cardIdx, const wchar_t* glyph,
                          COLORREF icBg, COLORREF icTx,
                          const wchar_t* name, const wchar_t* desc,
                          const std::wstring& num, const wchar_t* unit,
                          const wchar_t* badge, bool off, bool badgeOk)
    {
        m_rcModCard[cardIdx] = r;
        FillRoundRect(dc, r, theme::Surface, 12);
        DrawRoundRectOutline(dc, r, theme::Split, 12);

        // 图标块 40px（左上）
        RECT ic = { r.left + 16, r.top + 16, r.left + 56, r.top + 56 };
        FillRoundRect(dc, ic, icBg, 10);
        SelectObject(dc, m_fontIconBig);
        SetTextColor(dc, icTx);
        DrawTextW(dc, glyph, -1, &ic,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER);

        // 徽章（右上）
        DrawPill(dc, r.right - 16, r.top + 16, badge,
                 badgeOk ? theme::TagOkBg : RGB(239, 241, 244),
                 badgeOk ? theme::TagOkTx : RGB(143, 149, 158),
                 m_fontMeta);

        // 名称 + 描述
        RECT rcName = { r.left + 16, r.top + 64, r.right - 16, r.top + 86 };
        SelectObject(dc, m_fontTitle);
        SetTextColor(dc, off ? theme::TextFaint : theme::Text);
        DrawTextW(dc, name, -1, &rcName,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        RECT rcDesc = { r.left + 16, r.top + 88, r.right - 16, r.top + 108 };
        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, theme::TextFaint);
        DrawTextW(dc, desc, -1, &rcDesc,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

        // 大数字 + 单位
        RECT rcNum = { r.left + 16, r.top + 114, r.right - 16, r.top + 148 };
        SelectObject(dc, m_fontBig);
        SetTextColor(dc, off ? theme::TextFaint : theme::Text);
        DrawTextW(dc, num.c_str(), -1, &rcNum,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        SIZE szNum;
        GetTextExtentPoint32W(dc, num.c_str(), (int)num.size(), &szNum);
        RECT rcUnit = { r.left + 16 + szNum.cx + 8, r.top + 114,
                        r.right - 16, r.top + 148 };
        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, theme::TextFaint);
        DrawTextW(dc, unit, -1, &rcUnit,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

        // 底部入口链接
        RECT rcLink = { r.left + 16, r.top + 154, r.right - 16, r.top + 176 };
        SelectObject(dc, m_fontBtn);
        SetTextColor(dc, off ? RGB(169, 174, 184) : RGB(22, 104, 220));
        DrawTextW(dc, off ? L"敬请期待" : L"进入模块 →", -1, &rcLink,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    };

    std::wstring clipNum =
        m_stats.total >= 0 ? std::to_wstring((long)m_stats.total) : L"—";
    std::wstring clipUnit = m_stats.today > 0
        ? L"条记录 · 今日 +" + std::to_wstring((long)m_stats.today)
        : L"条记录";

    ModuleCard({ x0, y, x0 + cardW, y + cardH }, 0,
               L"\xE8C8", theme::TagInfoBg, theme::Primary,
               L"剪贴板监控", L"自动记录剪贴板内容，支持搜索与备注",
               clipNum, clipUnit.c_str(), L"运行中", false, true);
    ModuleCard({ x0 + cardW + 12, y, x0 + cardW * 2 + 12, y + cardH }, 1,
               L"\xE8B7", RGB(230, 244, 241), RGB(15, 163, 127),
               L"文件监控", L"监控指定目录的新增 / 修改 / 删除",
               L"—", L"未启用", L"规划中", true, false);
    ModuleCard({ x0 + cardW * 2 + 24, y, x1, y + cardH }, 2,
               L"\xE749", RGB(243, 238, 255), RGB(123, 97, 255),
               L"打印监控", L"记录本机打印任务与文档信息",
               L"—", L"未启用", L"规划中", true, false);

    // ---- 最近动态（跨模块聚合，行 = 图标块 + 内容 + 模块标签 + 时间） ----
    int y2 = y + cardH + 16;
    RECT rcAct = { x0, y2, x1, rcClient.bottom - kStatusH - 12 };
    FillRoundRect(dc, rcAct, theme::Surface, 10);
    DrawRoundRectOutline(dc, rcAct, theme::Split, 10);

    RECT rcHead = { x0 + 16, y2 + 12, x1 - 16, y2 + 32 };
    SelectObject(dc, m_fontSection);
    SetTextColor(dc, theme::Text);
    DrawTextW(dc, L"最近动态 · 全部模块", -1, &rcHead,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER);

    int ry = y2 + 42;
    int rows = 0;
    for (size_t i = 0; i < m_entries.size() && rows < 7; ++i, ++rows) {
        const UiEntry& e = m_entries[i];
        int rowBottom = ry + 34;
        if (rowBottom > rcAct.bottom - 4) break;

        // 图标块 26px
        RECT ic = { x0 + 16, ry + 4, x0 + 42, ry + 30 };
        FillRoundRect(dc, ic, RGB(242, 243, 245), 6);
        SelectObject(dc, m_fontIcon);
        SetTextColor(dc, (e.source == L"Manual") ? RGB(123, 97, 255)
                                                 : theme::Primary);
        DrawTextW(dc, (e.source == L"Manual") ? L"\xE70F" : L"\xE8C8",
                  -1, &ic, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

        // 内容
        RECT rcTxt = { x0 + 52, ry, x1 - 230, ry + 34 };
        SelectObject(dc, m_font);
        SetTextColor(dc, theme::Text);
        std::wstring one = e.content;
        for (auto& c : one)
            if (c == L'\r' || c == L'\n') c = L' ';
        DrawTextW(dc, one.c_str(), (int)one.size(), &rcTxt,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

        // 模块标签
        int px = DrawTag(dc, x1 - 220, ry + 7, L"剪贴板",
                         theme::TagInfoBg, theme::TagInfoTx, m_fontMeta);

        // 时间
        RECT rcTime = { px + 12, ry, x1 - 16, ry + 34 };
        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, RGB(169, 174, 184));
        std::wstring when = FormatRelTime(e.createdAt);
        DrawTextW(dc, when.c_str(), -1, &rcTime,
                  DT_SINGLELINE | DT_RIGHT | DT_VCENTER);

        // 虚线分隔
        HPEN pen = CreatePen(PS_DASH, 1, RGB(224, 228, 233));
        HPEN oldPen = (HPEN)SelectObject(dc, pen);
        MoveToEx(dc, x0 + 16, rowBottom, nullptr);
        LineTo(dc, x1 - 16, rowBottom);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
        ry = rowBottom;
    }

    if (!rows) {
        RECT rcEmpty = { x0 + 16, ry + 8, x1 - 16, rcAct.bottom - 8 };
        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, theme::TextFaint);
        DrawTextW(dc, L"暂无记录 —— 复制任意文字将自动出现在这里",
                  -1, &rcEmpty, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
}

// ============================================================
// 机器配置页：系统 + 硬件采集（纯 Win32 API）
// ============================================================

struct CfgCardData
{
    const wchar_t* glyph;
    const wchar_t* title;
    const wchar_t* badge;
    bool           badgeOk;     // true 绿色 OK，false 蓝色信息
    std::vector<std::wstring> rows;  // "k|v|bar"，bar<0 无进度条
};

int App::CardHeight(int rowCount) { return 44 + rowCount * 26 + 12; }

void App::DrawInfoCard(HDC dc, RECT rc, const wchar_t* icon,
                       const wchar_t* title, const wchar_t* badge,
                       bool badgeOk, const std::vector<std::wstring>& rows)
{
    FillRoundRect(dc, rc, theme::Surface, 10);
    DrawRoundRectOutline(dc, rc, theme::Split, 10);

    // 头部
    RECT ic = { rc.left + 14, rc.top + 10, rc.left + 40, rc.top + 36 };
    FillRoundRect(dc, ic, theme::TagInfoBg, 6);
    SelectObject(dc, m_fontIcon);
    SetTextColor(dc, theme::Primary);
    DrawTextW(dc, icon, -1, &ic, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    RECT rcT = { ic.right + 8, rc.top + 10, rc.right - 90, rc.top + 36 };
    SelectObject(dc, m_fontTitle);
    SetTextColor(dc, theme::Text);
    DrawTextW(dc, title, -1, &rcT,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    if (badge && *badge)
        DrawPill(dc, rc.right - 14, rc.top + 13, badge,
                 badgeOk ? theme::TagOkBg : theme::TagInfoBg,
                 badgeOk ? theme::TagOkTx : theme::TagInfoTx,
                 m_fontMeta);

    // 行
    int y = rc.top + 44;
    for (auto& row : rows) {
        auto f = ipc::SplitTabs(row);   // 用 \t 分段：k\tv\tbar
        const std::wstring k = f.size() > 0 ? f[0] : L"";
        const std::wstring v = f.size() > 1 ? f[1] : L"";
        int bar = f.size() > 2 ? _wtoi(f[2].c_str()) : -1;

        RECT rcK = { rc.left + 16, y, rc.left + 112, y + 22 };
        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, theme::TextFaint);
        DrawTextW(dc, k.c_str(), -1, &rcK,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER);

        bool hasBar = (bar >= 0 && bar <= 100);
        int vRight = hasBar ? rc.right - 16 - 158 : rc.right - 16;
        RECT rcV = { rc.left + 118, y, vRight, y + 22 };
        SelectObject(dc, m_font);
        SetTextColor(dc, theme::Text);
        DrawTextW(dc, v.c_str(), -1, &rcV,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

        if (hasBar) {
            RECT rcBar = { rc.right - 16 - 160, y + 7,
                           rc.right - 16 - 46, y + 14 };
            HBRUSH brBg = CreateSolidBrush(RGB(239, 241, 244));
            FillRect(dc, &rcBar, brBg);
            DeleteObject(brBg);
            if (bar > 0) {
                int w = (int)((rcBar.right - rcBar.left) * bar / 100.0);
                COLORREF c = (bar > 70) ? RGB(229, 72, 77)
                           : (bar > 45) ? RGB(250, 173, 20)
                                        : theme::Primary;
                RECT rcFill = { rcBar.left, rcBar.top,
                                rcBar.left + w, rcBar.bottom };
                HBRUSH br = CreateSolidBrush(c);
                FillRect(dc, &rcFill, br);
                DeleteObject(br);
            }
            wchar_t pct[16];
            swprintf_s(pct, 16, L"%d%%", bar);
            RECT rcPct = { rc.right - 16 - 42, y, rc.right - 16, y + 22 };
            SelectObject(dc, m_fontMeta);
            SetTextColor(dc, theme::TextFaint);
            DrawTextW(dc, pct, -1, &rcPct,
                      DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
        }
        y += 26;
    }
}

void App::DrawConfigPage(HDC dc, const RECT& rcClient)
{
    int x0 = kSideW + 16, x1 = rcClient.right - 16;
    int W = x1 - x0;
    int colW = (W - 14) / 2;

    if (!m_sysLoaded) {
        m_sys = sysinfo::Collect();
        m_sysLoaded = true;
    }

    // 数据库路径（与服务端同一用户目录）
    std::wstring dbPath = L"%APPDATA%\\AireRecord\\airecord.db";

    // 服务状态
    std::wstring svcText;
    int svcState = sysinfo::QueryServiceState(L"AireRecord", svcText);

    // ---- 组装卡片 ----
    std::vector<CfgCardData> cards;

    // 1) 服务与进程
    {
        CfgCardData c = { L"\xE9F9", L"服务与进程",
            svcState == 2 ? L"运行中" : (svcState == 1 ? L"已停止" : L"未安装"),
            svcState == 2, {} };
        wchar_t buf[128];
        c.rows.push_back(L"运行模式\t系统服务 + 会话代理\t-1");
        swprintf_s(buf, 128, L"服务状态\t● %s\t-1", svcText.c_str());
        c.rows.push_back(buf);
        c.rows.push_back(L"启动类型\t自动（随系统启动）\t-1");
        c.rows.push_back(std::wstring(L"IPC 管道\t") + ipc::PipeName());
        c.rows.push_back(L"数据库\t" + dbPath);
        cards.push_back(std::move(c));
    }

    // 2) 操作系统
    {
        CfgCardData c = { L"\xE770", L"操作系统", L"已采集", true, {} };
        c.rows.push_back(L"系统版本\t" + m_sys.osName +
                         L"（Build " + m_sys.osBuild + L"）");
        c.rows.push_back(L"系统架构\t" + m_sys.arch + L" · 中文简体");
        c.rows.push_back(L"计算机名\t" + m_sys.computerName);
        c.rows.push_back(L"当前用户\t" + m_sys.userName);
        c.rows.push_back(L"开机时长\t" + m_sys.uptime);
        cards.push_back(std::move(c));
    }

    // 3) 处理器与内存
    {
        CfgCardData c = { L"\xE950", L"处理器与内存", L"已采集", true, {} };
        c.rows.push_back(L"处理器\t" + m_sys.cpuName);
        c.rows.push_back(L"逻辑处理器\t" +
                         std::to_wstring(m_sys.logicalCpu) + L" 个");
        int cpu = (int)(m_sys.cpuUsage < 0 ? 0 : m_sys.cpuUsage);
        c.rows.push_back(L"CPU 占用\t" +
                         std::to_wstring(cpu) + L"%" +
                         L"\t" + std::to_wstring(cpu));
        double usedMB = (double)(m_sys.memTotalMB - m_sys.memAvailMB);
        int memPct = m_sys.memTotalMB
            ? (int)(usedMB * 100 / m_sys.memTotalMB) : 0;
        c.rows.push_back(L"物理内存\t" + FmtGB(m_sys.memTotalMB));
        c.rows.push_back(L"内存占用\t已用 " + FmtGB((long long)usedMB) +
                         L"\t" + std::to_wstring(memPct));
        cards.push_back(std::move(c));
    }

    // 4) 显示与图形
    {
        CfgCardData c = { L"\xE7F4", L"显示与图形", L"已采集", true, {} };
        c.rows.push_back(L"显卡\t" + m_sys.gpuName);
        c.rows.push_back(L"主显示器\t" + m_sys.primaryRes);
        c.rows.push_back(L"显示器数量\t" +
                         std::to_wstring(m_sys.monitorCount) + L" 台");
        cards.push_back(std::move(c));
    }

    // 5) 磁盘
    {
        CfgCardData c = { L"\xEDA2", L"磁盘", L"已采集", true, {} };
        for (auto& d : m_sys.disks) {
            int pct = d.totalMB
                ? (int)(((d.totalMB - d.freeMB) * 100) / d.totalMB) : 0;
            c.rows.push_back(d.letter + L" 剩余\t" +
                             FmtGB(d.freeMB) + L" / " + FmtGB(d.totalMB) +
                             L"\t" + std::to_wstring(pct));
        }
        if (m_sys.disks.empty())
            c.rows.push_back(L"磁盘\t未采集到固定磁盘\t-1");
        cards.push_back(std::move(c));
    }

    // 6) 网络
    {
        CfgCardData c = { L"\xE968", L"网络",
            m_sys.netIp.empty() ? L"未连接" : L"已连接",
            !m_sys.netIp.empty(), {} };
        c.rows.push_back(L"主机名\t" + m_sys.computerName);
        c.rows.push_back(L"IP 地址\t" +
                         (m_sys.netIp.empty() ? L"—" : m_sys.netIp));
        c.rows.push_back(L"网卡\t" +
                         (m_sys.netAdapter.empty() ? L"—"
                                                   : m_sys.netAdapter));
        cards.push_back(std::move(c));
    }

    // 7) 快捷键
    {
        CfgCardData c = { L"\xE765", L"快捷键", L"3 个已启用", false, {} };
        c.rows.push_back(L"呼出主窗口\tCtrl + Shift + V\t-1");
        c.rows.push_back(L"搜索\tF3\t-1");
        c.rows.push_back(L"手动录入\tF4\t-1");
        cards.push_back(std::move(c));
    }

    // 8) 功能模块
    {
        CfgCardData c = { L"\xE71A", L"功能模块（插件注册）",
                          L"1 / 3 已启用", false, {} };
        c.rows.push_back(L"剪贴板\t运行中 · ClipboardPlugin\t-1");
        c.rows.push_back(L"文件监控\t规划中 · FileWatchPlugin\t-1");
        c.rows.push_back(L"打印监控\t规划中 · PrintWatchPlugin\t-1");
        cards.push_back(std::move(c));
    }

    // ---- 双栏排版：放入当前较矮的一列 ----
    int yL = kTitleH + 16, yR = kTitleH + 16;
    int col = 0;
    for (auto& c : cards) {
        int h = CardHeight((int)c.rows.size());
        RECT rc;
        if (col == 0) { rc = { x0, yL, x0 + colW, yL + h }; yL += h + 14; }
        else          { rc = { x0 + colW + 14, yR, x1, yR + h }; yR += h + 14; }
        DrawInfoCard(dc, rc, c.glyph, c.title, c.badge, c.badgeOk, c.rows);
        col ^= 1;
    }
}

// ============================================================
// 剪贴板模块页：工具条 / 表单卡 / 详情面板（列表条目走 owner-draw）
// ============================================================

// 把主窗口坐标的点击转发给单行编辑框：聚焦 + 按 x 定位光标。
// 输入框的可见外框比 EDIT 客户区高（EDIT 只占中间一行文字高），
// 点击外框留白区也要能直接进入输入状态。
static void ForwardClickToEdit(HWND edit, HFONT font, POINT pt)
{
    if (!edit || !IsWindowVisible(edit)) return;
    RECT er;
    GetWindowRect(edit, &er);
    MapWindowPoints(nullptr, GetParent(edit), (LPPOINT)&er, 2);
    int cx = pt.x - er.left;
    int cy = pt.y - er.top;
    int ew = er.right - er.left;
    int eh = er.bottom - er.top;
    if (cx < 0) cx = 0;
    if (cx > ew) cx = ew;
    if (cy < 0) cy = 0;
    if (cy > eh) cy = eh;

    // 按文本宽度换算命中的字符下标（不依赖 EM_CHARINDEXFROMPOS）
    int len = GetWindowTextLengthW(edit);
    UINT idx = (UINT)len;
    if (len > 0 && font) {
        std::wstring buf(len + 1, L'\0');
        GetWindowTextW(edit, &buf[0], len + 1);
        buf.resize(len);
        HDC dc = GetDC(edit);
        HFONT old = (HFONT)SelectObject(dc, font);
        // 左内边距 8px（EM_SETMARGINS），文字起点即 client x=8
        int target = cx - 8;
        int acc = 0;
        for (int i = 0; i < len; ++i) {
            SIZE sz{};
            GetTextExtentPoint32W(dc, buf.c_str() + i, 1, &sz);
            if (target < acc + sz.cx / 2) { idx = (UINT)i; break; }
            acc += sz.cx;
        }
        SelectObject(dc, old);
        ReleaseDC(edit, dc);
    }
    SetFocus(edit);
    SendMessageW(edit, EM_SETSEL, idx, idx);
    SetCaretPos(cx, cy);
}

void App::DrawClipsPage(HDC dc, const RECT& rcClient)
{
    SetBkMode(dc, TRANSPARENT);

    // ---- 工具条 ----
    // 搜索框：白底圆角 + 1px 边框（父窗口绘制；EDIT 只占中间一行，故边框不能画在 EDIT 外框上）
    RECT rcS = m_rcSearchBox;
    FillRoundRect(dc, rcS, theme::Surface, 8);
    DrawRoundRectOutline(dc, rcS, theme::BorderIn, 8);

    // 类型筛选胶囊（全部 / 文本 / 链接 / 手动）
    const wchar_t* chips[4] = { L"全部", L"文本", L"链接", L"手动" };
    for (int i = 0; i < 4; ++i) {
        RECT r = m_rcChips[i];
        bool on = (m_typeFilter == i);
        FillRoundRect(dc, r, on ? theme::Primary : theme::Surface, 13);
        DrawRoundRectOutline(dc, r,
            on ? theme::Primary : theme::BorderIn, 13);
        SelectObject(dc, on ? m_fontBtn : m_fontMeta);
        SetTextColor(dc, on ? RGB(255, 255, 255) : RGB(78, 89, 105));
        DrawTextW(dc, chips[i], -1, &r,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }

    // ＋手动录入 主按钮（同保存按钮的 Primary 样式）
    {
        RECT b = m_rcAddBtn;
        FillRoundRect(dc, b, theme::Primary, 8);
        SelectObject(dc, m_fontBtn);
        SetTextColor(dc, RGB(255, 255, 255));
        DrawTextW(dc, L"＋ 手动录入", -1, &b,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }

    // ---- 表单卡（展开时） ----
    if (m_formOpen && !m_editingId) {
        RECT r = m_rcFormCard;
        FillRoundRect(dc, r, theme::Surface, 10);
        DrawRoundRectOutline(dc, r, theme::Split, 10);

        HBRUSH pbr = CreateSolidBrush(theme::Primary);
        RECT bar = { r.left + 16, r.top + 20, r.left + 19, r.top + 34 };
        FillRect(dc, &bar, pbr);
        DeleteObject(pbr);
        RECT rcFt = { r.left + 25, r.top + 16, r.right - 80, r.top + 38 };
        SelectObject(dc, m_fontSection);
        SetTextColor(dc, theme::Text);
        DrawTextW(dc, L"添加记录", -1, &rcFt,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER);

        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, theme::TextFaint);
        DrawTextW(dc, L"收起 ▴", -1, &m_rcFormClose,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER);

        auto BorderOf = [&](HWND ctrl, COLORREF color) {
            if (!ctrl) return;
            RECT rr;
            GetWindowRect(ctrl, &rr);
            MapWindowPoints(nullptr, m_hwnd, (LPPOINT)&rr, 2);
            InflateRect(&rr, 1, 1);
            DrawRoundRectOutline(dc, rr, color, 4);
        };
        BorderOf(m_hManualEdit, theme::BorderIn);
        // 备注框：边框画在自绘的视觉框上（EDIT 只占中间一行文字高）
        if (m_rcNoteBox.right > m_rcNoteBox.left) {
            RECT nr = m_rcNoteBox;
            InflateRect(&nr, 1, 1);
            DrawRoundRectOutline(dc, nr, theme::BorderIn, 4);
        }
    }

    // ---- 详情面板 ----
    DrawDetailPanel(dc);
}

void App::DrawDetailPanel(HDC dc)
{
    RECT d = m_rcDetail;
    FillRoundRect(dc, d, theme::Surface, 10);
    DrawRoundRectOutline(dc, d, theme::Split, 10);
    SetBkMode(dc, TRANSPARENT);

    if (m_selEntry < 0 || m_selEntry >= (int)m_entries.size()) {
        RECT rcE = { d.left + 16, d.top, d.right - 16, d.bottom };
        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, theme::TextFaint);
        DrawTextW(dc, L"选择左侧条目查看详情", -1, &rcE,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        return;
    }
    const UiEntry& e = m_entries[m_selEntry];

    // "详情" 标题
    RECT rcCap = { d.left + 16, d.top + 14, d.right - 16, d.top + 34 };
    SelectObject(dc, m_fontMeta);
    SetTextColor(dc, RGB(143, 149, 158));
    DrawTextW(dc, L"详情", -1, &rcCap,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER);

    // 类型胶囊
    int t = TypeOfEntry(e);
    bool isFile = (t == 1) && e.content.size() >= 3 &&
                  e.content[1] == L':' && e.content[2] == L'\\';
    const wchar_t* typeName =
        (e.source == L"Manual") ? L"手动录入"
      : (t == 2) ? L"链接" : (isFile ? L"文件" : L"文本");
    std::wstring pill = std::wstring(typeName) +
                        (e.source == L"Manual" ? L" · 手动添加"
                                               : L" · 来自剪贴板");
    DrawTag(dc, d.left + 16, d.top + 44, pill.c_str(),
            theme::TagInfoBg, theme::TagInfoTx, m_fontMeta);

    // 内容框（灰底圆角，多行截断）
    RECT rcBox = { d.left + 16, d.top + 76, d.right - 16, d.top + 172 };
    FillRoundRect(dc, rcBox, RGB(247, 248, 250), 10);
    DrawRoundRectOutline(dc, rcBox, theme::Split, 10);
    RECT rcContent = { rcBox.left + 12, rcBox.top + 10,
                       rcBox.right - 12, rcBox.bottom - 10 };
    SelectObject(dc, m_fontMeta);
    SetTextColor(dc, RGB(51, 56, 63));
    std::wstring txt = e.content;
    DrawTextW(dc, txt.c_str(), (int)txt.size(), &rcContent,
              DT_WORDBREAK | DT_END_ELLIPSIS | DT_LEFT);

    // 属性
    RECT rcSec = { d.left + 16, d.top + 186, d.right - 16, d.top + 204 };
    SelectObject(dc, m_fontMeta);
    SetTextColor(dc, RGB(143, 149, 158));
    DrawTextW(dc, L"属性", -1, &rcSec,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    std::wstring src = (e.source == L"Manual") ? L"手动录入" : e.source;
    const wchar_t* keys[4] =
        { L"来源应用", L"记录时间", L"内容类型", L"字符数" };
    std::wstring vals[4] = {
        src, e.createdAt, typeName,
        std::to_wstring((long)e.content.size()),
    };
    int ry = d.top + 208;
    for (int i = 0; i < 4; ++i) {
        RECT rcK = { d.left + 16, ry, d.left + 84, ry + 22 };
        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, theme::TextFaint);
        DrawTextW(dc, keys[i], -1, &rcK,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        RECT rcV = { d.left + 88, ry, d.right - 16, ry + 22 };
        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, RGB(78, 89, 105));
        DrawTextW(dc, vals[i].c_str(), (int)vals[i].size(), &rcV,
                  DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        ry += 22;
    }

    // 备注（点击编辑）
    RECT rcNs = { d.left + 16, d.top + 308, d.right - 16, d.top + 326 };
    SelectObject(dc, m_fontMeta);
    SetTextColor(dc, RGB(143, 149, 158));
    DrawTextW(dc, L"备注（点击编辑）", -1, &rcNs,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    RECT nb = DetailNoteBox(d);
    FillRoundRect(dc, nb, RGB(255, 254, 245), 8);
    HPEN pen = CreatePen(PS_DASH, 1, RGB(212, 217, 225));
    HPEN oldPen = (HPEN)SelectObject(dc, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, nb.left, nb.top, nb.right + 1, nb.bottom + 1, 8, 8);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBr);
    DeleteObject(pen);
    RECT rcNote = { nb.left + 10, nb.top + 8,
                    nb.right - 10, nb.bottom - 8 };
    SelectObject(dc, m_fontMeta);
    SetTextColor(dc, RGB(100, 106, 115));
    DrawTextW(dc, e.note.c_str(), (int)e.note.size(), &rcNote,
              DT_WORDBREAK | DT_END_ELLIPSIS | DT_LEFT);

    // 底部操作：复制内容 / 备注 / 删除
    int bw = (d.right - d.left - 32 - 16) / 3;
    int by = d.bottom - 46;
    m_rcDBtn[0] = { d.left + 16, by, d.left + 16 + bw, by + 32 };
    m_rcDBtn[1] = { m_rcDBtn[0].right + 8, by,
                    m_rcDBtn[0].right + 8 + bw, by + 32 };
    m_rcDBtn[2] = { m_rcDBtn[1].right + 8, by, d.right - 16, by + 32 };

    FillRoundRect(dc, m_rcDBtn[0], theme::Primary, 7);
    SelectObject(dc, m_fontBtn);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, L"复制内容", -1, &m_rcDBtn[0],
              DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    FillRoundRect(dc, m_rcDBtn[1], theme::Surface, 7);
    DrawRoundRectOutline(dc, m_rcDBtn[1], theme::BorderIn, 7);
    SelectObject(dc, m_fontBtn);
    SetTextColor(dc, RGB(78, 89, 105));
    DrawTextW(dc, L"备注", -1, &m_rcDBtn[1],
              DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    FillRoundRect(dc, m_rcDBtn[2], theme::Surface, 7);
    DrawRoundRectOutline(dc, m_rcDBtn[2], RGB(245, 194, 196), 7);
    SelectObject(dc, m_fontBtn);
    SetTextColor(dc, RGB(229, 72, 77));
    DrawTextW(dc, L"删除", -1, &m_rcDBtn[2],
              DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

// ============================================================
// 底部状态栏：服务状态 / 模块数 / 快捷键提示
// ============================================================

static int KbdChipW(HDC dc, const wchar_t* key, const wchar_t* label,
                    HFONT font)
{
    SelectObject(dc, font);
    SIZE szK, szL;
    GetTextExtentPoint32W(dc, key, (int)wcslen(key), &szK);
    GetTextExtentPoint32W(dc, label, (int)wcslen(label), &szL);
    return (szK.cx + 12) + 5 + szL.cx;
}

static void DrawKbdChip(HDC dc, int rightX, int cy,
                        const wchar_t* key, const wchar_t* label,
                        HFONT font)
{
    SelectObject(dc, font);
    SIZE szK, szL;
    GetTextExtentPoint32W(dc, key, (int)wcslen(key), &szK);
    GetTextExtentPoint32W(dc, label, (int)wcslen(label), &szL);
    int chipW = szK.cx + 12;
    int total = chipW + 5 + szL.cx;
    int x = rightX - total;
    RECT rcChip = { x, cy - 9, x + chipW, cy + 9 };
    FillRoundRect(dc, rcChip, RGB(242, 243, 245), 4);
    DrawRoundRectOutline(dc, rcChip, RGB(229, 232, 237), 4);
    SetTextColor(dc, RGB(90, 98, 112));
    TextOutW(dc, x + 6, cy - szK.cy / 2, key, (int)wcslen(key));
    SetTextColor(dc, RGB(143, 149, 158));
    TextOutW(dc, x + chipW + 5, cy - szL.cy / 2, label, (int)wcslen(label));
}

void App::DrawStatusBar(HDC dc, const RECT& rc)
{
    RECT sb = { 0, rc.bottom - kStatusH, rc.right, rc.bottom };
    FillRect(dc, &sb, m_brWhite);
    RECT rcLine = { 0, sb.top, rc.right, sb.top + 1 };
    HBRUSH brLine = CreateSolidBrush(theme::Split);
    FillRect(dc, &rcLine, brLine);
    DeleteObject(brLine);

    SetBkMode(dc, TRANSPARENT);
    int cy = sb.top + kStatusH / 2;

    // 左：服务状态点 + 文本 + 模块数
    COLORREF dot = m_serviceOk ? theme::OkDot : theme::ErrDot;
    HBRUSH brD = CreateSolidBrush(dot);
    HPEN oldPen = (HPEN)SelectObject(dc, GetStockObject(NULL_PEN));
    HBRUSH oldBr = (HBRUSH)SelectObject(dc, brD);
    Ellipse(dc, 14, cy - 3, 20, cy + 3);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBr);
    DeleteObject(brD);

    SelectObject(dc, m_fontMeta);
    SetTextColor(dc, m_serviceOk ? theme::TagOkTx : theme::ErrTagTx);
    std::wstring svc = m_serviceOk ? L"服务运行中（airecordd.exe）"
                                   : L"服务未连接";
    TextOutW(dc, 26, cy - 8, svc.c_str(), (int)svc.size());

    SIZE szS;
    GetTextExtentPoint32W(dc, svc.c_str(), (int)svc.size(), &szS);
    int x2 = 26 + szS.cx + 18;
    std::wstring mods = L"启用模块 1 / 3";
    SetTextColor(dc, RGB(143, 149, 158));
    TextOutW(dc, x2, cy - 8, mods.c_str(), (int)mods.size());

    // 右：快捷键提示（从右往左排）
    int right = rc.right - 14;
    DrawKbdChip(dc, right, cy, L"F4", L"手动录入", m_fontMeta);
    right -= KbdChipW(dc, L"F4", L"手动录入", m_fontMeta) + 20;
    DrawKbdChip(dc, right, cy, L"F3", L"搜索", m_fontMeta);
    right -= KbdChipW(dc, L"F3", L"搜索", m_fontMeta) + 20;
    DrawKbdChip(dc, right, cy, L"Ctrl+Shift+V", L"呼出 / 隐藏", m_fontMeta);
}

// ---------- 列表刷新（IPC → 解析 → 类型筛选 → 日期分组） ----------
void App::RefreshList(const std::wstring& filter)
{
    m_entries.clear();

    std::wstring resp = m_ipc.Request(
        L"LIST\t" + ipc::EscapeField(filter));

    bool ok = false;
    if (!resp.empty()) {
        auto lines = ipc::SplitLines(resp);
        if (!lines.empty() && lines[0] == L"OK") {
            ok = true;
            for (size_t i = 1; i < lines.size(); ++i) {
                if (lines[i] == L"END") break;
                auto f = ipc::SplitTabs(lines[i]);
                if (f.size() < 5) continue;
                UiEntry e;
                e.id        = _wtoi64(f[0].c_str());
                e.source    = f[1];
                e.createdAt = f[2];
                e.note      = ipc::UnescapeField(f[3]);
                e.content   = ipc::UnescapeField(f[4]);
                m_entries.push_back(std::move(e));
            }
        }
    }

    SetStatusText(ok ? L"记录中" : L"服务未连接", ok);

    // ---- 行模型：分组头 + 条目（类型筛选在客户端本地做） ----
    m_rows.clear();
    m_hoverItem = -1;
    SYSTEMTIME now = {}, yst = {};
    GetLocalTime(&now);
    {
        FILETIME ft = {};
        SystemTimeToFileTime(&now, &ft);
        ULARGE_INTEGER u = { ft.dwLowDateTime, ft.dwHighDateTime };
        u.QuadPart -= 86400ULL * 10000000ULL;    // 昨天
        ft.dwLowDateTime = u.LowPart;
        ft.dwHighDateTime = u.HighPart;
        FileTimeToSystemTime(&ft, &yst);
    }
    wchar_t today[16], yest[16];
    StringCchPrintfW(today, 16, L"%04d-%02d-%02d",
                     now.wYear, now.wMonth, now.wDay);
    StringCchPrintfW(yest, 16, L"%04d-%02d-%02d",
                     yst.wYear, yst.wMonth, yst.wDay);

    auto AddGroup = [&](const wchar_t* label) {
        m_rows.push_back({ true, label, 0 });
    };

    for (size_t i = 0; i < m_entries.size(); ++i) {
        const UiEntry& e = m_entries[i];
        if (m_typeFilter && TypeOfEntry(e) != m_typeFilter) continue;

        const wchar_t* want =
            (wcsncmp(e.createdAt.c_str(), today, 10) == 0) ? L"today"
          : (wcsncmp(e.createdAt.c_str(), yest, 10) == 0) ? L"yest"
          : L"older";
        bool needHead = m_rows.empty() || m_rows.back().header;
        if (!needHead) {
            // 与上一条不同组时插入新分组头
            const std::wstring& prev =
                m_entries[m_rows.back().entry].createdAt;
            const wchar_t* prevG =
                (wcsncmp(prev.c_str(), today, 10) == 0) ? L"today"
              : (wcsncmp(prev.c_str(), yest, 10) == 0) ? L"yest"
              : L"older";
            needHead = (wcscmp(prevG, want) != 0);
        }
        if (needHead) {
            if (want == std::wstring(L"today"))
                AddGroup(L"今天");
            else if (want == std::wstring(L"yest"))
                AddGroup(L"昨天");
            else
                AddGroup(L"更早");
        }
        m_rows.push_back({ false, L"", i });
    }

    // ---- 填充列表框（VARIABLE owner-draw：分组头 30 / 条目卡 64） ----
    SendMessageW(m_hList, LB_RESETCONTENT, 0, 0);
    int selRow = -1, firstEntryRow = -1;
    for (size_t r = 0; r < m_rows.size(); ++r) {
        int idx = (int)SendMessageW(m_hList, LB_ADDSTRING, 0, (LPARAM)L"");
        SendMessageW(m_hList, LB_SETITEMDATA, idx, (LPARAM)r);
        SendMessageW(m_hList, LB_SETITEMHEIGHT, idx,
                     m_rows[r].header ? 30 : 64);
        if (!m_rows[r].header) {
            if (firstEntryRow < 0) firstEntryRow = idx;
            if ((int)m_rows[r].entry == m_selEntry) selRow = idx;
        }
    }
    // 选中态：优先保持原选中，否则选第一条；详情面板随选中变化
    int curSel = (int)SendMessageW(m_hList, LB_GETCURSEL, 0, 0);
    if (selRow >= 0 && curSel != selRow)
        SendMessageW(m_hList, LB_SETCURSEL, selRow, 0);
    else if (selRow < 0 && firstEntryRow >= 0) {
        SendMessageW(m_hList, LB_SETCURSEL, firstEntryRow, 0);
        selRow = firstEntryRow;
    }
    if (selRow >= 0 && selRow < (int)m_rows.size() && !m_rows[selRow].header)
        m_selEntry = (int)m_rows[selRow].entry;
    else if (firstEntryRow < 0)
        m_selEntry = -1;

    InvalidateRect(m_hList, nullptr, TRUE);
    if (m_page == PageClips) {
        RECT rcD = m_rcDetail;
        InvalidateRect(m_hwnd, &rcD, FALSE);
    }
}

// ---------- 相对时间戳（产品化表达，替代机器格式） ----------
std::wstring App::FormatRelTime(const std::wstring& created)
{
    SYSTEMTIME st = {}, now = {};
    if (swscanf_s(created.c_str(), L"%hd-%hd-%hd %hd:%hd:%hd",
                  &st.wYear, &st.wMonth, &st.wDay,
                  &st.wHour, &st.wMinute, &st.wSecond) < 3)
        return created;
    GetLocalTime(&now);

    FILETIME fa = {}, fb = {};
    if (!SystemTimeToFileTime(&st, &fa) || !SystemTimeToFileTime(&now, &fb))
        return created;
    ULARGE_INTEGER ua = { fa.dwLowDateTime, fa.dwHighDateTime };
    ULARGE_INTEGER ub = { fb.dwLowDateTime, fb.dwHighDateTime };
    long long diffSec =
        (long long)((ub.QuadPart - ua.QuadPart) / 10000000ULL);
    if (diffSec < 0) diffSec = 0;

    wchar_t buf[64];
    if (diffSec < 60)          return L"刚刚";
    if (diffSec < 3600) { swprintf_s(buf, L"%d 分钟前", (int)(diffSec / 60)); return buf; }
    if (diffSec < 86400) { swprintf_s(buf, L"%d 小时前", (int)(diffSec / 3600)); return buf; }
    if (diffSec < 7 * 86400) { swprintf_s(buf, L"%d 天前", (int)(diffSec / 86400)); return buf; }
    if (st.wYear == now.wYear) {
        swprintf_s(buf, L"%d月%d日 %02d:%02d",
                   st.wMonth, st.wDay, st.wHour, st.wMinute);
        return buf;
    }
    swprintf_s(buf, L"%d年%d月%d日", st.wYear, st.wMonth, st.wDay);
    return buf;
}

// ---------- 自绘标题栏（无边框壳：Logo + 名称 + 页面 + 服务胶囊 + 三键） ----------
void App::DrawTitleBar(HDC dc, const RECT& rc)
{
    SetBkMode(dc, TRANSPARENT);

    RECT rcBar = { rc.left, rc.top, rc.right, kTitleH };
    FillRect(dc, &rcBar, m_brWhite);
    RECT rcDiv = { rc.left, kTitleH - 1, rc.right, kTitleH };
    HBRUSH brDiv = CreateSolidBrush(theme::Split);
    FillRect(dc, &rcDiv, brDiv);
    DeleteObject(brDiv);

    // Logo：26x26 主色圆角块 + 白色剪贴板图形
    RECT rcLogo = { 14, 9, 40, 35 };
    FillRoundRect(dc, rcLogo, theme::Primary, 7);
    RECT rcBoard = { rcLogo.left + 7, rcLogo.top + 6,
                     rcLogo.right - 7, rcLogo.bottom - 6 };
    FillRoundRect(dc, rcBoard, RGB(255, 255, 255), 2);
    RECT rcClip = { rcLogo.left + 11, rcLogo.top + 2,
                    rcLogo.right - 11, rcLogo.top + 9 };
    FillRoundRect(dc, rcClip, RGB(255, 255, 255), 2);
    HBRUSH brBar = CreateSolidBrush(theme::Primary);
    RECT rcL1 = { rcBoard.left + 3, rcBoard.top + 4, rcBoard.right - 8, rcBoard.top + 7 };
    RECT rcL2 = { rcBoard.left + 3, rcBoard.top + 10, rcBoard.right - 3, rcBoard.top + 13 };
    FillRect(dc, &rcL1, brBar);
    FillRect(dc, &rcL2, brBar);
    DeleteObject(brBar);

    // 应用名 + 竖分隔线 + 页面名
    RECT rcTitle = { 52, 0, rc.right - 380, kTitleH };
    SetTextColor(dc, theme::Text);
    SelectObject(dc, m_fontTitle);
    DrawTextW(dc, L"AireRecord", -1, &rcTitle,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    SIZE szName;
    GetTextExtentPoint32W(dc, L"AireRecord", 10, &szName);
    int dx = 52 + szName.cx + 10;
    HBRUSH brV = CreateSolidBrush(theme::Split);
    RECT rcV = { dx, kTitleH / 2 - 8, dx + 1, kTitleH / 2 + 8 };
    FillRect(dc, &rcV, brV);
    DeleteObject(brV);

    const wchar_t* pageName = L"工作台总览";
    if      (m_page == PageClips)  pageName = L"剪贴板模块";
    else if (m_page == PageConfig) pageName = L"机器配置";
    RECT rcSub = { dx + 10, 0, rc.right - 380, kTitleH };
    SetTextColor(dc, RGB(143, 149, 158));
    SelectObject(dc, m_fontMeta);
    DrawTextW(dc, pageName, -1, &rcSub,
              DT_SINGLELINE | DT_LEFT | DT_VCENTER);

    // 服务状态胶囊（绿：服务运行中 · 已记录 N 条 / 红：服务未连接）
    int tagRight = rc.right - kCapBtnW * 3 - 12;
    {
        std::wstring txt = m_serviceOk
            ? (L"服务运行中 · 已记录 " +
               (m_stats.total >= 0 ? std::to_wstring((long)m_stats.total)
                                   : L"—") + L" 条")
            : L"服务未连接";
        COLORREF bg  = m_serviceOk ? RGB(246, 255, 237) : theme::ErrTagBg;
        COLORREF brd = m_serviceOk ? RGB(183, 235, 143) : RGB(255, 204, 199);
        COLORREF tx  = m_serviceOk ? theme::TagOkTx    : theme::ErrTagTx;
        COLORREF dot = m_serviceOk ? theme::OkDot      : theme::ErrDot;

        SelectObject(dc, m_fontMeta);
        SIZE sz;
        GetTextExtentPoint32W(dc, txt.c_str(), (int)txt.size(), &sz);
        int h = 24, w = sz.cx + 34;
        RECT rcP = { tagRight - w, kTitleH / 2 - h / 2, tagRight,
                     kTitleH / 2 + h / 2 };
        FillRoundRect(dc, rcP, bg, 11);
        DrawRoundRectOutline(dc, rcP, brd, 11);
        HBRUSH brD = CreateSolidBrush(dot);
        HPEN oldPen = (HPEN)SelectObject(dc, GetStockObject(NULL_PEN));
        HBRUSH oldBr = (HBRUSH)SelectObject(dc, brD);
        Ellipse(dc, rcP.left + 10, kTitleH / 2 - 4,
                rcP.left + 18, kTitleH / 2 + 4);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBr);
        DeleteObject(brD);
        SetTextColor(dc, tx);
        TextOutW(dc, rcP.left + 26, kTitleH / 2 - sz.cy / 2,
                 txt.c_str(), (int)txt.size());
    }

    // 窗口三键（Segoe MDL2 字形，hover 灰底 / 关闭 hover 红底）
    SelectObject(dc, m_fontIcon);
    const wchar_t* glyphs[3] = { L"\xE921",
                                 IsZoomed(m_hwnd) ? L"\xE923" : L"\xE922",
                                 L"\xE8BB" };
    for (int i = 0; i < 3; ++i) {
        RECT rcBtn = { rc.right - kCapBtnW * (3 - i), 0,
                       rc.right - kCapBtnW * (2 - i), kTitleH };
        if (m_capHover == i + 1) {
            HBRUSH hb = CreateSolidBrush(
                (i == 2) ? RGB(232, 17, 35) : RGB(229, 229, 229));
            FillRect(dc, &rcBtn, hb);
            DeleteObject(hb);
        }
        SetTextColor(dc, (i == 2 && m_capHover == 3)
                             ? RGB(255, 255, 255) : theme::Text);
        DrawTextW(dc, glyphs[i], -1, &rcBtn,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
}

// 标题栏按钮命中测试（客户区坐标）：0无 1最小化 2最大化 3关闭
int App::CaptionButtonAt(POINT pt) const
{
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    if (pt.y < 0 || pt.y >= kTitleH) return 0;
    if (pt.x >= rc.right - kCapBtnW)      return 3;
    if (pt.x >= rc.right - kCapBtnW * 2)  return 2;
    if (pt.x >= rc.right - kCapBtnW * 3)  return 1;
    return 0;
}

// ---------- 条目操作（悬停钮 / 详情面板 / 右键菜单 共用） ----------
// op: 0=复制 1=备注 2=删除
void App::OnListOp(int entryIdx, int op)
{
    if (entryIdx < 0 || entryIdx >= (int)m_entries.size()) return;
    const UiEntry& e = m_entries[entryIdx];
    switch (op) {
    case 0:
        m_ipc.Request(L"COPY\t" + ipc::EscapeField(e.content));
        break;
    case 1:
        StartNoteEdit(e.id, e.note);
        break;
    case 2:
        if (MessageBoxW(m_hwnd,
                L"确定删除这条记录吗？删除后不可恢复。",
                L"删除记录", MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
            m_ipc.Request(L"DEL\t" + std::to_wstring(e.id));
            m_selEntry = -1;
            RefreshList(GetSearchText());
            RefreshStats();
        }
        break;
    }
}

// 详情面板操作按钮命中测试（客户区坐标）：0无 1复制 2备注 3删除
int App::DetailButtonAt(POINT pt) const
{
    for (int i = 0; i < 3; ++i)
        if (PtInRect(&m_rcDBtn[i], pt)) return i + 1;
    return 0;
}

// ---------- 备注编辑流（详情面板"备注"区 / 右键菜单入口） ----------
void App::StartNoteEdit(long long id, const std::wstring& note)
{
    if (m_formOpen) ToggleForm(false);   // 表单卡收起，备注控件让位
    m_editingId = id;
    Layout();                            // 备注框覆盖到详情面板
    SetWindowTextW(m_hNoteEdit, note.c_str());
    SetWindowTextW(m_hSaveBtn, L"保存备注");
    ShowWindow(m_hNoteEdit, SW_SHOW);
    ShowWindow(m_hSaveBtn, SW_SHOW);
    SetFocus(m_hNoteEdit);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::CancelNoteEdit()
{
    if (!m_editingId) return;
    m_editingId = 0;
    SetWindowTextW(m_hSaveBtn, L"保存记录");
    Layout();
    bool show = (m_page == PageClips && m_formOpen);
    ShowWindow(m_hNoteEdit, show ? SW_SHOW : SW_HIDE);
    ShowWindow(m_hSaveBtn, show ? SW_SHOW : SW_HIDE);
    ShowWindow(m_hManualEdit, show ? SW_SHOW : SW_HIDE);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::SaveNoteEdit()
{
    if (!m_editingId) return;
    std::wstring note = GetWindowTextStr(m_hNoteEdit);
    m_ipc.Request(L"NOTE\t" + std::to_wstring(m_editingId) +
                  L"\t" + ipc::EscapeField(note));
    m_editingId = 0;
    SetWindowTextW(m_hSaveBtn, L"保存记录");
    Layout();
    bool show = (m_page == PageClips && m_formOpen);
    ShowWindow(m_hNoteEdit, show ? SW_SHOW : SW_HIDE);
    ShowWindow(m_hSaveBtn, show ? SW_SHOW : SW_HIDE);
    ShowWindow(m_hManualEdit, show ? SW_SHOW : SW_HIDE);
    RefreshList(GetSearchText());
    RefreshStats();
}

// ---------- 列表右键菜单：复制 / 编辑备注 / 删除 ----------
void App::ShowItemMenu(POINT screenPt, int item)
{
    if (item < 0 || item >= (int)m_entries.size()) return;

    enum { CMD_COPY = 1, CMD_EDIT = 2, CMD_DEL = 3 };
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, CMD_COPY, L"复制内容");
    AppendMenuW(menu, MF_STRING, CMD_EDIT,
                m_entries[item].note.empty() ? L"添加备注" : L"编辑备注");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CMD_DEL, L"删除记录");

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY |
                                   TPM_RIGHTBUTTON,
                             screenPt.x, screenPt.y, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    // 动作与悬停钮/详情面板共用同一入口
    switch (cmd) {
    case CMD_COPY: OnListOp(item, 0); break;
    case CMD_EDIT: OnListOp(item, 1); break;
    case CMD_DEL:  OnListOp(item, 2); break;
    }
}

// ---------- 列表条目（白卡片：类型图标 + 内容 + 备注标签 + 元信息 + 悬停操作） ----------
void App::DrawListItem(const DRAWITEMSTRUCT* dis)
{
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    bool selected = (dis->itemState & ODS_SELECTED) != 0;
    bool hovered  = ((int)dis->itemID == m_hoverItem);

    // 行背景 = 页面灰，卡片 = 白色圆角（原型 entry-card 样式）
    HBRUSH br = CreateSolidBrush(theme::Bg);
    FillRect(dc, &rc, br);
    DeleteObject(br);

    if (dis->itemID >= m_rows.size())
        return;
    const ListRow& row = m_rows[dis->itemID];

    SetBkMode(dc, TRANSPARENT);

    // ---- 分组头（今天 / 昨天 / 更早） ----
    if (row.header) {
        int cy = rc.top + 15;
        SelectObject(dc, m_fontMeta);
        SetTextColor(dc, RGB(143, 149, 158));
        SIZE sz;
        GetTextExtentPoint32W(dc, row.label.c_str(),
                              (int)row.label.size(), &sz);
        TextOutW(dc, rc.left + 4, cy - sz.cy / 2,
                 row.label.c_str(), (int)row.label.size());
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(233, 236, 241));
        HPEN oldPen = (HPEN)SelectObject(dc, pen);
        MoveToEx(dc, rc.left + 12 + sz.cx + 10, cy, nullptr);
        LineTo(dc, rc.right - 4, cy);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
        return;
    }

    // ---- 条目卡片 ----
    const UiEntry& e = m_entries[row.entry];
    RECT card = { rc.left + 2, rc.top + 3, rc.right - 6, rc.bottom - 9 };

    FillRoundRect(dc, card, theme::Surface, 10);
    DrawRoundRectOutline(dc, card,
        selected ? theme::Primary
                 : (hovered ? RGB(198, 212, 236) : theme::Split), 10);
    if (selected) {   // 外圈柔和选中环
        RECT ring = { card.left - 1, card.top - 1,
                      card.right + 1, card.bottom + 1 };
        DrawRoundRectOutline(dc, ring, RGB(214, 230, 255), 11);
    }

    // 类型图标块 34px
    int t = TypeOfEntry(e);
    bool isFile = (t == 1) &&
        ((e.content.size() >= 3 && e.content[1] == L':' &&
          e.content[2] == L'\\') ||
         (e.content.compare(0, 2, L"\\\\") == 0));
    const wchar_t* glyph;
    COLORREF icBg, icTx;
    if (e.source == L"Manual") {
        glyph = L"\xE70F"; icBg = RGB(243, 238, 255); icTx = RGB(123, 97, 255);
    } else if (t == 2) {
        glyph = L"\xE71B"; icBg = RGB(255, 243, 224); icTx = RGB(232, 137, 12);
    } else if (isFile) {
        glyph = L"\xE8B7"; icBg = RGB(230, 244, 241); icTx = RGB(15, 163, 127);
    } else {
        glyph = L"\xE8A5"; icBg = theme::TagInfoBg;   icTx = theme::Primary;
    }
    RECT ic = { card.left + 10, card.top + 11,
                card.left + 44, card.top + 45 };
    FillRoundRect(dc, ic, icBg, 8);
    SelectObject(dc, m_fontIconBig);
    SetTextColor(dc, icTx);
    DrawTextW(dc, glyph, -1, &ic, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    int tx = ic.right + 12;

    // 行1：内容（单行省略）+ 备注标签（右对齐黄标）
    int line1Right = card.right - 12;
    if (hovered) line1Right -= 3 * 26 + 2 * 6 + 6;   // 悬停操作钮占位
    if (!e.note.empty()) {
        std::wstring tagTxt = L"备注: " + e.note;
        if (tagTxt.size() > 30) tagTxt = tagTxt.substr(0, 30) + L"…";
        SelectObject(dc, m_fontMeta);
        SIZE sz;
        GetTextExtentPoint32W(dc, tagTxt.c_str(),
                              (int)tagTxt.size(), &sz);
        int tagW = sz.cx + 14;
        RECT rcTag = { line1Right - tagW, card.top + 8,
                       line1Right, card.top + 26 };
        FillRoundRect(dc, rcTag, RGB(255, 251, 230), 4);
        DrawRoundRectOutline(dc, rcTag, RGB(255, 229, 143), 4);
        SetTextColor(dc, RGB(212, 136, 6));
        DrawTextW(dc, tagTxt.c_str(), (int)tagTxt.size(), &rcTag,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        line1Right = rcTag.left - 8;
    }
    RECT rcLine1 = { tx, card.top + 9, line1Right, card.top + 30 };
    SelectObject(dc, m_font);
    SetTextColor(dc, theme::Text);
    std::wstring oneLine = e.content;
    for (auto& c : oneLine)
        if (c == L'\r' || c == L'\n') c = L' ';
    DrawTextW(dc, oneLine.c_str(), (int)oneLine.size(),
              &rcLine1, DT_SINGLELINE | DT_END_ELLIPSIS | DT_LEFT | DT_VCENTER);

    // 行2：来源 · 时间 · 字符数
    std::wstring src = (e.source == L"Manual") ? L"手动录入" : e.source;
    std::wstring meta = src + L" · " + FormatRelTime(e.createdAt) +
                        L" · " + std::to_wstring((long)e.content.size()) +
                        L" 字符";
    RECT rcLine2 = { tx, card.top + 31, card.right - 12, card.top + 52 };
    SelectObject(dc, m_fontMeta);
    SetTextColor(dc, theme::TextFaint);
    DrawTextW(dc, meta.c_str(), (int)meta.size(),
              &rcLine2, DT_SINGLELINE | DT_END_ELLIPSIS | DT_LEFT | DT_VCENTER);

    // 悬停操作钮：复制 / 备注 / 删除
    if (hovered) {
        const wchar_t* ops[3] = { L"\xE8C8", L"\xE70F", L"\xE74D" };
        int bx = card.right - 12 - 3 * 26 - 2 * 6;
        for (int i = 0; i < 3; ++i) {
            RECT rb = { bx, card.top + 15, bx + 26, card.top + 41 };
            FillRoundRect(dc, rb, RGB(242, 243, 245), 6);
            SelectObject(dc, m_fontIcon);
            SetTextColor(dc, (i == 2) ? RGB(229, 72, 77)
                                      : RGB(90, 98, 112));
            DrawTextW(dc, ops[i], -1, &rb,
                      DT_SINGLELINE | DT_CENTER | DT_VCENTER);
            bx += 32;
        }
    }
}

// ---------- 主按钮（Primary #1677FF） ----------
void App::DrawSaveButton(const DRAWITEMSTRUCT* dis)
{
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;

    COLORREF fill = theme::Primary;
    if (dis->itemState & ODS_SELECTED)      fill = theme::PrimaryDown;
    else if (m_btnHover)                    fill = theme::PrimaryHover;

    FillRoundRect(dc, rc, fill, 6);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    HFONT old = (HFONT)SelectObject(dc, m_fontBtn);
    wchar_t label[32];
    GetWindowTextW(dis->hwndItem, label, 32);
    RECT rcText = rc;
    DrawTextW(dc, label, -1, &rcText, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    SelectObject(dc, old);
}

// ---------- 列表子类化：hover 高亮 + 空状态引导 ----------
LRESULT CALLBACK App::ListProc(HWND hwnd, UINT msg, WPARAM wParam,
                               LPARAM lParam, UINT_PTR idSub, DWORD_PTR ref)
{
    auto* self = (App*)ref;
    switch (msg) {
    case WM_LBUTTONDOWN: {
        // 悬停操作钮（复制/备注/删除）与分组头的点击处理
        DWORD pos = (DWORD)SendMessageW(hwnd, LB_ITEMFROMPOINT,
                                        0, lParam);
        int item = HIWORD(pos) ? -1 : LOWORD(pos);
        if (item >= 0 && item < (int)self->m_rows.size()) {
            if (self->m_rows[item].header)
                return 0;   // 分组头不可选
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT rc, card;
            SendMessageW(hwnd, LB_GETITEMRECT, item, (LPARAM)&rc);
            card = { rc.left + 2, rc.top + 3, rc.right - 6, rc.bottom - 9 };
            int bx = card.right - 12 - 3 * 26 - 2 * 6;
            for (int op = 0; op < 3; ++op) {
                RECT rb = { bx, card.top + 15, bx + 26, card.top + 41 };
                if (PtInRect(&rb, pt)) {
                    self->OnListOp((int)self->m_rows[item].entry, op);
                    return 0;
                }
                bx += 32;
            }
        }
        break;   // 其余交给列表默认选中逻辑
    }
    case WM_MOUSEMOVE: {
        DWORD pos = (DWORD)SendMessageW(hwnd, LB_ITEMFROMPOINT,
                                        0, lParam);
        int item = HIWORD(pos) ? -1 : LOWORD(pos);
        if (item != self->m_hoverItem) {
            int old = self->m_hoverItem;
            self->m_hoverItem = item;
            if (old >= 0) {
                RECT rc;
                SendMessageW(hwnd, LB_GETITEMRECT, old, (LPARAM)&rc);
                InvalidateRect(hwnd, &rc, TRUE);
            }
            if (item >= 0) {
                RECT rc;
                SendMessageW(hwnd, LB_GETITEMRECT, item, (LPARAM)&rc);
                InvalidateRect(hwnd, &rc, TRUE);
            }
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        break;
    }
    case WM_MOUSELEAVE: {
        int old = self->m_hoverItem;
        self->m_hoverItem = -1;
        if (old >= 0) {
            RECT rc;
            SendMessageW(hwnd, LB_GETITEMRECT, old, (LPARAM)&rc);
            InvalidateRect(hwnd, &rc, TRUE);
        }
        break;
    }
    case WM_PAINT: {
        // 空状态：三步引导（居中、克制）
        if (SendMessageW(hwnd, LB_GETCOUNT, 0, 0) == 0) {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(dc, &rc, self->m_brBg);
            SetBkMode(dc, TRANSPARENT);

            const wchar_t* title = L"暂无记录";
            const wchar_t* steps =
                L"①  复制任意文字 —— 自动存入这里\n"
                L"②  双击任意条目 —— 复制回剪贴板\n"
                L"③  点击右上角「＋ 手动录入」—— 添加并写备注";
            const wchar_t* foot = L"关闭窗口后仍在后台记录 · Ctrl+Shift+V 随时呼出";

            RECT rcT = rc; rcT.bottom = rcT.top + rcT.bottom / 2 - 40;
            SetTextColor(dc, theme::Text);
            SelectObject(dc, self->m_fontSection);
            DrawTextW(dc, title, -1, &rcT,
                      DT_SINGLELINE | DT_CENTER | DT_VCENTER);

            RECT rcS = rcT; rcS.top = rcT.bottom + 4; rcS.bottom = rcS.top + 90;
            SetTextColor(dc, theme::TextSub);
            SelectObject(dc, self->m_font);
            DrawTextW(dc, steps, -1, &rcS,
                      DT_CENTER | DT_VCENTER | DT_EXTERNALLEADING);

            RECT rcF = rc; rcF.top = rcS.bottom + 6;
            SetTextColor(dc, theme::TextFaint);
            SelectObject(dc, self->m_fontMeta);
            DrawTextW(dc, foot, -1, &rcF, DT_SINGLELINE | DT_CENTER);
            EndPaint(hwnd, &ps);
            return 0;
        }
        break;
    }
    case WM_CONTEXTMENU: {
        // 右键菜单：条目操作（复制/编辑备注/删除）
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int item = -1;
        if (pt.x != -1 || pt.y != -1) {
            POINT lpt = pt;
            ScreenToClient(hwnd, &lpt);
            DWORD pos = SendMessageW(hwnd, LB_ITEMFROMPOINT, 0,
                                     MAKELPARAM(lpt.x, lpt.y));
            item = HIWORD(pos) ? -1 : LOWORD(pos);
        } else {
            item = (int)SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
            if (item == LB_ERR) return 0;
            RECT rc;
            SendMessageW(hwnd, LB_GETITEMRECT, item, (LPARAM)&rc);
            MapWindowPoints(hwnd, nullptr, (LPPOINT)&rc, 2);
            pt = { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
        }
        // 列表行 → 条目下标（分组头无菜单）
        int entryIdx = -1;
        if (item >= 0 && item < (int)self->m_rows.size() &&
            !self->m_rows[item].header) {
            entryIdx = (int)self->m_rows[item].entry;
            SendMessageW(hwnd, LB_SETCURSEL, item, 0);
            RECT rcD = self->m_rcDetail;
            InvalidateRect(self->m_hwnd, &rcD, FALSE);
        }
        self->ShowItemMenu(pt, entryIdx);
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &App::ListProc, idSub);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK App::EditPlaceholderProc(HWND hwnd, UINT msg, WPARAM wParam,
                                          LPARAM lParam, UINT_PTR idSub,
                                          DWORD_PTR ref)
{
    auto* self = (App*)ref;
    // 备注编辑态：Esc 取消（吞掉默认提示音）
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE &&
        GetDlgCtrlID(hwnd) == IDC_NOTE_EDIT && self->m_editingId) {
        self->CancelNoteEdit();
        SetWindowTextW(hwnd, L"");
        return 0;
    }
    // 表单卡展开：Esc 收起
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE &&
        GetDlgCtrlID(hwnd) == IDC_MANUAL_EDIT && self->m_formOpen) {
        self->ToggleForm(false);
        return 0;
    }

    // IME 组词期间：编辑框内文本长度仍为 0，若照常画占位字会压在拼音候选字上面。
    // 用窗口属性标记组词态，并在组词结束时强制重绘一次。
    if (msg == WM_IME_STARTCOMPOSITION) {
        SetPropW(hwnd, L"AireRecPhComp", (HANDLE)1);
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
    if (msg == WM_IME_ENDCOMPOSITION) {
        RemovePropW(hwnd, L"AireRecPhComp");
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return r;
    }

    // 焦点变化：强制重绘一次，使占位提示在“获得/失去焦点”时正确切换
    // （失去焦点后 GetFocus()!=hwnd，WM_PAINT 才会画占位字；否则可能停留在空焦点态）
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    if (msg == WM_PAINT) {
        // 先让系统把编辑框本身（背景/文本/光标）画好，再在其上叠加占位提示，
        // 这样占位字永远只会出现在“内容为空”的前提下，绝不与已输入文字重叠。
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);

        // 占位提示：仅当“未聚焦 + 内容为空 + 不在 IME 组词中”时绘制。
        // 聚焦（输入 / IME 组词必在聚焦态）时一律不画，从根本上杜绝“输入时仍透出
        // 占位字”以及多行框占位字压在拼音候选字上的问题；多行框用
        // DT_WORDBREAK|DT_EDITCONTROL 跟随编辑框换行并裁剪在框内，不溢出框外“下方”。
        // 搜索框右侧的 Ctrl+Shift+V 标签已改为独立置顶子窗口，天然不会被本占位字覆盖。
        bool composing = GetPropW(hwnd, L"AireRecPhComp") != nullptr;
        if (!composing && GetWindowTextLengthW(hwnd) == 0 &&
            GetFocus() != hwnd) {
            const wchar_t* ph = nullptr;
            switch (GetDlgCtrlID(hwnd)) {
            case IDC_SEARCH_EDIT: ph = L"搜索内容和备注…"; break;
            case IDC_MANUAL_EDIT: ph = L"输入要保存的内容…"; break;
            case IDC_NOTE_EDIT:   ph = L"备注（可选）";     break;
            }
            if (ph) {
                HDC dc = GetDC(hwnd);
                RECT rc;
                GetClientRect(hwnd, &rc);
                bool multiline =
                    (GetWindowLongW(hwnd, GWL_STYLE) & ES_MULTILINE) != 0;
                rc.left += 9;
                rc.right -= 9;
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, theme::TextFaint);
                SelectObject(dc, self->m_font);
                // 真实文字由系统绘制：单行 EDIT 恒定贴在客户区顶部（不是垂直居中），
                // 多行 EDIT 首行同样在顶部。故占位字一律用 DT_TOP 顶对齐，
                // 与真实文字落在完全相同的基线上，避免"空态居中 / 有字贴顶"的错位。
                UINT fmt = DT_LEFT | DT_NOPREFIX | DT_TOP;
                if (multiline)
                    fmt |= DT_WORDBREAK | DT_EDITCONTROL | DT_NOFULLWIDTHCHARBREAK;
                else
                    fmt |= DT_SINGLELINE;
                DrawTextW(dc, ph, -1, &rc, fmt);
                ReleaseDC(hwnd, dc);
            }
        }
        return r;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ---------- 搜索框右侧 Ctrl+Shift+V 快捷键提示（独立置顶子窗口） ----------
// 作为独立窗口始终绘制在搜索框之上，彻底避免被编辑框 hover/焦点重绘覆盖；
// 对鼠标透明（HTTRANSPARENT），点击/悬停穿透到下方编辑框。
LRESULT CALLBACK App::SearchChipProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam, UINT_PTR idSub,
                                     DWORD_PTR ref)
{
    if (msg == WM_NCHITTEST) {
        // 命中测试透明：鼠标事件交给下方编辑框处理
        LRESULT r = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (r == HTCLIENT) return HTTRANSPARENT;
        return r;
    }
    if (msg == WM_ERASEBKGND) {
        // 自绘覆盖整块区域，无需擦背景（避免闪烁）
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        // 圆角底（与搜索框右侧留出 1px 间隙，避免贴边）
        RECT r = { rc.left + 1, rc.top, rc.right - 1, rc.bottom };
        FillRoundRect(dc, r, RGB(237, 239, 242), 4);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(214, 218, 224));
        HPEN oldPen = (HPEN)SelectObject(dc, pen);
        HBRUSH oldBr = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, r.left, r.top, r.right + 1, r.bottom + 1, 4, 4);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBr);
        DeleteObject(pen);
        // 文字
        auto* self = (App*)ref;
        const wchar_t* key = L"Ctrl+Shift+V";
        SelectObject(dc, self->m_fontMeta);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(110, 116, 125));
        SIZE sz;
        GetTextExtentPoint32W(dc, key, 12, &sz);
        TextOutW(dc, r.left + 6, r.top + (r.bottom - r.top) / 2 - sz.cy / 2,
                 key, 12);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ---------- 保存按钮子类化（hover 跟踪） ----------
LRESULT CALLBACK App::SaveBtnProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam, UINT_PTR idSub, DWORD_PTR ref)
{
    auto* self = (App*)ref;
    switch (msg) {
    case WM_MOUSEMOVE:
        if (!self->m_btnHover) {
            self->m_btnHover = true;
            InvalidateRect(hwnd, nullptr, TRUE);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        break;
    case WM_MOUSELEAVE:
        self->m_btnHover = false;
        InvalidateRect(hwnd, nullptr, TRUE);
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, &App::SaveBtnProc, idSub);
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    App* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lParam;
        self = (App*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->m_hwnd = hwnd;
    } else {
        self = (App*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);
    return self->HandleMessage(hwnd, msg, wParam, lParam);
}

LRESULT App::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 1) 先交给插件分发（F3 搜索防抖 / F4 手动录入）
    LRESULT result = 0;
    if (m_plugins.DispatchWndProc(*this, hwnd, msg, wParam, lParam, result))
        return result;

    switch (msg) {
    case WM_CREATE:
        CreateUI(hwnd);
        return 0;

    // ---- 无边框壳：去掉系统 NC 区（保留 WS_THICKFRAME 的缩放/吸附能力） ----
    case WM_ERASEBKGND:
        // 全自绘窗口：背景在 WM_PAINT 内存缓冲中整体填充，
        // 禁用系统背景擦除（这是切页"闪一下"的主要来源）
        return 1;

    case WM_NCCALCSIZE:
        if (!wParam) break;
        if (IsZoomed(hwnd)) {   // 最大化时收回出屏的边框
            auto* r = (RECT*)lParam;
            int fw = GetSystemMetrics(SM_CXSIZEFRAME) +
                     GetSystemMetrics(SM_CXPADDEDBORDER);
            int fh = GetSystemMetrics(SM_CYSIZEFRAME) +
                     GetSystemMetrics(SM_CXPADDEDBORDER);
            r->left += fw; r->top += fh; r->right -= fw; r->bottom -= fh;
        }
        return 0;   // 客户区 = 整个窗口
    case WM_NCACTIVATE:
        return DefWindowProcW(hwnd, msg, wParam, -1);

    case WM_NCHITTEST: {
        LRESULT ht = DefWindowProcW(hwnd, msg, wParam, lParam);
        if (ht != HTCLIENT) return ht;      // 边缘缩放交给系统
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        ScreenToClient(hwnd, &pt);
        if (CaptionButtonAt(pt)) return HTCLIENT;  // 三键按客户区点击处理
        if (pt.y >= 0 && pt.y < kTitleH) return HTCAPTION; // 拖动/双击最大化/吸附
        return HTCLIENT;
    }

    case WM_MOUSEMOVE: {
        // 标题栏三键 hover
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        int btn = CaptionButtonAt(pt);
        if (btn != m_capHover) {
            m_capHover = btn;
            RECT rc;
            GetClientRect(hwnd, &rc);
            rc.bottom = kTitleH;
            InvalidateRect(hwnd, &rc, FALSE);
        }
        // 侧栏导航 hover
        int nav = (pt.y >= kTitleH && pt.x < kSideW) ? NavItemAt(pt) : -1;
        if (nav != m_navHover) {
            m_navHover = nav;
            InvalidateRect(hwnd, &m_rcSide, FALSE);
        }
        if (btn || nav >= 0) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        break;
    }
    case WM_MOUSELEAVE:
        if (m_capHover) {
            m_capHover = 0;
            RECT rc;
            GetClientRect(hwnd, &rc);
            rc.bottom = kTitleH;
            InvalidateRect(hwnd, &rc, FALSE);
        }
        if (m_navHover >= 0) {
            m_navHover = -1;
            InvalidateRect(hwnd, &m_rcSide, FALSE);
        }
        break;
    case WM_LBUTTONDOWN: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        // 侧栏导航点击 → 页面切换 / 操作
        if (pt.x < kSideW && pt.y >= kTitleH) {
            OnNavClick(NavItemAt(pt));
            return 0;
        }
        // 总览页：点击模块卡进入模块（当前仅剪贴板已启用）
        if (m_page == PageOverview) {
            if (PtInRect(&m_rcModCard[0], pt)) {
                SwitchPage(PageClips);
                RefreshList(GetSearchText());
            }
            return 0;
        }
        // 剪贴板页：类型胶囊 / ＋手动录入 / 表单收起 / 详情按钮
        if (m_page == PageClips) {
            // 搜索框 / 备注框：可见外框高于 EDIT 客户区（EDIT 只占中间一行文字高），
            // 留白区点击也要能进入输入状态 → 转发给编辑框
            if (m_rcSearchBox.right > m_rcSearchBox.left &&
                PtInRect(&m_rcSearchBox, pt)) {
                ForwardClickToEdit(m_hSearch, m_font, pt);
                return 0;
            }
            if (m_rcNoteBox.right > m_rcNoteBox.left &&
                PtInRect(&m_rcNoteBox, pt)) {
                ForwardClickToEdit(m_hNoteEdit, m_font, pt);
                return 0;
            }
            for (int i = 0; i < 4; ++i) {
                if (PtInRect(&m_rcChips[i], pt)) {
                    m_typeFilter = i;
                    RefreshList(GetSearchText());
                    InvalidateRect(hwnd, nullptr, FALSE);  // 胶囊选中态重绘
                    return 0;
                }
            }
            if (PtInRect(&m_rcAddBtn, pt)) {
                ToggleForm(true);
                return 0;
            }
            if (m_formOpen && PtInRect(&m_rcFormClose, pt)) {
                ToggleForm(false);
                return 0;
            }
            int db = DetailButtonAt(pt);
            if (db) {
                if (m_selEntry >= 0)
                    OnListOp(m_selEntry, db - 1);
                return 0;
            }
            // 详情面板"备注"框：点击就地编辑
            if (m_selEntry >= 0) {
                RECT nb = DetailNoteBox(m_rcDetail);
                if (PtInRect(&nb, pt)) {
                    OnListOp(m_selEntry, 1);
                    return 0;
                }
            }
        }
        int btn = CaptionButtonAt(pt);
        if (btn == 1) PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        else if (btn == 2) PostMessageW(hwnd, WM_SYSCOMMAND,
                            IsZoomed(hwnd) ? SC_RESTORE : SC_MAXIMIZE, 0);
        else if (btn == 3) PostMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
        return 0;
    }

    case WM_PAINT: {
        // 双缓冲：整体绘入内存位图后一次性 BitBlt，消除切页/交互闪烁
        PAINTSTRUCT ps;
        HDC wdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int W = rc.right, H = rc.bottom;
        HDC dc = CreateCompatibleDC(wdc);
        HBITMAP bmp = CreateCompatibleBitmap(wdc, W, H);
        HGDIOBJ oldBmp = SelectObject(dc, bmp);

        FillRect(dc, &rc, m_brBg);
        DrawTitleBar(dc, rc);
        DrawSidebar(dc, rc);

        // ---- 总览 / 配置 / 剪贴板页 ----
        if (m_page == PageOverview) {
            DrawOverviewPage(dc, rc);
        } else if (m_page == PageConfig) {
            DrawConfigPage(dc, rc);
        } else {
            DrawClipsPage(dc, rc);
        }

        // 底部状态栏（所有页面）
        DrawStatusBar(dc, rc);

        // 一次性上屏
        BitBlt(wdc, 0, 0, W, H, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBmp);
        DeleteObject(bmp);
        DeleteDC(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        Layout();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_GETMINMAXINFO: {
        auto* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 780;
        mmi->ptMinTrackSize.y = 560;
        return 0;
    }

    case WM_SETFOCUS:
        if (m_page == PageClips) SetFocus(m_hSearch);
        return 0;

    // ---- 控件配色 ----
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, theme::Text);
        SetBkColor(dc, theme::Surface);
        return (LRESULT)m_brWhite;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wParam;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, theme::TextSub);
        return (LRESULT)m_brBg;
    }
    case WM_CTLCOLORLISTBOX:
        // 列表底色 = 页面灰（条目自绘白卡片浮于其上）
        return (LRESULT)m_brBg;

    // ---- Owner-draw ----
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT*)lParam)->itemHeight = 54;
        return TRUE;
    case WM_DRAWITEM: {
        auto* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlID == IDC_LIST) { DrawListItem(dis); return TRUE; }
        if (dis->CtlID == IDC_SAVE_BUTTON) { DrawSaveButton(dis); return TRUE; }
        break;
    }

    case WM_COMMAND: {
        int ctrlId = LOWORD(wParam), code = HIWORD(wParam);
        m_plugins.DispatchCommand(*this, ctrlId, code);

        // 列表选中 → 详情面板联动
        if (ctrlId == IDC_LIST && code == LBN_SELCHANGE) {
            int sel = (int)SendMessageW(m_hList, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR && sel < (int)m_rows.size() &&
                !m_rows[sel].header) {
                m_selEntry = (int)m_rows[sel].entry;
                RECT rcD = m_rcDetail;
                InvalidateRect(hwnd, &rcD, FALSE);
            }
            return 0;
        }

        // 双击列表项 → 请求服务端复制（服务端过滤自记录，防止循环入库）
        if (ctrlId == IDC_LIST && code == LBN_DBLCLK) {
            int sel = (int)SendMessageW(m_hList, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR && sel < (int)m_rows.size() &&
                !m_rows[sel].header)
                m_ipc.Request(L"COPY\t" +
                    ipc::EscapeField(m_entries[m_rows[sel].entry].content));
        }
        return 0;
    }

    case WM_APP + 2:
        // 服务端有新记录入库 → 防抖后刷新（合并连续复制的多次通知）
        SetTimer(hwnd, TIMER_REFRESH_NOTIFY, 300, nullptr);
        return 0;

    case WM_TIMER:
        m_plugins.DispatchTimer(*this, (UINT_PTR)wParam);
        if ((UINT_PTR)wParam == TIMER_SYSINFO) {
            // 配置页动态数据（CPU/内存）2s 轮询
            if (m_page == PageConfig) {
                m_sys = sysinfo::Collect();
                InvalidateRect(hwnd, nullptr, FALSE);
            } else {
                KillTimer(hwnd, TIMER_SYSINFO);
            }
            return 0;
        }
        if ((UINT_PTR)wParam == TIMER_REFRESH_NOTIFY) {
            KillTimer(hwnd, TIMER_REFRESH_NOTIFY);
            RefreshList(GetSearchText());
            RefreshStats();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
