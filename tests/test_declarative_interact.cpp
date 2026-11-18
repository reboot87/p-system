// ============================================================
// AirUI - declarative interaction test (platform-free)
//
// ValueStore binding ({key} placeholders), hit-testing (deepest
// node wins, frame stacking, scroll clipping) and click dispatch
// (bubble-up to the nearest onClick command).
// ============================================================
#include "airui/Declarative.h"

#include <cstdio>
#include <string>

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

LayoutNode& AddControl(LayoutNode& parent, const char* id, int w, int h)
{
    LayoutNode& n = parent.Add(NodeType::Control, id);
    n.SetIntrinsic(Size{w, h});
    return n;
}

} // namespace

int main()
{
    // ---- 1. 命中测试：最深节点优先，间隙命中容器 ----
    {
        LayoutNode root(NodeType::Column);
        LayoutNode& row = root.Add(NodeType::Row, "row");
        row.SetGap(10);
        AddControl(row, "btn", 50, 20);
        AddControl(row, "plain", 50, 20);

        PrepareTree(root, Rect{0, 0, 200, 200});

        Check(HitTest(root, Point{25, 10})->id == "btn", "hit returns the deepest control");
        Check(HitTest(root, Point{85, 10})->id == "plain", "second control hit");
        Check(HitTest(root, Point{55, 10})->id == "row", "gap hit falls back to the container");
        Check(HitTest(root, Point{150, 10})->id.empty(), "outside the row hits the root");
        Check(HitTest(root, Point{300, 10}) == nullptr, "outside the tree misses");
    }

    // ---- 2. Frame 叠放：后加入的子节点在上层 ----
    {
        LayoutNode root(NodeType::Frame);
        AddControl(root, "back", 0, 0).SetSize(Dim::Match(), Dim::Match());
        AddControl(root, "front", 0, 0).SetSize(Dim::Match(), Dim::Match());

        PrepareTree(root, Rect{0, 0, 100, 100});

        Check(HitTest(root, Point{50, 50})->id == "front",
              "later sibling wins in a frame stack");
    }

    // ---- 3. 点击分发：直接命令 / 沿父链上溯 / 无命令不回调 ----
    {
        LayoutNode root(NodeType::Column);
        LayoutNode& row = root.Add(NodeType::Row, "row");
        row.SetOnClick("rowCmd").SetGap(10);
        LayoutNode& btn = AddControl(row, "btn", 50, 20);
        btn.SetOnClick("save");
        AddControl(row, "plain", 50, 20);

        PrepareTree(root, Rect{0, 0, 200, 200});

        std::string cmd;
        LayoutNode* fired = nullptr;
        int calls = 0;
        auto handler = [&](LayoutNode& n, const std::string& c) {
            ++calls;
            fired = &n;
            cmd = c;
        };

        DispatchClick(root, Point{25, 10}, handler);
        Check(calls == 1 && cmd == "save" && fired == &btn, "direct onClick fires on the node");

        DispatchClick(root, Point{85, 10}, handler);
        Check(calls == 2 && cmd == "rowCmd" && fired == &row,
              "click without command bubbles to the parent");

        DispatchClick(root, Point{150, 10}, handler);
        Check(calls == 2, "no command in the chain -> no callback");

        DispatchClick(root, Point{300, 10}, handler);
        Check(calls == 2, "miss -> no callback");
    }

    // ---- 4. Scroll 裁剪：视口外的点不命中内容 ----
    {
        LayoutNode root(NodeType::Scroll);
        root.SetPadding({10});
        AddControl(root, "a", 50, 30);
        AddControl(root, "b", 50, 30);
        AddControl(root, "c", 50, 30);

        PrepareTree(root, Rect{0, 0, 100, 60});
        root.SetScrollY(20);
        root.Layout(root.Frame());

        // 内容框（视口）= {10,10,90,50}；滚动 20 后子节点：a{-10..20} b{20..50} c{50..80}
        Check(HitTest(root, Point{50, 45})->id == "b", "visible child is hittable");
        Check(HitTest(root, Point{50, 55})->id.empty(),
              "point below the viewport is clipped to the scroll container");
        Check(HitTest(root, Point{5, 30})->id.empty(),
              "point left of the viewport is clipped too");
    }

    // ---- 5. ValueStore：占位符替换 ----
    {
        ValueStore store;
        store.Set("name", L"剪贴板").Set("count", 3LL).Set("ratio", 0.5).Set("flag", true);

        Check(store.Resolve(L"{name} 有 {count} 条") == L"剪贴板 有 3 条",
              "string + integer substitution");
        Check(store.Resolve(L"({ratio}, {flag})") == L"(0.5, true)",
              "double + bool substitution");
        Check(store.Resolve(L"{missing} 保留") == L"{missing} 保留",
              "unknown key keeps the placeholder");
        Check(store.Resolve(L"{unterminated 保留") == L"{unterminated 保留",
              "unterminated brace keeps the text");
        Check(store.Get("nope").empty() && !store.Has("nope"), "missing key -> empty");
    }

    // ---- 6. ValueStore::Apply：整棵树文本填充 ----
    {
        const char* json =
            "{ \"type\": \"column\", \"children\": ["
            "  { \"type\": \"label\", \"id\": \"t\", \"text\": \"{title}\" },"
            "  { \"type\": \"label\", \"id\": \"s\", \"text\": \"{count} 条记录\" } ] }";

        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(json, &err);
        Check(root != nullptr && err.ok, "binding document inflates");
        if (root) {
            ValueStore store;
            store.Set("title", L"主页").Set("count", 42LL);
            store.Apply(*root);

            Check(root->Find("t")->text == L"主页", "Apply fills nested label text");
            Check(root->Find("s")->text == L"42 条记录", "Apply fills the second label");
        }
    }

    // ---- 7. onClick 从 JSON 解析并参与分发 ----
    {
        const char* json =
            "{ \"type\": \"column\", \"children\": ["
            "  { \"type\": \"control\", \"id\": \"save\", \"onClick\": \"doSave\","
            "    \"w\": 80, \"h\": 32 } ] }";

        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(json, &err);
        Check(root != nullptr && err.ok, "onClick document inflates");
        if (root) {
            PrepareTree(*root, Rect{0, 0, 200, 100});
            Check(root->Find("save")->onClick == "doSave", "onClick parsed from JSON");

            std::string cmd;
            DispatchClick(*root, Point{40, 16},
                          [&](LayoutNode&, const std::string& c) { cmd = c; });
            Check(cmd == "doSave", "click dispatched from a JSON-built tree");
        }
    }

    if (g_failures != 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nall declarative interaction checks passed\n");
    return 0;
}
