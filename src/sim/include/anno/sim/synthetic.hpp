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
    /// Growing tile objects (make_growth_types only; invalid ids elsewhere), each with
    /// its successor, the next id (a ROHSTOFF object that does not grow): a grain
    /// field (ROHSTWACHS, P 15 500, S 5, k 2: the values of fixture F8, entry 712), a
    /// forest (ROHSTWACHS, P 24 000, S 5, k 2) and a dry grass (ROHSTDUERR, P 1 000,
    /// S 5, k 0; P / 2 = 500 is shorter than a full growth sweep, so flips are lost).
    TypeId field, ripe_field, forest, ripe_forest, grass, ripe_grass;
    /// make_transport_types only (invalid ids elsewhere): a market (MARKT, PRODTYPE 34,
    /// 3 x 3, radius 8, one cart of figure 220, its entrance at (1.5, 2.5)) and a weaver
    /// with one carrier of figure 203 (radius 16, the weaver 1001's: P).
    TypeId market, carrier_weaver;
};

/// The registry of the types above on the original goods and class tables.
formats::Result<SyntheticTypes, std::string> make_synthetic_types();

/// make_synthetic_types plus the growing tile objects field (9011), forest (9013),
/// grass (9015) and their ripe successors 9012, 9014, 9016 (stage S13, the golden G5).
formats::Result<SyntheticTypes, std::string> make_growth_types();

/// make_synthetic_types with the road flag 0x4000 on the road (9010), plus the market
/// (9021) and the carrier weaver (9022) and the figure types 220 (the market cart:
/// 0.016 tiles per time unit, 5 t, layer 3, a wait of 1.0) and 203 (the carrier: 0.012,
/// 3 t, layer 0, a wait of 2.0), with the file's values (P, docs/re/transport.md §4)
/// converted as the registry converts FigurenHeader.dat (task T24, the golden G6).
formats::Result<SyntheticTypes, std::string> make_transport_types();

/// make_synthetic_types with a group table of two small groups written in code (P1, the
/// golden G7): the registry's SimConfig has `house_groups` 2, so a world made from it
/// takes `population_config()`. Group 0 (pioneers): Wohnmax 8, the needs NAHRUNG
/// (forced, Menge 41) and LEDER (forced, Menge 16); group 1 (settlers): Wohnmax 8, those
/// two and STOFFE (forced, Menge 20) as the one extra forced need; both with the VermehrTab
/// -16, -9, 0, 12, 32 (the integers of -0.25, -0.15, 0, 0.2, 0.5). Nothing of BGruppen.dat.
formats::Result<SyntheticTypes, std::string> make_population_types();

/// The SimConfig of the population registry: `house_groups` 2.
SimConfig population_config();

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

/// The types of make_synthetic_types (same ids, classes and production values) with
/// the build cost of the Anlagen.dat entry each one stands for (HEAD u16 [8] gold and
/// bytes [3..6] bricks, wood, tools, marble; the values of docs/research/playbook.md
/// §14 and docs/sim/state.md §14.5), for the commands of stage S10 and the golden G2:
/// weaver = weaving hut 1001 (300 gold, 3 t tools, 4 t wood), grain farm 811 (200
/// gold, 2 t tools, 3 t wood, 3 t bricks), fur trapper 824 (100 gold, 5 t tools, 6 t
/// wood), small farm 809 (250 gold, 2 t tools, 4 t wood), iron mine = ore mine 1205
/// (1 200 gold, 5 t tools, 15 t wood), pioneer house 501 (3 t wood), kontor 1302 (350
/// gold, 5 t tools, 12 t wood), market stall 1310 (50 gold, 1 t tools, 1 t wood); the
/// bow maker and the road cost nothing. Footprints (own test data, not the
/// original's): fur trapper 2 x 3, pioneer house and kontor 2 x 2, the others 1 x 1.
formats::Result<SyntheticTypes, std::string> make_build_types();

/// The scenario of the golden G2 (docs/sim/state.md §14.3, stage S10): g1_scenario on
/// `types` (make_build_types) with 13 t of tools and 30 t of wood in the warehouse of
/// city 0, so that four weavers can be paid and a fifth cannot.
ScenarioSpec g2_scenario(const SyntheticTypes& types, std::uint32_t seed = 1234);

/// The scenario of the golden G3 (docs/sim/state.md §14.3, stage S11), on
/// make_synthetic_types: two_island_scenario (two players, two islands, city 0 of player 0
/// on island 0 and city 1 of player 1 on island 3) with one building of every ticking
/// class and an inert house in city 0, and a second bow maker and a weaver in city 1.
/// City 0 at y = 110: the weaver (110; 5 t of wool), the grain farm (120; 5 t of raw
/// material), the fur trapper (130; 2 t of raw material), the small farm (140; 5 t of raw
/// material per line), the bow maker (150; 5 t of wood and 5 t of ropes); the iron mine at
/// (110, 118) over the two EISENERZ deposits of island 0, (110, 120) with 2 t and
/// (112, 121) with 1 t, both without the reserve flag, so the mine uses the first up,
/// moves to the second and runs dry in the add-on edition (the synthetic settings); the
/// pioneer house at (125, 125). City 1: the bow maker at (305, 205) and the weaver at
/// (310, 205), stocked as in city 0. Research bytes: player 0 has BOGEN's research (4)
/// done and ARMBRUST's (9) only researchable, so its bow maker works one line; player 1
/// has both done.
ScenarioSpec g3_scenario(const SyntheticTypes& types, std::uint32_t seed = 1234);

/// The scenario of the golden G4 (docs/sim/state.md §14.3, stage S12), on
/// make_synthetic_types; the seed 508 is the one the golden uses (its draws are worked
/// out in world_events_test.cpp). Three players: 0 (kind 8, Volk 1), 1 (kind 12, Volk 1,
/// 2 900 gold in debt) and 2 (kind 14, Volk 2). Three islands in table slots 0, 2 and 4
/// (slots 1 and 3 stay null): slot 0, 24 x 24 at (100, 100), climate 2, every tile owned
/// by city 0 of player 0, with the weaver (5 t of wool) at (110, 110) and the grain farm
/// (5 t of raw material) at (120, 110); slot 2, 16 x 16 at (300, 100), climate 1, with
/// city 0 of player 0 and city 1 of player 1; slot 4, 16 x 16 at (300, 300), climate 0,
/// with city 0 of player 1 and city 1 of player 2. World property 7's offset is 1000 (the
/// game seconds the monopoly table records). The test adds the state a scenario cannot
/// carry (upkeep sums, timers, queued events, figures on the expiry list).
ScenarioSpec g4_scenario(const SyntheticTypes& types, std::uint32_t seed = 508);

/// The scenario of the golden G5 (docs/sim/state.md §14.3, stage S13), on
/// make_growth_types; the seed 514 is the one the golden uses (its 300 s draws are worked
/// out in sweeps_test.cpp). One player (slot 0); one island in slot 0, 60 x 44 tiles at
/// (100, 100) (8 x 6 chunks, the last column and row of chunks clipped), climate 2, rows 0
/// .. 39 owned by city 0 and rows 40 .. 43 by city 3, both of player 0; the Kontor (the
/// market list) of city 0 at (131, 123), so the 300 s event centres a drought on the chunk
/// (128, 120); a grain field at island tile (2, 3) (chunk 0) and a forest at (58, 42) (the
/// last chunk, 47). World property 6 has bit 0x80: the 7.5 minute event is off, so the 300
/// s event makes the only draws.
ScenarioSpec g5_scenario(const SyntheticTypes& types, std::uint32_t seed = 514);

/// The scenario of the golden G6 (docs/sim/state.md §14.3, task T24), on
/// make_transport_types. One player; one island in slot 0, 40 x 32 tiles at (100, 100),
/// every tile city 0's. City 0: the market at (110, 110); on its road the weaver at (111,
/// 115) with 5 t of cloth and the grain farm at (114, 115) with 5 t of corn and 5 t of raw
/// material (roads (111, 113), (111, 114), (112, 114), (113, 114), (114, 114)); the carrier
/// weaver at (106, 114) without wool. The warehouse holds 30 t of wool.
ScenarioSpec g6_scenario(const SyntheticTypes& types, std::uint32_t seed = 624);

/// The scenario of the golden G7 (docs/sim/state.md §4.7c, task P1), on
/// make_population_types. One player; one island in slot 0, 40 x 32 tiles at (100, 100),
/// every tile city 0's. City 0: one pioneer house at (110, 110) with 5 residents and the
/// food stock of a 100 % satisfaction (s = 128), and a warehouse holding the settler-house
/// materials (goods 0x29..0x2d, 20 t each) and 10 t of food and 5 t of leather. P1 adds no
/// house behaviour: the golden is the created world (`p1_g7_create`).
ScenarioSpec g7_scenario(const SyntheticTypes& types, std::uint32_t seed = 700);

} // namespace anno::sim::synthetic
