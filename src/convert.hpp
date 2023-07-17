#ifndef CONVERT_HPP
#define CONVERT_HPP

#include "wx/wx.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <deque>
#include <vector>

#include "2d/geometry.hpp"

#include "guard.hpp"
#include "nes_colors.hpp"

using namespace i2d;

constexpr std::array<std::uint8_t, 16> default_palette = {{
    0x0F,
    0x11,
    0x21,
    0x31,

    0x0F,
    0x14,
    0x24,
    0x34,

    0x0F,
    0x17,
    0x27,
    0x37,

    0x0F,
    0x1A,
    0x2A,
    0x3A,
}};

std::vector<std::uint8_t> read_binary_file(char const* filename);

using attr_bitmaps_t = std::array<wxBitmap, 4>;

std::vector<attr_bitmaps_t> chr_to_bitmaps(std::uint8_t const* data, std::size_t size, std::uint8_t const* palette);

std::vector<wxBitmap> load_collision_file(wxString const& string);

std::vector<std::uint8_t> png_to_chr(std::uint8_t const* png, std::size_t size, bool chr16);

#endif
