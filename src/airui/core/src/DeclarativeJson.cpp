// ============================================================
// AirUI - DeclarativeJson.cpp
// 极简 JSON 解析 + 布局描述 -> LayoutNode 树。
// 平台无关（只用标准库），错误带 1-based 行列号。
// ============================================================
#include "airui/Declarative.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace air {
namespace ui {

// ------------------------------------------------------------
// InflateError
// ------------------------------------------------------------
std::string InflateError::Describe() const
{
    if (ok) return "ok";
    char head[40] = {0};
    if (line > 0) std::snprintf(head, sizeof(head), "%d:%d: ", line, column);
    return std::string(head) + message;
}

namespace {

// ------------------------------------------------------------
// 小工具
// ------------------------------------------------------------
std::string LowerAscii(const std::string& s)
{
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

void AppendUtf8(std::string& out, unsigned cp)
{
    if (cp <= 0x7F) {
        out += static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// UTF-8 -> UTF-16（std::wstring 在 Windows 上就是 UTF-16）。
std::wstring Utf8ToWide(const std::string& s)
{
    std::wstring out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        unsigned cp = 0;
        int len = 0;
        if (c < 0x80) { cp = c; len = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; len = 4; }
        else { ++i; continue; }  // 非法首字节：跳过

        if (i + static_cast<size_t>(len) > s.size()) break;
        for (int k = 1; k < len; ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + static_cast<size_t>(k)]) & 0x3Fu);
        }
        i += static_cast<size_t>(len);

        if (cp <= 0xFFFF) {
            out.push_back(static_cast<wchar_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    return out;
}

// ------------------------------------------------------------
// JSON DOM
// ------------------------------------------------------------
struct JValue
{
    enum class Kind { Null, Bool, Number, String, Array, Object };

    Kind        kind = Kind::Null;
    bool        b = false;
    double      num = 0.0;
    std::string str;
    std::vector<JValue> arr;
    std::vector<std::pair<std::string, JValue>> obj;

    int line = 0;  // 常量 0 表示未知
    int col = 0;

    const JValue* Get(const char* key) const
    {
        for (const auto& kv : obj) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }
};

// ------------------------------------------------------------
// JSON 解析器
// ------------------------------------------------------------
class Parser
{
public:
    explicit Parser(const std::string& src) : m_src(src) {}

    bool ParseRoot(JValue& out)
    {
        SkipWs();
        if (!ParseValue(out, 0)) return false;
        SkipWs();
        if (m_pos != m_src.size()) return Fail("unexpected characters after the JSON value");
        return true;
    }

    int ErrLine() const { return m_errLine; }
    int ErrCol() const { return m_errCol; }
    const std::string& Error() const { return m_error; }

private:
    static const int kMaxDepth = 100;

    const std::string& m_src;
    size_t m_pos = 0;
    int    m_line = 1;
    int    m_col = 1;
    int    m_errLine = 1;
    int    m_errCol = 1;
    std::string m_error;

    bool Fail(const char* msg)
    {
        if (m_error.empty()) {
            m_error = msg;
            m_errLine = m_line;
            m_errCol = m_col;
        }
        return false;
    }

    char Peek() const { return m_pos < m_src.size() ? m_src[m_pos] : '\0'; }

    void Advance()
    {
        if (m_pos >= m_src.size()) return;
        if (m_src[m_pos] == '\n') {
            ++m_line;
            m_col = 1;
        } else {
            ++m_col;
        }
        ++m_pos;
    }

    void SkipWs()
    {
        while (m_pos < m_src.size()) {
            const char c = m_src[m_pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                Advance();
            } else {
                break;
            }
        }
    }

    bool ParseValue(JValue& out, int depth)
    {
        if (depth > kMaxDepth) return Fail("JSON nesting is too deep");
        if (m_pos >= m_src.size()) return Fail("unexpected end of input");

        out.line = m_line;
        out.col = m_col;

        const char c = Peek();
        switch (c) {
        case '{': return ParseObject(out, depth);
        case '[': return ParseArray(out, depth);
        case '"':
            out.kind = JValue::Kind::String;
            return ParseString(out.str);
        case 't':
            if (!Literal("true")) return false;
            out.kind = JValue::Kind::Bool;
            out.b = true;
            return true;
        case 'f':
            if (!Literal("false")) return false;
            out.kind = JValue::Kind::Bool;
            out.b = false;
            return true;
        case 'n':
            if (!Literal("null")) return false;
            out.kind = JValue::Kind::Null;
            return true;
        default:
            break;
        }

        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(out);
        return Fail("unexpected token");
    }

    bool Literal(const char* lit)
    {
        const size_t n = std::strlen(lit);
        if (m_src.compare(m_pos, n, lit) != 0) return Fail("invalid literal");
        for (size_t i = 0; i < n; ++i) Advance();
        return true;
    }

    bool ParseNumber(JValue& out)
    {
        const size_t start = m_pos;
        if (Peek() == '-') Advance();

        bool anyDigit = false;
        while (Peek() >= '0' && Peek() <= '9') {
            Advance();
            anyDigit = true;
        }
        if (!anyDigit) return Fail("invalid number");

        if (Peek() == '.') {
            Advance();
            bool frac = false;
            while (Peek() >= '0' && Peek() <= '9') {
                Advance();
                frac = true;
            }
            if (!frac) return Fail("invalid number: digits expected after '.'");
        }

        if (Peek() == 'e' || Peek() == 'E') {
            Advance();
            if (Peek() == '+' || Peek() == '-') Advance();
            bool exp = false;
            while (Peek() >= '0' && Peek() <= '9') {
                Advance();
                exp = true;
            }
            if (!exp) return Fail("invalid number: digits expected in the exponent");
        }

        const std::string chunk = m_src.substr(start, m_pos - start);
        char* end = nullptr;
        out.num = std::strtod(chunk.c_str(), &end);
        if (!end || *end != '\0') return Fail("invalid number");
        out.kind = JValue::Kind::Number;
        return true;
    }

    int HexVal(char c) const
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    int HexAt(size_t index) const
    {
        if (index >= m_src.size()) return -1;
        return HexVal(m_src[index]);
    }

    bool ReadHex4(unsigned& out)
    {
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            const int h = HexVal(Peek());
            if (h < 0) return Fail("invalid \\u escape");
            v = v * 16 + static_cast<unsigned>(h);
            Advance();
        }
        out = v;
        return true;
    }

    bool ParseString(std::string& out)
    {
        if (Peek() != '"') return Fail("expected '\"'");
        Advance();

        while (true) {
            if (m_pos >= m_src.size()) return Fail("unterminated string");
            const char c = Peek();

            if (c == '"') {
                Advance();
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return Fail("unescaped control character in string");
            }
            if (c != '\\') {
                out += c;
                Advance();
                continue;
            }

            Advance();  // '\\'
            switch (Peek()) {
            case '"':  out += '"';  Advance(); break;
            case '\\': out += '\\'; Advance(); break;
            case '/':  out += '/';  Advance(); break;
            case 'b':  out += '\b'; Advance(); break;
            case 'f':  out += '\f'; Advance(); break;
            case 'n':  out += '\n'; Advance(); break;
            case 'r':  out += '\r'; Advance(); break;
            case 't':  out += '\t'; Advance(); break;
            case 'u': {
                Advance();  // 'u'
                unsigned cp = 0;
                if (!ReadHex4(cp)) return false;

                // 代理对：\uD800-\uDBFF + \uDC00-\uDFFF
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    unsigned lo = 0;
                    const bool hasPair =
                        m_src.compare(m_pos, 2, "\\u") == 0 &&
                        HexAt(m_pos + 2) >= 0 && HexAt(m_pos + 3) >= 0 &&
                        HexAt(m_pos + 4) >= 0 && HexAt(m_pos + 5) >= 0;
                    if (hasPair) {
                        for (int i = 0; i < 2; ++i) Advance();  // "\u"
                        ReadHex4(lo);
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else {
                            cp = 0xFFFD;
                        }
                    } else {
                        cp = 0xFFFD;
                    }
                }
                AppendUtf8(out, cp);
                break;
            }
            default:
                return Fail("invalid escape sequence");
            }
        }
    }

    bool ParseObject(JValue& out, int depth)
    {
        out.kind = JValue::Kind::Object;
        Advance();  // '{'
        SkipWs();
        if (Peek() == '}') {
            Advance();
            return true;
        }

        while (true) {
            SkipWs();
            if (Peek() != '"') return Fail("expected a string key");
            std::string key;
            if (!ParseString(key)) return false;

            SkipWs();
            if (Peek() != ':') return Fail("expected ':' after the key");
            Advance();
            SkipWs();

            JValue value;
            if (!ParseValue(value, depth + 1)) return false;
            out.obj.emplace_back(std::move(key), std::move(value));

            SkipWs();
            if (Peek() == ',') {
                Advance();
                continue;
            }
            if (Peek() == '}') {
                Advance();
                return true;
            }
            return Fail("expected ',' or '}' in object");
        }
    }

    bool ParseArray(JValue& out, int depth)
    {
        out.kind = JValue::Kind::Array;
        Advance();  // '['
        SkipWs();
        if (Peek() == ']') {
            Advance();
            return true;
        }

        while (true) {
            SkipWs();
            JValue value;
            if (!ParseValue(value, depth + 1)) return false;
            out.arr.push_back(std::move(value));

            SkipWs();
            if (Peek() == ',') {
                Advance();
                continue;
            }
            if (Peek() == ']') {
                Advance();
                return true;
            }
            return Fail("expected ',' or ']' in array");
        }
    }
};

// ------------------------------------------------------------
// JSON -> LayoutNode
// ------------------------------------------------------------
int AsInt(double d)
{
    return static_cast<int>(std::llround(d));
}

bool NodeTypeFromName(const std::string& raw, NodeType& out)
{
    const std::string s = LowerAscii(raw);
    if (s == "column") { out = NodeType::Column; return true; }
    if (s == "row")    { out = NodeType::Row;    return true; }
    if (s == "frame" || s == "stack") { out = NodeType::Frame; return true; }
    if (s == "grid")   { out = NodeType::Grid;   return true; }
    if (s == "scroll" || s == "scrollview") { out = NodeType::Scroll; return true; }
    if (s == "label" || s == "text") { out = NodeType::Label; return true; }
    if (s == "control" || s == "widget") { out = NodeType::Control; return true; }
    if (s == "spacer" || s == "space") { out = NodeType::Spacer; return true; }
    return false;
}

// Control 节点的 "widget" 字段；也接受把控件名直接写在 "type" 里
// （如 {"type": "switch"}），让 JSON 更短。
bool WidgetFromName(const std::string& raw, WidgetKind& out)
{
    const std::string s = LowerAscii(raw);
    if (s == "button" || s == "btn")            { out = WidgetKind::Button;       return true; }
    if (s == "checkbox" || s == "check")        { out = WidgetKind::Checkbox;     return true; }
    if (s == "radio")                           { out = WidgetKind::Radio;        return true; }
    if (s == "switch" || s == "toggle" ||
        s == "toggleswitch")                    { out = WidgetKind::Switch;       return true; }
    if (s == "slider")                          { out = WidgetKind::Slider;       return true; }
    if (s == "progressbar" || s == "progress_bar" || s == "progress-bar") {
        out = WidgetKind::ProgressBar;          return true;
    }
    if (s == "progressring" || s == "progress_ring" || s == "progress-ring") {
        out = WidgetKind::ProgressRing;         return true;
    }
    if (s == "textfield" || s == "text_field" || s == "input" || s == "textbox") {
        out = WidgetKind::TextField;            return true;
    }
    if (s == "tag" || s == "chip")              { out = WidgetKind::Tag;          return true; }
    if (s == "stepper")                         { out = WidgetKind::Stepper;      return true; }
    if (s == "rating" || s == "stars")          { out = WidgetKind::Rating;       return true; }
    if (s == "segmented" || s == "segment")     { out = WidgetKind::Segmented;    return true; }
    if (s == "combobox" || s == "combo" || s == "select") { out = WidgetKind::ComboBox; return true; }
    if (s == "avatar")                          { out = WidgetKind::Avatar;       return true; }
    if (s == "badge")                           { out = WidgetKind::Badge;        return true; }
    return false;
}

bool AlignFromName(const std::string& raw, LayoutAlign& out)
{
    const std::string s = LowerAscii(raw);
    if (s == "start" || s == "left" || s == "top")    { out = LayoutAlign::Start;  return true; }
    if (s == "center" || s == "middle")               { out = LayoutAlign::Center; return true; }
    if (s == "end" || s == "right" || s == "bottom")  { out = LayoutAlign::End;    return true; }
    if (s == "stretch" || s == "fill")                { out = LayoutAlign::Stretch; return true; }
    return false;
}

bool FontFromName(const std::string& raw, FontRole& out)
{
    const std::string s = LowerAscii(raw);
    if (s == "body")    { out = FontRole::Body;   return true; }
    if (s == "meta")    { out = FontRole::Meta;   return true; }
    if (s == "title")   { out = FontRole::Title;  return true; }
    if (s == "section") { out = FontRole::Section; return true; }
    if (s == "button")  { out = FontRole::Button; return true; }
    if (s == "icon")    { out = FontRole::Icon;   return true; }
    if (s == "iconlarge" || s == "icon_large" || s == "icon-large") {
        out = FontRole::IconLarge;
        return true;
    }
    if (s == "display") { out = FontRole::Display; return true; }
    return false;
}

const JValue* Pick(const JValue& obj, const char* first, const char* second = nullptr)
{
    if (const JValue* v = obj.Get(first)) return v;
    return second ? obj.Get(second) : nullptr;
}

bool OptInt(const JValue& obj, const char* key, int& dst, InflateError& err)
{
    const JValue* v = obj.Get(key);
    if (!v) return true;
    if (v->kind != JValue::Kind::Number) {
        err.Fail(v->line, v->col, std::string("'") + key + "' must be a number");
        return false;
    }
    dst = AsInt(v->num);
    return true;
}

bool OptFloat(const JValue& obj, const char* key, double& dst, InflateError& err)
{
    const JValue* v = obj.Get(key);
    if (!v) return true;
    if (v->kind != JValue::Kind::Number) {
        err.Fail(v->line, v->col, std::string("'") + key + "' must be a number");
        return false;
    }
    dst = v->num;
    return true;
}

bool OptWString(const JValue& obj, const char* key, std::wstring& dst, InflateError& err)
{
    const JValue* v = obj.Get(key);
    if (!v) return true;
    if (v->kind != JValue::Kind::String) {
        err.Fail(v->line, v->col, std::string("'") + key + "' must be a string");
        return false;
    }
    dst = Utf8ToWide(v->str);
    return true;
}

bool OptString(const JValue& obj, const char* key, std::string& dst, InflateError& err)
{
    const JValue* v = obj.Get(key);
    if (!v) return true;
    if (v->kind != JValue::Kind::String) {
        err.Fail(v->line, v->col, std::string("'") + key + "' must be a string");
        return false;
    }
    dst = v->str;
    return true;
}

bool OptBool(const JValue& obj, const char* key, bool& dst, InflateError& err)
{
    const JValue* v = obj.Get(key);
    if (!v) return true;
    if (v->kind != JValue::Kind::Bool) {
        err.Fail(v->line, v->col, std::string("'") + key + "' must be a boolean");
        return false;
    }
    dst = v->b;
    return true;
}

bool DimField(const JValue& obj, const char* first, const char* second,
              Dim& out, InflateError& err)
{
    const JValue* v = Pick(obj, first, second);
    if (!v) return true;

    if (v->kind == JValue::Kind::Number) {
        out = Dim::Fixed(AsInt(v->num));
        return true;
    }
    if (v->kind == JValue::Kind::String) {
        const std::string s = LowerAscii(v->str);
        if (s == "wrap" || s == "wrap_content" || s == "wrapcontent") {
            out = Dim::Wrap();
            return true;
        }
        if (s == "match" || s == "match_parent" || s == "matchparent" || s == "fill") {
            out = Dim::Match();
            return true;
        }
        err.Fail(v->line, v->col,
                 std::string("'") + first + "': expected a number, \"wrap\" or \"match\"");
        return false;
    }
    err.Fail(v->line, v->col,
             std::string("'") + first + "' must be a number or a string (\"wrap\"/\"match\")");
    return false;
}

bool MarginsField(const JValue& obj, const char* key, LayoutMargins& out, InflateError& err)
{
    const JValue* v = obj.Get(key);
    if (!v) return true;

    if (v->kind == JValue::Kind::Number) {
        out = LayoutMargins(AsInt(v->num));
        return true;
    }

    if (v->kind == JValue::Kind::Array) {
        for (const JValue& item : v->arr) {
            if (item.kind != JValue::Kind::Number) {
                err.Fail(item.line, item.col, std::string("'") + key + "' array must hold numbers");
                return false;
            }
        }
        const auto& a = v->arr;
        if (a.size() == 1) {
            out = LayoutMargins(AsInt(a[0].num));
        } else if (a.size() == 2) {
            out = LayoutMargins(AsInt(a[0].num), AsInt(a[1].num));  // [vertical, horizontal]
        } else if (a.size() == 4) {
            out = LayoutMargins(AsInt(a[0].num), AsInt(a[1].num),
                                AsInt(a[2].num), AsInt(a[3].num));   // [left, top, right, bottom]
        } else {
            err.Fail(v->line, v->col,
                     std::string("'") + key + "' array must have 1, 2 or 4 numbers");
            return false;
        }
        return true;
    }

    if (v->kind == JValue::Kind::Object) {
        int l = out.left, t = out.top, r = out.right, b = out.bottom;
        auto read = [&](const char* k, int& dst) -> bool {
            const JValue* f = v->Get(k);
            if (!f) return true;
            if (f->kind != JValue::Kind::Number) {
                err.Fail(f->line, f->col, std::string("'") + key + "." + k + "' must be a number");
                return false;
            }
            dst = AsInt(f->num);
            return true;
        };
        if (!read("left", l) || !read("top", t) || !read("right", r) || !read("bottom", b)) {
            return false;
        }
        out = LayoutMargins(l, t, r, b);
        return true;
    }

    err.Fail(v->line, v->col,
             std::string("'") + key + "' must be a number, an array or an object");
    return false;
}

bool IntrinsicField(const JValue& obj, Size& out, InflateError& err)
{
    const JValue* v = Pick(obj, "preferred", "intrinsic");
    if (!v) return true;

    if (v->kind == JValue::Kind::Array) {
        if (v->arr.size() != 2 ||
            v->arr[0].kind != JValue::Kind::Number ||
            v->arr[1].kind != JValue::Kind::Number) {
            err.Fail(v->line, v->col, "'preferred' must be [width, height] numbers");
            return false;
        }
        out = Size{AsInt(v->arr[0].num), AsInt(v->arr[1].num)};
        return true;
    }

    if (v->kind == JValue::Kind::Object) {
        const JValue* w = v->Get("w");
        const JValue* h = v->Get("h");
        if (!w || !h || w->kind != JValue::Kind::Number || h->kind != JValue::Kind::Number) {
            err.Fail(v->line, v->col, "'preferred' object needs numeric \"w\" and \"h\"");
            return false;
        }
        out = Size{AsInt(w->num), AsInt(h->num)};
        return true;
    }

    err.Fail(v->line, v->col, "'preferred' must be [width, height] or {\"w\":..,\"h\":..}");
    return false;
}

std::unique_ptr<LayoutNode> FailNode(const JValue& at, std::string msg, InflateError& err)
{
    if (err.ok) err.Fail(at.line, at.col, std::move(msg));
    return nullptr;
}

std::unique_ptr<LayoutNode> BuildNode(const JValue& v, InflateError& err, int depth)
{
    if (depth > 100) return FailNode(v, "layout tree is too deep", err);
    if (v.kind != JValue::Kind::Object) {
        return FailNode(v, "layout node must be a JSON object", err);
    }

    NodeType type = NodeType::Column;
    WidgetKind widget = WidgetKind::Button;
    if (const JValue* t = v.Get("type")) {
        if (t->kind != JValue::Kind::String) {
            return FailNode(*t, "'type' must be a string", err);
        }
        if (!NodeTypeFromName(t->str, type)) {
            // 控件名直接写在 type 里（{"type":"switch"}）也认。
            if (!WidgetFromName(t->str, widget)) {
                return FailNode(*t, "unknown node type '" + t->str + "'", err);
            }
            type = NodeType::Control;
        }
    }

    std::unique_ptr<LayoutNode> node(new LayoutNode(type));

    if (const JValue* id = v.Get("id")) {
        if (id->kind != JValue::Kind::String) return FailNode(*id, "'id' must be a string", err);
        node->id = id->str;
    }

    if (!DimField(v, "w", "width", node->width, err)) return nullptr;
    if (!DimField(v, "h", "height", node->height, err)) return nullptr;
    if (!OptInt(v, "minW", node->minW, err)) return nullptr;
    if (!OptInt(v, "minH", node->minH, err)) return nullptr;
    if (!OptInt(v, "maxW", node->maxW, err)) return nullptr;
    if (!OptInt(v, "maxH", node->maxH, err)) return nullptr;
    if (!OptInt(v, "gap", node->gap, err)) return nullptr;
    if (!OptInt(v, "columns", node->columns, err)) return nullptr;

    double weight = node->weight;
    if (!OptFloat(v, "weight", weight, err)) return nullptr;
    node->weight = static_cast<float>(weight);

    if (!MarginsField(v, "margin", node->margin, err)) return nullptr;
    if (!MarginsField(v, "padding", node->padding, err)) return nullptr;
    if (!OptWString(v, "text", node->text, err)) return nullptr;
    if (!OptWString(v, "style", node->style, err)) return nullptr;
    if (!OptString(v, "onClick", node->onClick, err)) return nullptr;
    if (!OptBool(v, "clip", node->clip, err)) return nullptr;
    if (!IntrinsicField(v, node->intrinsic, err)) return nullptr;

    // ---- Control 的控件语义字段 ----
    // type 速记（{"type":"switch"}）已经在上面解析进 widget；显式
    // "widget" 字段优先级更高，两者共存时以 widget 字段为准。
    node->widget = widget;
    if (const JValue* w = v.Get("widget")) {
        if (w->kind != JValue::Kind::String) {
            return FailNode(*w, "'widget' must be a string", err);
        }
        if (!WidgetFromName(w->str, node->widget)) {
            return FailNode(*w, "unknown widget '" + w->str + "'", err);
        }
    }
    if (!OptBool(v, "checked", node->checked, err)) return nullptr;
    if (!OptBool(v, "disabled", node->disabled, err)) return nullptr;
    if (!OptBool(v, "open", node->open, err)) return nullptr;
    if (!OptInt(v, "value", node->value, err)) return nullptr;
    if (!OptInt(v, "min", node->sliderMin, err)) return nullptr;
    if (!OptInt(v, "max", node->sliderMax, err)) return nullptr;
    if (!OptInt(v, "rating", node->rating, err)) return nullptr;
    if (!OptInt(v, "ratingMax", node->ratingMax, err)) return nullptr;
    if (!OptInt(v, "selectedIndex", node->selectedIndex, err)) return nullptr;
    if (!OptWString(v, "placeholder", node->placeholder, err)) return nullptr;
    if (const JValue* pr = v.Get("progress")) {
        if (pr->kind != JValue::Kind::Number) {
            return FailNode(*pr, "'progress' must be a number", err);
        }
        node->progress = pr->num;
    }
    // Segmented 的选项：字符串数组，或 "A|B|C" 速记。
    if (const JValue* opts = v.Get("options")) {
        if (opts->kind == JValue::Kind::Array) {
            for (const JValue& item : opts->arr) {
                if (item.kind != JValue::Kind::String) {
                    return FailNode(item, "'options' array must hold strings", err);
                }
                node->optionList.push_back(Utf8ToWide(item.str));
            }
        } else if (opts->kind == JValue::Kind::String) {
            std::wstring wide = Utf8ToWide(opts->str);
            std::wstring cur;
            for (wchar_t ch : wide) {
                if (ch == L'|') { node->optionList.push_back(cur); cur.clear(); }
                else            { cur += ch; }
            }
            if (!cur.empty()) node->optionList.push_back(cur);
        } else {
            return FailNode(*opts, "'options' must be an array or a string", err);
        }
    }

    if (const JValue* align = v.Get("align")) {
        if (align->kind != JValue::Kind::String) {
            return FailNode(*align, "'align' must be a string", err);
        }
        LayoutAlign value = LayoutAlign::Stretch;
        if (!AlignFromName(align->str, value)) {
            return FailNode(*align, "unknown align '" + align->str + "'", err);
        }
        node->align = value;
    }

    if (const JValue* font = v.Get("font")) {
        if (font->kind != JValue::Kind::String) {
            return FailNode(*font, "'font' must be a string", err);
        }
        FontRole value = FontRole::Body;
        if (!FontFromName(font->str, value)) {
            return FailNode(*font, "unknown font role '" + font->str + "'", err);
        }
        node->font = value;
    }

    if (const JValue* kids = v.Get("children")) {
        if (kids->kind != JValue::Kind::Array) {
            return FailNode(*kids, "'children' must be an array", err);
        }
        for (const JValue& kid : kids->arr) {
            std::unique_ptr<LayoutNode> child = BuildNode(kid, err, depth + 1);
            if (!child) return nullptr;
            node->Add(std::move(child));
        }
    }

    return node;
}

} // namespace

// ------------------------------------------------------------
// 入口
// ------------------------------------------------------------
std::unique_ptr<LayoutNode> InflateLayout(const std::string& utf8Json, InflateError* err)
{
    InflateError local;
    InflateError& e = err ? *err : local;
    e.Reset();

    Parser parser(utf8Json);
    JValue root;
    if (!parser.ParseRoot(root)) {
        e.Fail(parser.ErrLine(), parser.ErrCol(), parser.Error());
        return nullptr;
    }

    std::unique_ptr<LayoutNode> node = BuildNode(root, e, 0);
    if (!node && e.ok) e.Fail(root.line, root.col, "invalid layout description");
    return node;
}

} // namespace ui
} // namespace air
