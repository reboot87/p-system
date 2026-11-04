#include "airui/TestShell.h"
#include <windows.h>
#include <fstream>
#include <sstream>

namespace air {
namespace ui {

TestShell::TestShell()
{
}

TestShell::~TestShell()
{
    DestroyMemDC();
}

bool TestShell::CreateVirtual(int width, int height, const ShellOptions& options)
{
    m_opt = options;
    m_dpi = 96;
    m_client = {0,0,width,height};
    m_content = {0,0,width,height};
    m_side = {0,0,0,0};
    m_status = {0,0,0,0};
    m_caption = {0,0,width,0};
    DestroyMemDC();
    m_memDC = CreateMemDC(width, height);
    return m_memDC != nullptr;
}

void TestShell::AddPage(int id, std::unique_ptr<IPage> page)
{
    PageSlot slot;
    slot.id = id;
    slot.page = std::move(page);
    m_pages.push_back(std::move(slot));
}

bool TestShell::SwitchPage(int id)
{
    for (auto& s : m_pages) {
        if (s.id == id) {
            m_pageId = id;
            return true;
        }
    }
    return false;
}

IPage* TestShell::CurrentPage() const
{
    for (auto& s : m_pages) {
        if (s.id == m_pageId) return s.page.get();
    }
    return nullptr;
}

void TestShell::PaintFrame()
{
    m_drawCalls.clear();
    Canvas::SetRecorder(&m_drawCalls);
    IPage* page = CurrentPage();
    if (page && m_memDC) {
        page->OnLayout(m_content);
        PaintPage(page, m_memDC);
    }
    Canvas::ClearRecorder();
}

void TestShell::SimulateMouseMove(int x, int y)
{
    IPage* page = CurrentPage();
    if (page) page->OnMouseMove({x,y});
}

void TestShell::SimulateMouseDown(int x, int y)
{
    IPage* page = CurrentPage();
    if (page) page->OnMouseDown({x,y});
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
    if (index >= m_drawCalls.size()) return false;
    return m_drawCalls[index].Matches(expected);
}

bool TestShell::AssertDrawCallSequence(const std::vector<DrawCall>& expected) const
{
    if (expected.size() != m_drawCalls.size()) return false;
    for (size_t i = 0; i < expected.size(); ++i) {
        if (!m_drawCalls[i].Matches(expected[i])) return false;
    }
    return true;
}

size_t TestShell::CountDrawCalls(DrawCall::Type type) const
{
    size_t cnt = 0;
    for (const auto& d : m_drawCalls) if (d.type == type) ++cnt;
    return cnt;
}

bool TestShell::ExportDrawCallsToFile(const std::wstring& path) const
{
    std::wofstream file(path);
    if (!file) return false;
    file << L"[\n";
    for (size_t i = 0; i < m_drawCalls.size(); ++i) {
        const auto& dc = m_drawCalls[i];
        auto hex = [](COLORREF c)->std::wstring {
            wchar_t buf[16];
            swprintf_s(buf, L"%06X", c & 0xFFFFFF);
            return std::wstring(L"#") + buf;
        };
        file << L"  {\n";
        file << L"    \"type\": " << static_cast<int>(dc.type) << L",\n";
        file << L"    \"rect\": [" << dc.rect.left << L"," << dc.rect.top << L"," << dc.rect.right << L"," << dc.rect.bottom << L"],\n";
        file << L"    \"color\": \"" << hex(dc.color) << L"\",\n";
        file << L"    \"color2\": \"" << hex(dc.color2) << L"\",\n";
        file << L"    \"color3\": \"" << hex(dc.color3) << L"\",\n";
        file << L"    \"radius\": " << dc.radius << L",\n";
        file << L"    \"width\": " << dc.width << L",\n";
        file << L"    \"text\": \"" << dc.text << L"\"\n";
        file << L"  }";
        if (i + 1 < m_drawCalls.size()) file << L",";
        file << L"\n";
    }
    file << L"]\n";
    return true;
}

void TestShell::OnSize(int w, int h)
{
    m_client.right = w;
    m_client.bottom = h;
    m_content.right = w;
    m_content.bottom = h;
    DestroyMemDC();
    m_memDC = CreateMemDC(w, h);
}

HDC TestShell::CreateMemDC(int w, int h)
{
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return nullptr;
    HBITMAP bmp = CreateCompatibleBitmap(dc, w, h);
    if (!bmp) {
        DeleteDC(dc);
        return nullptr;
    }
    SelectObject(dc, bmp);
    m_memBmp = bmp;
    return dc;
}

void TestShell::DestroyMemDC()
{
    if (m_memDC) {
        if (m_memBmp) {
            DeleteObject(m_memBmp);
            m_memBmp = nullptr;
        }
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
}

void TestShell::PaintPage(IPage* page, HDC dc)
{
    if (!page || !dc) return;
    page->OnPaint(dc, m_content);
}

} // namespace ui
} // namespace air

