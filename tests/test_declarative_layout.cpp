// ============================================================
// AirUI - declarative layout test (platform-free)
//
// Exercises the Android-style node model and the measure/layout
// passes: linear weights, wrap/match/fixed dimensions, padding,
// margins, frames, grids, scroll offsets, min/max clamps.
// ============================================================
#include "airui/Declarative.h"

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

bool Same(const Rect& r, int l, int t, int rt, int b)
{
    return r.left == l && r.top == t && r.right == rt && r.bottom == b;
}

void CheckRect(const Rect& r, int l, int t, int rt, int b, const char* what)
{
    if (!Same(r, l, t, rt, b)) {
        std::printf("FAIL: %s (got %d,%d,%d,%d want %d,%d,%d,%d)\n",
                    what, r.left, r.top, r.right, r.bottom, l, t, rt, b);
        ++g_failures;
    } else {
        std::printf("PASS: %s\n", what);
    }
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
    // ---- 1. Column: fixed heights + gap + padding ----
    {
        LayoutNode root(NodeType::Column);
        root.SetPadding({10}).SetGap(5);
        LayoutNode& a = AddControl(root, "a", 50, 20);
        LayoutNode& b = AddControl(root, "b", 60, 30);

        PrepareTree(root, Rect{0, 0, 200, 100});

        CheckRect(root.ContentFrame(), 10, 10, 190, 90, "column content frame honours padding");
        CheckRect(a.Frame(), 10, 10, 60, 30, "column child a at padding origin");
        CheckRect(b.Frame(), 10, 35, 70, 65, "column child b below a + gap");
        Check(root.MeasuredHeight() == 100, "root wrap height takes the given window");
    }

    // ---- 2. Row: weights split the remaining space ----
    {
        LayoutNode root(NodeType::Row);
        root.SetGap(10);
        LayoutNode& a = AddControl(root, "a", 10, 20);
        LayoutNode& b = AddControl(root, "b", 10, 20);
        a.SetWeight(1);
        b.SetWeight(2);

        PrepareTree(root, Rect{0, 0, 300, 50});

        Check(a.Frame().Width() == 97, "row weight 1/3 width");
        Check(b.Frame().Width() == 193, "row weight 2/3 width");
        Check(a.Frame().left == 0 && b.Frame().left == 107, "row weighted children packed with gap");
        Check(a.Frame().Height() == 20 && b.Frame().Height() == 20, "row wrap-height children keep intrinsic height");
    }

    // ---- 3. Wrap vs Match vs Fixed ----
    {
        LayoutNode root(NodeType::Column);
        LayoutNode& wrapped = AddControl(root, "wrapped", 40, 10);
        LayoutNode& matched = AddControl(root, "matched", 10, 10);
        LayoutNode& fixed = AddControl(root, "fixed", 10, 10);
        matched.SetWidth(Dim::Match());
        fixed.SetSize(Dim::Fixed(80), Dim::Fixed(12));

        PrepareTree(root, Rect{0, 0, 200, 120});

        Check(wrapped.Frame().Width() == 40, "wrap width keeps intrinsic");
        Check(matched.Frame().Width() == 200, "match width fills the row");
        CheckRect(fixed.Frame(), 0, 20, 80, 32, "fixed width/height wins");
    }

    // ---- 4. Label intrinsic via injected text measurement ----
    {
        LayoutEnv env;
        env.measureText = [](const std::wstring& t, FontRole) {
            return Size{static_cast<int>(t.size()) * 10, 22};
        };

        LayoutNode root(NodeType::Column);
        LayoutNode& label = root.Add(NodeType::Label, "title");
        label.SetText(L"abcd");

        PrepareTree(root, Rect{0, 0, 300, 200}, env);

        CheckRect(label.Frame(), 0, 0, 40, 22, "label measured through env callback");
        Check(root.MeasuredHeight() == 200, "label column takes window height");
    }

    // ---- 5. Frame: Match fills, Wrap centres ----
    {
        LayoutNode root(NodeType::Frame);
        LayoutNode& fill = AddControl(root, "fill", 10, 10);
        LayoutNode& center = AddControl(root, "center", 40, 20);
        fill.SetSize(Dim::Match(), Dim::Match());
        center.SetAlign(LayoutAlign::Center);

        PrepareTree(root, Rect{0, 0, 200, 100});

        CheckRect(fill.Frame(), 0, 0, 200, 100, "frame match child fills content");
        CheckRect(center.Frame(), 80, 40, 120, 60, "frame wrap child centred");
    }

    // ---- 6. Grid: equal cells with gaps ----
    {
        LayoutNode root(NodeType::Grid);
        root.SetColumns(2).SetGap(10);
        for (int i = 0; i < 4; ++i) AddControl(root, "cell", 10, 10);

        PrepareTree(root, Rect{0, 0, 200, 200});

        CheckRect(root.children[0]->Frame(), 0, 0, 95, 10, "grid cell (0,0)");
        CheckRect(root.children[1]->Frame(), 105, 0, 200, 10, "grid cell (0,1)");
        CheckRect(root.children[2]->Frame(), 0, 20, 95, 30, "grid cell (1,0)");
        CheckRect(root.children[3]->Frame(), 105, 20, 200, 30, "grid cell (1,1)");
    }

    // ---- 7. Margins shift a child inside its parent ----
    {
        LayoutNode root(NodeType::Column);
        root.SetPadding({5});
        LayoutNode& a = AddControl(root, "a", 30, 30);
        a.SetMargin(LayoutMargins(2, 4, 6, 8));  // l=2 t=4 r=6 b=8

        PrepareTree(root, Rect{0, 0, 100, 100});

        CheckRect(a.Frame(), 7, 9, 37, 39, "child frame includes margin offset");
    }

    // ---- 8. Scroll: content overflows, offset shifts children ----
    {
        LayoutNode root(NodeType::Scroll);
        LayoutNode& a = AddControl(root, "a", 50, 30);
        LayoutNode& b = AddControl(root, "b", 50, 30);
        LayoutNode& c = AddControl(root, "c", 50, 30);
        (void)a; (void)b; (void)c;

        PrepareTree(root, Rect{0, 0, 100, 60});

        Check(root.Scrollable(), "scroll reports overflow");
        Check(root.ScrollContentHeight() == 90, "scroll content height is the stacked height");
        CheckRect(root.children[2]->Frame(), 0, 60, 50, 90, "third child beyond the viewport");

        root.SetScrollY(20);
        root.Layout(root.Frame());
        CheckRect(root.children[0]->Frame(), 0, -20, 50, 10, "scroll offset moves children up");
        CheckRect(root.children[2]->Frame(), 0, 40, 50, 70, "third child shifted by offset");

        root.SetScrollY(1000);
        Check(root.ScrollY() == 30, "scroll offset clamped to content minus viewport");
    }

    // ---- 9. Find + min/max clamps ----
    {
        LayoutNode root(NodeType::Column);
        LayoutNode& row = root.Add(NodeType::Row, "row");
        LayoutNode& inner = AddControl(row, "inner", 10, 10);
        inner.SetMinSize(40, 0);

        LayoutNode& clamped = AddControl(root, "clamped", 200, 10);
        clamped.SetMaxSize(50, -1);

        PrepareTree(root, Rect{0, 0, 300, 200});

        Check(root.Find("inner") == &inner, "Find recurses into nested children");
        Check(inner.MeasuredWidth() == 40, "min width clamp applies");
        Check(clamped.MeasuredWidth() == 50, "max width clamp applies");
        Check(root.Find("missing") == nullptr, "Find returns null for unknown id");
    }

    // ---- 10. Wrap content is capped by an AtMost parent ----
    {
        LayoutEnv env;  // built-in estimate: 8px per char
        LayoutNode root(NodeType::Column);
        LayoutNode& longLabel = root.Add(NodeType::Label, "long");
        longLabel.SetText(L"0123456789012345678901234567890123456789");  // 40 chars -> 320px

        PrepareTree(root, Rect{0, 0, 120, 80}, env);

        Check(longLabel.MeasuredWidth() == 120, "wrap label capped by available width");
    }

    if (g_failures != 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nall declarative layout checks passed\n");
    return 0;
}
