#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/dialog.h>
#include <wx/fswatcher.h>
#include <wx/bookctrl.h>
#include <wx/mstream.h>
#include <wx/clipbrd.h>

#include <filesystem>
#include <cstring>

#include "2d/geometry.hpp"

#include "stamp.png.inc"
#include "dropper.png.inc"
#include "select.png.inc"

#include "model.hpp"
#include "convert.hpp"
#include "metatiles.hpp"
#include "palette.hpp"
#include "level.hpp"
#include "class.hpp"
#include "id.hpp"
#include "tool.hpp"
#include "chr.hpp"

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

class app_t: public wxApp
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

IMPLEMENT_APP(app_t)

class frame_t : public wxFrame
{
public:
    frame_t();
 
private:
    void on_exit(wxCommandEvent& event);
    void on_about(wxCommandEvent& event);
    void on_help(wxCommandEvent& event);

    void on_new_window(wxCommandEvent& event);
    void on_open(wxCommandEvent& event);
    void on_save(wxCommandEvent& event);
    void on_save_as(wxCommandEvent& event);
    void do_save();
    void refresh_title();
    void on_tab_change(wxNotebookEvent& event);
    void refresh_tab(int tab);
    void refresh_tab();

    void on_close(wxCloseEvent& event);

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

        bool editing = false;

        if(editor_t* editor = get_editor())
        {
            editing = true;
            // 'can_paste' disabled due to GTK bug:
            /*
            tile_copy_t c;
            bool const can_paste = get_paste(c) && c.format == editor->layer().format();
            */
            bool const can_paste = true;
            paste->Enable(can_paste);

            bool const can_fill = model.tool == TOOL_SELECT;
            cut->Enable(can_fill);
            copy->Enable(can_fill);
            fill->Enable(can_fill);
            fill_paste->Enable(can_fill && can_paste);
            fill_attribute->Enable(can_fill && notebook->GetSelection() == TAB_METATILES);
        }

        for(auto* item : zoom)
            item->Enable(editing);

        switch(notebook->GetSelection())
        {
        default:
            manage->Enable(false);
            break;
        case TAB_PALETTE:
        case TAB_METATILES:
        case TAB_LEVELS:
            manage->Enable(true);
            break;
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
            refresh_tab();
    }

    void reset_watcher()
    {
        if(!watcher)
        {
            watcher.reset(new wxFileSystemWatcher());
            watcher->SetOwner(this);
        }

        watcher->RemoveAll();
        watcher->Add(wxString(model.collision_path.string()));

        for(auto const& chr : model.chr_files)
            watcher->Add(wxString(chr.path.string()));
    }

    template<tool_t T>
    void on_tool(wxCommandEvent& event)
    {
        model.tool = T;
        Refresh();
    }

    template<unsigned Amount>
    void on_zoom(wxCommandEvent& event)
    {
        if(editor_t* editor = get_editor())
            editor->canvas_box().set_zoom(1 << Amount, {});
    }

    void on_manage(wxCommandEvent& event)
    {
        if(auto* tabs = get_tab_panel())
            tabs->on_manage();
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
        if(auto* m = metatile_panel->object())
        {
            if(auto* page = metatile_panel->page())
            {
                page->history.push(m->chr_layer.fill_attribute());
                Refresh();
            }
        }
    }

    base_tab_panel_t* get_tab_panel()
    {
        switch(notebook->GetSelection())
        {
        default: return nullptr;
        case TAB_METATILES: return metatile_panel;
        case TAB_LEVELS: return levels_panel;
        case TAB_CLASSES: return class_panel;
        }
    }

    editor_t* get_editor()
    {
        switch(notebook->GetSelection())
        {
        default: return nullptr;
        case TAB_PALETTE: return palette_editor;
        case TAB_METATILES: return metatile_panel->page();
        case TAB_LEVELS: return levels_panel->page();
        }
    }

    wxNotebook* notebook;

    chr_editor_t* chr_editor;
    palette_editor_t* palette_editor;
    metatile_panel_t* metatile_panel;
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
    std::array<wxMenuItem*, 5> zoom;
    wxMenuItem* manage;

    std::unique_ptr<wxFileSystemWatcher> watcher;
};

bool app_t::OnInit()
{
    wxInitAllImageHandlers();
    wxFrame* frame = new frame_t();
    frame->Show();
    frame->SendSizeEvent();
    return true;
} 
 
frame_t::frame_t()
: wxFrame(nullptr, wxID_ANY, "MapFab", wxDefaultPosition, wxSize(800, 600))
{
    wxMenu* menu_file = new wxMenu;
    menu_file->Append(wxID_NEW, "&New Project Window\tCTRL+N");
    menu_file->Append(wxID_OPEN, "&Open Project\tCTRL+O");
    menu_file->Append(wxID_SAVE, "&Save Project\tCTRL+S");
    menu_file->Append(wxID_SAVEAS, "Save Project &As\tSHIFT+CTRL+S");
    menu_file->AppendSeparator();
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
    manage = menu_view->Append(ID_MANAGE_TABS, "&Manage Tabs\tCTRL+T");
    menu_view->AppendSeparator();
    zoom[0] = menu_view->Append(ID_ZOOM_100,  "&Zoom 1x");
    zoom[1] = menu_view->Append(ID_ZOOM_200,  "&Zoom 2x");
    zoom[2] = menu_view->Append(ID_ZOOM_400,  "&Zoom 4x");
    zoom[3] = menu_view->Append(ID_ZOOM_800,  "&Zoom 8x");
    zoom[4] = menu_view->Append(ID_ZOOM_1600,  "&Zoom 16x");

    wxMenu* menu_help = new wxMenu;
    menu_help->Append(wxID_ABOUT);
    menu_help->Append(wxID_INFO);
 
    wxMenuBar* menu_bar = new wxMenuBar;
    menu_bar->Append(menu_file, "&File");
    menu_bar->Append(menu_edit, "&Edit");
    menu_bar->Append(menu_view, "&View");
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

    chr_editor = new chr_editor_t(notebook, model);
    notebook->AddPage(chr_editor, wxT("CHR"));

    palette_editor = new palette_editor_t(notebook, model);
    notebook->AddPage(palette_editor, wxT("Palettes"));

    metatile_panel = new metatile_panel_t(notebook, model);
    notebook->AddPage(metatile_panel, wxT("Metatiles"));

    levels_panel = new levels_panel_t(notebook, model);
    notebook->AddPage(levels_panel, wxT("Levels"));

    class_panel = new class_panel_t(notebook, model);
    notebook->AddPage(class_panel, wxT("Object Classes"));

    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

    sizer->Add(tool_bar, wxSizerFlags().Expand());
    sizer->Add(notebook, wxSizerFlags().Expand().Proportion(1));
    SetSizer(sizer);

    Bind(wxEVT_MENU, &frame_t::on_about, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &frame_t::on_help, this, wxID_INFO);
    Bind(wxEVT_MENU, &frame_t::on_exit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &frame_t::on_undo<UNDO>, this, wxID_UNDO);
    Bind(wxEVT_MENU, &frame_t::on_undo<REDO>, this, wxID_REDO);
    Bind(wxEVT_MENU, &frame_t::on_new_window, this, wxID_NEW);
    Bind(wxEVT_MENU, &frame_t::on_open, this, wxID_OPEN);
    Bind(wxEVT_MENU, &frame_t::on_save, this, wxID_SAVE);
    Bind(wxEVT_MENU, &frame_t::on_save_as, this, wxID_SAVEAS);
    Bind(wxEVT_MENU, &frame_t::on_copy<true>, this, wxID_CUT);
    Bind(wxEVT_MENU, &frame_t::on_copy<false>, this, wxID_COPY);
    Bind(wxEVT_MENU, &frame_t::on_paste, this, wxID_PASTE);
    Bind(wxEVT_MENU, &frame_t::on_fill, this, ID_FILL);
    Bind(wxEVT_MENU, &frame_t::on_fill_paste, this, ID_FILL_PASTE);
    Bind(wxEVT_MENU, &frame_t::on_fill_attribute, this, ID_FILL_ATTRIBUTE);
    Bind(wxEVT_MENU, &frame_t::on_zoom<0>, this, ID_ZOOM_100);
    Bind(wxEVT_MENU, &frame_t::on_zoom<1>, this, ID_ZOOM_200);
    Bind(wxEVT_MENU, &frame_t::on_zoom<2>, this, ID_ZOOM_400);
    Bind(wxEVT_MENU, &frame_t::on_zoom<3>, this, ID_ZOOM_800);
    Bind(wxEVT_MENU, &frame_t::on_zoom<4>, this, ID_ZOOM_1600);
    Bind(wxEVT_MENU, &frame_t::on_manage, this, ID_MANAGE_TABS);

    Bind(wxEVT_TOOL, &frame_t::on_tool<TOOL_STAMP>, this, ID_TOOL_STAMP);
    Bind(wxEVT_TOOL, &frame_t::on_tool<TOOL_DROPPER>, this, ID_TOOL_DROPPER);
    Bind(wxEVT_TOOL, &frame_t::on_tool<TOOL_SELECT>, this, ID_TOOL_SELECT);

    Bind(wxEVT_CLOSE_WINDOW, &frame_t::on_close, this);

    notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGING, &frame_t::on_tab_change, this);


    Bind(wxEVT_UPDATE_UI, &frame_t::update_ui, this);

    //model.refresh_chr(); TODO

    Update();
}
 
void frame_t::on_exit(wxCommandEvent& event)
{
    Close(false);
}

void frame_t::on_close(wxCloseEvent& event)
{
    if(event.CanVeto() && model.modified_since_save)
    {
        wxMessageDialog dialog(this, "Close without saving?", "Warning", wxOK | wxCANCEL | wxICON_WARNING);
        if(dialog.ShowModal() != wxID_OK)
        {
            event.Veto();
            return;
        }
    }

    Destroy();
}
 
void frame_t::on_about(wxCommandEvent& event)
{
    wxString string;
    string << "MapFab version " << VERSION << ".\n";
    string << "Copyright (C) 2023, Patrick Bene.\n";
    string << "This is free software. There is no warranty.";
    wxMessageBox(string, "About MapFab", wxOK | wxICON_INFORMATION);
}

void frame_t::on_help(wxCommandEvent& event)
{
    wxLaunchDefaultBrowser("https://pubby.games/mapfab/doc.html");
}

void frame_t::on_new_window(wxCommandEvent& event)
{
    wxFrame* frame = new frame_t();
    frame->Show();
    frame->SendSizeEvent();
}

void frame_t::on_open(wxCommandEvent& event)
{
    using namespace std::filesystem;

    wxFileDialog* open_dialog = new wxFileDialog(
        this, _("Choose a file to open"), wxEmptyString, wxEmptyString, 
        _("MapFab Files (*.mapfab)|*.mapfab"),
        wxFD_OPEN, wxDefaultPosition);
    auto guard = make_scope_guard([&]{ open_dialog->Destroy(); });

    if(open_dialog->ShowModal() == wxID_OK) // if the user click "Open" instead of "Cancel"
    {
        frame_t* frame = this;

        if(model.modified)
            frame = new frame_t();
        frame->model.project_path = open_dialog->GetPath().ToStdString();

        FILE* fp = std::fopen(frame->model.project_path.string().c_str(), "rb");
        auto guard = make_scope_guard([&]{ std::fclose(fp); });

        std::string chr_name;
        std::string collisions_name;

        frame->model.read_file(fp, frame->model.project_path);

        path project(frame->model.project_path);
        if(project.has_filename())
            project.remove_filename();

        frame->chr_editor->load();
        frame->metatile_panel->load_pages();
        frame->levels_panel->load_pages();
        frame->reset_watcher();
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

void frame_t::on_save(wxCommandEvent& event)
{
    if(model.project_path.empty())
        on_save_as(event);
    else
        do_save();
}

void frame_t::on_save_as(wxCommandEvent& event)
{
    wxFileDialog save_dialog(
        this, _("Save file as"), wxEmptyString, _("unnamed"), 
        _("MapFab Files (*.mapfab)|*.mapfab"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT, wxDefaultPosition);

    if(save_dialog.ShowModal() != wxID_CANCEL) // if the user click "Save" instead of "Cancel"
    {
        model.project_path = save_dialog.GetPath().ToStdString();
        do_save();
    }

}
void frame_t::do_save()
{
    using namespace std::filesystem;

    if(model.project_path.empty())
        throw std::runtime_error("Invalid file");

    path project(model.project_path);
    if(project.has_filename())
        project.remove_filename();

    FILE* fp = std::fopen(model.project_path.string().c_str(), "wb");
    auto guard = make_scope_guard([&]{ std::fclose(fp); });

    model.write_file(fp, model.project_path);
    model.modified_since_save = false;
    Update();
}

void frame_t::refresh_title()
{
    using namespace std::filesystem;

    if(model.project_path.empty())
        SetTitle("MapFab");
    else
    {
        path path = model.project_path;
        wxString title;
        if(model.modified_since_save)
            title << "*";
        title << path.stem().string() + " - MapFab";
        title << " - MapFab";
        SetTitle(title);
    }
}

void frame_t::on_tab_change(wxNotebookEvent& event)
{
    refresh_tab(event.GetSelection());
}

void frame_t::refresh_tab(int tab)
{
    switch(tab)
    {
    default: break;
    case TAB_METATILES: return metatile_panel->page_changed();
    case TAB_LEVELS: return levels_panel->page_changed();
    }
}

void frame_t::refresh_tab()
{
    refresh_tab(notebook->GetSelection());
}
