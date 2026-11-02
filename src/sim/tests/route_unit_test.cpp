// Route engine unit tests (MAP-NAV-2 package C): packer and nibble against the emulated original,
// disc and learn tables, limits and cost validation, the coast index and its incremental rebuild,
// separate named geometry cases, stamps, perturbation (wrong tie / order / cost must fail the exact
// comparison while still walking a legal route), the work / memory limits with valid controls, and
// determinism across worker threads.
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/route.hpp>

#include "route_test_support.hpp"

namespace sim = anno::sim;
using namespace route_test;

namespace {

sim::RouteQuery tile_query(std::int32_t sx, std::int32_t sz, std::int32_t gx, std::int32_t gz) {
    sim::RouteQuery q;
    q.start_x = sx;
    q.start_z = sz;
    q.goal_x_q8 = gx * 256 + 128;
    q.goal_z_q8 = gz * 256 + 128;
    return q;
}

sim::RouteResult plan_default(const sim::TileMapSource& map, const sim::RouteQuery& q,
                              const std::vector<sim::CellCoord>& shore = {}) {
    const sim::CoastIndex index(map, shore);
    return sim::plan_route(index, q, sim::RouteLimits{}, sim::RouteCostTable::original());
}

sim::TileMapSource wall_map() {
    sim::TileMapSource m(64, 64);
    m.fill_rect(30, 0, 31, 39, false); // V7: a wall with an open end
    return m;
}

std::uint64_t total_run_steps(const std::vector<sim::RouteRun>& runs) {
    std::uint64_t n = 0;
    for (const auto& r : runs) {
        n += r.steps;
    }
    return n;
}

std::string serialize(const sim::RouteResult& r) {
    std::string s = std::to_string(static_cast<int>(r.status)) + "/" +
                    std::to_string(static_cast<int>(r.exceeded)) + "/" + runs_to_string(r.runs) +
                    "/" + std::to_string(r.end_x) + "," + std::to_string(r.end_z) + "/" +
                    std::to_string(r.score) + "/" + std::to_string(r.best_score) + "/" +
                    std::to_string(r.work.coarse_pops) + "," + std::to_string(r.work.fine_pops) +
                    "," + std::to_string(r.work.learned) + "," + std::to_string(r.work.bytes_high);
    return s;
}

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    std::int32_t below(std::int32_t n) {
        return static_cast<std::int32_t>(next() % static_cast<std::uint32_t>(n));
    }
};

} // namespace

// ---------------------------------------------------------------------------------------------
// packer, nibble, tables
// ---------------------------------------------------------------------------------------------

TEST_CASE("route packer: the documented worked examples") {
    const auto pack = [](const std::vector<std::pair<int, int>>& water) {
        std::array<std::uint8_t, 64> t{};
        for (const auto& [x, z] : water) {
            t[static_cast<std::size_t>(x + 8 * z)] = 1;
        }
        return sim::pack_edge_mask(std::span<const std::uint8_t, 64>(t));
    };
    std::vector<std::pair<int, int>> all, nrow, srow, wcol, ecol, c3, r3;
    for (int z = 0; z < 8; ++z) {
        for (int x = 0; x < 8; ++x) {
            all.push_back({x, z});
        }
    }
    for (int i = 0; i < 8; ++i) {
        nrow.push_back({i, 0});
        srow.push_back({i, 7});
        wcol.push_back({0, i});
        ecol.push_back({7, i});
        c3.push_back({3, i});
        r3.push_back({i, 3});
    }
    CHECK(pack(all) == 0xfff);
    CHECK(pack({}) == 0);
    CHECK(pack(nrow) == 0x0ee);
    CHECK(pack(srow) == 0xb83);
    CHECK(pack(wcol) == 0x770);
    CHECK(pack(ecol) == 0xc1d);
    CHECK(pack(c3) == 0x410);
    CHECK(pack(r3) == 0x082);
    CHECK(pack({{3, 0}}) == 0); // one edge tile: no pair
    std::vector<std::pair<int, int>> n06;
    for (int i = 0; i < 7; ++i) {
        n06.push_back({i, 0});
    }
    CHECK(pack(n06) == 0x060); // touches N and W (corner tile)
    std::vector<std::pair<int, int>> n25;
    for (int i = 2; i < 6; ++i) {
        n25.push_back({i, 0});
    }
    CHECK(pack(n25) == 0);
    // open minus column 3 / row 3 / tile (3,3)
    std::vector<std::pair<int, int>> m;
    for (const auto& p : all) {
        if (p.first != 3) {
            m.push_back(p);
        }
    }
    CHECK(pack(m) == 0xf7d);
    m.clear();
    for (const auto& p : all) {
        if (p.second != 3) {
            m.push_back(p);
        }
    }
    CHECK(pack(m) == 0xbef);
    m.clear();
    for (const auto& p : all) {
        if (p != std::pair<int, int>{3, 3}) {
            m.push_back(p);
        }
    }
    CHECK(pack(m) == 0xfff);
}

TEST_CASE("route packer: every emulated-original vector (random and structured 8x8 maps)") {
    std::uint32_t n = 0;
    for (const std::string& line : fixture_lines("packer_vectors.txt")) {
        const auto parts = split(line, '|');
        const std::string key = words(parts[0])[1];
        REQUIRE(key.size() == 64);
        std::array<std::uint8_t, 64> t{};
        for (std::size_t i = 0; i < 64; ++i) {
            t[i] = key[i] == '.' ? 1 : 0;
        }
        const std::uint32_t want = static_cast<std::uint32_t>(std::stoul(parts[1], nullptr, 16));
        CAPTURE(key);
        CHECK(sim::pack_edge_mask(std::span<const std::uint8_t, 64>(t)) == want);
        ++n;
    }
    CHECK(n >= 300);
}

TEST_CASE("route nibble: every emulated-original vector of the start-cell flood") {
    std::uint32_t n = 0;
    for (const std::string& line : fixture_lines("nibble_vectors.txt")) {
        const auto parts = split(line, '|');
        const std::string key = words(parts[0])[1];
        REQUIRE(key.size() == 64);
        std::array<std::uint8_t, 64> t{};
        for (std::size_t i = 0; i < 64; ++i) {
            t[i] = key[i] == '.' ? 1 : 0;
        }
        const auto at = words(parts[1]);
        const std::uint32_t want = static_cast<std::uint32_t>(std::stoul(parts[2], nullptr, 16));
        CAPTURE(key);
        CHECK(sim::route_detail::start_nibble(std::span<const std::uint8_t, 64>(t),
                                              std::stoi(at[0]), std::stoi(at[1])) == want);
        ++n;
    }
    CHECK(n >= 100);
    // A land start tile floods its water neighbours; a walled-in start has only its own edge bits.
    std::array<std::uint8_t, 64> open{};
    open.fill(1);
    CHECK(sim::route_detail::start_nibble(std::span<const std::uint8_t, 64>(open), 3, 3) == 0xf000);
    std::array<std::uint8_t, 64> land{};
    CHECK(sim::route_detail::start_nibble(std::span<const std::uint8_t, 64>(land), 0, 7) == 0x3000);
}

TEST_CASE("route disc table: the original rasteriser 0x1000d6a0 for radii 0..7") {
    const std::vector<std::vector<std::uint8_t>> want = {{1},
                                                         {1, 1},
                                                         {2, 2, 2},
                                                         {3, 3, 3, 2},
                                                         {4, 4, 3, 3, 1},
                                                         {5, 5, 5, 4, 3, 2},
                                                         {6, 6, 6, 5, 4, 3, 2},
                                                         {7, 7, 7, 6, 6, 5, 4, 2}};
    for (std::int32_t r = 0; r < 8; ++r) {
        CAPTURE(r);
        CHECK(sim::disc_half_widths(r) == want[static_cast<std::size_t>(r)]);
    }
}

TEST_CASE("route learn table: all 16 documented entries of 0x10013fb0") {
    using sim::route_detail::learn_mask;
    // (arrival label of P, arrival label of F) -> AND mask, RF-coarse section 6.
    const struct {
        std::uint32_t cl, al;
        std::uint16_t mask;
    } table[] = {
        {0x80, 0x82, 0xfffe}, {0x80, 0x80, 0xfffd}, {0x80, 0x86, 0xfffb}, {0x82, 0x84, 0xfff7},
        {0x82, 0x82, 0xffef}, {0x82, 0x80, 0xffdf}, {0x84, 0x86, 0xffbf}, {0x84, 0x84, 0xff7f},
        {0x84, 0x82, 0xfeff}, {0x86, 0x80, 0xfdff}, {0x86, 0x86, 0xfbff}, {0x86, 0x84, 0xf7ff},
        {7, 0x80, 0xefff},    {7, 0x82, 0xdfff},    {7, 0x84, 0xbfff},    {7, 0x86, 0x7fff},
    };
    for (const auto& e : table) {
        CAPTURE(e.cl);
        CAPTURE(e.al);
        CHECK(learn_mask(e.cl, e.al) == e.mask);
    }
    CHECK(learn_mask(0x80, 0x84) == 0xffff); // a reversal cannot occur
    CHECK(learn_mask(0x80, 0x81) == 0xffff); // not a label
}

TEST_CASE("route limits and costs: validation names the bad field, defaults are valid") {
    CHECK(sim::validate(sim::RouteLimits{}).empty());
    CHECK(sim::validate(sim::RouteCostTable::original()).empty());
    const auto bad = [](auto mutate) {
        sim::RouteLimits l;
        mutate(l);
        return sim::validate(l);
    };
    CHECK(bad([](sim::RouteLimits& l) { l.window_radius_tiles = 0; }).find("window_radius_tiles") !=
          std::string::npos);
    CHECK(bad([](sim::RouteLimits& l) { l.max_window_blocks = 0; }).find("max_window_blocks") !=
          std::string::npos);
    CHECK(bad([](sim::RouteLimits& l) { l.goal_radius_tiles = 0; }).find("goal_radius_tiles") !=
          std::string::npos);
    CHECK(bad([](sim::RouteLimits& l) { l.goal_radius_tiles = 256; }).find("goal_radius_tiles") !=
          std::string::npos);
    CHECK(bad([](sim::RouteLimits& l) { l.run_cap = 0; }).find("run_cap") != std::string::npos);
    CHECK(bad([](sim::RouteLimits& l) { l.run_cap = 256; }).find("run_cap") != std::string::npos);
    CHECK(bad([](sim::RouteLimits& l) { l.retry_cap = 256; }).find("retry_cap") !=
          std::string::npos);
    CHECK(bad([](sim::RouteLimits& l) { l.unit_cap = 1025; }).find("unit_cap") !=
          std::string::npos);
    CHECK(bad([](sim::RouteLimits& l) { l.coarse_budget = 0; }).find("coarse_budget") !=
          std::string::npos);
    CHECK(bad([](sim::RouteLimits& l) { l.fine_budget = 0; }).find("fine_budget") !=
          std::string::npos);
    CHECK(bad([](sim::RouteLimits& l) { l.memory_bytes = 0; }).find("memory_bytes") !=
          std::string::npos);
    // The valid edges of each range (controls).
    CHECK(bad([](sim::RouteLimits& l) {
              l.retry_cap = 0;
              l.unit_cap = 1024;
              l.run_cap = 255;
              l.goal_radius_tiles = 255;
          }).empty());

    sim::RouteCostTable t;
    CHECK(sim::validate(t).find("at least one") != std::string::npos);
    t = sim::RouteCostTable::original();
    t.classes[1].open_orth = 0;
    CHECK(sim::validate(t).find("class 1") != std::string::npos);
    t = sim::RouteCostTable::original();
    t.classes[2].open_orth = 20; // orth > diag (13)
    CHECK(sim::validate(t).find("class 2") != std::string::npos);
    t = sim::RouteCostTable::original();
    t.classes[0].shore_orth = 70; // > shore_diag 69
    CHECK(sim::validate(t).find("shore_orth") != std::string::npos);

    // plan_route refuses invalid limits, costs and queries without touching anything.
    const sim::TileMapSource m(64, 64);
    const sim::CoastIndex index(m, {});
    sim::RouteLimits l;
    l.run_cap = 0;
    CHECK(sim::plan_route(index, tile_query(5, 5, 20, 5), l, sim::RouteCostTable::original())
              .status == sim::RouteStatus::InvalidQuery);
    CHECK(sim::plan_route(index, tile_query(5, 5, 20, 5), sim::RouteLimits{}, sim::RouteCostTable{})
              .status == sim::RouteStatus::InvalidQuery);
    sim::RouteQuery q = tile_query(5, 5, 20, 5);
    q.nav_class = 9;
    CHECK(sim::plan_route(index, q, sim::RouteLimits{}, sim::RouteCostTable::original()).status ==
          sim::RouteStatus::InvalidQuery);
    CHECK(sim::plan_route(index, tile_query(-1, 5, 20, 5), sim::RouteLimits{},
                          sim::RouteCostTable::original())
              .status == sim::RouteStatus::InvalidQuery);
    CHECK(sim::plan_route(index, tile_query(64, 5, 20, 5), sim::RouteLimits{},
                          sim::RouteCostTable::original())
              .status == sim::RouteStatus::InvalidQuery);
    CHECK(sim::plan_route(sim::CoastIndex{}, tile_query(5, 5, 20, 5), sim::RouteLimits{},
                          sim::RouteCostTable::original())
              .status == sim::RouteStatus::InvalidQuery);
    // Control: the same query is planned with valid inputs.
    CHECK(sim::plan_route(index, tile_query(5, 5, 20, 5), sim::RouteLimits{},
                          sim::RouteCostTable::original())
              .status == sim::RouteStatus::Planned);
}

// ---------------------------------------------------------------------------------------------
// the coast index
// ---------------------------------------------------------------------------------------------

TEST_CASE("route coast index: null sectors are open water and the storage is canonical") {
    sim::TileMapSource m(200, 136);
    sim::CoastIndex index(m, {});
    CHECK(index.allocated_sectors() == 0);
    CHECK(index.cells_x() == 25);
    CHECK(index.cells_z() == 17);
    CHECK(index.mask(3, 3) == 0xfff);
    CHECK(index.type(3, 3) == 0);
    CHECK(index.mask(-1, 0) == 0); // outside the world
    CHECK(index.mask(25, 0) == 0);
    // One inner land tile leaves every edge connected: the cell word stays 0xfff and the sector
    // null.
    m.set_water(70, 70, false);
    index.rebuild_rect({70, 70, 71, 71}, {});
    CHECK(index.allocated_sectors() == 0);
    CHECK(index.mask(8, 8) == 0xfff);
    // A cell of land makes its sector real.
    m.fill_rect(64, 64, 71, 71, false);
    index.rebuild_rect({64, 64, 72, 72}, {});
    CHECK(index.allocated_sectors() == 1);
    CHECK(index.mask(8, 8) == 0);
    CHECK(index.verify_against_clean_rebuild({}).empty());
    m.fill_rect(64, 64, 71, 71, true);
    index.rebuild_rect({64, 64, 72, 72}, {});
    CHECK(index.allocated_sectors() == 0); // canonical again
    CHECK(index.verify_against_clean_rebuild({}).empty());
    // A cell partly outside the world (width 200 = 25 cells, but 196 makes a partial cell).
    sim::TileMapSource p(196, 64);
    const sim::CoastIndex pi(p, {});
    CHECK(pi.cells_x() == 25);
    CHECK(pi.mask(24, 0) != 0xfff); // tiles beyond the world are land
    CHECK(pi.mask(24, 0) != 0);
}

TEST_CASE("route coast index: shore band cells are typed 3 and survive rebuilds of other cells") {
    sim::TileMapSource m(128, 64);
    const std::vector<sim::CellCoord> band = {{2, 2}, {9, 3}, {100, 100}, {-1, 0}};
    const sim::CoastIndex index(m, band);
    CHECK(index.type(2, 2) == 3);
    CHECK(index.type(9, 3) == 3);
    CHECK(index.type(3, 3) == 0);
    CHECK(index.mask(2, 2) == 0xfff);
    CHECK(index.allocated_sectors() == 2); // sectors (0,0) and (1,0) hold band cells
}

TEST_CASE(
    "route coast index: incremental rebuilds equal a clean rebuild, the verifier is not vacuous") {
    sim::TileMapSource m(192, 136);
    Lcg rng{12345};
    std::vector<sim::CellCoord> band;
    for (int i = 0; i < 6; ++i) {
        band.push_back({rng.below(24), rng.below(17)});
    }
    sim::CoastIndex index(m, band);
    for (int round = 0; round < 60; ++round) {
        // Edit a random rectangle (place or remove land), and change the band cells inside it.
        const std::int32_t x0 = rng.below(180);
        const std::int32_t z0 = rng.below(124);
        const std::int32_t x1 = x0 + 1 + rng.below(12);
        const std::int32_t z1 = z0 + 1 + rng.below(12);
        const bool water = (rng.next() & 1u) != 0;
        m.fill_rect(x0, z0, x1 - 1, z1 - 1, water);
        // Band set after the change: drop the cells inside the rectangle, add a random one inside.
        std::vector<sim::CellCoord> next;
        for (const auto& c : band) {
            const bool inside =
                c.cx >= x0 / 8 && c.cx <= (x1 - 1) / 8 && c.cz >= z0 / 8 && c.cz <= (z1 - 1) / 8;
            if (!inside) {
                next.push_back(c);
            }
        }
        if ((rng.next() & 1u) != 0) {
            next.push_back({x0 / 8, z0 / 8});
        }
        band = next;
        index.rebuild_rect({x0, z0, x1, z1}, band);
        CAPTURE(round);
        REQUIRE(index.verify_against_clean_rebuild(band).empty());
        REQUIRE(index.same_words(sim::CoastIndex(m, band)));
    }
    // Negative control: edit the source, rebuild the WRONG rectangle -> the verifier reports a
    // cell.
    m.fill_rect(8, 8, 15, 15, false);
    index.rebuild_rect({100, 100, 101, 101}, band);
    CHECK_FALSE(index.verify_against_clean_rebuild(band).empty());
    index.rebuild_rect({8, 8, 16, 16}, band);
    CHECK(index.verify_against_clean_rebuild(band).empty());
    // A band cell removed from the set but its cell outside the rectangle keeps its type
    // (documented).
    const sim::CellCoord keep = band.empty() ? sim::CellCoord{0, 0} : band[0];
    std::vector<sim::CellCoord> without;
    index.rebuild_rect({190, 134, 191, 135}, without);
    if (!band.empty()) {
        CHECK(index.type(keep.cx, keep.cz) == 3);
    }
}

// ---------------------------------------------------------------------------------------------
// named geometry cases (separate, with hand-derived expectations)
// ---------------------------------------------------------------------------------------------

TEST_CASE("route geometry: a one-tile channel is passable (no hull dilation)") {
    sim::TileMapSource m(64, 64);
    m.fill_rect(30, 0, 31, 19, false);
    m.fill_rect(30, 21, 31, 63, false);
    const auto res = plan_default(m, tile_query(10, 20, 50, 20));
    REQUIRE(res.status == sim::RouteStatus::Planned);
    CHECK(runs_to_string(res.runs) == "E10 E10 E10 E10");
    CHECK(res.end_x == 50);
    CHECK(res.end_z == 20);
    CHECK(walk_runs(m, 10, 20, res.runs, 10).problem.empty());
    // Control: close the channel and the same query fails.
    m.set_water(30, 20, false);
    m.set_water(31, 20, false);
    CHECK(plan_default(m, tile_query(10, 20, 50, 20)).status == sim::RouteStatus::Unreachable);
}

TEST_CASE("route geometry: a closed diagonal gap is unreachable, an open tile reopens it") {
    sim::TileMapSource m(64, 64);
    for (int x = 0; x < 64; ++x) {
        m.set_water(x, 63 - x, false); // the anti-diagonal: land corners touch only diagonally
    }
    const auto closed = plan_default(m, tile_query(10, 10, 50, 50));
    CHECK(closed.status == sim::RouteStatus::Unreachable);
    CHECK(closed.runs.empty());
    m.set_water(30, 33, true); // one land tile of the wall becomes water
    const auto open = plan_default(m, tile_query(10, 10, 50, 50));
    CHECK(open.status == sim::RouteStatus::Planned);
    CHECK(open.end_x == 50);
    CHECK(walk_runs(m, 10, 10, open.runs, 10).problem.empty());
}

TEST_CASE("route geometry: an unreachable goal (walled-in water, goal 7 tiles from any reachable "
          "water)") {
    sim::TileMapSource m(64, 64);
    m.fill_rect(0, 0, 63, 31, false); // the north half is land
    // Goal 8 tiles inland: no candidate within the radius.
    CHECK(plan_default(m, tile_query(30, 50, 30, 24)).status == sim::RouteStatus::Unreachable);
    // Goal 3 tiles inland: routes to the coast tile (30, 32), score 2304 - 64 * 4^2 ... d = 3.5 ->
    // 1520.
    const auto near = plan_default(m, tile_query(30, 50, 30, 29));
    REQUIRE(near.status == sim::RouteStatus::Planned);
    CHECK(near.end_x == 30);
    CHECK(near.end_z == 32);
    CHECK(near.score == 2304 - (3 * 3) * 64 + 0); // d = 3.0 exactly: 32.5 - 29.5
}

TEST_CASE("route geometry: a voyage inside an enclosed lake succeeds") {
    sim::TileMapSource m(64, 64);
    m.fill_rect(20, 20, 40, 22, false);
    m.fill_rect(20, 23, 22, 40, false);
    m.fill_rect(38, 23, 40, 40, false);
    m.fill_rect(20, 41, 40, 43, false);
    const auto res = plan_default(m, tile_query(26, 30, 34, 30));
    REQUIRE(res.status == sim::RouteStatus::Planned);
    CHECK(runs_to_string(res.runs) == "E8");
    CHECK(res.end_x == 34);
    CHECK(res.score == 2304);
}

TEST_CASE("route geometry: from the sea into the lake, and from the lake to the sea, fail") {
    sim::TileMapSource m(64, 64);
    m.fill_rect(20, 20, 40, 22, false);
    m.fill_rect(20, 23, 22, 40, false);
    m.fill_rect(38, 23, 40, 40, false);
    m.fill_rect(20, 41, 40, 43, false);
    const auto in = plan_default(m, tile_query(10, 30, 30, 30));
    CHECK(in.status == sim::RouteStatus::Unreachable);
    CHECK(in.runs.empty());
    const auto out = plan_default(m, tile_query(30, 30, 10, 30));
    CHECK(out.status == sim::RouteStatus::Unreachable);
    CHECK(out.runs.empty());
}

TEST_CASE("route geometry: a route crosses a 64-tile sector boundary and a wall on the boundary") {
    sim::TileMapSource m(192, 128);
    const auto open = plan_default(m, tile_query(60, 20, 70, 20));
    REQUIRE(open.status == sim::RouteStatus::Planned);
    CHECK(runs_to_string(open.runs) == "E10");
    // A wall exactly on the boundary column x = 64 with a gap at z = 40: the route threads the gap.
    m.fill_rect(64, 0, 64, 39, false);
    m.fill_rect(64, 41, 64, 127, false);
    const auto gap = plan_default(m, tile_query(60, 20, 70, 20));
    REQUIRE(gap.status == sim::RouteStatus::Planned);
    CHECK(gap.end_x == 70);
    CHECK(gap.end_z == 20);
    const Walk w = walk_runs(m, 60, 20, gap.runs, 10);
    CHECK(w.problem.empty());
    CHECK(w.end_x == 70);
    // A wall that is closed from z = 0 to 127: unreachable.
    m.set_water(64, 40, false);
    CHECK(plan_default(m, tile_query(60, 20, 70, 20)).status == sim::RouteStatus::Unreachable);
}

TEST_CASE("route stamps: only the first unit_cap stamps count, distant stamps change nothing") {
    sim::TileMapSource m(64, 64);
    const sim::RouteQuery base = tile_query(4, 20, 40, 20);
    const auto none = plan_default(m, base);
    REQUIRE(none.status == sim::RouteStatus::Planned);
    CHECK(none.end_x == 40);
    // 64 stamps far from the goal, then 6 more exactly at it.
    sim::RouteQuery q = base;
    for (int i = 0; i < 64; ++i) {
        q.stamps.push_back({2 + (i % 8), 50 + i / 8, sim::StampKernel::K5});
    }
    const auto distant = plan_default(m, q);
    CHECK(serialize(distant) == serialize(none));
    for (int i = 0; i < 6; ++i) {
        q.stamps.push_back({40, 20, sim::StampKernel::K5});
    }
    const auto capped = plan_default(m, q);
    CHECK(serialize(capped) == serialize(none)); // stamps 65..70 are not used
    // Control: with room for them the same 70 stamps move the end tile.
    sim::RouteLimits l;
    l.unit_cap = 70;
    const sim::CoastIndex index(m, {});
    const auto wide = sim::plan_route(index, q, l, sim::RouteCostTable::original());
    CHECK((wide.end_x != none.end_x || wide.end_z != none.end_z));
    // unit_cap = 0 ignores every stamp.
    l.unit_cap = 0;
    CHECK(serialize(sim::plan_route(index, q, l, sim::RouteCostTable::original())) ==
          serialize(none));
}

TEST_CASE("route density: kernel placement, truncated stamp tile, record-order combine") {
    const std::vector<sim::RouteStamp> one = {{40, 20, sim::StampKernel::K5}};
    CHECK(sim::route_density(one, 40, 20) == 3);
    CHECK(sim::route_density(one, 39, 20) == 2);
    CHECK(sim::route_density(one, 38, 20) == 1);
    CHECK(sim::route_density(one, 37, 20) == 0);
    CHECK(sim::route_density(one, 40, 18) == 1);
    CHECK(sim::route_density(one, 41, 19) == 1);
    const std::vector<sim::RouteStamp> k3 = {{40, 20, sim::StampKernel::K3}};
    CHECK(sim::route_density(k3, 40, 20) == 2);
    CHECK(sim::route_density(k3, 41, 21) == 1);
    CHECK(sim::route_density(k3, 42, 20) == 0);
    // Case E of the notes: stamps at (40,20) and (38,20); tile (39,20) is kernel 2 then kernel 2
    // -> 3.
    const std::vector<sim::RouteStamp> two = {{40, 20, sim::StampKernel::K5},
                                              {38, 20, sim::StampKernel::K5}};
    CHECK(sim::route_density(two, 39, 20) == 3);
    CHECK(sim::route_density(two, 37, 19) == 1);
}

// ---------------------------------------------------------------------------------------------
// perturbation: a wrong tie / order / cost still walks a legal route but is not the original's
// ---------------------------------------------------------------------------------------------

TEST_CASE("route perturbation: every wrong order, tie, cost or score rule walks a legal route but "
          "fails the exact "
          "comparison") {
    struct Variant {
        std::string name;
        sim::route_detail::Perturbation p;
    };
    std::vector<Variant> variants;
    const auto add = [&](const char* name, auto mutate) {
        Variant v{name, {}};
        mutate(v.p);
        variants.push_back(v);
    };
    add("reverse fine order", [](auto& p) { p.reverse_fine_order = true; });
    add("lifo fine ties", [](auto& p) { p.lifo_fine_ties = true; });
    add("fine diagonal cost 20", [](auto& p) { p.fine_diag_cost = 20; });
    add("lifo coarse ties", [](auto& p) { p.lifo_coarse_ties = true; });
    add("fine NW <-> NE swapped", [](auto& p) { p.swap_fine_nw_ne = true; });
    add("fine SW <-> SE swapped", [](auto& p) { p.swap_fine_sw_se = true; });
    add("fine W <-> E swapped", [](auto& p) { p.swap_fine_w_e = true; });
    add("coarse straight step first", [](auto& p) { p.coarse_straight_first = true; });
    add("coarse perpendicular turns swapped", [](auto& p) { p.coarse_swap_perp = true; });
    add("coarse start order W,E,N,S", [](auto& p) { p.coarse_start_order_alt = true; });
    add("score rounds instead of truncating", [](auto& p) { p.round_score = true; });

    // The vectors: route_runs, the generated emulated routes (without shore cells), the mirror-tie
    // scenarios and the arbitrary-goal score scenarios; each carries the original's runs and, where
    // the fixture has them, the end tile and the engine's best score.
    struct Vec {
        sim::TileMapSource map;
        sim::RouteQuery q;
        std::string runs;
        std::int32_t status;
        std::int32_t end_x = -1, end_z = -1, best = -1;
        sim::RouteCostTable costs = sim::RouteCostTable::original();
    };
    std::vector<Vec> vecs;
    for (const char* file : {"route_runs.txt", "route_emulated.txt", "route_ties.txt",
                             "route_ties_cost.txt", "route_scores.txt"}) {
        const std::string f = file;
        // route_runs: name|dims|land|start|goal|status|runs ; route_emulated / route_ties: ... band
        // at 3 ; route_scores: name|dims|land|start|goal|status|runs|end|best
        const std::size_t off =
            (f == "route_emulated.txt" || f == "route_ties.txt" || f == "route_ties_cost.txt") ? 1
                                                                                               : 0;
        for (const std::string& line : fixture_lines(file)) {
            const auto parts = split(line, '|');
            const auto dims = words(parts[1]);
            Vec v{sim::TileMapSource(std::stoi(dims[0]), std::stoi(dims[1])),
                  {},
                  parts[6 + off],
                  std::stoi(parts[5 + off])};
            if (f == "route_ties_cost.txt") {
                v.costs.classes[0].open_orth = 3;
                v.costs.classes[0].open_diag = 6;
            }
            add_land(v.map, words(parts[2]));
            const auto st = words(parts[3 + off]);
            const auto go = words(parts[4 + off]);
            v.q.start_x = decimal_floor(st[1]);
            v.q.start_z = decimal_floor(st[2]);
            v.q.goal_x_q8 = decimal_to_q8(go[1]);
            v.q.goal_z_q8 = decimal_to_q8(go[2]);
            if (f == "route_scores.txt") {
                const auto e = words(parts[7]);
                if (e[1] != "-") {
                    v.end_x = std::stoi(e[1]);
                    v.end_z = std::stoi(e[2]);
                }
                v.best = std::stoi(words(parts[8])[1]);
            }
            if (off == 1 && words(parts[3]).size() > 1 && words(parts[3])[1] != "-") {
                continue; // band cells need the shore input; the plain vectors are enough here
            }
            vecs.push_back(std::move(v));
        }
    }
    CHECK(vecs.size() >= 300);
    for (const Variant& v : variants) {
        CAPTURE(v.name);
        std::uint32_t differing = 0, legal_differing = 0;
        for (const Vec& c : vecs) {
            const sim::CoastIndex index(c.map, {});
            const auto exact = sim::plan_route(index, c.q, sim::RouteLimits{}, c.costs);
            const auto wrong = sim::route_detail::plan_route_perturbed(
                index, c.q, sim::RouteLimits{}, c.costs, v.p);
            REQUIRE(runs_to_string(exact.runs) ==
                    c.runs); // the unperturbed engine is the original's
            if (c.end_x >= 0) {
                REQUIRE(exact.end_x == c.end_x);
                REQUIRE(exact.end_z == c.end_z);
            }
            if (c.best >= 0) {
                REQUIRE(exact.best_score == c.best);
            }
            const bool same = wrong.status == exact.status && wrong.end_x == exact.end_x &&
                              wrong.end_z == exact.end_z && wrong.best_score == exact.best_score &&
                              runs_to_string(wrong.runs) == c.runs;
            if (!same && wrong.status == exact.status) {
                ++differing;
                const Walk w = walk_runs(c.map, c.q.start_x, c.q.start_z, wrong.runs, 10);
                if (w.problem.empty() && w.end_x == wrong.end_x && w.end_z == wrong.end_z) {
                    ++legal_differing;
                }
            }
        }
        MESSAGE(v.name << ": " << differing << " of " << vecs.size() << " vectors differ, "
                       << legal_differing << " of them are legal routes");
        CHECK(differing >= 1);
        CHECK(legal_differing >= 1); // a legal route that only the exact comparison rejects
    }
}

TEST_CASE("route perturbation: corner cutting is caught by the legality walk") {
    // Squeeze: land (20,19) and (19,20) touch diagonally; start between them.
    sim::TileMapSource sq(64, 64);
    sq.set_water(20, 19, false);
    sq.set_water(19, 20, false);
    const sim::CoastIndex index(sq, {});
    sim::route_detail::Perturbation p;
    p.allow_corner_cut = true;
    const sim::RouteQuery q = tile_query(19, 19, 20, 20);
    const auto good =
        sim::plan_route(index, q, sim::RouteLimits{}, sim::RouteCostTable::original());
    REQUIRE(good.status == sim::RouteStatus::Planned);
    CHECK(walk_runs(sq, 19, 19, good.runs, 10).problem.empty());
    CHECK(runs_to_string(good.runs) == "N1 E2 S2 W1");
    const auto cut = sim::route_detail::plan_route_perturbed(index, q, sim::RouteLimits{},
                                                             sim::RouteCostTable::original(), p);
    CHECK(runs_to_string(cut.runs) == "SE1");
    CHECK(walk_runs(sq, 19, 19, cut.runs, 10).problem == "diagonal step cuts a corner");
}

// ---------------------------------------------------------------------------------------------
// limits: each work / memory / window limit with a valid control
// ---------------------------------------------------------------------------------------------

TEST_CASE("route limits: coarse pops, fine pops and memory each fail before any partial result") {
    const sim::TileMapSource m = wall_map();
    const sim::CoastIndex index(m, {});
    const sim::RouteQuery q = tile_query(10, 20, 50, 20);
    const auto costs = sim::RouteCostTable::original();
    const auto control = sim::plan_route(index, q, sim::RouteLimits{}, costs);
    REQUIRE(control.status == sim::RouteStatus::Planned);
    REQUIRE(control.work.coarse_pops > 4);
    REQUIRE(control.work.fine_pops > 4);
    REQUIRE(control.work.bytes_high > 0);

    sim::RouteLimits l;
    l.coarse_budget = control.work.coarse_pops; // exactly enough: the valid control
    auto r = sim::plan_route(index, q, l, costs);
    CHECK(serialize(r) == serialize(control));
    l.coarse_budget = control.work.coarse_pops - 1;
    r = sim::plan_route(index, q, l, costs);
    CHECK(r.status == sim::RouteStatus::BudgetExceeded);
    CHECK(r.exceeded == sim::RouteLimit::CoarsePops);
    CHECK(r.runs.empty());

    l = sim::RouteLimits{};
    l.fine_budget = control.work.fine_pops;
    r = sim::plan_route(index, q, l, costs);
    CHECK(serialize(r) == serialize(control));
    l.fine_budget = control.work.fine_pops - 1;
    r = sim::plan_route(index, q, l, costs);
    CHECK(r.status == sim::RouteStatus::BudgetExceeded);
    CHECK(r.exceeded == sim::RouteLimit::FinePops);
    CHECK(r.runs.empty());

    l = sim::RouteLimits{};
    l.memory_bytes = control.work.bytes_high;
    r = sim::plan_route(index, q, l, costs);
    CHECK(serialize(r) == serialize(control));
    l.memory_bytes = control.work.bytes_high - 1;
    r = sim::plan_route(index, q, l, costs);
    CHECK(r.status == sim::RouteStatus::BudgetExceeded);
    CHECK(r.exceeded == sim::RouteLimit::Memory);
    CHECK(r.runs.empty());
    // A budget failure keeps the work counters: the pops that happened are reported.
    CHECK(r.work.coarse_pops > 0);
}

TEST_CASE(
    "route limits: the learned-mask retries count toward the budgets, retry_cap bounds them") {
    // The V21 learning case: a bay open to the east whose back wall hides the goal.
    sim::TileMapSource m(64, 64);
    m.fill_rect(10, 20, 50, 23, false);
    m.fill_rect(10, 40, 50, 43, false);
    m.fill_rect(10, 20, 13, 43, false);
    const sim::CoastIndex index(m, {});
    const sim::RouteQuery q = tile_query(58, 32, 9, 32);
    const auto costs = sim::RouteCostTable::original();
    const auto control = sim::plan_route(index, q, sim::RouteLimits{}, costs);
    REQUIRE(control.status == sim::RouteStatus::Planned);
    CHECK(runs_to_string(control.runs) == "SW7 S5 W10 W10 W10 W10 W2 N10 N2");
    CHECK(control.work.learned == 2);
    sim::RouteLimits l;
    l.retry_cap = 1;
    auto r = sim::plan_route(index, q, l, costs);
    CHECK(r.status == sim::RouteStatus::Unreachable); // the cap hit: status -2
    CHECK(r.work.learned == 1);
    l.retry_cap = 2;
    CHECK(serialize(sim::plan_route(index, q, l, costs)) == serialize(control));
    l.retry_cap = 0;
    r = sim::plan_route(index, q, l, costs);
    CHECK(r.status == sim::RouteStatus::Unreachable);
    CHECK(r.work.learned == 0);
    // Pops accumulate over the retries: a budget of one search's worth is not enough.
    l = sim::RouteLimits{};
    l.coarse_budget = control.work.coarse_pops - 1;
    CHECK(sim::plan_route(index, q, l, costs).exceeded == sim::RouteLimit::CoarsePops);
}

TEST_CASE("route limits: the window block limit and the start-centred window") {
    // 2100 x 64 tiles: a start in the middle makes the 1024-radius box 2049 wide = 33 blocks.
    sim::TileMapSource wide(2100, 64);
    const sim::CoastIndex index(wide, {});
    const sim::RouteQuery q = tile_query(1050, 32, 1100, 32);
    const auto costs = sim::RouteCostTable::original();
    auto r = sim::plan_route(index, q, sim::RouteLimits{}, costs);
    CHECK(r.status == sim::RouteStatus::BudgetExceeded);
    CHECK(r.exceeded == sim::RouteLimit::Window);
    CHECK(r.runs.empty());
    sim::RouteLimits l;
    l.max_window_blocks = 33; // control: the same query with room for the box
    r = sim::plan_route(index, q, l, costs);
    CHECK(r.status == sim::RouteStatus::Planned);
    CHECK(runs_to_string(r.runs) == "E10 E10 E10 E10 E10");
    // A smaller radius also fits, and a goal beyond it is OutsideWindow (separate from the budget).
    l = sim::RouteLimits{};
    l.window_radius_tiles = 100;
    CHECK(sim::plan_route(index, q, l, costs).status == sim::RouteStatus::Planned);
    const sim::RouteQuery far = tile_query(1050, 32, 1500, 32);
    r = sim::plan_route(index, far, l, costs);
    CHECK(r.status == sim::RouteStatus::OutsideWindow);
    CHECK(r.runs.empty());
    // Control: the goal inside the radius is planned.
    CHECK(sim::plan_route(index, tile_query(1050, 32, 1140, 32), l, costs).status ==
          sim::RouteStatus::Planned);
}

// ---------------------------------------------------------------------------------------------
// determinism and reentrancy
// ---------------------------------------------------------------------------------------------

namespace {

struct Scenario {
    sim::TileMapSource map{192, 128};
    std::vector<sim::CellCoord> band;
    std::vector<sim::RouteQuery> queries;
    Scenario() {
        map.fill_rect(60, 0, 61, 90, false);
        map.fill_rect(100, 40, 101, 127, false);
        map.fill_rect(130, 20, 160, 22, false);
        map.fill_rect(130, 23, 132, 60, false);
        map.fill_rect(158, 23, 160, 60, false);
        map.fill_rect(130, 61, 160, 63, false);
        band = {{3, 3}, {4, 3}, {20, 10}};
        Lcg rng{777};
        for (int i = 0; i < 48; ++i) {
            sim::RouteQuery q;
            q.start_x = rng.below(192);
            q.start_z = rng.below(128);
            q.goal_x_q8 = rng.below(192 * 256);
            q.goal_z_q8 = rng.below(128 * 256);
            for (int k = 0; k < rng.below(8); ++k) {
                q.stamps.push_back(
                    {rng.below(192), rng.below(128),
                     (rng.next() & 1u) != 0 ? sim::StampKernel::K5 : sim::StampKernel::K3});
            }
            queries.push_back(q);
        }
    }
};

} // namespace

TEST_CASE(
    "route determinism: the same queries on 1, 2, 4 and 7 threads give byte-identical results") {
    const Scenario base;
    std::vector<std::string> serial;
    {
        const sim::CoastIndex index(base.map, base.band);
        for (const auto& q : base.queries) {
            serial.push_back(serialize(
                sim::plan_route(index, q, sim::RouteLimits{}, sim::RouteCostTable::original())));
        }
    }
    std::uint32_t planned = 0;
    for (const auto& s : serial) {
        planned += s.rfind("0/", 0) == 0 ? 1 : 0;
    }
    CHECK(planned >= 5); // the scenario is not all failures
    for (const unsigned threads : {1u, 2u, 4u, 7u}) {
        CAPTURE(threads);
        std::vector<std::string> got(base.queries.size());
        std::vector<std::thread> pool;
        for (unsigned t = 0; t < threads; ++t) {
            pool.emplace_back([&, t] {
                // Separate immutable inputs per worker: its own source and index, built here.
                const Scenario mine;
                const sim::CoastIndex index(mine.map, mine.band);
                for (std::size_t i = t; i < mine.queries.size(); i += threads) {
                    got[i] = serialize(sim::plan_route(index, mine.queries[i], sim::RouteLimits{},
                                                       sim::RouteCostTable::original()));
                }
            });
        }
        for (auto& th : pool) {
            th.join();
        }
        CHECK(got == serial);
    }
    // And on ONE shared immutable index (reentrancy).
    const sim::CoastIndex shared(base.map, base.band);
    std::vector<std::string> got(base.queries.size());
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < 4; ++t) {
        pool.emplace_back([&, t] {
            for (std::size_t i = t; i < base.queries.size(); i += 4) {
                got[i] = serialize(sim::plan_route(shared, base.queries[i], sim::RouteLimits{},
                                                   sim::RouteCostTable::original()));
            }
        });
    }
    for (auto& th : pool) {
        th.join();
    }
    CHECK(got == serial);
}

TEST_CASE("route memory accounting: logical bytes, independent of the query repeat, growing with "
          "the window") {
    const sim::TileMapSource small(64, 64);
    const sim::TileMapSource large(256, 256);
    const sim::CoastIndex si(small, {});
    const sim::CoastIndex li(large, {});
    const auto costs = sim::RouteCostTable::original();
    const auto a = sim::plan_route(si, tile_query(4, 20, 40, 20), sim::RouteLimits{}, costs);
    const auto b = sim::plan_route(si, tile_query(4, 20, 40, 20), sim::RouteLimits{}, costs);
    const auto c = sim::plan_route(li, tile_query(4, 20, 40, 20), sim::RouteLimits{}, costs);
    CHECK(a.work.bytes_high == b.work.bytes_high);
    CHECK(a.work.bytes_high >= 64 * 6); // 64 cells * (label + flag + ordinal) at least
    CHECK(c.work.bytes_high > a.work.bytes_high);
    // The result runs are counted: the plan with runs needs more than the same query as Arrived.
    const auto arrived = sim::plan_route(si, tile_query(4, 20, 4, 20), sim::RouteLimits{}, costs);
    CHECK(arrived.status == sim::RouteStatus::Arrived);
    CHECK(arrived.runs.empty());
    CHECK(arrived.work.bytes_high < a.work.bytes_high + 1);
}

TEST_CASE("route limits: the defaults carry a margin of at least 8x over the largest "
          "original-sized window") {
    // The original window is at most 32 x 32 blocks of 64 tiles = 2048 x 2048 tiles = 256 x 256
    // cells. Open sea corner to corner (a long legal route) and an unreachable goal (the flood of
    // every cell).
    sim::TileMapSource sea(2048, 2048);
    const sim::CoastIndex open_index(sea, {});
    const sim::RouteLimits defaults;
    const auto costs = sim::RouteCostTable::original();
    // A start in the middle sees the whole map; one at the corner only [0, 1025) (radius 1024).
    const auto far =
        sim::plan_route(open_index, tile_query(1024, 1024, 2040, 2040), defaults, costs);
    REQUIRE(far.status == sim::RouteStatus::Planned);
    CHECK(total_run_steps(far.runs) == 1016);
    const auto mid = sim::plan_route(open_index, tile_query(1024, 1024, 24, 1030), defaults, costs);
    REQUIRE(mid.status == sim::RouteStatus::Planned);
    CHECK(sim::plan_route(open_index, tile_query(0, 0, 2047, 2047), defaults, costs).status ==
          sim::RouteStatus::OutsideWindow);
    // A closed goal ring in the far corner: the coarse search floods the window and finds nothing.
    sim::TileMapSource walled(2048, 2048);
    walled.fill_rect(1990, 1990, 2047, 1990, false);
    walled.fill_rect(1990, 2047, 2047, 2047, false);
    walled.fill_rect(1990, 1990, 1990, 2047, false);
    walled.fill_rect(2047, 1990, 2047, 2047, false);
    walled.fill_rect(1991, 1991, 2046, 2046,
                     false); // solid island: the goal is 14+ tiles from water
    const sim::CoastIndex walled_index(walled, {});
    const auto flood =
        sim::plan_route(walled_index, tile_query(1024, 1024, 2020, 2020), defaults, costs);
    CHECK(flood.status == sim::RouteStatus::Unreachable);
    MESSAGE("diagonal 1016 tiles: " << far.work.coarse_pops << " coarse, " << far.work.fine_pops
                                    << " fine pops, " << far.work.bytes_high << " bytes");
    MESSAGE("1000 tiles west: " << mid.work.coarse_pops << " coarse, " << mid.work.fine_pops
                                << " fine pops, " << mid.work.bytes_high << " bytes");
    MESSAGE("unreachable goal (full flood): " << flood.work.coarse_pops << " coarse pops, "
                                              << flood.work.bytes_high << " bytes");
    for (const auto* r : {&far, &mid, &flood}) {
        CHECK(defaults.coarse_budget >= 8 * r->work.coarse_pops);
        CHECK(defaults.fine_budget >= 8 * r->work.fine_pops);
        CHECK(defaults.memory_bytes >= 8 * r->work.bytes_high);
    }
    // The structural bound: nine searches (the learned-mask cap plus one) of every cell of the
    // window.
    CHECK(defaults.coarse_budget >= 9u * 256u * 256u);
    // The small representative queries of the fixtures are orders of magnitude below the defaults.
    const auto small = plan_default(wall_map(), tile_query(10, 20, 50, 20));
    CHECK(small.work.coarse_pops * 1000 < defaults.coarse_budget);
}

TEST_CASE(
    "route limits: work of the representative small queries (the fixtures V1, V7, V21, V22)") {
    const sim::RouteLimits defaults;
    std::uint64_t max_coarse = 0, max_fine = 0, max_bytes = 0;
    for (const std::string& line : fixture_lines("route_runs.txt")) {
        const auto parts = split(line, '|');
        const std::string name = words(parts[0])[1];
        const auto dims = words(parts[1]);
        sim::TileMapSource map(std::stoi(dims[0]), std::stoi(dims[1]));
        add_land(map, words(parts[2]));
        const auto st = words(parts[3]);
        const auto go = words(parts[4]);
        sim::RouteQuery q;
        q.start_x = decimal_floor(st[1]);
        q.start_z = decimal_floor(st[2]);
        q.goal_x_q8 = decimal_to_q8(go[1]);
        q.goal_z_q8 = decimal_to_q8(go[2]);
        const sim::CoastIndex index(map, {});
        const auto r = sim::plan_route(index, q, defaults, sim::RouteCostTable::original());
        max_coarse = std::max(max_coarse, r.work.coarse_pops);
        max_fine = std::max(max_fine, r.work.fine_pops);
        max_bytes = std::max(max_bytes, r.work.bytes_high);
        if (name == "V1" || name == "V7" || name == "V21" || name == "V22") {
            MESSAGE(name << ": " << r.work.coarse_pops << " coarse pops, " << r.work.fine_pops
                         << " fine pops, " << r.work.learned << " learned, " << r.work.bytes_high
                         << " bytes");
        }
    }
    MESSAGE("largest of the 24 route-run vectors: " << max_coarse << " coarse, " << max_fine
                                                    << " fine, " << max_bytes << " bytes");
    CHECK(defaults.coarse_budget >= 8 * max_coarse);
    CHECK(defaults.fine_budget >= 8 * max_fine);
    CHECK(defaults.memory_bytes >= 8 * max_bytes);
}

// ---------------------------------------------------------------------------------------------
// the coarse builder: pass 1 shortcut + pass 2 shore band against the original under emulation
// ---------------------------------------------------------------------------------------------

namespace {

/// A world of island block records (the builder's inputs), as a TileSource.
class RecordSource final : public sim::TileSource {
  public:
    struct Rec {
        bool valid = false;
        std::uint8_t mn = 0, mx = 0;
    };
    RecordSource(std::int32_t w, std::int32_t h)
        : w_(w), h_(h), recs_(static_cast<std::size_t>((w / 8) * (h / 8))) {}
    std::int32_t width() const override { return w_; }
    std::int32_t height() const override { return h_; }
    Rec& rec(std::int32_t cx, std::int32_t cz) {
        return recs_[static_cast<std::size_t>(cz * (w_ / 8) + cx)];
    }
    const Rec& rec(std::int32_t cx, std::int32_t cz) const {
        return recs_[static_cast<std::size_t>(cz * (w_ / 8) + cx)];
    }
    bool tile_is_water(std::int32_t x, std::int32_t z) const override {
        if (x < 0 || z < 0 || x >= w_ || z >= h_) {
            return false;
        }
        switch (classify_cell(x / 8, z / 8)) {
        case sim::CellClass::AllWater:
            return true;
        case sim::CellClass::AllLand:
            return false;
        case sim::CellClass::Mixed:
            break;
        }
        return ((x + 2 * z + 5 * (x >> 3) + 3 * (z >> 3)) % 7) < 5;
    }
    sim::CellClass classify_cell(std::int32_t cx, std::int32_t cz) const override {
        const Rec& r = rec(cx, cz);
        if (!r.valid || 19 >= 2 * r.mx) {
            return sim::CellClass::AllWater;
        }
        return 19 < 2 * r.mn ? sim::CellClass::AllLand : sim::CellClass::Mixed;
    }
    bool block_range(std::int32_t cx, std::int32_t cz, std::uint8_t& mn,
                     std::uint8_t& mx) const override {
        if (cx < 0 || cz < 0 || cx >= w_ / 8 || cz >= h_ / 8) {
            return false;
        }
        const Rec& r = rec(cx, cz);
        mn = r.mn;
        mx = r.mx;
        return r.valid;
    }

  private:
    std::int32_t w_, h_;
    std::vector<Rec> recs_;
};

RecordSource parse_builder_world(const std::vector<std::string>& parts) {
    const auto dims = words(parts[1]);
    RecordSource src(std::stoi(dims[0]), std::stoi(dims[1]));
    for (const std::string& isl : split(parts[2], ';')) {
        const auto t = words(isl);
        const std::int32_t x0 = std::stoi(t[1]) / 8, z0 = std::stoi(t[2]) / 8;
        const std::int32_t bw = std::stoi(t[3]), bh = std::stoi(t[4]);
        for (std::int32_t b = 0; b < bw * bh; ++b) {
            const std::string& rec = t[5 + static_cast<std::size_t>(b)];
            auto& r = src.rec(x0 + b % bw, z0 + b / bw);
            r.valid = true;
            r.mn = static_cast<std::uint8_t>(std::stoi(rec.substr(0, 2), nullptr, 16));
            r.mx = static_cast<std::uint8_t>(std::stoi(rec.substr(2, 2), nullptr, 16));
        }
    }
    return src;
}

} // namespace

TEST_CASE("route builder: pass 1 shortcut and pass 2 shore band equal the original builder under "
          "emulation") {
    std::uint32_t n = 0, typed = 0, untyped_nonopen = 0;
    for (const std::string& line : fixture_lines("builder_vectors.txt")) {
        const auto parts = split(line, '|');
        const RecordSource src = parse_builder_world(parts);
        const sim::CoastIndex index(src, {});
        std::map<std::pair<int, int>, std::uint32_t> want;
        for (const std::string& tok : words(parts[3])) {
            if (tok == "words" || tok == "-") {
                continue;
            }
            const auto f = split(tok, ',');
            want[{std::stoi(f[0]), std::stoi(f[1])}] =
                static_cast<std::uint32_t>(std::stoul(f[2], nullptr, 16));
        }
        CAPTURE(parts[0]);
        for (std::int32_t cz = 0; cz < index.cells_z(); ++cz) {
            for (std::int32_t cx = 0; cx < index.cells_x(); ++cx) {
                const auto it = want.find({cx, cz});
                const std::uint32_t expect = it == want.end() ? 0xfffu : it->second;
                const std::uint32_t got = index.cell_word(cx, cz);
                if (got != expect) {
                    CAPTURE(cx);
                    CAPTURE(cz);
                    CHECK(got == expect);
                }
                typed += (expect >> 16) == 3 ? 1 : 0;
                untyped_nonopen += (expect >> 16) == 0 && expect != 0xfff ? 1 : 0;
            }
        }
        CHECK(index.verify_against_clean_rebuild({}).empty());
        ++n;
    }
    CHECK(n >= 50);
    CHECK(typed >= 500);           // the band is exercised
    CHECK(untyped_nonopen >= 500); // and so is its absence next to island cells
}

TEST_CASE("route builder: removal and neighbour effects of the tile-derived band, incremental "
          "equals clean") {
    RecordSource src(192, 128);
    Lcg rng{4242};
    const auto randomise = [&](std::int32_t cx, std::int32_t cz) {
        auto& r = src.rec(cx, cz);
        const std::int32_t k = rng.below(5);
        r.valid = true;
        if (k == 0) {
            r.mn = 0;
            r.mx = 5; // water
        } else if (k == 1) {
            r.mn = 20;
            r.mx = 40; // land
        } else if (k == 2) {
            r.mn = 5;
            r.mx = 25; // straddles 32
        } else if (k == 3) {
            r.mn = 3;
            r.mx = 14; // mixed, no straddle
        } else {
            r.valid = false;
        }
    };
    for (std::int32_t cz = 0; cz < 16; ++cz) {
        for (std::int32_t cx = 0; cx < 24; ++cx) {
            randomise(cx, cz);
        }
    }
    sim::CoastIndex index(src, {});
    std::uint32_t band_removed = 0, band_added_by_neighbour = 0;
    for (int round = 0; round < 80; ++round) {
        // Change the record of one cell (covered before and after, so island coverage is
        // unchanged); its 8 neighbours' types may change although their records and tiles did not.
        const std::int32_t cx = rng.below(24), cz = rng.below(16);
        if (!src.rec(cx, cz).valid) {
            continue;
        }
        const sim::CoastIndex before = index;
        auto& r = src.rec(cx, cz);
        const bool was = r.mn == 5 && r.mx == 25;
        if (was) {
            r.mn = 0;
            r.mx = 5; // a shore record removed
        } else {
            r.mn = 5;
            r.mx = 25; // a shore record added
        }
        index.rebuild_rect({cx * 8, cz * 8, cx * 8 + 8, cz * 8 + 8}, {});
        CAPTURE(round);
        REQUIRE(index.verify_against_clean_rebuild({}).empty());
        REQUIRE(index.same_words(sim::CoastIndex(src, {})));
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                if ((dx != 0 || dz != 0) &&
                    before.type(cx + dx, cz + dz) != index.type(cx + dx, cz + dz)) {
                    (was ? band_removed : band_added_by_neighbour) += 1;
                }
            }
        }
    }
    CHECK(band_removed > 0);
    CHECK(band_added_by_neighbour > 0);
    // Negative control: without the halo (rebuilding only the cell's own rectangle shrunk to its
    // interior is not possible), a stale neighbour is detected by the verifier once a record
    // changes under it.
    auto& r = src.rec(5, 5);
    r.valid = true;
    r.mn = 5;
    r.mx = 25;
    CHECK_FALSE(index.verify_against_clean_rebuild({}).empty());
}

TEST_CASE("route limits: memory is checked before every allocation of the path, the corridor and "
          "the heaps") {
    // The largest window (256 x 256 cells), a 1016-tile diagonal: the working set is far above the
    // window arrays (6 bytes per cell), so a limit between the two fails INSIDE the search, not at
    // the start.
    sim::TileMapSource sea(2048, 2048);
    const sim::CoastIndex index(sea, {});
    const sim::RouteQuery q = tile_query(1024, 1024, 2040, 2040);
    const auto costs = sim::RouteCostTable::original();
    const auto control = sim::plan_route(index, q, sim::RouteLimits{}, costs);
    REQUIRE(control.status == sim::RouteStatus::Planned);
    const std::uint64_t high = control.work.bytes_high;
    const std::uint64_t arrays = 256ull * 256ull * 6ull;
    REQUIRE(high > arrays + 1000);
    std::uint32_t failed = 0, passed = 0;
    for (std::uint64_t limit = arrays + 1; limit <= high + 64; limit += (high - arrays) / 37 + 1) {
        sim::RouteLimits l;
        l.memory_bytes = limit;
        const auto r = sim::plan_route(index, q, l, costs);
        CAPTURE(limit);
        if (limit >= high) {
            CHECK(serialize(r) == serialize(control)); // valid control: enough room, same result
            ++passed;
        } else {
            CHECK(r.status == sim::RouteStatus::BudgetExceeded);
            CHECK(r.exceeded == sim::RouteLimit::Memory);
            CHECK(r.runs.empty());
            // The accounted high-water mark never passes the limit by more than the one allocation
            // that was refused (a fine block with its ordinal, 68 bytes, is the largest).
            CHECK(r.work.bytes_high <= limit + 68);
            ++failed;
        }
    }
    CHECK(failed >= 20);
    CHECK(passed >= 1);
    // The exact boundary: high passes, high - 1 fails.
    sim::RouteLimits edge;
    edge.memory_bytes = high;
    CHECK(sim::plan_route(index, q, edge, costs).status == sim::RouteStatus::Planned);
    edge.memory_bytes = high - 1;
    CHECK(sim::plan_route(index, q, edge, costs).exceeded == sim::RouteLimit::Memory);
    // A window of more than 32 blocks (a configured max_window_blocks) cannot overshoot either: the
    // window arrays are accounted before they are allocated.
    sim::TileMapSource wide(2600, 64);
    const sim::CoastIndex wi(wide, {});
    sim::RouteLimits big;
    big.max_window_blocks = 64;
    big.window_radius_tiles = 4000;
    big.memory_bytes = 1000; // below the window arrays
    const auto w = sim::plan_route(wi, tile_query(1300, 32, 1400, 32), big, costs);
    CHECK(w.status == sim::RouteStatus::BudgetExceeded);
    CHECK(w.exceeded == sim::RouteLimit::Memory);
    big.memory_bytes = 64ull * 1024 * 1024;
    CHECK(sim::plan_route(wi, tile_query(1300, 32, 1400, 32), big, costs).status ==
          sim::RouteStatus::Planned);
}

TEST_CASE("route limits: unit_cap is bounded so the density work of a query is bounded") {
    sim::RouteLimits l;
    l.unit_cap = 1024;
    CHECK(sim::validate(l).empty());
    l.unit_cap = 1025;
    CHECK_FALSE(sim::validate(l).empty());
    // The worst legal case still plans: 1024 stamps on top of the goal.
    sim::TileMapSource m(64, 64);
    const sim::CoastIndex index(m, {});
    sim::RouteQuery q = tile_query(4, 20, 40, 20);
    for (int i = 0; i < 1500; ++i) {
        q.stamps.push_back({40 + i % 3, 20 + i % 2, sim::StampKernel::K5});
    }
    l.unit_cap = 1024;
    const auto r = sim::plan_route(index, q, l, sim::RouteCostTable::original());
    CHECK(r.status == sim::RouteStatus::Planned);
}

TEST_CASE("route partial cells: a world that is not a multiple of 8 treats the tiles beyond its "
          "edge as land") {
    // 70 x 50 tiles: the last cell column holds tiles 64..69 (and 70, 71 outside), the last cell
    // row 48..49.
    sim::TileMapSource edge(70, 50);
    edge.fill_rect(30, 10, 31, 30, false); // a wall to make the route non-trivial
    // The same world padded to 72 x 56 with explicit land beyond the edge: the results must be
    // identical.
    sim::TileMapSource padded(72, 56);
    padded.fill_rect(30, 10, 31, 30, false);
    padded.fill_rect(70, 0, 71, 55, false);
    padded.fill_rect(0, 50, 71, 55, false);
    const sim::CoastIndex ie(edge, {});
    const sim::CoastIndex ip(padded, {});
    CHECK(ie.cells_x() == 9);
    CHECK(ie.cells_z() == 7);
    CHECK(ie.mask(8, 0) != 0xfff); // the partial column is not open
    CHECK(ie.mask(8, 0) == ip.mask(8, 0));
    CHECK(ie.mask(0, 6) == ip.mask(0, 6));
    CHECK(ie.mask(8, 6) == ip.mask(8, 6));
    const auto costs = sim::RouteCostTable::original();
    struct Case {
        int sx, sz, gx, gz;
    };
    // Starts and goals in and beyond the partial cells; goals beyond the world edge (the outside is
    // land).
    const Case cases[] = {{60, 20, 69, 20}, {60, 49, 69, 49}, {10, 49, 65, 49}, {66, 5, 66, 45},
                          {10, 10, 69, 49}, {60, 20, 73, 20}, {20, 40, 20, 54}, {69, 0, 69, 49},
                          {5, 5, 71, 52},   {68, 48, 40, 5}};
    for (const Case& c : cases) {
        CAPTURE(c.sx);
        CAPTURE(c.sz);
        CAPTURE(c.gx);
        CAPTURE(c.gz);
        const auto a =
            sim::plan_route(ie, tile_query(c.sx, c.sz, c.gx, c.gz), sim::RouteLimits{}, costs);
        const auto b =
            sim::plan_route(ip, tile_query(c.sx, c.sz, c.gx, c.gz), sim::RouteLimits{}, costs);
        CHECK(runs_to_string(a.runs) == runs_to_string(b.runs));
        CHECK(a.status == b.status);
        CHECK(a.end_x == b.end_x);
        CHECK(a.end_z == b.end_z);
        if (a.status == sim::RouteStatus::Planned) {
            const Walk w = walk_runs(edge, c.sx, c.sz, a.runs, 10);
            CHECK(w.problem.empty());
            CHECK(w.end_x < 70);
            CHECK(w.end_z < 50);
        }
    }
    // A goal one tile beyond the east edge ends on the last column, d = 1.5; two beyond ends at d
    // = 2.5.
    const auto east = sim::plan_route(ie, tile_query(60, 20, 71, 20), sim::RouteLimits{}, costs);
    REQUIRE(east.status == sim::RouteStatus::Planned);
    CHECK(east.end_x == 69);
    // The start tile itself may lie in the partial column.
    CHECK(sim::plan_route(ie, tile_query(69, 49, 69, 49), sim::RouteLimits{}, costs).status ==
          sim::RouteStatus::Arrived);
}
