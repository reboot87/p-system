// ============================================================
// AirUI - PaintFrame.cpp
// Frame-scoped routing of every drawing primitive to a runtime
// selected IPainter. See PaintFrame.h for the architecture notes.
// ============================================================
#include "airui/PaintFrame.h"

#include "airui/Fonts.h"

#include <algorithm>
#include <cstring>

namespace air {
namespace ui {
namespace {

PaintFrame* g_current = nullptr;

inline Rect ToUi(const RECT& rc)
{
    return Rect(rc.left, rc.top, rc.right, rc.bottom);
}

inline RECT ToWin(const Rect& r)
{
    RECT rc = { r.left, r.top, r.right, r.bottom };
    return rc;
}

inline Color ToUi(COLORREF c)
{
    return static_cast<Color>(c & 0x00FFFFFFu);
}

inline COLORREF ToWin(Color c)
{
    return static_cast<COLORREF>(c & 0x00FFFFFFu);
}

const Fonts& AnyFonts(const Fonts* preferred)
{
    return preferred ? *preferred : Fonts::Default();
}

// ------------------------------------------------------------
// Per-primitive cost (AIRUI_FRAME_PROFILE=1)
//
// The frame timer in Shell only says "draw = 24 ms". That number is the
// sum of every routed call, and the split between cheap fills and
// expensive text is what decides where to optimise, so measure the
// backend call itself, per primitive kind. QPC costs ~20 ns per pair,
// which is noise against calls that run tens of microseconds.
// ------------------------------------------------------------
struct PrimCost
{
    long long n  = 0;
    long long us = 0;
};

struct PrimStats
{
    PrimCost fill;
    PrimCost fillAlpha;
    PrimCost fillRound;
    PrimCost stroke;
    PrimCost strokeDashed;
    PrimCost text;
    PrimCost textEll;      // Text() calls that carry DT_END_ELLIPSIS
    PrimCost textPlain;    // ... and the ones that do not
    PrimCost textBlock;
    PrimCost measure;
    PrimCost clip;
    long long textChars = 0;
    long long frames = 0;

    void Reset()
    {
        fill = fillAlpha = fillRound = stroke = strokeDashed = PrimCost();
        text = textEll = textPlain = textBlock = measure = clip = PrimCost();
        textChars = 0;
        frames = 0;
    }

    void Dump()
    {
        if (frames <= 0) return;
        wchar_t path[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH) == 0) return;
        wcscat_s(path, L"\\AireRecord\\prim_cost.log");
        FILE* f = _wfsopen(path, L"a", _SH_DENYWR);
        if (!f) return;
        fwprintf_s(f, L"[prim] frames=%lld\n", frames);
        auto row = [&](const wchar_t* name, const PrimCost& c) {
            fwprintf_s(f, L"  %-12s calls/frame=%7.1f us/call=%7.2f total/frame=%7.2f ms\n",
                       name,
                       (double)c.n / frames,
                       c.n ? (double)c.us / c.n : 0.0,
                       (double)c.us / frames / 1000.0);
        };
        row(L"fill",         fill);
        row(L"fillAlpha",    fillAlpha);
        row(L"fillRound",    fillRound);
        row(L"stroke",       stroke);
        row(L"strokeDashed", strokeDashed);
        PrimCost textAll;
        textAll.n  = textEll.n + textPlain.n;
        textAll.us = textEll.us + textPlain.us;
        row(L"text",         textAll);
        row(L"  ellipsis",   textEll);
        row(L"  plain",      textPlain);
        fwprintf_s(f, L"  chars/frame=%7.1f  us/char=%7.3f\n",
                   (double)textChars / frames,
                   textChars ? (double)textAll.us / textChars : 0.0);
        row(L"textBlock",    textBlock);
        row(L"measure",      measure);
        row(L"clip",         clip);
        fclose(f);
        Reset();
    }
};

bool PrimProfileEnabled()
{
    static int on = -1;
    if (on < 0) {
        wchar_t v[8] = {};
        const DWORD n = GetEnvironmentVariableW(L"AIRUI_FRAME_PROFILE", v, 8);
        on = (n > 0 && n < 8 && v[0] != L'0') ? 1 : 0;
    }
    return on != 0;
}

inline long long PrimQpcUs()
{
    static const double freq = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return f.QuadPart > 0 ? (double)f.QuadPart : 1.0;
    }();
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return static_cast<long long>(c.QuadPart * 1000000.0 / freq);
}

PrimStats& Stats()
{
    static PrimStats s;
    return s;
}

// Times one backend call. Zero cost when profiling is off.
struct PrimScope
{
    PrimCost& c;
    long long t0;
    explicit PrimScope(PrimCost& pc) : c(pc), t0(PrimProfileEnabled() ? PrimQpcUs() : 0) {}
    ~PrimScope()
    {
        if (!t0) return;
        c.us += PrimQpcUs() - t0;
        ++c.n;
    }
};

} // namespace

void PaintFrameReportPrimCost()
{
    if (!PrimProfileEnabled()) return;
    PrimStats& s = Stats();
    if (++s.frames >= 30) s.Dump();
}

PaintFrame& PaintFrame::Instance()
{
    static PaintFrame frame;
    return frame;
}

PaintFrame* CurrentPaintFrame()
{
    return g_current;
}

// ------------------------------------------------------------
// Frame lifetime
// ------------------------------------------------------------
void PaintFrame::Begin(IPainter* backend, HDC dc, const Fonts* fonts,
                       bool backendOwnsText)
{
    m_backend         = backend;
    m_dc              = dc;
    m_fonts           = fonts;
    m_backendOwnsText = backendOwnsText;
    m_offset          = Point{};
    m_clips.clear();
    m_textOps.clear();
    m_active = (backend != nullptr && dc != nullptr);
    g_current = m_active ? this : nullptr;
}

void PaintFrame::End()
{
    g_current = nullptr;
    m_active  = false;
}

void PaintFrame::Clear()
{
    m_textOps.clear();
    m_clips.clear();
    m_offset = Point{};
}

void PaintFrame::ReplayText()
{
    // The backend frame has already been composited onto m_dc here.
    // Host-rasterised text sits on top of that composite.
    if (m_backendOwnsText || !m_dc || m_textOps.empty()) return;

    for (const TextOp& op : m_textOps) {
        const int saved = SaveDC(m_dc);
        if (op.clipped)
            IntersectClipRect(m_dc, op.clip.left, op.clip.top,
                              op.clip.right, op.clip.bottom);
        SetBkMode(m_dc, TRANSPARENT);
        SetTextColor(m_dc, op.color);
        HGDIOBJ old = op.font ? SelectObject(m_dc, op.font) : nullptr;
        RECT rc = op.rc;
        DrawTextW(m_dc, op.text.c_str(), static_cast<int>(op.text.size()),
                  &rc, op.format);
        if (old) SelectObject(m_dc, old);
        RestoreDC(m_dc, saved);
    }
}

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
Rect PaintFrame::Shift(const Rect& r) const
{
    return Rect(r.left + m_offset.x, r.top + m_offset.y,
                r.right + m_offset.x, r.bottom + m_offset.y);
}

Rect PaintFrame::CurrentClip() const
{
    if (m_clips.empty()) return Rect{};
    Rect out = m_clips.front();
    for (size_t i = 1; i < m_clips.size(); ++i) {
        out.left   = (std::max)(out.left,   m_clips[i].left);
        out.top    = (std::max)(out.top,    m_clips[i].top);
        out.right  = (std::min)(out.right,  m_clips[i].right);
        out.bottom = (std::min)(out.bottom, m_clips[i].bottom);
    }
    return out;
}

HFONT PaintFrame::RoleFont(FontRole role) const
{
    return AnyFonts(m_fonts).Get(role);
}

FontRole PaintFrame::FontRoleOf(HFONT font) const
{
    const Fonts& f = AnyFonts(m_fonts);
    for (int i = 0; i < static_cast<int>(FontRole::Count); ++i) {
        const FontRole role = static_cast<FontRole>(i);
        if (f.Get(role) == font) return role;
    }
    return FontRole::Body;
}

void PaintFrame::RecordText(HFONT font, COLORREF color, const RECT& rc,
                            const std::wstring& s, UINT format)
{
    if (s.empty()) return;
    TextOp op;
    op.rc     = ToWin(Shift(ToUi(rc)));
    op.font   = font;
    op.color  = color;
    op.format = format;
    op.text   = s;
    op.clipped = !m_clips.empty();
    op.clip   = CurrentClip();
    m_textOps.push_back(std::move(op));
}

// ------------------------------------------------------------
// gfx::* shims
// ------------------------------------------------------------
void PaintFrame::GfxFill(const RECT& rc, COLORREF color)
{
    PrimScope _ps(Stats().fill);
    m_backend->FillRect(Shift(ToUi(rc)), ToUi(color));
}

void PaintFrame::GfxFillRound(const RECT& rc, COLORREF color, int radius)
{
    PrimScope _ps(Stats().fillRound);
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;
    m_backend->FillRound(Shift(ToUi(rc)), radius, ToUi(color));
}

void PaintFrame::GfxStroke(const RECT& rc, COLORREF color,
                           int radius, int width)
{
    PrimScope _ps(Stats().stroke);
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;
    const Rect r = Shift(ToUi(rc));
    const int w = width > 0 ? width : 1;
    if (radius > 0) m_backend->StrokeRound(r, radius, ToUi(color), w);
    else            m_backend->StrokeRect(r, ToUi(color), w);
}

void PaintFrame::GfxStrokeDashed(const RECT& rc, COLORREF color, int radius)
{
    PrimScope _ps(Stats().strokeDashed);
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;
    (void)radius;   // IPainter has no rounded dashed variant
    m_backend->StrokeDashed(Shift(ToUi(rc)), ToUi(color), 1, 4);
}

void PaintFrame::GfxHLine(int x1, int x2, int y, COLORREF color)
{
    PrimScope _ps(Stats().fill);
    m_backend->FillRect(Shift(Rect(x1, y, x2, y + 1)), ToUi(color));
}

void PaintFrame::GfxHLineDashed(int x1, int x2, int y, COLORREF color)
{
    PrimScope _ps(Stats().fill);
    // PS_DASH is roughly 4px on / 3px off; paint the same rhythm so the
    // backend receives ordinary rects (no path API needed).
    const Color c = ToUi(color);
    for (int x = x1; x < x2; x += 7)
        m_backend->FillRect(Shift(Rect(x, y, (std::min)(x + 4, x2), y + 1)), c);
}

void PaintFrame::GfxVLine(int x, int y1, int y2, COLORREF color)
{
    PrimScope _ps(Stats().fill);
    m_backend->FillRect(Shift(Rect(x, y1, x + 1, y2)), ToUi(color));
}

void PaintFrame::GfxDot(int cx, int cy, int diameter, COLORREF color)
{
    PrimScope _ps(Stats().fillRound);
    const int r = diameter / 2;
    m_backend->FillRound(Shift(Rect(cx - r, cy - r,
                                    cx - r + diameter, cy - r + diameter)),
                         (std::max)(r, 1), ToUi(color));
}

void PaintFrame::GfxText(HFONT font, COLORREF color, const RECT& rc,
                         const wchar_t* s, UINT format)
{
    const bool ell = (format & DT_END_ELLIPSIS) != 0;
    PrimScope _ps(ell ? Stats().textEll : Stats().textPlain);
    if (PrimProfileEnabled() && s) Stats().textChars += (long long)wcslen(s);
    if (!s) return;
    if (m_backendOwnsText) {
        m_backend->Text(Shift(ToUi(rc)), s, FontRoleOf(font),
                        ToUi(color), static_cast<TextFormat>(format));
    } else {
        RecordText(font, color, rc, s, format);
    }
}

// ------------------------------------------------------------
// IPainter
// ------------------------------------------------------------
void PaintFrame::FillRect(const Rect& r, Color c)
{
    PrimScope _ps(Stats().fill);
    m_backend->FillRect(Shift(r), c);
}

void PaintFrame::FillRectAlpha(const Rect& r, Color c, uint8_t alpha)
{
    PrimScope _ps(Stats().fillAlpha);
    m_backend->FillRectAlpha(Shift(r), c, alpha);
}

void PaintFrame::FillRound(const Rect& r, int radius, Color c)
{
    PrimScope _ps(Stats().fillRound);
    m_backend->FillRound(Shift(r), radius, c);
}

void PaintFrame::StrokeRect(const Rect& r, Color c, int width)
{
    PrimScope _ps(Stats().stroke);
    m_backend->StrokeRect(Shift(r), c, width);
}

void PaintFrame::StrokeRound(const Rect& r, int radius, Color c, int width)
{
    PrimScope _ps(Stats().stroke);
    m_backend->StrokeRound(Shift(r), radius, c, width);
}

void PaintFrame::StrokeDashed(const Rect& r, Color c, int width, int dash)
{
    PrimScope _ps(Stats().strokeDashed);
    m_backend->StrokeDashed(Shift(r), c, width, dash);
}

void PaintFrame::Text(const Rect& r, const std::wstring& s,
                      FontRole role, Color c, TextFormat fmt)
{
    const bool ell = (static_cast<UINT>(fmt) & DT_END_ELLIPSIS) != 0;
    PrimScope _ps(ell ? Stats().textEll : Stats().textPlain);
    if (PrimProfileEnabled()) Stats().textChars += (long long)s.size();
    if (m_backendOwnsText) {
        m_backend->Text(Shift(r), s, role, c, fmt);
    } else {
        RecordText(RoleFont(role), ToWin(c), ToWin(Shift(r)), s,
                   static_cast<UINT>(fmt));
    }
}

void PaintFrame::TextBlock(const Rect& r, const std::wstring& s,
                           FontRole role, Color c, int maxLines)
{
    PrimScope _ps(Stats().textBlock);
    if (m_backendOwnsText) {
        m_backend->TextBlock(Shift(r), s, role, c, maxLines);
    } else {
        // DrawTextW wraps and clips to rc by itself; maxLines only
        // matters for backends that lay out text manually.
        RecordText(RoleFont(role), ToWin(c), ToWin(Shift(r)), s,
                   DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

int PaintFrame::MeasureText(const std::wstring& s, FontRole role)
{
    PrimScope _ps(Stats().measure);
    // Layout must agree with the glyphs that actually appear on screen.
    // In host-text mode those glyphs are GDI's, so measure with GDI too.
    if (s.empty() || !m_dc) return 0;
    HGDIOBJ old = SelectObject(m_dc, RoleFont(role));
    SIZE sz{};
    GetTextExtentPoint32W(m_dc, s.c_str(), static_cast<int>(s.size()), &sz);
    SelectObject(m_dc, old);
    return sz.cx;
}

void PaintFrame::PushClip(const Rect& r)
{
    PrimScope _ps(Stats().clip);
    const Rect rr = Shift(r);
    m_backend->PushClip(rr);
    m_clips.push_back(rr);
}

void PaintFrame::PopClip()
{
    PrimScope _ps(Stats().clip);
    m_backend->PopClip();
    if (!m_clips.empty()) m_clips.pop_back();
}

void PaintFrame::Translate(Point offset)
{
    // Keep the transform per painter rather than translating the whole
    // backend canvas: gfx::* and CanvasPainter both route here, and a
    // canvas-wide translate would also shift unrelated page content.
    m_offset.x += offset.x;
    m_offset.y += offset.y;
}

} // namespace ui
} // namespace air
