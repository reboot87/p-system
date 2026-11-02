#pragma once
// ============================================================
// AirUI — Controls.h
// 通用控件与窗口绘制辅助
//
//   PlaceholderEdit  给任意 EDIT 加占位提示（含 IME / 焦点门控）
//   DrawButton       主/次/危险按钮的统一外观
//   AttachButton     OWNERDRAW 按钮的 hover / pressed 跟踪
//   NavBar           侧栏导航（分组 + 条目 + 角标 + hover）
//   PaintDC          双缓冲 WM_PAINT（整体绘入内存位图后一次上屏）
//   EmptyState       列表空状态的三段式引导
// ============================================================
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include "Theme.h"
#include "Fonts.h"

namespace air {
namespace ui {

// ============================================================
// PlaceholderEdit —— 占位提示
//
// Windows 的 EM_SETCUEBANNER 有两个硬限制：多行 EDIT 不支持、
// 且 IME 组词/输入过程中仍可能透出。本实现改为子类化自绘，规则：
//   · 仅当「内容为空 + 未聚焦 + 不在 IME 组词」时绘制
//   · 与真实文字同基线（DT_TOP），避免"空态居中 / 有字贴顶"错位
// ============================================================
class PlaceholderEdit
{
public:
    using KeyCallback = std::function<bool(HWND edit, UINT vk,
                                           bool ctrl, bool shift)>;

    static void Attach(HWND edit, const std::wstring& placeholder, HFONT font);
    static void Detach(HWND edit);
    static bool Attached(HWND edit);

    static void SetPlaceholder(HWND edit, const std::wstring& text);
    static void SetFont(HWND edit, HFONT font);

    // 按键回调（Esc 取消 / Ctrl+Enter 提交等）。返回 true 表示已消费。
    static void SetKeyCallback(HWND edit, KeyCallback cb);
};

// 输入框 8px 文字内边距（企业级输入规范）
void SetEditMargins(HWND edit, int left = 8, int right = 8);
// 清空 EDIT（EM_SETSEL + EM_REPLACESEL，多行单行均可靠）
void ClearEditText(HWND edit);
// 光标移到末尾（编辑已有内容时续写）
void MoveCaretToEnd(HWND edit);
// 读取 EDIT 文本（跨进程安全：走 WM_GETTEXT）
std::wstring GetEditText(HWND edit);

// 把父窗口坐标的点击转发给输入框：聚焦、按 x 定位光标。
// 用于「视觉外框比 EDIT 客户区高」的场景，点外框留白也能进入输入态。
void ForwardClickToEdit(HWND edit, HFONT font, POINT ptParent);

// ============================================================
// 按钮
// ============================================================
enum class BtnStyle
{
    Primary,    // 主色填充 + 白字
    Secondary,  // 白底 + 边框 + 深字
    Danger,     // 白底 + 红边 + 红字
    Ghost       // 透明底 + 主色字
};

void DrawButton(HDC dc, const RECT& rc, const wchar_t* text, HFONT font,
                BtnStyle style, bool hover, bool pressed, bool disabled = false);

// OWNERDRAW 按钮的 hover / pressed 跟踪。
// 用法：AttachButton(hBtn, BtnStyle::Primary)；
// 父窗口 WM_DRAWITEM 里查 IsButtonHover / IsButtonPressed 后调 DrawButton。
void AttachButton(HWND button, BtnStyle style);
void DetachButton(HWND button);
bool IsButtonHover(HWND button);
bool IsButtonPressed(HWND button);
BtnStyle ButtonStyleOf(HWND button);

// ============================================================
// 鼠标悬停跟踪（控件级）
// ============================================================
namespace hover {
// 在 WM_MOUSEMOVE / WM_MOUSELEAVE 中调用；返回 true 表示状态变化需重绘
bool OnMouseMove(HWND hwnd, bool& state);
bool OnMouseLeave(HWND hwnd, bool& state);
} // namespace hover

// ============================================================
// NavBar —— 侧栏导航
// ============================================================
struct NavItem
{
    int          id       = 0;      // 业务 id（页面 id 或命令 id）
    std::wstring glyph;             // Segoe MDL2 字形
    std::wstring label;
    std::wstring badge;             // 右侧角标文本（空 = 不显示）
    bool         badgeOk  = false;  // 角标使用「成功」配色
    bool         disabled = false;
};

struct NavGroup
{
    std::wstring         title;     // 分组标题（空 = 不显示标题行）
    std::vector<NavItem> items;
};

class NavBar
{
public:
    void SetGroups(std::vector<NavGroup> groups);
    // 侧栏区域（含标题栏以下的整块）。Shell 在 OnLayout 时调用。
    void Layout(const RECT& side);
    void Paint(HDC dc, const Fonts& fonts);
    // 命中条目 id；-1 = 无
    int  HitTest(POINT pt) const;
    // 悬停跟踪，返回 true 表示需重绘
    bool OnMouseMove(POINT pt);
    bool OnMouseLeave();

    void   SetActive(int id) { m_active = id; }
    int    Active() const { return m_active; }
    bool   IsDisabled(int id) const;
    // 更新角标（不重建分组）
    void   SetBadge(int id, const std::wstring& text, bool ok);
    // 内容总高（用于判断底部信息卡是否放得下）
    int    ContentHeight() const { return m_contentH; }
    RECT   Bounds() const { return m_side; }
    bool   Empty() const { return m_groups.empty(); }

private:
    struct ItemRect { int id; RECT rc; bool disabled; };
    struct GroupRect { std::wstring title; RECT rc; };

    void RebuildRects();

    std::vector<NavGroup>  m_groups;
    std::vector<ItemRect>  m_items;
    std::vector<GroupRect> m_groupsRect;
    RECT m_side        = {};
    int  m_active      = -1;
    int  m_hoverId     = -1;
    int  m_contentH    = 0;
};

// ============================================================
// 双缓冲绘制
// ============================================================
class PaintDC
{
public:
    explicit PaintDC(HWND hwnd);
    ~PaintDC();

    PaintDC(const PaintDC&) = delete;
    PaintDC& operator=(const PaintDC&) = delete;

    HDC         dc() const { return m_mem; }
    const RECT& client() const { return m_client; }
    int         width()  const { return m_client.right; }
    int         height() const { return m_client.bottom; }

    // 把内存位图整体上屏（只能调用一次；析构时若未调用会自动补一次空 EndPaint）
    void Flush();

private:
    HWND        m_hwnd = nullptr;
    PAINTSTRUCT m_ps   = {};
    HDC         m_wdc  = nullptr;
    HDC         m_mem  = nullptr;
    HBITMAP     m_bmp  = nullptr;
    HGDIOBJ     m_old  = nullptr;
    RECT        m_client = {};
    bool        m_began = false;
    bool        m_flushed = false;
};

// ============================================================
// 列表工具
// ============================================================
namespace list {
// 客户区坐标 → 列表项下标；-1 = 空白/越界
int ItemFromPoint(HWND listbox, POINT clientPt);
} // namespace list

struct EmptyState
{
    std::wstring title;   // 主标题
    std::wstring steps;   // 步骤（可含 \n 多行）
    std::wstring foot;    // 底部灰字
};

void DrawEmptyState(HDC dc, const RECT& rc, const Fonts& fonts,
                    const EmptyState& state);

// ============================================================
// 窗口外壳辅助
// ============================================================
// 给无边框窗口加 DWM 阴影 + Win11 圆角（Win10 上静默失败）
void ApplyDwmFrame(HWND hwnd);

} // namespace ui
} // namespace air
