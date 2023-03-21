#ifndef METATILES_HPP
#define METATILES_HPP

#include "wx/wx.h"

#include "2d/geometry.hpp"
#include "2d/pretty_print.hpp"

#include "controller.hpp"
#include "convert.hpp"
#include "grid_box.hpp"

using namespace i2d;

class metatile_editor_t : public grid_box_t
{
public:
    metatile_editor_t(wxFrame* parent, controller_t& controller);

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

        wxPen pen(wxColor(255, 255, 255, 127), 1);
        pen.SetCap(wxCAP_BUTT);
        dc.SetPen(pen);
        for(coord_t c : dimen_range(controller.metatiles.attributes.dimen()))
        {
            coord_t const c0 = to_screen(vec_mul(c, 2));
            coord_t const c1 = to_screen(vec_mul(c + coord_t{1,1}, 2));
            dc.DrawLine(c0.x, c0.y, c1.x, c0.y);
            dc.DrawLine(c0.x, c0.y+1, c0.x, c1.y-1);
        }

        dc.SetPen(wxPen(wxColor(255, 255, 255, 127), 1));
        dc.SetBrush(wxBrush(wxColor(0, 255, 255, mouse_down == MB_LEFT ? 127 : 31)));
        controller.for_each_mt_pen(from_screen(mouse_current, controller.active_tile_size()), [&](coord_t at, std::uint8_t tile)
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

            controller.for_each_mt_pen(pen_c, [&](coord_t at, std::uint8_t tile)
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

    virtual void grid_resize(dimen_t dimen) override
    {
        grid_box_t::grid_resize(dimen);
        controller.metatiles.resize(dimen);
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

metatile_editor_t::metatile_editor_t(wxFrame* parent, controller_t& controller)
: grid_box_t(parent)
, controller(controller)
{
    grid_resize({ 32, 32 });
    scale = 3;
}

#endif

