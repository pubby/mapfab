#include "convert.hpp"

#include "lodepng/lodepng.h"

std::vector<std::uint8_t> read_binary_file(char const* filename)
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

std::vector<attr_bitmaps_t> chr_to_bitmaps(std::uint8_t const* data, std::size_t size, std::uint8_t const* palette)
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

std::vector<wxBitmap> load_collision_file(wxString const& string)
{
    std::vector<wxBitmap> ret;
    wxImage base(string);

    for(coord_t c : dimen_range({8, 8}))
    {
        wxImage tile = base.Copy();
        //wxImage tile(string);
        tile.Resize({ 16, 16 }, { c.x * -16, c.y * -16 }, 255, 0, 255);
        ret.emplace_back(tile);
    }

    return ret;
}

static std::uint8_t map_grey_alpha(std::uint8_t grey, std::uint8_t alpha)
{
    return (grey * (alpha + 1)) >> (6 + 8);
}

std::vector<std::uint8_t> png_to_chr(std::uint8_t const* png, std::size_t size, bool chr16)
{
    unsigned width, height;
    std::vector<std::uint8_t> image; //the raw pixels
    lodepng::State state;
    unsigned error;

    if((error = lodepng_inspect(&width, &height, &state, png, size)))
        goto fail;

    if(width % 8 != 0)
        throw std::runtime_error("Image width is not a multiple of 8.");
    if(chr16 && height % 16 != 0)
        throw std::runtime_error("Image height is not a multiple of 16.");
    else if(!chr16 && height % 8 != 0)
        throw std::runtime_error("Image height is not a multiple of 8.");

    switch(state.info_png.color.colortype)
    {
    case LCT_PALETTE:
        state.info_raw.colortype = LCT_PALETTE;
        if((error = lodepng::decode(image, width, height, state, png, size)))
            goto fail;
        for(std::uint8_t& c : image)
            c &= 0b11;
        break;

    case LCT_GREY:
    case LCT_RGB:
        state.info_raw.colortype = LCT_GREY;
        if((error = lodepng::decode(image, width, height, state, png, size)))
            goto fail;
        for(std::uint8_t& c : image)
            c >>= 6;
        break;

    default:
        state.info_raw.colortype = LCT_GREY_ALPHA;
        if((error = lodepng::decode(image, width, height, state, png, size)))
            goto fail;
        assert(image.size() == width * height * 2);
        unsigned const n = image.size() / 2;
        for(unsigned i = 0; i < n; ++i)
            image[i] = map_grey_alpha(image[i*2], image[i*2 + 1]);
        image.resize(n);
        break;
    }

    // Now convert to CHR
    {
        std::vector<std::uint8_t> result;
        result.resize(image.size() / 4);

        unsigned i = 0;

        if(chr16)
        {
            for(unsigned ty = 0; ty < height; ty += 16)
            for(unsigned tx = 0; tx < width; tx += 8)
            {
                for(unsigned y = 0; y < 8; ++y, ++i)
                for(unsigned x = 0; x < 8; ++x)
                    result[i] |= (image[tx + x + (ty + y)*width] & 1) << (7-x);

                for(unsigned y = 0; y < 8; ++y, ++i)
                for(unsigned x = 0; x < 8; ++x)
                    result[i] |= (image[tx + x + (ty + y)*width] >> 1) << (7-x);

                for(unsigned y = 8; y < 16; ++y, ++i)
                for(unsigned x = 0; x < 8; ++x)
                    result[i] |= (image[tx + x + (ty + y)*width] & 1) << (7-x);

                for(unsigned y = 8; y < 16; ++y, ++i)
                for(unsigned x = 0; x < 8; ++x)
                    result[i] |= (image[tx + x + (ty + y)*width] >> 1) << (7-x);
            }
        }
        else
        {
            for(unsigned ty = 0; ty < height; ty += 8)
            for(unsigned tx = 0; tx < width; tx += 8)
            {
                for(unsigned y = 0; y < 8; ++y, ++i)
                for(unsigned x = 0; x < 8; ++x)
                    result[i] |= (image[tx + x + (ty + y)*width] & 1) << (7-x);

                for(unsigned y = 0; y < 8; ++y, ++i)
                for(unsigned x = 0; x < 8; ++x)
                    result[i] |= (image[tx + x + (ty + y)*width] >> 1) << (7-x);
            }
        }

        assert(i == result.size());

        return result;
    }
fail:
    throw std::runtime_error(std::string("png decoder error: ") + lodepng_error_text(error));
}

