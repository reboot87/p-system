#pragma once
// ============================================================
// AirUI — Canvas.h
// 画布：页面绘制的高级入口。封装原生绘制上下文 / 字体 / 背景模式等
// GDI 细节，页面代码只需要「画什么、画在哪、什么颜色」。
//
//   void MyPage::OnPaint(Canvas& g, const Rect& content) {
//       g.FillRound(rc, Pal().surface, Mx().radiusLg);
//       g.Text(rcTitle, L"标题", FontRole::Section, Pal().text);
//   }
//
// 字体通过 FontRole 语义化选取（由 Shell 按当前 DPI 创建），
// 颜色来自 Theme 调色板（Color 是纯值类型，不是 GDI 句柄）。
// ============================================================
#include <string>
#include <vector>
#include "airui/Controls.h"
#include "airui/types.h"
#include "airui/Fonts.h"
#include "airui/Theme.h"
#include "airui/DrawCall.h"

namespace air {
namespace ui {

struct EmptyState;

class Canvas
{
public:
    // 库内构造（Shell / Widget 在绘制时机传入）；页面只管收着用
    Canvas(DeviceContextHandle dc, const Fonts& fonts);
    ~Canvas();

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    // 逃生舱：极少数需要原生 GDI 的场景（一般用不到）
    DeviceContextHandle Native() const { return m_dc; }

    // 录制支持（测试用）
    static void SetRecorder(std::vector<DrawCall>* rec) { s_recorder = rec; }
    static void ClearRecorder() { s_recorder = nullptr; }

    // ---- 形状 ----
    void Fill(const Rect& rc, Color color);
    void FillRound(const Rect& rc, Color color, int radius);
    // radius 为 0 时退化为直角矩形
    void Stroke(const Rect& rc, Color color, int radius, int width = 1);
    // 虚线描边（「可点击但未激活」的提示框）
    void StrokeDashed(const Rect& rc, Color color, int radius);
    void HLine(int x1, int x2, int y, Color color);
    void HLineDashed(int x1, int x2, int y, Color color);
    void VLine(int x, int y1, int y2, Color color);
    void Dot(int cx, int cy, int diameter, Color color);
    // 进度条（track 底槽；fill 按阈值在 normal/warn/hot 间变色）
    void ProgressBar(const Rect& rc, int pct, Color track,
                     Color colorNormal, Color colorWarn, Color colorHot);

    // ---- clip (backend agnostic) ----
    // GDI uses the DeviceContextHandle clip region; a compositing backend (Skia) owns an
    // independent surface, so the DeviceContextHandle clip has no effect on it and the
    // backend clip stack must be used instead. Pages clip their content
    // through this pair so both backends produce the same pixels --
    // otherwise a control scrolled out of the content area is painted
    // over the title bar. Every PushClip needs one PopClip (unbalanced
    // pushes are cleaned up when the Canvas is destroyed).
    void PushClip(const Rect& rc);
    void PopClip();

    // ---- 文本（按角色选字体） ----
    void Text(const Rect& rc, const std::wstring& s, FontRole role,
              Color color,
              TextFormat format = dt::Left | dt::VCenter | dt::SingleLine);
    // 单行左对齐 + 垂直居中 + 尾部省略（最高频组合）
    void TextRow(const Rect& rc, const std::wstring& s, FontRole role,
                 Color color, TextFormat extra = 0);
    void TextRowRight(const Rect& rc, const std::wstring& s, FontRole role,
                      Color color, TextFormat extra = 0);
    void TextRowCenter(const Rect& rc, const std::wstring& s, FontRole role,
                       Color color, TextFormat extra = 0);
    // 多行折行 + 尾部省略
    void TextBlock(const Rect& rc, const std::wstring& s, FontRole role,
                   Color color, TextFormat extra = 0);

    int MeasureWidth(const std::wstring& s, FontRole role);
    int LineHeight(FontRole role);

    // ---- 复合部件 ----
    // 行内标签：浅底深字，圆角 2px。返回右边界 x。
    int  Tag(int x, int y, const std::wstring& s, FontRole role,
             Color bg, Color tx, int height = 20);
    // 右对齐胶囊（卡片右上角状态）
    void PillRight(int rightX, int y, const std::wstring& s, FontRole role,
                   Color bg, Color tx, int height = 20);
    // 状态胶囊：浅底 + 描边 + 同色圆点 + 深字
    void StatusTag(int rightX, int centerY, const std::wstring& s,
                   FontRole role, Color bg, Color border,
                   Color tx, Color dot, int height = 24);
    // 键位提示 chip（右对齐到 rightX）
    void KbdChip(int rightX, int centerY, const std::wstring& key,
                 const std::wstring& label);
    int  KbdChipWidth(const std::wstring& key, const std::wstring& label);
    // 图标块：圆角方块 + 居中字形（Segoe MDL2）
    void IconTile(const Rect& rc, const wchar_t* glyph, FontRole role,
                  Color bg, Color tx, int radius = 8);
    // 列表空状态（三段式引导）
    void EmptyState(const Rect& rc, const air::ui::EmptyState& state);
    // 窗口式分页指示器（第 3 项，core PaintPagination 委托版；
    // 页面用 PaginationLayout / PaginationHit 做命中：slot.page ==
    // -2 上一页 / -1 下一页 / > 0 页号）
    void Pagination(const Rect& rc, const air::ui::PaginationState& state);
    std::vector<air::ui::PaginationSlot> PaginationLayout(
        const Rect& rc, const air::ui::PaginationState& state) const;
    int PaginationHit(const Rect& rc, const air::ui::PaginationState& state,
                      Point pt) const;
    // 按钮（纯视觉；命中与点击由页面用 HotState 自己管）
    void Button(const Rect& rc, const std::wstring& text, BtnStyle style,
                bool hover = false, bool pressed = false, bool disabled = false);
    // 复选 / 单选（视觉与 Button 同属纯自绘控件；逻辑在页面）
    void Checkbox(const Rect& rc, const std::wstring& text, bool checked,
                  bool hover = false, bool disabled = false);
    void Radio(const Rect& rc, const std::wstring& text, bool checked,
               bool hover = false, bool disabled = false);

private:
    DeviceContextHandle          m_dc;
    const Fonts& m_fonts;
    // Legacy path: saved-context handles of the pushed clips. Backend path: the
    // sentinel kFrameClip marks a clip owned by the active PaintFrame.
    std::vector<int> m_clipStack;
    static std::vector<DrawCall>* s_recorder;
    static constexpr int kFrameClip = -1;
};

} // namespace ui
} // namespace air
