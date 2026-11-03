#pragma once
#include <windows.h>
#include <string>
#include "Theme.h"

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
        Dot
    } type = None;

    RECT rect = {};
    std::wstring text;
    COLORREF color = 0;
    COLORREF color2 = 0;
    COLORREF color3 = 0;
    int radius = 0;
    int width = 0;
    FontRole fontRole = FontRole::Body;
    UINT format = 0;
    bool hover = false;
    bool pressed = false;
    bool disabled = false;
    BtnStyle btnStyle = BtnStyle::Primary;

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
        swprintf_s(buf, L"DrawCall[type=%d, rect=(%d,%d,%d,%d), color=%06X, text='%s']",
            type, rect.left, rect.top, rect.right, rect.bottom, color, text.c_str());
        return buf;
    }
};

} // namespace ui
} // namespace air
