#ifndef UNDO_HPP
#define UNDO_HPP

#include <cstdint>
#include <variant>

#include "2d/units.hpp"
#include "2d/grid.hpp"

using namespace i2d;

enum undo_type_t { UNDO, REDO };

struct undo_tiles_t
{
    grid_t<std::uint8_t> tiles;
    grid_t<std::uint8_t> attributes;
};

struct undo_attribute_t
{
    coord_t at;
    std::uint8_t attribute;
};

struct undo_collisions_t
{
    grid_t<std::uint8_t> collisions;
};

using undo_t = std::variant
    < undo_tiles_t
    , undo_attribute_t
    , undo_collisions_t
    >;

#endif
