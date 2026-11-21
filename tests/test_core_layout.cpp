// ============================================================
// AirUI - core layout test (platform-free)
//
// Exercises airui-core Layout with a dummy ILayoutItem, so the
// test also proves core has no windowing dependency.
// ============================================================
#include "airui/Layout.h"
#include "airui/types.h"

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

struct Item : ILayoutItem
{
    Rect rect;
    void Move(const Rect& rc) override { rect = rc; }
};

} // namespace

int main()
{
    // ---- Row: two equal columns, 10px gap, 100x50 ----
    {
        Item a, b;
        Layout()
            .Row({ { &a, 1 }, { &b, 1 } }, 10)
            .Apply(Rect(0, 0, 100, 50));

        Check(Same(a.rect, 0, 0, 45, 50), "row: first item rect");
        Check(Same(b.rect, 55, 0, 100, 50), "row: second item rect");
    }

    // ---- Col: fixed 40 + grow, 10px gap, 200x100 ----
    {
        Item head, body;
        LayoutSpec fixedHead;
        fixedHead.widget = &head;
        fixedHead.fixed  = 40;

        LayoutSpec grows;
        grows.widget = &body;
        grows.grow   = 1;

        Layout().Col({ fixedHead, grows }, 10).Apply(Rect(0, 0, 200, 100));

        Check(Same(head.rect, 0, 0, 200, 40), "col: fixed item rect");
        Check(Same(body.rect, 0, 50, 200, 100), "col: grow item rect");
    }

    // ---- Grid: 4 items, 2 columns, 0 gap, 100x100 ----
    {
        Item cells[4];
        Layout()
            .Grid({ { &cells[0], 1 }, { &cells[1], 1 }, { &cells[2], 1 }, { &cells[3], 1 } },
                  2, 0, 0)
            .Apply(Rect(0, 0, 100, 100));

        Check(Same(cells[0].rect, 0, 0, 50, 50), "grid: cell 0");
        Check(Same(cells[1].rect, 50, 0, 100, 50), "grid: cell 1");
        Check(Same(cells[2].rect, 0, 50, 50, 100), "grid: cell 2");
        Check(Same(cells[3].rect, 50, 50, 100, 100), "grid: cell 3");
    }

    // ---- Margin ----
    {
        Item padded;
        LayoutSpec spec;
        spec.widget = &padded;
        spec.grow   = 1;
        spec.margin = LayoutMargins(8, 16);   // 8 vertical, 16 horizontal

        Layout().Row({ spec }, 0).Apply(Rect(0, 0, 100, 100));

        Check(Same(padded.rect, 16, 8, 84, 92), "row: margin applied");
    }

    // ---- 交叉轴对齐：PreferredSize 决定 Center/End 的真实盒子 ----
    {
        struct Chip : Item
        {
            Size pref{ 40, 20 };
            Size PreferredSize() const override { return pref; }
        };
        Chip center, end, start, nostretch;

        LayoutSpec sCenter, sEnd, sStart, sNoPref;
        sCenter.widget = &center;  sCenter.align = LayoutAlign::Center; sCenter.grow = 1;
        sEnd.widget    = &end;     sEnd.align    = LayoutAlign::End;    sEnd.grow    = 1;
        sStart.widget  = &start;   sStart.align  = LayoutAlign::Start;  sStart.grow  = 1;
        sNoPref.widget = &nostretch; sNoPref.grow = 1;  // 无偏好：保持满交叉尺寸

        Layout().Row({ sCenter, sEnd, sStart, sNoPref }, 0).Apply(Rect(0, 0, 100, 60));

        Check(Same(center.rect, 0, 20, 25, 40), "row: Center shrink-wraps to preferred height");
        Check(Same(end.rect, 25, 40, 50, 60), "row: End pins to the bottom edge");
        Check(Same(start.rect, 50, 0, 75, 20), "row: Start pins to the top edge");
        Check(Same(nostretch.rect, 75, 0, 100, 60),
              "row: no preference falls back to full cross size");

        // Col：交叉轴是宽度
        Chip wide;
        LayoutSpec sWide;
        sWide.widget = &wide;
        sWide.align  = LayoutAlign::Center;
        sWide.grow   = 1;
        Layout().Col({ sWide }, 0).Apply(Rect(0, 0, 80, 30));
        Check(Same(wide.rect, 20, 0, 60, 30), "col: Center centers preferred width");
    }

    if (g_failures == 0) {
        std::printf("PASS\n");
        return 0;
    }
    std::printf("FAILED: %d check(s)\n", g_failures);
    return 1;
}
