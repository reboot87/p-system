#pragma once
// ============================================================
// SkiaApiMock.h - NOT Skia, and NOT a rendering implementation.
//
// A minimal API-shaped stand-in so scripts/check-skia-syntax.sh can
// compile the real SkiaPainter.cpp / SkiaSurface.cpp with -fsyntax-only
// on a machine without a Skia SDK. It catches syntax errors, missing
// includes, wrong overload shapes and type mismatches in our backend.
// It deliberately does NOT draw anything and is never linked.
//
// The authoritative check is the CI job that builds with a real Skia
// SDK (see .github/workflows/ci.yml). When a real Skia version changes
// an API, update BOTH SkiaApi.h (the real include set) and this mock,
// otherwise this gate silently validates a dead contract.
//
// Current contract follows modern Skia: SkPath is immutable (build
// contours with SkPathBuilder), SkDashPathEffect::Make takes an
// SkSpan, and SkTypeface::MakeFromName no longer exists.
// ============================================================
#include <cstddef>
#include <cstdint>

using SkScalar = float;
using SkColor  = uint32_t;

template <class T>
class SkSpan
{
public:
    constexpr SkSpan(T* p, size_t n) : m_p(p), m_n(n) {}
    T*     data() const { return m_p; }
    size_t size() const { return m_n; }
private:
    T*     m_p = nullptr;
    size_t m_n = 0;
};

constexpr SkColor SK_ColorTRANSPARENT = 0u;

enum SkColorType { kBGRA_8888_SkColorType = 0 };
enum SkAlphaType { kPremul_SkAlphaType = 0 };
enum class SkTextEncoding { kUTF8, kUTF16, kUTF32, kGlyphID };

class SkPaint;

template <class T>
class sk_sp
{
public:
    sk_sp() = default;
    sk_sp(std::nullptr_t) : m_ptr(nullptr) {}
    explicit sk_sp(T* p) : m_ptr(p) {}
    T* get() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }
    explicit operator bool() const { return m_ptr != nullptr; }
private:
    T* m_ptr = nullptr;
};

struct SkRect
{
    float fLeft = 0, fTop = 0, fRight = 0, fBottom = 0;
    static SkRect MakeLTRB(float l, float t, float r, float b)
    { return SkRect{ l, t, r, b }; }
};

struct SkRRect
{
    static SkRRect MakeRectXY(const SkRect&, SkScalar, SkScalar) { return {}; }
};

struct SkFontMetrics
{
    SkScalar fAscent  = -10.0f;
    SkScalar fDescent = 3.0f;
};

struct SkFontStyle
{
    static SkFontStyle Normal() { return SkFontStyle(); }
};

using SkUnichar = int32_t;

// Modern Skia removed SkTypeface::MakeFromName. On Windows the backend
// builds typefaces from the GDI LOGFONT the UI already uses via Skia's
// SkCreateTypefaceFromLOGFONT (include/ports/SkTypeface_win.h); on other
// platforms it leaves faces null (non-Windows is not a supported runtime
// target). A SkFontMgr-based path is intentionally avoided because its
// factory API differs between Skia versions.
class SkTypeface
{
public:
    SkUnichar unicharToGlyph(SkUnichar) const { return 0; }
};

// ---- Windows font loading (mocked) ----
// In real Windows builds the backend calls Skia's free function
// SkCreateTypefaceFromLOGFONT(const LOGFONT&). Under the syntax gate we
// never include SkTypeface_win.h, so we declare the function and -- when
// the real <windows.h> has NOT been pulled into this TU -- a minimal
// LOGFONT plus the few GDI constants the backend's font setup needs.
#ifndef _INC_WINDOWS
// The backend's font setup uses the TCHAR/TEXT model so the same code
// compiles in both ANSI (LOGFONTA) and Unicode (LOGFONTW) Windows builds.
// Provide minimal stand-ins when the real <windows.h> is absent.
typedef wchar_t TCHAR;
#define TEXT(x) L##x
struct LOGFONT {
    long          lfHeight = 0;
    long          lfWidth = 0;
    long          lfEscapement = 0;
    long          lfOrientation = 0;
    long          lfWeight = 0;
    unsigned char lfItalic = 0;
    unsigned char lfUnderline = 0;
    unsigned char lfStrikeOut = 0;
    unsigned char lfCharSet = 0;
    unsigned char lfOutPrecision = 0;
    unsigned char lfClipPrecision = 0;
    unsigned char lfQuality = 0;
    unsigned char lfPitchAndFamily = 0;
    TCHAR         lfFaceName[32] = {};
};
#define FW_NORMAL           400
#define DEFAULT_CHARSET     1
#define OUT_DEFAULT_PRECIS  0
#define CLIP_DEFAULT_PRECIS 0
#define CLEARTYPE_QUALITY   5
#define DEFAULT_PITCH       0
#define FF_DONTCARE         0
#endif
sk_sp<SkTypeface> SkCreateTypefaceFromLOGFONT(const LOGFONT&);

enum class SkFontHinting { kNone, kSlight, kNormal, kFull };

class SkFont
{
public:
    enum class Edging { kAlias, kAntiAlias, kSubpixelAntiAlias };
    void setTypeface(sk_sp<SkTypeface>) {}
    void setSize(SkScalar) {}
    void setHinting(SkFontHinting) {}
    void setSubpixel(bool) {}
    void setEdging(Edging) {}
    void getMetrics(SkFontMetrics*) const {}
    SkScalar measureText(const void*, size_t, SkTextEncoding,
                         SkRect* = nullptr, const SkPaint* = nullptr) const
    { return 0.0f; }
};

class SkPathEffect {};

class SkDashPathEffect
{
public:
    static sk_sp<SkPathEffect> Make(SkSpan<const SkScalar>, SkScalar)
    { return sk_sp<SkPathEffect>(nullptr); }
};

class SkPaint
{
public:
    enum Style { kFill_Style, kStroke_Style };
    void setAntiAlias(bool) {}
    void setStyle(Style) {}
    void setColor(SkColor) {}
    void setStrokeWidth(SkScalar) {}
    void setPathEffect(sk_sp<SkPathEffect>) {}
};

// SkPath is immutable in current Skia; contours are built separately.
class SkPath {};

class SkPathBuilder
{
public:
    SkPathBuilder& addRect(const SkRect&) { return *this; }
    SkPath detach() { return SkPath{}; }
};

class SkImageInfo
{
public:
    static SkImageInfo Make(int, int, SkColorType, SkAlphaType) { return {}; }
};

class SkBitmap
{
public:
    void        allocPixels(const SkImageInfo&) {}
    const void* getPixels() const { return nullptr; }
    size_t      rowBytes() const { return 0; }
};

class SkCanvas
{
public:
    explicit SkCanvas(const SkBitmap&) {}
    void clear(SkColor) {}
    void save() {}
    void restore() {}
    void clipRect(const SkRect&) {}
    void translate(SkScalar, SkScalar) {}
    void drawRect(const SkRect&, const SkPaint&) {}
    void drawRRect(const SkRRect&, const SkPaint&) {}
    void drawPath(const SkPath&, const SkPaint&) {}
    void drawSimpleText(const void*, size_t, SkTextEncoding,
                        SkScalar, SkScalar, const SkFont&, const SkPaint&) {}
};
