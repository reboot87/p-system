#pragma once
// ============================================================
// AirUI — Layout.h
// Flex/Grid 布局系统：消除手写 Rect 计算
//
// 用法：
//     Layout()
//         .Row({ {search, 1}, {chip, 0, 80}, {nullptr, 1} }, 8)
//         .Col({ {list, 1}, {detail, 0, 0, 300} }, 12)
//         .Apply(content);
// ============================================================
#include "airui/types.h"
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

namespace air {
namespace ui {

// Minimal interface for anything a layout can position. This keeps
// core free of any concrete widget / windowing dependency.
class ILayoutItem
{
public:
    virtual ~ILayoutItem() = default;
    virtual void Move(const Rect& rc) = 0;
};

// ------------------------------------------------------------
// 规格
// ------------------------------------------------------------
enum class LayoutAlign
{
    Start,
    Center,
    End,
    Stretch
};

struct LayoutMargins
{
    int left = 0, top = 0, right = 0, bottom = 0;
    LayoutMargins() = default;
    LayoutMargins(int all) : left(all), top(all), right(all), bottom(all) {}
    LayoutMargins(int v, int h) : left(h), top(v), right(h), bottom(v) {}
    LayoutMargins(int l, int t, int r, int b) : left(l), top(t), right(r), bottom(b) {}
};

struct LayoutSpec
{
    ILayoutItem* widget   = nullptr;
    int         grow     = 0;
    int         minSize  = 0;
    int         maxSize  = -1;
    int         fixed    = 0;
    LayoutMargins margin = {};
    LayoutAlign align    = LayoutAlign::Stretch;
};

// ------------------------------------------------------------
// Layout 入口（链式 API）
// ------------------------------------------------------------
class Layout
{
public:
    enum class Dir { Row, Col };

    Layout() { m_root->forward = true; }

    Layout& Row(std::vector<LayoutSpec> items, int gap = 8);
    Layout& Col(std::vector<LayoutSpec> items, int gap = 8);
    Layout& Grid(std::vector<LayoutSpec> items, int cols, int hGap = 8, int vGap = 8);
    Layout& Nest(std::function<void(Layout&)> builder);
    void Apply(const Rect& container);
    const std::vector<Rect>& ComputedRects() const { return m_computed; }

private:
    struct Node
    {
        Dir dir = Dir::Row;
        int gap = 8;
        std::vector<LayoutSpec> items;
        std::vector<std::unique_ptr<Node>> children;
        Rect assigned = {};
        // The implicit root forwards its rect to every child (chained
        // .Row().Col() calls share the same container). Nested container
        // nodes instead stack their children along their own direction.
        bool forward = false;
    };

    std::unique_ptr<Node> m_root = std::make_unique<Node>();
    std::vector<Rect> m_computed;
    std::unordered_map<ILayoutItem*, Rect> m_widgetRects;

    void Compute(Node* node, const Rect& rc);
    void PlaceWidgets(Node* node, int& widgetIndex);
};

} // namespace ui
} // namespace air