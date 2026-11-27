#include "ThemeManager.h"
#include "ThemeJson.h"

namespace air {
namespace ui {

ThemeManager& ThemeManager::Instance()
{
    static ThemeManager inst;
    return inst;
}

bool ThemeManager::LoadFromFile(const std::wstring& path)
{
    if (!LoadTheme(path)) return false;
    m_name = LastLoadedThemeName();
    return true;
}

bool ThemeManager::ExportToFile(const std::wstring& path) const
{
    return ExportTheme(path);
}

} // namespace ui
} // namespace air
