#pragma once
// ============================================================
// AirUI — Accessibility.h / Localization.h
// 无障碍访问 (A11y) + 国际化 (i18n) 支持
//
// 设计原则：
//   · 非侵入式：现有代码无需修改即可工作
//   · 渐进增强：开启后自动为标准控件添加无障碍属性
//   · 运行时切换：语言/高对比度模式即时生效
// ============================================================
#include <windows.h>
#include <string>
#include <map>
#include <functional>
#include <vector>

// Forward declaration
class Widget;

namespace air {
namespace ui {

// ============================================================
// Accessibility (A11y)
// ============================================================
namespace a11y {

// 角色映射：Widget 类型 -> UIA/IA2 角色
enum Role {
    ROLE_UNKNOWN      = 0,
    ROLE_BUTTON       = 0x2C,   // ROLE_SYSTEM_PUSHBUTTON
    ROLE_TEXTBOX      = 0x2E,   // ROLE_SYSTEM_TEXT
    ROLE_LISTBOX      = 0x21,   // ROLE_SYSTEM_LIST
    ROLE_LISTITEM     = 0x22,   // ROLE_SYSTEM_LISTITEM
    ROLE_COMBOBOX     = 0x2F,   // ROLE_SYSTEM_COMBOBOX
    ROLE_SLIDER       = 0x30,   // ROLE_SYSTEM_SLIDER
    ROLE_PROGRESSBAR  = 0x32,   // ROLE_SYSTEM_PROGRESSBAR
    ROLE_STATUSBAR    = 0x33,   // ROLE_SYSTEM_STATUSBAR
    ROLE_MENU         = 0x0C,   // ROLE_SYSTEM_MENUPOPUP
    ROLE_MENUITEM     = 0x0D,   // ROLE_SYSTEM_MENUITEM
    ROLE_TOOLBAR      = 0x10,   // ROLE_SYSTEM_TOOLBAR
    ROLE_TAB          = 0x26,   // ROLE_SYSTEM_PAGETAB
    ROLE_TABPANEL     = 0x27,   // ROLE_SYSTEM_PAGETABLIST
    ROLE_DIALOG       = 0x12,   // ROLE_SYSTEM_DIALOG
    ROLE_ALERT        = 0x08,   // ROLE_SYSTEM_ALERT
    ROLE_TOOLTIP      = 0x0E,   // ROLE_SYSTEM_TOOLTIP
    ROLE_GROUP        = 0x15,   // ROLE_SYSTEM_GROUPING
    ROLE_PANE         = 0x16,   // ROLE_SYSTEM_PANE
    ROLE_WINDOW       = 0x09,   // ROLE_SYSTEM_WINDOW
};

// 状态标志
enum State {
    STATE_UNAVAILABLE   = 0x00000001, // 禁用
    STATE_SELECTED      = 0x00000002,
    STATE_FOCUSED       = 0x00000004,
    STATE_PRESSED       = 0x00000008,
    STATE_CHECKED       = 0x00000010,
    STATE_MIXED         = 0x00000020, // 三态
    STATE_READONLY      = 0x00000040,
    STATE_HOTTRACKED    = 0x00000080, // hover
    STATE_DEFAULT       = 0x00000100,
    STATE_EXPANDED      = 0x00000200,
    STATE_COLLAPSED     = 0x00000400,
    STATE_BUSY          = 0x00000800,
    STATE_INVISIBLE     = 0x00001000,
    STATE_OFFSCREEN     = 0x00002000,
    STATE_HASPOPUP      = 0x00004000,
};

// 为控件设置无障碍属性（在 CreateNative 后调用）
void SetAccessibleRole(HWND hwnd, Role role);
void SetAccessibleName(HWND hwnd, const std::wstring& name);
void SetAccessibleDescription(HWND hwnd, const std::wstring& desc);
void SetAccessibleState(HWND hwnd, State state, bool on);
void SetAccessibleValue(HWND hwnd, const std::wstring& value, 
                        const std::wstring& min = L"", const std::wstring& max = L"");

// 批量为标准控件启用无障碍
void EnableForWidget(Widget* w, const std::wstring& name = L"", Role role = ROLE_UNKNOWN);

// 实时区域：屏幕阅读器自动朗读变化的文本
class LiveRegion {
public:
    LiveRegion();
    ~LiveRegion();
    void Announce(const std::wstring& text, bool polite = true); // polite=true=礼貌模式
    void SetAssertive(bool assertive);
private:
    HWND m_hwnd = nullptr;
};

// 高对比度模式检测与跟随
bool IsHighContrastMode();
void SetHighContrastCallback(std::function<void(bool)> cb);

// 键盘导航增强
void EnableKeyboardNavigation(HWND hwnd); // Tab/Arrow/Enter/Esc 标准键盘交互

} // namespace a11y

// ============================================================
// Localization (i18n)
// ============================================================
namespace i18n {

struct Locale
{
    std::wstring code;                    // "zh-CN", "en-US", "ja-JP"
    std::wstring name;                    // 显示名
    std::map<std::wstring, std::wstring> strings; // key -> 翻译
    std::wstring dateFormat = L"yyyy/MM/dd";
    std::wstring timeFormat = L"HH:mm:ss";
    std::wstring numberFormat = L"#,##0.00";
    std::wstring currencySymbol = L"¥";
    std::wstring listSeparator = L",";
    std::wstring decimalPoint = L".";
    std::wstring thousandSep = L",";
    bool rtl = false;                     // 右到左语言
};

class LocalizationManager
{
public:
    static LocalizationManager& Instance();

    // 加载语言包（JSON 格式）
    bool LoadLocale(const std::wstring& jsonPath);

    // 切换语言
    void SetLocale(const std::wstring& code);

    // 当前语言
    const std::wstring& CurrentLocale() const { return m_current; }
    const Locale* Current() const;

    // 翻译查找
    std::wstring Tr(const std::wstring& key, const std::map<std::wstring, std::wstring>& args = {}) const;
    std::wstring Tr(const std::wstring& key, const std::wstring& arg1, const std::wstring& val1) const;
    std::wstring Tr(const std::wstring& key, const std::wstring& arg1, const std::wstring& val1,
                    const std::wstring& arg2, const std::wstring& val2) const;

    // 日期/时间/数字格式化
    std::wstring FormatDate(const SYSTEMTIME& st) const;
    std::wstring FormatTime(const SYSTEMTIME& st) const;
    std::wstring FormatNumber(double val, int decimals = 2) const;
    std::wstring FormatCurrency(double val) const;

    // 语言变更回调
    void OnLocaleChanged(std::function<void(const std::wstring&)> cb);

    // 可用语言列表
    const std::vector<std::wstring>& AvailableLocales() const { return m_available; }

private:
    LocalizationManager() = default;
    std::map<std::wstring, Locale> m_locales;
    std::wstring m_current = L"zh-CN";
    std::vector<std::wstring> m_available;
    std::vector<std::function<void(const std::wstring&)>> m_changeCallbacks;
};

// 便捷宏
#define _(key) air::ui::i18n::LocalizationManager::Instance().Tr(key)
#define _f(key, ...) air::ui::i18n::LocalizationManager::Instance().Tr(key, __VA_ARGS__)

} // namespace i18n

} // namespace ui
} // namespace air