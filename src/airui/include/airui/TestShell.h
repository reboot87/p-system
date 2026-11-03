#pragma once
// ============================================================
// AirUI — TestShell.h
// 无头测试模式：不创建真实 HWND，用内存 DC 验证绘制逻辑
//
// 用法：
//     TestShell shell;
//     shell.CreateVirtual(1200, 800);
//     shell.AddPage(1, std::make_unique<MyPage>());
//     shell.SwitchPage(1);
//     
//     // 触发绘制
//     shell.PaintFrame();
//     
//     // 验证绘制调用
//     auto calls = shell.CaptureDrawCalls();
//     ASSERT_EQ(calls.size(), 5);
//     ASSERT_TRUE(calls[0].type == DrawCall::FillRound);
// ============================================================
#include <windows.h>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include "Shell.h"
#include "Canvas.h"

namespace air {
namespace ui {

// 绘制调用记录（用于断言）
struct DrawCall
{
    enum Type {
        None,
        Fill,
        FillRound,
        Stroke,
        StrokeDashed,
        Text,
        TextRow,
        TextRowRight,
        TextRowCenter,
        TextBlock,
        ProgressBar,
        Tag,
        PillRight,
        StatusTag,
        KbdChip,
        IconTile,
        EmptyState,
        Button,
        HLine,
        HLineDashed,
        VLine,
        Dot
    } type = None;

    RECT rect = {};
    std::wstring text;
    COLORREF color = 0;
    COLORREF color2 = 0;
    COLORREF color3 = 0;
    int radius = 0;
    int width = 0;
    FontRole fontRole = FontRole::Body;
    UINT format = 0;
    bool hover = false;
    bool pressed = false;
    bool disabled = false;
    BtnStyle btnStyle = BtnStyle::Primary;

    bool Matches(const DrawCall& expected) const {
        if (type != expected.type) return false;
        if (expected.color != 0 && color != expected.color) return false;
        if (expected.color2 != 0 && color2 != expected.color2) return false;
        if (!expected.text.empty() && text != expected.text) return false;
        if (expected.radius != 0 && radius != expected.radius) return false;
        return true;
    }

    std::wstring ToString() const {
        wchar_t buf[512];
        swprintf_s(buf, L"DrawCall[type=%d, rect=(%d,%d,%d,%d), color=%06X, text='%s']",
            type, rect.left, rect.top, rect.right, rect.bottom, color, text.c_str());
        return buf;
    }
};

// 虚拟 Canvas：记录所有绘制调用，不真正画
class TestCanvas : public Canvas
{
public:
    TestCanvas() : Canvas(nullptr, Fonts::Default()) {}

    std::vector<DrawCall>& Calls() { return m_calls; }
    void ClearCalls() { m_calls.clear(); }

    // 重写所有虚函数记录调用
#define OVERRIDE_DRAW(name, typeEnum) \
    void name##_override(auto&&... args) { \
        m_calls.push_back({DrawCall::typeEnum}); \
        auto& c = m_calls.back(); \
        Record_##name(c, std::forward<decltype(args)>(args)...); \
    }

    // 由于 Canvas 不是虚函数，这里用组合而非继承
    // 实际测试时用 TestCanvas 替代 Canvas

private:
    std::vector<DrawCall> m_calls;

    // 记录辅助
    template<typename... Args>
    void Record_Fill(DrawCall& c, Args&&... args) {
        // 手动展开参数赋值
    }
};

// ------------------------------------------------------------
// TestShell — 无头 Shell
// ------------------------------------------------------------
class TestShell
{
public:
    TestShell();
    ~TestShell();

    // 创建虚拟环境（无真实窗口）
    bool CreateVirtual(int width, int height, const ShellOptions& options = {});

    // 页面管理
    void AddPage(int id, std::unique_ptr<IPage> page);
    bool SwitchPage(int id);
    IPage* CurrentPage() const;

    // 触发一帧绘制
    void PaintFrame();

    // 模拟输入
    void SimulateMouseMove(int x, int y);
    void SimulateMouseDown(int x, int y);
    void SimulateMouseUp(int x, int y);
    void SimulateKeyDown(UINT vk, bool ctrl = false, bool shift = false);
    void SimulateChar(wchar_t ch);

    // 绘制调用捕获
    const std::vector<DrawCall>& CaptureDrawCalls() const { return m_drawCalls; }
    void ClearDrawCalls() { m_drawCalls.clear(); }

    // 断言辅助
    bool AssertDrawCall(size_t index, const DrawCall& expected) const;
    bool AssertDrawCallSequence(const std::vector<DrawCall>& expected) const;
    size_t CountDrawCalls(DrawCall::Type type) const;

    // 访问内部状态
    const RECT& ContentRect() const { return m_content; }
    const RECT& ClientRect() const { return m_client; }
    int Dpi() const { return m_dpi; }
    Fonts& GetFonts() { return m_fonts; }

    // 生命周期
    void OnSize(int w, int h);

private:
    // 内存 DC 用于离屏渲染验证
    HDC CreateMemDC(int w, int h);
    void DestroyMemDC();

    // 页面管理
    struct PageSlot {
        int id = 0;
        std::unique_ptr<IPage> page;
    };
    std::vector<PageSlot> m_pages;
    int m_pageId = -1;

    // 虚拟窗口状态
    RECT m_client = {};
    RECT m_content = {};
    RECT m_side = {};
    RECT m_status = {};
    RECT m_caption = {};
    int m_dpi = 96;
    Fonts m_fonts;
    NavBar m_nav;
    ShellOptions m_opt;

    // 绘制捕获
    std::vector<DrawCall> m_drawCalls;
    HDC m_memDC = nullptr;
    HBITMAP m_memBmp = nullptr;

    // 递归绘制页面
    void PaintPage(IPage* page, HDC dc);
    void RecordDrawCall(const DrawCall& call) { m_drawCalls.push_back(call); }
};

} // namespace ui
} // namespace air