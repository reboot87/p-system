#pragma once
// ============================================================
// AirUI — Animation.h
// 入场/退场/展开/淡入淡出动画系统（由后端的 IClock 驱动）
//
// 用法：
//     Animation anim;
//     anim.durationMs = 300;
//     anim.easing = anim::Easing::EaseOutQuad;
//     anim.onFrame = [](float p) { ... };
//     anim::AnimationManager::Instance().Start(std::move(anim));
// ============================================================
#include <cstdint>
#include <functional>
#include <vector>
#include <memory>

namespace air {
namespace ui {
namespace anim {

enum class Easing
{
    Linear,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseOutCubic,
    EaseOutBack,
    EaseOutElastic
};

struct Animation
{
    int durationMs = 200;
    Easing easing = Easing::EaseOutQuad;
    std::function<void(float progress)> onFrame;  // 0.0 ~ 1.0
    std::function<void()> onComplete;
    bool autoDestroy = true;
};

struct AnimInstance
{
    Animation anim;
    uint64_t startTime = 0;
    bool running = true;
};

class AnimationManager
{
public:
    static AnimationManager& Instance();

    // 启动动画，返回动画 ID（可用于取消）
    // now 由后端时钟注入
    int Start(Animation anim, uint64_t now);

    // 取消动画
    void Cancel(int id);

    // 每帧调用（Shell 的消息循环里挂钩）
    void Tick(uint64_t now);

    // 缓动函数
    static float Ease(Easing e, float t);

    // 注入平台时钟。core 不自带时钟实现，保证平台无关。
    static void SetClock(std::function<uint64_t()> clock);
    static uint64_t Now();

private:
    AnimationManager() = default;
    int m_nextId = 1;
    std::vector<std::unique_ptr<AnimInstance>> m_anims;
};

} // namespace anim
} // namespace ui
} // namespace air