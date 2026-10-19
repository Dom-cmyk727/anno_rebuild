// The scenario a World is created from (spec: docs/sim/state.md §10, stage S5).
//
// Plain data: nothing in it points into a file buffer, so it can be built by
// the scenario adapter of stage S14 (the only code that touches the original
// scenario formats), by the synthetic builders of the tests (synthetic.hpp), or
// by a mod. Stage S5 has the part that exists: settings, the PRNG seed and
// table, the players and the islands with their tiles, owners and deposits.
// Cities and buildings (CitySpec, BuildingSpec) follow with stage S6.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <anno/sim/ids.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/production.hpp>
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
    };
    std::vector<PlayerSpec> players;
    /// The island table is as long as the highest slot + 1; unused slots stay null.
    std::vector<IslandSpec> islands;
};

} // namespace anno::sim
