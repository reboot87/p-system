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

    if (g_failures == 0) {
        std::printf("PASS\n");
        return 0;
    }
    std::printf("FAILED: %d check(s)\n", g_failures);
    return 1;
}
