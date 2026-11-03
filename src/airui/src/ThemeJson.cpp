#include "airui/ThemeJson.h"
#include "airui/Theme.h"
#include "airui/Fonts.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace air {
namespace ui {

static std::wstring ColorToHex(COLORREF c)
{
    int r = GetRValue(c);
    int g = GetGValue(c);
    int b = GetBValue(c);
    std::wostringstream oss;
    oss << L"#" << std::hex << std::uppercase << std::setfill(L'0')
        << std::setw(2) << r
        << std::setw(2) << g
        << std::setw(2) << b;
    return oss.str();
}

ThemeJson DefaultThemeJson()
{
    ThemeJson tj;
    tj.name = L"Default";
    tj.colors = LightPalette();
    tj.metrics = Metrics{};
    tj.fontFamily = L"Microsoft YaHei UI";
    tj.iconFontFamily = L"Segoe MDL2 Assets";
    return tj;
}

void ApplyThemeJson(const ThemeJson& theme)
{
    SetPalette(theme.colors);
    SetMetrics(theme.metrics);
    Fonts::Default().SetFamily(theme.fontFamily);
    Fonts::Default().SetIconFamily(theme.iconFontFamily);
    Fonts::Default().Create();
}

COLORREF ParseColor(const std::wstring& str, COLORREF fallback)
{
    if (str.empty()) return fallback;
    std::wstring s = str;
    auto start = s.find_first_not_of(L" \t\r\n");
    auto end = s.find_last_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return fallback;
    s = s.substr(start, end - start + 1);
    if (s[0] == L'#')
    {
        s = s.substr(1);
        if (s.size() == 6 || s.size() == 8)
        {
            try {
                unsigned int val = 0;
                std::wistringstream iss(s);
                iss >> std::hex >> val;
                if (s.size() == 6) {
                    int r = (val >> 16) & 0xFF;
                    int g = (val >> 8) & 0xFF;
                    int b = val & 0xFF;
                    return RGB(r, g, b);
                } else {
                    int r = (val >> 16) & 0xFF;
                    int g = (val >> 8) & 0xFF;
                    int b = val & 0xFF;
                    return RGB(r, g, b);
                }
            } catch (...) {}
        }
        else if (s.size() == 3) {
            try {
                int r = std::stoi(s.substr(0,1), nullptr, 16);
                int g = std::stoi(s.substr(1,1), nullptr, 16);
                int b = std::stoi(s.substr(2,1), nullptr, 16);
                r = (r << 4) | r;
                g = (g << 4) | g;
                b = (b << 4) | b;
                return RGB(r, g, b);
            } catch (...) {}
        }
    }
    if (s.rfind(L"rgb(", 0) == 0 || s.rfind(L"rgba(", 0) == 0) {
        std::wstring inner = s.substr(s.find(L'(')+1);
        inner = inner.substr(0, inner.find(L')'));
        std::wistringstream iss(inner);
        std::wstring token;
        int vals[3] = {0,0,0};
        int i = 0;
        while (std::getline(iss, token, L',') && i < 3) {
            auto st = token.find_first_not_of(L" \t");
            auto en = token.find_last_not_of(L" \t");
            if (st != std::wstring::npos) token = token.substr(st, en-st+1);
            try { vals[i++] = std::stoi(token); } catch (...) {}
        }
        if (i >= 3) return RGB(vals[0], vals[1], vals[2]);
    }
    return fallback;
}

bool LoadTheme(const std::wstring& jsonPath)
{
    (void)jsonPath;
    return false;
}

bool ExportTheme(const std::wstring& jsonPath)
{
    std::wofstream file(jsonPath);
    if (!file) return false;
    const Palette& p = Pal();
    const Metrics& m = Mx();
    const auto& fonts = Fonts::Default();
    
    file << L"{\n";
    file << L"  \"name\": \"Exported\",\n";
    file << L"  \"colors\": {\n";
    file << L"    \"bg\": \"" << ColorToHex(p.bg) << L"\",\n";
    file << L"    \"surface\": \"" << ColorToHex(p.surface) << L"\",\n";
    file << L"    \"sunken\": \"" << ColorToHex(p.sunken) << L"\",\n";
    file << L"    \"sidebar\": \"" << ColorToHex(p.sidebar) << L"\",\n";
    file << L"    \"sidebarLine\": \"" << ColorToHex(p.sidebarLine) << L"\",\n";
    file << L"    \"split\": \"" << ColorToHex(p.split) << L"\",\n";
    file << L"    \"border\": \"" << ColorToHex(p.border) << L"\",\n";
    file << L"    \"text\": \"" << ColorToHex(p.text) << L"\",\n";
    file << L"    \"textSub\": \"" << ColorToHex(p.textSub) << L"\",\n";
    file << L"    \"textFaint\": \"" << ColorToHex(p.textFaint) << L"\",\n";
    file << L"    \"textDisabled\": \"" << ColorToHex(p.textDisabled) << L"\",\n";
    file << L"    \"textInverse\": \"" << ColorToHex(p.textInverse) << L"\",\n";
    file << L"    \"hover\": \"" << ColorToHex(p.hover) << L"\",\n";
    file << L"    \"active\": \"" << ColorToHex(p.active) << L"\",\n";
    file << L"    \"selectFill\": \"" << ColorToHex(p.selectFill) << L"\",\n";
    file << L"    \"selectRing\": \"" << ColorToHex(p.selectRing) << L"\",\n";
    file << L"    \"hoverBorder\": \"" << ColorToHex(p.hoverBorder) << L"\",\n";
    file << L"    \"primary\": \"" << ColorToHex(p.primary) << L"\",\n";
    file << L"    \"primaryHover\": \"" << ColorToHex(p.primaryHover) << L"\",\n";
    file << L"    \"primaryDown\": \"" << ColorToHex(p.primaryDown) << L"\",\n";
    file << L"    \"primarySoft\": \"" << ColorToHex(p.primarySoft) << L"\",\n";
    file << L"    \"primaryText\": \"" << ColorToHex(p.primaryText) << L"\",\n";
    file << L"    \"ok\": \"" << ColorToHex(p.ok) << L"\",\n";
    file << L"    \"okSoft\": \"" << ColorToHex(p.okSoft) << L"\",\n";
    file << L"    \"okText\": \"" << ColorToHex(p.okText) << L"\",\n";
    file << L"    \"okBorder\": \"" << ColorToHex(p.okBorder) << L"\",\n";
    file << L"    \"warn\": \"" << ColorToHex(p.warn) << L"\",\n";
    file << L"    \"warnSoft\": \"" << ColorToHex(p.warnSoft) << L"\",\n";
    file << L"    \"warnText\": \"" << ColorToHex(p.warnText) << L"\",\n";
    file << L"    \"warnBorder\": \"" << ColorToHex(p.warnBorder) << L"\",\n";
    file << L"    \"error\": \"" << ColorToHex(p.error) << L"\",\n";
    file << L"    \"errorSoft\": \"" << ColorToHex(p.errorSoft) << L"\",\n";
    file << L"    \"errorText\": \"" << ColorToHex(p.errorText) << L"\",\n";
    file << L"    \"errorBorder\": \"" << ColorToHex(p.errorBorder) << L"\",\n";
    file << L"    \"noteBg\": \"" << ColorToHex(p.noteBg) << L"\",\n";
    file << L"    \"noteBorder\": \"" << ColorToHex(p.noteBorder) << L"\",\n";
    file << L"    \"iconTile\": \"" << ColorToHex(p.iconTile) << L"\",\n";
    file << L"    \"iconTileHover\": \"" << ColorToHex(p.iconTileHover) << L"\",\n";
    file << L"    \"chipBg\": \"" << ColorToHex(p.chipBg) << L"\",\n";
    file << L"    \"chipBorder\": \"" << ColorToHex(p.chipBorder) << L"\",\n";
    file << L"    \"captionBtnHover\": \"" << ColorToHex(p.captionBtnHover) << L"\",\n";
    file << L"    \"captionCloseHover\": \"" << ColorToHex(p.captionCloseHover) << L"\",\n";
    file << L"    \"scrollThumb\": \"" << ColorToHex(p.scrollThumb) << L"\"\n";
    file << L"  },\n";
    file << L"  \"metrics\": {\n";
    file << L"    \"titleH\": " << m.titleH << L",\n";
    file << L"    \"capBtnW\": " << m.capBtnW << L",\n";
    file << L"    \"sidebarW\": " << m.sidebarW << L",\n";
    file << L"    \"statusH\": " << m.statusH << L",\n";
    file << L"    \"unit\": " << m.unit << L",\n";
    file << L"    \"pagePad\": " << m.pagePad << L",\n";
    file << L"    \"cardGap\": " << m.cardGap << L",\n";
    file << L"    \"radiusSm\": " << m.radiusSm << L",\n";
    file << L"    \"radiusMd\": " << m.radiusMd << L",\n";
    file << L"    \"radiusLg\": " << m.radiusLg << L",\n";
    file << L"    \"radiusCard\": " << m.radiusCard << L",\n";
    file << L"    \"radiusPill\": " << m.radiusPill << L",\n";
    file << L"    \"rowH\": " << m.rowH << L",\n";
    file << L"    \"inputH\": " << m.inputH << L",\n";
    file << L"    \"searchH\": " << m.searchH << L",\n";
    file << L"    \"btnH\": " << m.btnH << L",\n";
    file << L"    \"tagH\": " << m.tagH << L",\n";
    file << L"    \"statusTagH\": " << m.statusTagH << L",\n";
    file << L"    \"itemH\": " << m.itemH << L",\n";
    file << L"    \"groupH\": " << m.groupH << L",\n";
    file << L"    \"fontBody\": " << m.fontBody << L",\n";
    file << L"    \"fontMeta\": " << m.fontMeta << L",\n";
    file << L"    \"fontTitle\": " << m.fontTitle << L",\n";
    file << L"    \"fontSection\": " << m.fontSection << L",\n";
    file << L"    \"fontButton\": " << m.fontButton << L",\n";
    file << L"    \"fontIcon\": " << m.fontIcon << L",\n";
    file << L"    \"fontIconLg\": " << m.fontIconLg << L",\n";
    file << L"    \"fontDisplay\": " << m.fontDisplay << L"\n";
    file << L"  },\n";
    file << L"  \"fonts\": {\n";
    file << L"    \"family\": \"" << fonts.Family() << L"\",\n";
    file << L"    \"iconFamily\": \"" << fonts.IconFamily() << L"\"\n";
    file << L"  }\n";
    file << L"}\n";
    return true;
}

} // namespace ui
} // namespace air
