#pragma once
// ============================================================
// AirUI - PaintBackend.h
// Runtime painter backend selection (Strategy + Registry).
//
// Which IPainter a frame uses is decided at RUNTIME, never by a
// compile-time option. A provider is registered by the platform layer
// (GDI is always there; Skia may be linked in or loaded from a
// plugin library), and callers ask the registry for a painter by id.
//
// This file lives in core on purpose: it only knows IPainter and an
// opaque native surface pointer, so it stays compilable on every
// target without a single platform header.
// ============================================================
#include "airui/hal.h"

#include <memory>
#include <string>
#include <vector>

namespace air {
namespace ui {

// ------------------------------------------------------------
// PaintTarget - everything a provider needs to open a frame.
//   surface : opaque native drawing surface (a Win32 device context
//             on Windows). nullptr asks for an off-screen frame,
//             which is what headless tests and golden renders use.
//   bounds  : the frame rectangle in logical pixels.
// ------------------------------------------------------------
struct PaintTarget
{
    void* surface = nullptr;
    Rect  bounds{};
};

// ------------------------------------------------------------
// IPainterProvider - one backend's factory.
//   Available() must be cheap: it is consulted on every Begin().
//   Begin() returns a painter whose destruction flushes the frame.
// ------------------------------------------------------------
class IPainterProvider
{
public:
    virtual ~IPainterProvider() = default;
    virtual std::string Id() const = 0;
    virtual bool Available() const = 0;
    virtual std::unique_ptr<IPainter> Begin(const PaintTarget& target) = 0;
};

// ------------------------------------------------------------
// PainterRegistry - process-wide set of providers.
//   Begin(choice, target) resolves "auto" to the default backend,
//   and gracefully falls back to "gdi" when the requested backend
//   is missing, unavailable, or fails to open a frame.
//   LastError() explains the last fallback (diagnostics / tests).
// ------------------------------------------------------------
class PainterRegistry
{
public:
    static PainterRegistry& Instance();

    // Replaces a provider with the same id (re-registration is how a
    // dynamically loaded plugin takes over from a built-in backend).
    void Register(std::unique_ptr<IPainterProvider> provider);

    std::vector<std::string> Backends() const;
    bool Available(const std::string& id) const;

    void               SetDefault(const std::string& id);
    const std::string& Default() const { return m_default; }

    std::unique_ptr<IPainter> Begin(const std::string& choice,
                                    const PaintTarget& target);
    const std::string& LastError() const { return m_error; }

    // Test hook: drop every provider and restore the initial state.
    void Reset();

    PainterRegistry(const PainterRegistry&) = delete;
    PainterRegistry& operator=(const PainterRegistry&) = delete;

private:
    PainterRegistry() = default;
    IPainterProvider* Find(const std::string& id) const;

    std::vector<std::unique_ptr<IPainterProvider>> m_providers;
    std::string m_default = "gdi";
    std::string m_error;
};

// Lower-case / trim a backend id. An empty string means "auto".
std::string NormalizeBackend(const std::string& name);

// Precedence for the application-level choice:
//   explicit request > environment > legacy skia preference > "auto".
// Kept as a pure function so it is testable on all platforms.
std::string ResolvePaintBackend(const std::string& requested,
                                const std::string& environment,
                                bool legacySkiaPreference = false);

// Reads AIRUI_PAINT_BACKEND (empty when unset).
std::string EnvironmentPaintBackend();

// Next backend for a UI "switch renderer" action. `available` must be
// the ids the registry currently reports as usable. Order is
// deterministic (gdi first, then registration order) and cycles back
// to the first id. Only available ids are returned; when there is a
// single option the same id comes back, which lets the UI show that
// no switch is possible without silently "succeeding".
// "auto" / unknown current ids are treated as the default ("gdi").
std::string NextPaintBackend(const std::string& current,
                             const std::vector<std::string>& available);

} // namespace ui
} // namespace air
