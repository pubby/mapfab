#ifndef MODEL_HPP
#define MODEL_HPP

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <variant>
#include <unordered_set>

#include <boost/container/small_vector.hpp>

#include "2d/geometry.hpp"
#include "2d/grid.hpp"

#include "convert.hpp"
#include "tool.hpp"

using namespace i2d;
namespace bc = ::boost::container;

class tile_layer_t;
class metatile_layer_t;
class level_model_t;

constexpr std::uint8_t ACTIVE_COLLISION = 4;
static constexpr std::size_t UNDO_LIMIT = 256;

enum undo_type_t { UNDO, REDO };

struct undo_tiles_t
{
    tile_layer_t* layer;
    rect_t rect;
    bc::small_vector<std::uint16_t, 4> tiles;
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

struct undo_level_palette_t
{
    level_model_t* model;
    int palette;
};

using undo_t = std::variant
    < std::monostate
    , undo_tiles_t
    , undo_palette_num_t
    , undo_level_dimen_t
    , undo_level_palette_t
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

struct tile_copy_t
{
    unsigned format;
    grid_t<std::uint16_t> tiles;

    std::vector<std::uint16_t> to_vec() const
    {
        std::vector<std::uint16_t> vec = { format, tiles.dimen().w, tiles.dimen().h };
        vec.insert(vec.end(), tiles.begin(), tiles.end());
        return vec;
    }

    static tile_copy_t from_vec(std::vector<std::uint16_t> const& vec)
    {
        tile_copy_t ret = { vec.at(0) };
        ret.tiles.resize({ vec.at(1), vec.at(2) });
        unsigned i = 3;
        for(coord_t c : dimen_range(ret.tiles.dimen()))
            ret.tiles[c] = vec.at(i++);
        return ret;
    }
};

enum
{
    LAYER_COLOR,
    LAYER_CHR,
    LAYER_COLLISION,
    LAYER_METATILE,
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
    , attributes({ 16, 16 })
    , active(active)
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

    std::uint16_t num = 1;
    std::uint8_t active = 0;
    std::uint8_t palette = 0;

    chr_layer_t chr_layer = chr_layer_t(this->active);
    collision_layer_t collision_layer;
};

////////////////////////////////////////////////////////////////////////////////
// levels //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/* TODO
struct object_field_t
{
    std::string name;
    std::string value;
};
*/

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

struct object_t
{
    int index;
    coord_t position;
    std::string name;
    std::string oclass;
    std::unordered_map<std::string, std::string> fields;
};

class metatile_layer_t : public tile_layer_t
{
public:
    metatile_layer_t() : tile_layer_t({ 16, 16 }, { 16, 15 }) {}

    virtual unsigned format() const override { return LAYER_METATILE; }
    virtual dimen_t tile_size() const { return { 16, 16 }; }

    undo_t save() { return undo_level_dimen_t{ this, tiles }; }
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

    std::string name = "level_0";
    std::uint8_t palette = 0;
    metatile_layer_t metatile_layer;

    std::unordered_set<int> object_selector;
    std::deque<object_t> objects = { object_t{ 0, coord_t{ 64, 32 } }};
};

////////////////////////////////////////////////////////////////////////////////
// model ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct model_t
{
    model_t()
    {
        levels.emplace_back(std::make_shared<level_model_t>());
    }

    bool modified = false;
    bool modified_since_save = false;
    void modify() { modified = modified_since_save = true; }

    tool_t tool = {};
    std::unique_ptr<tile_copy_t> paste; 

    std::array<std::uint8_t, 16*256> chr = {};
    palette_model_t palette;
    metatile_model_t metatiles;
    std::deque<std::shared_ptr<level_model_t>> levels;

    std::deque<std::shared_ptr<object_class_t>> object_classes;
    object_t object_picker = {};

    std::vector<attr_bitmaps_t> chr_bitmaps;
    std::vector<wxBitmap> collision_bitmaps;
    std::vector<wxBitmap> metatile_bitmaps;

    std::array<std::uint8_t, 16> active_palette_array();

    void refresh_chr();
    void refresh_metatiles();

    // Undo operations:
    undo_t undo(undo_t const& undo) { modify(); return std::visit(*this, undo); }
    undo_t operator()(std::monostate const& m) { return m; }
    undo_t operator()(undo_tiles_t const& undo);
    undo_t operator()(undo_palette_num_t const& undo);
    undo_t operator()(undo_level_dimen_t const& undo);
    undo_t operator()(undo_level_palette_t const& undo);

    void write_file(FILE* fp, std::string const& chr_path, std::string const& collision_path) const;
    void read_file(FILE* fp, std::string& chr_path, std::string& collisions_path);
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

#endif
