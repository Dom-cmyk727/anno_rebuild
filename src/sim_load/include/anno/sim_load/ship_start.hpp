// The ship-start loader (task MAP-2; contract: docs/sim/ship-start-integration.md).
// Reads nothing itself except the base scenario and the island templates of a game
// directory; the manifest is already parsed (anno/formats/ship_manifest.hpp). It builds
// a NEW headless World: five players with the baseline gold, one ship, one scout and one
// donkey per seat, the island templates placed, and no city, Kontor, reservation or
// territorial grant. It draws nothing from the PRNG. A failure returns an error and no
// world; `install_ship_start` replaces a world only on success.
//
// Adapter constraints (not limits of World or FigureRegistry): five seats and the baseline
// loadout, which is what manifest version 2 and this first loader support.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <anno/formats/result.hpp>
#include <anno/formats/ship_manifest.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/executor.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim_load {

/// A manifest milli-tile as Q24.8: `q = m * 32 / 125`, exact or an error. 125 and 32 are
/// coprime, so the quotient exists exactly when 125 divides `m * 32`. Nothing is rounded.
/// The error names what failed ("not a multiple of 1/256 tile", "overflows int32").
[[nodiscard]] formats::Result<std::int32_t, std::string> milli_tile_to_fixed(std::int64_t milli);

/// The version-2 facing 0, 1, 2, 3 as dir16 12, 8, 4, 0 (docs/re/ship-navigation.md §4).
/// nullopt outside 0..3.
[[nodiscard]] std::optional<std::uint8_t> facing_to_dir16(std::int64_t facing);

/// The goods of a manifest hold name in the registry: wood HOLZ, food NAHRUNG, tools WERKZEUG,
/// bricks ZIEGEL, spices GEWUERZE (docs/research/endless-start-conditions.md: the scenarios'
/// own WARETYPE table maps 42 wood, 26 food, 41 tools, 43 bricks, 31 spices). nullopt when the
/// name or the registry entry is missing.
[[nodiscard]] std::optional<sim::GoodsId> manifest_goods(const sim::TypeRegistry& types,
                                                         const std::string& name);

/// Builds the world. `game_dir` holds the manifest's `base` scenario and Scenes/Islands.
/// The manifest must be the supported baseline (ship_manifest::baseline_ok). The world has
/// developer_start semantics OFF: no Kontor, no city, no owned tile.
[[nodiscard]] formats::Result<sim::World, std::string>
load_ship_start(const formats::ship_manifest::Manifest& manifest,
                const std::filesystem::path& game_dir,
                std::shared_ptr<const sim::TypeRegistry> types, const sim::SimConfig& config = {},
                const sim::Rules& rules = {}, const sim::Executor& ex = sim::serial_executor());

/// load_ship_start into `target`: `target` is assigned only when the whole load succeeded; on
/// an error it is untouched. Returns the error, or nullopt.
[[nodiscard]] std::optional<std::string>
install_ship_start(sim::World& target, const formats::ship_manifest::Manifest& manifest,
                   const std::filesystem::path& game_dir,
                   std::shared_ptr<const sim::TypeRegistry> types,
                   const sim::SimConfig& config = {}, const sim::Rules& rules = {},
                   const sim::Executor& ex = sim::serial_executor());

/// The baseline expectation on a loaded world, stricter than the generic graph rules: five
/// seats, ids 3 * seat + 1 .. 3 * seat + 3 = ship 1502, scout 717, donkey 718 of that owner;
/// the scout's parent is the ship with crew slot 0, the donkey's parent is the SCOUT (not the
/// ship), the ship has no parent; no home; cargo_goods and cargo_88 zero; the ship has the
/// four baseline holds in order and the others none; scout and donkey share the ship's
/// position and facing; next_figure is 16; no city on any island. Empty when it holds, else
/// the first violation.
[[nodiscard]] std::string expedition_baseline_problem(const sim::World& world);

} // namespace anno::sim_load
