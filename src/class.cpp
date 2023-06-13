#include "class.hpp"

field_def_t::field_def_t(wxWindow* parent, class_field_t const& field, unsigned index)
: wxPanel(parent, wxID_ANY)
, index(index)
{
    wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText* type_label = new wxStaticText(this, wxID_ANY, "Type:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    wxTextCtrl* type_entry = new wxTextCtrl(this, wxID_ANY, field.type);
    type_entry->SetMinSize(wxSize(100, 24));

    wxStaticText* name_label = new wxStaticText(this, wxID_ANY, "Name:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    wxTextCtrl* name_entry = new wxTextCtrl(this, wxID_ANY, field.name);
    name_entry->SetMinSize(wxSize(300, 24));
    name_entry->SetMaxSize(wxSize(200, 24));

    wxButton* delete_button = new wxButton(this, wxID_ANY, "Delete");

    row_sizer->Add(type_label, wxSizerFlags().Left().Border().Center());
    row_sizer->Add(type_entry, wxSizerFlags().Left().Border().Center());
    row_sizer->AddSpacer(16);
    row_sizer->Add(name_label, wxSizerFlags().Left().Border().Center());
    row_sizer->Add(name_entry, wxSizerFlags().Left().Border().Center());
    row_sizer->AddSpacer(16);
    row_sizer->Add(delete_button, wxSizerFlags().Left().Border().Center());

    delete_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &field_def_t::on_click, this);
    type_entry->Bind(wxEVT_TEXT, &field_def_t::on_type, this);
    name_entry->Bind(wxEVT_TEXT, &field_def_t::on_name, this);

    SetSizerAndFit(row_sizer);
}

void field_def_t::on_click(wxCommandEvent& event)
{ 
    static_cast<class_editor_t*>(GetParent())->on_delete(index); 
}

void field_def_t::on_type(wxCommandEvent& event)
{ 
    static_cast<class_editor_t*>(GetParent())->on_type(index, event.GetString().ToStdString()); 
}

void field_def_t::on_name(wxCommandEvent& event)
{ 
    static_cast<class_editor_t*>(GetParent())->on_name(index, event.GetString().ToStdString()); 
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
        color_picker->SetColour(*wxWHITE);

        row_sizer->Add(color_label, wxSizerFlags().Left().Border().Center());
        row_sizer->Add(color_picker, wxSizerFlags().Left().Border().Center());

        color_picker->Bind(wxEVT_COLOURPICKER_CHANGED, &class_editor_t::on_color, this);
    }

    for(auto const& field : oc->fields)
        new_field(field);

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
        Fit();
    }
}

void class_editor_t::on_new(wxCommandEvent& event)
{
    new_field(oc->fields.emplace_back());
    Fit();
}

void class_editor_t::on_type(unsigned index, std::string str)
{
    oc->fields[index].type = str;
}

void class_editor_t::on_name(unsigned index, std::string str)
{
    oc->fields[index].name = str;
}

void class_editor_t::on_color(wxColourPickerEvent& event)
{
    wxColour const color = event.GetColour();
    oc->color = { color.Red(), color.Green(), color.Blue() };
}

void class_editor_t::new_field(class_field_t const& field)
{
    auto* def = new field_def_t(this, field, field_defs.size());
    field_defs.emplace_back(def);
    field_sizer->Add(def, wxSizerFlags().Expand());
}
