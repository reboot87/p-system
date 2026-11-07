// ============================================================
// AirUI - PaintBackend.cpp
// Runtime painter backend registry. Platform-free by construction.
// ============================================================
#include "airui/PaintBackend.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace air {
namespace ui {

PainterRegistry& PainterRegistry::Instance()
{
    static PainterRegistry instance;
    return instance;
}

IPainterProvider* PainterRegistry::Find(const std::string& id) const
{
    for (const auto& p : m_providers)
        if (p && p->Id() == id) return p.get();
    return nullptr;
}

void PainterRegistry::Register(std::unique_ptr<IPainterProvider> provider)
{
    if (!provider) return;
    const std::string id = provider->Id();
    for (auto& p : m_providers) {
        if (p && p->Id() == id) {
            p = std::move(provider);
            return;
        }
    }
    m_providers.push_back(std::move(provider));
}

std::vector<std::string> PainterRegistry::Backends() const
{
    std::vector<std::string> ids;
    ids.reserve(m_providers.size());
    for (const auto& p : m_providers)
        if (p) ids.push_back(p->Id());
    return ids;
}

bool PainterRegistry::Available(const std::string& id) const
{
    const IPainterProvider* p = Find(NormalizeBackend(id));
    return p && p->Available();
}

void PainterRegistry::SetDefault(const std::string& id)
{
    m_default = NormalizeBackend(id);
    if (m_default.empty()) m_default = "gdi";
}

std::unique_ptr<IPainter> PainterRegistry::Begin(const std::string& choice,
                                                 const PaintTarget& target)
{
    std::string want = NormalizeBackend(choice);
    if (want.empty() || want == "auto") want = m_default;
    if (want.empty() || want == "auto") want = "gdi";

    auto attempt = [&](const std::string& id) -> std::unique_ptr<IPainter> {
        IPainterProvider* provider = Find(id);
        if (!provider) {
            m_error = "backend '" + id + "' is not registered";
            return nullptr;
        }
        if (!provider->Available()) {
            m_error = "backend '" + id + "' is unavailable";
            return nullptr;
        }
        std::unique_ptr<IPainter> painter = provider->Begin(target);
        if (!painter) m_error = "backend '" + id + "' failed to open a frame";
        return painter;
    };

    std::unique_ptr<IPainter> painter = attempt(want);
    if (!painter && want != "gdi") {
        const std::string reason = m_error;
        std::unique_ptr<IPainter> fallback = attempt("gdi");
        if (fallback) {
            m_error = reason + "; fell back to gdi";
            return fallback;
        }
    }
    if (painter) m_error.clear();
    return painter;
}

void PainterRegistry::Reset()
{
    m_providers.clear();
    m_default = "gdi";
    m_error.clear();
}

std::string NormalizeBackend(const std::string& name)
{
    std::string::size_type first = 0;
    while (first < name.size() &&
           std::isspace(static_cast<unsigned char>(name[first]))) ++first;
    std::string::size_type last = name.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(name[last - 1]))) --last;

    std::string out = name.substr(first, last - first);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (out.empty()) return "auto";
    return out;
}

std::string ResolvePaintBackend(const std::string& requested,
                                const std::string& environment,
                                bool legacySkiaPreference)
{
    std::string choice = NormalizeBackend(requested);
    if (choice == "auto") choice = NormalizeBackend(environment);
    if (choice == "auto") return legacySkiaPreference ? "skia" : "auto";
    return choice;
}

std::string EnvironmentPaintBackend()
{
    const char* value = std::getenv("AIRUI_PAINT_BACKEND");
    return value ? NormalizeBackend(value) : std::string("auto");
}

std::string NextPaintBackend(const std::string& current,
                             const std::vector<std::string>& available)
{
    std::vector<std::string> ids;
    for (const std::string& id : available) {
        const std::string norm = NormalizeBackend(id);
        if (norm.empty() || norm == "auto") continue;
        if (std::find(ids.begin(), ids.end(), norm) != ids.end()) continue;
        ids.push_back(norm);
    }
    if (ids.empty()) return "gdi";

    // gdi is the always-present fallback and the natural start of a cycle.
    auto gdi = std::find(ids.begin(), ids.end(), "gdi");
    if (gdi != ids.end() && gdi != ids.begin()) {
        std::string first = *gdi;
        ids.erase(gdi);
        ids.insert(ids.begin(), std::move(first));
    }

    std::string cur = NormalizeBackend(current);
    if (cur.empty() || cur == "auto") cur = "gdi";
    for (size_t i = 0; i < ids.size(); ++i)
        if (ids[i] == cur) return ids[(i + 1) % ids.size()];
    return ids.front();
}

} // namespace ui
} // namespace air
