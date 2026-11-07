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

// ---- Typography calibration ----------------------------------------
//
// GDI and Skia do not share a font-size unit:
//   * LOGFONT.lfHeight is a *cell* height derived from the font's win
//     metrics, so a 13 px cell is roughly a 9.8 px em for Segoe UI;
//   * SkFont::setSize takes an *em* size, and SkFontMetrics reports the
//     typographic (OS/2 typo) metrics, which for Segoe UI add up to
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
    float    ascent  = 10.f;   // GDI tmAscent: baseline offset from the cell top
    float    descent = 3.f;    // GDI tmDescent
    int      cell    = 13;     // GDI cell height, which is also the line advance
};

// Width reference: Latin plus digits, so the average advance is
// representative of UI text and the ratio fixes the advance scale rather
// than one glyph's bearing.
constexpr wchar_t kRefText[] = L"AireRecord 0123456789";
// One Segoe MDL2 glyph, for icon-role calibration.
constexpr wchar_t kRefIcon   = 0xE700;
// CJK reference ("workbench overview clipboard"), written as code points so
// this file stays ASCII-only whatever the compiler's source charset is.
// GDI resolves it through font linking to Microsoft YaHei UI, which is a
// different size than the Latin em (see MeasureRole step 3).
constexpr wchar_t kRefCjk[] = {
    0x5DE5, 0x4F5C, 0x53F0, 0x603B, 0x89C8, 0x526A, 0x8D34, 0x677F, 0
};
constexpr int kRefCjkCount = 8;

struct SharedFaces
{
    sk_sp<SkTypeface> ui;        // Segoe UI           (UI text)
    sk_sp<SkTypeface> uiBold;    // Segoe UI Semibold  (headings, buttons)
    sk_sp<SkTypeface> cjk;       // Microsoft YaHei UI (what GDI font-links to)
    sk_sp<SkTypeface> cjkBold;   // Microsoft YaHei UI Semibold
    sk_sp<SkTypeface> icon;      // Segoe MDL2 Assets
    sk_sp<SkTypeface> fallback;  // system default, last resort
};

#if defined(_WIN32)
sk_sp<SkTypeface> MakeFace(const wchar_t* family, int weight, int cell)
{
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

// Resolve one role against GDI: same font the Win32 backend selects, same
// cell height, then copy GDI's own numbers for ascent/descent and solve
// the em that reproduces GDI's advance widths.
//
// Measured at the target cell, NOT extrapolated from a large size:
// CreateFontW(-13) realises a 17px cell for Segoe UI (GDI emulates the
// requested character height by picking the nearest bigger cell), and the
// 17/13 step is not linear across sizes, so scaling a 200px measurement
// down by cell/200 lands on a size GDI never draws. Everything here is
// therefore read straight off the DC at the size we actually ask for.
#if defined(_WIN32) && !defined(AIRUI_SKIA_MOCK)
RoleMetrics MeasureRole(HDC dc, const SharedFaces& f, FontRole role)
{
    RoleMetrics rm;
    const FontSpec spec = RoleFontSpec(role);
    const bool isIcon = (role == FontRole::Icon || role == FontRole::IconLarge);
    rm.cell = spec.cell > 0 ? spec.cell : 13;
    rm.em   = static_cast<SkScalar>(rm.cell);
    rm.emCjk = rm.em;
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
    // cell GDI actually realises (a -13 request yields 17 for Segoe UI),
    // and that realised cell -- not the request -- is what DrawText uses
    // for DT_VCENTER and for line advance.
    TEXTMETRICW tm{};
    if (GetTextMetricsW(dc, &tm)) {
        if (tm.tmHeight > 0)  rm.cell    = tm.tmHeight;
        if (tm.tmAscent > 0)  rm.ascent  = static_cast<float>(tm.tmAscent);
        if (tm.tmDescent > 0) rm.descent = static_cast<float>(tm.tmDescent);
    }

    // (2) Latin em: ratio of GDI's advance width to Skia's at em 1000.
    auto solve = [&](const wchar_t* text, int chars, const sk_sp<SkTypeface>& tf,
                     SkTextEncoding enc, int bytes, SkScalar& out) {
        SIZE sz{};
        if (!tf || !GetTextExtentPoint32W(dc, text, chars, &sz) || sz.cx <= 0)
            return;
        SkFont probe;
        probe.setTypeface(tf);
        probe.setSize(1000.f);
        const float w1000 = probe.measureText(text, (size_t)bytes, enc);
        if (w1000 <= 1.f) return;
        out = 1000.f * static_cast<float>(sz.cx) / w1000;
    };

    if (isIcon) {
        solve(&kRefIcon, 1, face, SkTextEncoding::kUTF16,
              (int)sizeof(wchar_t), rm.em);
    } else {
        solve(kRefText, static_cast<int>(sizeof(kRefText) / sizeof(wchar_t)) - 1,
              face, SkTextEncoding::kUTF16,
              (int)(sizeof(kRefText) - sizeof(wchar_t)), rm.em);

        // (3) CJK em: GDI hands Chinese to Microsoft YaHei UI through font
        // linking, at a size that is NOT the Latin em -- at cell 13 GDI
        // advances a hanzi 16px while the Latin em is ~12.8. So measure it:
        // two runs of the same glyph, one long one short, give the per-glyph
        // advance without any bearing contamination. Linking only kicks in
        // near the requested size (measuring at 200px returns unlinked
        // metrics), which is why this must run at the target cell.
        SIZE many{}, one{};
        if (GetTextExtentPoint32W(dc, kRefCjk, kRefCjkCount, &many) &&
            GetTextExtentPoint32W(dc, kRefCjk, 1, &one) &&
            many.cx > one.cx) {
            const float adv = static_cast<float>(many.cx - one.cx) /
                              static_cast<float>(kRefCjkCount - 1);
            // One hanzi advances exactly one em, so the advance is the em.
            if (adv > 0.f) rm.emCjk = adv;
        }
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
        fwprintf(fp, L"[calib] role=%d cell=%d em=%.2f cjk=%.2f ascent=%.1f descent=%.1f\n",
                 i, m[i].cell, (double)m[i].em, (double)m[i].emCjk,
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

} // namespace

struct SkiaPainter::Impl
{
    Impl(SkCanvas* canvas, void* nativeDc) : canvas(canvas), dc(nativeDc) {}

    SkCanvas* canvas = nullptr;
    void*     dc = nullptr;        // HDC used once for GDI calibration
    SkFont    fonts[static_cast<int>(FontRole::Count)];
    bool      ready[static_cast<int>(FontRole::Count)] = {};
    int       clipDepth = 0;

    // Faces and metrics per role: Segoe UI for UI text, Segoe MDL2
    // Assets for icons -- exactly what the GDI backend selects -- plus a
    // CJK face standing in for GDI's font linking.
    sk_sp<SkTypeface>      roleFace[static_cast<int>(FontRole::Count)];
    RoleMetrics            roleM[static_cast<int>(FontRole::Count)] = {};
    sk_sp<SkTypeface>      roleCjk[static_cast<int>(FontRole::Count)];
    sk_sp<SkTypeface>      cjkFace = nullptr;
    sk_sp<SkTypeface>      defaultFace = nullptr;
    bool                   facesReady = false;

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
            roleM[i] = cal.m[i];
        }
        cjkFace = shared.cjk;
        defaultFace = shared.fallback;
        DiagCalibration(cal.m);
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
            roleCjk[i] && roleCjk[i]->unicharToGlyph(cp))
            return roleCjk[i];
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
            // Typeface first, then size: SkFont carries both, and the
            // order keeps the explicit size authoritative.
            if (roleFace[i]) f.setTypeface(roleFace[i]);
            // roleM[i].em is calibrated against GDI, so the two backends
            // draw the same string at the same pixel width.
            f.setSize(roleM[i].em > 0 ? roleM[i].em : 13.f);
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
            if (face) {
                f.setTypeface(face);
                // Chinese is sized separately: its linked family has a
                // different win-metric ratio than the Latin face.
                if (roleCjk[Index(role)].get() == face.get())
                    f.setSize(roleM[Index(role)].emCjk);
            }
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
            if (face) {
                f.setTypeface(face);
                if (roleCjk[Index(role)].get() == face.get())
                    f.setSize(roleM[Index(role)].emCjk);
            }
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

    // GDI's DrawText lays text out inside a cell of `tmHeight` pixels and
    // puts the baseline `tmAscent` from the top of that cell. Using the
    // same numbers is what makes DT_TOP / DT_VCENTER / DT_BOTTOM land on
    // the same pixels as the Win32 backend; Skia's own ascent/descent are
    // typographic metrics and sit a couple of pixels elsewhere.
    float Baseline(const Rect& r, int i, skia::VAlign v) const
    {
        const RoleMetrics& m = roleM[i];
        const float cell = static_cast<float>(m.cell);
        float y = 0.f;
        switch (v) {
        case skia::VAlign::Middle:
            y = static_cast<float>(r.top)
              + std::floor((static_cast<float>(r.Height()) - cell) / 2.f) + m.ascent;
            break;
        case skia::VAlign::Bottom:
            y = static_cast<float>(r.bottom) - m.descent;
            break;
        case skia::VAlign::Top:
        default:
            y = static_cast<float>(r.top) + m.ascent;
            break;
        }
        return std::floor(y);
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
        // GDI's DrawText decides in integer device pixels, so a
        // sub-pixel difference must not flip a label from "fits" to
        // "ellipsised". One pixel of slack reproduces that rounding; the
        // nav labels sit exactly on this boundary at the Title role.
        const float avail = static_cast<float>(r.Width()) + 1.f;
        const float dotsW = primary.measureText("...", 3, SkTextEncoding::kUTF8);
        const float textW = m->MeasureRuns(role, cps, primary);
        // DT_END_ELLIPSIS only decorates text that does not fit. Adding
        // the dots unconditionally (as this used to) put "..." on every
        // ellipsis-capable label, even short ones with room to spare.
        if (textW + dotsW > avail) {
            while (cps.size() > 1 &&
                   m->MeasureRuns(role, cps, primary) + dotsW > avail)
                cps.pop_back();
            cps.push_back(static_cast<uint32_t>('.'));
            cps.push_back(static_cast<uint32_t>('.'));
            cps.push_back(static_cast<uint32_t>('.'));
            utf8 = EncodeUtf8(cps.data(), cps.size());
            DecodeUtf8(utf8, cps);
        }
    }

    const float totalW = m->MeasureRuns(role, cps, primary);
    float x = static_cast<float>(r.left);
    switch (skia::Horizontal(fmt)) {
    case skia::HAlign::Center: x += (static_cast<float>(r.Width()) - totalW) / 2.f; break;
    case skia::HAlign::Right:  x += static_cast<float>(r.Width()) - totalW;         break;
    default: break;
    }
    const float y = m->Baseline(r, m->Index(role), skia::Vertical(fmt));
    m->DrawRuns(primary, role, utf8, x, y, c);
    DiagText(static_cast<int>(role), primary.getSize(), totalW, x, y,
             static_cast<float>(r.Width()), m->canvas->getTotalMatrix().getScaleX(),
             utf8);
}

void SkiaPainter::TextBlock(const Rect& r, const std::wstring& s,
                            FontRole role, Color c, int maxLines)
{
    if (!m->canvas || s.empty() || r.Empty()) return;
    const std::string utf8 = skia::WideToUtf8(s);
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
            m->DrawRuns(primary, role, ln, static_cast<float>(r.left), y, c);
        y += lineH;
    }
}

int SkiaPainter::MeasureText(const std::wstring& s, FontRole role)
{
    if (s.empty()) return 0;
    const std::string utf8 = skia::WideToUtf8(s);
    std::vector<uint32_t> cps;
    DecodeUtf8(utf8, cps);
    SkFont& primary = m->Font(role);
    // Same per-codepoint fallback (and per-family size) as drawing, so
    // layout decisions match what actually gets painted.
    const float w = m->MeasureRuns(role, cps, primary);
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
