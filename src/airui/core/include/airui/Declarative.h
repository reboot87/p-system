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
#include <unordered_map>
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

// Control 节点画哪种控件（JSON 的 "widget" 字段）。
// 全部映射到 core 里现成的 Paint* 函数 —— 不新增绘制代码。
enum class WidgetKind
{
    Button,       // 默认：PaintButton，style 决定配色
    Checkbox,     // PaintCheckbox，checked 状态
    Radio,        // PaintRadio，checked 状态
    Switch,       // PaintToggleSwitch，checked = 开
    Slider,       // PaintSlider，value 0..100
    ProgressBar,  // PaintProgressBar，progress 0..1
    ProgressRing, // PaintProgressRing，progress 0..1
    TextField,    // PaintTextField，text + placeholder
    Tag,          // PaintTag，selected 状态
    Stepper,      // PaintStepper，value（可点击 +/-）
    Rating,       // PaintRating，rating 0..ratingMax
    Segmented,    // PaintSegmented，options + selectedIndex
    ComboBox,     // PaintComboBox，text + open
    Avatar,       // PaintAvatar，text = 名字
    Badge,        // PaintBadge，text = 角标内容
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
    std::string  onClick;                   // 点击命令名（非空 = 可点击，事件沿父链上溯）

    // ---- Control 的控件语义（widget 决定 RenderNode 画哪个 Paint*） ----
    WidgetKind   widget = WidgetKind::Button;
    bool         checked = false;           // Checkbox/Radio/Switch/Tag(selected)
    bool         disabled = false;          // 所有控件的禁用态
    bool         open = false;              // ComboBox 下拉展开态
    int          value = 0;                 // Slider/Stepper 的当前值
    int          sliderMin = 0;             // Slider 范围
    int          sliderMax = 100;
    int          rating = 0;                // Rating 当前星数
    int          ratingMax = 5;
    int          selectedIndex = 0;         // Segmented 选中项
    std::vector<std::wstring> optionList;   // Segmented 选项（"options" 字段）
    double       progress = 0.0;            // ProgressBar/ProgressRing 0..1
    std::wstring placeholder;               // TextField 占位文案

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
    LayoutNode& SetOnClick(std::string cmd)        { onClick = std::move(cmd); return *this; }

    // ---- 控件语义 ----
    LayoutNode& SetWidget(WidgetKind w)            { widget = w; return *this; }
    LayoutNode& SetChecked(bool on)                { checked = on; return *this; }
    LayoutNode& SetDisabled(bool off)              { disabled = off; return *this; }
    LayoutNode& SetOpen(bool o)                    { open = o; return *this; }
    LayoutNode& SetValue(int v)                    { value = v; return *this; }
    LayoutNode& SetSliderRange(int lo, int hi)     { sliderMin = lo; sliderMax = hi; return *this; }
    LayoutNode& SetRating(int stars, int maxStars = 5) { rating = stars; ratingMax = maxStars; return *this; }
    LayoutNode& SetSelectedIndex(int i)            { selectedIndex = i; return *this; }
    LayoutNode& SetOptions(std::vector<std::wstring> opts) { optionList = std::move(opts); return *this; }
    LayoutNode& SetProgress(double p)              { progress = p; return *this; }
    LayoutNode& SetPlaceholder(std::wstring ph)    { placeholder = std::move(ph); return *this; }

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

// 深拷贝一棵子树（LayoutNode 不可拷贝，用于“模板树保持纯净、
// 渲染前克隆一份填绑定”的用法）。
std::unique_ptr<LayoutNode> CloneTree(const LayoutNode& src);

// ------------------------------------------------------------
// 数据绑定：文本里的 {key} 占位符 -> 值（渲染前填充进树）
// ------------------------------------------------------------
class ValueStore
{
public:
    ValueStore& Set(const std::string& key, std::wstring value);
    ValueStore& Set(const std::string& key, const wchar_t* value);
    ValueStore& Set(const std::string& key, long long value);
    ValueStore& Set(const std::string& key, double value);
    ValueStore& Set(const std::string& key, bool value);

    bool Has(const std::string& key) const;
    // 不存在的 key 返回空串。
    const std::wstring& Get(const std::string& key) const;

    // 替换 text 中的 {key}；未命中的占位符保持原样（便于发现漏绑）。
    std::wstring Resolve(const std::wstring& text) const;

    // 遍历整棵树，用 Resolve 重写每个节点的 text。
    void Apply(LayoutNode& root) const;

private:
    std::unordered_map<std::string, std::wstring> m_values;
};

// ------------------------------------------------------------
// 命中测试与点击
// ------------------------------------------------------------
// 返回包含 pt 的最深节点（后绘制的子节点优先，Frame 叠放也按此规则）；
// 无命中返回 nullptr。Scroll 开启 clip 时，内容框（视口）之外的点不会
// 命中其子树——点会落在 Scroll 容器自身（或其外层）。
LayoutNode* HitTest(LayoutNode& root, Point pt);

// 命中后沿父链上溯，找第一个 onClick 非空的节点并回调；
// 整条链都没有命令则不回调。handler 可做命令分发/状态更新/触发重绘。
void DispatchClick(LayoutNode& root, Point pt,
                   const std::function<void(LayoutNode&, const std::string& cmd)>& handler);

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
