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

struct SkFontStyle {};

// Modern Skia removed SkTypeface::MakeFromName; typeface lookup goes
// through SkFontMgr instead. The backend currently relies on SkFont's
// default typeface, so no factory is part of our API contract.
class SkTypeface {};

class SkFont
{
public:
    void setTypeface(sk_sp<SkTypeface>) {}
    void setSize(SkScalar) {}
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
    bool        allocPixels(const SkImageInfo&) { return true; }
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
