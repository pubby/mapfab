#include "metatiles.hpp"

#include "id.hpp"

void draw_chr_tile(metatile_model_t const& model, wxDC& dc, std::uint8_t tile, std::uint8_t attribute, coord_t at)
{
    if(tile < model.chr_bitmaps.size())
        dc.DrawBitmap(model.chr_bitmaps[tile][attribute], { at.x, at.y }, false);
}

void draw_collision_tile(model_t const& model, wxDC& dc, std::uint8_t tile, coord_t at)
{
    if(tile < model.collision_bitmaps.size())
        dc.DrawBitmap(model.collision_bitmaps[tile], { at.x, at.y }, false);
}

////////////////////////////////////////////////////////////////////////////////
// chr_picker_t ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void chr_picker_t::draw_tile(wxDC& dc, unsigned tile, coord_t at)
{ 
    if(metatiles->collisions())
    {
        dc.SetPen(wxPen());
        dc.SetBrush(wxBrush(wxColor(230, 140, 230)));
        dc.DrawRectangle(at.x, at.y, tile_size().w, tile_size().h);
        draw_collision_tile(model, dc, tile, at);
    }
    else
        draw_chr_tile(*metatiles, dc, tile, metatiles->active, at); 
}

////////////////////////////////////////////////////////////////////////////////
// metatile_canvas_t ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

metatile_canvas_t::metatile_canvas_t(wxWindow* parent, model_t& model, std::shared_ptr<metatile_model_t> metatiles)
: canvas_box_t(parent, model)
, metatiles(std::move(metatiles))
{ resize(); }

void metatile_canvas_t::draw_tiles(wxDC& dc) 
{
    for(coord_t c : dimen_range(metatiles->chr_layer.tiles.dimen()))
    {
        int x0 = c.x * 8 + margin().w;
        int y0 = c.y * 8 + margin().h;

        unsigned const tile = metatiles->chr_layer.tiles.at(c);
        unsigned const attribute = metatiles->chr_layer.attributes.at(vec_div(c, 2));
        draw_chr_tile(*metatiles, dc, tile, attribute, { x0, y0 });
    }

    unsigned num = 0;
    for(coord_t c : dimen_range(metatiles->collision_layer.tiles.dimen()))
    {
        int x0 = c.x * 16 + margin().w;
        int y0 = c.y * 16 + margin().h;

        if(metatiles->collisions())
        {
            unsigned const tile = metatiles->collision_layer.tiles.at(c);
            draw_collision_tile(model, dc, tile, { x0, y0 });
        }

        dc.SetPen(wxPen(wxColor(255, 0, 255), 0, wxPENSTYLE_DOT));
        dc.SetBrush(wxBrush());
        dc.DrawRectangle(x0, y0, 16, 16);

        if(num >= metatiles->num)
        {
            dc.SetPen(wxPen(wxColor(255, 0, 0), 2, wxPENSTYLE_SOLID));
            dc.DrawLine(x0 + 2, y0 + 2, x0 + 14, y0 + 14);
            dc.SetPen(wxPen(wxColor(0, 0, 255), 2, wxPENSTYLE_SOLID));
            dc.DrawLine(x0 + 14, y0 + 2, x0 + 2, y0 + 14);
        }

        ++num;
    }

    draw_overlays(dc);
}

////////////////////////////////////////////////////////////////////////////////
// metatile_editor_t ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

metatile_editor_t::metatile_editor_t(wxWindow* parent, model_t& model, std::shared_ptr<metatile_model_t> metatiles_)
: editor_t(parent)
, model(model)
, metatiles(std::move(metatiles_))
{
    assert(this->metatiles);
    dimen_t const nes_colors_dimen = { 4, 16 };

    wxPanel* left_panel = new wxPanel(this);
    picker = new chr_picker_t(left_panel, model, metatiles);

    attributes[0] = new wxRadioButton(left_panel, wxID_ANY, "Attribute 0  (F1)", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    attributes[1] = new wxRadioButton(left_panel, wxID_ANY, "Attribute 1  (F2)");
    attributes[2] = new wxRadioButton(left_panel, wxID_ANY, "Attribute 2  (F3)");
    attributes[3] = new wxRadioButton(left_panel, wxID_ANY, "Attribute 3  (F4)");
    attributes[4] = new wxRadioButton(left_panel, wxID_ANY, "Collisions   (F5)");

    auto* num_label = new wxStaticText(left_panel, wxID_ANY, "Metatile Count");
    num_ctrl = new wxSpinCtrl(left_panel);
    num_ctrl->SetRange(1, 256);

    auto* chr_label = new wxStaticText(left_panel, wxID_ANY, "Display CHR");
    chr_combo = new wxComboBox(left_panel, wxID_ANY);

    auto* palette_text = new wxStaticText(left_panel, wxID_ANY, "Display Palette");
    palette_ctrl = new wxSpinCtrl(left_panel);
    palette_ctrl->SetRange(0, 255);

    canvas = new metatile_canvas_t(this, model, metatiles);

    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(picker, wxSizerFlags().Expand().Proportion(1));
        for(auto* ptr : attributes)
            sizer->Add(ptr, wxSizerFlags().Border(wxLEFT));
        sizer->Add(chr_label, wxSizerFlags().Border(wxLEFT | wxUP));
        sizer->Add(chr_combo, wxSizerFlags().Border(wxLEFT | wxDOWN));
        sizer->Add(palette_text, wxSizerFlags().Border(wxLEFT));
        sizer->Add(palette_ctrl, wxSizerFlags().Border(wxLEFT | wxDOWN));
        sizer->Add(num_label, wxSizerFlags().Border(wxLEFT));
        sizer->Add(num_ctrl, wxSizerFlags().Border(wxLEFT));
        sizer->AddSpacer(8);
        left_panel->SetSizer(sizer);
    }

    {
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(left_panel, wxSizerFlags().Expand());
        sizer->Add(canvas, wxSizerFlags().Expand().Proportion(1));
        SetSizer(sizer);
    }

    // Create an accelerator table with keyboard shortcuts
    wxAcceleratorEntry entries[5];
    entries[0].Set(wxACCEL_NORMAL, WXK_F1, ID_ATTR0);
    entries[1].Set(wxACCEL_NORMAL, WXK_F2, ID_ATTR1);
    entries[2].Set(wxACCEL_NORMAL, WXK_F3, ID_ATTR2);
    entries[3].Set(wxACCEL_NORMAL, WXK_F4, ID_ATTR3);
    entries[4].Set(wxACCEL_NORMAL, WXK_F5, ID_COLLISION);
    wxAcceleratorTable accel(5, entries);

    // Set the accelerator table for the frame
    SetAcceleratorTable(accel);

    palette_ctrl->Bind(wxEVT_SPINCTRL, &metatile_editor_t::on_change_palette, this);
    for(auto* ptr : attributes)
        ptr->Bind(wxEVT_RADIOBUTTON, &metatile_editor_t::on_radio, this);
    
    chr_combo->Bind(wxEVT_COMBOBOX, &metatile_editor_t::on_combo_select, this);
    chr_combo->Bind(wxEVT_TEXT, &metatile_editor_t::on_combo_text, this);
    num_ctrl->Bind(wxEVT_SPINCTRL, &metatile_editor_t::on_num, this);
    Bind(wxEVT_MENU, &metatile_editor_t::on_active<0>, this, ID_ATTR0);
    Bind(wxEVT_MENU, &metatile_editor_t::on_active<1>, this, ID_ATTR1);
    Bind(wxEVT_MENU, &metatile_editor_t::on_active<2>, this, ID_ATTR2);
    Bind(wxEVT_MENU, &metatile_editor_t::on_active<3>, this, ID_ATTR3);
    Bind(wxEVT_MENU, &metatile_editor_t::on_active<ACTIVE_COLLISION>, this, ID_COLLISION);

    model_refresh();
}

void metatile_editor_t::on_change_palette(wxSpinEvent& event)
{
    if(metatiles->palette != event.GetPosition())
        model.modify();
    metatiles->palette = event.GetPosition(); 
    load_chr();
}

void metatile_editor_t::on_update()
{ 
    if(last_palette != metatiles->palette) 
        palette_ctrl->SetValue(last_palette = metatiles->palette); 
}

void metatile_editor_t::on_radio(wxCommandEvent& event)
{
    wxRadioButton* radio = dynamic_cast<wxRadioButton*>(event.GetEventObject());
    for(unsigned attribute = 0; attribute < attributes.size(); ++attribute)
        if(radio == attributes[attribute])
            return on_active(attribute);
}

void metatile_editor_t::on_active(unsigned i)
{
    metatiles->active = i;
    attributes[i]->SetValue(true);
    Refresh();
}

void metatile_editor_t::on_combo_select(wxCommandEvent& event)
{
    int const index = event.GetSelection();
    if(index >= 0 && index < model.chr_files.size())
    {
        if(metatiles->chr_name != model.chr_files[index].name)
            model.modify();
        metatiles->chr_name = model.chr_files[index].name;
    }
    load_chr();
}

void metatile_editor_t::on_combo_text(wxCommandEvent& event)
{
    if(metatiles->chr_name != chr_combo->GetValue())
        model.modify();
    metatiles->chr_name = chr_combo->GetValue();
    load_chr();
}

void metatile_editor_t::load_chr()
{
    if(auto* chr_file = lookup_name(metatiles->chr_name, model.chr_files))
        metatiles->refresh_chr(chr_file->chr, model.palette_array(metatiles->palette));
    else
        metatiles->clear_chr();
    Refresh();
}

void metatile_editor_t::model_refresh()
{
    std::string const chr_name = metatiles->chr_name;
    chr_combo->Unbind(wxEVT_TEXT, &metatile_editor_t::on_combo_text, this);
    chr_combo->Clear();
    for(auto const& chr : model.chr_files)
        chr_combo->Append(chr.name);
    chr_combo->SetValue(chr_name);
    chr_combo->Bind(wxEVT_TEXT, &metatile_editor_t::on_combo_text, this);

    load_chr();
}

void metatile_editor_t::on_num(wxCommandEvent& event)
{
    if(metatiles->num != num_ctrl->GetValue())
        model.modify();
    metatiles->num = num_ctrl->GetValue();
    Refresh();
}
