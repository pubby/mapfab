#include "chr.hpp"

#include "convert.hpp"

////////////////////////////////////////////////////////////////////////////////
// file_def_t //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

file_def_t::file_def_t(wxWindow* parent, chr_file_t const& file, unsigned index)
: wxPanel(parent, wxID_ANY)
, index(index)
{
    wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText* name_label = new wxStaticText(this, wxID_ANY, "CHR Name:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    wxTextCtrl* name_entry = new wxTextCtrl(this, wxID_ANY, file.name);
    name_entry->SetMinSize(wxSize(150, 24));
    name_entry->SetMaxSize(wxSize(300, 24));

    wxStaticText* path_label = new wxStaticText(this, wxID_ANY, "Path:", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);

    filename = new wxTextCtrl(this, wxID_ANY);
    filename->SetValue(file.path.string());
    filename->SetWindowStyleFlag(wxTE_READONLY);
    filename->SetMinSize(wxSize(400, 24));
    filename->SetMaxSize(wxSize(400, 24));

    wxButton* open_button = new wxButton(this, wxID_ANY, "Set Path");
    wxButton* delete_button = new wxButton(this, wxID_ANY, "Delete");

    row_sizer->Add(name_label, wxSizerFlags().Left().Border().Center());
    row_sizer->Add(name_entry, wxSizerFlags().Left().Border().Center());
    row_sizer->AddSpacer(16);
    row_sizer->Add(path_label, wxSizerFlags().Left().Border().Center());
    row_sizer->AddSpacer(16);
    row_sizer->Add(filename, wxSizerFlags().Left().Border().Center());
    row_sizer->AddSpacer(16);
    row_sizer->Add(open_button, wxSizerFlags().Left().Border().Center());
    row_sizer->AddSpacer(4);
    row_sizer->Add(delete_button, wxSizerFlags().Left().Border().Center());

    delete_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &file_def_t::on_delete, this);
    open_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &file_def_t::on_open, this);
    name_entry->Bind(wxEVT_TEXT, &file_def_t::on_name, this);

    SetSizerAndFit(row_sizer);
}

void file_def_t::on_delete(wxCommandEvent& event)
{ 
    static_cast<chr_editor_t*>(GetParent())->on_delete(index); 
}

void file_def_t::on_open(wxCommandEvent& event)
{ 
    wxFileDialog open_dialog(
        this, _("Choose a file to open"), wxEmptyString, wxEmptyString, 
        _("CHR files (*.chr, *.bin, *.png)|*.chr;*.bin;*.png"),
        wxFD_OPEN, wxDefaultPosition);

    // Creates a "open file" dialog with the file types
    if(open_dialog.ShowModal() == wxID_OK) // if the user click "Open" instead of "Cancel"
    {
        std::string p = open_dialog.GetPath().ToStdString();
        filename->SetValue(p);
        static_cast<chr_editor_t*>(GetParent())->on_open(index, p); 
        Refresh();
    }
}

void file_def_t::on_name(wxCommandEvent& event)
{ 
    static_cast<chr_editor_t*>(GetParent())->on_name(index, event.GetString().ToStdString()); 
}

////////////////////////////////////////////////////////////////////////////////
// chr_editor_t ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

chr_editor_t::chr_editor_t(wxWindow* parent, model_t& model)
: wxScrolledWindow(parent)
, model(model)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    file_sizer = new wxBoxSizer(wxVERTICAL);

    {
        wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);
        main_sizer->Add(row_sizer, wxSizerFlags().Expand());
        main_sizer->AddSpacer(2);

        wxStaticText* collision_label = new wxStaticText(this, wxID_ANY, "Collision Tileset Path:", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
        collision_filename = new wxTextCtrl(this, wxID_ANY);
        collision_filename->SetValue(model.collision_path.string());
        collision_filename->SetWindowStyleFlag(wxTE_READONLY);
        collision_filename->SetMinSize(wxSize(400, 24));
        collision_filename->SetMaxSize(wxSize(400, 24));
        wxButton* open_button = new wxButton(this, wxID_ANY, "Set Path");

        row_sizer->Add(collision_label, wxSizerFlags().Left().Border().Center());
        row_sizer->Add(collision_filename, wxSizerFlags().Left().Border().Center());
        row_sizer->Add(open_button, wxSizerFlags().Left().Border().Center());

        open_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &chr_editor_t::on_open_collision, this);
    }

    for(auto const& file : model.chr_files)
        new_file(file);

    wxButton* new_button = new wxButton(this, wxID_ANY, "New CHR");
    main_sizer->Add(file_sizer);
    main_sizer->Add(new_button, wxSizerFlags().Left().Border());

    SetWindowStyle(wxVSCROLL);
    SetSizer(main_sizer);
    FitInside();
    SetScrollRate(25, 25);

    new_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &chr_editor_t::on_new, this);
}

void chr_editor_t::on_open_collision(wxCommandEvent& event)
{ 
    wxFileDialog open_dialog(
        this, _("Choose a file to open"), wxEmptyString, wxEmptyString, 
        _("Image Files (*.png)|*.png"),
        wxFD_OPEN, wxDefaultPosition);

    // Creates a "open file" dialog with the file types
    if(open_dialog.ShowModal() == wxID_OK) // if the user click "Open" instead of "Cancel"
    {
        std::string filename = open_dialog.GetPath().ToStdString();
        collision_filename->SetValue(filename);
        model.collision_path = filename;
        model.collision_bitmaps = load_collision_file(filename);
        Refresh();
    }
}

void chr_editor_t::on_delete(unsigned index) 
{ 
    wxString text;
    text << "Delete";
    if(!model.chr_files[index].name.empty())
    {
        text << ' ';
        text << model.chr_files[index].name;
    }
    text << "?\nThis cannot be undone.";

    wxMessageDialog dialog(this, text, "Warning", wxOK | wxCANCEL | wxICON_WARNING);
    if(dialog.ShowModal() == wxID_OK)
    {
        file_defs.erase(file_defs.begin() + index); 
        model.chr_files.erase(model.chr_files.begin() + index); 
        for(unsigned i = 0; i < file_defs.size(); ++i)
            file_defs[i]->index = i;
        Fit();
    }
}

void chr_editor_t::on_new(wxCommandEvent& event)
{
    new_file(model.chr_files.emplace_back());
    Fit();
}

void chr_editor_t::on_name(unsigned index, std::string str)
{
    model.chr_files[index].name = str;
}

void chr_editor_t::on_open(unsigned index, std::string path)
{
    model.chr_files[index].path = path;
    model.chr_files[index].load();
}

void chr_editor_t::new_file(chr_file_t const& file)
{
    auto* def = new file_def_t(this, file, file_defs.size());
    file_defs.emplace_back(def);
    file_sizer->Add(def, wxSizerFlags().Expand());
}
