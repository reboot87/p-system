// ============================================================
// AirUI - DeclarativeRender.cpp
// 节点树 -> IPainter。见 DeclarativeRender.h 的节点映射。
// ============================================================
#include "airui/DeclarativeRender.h"

#include "airui/ControlsCore.h"

namespace air {
namespace ui {

namespace {

ButtonStyle StyleFromToken(const std::wstring& style)
{
    if (style == L"primary") return ButtonStyle::Primary;
    if (style == L"danger")  return ButtonStyle::Danger;
    if (style == L"ghost")   return ButtonStyle::Ghost;
    return ButtonStyle::Secondary;
}

TextFormat HAlignToFormat(LayoutAlign align)
{
    switch (align) {
    case LayoutAlign::Center: return dt::Center;
    case LayoutAlign::End:    return dt::Right;
    default:                  return dt::Left;
    }
}

bool IsVisualContainer(const LayoutNode& n)
{
    return n.type == NodeType::Row || n.type == NodeType::Column ||
           n.type == NodeType::Frame || n.type == NodeType::Grid ||
           n.type == NodeType::Scroll;
}

// Slider/Stepper 值钳制，避免 JSON 写出界值把 Paint 函数画穿。
int ClampInt(int v, int lo, int hi)
{
    if (hi < lo) hi = lo;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

double Clamp01(double v)
{
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

void PaintControlNode(IPainter& g, const LayoutNode& node,
                      const Palette& pal, const Metrics& mx)
{
    const Rect rc = node.Frame();
    ControlState st;
    st.disabled = node.disabled;

    switch (node.widget) {
    case WidgetKind::Button:
        PaintButton(g, rc, node.text, StyleFromToken(node.style), st, pal, mx);
        break;
    case WidgetKind::Checkbox:
        st.checked = node.checked;
        PaintCheckbox(g, rc, node.text, st, pal);
        break;
    case WidgetKind::Radio:
        st.checked = node.checked;
        PaintRadio(g, rc, node.text, st, pal);
        break;
    case WidgetKind::Switch: {
        ToggleState ts;
        ts.base = st;
        ts.on = node.checked;
        PaintToggleSwitch(g, rc, ts, pal, mx);
        break;
    }
    case WidgetKind::Slider: {
        SliderState ss;
        ss.base = st;
        ss.value = ClampInt(node.value, node.sliderMin, node.sliderMax);
        ss.min = node.sliderMin;
        ss.max = node.sliderMax;
        PaintSlider(g, rc, ss, pal, mx);
        break;
    }
    case WidgetKind::ProgressBar:
        PaintProgressBar(g, rc, static_cast<float>(Clamp01(node.progress)), st, pal, mx);
        break;
    case WidgetKind::ProgressRing: {
        ProgressRingState ps;
        ps.base = st;
        ps.progress = static_cast<float>(Clamp01(node.progress));
        PaintProgressRing(g, rc, ps, pal, mx);
        break;
    }
    case WidgetKind::TextField: {
        TextFieldState ts;
        ts.base = st;
        ts.text = node.text;
        if (!node.placeholder.empty()) ts.placeholder = node.placeholder;
        PaintTextField(g, rc, ts, pal, mx);
        break;
    }
    case WidgetKind::Tag: {
        TagState ts;
        ts.base = st;
        ts.text = node.text;
        ts.selected = node.checked;   // Tag 用 checked 表达选中
        PaintTag(g, rc, ts, pal, mx);
        break;
    }
    case WidgetKind::Stepper: {
        StepperState ss;
        ss.base = st;
        ss.value = ClampInt(node.value, node.sliderMin, node.sliderMax);
        ss.min = node.sliderMin;
        ss.max = node.sliderMax;
        PaintStepper(g, rc, ss, pal, mx);
        break;
    }
    case WidgetKind::Rating: {
        RatingState rs;
        rs.base = st;
        rs.rating = ClampInt(node.rating, 0, node.ratingMax);
        rs.max = node.ratingMax;
        PaintRating(g, rc, rs, pal, mx);
        break;
    }
    case WidgetKind::Segmented: {
        SegmentedState ss;
        ss.base = st;
        ss.items = node.optionList.empty() ? std::vector<std::wstring>{node.text}
                                           : node.optionList;
        ss.selected = ClampInt(node.selectedIndex, 0,
                               static_cast<int>(ss.items.size()) - 1);
        PaintSegmented(g, rc, ss, pal, mx);
        break;
    }
    case WidgetKind::ComboBox: {
        ComboBoxState cs;
        cs.base = st;
        cs.text = node.text;
        cs.open = node.open;
        PaintComboBox(g, rc, cs, pal, mx);
        break;
    }
    case WidgetKind::Avatar: {
        AvatarState as;
        as.base = st;
        as.text = node.text.empty() ? std::wstring(L"A") : node.text;
        PaintAvatar(g, rc, as, pal, mx);
        break;
    }
    case WidgetKind::Badge: {
        BadgeState bs;
        bs.base = st;
        bs.text = node.text;
        PaintBadge(g, rc, bs, pal, mx);
        break;
    }
    }
}

} // namespace

void RenderNode(IPainter& g, const LayoutNode& node,
                const Palette& pal, const Metrics& mx,
                const RenderOptions& opts)
{
    const bool clip =
        node.type == NodeType::Scroll && opts.clipScroll && node.clip && node.Scrollable();
    if (clip) g.PushClip(node.ContentFrame());

    switch (node.type) {
    case NodeType::Label: {
        if (!node.text.empty()) {
            g.Text(node.Frame(), node.text.c_str(), node.text.size(), node.font, pal.text,
                   dt::SingleLine | dt::VCenter | HAlignToFormat(node.align));
        }
        break;
    }
    case NodeType::Control: {
        PaintControlNode(g, node, pal, mx);
        break;
    }
    default:
        break;
    }

    if (opts.debugBoxes && IsVisualContainer(node)) {
        g.StrokeRect(node.Frame(), pal.border, 1);
    }

    for (const auto& child : node.children) {
        RenderNode(g, *child, pal, mx, opts);
    }

    if (clip) g.PopClip();
}

void RenderTree(IPainter& g, const LayoutNode& root,
                const Palette& pal, const Metrics& mx,
                const RenderOptions& opts)
{
    RenderNode(g, root, pal, mx, opts);
}

} // namespace ui
} // namespace air
