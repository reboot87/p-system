#include "airui/Canvas.h"
#include "airui/CanvasPainter.h"
#include "airui/ControlsCore.h"
#include "airui/ControlsExtra.h"
#include "airui/Gfx.h"
#include "airui/Controls.h"

namespace air {
namespace ui {

std::vector<DrawCall>* Canvas::s_recorder = nullptr;

Canvas::Canvas(HDC dc, const Fonts& fonts) : m_dc(dc), m_fonts(fonts)
{
    if (m_dc) SetBkMode(m_dc, TRANSPARENT);
}

// ---- 形状 ----
void Canvas::Fill(const RECT& rc, COLORREF color)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::Fill; dc.rect = rc; dc.color = color;
        s_recorder->push_back(dc);
    }
    gfx::Fill(m_dc, rc, color);
}

void Canvas::FillRound(const RECT& rc, COLORREF color, int radius)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::FillRound; dc.rect = rc; dc.color = color; dc.radius = radius;
        s_recorder->push_back(dc);
    }
    gfx::FillRound(m_dc, rc, color, radius);
}

void Canvas::Stroke(const RECT& rc, COLORREF color, int radius, int width)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::Stroke; dc.rect = rc; dc.color = color; dc.radius = radius; dc.width = width;
        s_recorder->push_back(dc);
    }
    gfx::Stroke(m_dc, rc, color, radius, width);
}

void Canvas::StrokeDashed(const RECT& rc, COLORREF color, int radius)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::StrokeDashed; dc.rect = rc; dc.color = color; dc.radius = radius;
        s_recorder->push_back(dc);
    }
    gfx::StrokeDashed(m_dc, rc, color, radius);
}

void Canvas::HLine(int x1, int x2, int y, COLORREF color)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::HLine; dc.rect = {x1, y, x2, y+1}; dc.color = color;
        s_recorder->push_back(dc);
    }
    gfx::HLine(m_dc, x1, x2, y, color);
}

void Canvas::HLineDashed(int x1, int x2, int y, COLORREF color)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::HLineDashed; dc.rect = {x1, y, x2, y+1}; dc.color = color;
        s_recorder->push_back(dc);
    }
    gfx::HLineDashed(m_dc, x1, x2, y, color);
}

void Canvas::VLine(int x, int y1, int y2, COLORREF color)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::VLine; dc.rect = {x, y1, x+1, y2}; dc.color = color;
        s_recorder->push_back(dc);
    }
    gfx::VLine(m_dc, x, y1, y2, color);
}

void Canvas::Dot(int cx, int cy, int diameter, COLORREF color)
{
    if (s_recorder) {
        RECT rc = {cx - diameter/2, cy - diameter/2, cx + diameter/2, cy + diameter/2};
        DrawCall dc; dc.type = DrawCall::Dot; dc.rect = rc; dc.color = color;
        s_recorder->push_back(dc);
    }
    gfx::Dot(m_dc, cx, cy, diameter, color);
}

void Canvas::ProgressBar(const RECT& rc, int pct, COLORREF track,
                         COLORREF colorNormal, COLORREF colorWarn,
                         COLORREF colorHot)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::ProgressBar; dc.rect = rc; dc.color = track; dc.color2 = colorNormal; dc.color3 = colorWarn;
        s_recorder->push_back(dc);
    }
    gfx::ProgressBar(m_dc, rc, pct, track, colorNormal, colorWarn, colorHot);
}

// ---- 文本 ----
void Canvas::Text(const RECT& rc, const std::wstring& s, FontRole role,
                  COLORREF color, UINT format)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::Text; dc.rect = rc; dc.text = s; dc.color = color; dc.fontRole = role; dc.format = format;
        s_recorder->push_back(dc);
    }
    gfx::Text(m_dc, m_fonts.Get(role), color, rc, s, format);
}

void Canvas::TextRow(const RECT& rc, const std::wstring& s, FontRole role,
                     COLORREF color, UINT extra)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::TextRow; dc.rect = rc; dc.text = s; dc.color = color; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    gfx::TextRow(m_dc, m_fonts.Get(role), color, rc, s, extra);
}

void Canvas::TextRowRight(const RECT& rc, const std::wstring& s, FontRole role,
                          COLORREF color, UINT extra)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::TextRowRight; dc.rect = rc; dc.text = s; dc.color = color; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    gfx::TextRowRight(m_dc, m_fonts.Get(role), color, rc, s, extra);
}

void Canvas::TextRowCenter(const RECT& rc, const std::wstring& s, FontRole role,
                           COLORREF color, UINT extra)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::TextRowCenter; dc.rect = rc; dc.text = s; dc.color = color; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    gfx::TextRowCenter(m_dc, m_fonts.Get(role), color, rc, s, extra);
}

void Canvas::TextBlock(const RECT& rc, const std::wstring& s, FontRole role,
                       COLORREF color, UINT extra)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::TextBlock; dc.rect = rc; dc.text = s; dc.color = color; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    gfx::TextBlock(m_dc, m_fonts.Get(role), color, rc, s, extra);
}

int Canvas::MeasureWidth(const std::wstring& s, FontRole role)
{
    return gfx::MeasureWidth(m_dc, m_fonts.Get(role), s);
}

int Canvas::LineHeight(FontRole role)
{
    return gfx::LineHeight(m_dc, m_fonts.Get(role));
}

// ---- 复合部件 ----
int Canvas::Tag(int x, int y, const std::wstring& s, FontRole role,
                COLORREF bg, COLORREF tx, int height)
{
    if (s_recorder) {
        RECT rc = {x, y, x+100, y+height}; // approx
        DrawCall dc; dc.type = DrawCall::Tag; dc.rect = rc; dc.text = s; dc.color = bg; dc.color2 = tx; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    return gfx::Tag(m_dc, x, y, s.c_str(), m_fonts.Get(role), bg, tx, height);
}

void Canvas::PillRight(int rightX, int y, const std::wstring& s, FontRole role,
                       COLORREF bg, COLORREF tx, int height)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::PillRight; dc.text = s; dc.color = bg; dc.color2 = tx; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    gfx::PillRight(m_dc, rightX, y, s.c_str(), m_fonts.Get(role), bg, tx,
                   height);
}

void Canvas::StatusTag(int rightX, int centerY, const std::wstring& s,
                       FontRole role, COLORREF bg, COLORREF border,
                       COLORREF tx, COLORREF dot, int height)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::StatusTag; dc.text = s; dc.color = bg; dc.color2 = border; dc.color3 = tx; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    // Drawing lives in core now; suppress the low-level recorder while
    // forwarding so one high-level StatusTag stays one DrawCall.
    const int w = MeasureWidth(s, role) + 34;
    const Rect rc(rightX - w, centerY - height / 2,
                  rightX, centerY + height / 2);
    StatusTagColors colors;
    colors.bg = bg; colors.border = border;
    colors.text = tx; colors.dot = dot;
    CanvasPainter painter(*this);
    std::vector<DrawCall>* saved = s_recorder;
    s_recorder = nullptr;
    PaintStatusTagColored(painter, rc, s, true, colors, Mx());
    s_recorder = saved;
}

void Canvas::KbdChip(int rightX, int centerY, const std::wstring& key,
                     const std::wstring& label)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::KbdChip; dc.text = key + L":" + label;
        s_recorder->push_back(dc);
    }
    KbdChipState st;
    st.key = key;
    st.label = label;
    const int w = MeasureKbdChip(st, Mx());
    const Rect rc(rightX - w, centerY - 9, rightX, centerY + 9);
    CanvasPainter painter(*this);
    std::vector<DrawCall>* saved = s_recorder;
    s_recorder = nullptr;
    PaintKbdChip(painter, rc, st, Pal(), Mx());
    s_recorder = saved;
}

int Canvas::KbdChipWidth(const std::wstring& key, const std::wstring& label)
{
    return gfx::KbdChipWidth(m_dc, key.c_str(), label.c_str(),
                             m_fonts.Get(FontRole::Meta));
}

void Canvas::IconTile(const RECT& rc, const wchar_t* glyph, FontRole role,
                      COLORREF bg, COLORREF tx, int radius)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::IconTile; dc.rect = rc; dc.color = bg; dc.color2 = tx; dc.radius = radius;
        s_recorder->push_back(dc);
    }
    gfx::IconTile(m_dc, rc, glyph, m_fonts.Get(role), bg, tx, radius);
}

void Canvas::EmptyState(const RECT& rc, const air::ui::EmptyState& state)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::EmptyState; dc.rect = rc;
        s_recorder->push_back(dc);
    }
    EmptyViewState st;
    st.title = state.title;
    st.steps = state.steps;
    st.foot  = state.foot;
    CanvasPainter painter(*this);
    std::vector<DrawCall>* saved = s_recorder;
    s_recorder = nullptr;
    PaintEmptyView(painter, ToRect(rc), st, Pal(), Mx());
    s_recorder = saved;
}

void Canvas::Button(const RECT& rc, const std::wstring& text, BtnStyle style,
                    bool hover, bool pressed, bool disabled)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::Button; dc.rect = rc; dc.text = text; dc.btnStyle = style; dc.hover = hover; dc.pressed = pressed; dc.disabled = disabled;
        s_recorder->push_back(dc);
    }

    // One high-level DrawCall is the documented contract for Button,
    // so the individual fills/text painted by core are not recorded.
    std::vector<DrawCall>* saved = s_recorder;
    s_recorder = nullptr;

    ControlState st;
    st.hover    = hover;
    st.pressed  = pressed;
    st.disabled = disabled;

    CanvasPainter painter(*this);
    PaintButton(painter, ToRect(rc), text, style, st, Pal(), Mx());

    s_recorder = saved;
}

void Canvas::Checkbox(const RECT& rc, const std::wstring& text, bool checked,
                      bool hover, bool disabled)
{
    ControlState st;
    st.checked  = checked;
    st.hover    = hover;
    st.disabled = disabled;

    CanvasPainter painter(*this);
    PaintCheckbox(painter, ToRect(rc), text, st, Pal());
}

void Canvas::Radio(const RECT& rc, const std::wstring& text, bool checked,
                   bool hover, bool disabled)
{
    ControlState st;
    st.checked  = checked;
    st.hover    = hover;
    st.disabled = disabled;

    CanvasPainter painter(*this);
    PaintRadio(painter, ToRect(rc), text, st, Pal());
}

} // namespace ui
} // namespace air
