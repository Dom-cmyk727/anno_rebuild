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
#include <anno/sim/replay.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
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
    sim::BuildingType k43 = k;
    k43.id = kKontor43;
    k43.width = 6;
    k43.height = 3;
    b.add(k43);
    b.add_plain(kRoadObj, 193);
    b.add_plain(kTree, 193).flags_word(kTree, 0x20);
    b.add_workshop(kWeaver, "STOFFE", "WOLLE", 6, 11, 5, 0);
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
        {"a type-11 human has an AI object, outside this unit", sim::Reject::Unsupported,
         [](Setup& s, sim::Command&) { s.spec.players[0].kind = 11; }},
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
Setup tall_setup(std::uint32_t site1_y) {
    Setup s = base_setup();
    sim::ScenarioSpec::IslandSpec& is = s.spec.islands[0];
    is.h = 200;
    std::vector<std::uint8_t> height(120U * 200U);
    for (std::uint32_t y = 0; y < 200; ++y) {
        for (std::uint32_t x = 0; x < 120; ++x) {
            height[y * 120 + x] = x <= 49 ? 0x40 : (x == 50 ? 0x30 : 0x10);
        }
    }
    is.terrain = sim::make_terrain(std::move(height), {}, {});
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
        const std::int64_t loser_gold = w.players().find(sim::PlayerId{loser})->money;
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

TEST_CASE("founding replay: a recorded session with successes, refusals and play reproduces") {
    Setup s = tall_setup(62);
    sim::World w = make(s);
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    rec.checkpoint();
    CHECK(rec.submit(control_command(0)).ok());
    // Player 1's ship is 31 tiles from that click: refused, recorded, uncharged.
    CHECK(rec.submit(command_p1(30)).status == sim::Reject::OutOfRange);
    CHECK(rec.submit(command_p1(62)).ok()); // a nearby site: the reveal visits city 0
    rec.advance(5000);
    rec.checkpoint();
    sim::Command bad = control_command(0);
    found_of(bad).placement.rotation = 9; // refused with BadRotation, recorded anyway
    CHECK(rec.submit(bad).status == sim::Reject::BadRotation);
    CHECK(rec.submit(control_command(0)).status == sim::Reject::SiteOwned);
    rec.advance(5000);
    rec.checkpoint();
    const sim::ReplayLog log = rec.log();
    const std::uint64_t final_hash = w.state_hash();

    // Through bytes and back, then replayed into a fresh world: every checkpoint matches.
    auto bytes = sim::write_log(log);
    REQUIRE_MESSAGE(bytes.ok(), (bytes.ok() ? std::string() : bytes.error()));
    auto read = sim::read_log(bytes.value());
    REQUIRE_MESSAGE(read.ok(), (read.ok() ? std::string() : read.error()));
    CHECK(read.value() == log);
    sim::World fresh = make(s);
    auto outcome = sim::replay(fresh, read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(outcome.value().checkpoints_total == 3);
    CHECK(outcome.value().checkpoints_matched == 3);
    CHECK(fresh.state_hash() == final_hash);
    // The executor count does not change the result.
    for (const unsigned threads : {1U, 2U, 4U}) {
        sim::World copy = fresh;
        island_test::ThreadExecutor ex(threads);
        copy.prepare_hash(ex);
        CHECK(copy.state_hash() == final_hash);
    }
    // A determinism pin across processes and platforms (not an oracle: the exact values are
    // asserted above by hand).
    std::string update_line;
    const std::string problem =
        anno::test::check_golden("m2_found_session", "M2", final_hash, update_line);
    if (anno::test::update_mode()) {
        MESSAGE("golden line: " << update_line);
    }
    CHECK(problem == "");
}

#include <anno/sim/founding.hpp>
#include <anno/sim/territory.hpp>

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
