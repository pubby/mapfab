#ifndef PALETTE_HPP
#define PALETTE_HPP

#include <wx/wx.h>
#include <wx/grid.h>
#include <wx/spinctrl.h>

#include "2d/grid.hpp"

#include "grid_box.hpp"
#include "nes_colors.hpp"

using namespace i2d;

constexpr unsigned color_tile_size = 16;

void draw_color_tile(wxDC& dc, unsigned color, coord_t at);

class color_picker_t : public selector_box_t
{
public:
    color_picker_t(wxWindow* parent, model_t& model)
    : selector_box_t(parent)
    , model(model)
    { resize(); }

protected:
    model_t& model;

    virtual void draw_tile(wxDC& dc, unsigned color, coord_t at) override { draw_color_tile(dc, color, at); }
    virtual tile_model_t& tiles() const { return model.palette; }
};

class palette_canvas_t : public canvas_box_t
{
public:
    static constexpr unsigned WIDTH = 25;

    palette_canvas_t(wxWindow* parent, model_t& model)
    : canvas_box_t(parent, model)
    { resize(); }

private:
    virtual tile_model_t& tiles() const { return model.palette; }
    virtual dimen_t margin() const override { return { 16, 16 }; }
    virtual void post_update() override { model.refresh_chr(); }
    virtual void draw_tile(wxDC& dc, unsigned color, coord_t at) override { draw_color_tile(dc, color, at); }
    virtual void draw_tiles(wxDC& dc) override;
};

class palette_editor_t : public editor_t
{
public:
    palette_editor_t(wxWindow* parent, model_t& model);

    virtual void on_update() override;
    virtual canvas_box_t& canvas_box() override { return *canvas; }

private:
    model_t& model;
    color_picker_t* picker;
    palette_canvas_t* canvas;
    wxSpinCtrl* count_ctrl;
    int last_count = -1;

    void on_change_palette_count(wxSpinEvent& event);
};

#endif
