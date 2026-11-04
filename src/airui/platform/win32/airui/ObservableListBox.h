#pragma once
// ============================================================
// AirUI - ObservableListBox.h
// ListBox binding helper (platform layer).
//
//   ObservableList<Entry> entries;
//   auto binder = BindListBox(list, &entries,
//       [](Canvas& g, const ListItemDraw& it, const Entry& e) { ... });
//
// This header is in platform/ because ListBox / Canvas /
// EmptyState are Win32-backed in phase 0. It moves to core once
// ListBox becomes a pure core control.
// ============================================================
#include "airui/Observable.h"
#include "airui/Widgets.h"
#include "airui/Canvas.h"
#include "airui/Controls.h"
#include <memory>

namespace air {
namespace ui {

// ------------------------------------------------------------
// ListBox 绑定辅助
// ------------------------------------------------------------
template<typename T>
class ListBoxBinder
{
public:
    using Renderer = std::function<void(Canvas&, const ListItemDraw&, const T&)>;

    ListBoxBinder(ListBox* list, ObservableList<T>* source, Renderer renderer)
        : m_list(list), m_source(source), m_renderer(std::move(renderer))
    {
        m_list->onPaintItem = [this](Canvas& g, const ListItemDraw& it) {
            if (it.index >= 0 && it.index < (int)m_source->Size()) {
                m_renderer(g, it, (*m_source)[it.index]);
            }
        };
        m_list->onPaintEmpty = [](Canvas& g, const RECT& rc) {
            EmptyState st; st.title = L"暂无数据"; g.EmptyState(rc, st);
        };

        m_source->OnChanged([this]{ Rebuild(); });
        m_source->OnItemAdded([this](size_t idx, const T&) { 
            m_list->AddItem(m_rowHeight, idx); 
        });
        m_source->OnItemRemoved([this](size_t idx, const T&) { 
            // 简化：全部重建
            Rebuild(); 
        });
    }

    void SetRowHeight(int h) { m_rowHeight = h; Rebuild(); }

private:
    ListBox* m_list = nullptr;
    ObservableList<T>* m_source = nullptr;
    Renderer m_renderer;
    int m_rowHeight = 54;

    void Rebuild() {
        m_list->Reset();
        for (size_t i = 0; i < m_source->Size(); ++i) {
            m_list->AddItem(m_rowHeight, i);
        }
    }
};

// 便捷绑定函数
template<typename T>
std::unique_ptr<ListBoxBinder<T>> BindListBox(ListBox* list, ObservableList<T>* source,
    typename ListBoxBinder<T>::Renderer renderer)
{
    return std::make_unique<ListBoxBinder<T>>(list, source, std::move(renderer));
}

} // namespace ui
} // namespace air