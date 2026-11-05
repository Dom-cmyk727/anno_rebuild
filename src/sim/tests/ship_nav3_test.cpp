// MAP-NAV-3 regressions: the three reproduced review findings (N0) and, with each correction, its
// production-path proof (N1 carriage and indexes, N2 stamp units). Synthetic: an open 512 x 512
// sea, no game files. Every test drives the public scenario / command / advance / save path.
#include <array>
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

TEST_CASE("N0 stamp: a moving ship is stamped at its position plus 25 times its segment velocity") {
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

TEST_CASE(
    "N2 stamp: a moving neighbour's velocity stamp changes the endpoint the world planner picks") {
    // Ship 1 sails east from x = 5340/256 = 20.86; ship 2 at (18, 64) is ordered to (21, 64). The
    // velocity stamp lies at tile 21 (20.86 + 0.9 -> 21.76), the old unit-direction stamp at 45.
    sim::ScenarioSpec spec = base_spec();
    spec.figures.push_back(fig(kShip, sim::FixedPos{5340, tile_c(64)}));
    spec.figures.push_back(fig(kShip, sim::FixedPos{tile_c(18), tile_c(64)}));
    sim::World w = make(spec);
    REQUIRE(w.apply(move(1, 90, 64)).ok());
    bool moving = false;
    for (int i = 0; i < 40 && !moving; ++i) {
        w.advance(10);
        moving = fig_of(w, 1).nav->seg.kind == sim::NavSegKind::Move;
    }
    REQUIRE(moving);
    {
        // The stamp ship 2 will plan with, by the documented equation trunc(x + 25 * 0.036).
        const std::int64_t num = std::int64_t{fig_of(w, 1).pos.x} * (std::int64_t{1} << 24) +
                                 25 * std::int64_t{154618816};
        const auto st = sim::ShipMover::local_stamps(w, fig_of(w, 2));
        REQUIRE(st.size() == 1);
        CHECK(st[0].tile_x == static_cast<std::int32_t>(num >> 32));
        CHECK(st[0].tile_x < 30); // not the old 25-tile look-ahead
        CHECK(st[0].tile_z == 64);
    }
    REQUIRE(w.apply(move(2, 21, 64)).ok());
    for (int i = 0; i < 4000 && fig_of(w, 2).nav->order.has_value(); ++i) {
        w.advance(10);
    }
    REQUIRE_FALSE(fig_of(w, 2).nav->order.has_value());
    // Correct velocity stamp (tile 21,64 by the equation): the planner ends on (20,65). The old
    // unit-direction adapter (stamp at 45) or any far look-ahead ends on (21,64).
    CHECK(fig_of(w, 2).pos.x / kTile == 20);
    CHECK(fig_of(w, 2).pos.y / kTile == 65);
    MESSAGE("ship 2 ended at tile " << fig_of(w, 2).pos.x / kTile << ","
                                    << fig_of(w, 2).pos.y / kTile);
}

TEST_CASE("N2 stamp velocity: axis, diagonal, factor, turn and idle segments") {
    const auto seg = [](sim::NavSegKind k, std::uint8_t dir, std::uint32_t factor) {
        sim::NavSegment s;
        s.kind = k;
        s.dir8 = dir;
        s.factor_percent = factor;
        s.speed_q32 = 154618816;
        return s;
    };
    using V = std::array<std::int32_t, 2>;
    // dir8: 0 -x, 1 (-x,+z), 2 +z, 3 (+x,+z), 4 +x, 5 (+x,-z), 6 -z, 7 (-x,-z).
    CHECK(sim::ShipMover::stamp_velocity_q32(seg(sim::NavSegKind::Move, 4, 100)) ==
          V{154618816, 0});
    CHECK(sim::ShipMover::stamp_velocity_q32(seg(sim::NavSegKind::Move, 0, 100)) ==
          V{-154618816, 0});
    CHECK(sim::ShipMover::stamp_velocity_q32(seg(sim::NavSegKind::Move, 6, 100)) ==
          V{0, -154618816});
    // factor 69: 154618816 * 69 / 100 = 106686983.04 -> 106686983 (truncated).
    CHECK(sim::ShipMover::stamp_velocity_q32(seg(sim::NavSegKind::Move, 2, 69)) == V{0, 106686983});
    // diagonal: 154618816 * 11863283 / 2^24 = 109332011.x -> 109332011 on each axis, signs by dir.
    const std::int32_t dg =
        static_cast<std::int32_t>(std::int64_t{154618816} * 11863283 / 16777216);
    CHECK(dg == 109332011);
    CHECK(sim::ShipMover::stamp_velocity_q32(seg(sim::NavSegKind::Move, 5, 100)) == V{dg, -dg});
    CHECK(sim::ShipMover::stamp_velocity_q32(seg(sim::NavSegKind::Move, 7, 100)) == V{-dg, -dg});
    // diagonal at factor 69, one truncation: 154618816 * 11863283 * 69 / (100 * 2^24) = 75439087.
    CHECK(sim::ShipMover::stamp_velocity_q32(seg(sim::NavSegKind::Move, 3, 69)) ==
          V{75439087, 75439087});
    CHECK(sim::ShipMover::stamp_velocity_q32(seg(sim::NavSegKind::Turn, 4, 100)) == V{0, 0});
    CHECK(sim::ShipMover::stamp_velocity_q32(seg(sim::NavSegKind::Idle, 4, 100)) == V{0, 0});
}

// ---------------------------------------------------------------------------------------------
// N1: coherent carriage and local derived indexes.

namespace {

sim::World carried_world(int fillers, int depth, bool nav_child) {
    sim::ScenarioSpec spec = base_spec();
    spec.figures.push_back(fig(kShip, sim::FixedPos{5340, tile_c(64)}));
    for (int i = 0; i < depth; ++i) {
        sim::ScenarioSpec::FigureSpec f = fig(kScout, sim::FixedPos{5340, tile_c(64)});
        f.parent = static_cast<std::uint32_t>(i);
        spec.figures.push_back(f);
    }
    if (nav_child) {
        sim::ScenarioSpec::FigureSpec c = fig(kShip, sim::FixedPos{5340, tile_c(64)});
        c.parent = 0;
        spec.figures.push_back(c);
    }
    for (int i = 0; i < fillers; ++i) {
        spec.figures.push_back(fig(kScout, sim::FixedPos{tile_c(300), tile_c(300)}));
    }
    return make(spec);
}

} // namespace

TEST_CASE("N1 carriage: a navigating child rides, never sails, and the state reloads") {
    sim::World w = carried_world(11, 2, true);
    const std::uint64_t child = 4; // ship 1, scouts 2 and 3, the navigating child 4
    REQUIRE(fig_of(w, child).nav.has_value());
    REQUIRE(w.apply(move(1, 70, 64)).ok());
    // The carried child cannot be ordered (atomic refusal) ...
    const auto refused = w.apply(move(child, 60, 70));
    CHECK(refused.status == sim::Reject::ShipEmbarked);
    for (int i = 0; i < 400; ++i) {
        w.advance(10);
        REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    }
    const sim::Figure& c = fig_of(w, child);
    CHECK(c.pos.x == fig_of(w, 1).pos.x); // follows the carrier ...
    CHECK(c.pos.x != 5340);
    CHECK_FALSE(c.nav->order.has_value()); // ... never holds a plan of its own
    CHECK_FALSE(c.nav->ticket.has_value());
    CHECK(c.nav->seg.kind == sim::NavSegKind::Idle);
    // The fine position is the carrier's published position exactly.
    CHECK(c.nav->pos_u[0] == std::int64_t{c.pos.x} * sim::kNavPosUnitsPerQ8);
    auto reloaded = sim::World::load(w.save(), ship_test::nav_ship_registry());
    REQUIRE_MESSAGE(reloaded.ok(), (reloaded.ok() ? "" : reloaded.error()));
    sim::World r = std::move(reloaded).value();
    for (int i = 0; i < 50; ++i) {
        w.advance(10);
        r.advance(10);
    }
    CHECK(r.state_hash() == w.state_hash());
}

TEST_CASE("N1 carriage: a carried navigating figure with an order, ticket or motion is refused") {
    sim::World w = carried_world(0, 0, true);
    const std::uint64_t child = 2;
    REQUIRE(w.check_invariants().empty());
    sim::Figure* c = sim::find_figure(sim::WorldTestAccess::figures(w), sim::FigureId{child});
    REQUIRE(c != nullptr);
    c->nav->order = sim::NavOrder{1, false, 1000, 1000};
    const std::string msg = w.check_invariants();
    CHECK(msg.find("a carried navigating figure") != std::string::npos);
    CHECK(msg.find("does not sail on its own") != std::string::npos);
    c->nav->order.reset();
    CHECK(w.check_invariants().empty());
    c->state_word |= 1U;
    CHECK(w.check_invariants().find("a carried navigating figure") != std::string::npos);
}

TEST_CASE(
    "N1 carriage: a deep chain rides whole and the walk costs the subtree, not the registry") {
    std::uint64_t visits_small = 0, visits_big = 0;
    for (const int fillers : {11, 600}) {
        sim::World w = carried_world(fillers, 40, false); // 40 levels: beyond any small cutoff
        REQUIRE(w.apply(move(1, 70, 64)).ok());
        for (int i = 0; i < 60; ++i) {
            w.advance(10);
            REQUIRE(w.check_invariants().empty());
        }
        CHECK(fig_of(w, 1).pos.x != 5340);
        CHECK(fig_of(w, 41).pos.x == fig_of(w, 1).pos.x); // the 40th descendant
        const auto& work = sim::WorldTestAccess::route_cache(w).work;
        (fillers == 11 ? visits_small : visits_big) = work.crew_visits;
    }
    // Independent of the unrelated figures: the same ship moved the same number of times.
    CHECK(visits_small > 0);
    CHECK(visits_small == visits_big);
    CHECK(visits_small % 40 == 0); // exactly 40 descendants per moved visit
}

TEST_CASE("N1 index: stamp and admission work do not grow with unrelated figures") {
    std::uint64_t cells[2] = {0, 0}, entries[2] = {0, 0}, adm[2] = {0, 0};
    int k = 0;
    for (const int fillers : {20, 2000}) {
        sim::ScenarioSpec spec = base_spec();
        for (int i = 0; i < 6; ++i) {
            spec.figures.push_back(fig(kShip, sim::FixedPos{tile_c(20 + i), tile_c(64)}));
        }
        for (int i = 0; i < fillers; ++i) {
            spec.figures.push_back(fig(kScout, sim::FixedPos{tile_c(300), tile_c(300)}));
        }
        sim::World w = make(spec);
        for (std::uint64_t id = 1; id <= 6; ++id) {
            REQUIRE(w.apply(move(id, 90, 70)).ok());
        }
        for (int i = 0; i < 200; ++i) {
            w.advance(10);
        }
        REQUIRE(w.check_invariants().empty());
        const auto& wk = sim::WorldTestAccess::route_cache(w).work;
        cells[k] = wk.stamp_cells;
        entries[k] = wk.stamp_entries;
        adm[k] = wk.admission_steps;
        ++k;
    }
    // The registry visit cadence differs with the figure count, but each planning attempt costs a
    // constant: the totals per planned ship stay within the same small bound.
    CHECK(entries[0] > 0);
    CHECK(cells[1] <= 6 * 8 * 9); // <= 6 plans, a radius-5 window touches <= 4 cells... bound 9
    CHECK(entries[1] <= 6 * 8 * 9 * 6);
    CHECK(adm[1] <= 6 * 8 * 4); // each attempt looks at <= plans_per_advance (4) tickets
}

TEST_CASE("N1 index: moving and removing ships keep every derived index equal to a rebuild") {
    sim::World w = carried_world(5, 3, true);
    REQUIRE(w.apply(move(1, 120, 64)).ok());
    for (int i = 0; i < 500; ++i) {
        w.advance(10); // crosses several 16-tile cells
        REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    }
    CHECK(sim::figure_index_problem(w.figures()).empty());
    // A copy carries an equal index, advances independently.
    sim::World copy = w;
    CHECK(copy.check_invariants().empty());
    copy.advance(10);
    CHECK(w.check_invariants().empty());
    // Removal: the ship leaves its cell, its tickets and its children entries.
    sim::FigureRegistry reg = w.figures();
    REQUIRE(sim::remove_figure(reg, sim::FigureId{1}).has_value());
    CHECK(reg.ship_cells.empty()); // the carried navigating child is not a sailing ship
    for (const auto& kv : reg.ship_cells) {
        CHECK(kv.second.count(sim::FigureId{1}) == 0);
    }
    CHECK(sim::figure_index_problem(reg).empty());
    // A stale index is detected: edit a position without telling the registry.
    sim::Figure* f = sim::find_figure(sim::WorldTestAccess::figures(w), sim::FigureId{1});
    f->pos.x = tile_c(400);
    f->nav->pos_u[0] = std::int64_t{f->pos.x} * sim::kNavPosUnitsPerQ8;
    CHECK(w.check_invariants().find("ship_cells") != std::string::npos);
}

TEST_CASE("N1 index: more ships in range than the unit cap are cut at the cap in id order") {
    sim::ScenarioSpec spec = base_spec();
    for (int i = 0; i < 70; ++i) {
        spec.figures.push_back(fig(kShip, sim::FixedPos{tile_c(100), tile_c(100)}));
    }
    sim::World w = make(spec);
    const auto stamps = sim::ShipMover::local_stamps(w, fig_of(w, 1));
    // 64 hits (ids 1..64 in the single cell, ascending), the planner itself skipped.
    CHECK(stamps.size() == 63);
}

TEST_CASE("N1 carriage: navigating descendants below a plain carrier and a navigating child "
          "of a navigating child ride whole") {
    sim::ScenarioSpec spec = base_spec();
    spec.figures.push_back(fig(kShip, sim::FixedPos{5340, tile_c(64)})); // 1 the ship
    const auto child = [&](std::uint32_t kind, std::uint32_t parent) {
        sim::ScenarioSpec::FigureSpec f = fig(kind, sim::FixedPos{5340, tile_c(64)});
        f.parent = parent;
        spec.figures.push_back(f);
    };
    child(kScout, 0); // 2 plain under the ship
    child(kShip, 1);  // 3 navigating under a plain carrier
    child(kShip, 2);  // 4 navigating child of a navigating child
    child(kScout, 3); // 5 plain under that
    // An unrelated parent tree elsewhere: its members must not move.
    spec.figures.push_back(fig(kScout, sim::FixedPos{tile_c(300), tile_c(300)})); // 6
    child(kScout, 5);                                                             // 7
    sim::World w = make(spec);
    REQUIRE(w.apply(move(1, 70, 64)).ok());
    for (int i = 0; i < 300; ++i) {
        w.advance(10);
        REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    }
    const std::int32_t x = fig_of(w, 1).pos.x;
    CHECK(x != 5340);
    for (const std::uint64_t id : {2U, 3U, 4U, 5U}) {
        CHECK(fig_of(w, id).pos.x == x);
        CHECK(fig_of(w, id).pos.y == fig_of(w, 1).pos.y);
    }
    CHECK(fig_of(w, 6).pos.x == tile_c(300));
    CHECK(fig_of(w, 7).pos.x == 5340); // child of the unrelated tree keeps its own position
    for (const std::uint64_t id : {3U, 4U}) {
        CHECK_FALSE(fig_of(w, id).nav->order.has_value());
        CHECK(fig_of(w, id).nav->pos_u[0] == std::int64_t{x} * sim::kNavPosUnitsPerQ8);
    }
    CHECK(sim::World::load(w.save(), ship_test::nav_ship_registry()).ok());
}

TEST_CASE("N1 index: removing a parent keeps every derived index equal to a rebuild, and a hostile "
          "carried order is refused by save") {
    sim::World w = carried_world(2, 3, true);
    sim::FigureRegistry& reg = sim::WorldTestAccess::figures(w);
    REQUIRE(sim::figure_index_problem(reg).empty());
    REQUIRE(sim::remove_figure(reg, sim::FigureId{3}).has_value()); // a middle carrier
    CHECK(sim::figure_index_problem(reg).empty());
    CHECK(reg.children.count(sim::FigureId{3}) == 1); // its child still names it as parent
    REQUIRE(sim::remove_figure(reg, sim::FigureId{4}).has_value());
    CHECK(reg.children.count(sim::FigureId{3}) == 0);
    CHECK(sim::figure_index_problem(reg).empty());
    // A carried navigating figure given an order is refused when the state is saved.
    sim::World v = carried_world(0, 0, true);
    sim::find_figure(sim::WorldTestAccess::figures(v), sim::FigureId{2})->nav->order =
        sim::NavOrder{1, false, 1000, 1000};
    CHECK(v.check_invariants().find("does not sail on its own") != std::string::npos);
}

TEST_CASE("N1 index: the stamp query's work does not depend on far-away ships") {
    sim::NavWork seen[2];
    int k = 0;
    for (const int far_ships : {10, 600}) {
        sim::ScenarioSpec spec = base_spec();
        for (int i = 0; i < 3; ++i) { // a neighbourhood: three ships in range of ship 1
            spec.figures.push_back(fig(kShip, sim::FixedPos{tile_c(100 + i), tile_c(100)}));
        }
        for (int i = 0; i < far_ships; ++i) { // far-away parked ships, many cells away
            spec.figures.push_back(
                fig(kShip, sim::FixedPos{tile_c(300 + i % 100), tile_c(200 + i / 100)}));
        }
        sim::World w = make(spec);
        sim::NavWork work;
        const auto stamps = sim::ShipMover::local_stamps(w, fig_of(w, 1), &work);
        CHECK(stamps.size() == 2); // the two other near ships
        seen[k++] = work;
    }
    CHECK(seen[0] == seen[1]);
    CHECK(seen[0].stamp_entries == 3);
}
