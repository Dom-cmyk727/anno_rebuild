// Short synthetic operation/timing measurement, separate from original sailed canary.
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "freight_helpers.hpp"

namespace sim = anno::sim;
namespace ft = freight_test;
namespace {
struct ScaleInput {
    unsigned routes, stops, figures, buildings;
};
sim::World scale_world(const ScaleInput& in) {
    auto spec = sim::synthetic::empty_scenario(11);
    spec.settings.world_width = 2048;
    spec.settings.world_height = 2048;
    sim::ScenarioSpec::PlayerSpec player;
    player.id = sim::PlayerId{0};
    player.money = 5000;
    spec.players.push_back(player);
    const auto height = 32U + in.routes * 16U;
    auto island = sim::synthetic::plain_island(0, 100, 100, 32, height);
    island.owner_runs = {{0, 32 * static_cast<std::uint32_t>(height)}};
    auto city = sim::synthetic::plain_city(0, 0);
    city.stock = std::vector<std::uint32_t>(100, 0);
    (*city.stock)[42] = 1000000;
    city.capacity = 1000000;
    for (unsigned n = 0; n < in.routes; ++n)
        city.buildings.push_back(
            {.type = sim::TypeId{1302}, .x = 100, .y = 108 + static_cast<int>(n) * 16});
    island.cities.push_back(city);
    spec.islands.push_back(island);
    if (in.buildings) {
        auto extra = sim::synthetic::plain_island(1, 400, 400, 256, 256);
        extra.owner_runs = {{0, 65536}};
        auto town = sim::synthetic::plain_city(0, 0);
        for (unsigned i = 0; i < in.buildings; ++i)
            town.buildings.push_back({.type = sim::TypeId{1302},
                                      .x = 400 + static_cast<int>(i % 64) * 4,
                                      .y = 400 + static_cast<int>(i / 64) * 4});
        extra.cities.push_back(town);
        spec.islands.push_back(extra);
    }
    for (unsigned i = 0; i < in.routes; ++i) {
        sim::ScenarioSpec::FigureSpec f;
        f.kind = ft::ship_kind;
        f.owner = sim::PlayerId{0};
        f.pos = {98 * 256 + 128, (110 + static_cast<int>(i) * 16) * 256 + 128};
        f.holds = std::vector<sim::ShipHold>(4);
        spec.figures.push_back(f);
    }
    for (unsigned i = 0; i < in.figures; ++i) {
        sim::ScenarioSpec::FigureSpec f;
        f.kind = 717;
        f.owner = sim::PlayerId{0};
        f.pos = {(750 + static_cast<int>(i % 128)) * 256 + 128,
                 (750 + static_cast<int>(i / 128)) * 256 + 128};
        spec.figures.push_back(f);
    }
    sim::SimConfig cfg;
    cfg.warehouse_goods = 100;
    cfg.figure_min_growth = 1;
    auto made = sim::World::create(spec, ft::registry(), cfg);
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string{} : made.error()));
    return std::move(made).value();
}
auto operations(const sim::FreightWork& w) {
    return std::tuple{w.visits,           w.steps,         w.hold_scans,
                      w.resolver_queries, w.index_updates, w.target_checks,
                      w.transfers,        w.cold_scans,    w.heap_steps};
}
} // namespace

TEST_CASE("freight scale: short scheduler costs vary routes stops and unrelated content") {
    // Seven bounded samples, not engine caps. Zero-distance legs isolate freight work;
    // unrelated scheduler/navigation costs remain visible in raw wall times and NavWork.
    const std::array<ScaleInput, 7> inputs{{{1, 2, 0, 0},
                                            {8, 2, 0, 0},
                                            {64, 2, 0, 0},
                                            {8, 8, 0, 0},
                                            {8, 128, 0, 0},
                                            {8, 8, 512, 256},
                                            {8, 8, 8192, 2048}}};
    std::ostringstream rows, raw;
    rows << "routes stops figures buildings setup_ns assign_ns warmup_ns measured_ns visits steps "
            "scans resolvers updates target_checks transfers cold_scans heap_steps nav_admission "
            "nav_stamp_cells nav_stamp_entries nav_crew nav_index_builds nav_rects nav_cells\n";
    raw << "routes stops figures buildings advance elapsed_ns\n";
    std::optional<sim::FreightWork> unrelated_baseline;
    const auto ns = [](auto begin) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now() - begin)
            .count();
    };
    for (const auto in : inputs) {
        auto begin = std::chrono::steady_clock::now();
        auto w = scale_world(in);
        const auto setup = ns(begin);
        REQUIRE(w.check_invariants().empty());
        begin = std::chrono::steady_clock::now();
        for (unsigned n = 0; n < in.routes; ++n) {
            sim::AssignFreightRouteCmd route;
            route.ship = sim::FigureId{n + 1};
            for (unsigned i = 0; i < in.stops; ++i)
                route.stops.push_back({sim::make_building_id(sim::IslandId{0}, n + 1),
                                       98 * 256 + 128,
                                       (110 + static_cast<int>(n) * 16) * 256 + 128,
                                       {{sim::FreightDirection::LoadTarget, 42, 1},
                                        {sim::FreightDirection::UnloadAmount, 42, 1}}});
            REQUIRE(ft::apply(w, route).ok());
        }
        const auto assigned = ns(begin);
        begin = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < 100; ++i) {
            w.advance(1000);
            REQUIRE_FALSE(w.faulted());
            (void)w.drain_events();
        }
        const auto warmed = ns(begin);
        w.clear_freight_work();
        sim::WorldTestAccess::route_cache(w).work = {};
        const auto measured_begin = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < 200; ++i) {
            begin = std::chrono::steady_clock::now();
            w.advance(1000);
            const auto elapsed = ns(begin);
            REQUIRE_FALSE(w.faulted());
            (void)w.drain_events();
            raw << in.routes << " " << in.stops << " " << in.figures << " " << in.buildings << " "
                << i << " " << elapsed << "\n";
        }
        const auto measured = ns(measured_begin);
        const auto work = w.freight_work();
        const auto nav = sim::WorldTestAccess::route_cache(w).work;
        REQUIRE(work.visits == in.routes * 200ULL);
        REQUIRE(work.steps <= work.visits * w.rules().freight.work_per_visit);
        REQUIRE(work.hold_scans <= work.steps);
        REQUIRE(work.target_checks <= work.visits + work.steps);
        REQUIRE(work.cold_scans == 0);
        REQUIRE(work.transfers > 0);
        REQUIRE(w.check_invariants().empty());
        for (unsigned n = 0; n < in.routes; ++n) {
            const auto* route = w.freight_route(sim::FigureId{n + 1});
            REQUIRE(route);
            INFO("routes=" << in.routes << " stops=" << in.stops << " unrelated=" << in.figures
                           << "," << in.buildings << " ship=" << n + 1
                           << " reject=" << sim::reject_name(route->last_reject)
                           << " nav=" << static_cast<unsigned>(route->nav_failure) << "\n"
                           << rows.str());
            REQUIRE(route->status == sim::FreightStatus::Running);
            REQUIRE((route->completed_cycles > 0 || route->stop_index > 0));
        }
        if (in.routes == 8 && in.stops == 8) {
            if (!unrelated_baseline)
                unrelated_baseline = work;
            else
                CHECK(operations(work) == operations(*unrelated_baseline));
        }
        rows << in.routes << " " << in.stops << " " << in.figures << " " << in.buildings << " "
             << setup << " " << assigned << " " << warmed << " " << measured << " " << work.visits
             << " " << work.steps << " " << work.hold_scans << " " << work.resolver_queries << " "
             << work.index_updates << " " << work.target_checks << " " << work.transfers << " "
             << work.cold_scans << " " << work.heap_steps << " " << nav.admission_steps << " "
             << nav.stamp_cells << " " << nav.stamp_entries << " " << nav.crew_visits << " "
             << nav.index_builds << " " << nav.index_rect_rebuilds << " " << nav.index_cells_rebuilt
             << "\n";
    }
    if (const char* out = std::getenv("ANNO_FREIGHT_SCALE_OUT_DIR"); out && *out) {
        for (const auto& [name, value] : std::array<std::pair<const char*, std::string>, 2>{
                 {{"summary.tsv", rows.str()}, {"advances.tsv", raw.str()}}}) {
            std::ofstream file(std::filesystem::path(out) / name, std::ios::binary);
            REQUIRE(file.good());
            file << value;
            file.close();
            REQUIRE(file.good());
        }
    }
    MESSAGE(rows.str());
}
