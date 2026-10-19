// A city: the owner's settlement on one island (spec: docs/sim/state.md §4.7,
// stage S6). It owns the warehouse stock, the goods settings, the build-goods
// records, the trade lists and the building index (the six group lists with
// their scheduler cursors and the shared stamp).
//
// City needs the island's derived footprint grids and id map to answer point and
// rect queries, so the queries take the Island (declared in island.hpp, which
// includes this header: an Island holds its cities by value); Island offers the
// same names with a CityNo (§4.8).
#pragma once

#include <cstdint>
#include <vector>

#include <anno/sim/building.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/production.hpp>

namespace anno::sim {

struct Island;

/// D: city +0x334 + 12g: +0x0a bit 0 and +0x0b, written by SetTradeFlag (command
/// 0x7c modes 0/1, 0x1002dbd0: `value` as the byte +0x0b, `mode` as bit 0 of +0x0a)
/// and read back by the getter 0x1002db53 (the byte, sign-extended, when bit 0
/// equals the requested mode, else -1). The STADT2 VK / EK loader stores the loop
/// index of the entry in the same byte (0x1002e997, 0x1002ea7c), the removal of a
/// trade-list entry writes 0xff (0x100285f4, 0x10028655) and the city constructor
/// starts it at 0xff (0x1002fa11): there is ONE byte, the trade-list index, not a
/// separate goods limit.
struct GoodsSetting {
    /// +0x0a bit 0: the number of the trade list the goods is on (the mode of the
    /// setter and of the getter).
    bool buy = false;
    /// +0x0b: the index of the goods in that list, -1 (0xff) for none. A signed byte
    /// in the original; i32 here (F3). The values -2 .. -128 that a setter's `value`
    /// of 0x80 .. 0xfe leaves in the original are not states of the rebuild
    /// (validate_cities: -1 or an index inside the list, O-14).
    std::int32_t list_index = -1;
};

/// D: the 8-byte record at city +0x5d4 + 8k (k = goods - build_goods_first): the
/// setting (command 0x7c mode 9) and the cumulative spend (+0x5d8).
struct BuildGoods {
    std::uint32_t setting = 0;
    std::uint64_t spent = 0;
};

/// I: the fields of command 0x73 at @4 / @6 (O-14); stored only.
struct TradeEntry {
    GoodsId goods = 0;
    std::uint32_t amount_88 = 0;
    std::uint32_t price = 0;
};

struct City {
    /// D [city+4]: the slot in Island::cities and the owner value written to tiles.
    CityNo number;
    /// D: city slot +0x1c -> [city+0x20].
    PlayerId owner;
    /// D: city +0x334, capacity +0x624.
    CityStock stock;
    BuildingIndex index;
    /// D [city+8], set at the end of the city tick.
    std::uint16_t tick_stamp = 0;
    /// Size SimConfig::warehouse_goods.
    std::vector<GoodsSetting> goods;
    /// Size SimConfig::build_goods_count.
    std::vector<BuildGoods> build_goods;
    /// Size SimConfig::trade_lists; the capacity of a list is open (O-14).
    std::vector<std::vector<TradeEntry>> trade_lists;

    // ---- island-level queries (const; Island and the World members of the same
    // names delegate here or are delegated to). All return the canonical order
    // (D2). `layout` is the world's SimConfig::layout (the lookup and rect orders
    // and the list masks); the Island holds no configuration.

    /// The building with this id if it belongs to this city (the island's by_id
    /// cache), else nullptr.
    [[nodiscard]] const Building* find_building(const Island& island, BuildingId id) const;
    /// The building of this city on the tile (x, y) in the lists named by
    /// `list_mask`, searched in layout.lookup_order; nullptr when there is none.
    [[nodiscard]] const Building* building_at(const Island& island, const ListLayout& layout,
                                              std::int32_t x, std::int32_t y,
                                              std::uint32_t list_mask = ~0U) const;
    /// Clears `out`, then appends the ids of this city's buildings in the masked
    /// lists whose footprint shares a tile with `rect`, in the canonical order
    /// (list in layout.rect_order, y, x, id).
    void rect_query(const Island& island, const ListLayout& layout, const TileRect& rect,
                    std::uint32_t list_mask, std::vector<BuildingId>& out) const;
};

/// A city with the original's initial values: empty stock of
/// SimConfig::warehouse_goods goods (capacity 50 t), settings, build goods and
/// trade lists sized from the config, a fresh building index.
[[nodiscard]] City make_city(CityNo number, PlayerId owner, const SimConfig& cfg);

} // namespace anno::sim
