// The local-unit query (route_stamps.hpp): scan order, caps, radius, state / group filters, stamp
// positions, work bounded by the cells scanned, independence of the view's order.
#include <algorithm>
#include <cstdint>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/route.hpp>
#include <anno/sim/route_stamps.hpp>

#include "route_test_support.hpp"

namespace sim = anno::sim;

namespace {

/// A view over a flat list of ships; counts its calls. `reverse` answers each cell back to front.
class ListView final : public sim::ShipSpatialView {
  public:
    explicit ListView(std::vector<sim::LocalShip> ships, bool reverse = false)
        : ships_(std::move(ships)), reverse_(reverse) {}
    void ships_in_cell(std::int32_t cx, std::int32_t cz,
                       std::vector<sim::LocalShip>& out) const override {
        ++calls;
        // A real view indexes by cell; this one filters the whole list so the test can count work.
        for (const auto& s : ships_) {
            if (sim::ship_cell_of(s.x_q8) == cx && sim::ship_cell_of(s.z_q8) == cz) {
                out.push_back(s);
            }
        }
        if (reverse_) {
            std::reverse(out.begin(), out.end());
        }
    }
    mutable std::uint64_t calls = 0;

  private:
    std::vector<sim::LocalShip> ships_;
    bool reverse_;
};

sim::LocalShip ship(std::uint64_t id, double tx, double tz) {
    sim::LocalShip s;
    s.id = id;
    s.x_q8 = static_cast<std::int32_t>(tx * 256);
    s.z_q8 = static_cast<std::int32_t>(tz * 256);
    return s;
}

sim::LocalStampQuery query_at(double tx, double tz) {
    sim::LocalStampQuery q;
    q.x_q8 = static_cast<std::int32_t>(tx * 256);
    q.z_q8 = static_cast<std::int32_t>(tz * 256);
    q.self_id = 9999;
    q.world_w = 1431;
    q.world_h = 1431;
    return q;
}

} // namespace

TEST_CASE(
    "local stamps: cell scan order z-cell, x-cell, ascending id, independent of the view's order") {
    // Query at (48, 48) radius 5: cells (2..3, 2..3). Ships in all four cells, ids not in scan
    // order.
    std::vector<sim::LocalShip> ships = {ship(4, 50.5, 50.5), ship(1, 50.5, 46.5),
                                         ship(3, 46.5, 50.5), ship(2, 46.5, 46.5),
                                         ship(7, 46.9, 46.1), ship(6, 50.1, 46.9)};
    for (auto& s : ships) {
        s.has_direction = true; // stationary: direction 0 -> stamp at the ship's own tile
    }
    sim::RouteLimits l;
    sim::LocalStampStats st;
    const auto base = sim::local_ship_stamps(ListView(ships), query_at(48, 48), l, &st);
    // Scan: cell (2,2): ids 2, 7 -> tiles (46,46), (46,46); cell (3,2): ids 1, 6 -> (50,46),
    // (50,46); cell (2,3): id 3 -> (46,50); cell (3,3): id 4 -> (50,50).
    REQUIRE(base.size() == 6);
    const std::vector<std::pair<int, int>> want = {{46, 46}, {46, 46}, {50, 46},
                                                   {50, 46}, {46, 50}, {50, 50}};
    for (std::size_t i = 0; i < want.size(); ++i) {
        CHECK(base[i].tile_x == want[i].first);
        CHECK(base[i].tile_z == want[i].second);
        CHECK(base[i].kernel == sim::StampKernel::K5);
    }
    CHECK(st.cells_scanned == 4);
    CHECK_FALSE(st.capped);
    // Insertion order of the list and the view's in-cell order do not matter (ids 2,7 and 1,6 swap
    // tiles).
    std::vector<sim::LocalShip> shuffled = ships;
    std::reverse(shuffled.begin(), shuffled.end());
    CHECK(sim::local_ship_stamps(ListView(shuffled, true), query_at(48, 48), l) == base);
    // Distinguish the id order inside a cell: ids 2 and 7 at different tiles.
    std::vector<sim::LocalShip> two = {ship(7, 46.5, 46.5), ship(2, 45.5, 45.5)};
    const auto o = sim::local_ship_stamps(ListView(two), query_at(48, 48), l);
    REQUIRE(o.size() == 2);
    CHECK(o[0].tile_x == 45); // id 2 first although it was listed last
    CHECK(o[1].tile_x == 46);
}

TEST_CASE("local stamps: more units than unit_cap are truncated in scan order, self counts against "
          "the cap") {
    std::vector<sim::LocalShip> ships;
    for (std::uint64_t i = 1; i <= 100; ++i) {
        ships.push_back(ship(i, 40.5 + static_cast<double>(i % 4) * 0.5,
                             40.5 + static_cast<double>(i % 3) * 0.5));
    }
    sim::LocalStampQuery q = query_at(41, 41);
    sim::RouteLimits l; // unit_cap 64
    sim::LocalStampStats st;
    const auto capped = sim::local_ship_stamps(ListView(ships), q, l, &st);
    CHECK(capped.size() == 64);
    CHECK(st.capped);
    // The first 64 are ids 1..64 (one cell, ascending id): compare with the same list truncated by
    // hand.
    std::vector<sim::LocalShip> first64(ships.begin(), ships.begin() + 64);
    sim::RouteLimits wide;
    wide.unit_cap = 1000;
    CHECK(sim::local_ship_stamps(ListView(first64), q, wide) == capped);
    // Control: with room for all 100 the result has 100 stamps and the first 64 are the same.
    const auto all = sim::local_ship_stamps(ListView(ships), q, wide);
    REQUIRE(all.size() == 100);
    CHECK(std::equal(capped.begin(), capped.end(), all.begin()));
    // The querying ship is inside the cap and skipped afterwards: 63 stamps.
    q.self_id = 10;
    CHECK(sim::local_ship_stamps(ListView(ships), q, l).size() == 63);
    // unit_cap 0 scans nothing.
    l.unit_cap = 0;
    sim::LocalStampStats none;
    CHECK(sim::local_ship_stamps(ListView(ships), q, l, &none).empty());
    CHECK(none.cells_scanned == 0);
}

TEST_CASE("local stamps: unrelated distant figures are not looked at, work is bounded by the cells "
          "scanned") {
    std::vector<sim::LocalShip> ships = {ship(1, 100.5, 100.5)};
    for (std::uint64_t i = 0; i < 3000; ++i) {
        ships.push_back(ship(100 + i, 600.5 + static_cast<double>(i % 50),
                             700.5 + static_cast<double>(i / 50)));
    }
    const ListView view(ships);
    sim::LocalStampStats st;
    const auto s = sim::local_ship_stamps(view, query_at(101, 101), sim::RouteLimits{}, &st);
    REQUIRE(s.size() == 1);
    CHECK(st.cells_scanned == 1); // radius 5 inside one 16 x 16 cell
    CHECK(view.calls == 1);       // the view was asked for one cell, not for 3001 figures
    CHECK(st.ships_examined == 1);
    // A query on a cell border scans exactly four cells.
    const ListView view2(ships);
    sim::LocalStampStats st2;
    (void)sim::local_ship_stamps(view2, query_at(96, 96), sim::RouteLimits{}, &st2);
    CHECK(st2.cells_scanned == 4);
    CHECK(view2.calls == 4);
    // Near the world origin the scan is clipped to the grid.
    const ListView view3(ships);
    sim::LocalStampStats st3;
    (void)sim::local_ship_stamps(view3, query_at(1, 1), sim::RouteLimits{}, &st3);
    CHECK(st3.cells_scanned == 1);
    // And at the far edge (world 1431 -> 90 cells: cell 89 is the last).
    const ListView view4(ships);
    sim::LocalStampStats st4;
    (void)sim::local_ship_stamps(view4, query_at(1430, 1430), sim::RouteLimits{}, &st4);
    CHECK(st4.cells_scanned == 1);
}

TEST_CASE("local stamps: the radius is inclusive, the filters are state 0xa, ship group, direction "
          "and self") {
    const auto count = [](std::vector<sim::LocalShip> ships) {
        return sim::local_ship_stamps(ListView(std::move(ships)), query_at(40, 40),
                                      sim::RouteLimits{})
            .size();
    };
    // Exactly 5.0 away (1280 q8): in; one q8 unit further: out. 3-4-5 triangle: in; beyond by 1
    // unit: out.
    CHECK(count({ship(1, 45, 40)}) == 1);
    sim::LocalShip far = ship(1, 45, 40);
    far.x_q8 += 1;
    CHECK(count({far}) == 0);
    CHECK(count({ship(1, 43, 44)}) == 1);
    sim::LocalShip tri = ship(1, 43, 44);
    tri.z_q8 += 1;
    CHECK(count({tri}) == 0);
    // Negative side.
    CHECK(count({ship(1, 35, 40)}) == 1);
    sim::LocalShip left = ship(1, 35, 40);
    left.x_q8 -= 1;
    CHECK(count({left}) == 0);
    // State: 0x2 and 0x8 exclude, other bits do not.
    for (const std::uint32_t state : {0x2u, 0x8u, 0xau, 0x3u}) {
        sim::LocalShip s = ship(1, 41, 41);
        s.state_word = state;
        CHECK(count({s}) == 0);
    }
    for (const std::uint32_t state : {0x0u, 0x1u, 0x4u, 0x100u, 0x2000u}) {
        sim::LocalShip s = ship(1, 41, 41);
        s.state_word = state;
        CHECK(count({s}) == 1);
    }
    // Group, direction, self.
    sim::LocalShip not_ship = ship(1, 41, 41);
    not_ship.ship_group = false;
    CHECK(count({not_ship}) == 0);
    sim::LocalShip no_dir = ship(1, 41, 41);
    no_dir.has_direction = false;
    CHECK(count({no_dir}) == 0);
    CHECK(count({ship(9999, 41, 41)}) == 0); // self
    CHECK(count({ship(9998, 41, 41)}) == 1);
}

TEST_CASE("local stamps: the stamp point is pos + dir x 25 truncated toward zero, exactly") {
    const auto stamp_of = [](sim::LocalShip s) {
        const auto v = sim::local_ship_stamps(
            ListView({s}), query_at(s.x_q8 / 256.0, s.z_q8 / 256.0), sim::RouteLimits{});
        REQUIRE(v.size() == 1);
        return v[0];
    };
    sim::LocalShip e = ship(1, 10.5, 20.5);
    e.dir_x_q24 = 16777216; // east
    CHECK(stamp_of(e).tile_x == 35);
    CHECK(stamp_of(e).tile_z == 20);
    sim::LocalShip d = ship(1, 10.5, 20.5);
    d.dir_x_q24 = 11863283; // diagonal SE component
    d.dir_z_q24 = 11863283;
    // 10.5 + 25 * 0.70710677 = 28.1776; 20.5 + 17.6777 = 38.1776.
    CHECK(stamp_of(d).tile_x == 28);
    CHECK(stamp_of(d).tile_z == 38);
    sim::LocalShip w = ship(1, 40.5, 40.5);
    w.dir_x_q24 = -16777216;
    CHECK(stamp_of(w).tile_x == 15);
    // A ship near the origin heading west stamps at a negative point: truncation toward zero
    // (ftol).
    sim::LocalShip neg = ship(1, 3.5, 3.5);
    neg.dir_x_q24 = -16777216;
    CHECK(stamp_of(neg).tile_x == -21);
    sim::LocalShip idle = ship(1, 12.75, 8.25);
    CHECK(stamp_of(idle).tile_x == 12);
    CHECK(stamp_of(idle).tile_z == 8);
}

TEST_CASE("local stamps feed the route query: a ship ahead of the goal moves the end tile like a "
          "hand-built stamp") {
    sim::TileMapSource m(64, 64);
    const sim::CoastIndex index(m, {});
    // The ship is 3 tiles from the start and heads east: its look-ahead stamp lands on the goal
    // tile (40, 20).
    sim::LocalShip s = ship(5, 15.5, 20.5);
    s.dir_x_q24 = 16777216;
    sim::LocalStampQuery q = query_at(12.5, 20.5);
    q.world_w = 64;
    q.world_h = 64;
    const auto stamps = sim::local_ship_stamps(ListView({s}), q, sim::RouteLimits{});
    REQUIRE(stamps.size() == 1);
    CHECK(stamps[0].tile_x == 40);
    CHECK(stamps[0].tile_z == 20);
    sim::RouteQuery rq;
    rq.start_x = 12;
    rq.start_z = 20;
    rq.goal_x_q8 = 40 * 256 + 128;
    rq.goal_z_q8 = 20 * 256 + 128;
    const auto plain =
        sim::plan_route(index, rq, sim::RouteLimits{}, sim::RouteCostTable::original());
    rq.stamps = stamps;
    const auto stamped =
        sim::plan_route(index, rq, sim::RouteLimits{}, sim::RouteCostTable::original());
    CHECK(plain.end_x == 40);
    CHECK((stamped.end_x != 40 || stamped.end_z != 20));
}
