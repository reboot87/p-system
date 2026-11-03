#pragma once
// ============================================================
// AirUI — Layout.h
// Flex/Grid 布局系统：消除手写 RECT 计算
//
// 用法：
//     Layout()
//         .Row({ {search, 1}, {chip, 0, 80}, {nullptr, 1} }, 8)
//         .Col({ {list, 1}, {detail, 0, 0, 300} }, 12)
//         .Apply(content);
// ============================================================
#include <windows.h>
#include <vector>
#include <functional>
#include <memory>
#include "Widgets.h"

namespace air {
namespace ui {

class Widget;

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
    Widget*     widget   = nullptr;
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

    Layout() = default;

    Layout& Row(std::vector<LayoutSpec> items, int gap = 8);
    Layout& Col(std::vector<LayoutSpec> items, int gap = 8);
    Layout& Grid(std::vector<LayoutSpec> items, int cols, int hGap = 8, int vGap = 8);
    Layout& Nest(std::function<void(Layout&)> builder);
    void Apply(const RECT& container);
    const std::vector<RECT>& ComputedRects() const { return m_computed; }

private:
    struct Node
    {
        Dir dir = Dir::Row;
        int gap = 8;
        std::vector<LayoutSpec> items;
        std::vector<std::unique_ptr<Node>> children;
        RECT assigned = {};
    };

    std::unique_ptr<Node> m_root = std::make_unique<Node>();
    std::vector<RECT> m_computed;

    void Compute(Node* node, const RECT& rc);
    void PlaceWidgets(Node* node, int& widgetIndex);
};

} // namespace ui
} // namespace air