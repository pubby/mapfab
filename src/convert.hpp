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

inline std::vector<std::uint8_t> read_binary_file(char const* filename)
{
    FILE* fp = std::fopen(filename, "rb");
    if(!fp)
        return {};
    auto scope_guard = make_scope_guard([&]{ std::fclose(fp); });

    // Get the file size
    std::fseek(fp, 0, SEEK_END);
    std::size_t const file_size = ftell(fp);
    std::fseek(fp, 0, SEEK_SET);

    std::vector<std::uint8_t> data(file_size);

    if(data.empty() || std::fread(data.data(), file_size, 1, fp) != 1)
        return {};

    return data;
}

using attr_bitmaps_t = std::array<wxBitmap, 4>;

inline std::vector<attr_bitmaps_t> chr_to_bitmaps(std::uint8_t const* data, std::size_t size, std::uint8_t const* palette)
{
    std::vector<attr_bitmaps_t> ret;

    size = std::min<std::size_t>(size, 16*256);

    for(unsigned i = 0; i < size; i += 16)
    {
        std::array<std::array<rgb_t, 8*8>, 4> rgb;

        std::uint8_t const* plane0 = data + i;
        std::uint8_t const* plane1 = data + i + 8;

        for(unsigned y = 0; y < 8; ++y)
        for(unsigned x = 0; x < 8; ++x)
        {
            unsigned const rx = 7 - x;
            unsigned const entry = ((plane0[y] >> rx) & 1) | (((plane1[y] >> rx) << 1) & 0b10);
            assert(entry < 4);

            for(unsigned j = 0; j < 4; ++j)
            {
                std::uint8_t const color = palette[entry + (j*4)] % 64;
                rgb[j][y*8+x] = nes_colors[color];
            }
        }

        ret.push_back({{
            wxImage(8, 8, reinterpret_cast<unsigned char*>(rgb[0].data()), true),
            wxImage(8, 8, reinterpret_cast<unsigned char*>(rgb[1].data()), true),
            wxImage(8, 8, reinterpret_cast<unsigned char*>(rgb[2].data()), true),
            wxImage(8, 8, reinterpret_cast<unsigned char*>(rgb[3].data()), true),
        }});
    }

    return ret;
}

inline std::vector<wxBitmap> load_collision_file(wxString const& string)
{
    std::vector<wxBitmap> ret;
    wxImage base(string);

    for(coord_t c : dimen_range({8, 8}))
    {
        //wxImage tile = base.Copy();
        wxImage tile(string);
        tile.Resize({ 16, 16 }, { c.x * -16, c.y * -16 }, 255, 0, 255);
        ret.emplace_back(tile);
    }

    return ret;
}

#endif
