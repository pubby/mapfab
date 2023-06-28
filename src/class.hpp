#ifndef CLASS_HPP
#define CLASS_HPP

#include <cassert>
#include <ranges>

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/notebook.h>
#include <wx/grid.h>
#include <wx/clrpicker.h>

#include "2d/geometry.hpp"
#include "2d/pretty_print.hpp"

#include "id.hpp"
#include "model.hpp"
#include "convert.hpp"
#include "grid_box.hpp"

using namespace i2d;

class class_editor_t;
class delete_button_t;

class field_def_t : public wxPanel
{
public:
    field_def_t(wxWindow* parent, class_field_t const& field, unsigned index);

    void on_click(wxCommandEvent& event);
    void on_type(wxCommandEvent& event);
    void on_name(wxCommandEvent& event);

    unsigned index = 0;
};

class class_editor_t : public wxScrolledWindow
{
public:
    class_editor_t(wxWindow* parent, model_t& model, std::shared_ptr<object_class_t> oc_);

    void on_delete(unsigned index);
    void on_new(wxCommandEvent& event);
    void on_type(unsigned index, std::string str);
    void on_name(unsigned index, std::string str);
    void on_color(wxColourPickerEvent& event);

private:
    void new_field(class_field_t const& field);

    model_t& model;
    std::shared_ptr<object_class_t> oc;
    std::deque<std::unique_ptr<field_def_t>> field_defs;
    wxBoxSizer* field_sizer;
};


struct class_policy_t
{
    using object_type = object_class_t;
    using page_type = class_editor_t;
    static constexpr char const* name = "class";
    static auto& collection(model_t& m) { return m.object_classes; }
    static void on_page_changing(page_type& page, object_type& object) {}
};

class class_panel_t : public tab_panel_t<class_policy_t>
{
public:
    class_panel_t(wxWindow* parent, model_t& model)
    : tab_panel_t<class_policy_t>(parent, model)
    {
        load_pages();
        new_page();
    }
};

#endif

