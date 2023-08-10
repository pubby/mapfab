#include "grid_box.hpp"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/dcgraph.h>

////////////////////////////////////////////////////////////////////////////////
// grid_box_t //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

grid_box_t::grid_box_t(wxWindow* parent, bool can_zoom)
: wxScrolledWindow(parent)
{
    SetDoubleBuffered(true);
    Bind(wxEVT_LEFT_DCLICK, &grid_box_t::on_left_down, this);
    Bind(wxEVT_LEFT_DOWN, &grid_box_t::on_left_down, this);
    Bind(wxEVT_LEFT_UP, &grid_box_t::on_left_up, this);
    Bind(wxEVT_RIGHT_DCLICK, &grid_box_t::on_right_down, this);
    Bind(wxEVT_RIGHT_DOWN, &grid_box_t::on_right_down, this);
    Bind(wxEVT_RIGHT_UP, &grid_box_t::on_right_up, this);
    Bind(wxEVT_MOTION, &grid_box_t::on_motion, this);
    if(can_zoom)
        Bind(wxEVT_MOUSEWHEEL, &grid_box_t::on_wheel, this);

    Bind(wxEVT_UPDATE_UI, &grid_box_t::on_update, this);

    Connect(wxEVT_PAINT, wxPaintEventHandler(grid_box_t::on_paint), 0, this);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

void grid_box_t::on_paint(wxPaintEvent& event)
{
#if GC_RENDER
    wxPaintDC dc(this);
    dc.Clear();

#ifdef __WXMSW__
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetDirect2DRenderer();
#else
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetCairoRenderer();
#endif

    std::unique_ptr<wxGraphicsContext> gc(renderer->CreateContext(dc));
    if(gc)
    {
        gc->SetInterpolationQuality(wxINTERPOLATION_NONE);
        gc->SetAntialiasMode(wxANTIALIAS_NONE);
        gc->Translate(-GetViewStart().x, -GetViewStart().y);
        gc->Scale(scale, scale);
        on_draw(*gc);
    }
#else
    wxPaintDC dc(this);
    PrepareDC(dc);
    dc.SetUserScale(scale, scale);
    on_draw(dc);
#endif
}

void grid_box_t::grid_resize(dimen_t dimen)
{
    if(grid_dimen == dimen)
        return;
    grid_dimen = dimen;
    int const w = (dimen.w * tile_size().w + margin().w * 2) * scale;
    int const h = (dimen.h * tile_size().h + margin().h * 2) * scale;
    SetScrollbars(1,1, w, h, 0, 0);
    SetMinSize({ w, h });
}

coord_t grid_box_t::from_screen(coord_t pixel, dimen_t tile_size, int user_scale) const
{
    int sx, sy;
    GetViewStart(&sx, &sy);

    pixel.x += sx;
    pixel.y += sy;
    pixel.x *= user_scale;
    pixel.y *= user_scale;
    pixel.x /= scale;
    pixel.y /= scale;
    pixel.x -= margin().w;
    pixel.y -= margin().h;
    pixel.x /= tile_size.w;
    pixel.y /= tile_size.h;
    return pixel;
}

coord_t grid_box_t::to_screen(coord_t c, dimen_t tile_size) const
{
    c.x *= tile_size.w;
    c.y *= tile_size.h;
    c.x += margin().w;
    c.y += margin().h;
    return c;
}

void grid_box_t::on_down(wxMouseEvent& event, mouse_button_t mb) 
{
    SetFocus();

    if(mouse_down && mouse_down != mb)
    {
        mouse_down = MB_NONE;
        Refresh();
    }
    else
    {
        mouse_down = mb;
        wxPoint pos = event.GetPosition();
        on_down(mb, { pos.x, pos.y });
    }
}

void grid_box_t::on_up(wxMouseEvent& event, mouse_button_t mb) 
{
    if(mouse_down == mb)
    {
        mouse_down = MB_NONE;
        wxPoint pos = event.GetPosition();
        on_up(mb, { pos.x, pos.y });
    }
}

void grid_box_t::on_motion(wxMouseEvent& event) 
{
    wxPoint pos = event.GetPosition();
    mouse_current = { pos.x, pos.y };
    on_motion(mouse_current);
}

void grid_box_t::on_wheel(wxMouseEvent& event)
{
    int const rot = event.GetWheelRotation();
    int const delta = event.GetWheelDelta();
    int const turns = rot / delta;
    int const target = turns >= 0 ? (scale << turns) : (scale >> -turns);
    int const diff = target - scale;

    int const new_scale = std::clamp(scale + diff, 1, 16);
    auto cursor = event.GetPosition();
    set_zoom(new_scale, cursor);
}

void grid_box_t::set_zoom(int new_scale, wxPoint cursor)
{
    if(new_scale == scale)
        return;

    int const diff = new_scale - scale;
    double const scale_diff = double(new_scale) / double(scale);

    int sx, sy;
    GetViewStart(&sx, &sy);
    sx *= new_scale;
    sy *= new_scale;
    sx += cursor.x * diff;
    sy += cursor.y * diff;
    sx /= scale;
    sy /= scale;

    int const w = (grid_dimen.w * tile_size().w + margin().w * 2) * new_scale;
    int const h = (grid_dimen.h * tile_size().h + margin().h * 2) * new_scale;
    sx = std::clamp(sx, 0, w);
    sy = std::clamp(sy, 0, h);
    scale = new_scale;

    SetScrollbars(1,1, w, h, sx, sy);

    // Hack to fix the scroll bar:
    wxPoint p = CalcScrolledPosition({ 0, 0 });
    Scroll(-p.x + 1, -p.y + 1);
    Scroll(-p.x, -p.y);

    Refresh();
}

void grid_box_t::set_scale(int new_scale)
{
    new_scale = std::clamp(new_scale, 1, 8);

    coord_t s = to_screen(mouse_current);
    int const w = (grid_dimen.w * tile_size().w + margin().w * 2) * scale;
    int const h = (grid_dimen.h * tile_size().h + margin().h * 2) * scale;
    SetScrollbars(1,1, w, h, s.x, s.y);
    SetMinSize({ w, h });

    Refresh();
}

////////////////////////////////////////////////////////////////////////////////
// selector_box_t //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

editor_t& selector_box_t::editor()
{ 
    return static_cast<editor_t&>(*GetParent()); 
}

void selector_box_t::on_draw(render_t& gc)
{
    grid_box_t::on_draw(gc);

    if(!enable_tile_select())
        return;

    if(mouse_down)
    {
        gc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
        if(mouse_down == MBTN_LEFT)
            gc.SetBrush(wxBrush(wxColor(255, 255, 0, 127)));
        else
            gc.SetBrush(wxBrush(wxColor(255, 0, 0, 127)));
        rect_t const r = rect_from_2_coords(from_screen(mouse_start), from_screen(mouse_current));
        coord_t const c0 = to_screen(r.c);
        coord_t const c1 = to_screen(r.e());
        gc.DrawRectangle(c0.x, c0.y, (c1 - c0).x, (c1 - c0).y);
    }
}

void selector_box_t::on_up(mouse_button_t mb, coord_t mouse_end)
{
    if(!enable_tile_select())
        return;

    if(mb == MBTN_LEFT && !wxGetKeyState(WXK_SHIFT))
        selector().select_all(false);

    selector().select(
        rect_from_2_coords(from_screen(mouse_start), from_screen(mouse_end)), 
        mb == MBTN_LEFT);
    Refresh();
}

void selector_box_t::on_motion(coord_t at)
{
    if(!enable_tile_select())
        return;

    if(mouse_down)
        Refresh();
}

void selector_box_t::draw_tiles(render_t& gc)
{
    //dc.SetUserScale(scale, scale);

    if(!enable_tile_select())
        return;

    for(coord_t c : dimen_range(selector().dimen()))
    {
        int x0 = c.x * tile_size().w + margin().w;
        int y0 = c.y * tile_size().h + margin().h;

        unsigned const tile = tiles().layer().to_tile(c);
        draw_tile(gc, tile, { x0, y0 });
    }

    gc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
    gc.SetBrush(wxBrush(wxColor(0, 255, 255, 127)));

    for(coord_t c : dimen_range(selector().dimen()))
    {
        int x0 = c.x * tile_size().w + margin().w;
        int y0 = c.y * tile_size().h + margin().h;
        if(selector()[c])
            gc.DrawRectangle(x0, y0, tile_size().w, tile_size().h);
    }
}

////////////////////////////////////////////////////////////////////////////////
// canvas_box_t ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void canvas_box_t::on_down(mouse_button_t mb, coord_t at)
{
    selector_box_t::on_down(mb, at);

    coord_t const pen = from_screen(at);

    if(mb == MBTN_LEFT && (model.tool == TOOL_DROPPER || wxGetKeyState(WXK_CONTROL)) && in_bounds(pen, layer().canvas_dimen()))
    {
        layer().dropper(pen);
        GetParent()->Refresh();
    }
}

void canvas_box_t::on_up(mouse_button_t mb, coord_t mouse_end)
{
    coord_t const pen = from_screen(mouse_end);

    if(pasting())
    {
        if(mb == MBTN_RIGHT)
            goto done_paste;
        else if(mb == MBTN_LEFT)
        {
            model.modify();

            editor().history.push(layer().save(pen));
            layer().paste(*model.paste, pen);
            post_update();
        done_paste:
            model.paste.reset();
            Refresh();
            return;
        }
    }

    if(wxGetKeyState(WXK_CONTROL))
        return;

    if(model.tool == TOOL_SELECT)
        selector_box_t::on_up(mb, mouse_end);
    else if(model.tool == TOOL_STAMP && mb == MBTN_LEFT)
    {
        bool modify = false;
        layer().for_each_picked(pen, [&](coord_t c, std::uint16_t tile)
        { 
            modify |= (layer().get(c) != tile);
        });

        if(!modify)
            return;
        model.modify();

        editor().history.push(layer().save(pen));

        layer().for_each_picked(pen, [&](coord_t c, std::uint16_t tile)
        { 
            layer().set(c, tile);
        });

        post_update();
    }

    Refresh();
}

void canvas_box_t::draw_tiles(render_t& gc)
{
    draw_underlays(gc);
    draw_overlays(gc);
}

void canvas_box_t::draw_underlays(render_t& gc)
{
    for(coord_t c : dimen_range(layer().canvas_dimen()))
    {
        int x0 = c.x * tile_size().w + margin().w;
        int y0 = c.y * tile_size().h + margin().h;

        unsigned const tile = layer().get(c);
        draw_tile(gc, tile, { x0, y0 });
    }
}

void canvas_box_t::draw_overlays(render_t& gc)
{
    if(!enable_tile_select())
        return;

    if(model.tool == TOOL_SELECT)
    {
        for(coord_t c : dimen_range(layer().canvas_selector.dimen()))
        {
            int x0 = c.x * tile_size().w + margin().w;
            int y0 = c.y * tile_size().h + margin().h;

            if(model.tool == TOOL_SELECT && layer().canvas_selector[c])
            {
                gc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
                gc.SetBrush(wxBrush(wxColor(0, 255, 255, 127)));
                gc.DrawRectangle(x0, y0, tile_size().w, tile_size().h);
            }
        }
    }

    if(pasting())
    {
        if(auto* grid = std::get_if<grid_t<std::uint16_t>>(&model.paste->data))
        {
            gc.SetPen(wxPen(wxColor(255, 255, 0), 0));
            gc.SetBrush(wxBrush(wxColor(255, 0, 255, 127)));
            coord_t const pen = from_screen(mouse_current);
            for(coord_t c : dimen_range(grid->dimen()))
            {
                if((*grid)[c] != std::uint16_t(~0u))
                {
                    coord_t const c0 = to_screen(c + pen);
                    gc.DrawRectangle(c0.x, c0.y, tile_size().w, tile_size().h);
                }
            }
        }
    }
    else if(model.tool == TOOL_STAMP)
    {
        gc.SetPen(wxPen(wxColor(255, 255, 255, 127), 0));
        gc.SetBrush(wxBrush(wxColor(0, 255, 255, mouse_down == MBTN_LEFT ? 127 : 31)));
        layer().for_each_picked(from_screen(mouse_current), [&](coord_t c, std::uint8_t tile)
        {
            coord_t const c0 = to_screen(c);
            gc.DrawRectangle(c0.x, c0.y, tile_size().w, tile_size().h);
        });
    }
}

////////////////////////////////////////////////////////////////////////////////
// editor_t ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

editor_t::editor_t(wxWindow* parent) 
: wxPanel(parent)
{
    Bind(wxEVT_UPDATE_UI, &editor_t::on_update, this);
}

tile_copy_t editor_t::copy(bool cut) 
{ 
    tile_copy_t ret;

    if(cut)
    {
        undo_t undo;
        ret = layer().copy(&undo); 
        history.push(std::move(undo));
    }
    else
        ret = layer().copy(); 

    Refresh();
    return ret;
}
