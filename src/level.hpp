#ifndef LEVEL_HPP
#define LEVEL_HPP

#include <ranges>

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/notebook.h>
#include <wx/grid.h>

#include "2d/geometry.hpp"
#include "2d/pretty_print.hpp"

#include "id.hpp"
#include "model.hpp"
#include "convert.hpp"
#include "grid_box.hpp"

using namespace i2d;

void draw_metatile(level_model_t const& model, render_t& gc, std::uint8_t tile, coord_t at);

class object_field_t : public wxPanel
{
public:
    object_field_t(wxWindow* parent, object_t& object, class_field_t const& field, bool picker = false);

    void on_entry(wxCommandEvent& event);

    object_t& object;
    std::string const field_name;
};

class object_editor_t : public wxPanel
{
friend class object_dialog_t;
public:
    object_editor_t(wxWindow* parent, model_t& model, object_t& object, bool picker = false);

    object_t& object;

    void load_object();
    void update_classes();
private:
    model_t& model;

    bool const picker;

    wxScrolledWindow* scrolled;
    wxBoxSizer* sizer;

    wxComboBox* combo;
    wxTextCtrl* name = nullptr;
    wxSpinCtrl* x_ctrl = nullptr;
    wxSpinCtrl* y_ctrl = nullptr;

    std::unique_ptr<wxPanel> field_panel;

    std::shared_ptr<object_class_t> oc;

    void load_fields();

    void on_combo_select(wxCommandEvent& event);
    void on_combo_text(wxCommandEvent& event);
    void on_name(wxCommandEvent& event);
    void on_change_x(wxSpinEvent& event);
    void on_change_y(wxSpinEvent& event);
    void on_change_i(wxSpinEvent& event);
    void on_reset(wxCommandEvent& event);
};

class object_dialog_t : public wxDialog
{
public:
    object_dialog_t(wxWindow* parent, model_t& model, object_t& object, bool picker = false);

    object_t& object;
private:
    model_t& model;
    object_editor_t* editor;
};

class metatile_picker_t : public selector_box_t
{
public:
    metatile_picker_t(wxWindow* parent, model_t& model, std::shared_ptr<level_model_t> level)
    : selector_box_t(parent, model)
    , level(level)
    {
        scale = 1;
        resize();
    }

private:
    std::shared_ptr<level_model_t> level;

    virtual tile_model_t& tiles() const override { return *level; }

    virtual void draw_tile(render_t& gc, unsigned tile, coord_t at) override 
    { 
        draw_metatile(*level, gc, tile, at); 
    }
    virtual void draw_tiles(render_t& gc) override;
};


class level_canvas_t : public canvas_box_t
{
public:
    level_canvas_t(wxWindow* parent, model_t& model, std::shared_ptr<level_model_t> level)
    : canvas_box_t(parent, model)
    , level(level)
    { resize(); }

    virtual void draw_tile(render_t& gc, unsigned tile, coord_t at) override 
    { 
        draw_metatile(*level, gc, tile, at); 
    }
    virtual void draw_tiles(render_t& gc) override;

    coord_t crop(coord_t at)
    {
        return ::crop(at, to_rect(vec_mul(level->metatile_layer.tiles.dimen(), 16)));
    }

    double object_radius() const { return 8.0f; }

    virtual void on_down(mouse_button_t mb, coord_t at) override;
    virtual void on_up(mouse_button_t mb, coord_t at) override;
    virtual void on_motion(coord_t at) override;

    virtual bool enable_tile_select() const { return level->current_layer == TILE_LAYER; }

private:
    std::shared_ptr<level_model_t> level;
    bool dragging_objects = false;
    bool selecting_objects = false;
    coord_t drag_last = {};
    coord_t object_select_start = {};

    virtual tile_model_t& tiles() const override { return *level; }
};

class level_editor_t : public editor_t
{
friend class level_canvas_t;
public:
    level_editor_t(wxWindow* parent, model_t& model, std::shared_ptr<level_model_t> level);

    virtual bool enable_copy() override;
    virtual void on_update() override;
    level_model_t& level_model() { return *level; }
    auto ptr() { return level.get(); }

    model_t& model;
private:
    std::shared_ptr<level_model_t> level;

    metatile_picker_t* picker;
    level_canvas_t* canvas;
    wxSpinCtrl* palette_ctrl;
    wxSpinCtrl* width_ctrl;
    wxSpinCtrl* height_ctrl;
    wxPanel* object_panel;
    wxComboBox* metatiles_combo;
    wxComboBox* chr_combo;
    wxTextCtrl* macro_ctrl;
    std::array<wxRadioButton*, 2> layers;
    object_editor_t* object_editor;

    int last_palette = -1;
    int last_width = -1;
    int last_height = -1;

    virtual canvas_box_t& canvas_box() override { return *canvas; }
    virtual tile_copy_t copy(bool cut) override;

    void on_radio(wxCommandEvent& event);
    void on_change_palette(wxSpinEvent& event);
    void on_change_width(wxSpinEvent& event);
    void on_change_height(wxSpinEvent& event);
    void on_macro_name(wxCommandEvent& event);

    void on_metatiles_select(wxCommandEvent& event);
    void on_metatiles_text(wxCommandEvent& event);
    void on_chr_select(wxCommandEvent& event);
    void on_chr_text(wxCommandEvent& event);
    void on_delete(wxCommandEvent& event);

    template<unsigned I>
    void on_active(wxCommandEvent& event) { on_active(I); }
    void on_active(unsigned i);

    virtual void select_all(bool select = true) override;
    virtual void select_invert() override;
public:
    void model_refresh();
    void load_metatiles();
};

struct level_policy_t
{
    using object_type = level_model_t;
    using page_type = level_editor_t;
    static constexpr char const* name = "Level";
    static auto& collection(model_t& m) { return m.levels; }
    static void on_page_changing(page_type& page, object_type& object) 
    {
        page.model_refresh();
    }
    static void rename(model_t& m, std::string const& old_name, std::string const& new_name) {}
};

class levels_panel_t : public tab_panel_t<level_policy_t>
{
public:
    levels_panel_t(wxWindow* parent, model_t& model)
    : tab_panel_t<level_policy_t>(parent, model)
    {
        load_pages();
    }
};

#endif

