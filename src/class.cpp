#include "class.hpp"

field_def_t::field_def_t(wxWindow* parent, class_field_t const& field, unsigned index)
: wxPanel(parent, wxID_ANY)
, index(index)
, field(field)
{
    wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText* type_label = new wxStaticText(this, wxID_ANY, "Type:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    type_entry = new wxTextCtrl(this, wxID_ANY, field.type);
    type_entry->SetMinSize(wxSize(100, 24));
    type_entry->SetWindowStyleFlag(wxTE_READONLY);

    wxStaticText* name_label = new wxStaticText(this, wxID_ANY, "Name:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    name_entry = new wxTextCtrl(this, wxID_ANY, field.name);
    name_entry->SetWindowStyleFlag(wxTE_READONLY);
    name_entry->SetMinSize(wxSize(200, 24));
    name_entry->SetMaxSize(wxSize(200, 24));

    wxButton* retype_button = new wxButton(this, wxID_ANY, "Retype");
    wxButton* rename_button = new wxButton(this, wxID_ANY, "Rename");
    wxButton* delete_button = new wxButton(this, wxID_ANY, "Delete");

    row_sizer->Add(type_label, wxSizerFlags().Left().Border().Center());
    row_sizer->Add(type_entry, wxSizerFlags().Left().Border().Center());
    row_sizer->Add(retype_button, wxSizerFlags().Left().Border().Center());
    row_sizer->AddSpacer(16);
    row_sizer->Add(name_label, wxSizerFlags().Left().Border().Center());
    row_sizer->Add(name_entry, wxSizerFlags().Left().Border().Center());
    row_sizer->AddSpacer(16);
    row_sizer->Add(rename_button, wxSizerFlags().Left().Border().Center());
    row_sizer->Add(delete_button, wxSizerFlags().Left().Border().Center());

    rename_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &field_def_t::on_rename, this);
    delete_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &field_def_t::on_click, this);
    retype_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &field_def_t::on_retype, this);

    SetSizerAndFit(row_sizer);
}

void field_def_t::on_rename(wxCommandEvent& event)
{ 
    wxTextEntryDialog dialog(
        this, "New Name:", "Rename Field", field.name);

    if(dialog.ShowModal() == wxID_OK)
    {
        std::string new_name = dialog.GetValue().ToStdString();
        static_cast<class_editor_t*>(GetParent())->on_rename(index, new_name); 
        name_entry->SetValue(field.name);
    }
}

void field_def_t::on_click(wxCommandEvent& event)
{ 
    static_cast<class_editor_t*>(GetParent())->on_delete(index); 
}

void field_def_t::on_retype(wxCommandEvent& event)
{ 
    wxTextEntryDialog dialog(
        this, "New Type:", "Retype Field", field.type);

    if(dialog.ShowModal() == wxID_OK)
    {
        std::string new_type = dialog.GetValue().ToStdString();
        static_cast<class_editor_t*>(GetParent())->on_retype(index, new_type); 
        type_entry->SetValue(field.type);
    }
}

class_editor_t::class_editor_t(wxWindow* parent, model_t& model, std::shared_ptr<object_class_t> oc_)
: wxScrolledWindow(parent)
, model(model)
, oc(std::move(oc_))
{
    assert(oc);

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    field_sizer = new wxBoxSizer(wxVERTICAL);

    {
        wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);
        main_sizer->Add(row_sizer, wxSizerFlags().Expand());
        main_sizer->AddSpacer(2);

        wxStaticText* color_label = new wxStaticText(this, wxID_ANY, "Display Color:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
        wxColourPickerCtrl* color_picker = new wxColourPickerCtrl(this, wxID_ANY);
        color_picker->SetColour(wxColor(oc->color.r, oc->color.g, oc->color.b));

        row_sizer->Add(color_label, wxSizerFlags().Left().Border().Center());
        row_sizer->Add(color_picker, wxSizerFlags().Left().Border().Center());

        color_picker->Bind(wxEVT_COLOURPICKER_CHANGED, &class_editor_t::on_color, this);
    }

    load();

    wxButton* new_button = new wxButton(this, wxID_ANY, "New Field");
    main_sizer->Add(field_sizer);
    main_sizer->Add(new_button, wxSizerFlags().Left().Border());

    SetWindowStyle(wxVSCROLL);
    SetSizer(main_sizer);
    FitInside();
    SetScrollRate(25, 25);

    new_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &class_editor_t::on_new, this);
}

void class_editor_t::on_delete(unsigned index) 
{ 
    wxString text;
    text << "Delete";
    if(!oc->fields[index].name.empty())
    {
        text << ' ';
        text << oc->fields[index].name;
    }
    text << "?\nThis cannot be undone.";

    wxMessageDialog dialog(this, text, "Warning", wxOK | wxCANCEL | wxICON_WARNING);
    if(dialog.ShowModal() == wxID_OK)
    {
        field_defs.erase(field_defs.begin() + index); 
        oc->fields.erase(oc->fields.begin() + index); 
        for(unsigned i = 0; i < field_defs.size(); ++i)
            field_defs[i]->index = i;
        FitInside();
        model.modify();
    }
}

void class_editor_t::on_new(wxCommandEvent& event)
{
    wxTextEntryDialog dialog(
        this, "Name:", "New Field");

    if(dialog.ShowModal() == wxID_OK)
    {
        std::string new_name = dialog.GetValue().ToStdString();

        for(auto const& field : oc->fields)
        {
            if(field.name == new_name)
            {
                wxMessageBox( wxT("Names must be unique."), wxT("Error"), wxICON_ERROR);
                return;
            }
        }

        auto& field = oc->fields.emplace_back();
        field.name = new_name;
        new_field<true>(field);
        FitInside();
        model.modify();
    }
}

void class_editor_t::on_retype(unsigned index, std::string str)
{
    oc->fields[index].type = str;
    model.modify();
}

void class_editor_t::on_rename(unsigned index, std::string str)
{
    for(unsigned i = 0; i < oc->fields.size(); ++i)
    {
        if(i != index && oc->fields[i].name == str)
        {
            wxMessageBox( wxT("Names must be unique."), wxT("Error"), wxICON_ERROR);
            return;
        }
    }

    std::string const old_name = oc->fields[index].name;
    oc->fields[index].name = str;

    for(auto& level : model.levels)
    {
        for(auto& object : level->objects)
        {
            if(object.oclass == oc->name)
            {
                auto it = object.fields.find(old_name);
                if(it != object.fields.end())
                    object.fields[str] = it->second;
            }
        }
    }

    model.modify();
}

void class_editor_t::on_color(wxColourPickerEvent& event)
{
    wxColour const color = event.GetColour();
    oc->color = { color.Red(), color.Green(), color.Blue() };
    model.modify();
}

template<bool Modify>
void class_editor_t::new_field(class_field_t const& field)
{
    auto* def = new field_def_t(this, field, field_defs.size());
    field_defs.emplace_back(def);
    field_sizer->Add(def, wxSizerFlags().Expand());
    if(Modify)
        model.modify();
}

void class_editor_t::load()
{
    field_defs.clear();
    for(auto const& field : oc->fields)
        new_field<false>(field);

    FitInside();
}


