// MAP-NAV-3 package N5.2: bounded production scale measurements (docs/sim/ship-navigation.md
// 12.15). OPT-IN: runs only with ANNO_SIM_NAV_SCALE=<output directory>; without it the case skips
// with a message (an opt-in skip, like the transport statistics case). It builds real worlds
// through World::create (the production registry, scheduler, ShipMover, RouteCache), sails ships
// with MoveCmd -> advance, and measures:
//   (a) unrelated valid figures F (parked scouts far away, some in parent trees),
//   (b) island count I (small islands in a grid, unrelated to the ships),
//   (c) local density: ships within stamp range of the planner and pending orders.
// The wall time (std::chrono) is taken in this test only and is NEVER asserted. The deterministic
// locality facts (counters) are asserted, so the harness doubles as a check.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/route.hpp>
#include <anno/sim/route_cache.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "evidence_out.hpp"
#include "ship_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using ship_test::kScout;
using ship_test::kShip;
namespace fs = std::filesystem;

namespace {

constexpr std::int32_t kTile = 256;
constexpr std::uint32_t kSeed = 11;
constexpr std::uint32_t kAdvanceMs = 100; // one Advance = advance(100), every figure visited once
constexpr int kAdvances = 1000;
constexpr std::int32_t kIslandSize = 16;

std::int32_t tile_c(int t) {
    return t * kTile + kTile / 2;
}

struct Config {
    std::string name;
    std::uint32_t figures = 1000; // unrelated valid scouts F
    std::uint32_t islands = 2;    // I
    std::uint32_t near = 6;       // ships within stamp range of the planner (ship 1)
    std::uint32_t orders = 6;     // ships holding a pending MoveCmd
};

std::uint32_t ship_count(const Config& c) {
    return std::max(std::max(c.near, c.orders), 6U);
}

/// The ship positions: the first `near` on the 1-tile points inside radius 5 of (120, 20), ordered
/// by distance (ship 1 at the centre is the planner); the others on a far grid (10-tile spacing,
/// x 10..220, z 60..210), unrelated to the cluster.
std::vector<std::pair<int, int>> ship_positions(const Config& c) {
    std::vector<std::pair<int, int>> pts;
    for (int dz = -5; dz <= 5; ++dz) {
        for (int dx = -5; dx <= 5; ++dx) {
            if (dx * dx + dz * dz <= 25) {
                pts.emplace_back(dx, dz);
            }
        }
    }
    std::stable_sort(pts.begin(), pts.end(), [](auto a, auto b) {
        return a.first * a.first + a.second * a.second < b.first * b.first + b.second * b.second;
    });
    std::vector<std::pair<int, int>> out;
    const std::uint32_t n = ship_count(c);
    for (std::uint32_t i = 0; i < n; ++i) {
        if (i < c.near) {
            out.emplace_back(120 + pts[i].first, 20 + pts[i].second);
        } else {
            const int k = static_cast<int>(i - c.near);
            out.emplace_back(10 + (k % 21) * 10, 60 + (k / 21) * 10);
        }
    }
    return out;
}

sim::ScenarioSpec::FigureSpec fig(std::uint32_t kind, int tx, int tz) {
    sim::ScenarioSpec::FigureSpec f;
    f.kind = kind;
    f.owner = sim::PlayerId{0};
    f.pos = sim::FixedPos{tile_c(tx), tile_c(tz)};
    f.dir16 = 8;
    return f;
}

sim::ScenarioSpec make_spec(const Config& c) {
    sim::ScenarioSpec spec = syn::empty_scenario(kSeed);
    sim::ScenarioSpec::PlayerSpec ps;
    ps.id = sim::PlayerId{0};
    ps.money = 1000;
    spec.players.push_back(ps);
    // Islands: 16 x 16 land, 8 x 8 grid of 32-tile pitch from (240, 240): outside every ship and
    // scout position, so they are unrelated to the sailing work.
    for (std::uint32_t i = 0; i < c.islands; ++i) {
        const std::int32_t x0 = 240 + static_cast<std::int32_t>(i % 8) * 32;
        const std::int32_t z0 = 240 + static_cast<std::int32_t>(i / 8) * 32;
        sim::ScenarioSpec::IslandSpec is = syn::plain_island(i, x0, z0, kIslandSize, kIslandSize);
        is.owner_runs = {{0, static_cast<std::uint32_t>(kIslandSize * kIslandSize)}};
        is.cities.push_back(syn::plain_city(0, 0));
        is.terrain = sim::make_terrain(
            std::vector<std::uint8_t>(std::size_t{kIslandSize} * kIslandSize, std::uint8_t{30}), {},
            {});
        spec.islands.push_back(std::move(is));
    }
    const auto pos = ship_positions(c);
    for (const auto& [x, z] : pos) {
        spec.figures.push_back(fig(kShip, x, z));
    }
    const auto n = static_cast<std::uint32_t>(pos.size());
    // A constant crew of 3 aboard ship 1 (a chain: scout -> scout -> scout), independent of F.
    for (std::uint32_t k = 0; k < 3; ++k) {
        sim::ScenarioSpec::FigureSpec f = fig(kScout, pos[0].first, pos[0].second);
        f.parent = k == 0 ? 0U : n + k - 1;
        spec.figures.push_back(f);
    }
    // F unrelated scouts parked far away (x 300..499, z 5..244 stays north of the islands at
    // z >= 240 only for z < 240): rows of 200; every 8th starts a 3-deep parent tree.
    const std::uint32_t base = n + 3;
    for (std::uint32_t i = 0; i < c.figures; ++i) {
        const int x = 300 + static_cast<int>(i % 200);
        const int z = 5 + static_cast<int>((i / 200) % 230);
        sim::ScenarioSpec::FigureSpec f = fig(kScout, x, z);
        const std::uint32_t phase = i % 8;
        if (phase == 1 || phase == 2 || phase == 3) {
            f.parent = base + i - 1; // chain under the group's first scout
        }
        spec.figures.push_back(f);
    }
    return spec;
}

sim::World make_world(const sim::ScenarioSpec& spec) {
    auto w = sim::World::create(spec, ship_test::nav_ship_registry(), {}, {}, {},
                                sim::serial_executor());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

struct Sample {
    std::uint64_t ns = 0;
    sim::NavWork d; // counter deltas of this advance
    std::uint64_t tickets = 0;
    std::uint32_t unrouted = 0; // ordered ships whose order is not yet routed (before the advance)
};

std::uint64_t pct(std::vector<std::uint64_t> v, double p) {
    if (v.empty()) {
        return 0;
    }
    std::sort(v.begin(), v.end());
    const std::size_t i = static_cast<std::size_t>(p * static_cast<double>(v.size() - 1) + 0.5);
    return v[std::min(i, v.size() - 1)];
}

sim::NavWork delta(const sim::NavWork& a, const sim::NavWork& b) {
    sim::NavWork d;
    d.admission_steps = a.admission_steps - b.admission_steps;
    d.stamp_cells = a.stamp_cells - b.stamp_cells;
    d.stamp_entries = a.stamp_entries - b.stamp_entries;
    d.crew_visits = a.crew_visits - b.crew_visits;
    d.index_builds = a.index_builds - b.index_builds;
    d.index_rect_rebuilds = a.index_rect_rebuilds - b.index_rect_rebuilds;
    d.index_cells_rebuilt = a.index_cells_rebuilt - b.index_cells_rebuilt;
    return d;
}

struct Result {
    Config cfg;
    bool nav = false;
    std::vector<Sample> samples;
    sim::NavWork total;
    std::uint64_t create_ns = 0, cold_build_ns = 0, hot_sync_ns = 0;
    std::uint64_t index_bytes = 0, allocated_sectors = 0, tickets_peak = 0, ship_cells = 0;
    std::uint64_t figures_total = 0;
    std::vector<std::uint64_t> delays; // advances from MoveCmd to routed (or order gone)
    std::uint32_t unserved = 0;
    std::uint32_t served_after_first = 0;
};

std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

Result run(const Config& c, bool nav) {
    Result r;
    r.cfg = c;
    r.nav = nav;
    const sim::ScenarioSpec spec = make_spec(c);
    const std::uint64_t t0 = now_ns();
    sim::World w = make_world(spec);
    r.create_ns = now_ns() - t0;
    r.figures_total = spec.figures.size();
    REQUIRE(w.check_invariants().empty());

    // Cold index build vs hot sync, on a copy (a copy has an EMPTY cache): the first
    // RouteCache::sync builds, a repeated one only compares the per-island fingerprints.
    {
        sim::World cold = w;
        sim::RouteCache& rc = sim::WorldTestAccess::route_cache(cold);
        REQUIRE_FALSE(rc.built());
        const std::uint64_t a = now_ns();
        rc.sync(cold);
        r.cold_build_ns = now_ns() - a;
        REQUIRE(rc.built());
        r.allocated_sectors = rc.get(cold).allocated_sectors();
        r.index_bytes = r.allocated_sectors * 64 * sizeof(std::uint32_t);
        constexpr int kRep = 1000;
        const std::uint64_t b = now_ns();
        for (int i = 0; i < kRep; ++i) {
            rc.sync(cold);
        }
        r.hot_sync_ns = (now_ns() - b) / kRep;
    }

    std::vector<std::uint64_t> ordered;
    if (nav) {
        for (std::uint32_t i = 0; i < c.orders; ++i) {
            const auto [x, z] = ship_positions(c)[i];
            REQUIRE(w.apply(sim::Command{sim::PlayerId{0},
                                         sim::MoveCmd{sim::FigureId{i + 1ULL},
                                                      tile_c(std::min(x + 30, 250)), tile_c(z)}})
                        .ok());
            ordered.push_back(i + 1ULL);
        }
    }
    std::vector<int> served(ordered.size(), -1);
    sim::NavWork prev = sim::WorldTestAccess::route_cache(w).work;
    for (int a = 0; a < kAdvances; ++a) {
        Sample s;
        for (std::size_t k = 0; k < ordered.size(); ++k) {
            const sim::Figure* f = sim::find_figure(w.figures(), sim::FigureId{ordered[k]});
            if (f != nullptr && f->nav->order.has_value() && !f->nav->order->routed) {
                ++s.unrouted;
            }
        }
        const std::uint64_t t1 = now_ns();
        w.advance(kAdvanceMs);
        s.ns = now_ns() - t1;
        const sim::NavWork cur = sim::WorldTestAccess::route_cache(w).work;
        s.d = delta(cur, prev);
        prev = cur;
        s.tickets = w.figures().tickets.size();
        r.tickets_peak = std::max(r.tickets_peak, s.tickets);
        for (std::size_t k = 0; k < ordered.size(); ++k) {
            if (served[k] < 0) {
                const sim::Figure* f = sim::find_figure(w.figures(), sim::FigureId{ordered[k]});
                if (f != nullptr && (!f->nav->order.has_value() || f->nav->order->routed)) {
                    served[k] = a + 1;
                }
            }
        }
        r.samples.push_back(s);
    }
    r.total = sim::WorldTestAccess::route_cache(w).work;
    r.ship_cells = w.figures().ship_cells.size();
    for (const int d : served) {
        if (d < 0) {
            ++r.unserved;
        } else {
            r.delays.push_back(static_cast<std::uint64_t>(d));
        }
    }
    REQUIRE(w.check_invariants().empty());
    return r;
}

void check_locality(const Result& r) {
    const sim::NavRules rules; // the default Rules.nav of the world
    const std::uint64_t quota = rules.plans_per_advance;
    const std::uint64_t ships = ship_count(r.cfg);
    for (const Sample& s : r.samples) {
        // Each Advance admits at most `quota` plans; a plan makes ONE stamp query over at most
        // 3 x 3 cells of 16 x 16 tiles (the range is a few tiles; 12.12).
        CHECK(s.d.stamp_cells <= 9 * quota);
        // Admission examines at most quota + 1 ticket entries per attempt and every ship attempts
        // at most once per Advance (each figure is visited once at advance(100)).
        CHECK(s.d.admission_steps <= ships * (quota + 1));
        CHECK(s.d.index_rect_rebuilds == 0);
        CHECK(s.d.index_cells_rebuilt == 0);
        if (!r.nav) {
            CHECK(s.d.stamp_cells == 0);
            CHECK(s.d.admission_steps == 0);
        }
    }
    CHECK(r.total.index_builds == 1); // the single cold build, no rebuild afterwards
    if (!r.nav) {
        CHECK(r.total.crew_visits == 0); // nothing moves
    }
}

/// Writes config.txt, samples.csv and summary.csv under `dir`. Returns "" or the first failing
/// path and operation (MAP-BUILD-1 A: a requested file that cannot be written fails the case).
std::string write_outputs(const fs::path& dir, const std::vector<Result>& rs) {
    std::error_code mk;
    fs::create_directories(dir, mk);
    if (mk) {
        return dir.string() + ": create_directories failed";
    }
    std::ostringstream cfg;
    const sim::NavRules rules;
    cfg << "seed=" << kSeed << "\nadvance_ms=" << kAdvanceMs
        << "\nadvances_per_config=" << kAdvances
        << "\nworld=512x512 open sea, 16x16 islands in an 8x8 grid from (240,240) pitch 32\n"
        << "ships=nav_ship_registry type 1502 (speed q32 154618816, stamp radius 1280 q8)\n"
        << "crew=3 scouts aboard ship 1 (constant)\n"
        << "rules=default SimConfig; nav.plans_per_advance=" << rules.plans_per_advance
        << " unit_cap=" << rules.unit_cap << " run_cap=" << rules.run_cap
        << " window_radius_tiles=" << rules.window_radius_tiles << "\n"
        << "orders: MoveCmd to (x+30 clamp 250, z) for the first `orders` ships\n";
    std::ostringstream raw;
    raw << "config,mode,advance,ns,admission_steps,stamp_cells,stamp_entries,crew_visits,"
           "index_builds,index_rect_rebuilds,index_cells_rebuilt,tickets,unrouted_before\n";
    std::ostringstream sum;
    sum << "config,mode,F,I,near,orders,ships,figures_total,advances,p50_us,p95_us,p99_us,max_us,"
           "first_adv_us,p50_after_first_us,admission_steps,stamp_cells,stamp_entries,crew_visits,"
           "index_builds,index_rect_rebuilds,index_cells_rebuilt,plan_advances,"
           "cold_build_us,hot_sync_ns,index_bytes,allocated_sectors,tickets_peak,ship_cell_keys,"
           "delay_max_adv,delay_p50_adv,unserved,create_ms\n";
    for (const Result& r : rs) {
        std::vector<std::uint64_t> all, rest;
        std::uint64_t plan_adv = 0;
        for (std::size_t i = 0; i < r.samples.size(); ++i) {
            const Sample& s = r.samples[i];
            raw << r.cfg.name << ',' << (r.nav ? "nav" : "idle") << ',' << i << ',' << s.ns << ','
                << s.d.admission_steps << ',' << s.d.stamp_cells << ',' << s.d.stamp_entries << ','
                << s.d.crew_visits << ',' << s.d.index_builds << ',' << s.d.index_rect_rebuilds
                << ',' << s.d.index_cells_rebuilt << ',' << s.tickets << ',' << s.unrouted << '\n';
            all.push_back(s.ns);
            if (i > 0) {
                rest.push_back(s.ns);
            }
            if (s.d.stamp_cells > 0) {
                ++plan_adv;
            }
        }
        const auto us = [](std::uint64_t ns) { return static_cast<double>(ns) / 1000.0; };
        sum << r.cfg.name << ',' << (r.nav ? "nav" : "idle") << ',' << r.cfg.figures << ','
            << r.cfg.islands << ',' << r.cfg.near << ',' << r.cfg.orders << ',' << ship_count(r.cfg)
            << ',' << r.figures_total << ',' << r.samples.size() << ',' << us(pct(all, 0.5)) << ','
            << us(pct(all, 0.95)) << ',' << us(pct(all, 0.99)) << ',' << us(pct(all, 1.0)) << ','
            << us(r.samples.front().ns) << ',' << us(pct(rest, 0.5)) << ','
            << r.total.admission_steps << ',' << r.total.stamp_cells << ',' << r.total.stamp_entries
            << ',' << r.total.crew_visits << ',' << r.total.index_builds << ','
            << r.total.index_rect_rebuilds << ',' << r.total.index_cells_rebuilt << ',' << plan_adv
            << ',' << us(r.cold_build_ns) << ',' << r.hot_sync_ns << ',' << r.index_bytes << ','
            << r.allocated_sectors << ',' << r.tickets_peak << ',' << r.ship_cells << ','
            << pct(r.delays, 1.0) << ',' << pct(r.delays, 0.5) << ',' << r.unserved << ','
            << static_cast<double>(r.create_ns) / 1e6 << '\n';
    }
    // A stale file of an earlier run must not survive as if it were this run's output.
    for (const char* name : {"config.txt", "samples.csv", "summary.csv"}) {
        std::error_code rm;
        if (fs::is_regular_file(dir / name, rm)) {
            fs::remove(dir / name, rm);
        }
    }
    const std::pair<const char*, std::string> files[] = {
        {"config.txt", cfg.str()}, {"samples.csv", raw.str()}, {"summary.csv", sum.str()}};
    for (const auto& [name, text] : files) {
        if (std::string err = sim::test::write_evidence_file(dir / name, text); !err.empty()) {
            return err;
        }
    }
    return {};
}

} // namespace

TEST_CASE("nav scale measurements (ANNO_SIM_NAV_SCALE)") {
    const char* out = std::getenv("ANNO_SIM_NAV_SCALE");
    if (out == nullptr || *out == '\0') {
        MESSAGE("skipping: set ANNO_SIM_NAV_SCALE=<output directory> to run the N5.2 measurements");
        return;
    }
    std::vector<Config> configs;
    // (a) x (b): F and I with the fixed local work of 6 ships sailing in a cluster, 6 orders.
    for (const std::uint32_t isl : {2U, 64U}) {
        for (const std::uint32_t f : {20U, 1000U, 10000U}) {
            configs.push_back(
                Config{"F" + std::to_string(f) + "_I" + std::to_string(isl), f, isl, 6, 6});
        }
    }
    // (c) local density at F = 1000, I = 2.
    for (const std::uint32_t near : {2U, 20U, 70U}) {
        for (const std::uint32_t ord : {6U, 60U, 300U}) {
            configs.push_back(Config{"D_near" + std::to_string(near) + "_ord" + std::to_string(ord),
                                     1000, 2, near, ord});
        }
    }
    // The combined worst corner: 300 pending orders with 10 000 figures and 64 islands.
    configs.push_back(Config{"X_F10000_I64_ord300", 10000, 64, 6, 300});
    std::vector<Result> results;
    for (const Config& c : configs) {
        results.push_back(run(c, true));
        check_locality(results.back());
        results.push_back(run(c, false)); // idle baseline: the same world, no orders
        check_locality(results.back());
    }
    // Navigation counters must not depend on F or I (same ships and orders): compare every
    // configuration with the first one of its density class.
    std::map<std::string, const Result*> ref;
    for (const Result& r : results) {
        if (!r.nav) {
            continue;
        }
        const std::string key = std::to_string(r.cfg.near) + "/" + std::to_string(r.cfg.orders);
        if (r.cfg.name.rfind("F", 0) != 0) {
            continue; // the density class has one F and one I only
        }
        const auto [it, fresh] = ref.emplace(key, &r);
        if (!fresh) {
            const sim::NavWork& a = it->second->total;
            const sim::NavWork& b = r.total;
            INFO("config " << r.cfg.name << " against " << it->second->cfg.name);
            CHECK(a.admission_steps == b.admission_steps);
            CHECK(a.stamp_cells == b.stamp_cells);
            CHECK(a.stamp_entries == b.stamp_entries);
            CHECK(a.crew_visits == b.crew_visits);
            CHECK(it->second->delays == r.delays);
        }
    }
    const std::string write_error = write_outputs(fs::path(out), results);
    REQUIRE_MESSAGE(write_error.empty(), write_error);
    MESSAGE("wrote " << results.size() << " runs to " << std::string(out));
}

TEST_CASE("nav scale writer: a requested file that cannot be written is an error") {
    // Cheap controls on the actual writer (not the long measurement): a one-sample result.
    Result r;
    r.cfg.name = "tiny";
    r.samples.resize(1);
    const std::vector<Result> rs = {r};
    const fs::path dir = sim::test::make_private_dir("anno_nav_scale_writer_test");
    REQUIRE_FALSE(dir.empty());
    CHECK(write_outputs(dir, rs).empty()); // valid control
    CHECK(fs::file_size(dir / "summary.csv") > 0);
    CHECK_FALSE(fs::exists(dir / "summary.csv.partial"));
    for (const char* victim : {"config.txt", "samples.csv", "summary.csv"}) {
        fs::remove_all(dir);
        fs::create_directories(dir / victim); // the requested file is a directory
        const std::string err = write_outputs(dir, rs);
        INFO("victim " << victim);
        CHECK(err.find(victim) != std::string::npos);
        CHECK(err.find("directory") != std::string::npos);
    }
    fs::remove_all(dir);
}
