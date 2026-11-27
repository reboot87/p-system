#pragma once
// ============================================================
// AirUI - hal.h
// Hardware abstraction layer. Phase 0 froze the drawing surface;
// Phase 1 completes the event surface below.
//
// Three layers:
//   core       - components, layout, theme, state machines
//   hal        - the interfaces in this file
//   platform   - native window + painters, later Skia
//
// Everything in core talks to the outside world only through
// these interfaces, so core links and compiles on every target
// without a single platform header.
// ============================================================
#include "airui/types.h"

#include <cstddef>
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
// Backends read their OS tick source; only monotonicity is required.
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

    // Optional alpha fill, used by overlay scrims. Backends without
    // blending fall back to an opaque FillRect, so a scrim still
    // renders on every target. alpha is 0..255.
    virtual void FillRectAlpha(const Rect& r, Color c, uint8_t alpha)
    {
        (void)alpha;
        FillRect(r, c);
    }

    virtual void FillRound(const Rect& r, int radius, Color c) = 0;
    virtual void StrokeRect(const Rect& r, Color c, int width) = 0;
    virtual void StrokeRound(const Rect& r, int radius, Color c, int width) = 0;
    virtual void StrokeDashed(const Rect& r, Color c, int width, int dash) = 0;

    // Text takes a wide-character buffer plus its length (no std::wstring
    // crosses the IPainter vtable, which the Skia plugin implements across
    // a module/CRT boundary). Implementations build their own std::wstring
    // from (s, len) when they need one.
    virtual void Text(const Rect& r, const wchar_t* s, size_t len,
                      FontRole role, Color c, TextFormat fmt) = 0;
    virtual void TextBlock(const Rect& r, const wchar_t* s, size_t len,
                           FontRole role, Color c, int maxLines) = 0;

    virtual int  MeasureText(const wchar_t* s, size_t len, FontRole role) = 0;

    virtual void PushClip(const Rect& r) = 0;
    virtual void PopClip() = 0;

    virtual void Translate(Point offset) = 0;

    // ---- line / dot / track shorthands -------------------------
    // Appended after Translate on purpose: an existing backend (and the
    // cross-module vtable, see AIRUI_PAINTER_IFACE_REV) keeps its slot
    // order. Every one of them has a default that decomposes into the
    // rect primitives, which is what the frame router emits today, so a
    // backend that does not override them still renders the same pixels.
    virtual void HLine(int x1, int x2, int y, Color c)
    {
        FillRect(Rect(x1, y, x2, y + 1), c);
    }

    // A 1px dashed rule: 4px ink, 3px gap (the native dash rhythm).
    virtual void HLineDashed(int x1, int x2, int y, Color c)
    {
        for (int x = x1; x < x2; x += 7) {
            const int right = (x + 4 < x2) ? x + 4 : x2;
            FillRect(Rect(x, y, right, y + 1), c);
        }
    }

    virtual void VLine(int x, int y1, int y2, Color c)
    {
        FillRect(Rect(x, y1, x + 1, y2), c);
    }

    virtual void Dot(int cx, int cy, int diameter, Color c)
    {
        const int r = diameter / 2;
        FillRound(Rect(cx - r, cy - r, cx - r + diameter, cy - r + diameter),
                  r > 0 ? r : 1, c);
    }

    // Track + proportional fill. The warn/hot thresholds are part of the
    // shared visual language, so they live here instead of being copied
    // into every backend.
    virtual void ProgressBar(const Rect& r, int pct, Color track,
                             Color normal, Color warn, Color hot)
    {
        if (r.right <= r.left || r.bottom <= r.top) return;
        FillRect(r, track);
        if (pct <= 0) return;
        if (pct > 100) pct = 100;
        const Color c = (pct > 70) ? hot : (pct > 45) ? warn : normal;
        Rect fill = r;
        fill.right = r.left + (int)((r.right - r.left) * pct / 100.0);
        if (fill.right > r.right) fill.right = r.right;
        FillRect(fill, c);
    }

    // Line height in device pixels for a role. The default is the ink
    // height of a representative string; backends with real font metrics
    // override it.
    virtual int LineHeight(FontRole role)
    {
        return MeasureText(L"Ag", 2, role);
    }
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

    // ---- Phase 1 events: mouse enter/leave, text input, focus, DPI ----
    // A backend may not be able to deliver every one of these (e.g. a
    // headless test backend has no DPI changes), so defaults are no-ops
    // rather than pure virtual. Adding an event never breaks an existing
    // backend; Win32Window implements all of them.
    virtual void OnMouseEnter(std::function<void()>) {}
    virtual void OnMouseLeave(std::function<void()>) {}
    virtual void OnChar(std::function<void(wchar_t)>) {}
    virtual void OnFocus(std::function<void()>) {}
    virtual void OnBlur(std::function<void()>) {}
    virtual void OnDpiChanged(std::function<void(int dpi)>) {}

    // ---- caption buttons, decoded by the backend ----
    virtual void OnMinimize(std::function<void()>) = 0;
    virtual void OnMaximizeRestore(std::function<void()>) = 0;
    virtual void OnClose(std::function<void()>) = 0;
};

} // namespace ui
} // namespace air
