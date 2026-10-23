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
#include <anno/sim/ids.hpp>
#include <anno/sim/island.hpp>
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
        /// The start colour; 0xff = none (becomes kNoColour).
        std::uint8_t colour = 0xff;
        std::vector<std::uint8_t> name;
        /// Research state bytes; empty = all zero, else exactly
        /// SimConfig::research_entries bytes.
        std::vector<std::uint8_t> research;
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
    };
    /// A building placed at load (PROD2 record stand-in): created at tick 0
    /// through create_building, no cost charged.
    struct BuildingSpec {
        TypeId type;
        std::int32_t x = 0, y = 0;
        std::uint8_t rotation = 0, byte0f = 0;
        /// nullopt = the make_* default of the type's class (a mine finds its
        /// deposit at attach); a given payload replaces it, keeps its stocks,
        /// switches and `last`, and must satisfy payload_problem for the type.
        std::optional<Payload> payload;
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
        std::vector<BuildingSpec> buildings;
    };
    struct IslandSpec {
        /// The slot in the island table (INSELLNK order == slot order).
        IslandId slot;
        std::int32_t x0 = 0, y0 = 0;
        std::uint32_t w = 0, h = 0, climate = 0;
        std::shared_ptr<const Terrain> terrain;
        /// {owner, length}, row major from the top-left; the tiles after the last
        /// run are neutral (SimConfig::neutral_owner).
        std::vector<std::pair<std::uint32_t, std::uint32_t>> owner_runs;
        std::vector<TileObjectSpec> objects;
        std::vector<DepositSpec> deposits;
        std::vector<CitySpec> cities;
    };
    std::vector<PlayerSpec> players;
    /// The island table is as long as the highest slot + 1; unused slots stay null.
    std::vector<IslandSpec> islands;
};

/// The generator a world made from `spec` starts with (docs/sim/state.md §10 step 2):
/// the scenario's table or the original one, the cursor spec.seed mod its length
/// (world property 8, D AnnoWelt 0x100a3230). nullopt for an empty custom table.
/// World::create and the link resolution at load (resolve_scenario_links) share it.
[[nodiscard]] std::optional<WorldRng> scenario_rng(const ScenarioSpec& spec);

} // namespace anno::sim
