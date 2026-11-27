#include "internal/Canvas.h"
#include "internal/CanvasPainter.h"
#include "airui/ControlsCore.h"
#include "airui/ControlsExtra.h"
#include "internal/GfxPainter.h"
#include "internal/PaintFrame.h"
#include "internal/Controls.h"

#include <cwchar>

namespace air {
namespace ui {

std::vector<DrawCall>* Canvas::s_recorder = nullptr;

Canvas::Canvas(DeviceContextHandle dc, const Fonts& fonts) : m_dc(dc), m_fonts(fonts)
{
    if (m_dc) SetBkMode(reinterpret_cast<HDC>(m_dc), TRANSPARENT);
}

Canvas::~Canvas()
{
    // A page that forgets a PopClip would leak the clip into the rest of
    // the frame (GDI: the DC stays clipped; Skia: the backend clip stack
    // stays one level too deep). Balanced code never gets here.
    while (!m_clipStack.empty()) PopClip();
}

// ---- painters ----
IPainter& Canvas::Gdi()
{
    // Created on first use so a frame-backed paint (Skia) never pays for
    // it. GfxPainter forwards to gfx::*, which keeps the native pixels
    // byte-identical to the pre-HAL Canvas.
    if (!m_gdi) m_gdi.reset(new GfxPainter(m_dc, m_fonts));
    return *m_gdi;
}

IPainter& Canvas::Painter()
{
    // One frame is open for the whole client area while a compositing
    // backend is active; every primitive belongs to that frame. With no
    // frame the legacy GDI helpers are the target.
    if (PaintFrame* f = CurrentPaintFrame()) return *f;
    return Gdi();
}

// ---- clip ----
void Canvas::PushClip(const Rect& rc)
{
    // A compositing backend renders into its own surface: the native clip
    // region does not exist there, so the clip has to go to the frame
    // (and from there to the backend's clip stack).
    if (PaintFrame* f = CurrentPaintFrame()) {
        f->PushClip(rc);
        m_clipStack.push_back(kFrameClip);
        return;
    }
    Gdi().PushClip(rc);
    m_clipStack.push_back(kGdiClip);
}

void Canvas::PopClip()
{
    if (m_clipStack.empty()) return;
    const int kind = m_clipStack.back();
    m_clipStack.pop_back();
    if (kind == kFrameClip) {
        if (PaintFrame* f = CurrentPaintFrame()) f->PopClip();
        return;
    }
    if (m_gdi) m_gdi->PopClip();
}

// ---- 形状 ----
void Canvas::Fill(const Rect& rc, Color color)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::Fill; dc.rect = rc; dc.color = color;
        s_recorder->push_back(dc);
    }
    Painter().FillRect(rc, color);
}

void Canvas::FillRound(const Rect& rc, Color color, int radius)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::FillRound; dc.rect = rc; dc.color = color; dc.radius = radius;
        s_recorder->push_back(dc);
    }
    Painter().FillRound(rc, radius, color);
}

void Canvas::Stroke(const Rect& rc, Color color, int radius, int width)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::Stroke; dc.rect = rc; dc.color = color; dc.radius = radius; dc.width = width;
        s_recorder->push_back(dc);
    }
    IPainter& p = Painter();
    if (radius > 0) p.StrokeRound(rc, radius, color, width);
    else            p.StrokeRect(rc, color, width);
}

void Canvas::StrokeDashed(const Rect& rc, Color color, int radius)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::StrokeDashed; dc.rect = rc; dc.color = color; dc.radius = radius;
        s_recorder->push_back(dc);
    }
    // dash=4 is the legacy gfx::* rhythm the frame router used to send;
    // the GDI fallback ignores it and uses its native PS_DASH pen.
    (void)radius;   // IPainter has no rounded dashed variant
    Painter().StrokeDashed(rc, color, 1, 4);
}

void Canvas::HLine(int x1, int x2, int y, Color color)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::HLine; dc.rect = {x1, y, x2, y+1}; dc.color = color;
        s_recorder->push_back(dc);
    }
    Painter().HLine(x1, x2, y, color);
}

void Canvas::HLineDashed(int x1, int x2, int y, Color color)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::HLineDashed; dc.rect = {x1, y, x2, y+1}; dc.color = color;
        s_recorder->push_back(dc);
    }
    Painter().HLineDashed(x1, x2, y, color);
}

void Canvas::VLine(int x, int y1, int y2, Color color)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::VLine; dc.rect = {x, y1, x+1, y2}; dc.color = color;
        s_recorder->push_back(dc);
    }
    Painter().VLine(x, y1, y2, color);
}

void Canvas::Dot(int cx, int cy, int diameter, Color color)
{
    if (s_recorder) {
        RECT rc = {cx - diameter/2, cy - diameter/2, cx + diameter/2, cy + diameter/2};
        DrawCall dc; dc.type = DrawCall::Dot; dc.rect = ToRect(rc); dc.color = color;
        s_recorder->push_back(dc);
    }
    Painter().Dot(cx, cy, diameter, color);
}

void Canvas::ProgressBar(const Rect& rc, int pct, Color track,
                         Color colorNormal, Color colorWarn,
                         Color colorHot)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::ProgressBar; dc.rect = rc; dc.color = track; dc.color2 = colorNormal; dc.color3 = colorWarn;
        s_recorder->push_back(dc);
    }
    Painter().ProgressBar(rc, pct, track, colorNormal, colorWarn, colorHot);
}

// ---- 文本 ----
void Canvas::Text(const Rect& rc, const std::wstring& s, FontRole role,
                  Color color, TextFormat format)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::Text; dc.rect = rc; dc.text = s; dc.color = color; dc.fontRole = role; dc.format = format;
        s_recorder->push_back(dc);
    }
    Painter().Text(rc, s.c_str(), s.size(), role, color, format);
}

// The Row/Block helpers used to go through gfx, which appended its own
// flags to the caller's extra bits. Send the exact same format so the
// frame router and GDI both draw what they drew before.
void Canvas::TextRow(const Rect& rc, const std::wstring& s, FontRole role,
                     Color color, TextFormat extra)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::TextRow; dc.rect = rc; dc.text = s; dc.color = color; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    Painter().Text(rc, s.c_str(), s.size(), role, color,
                   dt::SingleLine | dt::Left | dt::VCenter |
                   dt::EndEllipsis | extra);
}

void Canvas::TextRowRight(const Rect& rc, const std::wstring& s, FontRole role,
                          Color color, TextFormat extra)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::TextRowRight; dc.rect = rc; dc.text = s; dc.color = color; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    Painter().Text(rc, s.c_str(), s.size(), role, color,
                   dt::SingleLine | dt::Right | dt::VCenter |
                   dt::EndEllipsis | extra);
}

void Canvas::TextRowCenter(const Rect& rc, const std::wstring& s, FontRole role,
                           Color color, TextFormat extra)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::TextRowCenter; dc.rect = rc; dc.text = s; dc.color = color; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    Painter().Text(rc, s.c_str(), s.size(), role, color,
                   dt::SingleLine | dt::Center | dt::VCenter |
                   dt::EndEllipsis | extra);
}

void Canvas::TextBlock(const Rect& rc, const std::wstring& s, FontRole role,
                       Color color, TextFormat extra)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::TextBlock; dc.rect = rc; dc.text = s; dc.color = color; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    // Not IPainter::TextBlock: that one adds dt::NoPrefix and drops the
    // caller's extra bits, which would change the glyphs for paths that
    // pass dt::EditCtrl / dt::ExpandTabs.
    Painter().Text(rc, s.c_str(), s.size(), role, color,
                   dt::WordBreak | dt::EndEllipsis | dt::Left | extra);
}

int Canvas::MeasureWidth(const std::wstring& s, FontRole role)
{
    return Painter().MeasureText(s.c_str(), s.size(), role);
}

int Canvas::LineHeight(FontRole role)
{
    return Painter().LineHeight(role);
}

// ---- 复合部件 ----
int Canvas::Tag(int x, int y, const std::wstring& s, FontRole role,
                Color bg, Color tx, int height)
{
    if (s_recorder) {
        RECT rc = {x, y, x+100, y+height}; // approx
        DrawCall dc; dc.type = DrawCall::Tag; dc.rect = ToRect(rc); dc.text = s; dc.color = bg; dc.color2 = tx; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    // Same geometry the gfx::Tag helper used: 8px side padding, 2px
    // radius, single-line left/centre text without prefix processing.
    IPainter& p = Painter();
    const int w = p.MeasureText(s.c_str(), s.size(), role) + 16;
    const Rect rc(x, y, x + w, y + height);
    p.FillRound(rc, 2, bg);
    const Rect inner(rc.left + 8, rc.top, rc.right - 8, rc.bottom);
    p.Text(inner, s.c_str(), s.size(), role, tx,
           dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix);
    return rc.right;
}

void Canvas::PillRight(int rightX, int y, const std::wstring& s, FontRole role,
                       Color bg, Color tx, int height)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::PillRight; dc.text = s; dc.color = bg; dc.color2 = tx; dc.fontRole = role;
        s_recorder->push_back(dc);
    }
    IPainter& p = Painter();
    const int w = p.MeasureText(s.c_str(), s.size(), role) + 18;
    const Rect rc(rightX - w, y, rightX, y + height);
    p.FillRound(rc, height / 2, bg);
    const Rect inner(rc.left + 9, rc.top, rc.right - 9, rc.bottom);
    p.Text(inner, s.c_str(), s.size(), role, tx,
           dt::SingleLine | dt::Left | dt::VCenter | dt::NoPrefix);
}

void Canvas::StatusTag(int rightX, int centerY, const std::wstring& s,
                       FontRole role, Color bg, Color border,
                       Color tx, Color dot, int height)
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
    // Mirrors gfx::KbdChipWidth: key text + 12px key box padding, 5px
    // gap, label text.
    IPainter& p = Painter();
    return p.MeasureText(key.c_str(), key.size(), FontRole::Meta) + 12 + 5 +
           p.MeasureText(label.c_str(), label.size(), FontRole::Meta);
}

void Canvas::IconTile(const Rect& rc, const wchar_t* glyph, FontRole role,
                      Color bg, Color tx, int radius)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::IconTile; dc.rect = rc; dc.color = bg; dc.color2 = tx; dc.radius = radius;
        s_recorder->push_back(dc);
    }
    IPainter& p = Painter();
    p.FillRound(rc, radius, bg);
    p.Text(rc, glyph, glyph ? std::wcslen(glyph) : 0, role, tx,
           dt::SingleLine | dt::Center | dt::VCenter | dt::NoPrefix);
}

void Canvas::EmptyState(const Rect& rc, const air::ui::EmptyState& state)
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
    PaintEmptyView(painter, rc, st, Pal(), Mx());
    s_recorder = saved;
}

void Canvas::Pagination(const Rect& rc, const air::ui::PaginationState& state)
{
    if (s_recorder) {
        DrawCall dc; dc.type = DrawCall::Pagination; dc.rect = rc;
        s_recorder->push_back(dc);
    }
    CanvasPainter painter(*this);
    std::vector<DrawCall>* saved = s_recorder;
    s_recorder = nullptr;
    air::ui::PaintPagination(painter, rc, state, Pal(), Mx());
    s_recorder = saved;
}

std::vector<air::ui::PaginationSlot> Canvas::PaginationLayout(
    const Rect& rc, const air::ui::PaginationState& state) const
{
    return air::ui::MeasurePagination(rc, state, Mx());
}

int Canvas::PaginationHit(const Rect& rc, const air::ui::PaginationState& state,
                          Point pt) const
{
    return air::ui::PaginationHit(rc, state, Mx(), pt);
}

void Canvas::Button(const Rect& rc, const std::wstring& text, BtnStyle style,
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
    PaintButton(painter, rc, text, style, st, Pal(), Mx());

    s_recorder = saved;
}

void Canvas::Checkbox(const Rect& rc, const std::wstring& text, bool checked,
                      bool hover, bool disabled)
{
    ControlState st;
    st.checked  = checked;
    st.hover    = hover;
    st.disabled = disabled;

    CanvasPainter painter(*this);
    PaintCheckbox(painter, rc, text, st, Pal());
}

void Canvas::Radio(const Rect& rc, const std::wstring& text, bool checked,
                   bool hover, bool disabled)
{
    ControlState st;
    st.checked  = checked;
    st.hover    = hover;
    st.disabled = disabled;

    CanvasPainter painter(*this);
    PaintRadio(painter, rc, text, st, Pal());
}

} // namespace ui
} // namespace air
