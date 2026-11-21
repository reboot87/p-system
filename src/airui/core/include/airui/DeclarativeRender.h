#pragma once
// ============================================================
// AirUI - DeclarativeRender.h
// 声明式节点树 -> IPainter 渲染桥。
//
// 只调用 core 的 IPainter 与控件绘制函数（PaintButton 等），
// 所以 GDI / Skia / PaintRecorder 走同一条路径，输出一致。
// 渲染前需要先 PrepareTree()（或 Measure + Layout）。
//
// 节点映射：
//   Label   -> IPainter::Text（font 角色 + align 水平对齐）
//   Control -> 按 widget 字段分发：PaintButton / PaintCheckbox /
//              PaintRadio / PaintToggleSwitch / PaintSlider / PaintStepper /
//              PaintProgressBar / PaintProgressRing / PaintTextField /
//              PaintTag / PaintRating / PaintSegmented / PaintComboBox /
//              PaintAvatar / PaintBadge（见 Declarative.h 的 WidgetKind）
//   Scroll  -> PushClip(视口) ... PopClip（clip=true 且可滚动时）
//   容器    -> 无视觉（debugBoxes=true 时画调试边框）
//   Spacer  -> 无视觉
// ============================================================
#include "airui/Declarative.h"
#include "airui/Theme.h"
#include "airui/hal.h"

namespace air {
namespace ui {

struct RenderOptions
{
    bool debugBoxes = false;   // 给容器画调试边框
    bool clipScroll = true;    // Scroll 视口裁剪（PushClip/PopClip）
};

// 画一个节点（含整棵子树）。
void RenderNode(IPainter& g, const LayoutNode& node,
                const Palette& pal, const Metrics& mx,
                const RenderOptions& opts = {});

// 等价于 RenderNode(g, root, ...)。
void RenderTree(IPainter& g, const LayoutNode& root,
                const Palette& pal, const Metrics& mx,
                const RenderOptions& opts = {});

} // namespace ui
} // namespace air
