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
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>

#include "island_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

constexpr std::uint32_t kT = 256;
constexpr sim::TypeId kKontor{1302};
constexpr sim::TypeId kRoadObj{9100}; // an obstruction (no flag 0x20)
constexpr sim::TypeId kTree{9101};    // overlappable scenery (flag 0x20)
constexpr sim::GoodsId kTools = 0x29, kWood = 0x2a, kFood = 5;

std::shared_ptr<const sim::TypeRegistry> registry() {
    // A market type, then patched into the Kontor: a second builder takes it fully specified.
    sim::TypeRegistry::Builder seed;
    seed.add_market(kKontor, 1, 1, 24, 4, 4);
    auto base = seed.build();
    REQUIRE_MESSAGE(base.ok(), (base.ok() ? std::string() : base.error()));
    sim::BuildingType k = *base.value()->find(kKontor);
    k.prodtype = 35;
    k.width = k.height = 4;
    k.action_radius = 24;
    k.cost_gold = 350;
    k.cost_goods = {sim::CostEntry{kTools, 5 * kT}, sim::CostEntry{kWood, 12 * kT}};
    k.flags_word = 0x2087;
    sim::TypeRegistry::Builder b;
    b.add(k);
    b.add_plain(kRoadObj, 193);
    b.add_plain(kTree, 193).flags_word(kTree, 0x20);
    const auto fig = [](std::uint32_t id, std::uint32_t kind, std::uint32_t range,
                        std::uint32_t hull) {
        sim::FigureType t;
        t.id = id;
        t.bewohntype = kind;
        t.range_88 = range;
        t.hull_half_88 = hull;
        return t;
    };
    b.figure_type(fig(1502, 113, 16 * kT, 397)); // the ship: range 16, hull 1.55 rounded
    b.figure_type(fig(717, 97, 7 * kT, 0));      // the scout
    b.figure_type(fig(718, 53, 7 * kT, 0));      // the donkey
    b.figure_type(fig(9501, 99, 7 * kT, 0));     // a soldier
    b.figure_type(fig(9502, 108, 7 * kT, 0));    // a war machine
    b.figure_type(fig(9503, 113, 16 * kT, 397)); // a rival ship
    auto r = b.build();
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return r.value();
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
        n += t.owner(i) == owner ? 1 : 0;
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
