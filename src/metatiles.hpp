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

void draw_chr_tile(model_t const& model, wxDC& dc, std::uint8_t tile, std::uint8_t attribute, coord_t at);
void draw_collision_tile(model_t const& model, wxDC& dc, std::uint8_t tile, coord_t at);

class chr_picker_t : public selector_box_t
{
public:
    chr_picker_t(wxWindow* parent, model_t& model)
    : selector_box_t(parent)
    , model(model)
    { resize(); }

private:
    model_t& model;

    virtual tile_model_t& tiles() const override { return model.metatiles; }
    virtual void draw_tile(wxDC& dc, unsigned tile, coord_t at) override;
};


class metatile_canvas_t : public canvas_box_t
{
public:
    metatile_canvas_t(wxWindow* parent, model_t& model);

private:
    virtual tile_model_t& tiles() const { return model.metatiles; }
    virtual dimen_t margin() const override { return { 16, 16 }; }
    virtual void post_update() override { } // TODO

    virtual void draw_tiles(wxDC& dc) override;
};

class metatile_editor_t : public editor_t
{
public:
    metatile_editor_t(wxWindow* parent, model_t& model);

    virtual void on_update() override;
    virtual canvas_box_t& canvas_box() override { return *canvas; }

private:
    model_t& model;
    chr_picker_t* picker;
    metatile_canvas_t* canvas;
    wxSpinCtrl* palette_ctrl;
    std::array<wxRadioButton*, 5> attributes;
    int last_palette = -1;

    void on_change_palette(wxSpinEvent& event);
    void on_radio(wxCommandEvent& event);

    template<unsigned I>
    void on_active(wxCommandEvent& event) { on_active(I); }
    void on_active(unsigned i);
};

#endif

