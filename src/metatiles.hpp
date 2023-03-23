#ifndef METATILES_HPP
#define METATILES_HPP

#include <wx/wx.h>
#include <wx/spinctrl.h>

#include "2d/geometry.hpp"
#include "2d/pretty_print.hpp"

#include "controller.hpp"
#include "convert.hpp"
#include "grid_box.hpp"

using namespace i2d;

class chr_picker_t : public selector_box_t
{
public:
    chr_picker_t(wxWindow* parent, controller_t& controller)
    : selector_box_t(parent)
    , controller(controller)
    {
        controller.mt_selector.resize({ 16, 16 });
        grid_resize({ 16, 16 });
    }

private:
    controller_t& controller;

    virtual unsigned tile_size() const { return controller.active_tile_size(); }
    virtual select_map_t& selector() { return controller.chr_selector(); }

    virtual void draw_tiles(wxDC& dc) override
    {
        if(controller.active == ACTIVE_COLLISION)
        {
            unsigned i = 0;
            for(coord_t c : dimen_range(controller.collision_palette.dimen()))
            {
                int x0 = c.x * controller.active_tile_size() + margin().w;
                int y0 = c.y * controller.active_tile_size() + margin().h;

                if(i >= controller.collision_bitmaps.size())
                    break;

                dc.DrawBitmap(controller.collision_bitmaps[i], { x0, y0 }, false);
                dc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
                dc.SetBrush(wxBrush(wxColor(0, 255, 255, 127)));
                if(controller.collision_palette.selection()[c])
                    dc.DrawRectangle(x0, y0, 16, 16);

                ++i;
            }
        }
        else
        {
            unsigned i = 0;
            for(coord_t c : dimen_range(controller.tile_palette.dimen()))
            {
                int x0 = c.x * controller.active_tile_size() + margin().w;
                int y0 = c.y * controller.active_tile_size() + margin().h;

                if(i >= controller.chr_bitmaps.size())
                    break;

                dc.DrawBitmap(controller.chr_bitmaps[i][controller.active], { x0, y0 }, false);
                dc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
                dc.SetBrush(wxBrush(wxColor(0, 255, 255, 127)));
                if(controller.tile_palette.selection()[c])
                    dc.DrawRectangle(x0, y0, 8, 8);

                ++i;
            }
        }
    }
};


class metatile_canvas_t : public grid_box_t
{
public:
    metatile_canvas_t(wxWindow* parent, controller_t& controller)
    : grid_box_t(parent)
    , controller(controller)
    {
        grid_resize({ 32, 32 });
    }

    virtual void draw_tiles(wxDC& dc) override
    {
        dc.SetUserScale(scale, scale);

        for(coord_t c : dimen_range(controller.metatiles.tiles.dimen()))
        {
            coord_t const c0 = to_screen(c);

            std::uint8_t const tile = controller.metatiles.tiles[c];
            std::uint8_t const attribute = controller.metatiles.attributes[vec_div(c, 2)];

            if(tile < controller.chr_bitmaps.size())
                dc.DrawBitmap(controller.chr_bitmaps[tile][attribute], { c0.x, c0.y }, false);
        }

        if(controller.active == ACTIVE_COLLISION)
        {
            for(coord_t c : dimen_range(controller.metatiles.collisions.dimen()))
            {
                coord_t const c0 = to_screen(c, 16);

                std::uint8_t const tile = controller.metatiles.collisions[c];

                if(tile < controller.collision_bitmaps.size())
                    dc.DrawBitmap(controller.collision_bitmaps[tile], { c0.x, c0.y }, false);
            }
        }

        dimen_t const dimen = controller.metatiles.attributes.dimen();
        dc.SetPen(wxPen(wxColor(255, 0, 255), 0, wxPENSTYLE_DOT));
        for(unsigned i = 1; i < dimen.h; ++i)
        {
            coord_t const c0 = to_screen(vec_mul(coord_t{ 0, i }, 2));
            coord_t const c1 = to_screen(vec_mul(coord_t{ dimen.w, i }, 2));
            dc.DrawLine(c0.x, c0.y, c1.x, c1.y);
        }
        for(unsigned i = 1; i < dimen.w; ++i)
        {
            coord_t const c0 = to_screen(vec_mul(coord_t{ i, 0 }, 2));
            coord_t const c1 = to_screen(vec_mul(coord_t{ i, dimen.h }, 2));
            dc.DrawLine(c0.x, c0.y, c1.x, c1.y);
        }

        dc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
        dc.SetBrush(wxBrush(wxColor(0, 255, 255, mouse_down == MB_LEFT ? 127 : 31)));
        controller.for_each_chr_pen(from_screen(mouse_current, controller.active_tile_size()), [&](coord_t at, std::uint8_t tile)
        {
            int const s = controller.active_tile_size();
            coord_t const c0 = to_screen(at, s);
            dc.DrawRectangle(c0.x, c0.y, s, s);
        });
    }

private:
    controller_t& controller;

    virtual void on_down(mouse_button_t mb, coord_t pen_c) override
    {
        if(mb == MB_RIGHT)
            color(from_screen(pen_c));
        Refresh();
    }

    virtual void on_up(mouse_button_t mb, coord_t pen_c) override
    {
        pen_c = from_screen(pen_c, controller.active_tile_size());

        if(mb == MB_LEFT)
        {
            if(controller.active == ACTIVE_COLLISION)
                controller.do_collisions();
            else
                controller.do_tiles();

            controller.for_each_chr_pen(pen_c, [&](coord_t at, std::uint8_t tile)
            {
                if(controller.active == ACTIVE_COLLISION)
                    controller.metatiles.collisions.at(at) = tile;
                else
                    controller.metatiles.set_tile(at, tile, controller.active);
            });
        }

        Refresh();
    }

    virtual void on_motion(coord_t at) override
    {
        if(mouse_down == MB_RIGHT)
            color(from_screen(at));
        Refresh();
    }

    void color(coord_t at)
    {
        if(controller.active >= 4)
            return;
        at = vec_div(at, 2);
        if(!in_bounds(at, controller.metatiles.attributes.dimen()))
            return;
        if(controller.metatiles.attributes[at] == controller.active)
            return;
        controller.do_attribute(at);
        controller.metatiles.attributes[at] = controller.active;
    }
};

class metatile_editor_t : public wxPanel
{
public:
    metatile_editor_t(wxWindow* parent, controller_t& controller)
    : wxPanel(parent)
    , controller(controller)
    {
        dimen_t const nes_colors_dimen = { 4, 16 };

        wxPanel* left_panel = new wxPanel(this);
        picker = new chr_picker_t(left_panel, controller);
        auto* palette_text = new wxStaticText(left_panel, wxID_ANY, "Display Palette");
        palette_ctrl = new wxSpinCtrl(left_panel);
        palette_ctrl->SetRange(0, 255);

        canvas = new metatile_canvas_t(this, controller);

        {
            wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
            sizer->Add(picker, wxSizerFlags().Expand().Proportion(1));
            sizer->Add(palette_text, wxSizerFlags().Border(wxLEFT));
            sizer->Add(palette_ctrl, wxSizerFlags().Border(wxLEFT));
            sizer->AddSpacer(8);
            left_panel->SetSizer(sizer);
        }

        {
            wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
            sizer->Add(left_panel, wxSizerFlags().Expand());
            sizer->Add(canvas, wxSizerFlags().Expand().Proportion(1));
            SetSizer(sizer);
        }

        palette_ctrl->Bind(wxEVT_SPINCTRL, &metatile_editor_t::on_change_palette, this);
    }

private:
    controller_t& controller;
    chr_picker_t* picker;
    metatile_canvas_t* canvas;
    wxSpinCtrl* palette_ctrl;

    void on_change_palette(wxSpinEvent& event)
    {
        controller.active_palette = event.GetPosition(); 
        controller.refresh_chr();
        Refresh();
    }
};

#endif

