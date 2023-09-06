#include "model.hpp"

#include <ranges>

#include "json.hpp"
#include "graphics.hpp"

using json = nlohmann::json;

////////////////////////////////////////////////////////////////////////////////
// object_t ////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void object_t::append_vec(std::vector<std::uint16_t>& vec) const
{
    auto const append_str = [&](std::string const& str)
    {
        for(char c : str)
            vec.push_back(c);
        vec.push_back('\0');
    };

    vec.push_back(position.x);
    vec.push_back(position.y);
    append_str(name);
    append_str(oclass);
    vec.push_back(fields.size());
    for(auto const& field : fields)
    {
        append_str(field.first);
        append_str(field.first);
    }
}

void object_t::from_vec(std::uint16_t const*& ptr, std::uint16_t const* end)
{
    auto const get = [&]() -> std::uint16_t
    {
        if(ptr >= end)
            throw std::runtime_error("Data out of bounds.");
        return *ptr++;
    };

    auto const from_str = [&]() -> std::string
    {
        std::string ret;
        while(char c = get())
            ret.push_back(c);
        return ret;
    };

    position.x = static_cast<std::int16_t>(get());
    position.y = static_cast<std::int16_t>(get());
    name = from_str();
    oclass = from_str();

    unsigned const num_fields = *ptr++;
    fields.clear();
    for(unsigned i = 0; i < num_fields; ++i)
        fields.insert({ from_str(), from_str() });
}

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

tile_copy_t tile_layer_t::copy(undo_t* cut)
{
    rect_t const rect = crop(canvas_selector.select_rect(), canvas_dimen());
    tile_copy_t copy = { format() };
    grid_t<std::uint16_t> tiles;
    tiles.resize(rect.d);
    if(cut)
        *cut = save(rect);

    for(coord_t c : rect_range(rect))
    {
        if(canvas_selector[c])
        {
            tiles[c - rect.c] = get(c);
            if(cut)
                reset(c);
        }
        else
            tiles[c - rect.c] = ~0u;
    }
    copy.data = std::move(tiles);
    return copy;
}

void tile_layer_t::paste(tile_copy_t const& copy, coord_t at)
{
    if(auto* grid = std::get_if<grid_t<std::uint16_t>>(&copy.data))
        for(coord_t c : dimen_range(grid->dimen()))
            if((*grid)[c] != std::uint16_t(~0u) && in_bounds(at + c, canvas_dimen()))
                set(at + c, (*grid)[c]);
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
        coord_t const o = c - canvas_rect.c;
        coord_t const p = coord_t{ o.x % picker_rect.d.w, o.y % picker_rect.d.h } + picker_rect.c;
        set(c, to_tile(p));
    });

    return ret;
}

undo_t tile_layer_t::fill_paste(tile_copy_t const& copy)
{
    if(auto* grid = std::get_if<grid_t<std::uint16_t>>(&copy.data))
    {
        rect_t const canvas_rect = crop(canvas_selector.select_rect(), canvas_dimen());
        dimen_t const copy_dimen = grid->dimen();

        if(!canvas_rect || !copy_dimen)
            return {};

        undo_t ret = save(canvas_rect);

        canvas_selector.for_each_selected([&](coord_t c)
        {
            coord_t const o = c - canvas_rect.c;
            coord_t const p = coord_t{ o.x % copy_dimen.w, o.y % copy_dimen.h };

            if((*grid)[p] != std::uint16_t(~0u) && in_bounds(c, canvas_dimen()))
                set(c, (*grid)[p]);
        });

        return ret;
    }

    return {};
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
// metatile_model_t ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void metatile_model_t::clear_chr()
{
    chr_bitmaps.clear();
}

void metatile_model_t::refresh_chr(chr_array_t const& chr, palette_array_t const& palette)
{
    auto bmp = chr_to_bitmaps(chr.data(), chr.size(), palette.data());
    chr_bitmaps.clear();
    chr_bitmaps.reserve(bmp.size());
    for(unsigned i = 0; i < bmp.size(); ++i)
        chr_bitmaps.push_back(convert_bitmap(bmp[i]));
}

////////////////////////////////////////////////////////////////////////////////
// level_model_t ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void level_model_t::clear_metatiles()
{
    metatile_bitmaps.clear();
}

void level_model_t::refresh_metatiles(
    metatile_model_t const& metatiles, chr_array_t const& chr, 
    std::vector<wxBitmap> const* collision_bitmaps, palette_array_t const& palette)
{
    auto chr_bitmaps = chr_to_bitmaps(chr.data(), chr.size(), palette.data());

    unsigned i = 0;
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

            if(collision_bitmaps)
            {
                unsigned const tile = metatiles.collision_layer.tiles.at(c);
                if(tile < collision_bitmaps->size())
                    dc.DrawBitmap((*collision_bitmaps)[tile], { 0, 0 }, false);
            }

            if(i >= metatiles.num)
            {
                dc.SetPen(wxPen(wxColor(255, 0, 0), 2, wxPENSTYLE_SOLID));
                dc.DrawLine(1, 1, 14, 14);
                dc.SetPen(wxPen(wxColor(0, 0, 255), 2, wxPENSTYLE_SOLID));
                dc.DrawLine(1, 14, 14, 1);
            }
        }

#ifdef GC_RENDER
        metatile_bitmaps.push_back(get_renderer()->CreateBitmap(std::move(bitmap)));
#else
        metatile_bitmaps.push_back(std::move(bitmap));
#endif
        ++i;
    }
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

undo_t model_t::operator()(undo_new_object_t const& undo)
{
    auto ret = undo_delete_object_t{ undo.level };
    for(unsigned index : undo.indices | std::views::reverse)
        ret.objects.emplace_back(index, undo.level->objects.at(index));
    for(unsigned index : undo.indices)
        undo.level->objects.erase(undo.level->objects.begin() + index);
    return ret;
}

undo_t model_t::operator()(undo_delete_object_t const& undo)
{
    auto ret = undo_new_object_t{ undo.level };
    for(auto const& pair : undo.objects | std::views::reverse)
        ret.indices.push_back(pair.first);
    for(auto const& pair : undo.objects)
        undo.level->objects.insert(undo.level->objects.begin() + pair.first, pair.second);
    return ret;
}

undo_t model_t::operator()(undo_edit_object_t const& undo)
{
    auto ret = undo_edit_object_t{ undo.level, undo.index, undo.level->objects.at(undo.index) };
    undo.level->objects.at(undo.index) = undo.object;
    return ret;
}

palette_array_t model_t::palette_array(unsigned palette_index)
{
    std::array<std::uint8_t, 16> ret;
    for(unsigned i = 0; i < 4; ++i)
    {
        ret[i*4] = palette.color_layer.tiles.at({ 24, palette_index });
        for(unsigned j = 0; j < 3; ++j)
            ret[i*4+j+1] = palette.color_layer.tiles.at({ i*3 + j, palette_index });
    }
    return ret;
}

constexpr std::uint8_t SAVE_VERSION = 1;

void model_t::write_file(FILE* fp, std::filesystem::path base_path) const
{
    base_path.remove_filename();

    auto const write_str = [&](std::string const& str)
    {
        if(!str.empty())
            std::fwrite(str.c_str(), str.size(), 1, fp);
        std::fputc(0, fp);
    };

    auto const write8 = [&](std::uint8_t i)
    {
        std::fputc(i & 0xFF, fp);
    };

    auto const write16 = [&](std::uint16_t i)
    {
        std::fputc(i & 0xFF, fp); // Lo
        std::fputc((i >> 8) & 0xFF, fp); // Hi
    };

    std::fwrite("MapFab", 7, 1, fp);

    // Version:
    write8(SAVE_VERSION);

    // Collision file:
    write_str(std::filesystem::proximate(collision_path, base_path).generic_string());

    // CHR:
    write8(chr_files.size() & 0xFF);
    for(auto const& file : chr_files)
    {
        write_str(file.name);
        write_str(std::filesystem::proximate(file.path, base_path).generic_string());
    }

    // Palettes:
    write8(palette.color_layer.num & 0xFF);
    for(std::uint8_t data : palette.color_layer.tiles)
        write8(data);

    // Metatiles:
    write8(metatiles.size() & 0xFF);
    for(auto const& mt : metatiles)
    {
        write_str(mt->name.c_str());
        write_str(mt->chr_name.c_str());
        write8(mt->palette & 0xFF);
        write8(mt->num & 0xFF);
        for(std::uint8_t data : mt->chr_layer.tiles)
            write8(data);
        for(std::uint8_t data : mt->chr_layer.attributes)
            write8(data);
        for(std::uint8_t data : mt->collision_layer.tiles)
            write8(data);
    }

    // Object classes:
    write8(object_classes.size() & 0xFF);
    for(auto const& oc : object_classes)
    {
        write_str(oc->name.c_str());
        write8(oc->color.r & 0xFF);
        write8(oc->color.g & 0xFF);
        write8(oc->color.b & 0xFF);
        write8(oc->fields.size() & 0xFF);
        for(auto const& field : oc->fields)
        {
            write_str(field.name.c_str());
            write_str(field.type.c_str());
        }
    }

    // Levels:
    write8(levels.size() & 0xFF);
    for(auto const& level : levels)
    {
        write_str(level->name.c_str());
        write_str(level->macro_name.c_str());
        write_str(level->chr_name.c_str());
        write8(level->palette & 0xFF);
        write_str(level->metatiles_name.c_str());
        write8(level->dimen().w & 0xFF);
        write8(level->dimen().h & 0xFF);
        for(std::uint8_t data : level->metatile_layer.tiles)
            write8(data);
        write16(level->objects.size());
        for(auto const& obj : level->objects)
        {
            write_str(obj.name.c_str());
            write_str(obj.oclass.c_str());
            write16(obj.position.x);
            write16(obj.position.y);
            for(auto const& oc : object_classes)
            {
                if(oc->name == obj.oclass)
                {
                    for(auto const& field : oc->fields)
                    {
                        auto it = obj.fields.find(field.name);
                        if(it != obj.fields.end())
                            write_str(it->second);
                        else
                            write8(0);
                    }
                    break;
                }
            }
        }
    }
}

void model_t::read_file(FILE* fp, std::filesystem::path base_path)
{
    base_path.remove_filename();

    auto const get8 = [&](bool adjust = false) -> unsigned
    {
        int const got = std::getc(fp);
        if(got == EOF)
            throw std::runtime_error("Unable to read 8-bit value.");
        if(adjust && got == 0)
            return 256;
        return got;
    };

    auto const get16 = [&]() -> std::uint16_t
    {
        int const lo = std::getc(fp);
        int const hi = std::getc(fp);
        if(lo == EOF || hi == EOF)
            throw std::runtime_error("Unable to read 16-bit value.");
        return (lo & 0xFF) | ((hi & 0xFF) << 8);
    };

    auto const get_str = [&]() -> std::string
    {
        std::string ret;
        while(char c = get8())
            ret.push_back(c);
        return ret;
    };

    auto const get_path = [&]() -> std::filesystem::path
    {
        std::filesystem::path path(get_str(), std::filesystem::path::generic_format);
        path.make_preferred();

        if(!path.empty() && path.is_relative())
            path = base_path / path;

        return path;
    };

    char buffer[8];
    if(!std::fread(buffer, 8, 1, fp))
        throw std::runtime_error("Unable to read magic number.");
    if(memcmp(buffer, "MapFab", 7) != 0)
        throw std::runtime_error("Incorrect magic number.");
    if(buffer[7] > SAVE_VERSION)
        throw std::runtime_error("File is from a newer version of MapFab.");

    // Collision file:
    collision_path = get_path();
    auto collisions = load_collision_file(collision_path.string());
    collision_bitmaps = std::move(collisions.first);
    collision_wx_bitmaps = std::move(collisions.second);

    // CHR:
    unsigned const num_chr = get8(true);
    chr_files.clear();
    for(unsigned i = 0; i < num_chr; ++i)
    {
        auto& chr = chr_files.emplace_back();
        chr.name = get_str();
        chr.path = get_path();
        chr.load();
    }

    // Palettes:
    palette.num = get8(true);
    for(std::uint8_t& data : palette.color_layer.tiles)
        data = get8();

    // Metatiles:
    unsigned const num_mt = get8(true);
    metatiles.clear();
    for(unsigned i = 0; i < num_mt; ++i)
    {
        auto& mt = *metatiles.emplace_back(std::make_shared<metatile_model_t>());
        mt.name = get_str();
        mt.chr_name = get_str();
        mt.palette = get8();
        mt.num = get8(true);
        assert(mt.chr_layer.tiles.size() == 32 * 32);
        for(std::uint8_t& data : mt.chr_layer.tiles)
            data = get8();
        for(std::uint8_t& data : mt.chr_layer.attributes)
            data = get8();
        for(std::uint8_t& data : mt.collision_layer.tiles)
            data = get8();
    }

    // Object classes:
    unsigned const num_oc = get8(true);
    object_classes.clear();
    for(unsigned i = 0; i < num_oc; ++i)
    {
        auto& oc = *object_classes.emplace_back(std::make_shared<object_class_t>());

        oc.name = get_str();
        oc.color.r = get8();
        oc.color.g = get8();
        oc.color.b = get8();
        unsigned const num_fields = get8();
        oc.fields.clear();
        for(unsigned i = 0; i < num_fields; ++i)
        {
            auto& field = oc.fields.emplace_back();
            field.name = get_str();
            field.type = get_str();
        }
    }

    // Levels:
    unsigned const num_levels = get8(true);
    levels.clear();
    for(unsigned i = 0; i < num_levels; ++i)
    {
        auto& level = *levels.emplace_back(std::make_shared<level_model_t>());
        level.name = get_str();
        level.macro_name = get_str();
        level.chr_name = get_str();
        level.palette = get8();
        level.metatiles_name = get_str();
        level.resize({get8(true), get8(true) });
        for(std::uint8_t& data : level.metatile_layer.tiles)
            data = get8();
        unsigned const num_objects = get16();
        for(unsigned i = 0; i < num_objects; ++i)
        {
            auto& obj = level.objects.emplace_back();

            obj.name = get_str();
            obj.oclass = get_str();
            obj.position.x = static_cast<std::int16_t>(get16());
            obj.position.y = static_cast<std::int16_t>(get16());

            for(auto const& oc : object_classes)
            {
                if(oc->name == obj.oclass)
                {
                    for(auto const& field : oc->fields)
                        obj.fields.emplace(field.name, get_str());
                    break;
                }
            }
        }
    }

    modified = modified_since_save = false;
}

void model_t::write_json(FILE* fp, std::filesystem::path base_path) const
{
    base_path.remove_filename();

    json data;

    data["version"] = SAVE_VERSION;

    // Collision file:
    data["collision_path"] = std::filesystem::proximate(collision_path, base_path).generic_string();

    // CHR:
    {
        json::array_t chr;
        for(auto const& file : chr_files)
        {
            std::string path = std::filesystem::proximate(file.path, base_path).generic_string();
            chr.push_back(json::object({{"name", file.name}, {"path",  std::move(path) } }));
        }
        data["chr"] = std::move(chr);
    }

    // Palettes:
    {
        json::array_t palettes;
        for(std::uint8_t data : palette.color_layer.tiles)
            palettes.push_back(data);

        data["palettes"] = json::object({
            {"num", palette.num },
            {"data", std::move(palettes) },
        });
    }

    // Metatiles:
    {
        json::array_t mt_sets;

        for(auto const& mt : metatiles)
        {
            json::array_t tiles;
            json::array_t attributes;
            json::array_t collisions;

            for(std::uint8_t data : mt->chr_layer.tiles)
                tiles.push_back(data);
            for(std::uint8_t data : mt->chr_layer.attributes)
                attributes.push_back(data);
            for(std::uint8_t data : mt->collision_layer.tiles)
                collisions.push_back(data);

            mt_sets.push_back(json::object({
                {"name", mt->name},
                {"chr", mt->chr_name},
                {"palette", mt->palette},
                {"num", mt->num},
                {"tiles", std::move(tiles)},
                {"attributes", std::move(attributes)},
                {"collisions", std::move(collisions)},
            }));
        }

        data["metatile_sets"] = std::move(mt_sets);
    }

    // Object classes:
    {
        json::array_t ocs;

        for(auto const& oc : object_classes)
        {
            json::array_t fields;

            for(auto const& field : oc->fields)
            {
                fields.push_back(json::object({
                    {"name", field.name},
                    {"type", field.type},
                }));
            }

            ocs.push_back(json::object({
                {"name", oc->name},
                {"color", json::array({ oc->color.r, oc->color.g, oc->color.b })},
                {"fields", std::move(fields)},
            }));
        }

        data["object_classes"] = std::move(ocs);
    }

    {
        json::array_t levels;

        for(auto const& level : this->levels)
        {
            json::array_t tiles;
            for(std::uint8_t data : level->metatile_layer.tiles)
                tiles.push_back(data);

            json::array_t objects;
            for(auto const& obj : level->objects)
            {
                json::object_t fields;

                for(auto const& oc : object_classes)
                {
                    if(oc->name == obj.oclass)
                    {
                        for(auto const& field : oc->fields)
                        {
                            auto it = obj.fields.find(field.name);
                            if(it != obj.fields.end())
                                fields[field.name] = it->second;
                        }
                        break;
                    }
                }

                objects.push_back(json::object({
                    {"name", obj.name},
                    {"object_class", obj.oclass},
                    {"fields", std::move(fields)},
                    {"x", obj.position.x},
                    {"y", obj.position.y},
                }));
            }

            levels.push_back(json::object({
                {"name", level->name},
                {"macro", level->macro_name},
                {"chr", level->chr_name},
                {"palette", level->palette},
                {"metatile_set", level->metatiles_name},
                {"width", level->dimen().w},
                {"height", level->dimen().h},
                {"tiles", std::move(tiles)},
                {"objects", std::move(objects)},
            }));
        }

        data["levels"] = std::move(levels);
    }

    std::string str = data.dump(2);
    std::fwrite(str.data(), str.size(), 1, fp);
}

void model_t::read_json(FILE* fp, std::filesystem::path base_path)
{
    auto const convert_path = [&](std::string const& str) -> std::filesystem::path
    {
        std::filesystem::path path(str, std::filesystem::path::generic_format);
        path.make_preferred();

        if(!path.empty() && path.is_relative())
            path = base_path / path;

        return path;
    };

    json data = json::parse(fp);

    base_path.remove_filename();

    if(data.at("version") > SAVE_VERSION)
        throw std::runtime_error("File is from a newer version of MapFab.");

    // Collision file:
    collision_path = convert_path(data.at("collision_path").get<std::string>());
    auto collisions = load_collision_file(collision_path.string());
    collision_bitmaps = std::move(collisions.first);
    collision_wx_bitmaps = std::move(collisions.second);

    // CHR:
    chr_files.clear();
    for(auto const& v : data.at("chr").get<json::array_t>())
    {
        auto& chr = chr_files.emplace_back();
        chr.name = v.at("name").get<std::string>();
        chr.path = convert_path(v.at("path").get<std::string>());
        chr.load();
    }

    // Palettes:
    {
        auto const& array = data.at("palettes").at("data").get<json::array_t>();
        palette.num = data.at("palettes").at("num").get<int>();

        unsigned i = 0;
        for(std::uint8_t& v : palette.color_layer.tiles)
            v = array.at(i++).get<int>();
    }

    // Metatiles:
    {
        metatiles.clear();

        auto const& array = data.at("metatile_sets").get<json::array_t>();
        for(auto const& mt_set : array)
        {
            auto& mt = *metatiles.emplace_back(std::make_shared<metatile_model_t>());
            mt.name = mt_set.at("name").get<std::string>();
            mt.chr_name = mt_set.at("chr").get<std::string>();
            mt.palette = mt_set.at("palette").get<int>();
            mt.num = mt_set.at("num").get<int>();

            auto const& tiles = mt_set.at("tiles").get<json::array_t>();
            auto const& attributes = mt_set.at("attributes").get<json::array_t>();
            auto const& collisions = mt_set.at("collisions").get<json::array_t>();

            unsigned i = 0;
            for(std::uint8_t& v : mt.chr_layer.tiles)
                v = tiles.at(i++).get<int>();

            i = 0;
            for(std::uint8_t& v : mt.chr_layer.attributes)
                v = attributes.at(i++).get<int>();

            i = 0;
            for(std::uint8_t& v : mt.collision_layer.tiles)
                v = collisions.at(i++).get<int>();
        }
    }

    // Object classes:
    {
        object_classes.clear();
        auto const& array = data.at("object_classes").get<json::array_t>();
        for(auto const& o : array)
        {
            auto& oc = *object_classes.emplace_back(std::make_shared<object_class_t>());

            oc.name = o.at("name").get<std::string>();
            oc.color.r = o.at("color").at(0).get<int>();
            oc.color.g = o.at("color").at(1).get<int>();
            oc.color.b = o.at("color").at(2).get<int>();

            auto const& fields = o.at("fields").get<json::array_t>();
            for(auto const& f : fields)
            {
                auto& field = oc.fields.emplace_back();
                field.name = f.at("name").get<std::string>();
                field.type = f.at("type").get<std::string>();
            }
        }
    }

    // Levels:
    {
        levels.clear();
        auto const& array = data.at("levels").get<json::array_t>();
        for(auto const& l : array)
        {
            auto& level = *levels.emplace_back(std::make_shared<level_model_t>());

            level.name = l.at("name").get<std::string>();
            level.macro_name = l.at("macro").get<std::string>();
            level.chr_name = l.at("chr").get<std::string>();
            level.palette = l.at("palette").get<int>();
            level.metatiles_name = l.at("metatile_set").get<std::string>();

            unsigned const w = l.at("width").get<int>();
            unsigned const h = l.at("height").get<int>();
            level.resize({ w, h });

            auto const& tiles = l.at("tiles").get<json::array_t>();
            unsigned i = 0;
            for(std::uint8_t& data : level.metatile_layer.tiles)
                data = tiles.at(i++);

            auto const& objects = l.at("objects").get<json::array_t>();
            for(auto const& o : objects)
            {
                auto& obj = level.objects.emplace_back();

                obj.name = o.at("name").get<std::string>();
                obj.oclass = o.at("object_class").get<std::string>();
                obj.position.x = o.at("x").get<int>();
                obj.position.y = o.at("y").get<int>();

                for(auto const& oc : object_classes)
                {
                    if(oc->name == obj.oclass)
                    {
                        for(auto const& field : oc->fields)
                            obj.fields.emplace(field.name, o.at("fields").at(field.name).get<std::string>());
                        break;
                    }
                }
            }
        }
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

////////////////////////////////////////////////////////////////////////////////
// chr_file_t //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void chr_file_t::load()
{
    chr = {};
    if(path.empty())
        return;
    std::vector<std::uint8_t> data = read_binary_file(path.string().c_str());

    if(data.empty())
        return;

    std::string ext = path.extension().string();
    for(char& c : ext)
        c = std::tolower(c);

    if(ext == ".png")
        data = png_to_chr(data.data(), data.size(), false);

    std::copy_n(data.begin(), std::min(data.size(), chr.size()), chr.begin());
}
