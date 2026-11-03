#include "airui/Layout.h"
#include <algorithm>

namespace air {
namespace ui {

// ------------------------------------------------------------
// Layout 链式 API
// ------------------------------------------------------------
Layout& Layout::Row(std::vector<LayoutSpec> items, int gap)
{
    auto node = std::make_unique<Node>();
    node->dir = Dir::Row;
    node->gap = gap;
    node->items = std::move(items);
    m_root->children.push_back(std::move(node));
    return *this;
}

Layout& Layout::Col(std::vector<LayoutSpec> items, int gap)
{
    auto node = std::make_unique<Node>();
    node->dir = Dir::Col;
    node->gap = gap;
    node->items = std::move(items);
    m_root->children.push_back(std::move(node));
    return *this;
}

Layout& Layout::Grid(std::vector<LayoutSpec> items, int cols, int hGap, int vGap)
{
    // Grid 实现为嵌套 Row/Col
    auto gridNode = std::make_unique<Node>();
    gridNode->dir = Dir::Col;
    gridNode->gap = vGap;

    for (size_t i = 0; i < items.size(); i += cols) {
        auto rowNode = std::make_unique<Node>();
        rowNode->dir = Dir::Row;
        rowNode->gap = hGap;
        for (int c = 0; c < cols && i + c < items.size(); ++c) {
            rowNode->items.push_back(items[i + c]);
        }
        gridNode->children.push_back(std::move(rowNode));
    }
    m_root->children.push_back(std::move(gridNode));
    return *this;
}

Layout& Layout::Nest(std::function<void(Layout&)> builder)
{
    Layout nested;
    builder(nested);
    // 把嵌套布局的根节点儿童移过来
    for (auto& child : nested.m_root->children) {
        m_root->children.push_back(std::move(child));
    }
    return *this;
}

// ------------------------------------------------------------
// 核心计算
// ------------------------------------------------------------
void Layout::Apply(const RECT& container)
{
    m_computed.clear();
    m_root->assigned = container;
    Compute(m_root.get(), container);
    int widgetIndex = 0;
    PlaceWidgets(m_root.get(), widgetIndex);
}

void Layout::Compute(Node* node, const RECT& rc)
{
    if (node->items.empty() && node->children.empty()) return;

    int totalFixed = 0;
    int totalGrow  = 0;
    int countItems = (int)node->items.size();

    // 先算固定尺寸总和
    for (const auto& spec : node->items) {
        if (spec.widget || spec.fixed > 0) {
            totalFixed += spec.fixed > 0 ? spec.fixed : 0;
        }
        totalGrow += spec.grow;
    }

    // 可用空间
    int containerMain = (node->dir == Dir::Row) ? (rc.right - rc.left) : (rc.bottom - rc.top);
    int containerCross = (node->dir == Dir::Row) ? (rc.bottom - rc.top) : (rc.right - rc.left);

    // 减去 gaps
    int gaps = (countItems - 1) * node->gap;
    int avail = containerMain - gaps;
    if (avail < 0) avail = 0;

    // 分配主轴尺寸
    std::vector<int> mainSizes(countItems, 0);
    int remaining = avail - totalFixed;
    if (remaining < 0) remaining = 0;

    for (int i = 0; i < countItems; ++i) {
        const auto& spec = node->items[i];
        if (spec.grow > 0 && totalGrow > 0) {
            mainSizes[i] = (remaining * spec.grow) / totalGrow;
        } else if (spec.fixed > 0) {
            mainSizes[i] = spec.fixed;
        } else {
            mainSizes[i] = 0; // 弹簧或隐藏
        }
        // clamp
        if (spec.minSize > 0 && mainSizes[i] < spec.minSize) mainSizes[i] = spec.minSize;
        if (spec.maxSize >= 0 && mainSizes[i] > spec.maxSize) mainSizes[i] = spec.maxSize;
    }

    // 剩余像素分配给第一个 grow>0 的
    int used = 0;
    for (int s : mainSizes) used += s;
    int extra = avail - used;
    if (extra > 0) {
        for (int i = 0; i < countItems && extra > 0; ++i) {
            if (node->items[i].grow > 0) {
                mainSizes[i] += extra;
                extra = 0;
            }
        }
    }

    // 交叉轴尺寸 = 容器交叉轴尺寸 - margin.top - margin.bottom
    // 位置计算
    int pos = (node->dir == Dir::Row) ? rc.left : rc.top;
    for (int i = 0; i < countItems; ++i) {
        const auto& spec = node->items[i];
        int mainSize = mainSizes[i];
        int crossSize = containerCross - spec.margin.top - spec.margin.bottom;
        if (crossSize < 0) crossSize = 0;

        int crossStart = (node->dir == Dir::Row) ? (rc.top + spec.margin.top) : (rc.left + spec.margin.left);

        // 对齐调整
        if (spec.align != LayoutAlign::Stretch && crossSize > 0) {
            int widgetCross = 0;
            // 这里需要 widget 的 preferred cross size，暂时用字体行高估算
            // 实际应该查 widget->Rect() 或预设
            switch (spec.align) {
                case LayoutAlign::Start: break;
                case LayoutAlign::Center: crossStart += (containerCross - widgetCross) / 2; break;
                case LayoutAlign::End: crossStart += containerCross - widgetCross; break;
                default: break;
            }
        }

        RECT assigned = {};
        if (node->dir == Dir::Row) {
            assigned.left   = pos + spec.margin.left;
            assigned.right  = assigned.left + mainSize - spec.margin.left - spec.margin.right;
            assigned.top    = crossStart;
            assigned.bottom = crossStart + crossSize;
        } else {
            assigned.top    = pos + spec.margin.top;
            assigned.bottom = assigned.top + mainSize - spec.margin.top - spec.margin.bottom;
            assigned.left   = crossStart;
            assigned.right  = crossStart + crossSize;
        }

        // 递归处理子节点
        if (i < (int)node->children.size() && node->children[i]) {
            node->children[i]->assigned = assigned;
        } else if (spec.widget) {
            // 记录 leaf widget 的最终矩形
            // 稍后在 PlaceWidgets 里统一处理
        }

        pos += mainSize + node->gap;
    }

    // 递归计算 children
    for (size_t i = 0; i < node->children.size(); ++i) {
        if (node->children[i]) {
            Compute(node->children[i].get(), node->children[i]->assigned);
        }
    }
}

void Layout::PlaceWidgets(Node* node, int& widgetIndex)
{
    for (size_t i = 0; i < node->items.size(); ++i) {
        const auto& spec = node->items[i];
        if (spec.widget) {
            if (widgetIndex < (int)m_computed.size()) {
                // 已有预留，更新
            } else {
                m_computed.push_back({});
            }
            RECT target = m_computed[widgetIndex];
            // 这里需要从计算结果获取，简化：直接 Move
            // 实际应该在 Compute 里存 leaf 的 assigned
            spec.widget->Move(target);
            widgetIndex++;
        }
        if (i < node->children.size() && node->children[i]) {
            PlaceWidgets(node->children[i].get(), widgetIndex);
        }
    }
}

} // namespace ui
} // namespace air