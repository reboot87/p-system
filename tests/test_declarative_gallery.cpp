// ============================================================
// AirUI - declarative gallery demo integration test (core-only)
//
// Drives the gallery's DeclarativeJson extra demo headlessly:
// paint via PaintRecorder, click dispatch (+1/reset), wheel
// scrolling and the layouts/demo.json hot-reload path.
// ============================================================
#include "../src/gallery/ExtraGallery.h"
#include "airui/PaintRecorder.h"
#include "airui/Theme.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace air::ui;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what)
{
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("PASS: %s\n", what);
    }
}

const extragallery::DemoEntry* FindDemo(const wchar_t* label)
{
    for (const auto& d : extragallery::Demos()) {
        if (d.label == label) return &d;
    }
    return nullptr;
}

const PaintOp* FindText(const PaintRecorder& rec, const std::wstring& text)
{
    for (const PaintOp& op : rec.Ops()) {
        if (op.kind == PaintOpKind::Text && op.text == text) return &op;
    }
    return nullptr;
}

bool HasText(const PaintRecorder& rec, const std::wstring& needle)
{
    for (const PaintOp& op : rec.Ops()) {
        if (op.kind == PaintOpKind::Text && op.text.find(needle) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

const Rect kDemoRect{0, 0, 520, 300};
const Palette kPal = LightPalette();
const Metrics kMx{};

void Paint(extragallery::ExtraDemoState& s, PaintRecorder& rec)
{
    rec.Clear();
    const extragallery::DemoEntry* d = FindDemo(L"DeclarativeJson");
    d->paint(rec, kDemoRect, s, kPal, kMx);
}

} // namespace

int main()
{
    const extragallery::DemoEntry* demo = FindDemo(L"DeclarativeJson");
    Check(demo != nullptr, "DeclarativeJson demo is registered");
    if (!demo) return 1;
    Check(demo->click != nullptr && demo->wheel != nullptr,
          "demo has click and wheel hooks");

    // 隔离外部布局目录：先用临时目录做热重载用例，再清空走内置。
    const auto tmpRoot = std::filesystem::temp_directory_path() / "airui_decl_test";
    std::filesystem::create_directories(tmpRoot);
    extragallery::LayoutDir() = tmpRoot.wstring();
    std::filesystem::remove(tmpRoot / "demo.json", std::error_code{});

    extragallery::ExtraDemoState s;

    // ---- 1. 首帧：内置布局渲染 ----
    {
        PaintRecorder rec;
        Paint(s, rec);
        Check(s.declLoaded && s.declTemplate != nullptr, "embedded layout loaded on first paint");
        Check(s.declOrigin == L"内置布局", "embedded origin shown");
        Check(rec.Count(PaintOpKind::FillRound) >= 2, "buttons painted");
        Check(HasText(rec, L"声明式布局"), "title painted");
        Check(HasText(rec, L"0 次点击"), "click counter binding resolved");
        Check(FindText(rec, L"{clicks}") == nullptr, "no placeholder leaks into draw calls");
    }

    // ---- 2. 点击 +1 / 清零（命令沿 JSON 的 onClick 分发）----
    {
        // 行内布局：标题 26 + gap10 + 来源 18 + gap10 -> 行顶 y=64，行高 32
        PaintRecorder rec;
        Paint(s, rec);
        demo->click(0, Point{36, 80}, kDemoRect, s, kMx, s.declToast);
        Check(s.declClicks == 1, "+1 command fired");

        Paint(s, rec);
        Check(HasText(rec, L"1 次点击"), "counter repaints with new value");

        demo->click(0, Point{108, 80}, kDemoRect, s, kMx, s.declToast);
        Check(s.declClicks == 0, "reset command fired");
        Paint(s, rec);
        Check(HasText(rec, L"0 次点击"), "counter back to zero");
    }

    // ---- 3. 滚轮：Scroll 节点偏移并夹紧 ----
    {
        PaintRecorder rec;
        Paint(s, rec);
        const bool consumed = demo->wheel(0, -240, s, kMx);
        Check(consumed, "wheel consumed over a scrollable area");
        Paint(s, rec);
        Check(s.declScrollY > 0, "wheel scrolls down");
        const LayoutNode* sc = nullptr;
        for (const auto& c : s.declLive->children) {
            if (c->type == NodeType::Scroll) { sc = c.get(); break; }
        }
        Check(sc != nullptr && sc->ScrollY() > 0, "scroll node offset applied");
        Check(sc->ScrollY() <= std::max(0, sc->ScrollContentHeight() - sc->Frame().Height()),
              "scroll offset clamped to content minus viewport");
    }

    // ---- 4. 热重载：外部 demo.json 变更后重新加载 ----
    {
        const auto file = tmpRoot / "demo.json";
        const char* v1 =
            "{ \"type\": \"label\", \"id\": \"t\", \"text\": \"外部布局 A\", \"h\": 24 }";
        {
            std::ofstream f(file, std::ios::binary);
            f << v1;
        }
        PaintRecorder rec;
        Paint(s, rec);
        Check(HasText(rec, L"外部布局 A"), "external layout replaces embedded one");
        Check(s.declOrigin.find(L"demo.json") != std::wstring::npos,
              "origin reports the external file");

        // 内容变更 + 等待时间戳推进（NTFS 时间戳精度足够，但仍加小等待保险）
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const char* v2 =
            "{ \"type\": \"label\", \"id\": \"t\", \"text\": \"外部布局 B\", \"h\": 24 }";
        {
            std::ofstream f(file, std::ios::binary);
            f << v2;
        }
        Paint(s, rec);
        Check(HasText(rec, L"外部布局 B"), "hot reload picks up the new content");

        // 坏 JSON：旧模板继续渲染，错误显示在状态行
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        {
            std::ofstream f(file, std::ios::binary);
            f << "{ \"type\": \"nope\" }";
        }
        Paint(s, rec);
        Check(!s.declError.empty(), "bad JSON reports an error");
        Check(HasText(rec, L"外部布局 B"), "last good layout keeps rendering");
    }

    // 清理
    extragallery::LayoutDir().clear();
    std::filesystem::remove_all(tmpRoot, std::error_code{});

    // ---- 6. 真机仿真：带 client 偏移的 rc，点击给 client 坐标 ----
    // 对应修复后的 main.cpp 行为：rc（client 空间）== 树布局空间，
    // 点击 pt 与之同空间，HitTest 必须命中。
    {
        PaintRecorder rec;
        rec.Clear();
        extragallery::ExtraDemoState s2;
        const Rect rc2{30, 45, 550, 345};
        demo->paint(rec, rc2, s2, kPal, kMx);
        Check(s2.declLive != nullptr, "offset layout painted");
        LayoutNode* inc2 = s2.declLive ? s2.declLive->Find("inc") : nullptr;
        Check(inc2 != nullptr, "inc node exists in the offset tree");
        if (inc2) {
            Point click{inc2->Frame().left + 5, inc2->Frame().top + 5};
            demo->click(0, click, rc2, s2, kMx, s2.declToast);
            Check(s2.declClicks == 1, "click on offset rc with client coords fires +1");
        }
        // 再用外部文件内容（配置文件路径来源）做一次热加载仿真
        std::filesystem::create_directories(tmpRoot);
        extragallery::LayoutDir() = tmpRoot.wstring();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        {
            std::ofstream f(tmpRoot / "demo.json", std::ios::binary);
            f << "{ \"type\": \"label\", \"id\": \"t\", \"text\": \"仿真热重载\", \"h\": 24 }";
        }
        rec.Clear();
        demo->paint(rec, rc2, s2, kPal, kMx);
        Check(s2.declError.empty(), "simulation layout loads without error");
        extragallery::LayoutDir().clear();
    }

    if (g_failures != 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nall declarative gallery demo checks passed\n");
    return 0;
}
