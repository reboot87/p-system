#include <iostream>
#include "airui/ThemeManager.h"
#include "airui/ThemeJson.h"

int main()
{
    auto& tm = air::ui::ThemeManager::Instance();
    std::wstring path = L"F:/project/github/p-system/test_theme.json";
    if (!tm.ExportToFile(path)) {
        std::wcerr << L"Export failed\n";
        return 1;
    }
    std::wcout << L"Exported theme to " << path << L"\n";
    if (!tm.LoadFromFile(path)) {
        std::wcerr << L"Load failed\n";
        return 1;
    }
    std::wcout << L"Loaded theme, name: " << tm.CurrentName() << L"\n";
    std::wcout << L"PASS\n";
    return 0;
}
