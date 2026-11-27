#pragma once
// ============================================================
// AirUI - SkiaPainter.h
// IPainter implementation backed by a SkCanvas. This header is
// deliberately free of Skia headers (pimpl + forward declaration),
// so core consumers can include it without pulling in the SDK.
//
// Coordinates are logical pixels in the canvas' current transform.
// Colors arrive as core 0x00BBGGRR and are converted in SkiaTypes.h.
// ============================================================
#include "airui/hal.h"

#include <memory>

class SkCanvas;   // global namespace in Skia

namespace air {
namespace ui {

class SkiaPainter : public IPainter
{
public:
    // The painter does not own the canvas. Caller keeps it alive.
    // `nativeDc` is optional: on Windows it is the native device context
    // the frame will be composited onto, used once to calibrate text
    // against the platform text engine (the two backends must produce
    // identical type size, and their cell metrics are not the same unit).
    explicit SkiaPainter(SkCanvas* canvas, void* nativeDc = nullptr);
    ~SkiaPainter() override;

    SkCanvas* Canvas() const;

    // Resets the per-frame state when the owning frame is reused for a
    // new paint (the canvas itself is reset by SkiaFrame::BeginFrame).
    void BeginFrame();

    void FillRect(const Rect& r, Color c) override;
    void FillRectAlpha(const Rect& r, Color c, uint8_t alpha) override;
    void FillRound(const Rect& r, int radius, Color c) override;
    void StrokeRect(const Rect& r, Color c, int width) override;
    void StrokeRound(const Rect& r, int radius, Color c, int width) override;
    void StrokeDashed(const Rect& r, Color c, int width, int dash) override;
    void StrokeDashedRound(const Rect& r, int radius, Color c, int width,
                           int dash) override;
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

} // namespace ui
} // namespace air
