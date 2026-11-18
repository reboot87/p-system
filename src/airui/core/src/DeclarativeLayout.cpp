// ============================================================
// AirUI - DeclarativeLayout.cpp
// 声明式布局：measure（算尺寸）/ layout（摆位置）两趟引擎。
// 平台无关，只有标准库 + types.h/Layout.h。
// ============================================================
#include "airui/Declarative.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace air {
namespace ui {

namespace {

int ClampDim(int v, int minV, int maxV)
{
    if (v < minV) v = minV;
    if (maxV >= 0 && v > maxV) v = maxV;
    return v;
}

// 父约束减去 padding：内容区可用的约束。
MeasureSpec Deflate(MeasureSpec s, int pad)
{
    if (!s.Bounded()) return s;
    int v = s.size - pad;
    if (v < 0) v = 0;
    s.size = v;
    return s;
}

// 子节点约束：Exactly 只用于根/显式定尺寸，给子节点时降为 AtMost，
// 这样 wrap_content 的子节点不会被父容器强行拉满。
MeasureSpec AtMostFrom(MeasureSpec s)
{
    if (s.mode == MeasureMode::Exactly) return MeasureSpec::AtMost(s.size);
    return s;
}

// 主轴上的弹性权重：显式 weight 优先；Match 等价于 weight=1。
float EffectiveWeight(const LayoutNode& n, bool horizontal)
{
    if (n.weight > 0.0f) return n.weight;
    const Dim& main = horizontal ? n.width : n.height;
    return main.mode == DimMode::Match ? 1.0f : 0.0f;
}

int ResolveDim(const Dim& d, MeasureSpec spec, int intrinsic, int minV, int maxV)
{
    int v = intrinsic;
    switch (d.mode) {
    case DimMode::Fixed:
        v = d.value;
        break;
    case DimMode::Match:
        v = spec.Bounded() ? spec.size : intrinsic;
        break;
    case DimMode::Wrap:
    default:
        v = intrinsic;
        if (spec.mode == MeasureMode::AtMost && v > spec.size) v = spec.size;
        if (spec.mode == MeasureMode::Exactly) v = spec.size;  // 根节点：按给定窗口
        break;
    }
    return ClampDim(v, minV, maxV);
}

} // namespace

// ------------------------------------------------------------
// 文字测量
// ------------------------------------------------------------
Size LayoutEnv::Measure(const std::wstring& t, FontRole role) const
{
    if (measureText) return measureText(t, role);
    Size s;
    s.w = static_cast<int>(t.size()) * charWidth;
    s.h = lineHeight;
    return s;
}

// ------------------------------------------------------------
// 树
// ------------------------------------------------------------
LayoutNode& LayoutNode::Add(std::unique_ptr<LayoutNode> child)
{
    if (!child) return *this;
    child->m_parent = this;
    children.push_back(std::move(child));
    return *children.back();
}

LayoutNode& LayoutNode::Add(NodeType t, std::string nodeId)
{
    return Add(std::make_unique<LayoutNode>(t, std::move(nodeId)));
}

LayoutNode* LayoutNode::Find(const std::string& nodeId)
{
    if (!nodeId.empty() && id == nodeId) return this;
    for (auto& c : children) {
        if (LayoutNode* hit = c->Find(nodeId)) return hit;
    }
    return nullptr;
}

const LayoutNode* LayoutNode::Find(const std::string& nodeId) const
{
    if (!nodeId.empty() && id == nodeId) return this;
    for (const auto& c : children) {
        if (const LayoutNode* hit = c->Find(nodeId)) return hit;
    }
    return nullptr;
}

// ------------------------------------------------------------
// Measure：算尺寸（自底向上求 intrinsic，再按约束收敛）
// ------------------------------------------------------------
void LayoutNode::Measure(MeasureSpec wSpec, MeasureSpec hSpec, const LayoutEnv& env)
{
    const int padX = padding.left + padding.right;
    const int padY = padding.top + padding.bottom;
    const MeasureSpec cw = Deflate(wSpec, padX);
    const MeasureSpec ch = Deflate(hSpec, padY);

    int intrinsicW = 0;
    int intrinsicH = 0;

    switch (type) {
    case NodeType::Label: {
        if (intrinsic.w > 0 || intrinsic.h > 0) {
            intrinsicW = intrinsic.w;
            intrinsicH = intrinsic.h;
        } else {
            const Size s = env.Measure(text, font);
            intrinsicW = s.w;
            intrinsicH = s.h;
        }
        break;
    }

    case NodeType::Control:
    case NodeType::Spacer: {
        intrinsicW = intrinsic.w > 0 ? intrinsic.w : minW;
        intrinsicH = intrinsic.h > 0 ? intrinsic.h : minH;
        break;
    }

    case NodeType::Row:
    case NodeType::Column: {
        const bool horizontal = (type == NodeType::Row);
        const int n = static_cast<int>(children.size());

        // 主轴约束：Fixed/Wrap 给 AtMost，Match/weight 给 Unspecified（先量 intrinsic）。
        for (auto& c : children) {
            const Dim& mainDim = horizontal ? c->width : c->height;
            const bool elastic = c->weight > 0.0f || mainDim.mode == DimMode::Match;
            MeasureSpec mainSpec;
            if (elastic) {
                mainSpec = MeasureSpec::Unspecified();
            } else {
                const int avail = horizontal ? cw.size : ch.size;
                mainSpec = (horizontal ? cw : ch).Bounded() ? MeasureSpec::AtMost(avail)
                                                            : MeasureSpec::Unspecified();
            }
            const MeasureSpec crossSpec = AtMostFrom(horizontal ? ch : cw);
            if (horizontal) {
                c->Measure(mainSpec, crossSpec, env);
            } else {
                c->Measure(crossSpec, mainSpec, env);
            }
        }

        int mainSum = 0;
        int crossMax = 0;
        for (auto& c : children) {
            if (horizontal) {
                mainSum += c->MeasuredWidth() + c->margin.left + c->margin.right;
                crossMax = std::max(crossMax, c->MeasuredHeight() + c->margin.top + c->margin.bottom);
            } else {
                mainSum += c->MeasuredHeight() + c->margin.top + c->margin.bottom;
                crossMax = std::max(crossMax, c->MeasuredWidth() + c->margin.left + c->margin.right);
            }
        }
        if (n > 1) mainSum += gap * (n - 1);

        intrinsicW = horizontal ? mainSum : crossMax;
        intrinsicH = horizontal ? crossMax : mainSum;
        break;
    }

    case NodeType::Frame: {
        for (auto& c : children) c->Measure(AtMostFrom(cw), AtMostFrom(ch), env);
        for (auto& c : children) {
            intrinsicW = std::max(intrinsicW, c->MeasuredWidth() + c->margin.left + c->margin.right);
            intrinsicH = std::max(intrinsicH, c->MeasuredHeight() + c->margin.top + c->margin.bottom);
        }
        break;
    }

    case NodeType::Grid: {
        const int cols = std::max(1, columns);
        const int n = static_cast<int>(children.size());
        const int hGap = gap;
        const int vGap = gap;

        MeasureSpec cellSpec = MeasureSpec::Unspecified();
        if (cw.Bounded()) {
            const int avail = cw.size - hGap * (cols - 1);
            cellSpec = MeasureSpec::AtMost(std::max(0, cols > 0 ? avail / cols : avail));
        }
        for (auto& c : children) c->Measure(cellSpec, AtMostFrom(ch), env);

        int cellW = 0;
        int cellH = 0;
        for (auto& c : children) {
            cellW = std::max(cellW, c->MeasuredWidth() + c->margin.left + c->margin.right);
            cellH = std::max(cellH, c->MeasuredHeight() + c->margin.top + c->margin.bottom);
        }
        const int rows = n > 0 ? (n + cols - 1) / cols : 0;
        intrinsicW = cols * cellW + hGap * (cols - 1);
        intrinsicH = rows * cellH + vGap * std::max(0, rows - 1);
        break;
    }

    case NodeType::Scroll: {
        for (auto& c : children) {
            const MeasureSpec ws = cw.Bounded() ? MeasureSpec::AtMost(cw.size)
                                                : MeasureSpec::Unspecified();
            c->Measure(ws, MeasureSpec::Unspecified(), env);
        }
        const int n = static_cast<int>(children.size());
        int sumH = 0;
        for (auto& c : children) {
            intrinsicW = std::max(intrinsicW, c->MeasuredWidth() + c->margin.left + c->margin.right);
            sumH += c->MeasuredHeight() + c->margin.top + c->margin.bottom;
        }
        if (n > 1) sumH += gap * (n - 1);
        intrinsicH = sumH;
        m_scrollContentH = sumH + padY;
        break;
    }
    }

    m_measuredW = ResolveDim(width, wSpec, intrinsicW + padX, minW, maxW);
    m_measuredH = ResolveDim(height, hSpec, intrinsicH + padY, minH, maxH);
}

// ------------------------------------------------------------
// Layout：按父给定的盒子摆位置（自顶向下）
// ------------------------------------------------------------
void LayoutNode::Layout(const Rect& bounds)
{
    m_frame = bounds;
    m_content = Rect{bounds.left + padding.left,
                     bounds.top + padding.top,
                     bounds.right - padding.right,
                     bounds.bottom - padding.bottom};
    if (m_content.right < m_content.left) m_content.right = m_content.left;
    if (m_content.bottom < m_content.top) m_content.bottom = m_content.top;
    PlaceChildren();
}

void LayoutNode::PlaceChildren()
{
    switch (type) {
    case NodeType::Row:    PlaceLinear(true);  break;
    case NodeType::Column: PlaceLinear(false); break;
    case NodeType::Frame:  PlaceFrame();       break;
    case NodeType::Grid:   PlaceGrid();        break;
    case NodeType::Scroll: PlaceScroll();      break;
    default: break;  // Label / Control / Spacer：叶子，无子节点
    }
}

void LayoutNode::PlaceLinear(bool horizontal)
{
    const int n = static_cast<int>(children.size());
    if (n == 0) return;

    float totalWeight = 0.0f;
    for (auto& c : children) totalWeight += EffectiveWeight(*c, horizontal);

    const int mainSize = horizontal ? m_content.Width() : m_content.Height();
    const int crossSize = horizontal ? m_content.Height() : m_content.Width();

    int used = gap * (n - 1);
    for (auto& c : children) {
        if (EffectiveWeight(*c, horizontal) <= 0.0f) {
            used += horizontal ? c->MeasuredWidth() + c->margin.left + c->margin.right
                               : c->MeasuredHeight() + c->margin.top + c->margin.bottom;
        }
    }
    const int remain = std::max(0, mainSize - used);

    int cursor = horizontal ? m_content.left : m_content.top;
    for (auto& c : children) {
        const float w = EffectiveWeight(*c, horizontal);

        int main;
        if (w > 0.0f && totalWeight > 0.0f) {
            main = static_cast<int>(std::lround(static_cast<double>(remain) * (w / totalWeight)));
        } else {
            main = horizontal ? c->MeasuredWidth() : c->MeasuredHeight();
        }
        main = ClampDim(main, horizontal ? c->minW : c->minH,
                              horizontal ? c->maxW : c->maxH);
        if (main < 0) main = 0;

        const Dim& crossDim = horizontal ? c->height : c->width;
        const int crossMargin = horizontal ? c->margin.top + c->margin.bottom
                                           : c->margin.left + c->margin.right;
        int cross;
        if (c->align == LayoutAlign::Stretch && crossDim.mode == DimMode::Match) {
            cross = crossSize - crossMargin;
        } else {
            cross = horizontal ? c->MeasuredHeight() : c->MeasuredWidth();
        }
        cross = ClampDim(cross, horizontal ? c->minH : c->minW,
                                horizontal ? c->maxH : c->maxW);
        if (cross < 0) cross = 0;

        const int crossStart = horizontal ? m_content.top : m_content.left;
        const int marginStart = horizontal ? c->margin.top : c->margin.left;
        const int marginEnd = horizontal ? c->margin.bottom : c->margin.right;

        int crossPos;
        switch (c->align) {
        case LayoutAlign::Center: crossPos = crossStart + (crossSize - cross) / 2; break;
        case LayoutAlign::End:    crossPos = crossStart + crossSize - marginEnd - cross; break;
        default:                  crossPos = crossStart + marginStart; break;
        }

        const int mainPos = cursor + (horizontal ? c->margin.left : c->margin.top);
        const Rect r = horizontal
            ? Rect{mainPos, crossPos, mainPos + main, crossPos + cross}
            : Rect{crossPos, mainPos, crossPos + cross, mainPos + main};
        c->Layout(r);

        cursor = mainPos + main + (horizontal ? c->margin.right : c->margin.bottom) + gap;
    }
}

void LayoutNode::PlaceFrame()
{
    for (auto& c : children) {
        int w = c->MeasuredWidth();
        int h = c->MeasuredHeight();
        if (c->width.mode == DimMode::Match)  w = m_content.Width();
        if (c->height.mode == DimMode::Match) h = m_content.Height();
        w = ClampDim(w, c->minW, c->maxW);
        h = ClampDim(h, c->minH, c->maxH);
        if (w < 0) w = 0;
        if (h < 0) h = 0;

        int x;
        switch (c->align) {
        case LayoutAlign::Center: x = m_content.left + (m_content.Width() - w) / 2; break;
        case LayoutAlign::End:    x = m_content.right - c->margin.right - w; break;
        default:                  x = m_content.left + c->margin.left; break;
        }

        int y;
        switch (c->align) {
        case LayoutAlign::Center: y = m_content.top + (m_content.Height() - h) / 2; break;
        case LayoutAlign::End:    y = m_content.bottom - c->margin.bottom - h; break;
        default:                  y = m_content.top + c->margin.top; break;
        }

        c->Layout(Rect{x, y, x + w, y + h});
    }
}

void LayoutNode::PlaceGrid()
{
    const int n = static_cast<int>(children.size());
    if (n == 0) return;

    const int cols = std::max(1, columns);
    const int rows = (n + cols - 1) / cols;
    const int hGap = gap;
    const int vGap = gap;
    const int availW = std::max(0, m_content.Width());
    const int cellW = cols > 1 ? std::max(0, (availW - hGap * (cols - 1)) / cols) : availW;

    int cellH = 0;
    for (auto& c : children) {
        cellH = std::max(cellH, c->MeasuredHeight() + c->margin.top + c->margin.bottom);
    }
    if (height.mode == DimMode::Match || height.mode == DimMode::Fixed) {
        const int availH = std::max(0, m_content.Height() - vGap * (rows - 1));
        const int fit = rows > 0 ? availH / rows : availH;
        if (fit > cellH) cellH = fit;
    }

    for (int i = 0; i < n; ++i) {
        LayoutNode& c = *children[static_cast<size_t>(i)];
        const int r = i / cols;
        const int col = i % cols;

        const int x = m_content.left + col * (cellW + hGap) + c.margin.left;
        const int y = m_content.top + r * (cellH + vGap) + c.margin.top;

        int w = std::max(0, cellW - c.margin.left - c.margin.right);
        int h = std::max(0, cellH - c.margin.top - c.margin.bottom);
        if (c.width.mode == DimMode::Fixed)  w = c.width.value;
        if (c.height.mode == DimMode::Fixed) h = c.height.value;
        w = ClampDim(w, c.minW, c.maxW);
        h = ClampDim(h, c.minH, c.maxH);

        c.Layout(Rect{x, y, x + w, y + h});
    }
}

void LayoutNode::PlaceScroll()
{
    int contentH = padding.top + padding.bottom;
    int y = m_content.top - m_scrollY;

    for (auto& c : children) {
        int w = c->width.mode == DimMode::Match ? m_content.Width() : c->MeasuredWidth();
        int h = c->MeasuredHeight();
        w = ClampDim(w, c->minW, c->maxW);
        h = ClampDim(h, c->minH, c->maxH);
        if (w < 0) w = 0;
        if (h < 0) h = 0;

        const int x = m_content.left + c->margin.left;
        const int top = y + c->margin.top;
        c->Layout(Rect{x, top, x + w, top + h});

        y = top + h + c->margin.bottom + gap;
        contentH += c->margin.top + h + c->margin.bottom + gap;
    }
    if (!children.empty() && gap > 0) contentH -= gap;

    m_scrollContentH = contentH;
    const int maxScroll = std::max(0, m_scrollContentH - m_frame.Height());
    if (m_scrollY > maxScroll) m_scrollY = maxScroll;
}

// ------------------------------------------------------------
// 滚动
// ------------------------------------------------------------
void LayoutNode::SetScrollY(int y)
{
    const int maxScroll = std::max(0, m_scrollContentH - m_frame.Height());
    if (y < 0) y = 0;
    if (y > maxScroll) y = maxScroll;
    m_scrollY = y;
}

bool LayoutNode::Scrollable() const
{
    return type == NodeType::Scroll && m_scrollContentH > m_frame.Height();
}

// ------------------------------------------------------------
// 入口
// ------------------------------------------------------------
void MeasureTree(LayoutNode& root, MeasureSpec w, MeasureSpec h, const LayoutEnv& env)
{
    root.Measure(w, h, env);
}

void PrepareTree(LayoutNode& root, const Rect& bounds, const LayoutEnv& env)
{
    root.Measure(MeasureSpec::Exactly(bounds.Width()),
                 MeasureSpec::Exactly(bounds.Height()), env);
    root.Layout(bounds);
}

// ------------------------------------------------------------
// ValueStore：{key} 占位符绑定
// ------------------------------------------------------------
ValueStore& ValueStore::Set(const std::string& key, std::wstring value)
{
    m_values[key] = std::move(value);
    return *this;
}

ValueStore& ValueStore::Set(const std::string& key, const wchar_t* value)
{
    m_values[key] = value ? std::wstring(value) : std::wstring();
    return *this;
}

ValueStore& ValueStore::Set(const std::string& key, long long value)
{
    wchar_t buf[32] = {};
    std::swprintf(buf, 32, L"%lld", value);
    m_values[key] = buf;
    return *this;
}

ValueStore& ValueStore::Set(const std::string& key, double value)
{
    wchar_t buf[64] = {};
    if (value == static_cast<double>(static_cast<long long>(value)) &&
        std::fabs(value) < 1e15) {
        std::swprintf(buf, 64, L"%lld", static_cast<long long>(value));
    } else {
        std::swprintf(buf, 64, L"%.6g", value);
    }
    m_values[key] = buf;
    return *this;
}

ValueStore& ValueStore::Set(const std::string& key, bool value)
{
    m_values[key] = value ? L"true" : L"false";
    return *this;
}

bool ValueStore::Has(const std::string& key) const
{
    return m_values.find(key) != m_values.end();
}

const std::wstring& ValueStore::Get(const std::string& key) const
{
    static const std::wstring kEmpty;
    const auto it = m_values.find(key);
    return it != m_values.end() ? it->second : kEmpty;
}

std::wstring ValueStore::Resolve(const std::wstring& text) const
{
    if (m_values.empty() || text.empty()) return text;

    std::wstring out;
    out.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] != L'{') {
            out += text[i++];
            continue;
        }
        const size_t close = text.find(L'}', i + 1);
        if (close == std::wstring::npos) {  // 没有配对：原样保留
            out += text[i++];
            continue;
        }
        // key 只取 ASCII（绑定名是代码资产）
        const std::wstring raw = text.substr(i + 1, close - i - 1);
        std::string key;
        bool ascii = !raw.empty();
        for (wchar_t wc : raw) {
            if (wc < 0 || wc > 0x7F) { ascii = false; break; }
            key += static_cast<char>(wc);
        }
        if (ascii && Has(key)) {
            out += Get(key);
            i = close + 1;
        } else {  // 未命中/非 ASCII key：原样保留整个占位符
            out += text[i++];
        }
    }
    return out;
}

void ValueStore::Apply(LayoutNode& root) const
{
    root.text = Resolve(root.text);
    for (auto& child : root.children) Apply(*child);
}

// ------------------------------------------------------------
// 命中测试与点击
// ------------------------------------------------------------
LayoutNode* HitTest(LayoutNode& root, Point pt)
{
    if (!root.Frame().Contains(pt)) return nullptr;

    // Scroll 裁剪：视口（内容框）之外的点不进入子树。
    const bool clipped =
        root.type == NodeType::Scroll && root.clip &&
        !root.ContentFrame().Contains(pt);

    if (!clipped) {
        // 逆序遍历：后加入的子节点绘制在上层，优先命中。
        for (auto it = root.children.rbegin(); it != root.children.rend(); ++it) {
            if (LayoutNode* hit = HitTest(**it, pt)) return hit;
        }
    }
    return &root;  // 子树都没命中，但点在自身盒子里 -> 命中容器本身
}

void DispatchClick(LayoutNode& root, Point pt,
                   const std::function<void(LayoutNode&, const std::string& cmd)>& handler)
{
    if (!handler) return;
    LayoutNode* hit = HitTest(root, pt);
    if (!hit) return;
    for (LayoutNode* n = hit; n; n = n->Parent()) {
        if (!n->onClick.empty()) {
            handler(*n, n->onClick);
            return;
        }
    }
}

} // namespace ui
} // namespace air
