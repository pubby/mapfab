#include "model.hpp"

////////////////////////////////////////////////////////////////////////////////
// select_map_t ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void select_map_t::select_all(bool select)
{ 
    m_selection.fill(select); 
    if(select)
        m_select_rect = to_rect(dimen());
    else
        m_select_rect = {}; 
}

void select_map_t::select(std::uint8_t tile, bool select_)
{
    this->select(coord_t{ tile % dimen().w, tile / dimen().w }, select_);
}

void select_map_t::select_transpose(std::uint8_t tile, bool select_)
{
    this->select(coord_t{ tile / dimen().h, tile % dimen().h }, select_);
}

void select_map_t::select(coord_t c, bool select)
{ 
    if(!in_bounds(c, dimen()))
        return;
    m_selection.at(c) = select; 
    if(select)
        m_select_rect = grow_rect_to_contain(m_select_rect, c);
    else
        recalc_select_rect(m_select_rect);
}

void select_map_t::select(rect_t r, bool select) 
{ 
    if((r = crop(r, dimen())))
    {
        for(coord_t c : rect_range(r))
        {
            assert(in_bounds(c, dimen()));
            m_selection.at(c) = select; 
        }
        if(select)
            m_select_rect = grow_rect_to_contain(m_select_rect, r);
        else
            recalc_select_rect(m_select_rect);
    }
}

void select_map_t::resize(dimen_t d) 
{ 
    m_selection.resize(d);
    recalc_select_rect(to_rect(d));
}

void select_map_t::recalc_select_rect(rect_t range)
{
    coord_t min = to_coord(dimen());
    coord_t max = { 0, 0 };

    for(coord_t c : rect_range(range))
    {
        if(m_selection[c])
        {
            min.x = std::min<int>(min.x, c.x);
            min.y = std::min<int>(min.y, c.y);
            max.x = std::max<int>(max.x, c.x);
            max.y = std::max<int>(max.y, c.y);
        }
    }

    if(min.x <= max.x && min.y <= max.y)
        m_select_rect = rect_from_2_coords(min, max);
    else
        m_select_rect = {};
}

////////////////////////////////////////////////////////////////////////////////
// tile_layer_t ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

tile_copy_t tile_layer_t::copy(bool cut)
{
    rect_t const rect = crop(canvas_selector.select_rect(), canvas_dimen());
    tile_copy_t copy = { format() };
    copy.tiles.resize(rect.d);
    for(coord_t c : rect_range(rect))
    {
        if(canvas_selector[c])
        {
            copy.tiles[c - rect.c] = get(c);
            if(cut)
                reset(c);
        }
        else
            copy.tiles[c - rect.c] = ~0u;
    }
    return copy;
}

void tile_layer_t::paste(tile_copy_t const& copy, coord_t at)
{
    for(coord_t c : dimen_range(copy.tiles.dimen()))
        if(~copy.tiles[c] && in_bounds(at + c, canvas_dimen()))
            set(at + c, copy.tiles[c]);
}

void tile_layer_t::dropper(coord_t at) 
{ 
    picker_selector.select_all(false);
    picker_selector.select(to_pick(get(at)));
}

undo_t tile_layer_t::fill()
{
    rect_t const canvas_rect = crop(canvas_selector.select_rect(), canvas_dimen());
    rect_t const picker_rect = picker_selector.select_rect();

    if(!canvas_rect || !picker_rect)
        return {};

    undo_t ret = save(canvas_rect);

    canvas_selector.for_each_selected([&](coord_t c)
    {
        std::uint8_t const tile = to_tile(c);
        coord_t const o = c - canvas_rect.c;
        coord_t const p = coord_t{ o.x % picker_rect.d.w, o.y % picker_rect.d.h } + picker_rect.c;
        set(c, to_tile(p));
    });

    return ret;
}

undo_t tile_layer_t::fill_paste(tile_copy_t const& copy)
{
    rect_t const canvas_rect = crop(canvas_selector.select_rect(), canvas_dimen());
    dimen_t const copy_dimen = copy.tiles.dimen();

    if(!canvas_rect || !copy_dimen)
        return {};

    undo_t ret = save(canvas_rect);

    canvas_selector.for_each_selected([&](coord_t c)
    {
        std::uint8_t const tile = to_tile(c);

        coord_t const o = c - canvas_rect.c;
        coord_t const p = coord_t{ o.x % copy_dimen.w, o.y % copy_dimen.h };

        if(~copy.tiles[p] && in_bounds(c, canvas_dimen()))
            set(c, copy.tiles[p]);
    });

    return ret;
}

undo_t tile_layer_t::save(rect_t rect)
{
    rect = crop(rect, canvas_dimen());
    undo_tiles_t ret = { this, rect };
    for(coord_t c : rect_range(rect))
        ret.tiles.push_back(get(c));
    return undo_t(std::move(ret));
}

////////////////////////////////////////////////////////////////////////////////
// chr_layer_t /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

undo_t chr_layer_t::fill_attribute()
{
    rect_t const canvas_rect = crop(canvas_selector.select_rect(), canvas_dimen());

    if(!canvas_rect || active >= 4)
        return {};

    undo_t ret = save(canvas_rect);

    canvas_selector.for_each_selected([&](coord_t c)
    {
        attributes.at(vec_div(c, 2)) = active;
    });

    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// model_t /////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

undo_t model_t::operator()(undo_tiles_t const& undo)
{ 
    auto ret = undo.layer->save(undo.rect);
    unsigned i = 0;
    for(coord_t c : rect_range(undo.rect))
        undo.layer->set(c, undo.tiles[i++]);
    return ret;
}

undo_t model_t::operator()(undo_palette_num_t const& undo)
{
    auto ret = undo_palette_num_t{ palette.num };
    palette.num = undo.num;
    return ret;
}

undo_t model_t::operator()(undo_level_dimen_t const& undo)
{
    auto ret = undo_level_dimen_t{ undo.layer, undo.layer->tiles };
    undo.layer->tiles = undo.tiles;
    return ret;
}

undo_t model_t::operator()(undo_level_palette_t const& undo)
{
    auto ret = undo_level_palette_t{ undo.model, undo.model->palette };
    undo.model->palette = undo.palette;
    return ret;
}

std::array<std::uint8_t, 16> model_t::active_palette_array()
{
    std::array<std::uint8_t, 16> ret;
    for(unsigned i = 0; i < 4; ++i)
    {
        ret[i*4] = palette.color_layer.tiles.at({ 0, metatiles.palette });
        for(unsigned j = 1; j < 4; ++j)
            ret[i*4+j] = palette.color_layer.tiles.at({ i*3 + j, metatiles.palette });
    }
    return ret;
}

void model_t::refresh_chr()
{
    auto const palette_array = active_palette_array();
    chr_bitmaps = chr_to_bitmaps(chr.data(), chr.size(), palette_array.data());
}

void model_t::refresh_metatiles()
{
    metatile_bitmaps.clear();
    for(coord_t c : dimen_range({ 16, 16 }))
    {
        wxBitmap bitmap(wxSize(16, 16));

        {
            wxMemoryDC dc;
            dc.SelectObject(bitmap);
            for(int y = 0; y < 2; ++y)
            for(int x = 0; x < 2; ++x)
            {
                coord_t const c0 = { c.x*2 + x, c.y*2 + y };
                if(in_bounds(c0, metatiles.chr_layer.tiles.dimen()))
                {
                    std::uint8_t const i = metatiles.chr_layer.tiles.at({ c.x*2 + x, c.y*2 + y });
                    std::uint8_t const a = metatiles.chr_layer.attributes.at(c);
                    dc.DrawBitmap(chr_bitmaps.at(i)[a], { x*8, y*8 }, false);
                }
            }
        }

        metatile_bitmaps.push_back(std::move(bitmap));
    }
}

constexpr std::uint8_t SAVE_VERSION = 1;

void model_t::write_file(FILE* fp, std::string const& chr_path, std::string const& collision_path) const
{
    auto const write_str = [&](std::string const& str)
    {
        std::fwrite(str.c_str(), str.size()+1, 1, fp);
    };

    std::fwrite("MapFab", 6, 1, fp);

    // Version:
    std::fputc(SAVE_VERSION, fp);

    // Unused:
    std::fputc(0, fp);

    // Paths:
    write_str(chr_path);
    write_str(collision_path);

    // Palettes:
    std::fputc(palette.color_layer.num-1, fp);
    for(std::uint8_t data : palette.color_layer.tiles)
        std::fputc(data, fp);

    // Metatiles:
    std::fputc(metatiles.palette, fp);
    std::fputc(metatiles.num-1, fp);
    for(std::uint8_t data : metatiles.chr_layer.tiles)
        std::fputc(data, fp);
    for(std::uint8_t data : metatiles.chr_layer.attributes)
        std::fputc(data, fp);
    for(std::uint8_t data : metatiles.collision_layer.tiles)
        std::fputc(data, fp);

    // Levels:
    std::fputc(levels.size()-1, fp);
    for(auto const& level : levels)
    {
        write_str(level->name.c_str());
        std::fputc(level->palette, fp);
        std::fputc(level->dimen().w, fp);
        std::fputc(level->dimen().h, fp);
        for(std::uint8_t data : level->metatile_layer.tiles)
            std::fputc(data, fp);
        std::fputc(0, fp); // TODO: Number of objects
    }
}

void model_t::read_file(FILE* fp, std::string& chr_path, std::string& collisions_path)
{
    constexpr char const* ERROR_STRING = "Invalid MapFab file.";

    auto const get = [&]() -> char
    {
        int const got = std::getc(fp);
        if(got == EOF)
            throw std::runtime_error(ERROR_STRING);
        return static_cast<char>(got);
    };

    auto const get_str = [&]() -> std::string
    {
        std::string ret;
        while(char c = get())
            ret.push_back(c);
        return ret;
    };

    char buffer[8];
    if(!std::fread(buffer, 8, 1, fp))
        throw std::runtime_error(ERROR_STRING);
    if(memcmp(buffer, "MapFab", 6) != 0)
        throw std::runtime_error(ERROR_STRING);
    if(buffer[6] > SAVE_VERSION)
        throw std::runtime_error("File is from a newer version of MapFab.");

    // Paths:
    chr_path = get_str();
    collisions_path = get_str();

    // Palettes:
    palette.num = get()+1;
    for(std::uint8_t& data : palette.color_layer.tiles)
        data = get();

    // Metatiles:
    metatiles.palette = get();
    metatiles.num = get()+1;
    for(std::uint8_t& data : metatiles.chr_layer.tiles)
        data = get();
    for(std::uint8_t& data : metatiles.chr_layer.attributes)
        data = get();
    for(std::uint8_t& data : metatiles.collision_layer.tiles)
        data = get();

    // Levels:
    levels.clear();
    levels.resize(get()+1);
    for(auto& level : levels)
    {
        level = std::make_shared<level_model_t>();
        level->name = get_str();
        level->palette = get();
        level->metatile_layer.tiles.resize({ get(), get() });
        for(std::uint8_t& data : level->metatile_layer.tiles)
            data = get();
        get(); // TODO: Number of objects
    }

    modified = modified_since_save = false;
}

////////////////////////////////////////////////////////////////////////////////
// undo_history_t //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void undo_history_t::cull()
{
    if(history[UNDO].size() > UNDO_LIMIT)
        history[UNDO].resize(UNDO_LIMIT);
}

void undo_history_t::push(undo_t const& undo)
{
    if(std::holds_alternative<std::monostate>(undo))
        return;
    history[REDO].clear(); 
    history[UNDO].push_front(undo); 
    cull();
}
