#ifndef model_HPP
#define model_HPP

#include <cassert>
#include <cstdint>

#include "2d/geometry.hpp"
#include "2d/grid.hpp"

#include "convert.hpp"
#include "undo.hpp"

using namespace i2d;

class select_map_t
{
public:
    dimen_t dimen() const { return m_selection.dimen(); }

    bool has_selection() const { return (bool)m_select_rect; }
    rect_t select_rect() const { return m_select_rect; }
    auto const& selection() const { return m_selection; }
    auto const& operator[](coord_t c) const { return m_selection.at(c); }

    void select_all(bool select = true)
    { 
        m_selection.fill(select); 
        if(select)
            m_select_rect = to_rect(dimen());
        else
            m_select_rect = {}; 
    }

    void select(coord_t c, bool select = true)
    { 
        if(!in_bounds(c, dimen()))
            return;
        m_selection.at(c) = select; 
        if(select)
            m_select_rect = grow_rect_to_contain(m_select_rect, c);
        else
            recalc_select_rect(m_select_rect);
    }

    void select(rect_t r, bool select = true) 
    { 
        if(r = crop(r, dimen()))
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

    virtual void resize(dimen_t d) 
    { 
        m_selection.resize(d);
        recalc_select_rect(to_rect(d));
    }

    template<typename Fn>
    void for_each_selected(Fn const& fn)
    {
        rect_t const r = select_rect();
        for(coord_t c : rect_range(select_rect()))
            if(m_selection[c])
                fn(c);
    }

private:
    void recalc_select_rect(rect_t range)
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

    rect_t m_select_rect = {};
    grid_t<std::uint8_t> m_selection;
};

struct palette_model_t
{
    palette_model_t() { clear(); }
    void clear() { grid.fill(0x0F); }

    unsigned num = 1; 
    fixed_grid_t<std::uint8_t, 25, 256> grid;
};

struct metatile_model_t
{
    void set_tile(coord_t c, std::uint8_t tile, std::uint8_t attr) 
    {
        tiles.at(c) = tile; 
        c.x /= 2;
        c.y /= 2;
        attributes.at(c) = attr;
    }

    fixed_grid_t<std::uint8_t, 32, 32> tiles;
    fixed_grid_t<std::uint8_t, 16, 16> attributes;
    fixed_grid_t<std::uint8_t, 16, 16> collisions;
    std::uint8_t palette = 0;
};

struct level_map_t
{
    level_map_t() { resize({ 16, 15 }); }
    dimen_t dimen() const { return grid.dimen(); }
    void resize(dimen_t dimen) { grid.resize(dimen); }

    std::string name;
    std::uint8_t palette = 0;
    grid_t<std::uint8_t> grid;
};

constexpr std::uint8_t ACTIVE_COLLISION = 4;
static constexpr std::size_t UNDO_LIMIT = 256;

struct model_t
{
    std::uint8_t active = 0;
    select_map_t color_selector;
    select_map_t tile_palette; // TODO
    select_map_t collision_palette; // TODO
    select_map_t mt_selector;

    std::array<std::uint8_t, 16*256> chr = {};
    palette_model_t palette;
    metatile_model_t metatiles;
    std::deque<level_model_t> levels;

    std::vector<attr_bitmaps_t> chr_bitmaps;
    std::vector<wxBitmap> collision_bitmaps;
    std::vector<wxBitmap> metatile_bitmaps;

    std::deque<undo_t> history[2];

    int active_tile_size() const { return active == ACTIVE_COLLISION ? 16 : 8; }
    auto& chr_selector() { return active == ACTIVE_COLLISION ? collision_palette : tile_palette; }

    std::array<std::uint8_t, 16> active_palette_array()
    {
        std::array<std::uint8_t, 16> ret;
        for(unsigned i = 0; i < 4; ++i)
        {
            ret[i*4] = palette_map[{ 0, active_palette }];
            for(unsigned j = 1; j < 4; ++j)
                ret[i*4+j] = palette_map[{ i*3 + j, active_palette }];
        }
        return ret;
    }

    void refresh_chr()
    {
        auto const palette_array = active_palette_array();
        chr_bitmaps = chr_to_bitmaps(chr.data(), chr.size(), palette_array.data());

        // TODO:
        tile_palette.resize({ std::min<int>(chr_bitmaps.size(), 16), chr_bitmaps.size() / 16 });
    }

    void refresh_metatiles()
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
                    if(in_bounds(c0, metatiles.tiles.dimen()))
                    {
                        std::uint8_t const i = metatiles.tiles.at({ c.x*2 + x, c.y*2 + y });
                        std::uint8_t const a = metatiles.attributes.at(c);
                        dc.DrawBitmap(chr_bitmaps.at(i)[a], { x*8, y*8 }, false);
                    }
                }
            }

            metatile_bitmaps.push_back(std::move(bitmap));
        }
    }


    template<undo_type_t U>
    void undo() 
    { 
        if(history[U].empty())
            return;
        history[!U].push_front(undo(history[U].front()));
        history[U].pop_front();
    }

    undo_t undo(undo_t const& undo) { return std::visit(*this, undo); }

    // Undo operations
    undo_t operator()(undo_tiles_t const& undo) 
    { 
        undo_tiles_t ret = { metatiles.tiles, metatiles.attributes };
        metatiles.tiles = undo.tiles; 
        metatiles.attributes = undo.attributes; 
        return ret;
    }

    undo_t operator()(undo_attribute_t const& undo) 
    { 
        undo_attribute_t ret = { metatiles.attributes.at(undo.at) };
        metatiles.attributes.at(undo.at) = undo.attribute; 
        return ret;
    }

    undo_t operator()(undo_collisions_t const& undo) 
    { 
        undo_collisions_t ret = { metatiles.collisions };
        metatiles.collisions = undo.collisions; 
        return ret;
    }

    void cull_undo()
    {
        if(history[UNDO].size() > UNDO_LIMIT)
            history[UNDO].resize(UNDO_LIMIT);
    }

    void do_tiles() 
    { 
        history[REDO].clear(); 
        history[UNDO].push_front(undo_tiles_t{ metatiles.tiles, metatiles.attributes }); 
        cull_undo();
    }

    void do_attribute(coord_t at) 
    { 
        history[REDO].clear(); 
        history[UNDO].push_front(undo_attribute_t{ at, metatiles.attributes.at(at) }); 
        cull_undo();
    }

    void do_collisions() 
    { 
        history[REDO].clear(); 
        history[UNDO].push_front(undo_collisions_t{ metatiles.collisions }); 
        cull_undo();
    }

    template<typename Fn>
    void for_each_color_pen(coord_t pen_c, Fn const& fn)
    {
        color_selector.for_each_selected([&](coord_t c)
        {
            std::uint8_t color = c.x * 16 + c.y;
            coord_t const at = pen_c + c - color_selector.select_rect().c;
            if(in_bounds(at, palette_map.dimen()))
                fn(at, color);
        });
    }

    template<typename Fn>
    void for_each_chr_pen(coord_t pen_c, Fn const& fn)
    {
        auto& selector = chr_selector();
        chr_selector().for_each_selected([&](coord_t c)
        {
            std::uint8_t tile = c.x + c.y * selector.dimen().w;
            coord_t const at = pen_c + c - selector.select_rect().c;
            if(in_bounds(at, metatiles.tiles.dimen()))
                fn(at, tile);
        });
    }

    template<typename Fn>
    void for_each_mt_pen(coord_t pen_c, Fn const& fn)
    {
        mt_selector.for_each_selected([&](coord_t c)
        {
            std::uint8_t color = c.x + c.y * 16;
            coord_t const at = pen_c + c - mt_selector.select_rect().c;
            if(in_bounds(at, level.dimen()))
                fn(at, color);
        });
    }
};

void model_t::write_file(FILE* fp) const
{
    std::fwrite("MapFab", 6, fp);

    // Version:
    std::fputc(VERSION, fp);

    // Unused:
    std::fputc(0, fp);

    // Palettes:
    std::fputc(palette.num-1, fp);
    for(std::uint8_t& data : palette.grid)
        std::fputc(data, fp);

    // Metatiles:
    std::fputc(metatiles.palette, fp);
    std::fputc(metatiles.num-1, fp);
    for(std::uint8_t data : metatiles.tiles)
        std::fputc(data, fp);
    for(std::uint8_t data : metatiles.attributes)
        std::fputc(data, fp);
    for(std::uint8_t data : metatiles.collisions)
        std::fputc(data, fp);

    // Levels:
    std::fputc(levels.size()-1, fp);
    for(auto const& level : levels)
    {
        std::fwrite(level.name.c_str(), level.name.size()+1, fp);
        std::fputc(level.palette, fp);
        std::fputc(level.dimen().w, fp);
        std::fputc(level.dimen().h, fp);
        for(std::uint8_t data : level.grid)
            std::fputc(data, fp);
        std::fputc(0, fp); // TODO: Number of objects
    }
}

void model_t::read_file(FILE* fp)
{
    constexpr char const* ERROR_STRING = "Invalid MapFab file.";

    auto const get = [&]() -> char
    {
        int const got = std::getc(fp);
        if(got == EOF)
            throw std::runtime_error(ERROR_STR);
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
        throw std::runtime_error(ERROR_STR);
    if(memcmp(buffer, "MapFab", 6) != 0)
        throw std::runtime_error(ERROR_STR);
    if(buffer[6] >= VERSION)
        throw std::runtime_error("File is from a newer version of MapFab.");

    // Palettes:
    palette.num = get()+1;
    palette.clear();
    for(std::uint8_t& data : palette.grid)
        palette[{ j, i }] = get();

    // Metatiles:
    metatiles.palette = get();
    metatiles.num = get()+1;
    for(std::uint8_t& data : metatiles.tiles)
        data = get();
    for(std::uint8_t& data : metatiles.attributes)
        data = get();
    for(std::uint8_t& data : metatiles.collisions)
        data = get();

    // Levels:
    levels.clear();
    levels.resize(get()+1);
    for(auto& level : levels)
    {
        level.name = get_str();
        level.palette = get();
        level.resize({ get(), get() });
        for(std::uint8_t& data : level.grid)
            data = get();
        get(); // TODO: Number of objects
    }
}

#endif
