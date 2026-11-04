#include "airui/ThemeManager.h"
#include "airui/ThemeJson.h"

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
    // 简化：从 ThemeJson 获取名称
    m_name = L"Loaded";
    return true;
}

bool ThemeManager::ExportToFile(const std::wstring& path) const
{
    return ExportTheme(path);
}

} // namespace ui
} // namespace air
