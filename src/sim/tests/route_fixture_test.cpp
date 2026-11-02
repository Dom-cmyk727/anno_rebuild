// The emulated-original fixtures (fixtures/synthetic/sim/nav) against the ACTUAL output of the C++
// router: route runs, goal candidates with stamps, passage statuses, shore-band runs. The expected
// values come from the original engine under emulation (see the fixture headers), not from this
// code.
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/route.hpp>

#include "route_test_support.hpp"

namespace sim = anno::sim;
using namespace route_test;

namespace {

struct Planned {
    sim::RouteResult res;
    sim::TileMapSource map;
    std::int32_t sx = 0, sz = 0;
};

sim::RouteResult plan(const sim::TileMapSource& map, std::int32_t sx, std::int32_t sz,
                      std::int32_t gx_q8, std::int32_t gz_q8,
                      const std::vector<sim::RouteStamp>& stamps = {},
                      const std::vector<sim::CellCoord>& shore = {}, std::uint32_t cls = 0,
                      const sim::RouteCostTable& costs = sim::RouteCostTable::original()) {
    const sim::CoastIndex index(map, shore);
    sim::RouteQuery q;
    q.start_x = sx;
    q.start_z = sz;
    q.goal_x_q8 = gx_q8;
    q.goal_z_q8 = gz_q8;
    q.nav_class = cls;
    q.stamps = stamps;
    return sim::plan_route(index, q, sim::RouteLimits{}, costs);
}

std::int32_t status_code(const sim::RouteResult& r) {
    return static_cast<std::int32_t>(r.status);
}

} // namespace

TEST_CASE("route fixtures: every emulated-original route-run vector, exact runs and status") {
    std::uint32_t executed = 0;
    for (const std::string& line : fixture_lines("route_runs.txt")) {
        const auto parts = split(line, '|');
        const std::string name = words(parts[0])[1];
        const auto dims = words(parts[1]);
        sim::TileMapSource map(std::stoi(dims[0]), std::stoi(dims[1]));
        add_land(map, words(parts[2]));
        const auto st = words(parts[3]);
        const auto go = words(parts[4]);
        const std::int32_t expect_status = std::stoi(parts[5]);
        const std::string expect_runs = parts[6];
        const auto res = plan(map, decimal_floor(st[1]), decimal_floor(st[2]), decimal_to_q8(go[1]),
                              decimal_to_q8(go[2]));
        CAPTURE(name);
        CHECK(status_code(res) == expect_status);
        CHECK(runs_to_string(res.runs) == expect_runs);
        if (parts.size() > 8) {
            const auto end = words(parts[8]);
            CHECK(res.end_x == std::stoi(end[1]));
            CHECK(res.end_z == std::stoi(end[2]));
        }
        // The legality walk is a SEPARATE check from the exact comparison above.
        if (res.status == sim::RouteStatus::Planned) {
            const Walk w = walk_runs(map, decimal_floor(st[1]), decimal_floor(st[2]), res.runs, 10);
            CHECK(w.problem.empty());
            CHECK(w.end_x == res.end_x);
            CHECK(w.end_z == res.end_z);
        }
        ++executed;
    }
    CHECK(executed >= 24);
}

TEST_CASE("route fixtures: stamped goal candidates (offer cases) end tile, best score and runs") {
    std::uint32_t executed = 0;
    for (const std::string& line : fixture_lines("offer_cases.txt")) {
        const auto parts = split(line, '|');
        const std::string name = words(parts[0])[1];
        const auto st = words(parts[1]);
        const auto go = words(parts[2]);
        std::vector<sim::RouteStamp> stamps;
        const auto sw = words(parts[3]);
        for (std::size_t i = 1; i < sw.size(); ++i) {
            if (sw[i] == "-") {
                continue;
            }
            const auto f = split(sw[i], ',');
            stamps.push_back({static_cast<std::int32_t>(std::stod(f[0])),
                              static_cast<std::int32_t>(std::stod(f[1])),
                              f[2] == "K5" ? sim::StampKernel::K5 : sim::StampKernel::K3});
        }
        const auto end = words(parts[5]);
        const auto best = words(parts[6]);
        sim::TileMapSource map(64, 64);
        const auto res = plan(map, decimal_floor(st[1]), decimal_floor(st[2]), decimal_to_q8(go[1]),
                              decimal_to_q8(go[2]), stamps);
        CAPTURE(name);
        CHECK(res.end_x == std::stoi(end[1]));
        CHECK(res.end_z == std::stoi(end[2]));
        if (best[1] != "-") {
            CHECK(res.best_score == std::stoi(best[1]));
        }
        CHECK(runs_to_string(res.runs) == parts[7]);
        ++executed;
    }
    CHECK(executed >= 21);
}

TEST_CASE("route fixtures: passage catalogue statuses, goal tiles and scores") {
    std::uint32_t executed = 0;
    for (const std::string& line : fixture_lines("passage_cases.txt")) {
        const auto parts = split(line, '|');
        const std::string name = words(parts[0])[1];
        const auto dims = words(parts[1]);
        sim::TileMapSource map(std::stoi(dims[0]), std::stoi(dims[1]));
        add_land(map, words(parts[2]));
        const auto st = words(parts[3]);
        const auto go = words(parts[4]);
        const auto want = words(parts[6]);
        const auto res = plan(map, decimal_floor(st[1]), decimal_floor(st[2]), decimal_to_q8(go[1]),
                              decimal_to_q8(go[2]));
        CAPTURE(name);
        CHECK(status_code(res) == std::stoi(want[0]));
        if (want[1] != "-") {
            CHECK(res.end_x == std::stoi(want[1]));
            CHECK(res.end_z == std::stoi(want[2]));
            // A goal that is not an exact Q24.8 value (26.6) cannot reproduce the f32 emulation's
            // score bit for bit; the Q24.8 score is still within one unit of the f32 one.
            const bool exact = q8_exact(go[1]) && q8_exact(go[2]);
            if (exact) {
                CHECK(res.score == std::stoi(want[3]));
            } else {
                CHECK(std::abs(res.score - std::stoi(want[3])) <= 2);
            }
        }
        ++executed;
    }
    CHECK(executed >= 16);
}

TEST_CASE("route fixtures: shore band runs with and without band cells (explicit shore input)") {
    std::uint32_t executed = 0;
    for (const std::string& line : fixture_lines("object_shore_cases.txt")) {
        if (line.rfind("shore", 0) != 0) {
            continue;
        }
        const auto parts = split(line, '|');
        const std::string name = words(parts[0])[1];
        sim::TileMapSource map(64, 64);
        add_land(map, words(parts[1]));
        std::vector<sim::CellCoord> band;
        const auto bw = words(parts[2]);
        for (std::size_t i = 1; i < bw.size(); ++i) {
            const auto c = split(bw[i], ',');
            band.push_back({std::stoi(c[0]), std::stoi(c[1])});
        }
        const auto st = words(parts[3]);
        const auto go = words(parts[4]);
        const std::uint32_t cls = static_cast<std::uint32_t>(std::stoi(words(parts[5])[1]));
        const auto nb = words(parts[6]);
        const auto bd = words(parts[7]);
        const auto end = words(parts[8]);
        const std::int32_t sx = decimal_floor(st[1]);
        const std::int32_t sz = decimal_floor(st[2]);
        const auto without =
            plan(map, sx, sz, decimal_to_q8(go[1]), decimal_to_q8(go[2]), {}, {}, cls);
        const auto with =
            plan(map, sx, sz, decimal_to_q8(go[1]), decimal_to_q8(go[2]), {}, band, cls);
        CAPTURE(name);
        CHECK(status_code(without) == std::stoi(nb[1]));
        CHECK(status_code(with) == std::stoi(bd[1]));
        std::string want_nb, want_bd;
        for (std::size_t i = 2; i < nb.size(); ++i) {
            want_nb += (i > 2 ? " " : "") + nb[i];
        }
        for (std::size_t i = 2; i < bd.size(); ++i) {
            want_bd += (i > 2 ? " " : "") + bd[i];
        }
        CHECK(runs_to_string(without.runs) == want_nb);
        CHECK(runs_to_string(with.runs) == want_bd);
        CHECK(with.end_x == std::stoi(end[1]));
        CHECK(with.end_z == std::stoi(end[2]));
        ++executed;
    }
    CHECK(executed >= 7);
}

TEST_CASE("route fixtures: the walk-legality helper rejects the corrupt records and accepts the "
          "controls") {
    // The helper that the cases above use for their separate legality check is itself checked here
    // against route_counterexamples.txt (the reference tool's reject list).
    std::uint32_t rejected = 0, accepted = 0;
    for (const std::string& line : fixture_lines("route_counterexamples.txt")) {
        const auto parts = split(line, '|');
        const auto dims = words(parts[1]);
        sim::TileMapSource map(std::stoi(dims[0]), std::stoi(dims[1]));
        add_land(map, words(parts[2]));
        const auto st = words(parts[3]);
        const auto runs = parse_runs(words(parts[6]));
        const Walk w = walk_runs(map, decimal_floor(st[1]), decimal_floor(st[2]), runs, 10);
        const std::string expect = parts[9].substr(std::string("expect ").size());
        if (expect == "accept") {
            CHECK(w.problem.empty());
            ++accepted;
        } else {
            CHECK_FALSE(w.problem.empty());
            ++rejected;
        }
    }
    CHECK(accepted >= 1);
    CHECK(rejected >= 3);
}

static void
check_emulated_routes(const std::vector<std::string>& lines, std::uint32_t min_cases,
                      const sim::RouteCostTable& costs = sim::RouteCostTable::original()) {
    std::uint32_t executed = 0, learning = 0, planned = 0, failed = 0, with_band = 0, big = 0;
    for (const std::string& line : lines) {
        const auto parts = split(line, '|');
        const std::string name = words(parts[0])[1];
        const auto dims = words(parts[1]);
        sim::TileMapSource map(std::stoi(dims[0]), std::stoi(dims[1]));
        add_land(map, words(parts[2]));
        std::vector<sim::CellCoord> band;
        const auto bw = words(parts[3]);
        for (std::size_t i = 1; i < bw.size(); ++i) {
            if (bw[i] == "-") {
                continue;
            }
            const auto c = split(bw[i], ',');
            band.push_back({std::stoi(c[0]), std::stoi(c[1])});
        }
        const auto st = words(parts[4]);
        const auto go = words(parts[5]);
        const auto res = plan(map, decimal_floor(st[1]), decimal_floor(st[2]), decimal_to_q8(go[1]),
                              decimal_to_q8(go[2]), {}, band, 0, costs);
        CAPTURE(name);
        CHECK(status_code(res) == std::stoi(parts[6]));
        CHECK(runs_to_string(res.runs) == parts[7]);
        // The original resets its learned-mask count to 0 when the cap is hit (the -1 path of
        // 0x10013fb0); the port reports the masks it applied.
        const std::uint32_t want_learned =
            static_cast<std::uint32_t>(std::stoi(words(parts[8])[1]));
        if (res.work.retry_cap_hit) {
            CHECK(want_learned == 0);
        } else {
            CHECK(res.work.learned == want_learned);
        }
        if (res.status == sim::RouteStatus::Planned) {
            const Walk w = walk_runs(map, decimal_floor(st[1]), decimal_floor(st[2]), res.runs, 10);
            CHECK(w.problem.empty());
            CHECK(w.end_x == res.end_x);
            CHECK(w.end_z == res.end_z);
            ++planned;
        } else if (res.status == sim::RouteStatus::Unreachable) {
            ++failed;
        }
        learning += res.work.learned > 0 ? 1 : 0;
        with_band += band.empty() ? 0U : 1U;
        big += std::stoi(dims[0]) > 64 ? 1U : 0U;
        ++executed;
    }
    CHECK(executed >= min_cases);
    MESSAGE("emulated routes: " << executed << " cases, " << planned << " planned, " << failed
                                << " unreachable, " << learning << " with learned masks, "
                                << with_band << " with shore cells, " << big << " beyond 64 x 64");
}

TEST_CASE("route fixtures: generated emulated-original routes (mixed cells, learning, block edges, "
          "shore band)") {
    check_emulated_routes(fixture_lines("route_emulated.txt"), 150);
}

TEST_CASE("route fixtures: optional extra campaign file (ANNO_ROUTE_EXTRA, same format, skipped "
          "when unset)") {
    const char* extra = std::getenv("ANNO_ROUTE_EXTRA");
    if (extra == nullptr || *extra == '\0') {
        return;
    }
    std::ifstream f(extra);
    REQUIRE(f.good());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (!line.empty() && line[0] != '#') {
            lines.push_back(line);
        }
    }
    check_emulated_routes(lines, 1);
}

TEST_CASE("route fixtures: arbitrary Q24.8 goals pin the score truncation, the end tile and the "
          "engine's best") {
    std::uint32_t n = 0, inexact_scores = 0;
    for (const std::string& line : fixture_lines("route_scores.txt")) {
        const auto parts = split(line, '|');
        const auto dims = words(parts[1]);
        sim::TileMapSource map(std::stoi(dims[0]), std::stoi(dims[1]));
        add_land(map, words(parts[2]));
        const auto st = words(parts[3]);
        const auto go = words(parts[4]);
        const auto end = words(parts[7]);
        const auto res = plan(map, decimal_floor(st[1]), decimal_floor(st[2]), decimal_to_q8(go[1]),
                              decimal_to_q8(go[2]));
        CAPTURE(parts[0]);
        CHECK(q8_exact(go[1]));
        CHECK(status_code(res) == std::stoi(parts[5]));
        CHECK(runs_to_string(res.runs) == parts[6]);
        if (end[1] != "-") {
            CHECK(res.end_x == std::stoi(end[1]));
            CHECK(res.end_z == std::stoi(end[2]));
            // 64 d^2 is not a multiple of 1024 for these goals: the score truncates a fraction.
            const std::int64_t dx = (2 * std::int64_t{res.end_x} + 1) * 128 - decimal_to_q8(go[1]);
            const std::int64_t dz = (2 * std::int64_t{res.end_z} + 1) * 128 - decimal_to_q8(go[2]);
            inexact_scores += ((dx * dx + dz * dz) % 1024 != 0) ? 1 : 0;
        }
        CHECK(res.best_score == std::stoi(words(parts[8])[1]));
        ++n;
    }
    CHECK(n >= 100);
    CHECK(inexact_scores >= 80);
}

TEST_CASE(
    "route fixtures: mirror ties with a patched coarse cost table (turn cost == straight cost)") {
    sim::RouteCostTable costs = sim::RouteCostTable::original();
    costs.classes[0].open_orth = 3;
    costs.classes[0].open_diag = 6;
    check_emulated_routes(fixture_lines("route_ties_cost.txt"), 90, costs);
}

TEST_CASE("route fixtures: mirror ties with the default costs (discovery order and FIFO decide)") {
    check_emulated_routes(fixture_lines("route_ties.txt"), 140);
}
