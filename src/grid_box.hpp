#ifndef GRID_BOX_HPP
#define GRID_BOX_HPP

#include <bit>
#include <unordered_set>
#include <type_traits>

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/rearrangectrl.h>

#include "2d/geometry.hpp"

#include "id.hpp"
#include "model.hpp"

using namespace i2d;

enum mouse_button_t
{
    MB_NONE,
    MBTN_LEFT,
    MBTN_RIGHT,
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

template<typename P>
class tab_dialog_t : public wxRearrangeDialog
{
public:
    using page_type = typename P::page_type;
    using object_type = typename P::object_type;

    template<typename... Args>
    tab_dialog_t(wxWindow *parent, model_t& model, Args&&... args)
    : wxRearrangeDialog(parent, std::forward<Args>(args)...)
    , model(model)
    {
        wxPanel* panel = new wxPanel(this);
        wxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* deselect_button = new wxButton(panel, wxID_ANY, "Deselect All");
        //wxButton* deselect_name_button = new wxButton(panel, wxID_ANY, "Deselect by Name");
        wxButton* select_name_button = new wxButton(panel, wxID_ANY, "Select by Name");
        sizer->Add(deselect_button, wxSizerFlags().Border(wxRIGHT));
        //sizer->Add(deselect_name_button, wxSizerFlags().Border(wxRIGHT));
        sizer->Add(select_name_button);
        panel->SetSizer(sizer);
        AddExtraControls(panel);

        deselect_button->Bind(wxEVT_BUTTON, &tab_dialog_t::on_deselect_all, this);
        //deselect_name_button->Bind(wxEVT_BUTTON, &tab_dialog_t::on_select_name<true>, this);
        select_name_button->Bind(wxEVT_BUTTON, &tab_dialog_t::on_select_name<false>, this);
        GetList()->Connect(wxID_ANY, wxEVT_RIGHT_UP, wxMouseEventHandler(tab_dialog_t::on_right_click), nullptr, this);

        Bind(wxEVT_MENU, &tab_dialog_t::on_new_page, this, ID_R_NEW_PAGE);
        Bind(wxEVT_MENU, &tab_dialog_t::on_delete_page, this, ID_R_DELETE_PAGE);
        Bind(wxEVT_MENU, &tab_dialog_t::on_rename_page, this, ID_R_RENAME_PAGE);
        Bind(wxEVT_MENU, &tab_dialog_t::on_clone_page, this, ID_R_CLONE_PAGE);
    }

    void on_deselect_all(wxCommandEvent& event)
    {
        for(unsigned i = 0; i < GetList()->GetCount(); ++i)
            GetList()->Check(i, false);
    }

    template<bool Deselect>
    void on_select_name(wxCommandEvent& event)
    {
        wxTextEntryDialog dlg(this, Deselect ? "Deselect all containing: ": "Select all containing:", "Select");
        if(dlg.ShowModal() == wxID_OK)
        {
            wxString string = dlg.GetValue();
            for(unsigned i = 0; i < GetList()->GetCount(); ++i)
                if(GetList()->GetString(i).Contains(string))
                    GetList()->Check(i, !Deselect);
        }
    }

    void on_right_click(wxMouseEvent& event)
    {
        rtab_id = GetList()->HitTest(wxPoint(event.GetX(), event.GetY()));
        if(rtab_id < 0)
            rtab_id = GetList()->GetCount() - 1;

        wxMenu tab_menu;
        tab_menu.Append(ID_R_NEW_PAGE, std::string("&New ") + P::name);
        tab_menu.Append(ID_R_DELETE_PAGE, std::string("&Delete ") + P::name);
        tab_menu.Append(ID_R_RENAME_PAGE, std::string("&Rename ") + P::name);
        tab_menu.Append(ID_R_CLONE_PAGE, std::string("&Clone ") + P::name);

        tab_menu.Enable(ID_R_DELETE_PAGE, rtab_id >= 0 && GetList()->GetCount() > 1);
        tab_menu.Enable(ID_R_NEW_PAGE, rtab_id >= 0);
        tab_menu.Enable(ID_R_RENAME_PAGE, rtab_id >= 0);
        tab_menu.Enable(ID_R_CLONE_PAGE, rtab_id >= 0);

        PopupMenu(&tab_menu);
    }

    void on_new_page(wxCommandEvent& event)
    {
        if(rtab_id < 0 || rtab_id >= int(GetList()->GetCount()))
            rtab_id = GetList()->GetCount() ? GetList()->GetCount() - 1 : 0;

        wxString name;
        wxTextEntryDialog dlg(this, "Name:", std::string("New ") + P::name);
        if(dlg.ShowModal() == wxID_OK)
            name = dlg.GetValue();
        else
            return;

        if(!handle_unique_name("", name.ToStdString()))
            return;

        auto& object = collection().emplace_back(std::make_shared<object_type>());
        model.modify();

        object->name = name.ToStdString();
        GetList()->InsertItems(1, &name, rtab_id + 1);
        GetList()->Select(rtab_id + 1);
        GetList()->Check(rtab_id + 1);
    }

    void on_clone_page(wxCommandEvent& event)
    {
        if(rtab_id < 0 || rtab_id >= int(GetList()->GetCount()))
            return;

        wxString name;
        wxTextEntryDialog dlg(this, "Name:", std::string("New ") + P::name);
        if(dlg.ShowModal() == wxID_OK)
            name = dlg.GetValue();
        else
            return;

        if(!handle_unique_name("", name.ToStdString()))
            return;

        auto& object = collection().emplace_back(std::make_shared<object_type>(*collection().at(rtab_id)));
        model.modify();

        object->name = name.ToStdString();
        GetList()->InsertItems(1, &name, rtab_id + 1);
        GetList()->Select(rtab_id + 1);
    }

    void on_delete_page(wxCommandEvent& event)
    {
        if(rtab_id < 0 || rtab_id >= int(GetList()->GetCount()))
            return;

        wxString text;
        text << "Delete";
        if(!collection().at(rtab_id)->name.empty())
            text << ' ' << collection().at(rtab_id)->name;
        text << "?\nThis cannot be undone.";

        wxMessageDialog dialog(this, text, "Warning", wxOK | wxCANCEL | wxICON_WARNING);
        if(dialog.ShowModal() == wxID_OK)
        {
            GetList()->Delete(rtab_id);
            collection().erase(collection().begin() + rtab_id);
            model.modify();
        }
    }

    void on_rename_page(wxCommandEvent& event)
    {
        if(rtab_id < 0 || rtab_id >= int(GetList()->GetCount()))
            return;

        wxTextEntryDialog dialog(
            this, "Rename Level", "New name:", collection().at(rtab_id)->name);

        if(dialog.ShowModal() == wxID_OK)
        {
            wxString new_name = dialog.GetValue();
            std::string old_name = collection().at(rtab_id)->name;

            if(!handle_unique_name(old_name, new_name.ToStdString()))
                return;

            collection().at(rtab_id)->name = new_name.ToStdString();

            P::rename(model, old_name, collection().at(rtab_id)->name);

            GetList()->SetString(rtab_id, new_name);
            model.modify();
        }
    }

private:
    auto& collection() { return P::collection(model); }

    bool unique_name(std::string const& name)
    {
        for(auto const& c : collection())
            if(c->name == name)
                return false;
        return true;
    }

    bool handle_unique_name(std::string const& old_name, std::string const& name)
    {
        if(!old_name.empty() && old_name == name)
            return true;

        if(!unique_name(name))
        {
            wxMessageBox( wxT("Names must be unique."), wxT("Error"), wxICON_ERROR);
            return false;
        }

        return true;
    }

    model_t& model;
    int rtab_id = -1;
};

class grid_box_t : public wxScrolledWindow
{
public:
    explicit grid_box_t(wxWindow* parent, bool can_zoom = true);

    void on_update(wxUpdateUIEvent&) { on_update(); }
    virtual void on_update() { resize(); }

    virtual dimen_t tile_size() const { return { 8, 8 }; }
    virtual dimen_t margin() const { return { 8, 8 }; }

    void OnPaint(wxPaintEvent& event);
    void OnDraw(wxDC& dc) override;

    virtual void grid_resize(dimen_t dimen);
    virtual void resize() = 0;

    coord_t from_screen(coord_t pixel) const { return from_screen(pixel, tile_size()); }
    coord_t from_screen(coord_t pixel, dimen_t tile_size, int user_scale = 1) const;

    coord_t to_screen(coord_t c) const { return to_screen(c, tile_size()); }
    coord_t to_screen(coord_t c, dimen_t tile_size) const;

    void set_zoom(int amount, wxPoint position);
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

    void on_left_down(wxMouseEvent& event)  { on_down(event, MBTN_LEFT); }
    void on_left_up(wxMouseEvent& event)    { on_up(event,   MBTN_LEFT); }
    void on_right_down(wxMouseEvent& event) { on_down(event, MBTN_RIGHT); }
    void on_right_up(wxMouseEvent& event)   { on_up(event,   MBTN_RIGHT); }

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

class base_tab_panel_t : public wxPanel
{
public:
    base_tab_panel_t(wxWindow* parent)
    : wxPanel(parent)
    {}

    virtual void on_manage() = 0;
};

template<typename P>
class tab_panel_t : public base_tab_panel_t
{
public:
    using page_type = typename P::page_type;
    using object_type = typename P::object_type;

    tab_panel_t(wxWindow* parent, model_t& model)
    : base_tab_panel_t(parent)
    , model(model)
    {
        notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_BOTTOM);
        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(notebook, wxSizerFlags().Expand().Proportion(1));
        SetSizer(sizer);

        notebook->Connect(wxID_ANY, wxEVT_RIGHT_UP, wxMouseEventHandler(tab_panel_t::on_right_click), nullptr, this);

        Bind(wxEVT_MENU, &tab_panel_t::on_close_tab, this, ID_R_CLOSE_TAB);
        Bind(wxEVT_MENU, &tab_panel_t::on_manage, this, ID_R_MANAGE_TABS);
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

    void prepare_page(int i)
    {
        if(i >= 0 && i <= (int)notebook->GetPageCount())
            P::on_page_changing(page(i), object(i));
    }

    void on_page_changing(wxNotebookEvent& event) { prepare_page(event.GetSelection()); }
    void page_changed() { prepare_page(notebook->GetSelection()); }

    void on_right_click(wxMouseEvent& event)
    {
        rtab_id = notebook->HitTest(wxPoint(event.GetX(), event.GetY()));

        wxMenu tab_menu;
        tab_menu.Append(ID_R_CLOSE_TAB, std::string("&Close Tab"));
        tab_menu.Append(ID_R_MANAGE_TABS, std::string("&Manage Tabs"));

        tab_menu.Enable(ID_R_CLOSE_TAB, rtab_id >= 0);

        PopupMenu(&tab_menu);
    }

    void on_close_tab(wxCommandEvent& event)
    {
        if(rtab_id < 0 || rtab_id >= int(notebook->GetPageCount()))
            return;
        notebook->RemovePage(rtab_id);
    }

    virtual void on_manage() override
    {
        wxArrayString items;
        for(auto const& object : collection())
            items.push_back(object->name);

        wxArrayInt order;
        int last = -1;
        for(unsigned i = 0; i < collection().size(); ++i)
        {
            auto ptr = collection()[i].get();

            for(unsigned j = 0; j < notebook->GetPageCount(); ++j)
            {
                if(page(j).ptr() == ptr)
                {
                    order.push_back(i);
                    if((unsigned)notebook->GetSelection() == j)
                        last = i;
                    goto next_iter;
                }
            }
            order.push_back(~i);
        next_iter:;
        }

        tab_dialog_t<P> dlg(nullptr, model,
            "Choose which tabs to display. Right-click for more options.", 
            P::name + std::string(" Manager"), order, items);

        if(last >= 0)
            dlg.GetList()->Select(last);

        if(dlg.ShowModal() == wxID_OK) 
        {
            notebook->DeleteAllPages();

            order = dlg.GetOrder();

            for(std::size_t n = 0; n < order.size(); ++n) {
                if(order[n] >= 0) {
                    auto& object = collection().at(order[n]);
                    notebook->AddPage(new page_type(notebook, model, object), object->name);
                    if(n == (std::size_t)dlg.GetList()->GetSelection())
                        notebook->SetSelection(notebook->GetPageCount() - 1);
                }
            }

            std::remove_reference_t<decltype(collection())> new_collection;
            for(size_t n = 0; n < order.size(); ++n) {
                auto const index = order[n] >= 0 ? order[n] : ~order[n];
                new_collection.emplace_back(std::move(collection()[index]));
            }
            collection() = std::move(new_collection);
        }
    }

    void on_manage(wxCommandEvent& event) { on_manage(); }

protected:

    auto& collection() { return P::collection(model); }

    model_t& model;
    int rtab_id = -1;
    wxNotebook* notebook;
};

#endif

