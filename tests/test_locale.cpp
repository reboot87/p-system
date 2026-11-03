#include <iostream>
#include <fstream>
#include "airui/Accessibility.h"

using namespace air::ui::i18n;

int main() {
    // Create a temporary locale JSON file
    const wchar_t* path = L"F:/project/github/p-system/test_locale.json";
    {
        std::wofstream f(path);
        f << L"{\n";
        f << L"  \"code\": \"en-US\",\n";
        f << L"  \"name\": \"English\",\n";
        f << L"  \"dateFormat\": \"MM/dd/yyyy\",\n";
        f << L"  \"strings\": {\n";
        f << L"    \"hello\": \"Hello\",\n";
        f << L"    \"goodbye\": \"Goodbye\"\n";
        f << L"  }\n";
        f << L"}\n";
    }
    LocalizationManager& lm = LocalizationManager::Instance();
    if (!lm.LoadLocale(path)) {
        std::wcerr << L"LoadLocale failed\n";
        return 1;
    }
    lm.SetLocale(L"en-US");
    std::wstring hello = lm.Tr(L"hello");
    std::wstring missing = lm.Tr(L"missing_key");
    if (hello != L"Hello") {
        std::wcerr << L"Tr hello mismatch: " << hello << L"\n";
        return 1;
    }
    if (missing != L"missing_key") {
        std::wcerr << L"Tr missing key fallback failed\n";
        return 1;
    }
    const Locale* loc = lm.Current();
    if (!loc || loc->code != L"en-US") {
        std::wcerr << L"Current locale incorrect\n";
        return 1;
    }
    std::wcout << L"Locale code: " << loc->code << L", name: " << loc->name << L"\n";
    std::wcout << L"Tr(hello) = " << hello << L"\n";
    std::wcout << L"PASS\n";
    return 0;
}
