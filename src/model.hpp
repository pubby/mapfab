#ifndef MODEL_HPP
#define MODEL_HPP

#include <cassert>
#include <cstdint>

#include "2d/geometry.hpp"
#include "2d/grid.hpp"

using namespace i2d;


class tile_map_t : public select_map_t
{
public:
    auto const& tiles() { return m_tiles; }
    void set_tile(coord_t c, std::uint8_t tile) { m_tiles.at(c) = tile; }

    virtual void resize(dimen_t d) 
    {
        select_map_t::resize(d);
        m_tiles.resize(d);
    }

private:
    grid_t<std::uint8_t> m_tiles;
};

class tile_attribute_map_t : public tile_map_t
{
    auto const& attributes() { return m_attributes; }
    void set_attribute(coord_t c, std::uint8_t tile) { m_attributes.at(attr_crd(c)) = tile; }
    std::uint8_t get_attribute(coord_t c) { return m_attributes.at(attr_crd(c)); }

    virtual void resize(dimen_t d) 
    {
        tile_map_t::resize(d);
        m_attributes.resize({ (d.w + 1) / 2, (d.h + 1) / 2 });
    }

private:
    static coord_t attr_crd(coord_t c) { return { c.x / 2, c.y / 2 }; }

    grid_t<std::uint8_t> m_attributes;
};

/* TODO
struct
{
    fixed_grid_t<std::uint8_t, 2, 2> patterns;
    std::uint8_t metadata;

    unsigned attribute() const { return (metadata >> 6) & 0b11; }
    unsigned collision() const { return metadata & 0b111111; }
};
*/

#endif
