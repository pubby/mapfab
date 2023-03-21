#include "wx/wx.h"
#include "wx/sizer.h"

#include "2d/geometry.hpp"

#include "convert.hpp"
#include "TilePalette.hpp"

using namespace i2d;

struct model_t
{
    // TODO
};

enum
{
    ID_NEW = 1,
    ID_OPEN = 2,
};

class MyApp: public wxApp
{
    bool OnInit();
    
    wxFrame *frame;
    model_t model;
public:
        
};

IMPLEMENT_APP(MyApp)

class MyFrame : public wxFrame
{
public:
    MyFrame();
    void add() {
    }
 
private:
    void OnOpen(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);

    TilePalette* tilePalette;
    TilePalette* tilePalette2;

    // The Path to the file we have open
    wxString CurrentDocPath;
};



bool MyApp::OnInit()
{
    wxInitAllImageHandlers();

    //frame = new wxFrame((wxFrame *)NULL, -1,  wxT("Hello wxDC"), wxPoint(50,50), wxSize(800,600));

    frame = new MyFrame();

    frame->Show();
    return true;
} 
 
MyFrame::MyFrame()
: wxFrame(nullptr, wxID_ANY, "Hello World", wxPoint(50, 50), wxSize(800, 600))
{
    wxMenu* menuFile = new wxMenu;
    menuFile->Append(ID_OPEN, "&Hello...\tCtrl-H",
                     "Help string shown in status bar for this menu item");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);
 
    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
 
    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
 
    SetMenuBar( menuBar );
 
    CreateStatusBar();
    SetStatusText("Welcome to wxWidgets!");

    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    tilePalette = new TilePalette((wxFrame*)this);
    tilePalette2 = new TilePalette((wxFrame*)this);

    sizer->Add(tilePalette, wxSizerFlags().Expand());
    sizer->Add(tilePalette2, wxSizerFlags().Expand().Proportion(1));
    SetSizer(sizer);
    //SetAutoLayout(true);
    Layout();
    Refresh();
    Update();
    Layout();
    Refresh();
    Update();
    Show(false);
    Show(true);
 
    Bind(wxEVT_MENU, &MyFrame::OnOpen, this, ID_OPEN);
    Bind(wxEVT_MENU, &MyFrame::OnAbout, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &MyFrame::OnExit, this, wxID_EXIT);
}
 
void MyFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}
 
void MyFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox("This is a wxWidgets Hello World example",
                 "About Hello World", wxOK | wxICON_INFORMATION);
}
 
void MyFrame::OnOpen(wxCommandEvent& event)
{
    wxFileDialog* OpenDialog = new wxFileDialog(
        this, _("Choose a file to open"), wxEmptyString, wxEmptyString, 
        _("CHR files (*.chr, *.bin)|*.chr;*.bin|Image Files (*.png)|*.png"),
        wxFD_OPEN, wxDefaultPosition);

    // Creates a "open file" dialog with 4 file types
    if(OpenDialog->ShowModal() == wxID_OK) // if the user click "Open" instead of "Cancel"
    {
        CurrentDocPath = OpenDialog->GetPath();

        std::vector<std::uint8_t> data = read_binary_file(CurrentDocPath.c_str());
        std::vector<wxImage> images = chr_to_image(data.data(), data.size(), default_palette.data());
        tilePalette->load_images(images);


        // Sets our current document to the file the user selected
        //LoadFile(CurrentDocPath); //Opens that file
#include "TilePalette.hpp"
        SetTitle(wxString("Edit - ") << 
            OpenDialog->GetFilename()); // Set the Title to reflect the file open
    }

    // Clean up after ourselves
    OpenDialog->Destroy();
}
