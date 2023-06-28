#ifndef GRID_BOX_HPP
#define GRID_BOX_HPP

#include <bit>

#include <wx/wx.h>
#include <wx/notebook.h>

#include "2d/geometry.hpp"

#include "id.hpp"
#include "model.hpp"

using namespace i2d;

enum mouse_button_t
{
    MB_NONE,
    MB_LEFT,
    MB_RIGHT,
};

enum
{
    TAB_CHR,
    TAB_PALETTE,
    TAB_METATILES,
    TAB_LEVELS,
    TAB_CLASSES,
};

class editor_t;

class grid_box_t : public wxScrolledWindow
{
public:
    explicit grid_box_t(wxWindow* parent, bool can_zoom = true);

    void on_update(wxUpdateUIEvent&) { on_update(); }
    virtual void on_update() { resize(); }

    virtual dimen_t tile_size() const { return { 8, 8 }; }
    virtual dimen_t margin() const { return { 8, 8 }; }

    void OnDraw(wxDC& dc) override;

    virtual void grid_resize(dimen_t dimen);
    virtual void resize() = 0;

    coord_t from_screen(coord_t pixel) const { return from_screen(pixel, tile_size()); }
    coord_t from_screen(coord_t pixel, dimen_t tile_size, int user_scale = 1) const;

    coord_t to_screen(coord_t c) const { return to_screen(c, tile_size()); }
    coord_t to_screen(coord_t c, dimen_t tile_size) const;

protected:
    dimen_t grid_dimen = {};
    mouse_button_t mouse_down = MB_NONE;
    coord_t mouse_current = {};
    int scale = 2;

    virtual void draw_tiles(wxDC& dc) = 0;

    virtual void on_down(mouse_button_t mb, coord_t) {}
    virtual void on_up(mouse_button_t mb, coord_t) {}
    virtual void on_motion(coord_t) {}

    void on_down(wxMouseEvent& event, mouse_button_t mb);
    void on_up(wxMouseEvent& event, mouse_button_t mb);
    void on_motion(wxMouseEvent& event);
    void on_wheel(wxMouseEvent& event);

    void on_left_down(wxMouseEvent& event)  { on_down(event, MB_LEFT); }
    void on_left_up(wxMouseEvent& event)    { on_up(event,   MB_LEFT); }
    void on_right_down(wxMouseEvent& event) { on_down(event, MB_RIGHT); }
    void on_right_up(wxMouseEvent& event)   { on_up(event,   MB_RIGHT); }

    void set_scale(int new_scale);
};

class selector_box_t : public grid_box_t
{
public:
    explicit selector_box_t(wxWindow* parent, bool can_zoom = false) 
    : grid_box_t(parent, can_zoom) 
    {}

    void OnDraw(wxDC& dc) override;

    virtual tile_model_t& tiles() const = 0;
    auto& layer() const { return tiles().layer(); }
    virtual select_map_t& selector() const { return layer().picker_selector; }
    editor_t& editor();

    virtual void resize() override { grid_resize(selector().dimen()); }
    virtual dimen_t tile_size() const { return layer().tile_size(); }

    virtual bool enable_tile_select() const { return true; }
protected:

    coord_t mouse_start = {};

    virtual void on_down(mouse_button_t mb, coord_t at) override { mouse_start = at; }
    virtual void on_up(mouse_button_t mb, coord_t mouse_end) override;
    virtual void on_motion(coord_t at) override;

    virtual void draw_tile(wxDC& dc, unsigned tile, coord_t at) {}
    virtual void draw_tiles(wxDC& dc) override;
};

class canvas_box_t : public selector_box_t
{
public:
    canvas_box_t(wxWindow* parent, model_t& model) 
    : selector_box_t(parent, true) 
    , model(model)
    {}

    void OnDraw(wxDC& dc) override
    {
        if(model.tool == TOOL_SELECT && !pasting())
            selector_box_t::OnDraw(dc);
        else
            grid_box_t::OnDraw(dc);
    }

    virtual void resize() override { grid_resize(layer().canvas_dimen()); }

protected:
    model_t& model;

    bool pasting() const { return model.paste && model.paste->format == layer().format(); }

    virtual select_map_t& selector() const override { return layer().canvas_selector; }

    //virtual undo_t make_undo(rect_t rect) = 0; TODO
    virtual void post_update() {}

    virtual void on_down(mouse_button_t mb, coord_t at) override;
    virtual void on_up(mouse_button_t mb, coord_t mouse_end) override;
    virtual void on_motion(coord_t at) override { Refresh(); }

    virtual void draw_tile(wxDC& dc, unsigned tile, coord_t at) {}
    virtual void draw_tiles(wxDC& dc) override;

    void draw_underlays(wxDC& dc);
    void draw_overlays(wxDC& dc);
};

class editor_t : public wxPanel
{
public:
    undo_history_t history;

    explicit editor_t(wxWindow* parent);

    void on_update(wxUpdateUIEvent&) { on_update(); }
    virtual void on_update() {}
    virtual canvas_box_t& canvas_box() = 0;
    auto& layer() { return canvas_box().layer(); }

    virtual tile_copy_t copy(bool cut);
};

template<typename P>
class tab_panel_t : public wxPanel
{
public:
    using page_type = typename P::page_type;
    using object_type = typename P::object_type;

    tab_panel_t(wxWindow* parent, model_t& model)
    : wxPanel(parent)
    , model(model)
    {
        notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_BOTTOM);
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(notebook, wxSizerFlags().Expand().Proportion(1));
        SetSizer(sizer);

        notebook->Connect(wxID_ANY, wxEVT_RIGHT_UP, wxMouseEventHandler(tab_panel_t::on_right_click), nullptr, this);

        Bind(wxEVT_MENU, &tab_panel_t::on_new_page, this, ID_R_NEW_PAGE);
        Bind(wxEVT_MENU, &tab_panel_t::on_delete_page, this, ID_R_DELETE_PAGE);
        Bind(wxEVT_MENU, &tab_panel_t::on_rename_page, this, ID_R_RENAME_PAGE);
        Bind(wxEVT_MENU, &tab_panel_t::on_clone_page, this, ID_R_CLONE_PAGE);
        Bind(wxEVT_NOTEBOOK_PAGE_CHANGING, &tab_panel_t::on_page_changing, this);
    }

    page_type& page(int i) { return *static_cast<page_type*>(notebook->GetPage(i)); }

    page_type* page()
    {
        if(notebook->GetSelection() == wxNOT_FOUND)
            return nullptr;
        return &page(notebook->GetSelection());
    }

    object_type& object(int i) { return *collection().at(i).get(); }

    object_type* object()
    {
        if(notebook->GetSelection() == wxNOT_FOUND)
            return nullptr;
        return &object(notebook->GetSelection());
    }

    void load_pages()
    {
        while(notebook->GetPageCount())
            notebook->RemovePage(0);

        for(auto const& object : collection())
            notebook->AddPage(new page_type(notebook, model, object), object->name);
    }

    void new_page()
    {
        unsigned id = 0;
        auto const make_string = [&]
        {
            wxString ret;
            ret << P::name;
            ret << "_";
            ret << id;
            return ret;
        };

        for(unsigned i = 0; i < notebook->GetPageCount(); ++i)
        {
            wxString used = notebook->GetPageText(i);
            if(used == make_string())
            {
                ++id;
                i = 0;
            }
        }

        auto& object = collection().emplace_back(std::make_shared<object_type>());
        assert(collection().back());
        auto* page = new page_type(notebook, model, object);
        wxString const name = make_string();
        notebook->AddPage(page, name);
        object->name = name.ToStdString();
    }

    void prepare_page(int i)
    {
        P::on_page_changing(page(i), object(i));
    }

    void on_page_changing(wxNotebookEvent& event) { prepare_page(event.GetSelection()); }
    void page_changed() { prepare_page(notebook->GetSelection()); }

    void on_right_click(wxMouseEvent& event)
    {
        rtab_id = notebook->HitTest(wxPoint(event.GetX(), event.GetY()));

        wxMenu tab_menu;
        tab_menu.Append(ID_R_NEW_PAGE, std::string("&New ") + P::name);
        tab_menu.Append(ID_R_DELETE_PAGE, std::string("&Delete ") + P::name);
        tab_menu.Append(ID_R_RENAME_PAGE, std::string("&Rename ") + P::name);
        tab_menu.Append(ID_R_CLONE_PAGE, std::string("&Clone ") + P::name);

        tab_menu.Enable(ID_R_DELETE_PAGE, rtab_id >= 0 && notebook->GetPageCount() > 1);
        tab_menu.Enable(ID_R_RENAME_PAGE, rtab_id >= 0);
        tab_menu.Enable(ID_R_CLONE_PAGE, rtab_id >= 0);

        PopupMenu(&tab_menu);
    }
    
    void on_new_page(wxCommandEvent& event) { new_page(); }

    void on_delete_page(wxCommandEvent& event)
    {
        if(rtab_id < 0 || notebook->GetPageCount() <= 1)
            return;

        wxString text;
        text << "Delete ";
        text < collection().at(rtab_id)->name;
        text << "?\nThis cannot be undone.";

        wxMessageDialog dialog(this, text, "Warning", wxOK | wxCANCEL | wxICON_WARNING);
        if(dialog.ShowModal() == wxID_OK)
        {
            notebook->RemovePage(rtab_id);
            collection().erase(collection().begin() + rtab_id);
        }
    }

    void on_rename_page(wxCommandEvent& event)
    {
        if(rtab_id < 0)
            return;
        wxTextEntryDialog dialog(
            this, "Rename Level", "New name:", collection().at(rtab_id)->name);

        if(dialog.ShowModal() == wxID_OK)
        {
            wxString new_name = dialog.GetValue();
            if(!new_name.IsEmpty())
            {
                for(unsigned i = 0; i < notebook->GetPageCount(); ++i)
                {
                    if(i == rtab_id)
                        continue;

                    wxString used = notebook->GetPageText(i);
                    if(used == new_name)
                    {
                        wxMessageDialog dialog(this, std::string("A ") + P::name + std::string("with that name already exists."), "Error", wxOK | wxICON_ERROR);
                        dialog.ShowModal();
                        return;
                    }
                }

                collection().at(rtab_id)->name = new_name.ToStdString();
                notebook->SetPageText(rtab_id, new_name);
            }
        }
    }

    // TODO
    void on_clone_page(wxCommandEvent& event)
    {
    }

protected:
    auto& collection() { return P::collection(model); }

    model_t& model;
    int rtab_id = -1;
    wxNotebook* notebook;
};

#endif

