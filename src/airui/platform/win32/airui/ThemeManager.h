#pragma once
#include <string>
#include "airui/ThemeJson.h"

namespace air {
namespace ui {

// 封装 ThemeJson 加载/保存/应用
class ThemeManager
{
public:
    static ThemeManager& Instance();

    // 从文件加载主题并应用
    bool LoadFromFile(const std::wstring& path);
    // 导出当前主题到文件
    bool ExportToFile(const std::wstring& path) const;
    // 获取当前主题名称
    const std::wstring& CurrentName() const { return m_name; }

private:
    ThemeManager() = default;
    std::wstring m_name = L"Default";
};

} // namespace ui
} // namespace air
