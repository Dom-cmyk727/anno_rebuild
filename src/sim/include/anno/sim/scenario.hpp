// The scenario a World is created from (spec: docs/sim/state.md §10, stage S5).
//
// Plain data: nothing in it points into a file buffer, so it can be built by
// the scenario adapter of stage S14 (the only code that touches the original
// scenario formats), by the synthetic builders of the tests (synthetic.hpp), or
// by a mod. Stage S5 has the part that exists: settings, the PRNG seed and
// table, the players and the islands with their tiles, owners and deposits; stage
// S6 added the cities and their buildings (CitySpec, BuildingSpec).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/sim/building.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/settings.hpp>

namespace anno::sim {

struct ScenarioSpec {
    /// HEAD bytes -> properties (D: 0x10009cc0); prop14 from the root name (1
    /// ANNO, 3 ANNOADDON1). world_width/height bound the island rectangles.
    WorldSettings settings;
    /// World property 8: the PRNG cursor is `seed mod table size`.
    std::uint32_t seed = 1;
    /// A custom PRNG table (non-empty); the default is the original's.
    std::optional<std::vector<std::uint16_t>> rng_table;

    struct PlayerSpec {
        /// The slot in the player table.
        PlayerId id;
        std::uint32_t kind = 8, volk = 1;
        std::int64_t money = 10000;
        std::uint32_t flags_a0 = 0;
        /// MAP-POP-IMPL-1 A (stream 127, docs/sim/population-contract.md): the player's stage mask
        /// (`Player::flags`, original +0x90), restored from an original SPIELER head
        /// (AnnoWelt 0x1001d389-0x1001d3df). nullopt = no head: the constructor's 1 stays (a
        /// synthetic, fixed-map or ship-start spec); a stored 0 is kept as 0.
        std::optional<std::uint32_t> flags;
        /// The start colour; 0xff = none (becomes kNoColour).
        std::uint8_t colour = 0xff;
        std::vector<std::uint8_t> name;
        /// Research state bytes; empty = all zero, else exactly
        /// SimConfig::research_entries bytes.
        std::vector<std::uint8_t> research;
        /// The relation rows (stream 120; the DIPL record): ascending by `other`, unique, never the
        /// player itself, `other` below SimConfig::player_slots. Empty = the constructor's zero
        /// fill.
        std::vector<Relation> relations;
        /// Player property 7 (stream 120): ascending unique players; empty by default.
        std::vector<PlayerId> discovery;
    };
    /// BERGWERK2 stand-in (D18): an ore object at a world tile.
    struct DepositSpec {
        std::string goods;
        std::int32_t x = 0, y = 0;
        std::uint32_t remaining = 2000 * kTon;
        bool reserve = true;
    };
    /// Scenery and growing items: the object on one tile (index row-major).
    struct TileObjectSpec {
        std::uint32_t index = 0;
        std::uint32_t object = 0;
        std::uint8_t phase = 0;
        /// The item's rotation (quarter turns; insel.md item `id` bits 14–15,
        /// read at AnnoWelt 0x1008964e). Follows `phase` so a positional
        /// `{index, object, phase}` aggregate keeps its phase. Default 0.
        std::uint8_t rotation = 0;
        /// The item's height byte (insel.md item `state >> 8`; the original's tile record
        /// second dword byte 0, written at AnnoWelt 0x10082494): the base of the ship-route
        /// object predicate (TileLayer::item_height). Follows `rotation` so the positional
        /// aggregates keep their meaning. Default 0.
        std::uint8_t height = 0;
    };
    /// A building placed at load (PROD2 record stand-in): created at tick 0
    /// through create_building, no cost charged.
    struct BuildingSpec {
        TypeId type;
        std::int32_t x = 0, y = 0;
        std::uint8_t rotation = 0, byte0f = 0;
        /// A market's KONTID (stream 119; 0 = none), kept from the nested record.
        std::uint32_t kontor_id = 0;
        /// nullopt = the make_* default of the type's class (a mine finds its
        /// deposit at attach); a given payload replaces it, keeps its stocks,
        /// switches and `last`, and must satisfy payload_problem for the type.
        std::optional<Payload> payload{};
    };
    /// A city of an island. Cities are created in ascending number, so the world
    /// does not depend on the order of the vector; the buildings of a city in file
    /// order (PROD2 order = list slot order).
    struct CitySpec {
        /// The slot in Island::cities = the owner value written to tiles; below
        /// SimConfig::city_slots and not SimConfig::neutral_owner.
        CityNo no;
        /// A player of `players`.
        PlayerId owner;
        /// The warehouse stock in 8.8 tons, exactly SimConfig::warehouse_goods
        /// entries; nullopt = empty.
        std::optional<std::vector<std::uint32_t>> stock;
        /// The warehouse capacity in 8.8 tons (CityStock::capacity; a savegame or
        /// scenario city's HEAD2, docs/re/annowelt.md "Scenario cities"); nullopt =
        /// the constructor's 50 t.
        std::optional<std::uint32_t> capacity;
        /// The per-goods trade-list setting (City::goods, size warehouse_goods);
        /// nullopt = all default. From STADT2 VK/EK (T26, docs/re/annowelt.md
        /// "Scenario cities" (g)).
        std::optional<std::vector<GoodsSetting>> goods;
        /// The build-goods settings (City::build_goods, size build_goods_count);
        /// nullopt = all default. From STADT2 BAUMAT (T26).
        std::optional<std::vector<BuildGoods>> build_goods;
        /// The trade lists (City::trade_lists, size trade_lists); nullopt = empty.
        /// From STADT2 VK (list 1) and EK (list 0) (T26).
        std::optional<std::vector<std::vector<TradeEntry>>> trade_lists;
        std::vector<BuildingSpec> buildings;
        /// City property 0 (stream 120): ascending unique players; empty by default (the loaders do
        /// not read HEAD2 [0x30] yet: docs/sim/ship-founding.md section 16).
        std::vector<PlayerId> player_mask;
    };
    struct IslandSpec {
        /// The slot in the island table (INSELLNK order == slot order).
        IslandId slot;
        std::int32_t x0 = 0, y0 = 0;
        std::uint32_t w = 0, h = 0, climate = 0;
        /// The island's LOCKED byte (stream 118): 0 when the island has no LOCKED record.
        std::uint8_t lock = 0;
        /// The island's Kontor counter (stream 119): the KONTORID record's u16, default 1.
        std::uint32_t kontor_counter = 1;
        /// Island property 0xe (stream 120): ascending unique players; empty by default.
        std::vector<PlayerId> players_present;
        /// The ore objects (stream 120; the OCCUR record), copied to Island::ore_objects.
        std::vector<OreObject> ore_objects;
        std::shared_ptr<const Terrain> terrain;
        /// {owner, length}, row major from the top-left; the tiles after the last
        /// run are neutral (SimConfig::neutral_owner).
        std::vector<std::pair<std::uint32_t, std::uint32_t>> owner_runs;
        std::vector<TileObjectSpec> objects;
        std::vector<DepositSpec> deposits;
        std::vector<CitySpec> cities;
    };
    /// A figure placed at load (MAP-2: a ship, the scout it carries and the scout's donkey).
    /// The figures get the ids next_figure, next_figure + 1, ... in this order, with no draw
    /// from the PRNG, so a spec fixes every id. A figure has no home building.
    struct FigureSpec {
        /// The FigurenHeader id; the type registry must have a figure type of it.
        std::uint32_t kind = 0;
        /// A player of `players`.
        PlayerId owner;
        FixedPos pos;
        /// 0..15.
        std::uint8_t dir16 = 0;
        /// The index in `figures` of the figure carrying this one; it must be smaller than
        /// this figure's own index (so the links cannot form a cycle) and name the same owner.
        std::optional<std::uint32_t> parent;
        /// With a parent only; unique among the parent's children.
        std::optional<std::uint32_t> crew_slot;
        /// Exact amounts in raw 1/256 ton (stream 117); each goods must be in the registry's goods
        /// table.
        std::vector<ShipHold> holds;
        /// The state word (stream 120); 0 = a scenario ship.
        std::uint32_t state_word = 0;
        /// A navigating ship's current hit points (MAP-NAV-3 N4, stream 123), in the unit of the
        /// type's max_hp, at most max_hp. Absent = full health (a new expedition). Not for a
        /// figure that does not navigate.
        std::optional<std::uint32_t> health;
        /// True = the health is explicitly unknown (what an import without the field would
        /// carry); exclusive with `health`. The speed then uses the full-health term.
        bool health_unknown = false;
    };
    std::vector<PlayerSpec> players;
    /// Figures at tick 0, in id order. Default: none.
    std::vector<FigureSpec> figures;
    /// The island table is as long as the highest slot + 1; unused slots stay null.
    std::vector<IslandSpec> islands;
};

/// The generator a world made from `spec` starts with (docs/sim/state.md §10 step 2):
/// the scenario's table or the original one, the cursor spec.seed mod its length
/// (world property 8, D AnnoWelt 0x100a3230). nullopt for an empty custom table.
/// World::create and the link resolution at load (resolve_scenario_links) share it.
[[nodiscard]] std::optional<WorldRng> scenario_rng(const ScenarioSpec& spec);

} // namespace anno::sim
