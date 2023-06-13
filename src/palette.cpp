#include "palette.hpp"

static char int_to_char(int i)
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

void draw_color_tile(wxDC& dc, unsigned color, coord_t at)
{
    rgb_t const rgb = nes_colors[color % 64];
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

    dc.SetPen(wxPen());
    dc.SetBrush(wxBrush(wxColor(rgb.r, rgb.g, rgb.b)));
    dc.DrawRectangle(at.x, at.y, color_tile_size, color_tile_size);

    dc.SetTextForeground(wxColor(text_rgb.r, text_rgb.g, text_rgb.b));
    wxString string;
    string << int_to_char(color >> 4);
    string << int_to_char(color & 0x0F);
    dc.SetFont(wxFont(wxFontInfo(4)));
    dc.DrawText(string, { at.x+4, at.y+5 });
}

////////////////////////////////////////////////////////////////////////////////
// palette_canvas_t ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void palette_canvas_t::draw_tiles(wxDC& dc)
{
    canvas_box_t::draw_tiles(dc);

    dc.SetPen(wxPen(wxColor(255, 0, 255), 0, wxPENSTYLE_DOT));
    dc.SetTextForeground(wxColor(0, 0, 0));
    auto const vline = [&](unsigned x, wxString text, bool line = true, int text_offset = -32)
    {
        unsigned sx = margin().w + x * color_tile_size;
        unsigned sy = margin().h + model.palette.color_layer.num * color_tile_size;
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

    for(unsigned i = 0; i < grid_dimen.h; ++i)
    {
        wxString string;
        string << i;
        wxCoord w, h;
        dc.GetTextExtent(string, &w, &h);
        dc.DrawText(string, { margin().w - w - 2, margin().h + 5 + i*16 });
    }
}

////////////////////////////////////////////////////////////////////////////////
// palette_editor_t ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

palette_editor_t::palette_editor_t(wxWindow* parent, model_t& model)
: editor_t(parent)
, model(model)
{
    dimen_t const nes_colors_dimen = { 4, 16 };

    wxPanel* left_panel = new wxPanel(this);
    picker = new color_picker_t(left_panel, model);
    auto* palette_count_text = new wxStaticText(left_panel, wxID_ANY, "Palette Count");
    count_ctrl = new wxSpinCtrl(left_panel);
    count_ctrl->SetRange(1, 256);

    canvas = new palette_canvas_t(this, model);

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

void palette_editor_t::on_change_palette_count(wxSpinEvent& event)
{
    if(!history.on_top<undo_palette_num_t>())
        history.push(undo_palette_num_t{ model.palette.color_layer.num });
    model.modify();
    model.palette.num = event.GetPosition(); 
    canvas->resize();
    Refresh();
}

void palette_editor_t::on_update() 
{ 
    if(last_count != model.palette.color_layer.num)
        count_ctrl->SetValue(last_count = model.palette.color_layer.num); 
}
