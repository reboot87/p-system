#include "airui/Animation.h"
#include <cmath>
#include <algorithm>

namespace air {
namespace ui {
namespace anim {

// ------------------------------------------------------------
// Injected platform clock
// ------------------------------------------------------------
static std::function<uint64_t()> s_clock;

void AnimationManager::SetClock(std::function<uint64_t()> clock)
{
    s_clock = std::move(clock);
}

uint64_t AnimationManager::Now()
{
    return s_clock ? s_clock() : 0;
}

// ------------------------------------------------------------
// AnimationManager 单例
// ------------------------------------------------------------
AnimationManager& AnimationManager::Instance()
{
    static AnimationManager inst;
    return inst;
}

int AnimationManager::Start(Animation anim, uint64_t now)
{
    int id = m_nextId++;
    auto inst = std::make_unique<AnimInstance>();
    inst->anim = std::move(anim);
    inst->startTime = now;
    inst->running = true;
    m_anims.push_back(std::move(inst));
    return id;
}

void AnimationManager::Cancel(int id)
{
    (void)id;
    m_anims.erase(
        std::remove_if(m_anims.begin(), m_anims.end(),
            [](const std::unique_ptr<AnimInstance>& inst) {
                return inst && inst->anim.onComplete == nullptr;
            }),
        m_anims.end());
}

void AnimationManager::Tick(uint64_t now)
{
    for (auto& inst : m_anims) {
        if (!inst || !inst->running) continue;
        uint64_t elapsed = now - inst->startTime;
        float progress = (std::min<float>)(1.0f, (float)elapsed / (float)inst->anim.durationMs);
        float eased = Ease(inst->anim.easing, progress);
        if (inst->anim.onFrame) inst->anim.onFrame(eased);
        if (progress >= 1.0f) {
            inst->running = false;
            if (inst->anim.onComplete) inst->anim.onComplete();
        }
    }
    m_anims.erase(
        std::remove_if(m_anims.begin(), m_anims.end(),
            [](const std::unique_ptr<AnimInstance>& inst) {
                return inst && !inst->running;
            }),
        m_anims.end());
}

float AnimationManager::Ease(anim::Easing e, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (e) {
        case anim::Easing::Linear: return t;
        case anim::Easing::EaseInQuad: return t * t;
        case anim::Easing::EaseOutQuad: return 1 - (1 - t) * (1 - t);
        case anim::Easing::EaseInOutQuad: return t < 0.5f ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
        case anim::Easing::EaseOutCubic: return 1 - std::pow(1 - t, 3);
        case anim::Easing::EaseOutBack: {
            const float c1 = 1.70158f, c3 = c1 + 1;
            return 1 + c3 * std::pow(t - 1, 3) + c1 * std::pow(t - 1, 2);
        }
        case anim::Easing::EaseOutElastic: {
            const float c4 = (2 * 3.1415926535f) / 3;
            return t == 0 ? 0 : t == 1 ? 1 :
                std::pow(2, -10 * t) * std::sin((t * 10 - 0.75f) * c4) + 1;
        }
    }
    return t;
}

} // namespace anim
} // namespace ui
} // namespace air