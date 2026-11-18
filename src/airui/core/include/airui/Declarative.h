#pragma once
// ============================================================
// AirUI - Declarative.h
// 声明式布局的节点模型 + measure/layout 两趟引擎（Android 模型）。
// 平台无关：只依赖 types.h / Layout.h，core 里可直接单测。
//
// 用法：
//     LayoutNode root(NodeType::Column);
//     root.SetPadding({16}).SetGap(8);
//     root.Add(NodeType::Label).SetText(L"Hello").SetHeight(Dim::Fixed(24));
//     root.Add(NodeType::Control).SetSize(Dim::Match(), Dim::Fixed(32))
//                                  .SetWeight(1).SetId("ok");
//     LayoutEnv env;                       // 可选：注入真实文字测量
//     PrepareTree(root, Rect{0,0,400,300}, env);
//     // 之后读 node.Frame() / node.ContentFrame() 即可绘制 + 命中
// ============================================================
#include "airui/types.h"
#include "airui/hal.h"
#include "airui/Layout.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace air {
namespace ui {

// ------------------------------------------------------------
// 节点类型
// ------------------------------------------------------------
enum class NodeType
{
    Column,   // 竖向堆叠（LinearLayout vertical）
    Row,      // 横向堆叠（LinearLayout horizontal）
    Frame,    // 层叠（FrameLayout）
    Grid,     // 网格（GridLayout，columns 列，等宽）
    Scroll,   // 竖向滚动容器（内容可超出）
    Label,    // 文本（纯测量 + 后续绘制）
    Control,  // 一个控件占位（按钮/输入框…，由绘制层决定长什么样）
    Spacer    // 弹性空白（配合 weight 撑开）
};

// ------------------------------------------------------------
// 尺寸规格（wrap_content / match_parent / 固定）
// ------------------------------------------------------------
enum class DimMode
{
    Wrap,   // 包裹内容
    Match,  // 填满父容器（主轴上等价于 weight=1）
    Fixed   // 固定像素
};

struct Dim
{
    DimMode mode = DimMode::Wrap;
    int     value = 0;      // 仅 Fixed 有效

    static constexpr Dim Wrap()             { return Dim{DimMode::Wrap, 0}; }
    static constexpr Dim Match()            { return Dim{DimMode::Match, 0}; }
    static constexpr Dim Fixed(int pixels)  { return Dim{DimMode::Fixed, pixels}; }
};

// ------------------------------------------------------------
// 测量约束（Android MeasureSpec 的简化版）
// ------------------------------------------------------------
enum class MeasureMode
{
    Unspecified,  // 父容器不限制
    Exactly,      // 必须正好是这个尺寸
    AtMost        // 最多这个尺寸
};

struct MeasureSpec
{
    MeasureMode mode = MeasureMode::Unspecified;
    int         size = 0;

    static constexpr MeasureSpec Unspecified()      { return MeasureSpec{MeasureMode::Unspecified, 0}; }
    static constexpr MeasureSpec Exactly(int s)     { return MeasureSpec{MeasureMode::Exactly, s}; }
    static constexpr MeasureSpec AtMost(int s)      { return MeasureSpec{MeasureMode::AtMost, s}; }

    bool Bounded() const { return mode != MeasureMode::Unspecified; }

    // 把期望尺寸收敛进约束：Exactly 强制、AtMost 截断、Unspecified 放行。
    int Resolve(int desired) const
    {
        switch (mode) {
        case MeasureMode::Exactly: return size;
        case MeasureMode::AtMost:  return desired < size ? desired : size;
        case MeasureMode::Unspecified: break;
        }
        return desired;
    }
};

// ------------------------------------------------------------
// 文字测量注入：无 IPainter 时用内置估算（便于单测/无后端场景）。
// ------------------------------------------------------------
using MeasureTextFn = std::function<Size(const std::wstring& text, FontRole role)>;

struct LayoutEnv
{
    MeasureTextFn measureText;   // 可为空 -> 内置估算
    int charWidth  = 8;          // 内置估算：每字符宽
    int lineHeight = 18;         // 内置估算：单行高

    Size Measure(const std::wstring& text, FontRole role) const;
};

// ------------------------------------------------------------
// 节点
// ------------------------------------------------------------
class LayoutNode
{
public:
    explicit LayoutNode(NodeType t = NodeType::Column, std::string nodeId = {})
        : type(t), id(std::move(nodeId)) {}

    LayoutNode(const LayoutNode&) = delete;
    LayoutNode& operator=(const LayoutNode&) = delete;

    // ---- 参数 ----
    NodeType     type = NodeType::Column;
    std::string  id;                        // 便于按 id 查找（绑定/事件用）
    Dim          width  = Dim::Wrap();
    Dim          height = Dim::Wrap();
    int          minW = 0, minH = 0;        // 最小尺寸（0 = 不限）
    int          maxW = -1, maxH = -1;      // 最大尺寸（-1 = 不限）
    LayoutMargins margin{};                 // 外边距（父容器负责让位）
    LayoutMargins padding{};                // 内边距（内容区缩进）
    float        weight = 0.0f;             // 主轴剩余空间占比
    LayoutAlign  align = LayoutAlign::Stretch;  // 交叉轴对齐
    int          gap = 0;                   // Row/Column/Scroll/Grid 间距
    int          columns = 2;               // Grid 列数
    std::wstring text;                      // Label 文本 / Control 文案
    std::wstring style;                     // 绘制层样式标记（如 "primary"）
    FontRole     font = FontRole::Body;
    Size         intrinsic{};               // Label/Control 期望尺寸（0 = 由测量得出）
    bool         clip = true;               // Scroll: 裁掉超出盒子的内容

    // ---- 便捷链式设置 ----
    LayoutNode& SetId(std::string nodeId)          { id = std::move(nodeId); return *this; }
    LayoutNode& SetSize(Dim w, Dim h)              { width = w; height = h; return *this; }
    LayoutNode& SetWidth(Dim w)                    { width = w; return *this; }
    LayoutNode& SetHeight(Dim h)                   { height = h; return *this; }
    LayoutNode& SetMinSize(int w, int h)           { minW = w; minH = h; return *this; }
    LayoutNode& SetMaxSize(int w, int h)           { maxW = w; maxH = h; return *this; }
    LayoutNode& SetMargin(LayoutMargins m)         { margin = m; return *this; }
    LayoutNode& SetPadding(LayoutMargins p)        { padding = p; return *this; }
    LayoutNode& SetWeight(float w)                 { weight = w; return *this; }
    LayoutNode& SetAlign(LayoutAlign a)            { align = a; return *this; }
    LayoutNode& SetGap(int g)                      { gap = g; return *this; }
    LayoutNode& SetColumns(int c)                  { columns = c < 1 ? 1 : c; return *this; }
    LayoutNode& SetText(std::wstring t)            { text = std::move(t); return *this; }
    LayoutNode& SetStyle(std::wstring s)           { style = std::move(s); return *this; }
    LayoutNode& SetFont(FontRole f)                { font = f; return *this; }
    LayoutNode& SetIntrinsic(Size s)               { intrinsic = s; return *this; }

    // ---- 树 ----
    std::vector<std::unique_ptr<LayoutNode>> children;

    LayoutNode& Add(std::unique_ptr<LayoutNode> child);
    LayoutNode& Add(NodeType t, std::string nodeId = {});
    LayoutNode* Parent() { return m_parent; }
    const LayoutNode* Parent() const { return m_parent; }
    LayoutNode* Find(const std::string& nodeId);
    const LayoutNode* Find(const std::string& nodeId) const;

    // ---- 测量结果 ----
    int MeasuredWidth()  const { return m_measuredW; }
    int MeasuredHeight() const { return m_measuredH; }
    const Rect& Frame() const { return m_frame; }               // 含 padding 的盒子
    const Rect& ContentFrame() const { return m_content; }      // 内容区（已扣 padding）

    // ---- 两趟 ----
    void Measure(MeasureSpec w, MeasureSpec h, const LayoutEnv& env);
    void Layout(const Rect& bounds);

    // ---- 滚动 ----
    int  ScrollY() const { return m_scrollY; }
    void SetScrollY(int y);
    int  ScrollContentHeight() const { return m_scrollContentH; }
    bool Scrollable() const;

private:
    void PlaceChildren();
    void PlaceLinear(bool horizontal);
    void PlaceFrame();
    void PlaceGrid();
    void PlaceScroll();

    LayoutNode* m_parent = nullptr;
    int   m_measuredW = 0;
    int   m_measuredH = 0;
    Rect  m_frame{};
    Rect  m_content{};
    int   m_scrollY = 0;
    int   m_scrollContentH = 0;
};

// ------------------------------------------------------------
// 一步到位：把整棵树测量 + 摆放到 bounds。
// ------------------------------------------------------------
void PrepareTree(LayoutNode& root, const Rect& bounds, const LayoutEnv& env = {});

// 便利入口：root 尺寸由 bounds 的 Exactly 约束决定。
void MeasureTree(LayoutNode& root, MeasureSpec w, MeasureSpec h, const LayoutEnv& env = {});

// ------------------------------------------------------------
// JSON 布局描述（布局文件 -> 节点树）
//
//   { "type": "column", "padding": 16, "gap": 12, "children": [
//       { "type": "label", "text": "Hello", "font": "title", "h": 24 },
//       { "type": "row", "gap": 8, "children": [
//           { "type": "control", "id": "ok", "w": 72, "h": 32 },
//           { "type": "spacer", "weight": 1 } ] } ] }
// ------------------------------------------------------------
struct InflateError
{
    bool        ok = true;
    int         line = 0;      // 1-based；0 表示与位置无关的错误
    int         column = 0;
    std::string message;

    void Reset() { ok = true; line = 0; column = 0; message.clear(); }
    void Fail(int l, int c, std::string msg)
    {
        ok = false;
        line = l;
        column = c;
        message = std::move(msg);
    }
    // "line:col: message"（便于日志/报错）
    std::string Describe() const;
};

// 成功返回根节点；失败返回 nullptr，并在 err（若非空）给出行列号与原因。
std::unique_ptr<LayoutNode> InflateLayout(const std::string& utf8Json, InflateError* err = nullptr);

} // namespace ui
} // namespace air
