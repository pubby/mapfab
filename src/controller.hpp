#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

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

struct metatile_map_t
{
    void set_tile(coord_t c, std::uint8_t tile, std::uint8_t attr) 
    {
        tiles.at(c) = tile; 
        c.x /= 2;
        c.y /= 2;
        attributes.at(c) = attr;
    }

    void resize(dimen_t d)
    {
        tiles.resize(d);
        d.w = (d.w + 1) / 2;
        d.h = (d.h + 1) / 2;
        attributes.resize(d);
        collisions.resize(d);
    }

    grid_t<std::uint8_t> tiles;
    grid_t<std::uint8_t> attributes;
    grid_t<std::uint8_t> collisions;
};

struct palette_map_t
{
    palette_map_t() 
    { 
        grid.resize({ 25, 256 }); 
        grid.fill(0x0F);
    }
    dimen_t dimen() const { return { 25, num }; }
    auto& operator[](coord_t i) { return grid.at(i); }

    unsigned num = 1; 
    grid_t<std::uint8_t> grid;
};

constexpr std::uint8_t ACTIVE_COLLISION = 4;
static constexpr std::size_t UNDO_LIMIT = 256;

struct controller_t
{
    std::uint8_t active = 0;
    select_map_t color_selector;
    select_map_t tile_palette;
    select_map_t collision_palette;

    palette_map_t palette_map;
    metatile_map_t metatiles;

    std::vector<attr_bitmaps_t> chr_bitmaps;
    std::vector<wxBitmap> collision_bitmaps;

    std::deque<undo_t> history[2];

    int active_tile_size() const { return active == ACTIVE_COLLISION ? 16 : 8; }
    auto& mt_selector() { return active == ACTIVE_COLLISION ? collision_palette : tile_palette; }

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
    void for_each_mt_pen(coord_t pen_c, Fn const& fn)
    {
        auto& selector = mt_selector();
        mt_selector().for_each_selected([&](coord_t c)
        {
            std::uint8_t tile = c.x + c.y * selector.dimen().w;
            coord_t const at = pen_c + c - selector.select_rect().c;
            if(in_bounds(at, metatiles.tiles.dimen()))
                fn(at, tile);
        });
    }
};

#endif
