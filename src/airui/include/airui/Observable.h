#pragma once
// ============================================================
// AirUI — Observable.h
// 轻量级响应式数据绑定：ObservableList / ObservableValue
//
// 用法：
//     ObservableList<UiEntry> entries;
//     entries.OnChanged([&]{ grid.Refresh(); });
//     entries.Set(newData);  // 自动触发回调
//
//     ObservableValue<int> count(0);
//     count.OnChanged([&](int v){ status.SetText(L"共 " + to_wstring(v) + L" 条"); });
//     count = 42;
// ============================================================
#include <functional>
#include <vector>
#include <algorithm>
#include <memory>

namespace air {
namespace ui {

// ------------------------------------------------------------
// ObservableValue<T> — 单值观察
// ------------------------------------------------------------
template<typename T>
class ObservableValue
{
public:
    using Callback = std::function<void(const T&)>;

    ObservableValue() = default;
    explicit ObservableValue(const T& v) : m_value(v) {}

    // 隐式转换
    operator const T& () const { return m_value; }
    const T& Get() const { return m_value; }

    // 赋值触发通知
    ObservableValue& operator=(const T& v) {
        if (!Equal(m_value, v)) {
            m_value = v;
            Notify();
        }
        return *this;
    }

    // 批量设置（不触发），最后手动 Notify()
    void SetSilent(const T& v) { m_value = v; }
    void Notify() { for (auto& cb : m_callbacks) cb(m_value); }

    // 订阅
    void OnChanged(Callback cb) { m_callbacks.push_back(std::move(cb)); }
    void ClearCallbacks() { m_callbacks.clear(); }

private:
    T m_value{};
    std::vector<Callback> m_callbacks;

    static bool Equal(const T& a, const T& b) {
        if constexpr (std::is_pointer_v<T>) return a == b;
        else if constexpr (std::is_same_v<T, std::wstring>) return a == b;
        else if constexpr (std::is_same_v<T, std::string>) return a == b;
        else return a == b;
    }
};

// ------------------------------------------------------------
// ObservableList<T> — 列表观察
// ------------------------------------------------------------
template<typename T>
class ObservableList
{
public:
    using Callback = std::function<void()>;
    using ItemCallback = std::function<void(size_t, const T&)>;

    ObservableList() = default;
    explicit ObservableList(std::vector<T> v) : m_items(std::move(v)) {}

    // 只读访问
    const std::vector<T>& Get() const { return m_items; }
    const T& operator[](size_t i) const { return m_items[i]; }
    size_t Size() const { return m_items.size(); }
    bool Empty() const { return m_items.empty(); }
    auto begin() const { return m_items.begin(); }
    auto end() const { return m_items.end(); }

    // 修改操作（自动通知）
    void Set(std::vector<T> v) {
        m_items = std::move(v);
        Notify();
    }

    void PushBack(const T& item) {
        m_items.push_back(item);
        NotifyItemAdded(m_items.size() - 1, m_items.back());
        Notify();
    }

    void PushBack(T&& item) {
        m_items.push_back(std::move(item));
        NotifyItemAdded(m_items.size() - 1, m_items.back());
        Notify();
    }

    void Insert(size_t idx, const T& item) {
        if (idx > m_items.size()) idx = m_items.size();
        m_items.insert(m_items.begin() + idx, item);
        NotifyItemAdded(idx, m_items[idx]);
        Notify();
    }

    void RemoveAt(size_t idx) {
        if (idx >= m_items.size()) return;
        m_items.erase(m_items.begin() + idx);
        NotifyItemRemoved(idx);
        Notify();
    }

    void Remove(const T& item) {
        auto it = std::find(m_items.begin(), m_items.end(), item);
        if (it != m_items.end()) {
            size_t idx = it - m_items.begin();
            m_items.erase(it);
            NotifyItemRemoved(idx);
            Notify();
        }
    }

    void Clear() {
        if (!m_items.empty()) {
            m_items.clear();
            Notify();
        }
    }

    // 批量操作（单次通知）
    template<typename Fn>
    void Batch(Fn&& fn) {
        m_batching = true;
        fn(*this);
        m_batching = false;
        if (m_dirty) { m_dirty = false; Notify(); }
    }

    // 订阅
    void OnChanged(Callback cb) { m_callbacks.push_back(std::move(cb)); }
    void OnItemAdded(ItemCallback cb) { m_itemAdded.push_back(std::move(cb)); }
    void OnItemRemoved(ItemCallback cb) { m_itemRemoved.push_back(std::move(cb)); }
    void ClearCallbacks() { m_callbacks.clear(); m_itemAdded.clear(); m_itemRemoved.clear(); }

private:
    std::vector<T> m_items;
    std::vector<Callback> m_callbacks;
    std::vector<ItemCallback> m_itemAdded;
    std::vector<ItemCallback> m_itemRemoved;
    bool m_batching = false;
    bool m_dirty = false;

    void Notify() {
        if (m_batching) { m_dirty = true; return; }
        for (auto& cb : m_callbacks) cb();
    }
    void NotifyItemAdded(size_t idx, const T& item) {
        if (m_batching) return;
        for (auto& cb : m_itemAdded) cb(idx, item);
    }
    void NotifyItemRemoved(size_t idx, const T&) {
        if (m_batching) return;
        for (auto& cb : m_itemRemoved) cb(idx, T{});
    }
};

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