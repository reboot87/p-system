#include <windows.h>
#include "airui/ShellCore.h"
#include "airui/platform/win32/airui/GdiPainter.h"
#include "airui/Theme.h"
#include <cstdio>

int main() {
    air::ui::ShellCore shell;
    shell.SetCaption({L"Test", L"Sub", L"OK", true});
    shell.SetStatus({L"Ready", true, L""});
    air::ui::Rect client{0,0,800,600};
    shell.Layout(client);

    HDC hdc = CreateCompatibleDC(nullptr);
    air::ui::GdiPainter painter(hdc);
    const auto& pal = air::ui::Pal();
    const auto& mx = air::ui::Mx();
    shell.Paint(painter, pal, mx);
    printf("ShellCore painted with GdiPainter OK\n");
    DeleteDC(hdc);
    return 0;
}
