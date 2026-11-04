#include "airui/Accessibility.h"
#include "airui/Widgets.h"
#include <oleacc.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <regex>

#pragma comment(lib, "oleacc.lib")

namespace air {
namespace ui {

// ============================================================
// Accessibility 实现
// ============================================================
namespace a11y {

void SetAccessibleRole(HWND hwnd, Role role)
{
    if (!hwnd) return;
    // 通过 IAccessible 或 UIA 设置角色
    // 简化：使用 SetProp 存储，实际需要实现 IAccessible
    SetPropW(hwnd, L"AirUI_A11y_Role", (HANDLE)(ULONG_PTR)role);
}

void SetAccessibleName(HWND hwnd, const std::wstring& name)
{
    if (!hwnd) return;
    SetPropW(hwnd, L"AirUI_A11y_Name", (HANDLE)new std::wstring(name));
}

void SetAccessibleDescription(HWND hwnd, const std::wstring& desc)
{
    if (!hwnd) return;
    SetPropW(hwnd, L"AirUI_A11y_Desc", (HANDLE)new std::wstring(desc));
}

void SetAccessibleState(HWND hwnd, State state, bool on)
{
    if (!hwnd) return;
    DWORD cur = (DWORD)(ULONG_PTR)GetPropW(hwnd, L"AirUI_A11y_State");
    if (on) cur |= state; else cur &= ~state;
    SetPropW(hwnd, L"AirUI_A11y_State", (HANDLE)(ULONG_PTR)cur);
}

void SetAccessibleValue(HWND hwnd, const std::wstring& value, 
                        const std::wstring& min, const std::wstring& max)
{
    if (!hwnd) return;
    SetPropW(hwnd, L"AirUI_A11y_Value", (HANDLE)new std::wstring(value));
    if (!min.empty()) SetPropW(hwnd, L"AirUI_A11y_Min", (HANDLE)new std::wstring(min));
    if (!max.empty()) SetPropW(hwnd, L"AirUI_A11y_Max", (HANDLE)new std::wstring(max));
}

void EnableForWidget(Widget* w, const std::wstring& name, Role role)
{
    if (!w || !w->Native()) return;
    HWND hwnd = w->Native();
    if (role != ROLE_UNKNOWN) SetAccessibleRole(hwnd, role);
    if (!name.empty()) SetAccessibleName(hwnd, name);
    
    // 根据控件类型推断角色
    if (role == ROLE_UNKNOWN) {
        wchar_t className[64];
        GetClassNameW(hwnd, className, 64);
        if (wcscmp(className, L"Button") == 0) SetAccessibleRole(hwnd, ROLE_BUTTON);
        else if (wcscmp(className, L"Edit") == 0) SetAccessibleRole(hwnd, ROLE_TEXTBOX);
        else if (wcscmp(className, L"ListBox") == 0) SetAccessibleRole(hwnd, ROLE_LISTBOX);
    }
}

LiveRegion::LiveRegion()
{
    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"STATIC", L"", WS_POPUP,
        0, 0, 1, 1, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (m_hwnd) {
        SetAccessibleRole(m_hwnd, ROLE_ALERT);
        SetAccessibleName(m_hwnd, L"Live Region");
    }
}

LiveRegion::~LiveRegion()
{
    if (m_hwnd) DestroyWindow(m_hwnd);
}

void LiveRegion::Announce(const std::wstring& text, bool polite)
{
    if (!m_hwnd) return;
    SetAccessibleName(m_hwnd, text);
    // 触发 EVENT_OBJECT_NAMECHANGE
    NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, m_hwnd, OBJID_CLIENT, CHILDID_SELF);
}

void LiveRegion::SetAssertive(bool assertive)
{
    // 简化：实际需要 UIA LiveSetting 属性
}

bool IsHighContrastMode()
{
    HIGHCONTRASTW hc = { sizeof(HIGHCONTRASTW) };
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, 0, &hc, 0) && (hc.dwFlags & HCF_HIGHCONTRASTON);
}

static std::function<void(bool)> g_hcCallback;

void SetHighContrastCallback(std::function<void(bool)> cb)
{
    g_hcCallback = std::move(cb);
    // 监听 WM_SETTINGCHANGE
    // 简化：定时轮询
}

void EnableKeyboardNavigation(HWND hwnd)
{
    // 子类化处理 Tab/Arrow/Enter/Esc
    // 简化：设置 WS_TABSTOP
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    SetWindowLong(hwnd, GWL_STYLE, style | WS_TABSTOP);
}

} // namespace a11y

// ============================================================
// Localization 实现
// ============================================================
namespace i18n {

LocalizationManager& LocalizationManager::Instance()
{
    static LocalizationManager inst;
    return inst;
}

bool LocalizationManager::LoadLocale(const std::wstring& jsonPath)
{
    std::wifstream file(jsonPath);
    if (!file) return false;
    std::wstringstream ss;
    ss << file.rdbuf();
    std::wstring json = ss.str();
    if (json.empty()) return false;

    auto getString = [&](const std::wstring& src, const std::wstring& key, std::wstring def = L"") -> std::wstring {
        std::wstring pattern = L"\"" + key + L"\"\\s*:\\s*\"([^\"]*)\"";
        std::wregex re(pattern);
        std::wsmatch m;
        if (std::regex_search(src, m, re)) return m[1].str();
        return def;
    };
    auto getBool = [&](const std::wstring& src, const std::wstring& key, bool def = false) -> bool {
        std::wstring pattern = L"\"" + key + L"\"\\s*:\\s*(true|false)";
        std::wregex re(pattern);
        std::wsmatch m;
        if (std::regex_search(src, m, re)) return m[1].str() == L"true";
        return def;
    };

    Locale loc;
    loc.code = getString(json, L"code");
    if (loc.code.empty()) return false;
    loc.name = getString(json, L"name");
    loc.dateFormat = getString(json, L"dateFormat", L"yyyy/MM/dd");
    loc.timeFormat = getString(json, L"timeFormat", L"HH:mm:ss");
    loc.numberFormat = getString(json, L"numberFormat", L"#,##0.00");
    loc.currencySymbol = getString(json, L"currencySymbol", L"¥");
    loc.listSeparator = getString(json, L"listSeparator", L",");
    loc.decimalPoint = getString(json, L"decimalPoint", L".");
    loc.thousandSep = getString(json, L"thousandSep", L",");
    loc.rtl = getBool(json, L"rtl", false);

    std::wstring stringsPattern = L"\"strings\"\\s*:\\s*\\{([^}]*)\\}";
    std::wregex stringsRe(stringsPattern);
    std::wsmatch stringsM;
    if (std::regex_search(json, stringsM, stringsRe)) {
        std::wstring obj = stringsM[1].str();
        std::wregex kvRe(L"\"([^\"]+)\"\\s*:\\s*\"([^\"]*)\"");
        auto it = std::wsregex_iterator(obj.begin(), obj.end(), kvRe);
        auto end = std::wsregex_iterator();
        for (; it != end; ++it) {
            std::wstring k = (*it)[1].str();
            std::wstring v = (*it)[2].str();
            loc.strings[k] = v;
        }
    }

    m_locales[loc.code] = loc;
    m_available.push_back(loc.code);
    if (m_current.empty() || m_locales.find(m_current) == m_locales.end()) {
        m_current = loc.code;
    }
    return true;
}

void LocalizationManager::SetLocale(const std::wstring& code)
{
    if (m_locales.find(code) == m_locales.end()) return;
    m_current = code;
    for (auto& cb : m_changeCallbacks) cb(code);
}

const Locale* LocalizationManager::Current() const
{
    auto it = m_locales.find(m_current);
    return it != m_locales.end() ? &it->second : nullptr;
}

std::wstring LocalizationManager::Tr(const std::wstring& key, const std::map<std::wstring, std::wstring>& args) const
{
    const Locale* loc = Current();
    if (!loc) return key;

    auto it = loc->strings.find(key);
    std::wstring result = (it != loc->strings.end()) ? it->second : key;

    // 简单占位符替换 {key}
    for (auto& kv : args) {
        std::wstring placeholder = L"{" + kv.first + L"}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::wstring::npos) {
            result.replace(pos, placeholder.length(), kv.second);
            pos += kv.second.length();
        }
    }
    return result;
}

std::wstring LocalizationManager::Tr(const std::wstring& key, const std::wstring& arg1, const std::wstring& val1) const
{
    return Tr(key, {{arg1, val1}});
}

std::wstring LocalizationManager::Tr(const std::wstring& key, const std::wstring& arg1, const std::wstring& val1,
                                     const std::wstring& arg2, const std::wstring& val2) const
{
    return Tr(key, {{arg1, val1}, {arg2, val2}});
}

std::wstring LocalizationManager::FormatDate(const SYSTEMTIME& st) const
{
    const Locale* loc = Current();
    if (!loc) return L"";
    // 简化实现
    wchar_t buf[64];
    swprintf_s(buf, L"%04d/%02d/%02d", st.wYear, st.wMonth, st.wDay);
    return buf;
}

std::wstring LocalizationManager::FormatTime(const SYSTEMTIME& st) const
{
    wchar_t buf[64];
    swprintf_s(buf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::wstring LocalizationManager::FormatNumber(double val, int decimals) const
{
    const Locale* loc = Current();
    if (!loc) return L"";
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(decimals) << val;
    std::wstring s = ss.str();
    // 替换小数点和千分位
    // 简化
    return s;
}

std::wstring LocalizationManager::FormatCurrency(double val) const
{
    const Locale* loc = Current();
    if (!loc) return L"";
    std::wstring num = FormatNumber(val, 2);
    return loc->currencySymbol + num;
}

void LocalizationManager::OnLocaleChanged(std::function<void(const std::wstring&)> cb)
{
    m_changeCallbacks.push_back(std::move(cb));
}

} // namespace i18n

} // namespace ui
} // namespace air