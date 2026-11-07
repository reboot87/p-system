// ============================================================
// AirUI - PaintProviders.cpp
// Win32 painter backend providers: GDI (always), Skia (linked or
// dynamically loaded). No Skia header is included here, so this
// translation unit builds in the default CMake + VS2017 toolchain
// without any Skia SDK.
// ============================================================
#include "airui/PaintProviders.h"

#include "airui/GdiPainter.h"
#include "airui/PaintBackend.h"
#include "airui/SkiaPluginAbi.h"
#ifdef AIRUI_HAVE_SKIA
#include "airui/SkiaSurface.h"
#endif

#include <windows.h>

#include <memory>
#include <string>

namespace air {
namespace ui {
namespace {

// ------------------------------------------------------------
// Plugin C ABI, resolved once per process.
// ------------------------------------------------------------
struct PluginApi
{
    HMODULE               module   = nullptr;
    AiruiSkiaAbiFn        abi      = nullptr;
    AiruiSkiaCreateFn     create   = nullptr;
    AiruiSkiaDestroyFn    destroy  = nullptr;
    AiruiSkiaPixelsFn     pixels   = nullptr;
    AiruiSkiaRowBytesFn   rowBytes = nullptr;
    bool                  ready    = false;
    bool                  attempted = false;
};

PluginApi& Api()
{
    static PluginApi api;
    return api;
}

template <typename Fn>
Fn Resolve(HMODULE module, const char* name)
{
    // GetProcAddress returns FARPROC; the cast is the documented way
    // to turn it into the typed entry point from SkiaPluginAbi.h.
    return reinterpret_cast<Fn>(GetProcAddress(module, name));
}

bool LoadPlugin()
{
    PluginApi& a = Api();
    if (a.ready) return true;
    if (a.attempted) return false;   // SkiaPluginReload() allows a retry
    a.attempted = true;

    wchar_t custom[MAX_PATH] = {};
    const DWORD len = GetEnvironmentVariableW(AIRUI_SKIA_PLUGIN_ENV,
                                              custom, MAX_PATH);
    const wchar_t* path =
        (len > 0 && len < MAX_PATH) ? custom : AIRUI_SKIA_PLUGIN_DLL;

    HMODULE module = LoadLibraryW(path);
    if (!module) return false;
    a.module   = module;
    a.abi      = Resolve<AiruiSkiaAbiFn>(module, AIRUI_SKIA_FN_ABI);
    a.create   = Resolve<AiruiSkiaCreateFn>(module, AIRUI_SKIA_FN_CREATE);
    a.destroy  = Resolve<AiruiSkiaDestroyFn>(module, AIRUI_SKIA_FN_DESTROY);
    a.pixels   = Resolve<AiruiSkiaPixelsFn>(module, AIRUI_SKIA_FN_PIXELS);
    a.rowBytes = Resolve<AiruiSkiaRowBytesFn>(module, AIRUI_SKIA_FN_ROW_BYTES);

    if (!a.abi || !a.create || !a.destroy) return false;
    if (a.abi() != AIRUI_SKIA_PLUGIN_ABI) return false;
    a.ready = true;
    return true;
}

// ------------------------------------------------------------
// A painter whose lifetime belongs to the plugin. Every call is
// forwarded through the IPainter vtable; destruction goes back to
// the plugin so `delete` never crosses a module boundary.
// ------------------------------------------------------------
class PluginPainter : public IPainter
{
public:
    PluginPainter(IPainter* raw, AiruiSkiaDestroyFn destroy)
        : m_raw(raw), m_destroy(destroy) {}

    ~PluginPainter() override
    {
        if (m_destroy) m_destroy(m_raw);
    }

    void FillRect(const Rect& r, Color c) override
    { m_raw->FillRect(r, c); }
    void FillRectAlpha(const Rect& r, Color c, uint8_t a) override
    { m_raw->FillRectAlpha(r, c, a); }
    void FillRound(const Rect& r, int radius, Color c) override
    { m_raw->FillRound(r, radius, c); }
    void StrokeRect(const Rect& r, Color c, int width) override
    { m_raw->StrokeRect(r, c, width); }
    void StrokeRound(const Rect& r, int radius, Color c, int width) override
    { m_raw->StrokeRound(r, radius, c, width); }
    void StrokeDashed(const Rect& r, Color c, int width, int dash) override
    { m_raw->StrokeDashed(r, c, width, dash); }
    void Text(const Rect& r, const std::wstring& s,
              FontRole role, Color c, TextFormat fmt) override
    { m_raw->Text(r, s, role, c, fmt); }
    void TextBlock(const Rect& r, const std::wstring& s,
                   FontRole role, Color c, int lines) override
    { m_raw->TextBlock(r, s, role, c, lines); }
    int MeasureText(const std::wstring& s, FontRole role) override
    { return m_raw->MeasureText(s, role); }
    void PushClip(const Rect& r) override  { m_raw->PushClip(r); }
    void PopClip() override                { m_raw->PopClip(); }
    void Translate(Point offset) override { m_raw->Translate(offset); }

private:
    IPainter*          m_raw     = nullptr;
    AiruiSkiaDestroyFn m_destroy = nullptr;
};

class GdiProvider : public IPainterProvider
{
public:
    std::string Id() const override { return "gdi"; }
    bool Available() const override { return true; }
    std::unique_ptr<IPainter> Begin(const PaintTarget& target) override
    {
        // A null surface yields a no-op painter (headless fallback);
        // GdiPainter guards every call on a null device context.
        return std::unique_ptr<IPainter>(
            new GdiPainter(static_cast<HDC>(target.surface)));
    }
};

class SkiaPluginProvider : public IPainterProvider
{
public:
    std::string Id() const override { return "skia"; }
    bool Available() const override { return LoadPlugin(); }
    std::unique_ptr<IPainter> Begin(const PaintTarget& target) override
    {
        if (!LoadPlugin()) return nullptr;
        PluginApi& a = Api();
        const Rect& b = target.bounds;
        void* handle = a.create(target.surface, b.left, b.top, b.right, b.bottom);
        if (!handle) return nullptr;
        return std::unique_ptr<IPainter>(
            new PluginPainter(reinterpret_cast<IPainter*>(handle), a.destroy));
    }
};

#ifdef AIRUI_HAVE_SKIA
// Built when a Skia SDK was present at configure time. It registers
// under the same id, so application code is identical either way.
class SkiaStaticProvider : public IPainterProvider
{
public:
    std::string Id() const override { return "skia"; }
    bool Available() const override { return true; }
    std::unique_ptr<IPainter> Begin(const PaintTarget& target) override
    {
        if (target.surface)
            return CreateSkiaBackend(static_cast<HDC>(target.surface),
                                     target.bounds);
        return std::unique_ptr<IPainter>(
            new SkiaFrame(target.bounds.Width(), target.bounds.Height()));
    }
};
#endif

} // namespace

void RegisterPlatformPaintProviders()
{
    PainterRegistry& registry = PainterRegistry::Instance();
    registry.Register(std::make_unique<GdiProvider>());
#ifdef AIRUI_HAVE_SKIA
    registry.Register(std::make_unique<SkiaStaticProvider>());
#else
    RegisterSkiaPluginProvider();
#endif
}

void RegisterSkiaPluginProvider()
{
    PainterRegistry::Instance().Register(std::make_unique<SkiaPluginProvider>());
}

bool SkiaPluginLoaded() { return Api().ready; }

void SkiaPluginReload()
{
    // Deliberately keep the module loaded: a painter created earlier
    // may still be alive and its vtable lives in that module.
    Api().ready = false;
    Api().attempted = false;
}

} // namespace ui
} // namespace air
