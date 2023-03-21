#ifndef TILE_PALETTE_HPP
#define TILE_PALETTE_HPP

#include "wx/wx.h"

#include "2d/geometry.hpp"

#include "convert.hpp"
#include "controller.hpp"
#include "grid_box.hpp"

using namespace i2d;

class tile_palette_t : public selector_box_t
{
public:
    tile_palette_t(wxFrame* parent, controller_t& controller)
    : selector_box_t(parent)
    , controller(controller)
    {
        grid_resize({ 16, 16 });
    }

private:
    controller_t& controller;

    virtual unsigned tile_size() const { return controller.active_tile_size(); }
    virtual select_map_t& selector() { return controller.mt_selector(); }

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
                dc.SetPen(wxPen(wxColor(255, 255, 255, 127), 1));
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
                dc.SetPen(wxPen(wxColor(255, 255, 255, 127), 1));
                dc.SetBrush(wxBrush(wxColor(0, 255, 255, 127)));
                if(controller.tile_palette.selection()[c])
                    dc.DrawRectangle(x0, y0, 8, 8);

                ++i;
            }
        }
    }
};

#endif

