#include <iostream>
#include <fstream>
#include <windows.h>
#include "airui/TestShell.h"
#include "airui/Page.h"

// 输出文件写到系统临时目录，避免依赖任何机器特定的绝对路径
static std::wstring TempFilePath(const wchar_t* name)
{
    wchar_t tmp[MAX_PATH] = L"";
    GetTempPathW(MAX_PATH, tmp);
    return std::wstring(tmp) + name;
}

using namespace air::ui;

class SimplePage : public IPage {
public:
    const wchar_t* Title() const override { return L"Simple"; }
    void OnPaint(HDC dc, const RECT& content) override {
        Canvas g(dc, Fonts::Default());
        RECT rc = {10, 10, 110, 110};
        g.Fill(rc, RGB(255,0,0));
        g.FillRound({120,10,220,110}, RGB(0,255,0), 8);
    }
};

int main() {
    TestShell shell;
    if (!shell.CreateVirtual(800, 600)) {
        std::wcerr << L"CreateVirtual failed\n";
        return 1;
    }
    shell.AddPage(1, std::make_unique<SimplePage>());
    if (!shell.SwitchPage(1)) {
        std::wcerr << L"SwitchPage failed\n";
        return 1;
    }
    shell.PaintFrame();
    const auto& calls = shell.CaptureDrawCalls();
    std::wcout << L"Captured draw calls: " << calls.size() << L"\n";
    if (calls.empty()) {
        std::wcerr << L"No draw calls captured\n";
        return 1;
    }
    std::wstring path = TempFilePath(L"test_draw_calls.json");
    if (!shell.ExportDrawCallsToFile(path)) {
        std::wcerr << L"Export failed\n";
        return 1;
    }
    std::wcout << L"Exported to " << path << L"\n";
    // Simple regression assertion: first call is Fill
    if (calls[0].type != DrawCall::Fill) {
        std::wcerr << L"First draw call type mismatch\n";
        return 1;
    }
    std::wcout << L"PASS\n";
    return 0;
}
