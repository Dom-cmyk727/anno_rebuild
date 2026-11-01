// Route engine unit tests (MAP-NAV-2 package C): packer and nibble against the emulated original,
// disc and learn tables, limits and cost validation, the coast index and its incremental rebuild,
// separate named geometry cases, stamps, perturbation (wrong tie / order / cost must fail the exact
// comparison while still walking a legal route), the work / memory limits with valid controls, and
// determinism across worker threads.
#include <array>
#include <cstdint>
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
    CHECK(bad([](sim::RouteLimits& l) { l.unit_cap = 65536; }).find("unit_cap") !=
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
              l.unit_cap = 0;
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
    "route coast index: incremental rebuilds equal a clean rebuild; the verifier is not vacuous") {
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

TEST_CASE("route perturbation: wrong order, tie or cost reach a legal endpoint but fail the exact "
          "comparison") {
    struct Variant {
        std::string name;
        sim::route_detail::Perturbation p;
    };
    std::vector<Variant> variants(4);
    variants[0] = {"reverse fine order", {}};
    variants[0].p.reverse_fine_order = true;
    variants[1] = {"lifo fine ties", {}};
    variants[1].p.lifo_fine_ties = true;
    variants[2] = {"fine diagonal cost 20", {}};
    variants[2].p.fine_diag_cost = 20;
    variants[3] = {"lifo coarse ties", {}};
    variants[3].p.lifo_coarse_ties = true;

    // Every vector of route_runs.txt and of the generated route_emulated.txt.
    struct Vec {
        sim::TileMapSource map;
        sim::RouteQuery q;
        std::string runs;
        std::int32_t status;
    };
    std::vector<Vec> vecs;
    for (const char* file : {"route_runs.txt", "route_emulated.txt"}) {
        const bool emu = std::string(file) == "route_emulated.txt";
        for (const std::string& line : fixture_lines(file)) {
            const auto parts = split(line, '|');
            const auto dims = words(parts[1]);
            Vec v{sim::TileMapSource(std::stoi(dims[0]), std::stoi(dims[1])),
                  {},
                  parts[emu ? 7 : 6],
                  std::stoi(parts[emu ? 6 : 5])};
            add_land(v.map, words(parts[2]));
            const auto st = words(parts[emu ? 4 : 3]);
            const auto go = words(parts[emu ? 5 : 4]);
            v.q.start_x = decimal_floor(st[1]);
            v.q.start_z = decimal_floor(st[2]);
            v.q.goal_x_q8 = decimal_to_q8(go[1]);
            v.q.goal_z_q8 = decimal_to_q8(go[2]);
            if (emu && words(parts[3]).size() > 1 && words(parts[3])[1] != "-") {
                continue; // band cells need the shore input; the plain vectors are enough here
            }
            vecs.push_back(std::move(v));
        }
    }
    CHECK(vecs.size() >= 140);
    for (const Variant& v : variants) {
        CAPTURE(v.name);
        std::uint32_t differing = 0, legal_differing = 0;
        for (const Vec& c : vecs) {
            const sim::CoastIndex index(c.map, {});
            const auto exact =
                sim::plan_route(index, c.q, sim::RouteLimits{}, sim::RouteCostTable::original());
            const auto wrong = sim::route_detail::plan_route_perturbed(
                index, c.q, sim::RouteLimits{}, sim::RouteCostTable::original(), v.p);
            REQUIRE(runs_to_string(exact.runs) ==
                    c.runs); // the unperturbed engine is the original's
            if (wrong.status == exact.status && wrong.end_x == exact.end_x &&
                wrong.end_z == exact.end_z && runs_to_string(wrong.runs) != c.runs) {
                ++differing;
                const Walk w = walk_runs(c.map, c.q.start_x, c.q.start_z, wrong.runs, 10);
                if (w.problem.empty() && w.end_x == exact.end_x && w.end_z == exact.end_z) {
                    ++legal_differing;
                }
            }
        }
        MESSAGE(v.name << ": " << differing << " of " << vecs.size() << " vectors differ, "
                       << legal_differing << " of them legal with the same endpoint");
        CHECK(differing >= 1);
        CHECK(legal_differing >=
              1); // legal route, same endpoint, different runs: only exact comparison sees it
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
