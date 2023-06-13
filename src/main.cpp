#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/dialog.h>
#include <wx/fswatcher.h>
#include <wx/bookctrl.h>
#include <wx/mstream.h>
#include <wx/clipbrd.h>

#include <filesystem>

#include "2d/geometry.hpp"

#include "stamp.png.inc"
#include "paint.png.inc"
#include "dropper.png.inc"
#include "select.png.inc"
#include "paste.png.inc"

#include "model.hpp"
#include "convert.hpp"
#include "metatiles.hpp"
#include "palette.hpp"
#include "level.hpp"
#include "class.hpp"
#include "id.hpp"
#include "tool.hpp"

using namespace i2d;

class clip_data_t : public wxDataObjectSimple
{
public:
    explicit clip_data_t(tile_copy_t const& cp = {}) 
    : wxDataObjectSimple("tiles")
    , data(cp.to_vec())
    {}

    virtual size_t GetDataSize() const override
    {
        return data.size() * sizeof(std::uint16_t);
    }

    virtual bool GetDataHere(void* buf) const override
    {
        std::memcpy(buf, data.data(), GetDataSize());
        return true;
    }

    virtual bool SetData(size_t len, const void* buf) override
    {
        std::uint16_t const* ptr = static_cast<std::uint16_t const*>(buf);
        data.assign(ptr, ptr + (len / sizeof(std::uint16_t)));
        return true;
    }

    tile_copy_t get() const { return tile_copy_t::from_vec(data); }

private:
    std::vector<std::uint16_t> data;
};

class MyApp: public wxApp
{
    bool OnInit();
    
    // In your App class that derived from wxApp
    virtual bool OnExceptionInMainLoop() override
    {
        try { throw; }
        catch(std::exception &e)
        {
            wxMessageBox(e.what(), "C++ Exception Caught", wxOK);
        }
        return true;   // continue on. Return false to abort program
    }

public:
        
};

IMPLEMENT_APP(MyApp)

class MyFrame : public wxFrame
{
public:
    MyFrame();
 
private:
    void on_open_chr(wxCommandEvent& event);
    void on_open_collision(wxCommandEvent& event);
    void on_exit(wxCommandEvent& event);
    void on_about(wxCommandEvent& event);

    void on_new_window(wxCommandEvent& event);
    void on_open(wxCommandEvent& event);
    void on_save(wxCommandEvent& event);
    void on_save_as(wxCommandEvent& event);
    void do_save();
    void refresh_title();

    template<undo_type_t U>
    void on_undo(wxCommandEvent& event)
    {
        if(editor_t* editor = get_editor())
            editor->history.undo<U>(model);
        Update();
        Refresh();
    }
 
    void refresh_menus()
    {
        for(unsigned i = 0; i < 2; ++i)
        {
            if(editor_t* editor = get_editor())
                undo_item[i]->Enable(!editor->history.empty(undo_type_t(i)));
            else
                undo_item[i]->Enable(false);
        }

        if(editor_t* editor = get_editor())
        {
            // 'can_paste' disabled due to GTK bug:
            /*
            tile_copy_t c;
            bool const can_paste = get_paste(c) && c.format == editor->layer().format();
            */
            bool const can_paste = true;
            paste->Enable(can_paste);

            bool const metatiles = notebook->GetSelection() == TAB_METATILES;

            bool const can_fill = model.tool == TOOL_SELECT && editor->layer().canvas_selector.select_rect();
            cut->Enable(can_fill);
            copy->Enable(can_fill);
            fill->Enable(can_fill);
            fill_paste->Enable(can_fill && can_paste);
            fill_attribute->Enable(can_fill && metatiles && model.metatiles.active < 4);
        }
    }

    void update_ui(wxUpdateUIEvent& event)
    {
        refresh_title();
        refresh_menus();
    }

    void on_watcher(wxFileSystemWatcherEvent& event)
    {
        wxFileName path = event.GetPath();

        // Ignore all watch events for folders.
        if(path.IsDir() || path.GetSize() == 0)
            return;

        if(event.GetChangeType() == wxFSW_EVENT_MODIFY)
        {
            if(path == chr_path)
                load_chr();
            if(path == collisions_path)
                load_collisions();
        }
    }

    void reset_watcher()
    {
        if(!watcher)
        {
            watcher.reset(new wxFileSystemWatcher());
            watcher->SetOwner(this);
        }

        watcher->RemoveAll();
        watcher->Add(chr_path);
        watcher->Add(collisions_path);
    }

    void load_chr()
    {
        if(chr_path.IsEmpty())
            return;
        std::vector<std::uint8_t> data = read_binary_file(chr_path.c_str());
        model.chr.fill(0);
        std::copy_n(data.begin(), std::min(data.size(), model.chr.size()), model.chr.begin());
        model.refresh_chr();
        Refresh();
    }

    void load_collisions()
    {
        if(collisions_path.IsEmpty())
            return;
        model.collision_bitmaps = load_collision_file(collisions_path);
        Refresh();
    }

    template<tool_t T>
    void on_tool(wxCommandEvent& event)
    {
        model.tool = T;
        Refresh();
    }

    template<bool Cut>
    void on_copy(wxCommandEvent& event)
    {
        if(wxTheClipboard->Open())
        {
            if(editor_t* editor = get_editor())
                wxTheClipboard->SetData(new clip_data_t(editor->copy(Cut)));
            wxTheClipboard->Close();
        }
    }

    bool get_paste(tile_copy_t& copy)
    {
        bool ret = false;

        if(editor_t* editor = get_editor())
        {
            if(wxTheClipboard->Open())
            {
                if(wxTheClipboard->IsSupported("tiles"))
                {
                    clip_data_t data;
                    wxTheClipboard->GetData(data);
                    copy = data.get();
                    ret = true;
                }
                wxTheClipboard->Close();
            }
        }

        return ret;
    }
    
    void on_paste(wxCommandEvent& event)
    {
        tile_copy_t copy;
        if(get_paste(copy))
        {
            model.paste.reset(new tile_copy_t(std::move(copy)));
            Refresh();
        }
    }

    void on_fill(wxCommandEvent& event)
    {
        if(editor_t* editor = get_editor())
        {
            editor->history.push(editor->layer().fill());
            Refresh();
        }
    }

    void on_fill_paste(wxCommandEvent& event)
    {
        if(editor_t* editor = get_editor())
        {
            if(wxTheClipboard->Open())
            {
                if(wxTheClipboard->IsSupported("tiles"))
                {
                    clip_data_t data;
                    wxTheClipboard->GetData(data);
                    editor->history.push(editor->layer().fill_paste(data.get()));
                    Refresh();
                }
                wxTheClipboard->Close();
            }
        }
    }

    void on_fill_attribute(wxCommandEvent& event)
    {
        if(editor_t* editor = get_editor())
        {
            editor->history.push(model.metatiles.chr_layer.fill_attribute());
            Refresh();
        }
    }

    editor_t* get_editor()
    {
        switch(notebook->GetSelection())
        {
        default: return nullptr;
        case TAB_PALETTE: return palette_editor;
        case TAB_METATILES: return metatile_editor;
        case TAB_LEVELS: return levels_panel->page();
        }
    }

    wxNotebook* notebook;

    palette_editor_t* palette_editor;
    metatile_editor_t* metatile_editor;
    levels_panel_t* levels_panel;
    class_panel_t* class_panel;

    model_t model;

    std::array<wxMenuItem*, 2> undo_item;
    wxMenuItem* cut;
    wxMenuItem* copy;
    wxMenuItem* paste;
    wxMenuItem* fill;
    wxMenuItem* fill_paste;
    wxMenuItem* fill_attribute;

    wxString project_path;
    wxString chr_path;
    wxString collisions_path;
    std::unique_ptr<wxFileSystemWatcher> watcher;
};

bool MyApp::OnInit()
{
    wxInitAllImageHandlers();

    //frame = new wxFrame((wxFrame *)NULL, -1,  wxT("Hello wxDC"), wxPoint(50,50), wxSize(800,600));

    wxFrame* frame = new MyFrame();
    frame->Show();
    frame->SendSizeEvent();
    return true;
} 
 
MyFrame::MyFrame()
: wxFrame(nullptr, wxID_ANY, "MapFab", wxDefaultPosition, wxSize(800, 600))
{
    wxMenu* menu_file = new wxMenu;
    menu_file->Append(wxID_NEW, "&New Project Window\tCTRL+N");
    menu_file->Append(wxID_OPEN, "&Open Project\tCTRL+O");
    menu_file->Append(wxID_SAVE, "&Save Project\tCTRL+S");
    menu_file->Append(wxID_SAVEAS, "Save Project &As\tSHIFT+CTRL+S");
    menu_file->AppendSeparator();
    menu_file->Append(ID_OPEN_CHR, "Load CHR Image",
                      "CHR comprises 256 8x8 tiles representing the graphics.");
    menu_file->Append(ID_OPEN_COLLISION, "Load Collision Image",
                      "Collision comprises 256 16x16 tiles representing tile properties.");
    menu_file->AppendSeparator();
    menu_file->Append(ID_IMPORT_MT,"Import Metatiles");
    menu_file->Append(ID_IMPORT_LEVEL, "Import Level");
    menu_file->AppendSeparator();
    menu_file->Append(ID_EXPORT_MT,"Export Metatiles");
    menu_file->Append(ID_EXPORT_LEVEL, "Export Level");
    menu_file->Append(wxID_EXIT);

    wxMenu* menu_edit = new wxMenu;
    undo_item[UNDO] = menu_edit->Append(wxID_UNDO, "&Undo\tCTRL+Z");
    undo_item[REDO] = menu_edit->Append(wxID_REDO, "&Redo\tCTRL+R");
    menu_edit->AppendSeparator();
    cut = menu_edit->Append(wxID_CUT, "Cut\tCTRL+X");
    copy = menu_edit->Append(wxID_COPY, "Copy\tCTRL+C");
    paste = menu_edit->Append(wxID_PASTE, "Paste\tCTRL+V");
    menu_edit->AppendSeparator();
    fill = menu_edit->Append(ID_FILL, "Fill Selection\tCTRL+F");
    fill_paste = menu_edit->Append(ID_FILL_PASTE, "Fill Selection with Paste\tCTRL+SHIFT+F");
    fill_attribute = menu_edit->Append(ID_FILL_ATTRIBUTE, "Fill Selection with Attribute\tCTRL+SHIFT+A");

    wxMenu* menu_view = new wxMenu;
    menu_view->Append(ID_ZOOM_100,  "&Zoom 100%");
    menu_view->Append(ID_ZOOM_200,  "&Zoom 200%");
    menu_view->Append(ID_ZOOM_400,  "&Zoom 400%");

    wxMenu* menu_level = new wxMenu;
    menu_level->Append(ID_NEW_LEVEL, "New Level\tSHIFT+CTRL+N");
    menu_level->Append(ID_DELETE_LEVEL, "Delete Level\tSHIFT+CTRL+D");
 
    wxMenu* menu_help = new wxMenu;
    menu_help->Append(wxID_ABOUT);
 
    wxMenuBar* menu_bar = new wxMenuBar;
    menu_bar->Append(menu_file, "&File");
    menu_bar->Append(menu_edit, "&Edit");
    menu_bar->Append(menu_view, "&View");
    //menu_bar->Append(menu_attr, "&Attribute");
    menu_bar->Append(menu_level, "&Levels");
    menu_bar->Append(menu_help, "&Help");

    SetMenuBar(menu_bar);
 
    CreateStatusBar();

    auto const make_bitmap = [&](char const* name, unsigned char const* data, std::size_t size)
    {
        wxMemoryInputStream stream(data, size);
        wxImage img;
        if(!img.LoadFile(stream, wxBITMAP_TYPE_PNG))
            throw std::runtime_error(name);
        return wxBitmap(img);
    };
#define MAKE_BITMAP(x) make_bitmap(#x, x, x##_size)

    wxToolBar* tool_bar = new wxToolBar(this, wxID_ANY);
    tool_bar->SetWindowStyle(wxTB_VERTICAL);
    tool_bar->AddRadioTool(ID_TOOL_STAMP, "Stamp", MAKE_BITMAP(src_img_stamp_png));
    tool_bar->AddRadioTool(ID_TOOL_DROPPER, "Dropper", MAKE_BITMAP(src_img_dropper_png));
    tool_bar->AddRadioTool(ID_TOOL_SELECT, "Select", MAKE_BITMAP(src_img_select_png));

    notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxSize(600, 300));

    palette_editor = new palette_editor_t(notebook, model);
    notebook->AddPage(palette_editor, wxT("Palettes"));

    metatile_editor = new metatile_editor_t(notebook, model);
    notebook->AddPage(metatile_editor, wxT("Metatiles"));

    levels_panel = new levels_panel_t(notebook, model);
    notebook->AddPage(levels_panel, wxT("Levels"));

    class_panel = new class_panel_t(notebook, model);
    notebook->AddPage(class_panel, wxT("Object Classes"));

    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

    sizer->Add(tool_bar, wxSizerFlags().Expand());
    sizer->Add(notebook, wxSizerFlags().Expand().Proportion(1));
    SetSizer(sizer);
    //SetAutoLayout(true);

    Bind(wxEVT_MENU, &MyFrame::on_open_chr, this, ID_OPEN_CHR);
    Bind(wxEVT_MENU, &MyFrame::on_open_collision, this, ID_OPEN_COLLISION);
    Bind(wxEVT_MENU, &MyFrame::on_about, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &MyFrame::on_exit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MyFrame::on_undo<UNDO>, this, wxID_UNDO);
    Bind(wxEVT_MENU, &MyFrame::on_undo<REDO>, this, wxID_REDO);
    Bind(wxEVT_MENU, &MyFrame::on_new_window, this, wxID_NEW);
    Bind(wxEVT_MENU, &MyFrame::on_open, this, wxID_OPEN);
    Bind(wxEVT_MENU, &MyFrame::on_save, this, wxID_SAVE);
    Bind(wxEVT_MENU, &MyFrame::on_save_as, this, wxID_SAVEAS);
    Bind(wxEVT_MENU, &MyFrame::on_copy<true>, this, wxID_CUT);
    Bind(wxEVT_MENU, &MyFrame::on_copy<false>, this, wxID_COPY);
    Bind(wxEVT_MENU, &MyFrame::on_paste, this, wxID_PASTE);
    Bind(wxEVT_MENU, &MyFrame::on_fill, this, ID_FILL);
    Bind(wxEVT_MENU, &MyFrame::on_fill_paste, this, ID_FILL_PASTE);
    Bind(wxEVT_MENU, &MyFrame::on_fill_attribute, this, ID_FILL_ATTRIBUTE);

    Bind(wxEVT_TOOL, &MyFrame::on_tool<TOOL_STAMP>, this, ID_TOOL_STAMP);
    Bind(wxEVT_TOOL, &MyFrame::on_tool<TOOL_DROPPER>, this, ID_TOOL_DROPPER);
    Bind(wxEVT_TOOL, &MyFrame::on_tool<TOOL_SELECT>, this, ID_TOOL_SELECT);

    Bind(wxEVT_UPDATE_UI, &MyFrame::update_ui, this);

    model.refresh_chr();

    Update();
}
 
void MyFrame::on_exit(wxCommandEvent& event)
{
    Close(true);
}
 
void MyFrame::on_about(wxCommandEvent& event)
{
    wxString string;
    string << "MapFab version " << VERSION << ".\n";
    string << "Copyright (C) 2023, Patrick Bene.\n";
    string << "This is free software. There is no warranty.";
    wxMessageBox(string, "About MapFab", wxOK | wxICON_INFORMATION);
}

void MyFrame::on_open_chr(wxCommandEvent& event)
{
    wxFileDialog* open_dialog = new wxFileDialog(
        this, _("Choose a file to open"), wxEmptyString, wxEmptyString, 
        _("CHR files (*.chr, *.bin)|*.chr;*.bin|Image Files (*.png)|*.png"),
        wxFD_OPEN, wxDefaultPosition);
    auto guard = make_scope_guard([&]{ open_dialog->Destroy(); });

    // Creates a "open file" dialog with the file types
    if(open_dialog->ShowModal() == wxID_OK) // if the user click "Open" instead of "Cancel"
    {
        chr_path = open_dialog->GetPath();
        reset_watcher();
        load_chr();
        Refresh();
    }
}

void MyFrame::on_open_collision(wxCommandEvent& event)
{
    wxFileDialog* open_dialog = new wxFileDialog(
        this, _("Choose a file to open"), wxEmptyString, wxEmptyString, 
        _("Image Files (*.bmp,*.gif,*.png)|*bmp;*.gif;*.png"),
        wxFD_OPEN, wxDefaultPosition);
    auto guard = make_scope_guard([&]{ open_dialog->Destroy(); });

    if(open_dialog->ShowModal() == wxID_OK) // if the user click "Open" instead of "Cancel"
    {
        collisions_path = open_dialog->GetPath();
        reset_watcher();
        load_collisions();
        Refresh();
    }
}

/* TODO
void MyFrame::on_new(wxCommandEvent& event)
{
    int const answer = wxMessageBox("Create a new project?\nUnsaved progress will be lost.", "Confirm", wxOK | wxCANCEL, this);

    if(answer != wxOK)
        return;

    project_path = "";
    model = {};
    model.refresh_chr();
    refresh_title();

    palette_editor->reload_model();
    metatile_editor->reload_model();
    level_editor->reload_model();

    Refresh();
}
*/

void MyFrame::on_new_window(wxCommandEvent& event)
{
    wxFrame* frame = new MyFrame();
    frame->Show();
    frame->SendSizeEvent();
}

void MyFrame::on_open(wxCommandEvent& event)
{
    using namespace std::filesystem;

    wxFileDialog* open_dialog = new wxFileDialog(
        this, _("Choose a file to open"), wxEmptyString, wxEmptyString, 
        _("MapFab Files (*.mapfab)|*.mapfab"),
        wxFD_OPEN, wxDefaultPosition);
    auto guard = make_scope_guard([&]{ open_dialog->Destroy(); });

    if(open_dialog->ShowModal() == wxID_OK) // if the user click "Open" instead of "Cancel"
    {
        MyFrame* frame = this;

        if(model.modified)
            frame = new MyFrame();
        frame->project_path = open_dialog->GetPath();

        FILE* fp = std::fopen(frame->project_path.c_str(), "rb");
        auto guard = make_scope_guard([&]{ std::fclose(fp); });

        std::string chr_name;
        std::string collisions_name;

        frame->model.read_file(fp, chr_name, collisions_name);

        path project(frame->project_path.ToStdString());
        if(project.has_filename())
            project.remove_filename();

        if(!chr_name.empty())
            frame->chr_path = (project / path(chr_name)).string();
        if(!collisions_name.empty())
            frame->collisions_path = (project / path(collisions_name)).string();

        frame->levels_panel->load_pages();
        frame->reset_watcher();
        frame->load_chr();
        frame->load_collisions();
        frame->Update();

        if(frame == this)
            frame->Refresh();
        else
        {
            frame->Show();
            frame->SendSizeEvent();
        }
    }
}

void MyFrame::on_save(wxCommandEvent& event)
{
    if(project_path.IsEmpty())
        on_save_as(event);
    else
        do_save();
}

void MyFrame::on_save_as(wxCommandEvent& event)
{
    wxFileDialog save_dialog(
        this, _("Save file as"), wxEmptyString, _("unnamed"), 
        _("MapFab Files (*.mapfab)|*.mapfab"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT, wxDefaultPosition);

    if(save_dialog.ShowModal() != wxID_CANCEL) // if the user click "Save" instead of "Cancel"
    {
        project_path = save_dialog.GetPath();
        do_save();
    }

}
void MyFrame::do_save()
{
    using namespace std::filesystem;

    if(project_path.IsEmpty())
        throw std::runtime_error("Invalid file");

    path project(project_path.ToStdString());
    if(project.has_filename())
        project.remove_filename();

    std::string relative_chr;
    if(!chr_path.IsEmpty())
    {
        path chr(chr_path.ToStdString());
        relative_chr = relative(chr, project).string();
    }

    std::string relative_collisions;
    if(!collisions_path.IsEmpty())
    {
        path collisions(collisions_path.ToStdString());
        relative_collisions = relative(collisions, project).string();
    }

    FILE* fp = std::fopen(project_path.c_str(), "wb");
    auto guard = make_scope_guard([&]{ std::fclose(fp); });

    model.write_file(fp, relative_chr, relative_collisions);
    model.modified_since_save = false;
    Update();
}

void MyFrame::refresh_title()
{
    using namespace std::filesystem;

    if(project_path.IsEmpty())
        SetTitle("MapFab");
    else
    {
        path path = project_path.ToStdString();
        wxString title;
        if(model.modified_since_save)
            title << "*";
        title << path.stem().string() + " - MapFab";
        title << " - MapFab";
        SetTitle(title);
    }
}
