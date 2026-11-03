#include "airui/TestShell.h"

namespace air {
namespace ui {

TestShell::TestShell()
{
}

TestShell::~TestShell()
{
}

bool TestShell::CreateVirtual(int width, int height, const ShellOptions& options)
{
    (void)width; (void)height; (void)options;
    return true;
}

void TestShell::AddPage(int id, std::unique_ptr<IPage> page)
{
    (void)id; (void)page;
}

bool TestShell::SwitchPage(int id)
{
    (void)id;
    return false;
}

IPage* TestShell::CurrentPage() const
{
    return nullptr;
}

void TestShell::PaintFrame()
{
}

void TestShell::SimulateMouseMove(int x, int y)
{
    (void)x; (void)y;
}

void TestShell::SimulateMouseDown(int x, int y)
{
    (void)x; (void)y;
}

void TestShell::SimulateMouseUp(int x, int y)
{
    (void)x; (void)y;
}

void TestShell::SimulateKeyDown(UINT vk, bool ctrl, bool shift)
{
    (void)vk; (void)ctrl; (void)shift;
}

void TestShell::SimulateChar(wchar_t ch)
{
    (void)ch;
}

bool TestShell::AssertDrawCall(size_t index, const DrawCall& expected) const
{
    (void)index; (void)expected;
    return false;
}

bool TestShell::AssertDrawCallSequence(const std::vector<DrawCall>& expected) const
{
    (void)expected;
    return false;
}

size_t TestShell::CountDrawCalls(DrawCall::Type type) const
{
    (void)type;
    return 0;
}

void TestShell::OnSize(int w, int h)
{
    (void)w; (void)h;
}

HDC TestShell::CreateMemDC(int w, int h)
{
    (void)w; (void)h;
    return nullptr;
}

void TestShell::DestroyMemDC()
{
}

void TestShell::PaintPage(IPage* page, HDC dc)
{
    (void)page; (void)dc;
}

} // namespace ui
} // namespace air
