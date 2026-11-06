#include <iostream>
#include <windows.h>
#include "airui/ThemeManager.h"
#include "airui/ThemeJson.h"
#include "airui/Theme.h"

// 输出文件写到系统临时目录，避免依赖任何机器特定的绝对路径
static std::wstring TempFilePath(const wchar_t* name)
{
    wchar_t tmp[MAX_PATH] = L"";
    GetTempPathW(MAX_PATH, tmp);
    return std::wstring(tmp) + name;
}

int main()
{
    auto& tm = air::ui::ThemeManager::Instance();
    std::wstring path = TempFilePath(L"test_theme.json");
    // Capture baseline values
    COLORREF primaryBefore = air::ui::Pal().primary;
    int unitBefore = air::ui::Mx().unit;
    std::wstring familyBefore = air::ui::Fonts::Default().Family();
    std::wstring iconFamilyBefore = air::ui::Fonts::Default().IconFamily();

    if (!tm.ExportToFile(path)) {
        std::wcerr << L"Export failed\n";
        return 1;
    }
    std::wcout << L"Exported theme to " << path << L"\n";

    // Mutate theme slightly to ensure load overrides
    air::ui::SetPalette(air::ui::DarkPalette());
    auto m = air::ui::Mx();
    m.unit = 999;
    air::ui::SetMetrics(m);
    air::ui::Fonts::Default().SetFamily(L"TestFamily");
    air::ui::Fonts::Default().SetIconFamily(L"TestIcon");

    if (!tm.LoadFromFile(path)) {
        std::wcerr << L"Load failed\n";
        return 1;
    }
    std::wcout << L"Loaded theme, name: " << tm.CurrentName() << L"\n";

    COLORREF primaryAfter = air::ui::Pal().primary;
    int unitAfter = air::ui::Mx().unit;
    std::wstring familyAfter = air::ui::Fonts::Default().Family();
    std::wstring iconFamilyAfter = air::ui::Fonts::Default().IconFamily();

    if (primaryBefore != primaryAfter) {
        std::wcerr << L"Primary color mismatch\n";
        return 1;
    }
    if (unitBefore != unitAfter) {
        std::wcerr << L"Metrics unit mismatch: " << unitBefore << L" vs " << unitAfter << L"\n";
        return 1;
    }
    if (familyBefore != familyAfter) {
        std::wcerr << L"Font family mismatch\n";
        return 1;
    }
    if (iconFamilyBefore != iconFamilyAfter) {
        std::wcerr << L"Icon font family mismatch\n";
        return 1;
    }

    std::wcout << L"Field verification PASS\n";
    std::wcout << L"PASS\n";
    return 0;
}
