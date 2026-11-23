#pragma once
// ============================================================
// AirUI — Widgets.h
// 控件包装：TextBox/ListBox 自绘（不再依赖 Win32 EDIT/LISTBOX 的行为），
// HWND + SetWindowSubclass 只保留三件事：焦点与生命周期、WM_GETOBJECT
// 转发、消息级探测（WM_GETTEXT/LB_GETCOUNT 仍被应答；窗口类名为
// AirUITextBox/AirUIListBox，test_ui 的探测端按 Edit/ListBox 别名查找）。
// 页面通过高级接口操作控件。
//
//   TextBox& search = CreateTextBox({ .id = 1, .placeholder = L"搜索…" });
//   search.Text();
//   search.OnKey([](UINT vk, bool ctrl, bool shift) { ... });
//
// 真正需要原生句柄的极端场景走 Native()（集成层专用，
// 页面代码不应出现）。
// ============================================================
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include "airui/Controls.h"
#include "airui/Canvas.h"
#include "airui/FrameworkCore.h"   // TextEditState/TextLines/IClipboard

namespace air {
namespace ui {

class Shell;
class Page;

// 内部窗口过程（Widgets.cpp 中定义；设为友元以访问控件内部状态）
LRESULT CALLBACK TextBoxProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK ListBoxProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK CustomProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

// ============================================================
// Widget —— 所有控件的基类
// ============================================================
class Widget
{
public:
    virtual ~Widget();

    void Show(bool visible);
    bool Visible() const;
    void Move(int x, int y, int w, int h);
    void Move(const RECT& rc);
    void Focus();
    RECT Rect() const;                 // 父窗口客户区坐标
    int  Id() const { return m_id; }
    HWND Native() const { return m_hwnd; }   // 逃生舱（集成层专用）

protected:
    friend class Page;
    friend class Shell;
    Widget() = default;

    // 由 Page 在 Init 之后调用：创建原生窗口 + 安装内部子类化
    virtual void CreateNative() = 0;

    virtual bool HandleCommand(int code) { (void)code; return false; }
    virtual bool HandleDrawItem(const DRAWITEMSTRUCT* dis) { (void)dis; return false; }
    virtual bool HandleMeasureItem(MEASUREITEMSTRUCT* mis) { (void)mis; return false; }

    void Init(Shell* shell, int id);

    Shell* m_shell = nullptr;
    HWND   m_hwnd  = nullptr;
    int    m_id    = 0;
};

// ============================================================
// TextBox —— 单行 / 多行输入框（内置占位提示与按键回调）
// ============================================================
struct TextBoxDesc
{
    int          id          = 0;
    bool         multiline   = false;
    std::wstring placeholder;                 // 空 = 无占位提示
    FontRole     fontRole    = FontRole::Body;
};

class TextBox : public Widget
{
public:
    std::wstring Text() const;
    void SetText(const std::wstring& s);
    void Clear();
    void CaretToEnd();
    void SetPlaceholder(const std::wstring& s);

    // 按键回调：返回 true 表示已消费（Esc 取消 / Ctrl+Enter 提交…）
    void OnKey(std::function<bool(UINT vk, bool ctrl, bool shift)> cb);
    // 右侧留白（给快捷键 chip 等让位）
    void SetRightInset(int px);
    // 视觉外框点击 → 进入输入态并按 x 定位光标
    void ClickAt(POINT parentPt);

    // ---- 自绘内部（TextBoxProc 是 friend，直接驱动这些） ----
    void SyncTextLayout();          // 文本/尺寸变化时重建 cum/lines
    void PaintSelf(HDC dc);         // WM_PAINT：单行/多行均走 m_cum 逐字宽度表
    void CaretAtClientPoint(POINT); // 客户区点击 → 光标/扩选 + 聚焦
    // 插入点在**文本区**（textRc）内的相对坐标（不含 unit 内边距；
    // 多行已减去 m_scroll）。调用方：PaintSelf（+textRc 原点）、
    // SyncIme（+unit 还原为客户区坐标给 ImmSet*Window）。
    POINT CaretPoint() const;
    void ScrollForCaret();          // 多行：caret 行滚进视口
    void SyncIme();                 // 组词串/候选窗跟随插入点
    void InsertChar(wchar_t ch);    // WM_CHAR 路径（表 6：中文多行先对齐 caret）
    void InsertText(const std::wstring& s);  // IME 结果 / 菜单粘贴
    bool DefaultKey(UINT vk, bool ctrl, bool shift);   // 编辑语义键
    void ApplyEditResult(EditAction action, const std::wstring& arg,
                         IClipboard* clip);            // 统一后处理入口
    void ApplyPostEdit();                             // 滚动/IME/重绘
    int  MaxScroll() const;
    void RestartCaretBlink();                         // 聚焦/光标移动时重置闪烁相位
    void StopCaretBlink();                            // 失焦：停闪烁定时器

protected:
    friend class Page;
    friend LRESULT CALLBACK TextBoxProc(HWND, UINT, WPARAM, LPARAM,
                                        UINT_PTR, DWORD_PTR);
    void CreateNative() override;
    bool HandleKey(UINT vk, bool ctrl, bool shift);

    TextBoxDesc m_desc;
    std::function<bool(UINT, bool, bool)> m_keyCb;

    TextEditState m_edit;            // core 引擎：文本/光标/选区/组词
    std::vector<int> m_cum;          // 逐字累计宽度表（比例字体）
    TextLines        m_lines;       // 多行折行结果
    int  m_layW = -1, m_layH = -1;   // 布局缓存的客户区尺寸（变了才重建）
    int  m_lineH = 21, m_ascent = 16;
    int  m_scroll = 0;               // 多行垂直滚动（px）
    int  m_rightInset = 0;           // 右侧留白（快捷键 chip 让位）
    bool m_composing  = false;      // IME 组词中（吞掉重复 WM_CHAR）
    bool m_caretOn    = true;       // 插入符闪烁相位（原生 EDIT 的系统 caret 由我们自管）
    HFONT m_font = nullptr;
};

// ============================================================
// Button —— 按钮（库内统一绘制，页面只需给文本与回调）
// ============================================================
struct ButtonDesc
{
    int          id    = 0;
    std::wstring text;
    BtnStyle     style = BtnStyle::Primary;
};

class Button : public Widget
{
public:
    void SetText(const std::wstring& s);
    std::wstring Text() const;
    void OnClick(std::function<void()> cb);
    void SetStyle(BtnStyle style);

protected:
    friend class Page;
    void CreateNative() override;
    bool HandleCommand(int code) override;
    bool HandleDrawItem(const DRAWITEMSTRUCT* dis) override;

    ButtonDesc m_desc;
    std::function<void()> m_click;
    BtnStyle m_style = BtnStyle::Primary;
};

// ============================================================
// ListBox —— owner-draw 列表（hover / 右键 / 空状态全部内置）
// ============================================================
struct ListBoxDesc
{
    int id = 0;
    // WM_MEASUREITEM 的兜底行高；逐项行高在 AddItem 时指定
    int fallbackItemHeight = 54;
};

// 传给 onPaintItem 的一行绘制信息（替代 DRAWITEMSTRUCT）
struct ListItemDraw
{
    int    index    = -1;    // 行下标
    size_t data     = 0;     // AddItem 附加的业务数据
    RECT   rc       = {};    // 行矩形（控件客户区坐标）
    bool   selected = false;
    bool   hovered  = false;
};

// 一行（自绘 ListBox 的内部模型；原生 LB_* 只保留给消息级探测）
struct ListBoxItem
{
    int    height = 0;
    size_t data   = 0;
};

class ListBox : public Widget
{
public:
    void Reset();
    int  AddItem(int height, size_t data = 0);   // 返回行下标

    // 批量重建：把 Reset() + N × AddItem() + SetSelection() 夹在中间，
    // 期间不失效（自绘列表的重建是纯内存操作），结束时只提交一帧。
    // 可嵌套，按引用计数逐个配对。
    void BeginUpdate();
    void EndUpdate();
    // RAII 版批量重建：构造时 BeginUpdate，析构时 EndUpdate。
    // 重建中间 return / 抛异常也会配对还原，不会把列表永远冻结住。
    class ScopedBatch {
    public:
        explicit ScopedBatch(ListBox* lb) : m_lb(lb) {
            if (m_lb) m_lb->BeginUpdate();
        }
        ~ScopedBatch() {
            if (m_lb) m_lb->EndUpdate();
        }
        ScopedBatch(const ScopedBatch&) = delete;
        ScopedBatch& operator=(const ScopedBatch&) = delete;
    private:
        ListBox* m_lb;
    };
    int  Count();
    int  Selection();
    void SetSelection(int index);
    RECT ItemRect(int index);
    void InvalidateItem(int index);
    void Invalidate();
    int  ItemAtPoint(POINT clientPt);
    int  HoverItem() const { return m_hover; }

    // ---- 页面按需设置的回调 ----
    std::function<void(Canvas&, const ListItemDraw&)> onPaintItem;
    std::function<void(Canvas&, const RECT&)>         onPaintEmpty;
    std::function<void(int index)>                    onSelectChanged;
    std::function<void(int index)>                    onDoubleClicked;
    // 行内点击：返回 true 表示已消费（不改变选中，如命中行内按钮 / 分组头）
    std::function<bool(int index, POINT ptInItem)>    onItemClicked;
    // 指针在同一行内移动（ptInItem 相对该行左上角）。LISTBOX 只会在换行
    // 时重画整行，行内那几个自绘图标按钮得靠这个回调知道自己被指着，
    // 否则一行的 hover 只有"图标出现了"和"图标消失了"两种状态。
    std::function<void(int index, POINT ptInItem)>    onHoverMove;
    // 右键菜单（screenPt 为屏幕坐标；index = -1 表示空白处）
    std::function<void(POINT screenPt, int index)>    onContextMenu;

protected:
    friend class Page;
    friend LRESULT CALLBACK ListBoxProc(HWND, UINT, WPARAM, LPARAM,
                                        UINT_PTR, DWORD_PTR);
    void CreateNative() override;
    bool HandleCommand(int code) override;

    // ---- 自绘内部 ----
    void PaintSelf(HDC dc);
    void PaintItem(Canvas& g, int index, const RECT& rc);
    void SelectItem(int index, bool notify);
    void EnsureVisible(int index);
    void SyncScrollBar();
    int  ContentHeight() const;
    int  VisibleHeight() const;
    int  ScrollStep() const;
    int  MaxScroll() const;

    ListBoxDesc m_desc;
    std::vector<ListBoxItem> m_items;
    int m_sel = -1;
    int m_scroll = 0;
    int m_hover = -1;
    int m_updateLock = 0;      // BeginUpdate/EndUpdate 嵌套层数
    int m_fallbackH = 54;
};

// ============================================================
// Custom —— 自绘小块（STATIC 基底）：快捷键提示 chip 等
// ============================================================
struct CustomDesc
{
    int  id = 0;
    bool mouseTransparent = true;   // 鼠标穿透到下层控件
};

class Custom : public Widget
{
public:
    void OnPaint(std::function<void(Canvas&, const RECT&)> painter);

protected:
    friend class Page;
    friend LRESULT CALLBACK CustomProc(HWND, UINT, WPARAM, LPARAM,
                                       UINT_PTR, DWORD_PTR);
    void CreateNative() override;

    CustomDesc m_desc;
    std::function<void(Canvas&, const RECT&)> m_painter;
};

} // namespace ui
} // namespace air
