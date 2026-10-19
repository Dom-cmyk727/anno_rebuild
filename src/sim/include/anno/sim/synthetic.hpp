// Synthetic scenarios: small, code-built type data, players, islands and cities
// (stage S6), so the tests and the goldens need no game files (spec:
// docs/sim/state.md §14.3; CI has none).
//
// The production types use the per-step values the original's Anlagen.dat
// holds for the entries named in docs/re/production.md §9 (F1: weaver 6 / 11 /
// 5 t, grain farm 37 / 80, fur trapper 26 / 25, small farm 13, 4 / 128, 50,
// bow maker 9, iron mine 21), under ids in a synthetic range (9001...) that no
// Anlagen.dat entry uses.
#pragma once

#include <memory>
#include <string>

#include <anno/formats/result.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim::synthetic {

/// The synthetic building types and their ids.
struct SyntheticTypes {
    std::shared_ptr<const TypeRegistry> registry;
    /// HANDWERK weaver: STOFFE from WOLLE, 6 per step for 11, 5 t.
    TypeId weaver;
    /// PLANTAGE grain farm: KORN from GETREIDE, 37 for 80.
    TypeId farm;
    /// PELZJAEGER: PELZE from PELZTIER, 26 for 25.
    TypeId fur_trapper;
    /// UNIVERSAL small farm, two lines: 13 and 4 for 128 and 50.
    TypeId small_farm;
    /// HANDWERKWAFFEN bow maker: BOGEN and ARMBRUST.
    TypeId bow_maker;
    /// MINE: EISENERZ, 21 per step.
    TypeId iron_mine;
    /// PIONIER house (PRODTYPE 97).
    TypeId pioneer_house;
    /// KONTOR (PRODTYPE 35, the market list).
    TypeId kontor;
    /// MARKTSTAND (PRODTYPE 161, the stall list, never scheduled).
    TypeId market_stall;
    /// STRASSE (PRODTYPE 193): a tile object, not a building.
    TypeId road;
};

/// The registry of the types above on the original goods and class tables.
formats::Result<SyntheticTypes, std::string> make_synthetic_types();

/// The settings of the synthetic worlds: a 512 x 512 tile world, the add-on
/// edition flag at its default (3), everything else as WorldSettings{}.
WorldSettings synthetic_settings();

/// An island of `w` x `h` tiles at world tile (x0, y0) in table slot `slot`,
/// climate 2 (NORD), every tile neutral, nothing else. The builders below start
/// from it.
ScenarioSpec::IslandSpec plain_island(std::uint32_t slot, std::int32_t x0, std::int32_t y0,
                                      std::uint32_t w, std::uint32_t h);

/// A world with no player and no island (settings and seed only).
ScenarioSpec empty_scenario(std::uint32_t seed = 1234);

/// An empty city: number `no`, owned by player slot `owner`, no stock, no
/// buildings.
ScenarioSpec::CitySpec plain_city(std::uint32_t no, std::uint32_t owner);

/// The island of the golden G1 (docs/sim/state.md §14.3) without its buildings
/// (g1_scenario adds them): one player (slot 0, 10000 gold) and one island 64 x 64
/// at (100,100), climate NORD, owner runs (two neutral rows, then city 0), a few
/// scenery/growing tiles, an EISENERZ deposit at (110,120) and the empty city 0 of
/// player 0. No building, so it works on the empty registry.
ScenarioSpec basic_scenario(std::uint32_t seed = 1234);

/// Two players (slots 0 and 1) and two islands: slot 0 as in basic_scenario and
/// slot 3 (slots 1 and 2 stay null) 30 x 20 at (300,200), climate WUESTE, with
/// terrain columns, two deposits, an owner run for city 1 and the empty city 1 of
/// player 1. The island rectangles are disjoint; 30 x 20 is not a multiple of the
/// chunk edge 8 in either direction, which exercises the clipped edge chunks (A14,
/// F20).
ScenarioSpec two_island_scenario(std::uint32_t seed = 1234);

/// The scenario of the golden G1 (docs/sim/state.md §14.3): basic_scenario with
/// the weaver (5 t of wool in its input store) at (110,110) and the grain farm
/// (5 t of raw material) at (120,110) in city 0. Needs the synthetic registry.
ScenarioSpec g1_scenario(const SyntheticTypes& types, std::uint32_t seed = 1234);

} // namespace anno::sim::synthetic
