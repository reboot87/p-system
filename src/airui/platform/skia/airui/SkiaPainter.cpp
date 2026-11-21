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
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32) && !defined(AIRUI_SKIA_MOCK)
// Skia's Windows ports. The GDI one (SkFontMgr_New_GDI) hands out faces
// whose outlines GDI itself grid-fits -- that is the one that matches
// the Win32 backend; the LOGFONT/DirectWrite factory next to it is the
// fallback. Under the syntax gate (AIRUI_SKIA_MOCK) SkiaApiMock.h
// provides the equivalent declarations without the real <windows.h>.
#  include <windows.h>
#  include "include/ports/SkTypeface_win.h"

// Pinned declaration: the factory is still exported by the Skia build
// (verified with `dumpbin /exports skia.dll`, symbol
// ?SkFontMgr_New_GDI@@YA?AV?$sk_sp@VSkFontMgr@@@@XZ) but the header
// that declares it has moved between Skia releases. Identical signature
// is a harmless redeclaration where the header does declare it.
sk_sp<SkFontMgr> SkFontMgr_New_GDI();
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

// GDI's DrawText works in whole device pixels: it computes the leftover
// gap as an int, so (width - textWidth) / 2 truncates instead of
// rounding. A fractional origin is rounded to nearest by Skia, which put
// every centred glyph half a pixel off -- all caption icons and nav
// icons landed one pixel to the right of where GDI draws them.
float AlignX(const Rect& r, float totalW, skia::HAlign a)
{
    const int tw = static_cast<int>(totalW + 0.5f);
    switch (a) {
    case skia::HAlign::Center:
        return static_cast<float>(r.left + (r.Width() - tw) / 2);
    case skia::HAlign::Right:
        return static_cast<float>(r.right - tw);
    case skia::HAlign::Left:
    default:
        return static_cast<float>(r.left);
    }
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

// ---- Typography calibration ----------------------------------------
//
// GDI and Skia do not share a font-size unit:
//   * LOGFONT.lfHeight is a *cell* height derived from the font's win
//     metrics, so a 13 px cell is roughly a 9.8 px em for the UI family
//     (Microsoft YaHei UI);
//   * SkFont::setSize takes an *em* size, and SkFontMetrics reports the
//     typographic (OS/2 typo) metrics, which for that family add up to
//     0.9375 em.
// Converting one to the other from Skia's side alone therefore
// overshoots: measured on screen, "AireRecord" came out 101 px wide in
// Skia where GDI drew 75 px, headings collided with the title bar and
// every nav row was ellipsised.
//
// So the size is calibrated against GDI itself: render a reference
// string with the exact LOGFONT the Win32 backend uses and solve for the
// em that reproduces that advance width. That is font-, DPI- and
// metric-source independent -- we measure what GDI does instead of
// assuming it. The same pass records GDI's baseline offset (tmAscent) so
// vertical alignment lands on the same pixels too.
struct RoleMetrics
{
    SkScalar em      = 13.f;   // SkFont::setSize, Latin/UI face
    SkScalar emCjk   = 13.f;   // same role, Microsoft YaHei UI (GDI's font link)
    SkScalar emSym   = 13.f;   // same role, Segoe UI Symbol (GDI's symbol font)
    SkScalar emLink  = 13.f;   // same role, Yu Gothic UI (GDI's link-chain tail)
    float    ascent  = 10.f;   // GDI tmAscent: baseline offset from the cell top
    float    descent = 3.f;    // GDI tmDescent
    int      cell    = 13;     // GDI cell height, which is also the line advance
};

// Width reference: a run of hanzi. GDI grid-fits every glyph to whole
// pixels, so the advance ratio of a *mixed* run depends on which glyph
// class dominates -- anchoring on "AireRecord 0123456789" made the
// semibold roles come out ~19% too wide and the regular ones a couple of
// percent narrow. A hanzi run has one advance for every glyph, no side
// bearings at the ends to speak of, and an exact 1:1 relation to the em,
// so the ratio it produces is the em itself. Latin then follows for free
// because it is the same face.
//
// The strings are code points so this file stays ASCII-only whatever the
// compiler's source charset is: "workbench overview".
constexpr wchar_t kRefCjk[] = { 0x5DE5, 0x4F5C, 0x53F0, 0x603B, 0x89C8, 0 };
constexpr int     kRefCjkCount = 5;
// Latin fallbacks, used only if the CJK run cannot be measured.
constexpr wchar_t kRefText[]   = L"AireRecord";
constexpr wchar_t kRefDigits[] = L"0123456789";
// One Segoe MDL2 glyph, for icon-role calibration.
constexpr wchar_t kRefIcon   = 0xE700;
// U+2713 CHECK MARK: the glyph GDI font-links to Segoe UI Symbol and
// that the Skia plugin must be able to draw (the checkbox tick).
constexpr wchar_t kRefSymbol = 0x2713;
// U+25BE BLACK DOWN-POINTING SMALL TRIANGLE: the glyph GDI links to
// Yu Gothic UI (the tail of the UI family's font-link chain).
constexpr wchar_t kRefLinked = 0x25BE;

struct SharedFaces
{
    sk_sp<SkTypeface> ui;        // Microsoft YaHei UI (UI text, 400)
    sk_sp<SkTypeface> uiBold;    // Microsoft YaHei UI (headings, buttons, 600)
    sk_sp<SkTypeface> cjk;       // Microsoft YaHei UI (what GDI font-links to)
    sk_sp<SkTypeface> cjkBold;   // Microsoft YaHei UI Semibold
    sk_sp<SkTypeface> icon;      // Segoe MDL2 Assets
    sk_sp<SkTypeface> symbol;    // Segoe UI Symbol    (GDI's symbol font)
    sk_sp<SkTypeface> symbolBold;// Segoe UI Symbol Bold
    sk_sp<SkTypeface> linked;    // Yu Gothic UI       (GDI's link-chain tail)
    sk_sp<SkTypeface> linkedBold;// Yu Gothic UI Bold
    sk_sp<SkTypeface> fallback;  // system default, last resort
};

#if defined(_WIN32)
// Which font manager hands out the faces decides how the glyph OUTLINES
// are produced, and that turned out to matter more than any of the
// sizing work below.
//
// SkCreateTypefaceFromLOGFONT() goes through DirectWrite, which has no
// grid-fitting: it rasterises the outline as-is ("natural" rendering).
// GDI grids the same outline against the pixel raster -- at cell 14 it
// snaps a hanzi vertical stroke to exactly one pixel, while DWrite
// keeps its geometric ~1.6 px. Measured with tests/test_font_lab: same
// advance grid, same bounding box, but 22-32 % more ink in Skia, and
// every stem one pixel wider -- the "Skia text looks bolder/fuzzier"
// report is that, not the size and not the anti-aliasing mode.
//
// Skia's GDI port (SkFontMgr_New_GDI, include/ports/SkTypeface_win.h)
// rasterises through GDI itself, so the grid-fitting is the same one
// the Win32 backend gets. The LOGFONT path stays as the fallback for a
// build without that port or a family GDI cannot match.
sk_sp<SkFontMgr> GdiFontMgr()
{
    static const sk_sp<SkFontMgr> mgr = SkFontMgr_New_GDI();
    return mgr;
}

// Families in UiText.h are ASCII, so the UTF-8 name SkFontMgr expects
// is a plain narrowing.
void NarrowAscii(const wchar_t* src, char (&dst)[64])
{
    int i = 0;
    for (; i < 63 && src[i]; ++i) dst[i] = static_cast<char>(src[i]);
    dst[i] = 0;
}

sk_sp<SkTypeface> MakeFace(const wchar_t* family, int weight, int cell)
{
    if (sk_sp<SkFontMgr> mgr = GdiFontMgr()) {
        char name[64] = {};
        NarrowAscii(family, name);
        if (sk_sp<SkTypeface> face = mgr->matchFamilyStyle(
                name, SkFontStyle(weight, SkFontStyle::kNormal_Width,
                                  SkFontStyle::kUpright_Slant)))
            return face;
    }

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
}
#endif

const SharedFaces& GetSharedFaces()
{
    static const SharedFaces* faces = new SharedFaces([] {
        SharedFaces f;
#if defined(_WIN32)
        f.ui     = MakeFace(kFamilyUi,   400, 13);
        f.uiBold = MakeFace(kFamilyUi,   600, 13);
        f.cjk    = MakeFace(kFamilyCjk,  400, 13);
        f.cjkBold = MakeFace(kFamilyCjk, 600, 13);
        f.icon   = MakeFace(kFamilyIcon, 400, 13);
        f.symbol = MakeFace(kFamilySymbol, 400, 13);
        f.symbolBold = MakeFace(kFamilySymbol, 600, 13);
        f.linked = MakeFace(kFamilyLinked, 400, 13);
        f.linkedBold = MakeFace(kFamilyLinked, 600, 13);

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
        if (!f.cjkBold) f.cjkBold = f.cjk;
        // A machine without these families keeps the chain intact: the
        // next stop, and finally the system default, try instead.
        if (!f.symbol)     f.symbol     = f.ui;
        if (!f.symbolBold) f.symbolBold = f.symbol;
        if (!f.linked)     f.linked     = f.ui;
        if (!f.linkedBold) f.linkedBold = f.linked;

        // Generic fallback for codepoints no family above covers (rare
        // symbols / emoji): an empty-faced LOGFONT lets GDI resolve the
        // best available system font.
        LOGFONT lfFall{};
        lfFall.lfHeight         = -13;
        lfFall.lfCharSet        = DEFAULT_CHARSET;
        lfFall.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        f.fallback = SkCreateTypefaceFromLOGFONT(lfFall);
#endif
        return f;
    }());
    // Deliberately leaked: these typefaces live for the whole process.
    // A static destructor would run while the plugin DLL unloads, when
    // Skia's own code may already be gone.
    return *faces;
}

// Face a role draws with. Headings and buttons are semibold in the
// design system, so they need the 600-weight face -- both in GDI and in
// Skia -- or the window title reads lighter than the rest of the UI.
const sk_sp<SkTypeface>& UiFaceFor(const SharedFaces& f, FontRole role)
{
    const bool isIcon = (role == FontRole::Icon || role == FontRole::IconLarge);
    if (isIcon) return f.icon;
    return RoleFontSpec(role).weight >= 600 && f.uiBold ? f.uiBold : f.ui;
}

const sk_sp<SkTypeface>& CjkFaceFor(const SharedFaces& f, FontRole role)
{
    return RoleFontSpec(role).weight >= 600 && f.cjkBold ? f.cjkBold : f.cjk;
}

// The face GDI reaches for symbols the UI family does not carry
// (U+2713 check mark, U+2715 ballot X). GDI's own fallback chain
// resolves the Dingbats block to Segoe UI Symbol -- its linked U+2713
// has exactly that family's advance and ink box -- so the plugin names
// the same family to draw the same glyph.
const sk_sp<SkTypeface>& SymbolFaceFor(const SharedFaces& f, FontRole role)
{
    return RoleFontSpec(role).weight >= 600 && f.symbolBold ? f.symbolBold : f.symbol;
}

// The face GDI reaches for the remaining symbols the UI family lacks
// (U+25BE, U+25B8, U+26A0, U+2139): the tail of the UI family's
// font-link chain. Measured against GDI: same advance, same ink box.
const sk_sp<SkTypeface>& LinkedFaceFor(const SharedFaces& f, FontRole role)
{
    return RoleFontSpec(role).weight >= 600 && f.linkedBold ? f.linkedBold : f.linked;
}

// Resolve one role against GDI: same font the Win32 backend selects, same
// cell height, then copy GDI's own numbers for ascent/descent and solve
// the em that reproduces GDI's advance widths.
//
// Measured at the target cell, NOT extrapolated from a large size:
// CreateFontW(-13) realises a 17px cell (GDI implements the requested
// character height by picking the nearest bigger cell), and that step is
// not linear across sizes, so scaling a 200px measurement down by
// cell/200 lands on a size GDI never draws. Everything here is therefore
// read straight off the DC at the size we actually ask for.
#if defined(_WIN32) && !defined(AIRUI_SKIA_MOCK)
RoleMetrics MeasureRole(HDC dc, const SharedFaces& f, FontRole role)
{
    RoleMetrics rm;
    const FontSpec spec = RoleFontSpec(role);
    const bool isIcon = (role == FontRole::Icon || role == FontRole::IconLarge);
    rm.cell = spec.cell > 0 ? spec.cell : 13;
    rm.em   = static_cast<SkScalar>(rm.cell);
    rm.emCjk = rm.em;
    rm.emSym = rm.em;
    rm.emLink = rm.em;
    rm.ascent  = static_cast<float>(rm.cell) * 0.8f;
    rm.descent = static_cast<float>(rm.cell) - rm.ascent;

    const sk_sp<SkTypeface>& face = UiFaceFor(f, role);
    if (!face || !dc) return rm;

    const wchar_t* family = isIcon ? kFamilyIcon : kFamilyUi;
    HFONT hf = CreateFontW(-rm.cell, 0, 0, 0, spec.weight, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH, family);
    if (!hf) return rm;
    HGDIOBJ old = SelectObject(dc, hf);

    // (1) Cell + baseline: GDI's own metrics at this size. tmHeight is the
    // cell GDI actually realises (a -13 request yields 17), and that
    // realised cell -- not the request -- is what DrawText uses for
    // DT_VCENTER and for line advance.
    TEXTMETRICW tm{};
    if (GetTextMetricsW(dc, &tm)) {
        if (tm.tmHeight > 0)  rm.cell    = tm.tmHeight;
        if (tm.tmAscent > 0)  rm.ascent  = static_cast<float>(tm.tmAscent);
        if (tm.tmDescent > 0) rm.descent = static_cast<float>(tm.tmDescent);
    }

    // (2) em: match GDI's advance width for a reference run.
    //
    // A pure ratio taken at em 1000 is only a seed: the painter draws with
    // full hinting, which snaps every advance to a whole pixel, so at 13 px
    // the hinted width can sit ~8% away from the linear one (a hanzi whose
    // linear advance is 13.78 hints to 15 in the semibold face). The seed is
    // therefore refined against widths measured at the *drawn* size, with
    // the same font settings the painter uses, until it stops moving.
    // The GDI side always measures with the font selected on the DC --
    // the role's own family, exactly like the Win32 backend -- so GDI's
    // own font linking is part of the measurement. `measureFace` is the
    // Skia face the run will actually be drawn with; it may be a linked
    // family (CJK, symbol) rather than the role's own.
    auto solve = [&](const wchar_t* text, int chars, int bytes,
                     const sk_sp<SkTypeface>& measureFace, SkScalar& out) {
        out = 0.f;
        SIZE sz{};
        if (!measureFace || !GetTextExtentPoint32W(dc, text, chars, &sz) || sz.cx <= 0)
            return;
        SkFont probe;
        probe.setTypeface(measureFace);
        probe.setSize(1000.f);
        const float w1000 =
            probe.measureText(text, (size_t)bytes, SkTextEncoding::kUTF16);
        if (w1000 <= 1.f) return;

        const SkScalar seed = 1000.f * static_cast<float>(sz.cx) / w1000;
        SkScalar em = seed;
        // Hinted widths are step functions, so guard against a refinement
        // step running away on a rounding cliff.
        const SkScalar lo = seed * 0.75f, hi = seed * 1.25f;
        for (int i = 0; i < 4; ++i) {
            SkFont f;
            f.setTypeface(measureFace);
            f.setSize(em);
            f.setHinting(SkFontHinting::kFull);
            f.setSubpixel(false);
            f.setEdging(SkFont::Edging::kSubpixelAntiAlias);
            const float w =
                f.measureText(text, (size_t)bytes, SkTextEncoding::kUTF16);
            if (w <= 1.f) break;
            const SkScalar next = em * static_cast<float>(sz.cx) / w;
            if (next <= 1.f || next < lo || next > hi) break;
            const SkScalar diff = next > em ? next - em : em - next;
            em = next;
            if (diff <= 0.02f) break;
        }
        out = em;
    };

    if (isIcon) {
        solve(&kRefIcon, 1, (int)sizeof(wchar_t), face, rm.em);
    } else {
        // Solve EACH reference run on its own, then pick the size that
        // minimises the summed relative error over ALL of them.
        //
        // The CJK run is only a 1:1 em anchor in the unhinted limit: with
        // full hinting every advance is a step function of the size, and
        // the anchors can disagree (measured: Display role -- the hanzi
        // run solves 23, while at 23 the Latin run measures 32px against
        // GDI's 31). A single run is the wrong estimator; the joint
        // minimum stops pretending any one script is ground truth. It is
        // never worse than the best single anchor (the anchors are all
        // candidates), and lands on the CJK answer when the other errors
        // cannot outvote it -- which is what happens on Display (23), so
        // the residual 1px there is per-glyph hinting divergence between
        // the two rasterisers, not anchor selection. aa-vs-sk being as
        // large as ct-vs-sk on the 600-weight samples is the same signal.
        constexpr int kLatChars = (int)(sizeof(kRefText) / sizeof(wchar_t)) - 1;
        constexpr int kLatBytes = (int)(sizeof(kRefText) - sizeof(wchar_t));
        constexpr int kDigChars = (int)(sizeof(kRefDigits) / sizeof(wchar_t)) - 1;
        constexpr int kDigBytes = (int)(sizeof(kRefDigits) - sizeof(wchar_t));
        constexpr int kCjkBytes = kRefCjkCount * (int)sizeof(wchar_t);

        SkScalar emCjk = 0.f, emLat = 0.f, emDig = 0.f;
        solve(kRefCjk,    kRefCjkCount, kCjkBytes, face, emCjk);
        solve(kRefText,   kLatChars,    kLatBytes, face, emLat);
        solve(kRefDigits, kDigChars,    kDigBytes, face, emDig);

        SIZE gdiCjk{}, gdiLat{}, gdiDig{};
        GetTextExtentPoint32W(dc, kRefCjk,    kRefCjkCount, &gdiCjk);
        GetTextExtentPoint32W(dc, kRefText,   kLatChars,    &gdiLat);
        GetTextExtentPoint32W(dc, kRefDigits, kDigChars,    &gdiDig);

        // Relative error of one run at a candidate em, or -1 when either
        // side refuses to measure (the run then contributes nothing).
        auto runErr = [&](SkScalar em, const wchar_t* text, int chars,
                          int bytes, int gdiW) -> SkScalar {
            if (gdiW <= 0 || em < 1.f || chars <= 0) return -1.f;
            SkFont probe;
            probe.setTypeface(face);
            probe.setSize(em);
            probe.setHinting(SkFontHinting::kFull);
            probe.setSubpixel(false);
            probe.setEdging(SkFont::Edging::kSubpixelAntiAlias);
            const float w =
                probe.measureText(text, (size_t)bytes, SkTextEncoding::kUTF16);
            if (w <= 1.f) return -1.f;
            return std::fabs(w - (float)gdiW) / (float)gdiW;
        };
        auto jointErr = [&](SkScalar em) -> SkScalar {
            SkScalar sum = 0.f, n = 0.f, e = -1.f;
            if ((e = runErr(em, kRefCjk, kRefCjkCount, kCjkBytes,
                            gdiCjk.cx)) >= 0) { sum += e; ++n; }
            if ((e = runErr(em, kRefText, kLatChars, kLatBytes,
                            gdiLat.cx)) >= 0) { sum += e; ++n; }
            if ((e = runErr(em, kRefDigits, kDigChars, kDigBytes,
                            gdiDig.cx)) >= 0) { sum += e; ++n; }
            return n > 0.f ? sum / n : -1.f;
        };

        // Candidates: every anchor, plus a 0.25px grid spanning them (the
        // error is a step function of the hinted advances, so the joint
        // minimum may fall strictly between anchors).
        SkScalar lo = 1e9f, hi = -1e9f;
        for (SkScalar v : { emCjk, emLat, emDig }) {
            if (v < 1.f) continue;
            lo = (std::min)(lo, v);
            hi = (std::max)(hi, v);
        }
        SkScalar best = 0.f;
        if (hi >= 1.f) {
            lo = (std::max)(1.f, lo - 1.f);
            hi += 1.f;
            SkScalar bestErr = -1.f;
            // Every anchor is an explicit candidate: the 0.25 grid is
            // aligned to (min - 1), so a lone anchor (emLat) need not fall
            // on it (emCjk=23.4, emLat=23.0 -> 0.6 off-grid) and would be
            // skipped if only the grid were walked.
            auto consider = [&](SkScalar cand) {
                if (cand < 1.f) return;
                const SkScalar err = jointErr(cand);
                if (bestErr < 0.f || (err >= 0.f && err < bestErr)) {
                    bestErr = err;
                    best = cand;
                }
            };
            consider(emCjk);
            consider(emLat);
            consider(emDig);
            for (SkScalar cand = lo; cand <= hi + 1e-4f; cand += 0.25f)
                consider(cand);
            if (bestErr < 0.f) best = 0.f;   // no run measured at all
        }
        rm.em = best;

        // Latin and CJK share the one face (the UI family *is* Microsoft
        // YaHei UI: GDI never font-links here) and now share the one
        // jointly fitted size -- a hanzi advances exactly 1 em in that
        // face, so no per-script em is needed.
        rm.emCjk = rm.em;

        // Symbols need their own size: the UI family has no U+2713 and
        // no U+25BE, and GDI draws those from other families, at the
        // same cell size but with their own metrics. Solving each face's
        // em against the advance GDI reports for the linked run makes
        // the glyph land on the same pixels in both backends (measured:
        // identical advance and identical ink box, so the glyph is the
        // same size, not just present).
        const sk_sp<SkTypeface>& sym = SymbolFaceFor(f, role);
        if (sym)
            solve(&kRefSymbol, 1, (int)sizeof(wchar_t), sym, rm.emSym);
        const sk_sp<SkTypeface>& link = LinkedFaceFor(f, role);
        if (link)
            solve(&kRefLinked, 1, (int)sizeof(wchar_t), link, rm.emLink);
    }

    SelectObject(dc, old);
    DeleteObject(hf);
    return rm;
}
#endif

// Calibration is per DPI (a monitor change re-measures) and per process,
// so the per-frame painter never pays for it after the first frame.
struct Calibration
{
    int         dpi = -1;
    RoleMetrics m[static_cast<int>(FontRole::Count)];
};

const Calibration& GetCalibration(void* nativeDc)
{
    static Calibration* cal = new Calibration();   // process lifetime

#if defined(_WIN32) && !defined(AIRUI_SKIA_MOCK)
    HDC dc = nativeDc ? static_cast<HDC>(nativeDc) : GetDC(nullptr);
    if (dc) {
        const int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        if (cal->dpi != dpi) {
            const SharedFaces& faces = GetSharedFaces();
            for (int i = 0; i < static_cast<int>(FontRole::Count); ++i)
                cal->m[i] = MeasureRole(dc, faces, static_cast<FontRole>(i));
            cal->dpi = dpi;
        }
        if (!nativeDc) ReleaseDC(nullptr, dc);
    }
#else
    (void)nativeDc;
#endif
    return *cal;
}

// Opt-in trace of the calibration result (AIRUI_PAINT_TRACE=1).
void DiagCalibration(const RoleMetrics* m)
{
#if defined(_WIN32) && !defined(AIRUI_SKIA_MOCK)
    static bool done = false;       // painter is per frame; dump once
    if (done) return;
    wchar_t v[8] = {};
    if (GetEnvironmentVariableW(L"AIRUI_PAINT_TRACE", v, 8) == 0) return;
    done = true;
    wchar_t path[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH) == 0) return;
    wcscat_s(path, L"\\AireRecord\\skia_font.log");
    FILE* fp = nullptr;
    _wfopen_s(&fp, path, L"w");     // truncate once, then trace appends
    if (!fp) return;
    fwprintf(fp, L"[calib] gdi-matched calibration\n");
    for (int i = 0; i < static_cast<int>(FontRole::Count); ++i) {
        fwprintf(fp, L"[calib] role=%d cell=%d em=%.2f cjk=%.2f sym=%.2f link=%.2f ascent=%.1f descent=%.1f\n",
                 i, m[i].cell, (double)m[i].em, (double)m[i].emCjk,
                 (double)m[i].emSym, (double)m[i].emLink,
                 (double)m[i].ascent, (double)m[i].descent);
    }
    fclose(fp);
#else
    (void)m;
#endif
}

// Per-call text trace (AIRUI_PAINT_TRACE=1), appended after the
// calibration dump: what the backend actually draws, so a screenshot can
// be matched against the calls that produced it.
void DiagText(int role, SkScalar size, float w, float x, float y,
              float avail, float canvasScale, const std::string& s)
{
#if defined(_WIN32) && !defined(AIRUI_SKIA_MOCK)
    static const bool on = [] {
        wchar_t v[8] = {};
        return GetEnvironmentVariableW(L"AIRUI_PAINT_TRACE", v, 8) > 0;
    }();
    if (!on) return;
    static int budget = 60;
    if (budget <= 0) return;
    --budget;

    wchar_t path[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH) == 0) return;
    wcscat_s(path, L"\\AireRecord\\skia_font.log");
    FILE* fp = nullptr;
    _wfopen_s(&fp, path, L"a");
    if (!fp) return;
    fwprintf(fp, L"[text] role=%d size=%.2f w=%.1f avail=%.1f at=%.1f,%.1f scale=%.2f | %hs\n",
             role, (double)size, (double)w, (double)avail, (double)x, (double)y,
             (double)canvasScale, s.c_str());
    fclose(fp);
#else
    (void)role; (void)size; (void)w; (void)x; (void)y;
    (void)avail; (void)canvasScale; (void)s;
#endif
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

// The Dingbats block. GDI resolves it from its symbol font (Segoe UI
// Symbol) even when the UI family's font-link chain has the glyph as
// well -- measured: GDI's U+2713 is Segoe UI Symbol's to the pixel,
// while GDI's U+25BE is Yu Gothic UI's. So the two need two different
// faces, and the block decides which one goes first.
bool IsDingbat(uint32_t cp)
{
    return cp >= 0x2700u && cp <= 0x27BFu;
}

} // namespace

// ------------------------------------------------------------
// Text layout cache
// ------------------------------------------------------------
//
// A frame draws ~70 short labels out of a vocabulary of a few hundred
// strings, and the same strings come back every frame. Two things are
// expensive and neither is the rasterisation:
//
//   * building the run -- decode UTF-16 to UTF-8, decode that back to
//     codepoints, resolve a face per codepoint (a cmap walk), re-encode
//     each face run to UTF-8, then measure it (another cmap walk);
//   * deciding whether to ellipsise, which used to re-measure the whole
//     string once per dropped character (quadratic) and then measure it
//     again for the draw.
//
// So each distinct (role, string) is laid out exactly once: a
// pre-positioned SkTextBlob plus the advance at every codepoint
// boundary. Measuring becomes a field read, an ellipsis decision
// becomes a linear scan of `cum`, and drawing becomes one
// drawTextBlob() whose glyph masks Skia's strike cache already holds.
//
// Process-wide on purpose: a SkiaFrame may be rebuilt between frames,
// while the vocabulary a window uses is a property of the UI, not of
// the frame. Keyed by role only, so the DPI is part of the validity
// check -- the calibration em depends on it.
struct TextLayout
{
    sk_sp<SkTextBlob>     blob;      // glyphs positioned from x = 0
    float                 width = 0.f;
    std::vector<float>    cum;       // cum[i] = advance of cps[0..i)
    std::vector<uint32_t> cps;
};

struct TextCache
{
    int dpi = -1;
    std::unordered_map<std::string, TextLayout>
        map[static_cast<int>(FontRole::Count)];
};

// Deliberately leaked, like GetSharedFaces(): a static destructor would
// run while the plugin DLL unloads, when Skia's own code may be gone.
TextCache& GetTextCache()
{
    static TextCache* cache = new TextCache();
    return *cache;
}

struct SkiaPainter::Impl
{
    Impl(SkCanvas* canvas, void* nativeDc) : canvas(canvas), dc(nativeDc)
    {
        for (float& w : dotsW) w = -1.f;
    }

    SkCanvas* canvas = nullptr;
    void*     dc = nullptr;        // HDC used once for GDI calibration
    SkFont    fonts[static_cast<int>(FontRole::Count)];
    bool      ready[static_cast<int>(FontRole::Count)] = {};
    int       clipDepth = 0;
    // Width of "..." per role; -1 = not measured yet (see DotsWidth).
    float     dotsW[static_cast<int>(FontRole::Count)] = {};

    // Faces and metrics per role: Microsoft YaHei UI for UI text (and for
    // CJK -- the UI family covers hanzi, so GDI never font-links here),
    // Segoe MDL2 Assets for icons -- exactly what the GDI backend selects.
    sk_sp<SkTypeface>      roleFace[static_cast<int>(FontRole::Count)];
    RoleMetrics            roleM[static_cast<int>(FontRole::Count)] = {};
    sk_sp<SkTypeface>      roleCjk[static_cast<int>(FontRole::Count)];
    sk_sp<SkTypeface>      roleSymbol[static_cast<int>(FontRole::Count)];
    sk_sp<SkTypeface>      roleLinked[static_cast<int>(FontRole::Count)];
    sk_sp<SkTypeface>      cjkFace = nullptr;
    sk_sp<SkTypeface>      defaultFace = nullptr;
    bool                   facesReady = false;
    int                    calDpi = -1;    // DPI the role metrics were solved at

    // ---- per-frame reuse of the text machinery -------------------
    //
    // Everything below exists because a frame draws ~70 short labels and
    // the naive path re-did the same work for every one of them:
    // unicharToGlyph() walks the face's cmap, and copying an SkFont or
    // calling setTypeface() drops its cached strike spec, so the next
    // measure/draw rebuilds a descriptor and looks the strike up again.
    // Measured (AIRUI_FRAME_PROFILE=1): 285 us per label, 58.9 us per
    // character, i.e. 20.4 ms of a 34.9 ms frame -- 89 % of the drawing
    // cost -- while GDI drew the same frame in 4.5 ms.
    //
    // Same few hundred characters repeat every frame, so cache which
    // face covers each codepoint per role, and keep one prebuilt SkFont
    // per (role, face kind) instead of re-shaping a copy per text run.
    std::unordered_map<uint32_t, sk_sp<SkTypeface>>
        cpFace[static_cast<int>(FontRole::Count)];
    SkFont fontCjk[static_cast<int>(FontRole::Count)];
    bool   cjkFontReady[static_cast<int>(FontRole::Count)] = {};
    SkFont fontSymbol[static_cast<int>(FontRole::Count)];
    bool   symbolFontReady[static_cast<int>(FontRole::Count)] = {};
    SkFont fontLinked[static_cast<int>(FontRole::Count)];
    bool   linkedFontReady[static_cast<int>(FontRole::Count)] = {};
    SkFont scratchFont;             // system-default fallback, rare
    sk_sp<SkTypeface> scratchFace;

    void EnsureFaces()
    {
        if (facesReady) return;
        facesReady = true;

        const SharedFaces& shared = GetSharedFaces();
        const Calibration& cal = GetCalibration(dc);
        for (int i = 0; i < static_cast<int>(FontRole::Count); ++i) {
            const FontRole role = static_cast<FontRole>(i);
            roleFace[i] = UiFaceFor(shared, role);
            if (!roleFace[i]) roleFace[i] = shared.ui;
            roleCjk[i] = CjkFaceFor(shared, role);
            roleSymbol[i] = SymbolFaceFor(shared, role);
            if (!roleSymbol[i]) roleSymbol[i] = shared.symbol;
            roleLinked[i] = LinkedFaceFor(shared, role);
            if (!roleLinked[i]) roleLinked[i] = shared.linked;
            roleM[i] = cal.m[i];
        }
        cjkFace = shared.cjk;
        defaultFace = shared.fallback;
        calDpi = cal.dpi;
        DiagCalibration(cal.m);
    }

    // Pick the typeface that can actually draw `cp` for this role. Order
    // mirrors GDI: role family -> font-linked CJK family -> the symbol
    // faces GDI's own fallback uses -> system default. Falling back to
    // the generic system font first would give Chinese a different
    // family from GDI's and change its shapes -- and the system default
    // carries no U+2713 at all, so the check mark would come out as the
    // .notdef box.
    // Which face covers `cp` for this role. Cached per role because the
    // answer only depends on the (fixed) face set, while unicharToGlyph
    // itself is a cmap walk that used to run twice per character per
    // frame -- once measuring, once drawing.
    sk_sp<SkTypeface> FaceFor(FontRole role, SkUnichar cp)
    {
        EnsureFaces();
        const int i = static_cast<int>(role) < 0 ||
                      static_cast<int>(role) >= static_cast<int>(FontRole::Count)
                          ? 0 : static_cast<int>(role);
        auto& cache = cpFace[i];
        const uint32_t key = static_cast<uint32_t>(cp);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;

        sk_sp<SkTypeface>& base = roleFace[i];
        sk_sp<SkTypeface> picked = base;
        if (!(base && base->unicharToGlyph(cp))) {
            if (IsCjk(key) && roleCjk[i] && roleCjk[i]->unicharToGlyph(cp)) {
                picked = roleCjk[i];
            } else {
                // GDI's universal fallback, measured on Windows: the
                // Dingbats block comes from the symbol font, the other
                // missing symbols from the tail of the UI family's
                // font-link chain. Which one goes first depends on the
                // block, not on which face happens to have the glyph.
                const bool dingbat = IsDingbat(key);
                const sk_sp<SkTypeface>& first = dingbat ? roleSymbol[i] : roleLinked[i];
                const sk_sp<SkTypeface>& second = dingbat ? roleLinked[i] : roleSymbol[i];
                if (first && first->unicharToGlyph(cp))
                    picked = first;
                else if (second && second->unicharToGlyph(cp))
                    picked = second;
                else if (defaultFace && defaultFace->unicharToGlyph(cp))
                    picked = defaultFace;
            }
        }
        // Bounded: a UI shows a few hundred distinct characters, but the
        // map must not grow without limit in a long-lived process.
        if (cache.size() > 4096) cache.clear();
        cache.emplace(key, picked);
        return picked;
    }

    // The prebuilt font for a run: the role's Latin face, its CJK face
    // (sized by its own em -- the linked family has a different
    // win-metric ratio), its symbol faces (sized against the advance
    // GDI reports for the linked run), or a scratch font for the
    // system-default fallback. Returning a reference is the point:
    // copying an SkFont or calling setTypeface() throws away the strike
    // spec, and the next measureText()/drawSimpleText() has to rebuild
    // it.
    const SkFont& FontForFace(FontRole role, const sk_sp<SkTypeface>& face)
    {
        Font(role);                     // ensures fonts[i] / roleFace[i]
        const int i = Index(role);
        if (!face || face.get() == roleFace[i].get()) return fonts[i];
        if (face.get() == roleCjk[i].get()) {
            if (!cjkFontReady[i]) {
                SkFont f = fonts[i];
                f.setTypeface(roleCjk[i]);
                const SkScalar em = roleM[i].emCjk > 0 ? roleM[i].emCjk
                                                       : fonts[i].getSize();
                f.setSize(em);
                fontCjk[i] = f;
                cjkFontReady[i] = true;
            }
            return fontCjk[i];
        }
        if (roleSymbol[i] && face.get() == roleSymbol[i].get()) {
            if (!symbolFontReady[i]) {
                SkFont f = fonts[i];
                f.setTypeface(roleSymbol[i]);
                const SkScalar em = roleM[i].emSym > 0 ? roleM[i].emSym
                                                      : fonts[i].getSize();
                f.setSize(em);
                fontSymbol[i] = f;
                symbolFontReady[i] = true;
            }
            return fontSymbol[i];
        }
        if (roleLinked[i] && face.get() == roleLinked[i].get()) {
            if (!linkedFontReady[i]) {
                SkFont f = fonts[i];
                f.setTypeface(roleLinked[i]);
                const SkScalar em = roleM[i].emLink > 0 ? roleM[i].emLink
                                                       : fonts[i].getSize();
                f.setSize(em);
                fontLinked[i] = f;
                linkedFontReady[i] = true;
            }
            return fontLinked[i];
        }
        // Rare: a character only the system default can draw. Shape it
        // once per distinct face rather than per draw.
        if (scratchFace.get() != face.get()) {
            scratchFont = fonts[i];
            scratchFont.setTypeface(face);
            scratchFace = face;
        }
        return scratchFont;
    }

    SkFont& Font(FontRole role)
    {
        int i = static_cast<int>(role);
        if (i < 0 || i >= static_cast<int>(FontRole::Count)) i = 0;
        if (!ready[i]) {
            EnsureFaces();
            SkFont f;
            // Typeface first, then size: SkFont carries both, and the
            // order keeps the explicit size authoritative.
            if (roleFace[i]) f.setTypeface(roleFace[i]);
            // roleM[i].em is calibrated against GDI, so the two backends
            // draw the same string at the same pixel width.
            f.setSize(roleM[i].em > 0 ? roleM[i].em : 13.f);
            // GDI CLEARTYPE_QUALITY snaps stems to the pixel grid with
            // integer advances and colours the coverage per subpixel.
            // Full hinting + no subpixel positioning reproduces the grid,
            // and LCD edging reproduces the ClearType coverage -- without
            // it Skia falls back to greyscale AA and the same glyph looks
            // softer and muddier than GDI's, which is exactly the
            // difference the two backends were showing.
            f.setHinting(SkFontHinting::kFull);
            f.setSubpixel(false);
            f.setEdging(SkFont::Edging::kSubpixelAntiAlias);
            fonts[i] = f;
            ready[i] = true;
        }
        return fonts[i];
    }

    // ---- laid-out text, built once per (role, string) --------------
    //
    // See the TextLayout comment above the class: this is where a string
    // stops being "bytes to re-shape on every call" and becomes "a blob
    // plus a width table".
    TextLayout BuildLayout(FontRole role, const std::string& utf8)
    {
        TextLayout lay;
        DecodeUtf8(utf8, lay.cps);
        lay.cum.reserve(lay.cps.size() + 1);
        lay.cum.push_back(0.f);
        if (lay.cps.empty()) return lay;

        SkTextBlobBuilder       builder;
        std::vector<SkScalar>   widths;
        float                   x = 0.f;
        size_t                  i = 0;
        while (i < lay.cps.size()) {
            // One face per run, exactly like GDI's font linking: the
            // role's own family first, then the linked CJK family, then
            // the symbol family for characters neither covers.
            const sk_sp<SkTypeface> face = FaceFor(role, lay.cps[i]);
            size_t j = i + 1;
            while (j < lay.cps.size() &&
                   FaceFor(role, lay.cps[j]).get() == face.get())
                ++j;

            const SkFont& f  = FontForFace(role, face);
            const int     n  = static_cast<int>(j - i);
            // allocRunPosH returns a reference into the builder; the
            // buffer is valid until the next alloc*, which is why the
            // glyphs and positions are filled before looping.
            const SkTextBlobBuilder::RunBuffer& run =
                builder.allocRunPosH(f, n, 0.f);
            if (!run.glyphs || !run.pos) { i = j; continue; }

            // Codepoints -> glyph ids. This is the cmap walk, and it now
            // happens once per distinct string instead of once per
            // measure and once per draw, every frame.
            for (int k = 0; k < n; ++k)
                run.glyphs[k] = f.unicharToGlyph(lay.cps[i + static_cast<size_t>(k)]);

            widths.resize(static_cast<size_t>(n));
            // Modern Skia takes spans here, not (pointer, count).
            f.getWidths(SkSpan<const SkGlyphID>(run.glyphs, static_cast<size_t>(n)),
                        SkSpan<SkScalar>(widths.data(), static_cast<size_t>(n)));
            for (int k = 0; k < n; ++k) {
                run.pos[k] = x;
                x += widths[static_cast<size_t>(k)];
                lay.cum.push_back(x);
            }
            i = j;
        }
        lay.width = x;
        lay.blob  = builder.make();
        return lay;
    }

    // Cached layout for a UTF-8 string. The returned pointer is stable
    // until the next cache flush (only on a DPI change or overflow).
    const TextLayout* LayoutFor(FontRole role, const std::string& utf8)
    {
        EnsureFaces();
        TextCache& cache = GetTextCache();
        if (cache.dpi != calDpi) {
            for (auto& m : cache.map) m.clear();
            cache.dpi = calDpi;
        }
        auto& map = cache.map[Index(role)];
        auto it = map.find(utf8);
        if (it != map.end()) return &it->second;

        // Bounded: a UI shows a few hundred distinct strings. Clearing
        // wholesale keeps the cache flat without an LRU's bookkeeping.
        if (map.size() >= 4096) map.clear();
        return &map.emplace(utf8, BuildLayout(role, utf8)).first->second;
    }

    // Width of "...", per role. Constant for the lifetime of the cache.
    float DotsWidth(FontRole role)
    {
        const int i = Index(role);
        if (dotsW[i] < 0.f)
            dotsW[i] = Font(role).measureText("...", 3, SkTextEncoding::kUTF8);
        return dotsW[i];
    }

    // DT_END_ELLIPSIS: drop codepoints from the end until the remainder
    // plus "..." fits. `cum` turns this into a table scan -- the old code
    // re-measured the whole string once per dropped character.
    const TextLayout* Ellipsized(FontRole role, const TextLayout& lay,
                                 float avail, float dotsW)
    {
        size_t k = lay.cps.size();
        while (k > 1 && lay.cum[k] + dotsW > avail) --k;
        std::string key = EncodeUtf8(lay.cps.data(), k);
        key += "...";
        return LayoutFor(role, key);
    }

    // Draw UTF-8 text already positioned at baseline origin (x, y). One
    // call for the whole string: the blob carries the per-run fonts.
    void DrawRuns(FontRole role, const std::string& utf8,
                  float x, float y, Color c)
    {
        if (utf8.empty()) return;
        const TextLayout* lay = LayoutFor(role, utf8);
        if (!lay || !lay->blob) return;
        canvas->drawTextBlob(lay->blob.get(), x, y, MakeFill(c));
    }

    // GDI's DrawText lays text out inside a cell of `tmHeight` pixels and
    // puts the baseline `tmAscent` from the top of that cell. Using the
    // same numbers is what makes DT_TOP / DT_VCENTER / DT_BOTTOM land on
    // the same pixels as the Win32 backend; Skia's own ascent/descent are
    // typographic metrics and sit a couple of pixels elsewhere.
    //
    // Everything is integer arithmetic on purpose: DrawText divides the
    // leftover space with ints, and truncation (not rounding) is what
    // decides the row a label lands on when the slack is odd.
    float Baseline(const Rect& r, int i, skia::VAlign v) const
    {
        const RoleMetrics& m = roleM[i];
        const int cell    = m.cell;
        const int ascent  = static_cast<int>(m.ascent + 0.5f);
        const int descent = static_cast<int>(m.descent + 0.5f);
        switch (v) {
        case skia::VAlign::Middle:
            return static_cast<float>(r.top + (r.Height() - cell) / 2 + ascent);
        case skia::VAlign::Bottom:
            return static_cast<float>(r.bottom - descent);
        case skia::VAlign::Top:
        default:
            return static_cast<float>(r.top + ascent);
        }
    }

    int Index(FontRole role) const
    {
        int i = static_cast<int>(role);
        return (i < 0 || i >= static_cast<int>(FontRole::Count)) ? 0 : i;
    }
};

SkiaPainter::SkiaPainter(SkCanvas* canvas, void* nativeDc)
    : m(new Impl(canvas, nativeDc)) {}

SkiaPainter::~SkiaPainter() = default;

SkCanvas* SkiaPainter::Canvas() const { return m->canvas; }

void SkiaPainter::BeginFrame() { m->clipDepth = 0; }

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
    // GDI's rounded fill covers one pixel less than its square fill:
    // measured, RoundRect(l,t,r,b) paints the columns [l, r-1] while
    // FillRect paints [l, r). Skia's drawRRect fills [l, r), so every
    // rounded surface -- cards, chips, pills, the logo tile -- came out
    // one pixel wider and one pixel taller than the GDI backend's.
    // Shrinking the box by that pixel is what makes the two coincide;
    // gfx::Stroke compensates on the border side by covering [l, r].
    const SkScalar shrink = radius > 0 ? 1.f : 0.f;
    const SkRect box = SkRect::MakeLTRB(
        static_cast<SkScalar>(r.left), static_cast<SkScalar>(r.top),
        static_cast<SkScalar>(r.right) - shrink,
        static_cast<SkScalar>(r.bottom) - shrink);
    m->canvas->drawRRect(SkRRect::MakeRectXY(box, rr, rr), MakeFill(c));
}

// GDI borders occupy whole pixels, and which pixels depends on the shape:
//   * Rectangle(l,t,r,b) covers the columns [l, r) -- right/bottom are
//     exclusive, measured on a 1px pen;
//   * RoundRect(l,t,r+1,b+1) covers [l, r] -- gfx::Stroke widens the
//     rounded case by one pixel for exactly that reason.
// A Skia stroke is centred on its path, so drawing at the raw rect splits
// a 1px border across two half-covered pixels: every card, chip and
// separator came out lighter and blurrier than the GDI backend's. Insets
// by half the pen width turn the covered box back into a stroke path.
SkRect StrokePath(const Rect& r, int width, bool rounded)
{
    const SkScalar w = static_cast<SkScalar>(width > 0 ? width : 1);
    const SkScalar extra = rounded ? 1.f : 0.f;
    return SkRect::MakeLTRB(
        static_cast<SkScalar>(r.left) + w / 2.f,
        static_cast<SkScalar>(r.top) + w / 2.f,
        static_cast<SkScalar>(r.right) + extra - w / 2.f,
        static_cast<SkScalar>(r.bottom) + extra - w / 2.f);
}

void SkiaPainter::StrokeRect(const Rect& r, Color c, int width)
{
    if (!m->canvas || r.Empty()) return;
    m->canvas->drawRect(StrokePath(r, width, false), MakeStroke(c, width));
}

void SkiaPainter::StrokeRound(const Rect& r, int radius, Color c, int width)
{
    if (!m->canvas || r.Empty()) return;
    const SkScalar rr = static_cast<SkScalar>(radius > 0 ? radius : 0);
    m->canvas->drawRRect(SkRRect::MakeRectXY(StrokePath(r, width, true), rr, rr),
                         MakeStroke(c, width));
}

namespace {

// Dash geometry is a constant of the design system, and building the
// effect allocates an interval table that Skia then walks per segment.
// The dashed focus ring is drawn once per frame, so this is only worth
// one warm slot -- but that slot was 539 us per frame before.
const sk_sp<SkPathEffect>& DashEffectFor(int dash)
{
    static int                  cachedDash = -1;
    static sk_sp<SkPathEffect>* cached = new sk_sp<SkPathEffect>();  // process lifetime
    if (cachedDash != dash || !*cached) {
        const SkScalar d   = static_cast<SkScalar>(dash > 0 ? dash : 6);
        const SkScalar gap = static_cast<SkScalar>(dash > 0 ? dash : 4);
        const SkScalar intervals[2] = { d, gap };
        // Make() now takes (SkSpan<const SkScalar> intervals, SkScalar phase);
        // the old (pointer, count, phase) overload is gone.
        *cached = SkDashPathEffect::Make(
            SkSpan<const SkScalar>(intervals, 2), 0);
        cachedDash = dash;
    }
    return *cached;
}

} // namespace

void SkiaPainter::StrokeDashed(const Rect& r, Color c, int width, int dash)
{
    if (!m->canvas || r.Empty()) return;
    // SkPath is immutable in current Skia: contours are built with
    // SkPathBuilder and materialised with detach().
    SkPathBuilder builder;
    builder.addRect(ToSk(r));
    const SkPath path = builder.detach();
    SkPaint p = MakeStroke(c, width);
    p.setPathEffect(DashEffectFor(dash));
    m->canvas->drawPath(path, p);
}

void SkiaPainter::Text(const Rect& r, const wchar_t* s, size_t len,
                       FontRole role, Color c, TextFormat fmt)
{
    if (!m->canvas || !s || len == 0 || r.Empty()) return;

    const std::wstring w(s, len);
    const std::string utf8 = skia::WideToUtf8(w);
    const TextLayout* lay = m->LayoutFor(role, utf8);
    if (!lay) return;

    if (fmt & dt::EndEllipsis) {
        // GDI's DrawText decides in integer device pixels, so a
        // sub-pixel difference must not flip a label from "fits" to
        // "ellipsised". One pixel of slack reproduces that rounding; the
        // nav labels sit exactly on this boundary at the Title role.
        // The decision is now a scan of the precomputed advance table
        // instead of a re-measurement per dropped character.
        const float avail = static_cast<float>(r.Width()) + 1.f;
        const float dotsW = m->DotsWidth(role);
        // DT_END_ELLIPSIS only decorates text that does not fit. Adding
        // the dots unconditionally would put "..." on every
        // ellipsis-capable label, even short ones with room to spare.
        if (lay->width + dotsW > avail)
            lay = m->Ellipsized(role, *lay, avail, dotsW);
    }

    const float totalW = lay->width;
    const float x = AlignX(r, totalW, skia::Horizontal(fmt));
    const float y = m->Baseline(r, m->Index(role), skia::Vertical(fmt));
    // DrawText clips to its rect -- that clipping is the reason
    // DT_END_ELLIPSIS has to exist at all. Without it an over-long label
    // spills past its cell in Skia while GDI cuts it at the boundary.
    const bool clip = !(fmt & dt::NoClipping);
    if (clip) {
        m->canvas->save();
        m->canvas->clipRect(ToSk(r));
    }
    if (lay->blob) m->canvas->drawTextBlob(lay->blob.get(), x, y, MakeFill(c));
    if (clip) m->canvas->restore();
    DiagText(static_cast<int>(role), m->Font(role).getSize(), totalW, x, y,
             static_cast<float>(r.Width()), m->canvas->getTotalMatrix().getScaleX(),
             utf8);
}

void SkiaPainter::TextBlock(const Rect& r, const wchar_t* s, size_t len,
                            FontRole role, Color c, int maxLines)
{
    if (!m->canvas || !s || len == 0 || r.Empty()) return;
    const std::wstring w(s, len);
    const std::string utf8 = skia::WideToUtf8(w);
    SkFont& primary = m->Font(role);
    const RoleMetrics& rm = m->roleM[m->Index(role)];
    // GDI breaks lines at tmHeight, not at Skia's typographic line box.
    const float lineH = static_cast<float>(rm.cell);
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

    float y = static_cast<float>(r.top) + rm.ascent;
    for (const std::string& ln : lines) {
        if (y - rm.ascent > static_cast<float>(r.bottom)) break;
        if (!ln.empty())
            m->DrawRuns(role, ln, static_cast<float>(r.left), y, c);
        y += lineH;
    }
}

int SkiaPainter::MeasureText(const wchar_t* s, size_t len, FontRole role)
{
    if (!s || len == 0) return 0;
    const std::wstring w(s, len);
    const std::string utf8 = skia::WideToUtf8(w);
    // Same per-codepoint fallback (and per-family size) as drawing, so
    // layout decisions match what actually gets painted.
    const TextLayout* lay = m->LayoutFor(role, utf8);
    return lay ? static_cast<int>(lay->width + 0.5f) : 0;
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
