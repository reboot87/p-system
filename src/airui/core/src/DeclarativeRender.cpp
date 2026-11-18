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
        ControlState st;  // 声明式树暂不持有 hover/pressed，交互态由外层控件宿主叠加
        PaintButton(g, node.Frame(), node.text, StyleFromToken(node.style), st, pal, mx);
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
