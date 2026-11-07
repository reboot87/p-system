// ============================================================
// AirUI - SkiaPainter.cpp
// IPainter -> SkCanvas. This is the second real rendering backend
// (the first being the Win32 GDI backend), and the piece that makes
// the G5 cross-backend DrawCall comparison possible.
// ============================================================
#include "airui/SkiaPainter.h"
#include "airui/SkiaApi.h"
#include "airui/SkiaTypes.h"

#include <algorithm>
#include <string>
#include <vector>

namespace air {
namespace ui {

namespace {

SkRect ToSk(const Rect& r)
{
    return SkRect::MakeLTRB(static_cast<SkScalar>(r.left),
                            static_cast<SkScalar>(r.top),
                            static_cast<SkScalar>(r.right),
                            static_cast<SkScalar>(r.bottom));
}

SkPaint MakeFill(Color c, uint8_t alpha = 255)
{
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kFill_Style);
    p.setColor(skia::ToSkColor(c, alpha));
    return p;
}

SkPaint MakeStroke(Color c, int width)
{
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(static_cast<SkScalar>(width > 0 ? width : 1));
    p.setColor(skia::ToSkColor(c));
    return p;
}

size_t Utf8Unit(const std::string& s, size_t i)
{
    const unsigned char b = static_cast<unsigned char>(s[i]);
    size_t n = 1;
    if      (b >= 0xF0u) n = 4;
    else if (b >= 0xE0u) n = 3;
    else if (b >= 0xC0u) n = 2;
    return (i + n <= s.size()) ? n : 1;
}

} // namespace

struct SkiaPainter::Impl
{
    explicit Impl(SkCanvas* canvas) : canvas(canvas) {}

    SkCanvas* canvas = nullptr;
    SkFont    fonts[static_cast<int>(FontRole::Count)];
    bool      ready[static_cast<int>(FontRole::Count)] = {};
    int       clipDepth = 0;

    SkFont& Font(FontRole role)
    {
        int i = static_cast<int>(role);
        if (i < 0 || i >= static_cast<int>(FontRole::Count)) i = 0;
        if (!ready[i]) {
            SkFont f;
            // Skia's font factory keeps moving between releases
            // (SkTypeface::MakeFromName is gone; choose-your-own SkFontMgr
            // is platform-dependent). SkFont's default typeface already
            // uses the platform font manager, so only the role size is
            // set here. Family alignment with GDI is a separate task.
            f.setSize(static_cast<SkScalar>(skia::RoleFontSize(role)));
            fonts[i] = f;
            ready[i] = true;
        }
        return fonts[i];
    }

    float Width(const std::string& utf8, SkFont& f) const
    {
        if (utf8.empty()) return 0.f;
        return f.measureText(utf8.data(), utf8.size(), SkTextEncoding::kUTF8);
    }

    float Baseline(const Rect& r, SkFont& f, skia::VAlign v) const
    {
        SkFontMetrics fm{};
        f.getMetrics(&fm);
        const float line = fm.fDescent - fm.fAscent;
        switch (v) {
        case skia::VAlign::Middle:
            return static_cast<float>(r.top)
                 + (static_cast<float>(r.Height()) - line) / 2.f - fm.fAscent;
        case skia::VAlign::Bottom:
            return static_cast<float>(r.bottom) - fm.fDescent;
        default:
            return static_cast<float>(r.top) - fm.fAscent;
        }
    }
};

SkiaPainter::SkiaPainter(SkCanvas* canvas)
    : m(new Impl(canvas)) {}

SkiaPainter::~SkiaPainter() = default;

SkCanvas* SkiaPainter::Canvas() const { return m->canvas; }

void SkiaPainter::FillRect(const Rect& r, Color c)
{
    if (!m->canvas || r.Empty()) return;
    m->canvas->drawRect(ToSk(r), MakeFill(c));
}

void SkiaPainter::FillRectAlpha(const Rect& r, Color c, uint8_t alpha)
{
    if (alpha == 0 || !m->canvas || r.Empty()) return;
    m->canvas->drawRect(ToSk(r), MakeFill(c, alpha));
}

void SkiaPainter::FillRound(const Rect& r, int radius, Color c)
{
    if (!m->canvas || r.Empty()) return;
    const SkScalar rr = static_cast<SkScalar>(radius > 0 ? radius : 0);
    m->canvas->drawRRect(SkRRect::MakeRectXY(ToSk(r), rr, rr), MakeFill(c));
}

void SkiaPainter::StrokeRect(const Rect& r, Color c, int width)
{
    if (!m->canvas || r.Empty()) return;
    m->canvas->drawRect(ToSk(r), MakeStroke(c, width));
}

void SkiaPainter::StrokeRound(const Rect& r, int radius, Color c, int width)
{
    if (!m->canvas || r.Empty()) return;
    const SkScalar rr = static_cast<SkScalar>(radius > 0 ? radius : 0);
    m->canvas->drawRRect(SkRRect::MakeRectXY(ToSk(r), rr, rr), MakeStroke(c, width));
}

void SkiaPainter::StrokeDashed(const Rect& r, Color c, int width, int dash)
{
    if (!m->canvas || r.Empty()) return;
    // SkPath is immutable in current Skia: contours are built with
    // SkPathBuilder and materialised with detach().
    SkPathBuilder builder;
    builder.addRect(ToSk(r));
    const SkPath path = builder.detach();
    const SkScalar d = static_cast<SkScalar>(dash > 0 ? dash : 6);
    const SkScalar gap = static_cast<SkScalar>(dash > 0 ? dash : 4);
    const SkScalar intervals[2] = { d, gap };
    SkPaint p = MakeStroke(c, width);
    // Make() now takes (SkSpan<const SkScalar> intervals, SkScalar phase);
    // the old (pointer, count, phase) overload is gone.
    p.setPathEffect(SkDashPathEffect::Make(
        SkSpan<const SkScalar>(intervals, 2), 0));
    m->canvas->drawPath(path, p);
}

void SkiaPainter::Text(const Rect& r, const std::wstring& s,
                       FontRole role, Color c, TextFormat fmt)
{
    if (!m->canvas || s.empty() || r.Empty()) return;
    std::string utf8 = skia::WideToUtf8(s);
    SkFont& f = m->Font(role);
    const float avail = static_cast<float>(r.Width());

    if ((fmt & dt::EndEllipsis) && m->Width(utf8, f) > avail) {
        const float dotsW = f.measureText("...", 3, SkTextEncoding::kUTF8);
        size_t cut = utf8.size();
        while (cut > 0) {
            --cut;
            while (cut > 0 && (static_cast<unsigned char>(utf8[cut]) & 0xC0u) == 0x80u)
                --cut;
            if (f.measureText(utf8.data(), cut, SkTextEncoding::kUTF8) + dotsW <= avail) {
                utf8.resize(cut);
                utf8 += "...";
                break;
            }
        }
    }

    const float w = m->Width(utf8, f);
    float x = static_cast<float>(r.left);
    switch (skia::Horizontal(fmt)) {
    case skia::HAlign::Center: x += (avail - w) / 2.f; break;
    case skia::HAlign::Right:  x += avail - w;         break;
    default: break;
    }
    const float y = m->Baseline(r, f, skia::Vertical(fmt));
    m->canvas->drawSimpleText(utf8.data(), utf8.size(), SkTextEncoding::kUTF8,
                              x, y, f, MakeFill(c));
}

void SkiaPainter::TextBlock(const Rect& r, const std::wstring& s,
                            FontRole role, Color c, int maxLines)
{
    if (!m->canvas || s.empty() || r.Empty()) return;
    const std::string utf8 = skia::WideToUtf8(s);
    SkFont& f = m->Font(role);
    SkFontMetrics fm{};
    f.getMetrics(&fm);
    const float lineH = fm.fDescent - fm.fAscent;
    const float avail = static_cast<float>(r.Width());
    const int limit = maxLines > 0 ? maxLines : 1;

    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    while (i < utf8.size() && static_cast<int>(lines.size()) < limit) {
        if (utf8[i] == '\n') {
            lines.push_back(cur);
            cur.clear();
            ++i;
            continue;
        }
        const size_t n = Utf8Unit(utf8, i);
        const std::string next = cur + utf8.substr(i, n);
        if (!cur.empty() &&
            f.measureText(next.data(), next.size(), SkTextEncoding::kUTF8) > avail) {
            lines.push_back(cur);
            cur = utf8.substr(i, n);
        } else {
            cur = next;
        }
        i += n;
    }
    if (!cur.empty() && static_cast<int>(lines.size()) < limit)
        lines.push_back(cur);

    const SkPaint p = MakeFill(c);
    float y = static_cast<float>(r.top) - fm.fAscent;
    for (const std::string& ln : lines) {
        if (y - fm.fDescent > static_cast<float>(r.bottom)) break;
        if (!ln.empty())
            m->canvas->drawSimpleText(ln.data(), ln.size(), SkTextEncoding::kUTF8,
                                      static_cast<float>(r.left), y, f, p);
        y += lineH;
    }
}

int SkiaPainter::MeasureText(const std::wstring& s, FontRole role)
{
    if (s.empty()) return 0;
    const std::string utf8 = skia::WideToUtf8(s);
    const float w = m->Width(utf8, m->Font(role));
    return static_cast<int>(w + 0.5f);
}

void SkiaPainter::PushClip(const Rect& r)
{
    if (!m->canvas) return;
    m->canvas->save();
    m->canvas->clipRect(ToSk(r));
    ++m->clipDepth;
}

void SkiaPainter::PopClip()
{
    if (!m->canvas || m->clipDepth <= 0) return;
    m->canvas->restore();
    --m->clipDepth;
}

void SkiaPainter::Translate(Point offset)
{
    // Additive, matching GdiPainter's SetViewportOrgEx behaviour.
    if (!m->canvas) return;
    m->canvas->translate(static_cast<SkScalar>(offset.x),
                         static_cast<SkScalar>(offset.y));
}

} // namespace ui
} // namespace air
