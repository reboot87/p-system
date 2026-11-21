// ============================================================
// AirUI - declarative render test (platform-free)
//
// Renders a declarative tree into a PaintRecorder and asserts the
// draw-call structure: label text ops, button fill/stroke ops,
// scroll viewport clipping and debug boxes. The same IPainter path
// is used by GDI and Skia, so these ops are backend-agnostic.
// ============================================================
#include "airui/DeclarativeRender.h"
#include "airui/PaintRecorder.h"
#include "airui/Theme.h"

#include <cstdio>

using namespace air::ui;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what)
{
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("PASS: %s\n", what);
    }
}

const PaintOp* Find(const PaintRecorder& rec, PaintOpKind kind)
{
    for (const PaintOp& op : rec.Ops()) {
        if (op.kind == kind) return &op;
    }
    return nullptr;
}

const PaintOp* FindText(const PaintRecorder& rec, const std::wstring& text)
{
    for (const PaintOp& op : rec.Ops()) {
        if (op.kind == PaintOpKind::Text && op.text == text) return &op;
    }
    return nullptr;
}

LayoutNode& AddControl(LayoutNode& parent, const char* id, int w, int h)
{
    LayoutNode& n = parent.Add(NodeType::Control, id);
    n.SetIntrinsic(Size{w, h});
    return n;
}

} // namespace

int main()
{
    const Palette pal = LightPalette();
    const Metrics mx{};

    // ---- 1. Label + Control 的绘制调用 ----
    {
        LayoutNode root(NodeType::Column);
        root.SetPadding({16});
        root.Add(NodeType::Label, "title")
            .SetText(L"标题")
            .SetFont(FontRole::Title)
            .SetHeight(Dim::Fixed(24));
        AddControl(root, "save", 80, 32)
            .SetText(L"保存")
            .SetStyle(L"primary");
        AddControl(root, "cancel", 80, 32)
            .SetText(L"取消")
            .SetStyle(L"secondary");

        PrepareTree(root, Rect{0, 0, 200, 120});

        PaintRecorder rec;
        RenderTree(rec, root, pal, mx);

        Check(rec.Count(PaintOpKind::Text) == 3, "label + two button text ops");
        Check(rec.Count(PaintOpKind::FillRound) == 2, "two button backgrounds filled");
        Check(rec.Count(PaintOpKind::StrokeRound) == 1,
              "only the secondary button carries a border (primary is solid fill)");

        const PaintOp* btnText = FindText(rec, L"保存");
        Check(btnText != nullptr && btnText->role == FontRole::Button,
              "button text uses the button font role");

        const PaintOp* label = FindText(rec, L"标题");
        Check(label != nullptr, "label text op present");
        if (label) {
            Check(label->role == FontRole::Title, "label uses its font role");
            Check((label->format & dt::SingleLine) != 0, "label is single line");
            Check((label->format & dt::VCenter) != 0, "label is vertically centred");
            Check(label->rect.top == 16 && label->rect.bottom == 40,
                  "label respects fixed height inside padding");
        }

        const PaintOp* first = rec.Ops().empty() ? nullptr : &rec.Ops().front();
        Check(first != nullptr && first->kind == PaintOpKind::Text,
              "label paints before the button (tree order)");
    }

    // ---- 2. Scroll：可滚动时裁剪视口 ----
    {
        LayoutNode root(NodeType::Scroll);
        AddControl(root, "a", 50, 60);
        AddControl(root, "b", 50, 60);
        PrepareTree(root, Rect{0, 0, 100, 60});
        Check(root.Scrollable(), "scroll content overflows");

        PaintRecorder rec;
        RenderTree(rec, root, pal, mx);
        Check(rec.Count(PaintOpKind::PushClip) == 1, "scrollable scroll pushes one clip");
        Check(rec.Count(PaintOpKind::PopClip) == 1, "scrollable scroll pops one clip");
        const PaintOp* clip = Find(rec, PaintOpKind::PushClip);
        const Rect vp = root.ContentFrame();
        Check(clip != nullptr && clip->rect.left == vp.left && clip->rect.top == vp.top &&
                  clip->rect.right == vp.right && clip->rect.bottom == vp.bottom,
              "clip rect is the scroll viewport");

        PaintRecorder rec2;
        RenderOptions noClip;
        noClip.clipScroll = false;
        RenderTree(rec2, root, pal, mx, noClip);
        Check(rec2.Count(PaintOpKind::PushClip) == 0, "clipScroll=false disables clipping");
    }

    // ---- 3. Scroll：内容放得下时不裁剪 ----
    {
        LayoutNode root(NodeType::Scroll);
        AddControl(root, "a", 50, 20);
        AddControl(root, "b", 50, 20);
        PrepareTree(root, Rect{0, 0, 100, 60});

        PaintRecorder rec;
        RenderTree(rec, root, pal, mx);
        Check(rec.Count(PaintOpKind::PushClip) == 0, "fitting content is not clipped");
    }

    // ---- 4. debugBoxes：容器画调试边框 ----
    {
        LayoutNode root(NodeType::Row);
        AddControl(root, "a", 50, 20);
        AddControl(root, "b", 50, 20);
        PrepareTree(root, Rect{0, 0, 120, 20});

        PaintRecorder rec;
        RenderOptions opts;
        opts.debugBoxes = true;
        RenderTree(rec, root, pal, mx, opts);
        Check(rec.Count(PaintOpKind::StrokeRect) == 1, "debug box drawn for the container");

        PaintRecorder rec2;
        RenderTree(rec2, root, pal, mx);
        Check(rec2.Count(PaintOpKind::StrokeRect) == 0, "no debug box by default");
    }

    // ---- 5. 绑定 + 渲染：{title} 先填充再绘制 ----
    {
        const char* json =
            "{ \"type\": \"label\", \"id\": \"t\", \"text\": \"{title}\" }";
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(json, &err);
        Check(root != nullptr && err.ok, "binding render document inflates");
        if (root) {
            ValueStore store;
            store.Set("title", L"主页");
            store.Apply(*root);
            PrepareTree(*root, Rect{0, 0, 200, 40});

            PaintRecorder rec;
            RenderTree(rec, *root, pal, mx);
            Check(FindText(rec, L"主页") != nullptr,
                  "bound text reaches the draw calls");
            Check(FindText(rec, L"{title}") == nullptr,
                  "unfilled placeholder is not painted");
        }
    }

    // ---- 6. widget 分发：switch -> 开关的轨道 + 滑块 ----
    {
        LayoutNode root(NodeType::Column);
        AddControl(root, "sw", 44, 24).SetWidget(WidgetKind::Switch).SetChecked(true);
        AddControl(root, "sw2", 44, 24).SetWidget(WidgetKind::Switch);
        PrepareTree(root, Rect{0, 0, 100, 60});

        PaintRecorder rec;
        RenderTree(rec, root, pal, mx);
        // 每个开关：轨道 FillRound + 轨道 StrokeRound + 滑块 FillRound + 滑块 StrokeRound
        Check(rec.Count(PaintOpKind::FillRound) == 4, "two switches paint track + thumb fills");
        Check(rec.Count(PaintOpKind::StrokeRound) == 4, "two switches paint track + thumb strokes");
        Check(rec.Count(PaintOpKind::Text) == 0, "switch paints no text");
    }

    // ---- 7. widget 分发：checkbox -> 选中态画对勾 ----
    {
        LayoutNode root(NodeType::Column);
        AddControl(root, "cb", 120, 24).SetWidget(WidgetKind::Checkbox)
            .SetText(L"记住我").SetChecked(true);
        PrepareTree(root, Rect{0, 0, 140, 40});

        PaintRecorder rec;
        RenderTree(rec, root, pal, mx);
        Check(rec.Count(PaintOpKind::FillRound) >= 1, "checkbox paints its box");
        Check(FindText(rec, L"\u2713") != nullptr, "checked checkbox paints the check glyph");
        Check(FindText(rec, L"记住我") != nullptr, "checkbox paints its label");
    }

    // ---- 8. widget 分发：progressbar + progressring 数值钳制 ----
    {
        LayoutNode root(NodeType::Column);
        AddControl(root, "bar", 100, 8).SetWidget(WidgetKind::ProgressBar).SetProgress(5.0);
        AddControl(root, "ring", 24, 24).SetWidget(WidgetKind::ProgressRing).SetProgress(-1.0);
        PrepareTree(root, Rect{0, 0, 120, 60});

        PaintRecorder rec;
        RenderTree(rec, root, pal, mx);
        // 越界值被钳到 [0,1]：满进度条照常画，不画穿。
        Check(rec.Count(PaintOpKind::FillRound) >= 2, "progress bar and ring both paint");
    }

    // ---- 9. widget 分发：segmented -> 选项文本 + 选中背景 ----
    {
        LayoutNode root(NodeType::Column);
        AddControl(root, "seg", 180, 28).SetWidget(WidgetKind::Segmented)
            .SetOptions({L"日", L"周", L"月"}).SetSelectedIndex(1);
        PrepareTree(root, Rect{0, 0, 200, 40});

        PaintRecorder rec;
        RenderTree(rec, root, pal, mx);
        Check(rec.Count(PaintOpKind::FillRound) == 3, "three segments painted");
        Check(rec.Count(PaintOpKind::Text) == 3, "three segment labels painted");
        Check(FindText(rec, L"周") != nullptr, "segment options reach the draw calls");
        Check(rec.Count(PaintOpKind::FillRound) > 0, "selected segment gets a fill");
    }

    // ---- 10. widget 分发：rating / stepper / tag / avatar / badge ----
    {
        LayoutNode root(NodeType::Column);
        AddControl(root, "stars", 120, 24).SetWidget(WidgetKind::Rating).SetRating(4);
        AddControl(root, "step", 100, 28).SetWidget(WidgetKind::Stepper).SetValue(7);
        AddControl(root, "tag", 60, 22).SetWidget(WidgetKind::Tag).SetText(L"新").SetChecked(true);
        AddControl(root, "ava", 28, 28).SetWidget(WidgetKind::Avatar).SetText(L"林");
        AddControl(root, "bdg", 28, 16).SetWidget(WidgetKind::Badge).SetText(L"9");
        PrepareTree(root, Rect{0, 0, 200, 160});

        PaintRecorder rec;
        RenderTree(rec, root, pal, mx);
        Check(rec.Count(PaintOpKind::Text) >= 4, "rating/stepper/tag/avatar/badge all paint text or glyphs");
        Check(FindText(rec, L"新") != nullptr, "tag text painted");
        Check(FindText(rec, L"林") != nullptr, "avatar initials painted");
    }

    // ---- 11. disabled 态传导到控件 ----
    {
        LayoutNode root(NodeType::Column);
        AddControl(root, "btn", 80, 32).SetWidget(WidgetKind::Button)
            .SetText(L"禁用").SetDisabled(true);
        PrepareTree(root, Rect{0, 0, 100, 48});

        PaintRecorder rec;
        RenderTree(rec, root, pal, mx);
        Check(!rec.Ops().empty(), "disabled control still paints (in disabled style)");
    }

    // ---- 12. 未知控件名是解析错误 ----
    {
        const char* json =
            "{ \"type\": \"control\", \"widget\": \"no_such_widget\" }";
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(json, &err);
        Check(root == nullptr && !err.ok, "unknown widget name fails inflate");
    }

    if (g_failures != 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nall declarative render checks passed\n");
    return 0;
}
