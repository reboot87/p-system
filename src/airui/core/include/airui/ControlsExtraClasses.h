#pragma once
// ============================================================
// AirUI - ControlsExtraClasses.h
//
// Object layer for the controls declared in ControlsExtra.h.
// Every widget owns its state, derives from Control and can be
// hosted by ControlHost for event routing, tab focus and the
// accessible tree. The free Paint*/Measure*/Hit* functions are
// kept as a source-compatible drawing kernel.
// ============================================================
#include "airui/FrameworkCore.h"
#include "airui/ControlsExtra.h"

namespace air {
namespace ui {
namespace widgets {

// ---- Card ----
class Card : public Control
{
public:
    CardState state{};

    Card() { SetRole(ControlRole::Custom); }

    CardLayout Measure(const Metrics& mx) const;
    CardHit HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Card");
    }
};

// ---- EmptyView ----
class EmptyView : public Control
{
public:
    EmptyViewState state{};

    EmptyView() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"EmptyView");
    }
};

// ---- StatusTag ----
class StatusTag : public Control
{
public:
    StatusTagState state{};

    StatusTag() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"StatusTag");
    }
};

// ---- KbdChip ----
class KbdChip : public Control
{
public:
    KbdChipState state{};

    KbdChip() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"KbdChip");
    }
};

// ---- Toolbar ----
class Toolbar : public Control
{
public:
    ToolbarState state{};

    Toolbar() { SetRole(ControlRole::Custom); }

    ToolbarLayout Measure(const Metrics& mx) const;
    ToolbarHit HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Toolbar");
    }
};

// ---- Form ----
class Form : public Control
{
public:
    FormState state{};

    Form() { SetRole(ControlRole::Group); }

    FormLayout Measure(const Metrics& mx) const;
    FormHit HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Form");
    }
};

// ---- DataGrid ----
class DataGrid : public Control
{
public:
    DataGridState state{};

    DataGrid() { SetRole(ControlRole::Custom); }

    DataGridLayout Measure(const Metrics& mx) const;
    DataGridHit HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"DataGrid");
    }
};

// ---- ListData ----
class ListData : public Control
{
public:
    ListDataState state{};

    ListData() { SetRole(ControlRole::Custom); }

    int RowAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"ListData");
    }
};

// ---- Custom ----
class Custom : public Control
{
public:
    CustomState state{};

    Custom() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Custom");
    }
};

// ---- SearchBox ----
class SearchBox : public Control
{
public:
    SearchBoxState state{};

    SearchBox() { SetRole(ControlRole::TextBox); }

    SearchBoxHit HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"SearchBox");
    }
};

// ---- PasswordBox ----
class PasswordBox : public Control
{
public:
    PasswordBoxState state{};

    PasswordBox() { SetRole(ControlRole::TextBox); }

    PasswordBoxHit HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"PasswordBox");
    }
};

// ---- TextArea ----
class TextArea : public Control
{
public:
    TextAreaState state{};

    TextArea() { SetRole(ControlRole::TextBox); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"TextArea");
    }
};

// ---- NumberInput ----
class NumberInput : public Control
{
public:
    NumberInputState state{};

    NumberInput() { SetRole(ControlRole::TextBox); }

    NumberInputHit HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"NumberInput");
    }
};

// ---- DatePicker ----
class DatePicker : public Control
{
public:
    DatePickerState state{};

    DatePicker() { SetRole(ControlRole::Custom); }

    int DayAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"DatePicker");
    }
};

// ---- ColorPicker ----
class ColorPicker : public Control
{
public:
    ColorPickerState state{};

    ColorPicker() { SetRole(ControlRole::Custom); }

    int SwatchAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"ColorPicker");
    }
};

// ---- Dropdown ----
class Dropdown : public Control
{
public:
    DropdownState state{};

    Dropdown() { SetRole(ControlRole::Custom); }

    DropdownLayout Measure(const Metrics& mx) const;
    int OptionAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Dropdown");
    }
};

// ---- Menu ----
class Menu : public Control
{
public:
    MenuState state{};

    Menu() { SetRole(ControlRole::MenuItem); }

    int ItemAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Menu");
    }
};

// ---- ContextMenu ----
class ContextMenu : public Control
{
public:
    ContextMenuState state{};

    ContextMenu() { SetRole(ControlRole::MenuItem); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"ContextMenu");
    }
};

// ---- MultiSelect ----
class MultiSelect : public Control
{
public:
    MultiSelectState state{};

    MultiSelect() { SetRole(ControlRole::Custom); }

    int At(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"MultiSelect");
    }
};

// ---- Transfer ----
class Transfer : public Control
{
public:
    TransferState state{};

    Transfer() { SetRole(ControlRole::Custom); }

    TransferLayout Measure(const Metrics& mx) const;
    TransferHit HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Transfer");
    }
};

// ---- Timeline ----
class Timeline : public Control
{
public:
    TimelineState state{};

    Timeline() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Timeline");
    }
};

// ---- Steps ----
class Steps : public Control
{
public:
    StepsState state{};

    Steps() { SetRole(ControlRole::Custom); }

    int StepAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Steps");
    }
};

// ---- Calendar ----
class Calendar : public Control
{
public:
    CalendarState state{};

    Calendar() { SetRole(ControlRole::Custom); }

    CalendarLayout Measure(const Metrics& mx) const;
    int DayAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Calendar");
    }
};

// ---- DescriptionList ----
class DescriptionList : public Control
{
public:
    DescriptionListState state{};

    DescriptionList() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"DescriptionList");
    }
};

// ---- StatCard ----
class StatCard : public Control
{
public:
    StatCardState state{};

    StatCard() { SetRole(ControlRole::Custom); }

    StatCardLayout Measure(const Metrics& mx) const;
    StatCardHit HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"StatCard");
    }
};

// ---- Chart ----
class Chart : public Control
{
public:
    ChartState state{};

    Chart() { SetRole(ControlRole::Custom); }

    int BarAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Chart");
    }
};

// ---- Carousel ----
class Carousel : public Control
{
public:
    CarouselState state{};

    Carousel() { SetRole(ControlRole::Custom); }

    CarouselHitResult HitAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Carousel");
    }
};

// ---- Spinner ----
class Spinner : public Control
{
public:
    SpinnerState state{};

    Spinner() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Spinner");
    }
};

// ---- Tooltip ----
class Tooltip : public Control
{
public:
    TooltipState state{};
    Rect client{};

    Tooltip() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Tooltip");
    }
};

// ---- Popover ----
class Popover : public Control
{
public:
    PopoverState state{};
    Rect client{};

    Popover() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Popover");
    }
};

// ---- Modal ----
class Modal : public Control
{
public:
    ModalState state{};

    Modal() { SetRole(ControlRole::Custom); }

    ModalLayout Measure(const Metrics& mx) const;
    int ButtonAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Modal");
    }
};

// ---- Drawer ----
class Drawer : public Control
{
public:
    DrawerState state{};

    Drawer() { SetRole(ControlRole::Custom); }

    DrawerLayout Measure(const Metrics& mx) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Drawer");
    }
};

// ---- Notification ----
class Notification : public Control
{
public:
    NotificationState state{};

    Notification() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Notification");
    }
};

// ---- Result ----
class Result : public Control
{
public:
    ResultState state{};

    Result() { SetRole(ControlRole::Custom); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Result");
    }
};

// ---- Accordion ----
class Accordion : public Control
{
public:
    AccordionState state{};

    Accordion() { SetRole(ControlRole::Custom); }

    int HeaderAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Accordion");
    }
};

// ---- Anchor ----
class Anchor : public Control
{
public:
    AnchorState state{};

    Anchor() { SetRole(ControlRole::Link); }

    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        state.base.hover = Hovered() || (ev.move && Hit(ev.pos));
        state.base.pressed = ev.down && Hit(ev.pos);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        state.base.focused = Focused();
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Anchor");
    }
};

// ---- Splitter ----
class Splitter : public Control
{
public:
    SplitterState state{};

    Splitter() { SetRole(ControlRole::Custom); }

    SplitterLayout Measure(const Metrics& mx) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"Splitter");
    }
};

// ---- ColumnResize ----
class ColumnResize : public Control
{
public:
    ColumnResizeState state{};

    ColumnResize() { SetRole(ControlRole::Custom); }

    ColumnResizeLayout Measure(const Metrics& mx) const;
    int HandleAt(const Metrics& mx, Point pt) const;
    void Paint(IPainter& g, const Palette& pal, const Metrics& mx) override;

    ControlResult OnPointer(const PointerEvent& ev) override
    {
        ControlResult r = Control::OnPointer(ev);
        r.redraw = true;
        return r;
    }

    ControlResult OnKey(const KeyEvent& ev) override
    {
        ControlResult r = Control::OnKey(ev);
        r.redraw = true;
        return r;
    }

    std::wstring AccessibleText() const override
    {
        if (!Name().empty()) return Name();
        return std::wstring(L"ColumnResize");
    }
};

} // namespace widgets
} // namespace ui
} // namespace air
