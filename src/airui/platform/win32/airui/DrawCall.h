#pragma once
#include <cstdio>
#include <string>
#include "airui/Theme.h"
#include "airui/ControlsCore.h"

namespace air {
namespace ui {

// 绘制调用记录（用于断言）
struct DrawCall
{
    enum Type {
        None,
        Fill,
        FillRound,
        Stroke,
        StrokeDashed,
        Text,
        TextRow,
        TextRowRight,
        TextRowCenter,
        TextBlock,
        ProgressBar,
        Tag,
        PillRight,
        StatusTag,
        KbdChip,
        IconTile,
        EmptyState,
        Button,
        HLine,
        HLineDashed,
        VLine,
        Dot,
        Pagination
    } type = None;

    Rect rect = {};
    std::wstring text;
    Color color = 0;
    Color color2 = 0;
    Color color3 = 0;
    int radius = 0;
    int width = 0;
    FontRole fontRole = FontRole::Body;
    TextFormat format = 0;
    bool hover = false;
    bool pressed = false;
    bool disabled = false;
    ButtonStyle btnStyle = ButtonStyle::Primary;

    bool Matches(const DrawCall& expected) const {
        if (type != expected.type) return false;
        if (expected.color != 0 && color != expected.color) return false;
        if (expected.color2 != 0 && color2 != expected.color2) return false;
        if (!expected.text.empty() && text != expected.text) return false;
        if (expected.radius != 0 && radius != expected.radius) return false;
        return true;
    }

    std::wstring ToString() const {
        wchar_t buf[512];
        std::swprintf(buf, L"DrawCall[type=%d, rect=(%d,%d,%d,%d), color=%06X, text='%s']",
            type, rect.left, rect.top, rect.right, rect.bottom, color, text.c_str());
        return buf;
    }
};

// Platform-neutral text helpers shared with page authors.
namespace gfx {
// Replace embedded newlines with spaces for single-line list display.
std::wstring OneLine(const std::wstring& s);
} // namespace gfx

} // namespace ui
} // namespace air
