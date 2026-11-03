// ============================================================
// AirUI — GalleryDraw.h
// 组件画廊的共享绘制代码，供无头截图和 GUI 预览程序复用。
// ============================================================
#include "airui/Canvas.h"
#include "airui/Components.h"

namespace air {
namespace ui {
namespace gallery {

// 画廊总高度（逻辑像素，top 为 0 时）
constexpr int kGalleryHeight = 1480;

// 组件布局信息（用于 GUI 程序实例化组件）
struct ComponentLayout
{
    Card::Config cardConfig; 
    RECT drawRect;
    enum Type { CARD_TYPE } type;
};

// 布局计算函数（不绘制，仅返回组件信息）
std::vector<ComponentLayout> CalculateGalleryLayout(int left, int top, int width);

// 原有绘制函数（无头截图使用）
void DrawGallery(Canvas& g, int left, int top, int width, bool dark);

// 静态绘制辅助函数（无头截图使用）
static void Section(Canvas& g, int x, int y, int right,
                    const wchar_t* title, const wchar_t* desc);
static void Swatch(Canvas& g, const RECT& rc, COLORREF color,
                   const std::wstring& name);
static void ButtonMatrix(Canvas& g, int x, int y);
static void PrimitiveSection(Canvas& g, int x, int y, int right);
static void TypographyPanel(Canvas& g, const RECT& rc);
static void EmptyPanel(Canvas& g, const RECT& rc);

} // namespace gallery
} // namespace ui
} // namespace air