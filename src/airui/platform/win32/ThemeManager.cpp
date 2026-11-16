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
    // TODO(theme): 占位——此处硬编码 "Loaded"，未从 ThemeJson 读取真实主题名称。
    m_name = L"Loaded";
    return true;
}

bool ThemeManager::ExportToFile(const std::wstring& path) const
{
    return ExportTheme(path);
}

} // namespace ui
} // namespace air
