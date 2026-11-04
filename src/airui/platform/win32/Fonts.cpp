#include "airui/Fonts.h"
#include "airui/Theme.h"

namespace air {
namespace ui {

namespace {
int DetectDpi()
{
    HDC dc = GetDC(nullptr);
    if (!dc) return 96;
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return dpi > 0 ? dpi : 96;
}
} // namespace

Fonts::~Fonts()
{
    Destroy();
}

HFONT Fonts::Make(int pixelHeight, int weight, const std::wstring& family) const
{
    int h = -MulDiv(pixelHeight, m_dpi, 96);
    if (h == 0) h = -pixelHeight;
    return CreateFontW(h, 0, 0, 0, weight, 0, 0, 0,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                       family.c_str());
}

bool Fonts::Create(int dpi)
{
    Destroy();
    m_dpi = (dpi > 0) ? dpi : DetectDpi();

    // 字体尺寸使用 96 DPI 基准的默认度量，由 Make() 内部按 m_dpi 缩放。
    // 这里刻意不取 Mx()：Mx() 可能已被 ForDpi 缩放，二次缩放会导致字体过大。
    Metrics m96;

    m_body    = Make(m96.fontBody,    FW_NORMAL,   m_family);
    m_meta    = Make(m96.fontMeta,    FW_NORMAL,   m_family);
    m_title   = Make(m96.fontTitle,   FW_SEMIBOLD, m_family);
    m_section = Make(m96.fontSection, FW_SEMIBOLD, m_family);
    m_button  = Make(m96.fontButton,  FW_SEMIBOLD, m_family);
    m_icon    = Make(m96.fontIcon,    FW_NORMAL,   m_iconFamily);
    m_iconLg  = Make(m96.fontIconLg,  FW_NORMAL,   m_iconFamily);
    m_display = Make(m96.fontDisplay, FW_SEMIBOLD, m_family);

    if (!m_body) { Destroy(); return false; }

    // 正文行高：单行 EDIT 垂直居中的依据
    HDC dc = GetDC(nullptr);
    if (dc) {
        HGDIOBJ old = SelectObject(dc, m_body);
        TEXTMETRICW tm{};
        GetTextMetricsW(dc, &tm);
        m_lineH = tm.tmHeight + tm.tmExternalLeading;
        SelectObject(dc, old);
        ReleaseDC(nullptr, dc);
    }
    if (m_lineH <= 0) m_lineH = MulDiv(21, m_dpi, 96);
    return true;
}

void Fonts::Destroy()
{
    HFONT* all[] = { &m_body, &m_meta, &m_title, &m_section,
                     &m_button, &m_icon, &m_iconLg, &m_display };
    for (HFONT* p : all) {
        if (*p) { DeleteObject(*p); *p = nullptr; }
    }
}

HFONT Fonts::Get(FontRole role) const
{
    switch (role) {
    case FontRole::Body:      return m_body;
    case FontRole::Meta:      return m_meta;
    case FontRole::Title:     return m_title;
    case FontRole::Section:   return m_section;
    case FontRole::Button:    return m_button;
    case FontRole::Icon:      return m_icon;
    case FontRole::IconLarge: return m_iconLg;
    case FontRole::Display:   return m_display;
    default:                  return m_body;
    }
}

Fonts& Fonts::Default()
{
    static Fonts s_fonts;
    if (!s_fonts.Valid()) s_fonts.Create(0);
    return s_fonts;
}

} // namespace ui
} // namespace air
