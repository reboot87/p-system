#include "airui/Page.h"
#include "airui/Shell.h"

namespace air {
namespace ui {

// ============================================================
// 控件工厂 —— 创建、登记、交由 Shell 路由消息
// ============================================================
TextBox& Page::CreateTextBox(const TextBoxDesc& desc)
{
    auto w = std::make_unique<TextBox>();
    w->Init(m_shell, desc.id);
    w->m_desc  = desc;
    w->CreateNative();
    m_shell->RegisterWidget(w.get());
    m_widgets.push_back(std::move(w));
    return static_cast<TextBox&>(*m_widgets.back());
}

Button& Page::CreateButton(const ButtonDesc& desc)
{
    auto w = std::make_unique<Button>();
    w->Init(m_shell, desc.id);
    w->m_desc  = desc;
    w->CreateNative();
    m_shell->RegisterWidget(w.get());
    m_widgets.push_back(std::move(w));
    return static_cast<Button&>(*m_widgets.back());
}

ListBox& Page::CreateListBox(const ListBoxDesc& desc)
{
    auto w = std::make_unique<ListBox>();
    w->Init(m_shell, desc.id);
    w->m_desc  = desc;
    w->CreateNative();
    m_shell->RegisterWidget(w.get());
    m_widgets.push_back(std::move(w));
    return static_cast<ListBox&>(*m_widgets.back());
}

Custom& Page::CreateCustom(const CustomDesc& desc)
{
    auto w = std::make_unique<Custom>();
    w->Init(m_shell, desc.id);
    w->m_desc  = desc;
    w->CreateNative();
    m_shell->RegisterWidget(w.get());
    m_widgets.push_back(std::move(w));
    return static_cast<Custom&>(*m_widgets.back());
}

// ============================================================
// 常用操作
// ============================================================
void Page::StartTimer(UINT_PTR id, int ms)
{
    if (m_shell && m_shell->Hwnd())
        SetTimer(m_shell->Hwnd(), id, (UINT)ms, nullptr);
}

void Page::KillTimer(UINT_PTR id)
{
    if (m_shell && m_shell->Hwnd())
        ::KillTimer(m_shell->Hwnd(), id);
}

int Page::ShowMessageBox(const std::wstring& text, const std::wstring& caption,
                         UINT flags)
{
    return m_shell ? m_shell->ShowMessageBox(text, caption, flags) : 0;
}

int Page::ShowPopupMenu(POINT screenPt, const std::vector<MenuItem>& items)
{
    return m_shell ? m_shell->ShowPopupMenu(screenPt, items) : 0;
}

void Page::InvalidateRegion(const RECT& rcClient)
{
    if (m_shell) m_shell->InvalidateRegion(rcClient);
}

void Page::BatchDraw(bool begin)
{
    if (m_shell) m_shell->SetRedraw(begin);
}

const Fonts& Page::Fonts() const
{
    return m_shell->UiFonts();
}

// ============================================================
// IPage 绘制入口收编 —— HDC / DRAWITEMSTRUCT 到此为止
// ============================================================
void Page::OnPaint(HDC dc, const RECT& content)
{
    if (!m_shell) return;
    Canvas g(dc, m_shell->UiFonts());
    OnPaint(g, content);
}

bool Page::OnDrawItem(const DRAWITEMSTRUCT* dis)
{
    // 控件的 owner-draw 由 Shell → Widget 路由，页面不再参与
    (void)dis;
    return false;
}

bool Page::OnMeasureItem(MEASUREITEMSTRUCT* mis)
{
    (void)mis;
    return false;
}

} // namespace ui
} // namespace air
