#pragma once
// ============================================================
// AirUI — Widgets.h
// 控件包装：TextBox/ListBox 自绘（不再依赖 Win32 EDIT/LISTBOX 的行为），
// native subclassing only keeps three things: focus/lifetime, accessibility forwarding
// and message-level probing (native get-text/count still answered; class names are
// AirUITextBox/AirUIListBox; probes use Edit/ListBox aliases).
// 页面通过高级接口操作控件。
//
//   TextBox& search = CreateTextBox({ .id = 1, .placeholder = L"搜索…" });
//   search.Text();
//   search.OnKey([](VirtualKey vk, bool ctrl, bool shift) { ... });
//
// Integration-only escape hatch Native() returns an opaque handle (
// page code should not use it).
// ============================================================
#include <functional>
#include <string>
#include <vector>
#include "airui/Controls.h"
#include "airui/Canvas.h"
#include "airui/FrameworkCore.h"

namespace air {
namespace ui {

class Shell;
class Page;

// Internal window procedures (defined in the private implementation).
MessageResult TextBoxProc(WindowHandle, MessageId, MessageWParam, MessageLParam, SubclassId, SubclassRef);
MessageResult ListBoxProc(WindowHandle, MessageId, MessageWParam, MessageLParam, SubclassId, SubclassRef);
MessageResult CustomProc(WindowHandle, MessageId, MessageWParam, MessageLParam, SubclassId, SubclassRef);

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
    void Move(const Rect& rc);
    void Focus();
    Rect Bounds() const;               // parent client-area coordinates
    int  Id() const { return m_id; }
    WindowHandle Native() const { return m_hwnd; }   // 逃生舱（集成层专用）

protected:
    friend class Page;
    friend class Shell;
    Widget() = default;

    // 由 Page 在 Init 之后调用：创建原生窗口 + 安装内部子类化
    virtual void CreateNative() = 0;

    virtual bool HandleCommand(int code) { (void)code; return false; }
    virtual bool HandleDrawItem(const DrawItemInfo* dis) { (void)dis; return false; }
    virtual bool HandleMeasureItem(MeasureItemInfo* mis) { (void)mis; return false; }

    void Init(Shell* shell, int id);

    Shell* m_shell = nullptr;
    WindowHandle   m_hwnd  = nullptr;
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
    // max length (0 = unlimited). Insertion after typing/paste is enforced by core
    // InsertAtCursor; programmatic SetText truncates the same way.
    // counted in UTF-16 units (same as TextEditState::text.size()).
    int  maxLength = 0;
    // password box: draws a mask (core DisplayText), real text stays in
    // m_edit.text; Text() returns the real value (native-edit semantics),
    // caret/selection geometry and real text share one source.
    bool password  = false;
};

class TextBox : public Widget
{
public:
    std::wstring Text() const;
    void SetText(const std::wstring& s);
    void Clear();
    void CaretToEnd();
    void SetPlaceholder(const std::wstring& s);
    // 最大字符数（0 = 不限；负数按 0 处理）。创建后经 desc.maxLength
    // 设定，运行期也可改。约束作用于后续所有编辑路径（键入/粘贴/
    // 程序化 SetText），已有超长内容在下次编辑时被截断。
    void SetMaxLength(int maxLen);
    int  MaxLength() const { return m_desc.maxLength; }
    // password mask (from desc.password, togglable at runtime -> repaint).
    // the mask only affects painting: Text() still returns real text.
    void SetPassword(bool on);
    bool IsPassword() const { return m_desc.password; }

    // key callback: return true to consume (Esc cancel / Ctrl+Enter submit).
    void OnKey(std::function<bool(VirtualKey vk, bool ctrl, bool shift)> cb);
    // 右侧留白（给快捷键 chip 等让位）
    void SetRightInset(int px);
    // 视觉外框点击 → 进入输入态并按 x 定位光标
    void ClickAt(Point parentPt);

    // ---- self-paint internals (driven by the private window procedure) ----
    void SyncTextLayout();          // 文本/尺寸变化时重建 cum/lines
    void PaintSelf(DeviceContextHandle dc);         // paint: single/multi-line both use the per-char width table
    void CaretAtClientPoint(Point); // 客户区点击 → 光标/扩选 + 聚焦
    // caret position is relative to the text area (excluding unit padding;
    // multi-line already subtracts m_scroll). Callers: PaintSelf (+textRc origin),
    // SyncIme (+unit restores client coords for the IME API).
    Point CaretPoint() const;
    void ScrollForCaret();          // 多行：caret 行滚进视口
    void SyncIme();                 // composition/candidate window follows the caret
    void InsertChar(wchar_t ch);    // character-input path (CJK multi-line aligns the caret first)
    void InsertText(const std::wstring& s);  // IME result / menu paste
    bool DefaultKey(VirtualKey vk, bool ctrl, bool shift);   // 编辑语义键
    void ApplyEditResult(EditAction action, const std::wstring& arg,
                         IClipboard* clip);            // 统一后处理入口
    void ApplyPostEdit();                             // 滚动/IME/重绘
    int  MaxScroll() const;
    void RestartCaretBlink();                         // 聚焦/光标移动时重置闪烁相位
    void StopCaretBlink();                            // 失焦：停闪烁定时器
    // ---- 鼠标交互（点击 / 拖选 / 双击选词 / 右键菜单） ----
    int  HitIndex(Point pt);        // 客户区点 → 文本下标（点击/拖选/双击/右键共用）
    // 显示串 = password ? 掩码(• × N) : 真实文本。掩码与真实文本
    // 1:1 等长（core DisplayText），所以布局/命中/绘制全部走它，
    // 编辑/读取/变更通知 走 m_edit.text（真实）。
    const std::wstring& ShownText() const;
    void BeginDrag(Point pt);         // left down: place caret + remember anchor + capture pointer
    void DragTo(Point pt);            // drag-select (multi-line auto-scrolls near edges)
    void SelectWordAt(Point pt);      // double-click: select the word under the point
    void ShowEditMenu(Point screenPt);// 右键 / Shift+F10：撤销/重做/复制/剪切/粘贴
    void Undo();                     // 平台级撤销：恢复编辑前快照（Ctrl+Z / 菜单）
    void Redo();                     // 平台级重做：恢复被撤销的文本（Ctrl+Y 等 / 菜单）
    bool CanUndo()  const { return !m_undoStack.empty(); }
    bool CanRedo()  const { return !m_redoStack.empty(); }

protected:
    friend class Page;
    friend MessageResult TextBoxProc(WindowHandle, MessageId, MessageWParam, MessageLParam, SubclassId, SubclassRef);
    void CreateNative() override;
    bool HandleKey(VirtualKey vk, bool ctrl, bool shift);

    TextBoxDesc m_desc;
    std::function<bool(MessageId, bool, bool)> m_keyCb;

    TextEditState m_edit;            // core 引擎：文本/光标/选区/组词
    std::wstring m_lastNotified;     // 已发变更通知的文本内容（纯光标/选区移动不重复通知）

    // ---- 撤销 / 重做（平台级）----
    // 核心引擎保持纯函数：TextBox 在**可能改文本**的动作执行前把整个
    // TextEditState 快照进 m_undoStack（文本/光标/选区一起还原），
    // 被撤销的状态进 m_redoStack（重做后丢弃）。快照只进不改，
    // text-changing and caret-only operations differ on m_lastNotified
    // 天然可区分——纯移动不产生新条目。SetText / programmatic set is
    // programmatic reset clears both stacks (native-edit semantics).
    std::vector<air::ui::TextEditState> m_undoStack;
    std::vector<air::ui::TextEditState> m_redoStack;
    mutable std::wstring m_shown;    // 显示串缓存（password 时 = 掩码；与 m_edit.text 同步重建）
    std::wstring m_laidShown;        // 上次布局所用的显示串（判据：同串+同尺寸才复用 m_cum）
    std::vector<int> m_cum;          // 逐字累计宽度表（比例字体，按显示串测）
    TextLines        m_lines;       // 多行折行结果（按显示串折）
    int  m_layW = -1, m_layH = -1;   // 布局缓存的客户区尺寸（变了才重建）
    int  m_layFontDpi = -1;          // 上次布局所用的字体 DPI（DPI 变了字形
                                     // 宽度全变，即使客户区尺寸不变也要重测）
    int  m_lineH = 21, m_ascent = 16;
    int  m_scroll = 0;               // 多行垂直滚动（px）
    int  m_rightInset = 0;           // 右侧留白（快捷键 chip 让位）
    bool m_composing  = false;      // IME composing (swallow duplicate char events)
    bool m_caretOn    = true;       // caret blink phase (the native system caret is managed here)
    bool m_dragging   = false;      // 左键按住拖动选区中
    int  m_dragAnchor = -1;         // 拖选起点（全局文本下标）
    FontHandle m_font = nullptr;
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
    bool HandleDrawItem(const DrawItemInfo* dis) override;

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
    // Fallback row height; per-item height is set in AddItem
    int fallbackItemHeight = 54;
    // multi-select (runtime SetMultiSelect). When on, click semantics:
    //   普通点击  = 单选（清空其他、锚点=该行）
    //   Ctrl+点击 = 切换该行（锚点保持）
    //   Shift+点击= 加选该行与锚点之间的区间
    // Modifier bits come from the message word: real mouse events and
    // cross-process posted events both carry them (no key-state polling).
    bool multiSelect = false;
};

// Per-row draw info passed to onPaintItem (platform-neutral).
struct ListItemDraw
{
    int    index        = -1;    // 行下标
    size_t data         = 0;     // AddItem 附加的业务数据
    Rect   rc           = {};    // 行矩形（控件客户区坐标）
    bool   selected     = false;
    bool   hovered      = false;
    bool   multiSelected = false; // 多选模式下：行在选择集中（== selected，双写供页按需用）
};

// One row (internal model; native list only kept for message probing).
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

    // batch rebuild: bracket Reset() + N x AddItem() + SetSelection(),
    // no invalidation in between (pure in-memory), commit one frame at the end.
    // 可嵌套，按引用计数逐个配对。
    void BeginUpdate();
    void EndUpdate();
    // RAII batch rebuild: BeginUpdate on construct, EndUpdate on destroy.
    // early return / exception still unpairs; the list is never frozen.
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
    Rect ItemRect(int index);
    void InvalidateItem(int index);
    void Invalidate();
    int  ItemAtPoint(Point clientPt);
    int  HoverItem() const { return m_hover; }

    // ---- 多选 API（仅 multiSelect 为 true 时有意义；调用方可按模式探询）----
    void             SetMultiSelect(bool on);
    bool             IsMultiSelect() const { return m_desc.multiSelect; }
    bool             IsSelected(int index) const;
    int              MultiSelectionCount() const;
    std::vector<int> SelectedItems() const;
    void             ClearSelection();
    // Lightweight range op for message probing (1 = add range, 0 = remove).
    void             SetSelectionRange(int start, int end, bool add);

    // ---- 页面按需设置的回调 ----
    std::function<void(Canvas&, const ListItemDraw&)> onPaintItem;
    std::function<void(Canvas&, const Rect&)>         onPaintEmpty;
    std::function<void(int index)>                    onSelectChanged;
    std::function<void(int index)>                    onDoubleClicked;
    // 行内点击：返回 true 表示已消费（不改变选中，如命中行内按钮 / 分组头）
    std::function<bool(int index, Point ptInItem)>    onItemClicked;
    // 指针在同一行内移动（ptInItem 相对该行左上角）。LISTBOX 只会在换行
    // 时重画整行，行内那几个自绘图标按钮得靠这个回调知道自己被指着，
    // 否则一行的 hover 只有"图标出现了"和"图标消失了"两种状态。
    std::function<void(int index, Point ptInItem)>    onHoverMove;
    // context menu (screenPt is screen coords; index = -1 means blank area).
    std::function<void(Point screenPt, int index)>    onContextMenu;

protected:
    friend class Page;
    friend MessageResult ListBoxProc(WindowHandle, MessageId, MessageWParam, MessageLParam, SubclassId, SubclassRef);
    void CreateNative() override;
    bool HandleCommand(int code) override;

    // ---- 自绘内部 ----
    void PaintSelf(DeviceContextHandle dc);
    void PaintItem(Canvas& g, int index, const Rect& rc);
    void SelectItem(int index, bool notify);
    void OnMultiClick(int item, bool ctrl, bool shift);
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
    // multi-select: m_selSet matches m_items (maintained by AddItem/Reset),
    // m_selAnchor = last shift-range anchor row (in-memory only).
    std::vector<bool> m_selSet;
    int               m_selAnchor = -1;
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
    void OnPaint(std::function<void(Canvas&, const Rect&)> painter);
    // with a click callback = clickable (captures mouse, tracks hover/press);
    // not set = decoration only (hit-test transparent, mouse falls through,
    // e.g. the Ctrl+Shift+V hint chip over the search box).
    void OnClick(std::function<void()> cb);
    bool Hovered() const { return m_hover; }
    bool Pressed() const { return m_press; }

protected:
    friend class Page;
    friend MessageResult CustomProc(WindowHandle, MessageId, MessageWParam, MessageLParam, SubclassId, SubclassRef);
    void CreateNative() override;

    CustomDesc m_desc;
    std::function<void(Canvas&, const Rect&)> m_painter;
    std::function<void()> m_click;
    bool m_hover = false, m_press = false;
};

} // namespace ui
} // namespace air
