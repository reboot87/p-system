#pragma once
// ============================================================
// AirUI — Fonts.h
// 字体集合：一套界面所需的标准字重/字号，按 DPI 自动缩放
//
// 关键点：单行 EDIT 永远把文字画在客户区顶部，
// 因此「垂直居中」只能靠把 EDIT 客户区压到恰好一行文字高。
// BodyLineHeight() 给出的就是这个高度。
// ============================================================
#include "airui/types.h"
#include <string>

// FontRole is defined once, in the platform-free HAL. The native
// backend only maps each role to a platform font handle.
#include "airui/hal.h"

namespace air {
namespace ui {

class Fonts
{
public:
    Fonts() = default;
    ~Fonts();

    Fonts(const Fonts&) = delete;
    Fonts& operator=(const Fonts&) = delete;

    // dpi = 0 → 从屏幕 DC 自动探测
    bool Create(int dpi = 0);
    void Destroy();
    bool Valid() const { return m_body != nullptr; }

    FontHandle Get(FontRole role) const;
    FontHandle Body() const { return m_body; }
    FontHandle Meta() const { return m_meta; }

    // 单行 EDIT 客户区应设为这个高度，文字才真正上下居中
    int BodyLineHeight() const { return m_lineH; }
    int Dpi() const { return m_dpi; }

    // 字体族（需重新 Create 生效）
    void SetFamily(const std::wstring& family) { m_family = family; }
    void SetIconFamily(const std::wstring& family) { m_iconFamily = family; }
    const std::wstring& Family() const { return m_family; }
    const std::wstring& IconFamily() const { return m_iconFamily; }

    // 进程内共享实例（懒加载，第一次 Get 时创建）
    static Fonts& Default();

private:
    FontHandle Make(int pixelHeight, int weight, const std::wstring& family) const;

    FontHandle m_body    = nullptr;
    FontHandle m_meta    = nullptr;
    FontHandle m_title   = nullptr;
    FontHandle m_section = nullptr;
    FontHandle m_button  = nullptr;
    FontHandle m_icon    = nullptr;
    FontHandle m_iconLg  = nullptr;
    FontHandle m_display = nullptr;

    int m_dpi   = 96;
    int m_lineH = 21;

    std::wstring m_family     = L"Microsoft YaHei UI";
    std::wstring m_iconFamily = L"Segoe MDL2 Assets";
};

} // namespace ui
} // namespace air
