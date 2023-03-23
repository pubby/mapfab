#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/dialog.h>
#include <wx/fswatcher.h>
#include <wx/bookctrl.h>
#include <wx/mstream.h>

#include "2d/geometry.hpp"

#include "stamp.png.inc"
#include "paint.png.inc"
#include "dropper.png.inc"
#include "select.png.inc"

#include "controller.hpp"
#include "convert.hpp"
#include "metatiles.hpp"
#include "palette.hpp"
#include "level.hpp"

using namespace i2d;

enum
{
    ID_NEW = 1,
    ID_OPEN_CHR,
    ID_OPEN_COLLISION,
    ID_ATTR0,
    ID_ATTR1,
    ID_ATTR2,
    ID_ATTR3,
    ID_COLLISION,
    ID_VIEW_MT,
    ID_VIEW_LEVELS,
    ID_VIEW_PALETTES,
    ID_VIEW_SPECIFIC_LEVEL,
    ID_VIEW_SPECIFIC_PALETTE,
    ID_VIEW_TOGGLE,
    ID_ZOOM_100,
    ID_ZOOM_200,
    ID_ZOOM_400,
    ID_IMPORT_MT,
    ID_IMPORT_LEVEL,
    ID_EXPORT_MT,
    ID_EXPORT_LEVEL,
    ID_NEW_LEVEL,
};

class MyApp: public wxApp
{
    bool OnInit();
    
    wxFrame* frame;

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

    template<undo_type_t U>
    void on_undo(wxCommandEvent& event)
    {
        controller.undo<U>();
        Refresh();
    }
 
    template<unsigned A>
    void on_active(wxCommandEvent& event)
    {
        controller.active = A;
        Refresh();
    }

    void refresh_undo_menus(wxUpdateUIEvent& event)
    {
        for(unsigned i = 0; i < 2; ++i)
            if(undo_item[i])
                undo_item[i]->Enable(!controller.history[i].empty());
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
            watcher = new wxFileSystemWatcher();
            watcher->SetOwner(this);
        }

        watcher->RemoveAll();
        watcher->Add(chr_path);
        watcher->Add(collisions_path);
    }

    void load_chr()
    {
        std::vector<std::uint8_t> data = read_binary_file(chr_path.c_str());
        controller.chr.fill(0);
        std::copy_n(data.begin(), std::min(data.size(), controller.chr.size()), controller.chr.begin());
        controller.refresh_chr();
        Refresh();
    }

    void load_collisions()
    {
        controller.collision_bitmaps = load_collision_file(collisions_path);
        controller.collision_palette.resize({ std::min<int>(controller.collision_bitmaps.size(), 8), controller.collision_bitmaps.size() / 8 });
        Refresh();
    }

    palette_editor_t* palette_editor;
    metatile_editor_t* metatile_editor;
    level_editor_t* level_editor;

    controller_t controller;

    std::array<wxMenuItem*, 2> undo_item;

    wxString chr_path;
    wxString collisions_path;
    wxFileSystemWatcher* watcher = nullptr;
};

bool MyApp::OnInit()
{
    wxInitAllImageHandlers();

    //frame = new wxFrame((wxFrame *)NULL, -1,  wxT("Hello wxDC"), wxPoint(50,50), wxSize(800,600));

    frame = new MyFrame();

    frame->Show();
    frame->SendSizeEvent();
    return true;
} 
 
MyFrame::MyFrame()
: wxFrame(nullptr, wxID_ANY, "Hello World", wxPoint(50, 50), wxSize(800, 600))
{
    wxMenu* menu_file = new wxMenu;
    menu_file->Append(wxID_NEW, "&New Project\tCTRL+N");
    menu_file->Append(wxID_OPEN, "&Open Project\tCTRL+O");
    menu_file->Append(wxID_SAVE, "&Save Project\tCTRL+S");
    menu_file->Append(wxID_SAVEAS, "Save Project &As\tSHIFT+CTRL+S");
    menu_file->AppendSeparator();
    menu_file->Append(ID_NEW_LEVEL, "New &Level\tSHIFT+CTRL+N");
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

    wxMenu* menu_view = new wxMenu;
    menu_view->Append(ID_VIEW_MT,  "View &Metatiles\tCTRL+M");
    menu_view->Append(ID_VIEW_LEVELS, "View &Levels\tCTRL+L");
    menu_view->Append(ID_VIEW_PALETTES, "View &Palettes\tCTRL+P");
    menu_view->Append(ID_VIEW_TOGGLE, "&Toggle View\tCTRL+T");
    menu_view->AppendSeparator();
    menu_view->Append(ID_VIEW_SPECIFIC_LEVEL, "View &Specific Level\tSHIFT+CTRL+L");
    menu_view->Append(ID_VIEW_SPECIFIC_PALETTE, "View &Specific Palette\tSHIFT+CTRL+P");
    menu_view->AppendSeparator();
    menu_view->Append(ID_ZOOM_100,  "&Zoom 100%");
    menu_view->Append(ID_ZOOM_200,  "&Zoom 200%");
    menu_view->Append(ID_ZOOM_400,  "&Zoom 400%");

    wxMenu* menu_attr = new wxMenu;
    menu_attr->Append(ID_ATTR0, "Attribute &0\tF1");
    menu_attr->Append(ID_ATTR1, "Attribute &1\tF2");
    menu_attr->Append(ID_ATTR2, "Attribute &2\tF3");
    menu_attr->Append(ID_ATTR3, "Attribute &3\tF4");
    menu_attr->Append(ID_COLLISION, "&Collision\tF5");
 
    wxMenu* menu_help = new wxMenu;
    menu_help->Append(wxID_ABOUT);
 
    wxMenuBar* menu_bar = new wxMenuBar;
    menu_bar->Append(menu_file, "&File");
    menu_bar->Append(menu_edit, "&Edit");
    menu_bar->Append(menu_view, "&View");
    menu_bar->Append(menu_attr, "&Attribute");
    menu_bar->Append(menu_help, "&Help");

    SetMenuBar(menu_bar);
 
    CreateStatusBar();
    SetStatusText("Welcome to wxWidgets!");

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
    tool_bar->AddRadioTool(wxID_ANY, "Stamp", MAKE_BITMAP(src_img_stamp_png));
    tool_bar->AddRadioTool(wxID_ANY, "Paint", MAKE_BITMAP(src_img_paint_png));
    tool_bar->AddRadioTool(wxID_ANY, "Dropper", MAKE_BITMAP(src_img_dropper_png));
    tool_bar->AddRadioTool(wxID_ANY, "Select", MAKE_BITMAP(src_img_select_png));

    wxNotebook* notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxSize(600, 300));

    palette_editor = new palette_editor_t(notebook, controller);
    notebook->AddPage(palette_editor, wxT("Palettes"));

    metatile_editor = new metatile_editor_t(notebook, controller);
    notebook->AddPage(metatile_editor, wxT("Metatiles"));

    level_editor = new level_editor_t(notebook, controller);
    notebook->AddPage(level_editor, wxT("Levels"));

    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

    //sizer->Add(tile_palette, wxSizerFlags().Expand());
    //sizer->Add(metatile_editor, wxSizerFlags().Expand().Proportion(1));
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
    Bind(wxEVT_MENU, &MyFrame::on_active<0>, this, ID_ATTR0);
    Bind(wxEVT_MENU, &MyFrame::on_active<1>, this, ID_ATTR1);
    Bind(wxEVT_MENU, &MyFrame::on_active<2>, this, ID_ATTR2);
    Bind(wxEVT_MENU, &MyFrame::on_active<3>, this, ID_ATTR3);
    Bind(wxEVT_MENU, &MyFrame::on_active<ACTIVE_COLLISION>, this, ID_COLLISION);
    Bind(wxEVT_UPDATE_UI, &MyFrame::refresh_undo_menus, this, wxID_UNDO);
    Bind(wxEVT_UPDATE_UI, &MyFrame::refresh_undo_menus, this, wxID_REDO);
    Bind(wxEVT_FSWATCHER, &MyFrame::on_watcher, this);

    controller.refresh_chr();
}
 
void MyFrame::on_exit(wxCommandEvent& event)
{
    Close(true);
}
 
void MyFrame::on_about(wxCommandEvent& event)
{
    wxMessageBox("This is a wxWidgets Hello World example",
                 "About Hello World", wxOK | wxICON_INFORMATION);
}

void MyFrame::on_open_chr(wxCommandEvent& event)
{
    wxFileDialog* OpenDialog = new wxFileDialog(
        this, _("Choose a file to open"), wxEmptyString, wxEmptyString, 
        _("CHR files (*.chr, *.bin)|*.chr;*.bin|Image Files (*.png)|*.png"),
        wxFD_OPEN, wxDefaultPosition);

    // Creates a "open file" dialog with the file types
    if(OpenDialog->ShowModal() == wxID_OK) // if the user click "Open" instead of "Cancel"
    {
        chr_path = OpenDialog->GetPath();
        reset_watcher();
        load_chr();
        Refresh();
    }

    // Clean up after ourselves
    OpenDialog->Destroy();
}

void MyFrame::on_open_collision(wxCommandEvent& event)
{
    wxFileDialog* OpenDialog = new wxFileDialog(
        this, _("Choose a file to open"), wxEmptyString, wxEmptyString, 
        _("Image Files (*.bmp,*.gif,*.png)|*bmp;*.gif;*.png"),
        wxFD_OPEN, wxDefaultPosition);

    // Creates a "open file" dialog with the file types
    if(OpenDialog->ShowModal() == wxID_OK) // if the user click "Open" instead of "Cancel"
    {
        collisions_path = OpenDialog->GetPath();
        reset_watcher();
        load_collisions();
        Refresh();
    }

    // Clean up after ourselves
    OpenDialog->Destroy();
}

