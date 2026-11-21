#include "airui/Accessibility.h"
#include "airui/Widgets.h"
#include <oleacc.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <regex>
#include <map>
#include <new>

#pragma comment(lib, "oleacc.lib")
#pragma comment(lib, "oleaut32.lib")

namespace air {
namespace ui {

// ============================================================
// Accessibility 实现
// ============================================================
namespace a11y {

// ------------------------------------------------------------
// 每窗口无障碍属性。存放于进程级 map（不挂 SetProp、不 new 裸指针，
// 修复旧实现「每个窗口泄漏一个 std::wstring 且无读取方」的问题）。
// g_lock 串行化所有表访问；MSAA 客户端通过 marshaling 读取属性时不
// 回调本表，故无重入风险。
// ------------------------------------------------------------
struct WidgetAccProps
{
    Role         role     = ROLE_UNKNOWN;
    DWORD        state    = 0;     // State 位组合
    std::wstring name;
    std::wstring desc;
    std::wstring value;
    std::wstring valueMin;         // 滑块/进度条的取值范围（供 UIA 层读取）
    std::wstring valueMax;
};
static std::map<HWND, WidgetAccProps> g_accProps;
static SRWLOCK g_lock = SRWLOCK_INIT;

static WidgetAccProps GetProps(HWND hwnd)
{
    WidgetAccProps p;
    AcquireSRWLockShared(&g_lock);
    auto it = g_accProps.find(hwnd);
    if (it != g_accProps.end()) p = it->second;
    ReleaseSRWLockShared(&g_lock);
    return p;
}

static void PutProps(HWND hwnd, const WidgetAccProps& p)
{
    AcquireSRWLockExclusive(&g_lock);
    if (p.role == ROLE_UNKNOWN && p.state == 0 && p.name.empty() &&
        p.desc.empty() && p.value.empty() &&
        p.valueMin.empty() && p.valueMax.empty())
        g_accProps.erase(hwnd);          // 全空 -> 收掉表项
    else
        g_accProps[hwnd] = p;            // 值拷贝进表
    ReleaseSRWLockExclusive(&g_lock);
}

static void EraseProps(HWND hwnd)
{
    AcquireSRWLockExclusive(&g_lock);
    g_accProps.erase(hwnd);
    ReleaseSRWLockExclusive(&g_lock);
}

// 叶子 provider 只服务 CHILDID_SELF（部分客户端以 VT_EMPTY 寻址自身）。
static bool IsSelf(const VARIANT& v)
{
    if (v.vt == VT_EMPTY) return true;
    return v.vt == VT_I4 && v.lVal == CHILDID_SELF;
}

// ------------------------------------------------------------
// AirUI 的 IAccessible 实现（每窗口一个叶子 provider）。
//
// 生命周期：WM_GETOBJECT 时创建，LresultFromObject 内部持有自身引用，
// marshaling 层负责释放；本侧 Release 只丢掉创建引用。窗口销毁由
// ForgetWindow() 清属性表，之后的 WM_GETOBJECT 返回 0 交给系统默认
// provider。
// ------------------------------------------------------------
class AccessibleObject : public IAccessible
{
public:
    explicit AccessibleObject(HWND hwnd) : m_hwnd(hwnd) {}

    // ---- IUnknown ----
    IFACEMETHODIMP QueryInterface(REFIID riid, void** out) override
    {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (riid == IID_IUnknown || riid == IID_IDispatch ||
            riid == IID_IAccessible) {
            *out = static_cast<IAccessible*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return (ULONG)InterlockedIncrement(&m_refs);
    }
    IFACEMETHODIMP_(ULONG) Release() override
    {
        const ULONG r = (ULONG)InterlockedDecrement(&m_refs);
        if (r == 0) delete this;
        return r;
    }

    // ---- IDispatch（oleacc 客户端会 QI；类型库未随库发布，动态调用
    //      一律 E_NOTIMPL，静态属性走 IAccessible vtable） ----
    IFACEMETHODIMP GetTypeInfoCount(UINT* pctinfo) override
    { if (pctinfo) *pctinfo = 0; return S_OK; }
    IFACEMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override
    { return E_NOTIMPL; }
    IFACEMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override
    { return E_NOTIMPL; }
    IFACEMETHODIMP Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*,
                          VARIANT*, EXCEPINFO*, UINT*) override
    { return E_NOTIMPL; }

    // ---- IAccessible ----
    IFACEMETHODIMP get_accParent(IDispatch** ppdispParent) override
    {
        if (!ppdispParent) return E_POINTER;
        *ppdispParent = nullptr;
        if (!IsWindow(m_hwnd)) return E_FAIL;
        const HWND parent = GetAncestor(m_hwnd, GA_PARENT);
        if (!parent) return S_FALSE;   // 顶层：无父对象
        AccessibleObject* acc = new (std::nothrow) AccessibleObject(parent);
        if (!acc) return E_OUTOFMEMORY;
        const HRESULT hr = acc->QueryInterface(IID_IDispatch,
                                               (void**)ppdispParent);
        acc->Release();
        return hr;
    }
    IFACEMETHODIMP get_accChildCount(long* pcountChildren) override
    {
        if (!pcountChildren) return E_POINTER;
        *pcountChildren = 0;
        return IsWindow(m_hwnd) ? S_OK : E_FAIL;
    }
    IFACEMETHODIMP get_accChild(VARIANT varChild, IDispatch** ppdispChild) override
    {
        if (!ppdispChild) return E_POINTER;
        *ppdispChild = nullptr;
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (!IsSelf(varChild)) return E_INVALIDARG;
        return S_FALSE;   // 叶子：无子对象
    }
    IFACEMETHODIMP get_accName(VARIANT varChild, BSTR* pszName) override
    {
        if (!pszName) return E_POINTER;
        *pszName = nullptr;
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (!IsSelf(varChild)) return E_INVALIDARG;
        const WidgetAccProps p = GetProps(m_hwnd);
        if (p.name.empty()) return S_FALSE;
        *pszName = SysAllocStringLen(p.name.c_str(), (UINT)p.name.size());
        return *pszName ? S_OK : E_OUTOFMEMORY;
    }
    IFACEMETHODIMP get_accDescription(VARIANT varChild, BSTR* pszDescription) override
    {
        if (!pszDescription) return E_POINTER;
        *pszDescription = nullptr;
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (!IsSelf(varChild)) return E_INVALIDARG;
        const WidgetAccProps p = GetProps(m_hwnd);
        if (p.desc.empty()) return S_FALSE;
        *pszDescription = SysAllocStringLen(p.desc.c_str(), (UINT)p.desc.size());
        return *pszDescription ? S_OK : E_OUTOFMEMORY;
    }
    IFACEMETHODIMP get_accValue(VARIANT varChild, BSTR* pszValue) override
    {
        if (!pszValue) return E_POINTER;
        *pszValue = nullptr;
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (!IsSelf(varChild)) return E_INVALIDARG;
        const WidgetAccProps p = GetProps(m_hwnd);
        if (p.value.empty()) return S_FALSE;
        *pszValue = SysAllocStringLen(p.value.c_str(), (UINT)p.value.size());
        return *pszValue ? S_OK : E_OUTOFMEMORY;
    }
    IFACEMETHODIMP get_accRole(VARIANT varChild, VARIANT* pvarRole) override
    {
        if (!pvarRole) return E_POINTER;
        VariantInit(pvarRole);
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (!IsSelf(varChild)) return E_INVALIDARG;
        const WidgetAccProps p = GetProps(m_hwnd);
        if (p.role == ROLE_UNKNOWN) return S_FALSE;
        pvarRole->vt = VT_I4;
        pvarRole->lVal = p.role;
        return S_OK;
    }
    IFACEMETHODIMP get_accState(VARIANT varChild, VARIANT* pvarState) override
    {
        if (!pvarState) return E_POINTER;
        VariantInit(pvarState);
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (!IsSelf(varChild)) return E_INVALIDARG;
        const WidgetAccProps p = GetProps(m_hwnd);
        // 基础状态从窗口实时推导（真实来源），属性表位做增量映射。
        DWORD st = 0;
        if (!IsWindowVisible(m_hwnd))  st |= STATE_SYSTEM_INVISIBLE;
        if (!IsWindowEnabled(m_hwnd))  st |= STATE_SYSTEM_UNAVAILABLE;
        if (GetFocus() == m_hwnd)      st |= STATE_SYSTEM_FOCUSED;
        if (p.state & STATE_FOCUSED)    st |= STATE_SYSTEM_FOCUSED;
        if (p.state & STATE_CHECKED)    st |= STATE_SYSTEM_CHECKED;
        if (p.state & STATE_MIXED)      st |= STATE_SYSTEM_MIXED;
        if (p.state & STATE_SELECTED)   st |= STATE_SYSTEM_SELECTED;
        if (p.state & STATE_PRESSED)    st |= STATE_SYSTEM_PRESSED;
        if (p.state & STATE_READONLY)   st |= STATE_SYSTEM_READONLY;
        if (p.state & STATE_HOTTRACKED) st |= STATE_SYSTEM_HOTTRACKED;
        if (p.state & STATE_DEFAULT)    st |= STATE_SYSTEM_DEFAULT;
        if (p.state & STATE_EXPANDED)   st |= STATE_SYSTEM_EXPANDED;
        if (p.state & STATE_COLLAPSED)  st |= STATE_SYSTEM_COLLAPSED;
        if (p.state & STATE_BUSY)       st |= STATE_SYSTEM_BUSY;
        if (p.state & STATE_OFFSCREEN)  st |= STATE_SYSTEM_OFFSCREEN;
        if (p.state & STATE_HASPOPUP)   st |= STATE_SYSTEM_HASPOPUP;
        pvarState->vt = VT_I4;
        pvarState->lVal = (LONG)st;
        return S_OK;
    }
    IFACEMETHODIMP accSelect(long flagsSel, VARIANT varChild) override
    {
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (!IsSelf(varChild)) return E_INVALIDARG;
        if (flagsSel & SELFLAG_TAKEFOCUS) {
            SetFocus(m_hwnd);
            return S_OK;
        }
        return S_FALSE;
    }
    IFACEMETHODIMP get_accFocus(VARIANT* pvarFocus) override
    {
        if (!pvarFocus) return E_POINTER;
        VariantInit(pvarFocus);
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (GetFocus() == m_hwnd) {
            pvarFocus->vt = VT_I4;
            pvarFocus->lVal = CHILDID_SELF;
        }
        return S_OK;   // VT_EMPTY = 焦点不在本对象
    }
    IFACEMETHODIMP accLocation(long* pxLeft, long* pyTop,
                               long* pcxWidth, long* pcyHeight,
                               VARIANT varChild) override
    {
        if (!pxLeft || !pyTop || !pcxWidth || !pcyHeight) return E_POINTER;
        *pxLeft = *pyTop = *pcxWidth = *pcyHeight = 0;
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (!IsSelf(varChild)) return E_INVALIDARG;
        RECT rc{};
        if (!GetWindowRect(m_hwnd, &rc)) return E_FAIL;
        *pxLeft = rc.left;  *pyTop = rc.top;
        *pcxWidth = rc.right - rc.left;
        *pcyHeight = rc.bottom - rc.top;
        return S_OK;
    }
    IFACEMETHODIMP get_accDefaultAction(VARIANT varChild,
                                        BSTR* pszDefaultAction) override
    {
        if (!pszDefaultAction) return E_POINTER;
        *pszDefaultAction = nullptr;
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (!IsSelf(varChild)) return E_INVALIDARG;
        const WidgetAccProps p = GetProps(m_hwnd);
        if (p.role != ROLE_BUTTON && p.role != ROLE_MENUITEM) return S_FALSE;
        *pszDefaultAction = SysAllocString(L"Press");
        return *pszDefaultAction ? S_OK : E_OUTOFMEMORY;
    }
    IFACEMETHODIMP accDoDefaultAction(VARIANT varChild) override
    {
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (!IsSelf(varChild)) return E_INVALIDARG;
        const WidgetAccProps p = GetProps(m_hwnd);
        if (p.role != ROLE_BUTTON && p.role != ROLE_MENUITEM) return S_FALSE;
        const int id = GetDlgCtrlID(m_hwnd);
        const HWND parent = GetParent(m_hwnd);
        if (id <= 0 || !parent) return S_FALSE;
        // 复用 Shell/Page 的 WM_COMMAND 路由，与真实点击同构。
        SendMessageW(parent, WM_COMMAND,
                     MAKEWPARAM(id, BN_CLICKED), (LPARAM)m_hwnd);
        return S_OK;
    }
    // 薄 provider 不支持的查询：S_FALSE（值语义）/ E_NOTIMPL。
    IFACEMETHODIMP get_accKeyboardShortcut(VARIANT, BSTR* psz) override
    { if (psz) *psz = nullptr; return S_FALSE; }
    IFACEMETHODIMP get_accHelp(VARIANT, BSTR* pszHelp) override
    { if (pszHelp) *pszHelp = nullptr; return S_FALSE; }
    IFACEMETHODIMP get_accHelpTopic(BSTR*, VARIANT, long*) override
    { return E_NOTIMPL; }
    IFACEMETHODIMP get_accSelection(VARIANT* pvarSelection) override
    { if (pvarSelection) VariantInit(pvarSelection); return S_FALSE; }
    // 导航：叶子对象没有兄弟/子节点可导航，S_FALSE（未处理）。
    IFACEMETHODIMP accNavigate(long, VARIANT varStart, VARIANT* pvarEndUpAt) override
    {
        if (!pvarEndUpAt) return E_POINTER;
        VariantInit(pvarEndUpAt);
        if (!IsWindow(m_hwnd)) return E_FAIL;
        if (!IsSelf(varStart)) return E_INVALIDARG;
        return S_FALSE;
    }
    // 命中测试：叶子对象只回答「点在我上面 -> CHILDID_SELF」。
    IFACEMETHODIMP accHitTest(long xLeft, long yTop, VARIANT* pvarChild) override
    {
        if (!pvarChild) return E_POINTER;
        VariantInit(pvarChild);
        if (!IsWindow(m_hwnd)) return E_FAIL;
        RECT rc{};
        if (!GetWindowRect(m_hwnd, &rc)) return E_FAIL;   // xLeft/yTop 是屏幕坐标
        const POINT pt{ xLeft, yTop };
        if (PtInRect(&rc, pt)) {
            pvarChild->vt = VT_I4;
            pvarChild->lVal = CHILDID_SELF;
        }
        return S_OK;   // VT_EMPTY = 点不在本对象
    }
    // 旧式 put 接口（MSAA 已弃用）：属性只经 SetAccessible* API 写入。
    IFACEMETHODIMP put_accName(VARIANT, BSTR) override { return E_NOTIMPL; }
    IFACEMETHODIMP put_accValue(VARIANT, BSTR) override { return E_NOTIMPL; }

private:
    ~AccessibleObject() = default;   // COM 引用计数，不公开析构
    HWND m_hwnd = nullptr;
    LONG m_refs = 1;
};

// ------------------------------------------------------------
// WM_GETOBJECT 管线（由宿主 WndProc / 子类过程转发）
// ------------------------------------------------------------
LRESULT HandleGetObject(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    if (!hwnd || !IsWindow(hwnd)) return 0;
    const LONG objId = (LONG)(LONG_PTR)lParam;
    // 只接管 CLIENT / WINDOW；UNICOBJ / OEMID 等原样交回系统。
    if (objId != OBJID_CLIENT && objId != OBJID_WINDOW) return 0;
    AccessibleObject* acc = new (std::nothrow) AccessibleObject(hwnd);
    if (!acc) return 0;
    // LresultFromObject 会 AddRef 并做调用线程 <-> 窗口线程校验。
    const LRESULT res = LresultFromObject(IID_IAccessible, wParam, acc);
    acc->Release();   // 只释放创建引用
    return res;
}

LRESULT HandleGetObjectIfAnnotated(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    if (!hwnd || !IsWindow(hwnd)) return 0;
    const LONG objId = (LONG)(LONG_PTR)lParam;
    if (objId != OBJID_CLIENT && objId != OBJID_WINDOW) return 0;
    {
        AcquireSRWLockShared(&g_lock);
        const bool annotated = g_accProps.find(hwnd) != g_accProps.end();
        ReleaseSRWLockShared(&g_lock);
        if (!annotated) return 0;   // 未标注：交给系统原生 provider
    }
    return HandleGetObject(hwnd, wParam, lParam);
}

void ForgetWindow(HWND hwnd)
{
    if (!hwnd) return;
    EraseProps(hwnd);
}

void SetAccessibleRole(HWND hwnd, Role role)
{
    if (!hwnd) return;
    WidgetAccProps p = GetProps(hwnd);
    p.role = role;
    PutProps(hwnd, p);
}

void SetAccessibleName(HWND hwnd, const std::wstring& name)
{
    if (!hwnd) return;
    WidgetAccProps p = GetProps(hwnd);
    p.name = name;
    PutProps(hwnd, p);
    // 屏幕阅读器监听 NAMECHANGE 朗读新文本（LiveRegion 依赖此事件）。
    NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, hwnd, OBJID_CLIENT, CHILDID_SELF);
}

void SetAccessibleDescription(HWND hwnd, const std::wstring& desc)
{
    if (!hwnd) return;
    WidgetAccProps p = GetProps(hwnd);
    p.desc = desc;
    PutProps(hwnd, p);
    // MSAA 无 description 专用事件；UIA 桥在读取
    // UIA_FullDescriptionPropertyId 时取到最新值即可。
}

void SetAccessibleState(HWND hwnd, State state, bool on)
{
    if (!hwnd) return;
    WidgetAccProps p = GetProps(hwnd);
    if (on) p.state |= state; else p.state &= ~(DWORD)state;
    PutProps(hwnd, p);
    NotifyWinEvent(EVENT_OBJECT_STATECHANGE, hwnd, OBJID_CLIENT, CHILDID_SELF);
}

void SetAccessibleValue(HWND hwnd, const std::wstring& value,
                        const std::wstring& min, const std::wstring& max)
{
    if (!hwnd) return;
    WidgetAccProps p = GetProps(hwnd);
    p.value    = value;
    p.valueMin = min;
    p.valueMax = max;
    PutProps(hwnd, p);
    NotifyWinEvent(EVENT_OBJECT_VALUECHANGE, hwnd, OBJID_CLIENT, CHILDID_SELF);
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
        0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (m_hwnd) {
        SetAccessibleRole(m_hwnd, ROLE_ALERT);
        SetAccessibleName(m_hwnd, L"Live Region");   // 同时把窗口标注进属性表
    }
}

LiveRegion::~LiveRegion()
{
    if (m_hwnd) {
        ForgetWindow(m_hwnd);   // 先清属性表再销毁窗口
        DestroyWindow(m_hwnd);
    }
}

void LiveRegion::Announce(const std::wstring& text, bool polite)
{
    if (!m_hwnd) return;
    SetAssertive(!polite);   // LiveSetting 跟随 polite 参数
    SetAccessibleName(m_hwnd, text);   // 自带 EVENT_OBJECT_NAMECHANGE
    // 高优先级播报追加 ALERT 事件：Narrator 立即打断当前朗读；
    // polite 只依赖 NAMECHANGE + LiveSetting=polite 的正常队列。
    if (!polite)
        NotifyWinEvent(EVENT_SYSTEM_ALERT, m_hwnd, OBJID_CLIENT, CHILDID_SELF);
}

void LiveRegion::SetAssertive(bool assertive)
{
    if (!m_hwnd) return;
    // MSAA 没有 LiveSetting 的等价物；把标记存到窗口属性，供未来的
    // UIA provider（IRawElementProviderSimple::GetPropertyValue(
    // UIA_LiveSettingPropertyId)）直接读取。Assertive 播报的即时送达
    // 由 Announce() 的 EVENT_SYSTEM_ALERT 承担。
    SetPropW(m_hwnd, L"AirUI_A11y_LiveSetting",
             (HANDLE)(ULONG_PTR)(assertive ? 1 : 0));
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
    // TODO(a11y): 监听 WM_SETTINGCHANGE 的占位——当前为定时轮询，未真正监听
    // 系统设置变更消息；真实实现见 docs/ROADMAP.md。
}

void EnableKeyboardNavigation(HWND hwnd)
{
    // TODO(a11y): 子类化处理 Tab/Arrow/Enter/Esc 的占位——当前只设置
    // WS_TABSTOP，未实现真正的键盘导航；真实实现见 docs/ROADMAP.md。
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
