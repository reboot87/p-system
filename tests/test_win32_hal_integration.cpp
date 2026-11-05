#include "airui/platform/win32/airui/Win32Window.h"
#include "airui/platform/win32/airui/GdiPainter.h"
#include "airui/ShellCore.h"
#include <cstdio>

int main() {
    // Just verify headers compile and ShellCore can be constructed
    air::ui::ShellCore shell;
    air::ui::ShellCore::Rect client{0,0,800,600};
    // Layout works without HWND
    shell.Layout(client);
    printf("ShellCore layout ok\n");
    return 0;
}
