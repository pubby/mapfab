#ifndef MODEL_HPP
#define MODEL_HPP

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <variant>
#include <set>
#include <filesystem>
#include <vector>

#include "2d/geometry.hpp"
#include "2d/grid.hpp"

#include "convert.hpp"
#include "tool.hpp"

using namespace i2d;
namespace bc = ::boost::container;

class tile_layer_t;
class metatile_layer_t;
class level_model_t;
struct object_t;

using palette_array_t = std::array<std::uint8_t, 16>;
using chr_array_t = std::array<std::uint8_t, 16*256>;

constexpr std::uint8_t ACTIVE_COLLISION = 4;
static constexpr std::size_t UNDO_LIMIT = 256;

struct object_t
{
    coord_t position;
    std::string name;
    std::string oclass;
    std::unordered_map<std::string, std::string> fields;

    void append_vec(std::vector<std::uint16_t>& vec) const;
    void from_vec(std::uint16_t const*& ptr, std::uint16_t const* end);
    auto operator<=>(object_t const&) const = default;
};

enum undo_type_t { UNDO, REDO };

struct undo_tiles_t
{
    tile_layer_t* layer;
    rect_t rect;
    std::vector<std::uint16_t> tiles;
};

struct undo_palette_num_t
{
    int num;
};

struct undo_level_dimen_t
{
    metatile_layer_t* layer;
    grid_t<std::uint8_t> tiles;
};

struct undo_new_object_t
{
    level_model_t* level;
    std::deque<unsigned> indices;
};

struct undo_delete_object_t
{
    level_model_t* level;
    std::deque<std::pair<unsigned, object_t>> objects;
};

struct undo_edit_object_t
{
    level_model_t* level;
    unsigned index;
    object_t object;
};

using undo_t = std::variant
    < std::monostate
    , undo_tiles_t
    , undo_palette_num_t
    , undo_level_dimen_t
    , undo_new_object_t
    , undo_delete_object_t
    , undo_edit_object_t
    >;

// Used to select and deselect specific tiles:
class select_map_t
{
public:
    select_map_t() = default;
    select_map_t(dimen_t dimen) { resize(dimen); }
    dimen_t dimen() const { return m_selection.dimen(); }

    bool has_selection() const { return (bool)m_select_rect; }
    rect_t select_rect() const { return m_select_rect; }
    auto const& selection() const { return m_selection; }
    auto const& operator[](coord_t c) const { return m_selection.at(c); }

    void select_all(bool select = true);
    void select(std::uint8_t tile, bool select_ = true);
    void select_transpose(std::uint8_t tile, bool select_ = true);
    void select(coord_t c, bool select = true);
    void select(rect_t r, bool select = true);

    virtual void resize(dimen_t d) ;

    template<typename Fn>
    void for_each_selected(Fn const& fn)
    {
        for(coord_t c : rect_range(select_rect()))
            if(m_selection[c])
                fn(c);
    }

private:
    void recalc_select_rect(rect_t range);

    rect_t m_select_rect = {};
    grid_t<std::uint8_t> m_selection;
};

enum
{
    LAYER_COLOR,
    LAYER_CHR,
    LAYER_COLLISION,
    LAYER_METATILE,
    LAYER_OBJECTS,
};

struct tile_copy_t
{
    unsigned format;
    std::variant<grid_t<std::uint16_t>, std::vector<object_t>> data;

    std::vector<std::uint16_t> to_vec() const
    {
        if(auto* grid = std::get_if<grid_t<std::uint16_t>>(&data))
        {
            assert(format != LAYER_OBJECTS);
            std::vector<std::uint16_t> vec = { format, grid->dimen().w, grid->dimen().h };
            vec.insert(vec.end(), grid->begin(), grid->end());
            return vec;
        }
        else if(auto* objects = std::get_if<std::vector<object_t>>(&data))
        {
            assert(format == LAYER_OBJECTS);
            std::vector<std::uint16_t> vec = { format, objects->size() };
            for(auto const& object : *objects)
                object.append_vec(vec);
            return vec;
        }

        throw std::runtime_error("Unable to convert clip data to vec.");
    }

    static tile_copy_t from_vec(std::vector<std::uint16_t> const& vec)
    {
        tile_copy_t ret = { vec.at(0) };
        if(ret.format == LAYER_OBJECTS)
        {
            unsigned const size = vec.at(1);
            std::vector<object_t> objects;
            std::uint16_t const* ptr = vec.data() + 2;
            for(unsigned i = 0; i < size; ++i)
                objects.emplace_back().from_vec(ptr, &*vec.end());
            ret.data = std::move(objects);
        }
        else
        {
            grid_t<std::uint16_t> tiles;
            tiles.resize({ vec.at(1), vec.at(2) });
            unsigned i = 3;
            for(coord_t c : dimen_range(tiles.dimen()))
                tiles[c] = vec.at(i++);
            ret.data = std::move(tiles);
        }
        return ret;
    }
};

struct object_copy_t
{
    std::vector<object_t> objects;
};

class tile_layer_t
{
public:
    tile_layer_t(dimen_t picker_dimen, dimen_t canvas_dimen)
    : picker_selector(picker_dimen)
    , canvas_selector(canvas_dimen)
    , tiles(canvas_dimen)
    {}

    virtual unsigned format() const = 0;
    virtual dimen_t tile_size() const { return { 8, 8 }; }
    virtual dimen_t canvas_dimen() const { return tiles.dimen(); }
    virtual void canvas_resize(dimen_t d) { canvas_selector.resize(d); tiles.resize(d); }
    virtual std::uint16_t get(coord_t c) const { return tiles.at(c); }
    virtual void set(coord_t c, std::uint16_t value) { tiles.at(c) = value; }
    virtual void reset(coord_t c) { set(c, 0); }
    virtual std::uint16_t to_tile(coord_t pick) const { return pick.x + pick.y * picker_selector.dimen().w; }
    virtual coord_t to_pick(std::uint8_t tile) const { return { tile % picker_selector.dimen().w, tile / picker_selector.dimen().w }; }

    virtual tile_copy_t copy(bool cut = false);
    virtual void paste(tile_copy_t const& copy, coord_t at);

    virtual undo_t fill();
    virtual undo_t fill_paste(tile_copy_t const& copy);

    virtual void dropper(coord_t at);

    virtual undo_t save(coord_t at) { return save({ at, picker_selector.dimen() }); }
    virtual undo_t save(rect_t rect);

    template<typename Fn>
    void for_each_picked(coord_t pen_c, Fn const& fn)
    {
        rect_t const select_rect = picker_selector.select_rect();
        picker_selector.for_each_selected([&](coord_t c)
        {
            std::uint16_t const tile = to_tile(c);
            coord_t const at = pen_c + c - select_rect.c;
            if(in_bounds(at, canvas_dimen()))
                fn(at, tile);
        });
    }

    select_map_t picker_selector;
    select_map_t canvas_selector;
    grid_t<std::uint8_t> tiles;
};

class tile_model_t
{
public:
    virtual tile_layer_t& layer() = 0;
    tile_layer_t const& clayer() const { return const_cast<tile_model_t*>(this)->layer(); }

};

////////////////////////////////////////////////////////////////////////////////
// chr /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct chr_file_t
{
    std::string name;
    std::filesystem::path path;
    chr_array_t chr = {};

    void load();
};

////////////////////////////////////////////////////////////////////////////////
// color palette ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

class color_layer_t : public tile_layer_t
{
public:
    explicit color_layer_t(std::uint16_t& num) 
    : tile_layer_t({ 4, 16 }, { 25, 256 }) 
    , num(num)
    { tiles.fill(0x0F); }

    virtual unsigned format() const override { return LAYER_COLOR; }
    virtual dimen_t tile_size() const override { return { 16, 16 }; }
    virtual dimen_t canvas_dimen() const override { return { tiles.dimen().w, num }; }
    virtual void reset(coord_t c) override { set(c, 0x0F); }
    virtual std::uint16_t to_tile(coord_t pick) const override { return pick.y + pick.x * picker_selector.dimen().h; }
    virtual coord_t to_pick(std::uint8_t tile) const override { return { tile / picker_selector.dimen().h, tile % picker_selector.dimen().h }; }

    std::uint16_t const& num;
};

class palette_model_t : public tile_model_t
{
public:
    virtual tile_layer_t& layer() override { return color_layer; }

    std::uint16_t num = 1;
    color_layer_t color_layer = color_layer_t(this->num);
};

////////////////////////////////////////////////////////////////////////////////
// metatiles ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

class chr_layer_t : public tile_layer_t
{
public:
    explicit chr_layer_t(std::uint8_t const& active) 
    : tile_layer_t({ 16, 16 }, { 32, 32 })
    , active(active)
    , attributes({ 16, 16 })
    {}

    virtual unsigned format() const override { return LAYER_CHR; }
    virtual std::uint16_t get(coord_t c) const override { return tiles.at(c) | attributes.at(vec_div(c, 2)) << 8; }
    virtual void set(coord_t c, std::uint16_t value) override { tiles.at(c) = value; attributes.at(vec_div(c, 2)) = value >> 8; }
    virtual std::uint16_t to_tile(coord_t pick) const { return tile_layer_t::to_tile(pick) | (active << 8); }

    undo_t fill_attribute();

    std::uint8_t const& active;
    grid_t<std::uint8_t> attributes;
};

class collision_layer_t : public tile_layer_t
{
public:
    collision_layer_t() : tile_layer_t({ 8, 8 }, { 16, 16 }) {}

    virtual unsigned format() const override { return LAYER_COLLISION; }
    virtual dimen_t tile_size() const { return { 16, 16 }; }
};

class metatile_model_t : public tile_model_t
{
public:
    bool collisions() const { return active == ACTIVE_COLLISION; }
    virtual tile_layer_t& layer() override { if(collisions()) return collision_layer; else return chr_layer; }

    void clear_chr();
    void refresh_chr(chr_array_t const& chr, palette_array_t const& palette);

    std::string name = "metatiles";
    std::string chr_name;
    std::uint16_t num = 1;
    std::uint8_t active = 0;
    std::uint8_t palette = 0;
    std::vector<attr_bitmaps_t> chr_bitmaps;

    chr_layer_t chr_layer = chr_layer_t(this->active);
    collision_layer_t collision_layer;
};

////////////////////////////////////////////////////////////////////////////////
// levels //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct class_field_t
{
    std::string type = "U";
    std::string name;
};


struct object_class_t
{
    std::string name;
    rgb_t color = { 255, 255, 255 };
    std::deque<class_field_t> fields;
};

class metatile_layer_t : public tile_layer_t
{
public:
    metatile_layer_t() : tile_layer_t({ 16, 16 }, { 16, 15 }) {}

    virtual unsigned format() const override { return LAYER_METATILE; }
    virtual dimen_t tile_size() const { return { 16, 16 }; }

    undo_t save() { return undo_level_dimen_t{ this, tiles }; }
};

enum level_layer_t
{
    TILE_LAYER = 0,
    OBJECT_LAYER,
};


class level_model_t : public tile_model_t
{
public:
    virtual tile_layer_t& layer() override { return metatile_layer; }
    dimen_t dimen() const { return metatile_layer.tiles.dimen(); }
    void resize(dimen_t dimen) 
    {
        metatile_layer.tiles.resize(dimen);
        metatile_layer.canvas_selector.resize(dimen);
    }

    void clear_metatiles();
    void refresh_metatiles(metatile_model_t const& metatiles, chr_array_t const& chr, palette_array_t const& palette);

    void reindex_objects();

    std::string name = "level";
    std::string macro_name;
    std::string metatiles_name;
    std::string chr_name;
    std::uint8_t palette = 0;
    metatile_layer_t metatile_layer;
    std::vector<wxBitmap> metatile_bitmaps;
    level_layer_t current_layer = TILE_LAYER;

    std::set<int> object_selector;
    std::deque<object_t> objects;
};

////////////////////////////////////////////////////////////////////////////////
// model ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct model_t
{
    model_t()
    {
        chr_files.push_back({ "chr" });
        object_classes.push_back(std::make_shared<object_class_t>("object"));
        metatiles.emplace_back(std::make_shared<metatile_model_t>());
        levels.emplace_back(std::make_shared<level_model_t>());
    }

    bool modified = false;
    bool modified_since_save = false;
    void modify() { modified = modified_since_save = true; }

    std::filesystem::path project_path;

    tool_t tool = {};
    std::unique_ptr<tile_copy_t> paste; 

    palette_model_t palette;
    std::deque<std::shared_ptr<metatile_model_t>> metatiles;
    std::deque<std::shared_ptr<level_model_t>> levels;

    std::deque<std::shared_ptr<object_class_t>> object_classes;
    object_t object_picker = {};

    std::deque<chr_file_t> chr_files;

    std::filesystem::path collision_path;
    std::vector<wxBitmap> collision_bitmaps;

    palette_array_t palette_array(unsigned palette_index = 0);

    // Undo operations:
    undo_t undo(undo_t const& undo) { modify(); return std::visit(*this, undo); }
    undo_t operator()(std::monostate const& m) { return m; }
    undo_t operator()(undo_tiles_t const& undo);
    undo_t operator()(undo_palette_num_t const& undo);
    undo_t operator()(undo_level_dimen_t const& undo);
    undo_t operator()(undo_new_object_t const& undo);
    undo_t operator()(undo_delete_object_t const& undo);
    undo_t operator()(undo_edit_object_t const& undo);

    void write_file(FILE* fp, std::filesystem::path base_path) const;
    void read_file(FILE* fp, std::filesystem::path base_path);
};

struct undo_history_t
{
    std::deque<undo_t> history[2];

    template<undo_type_t U>
    void undo(model_t& model) 
    { 
        if(history[U].empty())
            return;
        history[!U].push_front(model.undo(history[U].front()));
        history[U].pop_front();
    }

    void cull(); 
    void push(undo_t const& undo); 
    bool empty(undo_type_t U) const { return history[U].empty(); }

    template<typename T>
    bool on_top() const
    {
        if(history[UNDO].empty())
            return false;
        return std::holds_alternative<T>(history[UNDO].front());
    }
};

template<typename C>
typename C::value_type* lookup_name(std::string const& name, C& c)
{
    for(auto& e : c)
        if(e.name == name)
            return &e;
    return nullptr;
}

template<typename C>
typename C::value_type lookup_name_ptr(std::string const& name, C& c)
{
    for(auto& e : c)
        if(e->name == name)
            return e;
    return typename C::value_type();
}

#endif
