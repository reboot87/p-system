#pragma once
// ============================================================
// AirUI - FrameworkCore.h
// Phase 0 framework capabilities, platform free.
//
//   1. Control / ControlHost   - optional object layer on top of the
//                               Paint* free functions: z-order, event
//                               routing, capture and Tab focus.
//   2. ScrollModel            - scroll container, virtual row ranges
//                               and a self-drawn scrollbar.
//   3. Dpi                    - Metrics::ForDpi actually applied.
//   4. AnimationClock         - IClock driven phases for spinners etc.
//   5. Accessibility          - a flat tree a backend can forward to
//                               its native accessibility API.
//   6. TextEdit / IClipboard  - editable text: caret, selection, IME
//                               composition, clipboard through HAL.
//
// Nothing here includes a platform header. The object layer is an
// *option*: existing galleries may keep calling Paint* directly.
// ============================================================
#include "airui/types.h"
#include "airui/hal.h"
#include "airui/Theme.h"
#include "airui/ControlsCore.h"
#include "airui/OverlayCore.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace air {
namespace ui {

// ============================================================
// 1. Control base + event routing
// ============================================================
enum class ControlRole
{
    None, Button, CheckBox, Radio, TextBox, Slider, Tab, MenuItem,
    Link, Group, Image, Custom
};

struct PointerEvent
{
    Point pos{};
    int  button = 0;      // 0 = left
    bool down = false;
    bool up = false;
    bool move = false;
};

struct KeyEvent
{
    int  vk = 0;          // numeric key codes, see keys:: in OverlayCore.h
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

struct ControlResult
{
    bool handled = false;
    bool redraw = false;
    int  action = 0;      // 0 none, 1 activate, 2 toggle, 3 value changed
    int  id = -1;
    std::wstring status;
};

class Control
{
public:
    explicit Control(int id = 0) : m_id(id) {}
    virtual ~Control() = default;

    int  Id() const { return m_id; }
    void SetId(int id) { m_id = id; }

    const Rect& Bounds() const { return m_bounds; }
    void SetBounds(const Rect& r) { m_bounds = r; }

    int  Z() const { return m_z; }
    void SetZ(int z) { m_z = z; }

    int  TabIndex() const { return m_tabIndex; }
    void SetTabIndex(int t) { m_tabIndex = t; }

    bool Enabled() const { return m_enabled; }
    void SetEnabled(bool v) { m_enabled = v; }

    bool Visible() const { return m_visible; }
    void SetVisible(bool v) { m_visible = v; }

    bool Focused() const { return m_focused; }
    void SetFocused(bool v) { m_focused = v; }

    bool Hovered() const { return m_hovered; }
    void SetHovered(bool v) { m_hovered = v; }

    ControlRole Role() const { return m_role; }
    void SetRole(ControlRole r) { m_role = r; }

    const std::wstring& Name() const { return m_name; }
    void SetName(std::wstring n) { m_name = std::move(n); }

    const std::wstring& Value() const { return m_value; }
    void SetValue(std::wstring v) { m_value = std::move(v); }

    virtual bool Hit(Point pt) const
    {
        return m_visible && m_enabled && m_bounds.Contains(pt);
    }

    virtual ControlResult OnPointer(const PointerEvent& ev)
    {
        ControlResult out;
        if (ev.down) {
            m_focused = true;
            out.handled = true;
            out.redraw = true;
            out.action = 1;
            out.id = m_id;
        } else if (ev.up) {
            out.handled = true;
            out.redraw = true;
            out.id = m_id;
        }
        return out;
    }

    virtual ControlResult OnKey(const KeyEvent& ev)
    {
        ControlResult out;
        if (ev.vk == keys::Return || ev.vk == keys::Space) {
            out.handled = true;
            out.redraw = true;
            out.action = 1;
            out.id = m_id;
        }
        return out;
    }

    // Default visual: a framed box with the accessible name. Derived
    // controls normally override this with a Paint* call.
    virtual void Paint(IPainter& g, const Palette& pal, const Metrics& mx);

    virtual std::wstring AccessibleText() const;

private:
    int  m_id = 0;
    Rect m_bounds{};
    int  m_z = 0;
    int  m_tabIndex = 0;
    bool m_enabled = true;
    bool m_visible = true;
    bool m_focused = false;
    bool m_hovered = false;
    ControlRole m_role = ControlRole::None;
    std::wstring m_name;
    std::wstring m_value;
};

class ControlHost
{
public:
    // Takes ownership. Returns the assigned id (>= 0).
    int  Add(std::unique_ptr<Control> c);
    bool AddWithId(int id, std::unique_ptr<Control> c);
    Control* Find(int id) const;
    bool Remove(int id);
    void Clear();
    int  Count() const { return static_cast<int>(m_controls.size()); }
    const std::vector<std::unique_ptr<Control>>& Controls() const { return m_controls; }

    // Topmost visible control under the point (highest z wins), -1 if none.
    int  HitControl(Point pt) const;

    ControlResult DispatchPointer(const PointerEvent& ev);
    ControlResult DispatchKey(const KeyEvent& ev);

    void PaintAll(IPainter& g, const Palette& pal, const Metrics& mx);

    // Rebuild the focus table from the current controls, keeping the
    // focused id when it still exists and is enabled.
    void SyncFocus();
    int  FocusedId() const { return m_focus.Focused(); }
    bool SetFocus(int id);
    bool MoveFocus(bool forward);
    Rect FocusRect() const { return m_focus.FocusRect(); }
    const FocusScope& Focus() const { return m_focus; }

    int  HoveredId() const { return m_hover; }
    int  CapturedId() const { return m_capture; }

private:
    std::vector<std::unique_ptr<Control>> m_controls;
    FocusScope m_focus;
    int m_nextId = 1;
    int m_hover = -1;
    int m_capture = -1;
};

// ============================================================
// 2. Scroll container + virtual scrolling
// ============================================================
struct ScrollState
{
    int offset = 0;       // first visible logical pixel
    int contentH = 0;     // total logical content height
    int viewH = 0;        // viewport height
    int lineH = 32;       // wheel step
    bool dragging = false;
    bool thumbHovered = false;
};

int ScrollMax(const ScrollState& st);
int ClampScrollOffset(int offset, const ScrollState& st);
void ScrollBy(ScrollState& st, int delta);
void ScrollPage(ScrollState& st, int direction);

struct ScrollGeometry
{
    Rect track{};
    Rect thumb{};
    Rect upBtn{};
    Rect downBtn{};
    int  thumbH = 0;
    bool visible = false;
};

ScrollGeometry MeasureScrollbar(const Rect& view, const ScrollState& st,
                                const Metrics& mx);

enum class ScrollBarHit
{
    None, Thumb, TrackAbove, TrackBelow, UpButton, DownButton
};

ScrollBarHit HitScrollbar(const Rect& view, const ScrollState& st,
                          const Metrics& mx, Point pt);

// Drag helper: maps a vertical pointer position back to a scroll offset.
int ScrollOffsetFromDrag(const ScrollGeometry& geo, const ScrollState& st,
                         int pointerY, int grabOffsetInThumb);

void PaintScrollbar(IPainter& g, const Rect& view, const ScrollState& st,
                    const Palette& pal, const Metrics& mx);

struct VirtualRange
{
    int first = 0;        // inclusive
    int last = 0;         // exclusive
    int topPixel = 0;     // y of row `first` relative to the viewport
    int count = 0;
};

// Uniform row height.
VirtualRange VirtualRangeUniform(const ScrollState& st, int rowCount,
                                 int rowH, int overscan = 2);

// Per-row heights (variable height lists).
VirtualRange VirtualRangeVariable(const ScrollState& st,
                                  const std::vector<int>& heights,
                                  int overscan = 2);

int FirstVisibleRow(const ScrollState& st, const std::vector<int>& heights);

// ============================================================
// 3. DPI
// ============================================================
int    ScaleInt(int logical, int dpi, int baseDpi = 96);
Rect   ScaleRect(const Rect& logical, int dpi, int baseDpi = 96);
Metrics MetricsForDpi(const Metrics& base, int dpi);

// Applies the scaled metrics globally (calls SetMetrics from Theme.h).
void   ApplyDpi(const Metrics& base, int dpi);

// ============================================================
// 4. Animation clock
// ============================================================
class AnimationClock
{
public:
    void SetClock(IClock* clock) { m_clock = clock; }
    IClock* Clock() const { return m_clock; }

    // Reads the injected clock. Returns 0 when no clock is attached,
    // which keeps every animation deterministic in tests.
    uint64_t Tick();

    float Phase(uint64_t startMs, int durationMs) const;  // 0..1, no loop
    float Loop(uint64_t startMs, int periodMs) const;     // 0..1, wrapping
    bool  Running() const { return m_clock != nullptr && m_now > 0; }

private:
    IClock* m_clock = nullptr;
    uint64_t m_now = 0;
};

float SpinnerPhase(uint64_t nowMs, int periodMs = 1000);
float PulsePhase(uint64_t nowMs, int periodMs = 1600);
int   ShimmerOffset(uint64_t nowMs, int width);

// ============================================================
// 5. Accessibility
// ============================================================
struct AccessibleNode
{
    int id = -1;
    ControlRole role = ControlRole::None;
    std::wstring name;
    std::wstring value;
    std::wstring state;      // e.g. "focused disabled"
    Rect bounds{};
    int  childCount = 0;
};

std::vector<AccessibleNode> BuildAccessibleTree(const ControlHost& host);
std::wstring DescribeAccessible(const AccessibleNode& node);
std::wstring DescribeAll(const ControlHost& host);

// ============================================================
// 6. Editable text
// ============================================================
class IClipboard
{
public:
    virtual ~IClipboard() = default;
    virtual std::wstring Read() = 0;
    virtual void Write(const std::wstring& text) = 0;
};

struct TextEditState
{
    std::wstring text;
    int cursor = 0;                 // 0 .. text.size()
    int selectionAnchor = -1;      // -1 = no selection
    std::wstring composition;      // IME pre-edit string
    int  compositionCursor = 0;
    bool hasFocus = false;
    bool insertMode = true;
    int  maxLength = 0;            // 0 = unlimited
};

int SelectionBegin(const TextEditState& st);
int SelectionEnd(const TextEditState& st);
bool HasSelection(const TextEditState& st);
std::wstring SelectedText(const TextEditState& st);
std::wstring DisplayText(const TextEditState& st, bool password);

struct TextEditLayout
{
    Rect frame{};
    Rect textRc{};
    int  charW = 8;
    int  scrollX = 0;
    int  caretX = 0;
    int  selectionLeft = 0;
    int  selectionRight = 0;
};

// charWidth comes from IPainter::MeasureText so geometry and hit
// testing share exactly the same metric.
TextEditLayout MeasureTextEdit(const Rect& rc, const TextEditState& st,
                               const Metrics& mx, int charWidth);

int CaretIndexAt(const Rect& rc, const TextEditState& st,
                 const Metrics& mx, int charWidth, Point pt);

enum class EditAction
{
    None, Insert, Backspace, Delete, MoveLeft, MoveRight, MoveUp, MoveDown,
    Home, End, SelectAll, Copy, Cut, Paste,
    CompositionStart, CompositionUpdate, CompositionEnd
};

struct EditResult
{
    bool changed = false;
    bool redraw = false;
    bool consumed = false;
    std::wstring status;
};

EditResult ApplyEdit(TextEditState& st, EditAction action,
                     const std::wstring& argument, IClipboard* clipboard);

struct TextBoxState
{
    ControlState base = {};
    TextEditState edit;
    std::wstring placeholder = L"请输入…";
    std::wstring prefix;
    std::wstring suffix;
    bool password = false;
    bool revealed = false;
    bool showCaret = true;
};

enum class TextBoxHit { None, Body, Suffix };
TextBoxHit HitTextBox(const Rect& rc, const TextBoxState& st,
                      const Metrics& mx, Point pt);

void PaintTextBox(IPainter& g, const Rect& rc, const TextBoxState& st,
                  const Palette& pal, const Metrics& mx);

} // namespace ui
} // namespace air
