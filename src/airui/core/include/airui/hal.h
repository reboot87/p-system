#pragma once
// ============================================================
// AirUI - hal.h
// Hardware abstraction layer, phase 0 freeze.
//
// Three layers:
//   core       - components, layout, theme, state machines
//   hal        - the interfaces in this file
//   platform   - Win32 (GDI/DWM), later Skia/Cocoa/X11
//
// Everything in core talks to the outside world only through
// these interfaces, so core links and compiles on every target
// without a single platform header.
// ============================================================
#include "airui/types.h"

#include <cstdint>
#include <functional>
#include <string>

namespace air {
namespace ui {

enum class FontRole
{
    Body,
    Meta,
    Title,
    Section,
    Button,
    Icon,
    IconLarge,
    Display,
    Count
};

// ------------------------------------------------------------
// IClock - monotonic time in milliseconds for animations.
// Win32 uses its tick counter, Linux uses a monotonic clock.
// ------------------------------------------------------------
class IClock
{
public:
    virtual ~IClock() = default;
    virtual uint64_t NowMs() = 0;
};

// ------------------------------------------------------------
// IFont - opaque font handle owned by a backend.
// Core only asks for metrics, never for a native handle.
// ------------------------------------------------------------
class IFont
{
public:
    struct Metrics
    {
        int height = 0;      // line height in device pixels
        int ascent = 0;
        int descent = 0;
    };

    virtual ~IFont() = default;
    virtual Metrics GetMetrics() const = 0;
};

// ------------------------------------------------------------
// IPainter - the drawing surface exposed to widgets.
// All coordinates are logical pixels in the target's space.
// ------------------------------------------------------------
class IPainter
{
public:
    virtual ~IPainter() = default;

    virtual void FillRect(const Rect& r, Color c) = 0;
    virtual void FillRound(const Rect& r, int radius, Color c) = 0;
    virtual void StrokeRect(const Rect& r, Color c, int width) = 0;
    virtual void StrokeRound(const Rect& r, int radius, Color c, int width) = 0;
    virtual void StrokeDashed(const Rect& r, Color c, int width, int dash) = 0;

    virtual void Text(const Rect& r, const std::wstring& s,
                      FontRole role, Color c, TextFormat fmt) = 0;
    virtual void TextBlock(const Rect& r, const std::wstring& s,
                           FontRole role, Color c, int maxLines) = 0;

    virtual int  MeasureText(const std::wstring& s, FontRole role) = 0;

    virtual void PushClip(const Rect& r) = 0;
    virtual void PopClip() = 0;

    virtual void Translate(Point offset) = 0;
};

// ------------------------------------------------------------
// IWindow - one native top-level window.
// Backend owns the message loop; core owns the state.
// ------------------------------------------------------------
class IWindow
{
public:
    virtual ~IWindow() = default;

    virtual void SetTitle(const std::wstring& title) = 0;
    virtual void SetMinSize(int w, int h) = 0;
    virtual void Show() = 0;
    virtual int  PumpOne() = 0;      // returns false when the loop must exit
    virtual void RequestRedraw() = 0;
    virtual Size ClientSize() const = 0;

    // ---- caption / status / sidebar (ShellCore renders these) ----
    virtual void SetCaptionBadge(const std::wstring& text, bool ok) = 0;
    virtual void SetCaptionSubtitle(const std::wstring& text) = 0;
    virtual void SetStatus(const std::wstring& text, bool ok) = 0;
    virtual void SetStatusExtra(const std::wstring& text) = 0;
    virtual void SetNavBadge(int id, const std::wstring& text, bool ok) = 0;

    // ---- events injected by the backend, consumed by core ----
    virtual void OnMouseMove(std::function<void(Point)>) = 0;
    virtual void OnMouseDown(std::function<void(Point, int button)>) = 0;
    virtual void OnMouseUp(std::function<void(Point, int button)>) = 0;
    virtual void OnWheel(std::function<void(Point, float delta)>) = 0;
    virtual void OnKeyDown(std::function<void(int vk, bool ctrl, bool shift)>) = 0;
    virtual void OnResized(std::function<void(Rect content)>) = 0;

    // ---- caption buttons, decoded by the backend ----
    virtual void OnMinimize(std::function<void()>) = 0;
    virtual void OnMaximizeRestore(std::function<void()>) = 0;
    virtual void OnClose(std::function<void()>) = 0;
};

} // namespace ui
} // namespace air
