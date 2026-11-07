// ============================================================
// AirUI - SkiaPainter.cpp
// IPainter -> SkCanvas. This is the second real rendering backend
// (the first being the Win32 GDI backend), and the piece that makes
// the G5 cross-backend DrawCall comparison possible.
// ============================================================
#include "airui/SkiaPainter.h"
#include "airui/SkiaApi.h"
#include "airui/SkiaTypes.h"
#include "airui/UiText.h"

#include <algorithm>
#include <string>
#include <vector>

#if defined(_WIN32) && !defined(AIRUI_SKIA_MOCK)
// Skia's Windows ports let us turn the exact GDI LOGFONT the UI already
// uses (Microsoft YaHei UI / Segoe MDL2 Assets) into a Skia typeface.
// This sidesteps the Skia default-font-manager factory, which differs
// between Skia versions and is not exposed in a stable way. Under the
// syntax gate (AIRUI_SKIA_MOCK) SkiaApiMock.h provides the equivalent
// declarations without the real <windows.h> / SkTypeface_win.h.
#  include <windows.h>
#  include "include/ports/SkTypeface_win.h"
#endif

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

// UTF-8 -> Unicode codepoints (text rendering needs per-codepoint font
// fallback, so we work in codepoints, not bytes).
void DecodeUtf8(const std::string& s, std::vector<uint32_t>& out)
{
    out.clear();
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char b = static_cast<unsigned char>(s[i]);
        uint32_t cp;
        size_t   n;
        if      (b < 0x80u) { cp = b;             n = 1; }
        else if (b < 0xE0u) { cp = b & 0x1Fu;     n = 2; }
        else if (b < 0xF0u) { cp = b & 0x0Fu;     n = 3; }
        else                { cp = b & 0x07u;     n = 4; }
        if (i + n > s.size()) { out.push_back(0xFFFDu); i = s.size(); break; }
        for (size_t k = 1; k < n; ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3Fu);
        out.push_back(cp);
        i += n;
    }
}

// Unicode codepoints -> UTF-8 (used to feed each font run back to Skia).
std::string EncodeUtf8(const uint32_t* cps, size_t n)
{
    std::string out;
    out.reserve(n * 3 + 1);
    for (size_t i = 0; i < n; ++i) {
        const uint32_t cp = cps[i];
        if      (cp <= 0x7Fu) out.push_back(static_cast<char>(cp));
        else if (cp <= 0x7FFu) {
            out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else if (cp <= 0xFFFFu) {
            out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else {
            out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
    }
    return out;
}

// Copy a face name into a LOGFONT's fixed-size buffer without relying on
// the secure CRT (so it also works under the syntax gate, where the real
// <windows.h> may be absent). LOGFONT is TCHAR-based, which is char[] in
// ANSI builds and wchar_t[] in Unicode builds -- TCHAR/TEXT keep this
// code correct under either setting. UiText.h families are ASCII, so the
// narrowing cast is lossless even when TCHAR is char.
static void CopyFaceName(TCHAR (&dst)[32], const wchar_t* src)
{
    for (int k = 0; k < 32; ++k) {
        dst[k] = static_cast<TCHAR>(src[k]);
        if (src[k] == 0) break;
    }
}

// Skia sizes text by em, GDI's LOGFONT.lfHeight is the *cell* height
// (ascent + descent). For Segoe UI the cell is ~1.33x the em, so feeding
// a cell height straight into SkFont::setSize made every string a third
// too large -- headings collided with the title bar and nav rows were
// ellipsised. Metrics scale linearly with size, so one probe at 1000
// gives an exact conversion for any cell height.
static SkScalar EmForCell(const sk_sp<SkTypeface>& face, int cell)
{
    const SkScalar fallback = static_cast<SkScalar>(cell > 0 ? cell : 13);
#if defined(_WIN32)
    if (face && cell > 0) {
        SkFont probe;
        probe.setTypeface(face);
        probe.setSize(1000.f);
        SkFontMetrics fm{};
        probe.getMetrics(&fm);
        const float box = fm.fDescent - fm.fAscent;
        if (box > 1.f) return static_cast<SkScalar>(cell) * 1000.f / box;
    }
#endif
    return fallback;
}

// Process-wide typeface set.
//
// Building a typeface goes through GDI's font matcher
// (SkCreateTypefaceFromLOGFONT) and costs tens of milliseconds. A
// SkiaPainter is constructed once per *frame*, so caching the faces on
// the painter instance meant every repaint re-resolved three faces --
// measured at 78 ms for a 1120x780 window. Sharing one immutable set
// removes that cost from every frame after the first.
struct SharedFaces
{
    sk_sp<SkTypeface> ui;        // Segoe UI           (shapes Latin/UI text)
    sk_sp<SkTypeface> uiBold;    // Segoe UI Semibold  (reserved for weights)
    sk_sp<SkTypeface> cjk;       // Microsoft YaHei UI (what GDI font-links to)
    sk_sp<SkTypeface> icon;      // Segoe MDL2 Assets
    sk_sp<SkTypeface> fallback;  // system default, last resort
    // Em size per role, derived once from the role's cell height.
    SkScalar em[static_cast<int>(FontRole::Count)] = {};
};

void FillSharedFaces(SharedFaces& f)
{
#if defined(_WIN32)
    auto make = [](const wchar_t* family, int weight, int cell) {
        LOGFONT lf{};
        lf.lfHeight         = -cell;
        lf.lfWeight         = weight;
        lf.lfCharSet        = DEFAULT_CHARSET;
        lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
        lf.lfQuality        = CLEARTYPE_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        CopyFaceName(lf.lfFaceName, family);
        return SkCreateTypefaceFromLOGFONT(lf);
    };

    f.ui     = make(kFamilyUi,   400, 13);
    f.uiBold = make(kFamilyUi,   600, 13);
    f.cjk    = make(kFamilyCjk,  400, 13);
    f.icon   = make(kFamilyIcon, 400, 13);

    if (!f.ui) {
        // Last resort: let GDI pick the system default font.
        LOGFONT lfDef{};
        lfDef.lfHeight         = -13;
        lfDef.lfCharSet        = DEFAULT_CHARSET;
        lfDef.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        f.ui = SkCreateTypefaceFromLOGFONT(lfDef);
    }
    if (!f.uiBold) f.uiBold = f.ui;
    if (!f.cjk)    f.cjk    = f.ui;

    // Generic fallback for codepoints no family above covers (rare
    // symbols / emoji): an empty-faced LOGFONT lets GDI resolve the
    // best available system font.
    LOGFONT lfFall{};
    lfFall.lfHeight         = -13;
    lfFall.lfCharSet        = DEFAULT_CHARSET;
    lfFall.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    f.fallback = SkCreateTypefaceFromLOGFONT(lfFall);
#endif
    // Em size per role: Latin (primary face) and icon roles are measured
    // against the face that actually draws them.
    for (int i = 0; i < static_cast<int>(FontRole::Count); ++i) {
        const FontRole role = static_cast<FontRole>(i);
        const FontSpec spec = RoleFontSpec(role);
        const bool isIcon = (role == FontRole::Icon || role == FontRole::IconLarge);
        f.em[i] = EmForCell(isIcon ? f.icon : f.ui, spec.cell);
    }
}

const SharedFaces& GetSharedFaces()
{
    static const SharedFaces* faces = new SharedFaces([] {
        SharedFaces f;
        FillSharedFaces(f);
        return f;
    }());
    // Deliberately leaked: these typefaces live for the whole process.
    // A static destructor would run while the plugin DLL unloads, when
    // Skia's own code may already be gone.
    return *faces;
}

// CJK / fullwidth ranges. GDI resolves these through font linking to
// Microsoft YaHei UI; doing the same here is what keeps Chinese text on
// the same family (and therefore the same shapes) across backends.
bool IsCjk(uint32_t cp)
{
    return (cp >= 0x2E80u && cp <= 0x9FFFu) ||   // radicals .. CJK unified
           (cp >= 0x3000u && cp <= 0x303Fu) ||   // CJK punctuation
           (cp >= 0x3040u && cp <= 0x30FFu) ||   // kana
           (cp >= 0xAC00u && cp <= 0xD7AFu) ||   // hangul
           (cp >= 0xF900u && cp <= 0xFAFFu) ||   // compatibility ideographs
           (cp >= 0xFF00u && cp <= 0xFFEFu);     // fullwidth forms
}

} // namespace

struct SkiaPainter::Impl
{
    explicit Impl(SkCanvas* canvas) : canvas(canvas) {}

    SkCanvas* canvas = nullptr;
    SkFont    fonts[static_cast<int>(FontRole::Count)];
    bool      ready[static_cast<int>(FontRole::Count)] = {};
    int       clipDepth = 0;

    // Font faces per role: Segoe UI for UI text, Segoe MDL2 Assets for
    // icons -- exactly what the GDI backend selects -- plus a CJK face
    // standing in for GDI's font linking. Lazy: resolved on first use.
    sk_sp<SkTypeface>      roleFace[static_cast<int>(FontRole::Count)];
    sk_sp<SkTypeface>      cjkFace = nullptr;
    sk_sp<SkTypeface>      defaultFace = nullptr;
    SkScalar               roleSize[static_cast<int>(FontRole::Count)] = {};
    bool                   facesReady = false;

    void EnsureFaces()
    {
        if (facesReady) return;
        facesReady = true;

        const SharedFaces& shared = GetSharedFaces();
        for (int i = 0; i < static_cast<int>(FontRole::Count); ++i) {
            const FontRole role = static_cast<FontRole>(i);
            const bool isIcon = (role == FontRole::Icon || role == FontRole::IconLarge);
            roleFace[i] = isIcon ? (shared.icon ? shared.icon : shared.ui) : shared.ui;
            roleSize[i] = shared.em[i];
        }
        cjkFace = shared.cjk;
        defaultFace = shared.fallback;
    }

    // Pick the typeface that can actually draw `cp` for this role. Order
    // mirrors GDI: role family -> font-linked CJK family -> system
    // default. Falling back to the generic system font first would give
    // Chinese a different family from GDI's and change its shapes.
    sk_sp<SkTypeface> FaceFor(FontRole role, SkUnichar cp)
    {
        EnsureFaces();
        const int i = static_cast<int>(role) < 0 ||
                      static_cast<int>(role) >= static_cast<int>(FontRole::Count)
                          ? 0 : static_cast<int>(role);
        sk_sp<SkTypeface>& base = roleFace[i];
        if (base && base->unicharToGlyph(cp)) return base;
        if (IsCjk(static_cast<uint32_t>(cp)) &&
            cjkFace && cjkFace->unicharToGlyph(cp))
            return cjkFace;
        if (defaultFace && defaultFace->unicharToGlyph(cp)) return defaultFace;
        return base;
    }

    SkFont& Font(FontRole role)
    {
        int i = static_cast<int>(role);
        if (i < 0 || i >= static_cast<int>(FontRole::Count)) i = 0;
        if (!ready[i]) {
            EnsureFaces();
            SkFont f;
            // roleSize is already an em size converted from the GDI cell
            // height, so both backends occupy the same box.
            f.setSize(roleSize[i] > 0 ? roleSize[i] : 13.f);
            if (roleFace[i]) f.setTypeface(roleFace[i]);
            // GDI CLEARTYPE_QUALITY snaps stems to the pixel grid with
            // integer advances. Full hinting + no subpixel positioning is
            // the closest Skia equivalent, and is what keeps 13 px UI
            // text from looking soft and blurry next to GDI's.
            f.setHinting(SkFontHinting::kFull);
            f.setSubpixel(false);
            f.setEdging(SkFont::Edging::kAntiAlias);
            fonts[i] = f;
            ready[i] = true;
        }
        return fonts[i];
    }

    // Draw UTF-8 text already positioned at baseline origin (x, y), with
    // per-codepoint font fallback so mixed Latin/CJK/icon runs all paint.
    void DrawRuns(SkFont& primary, FontRole role, const std::string& utf8,
                  float x, float y, Color c)
    {
        if (utf8.empty()) return;
        std::vector<uint32_t> cps;
        DecodeUtf8(utf8, cps);
        if (cps.empty()) return;

        const SkPaint fill = MakeFill(c);
        size_t i = 0;
        while (i < cps.size()) {
            sk_sp<SkTypeface> face = FaceFor(role, cps[i]);
            size_t j = i + 1;
            while (j < cps.size() &&
                   FaceFor(role, cps[j]).get() == face.get())
                ++j;

            const std::string run = EncodeUtf8(&cps[i], j - i);
            SkFont f = primary;
            if (face) f.setTypeface(face);
            const float w =
                f.measureText(run.data(), run.size(), SkTextEncoding::kUTF8);
            canvas->drawSimpleText(run.data(), run.size(),
                                   SkTextEncoding::kUTF8, x, y, f, fill);
            x += w;
            i = j;
        }
    }

    // Width of a codepoint sequence using the same per-codepoint font
    // fallback as DrawRuns (used for ellipsis trimming and alignment).
    float MeasureRuns(FontRole role, const std::vector<uint32_t>& cps,
                      SkFont& primary)
    {
        float w = 0.f;
        size_t i = 0;
        while (i < cps.size()) {
            sk_sp<SkTypeface> face = FaceFor(role, cps[i]);
            size_t j = i + 1;
            while (j < cps.size() &&
                   FaceFor(role, cps[j]).get() == face.get())
                ++j;
            const std::string run = EncodeUtf8(&cps[i], j - i);
            SkFont f = primary;
            if (face) f.setTypeface(face);
            w += f.measureText(run.data(), run.size(), SkTextEncoding::kUTF8);
            i = j;
        }
        return w;
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
    SkFont& primary = m->Font(role);
    std::string utf8 = skia::WideToUtf8(s);

    std::vector<uint32_t> cps;
    DecodeUtf8(utf8, cps);

    if ((fmt & dt::EndEllipsis) && !cps.empty()) {
        const float avail = static_cast<float>(r.Width());
        const float dotsW = primary.measureText("...", 3, SkTextEncoding::kUTF8);
        while (cps.size() > 1 &&
               m->MeasureRuns(role, cps, primary) + dotsW > avail)
            cps.pop_back();
        cps.push_back(static_cast<uint32_t>('.'));
        cps.push_back(static_cast<uint32_t>('.'));
        cps.push_back(static_cast<uint32_t>('.'));
        utf8 = EncodeUtf8(cps.data(), cps.size());
        DecodeUtf8(utf8, cps);
    }

    const float totalW = m->MeasureRuns(role, cps, primary);
    float x = static_cast<float>(r.left);
    switch (skia::Horizontal(fmt)) {
    case skia::HAlign::Center: x += (static_cast<float>(r.Width()) - totalW) / 2.f; break;
    case skia::HAlign::Right:  x += static_cast<float>(r.Width()) - totalW;         break;
    default: break;
    }
    const float y = m->Baseline(r, primary, skia::Vertical(fmt));
    m->DrawRuns(primary, role, utf8, x, y, c);
}

void SkiaPainter::TextBlock(const Rect& r, const std::wstring& s,
                            FontRole role, Color c, int maxLines)
{
    if (!m->canvas || s.empty() || r.Empty()) return;
    const std::string utf8 = skia::WideToUtf8(s);
    SkFont& primary = m->Font(role);
    SkFontMetrics fm{};
    primary.getMetrics(&fm);
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
            primary.measureText(next.data(), next.size(), SkTextEncoding::kUTF8) > avail) {
            lines.push_back(cur);
            cur = utf8.substr(i, n);
        } else {
            cur = next;
        }
        i += n;
    }
    if (!cur.empty() && static_cast<int>(lines.size()) < limit)
        lines.push_back(cur);

    float y = static_cast<float>(r.top) - fm.fAscent;
    for (const std::string& ln : lines) {
        if (y - fm.fDescent > static_cast<float>(r.bottom)) break;
        if (!ln.empty())
            m->DrawRuns(primary, role, ln, static_cast<float>(r.left), y, c);
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
