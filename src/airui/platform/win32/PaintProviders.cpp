// ============================================================
// AirUI - PaintProviders.cpp
// Win32 painter backend providers: GDI (always), Skia (linked or
// dynamically loaded). No Skia header is included here, so this
// translation unit builds in the default CMake + VS2017 toolchain
// without any Skia SDK.
// ============================================================
#include "PaintProviders.h"

#include "internal/GdiPainter.h"
#include "airui/PaintBackend.h"
#include "airui/SkiaPluginAbi.h"
#ifdef AIRUI_HAVE_SKIA
#include "airui/SkiaSurface.h"
#endif

#include <windows.h>

#include <memory>
#include <string>
#include <wchar.h>

namespace air {
namespace ui {
namespace {

// ------------------------------------------------------------
// Plugin C ABI, resolved once per process.
// ------------------------------------------------------------
struct PluginApi
{
    HMODULE               module   = nullptr;
    AiruiSkiaDescribeFn   describe = nullptr;
    AiruiSkiaCreateFn     create   = nullptr;
    AiruiSkiaDestroyFn    destroy  = nullptr;
    AiruiSkiaPixelsFn     pixels   = nullptr;
    AiruiSkiaRowBytesFn   rowBytes = nullptr;
    const AiruiPluginDesc* desc    = nullptr;   // valid once LoadPlugin() succeeds
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
    const wchar_t* path = (len > 0 && len < MAX_PATH) ? custom : nullptr;

    // Resolve the plugin against the application's own directory, not the
    // bare DLL name. LoadLibraryW with a bare name depends on the standard
    // DLL search order (and the process CWD); when the app is launched by
    // the service or from a shortcut the installed airui-skia.dll next to
    // the exe may not be found, surfacing as "无插件" even though the file
    // is present. Building an absolute <exe-dir>\airui-skia.dll path makes
    // it load from wherever the app lives.
    wchar_t absPath[MAX_PATH] = {};
    wchar_t self[MAX_PATH] = {};
    if (!path) {
        if (GetModuleFileNameW(nullptr, self, MAX_PATH) > 0) {
            wchar_t* slash = wcsrchr(self, L'\\');
            if (slash) {
                *slash = 0;
                wsprintfW(absPath, L"%s\\%s", self, AIRUI_SKIA_PLUGIN_DLL);
                path = absPath;
            }
        }
    }
    if (!path) path = AIRUI_SKIA_PLUGIN_DLL;

    // Make the app's own directory part of the DLL search order so that the
    // plugin and ITS import dependencies (skia.dll, icu*.dll, harfbuzz*.dll,
    // ...) all resolve from the install dir, even when the process was
    // launched with a different CWD (service / shortcut). Without this the
    // absolute <exe-dir>\airui-skia.dll loads but its own imports fail to
    // resolve, so the plugin load still fails and Skia stays unavailable.
    // Best-effort: failure here only degrades the search path, never the
    // rest of plugin resolution.
    if (self[0]) {
        SetDllDirectoryW(self);
    }

    HMODULE module = LoadLibraryW(path);
    if (!module) {
        wchar_t why[128] = {};
        _snwprintf_s(why, _TRUNCATE,
                     L"airui-skia plugin load failed: err=%u path=%ls",
                     (unsigned)GetLastError(), path);
        OutputDebugStringW(why);
        return false;
    }
    a.module   = module;
    a.describe = Resolve<AiruiSkiaDescribeFn>(module, AIRUI_SKIA_FN_DESCRIBE);
    a.create   = Resolve<AiruiSkiaCreateFn>(module, AIRUI_SKIA_FN_CREATE);
    a.destroy  = Resolve<AiruiSkiaDestroyFn>(module, AIRUI_SKIA_FN_DESTROY);
    a.pixels   = Resolve<AiruiSkiaPixelsFn>(module, AIRUI_SKIA_FN_PIXELS);
    a.rowBytes = Resolve<AiruiSkiaRowBytesFn>(module, AIRUI_SKIA_FN_ROW_BYTES);

    if (!a.describe || !a.create || !a.destroy) {
        a.describe = nullptr; a.create = nullptr; a.destroy = nullptr;
        a.pixels = nullptr; a.rowBytes = nullptr;
        // No painter was ever created: the live vtable constraint does not
        // apply yet, so the failed module can be released.
        FreeLibrary(module);
        a.module = nullptr;
        return false;
    }

    const AiruiPluginDesc* desc = a.describe();
    if (!desc) {
        a.describe = nullptr; a.create = nullptr; a.destroy = nullptr;
        a.pixels = nullptr; a.rowBytes = nullptr;
        FreeLibrary(module);
        a.module = nullptr;
        return false;
    }

    // Negotiate the ABI: the C function shapes, the struct layout and the
    // cross-module IPainter vtable revision must ALL match, otherwise a
    // stale plugin would be loaded as compatible and crash opaquely.
    if (desc->abi != AIRUI_PLUGIN_ABI ||
        desc->size < sizeof(AiruiPluginDesc) ||
        desc->painterIfaceRev != AIRUI_PAINTER_IFACE_REV) {
        wchar_t why[160] = {};
        _snwprintf_s(why, _TRUNCATE,
                     L"airui-skia ABI mismatch: abi=%u want=%u size=%u want>=%u "
                     L"ifaceRev=%u want=%u caps=0x%x",
                     (unsigned)desc->abi, (unsigned)AIRUI_PLUGIN_ABI,
                     (unsigned)desc->size, (unsigned)sizeof(AiruiPluginDesc),
                     (unsigned)desc->painterIfaceRev,
                     (unsigned)AIRUI_PAINTER_IFACE_REV, (unsigned)desc->caps);
        OutputDebugStringW(why);
        a.describe = nullptr; a.create = nullptr; a.destroy = nullptr;
        a.pixels = nullptr; a.rowBytes = nullptr;
        // Still no live painter: safe to unload the stale module.
        FreeLibrary(module);
        a.module = nullptr;
        return false;
    }

    a.desc = desc;
    OutputDebugStringW(L"airui-skia plugin loaded");
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
    void Text(const Rect& r, const wchar_t* s, size_t len,
              FontRole role, Color c, TextFormat fmt) override
    { m_raw->Text(r, s, len, role, c, fmt); }
    void TextBlock(const Rect& r, const wchar_t* s, size_t len,
                   FontRole role, Color c, int lines) override
    { m_raw->TextBlock(r, s, len, role, c, lines); }
    int MeasureText(const wchar_t* s, size_t len, FontRole role) override
    { return m_raw->MeasureText(s, len, role); }
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

bool SkiaPluginSupportsWideText()
{
    static int cached = -1;
    if (cached >= 0) return cached != 0;
    cached = 0;

    // Explicit override wins: useful when a newer plugin is deployed or
    // when comparing host-rasterised text against backend text.
    wchar_t override[16] = {};
    const DWORD n = GetEnvironmentVariableW(L"AIRUI_SKIA_TEXT",
                                            override, 16);
    if (n > 0 && n < 16) {
        if (_wcsicmp(override, L"host") == 0) return false;
        if (_wcsicmp(override, L"backend") == 0) { cached = 1; return true; }
    }

    // Negotiated capability: the plugin advertises AIRUI_CAP_WIDE_TEXT in
    // its AiruiPluginDesc, replacing the old "draw a han and count opaque
    // pixels" probe. A plugin that predates caps negotiation simply does
    // not set the bit, and the host falls back to GDI text.
    PluginApi& a = Api();
    if (!LoadPlugin() || !a.desc) return false;
    cached = (a.desc->caps & AIRUI_CAP_WIDE_TEXT) ? 1 : 0;
    return cached != 0;
}

bool SkiaPluginSupportsSymbolFallback()
{
    static int cached = -1;
    if (cached >= 0) return cached != 0;

    // Negotiated capability: the plugin advertises that it resolves the
    // symbols the UI family lacks through GDI's own font-link chain.
    // A plugin that predates the bit simply does not set it, and the
    // caller falls back to GDI's own rendering for those glyphs.
    PluginApi& a = Api();
    if (!LoadPlugin() || !a.desc) return false;
    cached = (a.desc->caps & AIRUI_CAP_SYMBOL_FALLBACK) ? 1 : 0;
    return cached != 0;
}

} // namespace ui
} // namespace air
