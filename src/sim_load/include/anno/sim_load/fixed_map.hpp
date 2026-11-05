// The fixed-map loader (docs/sim/fixed-map.md). Reads the named base scenario
// and the named templates from a game directory and builds the ScenarioSpec a
// World is created from. It does not draw: the link resolver is not called.
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

#include <anno/formats/fixed_map.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/game_setup.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim_load {

/// What a layout does not decide. `developer_start` is on for MVP-A: one city
/// per seated player, the whole start island owned by that city, one Kontor at
/// the layout's tile, created at tick 0 with no cost.
/// `kontor_type` is the original Anlagen id of PRODTYPE 35 (synthetic.hpp). It
/// is a default, not a limit: a mod registry passes another id.
/// Test resources for the developer start (owner, round dr): gold for every seat's
/// player and the same goods stock for each start city, so a Build of the first
/// layout types is affordable on the developer map. Off by default; `--play` of a
/// fixed map turns it on, the original scenarios never do. Not a rule: the sim
/// charges and refuses as before (Build step 6, apply.cpp).
struct DeveloperResources {
    bool enabled = false;
    /// The most generous human money the original's endless presets state:
    /// 10 000 000 (endless01.sze, docs/formats/endless-presets.md, preset table).
    std::int64_t gold = 10000000;
    /// Tons per goods in each start city. The largest city stock in the same table:
    /// 190 t (endless02.sze, computer player 8; docs/formats/endless-presets.md).
    /// Converted to 8.8 by kTon when the city is built.
    std::uint32_t goods_tons = 190;
};

struct FixedMapOptions {
    bool developer_start = true;
    std::uint32_t kontor_type = 1302;
    DeveloperResources resources;
};

/// `seats` is a prefix of the layout's starts: index 0 is start seat 0. Two
/// seats up to the number of start regions. `game_dir` is the folder that
/// holds the base scenario (`map.base`, relative) and Scenes/Islands.
/// The board's shorter signature omits `game_dir` and `options`; both are
/// required by the same task (the base file, and the developer-start switch).
[[nodiscard]] formats::Result<sim::LoadedScenario, std::string>
load_fixed_map(const formats::fixed_map::FixedMap& map, std::span<const sim::Seat> seats,
               const std::filesystem::path& game_dir, const sim::TypeRegistry& types,
               const sim::SimConfig& config, FixedMapOptions options = {});

} // namespace anno::sim_load
