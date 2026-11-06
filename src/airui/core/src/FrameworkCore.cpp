// ============================================================
// AirUI - FrameworkCore.cpp
// Phase 0 framework capabilities: object/event layer, scrolling,
// DPI, animation phases, accessibility and editable text.
//
// Everything is platform free: the only outside world is IPainter,
// IClock and IClipboard, all declared in hal.h / this header.
// ============================================================
#include "airui/FrameworkCore.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace air {
namespace ui {

namespace {

constexpr TextFormat kCenterText =
    dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;
constexpr TextFormat kLeftText =
    dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix | dt::EndEllipsis;

int ClampInt(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

const wchar_t* RoleName(ControlRole role)
{
    switch (role) {
    case ControlRole::Button:    return L"按钮";
    case ControlRole::CheckBox:  return L"复选框";
    case ControlRole::Radio:     return L"单选框";
    case ControlRole::TextBox:   return L"文本框";
    case ControlRole::Slider:    return L"滑块";
    case ControlRole::Tab:       return L"标签页";
    case ControlRole::MenuItem:  return L"菜单项";
    case ControlRole::Link:      return L"链接";
    case ControlRole::Group:     return L"分组";
    case ControlRole::Image:     return L"图像";
    case ControlRole::Custom:    return L"自定义";
    default:                     return L"控件";
    }
}

int TextLength(const std::wstring& s)
{
    return static_cast<int>(s.size());
}

} // namespace

// ============================================================
// 1. Control / ControlHost
// ============================================================
void Control::Paint(IPainter& g, const Palette& pal, const Metrics& mx)
{
    if (!m_visible || m_bounds.Empty()) return;
    const Color fill = m_enabled ? pal.surface : pal.sunken;
    const Color edge = m_focused ? pal.primary
                     : m_hovered ? pal.hoverBorder : pal.split;
    g.FillRound(m_bounds, mx.radiusSm, fill);
    g.StrokeRound(m_bounds, mx.radiusSm, edge, 1);
    if (!m_name.empty()) {
        g.Text(m_bounds, m_name, FontRole::Body,
               m_enabled ? pal.text : pal.textDisabled, kCenterText);
    }
}

std::wstring Control::AccessibleText() const
{
    std::wstring out = RoleName(m_role);
    if (!m_name.empty()) {
        out += L" ";
        out += m_name;
    }
    if (!m_value.empty()) {
        out += L" 值:";
        out += m_value;
    }
    if (m_focused)  out += L" 已聚焦";
    if (!m_enabled) out += L" 已禁用";
    if (!m_visible) out += L" 不可见";
    return out;
}

int ControlHost::Add(std::unique_ptr<Control> c)
{
    if (!c) return -1;
    const int id = m_nextId++;
    c->SetId(id);
    m_controls.push_back(std::move(c));
    SyncFocus();
    return id;
}

bool ControlHost::AddWithId(int id, std::unique_ptr<Control> c)
{
    if (!c || id < 0 || Find(id) != nullptr) return false;
    c->SetId(id);
    m_controls.push_back(std::move(c));
    if (id >= m_nextId) m_nextId = id + 1;
    SyncFocus();
    return true;
}

Control* ControlHost::Find(int id) const
{
    for (const auto& c : m_controls)
        if (c && c->Id() == id) return c.get();
    return nullptr;
}

bool ControlHost::Remove(int id)
{
    for (auto it = m_controls.begin(); it != m_controls.end(); ++it) {
        if (*it && (*it)->Id() == id) {
            m_controls.erase(it);
            if (m_hover == id)   m_hover = -1;
            if (m_capture == id) m_capture = -1;
            SyncFocus();
            return true;
        }
    }
    return false;
}

void ControlHost::Clear()
{
    m_controls.clear();
    m_focus.SetEntries({});
    m_hover = -1;
    m_capture = -1;
}

int ControlHost::HitControl(Point pt) const
{
    int best = -1;
    int bestZ = 0;
    bool found = false;
    for (const auto& c : m_controls) {
        if (!c || !c->Visible() || !c->Enabled()) continue;
        if (!c->Hit(pt)) continue;
        if (!found || c->Z() >= bestZ) {
            best = c->Id();
            bestZ = c->Z();
            found = true;
        }
    }
    return best;
}

void ControlHost::SyncFocus()
{
    const int previous = m_focus.Focused();
    std::vector<FocusEntry> entries;
    entries.reserve(m_controls.size());
    bool previousStillValid = false;
    for (const auto& c : m_controls) {
        if (!c || !c->Visible() || !c->Enabled()) continue;
        FocusEntry e;
        e.id = c->Id();
        e.bounds = c->Bounds();
        e.tabIndex = c->TabIndex();
        e.group = 0;
        e.enabled = true;
        e.name = c->Name();
        entries.push_back(e);
        if (e.id == previous) previousStillValid = true;
    }
    m_focus.SetEntries(std::move(entries));
    if (previousStillValid) m_focus.SetFocus(previous);
    else if (m_focus.Focused() < 0 && m_focus.Count() > 0) m_focus.SetFocus(m_focus.First(0));
    for (auto& c : m_controls) {
        if (c) c->SetFocused(c->Id() == m_focus.Focused());
    }
}

bool ControlHost::SetFocus(int id)
{
    if (!m_focus.SetFocus(id)) return false;
    for (auto& c : m_controls)
        if (c) c->SetFocused(c->Id() == id);
    return true;
}

bool ControlHost::MoveFocus(bool forward)
{
    if (m_focus.Count() == 0) return false;
    const int next = m_focus.Next(0, !forward);
    if (next < 0) return false;
    return SetFocus(next);
}

ControlResult ControlHost::DispatchPointer(const PointerEvent& ev)
{
    ControlResult out;
    if (ev.move) {
        const int hit = HitControl(ev.pos);
        if (hit != m_hover) {
            if (Control* old = Find(m_hover)) old->SetHovered(false);
            if (Control* now = Find(hit))     now->SetHovered(true);
            m_hover = hit;
            out.redraw = true;
        }
        out.handled = hit >= 0;
        out.id = hit;
        return out;
    }

    if (ev.down) {
        const int hit = HitControl(ev.pos);
        if (hit >= 0) {
            SetFocus(hit);
            m_capture = hit;
            if (Control* c = Find(hit)) {
                out = c->OnPointer(ev);
                out.id = hit;
            }
        }
        out.handled = hit >= 0;
        return out;
    }

    if (ev.up) {
        const int target = m_capture >= 0 ? m_capture : HitControl(ev.pos);
        m_capture = -1;
        if (Control* c = Find(target)) {
            out = c->OnPointer(ev);
            out.id = target;
            out.handled = true;
        }
        return out;
    }
    return out;
}

ControlResult ControlHost::DispatchKey(const KeyEvent& ev)
{
    ControlResult out;
    if (ev.vk == keys::Tab) {
        const int before = m_focus.Focused();
        if (MoveFocus(!ev.shift)) {
            out.handled = true;
            out.redraw = true;
            out.id = m_focus.Focused();
            out.action = 4;   // focus moved
            out.status = L"焦点: " + std::wstring(
                m_focus.Find(m_focus.Focused()) ? m_focus.Find(m_focus.Focused())->name : L"");
        }
        (void)before;
        return out;
    }
    if (Control* c = Find(m_focus.Focused())) {
        out = c->OnKey(ev);
        out.id = c->Id();
    }
    return out;
}

void ControlHost::PaintAll(IPainter& g, const Palette& pal, const Metrics& mx)
{
    std::vector<const Control*> ordered;
    ordered.reserve(m_controls.size());
    for (const auto& c : m_controls)
        if (c && c->Visible()) ordered.push_back(c.get());
    std::stable_sort(ordered.begin(), ordered.end(),
        [](const Control* a, const Control* b) { return a->Z() < b->Z(); });
    for (Control const* c : ordered) {
        // Paint is non-const on the interface; the host owns the object.
        const_cast<Control*>(c)->Paint(g, pal, mx);
    }
}

// ============================================================
// 2. Scrolling
// ============================================================
int ScrollMax(const ScrollState& st)
{
    const int extra = st.contentH - st.viewH;
    return extra > 0 ? extra : 0;
}

int ClampScrollOffset(int offset, const ScrollState& st)
{
    return ClampInt(offset, 0, ScrollMax(st));
}

void ScrollBy(ScrollState& st, int delta)
{
    st.offset = ClampScrollOffset(st.offset + delta, st);
}

void ScrollPage(ScrollState& st, int direction)
{
    const int step = st.viewH > 0 ? st.viewH : st.lineH;
    ScrollBy(st, direction >= 0 ? step : -step);
}

ScrollGeometry MeasureScrollbar(const Rect& view, const ScrollState& st,
                                const Metrics& mx)
{
    ScrollGeometry geo;
    if (view.Empty()) return geo;
    const int w = (std::max)(6, mx.unit);
    const int btnH = (std::max)(10, mx.unit + 4);
    geo.track = Rect(view.right - w, view.top, view.right, view.bottom);
    geo.upBtn = Rect(geo.track.left, geo.track.top, geo.track.right,
                     geo.track.top + btnH);
    geo.downBtn = Rect(geo.track.left, geo.track.bottom - btnH,
                       geo.track.right, geo.track.bottom);

    const int maxOff = ScrollMax(st);
    const int innerTop = geo.upBtn.bottom;
    const int innerBottom = geo.downBtn.top;
    const int innerH = innerBottom - innerTop;
    if (innerH <= 0 || maxOff <= 0) return geo;

    geo.visible = true;
    const long long full = static_cast<long long>(st.contentH) > 0 ? st.contentH : 1;
    int thumbH = static_cast<int>(static_cast<long long>(innerH) * st.viewH / full);
    thumbH = ClampInt(thumbH, 16, innerH);
    geo.thumbH = thumbH;
    const int travel = innerH - thumbH;
    const int y = innerTop + (travel > 0 && maxOff > 0
        ? static_cast<int>(static_cast<long long>(travel) * st.offset / maxOff) : 0);
    geo.thumb = Rect(geo.track.left, y, geo.track.right, y + thumbH);
    return geo;
}

ScrollBarHit HitScrollbar(const Rect& view, const ScrollState& st,
                          const Metrics& mx, Point pt)
{
    const ScrollGeometry geo = MeasureScrollbar(view, st, mx);
    if (!geo.visible) return ScrollBarHit::None;
    if (geo.upBtn.Contains(pt))   return ScrollBarHit::UpButton;
    if (geo.downBtn.Contains(pt)) return ScrollBarHit::DownButton;
    if (geo.thumb.Contains(pt))   return ScrollBarHit::Thumb;
    if (geo.track.Contains(pt))
        return pt.y < geo.thumb.top ? ScrollBarHit::TrackAbove
                                    : ScrollBarHit::TrackBelow;
    return ScrollBarHit::None;
}

int ScrollOffsetFromDrag(const ScrollGeometry& geo, const ScrollState& st,
                         int pointerY, int grabOffsetInThumb)
{
    const int maxOff = ScrollMax(st);
    if (maxOff <= 0 || !geo.visible) return 0;
    const int innerTop = geo.upBtn.bottom;
    const int innerBottom = geo.downBtn.top;
    const int travel = innerBottom - innerTop - geo.thumbH;
    if (travel <= 0) return 0;
    const int thumbTop = ClampInt(pointerY - grabOffsetInThumb, innerTop,
                                  innerTop + travel);
    return static_cast<int>(static_cast<long long>(thumbTop - innerTop) * maxOff / travel);
}

void PaintScrollbar(IPainter& g, const Rect& view, const ScrollState& st,
                    const Palette& pal, const Metrics& mx)
{
    const ScrollGeometry geo = MeasureScrollbar(view, st, mx);
    if (!geo.visible) return;
    g.FillRect(geo.track, pal.sunken);
    g.FillRect(geo.upBtn, pal.surface);
    g.FillRect(geo.downBtn, pal.surface);
    g.Text(geo.upBtn, L"\u25B2", FontRole::Meta, pal.textSub, kCenterText);
    g.Text(geo.downBtn, L"\u25BC", FontRole::Meta, pal.textSub, kCenterText);
    g.FillRound(geo.thumb, mx.radiusSm,
                (st.dragging || st.thumbHovered) ? pal.primary : pal.scrollThumb);
}

VirtualRange VirtualRangeUniform(const ScrollState& st, int rowCount,
                                 int rowH, int overscan)
{
    VirtualRange out;
    if (rowCount <= 0 || rowH <= 0) return out;
    const int pad = (std::max)(0, overscan);
    out.first = ClampInt(st.offset / rowH - pad, 0, rowCount);
    out.last = ClampInt((st.offset + st.viewH) / rowH + 1 + pad, 0, rowCount);
    if (out.last < out.first) out.last = out.first;
    out.topPixel = out.first * rowH - st.offset;
    out.count = out.last - out.first;
    return out;
}

int FirstVisibleRow(const ScrollState& st, const std::vector<int>& heights)
{
    int y = 0;
    for (size_t i = 0; i < heights.size(); ++i) {
        const int h = (std::max)(1, heights[i]);
        if (y + h > st.offset) return static_cast<int>(i);
        y += h;
    }
    return heights.empty() ? 0 : static_cast<int>(heights.size() - 1);
}

VirtualRange VirtualRangeVariable(const ScrollState& st,
                                  const std::vector<int>& heights,
                                  int overscan)
{
    VirtualRange out;
    const int n = static_cast<int>(heights.size());
    if (n == 0) return out;
    const int pad = (std::max)(0, overscan);

    std::vector<int> prefix(n + 1, 0);
    for (int i = 0; i < n; ++i)
        prefix[i + 1] = prefix[i] + (std::max)(1, heights[i]);

    int first = 0;
    while (first < n && prefix[first + 1] <= st.offset) ++first;
    int last = first;
    const int bottom = st.offset + (std::max)(0, st.viewH);
    while (last < n && prefix[last] < bottom) ++last;

    first = ClampInt(first - pad, 0, n);
    last = ClampInt(last + pad, 0, n);
    out.first = first;
    out.last = last;
    out.topPixel = prefix[first] - st.offset;
    out.count = last - first;
    return out;
}

// ============================================================
// 3. DPI
// ============================================================
int ScaleInt(int logical, int dpi, int baseDpi)
{
    if (baseDpi <= 0) baseDpi = 96;
    if (dpi <= 0) dpi = 96;
    return static_cast<int>(static_cast<long long>(logical) * dpi / baseDpi);
}

Rect ScaleRect(const Rect& r, int dpi, int baseDpi)
{
    return Rect(ScaleInt(r.left, dpi, baseDpi), ScaleInt(r.top, dpi, baseDpi),
                ScaleInt(r.right, dpi, baseDpi), ScaleInt(r.bottom, dpi, baseDpi));
}

Metrics MetricsForDpi(const Metrics& base, int dpi)
{
    return base.ForDpi(dpi <= 0 ? 96 : dpi);
}

void ApplyDpi(const Metrics& base, int dpi)
{
    SetMetrics(MetricsForDpi(base, dpi));
}

// ============================================================
// 4. Animation clock
// ============================================================
uint64_t AnimationClock::Tick()
{
    m_now = m_clock ? m_clock->NowMs() : 0;
    return m_now;
}

float AnimationClock::Phase(uint64_t startMs, int durationMs) const
{
    if (durationMs <= 0 || m_now <= startMs) return 0.0f;
    const float t = static_cast<float>(m_now - startMs) /
                    static_cast<float>(durationMs);
    return t >= 1.0f ? 1.0f : t;
}

float AnimationClock::Loop(uint64_t startMs, int periodMs) const
{
    if (periodMs <= 0 || m_now <= startMs) return 0.0f;
    const uint64_t elapsed = m_now - startMs;
    return static_cast<float>(elapsed % static_cast<uint64_t>(periodMs)) /
           static_cast<float>(periodMs);
}

float SpinnerPhase(uint64_t nowMs, int periodMs)
{
    if (periodMs <= 0) return 0.0f;
    return static_cast<float>(nowMs % static_cast<uint64_t>(periodMs)) /
           static_cast<float>(periodMs);
}

float PulsePhase(uint64_t nowMs, int periodMs)
{
    if (periodMs <= 0) return 0.0f;
    const uint64_t p = static_cast<uint64_t>(periodMs);
    const uint64_t half = p / 2;
    const uint64_t t = nowMs % p;
    if (t <= half) return static_cast<float>(t) / static_cast<float>(half ? half : 1);
    return 1.0f - static_cast<float>(t - half) / static_cast<float>(p - half);
}

int ShimmerOffset(uint64_t nowMs, int width)
{
    if (width <= 0) return 0;
    const float phase = SpinnerPhase(nowMs, 1600);
    return static_cast<int>(phase * static_cast<float>(width * 2)) - width;
}

// ============================================================
// 5. Accessibility
// ============================================================
std::vector<AccessibleNode> BuildAccessibleTree(const ControlHost& host)
{
    std::vector<AccessibleNode> nodes;
    nodes.reserve(host.Count());
    for (const auto& c : host.Controls()) {
        if (!c || !c->Visible()) continue;
        AccessibleNode n;
        n.id = c->Id();
        n.role = c->Role();
        n.name = c->Name();
        n.value = c->Value();
        n.bounds = c->Bounds();
        n.state.clear();
        if (c->Focused())  n.state += L"focused ";
        if (c->Hovered())  n.state += L"hovered ";
        if (!c->Enabled()) n.state += L"disabled ";
        nodes.push_back(std::move(n));
    }
    return nodes;
}

std::wstring DescribeAccessible(const AccessibleNode& node)
{
    std::wstring out = RoleName(node.role);
    if (!node.name.empty()) { out += L" "; out += node.name; }
    if (!node.value.empty()) { out += L" 值:"; out += node.value; }
    if (!node.state.empty()) { out += L" "; out += node.state; }
    return out;
}

std::wstring DescribeAll(const ControlHost& host)
{
    const std::vector<AccessibleNode> nodes = BuildAccessibleTree(host);
    std::wstring out;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) out += L"\n";
        out += DescribeAccessible(nodes[i]);
    }
    return out;
}

// ============================================================
// 6. Editable text
// ============================================================
int SelectionBegin(const TextEditState& st)
{
    if (st.selectionAnchor < 0) return st.cursor;
    return (std::min)(st.selectionAnchor, st.cursor);
}

int SelectionEnd(const TextEditState& st)
{
    if (st.selectionAnchor < 0) return st.cursor;
    return (std::max)(st.selectionAnchor, st.cursor);
}

bool HasSelection(const TextEditState& st)
{
    return st.selectionAnchor >= 0 && st.selectionAnchor != st.cursor;
}

std::wstring SelectedText(const TextEditState& st)
{
    if (!HasSelection(st)) return L"";
    const int a = SelectionBegin(st);
    const int b = SelectionEnd(st);
    const int n = TextLength(st.text);
    if (a < 0 || b > n || a >= b) return L"";
    return st.text.substr(static_cast<size_t>(a), static_cast<size_t>(b - a));
}

std::wstring DisplayText(const TextEditState& st, bool password)
{
    if (!password) return st.text;
    return std::wstring(st.text.size(), L'\u2022');
}

TextEditLayout MeasureTextEdit(const Rect& rc, const TextEditState& st,
                               const Metrics& mx, int charWidth)
{
    TextEditLayout out;
    if (rc.Empty()) return out;
    out.frame = rc;
    out.textRc = Rect(rc.left + mx.unit, rc.top, rc.right - mx.unit, rc.bottom);
    out.charW = (std::max)(1, charWidth);
    const int textW = (std::max)(0, out.textRc.Width());
    const int visibleChars = textW / out.charW;
    const int n = TextLength(st.text);
    int scrollChars = 0;
    if (st.cursor > visibleChars) scrollChars = st.cursor - visibleChars + 1;
    scrollChars = ClampInt(scrollChars, 0, (std::max)(0, n));
    out.scrollX = scrollChars * out.charW;
    out.caretX = out.textRc.left + st.cursor * out.charW - out.scrollX;
    out.caretX = ClampInt(out.caretX, out.textRc.left, out.textRc.right);
    out.selectionLeft = out.textRc.left + SelectionBegin(st) * out.charW - out.scrollX;
    out.selectionRight = out.textRc.left + SelectionEnd(st) * out.charW - out.scrollX;
    return out;
}

int CaretIndexAt(const Rect& rc, const TextEditState& st,
                 const Metrics& mx, int charWidth, Point pt)
{
    const TextEditLayout L = MeasureTextEdit(rc, st, mx, charWidth);
    if (L.textRc.Empty()) return 0;
    const int charW = L.charW;
    int index = L.scrollX / charW + (pt.x - L.textRc.left) / charW;
    // Round to the nearest character boundary.
    if ((pt.x - L.textRc.left) % charW > charW / 2) ++index;
    return ClampInt(index, 0, TextLength(st.text));
}

namespace {

void EraseSelection(TextEditState& st)
{
    if (!HasSelection(st)) return;
    const int a = SelectionBegin(st);
    const int b = SelectionEnd(st);
    st.text.erase(static_cast<size_t>(a), static_cast<size_t>(b - a));
    st.cursor = a;
    st.selectionAnchor = -1;
}

void InsertAtCursor(TextEditState& st, const std::wstring& text)
{
    if (text.empty()) return;
    EraseSelection(st);
    std::wstring value = text;
    if (st.maxLength > 0) {
        const int room = st.maxLength - TextLength(st.text);
        if (room <= 0) return;
        if (TextLength(value) > room)
            value = value.substr(0, static_cast<size_t>(room));
    }
    st.text.insert(static_cast<size_t>(st.cursor), value);
    st.cursor += TextLength(value);
    st.selectionAnchor = -1;
}

} // namespace

EditResult ApplyEdit(TextEditState& st, EditAction action,
                     const std::wstring& argument, IClipboard* clipboard)
{
    EditResult out;
    const int n = TextLength(st.text);
    switch (action) {
    case EditAction::None:
        return out;

    case EditAction::Insert:
        InsertAtCursor(st, argument);
        out.changed = true;
        break;

    case EditAction::Backspace:
        if (HasSelection(st)) {
            EraseSelection(st);
            out.changed = true;
        } else if (st.cursor > 0) {
            st.text.erase(static_cast<size_t>(st.cursor - 1), 1);
            --st.cursor;
            out.changed = true;
        }
        break;

    case EditAction::Delete:
        if (HasSelection(st)) {
            EraseSelection(st);
            out.changed = true;
        } else if (st.cursor < n) {
            st.text.erase(static_cast<size_t>(st.cursor), 1);
            out.changed = true;
        }
        break;

    case EditAction::MoveLeft:
        st.cursor = ClampInt(st.cursor - 1, 0, n);
        st.selectionAnchor = -1;
        break;

    case EditAction::MoveRight:
        st.cursor = ClampInt(st.cursor + 1, 0, n);
        st.selectionAnchor = -1;
        break;

    case EditAction::MoveUp:
    case EditAction::Home:
        st.cursor = 0;
        st.selectionAnchor = -1;
        break;

    case EditAction::MoveDown:
    case EditAction::End:
        st.cursor = n;
        st.selectionAnchor = -1;
        break;

    case EditAction::SelectAll:
        st.selectionAnchor = 0;
        st.cursor = n;
        break;

    case EditAction::Copy:
        if (clipboard && HasSelection(st)) clipboard->Write(SelectedText(st));
        break;

    case EditAction::Cut:
        if (clipboard && HasSelection(st)) {
            clipboard->Write(SelectedText(st));
            EraseSelection(st);
            out.changed = true;
        }
        break;

    case EditAction::Paste:
        if (clipboard) {
            InsertAtCursor(st, clipboard->Read());
            out.changed = true;
        } else if (!argument.empty()) {
            InsertAtCursor(st, argument);
            out.changed = true;
        }
        break;

    case EditAction::CompositionStart:
        st.composition.clear();
        st.compositionCursor = 0;
        break;

    case EditAction::CompositionUpdate:
        st.composition = argument;
        st.compositionCursor = TextLength(argument);
        break;

    case EditAction::CompositionEnd:
        if (!st.composition.empty()) {
            InsertAtCursor(st, st.composition);
            st.composition.clear();
            out.changed = true;
        }
        break;
    }
    out.redraw = true;
    out.consumed = true;
    return out;
}

TextBoxHit HitTextBox(const Rect& rc, const TextBoxState& st,
                      const Metrics& mx, Point pt)
{
    if (!rc.Contains(pt)) return TextBoxHit::None;
    (void)mx;
    if (!st.suffix.empty()) {
        const int w = (std::max)(mx.unit * 4, 24);
        if (Rect(rc.right - w, rc.top, rc.right, rc.bottom).Contains(pt))
            return TextBoxHit::Suffix;
    }
    return TextBoxHit::Body;
}

void PaintTextBox(IPainter& g, const Rect& rc, const TextBoxState& st,
                  const Palette& pal, const Metrics& mx)
{
    if (rc.Empty()) return;
    const bool focused = st.edit.hasFocus || st.base.focused;
    const Color textColor = st.base.disabled ? pal.textDisabled : pal.text;

    g.FillRound(rc, mx.radiusMd, st.base.disabled ? pal.sunken : pal.surface);
    g.StrokeRound(rc, mx.radiusMd,
                  focused ? pal.primary : (st.base.hover ? pal.hoverBorder : pal.border), 1);

    int charW = (std::max)(4, g.MeasureText(L"0", FontRole::Body));
    int left = rc.left + mx.unit;
    int right = rc.right - mx.unit;
    if (!st.prefix.empty()) {
        const Rect pre(rc.left + mx.unit, rc.top,
                       rc.left + mx.unit + mx.unit * 6, rc.bottom);
        g.Text(pre, st.prefix, FontRole::Meta, pal.textSub, kLeftText);
        left = pre.right + mx.unit / 2;
    }
    if (!st.suffix.empty()) {
        const int w = (std::max)(mx.unit * 4, 24);
        const Rect suf(rc.right - w, rc.top, rc.right, rc.bottom);
        g.Text(suf, st.suffix, FontRole::Icon, pal.textSub, kCenterText);
        right = suf.left - mx.unit / 2;
    }
    if (right <= left) return;

    const Rect textRc(left, rc.top, right, rc.bottom);
    const TextEditState& edit = st.edit;
    const TextEditLayout L = MeasureTextEdit(textRc, edit, mx, charW);
    const std::wstring shown = DisplayText(edit, st.password && !st.revealed);

    g.PushClip(textRc);
    if (HasSelection(edit)) {
        const int a = ClampInt(L.selectionLeft, textRc.left, textRc.right);
        const int b = ClampInt(L.selectionRight, textRc.left, textRc.right);
        if (b > a) g.FillRect(Rect(a, rc.top + 2, b, rc.bottom - 2), pal.selectFill);
    }
    if (shown.empty() && edit.composition.empty()) {
        g.Text(textRc, st.placeholder, FontRole::Body, pal.textFaint, kLeftText);
    } else {
        const int x = textRc.left - L.scrollX;
        const int textW = static_cast<int>(shown.size()) * charW + textRc.Width();
        g.Text(Rect(x, rc.top, x + textW, rc.bottom),
               shown, FontRole::Body, textColor, kLeftText);
        if (!edit.composition.empty()) {
            g.Text(Rect(L.caretX, rc.top, L.caretX + mx.unit * 8, rc.bottom),
                   edit.composition, FontRole::Body, pal.primaryText, kLeftText);
        }
    }
    if (focused && st.showCaret && st.edit.composition.empty()) {
        g.FillRect(Rect(L.caretX, rc.top + 3, L.caretX + 1, rc.bottom - 3),
                   pal.primary);
    }
    g.PopClip();
}

} // namespace ui
} // namespace air
