#ifndef GRID_BOX_HPP
#define GRID_BOX_HPP

#include <bit>

#include "wx/wx.h"

#include "2d/geometry.hpp"

using namespace i2d;

enum mouse_button_t
{
    MB_NONE,
    MB_LEFT,
    MB_RIGHT,
};

class grid_box_t : public wxScrolledWindow
{
public:
    explicit grid_box_t(wxWindow* parent, bool can_zoom = true)
    : wxScrolledWindow(parent)
    {
        Bind(wxEVT_LEFT_DOWN, &grid_box_t::on_left_down, this);
        Bind(wxEVT_LEFT_UP, &grid_box_t::on_left_up, this);
        Bind(wxEVT_RIGHT_DOWN, &grid_box_t::on_right_down, this);
        Bind(wxEVT_RIGHT_UP, &grid_box_t::on_right_up, this);
        Bind(wxEVT_MOTION, &grid_box_t::on_motion, this);
        if(can_zoom)
            Bind(wxEVT_MOUSEWHEEL, &grid_box_t::on_wheel, this);
    }

    virtual unsigned tile_size() const { return 8; }
    virtual dimen_t margin() const { return { 8, 8 }; }

    void OnDraw(wxDC& dc) override
    {
        dc.SetUserScale(scale, scale);
        draw_tiles(dc);
    }

    virtual void grid_resize(dimen_t dimen)
    {
        grid_dimen = dimen;
        int const w = (dimen.w * tile_size() + margin().w * 2) * scale;
        int const h = (dimen.h * tile_size() + margin().h * 2) * scale;
        SetScrollbars(1,1, w, h, 0, 0);
        SetMinSize({ w, h });
    }


protected:
    dimen_t grid_dimen = {};
    mouse_button_t mouse_down = MB_NONE;
    coord_t mouse_current = {};
    int scale = 2;

    coord_t from_screen(coord_t pixel) const { return from_screen(pixel, tile_size()); }
    coord_t from_screen(coord_t pixel, int tile_size) const
    {
        int sx, sy;
        GetViewStart(&sx, &sy);

        pixel.x += sx;
        pixel.y += sy;
        pixel.x /= scale;
        pixel.y /= scale;
        pixel.x -= margin().w;
        pixel.y -= margin().h;
        pixel.x /= tile_size;
        pixel.y /= tile_size;
        return pixel;
    }

    coord_t to_screen(coord_t c) const { return to_screen(c, tile_size()); }
    coord_t to_screen(coord_t c, int tile_size) const
    {
        c.x *= tile_size;
        c.y *= tile_size;
        c.x += margin().w;
        c.y += margin().h;
        return c;
    }

    virtual void draw_tiles(wxDC& dc) = 0;

    virtual void on_down(mouse_button_t mb, coord_t) {}
    virtual void on_up(mouse_button_t mb, coord_t) {}
    virtual void on_motion(coord_t) {}

    void on_down(wxMouseEvent& event, mouse_button_t mb) 
    {
        if(mouse_down && mouse_down != mb)
        {
            mouse_down = MB_NONE;
            Refresh();
        }
        else
        {
            mouse_down = mb;
            wxPoint pos = event.GetPosition();
            on_down(mb, { pos.x, pos.y });
        }
    }

    void on_up(wxMouseEvent& event, mouse_button_t mb) 
    {
        if(mouse_down == mb)
        {
            mouse_down = MB_NONE;
            wxPoint pos = event.GetPosition();
            on_up(mb, { pos.x, pos.y });
        }
    }

    void on_motion(wxMouseEvent& event) 
    {
        wxPoint pos = event.GetPosition();
        mouse_current = { pos.x, pos.y };
        on_motion(mouse_current);
    }

    void on_wheel(wxMouseEvent& event)
    {
        int const rot = event.GetWheelRotation();
        int const delta = event.GetWheelDelta();
        int const turns = rot / delta;
        int const target = turns >= 0 ? (scale << turns) : (scale >> -turns);
        int const diff = target - scale;

        int const new_scale = std::clamp(scale + diff, 1, 16);
        if(new_scale == scale)
            return;

        auto cursor = event.GetPosition();

        double const scale_diff = double(new_scale) / double(scale);

        int sx, sy;
        GetViewStart(&sx, &sy);
        sx *= new_scale;
        sy *= new_scale;
        sx += cursor.x * diff;
        sy += cursor.y * diff;
        sx /= scale;
        sy /= scale;

        int const w = (grid_dimen.w * tile_size() + margin().w * 2) * new_scale;
        int const h = (grid_dimen.h * tile_size() + margin().h * 2) * new_scale;
        sx = std::clamp(sx, 0, w);
        sy = std::clamp(sy, 0, h);
        scale = new_scale;

        SetScrollbars(1,1, w, h, sx, sy);

        // Hack to fix the scroll bar:
        wxPoint p = CalcScrolledPosition({ 0, 0 });
        Scroll(-p.x + 1, -p.y + 1);
        Scroll(-p.x, -p.y);

        Refresh();
    }

    void on_left_down(wxMouseEvent& event)  { on_down(event, MB_LEFT); }
    void on_left_up(wxMouseEvent& event)    { on_up(event,   MB_LEFT); }
    void on_right_down(wxMouseEvent& event) { on_down(event, MB_RIGHT); }
    void on_right_up(wxMouseEvent& event)   { on_up(event,   MB_RIGHT); }

    void set_scale(int new_scale)
    {
        new_scale = std::clamp(new_scale, 1, 8);

        coord_t s = to_screen(mouse_current);
        int const w = (grid_dimen.w * tile_size() + margin().w * 2) * scale;
        int const h = (grid_dimen.h * tile_size() + margin().h * 2) * scale;
        SetScrollbars(1,1, w, h, s.x, s.y);
        SetMinSize({ w, h });

        Refresh();
    }
};

class selector_box_t : public grid_box_t
{
public:
    explicit selector_box_t(wxWindow* parent) : grid_box_t(parent, false) {}

    void OnDraw(wxDC& dc) override
    {
        grid_box_t::OnDraw(dc);

        if(mouse_down)
        {
            dc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
            if(mouse_down == MB_LEFT)
                dc.SetBrush(wxBrush(wxColor(255, 255, 0, 127)));
            else
                dc.SetBrush(wxBrush(wxColor(255, 0, 0, 127)));
            rect_t const r = rect_from_2_coords(from_screen(mouse_start), from_screen(mouse_current));
            coord_t const c0 = to_screen(r.c);
            coord_t const c1 = to_screen(r.e());
            dc.DrawRectangle(c0.x, c0.y, (c1 - c0).x, (c1 - c0).y);
        }
    }

protected:
    virtual select_map_t& selector() = 0;

    coord_t mouse_start = {};

    virtual void on_down(mouse_button_t mb, coord_t at) override
    {
        mouse_start = at;
    }

    virtual void on_up(mouse_button_t mb, coord_t mouse_end) override
    {
        if(mb == MB_LEFT && !wxGetKeyState(WXK_SHIFT))
            selector().select_all(false);

        selector().select(
            rect_from_2_coords(from_screen(mouse_start), from_screen(mouse_end)), 
            mb == MB_LEFT);
        Refresh();
    }

    virtual void on_motion(coord_t at) override
    {
        if(mouse_down)
            Refresh();
    }
};

#endif

