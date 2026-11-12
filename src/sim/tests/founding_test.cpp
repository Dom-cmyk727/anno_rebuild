// The ship-founding command through the production path (World::apply), task MAP-2 packages 6-8.
// Contract: docs/sim/ship-founding.md section 16 (worked example 13.3). Every expected value is
// derived by hand from the contract and the original's rules, not from the code under test:
//
//   island 0: 120 x 64 tiles at (0, 0); vertex heights by column: x <= 49 land 0x40, x = 50 shore
//   0x30, x >= 51 sea 0x10. Type 1302: 4 x 4, flag word 0x2087, wood 12 (3072), tools 5 (1280),
//   gold 350, radius 24, PRODTYPE 35. Click (51, 31) with initial rotation 1: origin (50, 30)
//   (51 - 1, 31 - 1). Rotation 1's shore edge is the left column x = 50 (all 0x30) and the
//   opposite column x = 54 is 0x10 (>= 2 of 5 below 0x20): accepted; rotation 0 would need the top
//   row y = 30 (0x30, 0x10, ...) and fails. Ship at (57.5, 32.5): dx = trunc(51 - 57.5) = -6,
//   dy = trunc(31 - 32.5) = -1, 37 <= 168. Hull 397/256: xmin = trunc((57.5 - 1.55)) = 55 >= 54.
//   Seed 1234: the name draw is T[1235] = 30195 (MSVC rand() from 1); mod 4 = 3 -> names[3].
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/founding.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/territory.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "golden.hpp"
#include "island_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

constexpr std::uint32_t kT = 256;
constexpr sim::TypeId kKontor{1302};
constexpr sim::TypeId kRoadObj{9100};  // an obstruction (no flag 0x20)
constexpr sim::TypeId kTree{9101};     // overlappable scenery (flag 0x20)
constexpr sim::TypeId kKontor43{9301}; // a 6 x 3 Kontor-class type (non-square)
constexpr sim::TypeId kWeaver{9201};   // a free workshop for the ordinary Build afterwards
constexpr sim::GoodsId kTools = 0x29, kWood = 0x2a, kFood = 5;

std::shared_ptr<const sim::TypeRegistry> registry() {
    auto r = syn::make_founding_types();
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return r.value().registry;
}

struct Setup {
    sim::ScenarioSpec spec;
    sim::Rules rules;
    sim::SimConfig cfg;
};

Setup base_setup() {
    Setup s;
    s.spec = syn::empty_scenario(1234);
    for (std::uint32_t p = 0; p < 3; ++p) {
        sim::ScenarioSpec::PlayerSpec ps;
        ps.id = sim::PlayerId{p};
        ps.kind = 8;
        ps.money = p == 0 ? 30000 : 5000;
        s.spec.players.push_back(ps);
    }
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 0, 0, 120, 64);
    std::vector<std::uint8_t> height(120U * 64U);
    for (std::uint32_t y = 0; y < 64; ++y) {
        for (std::uint32_t x = 0; x < 120; ++x) {
            height[y * 120 + x] = x <= 49 ? 0x40 : (x == 50 ? 0x30 : 0x10);
        }
    }
    is.terrain = sim::make_terrain(std::move(height), {}, {});
    s.spec.islands.push_back(std::move(is));
    // The expedition of player 0: the ship (figure 1) with four holds, its scout (2, embarked,
    // crew slot 0) and the donkey (3, carried by the scout), all at (57.5, 32.5).
    sim::ScenarioSpec::FigureSpec ship;
    ship.kind = 1502;
    ship.owner = sim::PlayerId{0};
    ship.pos = sim::FixedPos{static_cast<std::int32_t>(57 * kT + kT / 2),
                             static_cast<std::int32_t>(32 * kT + kT / 2)};
    ship.holds = {sim::ShipHold{kWood, 12800}, sim::ShipHold{kFood, 2560},
                  sim::ShipHold{kTools, 12800}, sim::ShipHold{kTools, 7680}};
    s.spec.figures.push_back(ship);
    sim::ScenarioSpec::FigureSpec scout = ship;
    scout.kind = 717;
    scout.holds.clear();
    scout.parent = 0;
    scout.crew_slot = 0;
    s.spec.figures.push_back(scout);
    sim::ScenarioSpec::FigureSpec donkey = scout;
    donkey.kind = 718;
    donkey.parent = 1;
    donkey.crew_slot = std::nullopt;
    s.spec.figures.push_back(donkey);
    s.rules.city_names = {"Alpha", "Bravo", "Charlie", "Delta"};
    return s;
}

sim::World make(const Setup& s) {
    auto r = sim::World::create(s.spec, registry(), s.cfg, s.rules);
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

sim::Command control_command(std::uint32_t issuer = 0) {
    sim::FoundCmd f;
    f.ship = sim::FigureId{1};
    f.placement.type = kKontor;
    f.placement.x = 50;
    f.placement.y = 30;
    f.placement.rotation = 1;
    f.click_x = 51;
    f.click_y = 31;
    return sim::Command{sim::PlayerId{issuer}, f};
}

sim::FoundCmd& found_of(sim::Command& c) {
    return std::get<sim::FoundCmd>(c.body);
}

struct Snap {
    std::vector<std::uint8_t> bytes;
    std::uint64_t hash;
    std::uint32_t rng;
    friend bool operator==(const Snap&, const Snap&) = default;
};
Snap snap(const sim::World& w) {
    return Snap{w.save(), w.state_hash(), w.rng().cursor()};
}

const sim::Island& isl(const sim::World& w) {
    return *w.islands()[0];
}

std::uint64_t owned_by(const sim::World& w, std::uint32_t owner) {
    std::uint64_t n = 0;
    const sim::TileLayer& t = isl(w).tiles;
    for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
        n += t.owner(i) == owner ? 1U : 0U;
    }
    return n;
}

/// Applies `c` on a copy-free fresh world and expects exactly `want` with nothing changed.
void rejected(sim::World& w, const sim::Command& c, sim::Reject want) {
    const Snap before = snap(w);
    const sim::ApplyResult r = w.apply(c);
    CHECK(r.status == want);
    CHECK(r.records.empty());
    CHECK(snap(w) == before);
}

} // namespace

TEST_CASE("founding: the control command founds a city with a warehouse, exactly") {
    Setup s = base_setup();
    sim::World w = make(s);
    REQUIRE(w.rng().cursor() == 1234);
    const sim::ApplyResult r = w.apply(control_command());
    REQUIRE(r.ok());
    REQUIRE(r.records.size() == 1);

    // Payment: gold 30000 - 350; wood 12800 - 3072; the FIRST tools hold 12800 - 1280; the
    // second tools hold and the food hold untouched.
    CHECK(w.players().find(sim::PlayerId{0})->money == 29650);
    const sim::Figure* ship = sim::find_figure(w.figures(), sim::FigureId{1});
    REQUIRE(ship != nullptr);
    REQUIRE(ship->holds.size() == 4);
    CHECK(ship->holds[0] == sim::ShipHold{kWood, 9728});
    CHECK(ship->holds[1] == sim::ShipHold{kFood, 2560});
    CHECK(ship->holds[2] == sim::ShipHold{kTools, 11520});
    CHECK(ship->holds[3] == sim::ShipHold{kTools, 7680});
    // Rivals unchanged.
    CHECK(w.players().find(sim::PlayerId{1})->money == 5000);

    // The city: number 0 (cursor -1 + 1), owner 0, the name drawn once, the cursor and the
    // Kontor counter.
    const sim::Island& island = isl(w);
    REQUIRE(island.cities[0].has_value());
    const sim::City& city = *island.cities[0];
    CHECK(city.owner == sim::PlayerId{0});
    CHECK(city.name == "Delta");
    CHECK(w.rng().cursor() == 1235);
    CHECK(island.city_cursor == 0);
    CHECK(island.kontor_counter == 2);
    // Registration: the owner knows his city; the island has him present; the recount made him
    // learn his own city's owner.
    CHECK(city.player_mask == std::vector<sim::PlayerId>{sim::PlayerId{0}});
    CHECK(island.players_present == std::vector<sim::PlayerId>{sim::PlayerId{0}});
    CHECK(w.players().find(sim::PlayerId{0})->discovery ==
          std::vector<sim::PlayerId>{sim::PlayerId{0}});

    // The warehouse: origin (50, 30), rotation 1, byte0f 0x30 (the coast result), KONTID = the old
    // counter 1.
    const sim::Building* b = island.find_building(r.records[0].building);
    REQUIRE(b != nullptr);
    CHECK(b->type == kKontor);
    CHECK(b->x == 50);
    CHECK(b->y == 30);
    CHECK(b->rotation == 1);
    CHECK(b->byte0f == 0x30);
    CHECK(b->kontor_id == 1);
    // Its 16 footprint tiles carry the type id and the owner 0.
    for (std::uint32_t y = 30; y < 34; ++y) {
        for (std::uint32_t x = 50; x < 54; ++x) {
            const std::uint64_t idx = y * 120ULL + x;
            CHECK(island.tiles.object(idx) == 1302);
            CHECK(island.tiles.owner(idx) == 0);
        }
    }
    // Territory: the R = 24 disc around the 4 x 4 footprint is 1,976 tiles (the emulated original,
    // contract 13.1): rows 7..56, row 7 = x 47..56, the footprint inside it; nothing else owned.
    CHECK(owned_by(w, 0) == 1976);
    CHECK(island.tiles.owner(7 * 120ULL + 47) == 0);
    CHECK(island.tiles.owner(7 * 120ULL + 56) == 0);
    CHECK(island.tiles.owner(7 * 120ULL + 46) == s.cfg.neutral_owner);
    CHECK(island.tiles.owner(7 * 120ULL + 57) == s.cfg.neutral_owner);
    CHECK(island.tiles.owner(6 * 120ULL + 50) == s.cfg.neutral_owner);
    CHECK(island.tiles.owner(56 * 120ULL + 50) == 0);
    CHECK(island.tiles.owner(57 * 120ULL + 50) == s.cfg.neutral_owner);

    // Ship, scout and donkey keep ids, owners, parents and positions.
    for (std::uint64_t id = 1; id <= 3; ++id) {
        const sim::Figure* f = sim::find_figure(w.figures(), sim::FigureId{id});
        REQUIRE(f != nullptr);
        CHECK(f->owner == sim::PlayerId{0});
        CHECK(f->pos.x == static_cast<std::int32_t>(57 * kT + kT / 2));
        CHECK(f->pos.y == static_cast<std::int32_t>(32 * kT + kT / 2));
    }
    CHECK(sim::find_figure(w.figures(), sim::FigureId{2})->parent == sim::FigureId{1});
    CHECK(sim::find_figure(w.figures(), sim::FigureId{3})->parent == sim::FigureId{2});
    // The world's invariants hold.
    CHECK(w.check_invariants().empty());
}

TEST_CASE(
    "founding: a second command of the same player on the island is OwnCityOnIsland, uncharged") {
    Setup s = base_setup();
    sim::World w = make(s);
    REQUIRE(w.apply(control_command()).ok());
    sim::Command again = control_command();
    found_of(again).placement.x = 80;
    found_of(again).click_x = 81;
    // The ship is within 12 of (81, 31)? ship x 57.5: dx = 23, so move the ship first.
    sim::Figure* ship = sim::find_figure(sim::WorldTestAccess::figures(w), sim::FigureId{1});
    ship->pos.x = 82 * kT;
    rejected(w, again, sim::Reject::OwnCityOnIsland);
}

namespace {

sim::ScenarioSpec::FigureSpec soldier_at(std::uint32_t owner, double x_tiles, double y_tiles,
                                         std::uint32_t kind = 9501) {
    sim::ScenarioSpec::FigureSpec f;
    f.kind = kind;
    f.owner = sim::PlayerId{owner};
    f.pos = sim::FixedPos{static_cast<std::int32_t>(x_tiles * kT),
                          static_cast<std::int32_t>(y_tiles * kT)};
    return f;
}

struct Row {
    const char* name;
    sim::Reject want;
    void (*edit)(Setup&, sim::Command&);
};

/// Founds on `s` with `c` and expects Ok (the control side of a row).
void succeeds(const Setup& s, sim::Command c = control_command()) {
    sim::World w = make(s);
    const sim::ApplyResult r = w.apply(c);
    CHECK(r.status == sim::Reject::None);
}

} // namespace

TEST_CASE("founding: every refusal from the control, one condition at a time, nothing changes") {
    // claim | the control reaches the operation | ONE input changed | every earlier check still
    // satisfied | the specific refusal | state, hash and PRNG cursor equal afterwards.
    const std::vector<Row> rows = {
        {"issuer is not a player", sim::Reject::UnknownIssuer,
         [](Setup&, sim::Command& c) { c.issuer = sim::PlayerId{9}; }},
        {"a computer player (kind 12) is not a human founder", sim::Reject::Unsupported,
         [](Setup& s, sim::Command&) { s.spec.players[0].kind = 12; }},
        {"no figure of that id", sim::Reject::UnknownFigure,
         [](Setup&, sim::Command& c) { found_of(c).ship = sim::FigureId{99}; }},
        {"the figure is the scout, not a ship", sim::Reject::NotAShip,
         [](Setup&, sim::Command& c) { found_of(c).ship = sim::FigureId{2}; }},
        {"the issuer is not the ship's owner", sim::Reject::NotShipOwner,
         [](Setup&, sim::Command& c) { c.issuer = sim::PlayerId{1}; }},
        {"only state bit 0x02 set on the ship", sim::Reject::ShipDisabled,
         [](Setup& s, sim::Command&) { s.spec.figures[0].state_word = 0x02; }},
        {"unknown building type", sim::Reject::UnknownType,
         [](Setup&, sim::Command& c) { found_of(c).placement.type = sim::TypeId{9999}; }},
        {"type id 0", sim::Reject::UnknownType,
         [](Setup&, sim::Command& c) { found_of(c).placement.type = sim::TypeId{0}; }},
        {"rotation 4", sim::Reject::BadRotation,
         [](Setup&, sim::Command& c) { found_of(c).placement.rotation = 4; }},
        {"a type that is not a market block", sim::Reject::NotAFoundingType,
         [](Setup&, sim::Command& c) { found_of(c).placement.type = kRoadObj; }},
        {"1302 is not in Rules::founding_types", sim::Reject::NotAFoundingType,
         [](Setup& s, sim::Command&) { s.rules.founding_types = {1304}; }},
        {"the supplied origin differs from the derived one", sim::Reject::BadValue,
         [](Setup&, sim::Command& c) { found_of(c).placement.x = 51; }},
        {"the origin lies on no island", sim::Reject::UnknownIsland,
         [](Setup&, sim::Command& c) {
             found_of(c).click_x = 300;
             found_of(c).click_y = 300;
             found_of(c).placement.x = 299;
             found_of(c).placement.y = 299;
         }},
        {"the island is locked", sim::Reject::IslandLocked,
         [](Setup& s, sim::Command&) { s.spec.islands[0].lock = 1; }},
        {"the first footprint leaves the island on the east", sim::Reject::OutOfIsland,
         [](Setup&, sim::Command& c) {
             found_of(c).click_x = 118;
             found_of(c).placement.x = 117;
         }},
        {"the click is 19 tiles from the ship", sim::Reject::OutOfRange,
         [](Setup& s, sim::Command&) { s.spec.figures[0].pos.x = 70 * kT; }},
        {"the origin tile belongs to another player's city", sim::Reject::SiteOwned,
         [](Setup& s, sim::Command&) {
             s.spec.islands[0].owner_runs = {{15, 30U * 120U + 50U}, {1, 1}};
             s.spec.islands[0].cities.push_back(syn::plain_city(1, 1));
         }},
        {"the issuer already has a city on the island", sim::Reject::OwnCityOnIsland,
         [](Setup& s, sim::Command&) {
             s.spec.islands[0].cities.push_back(syn::plain_city(3, 0));
         }},
        {"no shore: every rotation fails the coast rule", sim::Reject::NoRotationFits,
         [](Setup& s, sim::Command&) {
             s.spec.islands[0].terrain =
                 sim::make_terrain(std::vector<std::uint8_t>(120U * 64U, 0x40), {}, {});
         }},
        {"a footprint tile other than the origin belongs to another city",
         sim::Reject::NoRotationFits,
         [](Setup& s, sim::Command&) {
             s.spec.islands[0].owner_runs = {{15, 33U * 120U + 53U}, {1, 1}};
             s.spec.islands[0].cities.push_back(syn::plain_city(1, 1));
         }},
        {"an obstruction object on a footprint tile", sim::Reject::NoRotationFits,
         [](Setup& s, sim::Command&) {
             s.spec.islands[0].objects.push_back({31U * 120U + 51U, kRoadObj.v, 0});
         }},
        {"a neutral soldier stands inside the footprint", sim::Reject::FigureInTheWay,
         [](Setup& s, sim::Command&) {
             s.spec.figures.push_back(soldier_at(1, 51.5, 31.5)); // relation 0: point rule
         }},
        {"a hostile soldier within 7 of a corner", sim::Reject::FigureInTheWay,
         [](Setup& s, sim::Command&) {
             s.spec.players[0].relations = {sim::Relation{sim::PlayerId{1}, 0, 0, 3, 0}};
             s.spec.figures.push_back(soldier_at(1, 58, 30)); // 4 east of corner (54, 30)
         }},
        {"gold 349 of 350", sim::Reject::CannotAfford,
         [](Setup& s, sim::Command&) { s.spec.players[0].money = 349; }},
        {"wood 11 t of 12", sim::Reject::CannotAfford,
         [](Setup& s, sim::Command&) { s.spec.figures[0].holds[0].amount_88 = 11 * kT; }},
        {"tools 4 t of 5 (two holds sum to 4)", sim::Reject::CannotAfford,
         [](Setup& s, sim::Command&) {
             s.spec.figures[0].holds[2].amount_88 = 2 * kT;
             s.spec.figures[0].holds[3].amount_88 = 2 * kT;
         }},
        {"the ring has no slot (15 cities, slot 15 is the neutral value)", sim::Reject::NoCitySlot,
         [](Setup& s, sim::Command&) {
             for (std::uint32_t n = 0; n < 15; ++n) {
                 s.spec.islands[0].cities.push_back(syn::plain_city(n, 1));
             }
         }},
    };
    CHECK(rows.size() == 27);
    for (const Row& row : rows) {
        INFO("row: " << row.name);
        Setup s = base_setup();
        sim::Command c = control_command();
        row.edit(s, c);
        sim::World w = make(s);
        rejected(w, c, row.want);
    }
}

TEST_CASE("founding: valid controls next to the refusals (the refused input, varied, passes)") {
    // Scenery that may be overlapped is replaced by the warehouse (flag 0x20, contract 12.1).
    {
        Setup s = base_setup();
        s.spec.islands[0].objects.push_back({31U * 120U + 51U, kTree.v, 0});
        sim::World w = make(s);
        REQUIRE(w.apply(control_command()).ok());
        CHECK(isl(w).tiles.object(31U * 120U + 51U) == 1302);
    }
    // A state word with only bit 0x08 does not disable the ship (0x02 alone does).
    {
        Setup s = base_setup();
        s.spec.figures[0].state_word = 0x08;
        succeeds(s);
    }
    // Another player's city elsewhere on the island is not a reason to refuse.
    {
        Setup s = base_setup();
        s.spec.islands[0].cities.push_back(syn::plain_city(1, 1));
        succeeds(s);
    }
    // Duplicate holds: wood 5 + 10 + 20 t pays 12 t, tools 2 + 3 t pays 5 t.
    {
        Setup s = base_setup();
        s.spec.figures[0].holds = {sim::ShipHold{kWood, 5 * kT}, sim::ShipHold{kWood, 10 * kT},
                                   sim::ShipHold{kWood, 20 * kT}, sim::ShipHold{kTools, 2 * kT},
                                   sim::ShipHold{kTools, 3 * kT}};
        sim::World w = make(s);
        REQUIRE(w.apply(control_command()).ok());
        const sim::Figure* ship = sim::find_figure(w.figures(), sim::FigureId{1});
        CHECK(ship->holds[0].amount_88 == 0);      // emptied first
        CHECK(ship->holds[1].amount_88 == 3 * kT); // 10 t - the remaining 7 t
        CHECK(ship->holds[2].amount_88 == 20 * kT);
        CHECK(ship->holds[3].amount_88 == 0);
        CHECK(ship->holds[4].amount_88 == 0);
    }
    // A soldier of a non-hostile player outside the rectangle; the same soldier with relation 3
    // 8 tiles from the nearest corner (range 7); with property 0x1c = 0x11 the hostile one is
    // skipped even at 4 tiles; a war machine without crew is not usable and so not hostile-ranged.
    {
        Setup s = base_setup();
        s.spec.figures.push_back(soldier_at(1, 58, 30));
        succeeds(s); // relation 0, outside the rectangle
    }
    {
        Setup s = base_setup();
        s.spec.players[0].relations = {sim::Relation{sim::PlayerId{1}, 0, 0, 3, 0}};
        s.spec.figures.push_back(soldier_at(1, 62.5, 30)); // 8.5 from the corner (54, 30)
        succeeds(s);
    }
    {
        Setup s = base_setup();
        s.spec.players[0].relations = {sim::Relation{sim::PlayerId{1}, 0, 0, 3, 0}};
        s.spec.players[1].volk = 0x11;
        s.spec.figures.push_back(soldier_at(1, 58, 30));
        succeeds(s);
    }
    {
        Setup s = base_setup();
        s.spec.players[0].relations = {sim::Relation{sim::PlayerId{1}, 0, 0, 3, 0}};
        s.spec.figures.push_back(soldier_at(1, 58, 30, 9502)); // a machine, no crew
        succeeds(s);
    }
    {
        Setup s = base_setup();
        s.spec.players[0].relations = {sim::Relation{sim::PlayerId{1}, 0, 0, 3, 0}};
        s.spec.figures.push_back(soldier_at(1, 58, 30, 9502));
        s.spec.figures.push_back(soldier_at(1, 58, 30)); // a crew member (child)
        s.spec.figures.back().parent = 3;                // index of the machine in `figures`
        s.spec.figures.back().crew_slot = 0;
        sim::World w = make(s);
        CHECK(w.apply(control_command()).status == sim::Reject::FigureInTheWay);
    }
    // A missing fact is a refusal, not a pass: the hostile owner's player slot is gone.
    {
        Setup s = base_setup();
        s.spec.players[0].relations = {sim::Relation{sim::PlayerId{2}, 0, 0, 3, 0}};
        s.spec.figures.push_back(soldier_at(2, 58, 30));
        sim::World w = make(s);
        sim::WorldTestAccess::players(w).slots[2].reset();
        rejected(w, control_command(), sim::Reject::FigureFactsMissing);
    }
}

namespace {

/// A taller island (120 x 200) with the same coast column x = 50 and two expeditions: player 0's at
/// the control site (rows 30..33) and player 1's ships (figures 4, 5, 6) at (57.5, y + 2.5) of
/// the site `y1` it will target. Player 1 has 30000 gold and the same four holds.
Setup tall_setup(std::uint32_t site1_y, std::uint32_t height = 200) {
    Setup s = base_setup();
    sim::ScenarioSpec::IslandSpec& is = s.spec.islands[0];
    is.h = height;
    std::vector<std::uint8_t> heights(120U * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < 120; ++x) {
            heights[y * 120 + x] = x <= 49 ? 0x40 : (x == 50 ? 0x30 : 0x10);
        }
    }
    is.terrain = sim::make_terrain(std::move(heights), {}, {});
    s.spec.players[1].money = 30000;
    sim::ScenarioSpec::FigureSpec ship = s.spec.figures[0];
    ship.owner = sim::PlayerId{1};
    ship.pos.y = static_cast<std::int32_t>(site1_y * kT + 2 * kT + kT / 2);
    ship.parent = std::nullopt;
    s.spec.figures.push_back(ship);
    sim::ScenarioSpec::FigureSpec scout = s.spec.figures[1];
    scout.owner = sim::PlayerId{1};
    scout.pos = ship.pos;
    scout.parent = 3;
    s.spec.figures.push_back(scout);
    sim::ScenarioSpec::FigureSpec donkey = s.spec.figures[2];
    donkey.owner = sim::PlayerId{1};
    donkey.pos = ship.pos;
    donkey.parent = 4;
    s.spec.figures.push_back(donkey);
    return s;
}

/// Player 1's command to found at origin (50, y) with its ship (figure 4).
sim::Command command_p1(std::uint32_t y) {
    sim::Command c = control_command(1);
    found_of(c).ship = sim::FigureId{4};
    found_of(c).placement.y = static_cast<std::int32_t>(y);
    found_of(c).click_y = static_cast<std::int32_t>(y + 1);
    return c;
}

} // namespace

TEST_CASE("founding conflicts: the same site in one turn, in both orders") {
    for (const bool p0_first : {true, false}) {
        INFO("order: " << (p0_first ? "player 0 first" : "player 1 first"));
        Setup s = tall_setup(30); // player 1's ship is next to the same site
        sim::World w = make(s);
        const sim::Command c0 = control_command(0);
        const sim::Command c1 = command_p1(30);
        const sim::Command& first = p0_first ? c0 : c1;
        const sim::Command& second = p0_first ? c1 : c0;
        const std::uint32_t winner = p0_first ? 0 : 1, loser = p0_first ? 1 : 0;
        REQUIRE(w.apply(first).ok());
        const auto* const loser_player = w.players().find(sim::PlayerId{loser});
        REQUIRE(loser_player != nullptr);
        const std::int64_t loser_gold = loser_player->money;
        const std::vector<sim::ShipHold> loser_holds =
            sim::find_figure(w.figures(), sim::FigureId{loser == 0 ? 1U : 4U})->holds;
        // The loser sees owned tiles: SiteOwned, uncharged, nothing changed.
        rejected(w, second, sim::Reject::SiteOwned);
        CHECK(w.players().find(sim::PlayerId{loser})->money == loser_gold);
        CHECK(sim::find_figure(w.figures(), sim::FigureId{loser == 0 ? 1U : 4U})->holds ==
              loser_holds);
        CHECK(w.players().find(sim::PlayerId{winner})->money == 29650);
        CHECK(isl(w).cities[0]->owner == sim::PlayerId{winner});
        CHECK_FALSE(isl(w).cities[1].has_value());
        CHECK(owned_by(w, 0) == 1976);
    }
}

TEST_CASE("founding conflicts: two distant sites on one island both succeed, in either order") {
    // Sites rows 30 and 150 on the coast column: the discs (rows 7..56 and 127..176) do not meet
    // and the reveal rectangle of the second (rows 119..183) holds no tile of the first city.
    for (const bool p0_first : {true, false}) {
        INFO("order: " << (p0_first ? "player 0 first" : "player 1 first"));
        Setup s = tall_setup(150);
        sim::World w = make(s);
        const sim::Command c0 = control_command(0);
        const sim::Command c1 = command_p1(150);
        REQUIRE(w.apply(p0_first ? c0 : c1).ok());
        REQUIRE(w.apply(p0_first ? c1 : c0).ok());
        const std::uint32_t first = p0_first ? 0 : 1, second = p0_first ? 1 : 0;
        // City numbers follow the order of the commands (cursor -1, then 0, then 1); the names are
        // the next two table entries: T[1235] = 30195 -> 3 ("Delta"), T[1236] mod 4 below.
        CHECK(isl(w).cities[0]->owner == sim::PlayerId{first});
        CHECK(isl(w).cities[1]->owner == sim::PlayerId{second});
        CHECK(isl(w).city_cursor == 1);
        CHECK(isl(w).kontor_counter == 3);
        CHECK(isl(w).cities[0]->name == "Delta");
        CHECK(w.rng().cursor() == 1236);
        CHECK(owned_by(w, 0) == 1976);
        CHECK(owned_by(w, 1) == 1976);
        CHECK(w.players().find(sim::PlayerId{0})->money == 29650);
        CHECK(w.players().find(sim::PlayerId{1})->money == 29650);
        // No reveal: neither city's mask learned the other player, nobody discovered the other.
        CHECK(isl(w).cities[0]->player_mask.size() == 1);
        CHECK(isl(w).cities[1]->player_mask.size() == 1);
        CHECK(w.players().find(sim::PlayerId{0})->discovery.size() == 1);
        CHECK(w.players().find(sim::PlayerId{1})->discovery.size() == 1);
        CHECK(isl(w).players_present.size() == 2);
        CHECK(w.check_invariants().empty());
    }
}

TEST_CASE(
    "founding conflicts: a nearby second site keeps the first player's tiles and reveals him") {
    // Player 1 founds at (50, 62): the footprint rows 62..65 are neutral (player 0's disc ends at
    // row 56), its disc rows 39..88 overlaps player 0's rows 39..56, which stay player 0's.
    Setup s = tall_setup(62);
    sim::World w = make(s);
    REQUIRE(w.apply(control_command(0)).ok());
    const std::uint64_t before0 = owned_by(w, 0);
    REQUIRE(before0 == 1976);
    REQUIRE(w.apply(command_p1(62)).ok());
    CHECK(owned_by(w, 0) == before0); // never overwritten
    CHECK(isl(w).tiles.owner(40 * 120ULL + 50) == 0);
    CHECK(isl(w).tiles.owner(60 * 120ULL + 50) == 1); // rows 57+ went to the new city
    CHECK(isl(w).tiles.owner(56 * 120ULL + 50) == 0);
    // The reveal (centre (51, 63), radius 32) saw city 0 (e.g. tile (50, 34), dy 29 <= 32,
    // half-width 13 >= dx 1) and visited it: city 0's mask gained player 1, player 1 learned
    // player 0 (property 7), and the recount: player 1 is in city 0's mask, so it learns
    // owner 0; player 0 is in no mask of city 1.
    CHECK(isl(w).cities[0]->player_mask ==
          std::vector<sim::PlayerId>{sim::PlayerId{0}, sim::PlayerId{1}});
    CHECK(isl(w).cities[1]->player_mask == std::vector<sim::PlayerId>{sim::PlayerId{1}});
    CHECK(w.players().find(sim::PlayerId{1})->discovery ==
          std::vector<sim::PlayerId>{sim::PlayerId{0}, sim::PlayerId{1}});
    CHECK(w.players().find(sim::PlayerId{0})->discovery ==
          std::vector<sim::PlayerId>{sim::PlayerId{0}});
    CHECK(w.check_invariants().empty());
}

TEST_CASE("founding: registration marks the peers whose MILIT relation is 3") {
    Setup s = base_setup();
    s.spec.players[0].relations = {sim::Relation{sim::PlayerId{1}, 3, 3, 0, 0},
                                   sim::Relation{sim::PlayerId{2}, 3, 2, 0, 0}};
    sim::World w = make(s);
    REQUIRE(w.apply(control_command()).ok());
    // Player 1's MILIT is 3: in the mask. Player 2's MILIT is 2 (TRADE 3 does not count): not.
    CHECK(isl(w).cities[0]->player_mask ==
          std::vector<sim::PlayerId>{sim::PlayerId{0}, sim::PlayerId{1}});
    // The recount: both members of the mask learn the owner of the city.
    CHECK(w.players().find(sim::PlayerId{0})->discovery ==
          std::vector<sim::PlayerId>{sim::PlayerId{0}});
    CHECK(w.players().find(sim::PlayerId{1})->discovery ==
          std::vector<sim::PlayerId>{sim::PlayerId{0}});
    CHECK(w.players().find(sim::PlayerId{2})->discovery.empty());
}

TEST_CASE("founding: save and load right after founding, ordinary play continues in the new city") {
    Setup s = base_setup();
    sim::World w = make(s);
    REQUIRE(w.apply(control_command()).ok());
    const std::vector<std::uint8_t> bytes = w.save();
    auto loaded = sim::World::load(bytes, registry());
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    CHECK(loaded.value().state_hash() == w.state_hash());
    CHECK(loaded.value().save() == bytes);
    CHECK(loaded.value().check_invariants().empty());
    CHECK(loaded.value().rng().cursor() == 1235);
    // The hash does not depend on the executor.
    for (const unsigned threads : {1U, 2U, 4U}) {
        sim::World copy = w;
        island_test::ThreadExecutor ex(threads);
        copy.prepare_hash(ex);
        CHECK(copy.state_hash() == w.state_hash());
    }
    // An ordinary Build in the new city: a free workshop on an owned, empty tile.
    sim::BuildCmd b;
    b.city = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    b.placements = {sim::PlacementWire{kWeaver, 46, 30, 0, 0, 0}};
    const sim::ApplyResult br = loaded.value().apply(sim::Command{sim::PlayerId{0}, b});
    REQUIRE(br.ok());
    CHECK(loaded.value().check_invariants().empty());
    // The same Build in a world that was not saved and loaded gives the same hash.
    REQUIRE(w.apply(sim::Command{sim::PlayerId{0}, b}).ok());
    CHECK(loaded.value().state_hash() == w.state_hash());
    // The warehouse and the new building tick without breaking anything.
    for (int i = 0; i < 300; ++i) {
        w.advance(100);
        loaded.value().advance(100);
    }
    CHECK(w.state_hash() == loaded.value().state_hash());
    CHECK(w.check_invariants().empty());
    // The same command again hits his own tiles first (SiteOwned precedes OwnCityOnIsland in the
    // refusal order, contract 16.4 step 4).
    rejected(w, control_command(), sim::Reject::SiteOwned);
}

namespace {

/// Everything one pass of the recorded session produced, for comparison between passes.
struct Session {
    std::vector<sim::Reject> statuses; // the status of every submitted command, in order
    std::vector<std::uint64_t> hashes; // the state hash after every step, prepared on `ex`
    sim::ReplayLog log;
    std::uint64_t final_hash = 0;
};

/// The session of the recorded-replay tests, run on a world CREATED on `ex` (island content is
/// built in parallel jobs there) with every hash prepared on `ex` (chunk digests in parallel
/// jobs). The simulation itself (apply, advance) is serial; only world creation and hashing take
/// an executor, and those are what this exercises.
Session run_session(const Setup& s, const sim::Executor& ex) {
    auto created = sim::World::create(s.spec, registry(), s.cfg, s.rules, {}, ex);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World w = std::move(created).value();
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    Session out;
    const auto hash = [&] {
        w.prepare_hash(ex);
        out.hashes.push_back(w.state_hash());
    };
    const auto submit = [&](const sim::Command& c) {
        out.statuses.push_back(rec.submit(c).status);
        hash();
    };
    rec.checkpoint();
    hash();
    submit(control_command(0));
    submit(command_p1(30)); // player 1's ship is 31 tiles away: OutOfRange
    submit(command_p1(62)); // a nearby site: the reveal visits city 0
    rec.advance(5000);
    hash();
    rec.checkpoint();
    sim::Command bad = control_command(0);
    found_of(bad).placement.rotation = 9;
    submit(bad);                // BadRotation
    submit(control_command(0)); // SiteOwned
    rec.advance(5000);
    hash();
    rec.checkpoint();
    out.log = rec.log();
    out.final_hash = w.state_hash();
    return out;
}

} // namespace

TEST_CASE("founding replay: byte round trip and in-process replay of a recorded session") {
    // Claims, each with its own evidence: (1) the statuses of the submitted commands; (2) the log
    // survives write_log / read_log unchanged; (3) replay() of the read log into a FRESH world
    // reproduces all 3 checkpoints and the final hash. In-process, serial: not a fresh process and
    // not parallel simulation (the next case covers executor creation and hashing; the CLI fixture
    // a fresh process).
    Setup s = tall_setup(62);
    const Session ref = run_session(s, sim::serial_executor());
    const std::vector<sim::Reject> want = {sim::Reject::None, sim::Reject::OutOfRange,
                                           sim::Reject::None, sim::Reject::BadRotation,
                                           sim::Reject::SiteOwned};
    CHECK(ref.statuses == want);
    auto bytes = sim::write_log(ref.log);
    REQUIRE_MESSAGE(bytes.ok(), (bytes.ok() ? std::string() : bytes.error()));
    auto read = sim::read_log(bytes.value());
    REQUIRE_MESSAGE(read.ok(), (read.ok() ? std::string() : read.error()));
    CHECK(read.value() == ref.log);
    sim::World fresh = make(s);
    auto outcome = sim::replay(fresh, read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(outcome.value().checkpoints_total == 3);
    CHECK(outcome.value().checkpoints_matched == 3);
    CHECK(fresh.state_hash() == ref.final_hash);
    // A determinism pin across processes and platforms (not an oracle: the exact values are
    // asserted by hand in the cases above).
    std::string update_line;
    const std::string problem =
        anno::test::check_golden("m2_found_session", "M2", ref.final_hash, update_line);
    if (anno::test::update_mode()) {
        MESSAGE("golden line: " << update_line);
    }
    CHECK(problem == "");
}

TEST_CASE("founding replay: world creation and hashing on 1, 2, 4 and 7 executor threads agree") {
    // The executor-taking operations are World::create (island content in parallel jobs) and
    // prepare_hash (chunk digests in parallel jobs). For each thread count a FRESH world is
    // created on that executor and the whole session is run on it: command results, the hash after
    // every step, the recorded log and the final hash equal the serial reference's. apply() and
    // advance() are serial by design; this is not a claim about parallel simulation.
    Setup s = tall_setup(62);
    const Session ref = run_session(s, sim::serial_executor());
    REQUIRE(ref.hashes.size() == 8); // checkpoint, 3 commands, advance, 2 commands, advance
    for (const unsigned threads : {1U, 2U, 4U, 7U}) {
        INFO("threads " << threads);
        island_test::ThreadExecutor ex(threads);
        const Session got = run_session(s, ex);
        // The executor really was used: create() ran its island jobs on it and every hash was
        // prepared on it (8 hashes, 1 creation, plus the chunk jobs each of them runs).
        CHECK(ex.calls() >= 9);
        CHECK(ex.jobs() >= ex.calls());
        CHECK(got.statuses == ref.statuses);
        CHECK(got.hashes == ref.hashes);
        CHECK(got.log == ref.log);
        CHECK(got.final_hash == ref.final_hash);
    }
}

#include <anno/sim/founding.hpp>
#include <anno/sim/territory.hpp>
#include <anno/sim/walk_cells.hpp>

TEST_CASE("founding: the reveal visits only the lowest-numbered kept city, and the early-out") {
    // A 64 x 64 island at (0, 0), centre (32, 32), radius 32. Samples: x and y in {2, 10, 18, 26,
    // 34, 42, 50, 58} plus the +4 offsets. Half-width table T[k] for radius 32 is the Bresenham
    // profile (T[2] = 31, T[26] = 18 by sqrt(32^2 - k^2) truncated).
    const sim::SimConfig cfg;
    sim::Island is;
    REQUIRE(is.tiles.reset(0, 0, 64, 64, 8));
    for (std::uint64_t i = 0; i < is.tiles.tile_count(); ++i) {
        is.tiles.set_owner(i, cfg.neutral_owner);
    }
    is.cities.resize(cfg.city_slots);
    const sim::PlayerId founder{7};
    for (const std::uint32_t n : {3U, 5U, 9U}) {
        is.cities[n] = sim::make_city(sim::CityNo{n}, sim::PlayerId{1}, cfg);
    }
    const auto at = [](std::uint32_t x, std::uint32_t y) { return y * 64ULL + x; };
    is.tiles.set_owner(at(34, 34), 5); // sample (xl=34, yl=34): dx 2, dy 2: kept
    is.tiles.set_owner(at(6, 34), 3);  // sample (xl+4 = 6, yl=34): dx 26 <= T[2], dy 2: kept
    is.tiles.set_owner(at(58, 58), 9); // dx 26 > T[26] = 18: not kept
    const auto profile = sim::territory::disc_profile(32, 32);
    REQUIRE(profile.has_value());
    const auto reveal = [&]() {
        return sim::Founding::reveal_city(is, cfg.neutral_owner, founder, 32, 32, 32, *profile);
    };
    // City 5 is nearer, city 3 is lower: the original visits the lowest number only.
    REQUIRE(reveal().has_value());
    CHECK(*reveal() == 3);
    // A city whose mask already holds the founder is not kept: the next lowest is.
    is.cities[3]->player_mask = {founder};
    CHECK(*reveal() == 5);
    // Out of reach is not kept even when it is the only unknown city.
    is.cities[5]->player_mask = {founder};
    CHECK_FALSE(reveal().has_value()); // city 9 lacks the founder but its sample is out of reach
    // Early-out: every present city knows the founder (an island with only the new city is this
    // case too): nothing is visited whatever the tiles hold.
    is.cities[9]->player_mask = {founder};
    CHECK_FALSE(reveal().has_value());
    // The neutral value is never a city; a sample on it is skipped even if a city had that number.
    is.cities[3]->player_mask.clear();
    is.tiles.set_owner(at(6, 34), cfg.neutral_owner);
    CHECK_FALSE(reveal().has_value());
    // The distance test is inclusive on dy: a sample exactly r = 32 rows away passes, 33 does not.
    is.tiles.set_owner(at(34, 0), 3);  // sample (34, 2)? no: use a hand row: yl=2 gives dy 30
    CHECK_FALSE(reveal().has_value()); // (34, 0) is not a sample tile at all
    is.tiles.set_owner(at(34, 2), 3);  // sample (xl=34, yl=2): dy 30, T[30] = 11, dx 2: kept
    CHECK(*reveal() == 3);
}

TEST_CASE("founding: the initial rotation only starts the trials, the accepted one is built") {
    // Contract 13.2: rotation 1's shore edge is the left column x = 50 (all 0x30). Rotations 0
    // (top row), 2 (bottom row) and 3 (right column) need a 0x30 edge that the grid lacks, so
    // from every start the first accepted rotation in the order r, r+1, r+2, r+3 is 1.
    for (const int start : {0, 1, 2, 3}) {
        INFO("initial rotation " << start);
        Setup s = base_setup();
        sim::World w = make(s);
        sim::Command c = control_command();
        found_of(c).placement.rotation = static_cast<std::uint8_t>(start);
        const sim::ApplyResult r = w.apply(c);
        REQUIRE(r.ok());
        CHECK(isl(w).find_building(r.records[0].building)->rotation == 1);
    }
}

TEST_CASE("founding: the reveal reaches R + 8 = 32 rows, not R = 24") {
    // Player 0 founds at (50, 30): its disc owns rows 7..56. Player 1 founds at origin (50, 84),
    // click (51, 85): the centre is (51, 85) ((4 - 1) / 2 = 1), radius 32, ys = floor8(85 - 32) =
    // 48, sample rows 50, 58, ... and +4: the row-54 sample (50, 54) is p0's (disc row 54 = 33 +
    // 21, half-width 11), dy = 31 <= 32, T[31] >= 1 >= dx 1: kept, so the reveal visits city 0.
    // With radius 24 it would be 31 > 24 and nothing would be visited.
    {
        Setup s = tall_setup(84);
        sim::World w = make(s);
        REQUIRE(w.apply(control_command(0)).ok());
        REQUIRE(w.apply(command_p1(84)).ok());
        CHECK(isl(w).cities[0]->player_mask ==
              std::vector<sim::PlayerId>{sim::PlayerId{0}, sim::PlayerId{1}});
    }
    // Origin (50, 90), centre y 91: the nearest p0 tile any sample reads is row 56, 35 > 32 rows
    // away (and the sample rows 58, 66, ... are not p0's): not visited.
    {
        Setup s = tall_setup(90);
        sim::World w = make(s);
        REQUIRE(w.apply(control_command(0)).ok());
        REQUIRE(w.apply(command_p1(90)).ok());
        CHECK(isl(w).cities[0]->player_mask == std::vector<sim::PlayerId>{sim::PlayerId{0}});
        CHECK(w.players().find(sim::PlayerId{1})->discovery ==
              std::vector<sim::PlayerId>{sim::PlayerId{1}});
    }
}

TEST_CASE("founding: a non-square type, the accepted rotation decides the footprint and the scan") {
    // Type 9301 is 6 wide and 3 tall. Click (52, 31), initial rotation 0: origin
    // (52 - (6 - 1) / 2, 31 - (3 - 1) / 2) = (50, 30) from the INITIAL footprint 6 x 3. Rotation 0
    // needs a 0x30 top row (0x30, 0x10, 0x10, ...): refused; rotation 1 (3 wide, 6 tall) has the
    // shore column x = 50 (7 vertices, all 0x30) and the far column x = 53 sea: accepted. The
    // building stands on x 50..52, y 30..35 and nowhere else.
    Setup s = base_setup();
    s.rules.founding_types = {1302, 9301};
    sim::World w = make(s);
    sim::Command c = control_command();
    found_of(c).placement.type = kKontor43;
    found_of(c).placement.rotation = 0;
    found_of(c).click_x = 52;
    const sim::ApplyResult r = w.apply(c);
    REQUIRE(r.ok());
    const sim::Building* b = isl(w).find_building(r.records[0].building);
    REQUIRE(b != nullptr);
    CHECK(b->rotation == 1);
    for (std::uint32_t y = 30; y < 36; ++y) {
        for (std::uint32_t x = 50; x < 56; ++x) {
            CHECK(isl(w).tiles.object(y * 120ULL + x) == (x < 53 ? 9301U : 0U));
        }
    }
    // The original scans B x B = 3 x 3 (x 50..52, y 30..32) for an odd rotation (AnnoFrame
    // 0x100450d5): a neutral soldier on row 35 of the real 3 x 6 footprint is NOT seen, one on row
    // 32 is.
    for (const double y : {35.5, 32.5}) {
        Setup t = base_setup();
        t.rules.founding_types = {1302, 9301};
        t.spec.figures.push_back(soldier_at(1, 51.5, y));
        sim::World v = make(t);
        const sim::ApplyResult q = v.apply(c);
        CHECK(q.status == (y > 33.0 ? sim::Reject::None : sim::Reject::FigureInTheWay));
    }
}

TEST_CASE("founding: the reveal centre is origin + (w - 1) / 2, (h - 1) / 2, by hand") {
    using sim::Founding;
    CHECK(Founding::reveal_centre(50, 30, 4, 4) == std::pair<std::int64_t, std::int64_t>{51, 31});
    CHECK(Founding::reveal_centre(50, 30, 3, 6) == std::pair<std::int64_t, std::int64_t>{51, 32});
    CHECK(Founding::reveal_centre(10, 10, 6, 3) == std::pair<std::int64_t, std::int64_t>{12, 11});
    CHECK(Founding::reveal_centre(0, 0, 1, 1) == std::pair<std::int64_t, std::int64_t>{0, 0});
    CHECK(Founding::reveal_centre(7, 9, 2, 2) == std::pair<std::int64_t, std::int64_t>{7, 9});
}

TEST_CASE("founding: the reveal centre follows the ACCEPTED rotation, not the initial footprint") {
    // Player 1 founds the 6 x 3 type with initial rotation 0, click (52, 86): origin (50, 85).
    // Rotation 1 is accepted (3 wide, 6 tall), so the centre is (50 + 1, 85 + 2) = (51, 87), not
    // the initial footprint's (52, 86). Sample (50, 54) of player 0's disc: dy 33 > 32 from the
    // accepted centre (not visited); from the initial centre it would be dy 32 <= 32 with
    // T[32] = 5 >= dx 1 (visited).
    Setup s = tall_setup(85);
    s.rules.founding_types = {1302, 9301};
    sim::World w = make(s);
    REQUIRE(w.apply(control_command(0)).ok());
    sim::Command c = command_p1(85);
    found_of(c).placement.type = kKontor43;
    found_of(c).placement.rotation = 0;
    found_of(c).click_x = 52;
    found_of(c).click_y = 86;
    const sim::ApplyResult r = w.apply(c);
    REQUIRE(r.ok());
    CHECK(isl(w).find_building(r.records[0].building)->rotation == 1);
    CHECK(isl(w).cities[0]->player_mask == std::vector<sim::PlayerId>{sim::PlayerId{0}});
}

namespace {

/// One character per 2 x 4 tiles for x 30..79: the owner digit, '.' for neutral, 'K' on a
/// warehouse.
std::string ownership_diagram(const sim::World& w, std::uint32_t y0, std::uint32_t y1) {
    const sim::Island& is = isl(w);
    std::string out;
    for (std::uint32_t y = y0; y < y1; y += 4) {
        for (std::uint32_t x = 30; x < 80; x += 2) {
            const std::uint64_t idx = y * 120ULL + x;
            char c = '.';
            if (is.tiles.owner(idx) != w.config().neutral_owner) {
                c = static_cast<char>('0' + is.tiles.owner(idx));
            }
            if (is.tiles.object(idx) == 1302) {
                c = 'K';
            }
            out.push_back(c);
        }
        out.push_back('\n');
    }
    return out;
}

std::string holds_text(const sim::World& w, std::uint64_t ship) {
    std::string t;
    for (const sim::ShipHold& h : sim::find_figure(w.figures(), sim::FigureId{ship})->holds) {
        t += " [goods " + std::to_string(h.goods) + ": " + std::to_string(h.amount_88) + "/256 t]";
    }
    return t;
}

} // namespace

TEST_CASE("founding demonstration: command, result, territory, a conflict and the replay") {
    // The assertions always run; ANNO_FOUNDING_DEMO=1 prints the demonstration:
    //   ANNO_FOUNDING_DEMO=1 anno_sim_tests -tc='founding demonstration*'
    const bool print = std::getenv("ANNO_FOUNDING_DEMO") != nullptr;
    std::ostringstream o;
    Setup s = tall_setup(30); // player 1's ship #4 lies next to the same site as player 0's
    // A second ship of player 1 (#7) beside the site (50, 62), for its later, legal founding.
    sim::ScenarioSpec::FigureSpec second = s.spec.figures[3];
    second.pos.y = static_cast<std::int32_t>(62 * kT + 2 * kT + kT / 2);
    s.spec.figures.push_back(second);
    sim::World w = make(s);
    auto started = sim::Recorder::start(w);
    REQUIRE(started.ok());
    sim::Recorder rec = std::move(started).value();
    rec.checkpoint();

    o << "== world before founding (tick 0, seed 1234) ==\n"
      << "island 0: 120 x 200 tiles, coast column x = 50 (land west, sea east), all neutral\n"
      << "player 0: gold " << w.players().find(sim::PlayerId{0})->money
      << ", ship #1 at (57.5, 32.5)," << holds_text(w, 1) << "\n"
      << "player 1: gold " << w.players().find(sim::PlayerId{1})->money
      << ", ship #4 at (57.5, 32.5), ship #7 at (57.5, 64.5)\n"
      << "state hash " << anno::test::hex16(w.state_hash()) << "\n\n";

    const sim::Command c0 = control_command(0);
    const auto& f0 = std::get<sim::FoundCmd>(c0.body);
    o << "== command: issuer 0, FoundCmd ship " << f0.ship.v << ", type " << f0.placement.type.v
      << ", origin (" << f0.placement.x << ", " << f0.placement.y << "), rotation "
      << int{f0.placement.rotation} << ", click (" << f0.click_x << ", " << f0.click_y << ")\n";
    const auto wire = sim::encode_record(c0.body);
    REQUIRE(wire.ok());
    o << "wire record (" << wire.value().size() << " bytes):";
    for (const std::uint8_t b : wire.value()) {
        char hex[4];
        std::snprintf(hex, sizeof hex, " %02x", b);
        o << hex;
    }
    o << "\n";
    const sim::ApplyResult r0 = rec.submit(c0);
    REQUIRE(r0.ok());
    const sim::Building* b0 = isl(w).find_building(r0.records[0].building);
    REQUIRE(b0 != nullptr);
    o << "== result: " << sim::reject_name(r0.status) << "\n"
      << "city 0 of player 0, name '" << isl(w).cities[0]->name << "'; warehouse type "
      << b0->type.v << " at (" << b0->x << ", " << b0->y << "), rotation " << int{b0->rotation}
      << ", byte0f 0x" << std::hex << int{b0->byte0f} << std::dec << ", KONTID " << b0->kontor_id
      << "\n"
      << "gold " << w.players().find(sim::PlayerId{0})->money
      << " (-350); ship #1 holds:" << holds_text(w, 1) << "\nPRNG cursor " << w.rng().cursor()
      << " (one draw); tiles of city 0: " << owned_by(w, 0)
      << "\nterritory (x 30..79 in steps of 2, rows 0..63 in steps of 4):\n"
      << ownership_diagram(w, 0, 64) << "\n";

    const std::int64_t gold1 = w.players().find(sim::PlayerId{1})->money;
    const sim::ApplyResult rc = rec.submit(command_p1(30));
    o << "== conflicting command: player 1 with ship #4 targets the same site ==\nresult: "
      << sim::reject_name(rc.status) << "; player 1 gold "
      << w.players().find(sim::PlayerId{1})->money << " (was " << gold1 << ")\n\n";
    sim::Command c1 = command_p1(62);
    found_of(c1).ship = sim::FigureId{7};
    const sim::ApplyResult r1 = rec.submit(c1);
    o << "== player 1 founds at (50, 62) with ship #7: " << sim::reject_name(r1.status) << " ==\n"
      << "city 1 owned by player " << isl(w).cities[1]->owner.v
      << "; the reveal added player 1 to city 0's mask (" << isl(w).cities[0]->player_mask.size()
      << " players); player 1 discovered " << w.players().find(sim::PlayerId{1})->discovery.size()
      << " owners\n"
      << ownership_diagram(w, 0, 100) << "\n";
    rec.advance(5000);
    rec.checkpoint();
    const sim::ReplayLog log = rec.log();
    sim::World fresh = make(s);
    auto outcome = sim::replay(fresh, log);
    REQUIRE(outcome.ok());
    o << "== replay of the recorded session (" << log.entries.size()
      << " entries) in a fresh world ==\n"
      << "checkpoints " << outcome.value().checkpoints_matched << "/"
      << outcome.value().checkpoints_total << " matched; final hash "
      << anno::test::hex16(fresh.state_hash()) << " (live world "
      << anno::test::hex16(w.state_hash()) << ")\n";
    CHECK(rc.status == sim::Reject::SiteOwned);
    CHECK(w.players().find(sim::PlayerId{1})->money == gold1 - 350);
    CHECK(r1.ok());
    CHECK(isl(w).cities[0]->player_mask.size() == 2);
    CHECK(outcome.value().reproduced());
    CHECK(outcome.value().checkpoints_matched == 2);
    CHECK(fresh.state_hash() == w.state_hash());
    if (print) {
        std::cout << o.str();
    }
}

#include <anno/sim/draw.hpp>
#include <anno/sim/tick.hpp>

TEST_CASE("founding A1: only a child with a crew slot crews a war machine") {
    // A hostile (WAR 3) war machine of player 1, kind 9502 (BEWOHNTYPE 108), stands 4 tiles east of
    // the footprint's corner (54, 30), inside the machine's range 7. Without a crew it is not
    // usable, so it takes the point/hull branch, stands outside the rectangle and does not block.
    // A child that is merely CARRIED (a parent link, no crew slot) is containment, not crew.
    const auto fixture = [](int child) {
        Setup s = base_setup();
        s.spec.players[0].relations = {sim::Relation{sim::PlayerId{1}, 0, 0, 3, 0}};
        s.spec.figures.push_back(soldier_at(1, 58, 30, 9502)); // figure index 3
        if (child > 0) {
            sim::ScenarioSpec::FigureSpec c = soldier_at(1, 58, 30, 718);
            c.parent = 3;
            c.crew_slot = child == 2 ? std::optional<std::uint32_t>(0U) : std::nullopt;
            s.spec.figures.push_back(c);
        }
        return s;
    };
    {
        sim::World w = make(fixture(0)); // no child
        CHECK(w.check_invariants().empty());
        CHECK(w.apply(control_command()).ok());
    }
    {
        sim::World w = make(fixture(1)); // a carried child: containment only
        CHECK(w.check_invariants().empty());
        CHECK(w.apply(control_command()).ok());
    }
    {
        sim::World w = make(fixture(2)); // the same child in a real crew slot
        CHECK(w.check_invariants().empty());
        rejected(w, control_command(), sim::Reject::FigureInTheWay);
    }
}

TEST_CASE("founding A3: the city-name draw is reported once inside an advance") {
    const auto count = [](const sim::World& w, sim::DrawSite site) {
        std::uint32_t n = 0;
        for (const sim::DrawRecord& d : w.last_output().draws) {
            n += d.site == site ? d.count : 0;
        }
        return n;
    };
    // Accepted: the AI seam submits the control command inside the advance.
    {
        sim::World w = make(base_setup());
        auto& ai = sim::WorldTestAccess::seams(w).ai;
        ai.name = "found-command";
        ai.fn = [](void*, const sim::TickContext&, const sim::SerialWorld&,
                   std::vector<sim::Command>& out) { out.push_back(control_command()); };
        w.advance(5000);
        REQUIRE(isl(w).cities[0].has_value());
        CHECK(w.last_output().rng_cursor_before == 1234);
        CHECK(w.last_output().rng_cursor_after == 1235);
        CHECK(count(w, sim::DrawSite::CityName) == 1);
        std::uint32_t all = 0;
        for (const sim::DrawRecord& d : w.last_output().draws) {
            all += d.count;
        }
        CHECK(all == 1); // nothing else drew
    }
    // Rejected: a bad rotation draws nothing and attributes nothing.
    {
        sim::World w = make(base_setup());
        auto& ai = sim::WorldTestAccess::seams(w).ai;
        ai.name = "found-command";
        ai.fn = [](void*, const sim::TickContext&, const sim::SerialWorld&,
                   std::vector<sim::Command>& out) {
            sim::Command c = control_command();
            std::get<sim::FoundCmd>(c.body).placement.rotation = 9;
            out.push_back(c);
        };
        w.advance(5000);
        CHECK_FALSE(isl(w).cities[0].has_value());
        CHECK(w.last_output().rng_cursor_after == w.last_output().rng_cursor_before);
        CHECK(count(w, sim::DrawSite::CityName) == 0);
    }
    // Between advances (World::apply): the generator advances by the one draw; no advance is in
    // progress, so there is no per-advance report to attribute it to (last_output is the previous
    // advance's, untouched), as for every other seam draw outside an advance.
    {
        sim::World w = make(base_setup());
        w.advance(100);
        const std::size_t before_n = w.last_output().draws.size();
        const std::uint32_t cursor = w.rng().cursor();
        REQUIRE(w.apply(control_command()).ok());
        CHECK(w.rng().cursor() == cursor + 1);
        CHECK(w.last_output().draws.size() == before_n);
    }
}

TEST_CASE("founding A2: a rival ship's hull fact decides the hull test, absent hull refuses") {
    // Footprint [50,54) x [30,34). A rival (relation 0: the point/hull branch) ship at (55.5, 31.5)
    // is outside the rectangle: only the hull box can matter. Hull 397: xmin = trunc(53.95) = 53 <
    // 54, overlaps. Hull 0 and an inverted (-7 tiles) box do not. An absent hull cannot be decided:
    // FigureFactsMissing, state unchanged. Inside the rectangle the point test needs no hull.
    const auto run = [](std::uint32_t kind, double x) {
        Setup s = base_setup();
        s.spec.figures.push_back(soldier_at(1, x, 31.5, kind));
        sim::World w = make(s);
        CHECK(w.check_invariants().empty());
        return w;
    };
    {
        sim::World w = run(9503, 55.5); // hull 397
        rejected(w, control_command(), sim::Reject::FigureInTheWay);
    }
    {
        sim::World w = run(9504, 55.5); // hull absent
        rejected(w, control_command(), sim::Reject::FigureFactsMissing);
    }
    {
        sim::World w =
            run(9504, 53.5); // absent hull, but inside the rectangle: the point test decides
        rejected(w, control_command(), sim::Reject::FigureInTheWay);
    }
    {
        sim::World w = run(9505, 55.5); // inverted box
        CHECK(w.apply(control_command()).ok());
    }
    {
        sim::World w = run(9506, 55.5); // present zero hull
        CHECK(w.apply(control_command()).ok());
    }
    {
        sim::World w = run(9506, 53.5); // present zero hull inside the rectangle
        rejected(w, control_command(), sim::Reject::FigureInTheWay);
    }
}

TEST_CASE("founding hull domain: the edge of the supported domain through registry and founding") {
    // Extra ship types at the largest supported hulls (the builder refuses anything beyond:
    // figure_facts_test.cpp). A rival ship near the site with the large positive hull overlaps
    // (FigureInTheWay, nothing changed: rejected() compares bytes and PRNG); with the large
    // negative hull the box is inverted and the founding is accepted (the control).
    const std::int64_t k = sim::kMaxHullHalf88;
    const auto world_with = [&](std::int64_t hull) {
        sim::FigureType t;
        t.id = 9601;
        t.bewohntype = 113;
        t.range_88 = 16 * 256;
        t.hull_half_88 = hull;
        auto reg = syn::make_founding_types_with({t});
        REQUIRE_MESSAGE(reg.ok(), (reg.ok() ? std::string() : reg.error()));
        Setup s = base_setup();
        s.spec.figures.push_back(soldier_at(1, 55.5, 31.5, 9601));
        auto r = sim::World::create(s.spec, reg.value().registry, s.cfg, s.rules);
        REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
        return std::move(r).value();
    };
    {
        sim::World w = world_with(k - 1);
        rejected(w, control_command(), sim::Reject::FigureInTheWay);
    }
    {
        sim::World w = world_with(-(k - 1));
        CHECK(w.apply(control_command()).ok());
    }
    // Beyond the domain the registry itself is refused before any world exists.
    {
        sim::FigureType t;
        t.id = 9601;
        t.bewohntype = 113;
        t.hull_half_88 = k;
        const auto r = syn::make_founding_types_with({t});
        REQUIRE_FALSE(r.ok());
        CHECK(r.error().find("outside the supported domain") != std::string::npos);
    }
}

namespace {

std::filesystem::path fixture_log_path(const char* name) {
    return anno::test::current_replay_fixture_path(name);
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/// The recorded session of the committed fixture m2_found.log: tall island, two players; a
/// founding that succeeds, an OutOfRange refusal, a nearby second founding (the reveal), an
/// ordinary Build of a free workshop in the new city, a BadRotation and a SiteOwned refusal, two
/// advances, checkpoints in between and at the end.
sim::ReplayLog fixture_session(std::vector<sim::Reject>& statuses) {
    Setup s = tall_setup(62, 100);
    sim::World w = make(s);
    auto started = sim::Recorder::start(w);
    REQUIRE(started.ok());
    sim::Recorder rec = std::move(started).value();
    const auto submit = [&](const sim::Command& c) { statuses.push_back(rec.submit(c).status); };
    rec.checkpoint();
    submit(control_command(0));
    submit(command_p1(30));
    rec.checkpoint();
    submit(command_p1(62));
    sim::BuildCmd b;
    b.city = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    b.placements = {sim::PlacementWire{kWeaver, 46, 30, 0, 0, 0}};
    submit(sim::Command{sim::PlayerId{0}, b});
    rec.advance(5000);
    rec.checkpoint();
    sim::Command bad = control_command(0);
    found_of(bad).placement.rotation = 9;
    submit(bad);
    submit(control_command(0));
    rec.advance(5000);
    rec.checkpoint();
    return rec.log();
}

} // namespace

TEST_CASE("founding fixture: the committed log is the session, reaches founding, and a wrong "
          "outcome diverges") {
    std::vector<sim::Reject> statuses;
    const sim::ReplayLog log = fixture_session(statuses);
    const std::vector<sim::Reject> want = {sim::Reject::None,        sim::Reject::OutOfRange,
                                           sim::Reject::None,        sim::Reject::None,
                                           sim::Reject::BadRotation, sim::Reject::SiteOwned};
    CHECK(statuses == want);
    auto bytes = sim::write_log(log);
    REQUIRE(bytes.ok());
    // The wrong log: the same session with the first founding command's click moved by one tile,
    // so its origin no longer matches and it is refused (BadValue): the founding never happens.
    sim::ReplayLog wrong = log;
    bool changed = false;
    for (sim::ReplayEntry& e : wrong.entries) {
        for (sim::Command& c : e.commands) {
            if (!changed && std::holds_alternative<sim::FoundCmd>(c.body)) {
                std::get<sim::FoundCmd>(c.body).click_x += 1;
                changed = true;
            }
        }
    }
    REQUIRE(changed);
    auto wrong_bytes = sim::write_log(wrong);
    REQUIRE(wrong_bytes.ok());
    if (anno::test::update_mode()) {
        std::ofstream(fixture_log_path("m2_found.log"), std::ios::binary)
            .write(reinterpret_cast<const char*>(bytes.value().data()),
                   static_cast<std::streamsize>(bytes.value().size()));
        std::ofstream(fixture_log_path("m2_found_wrong.log"), std::ios::binary)
            .write(reinterpret_cast<const char*>(wrong_bytes.value().data()),
                   static_cast<std::streamsize>(wrong_bytes.value().size()));
        MESSAGE("fixtures m2_found.log and m2_found_wrong.log rewritten");
    }
    // The committed bytes are what the code records today.
    CHECK(read_file(fixture_log_path("m2_found.log")) == bytes.value());
    CHECK(read_file(fixture_log_path("m2_found_wrong.log")) == wrong_bytes.value());

    // Replaying the COMMITTED file the way the CLI does: the world is rebuilt from the log's
    // initial save on the code-built registry whose digest the log names.
    const auto replay_file = [&](const char* name, sim::World& world_out) {
        const auto file = read_file(fixture_log_path(name));
        auto read = sim::read_log(file);
        REQUIRE_MESSAGE(read.ok(), (read.ok() ? std::string() : read.error()));
        CHECK(read.value().registry_digest == registry()->digest());
        auto loaded = sim::World::load(read.value().initial_save, registry(), {});
        REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
        world_out = std::move(loaded).value();
        auto outcome = sim::replay(world_out, read.value());
        REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
        return outcome.value();
    };
    sim::World good(1);
    const auto ok = replay_file("m2_found.log", good);
    CHECK(ok.reproduced());
    CHECK(ok.checkpoints_total == 4);
    CHECK(ok.checkpoints_matched == 4);
    // It genuinely reached founding: both cities, the warehouses, the ordinary building.
    REQUIRE(isl(good).cities[0].has_value());
    REQUIRE(isl(good).cities[1].has_value());
    CHECK(isl(good).cities[0]->owner == sim::PlayerId{0});
    CHECK(isl(good).cities[1]->owner == sim::PlayerId{1});
    CHECK(isl(good).tiles.object(30U * 120U + 50U) == 1302);
    CHECK(isl(good).tiles.object(62U * 120U + 50U) == 1302);
    CHECK(isl(good).tiles.object(30U * 120U + 46U) == kWeaver.v);
    CHECK(owned_by(good, 0) == 1976);
    // The wrong log is detected at its first checkpoint after the missing founding.
    sim::World bad(1);
    const auto no = replay_file("m2_found_wrong.log", bad);
    CHECK_FALSE(no.reproduced());
    REQUIRE(no.diverged_entry.has_value());
    CHECK(no.checkpoints_matched < no.checkpoints_total);
    CHECK_FALSE(isl(bad).cities[0].has_value()); // the refused founding left no city 0
}

TEST_CASE("founding B4: serial and counter exhaustion refuse, a MILIT peer absent from the world "
          "is not marked") {
    // Valid control first: the same world with ordinary counters founds.
    {
        sim::World w = make(base_setup());
        CHECK(w.apply(control_command()).ok());
    }
    // The Kontor counter at its last value: the increment would overflow u32 (the original wraps at
    // 16 bits; this build refuses, ADAPT). Nothing changes, the PRNG does not move.
    {
        Setup s = base_setup();
        s.spec.islands[0].kontor_counter = 0xffffffffU;
        sim::World w = make(s);
        rejected(w, control_command(), sim::Reject::SerialExhausted);
    }
    // The building serial counter at 0xffffffff (creation would hand out an unusable serial).
    {
        sim::World w = make(base_setup());
        sim::WorldTestAccess::islands(w)[0]->next_serial = 0xffffffffU;
        rejected(w, control_command(), sim::Reject::SerialExhausted);
    }
    // A MILIT = 3 row for player 5, a legal slot with no player in the world, is not a peer: the
    // mask holds the founder only, nobody is discovered, invariants hold. With player 1 (present)
    // the same row marks him.
    {
        Setup s = base_setup();
        s.spec.players[0].relations = {sim::Relation{sim::PlayerId{1}, 0, 3, 0, 0},
                                       sim::Relation{sim::PlayerId{5}, 0, 3, 0, 0}};
        sim::World w = make(s);
        REQUIRE(w.check_invariants().empty());
        REQUIRE(w.apply(control_command()).ok());
        CHECK(isl(w).cities[0]->player_mask ==
              std::vector<sim::PlayerId>{sim::PlayerId{0}, sim::PlayerId{1}});
        CHECK(w.players().find(sim::PlayerId{5}) == nullptr);
        CHECK(w.check_invariants().empty());
    }
}

TEST_CASE("founding E1: an alternate definition, a non-default player and city slot, end to end") {
    // Type 9401: id 9401, 5 x 3, gold 120, tools 2 t, wood 4 t, action radius 10 (none of these is
    // 1302's value). Player 3 founds with its own ship (figure 4) holding wood 10 t and tools 5 t.
    // A city of player 1 sits in slot 4, so the allocation cursor is 4 and the new city is
    // number 5. Initial rotation 1 (3 wide, 5 tall): click (51, 32) -> origin (51 - 1, 32 - 2) =
    // (50, 30); the shore column x = 50 has 6 vertices (n = unrotated width + 1), accepted at once.
    Setup s = base_setup();
    sim::ScenarioSpec::PlayerSpec p3;
    p3.id = sim::PlayerId{3};
    p3.kind = 8;
    p3.money = 1000;
    s.spec.players.push_back(p3);
    s.spec.islands[0].cities.push_back(syn::plain_city(4, 1));
    s.rules.founding_types = {1302, 9401};
    sim::ScenarioSpec::FigureSpec ship;
    ship.kind = 1502;
    ship.owner = sim::PlayerId{3};
    ship.pos = sim::FixedPos{static_cast<std::int32_t>(57 * kT + kT / 2),
                             static_cast<std::int32_t>(32 * kT + kT / 2)};
    ship.holds = {sim::ShipHold{kWood, 10 * kT}, sim::ShipHold{kTools, 5 * kT}};
    s.spec.figures.push_back(ship); // figure id 4
    sim::FoundCmd f;
    f.ship = sim::FigureId{4};
    f.placement.type = sim::TypeId{9401};
    f.placement.x = 50;
    f.placement.y = 30;
    f.placement.rotation = 1;
    f.click_x = 51;
    f.click_y = 32;
    const sim::Command cmd{sim::PlayerId{3}, f};

    sim::World w = make(s);
    REQUIRE(w.check_invariants().empty());
    const sim::ApplyResult r = w.apply(cmd);
    REQUIRE(r.ok());
    // Payment from the definition: gold 1000 - 120, wood 2560 - 1024, tools 1280 - 512.
    CHECK(w.players().find(sim::PlayerId{3})->money == 880);
    const sim::Figure* sh = sim::find_figure(w.figures(), sim::FigureId{4});
    CHECK(sh->holds[0] == sim::ShipHold{kWood, 1536});
    CHECK(sh->holds[1] == sim::ShipHold{kTools, 768});
    // City number 5, owner player 3; the cursor follows; the Kontor id is the island's counter.
    REQUIRE(isl(w).cities[5].has_value());
    CHECK(isl(w).cities[5]->owner == sim::PlayerId{3});
    CHECK(isl(w).city_cursor == 5);
    const sim::Building* b = isl(w).find_building(r.records[0].building);
    REQUIRE(b != nullptr);
    CHECK(b->type == sim::TypeId{9401});
    CHECK(b->rotation == 1);
    CHECK(b->byte0f == 0x30);
    CHECK(b->kontor_id == 1);
    // The footprint is 3 wide and 5 tall (x 50..52, y 30..34), nothing at x 53 or y 35.
    for (std::uint32_t y = 30; y < 36; ++y) {
        for (std::uint32_t x = 50; x < 54; ++x) {
            CHECK(isl(w).tiles.object(y * 120ULL + x) == ((x < 53 && y < 35) ? 9401U : 0U));
        }
    }
    // Territory: radius 10 around that footprint, then the footprint: 349 tiles, rows 22..42, the
    // first and last row x 48..54, the middle rows x 41..61 (the emulated original,
    // cache/map2-gates4/emu_alt_territory.py: AnnoWelt 0x10085840 over a neutral 120 x 100 grid).
    CHECK(owned_by(w, 5) == 349);
    CHECK(isl(w).tiles.owner(22 * 120ULL + 48) == 5);
    CHECK(isl(w).tiles.owner(22 * 120ULL + 54) == 5);
    CHECK(isl(w).tiles.owner(22 * 120ULL + 47) == s.cfg.neutral_owner);
    CHECK(isl(w).tiles.owner(22 * 120ULL + 55) == s.cfg.neutral_owner);
    CHECK(isl(w).tiles.owner(21 * 120ULL + 50) == s.cfg.neutral_owner);
    CHECK(isl(w).tiles.owner(43 * 120ULL + 50) == s.cfg.neutral_owner);
    for (const std::uint32_t y : {30U, 32U, 34U}) {
        CHECK(isl(w).tiles.owner(y * 120ULL + 41) == 5);
        CHECK(isl(w).tiles.owner(y * 120ULL + 61) == 5);
        CHECK(isl(w).tiles.owner(y * 120ULL + 40) == s.cfg.neutral_owner);
        CHECK(isl(w).tiles.owner(y * 120ULL + 62) == s.cfg.neutral_owner);
    }
    CHECK(w.check_invariants().empty());
    // Persistence and replay through the same path.
    auto loaded = sim::World::load(w.save(), registry());
    REQUIRE(loaded.ok());
    CHECK(loaded.value().state_hash() == w.state_hash());
}

TEST_CASE("founding E2: definitions the command cannot decode refuse, nothing changes") {
    // A market-block type whose flag word is not the decoded coast class: Unsupported.
    {
        Setup s = base_setup();
        s.rules.founding_types = {1302, 9402};
        sim::World w = make(s);
        sim::Command c = control_command();
        found_of(c).placement.type = sim::TypeId{9402};
        rejected(w, c, sim::Reject::Unsupported);
    }
    // A type offered in the rules but absent from the registry: UnknownType (the rules list is
    // data; the registry is the authority on what exists).
    {
        Setup s = base_setup();
        s.rules.founding_types = {1302, 9999};
        sim::World w = make(s);
        sim::Command c = control_command();
        found_of(c).placement.type = sim::TypeId{9999};
        rejected(w, c, sim::Reject::UnknownType);
    }
    // The rules list itself must be ascending, unique and non-zero: a malformed list does not
    // create a world.
    for (const std::vector<std::uint32_t>& bad :
         {std::vector<std::uint32_t>{1304, 1302}, std::vector<std::uint32_t>{1302, 1302},
          std::vector<std::uint32_t>{0}}) {
        Setup s = base_setup();
        s.rules.founding_types = bad;
        auto r = sim::World::create(s.spec, registry(), s.cfg, s.rules);
        CHECK_FALSE(r.ok());
    }
    // An empty list offers nothing: every founding is NotAFoundingType.
    {
        Setup s = base_setup();
        s.rules.founding_types.clear();
        sim::World w = make(s);
        rejected(w, control_command(), sim::Reject::NotAFoundingType);
    }
}

#include <chrono>

TEST_CASE(
    "founding E3: the cost of one founding as unrelated figures and cities grow (opt-in measure)") {
    // Correctness always (the same result whatever lies far away); with ANNO_FOUNDING_BENCH=1 the
    // wall time of World::apply is printed per size. The work of one founding is: one pass over
    // the figure registry (a cheap position test per figure; only figures inside the grown query
    // rectangle build candidates), one scan of the island's city slots (a configured bound, 16),
    // the territory (area of the disc), the reveal (area / 64 samples) and the payment (holds).
    // Nothing is sorted and no pair of figures is compared. The only term that grows with the
    // world is the linear figure pass.
    const bool print = std::getenv("ANNO_FOUNDING_BENCH") != nullptr;
    std::uint64_t reference_hash = 0;
    for (const std::uint32_t extra : {0U, 1000U, 10000U, 50000U}) {
        Setup s = tall_setup(150);
        for (std::uint32_t i = 0; i < extra; ++i) {
            // Far from both sites: x 100..119 (the sea), rows 0..199, owned by player 2.
            s.spec.figures.push_back(soldier_at(2, 100 + static_cast<double>(i % 20),
                                                static_cast<double>((i / 20) % 200)));
        }
        sim::World w = make(s);
        const auto t0 = std::chrono::steady_clock::now();
        const sim::ApplyResult r = w.apply(control_command(0));
        const auto t1 = std::chrono::steady_clock::now();
        REQUIRE(r.ok());
        CHECK(owned_by(w, 0) == 1976);
        if (extra == 0) {
            reference_hash = isl(w).tiles.tile_count();
        }
        CHECK(isl(w).tiles.tile_count() == reference_hash);
        if (print) {
            std::cout << "figures " << extra + 6 << ": apply "
                      << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()
                      << " us\n";
        }
    }
}

namespace {

sim::OreObject ore_at(std::int32_t x, std::int32_t y, std::vector<sim::PlayerId> known = {}) {
    sim::OreObject o;
    o.type = 1560;
    o.x = x;
    o.y = y;
    o.amount = 400;
    o.discovered = std::move(known);
    return o;
}

} // namespace

TEST_CASE(
    "founding D3: the object scan uses radius 30 with the row limit 24 and the parity terms") {
    // Control footprint 4 x 4 at (50, 30): centre (51, 31); w = h = 4 so the parity term
    // ((w - 1) & 1) is 1 on the south and east side. R = 24, table T of radius 30
    // (T[0] = 30, T[10] = 28, T[24] = 18). Hand-derived reach: north dy = 31 - oy; south
    // dy = 1 + (oy - 31); west dx = 51 - ox; east dx = 1 + (ox - 51); accept dy <= 24 and
    // dx <= T[dy]. Pairs are {inside, just outside}.
    struct Probe {
        std::int32_t x, y;
        bool in;
        const char* why;
    };
    const std::vector<Probe> probes = {
        {51, 7, true, "dy 24 north, dx 0"},
        {51, 6, false, "dy 25 north: the row limit is R = 24, not the table length"},
        {51, 54, true, "dy 1 + 23 = 24 south"},
        {51, 55, false, "dy 1 + 24 = 25 south"},
        {21, 31, true, "dy 0, dx 30 west (the stage-1 disc is only 24 wide)"},
        {20, 31, false, "dx 31 west"},
        {80, 31, true, "dx 1 + 29 = 30 east"},
        {81, 31, false, "dx 1 + 30 = 31 east"},
        {33, 7, true, "dy 24, dx 18 west = T[24]"},
        {32, 7, false, "dy 24, dx 19 west"},
        {68, 7, true, "dy 24, dx 1 + 17 = 18 east"},
        {69, 7, false, "dy 24, dx 19 east"},
        {23, 21, true, "dy 10, dx 28 west = T[10]"},
        {22, 21, false, "dy 10, dx 29 west"},
    };
    Setup s = base_setup();
    for (const Probe& p : probes) {
        s.spec.islands[0].ore_objects.push_back(ore_at(p.x, p.y));
    }
    sim::World w = make(s);
    REQUIRE(w.check_invariants().empty());
    REQUIRE(w.apply(control_command()).ok());
    for (std::size_t i = 0; i < probes.size(); ++i) {
        INFO("probe (" << probes[i].x << ", " << probes[i].y << "): " << probes[i].why);
        CHECK(isl(w).ore_objects[i].discovered.size() == (probes[i].in ? 1U : 0U));
        if (probes[i].in) {
            CHECK(isl(w).ore_objects[i].discovered == std::vector<sim::PlayerId>{sim::PlayerId{0}});
        }
    }
    // Distinct from the territory: tile (21, 31) was discovered but is not claimed (the stage-1
    // disc has radius 24 and reaches x = 27 at most there), and distinct from the reveal radius
    // 32: an object 31 away west is NOT discovered although the reveal would sample it.
    CHECK(isl(w).tiles.owner(31 * 120ULL + 21) == s.cfg.neutral_owner);
    // The pure selection function on the same numbers (table = radius 30 profile).
    const auto table = sim::territory::disc_profile(30, 30);
    REQUIRE(table.has_value());
    REQUIRE(table->size() > 24);
    const std::vector<std::uint32_t> expect = {30, 30, 30, 30, 30, 30, 29, 29, 29, 29, 28, 28, 27,
                                               27, 27, 26, 25, 25, 24, 23, 22, 21, 20, 19, 18};
    for (std::size_t k = 0; k < expect.size(); ++k) {
        CHECK((*table)[k] == expect[k]); // the emulated original's table (cache/map2-research5)
    }
    for (const Probe& p : probes) {
        CHECK(sim::Founding::object_in_reach(p.x, p.y, 51, 31, 4, 4, 24, *table) == p.in);
    }
    // Mixed parity: w = 4 (term 1 east), h = 5 (term 0 south). Centre (51, 32): south dy = oy - 32,
    // east dx = 1 + (ox - 51). A swap of the two terms flips both boundaries below.
    CHECK(sim::Founding::object_in_reach(51, 56, 51, 32, 4, 5, 24, *table));       // dy 24 south
    CHECK_FALSE(sim::Founding::object_in_reach(51, 57, 51, 32, 4, 5, 24, *table)); // dy 25
    CHECK(sim::Founding::object_in_reach(80, 32, 51, 32, 4, 5, 24, *table));       // dx 1 + 29 = 30
    CHECK_FALSE(sim::Founding::object_in_reach(81, 32, 51, 32, 4, 5, 24, *table)); // dx 31
    // Odd footprints have no parity term: 5 x 5 at centre (52, 32): south dy = oy - 32.
    CHECK(sim::Founding::object_in_reach(52, 56, 52, 32, 5, 5, 24, *table));
    CHECK_FALSE(sim::Founding::object_in_reach(52, 57, 52, 32, 5, 5, 24, *table));
}

TEST_CASE("founding D2: first, repeated and another player's discovery, persistence and replay") {
    // Three ore objects in reach of the control site (50, 30): empty mask, already known to player
    // 0 (as a loaded file has it), and known to player 1 only.
    Setup s = tall_setup(62);
    s.spec.islands[0].ore_objects = {ore_at(44, 25), ore_at(46, 26, {sim::PlayerId{0}}),
                                     ore_at(48, 28, {sim::PlayerId{1}}),
                                     ore_at(100, 150) /* far from both sites */};
    sim::World w = make(s);
    REQUIRE(w.check_invariants().empty());
    auto started = sim::Recorder::start(w);
    REQUIRE(started.ok());
    sim::Recorder rec = std::move(started).value();
    REQUIRE(rec.submit(control_command(0)).ok());
    const auto known = [&](std::size_t i) { return isl(w).ore_objects[i].discovered; };
    const auto ids = [](std::initializer_list<std::uint32_t> v) {
        std::vector<sim::PlayerId> out;
        for (const std::uint32_t i : v) {
            out.push_back(sim::PlayerId{i});
        }
        return out;
    };
    CHECK(known(0) == ids({0}));    // first discovery
    CHECK(known(1) == ids({0}));    // already known: no duplicate
    CHECK(known(2) == ids({0, 1})); // another player already knew it: ascending set, both
    CHECK(known(3).empty());        // out of reach
    // Player 1 founds at (50, 62): its reach (centre (51, 63), rows 39..87) holds objects 0..2? The
    // nearest, (48, 28), has dy 35 > 24: none of them. Player 1 learns nothing here.
    REQUIRE(rec.submit(command_p1(62)).ok());
    CHECK(known(0) == ids({0}));
    CHECK(known(2) == ids({0, 1}));
    // Persistence and replay: the ore sets are state (hash, save/load, replayed log).
    auto loaded = sim::World::load(w.save(), registry());
    REQUIRE(loaded.ok());
    CHECK(loaded.value().state_hash() == w.state_hash());
    CHECK(isl(loaded.value()).ore_objects[2].discovered == ids({0, 1}));
    rec.advance(1000);
    rec.checkpoint();
    sim::World fresh = make(s);
    auto outcome = sim::replay(fresh, rec.log());
    REQUIRE(outcome.ok());
    CHECK(outcome.value().reproduced());
    CHECK(isl(fresh).ore_objects[0].discovered == ids({0}));
    // A mutation of a mask is a different world.
    sim::World other = w;
    sim::WorldTestAccess::islands(other)[0]->ore_objects[3].discovered.push_back(sim::PlayerId{2});
    CHECK(other.state_hash() != w.state_hash());
}

TEST_CASE("founding D4: loaded cities and masks interact with a new founding") {
    // City 3 of player 1 owns the single tile (50, 34) and is a LOADED city: its mask and the
    // island's players present come from the file. Player 0's founding at (50, 30): the reveal
    // (centre (51, 31), radius 32) samples tile (50, 34) (xl = 50, yl = 34, dy 3, dx 1) and visits
    // city 3 if its mask lacks player 0.
    const auto build = [](std::vector<sim::PlayerId> mask, std::vector<sim::PlayerId> present) {
        Setup s = base_setup();
        s.spec.islands[0].owner_runs = {{15, 34U * 120U + 50U}, {3, 1}};
        sim::ScenarioSpec::CitySpec c = syn::plain_city(3, 1);
        c.player_mask = std::move(mask);
        s.spec.islands[0].cities.push_back(c);
        s.spec.islands[0].players_present = std::move(present);
        return s;
    };
    const auto ids = [](std::initializer_list<std::uint32_t> v) {
        std::vector<sim::PlayerId> out;
        for (const std::uint32_t i : v) {
            out.push_back(sim::PlayerId{i});
        }
        return out;
    };
    {
        sim::World w = make(build(ids({1}), ids({1})));
        REQUIRE(w.check_invariants().empty());
        REQUIRE(w.apply(control_command()).ok());
        CHECK(isl(w).cities[3]->player_mask == ids({0, 1})); // visited: learned player 0
        CHECK(w.players().find(sim::PlayerId{0})->discovery == ids({0, 1}));
        CHECK(isl(w).players_present == ids({0, 1}));
        CHECK(isl(w).tiles.owner(34 * 120ULL + 50) == 3); // the loaded city tile is never taken
    }
    {
        sim::World w =
            make(build(ids({0, 1}), ids({0, 1}))); // the file already says city 3 knows 0
        REQUIRE(w.apply(control_command()).ok());
        CHECK(isl(w).cities[3]->player_mask == ids({0, 1}));
        // The reveal early-outs; the recount still lets player 0 learn owner 1 (0 is in city 3
        // mask).
        CHECK(w.players().find(sim::PlayerId{0})->discovery == ids({0, 1}));
    }
    {
        // An empty loaded mask (a file without HEAD2 mask) is a state, not "unknown": visited too.
        sim::World w = make(build({}, {}));
        REQUIRE(w.apply(control_command()).ok());
        CHECK(isl(w).cities[3]->player_mask == ids({0}));
    }
}

TEST_CASE("founding C: kind-11 humans found with kind 8's effects, a wrapped number is refused") {
    // Same world, same command, player 0 of kind 8 and of kind 11: identical money, holds, city,
    // building, territory and PRNG cursor (one draw), nothing else.
    const auto run = [](std::uint32_t kind) {
        Setup s = base_setup();
        s.spec.players[0].kind = kind;
        sim::World w = make(s);
        const sim::ApplyResult r = w.apply(control_command());
        REQUIRE(r.ok());
        return std::tuple(w.players().find(sim::PlayerId{0})->money,
                          sim::find_figure(w.figures(), sim::FigureId{1})->holds,
                          isl(w).cities[0]->name, w.rng().cursor(), owned_by(w, 0),
                          isl(w).find_building(r.records[0].building)->kontor_id);
    };
    CHECK(run(11) == run(8));
    CHECK(std::get<3>(run(11)) == 1235);
    // The boundary (17.4/17.6): cities in slots 6..14 and a loaded cursor of 14 make the next
    // allocation wrap to number 0 <= 14. A kind-11 founding is refused, uncharged, nothing changes;
    // the identical world founds for kind 8 (number 0) and for kind 11 when no wrap is needed.
    const auto wrapped = [](std::uint32_t kind) {
        Setup s = base_setup();
        s.spec.players[0].kind = kind;
        for (std::uint32_t n = 6; n < 15; ++n) {
            s.spec.islands[0].cities.push_back(syn::plain_city(n, 1));
        }
        return s;
    };
    {
        sim::World w = make(wrapped(11));
        REQUIRE(isl(w).city_cursor == 14);
        rejected(w, control_command(), sim::Reject::Unsupported);
    }
    {
        sim::World w = make(wrapped(8));
        const sim::ApplyResult r = w.apply(control_command());
        REQUIRE(r.ok());
        CHECK(isl(w).cities[0].has_value()); // wrapped to number 0
    }
    {
        Setup s = base_setup();
        s.spec.players[0].kind = 11;
        s.spec.islands[0].cities.push_back(syn::plain_city(5, 1)); // cursor 5, next number 6
        sim::World w = make(s);
        REQUIRE(w.apply(control_command()).ok());
        CHECK(isl(w).cities[6].has_value());
    }
    // Kinds that are not human founders stay refused: 12 (computer), 14 (natives), 15, 16.
    for (const std::uint32_t kind : {9U, 10U, 12U, 14U, 15U, 16U}) {
        Setup s = base_setup();
        s.spec.players[0].kind = kind;
        sim::World w = make(s);
        rejected(w, control_command(), sim::Reject::Unsupported);
    }
}

// ---- MAP-FOUND-3 package D: the shared-visibility policy (docs/sim/shared-visibility.md) --------

namespace {

std::vector<sim::PlayerId> pids(std::initializer_list<std::uint32_t> v) {
    std::vector<sim::PlayerId> out;
    for (const std::uint32_t i : v) {
        out.push_back(sim::PlayerId{i});
    }
    return out;
}

/// Two cities of player 2 (non-member) on single tiles (50, 34) and (58, 34), masks {2}.
Setup visit_setup(std::vector<std::vector<std::uint32_t>> groups) {
    Setup s = tall_setup(62);
    s.rules.shared_visibility = std::move(groups);
    s.spec.islands[0].owner_runs = {{15, 34U * 120U + 50U}, {3, 1}, {15, 7}, {4, 1}};
    for (const std::uint32_t n : {3U, 4U}) {
        sim::ScenarioSpec::CitySpec c = syn::plain_city(n, 2);
        c.player_mask = pids({2});
        s.spec.islands[0].cities.push_back(c);
    }
    s.spec.islands[0].players_present = pids({2});
    return s;
}

} // namespace

TEST_CASE("shared visibility: a member's founding reaches the group, nobody else, nothing more") {
    const auto run = [](std::vector<std::vector<std::uint32_t>> groups) {
        Setup s = base_setup();
        s.rules.shared_visibility = std::move(groups);
        s.spec.islands[0].ore_objects = {ore_at(44, 25), ore_at(100, 150) /* out of reach */};
        sim::World w = make(s);
        REQUIRE(w.check_invariants().empty());
        const std::uint64_t cursor = w.rng().cursor();
        REQUIRE(w.apply(control_command(0)).ok());
        CHECK(w.rng().cursor() == cursor + 1); // the one name draw, the policy draws nothing
        return w;
    };
    const sim::World off = run({});
    const sim::World on = run({{0, 1}});
    // Sharing off: exactly the original's founder-only state.
    CHECK(isl(off).cities[0]->player_mask == pids({0}));
    CHECK(isl(off).players_present == pids({0}));
    CHECK(isl(off).ore_objects[0].discovered == pids({0}));
    CHECK(off.players().find(sim::PlayerId{1})->discovery.empty());
    // Sharing on, group {0, 1}: the new city, the island, the ore in reach and the contact reach
    // player 1; player 2 (existing, not a member) gets nothing; the ore out of reach stays unknown.
    CHECK(isl(on).cities[0]->player_mask == pids({0, 1}));
    CHECK(isl(on).players_present == pids({0, 1}));
    CHECK(isl(on).ore_objects[0].discovered == pids({0, 1}));
    CHECK(isl(on).ore_objects[1].discovered.empty());
    CHECK(on.players().find(sim::PlayerId{0})->discovery == pids({0}));
    CHECK(on.players().find(sim::PlayerId{1})->discovery == pids({0}));
    CHECK(on.players().find(sim::PlayerId{2})->discovery.empty());
    // Not a right, not a relation: the territory, the money and every relation row are exactly
    // the sharing-off world's.
    for (std::uint64_t i = 0; i < isl(on).tiles.tile_count(); ++i) {
        REQUIRE(isl(on).tiles.owner(i) == isl(off).tiles.owner(i));
    }
    for (std::uint32_t p = 0; p < 3; ++p) {
        CHECK(on.players().find(sim::PlayerId{p})->money ==
              off.players().find(sim::PlayerId{p})->money);
        CHECK(on.players().find(sim::PlayerId{p})->relations.empty());
    }
    // The nearby-figure veto is the policy-independent one: a rival ship in the way is refused
    // the same with and without the policy (member 1's ship near member 0's site, and the
    // outsider's).
    for (const std::uint32_t rival : {1U, 2U}) {
        for (const bool shared : {false, true}) {
            Setup s = base_setup();
            s.rules.shared_visibility = shared ? std::vector<std::vector<std::uint32_t>>{{0, 1}}
                                               : std::vector<std::vector<std::uint32_t>>{};
            s.spec.figures.push_back(soldier_at(rival, 51.5, 31.5));
            sim::World w = make(s);
            rejected(w, control_command(0), sim::Reject::FigureInTheWay);
        }
    }
}

TEST_CASE("shared visibility: the reveal's single visit, the early-out and Q's own founding") {
    // Player 2 (a non-member) owns cities 3 and 4. Without sharing, player 0 visits city 3 and then
    // player 1 visits city 3 as well (the lowest city lacking it); with the group {0, 1} city 3
    // already holds player 1 after player 0's founding, so player 1's visit reaches city 4: the
    // group reveals faster than one player (docs/sim/shared-visibility.md 2, rule d).
    for (const bool shared : {false, true}) {
        INFO(std::string(shared ? "group {0,1}" : "sharing off"));
        sim::World w = make(visit_setup(shared ? std::vector<std::vector<std::uint32_t>>{{0, 1}}
                                               : std::vector<std::vector<std::uint32_t>>{}));
        REQUIRE(w.check_invariants().empty());
        REQUIRE(w.apply(control_command(0)).ok());
        CHECK(isl(w).cities[3]->player_mask == (shared ? pids({0, 1, 2}) : pids({0, 2})));
        CHECK(isl(w).cities[4]->player_mask == pids({2}));
        // Contact: the visit tells the founder (and the group) who owns city 3.
        CHECK(w.players().find(sim::PlayerId{0})->discovery == pids({0, 2}));
        CHECK(w.players().find(sim::PlayerId{1})->discovery == (shared ? pids({0, 2}) : pids({})));
        // Player 1's own founding is legal exactly as without a policy (same site rules).
        REQUIRE(w.apply(command_p1(62)).ok());
        CHECK(isl(w).cities[3]->player_mask == pids({0, 1, 2}));
        CHECK(isl(w).cities[4]->player_mask == (shared ? pids({0, 1, 2}) : pids({2})));
        // The non-member never learns of the members through the policy.
        CHECK(w.players().find(sim::PlayerId{2})->discovery == pids({2}));
    }
}

TEST_CASE(
    "shared visibility: groups are validated, peace is required, nothing leaks between groups") {
    const auto create_error = [](Setup s) {
        auto r = sim::World::create(s.spec, registry(), s.cfg, s.rules);
        return r.ok() ? std::string() : r.error();
    };
    const auto has = [](const std::string& e, const char* part) {
        return e.find(part) != std::string::npos;
    };
    // The valid control.
    {
        Setup s = base_setup();
        s.rules.shared_visibility = {{0, 1}};
        CHECK(create_error(s).empty());
    }
    // Structure (Rules::validate): short, unsorted, duplicated, overlapping groups.
    for (const auto& [groups, part] :
         std::vector<std::pair<std::vector<std::vector<std::uint32_t>>, const char*>>{
             {{{0}}, "fewer than two members"},
             {{{1, 0}}, "not ascending and unique"},
             {{{0, 0}}, "not ascending and unique"},
             {{{0, 1}, {1, 2}}, "more than one group"}}) {
        Setup s = base_setup();
        s.rules.shared_visibility = groups;
        const std::string e = create_error(s);
        CHECK_MESSAGE(has(e, part), e);
        CHECK(has(e, "shared_visibility"));
    }
    // World (create): a member that is not a player, one beyond player_slots.
    for (const std::uint32_t missing : {7U, 40U}) {
        Setup s = base_setup();
        s.rules.shared_visibility = {{0, missing}};
        const std::string e = create_error(s);
        CHECK_MESSAGE(has(e, ("player " + std::to_string(missing) + " is not a player").c_str()),
                      e);
    }
    // Peace: every relation byte of every ordered pair must be zero; an explicit all-zero row and
    // an absent row are both peace; the credit is ignored; towards a non-member anything goes.
    const auto relation_world = [&](sim::Relation r, std::uint32_t owner = 0) {
        Setup s = base_setup();
        s.rules.shared_visibility = {{0, 1}};
        s.spec.players[owner].relations = {r};
        return create_error(s);
    };
    CHECK(relation_world(sim::Relation{sim::PlayerId{1}, 0, 0, 0, 0}).empty());
    CHECK(relation_world(sim::Relation{sim::PlayerId{1}, 0, 0, 0, 500}).empty()); // credit only
    CHECK(relation_world(sim::Relation{sim::PlayerId{2}, 3, 3, 3, 0}).empty());   // non-member
    CHECK(has(relation_world(sim::Relation{sim::PlayerId{1}, 3, 0, 0, 0}), "TRADE is 3"));
    CHECK(has(relation_world(sim::Relation{sim::PlayerId{1}, 0, 3, 0, 0}), "MILIT is 3"));
    CHECK(has(relation_world(sim::Relation{sim::PlayerId{1}, 0, 0, 3, 0}), "WAR is 3"));
    CHECK(
        has(relation_world(sim::Relation{sim::PlayerId{0}, 0, 0, 1, 0}, 1), "WAR is 1")); // 1 -> 0
    // A refused creation leaves nothing: two creations with the same invalid input give the same
    // error and no world (World::create returns a Result).
    // A save that names a missing member is refused on load (the world-level invariant).
    {
        Setup s = base_setup();
        s.rules.shared_visibility = {{0, 1}};
        sim::World w = make(s);
        sim::WorldTestAccess::rules(w).shared_visibility = {{0, 9}};
        auto loaded = sim::World::load(w.save(), registry(), {});
        REQUIRE_FALSE(loaded.ok());
        CHECK(has(loaded.error(), "player 9 is not a player"));
        sim::WorldTestAccess::rules(w).shared_visibility = {{0, 1}};
        CHECK(sim::World::load(w.save(), registry(), {}).ok()); // the control
    }
    // Two disjoint groups: nothing leaks between them; a member that is defeated still receives.
    {
        Setup s = tall_setup(62);
        s.spec.players.push_back(sim::ScenarioSpec::PlayerSpec{});
        s.spec.players.back().id = sim::PlayerId{3};
        s.spec.players.back().kind = 8;
        s.rules.shared_visibility = {{0, 1}, {2, 3}};
        sim::World w = make(s);
        sim::WorldTestAccess::players(w).slots[1]->state = sim::PlayerState::Defeated;
        REQUIRE(w.apply(control_command(0)).ok());
        CHECK(isl(w).cities[0]->player_mask == pids({0, 1}));
        CHECK(isl(w).players_present == pids({0, 1}));
        CHECK(w.players().find(sim::PlayerId{2})->discovery.empty());
        CHECK(w.players().find(sim::PlayerId{3})->discovery.empty());
        CHECK(w.players().find(sim::PlayerId{1})->discovery == pids({0}));
    }
}

TEST_CASE(
    "shared visibility: save/load, replay on 1, 2, 4 and 7 executor threads, refused founding") {
    Setup s = visit_setup({{0, 1}});
    sim::World w = make(s);
    // A refused founding writes no mirror: the veto rejects, bytes and PRNG unchanged.
    {
        Setup bad = s;
        bad.spec.figures.push_back(soldier_at(2, 51.5, 31.5));
        sim::World blocked = make(bad);
        rejected(blocked, control_command(0), sim::Reject::FigureInTheWay);
        CHECK(isl(blocked).cities[3]->player_mask == pids({2}));
    }
    auto started = sim::Recorder::start(w);
    REQUIRE(started.ok());
    sim::Recorder rec = std::move(started).value();
    REQUIRE(rec.submit(control_command(0)).ok());
    REQUIRE(rec.submit(command_p1(62)).ok());
    rec.advance(500);
    rec.checkpoint();
    // Save and load keep the policy and the mirrored sets (same hash).
    auto loaded = sim::World::load(w.save(), registry(), {});
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    CHECK(loaded.value().state_hash() == w.state_hash());
    CHECK(loaded.value().rules().shared_visibility == s.rules.shared_visibility);
    // Replay in a fresh world reproduces every checkpoint, whatever the thread count.
    for (const unsigned threads : {1U, 2U, 4U, 7U}) {
        island_test::ThreadExecutor ex(threads);
        auto fresh = sim::World::create(s.spec, registry(), s.cfg, s.rules, {}, ex);
        REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
        auto outcome = sim::replay(fresh.value(), rec.log());
        REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
        CHECK(outcome.value().reproduced());
        CHECK(fresh.value().state_hash() == w.state_hash());
    }
    // A different participant set is a different world identity (rules digest and state hash), and
    // a replay of this log on it is refused.
    Setup other = s;
    other.rules.shared_visibility = {{0, 2}};
    sim::World ow = make(other);
    CHECK(ow.rules().digest() != w.rules().digest());
    CHECK(ow.state_hash() != make(s).state_hash());
    auto refused = sim::replay(ow, rec.log());
    CHECK((!refused.ok() || !refused.value().reproduced()));
}

TEST_CASE("shared visibility: a MILIT peer outside the group joins the group, other members' peers "
          "do not") {
    // Group {0, 1}. Player 0 has MILIT 3 towards outsider 2 (a registration peer, original rule);
    // player 1 has MILIT 3 towards outsider 3 (a peer of ANOTHER member: not added). Peace holds
    // for the member pair. Expected city mask after player 0's founding: {0, 1} + peer {2} =
    // {0,1,2}.
    Setup s = base_setup();
    s.spec.players.push_back(sim::ScenarioSpec::PlayerSpec{});
    s.spec.players.back().id = sim::PlayerId{3};
    s.spec.players.back().kind = 8;
    s.rules.shared_visibility = {{0, 1}};
    s.spec.players[0].relations = {sim::Relation{sim::PlayerId{2}, 0, 3, 0, 0}};
    s.spec.players[1].relations = {sim::Relation{sim::PlayerId{3}, 0, 3, 0, 0}};
    sim::World w = make(s);
    REQUIRE(w.check_invariants().empty());
    REQUIRE(w.apply(control_command(0)).ok());
    CHECK(isl(w).cities[0]->player_mask == pids({0, 1, 2}));
    CHECK(isl(w).players_present == pids({0, 1})); // peers are not "present"
    for (const std::uint32_t p : {0U, 1U, 2U}) {
        CHECK(w.players().find(sim::PlayerId{p})->discovery == pids({0}));
    }
    CHECK(w.players().find(sim::PlayerId{3})->discovery.empty());
    // The same world without the group: the peer rule alone, as in the original.
    s.rules.shared_visibility.clear();
    sim::World off = make(s);
    REQUIRE(off.apply(control_command(0)).ok());
    CHECK(isl(off).cities[0]->player_mask == pids({0, 2}));
}

TEST_CASE("freight takeover: a refused Found preserves assignment and a successful public Found "
          "cancels it") {
    auto setup = base_setup();
    const auto base = registry();
    sim::TypeRegistry::Builder builder;
    builder.walk_tables(sim::synthetic_walk_tables());
    for (const auto& type : base->types())
        builder.add(type);
    for (const auto& original : base->figure_types()) {
        auto type = original;
        if (type.id == 1502) {
            type.capacity_88 = 12800;
            type.nav = sim::NavFacts{154618816U, 472446400U, 6000U, 4U, 2U, 0U, 1280U};
        }
        builder.figure_type(type);
    }
    auto types = builder.build();
    REQUIRE(types);
    auto other = syn::plain_island(1, 200, 0, 16, 16);
    other.owner_runs = {{0, 256}};
    auto city = syn::plain_city(0, 0);
    city.buildings.push_back({.type = kKontor, .x = 204, .y = 4});
    other.cities.push_back(city);
    setup.spec.islands.push_back(other);
    auto made = sim::World::create(setup.spec, types.value(), setup.cfg, setup.rules);
    REQUIRE(made);
    auto world = std::move(made).value();
    const auto* warehouse = world.building_at({sim::IslandId{1}, sim::CityNo{0}}, 204, 4);
    REQUIRE(warehouse);
    const sim::AssignFreightRouteCmd route{
        sim::FigureId{1},
        {{warehouse->id, 198 * 256, 6 * 256, {{sim::FreightDirection::LoadTarget, kWood, 0}}}}};
    REQUIRE(world.apply({sim::PlayerId{0}, route}).ok());
    REQUIRE(world.freight_route(sim::FigureId{1}) != nullptr);
    world.advance(2);
    REQUIRE(world.freight_route(sim::FigureId{1})->phase == sim::FreightPhase::Sailing);
    auto refused = control_command();
    found_of(refused).placement.type = sim::TypeId{9999};
    const auto before = world.save();
    CHECK(world.apply(refused).status == sim::Reject::UnknownType);
    CHECK(world.save() == before);
    const auto generation = sim::find_figure(world.figures(), sim::FigureId{1})->freight_generation;
    REQUIRE(world.apply(control_command()).ok());
    CHECK(world.freight_route(sim::FigureId{1}) == nullptr);
    CHECK(sim::find_figure(world.figures(), sim::FigureId{1})->freight_generation == generation);
    CHECK(world.players().find(sim::PlayerId{0})->money == 29650);
    const auto* paid = sim::find_figure(world.figures(), sim::FigureId{1});
    REQUIRE(paid);
    CHECK(paid->holds[0].amount_88 == 38 * 256);
    CHECK(paid->holds[2].amount_88 == 45 * 256);
    CHECK(paid->holds[3].amount_88 == 30 * 256);
    CHECK(world.check_invariants().empty());
    auto loaded = sim::World::load(world.save(), types.value());
    REQUIRE(loaded);
    CHECK(loaded.value().save() == world.save());
}
