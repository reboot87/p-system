#pragma once
// ============================================================
// AirUI — Fonts.h
// 字体集合：一套界面所需的标准字重/字号，按 DPI 自动缩放
//
// 关键点：单行 EDIT 永远把文字画在客户区顶部，
// 因此「垂直居中」只能靠把 EDIT 客户区压到恰好一行文字高。
// BodyLineHeight() 给出的就是这个高度。
// ============================================================
#include <windows.h>
#include <string>

namespace air {
namespace ui {

enum class FontRole
{
    Body,       // 正文 14
    Meta,       // 元信息 12
    Title,      // 小标题 13 semibold
    Section,    // 区块标题 14 semibold
    Button,     // 按钮 14 semibold
    Icon,       // 图标 11（Segoe MDL2 Assets）
    IconLarge,  // 大图标 16
    Display,    // 大数字 22 semibold
    Count
};

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

    HFONT Get(FontRole role) const;
    HFONT Body() const { return m_body; }
    HFONT Meta() const { return m_meta; }

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
    HFONT Make(int pixelHeight, int weight, const std::wstring& family) const;

    HFONT m_body    = nullptr;
    HFONT m_meta    = nullptr;
    HFONT m_title   = nullptr;
    HFONT m_section = nullptr;
    HFONT m_button  = nullptr;
    HFONT m_icon    = nullptr;
    HFONT m_iconLg  = nullptr;
    HFONT m_display = nullptr;

    int m_dpi   = 96;
    int m_lineH = 21;

    std::wstring m_family     = L"Microsoft YaHei UI";
    std::wstring m_iconFamily = L"Segoe MDL2 Assets";
};

} // namespace ui
} // namespace air
