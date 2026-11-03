#pragma once
// ============================================================
// AirUI — ThemeJson.h
// JSON 主题加载/导出（设计令牌持久化）
//
// 用法：
//     air::ui::LoadTheme(L"theme.json");        // 运行时换肤
//     air::ui::ExportTheme(L"my-theme.json");   // 导出当前主题供设计师调整
//
// JSON 结构：
// {
//   "name": "My Theme",
//   "colors": {
//     "primary": "#1677FF",
//     "primaryHover": "#4096FF",
//     ...
//   },
//   "metrics": {
//     "unit": 8,
//     "radiusMd": 8,
//     "fontBody": 14,
//     ...
//   },
//   "fonts": {
//     "family": "Microsoft YaHei UI",
//     "iconFamily": "Segoe MDL2 Assets"
//   }
// }
// ============================================================
#include <windows.h>
#include <string>
#include <map>
#include "Theme.h"
#include "Fonts.h"

namespace air {
namespace ui {

struct ThemeJson
{
    std::wstring name;
    Palette colors;
    Metrics metrics;
    std::wstring fontFamily;
    std::wstring iconFontFamily;
};

// 加载主题（失败返回 false，保持当前主题不变）
bool LoadTheme(const std::wstring& jsonPath);

// 导出当前主题
bool ExportTheme(const std::wstring& jsonPath);

// 解析十六进制颜色字符串（支持 #RRGGBB / #AARRGGBB / rgb() / rgba()）
COLORREF ParseColor(const std::wstring& str, COLORREF fallback = 0);

// 内部：应用 ThemeJson 到全局 Palette/Metrics/Fonts
void ApplyThemeJson(const ThemeJson& theme);

// 获取默认主题 JSON（用于导出模板）
ThemeJson DefaultThemeJson();

} // namespace ui
} // namespace air