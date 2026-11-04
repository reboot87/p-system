// ============================================================
// AirUI 组件布局验证 —— 验证 CalculateGalleryLayout 的输出
// ============================================================
#include <windows.h>
#include <iostream>
#include <string>

#include "airui/AirUi.h"
#include "gallery/GalleryDraw.h"

using namespace air::ui;

int main() {
    // 1. 验证布局计算
    const int W = 1280;
    const int H = gallery::kGalleryHeight;

    auto layouts = gallery::CalculateGalleryLayout(32, 0, W - 64);

    // 预期：2个固定/头部元素 + 1个调色板 + 1个按钮区域 + 3个卡片 = 7个布局项
    const int EXPECTED_LAYOUTS = 7;
    if (layouts.size() != EXPECTED_LAYOUTS) {
        std::cerr << "FAIL: Expected " << EXPECTED_LAYOUTS << " layouts, got " << layouts.size() << std::endl;
        return 1;
    }
    std::cout << "PASS: Layout count matches expected: " << EXPECTED_LAYOUTS << std::endl;

    // 2. 验证特定组件配置 (第一个卡片)
    bool card0_ok = false;
    for (const auto& layout : layouts) {
        if (layout.type == gallery::ComponentLayout::CARD_TYPE) {
            // 查找第一个卡片 (在布局中应是第4个元素，索引为3)
            if (layout.cardConfig.title == L"剪贴板监控") {
                if (layout.cardConfig.number == L"1,286" && layout.cardConfig.disabled == false) {
                    card0_ok = true;
                    break;
                }
            }
        }
    }

    if (!card0_ok) {
        std::cerr << "FAIL: Card config for '剪贴板监控' does not match expected properties."
                  << std::endl;
        return 1;
    }
    std::cout << "PASS: Specific Card configuration verified."
              << std::endl;

    std::cout << "PASS\n";
    return 0;
}

