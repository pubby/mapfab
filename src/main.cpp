#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/dialog.h>
#include <wx/fswatcher.h>
#include <wx/bookctrl.h>
#include <wx/mstream.h>
#include <wx/clipbrd.h>

#include <filesystem>
#include <cstring>
#include <map>

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

class level_grid_dialog_t : public wxDialog
{
public:
    level_grid_dialog_t(wxWindow* parent, model_t& model)
    : wxDialog(parent, wxID_ANY, "Level Grid")
    , model(model)
    {
        wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* label = new wxStaticText(this, wxID_ANY, "Values of 0 use the default behavior.");
        main_sizer->Add(label, 0, wxALL, 2);
        main_sizer->AddSpacer(8);

        wxPanel* xy_panel = new wxPanel(this);
        {
            wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

            wxStaticText* x_label = new wxStaticText(xy_panel, wxID_ANY, "X:");
            x_ctrl= new wxSpinCtrl(xy_panel);
            x_ctrl->SetRange(0, 256*16);
            x_ctrl->SetValue(model.level_grid_x);
            sizer->Add(x_label, 0, wxALL | wxCENTER, 2);
            sizer->Add(x_ctrl, 0, wxALL, 2);
            sizer->AddSpacer(16);

            wxStaticText* y_label = new wxStaticText(xy_panel, wxID_ANY, "Y:");
            y_ctrl= new wxSpinCtrl(xy_panel);
            y_ctrl->SetRange(0, 256*16);
            y_ctrl->SetValue(model.level_grid_y);
            sizer->Add(y_label, 0, wxALL | wxCENTER, 2);
            sizer->Add(y_ctrl, 0, wxALL, 2);

            xy_panel->SetSizer(sizer);
        }
        main_sizer->Add(xy_panel, 0, wxALL, 2);
        main_sizer->AddSpacer(8);

        wxButton* ok_button = new wxButton(this, wxID_OK, "Ok");
        main_sizer->Add(ok_button, 0, wxALL | wxCENTER, 2);

        x_ctrl->Bind(wxEVT_SPINCTRL, &level_grid_dialog_t::on_change_x, this);
        y_ctrl->Bind(wxEVT_SPINCTRL, &level_grid_dialog_t::on_change_y, this);

        SetSizerAndFit(main_sizer);
    }

private:
    model_t& model;

    wxSpinCtrl* x_ctrl;
    wxSpinCtrl* y_ctrl;

    void on_change_x(wxSpinEvent& event)
    {
        model.level_grid_x = event.GetPosition(); 
    }

    void on_change_y(wxSpinEvent& event)
    {
        model.level_grid_y = event.GetPosition(); 
    }
};

class usage_dialog_t : public wxDialog
{
public:
    usage_dialog_t(wxWindow* parent, model_t& model)
    : wxDialog(parent, wxID_ANY, "Select by Usage")
    , model(model)
    {
        wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* label = new wxStaticText(this, wxID_ANY, "Rarely used tiles will be selected.");
        main_sizer->Add(label, 0, wxALL, 2);
        main_sizer->AddSpacer(8);

        wxPanel* panel = new wxPanel(this);
        {
            wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

            wxStaticText* usage_label = new wxStaticText(panel, wxID_ANY, "Maximum Usage:");
            usage_ctrl= new wxSpinCtrl(panel);
            usage_ctrl->SetRange(0, 256*256);
            usage_ctrl->SetValue(usage);
            sizer->Add(usage_label, 0, wxALL | wxCENTER, 2);
            sizer->Add(usage_ctrl, 0, wxALL | wxCENTER, 2);
            sizer->AddSpacer(16);

            mtt_ctrl = new wxCheckBox(panel, wxID_ANY, "32x32 Mode");
            mtt_ctrl->SetValue(mtt);
            sizer->Add(mtt_ctrl, 0, wxALL | wxCENTER, 2);

            panel->SetSizer(sizer);
        }
        main_sizer->Add(panel, 0, wxALL, 2);
        main_sizer->AddSpacer(8);

        wxPanel* button_panel = new wxPanel(this);
        {
            wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

            wxButton* ok_button = new wxButton(button_panel, wxID_OK, "Ok");
            sizer->Add(ok_button, 0, wxALL | wxALIGN_CENTER, 2);
            sizer->AddSpacer(16);
            wxButton* cancel_button = new wxButton(button_panel, wxID_OK, "Cancel");
            sizer->Add(cancel_button, 0, wxALL | wxALIGN_CENTER, 2);

            button_panel->SetSizer(sizer);
        }
        main_sizer->Add(button_panel, 0, wxALL | wxALIGN_CENTER, 2);

        usage_ctrl->Bind(wxEVT_SPINCTRL, &usage_dialog_t::on_change_usage, this);
        mtt_ctrl->Bind(wxEVT_CHECKBOX, &usage_dialog_t::on_change_mtt, this);

        SetSizerAndFit(main_sizer);
    }

public:
    int usage = 0;
    bool mtt = false;

private:
    model_t& model;

    wxSpinCtrl* usage_ctrl;
    wxCheckBox* mtt_ctrl;

    void on_change_usage(wxSpinEvent& event)
    {
        usage = event.GetPosition(); 
    }

    void on_change_mtt(wxCommandEvent& event)
    {
        mtt = event.GetInt();
    }
};
 
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
            cut->Enable(can_fill || editor->enable_copy());
            copy->Enable(can_fill || editor->enable_copy());
            fill->Enable(can_fill);
            fill_paste->Enable(can_fill && can_paste);
            fill_attribute->Enable(can_fill && notebook->GetSelection() == TAB_METATILES);
            select_all->Enable(true);
            select_none->Enable(true);
            select_invert->Enable(true);
            select_usage->Enable(true);
        }
        else
        {
            cut->Enable(false);
            copy->Enable(false);
            fill->Enable(false);
            fill_paste->Enable(false);
            fill_attribute->Enable(false);
            select_all->Enable(false);
            select_none->Enable(false);
            select_invert->Enable(false);
            select_usage->Enable(false);
        }

        for(auto* item : zoom)
            item->Enable(editing);

        show_collisions->Enable(notebook->GetSelection() == TAB_LEVELS || notebook->GetSelection() == TAB_METATILES);
        level_grid->Enable(notebook->GetSelection() == TAB_LEVELS);

        switch(notebook->GetSelection())
        {
        default:
            manage->Enable(false);
            break;
        case TAB_LEVELS:
        case TAB_METATILES:
        case TAB_CLASSES:
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
        //wxFileName path = event.GetPath();

        if(event.GetChangeType() == wxFSW_EVENT_MODIFY)
        {
            try
            {
                if(std::filesystem::exists(model.collision_path))
                {
                    auto bm = load_collision_file(model.collision_path.string());
                    model.collision_bitmaps = std::move(bm.first);
                    model.collision_wx_bitmaps = std::move(bm.second);
                }
            }
            catch(...) {}

            for(auto& chr : model.chr_files)
            {
                try
                {
                    chr.load();
                }
                catch(...) {}
            }

            refresh_tab();
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

        auto const watch = [&](std::filesystem::path path)
        {
#ifdef __WXMSW__
            path.remove_filename();
#endif
            watcher->Add(wxString(path.string()));
        };

        watch(model.collision_path);
        for(auto const& chr : model.chr_files)
            watch(chr.path.string());
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

    void on_show_collisions(wxCommandEvent& event)
    {
        model.show_collisions ^= true;
        metatile_panel->Refresh();
        if(auto* page = levels_panel->page())
            page->load_metatiles();
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
            model.modify();
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
                    model.modify();
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
                model.modify();
                page->history.push(m->chr_layer.fill_attribute());
                Refresh();
            }
        }
    }

    void enable_select()
    {
        model.tool = TOOL_SELECT;
        for(auto* tool : tools)
            tool->Toggle(false);
        tool_bar->ToggleTool(ID_TOOL_SELECT, true);
        tool_bar->Refresh();
    }

    template<bool Select>
    void on_select_all(wxCommandEvent& event)
    {
        enable_select();
        if(editor_t* editor = get_editor())
            editor->select_all(Select);
    }

    void on_select_invert(wxCommandEvent& event)
    {
        enable_select();
        if(editor_t* editor = get_editor())
            editor->select_invert();
    }

    void on_select_usage(wxCommandEvent& event)
    {
        usage_dialog_t dialog(this, model);
        if(dialog.ShowModal() == wxID_OK)
        {
            enable_select();
            select_by_usage(dialog.usage, dialog.mtt);
        }
    }

    void select_by_usage(int usage, bool mtt)
    {
        // Palette
        std::array<int, 64> color_map = {};
        std::array<std::array<int, 4>, 256> palette_map = {};
        model.palette.color_layer.picker_selector.select_all(false);
        model.palette.color_layer.canvas_selector.select_all(false);

        for(unsigned y = 0; y < model.palette.num; ++y)
        for(unsigned x = 0; x < 12; ++x)
            color_map[model.palette.color_layer.tiles[coord_t{ x, y }]] += 1;

        for(unsigned i = 0; i < 64; ++i)
            if(color_map[i] <= usage)
                model.palette.color_layer.picker_selector.select(coord_t{ i % 4, i / 4 });

        for(auto const& mt : model.metatiles)
        {
            auto& map = palette_map[mt->palette];
            for(std::uint8_t a : mt->chr_layer.attributes)
                map[a] += 1;
        }

        for(unsigned y = 0; y < model.palette.num; ++y)
        for(unsigned x = 0; x < 4; ++x)
        {
            if(palette_map[y][x] <= usage)
                for(unsigned i = 0; i < 3; ++i)
                    model.palette.color_layer.canvas_selector.select(coord_t{ x*3 + i , y });
        }

        // CHR
        std::map<std::string, std::array<int, 256>> chr_map;
        for(auto const& mt : model.metatiles)
        {
            auto& map = chr_map[mt->chr_name];
            for(std::uint8_t t : mt->chr_layer.tiles)
                map[t] += 1;
        }

        for(auto const& mt : model.metatiles)
        {
            auto& map = chr_map[mt->chr_name];
            mt->chr_layer.picker_selector.select_all(false);
            for(unsigned i = 0; i < 256; ++i)
                if(map[i] <= usage)
                    mt->chr_layer.picker_selector.select(coord_t{ i % 16, i / 16 });
        }

        std::map<std::string, std::array<int, 256>> mt_map;
        for(auto const& level : model.levels)
        {
            auto& map = mt_map[level->metatiles_name];
            for(std::uint8_t t : level->metatile_layer.tiles)
                map[t] += 1;
        }

        for(auto const& level : model.levels)
        {
            auto& map = mt_map[level->metatiles_name];
            level->metatile_layer.canvas_selector.select_all(false);

            if(!mtt)
            {
                for(coord_t c : dimen_range(level->metatile_layer.tiles.dimen()))
                {
                    std::uint8_t const t = level->metatile_layer.tiles[c];
                    if(map[t] <= usage)
                        level->metatile_layer.canvas_selector.select(c);
                }
            }

            level->metatile_layer.picker_selector.select_all(false);
            for(unsigned i = 0; i < 256; ++i)
                if(map[i] <= usage)
                    level->metatile_layer.picker_selector.select(coord_t{ (i % 16), (i / 16) });
        }

        for(auto const& mt : model.metatiles)
        {
            auto& map = mt_map[mt->name];
            mt->chr_layer.canvas_selector.select_all(false);
            for(unsigned i = 0; i < 256; ++i)
            {
                if(map[i] <= usage)
                {
                    for(unsigned x = 0; x < 2; ++x)
                    for(unsigned y = 0; y < 2; ++y)
                        mt->chr_layer.canvas_selector.select(coord_t{ (i % 16)*2 + x, (i / 16)*2+y });
                }
            }
        }

        // Collisions
        std::array<int, 64> collision_map = {};

        for(auto const& mt : model.metatiles)
            for(std::uint8_t t : mt->collision_layer.tiles)
                collision_map[t] += 1;

        for(auto const& mt : model.metatiles)
        {
            mt->collision_layer.picker_selector.select_all(false);
            for(unsigned i = 0; i < 64; ++i)
                if(collision_map[i] <= usage)
                    mt->collision_layer.picker_selector.select(coord_t{ i % 8, i / 8 });
        }

        // Metatiles
        if(mtt)
        {
            using mtt_t = std::array<std::uint8_t, 4>;
            std::map<std::string, std::map<mtt_t, int>> mtt_map;

            auto const get_mtt = [&](auto const& level, unsigned x, unsigned y) -> mtt_t
            {
                std::uint8_t nw = 0;
                std::uint8_t ne = 0;
                std::uint8_t sw = 0;
                std::uint8_t se = 0;

                nw = level->metatile_layer.tiles[{ x+0, y+0 }];
                if(x+1 < level->metatile_layer.tiles.dimen().w)
                {
                    ne = level->metatile_layer.tiles[{ x+1, y+0 }];
                    if(y+1 < level->metatile_layer.tiles.dimen().h)
                        se = level->metatile_layer.tiles[{ x+1, y+1 }];
                }
                if(y+1 < level->metatile_layer.tiles.dimen().h)
                    sw = level->metatile_layer.tiles[{ x+0, y+1 }];

                return {{ nw, ne, sw, se }};
            };

            for(auto const& level : model.levels)
            {
                auto& map = mtt_map[level->metatiles_name];
                for(unsigned y = 0; y < level->metatile_layer.tiles.dimen().h; y += 2)
                for(unsigned x = 0; x < level->metatile_layer.tiles.dimen().w; x += 2)
                    map[get_mtt(level, x, y)] += 1;
            }

            for(auto const& level : model.levels)
            {
                auto& map = mtt_map[level->metatiles_name];
                for(unsigned y = 0; y < level->metatile_layer.tiles.dimen().h; y += 2)
                for(unsigned x = 0; x < level->metatile_layer.tiles.dimen().w; x += 2)
                {
                    if(map[get_mtt(level, x, y)] <= usage)
                    {
                        for(unsigned xo = 0; xo < 2; ++xo)
                        for(unsigned yo = 0; yo < 2; ++yo)
                            level->metatile_layer.canvas_selector.select(coord_t{ x+xo, y+yo });
                    }
                }
            }
        }

        if(editor_t* editor = get_editor())
            editor->Refresh();
    }

    void on_level_grid(wxCommandEvent& event)
    {
        level_grid_dialog_t dialog(this, model);
        dialog.ShowModal();
        dialog.Destroy();
        levels_panel->Refresh();
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
    wxMenuItem* show_collisions;
    wxMenuItem* level_grid;
    wxMenuItem* select_all;
    wxMenuItem* select_none;
    wxMenuItem* select_invert;
    wxMenuItem* select_usage;
    wxToolBar* tool_bar;
    std::vector<wxToolBarToolBase*> tools;

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
    fill_attribute = menu_edit->Append(ID_FILL_ATTRIBUTE, "Fill Selection with Attribute\tCTRL+D");
    menu_edit->AppendSeparator();
    select_all = menu_edit->Append(ID_SELECT_ALL, "Select All\tCTRL+A");
    select_none = menu_edit->Append(ID_SELECT_NONE, "Select None\tCTRL+SHIFT+A");
    select_invert = menu_edit->Append(ID_SELECT_INVERT, "Invert Selection\tCTRL+I");
    select_usage = menu_edit->Append(ID_SELECT_USAGE, "Select by Usage\tCTRL+U");

    wxMenu* menu_view = new wxMenu;
    manage = menu_view->Append(ID_MANAGE_TABS, "&Manage Tabs\tCTRL+T");
    menu_view->AppendSeparator();
    show_collisions = menu_view->Append(ID_SHOW_COLLISIONS, "&Toggle Collisions\tALT+C");
    level_grid = menu_view->Append(ID_LEVEL_GRID, "&Configure Level Grid");
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
 
    model.status_bar = CreateStatusBar();

    auto const make_bitmap = [&](char const* name, unsigned char const* data, std::size_t size)
    {
        wxMemoryInputStream stream(data, size);
        wxImage img;
        if(!img.LoadFile(stream, wxBITMAP_TYPE_PNG))
            throw std::runtime_error(name);
        return wxBitmap(img);
    };
#define MAKE_BITMAP(x) make_bitmap(#x, x, x##_size)

    tool_bar = new wxToolBar(this, wxID_ANY);
    tool_bar->SetWindowStyle(wxTB_VERTICAL);
    tools.push_back(tool_bar->AddRadioTool(ID_TOOL_STAMP, "Stamp", MAKE_BITMAP(src_img_stamp_png)));
    tools.push_back(tool_bar->AddRadioTool(ID_TOOL_DROPPER, "Dropper", MAKE_BITMAP(src_img_dropper_png)));
    tools.push_back(tool_bar->AddRadioTool(ID_TOOL_SELECT, "Select", MAKE_BITMAP(src_img_select_png)));
    tool_bar->Realize();

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
    Bind(wxEVT_MENU, &frame_t::on_show_collisions, this, ID_SHOW_COLLISIONS);
    Bind(wxEVT_MENU, &frame_t::on_select_all<true>, this, ID_SELECT_ALL);
    Bind(wxEVT_MENU, &frame_t::on_select_all<false>, this, ID_SELECT_NONE);
    Bind(wxEVT_MENU, &frame_t::on_select_invert, this, ID_SELECT_INVERT);
    Bind(wxEVT_MENU, &frame_t::on_select_usage, this, ID_SELECT_USAGE);
    Bind(wxEVT_MENU, &frame_t::on_level_grid, this, ID_LEVEL_GRID);

    Bind(wxEVT_TOOL, &frame_t::on_tool<TOOL_STAMP>, this, ID_TOOL_STAMP);
    Bind(wxEVT_TOOL, &frame_t::on_tool<TOOL_DROPPER>, this, ID_TOOL_DROPPER);
    Bind(wxEVT_TOOL, &frame_t::on_tool<TOOL_SELECT>, this, ID_TOOL_SELECT);

    Bind(wxEVT_CLOSE_WINDOW, &frame_t::on_close, this);
    Bind(wxEVT_FSWATCHER, &frame_t::on_watcher, this);

    notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &frame_t::on_tab_change, this);


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
        _("MapFab Imports (*.mapfab;*.json)|*.mapfab;*.json|MapFab Files (*.mapfab)|*.mapfab|JSON Files (*.json)|*.json"),
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

        if(model.project_path.extension() == ".json")
            frame->model.read_json(fp, frame->model.project_path);
        else
            frame->model.read_file(fp, frame->model.project_path);

        path project(frame->model.project_path);
        if(project.has_filename())
            project.remove_filename();

        frame->chr_editor->load();
        frame->metatile_panel->load_pages();
        frame->levels_panel->load_pages();
        frame->class_panel->load_pages();
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
        _("MapFab Exports (*.mapfab;*.json)|*.mapfab;*.json|MapFab Files (*.mapfab)|*.mapfab|JSON Files (*.json)|*.json"),
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

    if(model.project_path.extension() == ".json")
        model.write_json(fp, model.project_path);
    else
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
    model.status_bar->SetStatusText("");
    if(event.GetOldSelection() == TAB_CHR)
        reset_watcher();
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
