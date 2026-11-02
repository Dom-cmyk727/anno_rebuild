// MAP-NAV-3 regressions: the three reproduced review findings (N0) and, with each correction, its
// production-path proof (N1 carriage and indexes, N2 stamp units). Synthetic: an open 512 x 512
// sea, no game files. Every test drives the public scenario / command / advance / save path.
#include <cstdint>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/ship_mover.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "ship_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using ship_test::kScout;
using ship_test::kShip;

namespace {

constexpr std::int32_t kTile = 256;

std::int32_t tile_c(int t) {
    return t * kTile + kTile / 2;
}

sim::ScenarioSpec::FigureSpec fig(std::uint32_t kind, sim::FixedPos pos) {
    sim::ScenarioSpec::FigureSpec f;
    f.kind = kind;
    f.owner = sim::PlayerId{0};
    f.pos = pos;
    f.dir16 = 8;
    return f;
}

sim::ScenarioSpec base_spec() {
    sim::ScenarioSpec spec = syn::empty_scenario(11);
    sim::ScenarioSpec::PlayerSpec ps;
    ps.id = sim::PlayerId{0};
    ps.money = 1000;
    spec.players.push_back(ps);
    return spec;
}

sim::World make(const sim::ScenarioSpec& spec) {
    auto w = sim::World::create(spec, ship_test::nav_ship_registry(), {}, {}, {},
                                sim::serial_executor());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

const sim::Figure& fig_of(const sim::World& w, std::uint64_t id) {
    const sim::Figure* f = sim::find_figure(w.figures(), sim::FigureId{id});
    REQUIRE(f != nullptr);
    return *f;
}

sim::Command move(std::uint64_t id, int tx, int ty) {
    return sim::Command{sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{id}, tile_c(tx), tile_c(ty)}};
}

} // namespace

TEST_CASE("N0 stamp: a moving ship is stamped at position + 25 * its segment velocity") {
    // Ship 1 sails east at the type speed 154618816 / 2^32 = 0.036 tile per stepper unit, factor
    // 100: its velocity is (0.036, 0), the stamp (x + 0.9, z) -> one tile ahead, not 25.
    sim::ScenarioSpec spec = base_spec();
    spec.figures.push_back(fig(kShip, sim::FixedPos{5340, tile_c(64)}));
    spec.figures.push_back(fig(kShip, sim::FixedPos{tile_c(22), tile_c(66)})); // the planner
    sim::World w = make(spec);
    REQUIRE(w.apply(move(1, 70, 64)).ok());
    bool moving = false;
    for (int i = 0; i < 40 && !moving; ++i) {
        w.advance(10);
        moving = fig_of(w, 1).nav->seg.kind == sim::NavSegKind::Move;
    }
    REQUIRE(moving);
    const sim::Figure& a = fig_of(w, 1);
    REQUIRE(a.nav->seg.dir8 == 4); // east
    REQUIRE(a.nav->seg.speed_q32 == 154618816U);
    REQUIRE(a.nav->seg.factor_percent == 100U);
    const auto stamps = sim::ShipMover::local_stamps(w, fig_of(w, 2));
    REQUIRE(stamps.size() == 1);
    // The documented equation: trunc(x + 25 * velocity), x in tiles, velocity in tile per unit.
    const std::int64_t num =
        static_cast<std::int64_t>(a.pos.x) * (std::int64_t{1} << 24) + 25 * std::int64_t{154618816};
    const std::int32_t expected_x = static_cast<std::int32_t>(num >> 32);
    MESSAGE("ship x_q8=" << a.pos.x << " stamp tile x=" << stamps[0].tile_x
                         << " expected (velocity) x=" << expected_x);
    CHECK(stamps[0].tile_x == expected_x);
    CHECK(stamps[0].tile_z == 64);
}

TEST_CASE("N0 carriage: a navigating figure carried by a ship keeps a coherent state") {
    sim::ScenarioSpec spec = base_spec();
    spec.figures.push_back(fig(kShip, sim::FixedPos{5340, tile_c(64)}));
    for (int i = 0; i < 11; ++i) {
        spec.figures.push_back(fig(kScout, sim::FixedPos{tile_c(300), tile_c(300)}));
    }
    sim::ScenarioSpec::FigureSpec child = fig(kShip, sim::FixedPos{5340, tile_c(64)});
    child.parent = 0;
    spec.figures.push_back(child);
    sim::World w = make(spec);
    REQUIRE(w.apply(move(1, 70, 64)).ok());
    std::string first_problem;
    for (int i = 0; i < 20 && first_problem.empty(); ++i) {
        w.advance(10);
        first_problem = w.check_invariants();
    }
    MESSAGE("first invariant problem: " << (first_problem.empty() ? "none" : first_problem));
    CHECK(first_problem.empty());
    const auto reloaded = sim::World::load(w.save(), ship_test::nav_ship_registry());
    MESSAGE("reload: " << (reloaded.ok() ? "ok" : reloaded.error()));
    CHECK(reloaded.ok());
}

TEST_CASE("N0 carriage: a ninth-level plain descendant rides with the ship") {
    sim::ScenarioSpec spec = base_spec();
    spec.figures.push_back(fig(kShip, sim::FixedPos{5340, tile_c(64)}));
    for (int i = 0; i < 9; ++i) {
        sim::ScenarioSpec::FigureSpec f = fig(kScout, sim::FixedPos{5340, tile_c(64)});
        f.parent = static_cast<std::uint32_t>(i);
        spec.figures.push_back(f);
    }
    for (int i = 0; i < 11; ++i) {
        spec.figures.push_back(fig(kScout, sim::FixedPos{tile_c(300), tile_c(300)}));
    }
    sim::World w = make(spec);
    REQUIRE(w.apply(move(1, 70, 64)).ok());
    for (int i = 0; i < 40; ++i) {
        w.advance(10);
    }
    const std::int32_t sx = fig_of(w, 1).pos.x;
    MESSAGE("ship x=" << sx << ", level 1 x=" << fig_of(w, 2).pos.x
                      << ", level 9 x=" << fig_of(w, 10).pos.x);
    CHECK(sx != 5340);
    CHECK(fig_of(w, 10).pos.x == sx);
    CHECK(w.check_invariants().empty());
}

TEST_CASE("N0 controls: a standalone ship and a scout with its donkey stay coherent") {
    sim::ScenarioSpec spec = base_spec();
    spec.figures.push_back(fig(kShip, sim::FixedPos{5340, tile_c(64)}));
    sim::ScenarioSpec::FigureSpec scout = fig(kScout, sim::FixedPos{5340, tile_c(64)});
    scout.parent = 0;
    spec.figures.push_back(scout);
    sim::ScenarioSpec::FigureSpec donkey = scout;
    donkey.kind = ship_test::kDonkey;
    donkey.parent = 1;
    spec.figures.push_back(donkey);
    for (int i = 0; i < 11; ++i) {
        spec.figures.push_back(fig(kScout, sim::FixedPos{tile_c(300), tile_c(300)}));
    }
    sim::World w = make(spec);
    REQUIRE(w.apply(move(1, 70, 64)).ok());
    for (int i = 0; i < 40; ++i) {
        w.advance(10);
        REQUIRE(w.check_invariants().empty());
    }
    CHECK(fig_of(w, 1).pos.x != 5340);
    CHECK(fig_of(w, 3).pos.x == fig_of(w, 1).pos.x);
    CHECK(sim::World::load(w.save(), ship_test::nav_ship_registry()).ok());
}
