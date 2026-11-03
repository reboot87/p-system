#pragma once
// ============================================================
// AirUI — GalleryDraw.h
// 组件画廊的共享绘制代码，供无头截图和 GUI 预览程序复用。
// ============================================================
#include "airui/Canvas.h"

namespace air {
namespace ui {
namespace gallery {

// 画廊总高度（逻辑像素，top 为 0 时）
constexpr int kGalleryHeight = 1480;

// 在 Canvas 上绘制完整组件画廊。
// left/top 为左上角逻辑坐标，width 为可用宽度。
// dark 仅影响右上角主题状态胶囊的文字。
void DrawGallery(Canvas& g, int left, int top, int width, bool dark);

} // namespace gallery
} // namespace ui
} // namespace air
