#ifndef PALETTE_HPP
#define PALETTE_HPP

#include <wx/wx.h>
#include <wx/grid.h>
#include <wx/spinctrl.h>

#include "2d/grid.hpp"

#include "grid_box.hpp"
#include "nes_colors.hpp"

using namespace i2d;

inline char int_to_char(int i)
{
    switch(i)
    {
    default:
    case 0: return '0';
    case 1: return '1';
    case 2: return '2';
    case 3: return '3';
    case 4: return '4';
    case 5: return '5';
    case 6: return '6';
    case 7: return '7';
    case 8: return '8';
    case 9: return '9';
    case 10: return 'A';
    case 11: return 'B';
    case 12: return 'C';
    case 13: return 'D';
    case 14: return 'E';
    case 15: return 'F';
    }
}

class color_picker_t : public selector_box_t
{
public:
    static constexpr dimen_t DIMEN = { 4, 16 };

    color_picker_t(wxWindow* parent, controller_t& controller)
    : selector_box_t(parent)
    , controller(controller)
    {
        controller.color_selector.resize(DIMEN);
        grid_resize(DIMEN);
    }

private:
    controller_t& controller;

    virtual void draw_tiles(wxDC& dc) override
    {
        dc.SetUserScale(scale, scale);

        for(coord_t c : dimen_range(DIMEN))
        {
            int x0 = c.x * 16 + margin().w;
            int y0 = c.y * 16 + margin().h;

            unsigned const color = c.x * 16 + c.y;
            rgb_t const rgb = nes_colors[color];
            rgb_t text_rgb;

            if(color == 0x0D)
                text_rgb = RED;
            else
            {
                if((color & 0xF) >= 0xE && color != 0xF)
                    text_rgb = GREY;
                else if(distance(rgb, BLACK) < distance(rgb, WHITE))
                    text_rgb = WHITE;
                else
                    text_rgb = BLACK;
            }

            dc.SetPen(wxPen(wxColor(255, 255, 255), 0));
            dc.SetBrush(wxBrush(wxColor(rgb.r, rgb.g, rgb.b)));
            dc.DrawRectangle(x0, y0, 16, 16);

            dc.SetTextForeground(wxColor(text_rgb.r, text_rgb.g, text_rgb.b));
            dc.SetFont(wxFont(wxFontInfo(4)));
            wxString string;
            string << int_to_char(color >> 4);
            string << int_to_char(color & 0x0F);
            dc.DrawText(string, { x0+4, y0+5 });

            dc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
            dc.SetBrush(wxBrush(wxColor(0, 255, 255, 127)));
            if(controller.color_selector[c])
                dc.DrawRectangle(x0, y0, 16, 16);
        }
    }

    virtual unsigned tile_size() const override { return 16; }
    virtual select_map_t& selector() { return controller.color_selector; }
};

class palette_canvas_t : public grid_box_t
{
public:
    static constexpr unsigned WIDTH = 25;

    palette_canvas_t(wxWindow* parent, controller_t& controller)
    : grid_box_t(parent)
    , controller(controller)
    {
        grid_resize(controller.palette_map.dimen());
    }

private:
    controller_t& controller;

    virtual dimen_t margin() const override { return { 16, 16 }; }
    virtual unsigned tile_size() const override { return 16; }

    virtual void draw_tiles(wxDC& dc) override
    {
        dc.SetFont(wxFont(wxFontInfo(4)));

        for(coord_t c : dimen_range(controller.palette_map.dimen()))
        {
            int x0 = c.x * 16 + margin().w;
            int y0 = c.y * 16 + margin().h;

            unsigned const color = controller.palette_map[c];
            rgb_t const rgb = nes_colors[color];
            rgb_t text_rgb;

            if(color == 0x0D)
                text_rgb = RED;
            else
            {
                if((color & 0xF) >= 0xE && color != 0xF)
                    text_rgb = GREY;
                else if(distance(rgb, BLACK) < distance(rgb, WHITE))
                    text_rgb = WHITE;
                else
                    text_rgb = BLACK;
            }

            dc.SetPen(wxPen(wxColor(255, 255, 255), 0));
            dc.SetBrush(wxBrush(wxColor(rgb.r, rgb.g, rgb.b)));
            dc.DrawRectangle(x0, y0, 16, 16);

            dc.SetTextForeground(wxColor(text_rgb.r, text_rgb.g, text_rgb.b));
            wxString string;
            string << int_to_char(color >> 4);
            string << int_to_char(color & 0x0F);
            dc.DrawText(string, { x0+4, y0+5 });

            dc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
            dc.SetBrush(wxBrush(wxColor(0, 255, 255, 127)));
        }

        dc.SetPen(wxPen(wxColor(255, 0, 255), 0, wxPENSTYLE_DOT));
        dc.SetTextForeground(wxColor(0, 0, 0));
        auto const vline = [&](unsigned x, wxString text, bool line = true, int text_offset = -32)
        {
            unsigned sx = margin().w + x * 16;
            unsigned sy = margin().h + controller.palette_map.dimen().h * 16;
            if(line)
                dc.DrawLine(sx, margin().h/2, sx, sy + margin().h/2);
            dc.DrawText(text, { sx + text_offset, margin().h / 2 });
        };
        vline(1,  "UBC", true, -14);
        vline(4,  "BG 0");
        vline(7,  "BG 1");
        vline(10, "BG 2");
        vline(13, "BG 3");
        vline(16, "SPR 0");
        vline(19, "SPR 1");
        vline(22, "SPR 2");
        vline(25, "SPR 3", false);

        for(unsigned i = 0; i < controller.palette_map.dimen().h; ++i)
        {
            wxString string;
            string << i;
            wxCoord w, h;
            dc.GetTextExtent(string, &w, &h);
            dc.DrawText(string, { margin().w - w - 2, margin().h + 5 + i*16 });
        }

        dc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
        dc.SetBrush(wxBrush(wxColor(0, 255, 255, mouse_down == MB_LEFT ? 127 : 31)));
        controller.for_each_color_pen(from_screen(mouse_current), [&](coord_t c, std::uint8_t color)
        {
            coord_t const c0 = to_screen(c);
            dc.DrawRectangle(c0.x, c0.y, 16, 16);
        });
    }

    virtual void on_up(mouse_button_t mb, coord_t pen_c) override
    {
        pen_c = from_screen(pen_c, controller.active_tile_size());

        if(mb == MB_LEFT)
        {
            controller.for_each_color_pen(from_screen(mouse_current), [&](coord_t c, std::uint8_t color)
            {
                controller.palette_map[c] = color;
            });
            controller.refresh_chr();
        }

        Refresh();
    }

    virtual void on_motion(coord_t at) override
    {
        Refresh();
    }
};

class palette_editor_t : public wxPanel
{
public:
    palette_editor_t(wxWindow* parent, controller_t& controller)
    : wxPanel(parent)
    , controller(controller)
    {
        dimen_t const nes_colors_dimen = { 4, 16 };

        wxPanel* left_panel = new wxPanel(this);
        picker = new color_picker_t(left_panel, controller);
        auto* palette_count_text = new wxStaticText(left_panel, wxID_ANY, "Palette Count");
        count_ctrl = new wxSpinCtrl(left_panel);
        count_ctrl->SetRange(1, 256);

        canvas = new palette_canvas_t(this, controller);

        {
            wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
            sizer->Add(picker, wxSizerFlags().Expand().Proportion(1));
            sizer->Add(palette_count_text, wxSizerFlags().Border(wxLEFT));
            sizer->Add(count_ctrl, wxSizerFlags().Border(wxLEFT));
            sizer->AddSpacer(8);
            left_panel->SetSizer(sizer);
        }

        {
            wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
            sizer->Add(left_panel, wxSizerFlags().Expand());
            sizer->Add(canvas, wxSizerFlags().Expand().Proportion(1));
            SetSizer(sizer);
        }

        count_ctrl->Bind(wxEVT_SPINCTRL, &palette_editor_t::on_change_palette_count, this);
    }

private:
    controller_t& controller;
    color_picker_t* picker;
    palette_canvas_t* canvas;
    wxSpinCtrl* count_ctrl;

    void on_change_palette_count(wxSpinEvent& event)
    {
        controller.palette_map.num = event.GetPosition(); 
        canvas->grid_resize(controller.palette_map.dimen());
        Refresh();
    }
};

#endif
