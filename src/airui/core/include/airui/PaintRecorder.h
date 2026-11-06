#pragma once
// ============================================================
// AirUI - PaintRecorder.h
// Platform-free DrawCall recorder: the acceptance base for G5
// (cross-backend DrawCall consistency, docs/cross-platform.md).
//
// The old DrawCall.h / TestShell.h recorder lives in platform/win32
// and depends on the native SDK and its handle / geometry types, so it
// cannot compile on Linux/macOS. This one records IPainter calls using
// only core types (Rect / Color / FontRole / TextFormat), which makes
// a single golden sequence reproducible on every platform.
//
// Workflow once a second backend exists:
//   PaintRecorder a; coreScene.Paint(a, ...);     // reference
//   PaintRecorder b; coreScene.Paint(b, ...);     // via Skia backend
//   assert(a.Diff(b).empty());                    // G5 gate
// ============================================================
#include "airui/hal.h"
#include "airui/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace air {
namespace ui {

enum class PaintOpKind
{
    FillRect,
    FillRectAlpha,
    FillRound,
    StrokeRect,
    StrokeRound,
    StrokeDashed,
    Text,
    TextBlock,
    MeasureText,
    PushClip,
    PopClip,
    Translate
};

// ASCII name, safe to print in CI logs on any platform.
const char* PaintOpName(PaintOpKind kind);

struct PaintOp
{
    PaintOpKind kind = PaintOpKind::FillRect;
    Rect        rect{};
    Color       color = 0;
    uint8_t     alpha = 0;        // 0 = unspecified / wildcard
    int         radius = 0;
    int         width = 0;
    int         dash = 0;
    int         maxLines = 0;
    int         measureResult = 0;
    FontRole    role = FontRole::Body;
    TextFormat  format = 0;
    std::wstring text;
    Point       offset{};

    // Wildcard comparison: on the EXPECTED side, zero scalar values,
    // empty text, an empty rect and FontRole::Body mean "don't care".
    // Non-empty expected fields must match exactly.
    bool Matches(const PaintOp& expected) const;

    // Deterministic ASCII serialization (no wide chars, no locale
    // dependent formatting) suitable for golden files and diffs.
    std::string ToString() const;
};

class PaintRecorder : public IPainter
{
public:
    const std::vector<PaintOp>& Ops() const { return m_ops; }
    void Clear() { m_ops.clear(); }

    size_t Count(PaintOpKind kind) const;

    // Strict, all-fields comparison for golden sequences.
    bool   SequenceEquals(const PaintRecorder& other) const;
    size_t FirstDiff(const PaintRecorder& other) const;   // (size_t)-1 when equal
    // Empty string when equal; otherwise a short human-readable diff.
    std::string Diff(const PaintRecorder& other) const;

    // ---- IPainter: every call becomes one PaintOp ----
    void FillRect(const Rect& r, Color c) override;
    void FillRectAlpha(const Rect& r, Color c, uint8_t alpha) override;
    void FillRound(const Rect& r, int radius, Color c) override;
    void StrokeRect(const Rect& r, Color c, int width) override;
    void StrokeRound(const Rect& r, int radius, Color c, int width) override;
    void StrokeDashed(const Rect& r, Color c, int width, int dash) override;
    void Text(const Rect& r, const std::wstring& s,
              FontRole role, Color c, TextFormat fmt) override;
    void TextBlock(const Rect& r, const std::wstring& s,
                   FontRole role, Color c, int maxLines) override;
    int  MeasureText(const std::wstring& s, FontRole role) override;
    void PushClip(const Rect& r) override;
    void PopClip() override;
    void Translate(Point offset) override;

private:
    std::vector<PaintOp> m_ops;
};

} // namespace ui
} // namespace air
