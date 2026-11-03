#include "airui/ThemeJson.h"
#include "airui/Theme.h"
#include "airui/Fonts.h"

namespace air {
namespace ui {

ThemeJson DefaultThemeJson()
{
    ThemeJson tj;
    tj.name = L"Default";
    tj.colors = LightPalette();
    tj.metrics = Metrics{};
    tj.fontFamily = L"Microsoft YaHei UI";
    tj.iconFontFamily = L"Segoe MDL2 Assets";
    return tj;
}

void ApplyThemeJson(const ThemeJson& theme)
{
    SetPalette(theme.colors);
    SetMetrics(theme.metrics);
    Fonts::Default().SetFamily(theme.fontFamily);
    Fonts::Default().SetIconFamily(theme.iconFontFamily);
    Fonts::Default().Create();
}

COLORREF ParseColor(const std::wstring& str, COLORREF fallback)
{
    (void)str;
    return fallback;
}

bool LoadTheme(const std::wstring& jsonPath)
{
    (void)jsonPath;
    // Stub: JSON parsing not implemented yet
    return false;
}

bool ExportTheme(const std::wstring& jsonPath)
{
    (void)jsonPath;
    // Stub: export not implemented
    return false;
}

} // namespace ui
} // namespace air
