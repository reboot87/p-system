// ============================================================
// test_hot_state — 自绘热区状态机的行为契约
//
// 这几个断言对应的是"桌面按钮应该怎么反应"，不是实现细节：
//   1. 指针进入/离开热区只报告一次变化（页面据此重绘，多了就是抖动）
//   2. 按下后拖出热区再松手 = 取消（不触发点击）
//   3. 拖出去再拖回来松手 = 仍然触发
//   4. 按在 A 上、在 B 上松手 = 不触发 A，也不触发 B
// ============================================================
#include "airui/Interaction.h"
#include <cstdio>

using air::ui::HotState;

static int g_fail = 0;

static void Check(bool ok, const char* what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

int main()
{
    const int kA = 10, kB = 20;

    // ---- 悬停只报告一次变化 ----
    {
        HotState h;
        Check(h.Move(kA), "move into A reports change");
        Check(!h.Move(kA), "staying on A reports nothing");
        Check(h.Hovered(kA), "A is hovered");
        Check(!h.Pressed(kA), "not pressed yet");
        Check(h.Move(0), "move off A reports change");
        Check(!h.Hovered(kA), "A no longer hovered");
    }

    // ---- 同一点按下抬起 = 点击 ----
    {
        HotState h;
        h.Move(kA);
        Check(h.Down(kA), "down on A reports change");
        Check(h.Pressed(kA), "A looks pressed while pointer stays");
        Check(h.Release(kA), "release on A is a click");
        Check(!h.Pressing(), "press cleared after release");
    }

    // ---- 按下后拖出热区：视觉退回普通悬停，松手不算点击 ----
    {
        HotState h;
        h.Move(kA);
        h.Down(kA);
        Check(h.Move(0), "leaving A while held reports change");
        Check(!h.Pressed(kA), "A is no longer pressed after drag out");
        Check(!h.Release(0), "release outside A is not a click");
        Check(!h.Pressing(), "press cleared after release outside");
    }

    // ---- 拖出去再拖回：恢复按下，松手仍算点击 ----
    {
        HotState h;
        h.Move(kA);
        h.Down(kA);
        h.Move(0);
        h.Move(kA);
        Check(h.Pressed(kA), "drag back re-arms the press");
        Check(h.Release(kA), "release after drag back is a click");
    }

    // ---- 按在 A、在 B 上松手 ----
    {
        HotState h;
        h.Move(kA);
        h.Down(kA);
        h.Move(kB);
        Check(!h.Pressed(kA), "A not pressed while over B");
        Check(!h.Release(kB), "releasing on B does not click A or B");
    }

    // ---- 离开窗口：悬停与按下一起清掉 ----
    {
        HotState h;
        h.Move(kA);
        h.Down(kA);
        Check(h.Leave(), "leave reports change");
        Check(!h.Hovered(kA) && !h.Pressing(), "leave clears hover and press");
        Check(!h.Leave(), "second leave reports nothing");
    }

    std::printf("%s\n", g_fail ? "FAILED" : "ALL PASS");
    return g_fail ? 1 : 0;
}
