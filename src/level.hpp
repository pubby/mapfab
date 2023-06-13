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

void draw_metatile(model_t const& model, wxDC& dc, std::uint8_t tile, coord_t at);

class object_field_t : public wxPanel
{
public:
    object_field_t(wxWindow* parent, object_t& object, class_field_t const& field);

    void on_entry(wxCommandEvent& event);

    object_t& object;
    std::string const field_name;
};

class object_dialog_t : public wxDialog
{
public:
    object_dialog_t(wxWindow* parent, model_t& model, object_t& object, bool picker = false);

    object_t& object;
private:
    model_t& model;

    bool const picker;

    wxScrolledWindow* scrolled;
    wxBoxSizer* sizer;

    wxComboBox* combo;
    wxTextCtrl* name;
    wxSpinCtrl* x_ctrl;
    wxSpinCtrl* y_ctrl;
    wxSpinCtrl* i_ctrl;

    std::unique_ptr<wxPanel> field_panel;

    std::shared_ptr<object_class_t> oc;

    void load_fields();
    void load_object();

    void on_combo_select(wxCommandEvent& event);
    void on_combo_text(wxCommandEvent& event);
    void on_name(wxCommandEvent& event);
    void on_change_x(wxSpinEvent& event);
    void on_change_y(wxSpinEvent& event);
    void on_change_i(wxSpinEvent& event);
    void on_reset(wxCommandEvent& event);
};

class metatile_picker_t : public selector_box_t
{
public:
    metatile_picker_t(wxWindow* parent, model_t& model, std::shared_ptr<level_model_t> level)
    : selector_box_t(parent)
    , model(model)
    , level(level)
    {
        scale = 1;
        resize();
    }

private:
    model_t& model;
    std::shared_ptr<level_model_t> level;

    virtual tile_model_t& tiles() const override { return *level; }

    virtual void draw_tile(wxDC& dc, unsigned tile, coord_t at) override { draw_metatile(model, dc, tile, at); }
    virtual void draw_tiles(wxDC& dc) override;
};


class level_canvas_t : public canvas_box_t
{
public:
    level_canvas_t(wxWindow* parent, model_t& model, std::shared_ptr<level_model_t> level)
    : canvas_box_t(parent, model)
    , level(level)
    { resize(); }

    virtual void draw_tile(wxDC& dc, unsigned tile, coord_t at) override { draw_metatile(model, dc, tile, at); }
    virtual void draw_tiles(wxDC& dc) override;

    coord_t crop(coord_t at)
    {
        return ::crop(at, to_rect(vec_mul(level->metatile_layer.tiles.dimen(), 16)));
    }

    double object_radius() const { return 8.0f; }

    virtual void on_down(mouse_button_t mb, coord_t at) override;
    virtual void on_up(mouse_button_t mb, coord_t at) override;
    virtual void on_motion(coord_t at) override;

    virtual bool enable_tile_select() const { return false; }

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
public:
    level_editor_t(wxWindow* parent, model_t& model, std::shared_ptr<level_model_t> level);

    virtual void on_update() override;

private:
    model_t& model;
    std::shared_ptr<level_model_t> level;

    metatile_picker_t* picker;
    level_canvas_t* canvas;
    wxSpinCtrl* palette_ctrl;
    wxSpinCtrl* width_ctrl;
    wxSpinCtrl* height_ctrl;
    wxPanel* object_panel;

    int last_palette = -1;
    int last_width = -1;
    int last_height = -1;

    virtual canvas_box_t& canvas_box() override { return *canvas; }

    void on_change_palette(wxSpinEvent& event);
    void on_change_width(wxSpinEvent& event);
    void on_change_height(wxSpinEvent& event);
    void on_pick_object(wxCommandEvent& event);

    bool confirm_object();
};

struct level_policy_t
{
    using object_type = level_model_t;
    using page_type = level_editor_t;
    static constexpr char const* name = "level";
    static auto& collection(model_t& m) { return m.levels; }
};

class levels_panel_t : public tab_panel_t<level_policy_t>
{
public:
    levels_panel_t(wxWindow* parent, model_t& model)
    : tab_panel_t<level_policy_t>(parent, model)
    {
        load_pages();
        new_page();
        new_page();
        new_page();
    }
};

#endif

