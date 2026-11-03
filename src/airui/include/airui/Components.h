#pragma once
// ============================================================
// AirUI — Components.h
// 复合组件库：Card / DataGrid / Form / Toolbar / Toast
// ============================================================
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "Layout.h"
#include "Canvas.h"
#include "Widgets.h"
#include "Theme.h"

namespace air {
namespace ui {

struct Icon {
    std::wstring glyph;
    COLORREF     bg    = 0;
    COLORREF     color = 0;
    int          size  = 40;
    int          radius = 8;
};

struct Badge {
    std::wstring text;
    bool         ok = true;
};

struct Action {
    int          id = 0;
    std::wstring label;
    std::wstring glyph;
    std::function<void()> callback;
    bool         primary = false;
    bool         disabled = false;
    std::wstring hotkey;
};

// ------------------------------------------------------------
// Card —— 通用卡片
// ------------------------------------------------------------
class Card
{
public:
    struct Config {
        RECT rect = {};
        Icon icon;
        std::wstring title;
        std::wstring desc;
        std::wstring number;
        std::wstring unit;
        Badge badge;
        std::wstring linkText;
        std::function<void()> onClick;
        bool disabled = false;
        bool hoverable = true;
    };

    explicit Card(const Config& cfg) : m_cfg(cfg) {}

    void Paint(Canvas& g);
    bool HitTest(POINT pt) const { return PtInRect(&m_cfg.rect, pt); }
    const RECT& GetRect() const { return m_cfg.rect; }
    void SetRect(const RECT& r) { m_cfg.rect = r; }
    void SetNumber(const std::wstring& n) { m_cfg.number = n; }
    void SetBadge(const Badge& b) { m_cfg.badge = b; }
    // Interaction methods for GUI
    void SetHover(bool hover) { m_hover = hover; }
    const Config& GetConfig() const { return m_cfg; }

private:
    Config m_cfg;
    bool   m_hover = false;
};

// ------------------------------------------------------------
// DataGrid —— 可排序/筛选/分页的表格
// ------------------------------------------------------------
template<typename T>
class DataGrid
{
public:
    struct Column {
        std::wstring id;
        std::wstring header;
        int          width = 120;
        int          minWidth = 80;
        int          maxWidth = -1;
        bool         sortable = true;
        std::function<std::wstring(const T&)> render;
        std::function<int(const T&, const T&)> compare;
    };

    struct Config {
        std::vector<Column> columns;
        std::function<void(const T&)> onDoubleClick;
        std::function<void(POINT, const T*)> onContextMenu;
        std::function<bool(int, POINT, const T&)> onCellClick;
        int rowHeight = 54;
        bool showHeader = true;
        bool striped = true;
        bool hoverHighlight = true;
    };

    explicit DataGrid(ListBox* list, const Config& cfg)
        : m_list(list), m_cfg(cfg)
    {
        m_list->onPaintItem = [this](Canvas& g, const ListItemDraw& it) { PaintRow(g, it); };
        m_list->onPaintEmpty = [this](Canvas& g, const RECT& rc) { PaintEmpty(g, rc); };
        m_list->onDoubleClicked = [this](int sel) { OnDoubleClick(sel); };
        m_list->onItemClicked = [this](int idx, POINT pt) { return OnCellClick(idx, pt); };
        m_list->onContextMenu = [this](POINT pt, int idx) { OnContextMenu(pt, idx); };
    }

    void Bind(const std::vector<T>& data) {
        m_data = data;
        m_filtered = data;
        ApplyFilter();
    }

    void SetFilter(std::function<bool(const T&)> filter) {
        m_filter = std::move(filter);
        ApplyFilter();
    }

    void SetSort(int colIdx, bool asc = true) {
        if (colIdx < 0 || colIdx >= (int)m_cfg.columns.size()) return;
        m_sortCol = colIdx;
        m_sortAsc = asc;
        ApplyFilter();
    }

    void Refresh() { ApplyFilter(); }
    ListBox* GetListBox() { return m_list; }

private:
    ListBox* m_list = nullptr;
    Config   m_cfg;
    std::vector<T> m_data;
    std::vector<T> m_filtered;
    std::function<bool(const T&)> m_filter;
    int m_sortCol = -1;
    bool m_sortAsc = true;

    void ApplyFilter() {
        m_filtered.clear();
        for (auto& item : m_data) {
            if (!m_filter || m_filter(item)) m_filtered.push_back(item);
        }
        if (m_sortCol >= 0 && m_sortCol < (int)m_cfg.columns.size()) {
            auto& col = m_cfg.columns[m_sortCol];
            if (col.compare) {
                std::sort(m_filtered.begin(), m_filtered.end(),
                    [&](const T& a, const T& b) {
                        int r = col.compare(a, b);
                        return m_sortAsc ? r < 0 : r > 0;
                    });
            }
        }
        RebuildList();
    }

    void RebuildList() {
        m_list->Reset();
        for (size_t i = 0; i < m_filtered.size(); ++i) {
            m_list->AddItem(m_cfg.rowHeight, i);
        }
    }

    void PaintRow(Canvas& g, const ListItemDraw& it) {
        if (it.index < 0 || it.index >= (int)m_filtered.size()) return;
        const T& item = m_filtered[it.data];
        const auto& p = Pal();
        const auto& m = Mx();

        if (it.selected) {
            g.FillRound(it.rc, p.selectFill, 0);
            g.Stroke(it.rc, p.selectRing, 0);
        } else if (it.hovered && m_cfg.hoverHighlight) {
            g.FillRound(it.rc, p.hover, 0);
        } else if (m_cfg.striped && (it.index % 2 == 1)) {
            g.FillRound(it.rc, RGB(248, 249, 250), 0);
        } else {
            g.FillRound(it.rc, p.surface, 0);
        }

        int x = it.rc.left + m.unit;
        for (size_t c = 0; c < m_cfg.columns.size(); ++c) {
            const auto& col = m_cfg.columns[c];
            int colW = col.width > 0 ? col.width : 120;
            RECT cell = { x, it.rc.top, x + colW, it.rc.bottom };
            if (col.render) {
                std::wstring txt = col.render(item);
                g.TextRow(cell, txt, FontRole::Body,
                    it.selected ? p.textInverse : p.text);
            }
            x += colW + m.unit;
        }
    }

    void PaintEmpty(Canvas& g, const RECT& rc) {
        EmptyState st;
        st.title = L"暂无数据";
        st.steps = L"暂无符合条件的记录";
        st.foot  = L"尝试调整筛选条件";
        g.EmptyState(rc, st);
    }

    void OnDoubleClick(int sel) {
        if (sel >= 0 && sel < (int)m_filtered.size() && m_cfg.onDoubleClick) {
            m_cfg.onDoubleClick(m_filtered[sel]);
        }
    }

    bool OnCellClick(int idx, POINT ptInItem) {
        if (idx < 0 || idx >= (int)m_filtered.size()) return false;
        if (m_cfg.onCellClick) {
            return m_cfg.onCellClick(idx, ptInItem, m_filtered[idx]);
        }
        return false;
    }

    void OnContextMenu(POINT screenPt, int idx) {
        const T* item = nullptr;
        if (idx >= 0 && idx < (int)m_filtered.size()) item = &m_filtered[idx];
        if (m_cfg.onContextMenu) m_cfg.onContextMenu(screenPt, item);
    }
};

// ------------------------------------------------------------
// Form —— 声明式表单
// ------------------------------------------------------------
class Form
{
public:
    struct Field {
        std::wstring label;
        std::wstring placeholder;
        Widget* input = nullptr;
        FontRole fontRole = FontRole::Body;
        std::function<bool(const std::wstring&)> validate;
        std::wstring errorMsg;
        int width = 0;
        bool required = false;
    };

    struct Config {
        std::vector<Field> fields;
        Button* submitBtn = nullptr;
        std::function<void(std::map<std::wstring, std::wstring>)> onSubmit;
        int labelWidth = 120;
        int gapV = 16;
        int gapH = 16;
    };

    explicit Form(const Config& cfg) : m_cfg(cfg) {}

    // 重命名为 Arrange 避免与 Layout 类冲突
    void Arrange(const RECT& rc) {
        m_rc = rc;
        int y = rc.top;
        for (auto& f : m_cfg.fields) {
            RECT inputRc = { rc.left + m_cfg.labelWidth + m_cfg.gapH, y,
                             rc.right, y + 32 };
            if (f.input) f.input->Move(inputRc);
            y += 32 + m_cfg.gapV;
        }
        if (m_cfg.submitBtn) {
            RECT btnRc = { rc.right - 120, y, rc.right, y + 36 };
            m_cfg.submitBtn->Move(btnRc);
        }
    }

    bool Validate() {
        std::map<std::wstring, std::wstring> values;
        for (auto& f : m_cfg.fields) {
            std::wstring val;
            if (auto tb = dynamic_cast<TextBox*>(f.input)) {
                val = tb->Text();
            }
            if (f.required && val.empty()) return false;
            if (f.validate && !f.validate(val)) return false;
            values[f.label] = val;
        }
        m_values = values;
        return true;
    }

    void Submit() {
        if (Validate() && m_cfg.onSubmit) {
            m_cfg.onSubmit(m_values);
        }
    }

    const std::map<std::wstring, std::wstring>& Values() const { return m_values; }

private:
    Config m_cfg;
    RECT m_rc = {};
    std::map<std::wstring, std::wstring> m_values;
};

// ------------------------------------------------------------
// Toolbar —— 顶部工具栏
// ------------------------------------------------------------
class Toolbar
{
public:
    struct Config {
        TextBox* search = nullptr;
        std::wstring searchPlaceholder = L"搜索…";
        std::vector<Action> actions;
        std::vector<std::pair<std::wstring, std::wstring>> breadcrumbs;
        std::function<void(const std::wstring&)> onSearch;
        std::function<void(const std::wstring&)> onBreadcrumbClick;
    };

    explicit Toolbar(const Config& cfg) : m_cfg(cfg) {
        if (m_cfg.search) {
            m_cfg.search->SetPlaceholder(m_cfg.searchPlaceholder);
            m_cfg.search->OnKey([this](UINT vk, bool ctrl, bool shift) {
                if (vk == VK_RETURN && m_cfg.onSearch) {
                    m_cfg.onSearch(m_cfg.search->Text());
                    return true;
                }
                return false;
            });
        }
    }

    void Paint(Canvas& g, const RECT& rc) {
        const auto& p = Pal();
        const auto& m = Mx();
        g.FillRound(rc, p.surface, 0);
        g.HLine(rc.left, rc.right, rc.bottom, p.split);

        int x = rc.left + m.unit;
        for (size_t i = 0; i < m_cfg.breadcrumbs.size(); ++i) {
            if (i > 0) {
                g.TextRow({x, rc.top, x+20, rc.bottom}, L"›", FontRole::Meta, p.textFaint);
                x += 24;
            }
            auto& bc = m_cfg.breadcrumbs[i];
            int w = g.MeasureWidth(bc.first, FontRole::Body) + 16;
            RECT r = {x, rc.top, x+w, rc.bottom};
            g.TextRowCenter(r, bc.first, FontRole::Body, i+1==m_cfg.breadcrumbs.size()?p.text:p.textSub);
            x += w + 8;
        }
        x += m.unit * 2;

        int rightX = rc.right - m.unit;
        for (auto it = m_cfg.actions.rbegin(); it != m_cfg.actions.rend(); ++it) {
            int w = g.MeasureWidth(it->label, FontRole::Button) + 24;
            RECT r = { rightX - w, rc.top + 4, rightX, rc.bottom - 4 };
            BtnStyle style = it->primary ? BtnStyle::Primary : BtnStyle::Secondary;
            g.Button(r, it->label, style, false, false, it->disabled);
            rightX = r.left - 8;
        }

        if (m_cfg.search) {
            RECT sr = { x, rc.top + 4, rightX - 8, rc.bottom - 4 };
            m_cfg.search->Move(sr);
        }
    }

    void SetBreadcrumbs(const std::vector<std::pair<std::wstring, std::wstring>>& bc) {
        m_cfg.breadcrumbs = bc;
    }

private:
    Config m_cfg;
};

} // namespace ui
} // namespace air