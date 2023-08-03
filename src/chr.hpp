#ifndef CHR_HPP
#define CHR_HPP

#include <cassert>
#include <ranges>

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/notebook.h>
#include <wx/grid.h>
#include <wx/clrpicker.h>

#include "2d/geometry.hpp"
#include "2d/pretty_print.hpp"

#include "id.hpp"
#include "model.hpp"
#include "convert.hpp"
#include "grid_box.hpp"

using namespace i2d;

class file_def_t : public wxPanel
{
public:
    file_def_t(wxWindow* parent, model_t const& model, chr_file_t const& file, unsigned index);

    void on_open(wxCommandEvent& event);
    void on_delete(wxCommandEvent& event);
    void on_rename(wxCommandEvent& event);

    unsigned index = 0;
private:
    model_t const& model;
    chr_file_t const& file;
    wxTextCtrl* filename;
    wxTextCtrl* name_entry;
};

class chr_editor_t : public wxScrolledWindow
{
public:
    chr_editor_t(wxWindow* parent, model_t& model);

    void on_delete(unsigned index);
    void on_new(wxCommandEvent& event);
    void on_rename(unsigned index, std::string str);
    void on_open(unsigned index, std::string path);
    void on_open_collision(wxCommandEvent& event);

    void load();

private:
    void new_file(chr_file_t const& file);

    model_t& model;
    std::deque<std::unique_ptr<file_def_t>> file_defs;
    wxBoxSizer* file_sizer;
    wxTextCtrl* collision_filename;
};

#endif
