#ifndef METATILES_HPP
#define METATILES_HPP

#include <wx/wx.h>
#include <wx/spinctrl.h>

#include "2d/geometry.hpp"
#include "2d/pretty_print.hpp"

#include "model.hpp"
#include "convert.hpp"
#include "grid_box.hpp"

using namespace i2d;

void draw_chr_tile(metatile_model_t const& model, render_t& gc, std::uint8_t tile, std::uint8_t attribute, coord_t at);
void draw_collision_tile(model_t const& model, render_t& gc, std::uint8_t tile, coord_t at);

class chr_picker_t : public selector_box_t
{
public:
    chr_picker_t(wxWindow* parent, model_t& model, std::shared_ptr<metatile_model_t> metatiles)
    : selector_box_t(parent, model)
    , metatiles(std::move(metatiles))
    { resize(); }

private:
    std::shared_ptr<metatile_model_t> metatiles;

    virtual tile_model_t& tiles() const override { return *metatiles; }
    virtual void draw_tile(render_t& gc, unsigned tile, coord_t at) override;
};


class metatile_canvas_t : public canvas_box_t
{
public:
    metatile_canvas_t(wxWindow* parent, model_t& model, std::shared_ptr<metatile_model_t> metatiles);

    metatile_model_t& metatile_model() const { return *metatiles; }
private:
    std::shared_ptr<metatile_model_t> metatiles;

    virtual tile_model_t& tiles() const { return *metatiles; }
    virtual dimen_t margin() const override { return { 16, 16 }; }
    virtual void post_update() override { } // TODO
    virtual int tile_code(coord_t c) override 
    { 
        if(metatiles->collisions())
            return c.x + c.y * 16;
        return (c.x / 2) + (c.y / 2) * 16; 
    }

    virtual void draw_tiles(render_t& gc) override;
};

class metatile_editor_t : public editor_t
{
public:
    metatile_editor_t(wxWindow* parent, model_t& model, std::shared_ptr<metatile_model_t> metatiles);

    virtual void on_update() override;
    virtual canvas_box_t& canvas_box() override { return *canvas; }
    auto ptr() { return metatiles.get(); }

    model_t& model;

    void model_refresh();
    void load_chr();
private:
    std::shared_ptr<metatile_model_t> metatiles;
    chr_picker_t* picker;
    metatile_canvas_t* canvas;
    wxSpinCtrl* palette_ctrl;
    wxComboBox* chr_combo;
    wxSpinCtrl* num_ctrl;
    std::array<wxRadioButton*, 5> attributes;
    int last_palette = -1;
    int last_num = -1;

    void on_change_palette(wxSpinEvent& event);
    void on_radio(wxCommandEvent& event);
    void on_combo_select(wxCommandEvent& event);
    void on_combo_text(wxCommandEvent& event);

    template<unsigned I>
    void on_active(wxCommandEvent& event) { on_active(I); }
    void on_active(unsigned i);

    void on_num(wxCommandEvent& event);
};

struct metatile_policy_t
{
    using object_type = metatile_model_t;
    using page_type = metatile_editor_t;
    static constexpr char const* name = "Metatiles";
    static auto& collection(model_t& m) { return m.metatiles; }
    static void on_page_changing(page_type& page, object_type& object) { page.model_refresh(); }
    static void rename(model_t& m, std::string const& old_name, std::string const& new_name)
    {
        for(auto& level : m.levels)
            if(level->metatiles_name == old_name)
                level->metatiles_name = new_name;
    }
};

class metatile_panel_t : public tab_panel_t<metatile_policy_t>
{
public:
    metatile_panel_t(wxWindow* parent, model_t& model)
    : tab_panel_t<metatile_policy_t>(parent, model)
    {
        load_pages();
    }
};

#endif

