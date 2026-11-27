#pragma once
// ============================================================
// AirUI - SkiaSurface.h
// The Windows bridge: a raster Skia canvas (32bpp BGRA premultiplied)
// composited onto a native device context through the platform blit.
// Transparent pixels leave the underlying drawing untouched, so Skia
// chrome and legacy drawing code can coexist.
//
// On hosts without that native surface path the same class works as an
// off-screen frame, which is what the cross-backend tests use.
// ============================================================
#include "airui/hal.h"

#include <cstddef>
#include <memory>

class SkCanvas;

namespace air {
namespace ui {

class SkiaFrame : public IPainter
{
public:
#ifdef _WIN32
    // Draws into `bounds` of the given native device context and
    // composites on destruction. The handle is opaque here; the
    // backend casts it to its own native type.
    SkiaFrame(void* nativeSurface, const Rect& bounds);
#endif
    // Off-screen frame, usable on any OS with a Skia SDK.
    SkiaFrame(int width, int height);
    ~SkiaFrame() override;

    bool      Valid() const;
    void      Present();
    SkCanvas* Canvas() const;

    // Re-arms a pooled frame for the next paint on `dc`: resets the
    // canvas to its root state and re-binds the target. `clearScreen`
    // is false for a reused frame -- the previous frame's pixels are
    // about to be fully repainted, and clearing 8 MB costs 2 ms.
    // Not part of IPainter: only the frame pool calls it.
#ifdef _WIN32
    void BeginFrame(void* nativeSurface, const Rect& bounds, bool clearScreen);
#endif

    // Raw BGRA premultiplied pixels (off-screen frames / golden tests).
    const void* Pixels() const;
    size_t      RowBytes() const;
    int         Width() const;
    int         Height() const;

    // ---- IPainter (delegated to SkiaPainter) ----
    void FillRect(const Rect& r, Color c) override;
    void FillRectAlpha(const Rect& r, Color c, uint8_t alpha) override;
    void FillRound(const Rect& r, int radius, Color c) override;
    void StrokeRect(const Rect& r, Color c, int width) override;
    void StrokeRound(const Rect& r, int radius, Color c, int width) override;
    void StrokeDashed(const Rect& r, Color c, int width, int dash) override;
    void Text(const Rect& r, const wchar_t* s, size_t len,
              FontRole role, Color c, TextFormat fmt) override;
    void TextBlock(const Rect& r, const wchar_t* s, size_t len,
                   FontRole role, Color c, int maxLines) override;
    int  MeasureText(const wchar_t* s, size_t len, FontRole role) override;
    void PushClip(const Rect& r) override;
    void PopClip() override;
    void Translate(Point offset) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

// Backend factory (docs/cross-platform.md section 3.1). Where a native
// surface path exists it returns a frame bound to that surface;
// elsewhere an off-screen frame.
#ifdef _WIN32
// Pooled frame lifecycle (see SkiaSurface.cpp). Opening a frame costs a
// fresh 8 MB bitmap plus a full-surface clear -- measured at 5.4 ms of a
// 21 ms frame, i.e. a quarter of the frame for pixels that are then
// completely overwritten. The pool keeps the previous frame's bitmap,
// surface and painter warm between paints of the same size.
SkiaFrame* AcquireSkiaFrame(void* nativeSurface, const Rect& bounds);
// Composites the frame to its target, then parks it for the next paint.
// Frames that were never pooled (off-screen) are destroyed.
void       ReleaseSkiaFrame(SkiaFrame* frame);

std::unique_ptr<IPainter> CreateSkiaBackend(void* nativeSurface, const Rect& bounds);
#else
std::unique_ptr<IPainter> CreateSkiaBackend(int width, int height);
#endif

} // namespace ui
} // namespace air
