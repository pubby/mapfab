#include "level.hpp"

#include <ranges>

void draw_metatile(level_model_t const& model, wxDC& dc, std::uint8_t tile, coord_t at)
{
    if(tile < model.metatile_bitmaps.size())
        dc.DrawBitmap(model.metatile_bitmaps[tile], { at.x, at.y }, false);
}

////////////////////////////////////////////////////////////////////////////////
// object_field_t //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

object_field_t::object_field_t(wxWindow* parent, object_t& object, class_field_t const& field)
: wxPanel(parent, wxID_ANY)
, object(object)
, field_name(field.name)
{
    wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText* label = new wxStaticText(this, wxID_ANY, field.type + " " + field_name, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    wxTextCtrl* entry = new wxTextCtrl(this, wxID_ANY, object.fields[field_name]);
    entry->SetMinSize(wxSize(300, 24));

    row_sizer->Add(label, wxSizerFlags().Proportion(1).Center());
    row_sizer->AddSpacer(8);
    row_sizer->Add(entry, wxSizerFlags().Expand().Proportion(4).Border());

    entry->Bind(wxEVT_TEXT, &object_field_t::on_entry, this);

    SetSizerAndFit(row_sizer);
}

void object_field_t::on_entry(wxCommandEvent& event)
{ 
    object.fields[field_name] = event.GetString().ToStdString(); 
}

////////////////////////////////////////////////////////////////////////////////
// object_dialog_t ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

object_dialog_t::object_dialog_t(wxWindow* parent, model_t& model, object_t& object, bool picker)
: wxDialog(parent, wxID_ANY, "Object Editor")
, object(object)
, model(model)
, picker(picker)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    scrolled = new wxScrolledWindow(this);
    sizer = new wxBoxSizer(wxVERTICAL);

    wxPanel* name_panel = new wxPanel(this);
    wxStaticText* combo_label = new wxStaticText(name_panel, wxID_ANY, "Class:");
    combo = new wxComboBox(name_panel, wxID_ANY);
    for(auto const& ptr : model.object_classes)
        combo->Append(ptr->name);

    wxStaticText* name_label = new wxStaticText(name_panel, wxID_ANY, "Name:");
    name_label->Enable(!picker);
    name = new wxTextCtrl(name_panel, wxID_ANY);
    name->Enable(!picker);
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(name_label, 0, wxALL | wxCENTER, 2);
        sizer->Add(name, 0, wxALL | wxCENTER, 2);
        sizer->AddSpacer(16);

        sizer->Add(combo_label, 0, wxALL | wxCENTER, 2);
        sizer->Add(combo, 0, wxALL, 2);
        sizer->AddSpacer(16);

        name_panel->SetSizer(sizer);
    }
    main_sizer->Add(name_panel, 0, wxALL, 2);

    wxPanel* xy_panel = new wxPanel(this);
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticText* x_label = new wxStaticText(xy_panel, wxID_ANY, "X:");
        x_label->Enable(!picker);
        x_ctrl= new wxSpinCtrl(xy_panel);
        x_ctrl->SetRange(-32768, 32767);
        x_ctrl->Enable(!picker);
        sizer->Add(x_label, 0, wxALL | wxCENTER, 2);
        sizer->Add(x_ctrl, 0, wxALL, 2);
        sizer->AddSpacer(16);

        wxStaticText* y_label = new wxStaticText(xy_panel, wxID_ANY, "Y:");
        y_label->Enable(!picker);
        y_ctrl= new wxSpinCtrl(xy_panel);
        y_ctrl->SetRange(-32768, 32767);
        y_ctrl->Enable(!picker);
        sizer->Add(y_label, 0, wxALL | wxCENTER, 2);
        sizer->Add(y_ctrl, 0, wxALL, 2);
        sizer->AddSpacer(16);

        xy_panel->SetSizer(sizer);
    }
    main_sizer->Add(xy_panel, 0, wxALL, 2);

    wxStaticText* vars_label = new wxStaticText(this, wxID_ANY, "Vars:");
    main_sizer->AddSpacer(4);
    main_sizer->Add(vars_label, 0, wxALL, 2);

    scrolled->SetWindowStyle(wxVSCROLL);
    //hsizer->Add(row1Sizer, 0, wxEXPAND | wxALL, 10);

    load_fields();

    wxButton* reset_button = new wxButton(this, wxID_ANY, "Reset");
    wxButton* ok_button = new wxButton(this, wxID_OK, "Ok");

    wxBoxSizer* button_sizer = new wxBoxSizer(wxHORIZONTAL);
    button_sizer->Add(reset_button, 0, wxALL, 32);
    button_sizer->Add(ok_button, 0, wxALL, 32);
    main_sizer->Add(scrolled, wxSizerFlags().Expand());
    main_sizer->Add(button_sizer, 0, wxALIGN_CENTER);

    scrolled->SetMinSize(wxSize(600, 400));
    scrolled->SetSizer(sizer);
    scrolled->FitInside();
    scrolled->SetScrollRate(25, 25);

    load_object();

    SetSizerAndFit(main_sizer);

    combo->Bind(wxEVT_COMBOBOX, &object_dialog_t::on_combo_select, this);
    combo->Bind(wxEVT_TEXT, &object_dialog_t::on_combo_text, this);
    name->Bind(wxEVT_TEXT, &object_dialog_t::on_name, this);
    x_ctrl->Bind(wxEVT_SPINCTRL, &object_dialog_t::on_change_x, this);
    y_ctrl->Bind(wxEVT_SPINCTRL, &object_dialog_t::on_change_y, this);
    reset_button->Bind(wxEVT_BUTTON, &object_dialog_t::on_reset, this);
}

void object_dialog_t::load_fields()
{
    field_panel.reset(new wxPanel(scrolled));
    sizer->Add(field_panel.get(), wxSizerFlags().Expand());

    wxBoxSizer* panel_sizer = new wxBoxSizer(wxVERTICAL);

    if(oc)
    {
        for(auto const& field : oc->fields)
        {
            auto* of = new object_field_t(field_panel.get(), object, field);
            panel_sizer->Add(of);
        }
    }

    field_panel->SetSizer(panel_sizer);
    scrolled->FitInside();
    Fit();
}

void object_dialog_t::load_object()
{
    if(!picker)
    {
        x_ctrl->SetValue(object.position.x);
        y_ctrl->SetValue(object.position.y);
    }

    for(auto const& ptr : model.object_classes)
    {
        if(ptr->name == object.oclass)
        {
            oc = ptr;
            break;
        }
    }

    combo->SetValue(object.oclass);
    name->SetValue(object.name);

    load_fields();
}

void object_dialog_t::on_combo_select(wxCommandEvent& event)
{
    int const index = event.GetSelection();
    if(index >= 0 && index < model.object_classes.size())
    {
        oc = model.object_classes[index];
        if(oc)
            object.oclass = oc->name;
    }

    load_fields();
}

void object_dialog_t::on_combo_text(wxCommandEvent& event)
{
    object.oclass = combo->GetValue();

    for(auto const& ptr : model.object_classes)
    {
        if(ptr->name == object.oclass)
        {
            oc = ptr;
            break;
        }
    }

    load_fields();
}

void object_dialog_t::on_name(wxCommandEvent& event)
{ 
    object.name = event.GetString().ToStdString(); 
}

void object_dialog_t::on_change_x(wxSpinEvent& event)
{
    // TODO
    //if(!history.on_top<undo_level_palette_t>())
        //history.push(undo_level_palette_t{ level.get(), level->palette });
    object.position.x = event.GetPosition(); 
    //model.refresh_chr(); // TODO
    model.modify();
    GetParent()->Refresh();
}

void object_dialog_t::on_change_y(wxSpinEvent& event)
{
    // TODO
    //if(!history.on_top<undo_level_palette_t>())
        //history.push(undo_level_palette_t{ level.get(), level->palette });
    object.position.y = event.GetPosition(); 
    //model.refresh_chr(); // TODO
    model.modify();
    GetParent()->Refresh();
}

void object_dialog_t::on_change_i(wxSpinEvent& event)
{
    // TODO
}

void object_dialog_t::on_reset(wxCommandEvent& event)
{
    object = object_t{ .position = object.position };
    load_object();
    GetParent()->Refresh();
}

////////////////////////////////////////////////////////////////////////////////
// metatile_picker_t //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void metatile_picker_t::draw_tiles(wxDC& dc)
{
    selector_box_t::draw_tiles(dc);
}

////////////////////////////////////////////////////////////////////////////////
// level_canvas_t //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void level_canvas_t::draw_tiles(wxDC& dc)
{
    bool const object_select = 
        selecting_objects && mouse_down && model.tool == TOOL_SELECT && level->current_layer == OBJECT_LAYER;

    canvas_box_t::draw_tiles(dc);

    dc.SetPen(wxPen(wxColor(255, 0, 255), 0, wxPENSTYLE_DOT));
    dc.SetBrush(wxBrush());

    int const x0 = margin().w;
    int const x1 = margin().w + level->metatile_layer.tiles.dimen().w * 16;
    int const y0 = margin().h;
    int const y1 = margin().h + level->metatile_layer.tiles.dimen().h * 16;

    int y_lines = 0;
    if(level->metatile_layer.tiles.dimen().h % 16 == 0)
        y_lines = 16;
    else if(level->metatile_layer.tiles.dimen().h % 15 == 0)
        y_lines = 15;

    for(unsigned x = 1; x < (level->metatile_layer.tiles.dimen().w + 15) / 16; ++x)
    {
        int const x0 = x * 256 + margin().w;
        dc.DrawLine(x0, y0, x0, y1);
    }

    if(y_lines)
    {
        unsigned const y_limit = (level->metatile_layer.tiles.dimen().h + y_lines - 1) / y_lines;
        for(unsigned y = 1; y < y_limit; ++y)
        {
            int const y0 = y * (16 * y_lines) + margin().h;
            dc.DrawLine(x0, y0, x1, y0);
        }
    }

    // Objects:
    dc.SetLogicalScale(1.0f / scale, 1.0f / scale);

    for(unsigned i = 0; i < level->objects.size(); ++i)
    {
        auto const& object = level->objects[i];
        coord_t const at = vec_mul(crop(object.position) + to_coord(margin()), scale);

        dc.SetPen(wxPen());
        dc.SetBrush(wxBrush(wxColor(255, 255, 255, 32)));
        dc.DrawCircle(at.x, at.y, object_radius() * 3 / 2);
    }

    for(unsigned i = 0; i < level->objects.size(); ++i)
    {
        auto const& object = level->objects[i];

        auto style = wxPENSTYLE_SOLID;
        if(!in_bounds(object.position, vec_mul(level->metatile_layer.tiles.dimen(), 16)))
            style = wxPENSTYLE_DOT;

        rgb_t color = { 120, 120, 120 };

        for(auto const& ptr : model.object_classes)
        {
            if(ptr->name == object.oclass)
            {
                color = ptr->color;
                break;
            }
        }

        if(level->object_selector.count(i))
        {
            dc.SetPen(wxPen(wxColor(color.r, color.g, color.b), 0, style));
            dc.SetBrush(wxBrush(wxColor(color.r, color.g, color.b, 127)));
        }
        else
        {
            dc.SetPen(wxPen(wxColor(color.r, color.g, color.b, 200), 0, style));
            dc.SetBrush(wxBrush(wxColor(color.r, color.g, color.b, 60)));
        }

        coord_t const at = vec_mul(crop(object.position) + to_coord(margin()), scale);
        dc.DrawCircle(at.x, at.y, object_radius());
        dc.DrawPoint(at.x, at.y);
    }

    if(level->current_layer == OBJECT_LAYER)
    {
        if(model.paste && model.paste->format == LAYER_OBJECTS)
        {
            if(auto const* objects = std::get_if<std::vector<object_t>>(&model.paste->data))
            {
                dc.SetPen(wxPen(wxColor(255, 0, 255, 200), 0, wxPENSTYLE_SOLID));
                dc.SetBrush(wxBrush(wxColor(255, 255, 0, 127)));

                for(auto const& object : *objects)
                {
                    coord_t position = object.position;
                    position += from_screen(mouse_current, {1,1});
                    position.x += margin().w;
                    position.y += margin().h;
                    position = vec_mul(position, scale);

                    dc.DrawCircle(position.x, position.y, object_radius());
                    dc.DrawPoint(position.x, position.y);
                }
            }
        }
    }

    dc.SetLogicalScale(1.0f, 1.0f);

    if(object_select)
    {
        rect_t const r = rect_from_2_coords(from_screen(object_select_start, {1,1}), from_screen(mouse_current, {1,1}));
        coord_t const c0 = to_screen(r.c, {1,1});
        coord_t const c1 = to_screen(r.e(), {1,1});

        dc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
        if(mouse_down == MB_LEFT)
            dc.SetBrush(wxBrush(wxColor(0, 255, 255, 127)));
        else
            dc.SetBrush(wxBrush(wxColor(255, 0, 0, 127)));
        dc.DrawRectangle(c0.x, c0.y, (c1 - c0).x, (c1 - c0).y);
    }

}

void level_canvas_t::on_down(mouse_button_t mb, coord_t at)
{
    if(level->current_layer == OBJECT_LAYER)
    {
        coord_t const pixel256 = from_screen(at, {1,1}, 256);
        coord_t const pixel = from_screen(at, {1,1});
        bool const shift = wxGetKeyState(WXK_SHIFT);
        selecting_objects = false;

        if(model.tool == TOOL_STAMP || model.tool == TOOL_SELECT)
        {
            for(int i : level->object_selector)
            {
                if(i >= level->objects.size())
                    continue;

                auto& object = level->objects[i];
                coord_t const at = crop(object.position) + to_coord(margin());

                if(e_dist(vec_mul(at, 256), pixel256) <= object_radius() * 256.0 / scale)
                {
                    if(mb == MB_LEFT)
                    {
                        CallAfter([&, i]()
                        {
                            object_t prev = object;

                            object_dialog_t dialog(this, model, object);
                            dialog.ShowModal();
                            dialog.Destroy();
                            SetFocus();

                            if(prev != object)
                                static_cast<level_editor_t*>(GetParent())->history.push(undo_edit_object_t{ level.get(), i, std::move(prev) });
                        });
                        goto selected;
                    }
                }
            }

            for(int i = level->objects.size() - 1; i >= 0; --i)
            {
                auto const& object = level->objects[i];
                coord_t const at = crop(object.position) + to_coord(margin());

                if(e_dist(vec_mul(at, 256), pixel256) <= object_radius() * 256.0 / scale)
                {
                    if(mb == MB_LEFT)
                    {
                        if(!shift)
                            level->object_selector.clear();
                        level->object_selector.insert(i);
                    }
                    else if(mb == MB_RIGHT)
                    {
                        if(!level->object_selector.count(i))
                        {
                            if(!shift)
                                level->object_selector.clear();
                            level->object_selector.insert(i);
                        }

                        dragging_objects = true;
                        drag_last = pixel;
                    }

                    goto selected;
                }
            }

            if(model.tool == TOOL_SELECT)
            {
                if(mb == MB_RIGHT)
                {
                    for(int i = level->objects.size() - 1; i >= 0; --i)
                    {
                        auto const& object = level->objects[i];
                        coord_t const at = crop(object.position) + to_coord(margin());

                        if(e_dist(vec_mul(at, 256), pixel256) <= object_radius() * 256.0 / scale)
                        {
                            if(!shift && !level->object_selector.count(i))
                                level->object_selector.clear();
                            level->object_selector.insert(i);
                            dragging_objects = true;
                            drag_last = pixel;
                            return;
                        }
                    }
                }

                object_select_start = at;
                selecting_objects = true;
                return;
            }
            else if(model.tool == TOOL_STAMP)
            {
                level->object_selector.clear();

                if(mb == MB_LEFT && !dragging_objects)
                {
                    level->object_selector.insert(level->objects.size());

                    object_t object = model.object_picker;
                    object.position = pixel;


                    static_cast<level_editor_t*>(GetParent())->history.push(undo_new_object_t{ level.get(), { level->objects.size() } });
                    level->objects.push_back(std::move(object));

                    dragging_objects = true;
                    drag_last = pixel;
                    goto selected;
                }
            }
        }
    }

selected:
    Refresh();

    if(level->current_layer == TILE_LAYER)
        canvas_box_t::on_down(mb, at);
}

void level_canvas_t::on_up(mouse_button_t mb, coord_t at)
{
    if(level->current_layer == OBJECT_LAYER)
    {
        if(model.paste && model.paste->format == LAYER_OBJECTS)
        {
            if(mb == MB_RIGHT)
                goto done_paste;
            else if(mb == MB_LEFT)
            {
                model.modify();

                if(auto const* objects = std::get_if<std::vector<object_t>>(&model.paste->data))
                {
                    undo_new_object_t undo = { level.get() };

                    for(auto const& object : *objects)
                    {
                        undo.indices.push_back(level->objects.size());
                        auto& new_object = level->objects.emplace_back(object);
                        new_object.position += from_screen(at, {1,1});
                    }

                    std::sort(undo.indices.begin(), undo.indices.end(), std::greater<>());
                    static_cast<level_editor_t*>(GetParent())->history.push(std::move(undo));
                }

                post_update();
            done_paste:
                model.paste.reset();
                Refresh();
                return;
            }
        }

        bool const shift = wxGetKeyState(WXK_SHIFT);
        dragging_objects = false;

        if(selecting_objects && model.tool == TOOL_SELECT)
        {
            if(mb == MB_LEFT && !shift)
                level->object_selector.clear();

            rect_t const r = rect_from_2_coords(from_screen(object_select_start, {1,1}), from_screen(at, {1,1}));

            for(int i = 0; i < level->objects.size(); ++i)
            {
                auto const& object = level->objects[i];
                coord_t const at = crop(object.position);

                if(in_bounds(at, r))
                {
                    if(mb == MB_LEFT)
                        level->object_selector.insert(i);
                    else
                        level->object_selector.erase(i);
                }
            }
        }
    }

    if(level->current_layer == TILE_LAYER)
        canvas_box_t::on_up(mb, at);
}

void level_canvas_t::on_motion(coord_t at)
{
    if(dragging_objects)
    {
        coord_t const pixel = from_screen(at, {1,1});

        for(int i : level->object_selector)
            if(i < level->objects.size())
                level->objects[i].position += (pixel - drag_last);

        drag_last = pixel;

        Refresh();
    }
    else
        canvas_box_t::on_motion(at);
}

////////////////////////////////////////////////////////////////////////////////
// level_editor_t //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

level_editor_t::level_editor_t(wxWindow* parent, model_t& model, std::shared_ptr<level_model_t> level)
: editor_t(parent)
, model(model)
, level(level)
{
    wxPanel* left_panel = new wxPanel(this);
    picker = new metatile_picker_t(left_panel, model, level);

    object_panel = new wxPanel(left_panel);
    object_panel->SetMinSize(wxSize(256 + 16, 0));
    wxButton* edit_button = new wxButton(object_panel, wxID_ANY, "Edit");
    edit_button->SetMinSize(wxSize(256, 128));
    auto* object_text = new wxStaticText(object_panel, wxID_ANY, "Object Picker");
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(object_text, wxSizerFlags().Border(wxLEFT));
        sizer->Add(edit_button, wxSizerFlags().Border(wxLEFT));
        object_panel->SetSizer(sizer);
    }
    object_panel->Hide();

    auto* macro_label = new wxStaticText(left_panel, wxID_ANY, "Macro");
    macro_ctrl = new wxTextCtrl(left_panel, wxID_ANY);
    macro_ctrl->SetValue(level->macro_name);

    auto* metatiles_label = new wxStaticText(left_panel, wxID_ANY, "Metatiles");
    metatiles_combo = new wxComboBox(left_panel, wxID_ANY);

    auto* chr_label = new wxStaticText(left_panel, wxID_ANY, "CHR");
    chr_combo = new wxComboBox(left_panel, wxID_ANY);

    auto* palette_text = new wxStaticText(left_panel, wxID_ANY, "Palette");
    palette_ctrl = new wxSpinCtrl(left_panel);
    palette_ctrl->SetRange(0, 255);

    auto* dimensions_text = new wxStaticText(left_panel, wxID_ANY, "Dimensions");
    wxPanel* dimensions_panel = new wxPanel(left_panel);
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

        width_ctrl = new wxSpinCtrl(dimensions_panel);
        width_ctrl->SetRange(1, 256);
        width_ctrl->SetValue(level->dimen().w);

        height_ctrl = new wxSpinCtrl(dimensions_panel);
        height_ctrl->SetRange(1, 256);
        height_ctrl->SetValue(level->dimen().h);

        sizer->Add(width_ctrl, wxSizerFlags().Border(wxRIGHT));
        sizer->Add(height_ctrl, wxSizerFlags());
        dimensions_panel->SetSizer(sizer);
    }

    layers[0] = new wxRadioButton(left_panel, wxID_ANY, "Tile Layer    (F1)", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    layers[1] = new wxRadioButton(left_panel, wxID_ANY, "Object Layer  (F2)");

    canvas = new level_canvas_t(this, model, level);

    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(picker, wxSizerFlags().Expand().Proportion(1));
        sizer->Add(object_panel, wxSizerFlags().Expand().Proportion(1));
        for(auto* ptr : layers)
            sizer->Add(ptr, wxSizerFlags().Border(wxLEFT));
        sizer->Add(macro_label, wxSizerFlags().Border(wxLEFT | wxUP));
        sizer->Add(macro_ctrl, wxSizerFlags().Border(wxLEFT | wxDOWN));
        sizer->Add(metatiles_label, wxSizerFlags().Border(wxLEFT));
        sizer->Add(metatiles_combo, wxSizerFlags().Border(wxLEFT | wxDOWN));
        sizer->Add(chr_label, wxSizerFlags().Border(wxLEFT));
        sizer->Add(chr_combo, wxSizerFlags().Border(wxLEFT | wxDOWN));
        sizer->Add(palette_text, wxSizerFlags().Border(wxLEFT));
        sizer->Add(palette_ctrl, wxSizerFlags().Border(wxLEFT | wxDOWN));
        sizer->Add(dimensions_text, wxSizerFlags().Border(wxLEFT));
        sizer->Add(dimensions_panel, wxSizerFlags().Border(wxLEFT));
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
    wxAcceleratorEntry entries[3];
    entries[0].Set(wxACCEL_NORMAL, WXK_F1, ID_LAYER0);
    entries[1].Set(wxACCEL_NORMAL, WXK_F2, ID_LAYER1);
    entries[2].Set(wxACCEL_NORMAL, WXK_DELETE, ID_DELETE_OBJ);
    wxAcceleratorTable accel(3, entries);

    // Set the accelerator table for the frame
    SetAcceleratorTable(accel);

    for(auto* ptr : layers)
        ptr->Bind(wxEVT_RADIOBUTTON, &level_editor_t::on_radio, this);
    Bind(wxEVT_MENU, &level_editor_t::on_active<0>, this, ID_LAYER0);
    Bind(wxEVT_MENU, &level_editor_t::on_active<1>, this, ID_LAYER1);
    Bind(wxEVT_MENU, &level_editor_t::on_delete, this, ID_DELETE_OBJ);
    palette_ctrl->Bind(wxEVT_SPINCTRL, &level_editor_t::on_change_palette, this);
    width_ctrl->Bind(wxEVT_SPINCTRL, &level_editor_t::on_change_width, this);
    height_ctrl->Bind(wxEVT_SPINCTRL, &level_editor_t::on_change_height, this);
    edit_button->Bind(wxEVT_BUTTON, &level_editor_t::on_pick_object, this);
    metatiles_combo->Bind(wxEVT_COMBOBOX, &level_editor_t::on_metatiles_select, this);
    metatiles_combo->Bind(wxEVT_TEXT, &level_editor_t::on_metatiles_text, this);
    chr_combo->Bind(wxEVT_COMBOBOX, &level_editor_t::on_chr_select, this);
    chr_combo->Bind(wxEVT_TEXT, &level_editor_t::on_chr_text, this);
    macro_ctrl->Bind(wxEVT_TEXT, &level_editor_t::on_macro_name, this);

    model_refresh();
}

void level_editor_t::on_active(unsigned i)
{
    level->current_layer = level_layer_t(i);

    if(i == 0)
    {
        picker->Show(true);
        object_panel->Hide();
    }
    else
    {
        picker->Hide();
        object_panel->Show(true);
    }

    layers[i]->SetValue(true);

    Layout();
    Refresh();
}

void level_editor_t::on_radio(wxCommandEvent& event)
{
    wxRadioButton* radio = dynamic_cast<wxRadioButton*>(event.GetEventObject());
    for(unsigned layer = 0; layer < layers.size(); ++layer)
        if(radio == layers[layer])
            return on_active(layer);
}

void level_editor_t::on_update()
{
    if(last_palette != level->palette)
        palette_ctrl->SetValue(last_palette = level->palette);

    if(last_width != level->dimen().w)
        width_ctrl->SetValue(last_width = level->dimen().w);

    if(last_height != level->dimen().h)
        height_ctrl->SetValue(last_height = level->dimen().h);
}


void level_editor_t::on_change_palette(wxSpinEvent& event)
{
    level->palette = event.GetPosition(); 
    model.modify();
    load_metatiles();
    Refresh();

}

void level_editor_t::on_change_width(wxSpinEvent& event)
{
    if(!history.on_top<undo_level_dimen_t>())
        history.push(level->metatile_layer.save());
    int const w = event.GetPosition(); 
    level->resize({ w, level->dimen().h });
    model.modify();
    Update();
    Refresh();
}

void level_editor_t::on_change_height(wxSpinEvent& event)
{
    if(!history.on_top<undo_level_dimen_t>())
        history.push(level->metatile_layer.save());
    int const h = event.GetPosition(); 
    level->resize({ level->dimen().w, h });
    model.modify();
    Update();
    Refresh();
}

void level_editor_t::on_pick_object(wxCommandEvent& event)
{
    object_dialog_t dialog(this, model, model.object_picker, true);
    dialog.ShowModal();
    dialog.Destroy();
    SetFocus();
}

void level_editor_t::model_refresh()
{
    std::string const metatiles_name = level->metatiles_name;
    std::string const chr_name = level->chr_name;

    metatiles_combo->Clear();
    for(auto const& metatiles : model.metatiles)
        metatiles_combo->Append(metatiles->name);
    metatiles_combo->SetValue(metatiles_name);

    chr_combo->Clear();
    for(auto const& chr : model.chr_files)
        chr_combo->Append(chr.name);
    chr_combo->SetValue(chr_name);

    load_metatiles();
}

void level_editor_t::load_metatiles()
{
    auto* chr_file = lookup_name(level->chr_name, model.chr_files);
    auto* metatiles = lookup_name_ptr(level->metatiles_name, model.metatiles).get();
    if(chr_file && metatiles)
        level->refresh_metatiles(*metatiles, chr_file->chr, model.palette_array(level->palette));
    else
        level->clear_metatiles();
    Refresh();
}

void level_editor_t::on_metatiles_select(wxCommandEvent& event)
{
    int const index = event.GetSelection();
    if(index >= 0 && index < model.metatiles.size())
    {
        level->metatiles_name = model.metatiles[index]->name;

        level->chr_name = model.metatiles[index]->chr_name;
        chr_combo->SetValue(level->chr_name);

        level->palette = model.metatiles[index]->palette;
        palette_ctrl->SetValue(level->palette);
    }
    load_metatiles();
}

void level_editor_t::on_metatiles_text(wxCommandEvent& event)
{
    level->metatiles_name = metatiles_combo->GetValue();
    load_metatiles();
}

void level_editor_t::on_chr_select(wxCommandEvent& event)
{
    int const index = event.GetSelection();
    if(index >= 0 && index < model.chr_files.size())
        level->chr_name = model.chr_files[index].name;
    load_metatiles();
}

void level_editor_t::on_chr_text(wxCommandEvent& event)
{
    level->chr_name = chr_combo->GetValue();
    load_metatiles();
}

void level_editor_t::on_delete(wxCommandEvent& event)
{
    if(level->object_selector.empty())
        return;

    undo_delete_object_t undo = { level.get() };

    for(int i : level->object_selector)
        if(i < level->objects.size())
            undo.objects.emplace_back(i, level->objects.at(i));

    history.push(std::move(undo));

    for(int i : level->object_selector | std::views::reverse)
        if(i < level->objects.size())
            level->objects.erase(level->objects.begin() + i);

    level->object_selector.clear();
    Refresh();
}

void level_editor_t::on_macro_name(wxCommandEvent& event)
{ 
    level->macro_name = event.GetString().ToStdString(); 
}

tile_copy_t level_editor_t::copy(bool cut)
{
    if(level->current_layer == OBJECT_LAYER)
    {
        std::vector<object_t> objects;

        coord_t avg = {};
        unsigned count = 0;
        for(int i : level->object_selector)
        {
            if(i >= level->objects.size())
                continue;
            avg += level->objects.at(i).position;
            count += 1;
        }

        if(count > 0)
            avg = vec_div(avg, count);

        if(cut && !level->object_selector.empty())
        {
            undo_delete_object_t undo = { level.get() };

            for(int i : level->object_selector)
                if(i < level->objects.size())
                    undo.objects.emplace_back(i, level->objects.at(i));

            history.push(std::move(undo));
        }

        for(int i : level->object_selector | std::views::reverse)
        {
            if(i >= level->objects.size())
                continue;

            auto& copied = objects.emplace_back(level->objects.at(i));
            copied.position -= avg;
            
            if(cut)
                level->objects.erase(level->objects.begin() + i);
        }

        if(cut)
        {
            level->object_selector.clear();
            Refresh();
        }

        return { LAYER_OBJECTS, std::move(objects) };
    }
    else
        return editor_t::copy(cut);
}
