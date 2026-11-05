// MAP-NAV-3 N4: the health input of the speed factor V (docs/re/ship-navigation.md 7.5, docs/sim/
// ship-navigation.md 12.14; stream 123). Synthetic: an open sea, no game files. Expected factors
// come from tools/ship_step_ref.py (fixtures/synthetic/sim/nav/speed_factor_vectors.txt, an
// exact-rational transcription of the formula), the arrival bounds from the hand arithmetic in the
// comments, never from the code under test.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/ship.hpp>
#include <anno/sim/ship_step.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "island_helpers.hpp"
#include "ship_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using ship_test::kDonkey;
using ship_test::kScout;
using ship_test::kShip;

namespace {

constexpr std::int32_t kTile = 256;
constexpr sim::NavFacts kFacts{154618816U, 472446400U, 6000U, 4U, 2U, 0U, 1280U};

std::int32_t tile_c(int t) {
    return t * kTile + kTile / 2;
}

/// The synthetic registry with a navigating ship type `nav_id` of the facts `facts`.
std::shared_ptr<const sim::TypeRegistry> registry_of(const sim::NavFacts& facts,
                                                     std::uint32_t nav_id = kShip) {
    auto base = syn::make_synthetic_types();
    REQUIRE(base);
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    for (const sim::BuildingType& t : base.value().registry->types()) {
        b.add(t);
    }
    sim::FigureType ship{nav_id, 0, 0, 0, 0};
    ship.bewohntype = 113;
    ship.capacity_88 = 1000 * 256; // 1000 tons: the cargo term has a divisor
    ship.nav = facts;
    b.figure_type(ship)
        .figure_type(sim::FigureType{kScout, 0, 0, 0, 0})
        .figure_type(sim::FigureType{kDonkey, 0, 0, 0, 0});
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

struct ShipSpec {
    std::uint32_t kind = kShip;
    std::uint32_t owner = 0;
    int row = 64;
    std::optional<std::uint32_t> health{};
    bool unknown = false;
    std::vector<sim::ShipHold> holds{};
};

sim::ScenarioSpec spec_of(const std::vector<ShipSpec>& ships, std::uint32_t volk1 = 2) {
    sim::ScenarioSpec spec = syn::empty_scenario(11);
    for (std::uint32_t p = 0; p < 2; ++p) {
        sim::ScenarioSpec::PlayerSpec ps;
        ps.id = sim::PlayerId{p};
        ps.money = 1000;
        ps.volk =
            p == 0 ? 1 : volk1; // player 0 is the Volk of the cargo term (Rules.nav.volk_number)
        spec.players.push_back(ps);
    }
    for (const ShipSpec& s : ships) {
        sim::ScenarioSpec::FigureSpec f;
        f.kind = s.kind;
        f.owner = sim::PlayerId{s.owner};
        f.pos = sim::FixedPos{tile_c(20), tile_c(s.row)};
        f.dir16 = 8;
        f.health = s.health;
        f.health_unknown = s.unknown;
        f.holds = s.holds;
        spec.figures.push_back(f);
    }
    return spec;
}

std::string create_error(const sim::ScenarioSpec& spec,
                         const std::shared_ptr<const sim::TypeRegistry>& types) {
    auto w = sim::World::create(spec, types, {}, {}, {}, sim::serial_executor());
    return w.ok() ? std::string() : w.error();
}

sim::World make(const sim::ScenarioSpec& spec,
                const std::shared_ptr<const sim::TypeRegistry>& types) {
    auto w = sim::World::create(spec, types, {}, {}, {}, sim::serial_executor());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

sim::Figure& fig(sim::World& w, std::uint64_t id) {
    sim::Figure* f = sim::find_figure(sim::WorldTestAccess::figures(w), sim::FigureId{id});
    REQUIRE(f != nullptr);
    return *f;
}

sim::Command move(std::uint64_t id, int tx, int ty, std::uint32_t issuer = 0) {
    return sim::Command{sim::PlayerId{issuer},
                        sim::MoveCmd{sim::FigureId{id}, tile_c(tx), tile_c(ty)}};
}

struct Sail {
    std::uint64_t arrival_tick = 0;  // 0 = not within the budget
    std::uint32_t moving_factor = 0; // the factor of the first movement segment
};

/// Advance `w` in 10 ms steps until ship `id` has arrived (event ShipArrived of that figure).
/// Records the arrival tick and the first movement segment's factor.
Sail sail(sim::World& w, std::uint64_t id, int max_steps = 4000) {
    Sail out;
    for (int i = 0; i < max_steps && out.arrival_tick == 0; ++i) {
        w.advance(10);
        const sim::Figure& f = fig(w, id);
        if (out.moving_factor == 0 && f.nav->seg.kind == sim::NavSegKind::Move) {
            out.moving_factor = f.nav->seg.factor_percent;
        }
        for (const sim::SimEvent& e : w.drain_events()) {
            if (e.kind == sim::SimEventKind::ShipArrived && e.figure == sim::FigureId{id}) {
                out.arrival_tick = w.clock().tick();
            }
        }
    }
    return out;
}

} // namespace

TEST_CASE("health: nav_speed_factor equals the independent Python vectors") {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "nav" /
                     "speed_factor_vectors.txt");
    REQUIRE(in.good());
    std::string line;
    int checked = 0;
    int unknown_cases = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ls(line);
        std::string tok, name;
        ls >> tok >> name;
        REQUIRE(tok == "factor");
        sim::NavRules rules;
        sim::NavFacts facts = kFacts;
        std::uint32_t cap = 0, owner = 0, expect = 0;
        std::optional<std::uint32_t> hp;
        std::vector<sim::ShipHold> holds;
        std::string key;
        while (ls >> key) {
            if (key == "floor") {
                ls >> rules.health_floor_percent;
            } else if (key == "span") {
                ls >> rules.health_span_percent;
            } else if (key == "penalty") {
                ls >> rules.cargo_penalty_percent;
            } else if (key == "volk") {
                ls >> rules.volk_number;
            } else if (key == "maxhp") {
                ls >> facts.max_hp;
            } else if (key == "n0") {
                ls >> facts.hold_count_a;
            } else if (key == "n1") {
                ls >> facts.hold_count_b;
            } else if (key == "cap") {
                ls >> cap;
            } else if (key == "owner") {
                ls >> owner;
            } else if (key == "hp") {
                std::string v;
                ls >> v;
                if (v != "unknown") {
                    hp = static_cast<std::uint32_t>(std::stoul(v));
                } else {
                    ++unknown_cases;
                }
            } else if (key == "holds") {
                std::string v;
                ls >> v;
                std::istringstream hs(v);
                std::string a;
                sim::GoodsId g = 1;
                while (v != "-" && std::getline(hs, a, ',')) {
                    holds.push_back(sim::ShipHold{g++, std::stoull(a)});
                }
            } else if (key == "expect") {
                ls >> expect;
            }
        }
        INFO(name);
        CHECK(sim::nav_speed_factor(facts, rules, cap, holds, owner, hp) == expect);
        ++checked;
    }
    CHECK(checked == 24);
    CHECK(unknown_cases == 2);
    // A health above the type's maximum is not a valid state (validate_navigation refuses it);
    // the formula itself clamps it to the maximum instead of overflowing the term.
    sim::NavRules rules;
    CHECK(sim::nav_speed_factor(kFacts, rules, 0, {}, 1, 6001U) == 100);
    CHECK(sim::nav_speed_factor(kFacts, rules, 0, {}, 1, 0xFFFFFFFFU) == 100);
}

TEST_CASE("health: a new expedition is at full health, a spec can name a value or unknown") {
    const auto types = registry_of(kFacts);
    sim::ScenarioSpec spec =
        spec_of({ShipSpec{}, ShipSpec{.row = 70, .health = 2500},
                 ShipSpec{.row = 76, .unknown = true}, ShipSpec{.row = 82, .health = 0}});
    sim::World w = make(spec, types);
    CHECK(fig(w, 1).nav->health == std::optional<std::uint32_t>{6000});
    CHECK(fig(w, 2).nav->health == std::optional<std::uint32_t>{2500});
    CHECK_FALSE(fig(w, 3).nav->health.has_value());
    CHECK(fig(w, 4).nav->health == std::optional<std::uint32_t>{0});
    CHECK(w.check_invariants().empty());
}

TEST_CASE("health: invalid specs and states are refused with their own diagnostic") {
    const auto types = registry_of(kFacts);
    // At creation.
    std::string e = create_error(spec_of({ShipSpec{.health = 6001}}), types);
    CHECK_MESSAGE(
        e.find("figures[0].health: 6001 is above the type's max_hp 6000") != std::string::npos, e);
    CHECK(create_error(spec_of({ShipSpec{.health = 6000}}), types).empty());
    e = create_error(spec_of({ShipSpec{.health = 1, .unknown = true}}), types);
    CHECK_MESSAGE(e.find("figures[0].health: a value and health_unknown are exclusive") !=
                      std::string::npos,
                  e);
    e = create_error(spec_of({ShipSpec{.kind = kScout, .health = 1}}), types);
    CHECK_MESSAGE(
        e.find("figures[0].health: figure type 717 does not navigate") != std::string::npos, e);
    // A figure that does not navigate carries no health, full or not: it has no NavMotion.
    {
        sim::World w = make(spec_of({ShipSpec{.kind = kScout}}), types);
        CHECK_FALSE(fig(w, 1).nav.has_value());
    }
    // In a state: the invariant names the field; a saved copy of the bad state does not load.
    sim::World w = make(spec_of({ShipSpec{}}), types);
    REQUIRE(w.check_invariants().empty());
    const std::vector<std::uint8_t> good = w.save();
    fig(w, 1).nav->health = 6001;
    const std::string bad = w.check_invariants();
    CHECK_MESSAGE(bad.find("figures.list.slots[0].nav.health: 6001 hit points exceed the type's "
                           "max_hp 6000") != std::string::npos,
                  bad);
    auto loaded_bad = sim::World::load(w.save(), types);
    REQUIRE_FALSE(loaded_bad.ok());
    CHECK_MESSAGE(loaded_bad.error().find("nav.health: 6001 hit points exceed") !=
                      std::string::npos,
                  loaded_bad.error());
    // hp 0 and hp = max are valid; the valid control loads.
    fig(w, 1).nav->health = 0;
    CHECK(w.check_invariants().empty());
    fig(w, 1).nav->health = 6000;
    CHECK(sim::World::load(w.save(), types).ok());
    CHECK(sim::World::load(good, types).ok());
}

TEST_CASE("health: a damaged ship sails slower by the stated factor (full-health-only fails)") {
    // Two otherwise identical ships, 36 rows apart, 50 tiles east. Ship 2 is at 3000 of 6000 hp:
    // term trunc(35 * 3000 / 6000) = 17, V = 82 (speed_factor_vectors.txt half_empty). The sailed
    // time is proportional to 1 / V: ticks_damaged * 82 == ticks_full * 100 up to the 5-tick
    // advance granularity and the one-time planning wait.
    const auto types = registry_of(kFacts);
    sim::World w =
        make(spec_of({ShipSpec{.row = 64}, ShipSpec{.row = 100, .health = 3000}}), types);
    REQUIRE(w.apply(move(1, 70, 64)).ok());
    REQUIRE(w.apply(move(2, 70, 100)).ok());
    (void)w.drain_events();
    std::uint64_t t1 = 0, t2 = 0;
    std::uint32_t f1 = 0, f2 = 0;
    std::int32_t x1_at_1000 = 0, x2_at_1000 = 0;
    for (int i = 0; i < 4000 && (t1 == 0 || t2 == 0); ++i) {
        w.advance(10);
        if (f1 == 0 && fig(w, 1).nav->seg.kind == sim::NavSegKind::Move) {
            f1 = fig(w, 1).nav->seg.factor_percent;
        }
        if (f2 == 0 && fig(w, 2).nav->seg.kind == sim::NavSegKind::Move) {
            f2 = fig(w, 2).nav->seg.factor_percent;
        }
        if (w.clock().tick() == 1000) {
            x1_at_1000 = fig(w, 1).pos.x;
            x2_at_1000 = fig(w, 2).pos.x;
        }
        for (const sim::SimEvent& e : w.drain_events()) {
            if (e.kind == sim::SimEventKind::ShipArrived) {
                (e.figure == sim::FigureId{1} ? t1 : t2) = w.clock().tick();
            }
        }
    }
    // The factors of the movement segments: exact.
    CHECK(f1 == 100);
    CHECK(f2 == 82);
    REQUIRE(t1 != 0);
    REQUIRE(t2 != 0);
    MESSAGE("arrival full " << t1 << ", damaged " << t2);
    CHECK(t2 > t1);
    // 50 tiles: about 6900 ticks at 100 percent; the damaged ship needs 100 / 82 of the sailing
    // time. The identical start and planning wait cancel to a few ticks.
    const std::int64_t lhs = static_cast<std::int64_t>(t2) * 82;
    const std::int64_t rhs = static_cast<std::int64_t>(t1) * 100;
    CHECK(std::abs(lhs - rhs) <= 82 * 60); // 60 ticks of slack on the damaged time
    CHECK(t2 - t1 > 1200);                 // a full-health-only implementation arrives together
    // At tick 1000 both have sailed: ship 2 covered 82 percent of ship 1's distance (+- a tile).
    const std::int32_t d1 = x1_at_1000 - tile_c(20);
    const std::int32_t d2 = x2_at_1000 - tile_c(20);
    REQUIRE(d1 > 5 * kTile);
    CHECK(std::abs(d2 * 100 - d1 * 82) <= 100 * 24);
    CHECK(w.check_invariants().empty());
}

TEST_CASE(
    "health: unknown health sails like full health, cargo and health combine, owner and type") {
    const auto types = registry_of(kFacts);
    const sim::GoodsId g = ship_test::goods(*types, "HOLZ");
    const std::uint64_t cap = types->find_figure_type(kShip)->capacity_88;
    REQUIRE(cap == 1000 * 256);
    const std::vector<sim::ShipHold> full4 = {{g, cap}, {g, cap}, {g, cap}, {g, cap}};
    // 1 full, 2 unknown, 3 cargo at full, 4 cargo at half hp, 5 cargo at half hp, other owner,
    // 6 cargo at zero hp.
    sim::World w = make(spec_of({ShipSpec{.row = 40}, ShipSpec{.row = 46, .unknown = true},
                                 ShipSpec{.row = 52, .holds = full4},
                                 ShipSpec{.row = 58, .health = 3000, .holds = full4},
                                 ShipSpec{.owner = 1, .row = 64, .health = 3000, .holds = full4},
                                 ShipSpec{.row = 70, .health = 0, .holds = full4}}),
                        types);
    for (std::uint64_t id = 1; id <= 6; ++id) {
        REQUIRE(w.apply(move(id, 60, 40 + 6 * static_cast<int>(id - 1), id == 5 ? 1 : 0)).ok());
    }
    std::map<std::uint64_t, std::uint32_t> factor;
    for (int i = 0; i < 200; ++i) {
        w.advance(10);
        for (std::uint64_t id = 1; id <= 6; ++id) {
            if (!factor.count(id) && fig(w, id).nav->seg.kind == sim::NavSegKind::Move) {
                factor[id] = fig(w, id).nav->seg.factor_percent;
            }
        }
    }
    // speed_factor_vectors.txt: full_empty 100, unknown_equals_full 100, full_cargo_full_hp 89,
    // full_cargo_half_hp 72, other_owner_damaged 82 (player 1 is not the cargo Volk),
    // full_cargo_zero_hp 57.
    CHECK(factor[1] == 100);
    CHECK(factor[2] == 100);
    CHECK(factor[3] == 89);
    CHECK(factor[4] == 72);
    CHECK(factor[5] == 82);
    CHECK(factor[6] == 57);
}

TEST_CASE("health: another type with another maximum scales by its own max_hp") {
    const sim::NavFacts small{154618816U, 472446400U, 200U, 4U, 2U, 0U, 1280U};
    const auto types = registry_of(small, 1503);
    sim::World w = make(spec_of({ShipSpec{.kind = 1503, .row = 40, .health = 100},
                                 ShipSpec{.kind = 1503, .row = 46, .health = 199},
                                 ShipSpec{.kind = 1503, .row = 52}}),
                        types);
    // Full health of the small type is 200, not 6000.
    CHECK(fig(w, 3).nav->health == std::optional<std::uint32_t>{200});
    for (std::uint64_t id = 1; id <= 3; ++id) {
        REQUIRE(w.apply(move(id, 60, 40 + 6 * static_cast<int>(id - 1))).ok());
    }
    std::map<std::uint64_t, std::uint32_t> factor;
    for (int i = 0; i < 200; ++i) {
        w.advance(10);
        for (std::uint64_t id = 1; id <= 3; ++id) {
            if (!factor.count(id) && fig(w, id).nav->seg.kind == sim::NavSegKind::Move) {
                factor[id] = fig(w, id).nav->seg.factor_percent;
            }
        }
    }
    CHECK(factor[1] == 82); // type_200_half
    CHECK(factor[2] == 99); // type_200_199
    CHECK(factor[3] == 100);
    // The same health of 100 on the 6000 hp type is a sixtieth of the maximum: term 0, V 65.
    sim::World w6 = make(spec_of({ShipSpec{.health = 100}}), registry_of(kFacts));
    REQUIRE(w6.apply(move(1, 60, 64)).ok());
    std::uint32_t f = 0;
    for (int i = 0; i < 200 && f == 0; ++i) {
        w6.advance(10);
        if (fig(w6, 1).nav->seg.kind == sim::NavSegKind::Move) {
            f = fig(w6, 1).nav->seg.factor_percent;
        }
    }
    CHECK(f == 65);
}

TEST_CASE(
    "health: damage applied mid-voyage, saved and reloaded, the voyage agrees hash for hash") {
    const auto types = registry_of(kFacts);
    sim::World w = make(spec_of({ShipSpec{.row = 64}}), types);
    REQUIRE(w.apply(move(1, 70, 64)).ok());
    for (int i = 0; i < 300; ++i) {
        w.advance(10);
    }
    REQUIRE(fig(w, 1).nav->seg.kind == sim::NavSegKind::Move);
    REQUIRE(fig(w, 1).nav->seg.factor_percent == 100);
    // Damage (the test seam; no combat exists): the running segment keeps the factor it was
    // started with, the next segment start reads the new health (7.5: V is computed at the start).
    fig(w, 1).nav->health = 1500; // trunc(35 * 1500 / 6000) = 8: V = 73
    REQUIRE(w.check_invariants().empty());
    CHECK(fig(w, 1).nav->seg.factor_percent == 100);
    auto reloaded = sim::World::load(w.save(), types);
    REQUIRE_MESSAGE(reloaded.ok(), (reloaded.ok() ? std::string() : reloaded.error()));
    sim::World r = std::move(reloaded).value();
    REQUIRE(r.state_hash() == w.state_hash());
    CHECK(fig(r, 1).nav->health == std::optional<std::uint32_t>{1500});
    bool factor_changed = false;
    std::uint64_t arrived_w = 0, arrived_r = 0;
    for (int i = 0; i < 3000 && (arrived_w == 0 || arrived_r == 0); ++i) {
        w.advance(10);
        r.advance(10);
        REQUIRE(r.state_hash() == w.state_hash());
        factor_changed = factor_changed || fig(w, 1).nav->seg.factor_percent == 73;
        for (const sim::SimEvent& e : w.drain_events()) {
            if (e.kind == sim::SimEventKind::ShipArrived && arrived_w == 0) {
                arrived_w = w.clock().tick();
            }
        }
        for (const sim::SimEvent& e : r.drain_events()) {
            if (e.kind == sim::SimEventKind::ShipArrived && arrived_r == 0) {
                arrived_r = r.clock().tick();
            }
        }
    }
    CHECK(factor_changed);
    CHECK(arrived_w != 0);
    CHECK(arrived_w == arrived_r);
    // A full-health voyage of the same ship would have ended much earlier (about 6900 ticks).
    sim::World f = make(spec_of({ShipSpec{.row = 64}}), types);
    REQUIRE(f.apply(move(1, 70, 64)).ok());
    const Sail full = sail(f, 1);
    REQUIRE(full.arrival_tick != 0);
    CHECK(arrived_w > full.arrival_tick + 300);
    // The saved health survives a second save/load unchanged (byte-identical frame).
    CHECK(r.save() == w.save());
}

TEST_CASE("health: unknown health survives save and load and reloads unknown") {
    const auto types = registry_of(kFacts);
    sim::World w = make(spec_of({ShipSpec{.unknown = true}}), types);
    REQUIRE(w.apply(move(1, 70, 64)).ok());
    for (int i = 0; i < 100; ++i) {
        w.advance(10);
    }
    auto reloaded = sim::World::load(w.save(), types);
    REQUIRE(reloaded.ok());
    sim::World r = std::move(reloaded).value();
    CHECK_FALSE(fig(r, 1).nav->health.has_value());
    CHECK(r.state_hash() == w.state_hash());
    // Unknown and full health are different states (the hash tells them apart) with the same
    // voyage (the adaptation of 12.14).
    sim::World full = make(spec_of({ShipSpec{}}), types);
    REQUIRE(full.apply(move(1, 70, 64)).ok());
    for (int i = 0; i < 100; ++i) {
        full.advance(10);
    }
    CHECK(full.state_hash() != w.state_hash());
    CHECK(fig(full, 1).pos.x == fig(w, 1).pos.x);
    CHECK(fig(full, 1).nav->seg == fig(w, 1).nav->seg);
}
