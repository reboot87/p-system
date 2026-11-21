// ============================================================
// AirUI - declarative JSON inflate test (platform-free)
//
// Verifies layout JSON -> LayoutNode tree, plus error reporting
// (line/column) for malformed input and bad node properties.
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

void CheckRect(const Rect& r, int l, int t, int rt, int b, const char* what)
{
    if (r.left != l || r.top != t || r.right != rt || r.bottom != b) {
        std::printf("FAIL: %s (got %d,%d,%d,%d want %d,%d,%d,%d)\n",
                    what, r.left, r.top, r.right, r.bottom, l, t, rt, b);
        ++g_failures;
    } else {
        std::printf("PASS: %s\n", what);
    }
}

bool Contains(const std::string& haystack, const char* needle)
{
    return haystack.find(needle) != std::string::npos;
}

// 期望解析失败，且错误信息包含 needle。
void CheckError(const std::string& json, const char* needle, const char* what,
                int expectLine = 0, int expectCol = 0)
{
    InflateError err;
    std::unique_ptr<LayoutNode> root = InflateLayout(json, &err);
    bool ok = (root == nullptr) && !err.ok && Contains(err.message, needle) && err.line > 0;
    if (ok && expectLine > 0) ok = (err.line == expectLine);
    if (ok && expectCol > 0) ok = (err.column == expectCol);
    if (!ok) {
        std::printf("FAIL: %s (ok=%d message='%s' at %d:%d)\n",
                    what, err.ok ? 1 : 0, err.Describe().c_str(), err.line, err.column);
        ++g_failures;
    } else {
        std::printf("PASS: %s\n", what);
    }
}

const char* kDoc = R"({
  "type": "column",
  "padding": [12, 16],
  "gap": 10,
  "unknownKey": true,
  "children": [
    { "type": "label", "id": "title", "text": "设置", "font": "title", "h": 24 },
    { "type": "row", "gap": 8, "children": [
        { "type": "control", "id": "save", "style": "primary", "w": 72, "h": 32 },
        { "type": "spacer", "weight": 1 },
        { "type": "control", "id": "cancel", "preferred": [72, 32] }
    ]},
    { "type": "grid", "columns": 2, "gap": 6, "children": [
        { "type": "label", "text": "a" },
        { "type": "label", "text": "b" }
    ]}
  ]
})";

} // namespace

int main()
{
    // ---- 1. 结构与属性 ----
    {
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(kDoc, &err);

        Check(root != nullptr, "document inflates");
        Check(err.ok, "no inflate error for a valid document");
        if (root) {
            Check(root->type == NodeType::Column, "root is a column");
            Check(root->padding.left == 16 && root->padding.top == 12 &&
                  root->padding.right == 16 && root->padding.bottom == 12,
                  "padding [12,16] maps to vertical/horizontal");
            Check(root->gap == 10, "gap parsed");
            Check(root->children.size() == 3, "root has three children");

            LayoutNode* title = root->Find("title");
            Check(title != nullptr && title->type == NodeType::Label, "label found by id");
            if (title) {
                Check(title->text == L"设置", "UTF-8 text decoded to UTF-16");
                Check(title->font == FontRole::Title, "font role title");
                Check(title->height.mode == DimMode::Fixed && title->height.value == 24,
                      "fixed height from JSON number");
            }

            LayoutNode* save = root->Find("save");
            Check(save != nullptr && save->width.mode == DimMode::Fixed &&
                  save->width.value == 72 && save->height.value == 32,
                  "control fixed size");
            if (save) Check(save->style == L"primary", "style token kept");

            LayoutNode* row = root->children[1].get();
            Check(row->type == NodeType::Row && row->gap == 8, "nested row parsed");
            Check(row->children.size() == 3 && row->children[1]->type == NodeType::Spacer,
                  "spacer child parsed");
            Check(row->children[1]->weight == 1.0f, "weight parsed");

            LayoutNode* cancel = root->Find("cancel");
            Check(cancel != nullptr && cancel->intrinsic.w == 72 && cancel->intrinsic.h == 32,
                  "preferred [w,h] maps to intrinsic");

            LayoutNode* grid = root->children[2].get();
            Check(grid->type == NodeType::Grid && grid->columns == 2 && grid->gap == 6,
                  "grid columns/gap parsed");
        }
    }

    // ---- 2. 解析出的树可以直接布局 ----
    {
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(kDoc, &err);
        if (!root) {
            Check(false, "document inflates for layout");
        } else {
            PrepareTree(*root, Rect{0, 0, 400, 300});

            LayoutNode* title = root->Find("title");
            LayoutNode* save = root->Find("save");
            LayoutNode* cancel = root->Find("cancel");
            LayoutNode* row = root->children[1].get();

            CheckRect(title->Frame(), 16, 12, 32, 36, "label laid out after padding");
            CheckRect(row->Frame(), 16, 46, 176, 78, "row below label + gap");
            CheckRect(save->Frame(), 16, 46, 88, 78, "control placed at the row origin");
            CheckRect(cancel->Frame(), 104, 46, 176, 78, "trailing control right-aligned by spacer");
            Check(root->children[2]->Frame().top == 88, "grid follows the row + gap");
        }
    }

    // ---- 3. 转义 / 代理对 / 别名 ----
    {
        const char* json =
            "{ \"type\": \"label\","
            "  \"text\": \"a\\\"b\\\\c\\n\\u4e2d\\u6587\\uD83D\\uDE00\","
            "  \"width\": \"match\", \"height\": 12,"
            "  \"align\": \"middle\", \"font\": \"icon_large\" }";

        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(json, &err);
        Check(root != nullptr && err.ok, "escaped document inflates");
        if (root) {
            const std::wstring want = std::wstring(L"a\"b\\c\n") + L"中文" + L"\xD83D\xDE00";
            Check(root->text == want, "escapes, \\u escapes and surrogate pairs decoded");
            Check(root->width.mode == DimMode::Match, "width alias accepts \"match\"");
            Check(root->height.mode == DimMode::Fixed && root->height.value == 12,
                  "height alias accepts a number");
            Check(root->align == LayoutAlign::Center, "align alias 'middle' -> center");
            Check(root->font == FontRole::IconLarge, "font alias 'icon_large'");
        }
    }

    // ---- 4. 错误：语法 ----
    CheckError("{ \"type\": \"column\",", "expected a string key", "syntax error reports line 1", 1);
    CheckError("{ \"a\" 1 }", "expected ':'", "missing colon detected");
    CheckError("[1, 2]", "layout node must be a JSON object", "array root rejected");
    CheckError("{ \"type\": \"column\" } trailing", "after the JSON value", "trailing input rejected");
    CheckError("{ \"gap\": 1. }", "digits expected after '.'", "bad number detected");

    // ---- 5. 错误：语义（带值的行列） ----
    CheckError("{ \"type\": \"bogus\" }", "unknown node type 'bogus'",
               "unknown node type", 1, 11);
    CheckError("{ \"gap\": \"x\" }", "'gap' must be a number", "non-numeric gap rejected");
    CheckError("{ \"children\": {} }", "'children' must be an array", "children type checked");
    CheckError("{ \"type\": \"control\", \"align\": \"up\" }", "unknown align 'up'",
               "unknown align rejected");
    CheckError("{ \"type\": \"label\", \"preferred\": [1] }", "'preferred' must be",
               "bad preferred rejected");
    CheckError("{ \"type\": \"label\", \"padding\": [1,2,3] }", "must have 1, 2 or 4 numbers",
               "bad padding array rejected");

    // ---- 6. Describe 输出 ----
    {
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout("{ \"type\": \"nope\" }", &err);
        (void)root;
        const std::string text = err.Describe();
        Check(Contains(text, "1:11:"), "Describe prefixes line:col");
        Check(Contains(text, "unknown node type"), "Describe carries the reason");
    }

    // ---- 7. 深层嵌套被限制 ----
    {
        std::string deep = "{ \"type\": \"column\", \"children\": [";
        for (int i = 0; i < 120; ++i) deep += "{ \"type\": \"column\", \"children\": [";
        for (int i = 0; i < 120; ++i) deep += "] }";
        deep += "] }";

        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(deep, &err);
        Check(root == nullptr && !err.ok, "runaway nesting is rejected");
    }

    // ---- 8. widget 字段解析 ----
    {
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(
            "{ \"type\": \"control\", \"id\": \"sw\", \"widget\": \"switch\", "
            "\"checked\": true, \"disabled\": false }", &err);
        Check(root != nullptr && err.ok, "widget field inflates");
        LayoutNode* sw = root ? root->Find("sw") : nullptr;
        Check(sw != nullptr && sw->type == NodeType::Control, "widget node is a control");
        if (sw) {
            Check(sw->widget == WidgetKind::Switch, "widget name maps to WidgetKind");
            Check(sw->checked, "checked parsed");
        }
    }

    // ---- 9. type 速记：控件名直接写在 type 里 ----
    {
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(
            "{ \"type\": \"slider\", \"id\": \"sl\", \"value\": 30, "
            "\"min\": 0, \"max\": 200, \"widget\": \"slider\" }", &err);
        Check(root != nullptr && err.ok, "widget-in-type shorthand inflates");
        LayoutNode* sl = root ? root->Find("sl") : nullptr;
        if (sl) {
            Check(sl->type == NodeType::Control && sl->widget == WidgetKind::Slider,
                  "type shorthand maps to Control + WidgetKind");
            Check(sl->value == 30 && sl->sliderMin == 0 && sl->sliderMax == 200,
                  "slider range parsed");
        }
    }

    // ---- 10. options：数组与 "A|B|C" 速记两种写法 ----
    {
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(
            "{ \"type\": \"column\", \"children\": ["
            "  { \"type\": \"segmented\", \"id\": \"a\", \"options\": [\"日\", \"周\"] },"
            "  { \"type\": \"segmented\", \"id\": \"b\", \"options\": \"月|年\" }"
            "]}", &err);
        Check(root != nullptr && err.ok, "options document inflates");
        if (root) {
            LayoutNode* a = root->Find("a");
            LayoutNode* b = root->Find("b");
            Check(a != nullptr && a->optionList.size() == 2 &&
                  a->optionList[0] == L"日" && a->optionList[1] == L"周",
                  "options array parsed to UTF-16 list");
            Check(b != nullptr && b->optionList.size() == 2 &&
                  b->optionList[0] == L"月" && b->optionList[1] == L"年",
                  "pipe-string options split");
            if (a) Check(a->widget == WidgetKind::Segmented,
                         "segmented shorthand sets widget kind");
        }
    }

    // ---- 11. 其余控件字段 ----
    {
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(
            "{ \"type\": \"column\", \"children\": ["
            "  { \"type\": \"progressbar\", \"id\": \"p\", \"progress\": 0.75 },"
            "  { \"type\": \"textfield\", \"id\": \"t\", \"placeholder\": \"请输入\", \"text\": \"abc\" },"
            "  { \"type\": \"rating\", \"id\": \"r\", \"rating\": 2, \"ratingMax\": 6 },"
            "  { \"type\": \"combobox\", \"id\": \"c\", \"text\": \"选项\", \"open\": true, \"disabled\": true },"
            "  { \"type\": \"stepper\", \"id\": \"s\", \"value\": 5, \"min\": 0, \"max\": 10 },"
            "  { \"type\": \"badge\", \"id\": \"g\", \"text\": \"99+\" }"
            "]}", &err);
        Check(root != nullptr && err.ok, "control-fields document inflates");
        if (root) {
            LayoutNode* p = root->Find("p");
            LayoutNode* t = root->Find("t");
            LayoutNode* r = root->Find("r");
            LayoutNode* c = root->Find("c");
            LayoutNode* s = root->Find("s");
            LayoutNode* g2 = root->Find("g");
            if (p) Check(p->widget == WidgetKind::ProgressBar && p->progress > 0.74 &&
                         p->progress < 0.76, "progressbar + progress parsed");
            if (t) Check(t->widget == WidgetKind::TextField &&
                         t->placeholder == L"请输入" && t->text == L"abc",
                         "textfield + placeholder parsed");
            if (r) Check(r->widget == WidgetKind::Rating && r->rating == 2 &&
                         r->ratingMax == 6, "rating + ratingMax parsed");
            if (c) Check(c->widget == WidgetKind::ComboBox && c->open && c->disabled &&
                         c->text == L"选项", "combobox open/disabled parsed");
            if (s) Check(s->widget == WidgetKind::Stepper && s->value == 5 &&
                         s->sliderMax == 10, "stepper value/range parsed");
            if (g2) Check(g2->widget == WidgetKind::Badge && g2->text == L"99+",
                          "badge text parsed");
        }
    }

    // ---- 12. 错误路径：坏字段类型 ----
    {
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(
            "{ \"type\": \"switch\", \"checked\": \"yes\" }", &err);
        Check(root == nullptr && !err.ok, "non-bool checked is rejected");
    }
    {
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(
            "{ \"type\": \"progressbar\", \"progress\": \"half\" }", &err);
        Check(root == nullptr && !err.ok, "non-number progress is rejected");
    }
    {
        InflateError err;
        std::unique_ptr<LayoutNode> root = InflateLayout(
            "{ \"type\": \"segmented\", \"options\": [1, 2] }", &err);
        Check(root == nullptr && !err.ok, "non-string options are rejected");
    }

    if (g_failures != 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nall declarative JSON checks passed\n");
    return 0;
}
