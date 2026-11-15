// FISHER-1e: unchanged original maps; no projection or gameplay-state injection.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/driver.hpp>
#include <anno/sim/fisher.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_search.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>

namespace {
namespace sim = anno::sim;
// A bounded test workload, fixed before the first run: 120 game seconds, below the
// first u16 tick wrap. No production timing or movement constant is changed.
constexpr std::uint64_t kTripBound = 60000;
constexpr std::uint32_t kTurnMs = 20;
struct Trip {
    sim::BuildingId home;
    sim::IslandId island;
    sim::FigureId worker;
    std::int32_t x = 0, y = 0;
    sim::FixedPos launch;
    std::uint64_t spawn = 0, open_entrance = 0, left = 0, caught = 0, returned = 0, food = 0;
    std::uint32_t cuts = 0, raw = 0, out = 0, cargo = 0;
    std::optional<sim::TileXY> waiting;
};
const sim::Figure* find_worker(const sim::World& w, sim::FigureId id) {
    for (const auto& f : w.figures().list.slots)
        if (f && f->id == id)
            return &*f;
    return nullptr;
}
void observe(sim::World& w, Trip& t) {
    const auto* b = w.find_building(t.home);
    REQUIRE(b);
    const auto* type = w.types().find(b->type);
    REQUIRE(type);
    const auto& farm = std::get<sim::Farm>(b->payload);
    const auto tick = w.clock().tick();
    const auto& island = *w.islands().at(t.island.v);
    if (!t.worker.valid()) {
        for (const auto& f : w.figures().list.slots)
            if (f && f->home == t.home && !f->expiry_stamp) {
                REQUIRE(f->kind == 142);
                REQUIRE(f->job == 45);
                REQUIRE(f->harvest);
                t.worker = f->id;
                t.spawn = tick;
                t.launch = sim::entrance_position(*b, *type, 0);
                REQUIRE(f->pos == t.launch);
                const auto cell = sim::fisher_cell_at(
                    island.terrain->height, island.tiles.width(), island.tiles.height(),
                    static_cast<std::uint32_t>(sim::tile_of_q8(t.launch.x) - island.tiles.x0()),
                    static_cast<std::uint32_t>(sim::tile_of_q8(t.launch.y) - island.tiles.y0()));
                REQUIRE(cell);
                std::cout << "FISHER_SPAWN hut=" << t.x << ',' << t.y << " tick=" << tick
                          << " worker=" << t.worker.v << " mean=" << unsigned(cell->mean) << '\n';
                break;
            }
    }
    const auto* f = find_worker(w, t.worker);
    if (f && f->harvest && !f->expiry_stamp) {
        // The faithful first launch can stand on land. Observe an original open
        // entrance before departure, rather than relocating or requiring a water launch.
        if (!t.open_entrance) {
            for (const auto& entrance : sim::entrance_walk_points(*b, *type)) {
                if (sim::tile_of_q8(f->pos.x) != entrance.tile.x ||
                    sim::tile_of_q8(f->pos.y) != entrance.tile.y)
                    continue;
                const auto cell = sim::fisher_cell_at(
                    island.terrain->height, island.tiles.width(), island.tiles.height(),
                    static_cast<std::uint32_t>(entrance.tile.x - island.tiles.x0()),
                    static_cast<std::uint32_t>(entrance.tile.y - island.tiles.y0()));
                if (cell && cell->walkable_water) {
                    t.open_entrance = tick;
                    std::cout << "FISHER_OPEN_ENTRANCE hut=" << t.x << ',' << t.y
                              << " tick=" << tick << " tile=" << entrance.tile.x << ','
                              << entrance.tile.y << '\n';
                }
            }
        }
        if (!t.left && !sim::tile_in_footprint(*b, *type, sim::tile_of_q8(f->pos.x),
                                               sim::tile_of_q8(f->pos.y))) {
            REQUIRE(t.open_entrance != 0);
            t.left = tick;
            std::cout << "FISHER_LEFT hut=" << t.x << ',' << t.y << " tick=" << tick << '\n';
        }
        if (f->cargo_88 > t.cargo) {
            REQUIRE(t.waiting);
            const auto at = island.tile_index(t.waiting->x, t.waiting->y);
            REQUIRE(at != sim::kNoSlot);
            REQUIRE(island.tiles.object(at) == 10);
            REQUIRE_FALSE(island.tiles.claimed(at));
            REQUIRE(f->cargo_88 == t.cargo + 256);
            REQUIRE(t.left != 0);
            ++t.cuts;
            if (!t.caught)
                t.caught = tick;
            std::cout << "FISHER_CATCH hut=" << t.x << ',' << t.y << " tick=" << tick
                      << " target=" << t.waiting->x << ',' << t.waiting->y
                      << " cargo=" << f->cargo_88 << '\n';
        }
        t.cargo = f->cargo_88;
        t.waiting.reset();
        if (f->harvest->step == sim::HarvestStep::Cut) {
            const auto target = f->harvest->target;
            REQUIRE(sim::tile_of_q8(f->pos.x) == target.x);
            REQUIRE(sim::tile_of_q8(f->pos.y) == target.y);
            const auto at = island.tile_index(target.x, target.y);
            REQUIRE(at != sim::kNoSlot);
            REQUIRE(island.tiles.object(at) == 11);
            REQUIRE(island.tiles.claimed(at));
            t.waiting = target;
        }
    }
    if (!t.returned && farm.raw > t.raw) {
        REQUIRE(t.caught != 0);
        REQUIRE(t.cuts == 3);
        REQUIRE(farm.raw - t.raw == w.types().find_figure_type(142)->capacity_88);
        REQUIRE(f);
        REQUIRE(f->expiry_stamp);
        REQUIRE(sim::tile_in_footprint(*b, *type, sim::tile_of_q8(f->pos.x),
                                       sim::tile_of_q8(f->pos.y)));
        t.returned = tick;
        std::cout << "FISHER_RETURN hut=" << t.x << ',' << t.y << " tick=" << tick
                  << " raw=" << farm.raw << " trip_ticks=" << tick - t.spawn << '\n';
    }
    if (!t.food && farm.out > t.out) {
        REQUIRE(t.returned != 0);
        REQUIRE(farm.out - t.out == type->prod.output_per_step.at(0));
        t.food = tick;
        std::cout << "FISHER_FOOD hut=" << t.x << ',' << t.y << " tick=" << tick
                  << " output=" << farm.out << '\n';
    }
    t.raw = farm.raw;
    t.out = farm.out;
}
sim::World original_world(const std::filesystem::path& game, const std::string& map, bool enabled,
                          bool human) {
    const auto registry =
        anno::sim_load::load_original_registry(game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    const auto templates = anno::sim_load::scan_island_templates(game);
    const auto loaded = anno::sim_load::load_scenario_file(game / "Scenes" / map, game, templates,
                                                           *registry.types, {}, {});
    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
    sim::ExtensionContext extensions;
    extensions.fisher_rules.enabled = enabled ? std::uint8_t{1} : std::uint8_t{0};
    auto made = sim::World::create(loaded.value().loaded.spec, registry.types, {}, {}, {},
                                   sim::serial_executor(), extensions);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    auto w = std::move(made).value();
    auto session = sim::Recorder::start(w);
    REQUIRE_MESSAGE(session, (session ? "" : session.error()));
    if (human) {
        const auto result =
            w.apply({sim::PlayerId{2}, sim::BuildCmd{{sim::IslandId{3}, sim::CityNo{0}},
                                                     {{sim::TypeId{810}, 243, 216, 0, 0, 0}}}});
        REQUIRE(result.ok());
        REQUIRE(result.records.size() == 1);
        REQUIRE(result.records.front().status == sim::Reject::None);
    }
    return w;
}
std::vector<Trip> original_huts(const sim::World& w, bool human) {
    const sim::TypeId expected_type{human ? 810U : 3978U};
    const sim::PlayerId owner{human ? 2U : 5U};
    std::vector<Trip> trips;
    for (const auto& is : w.islands())
        if (is)
            for (const auto& city : is->cities)
                if (city)
                    for (const auto& list : city->index.lists)
                        for (const auto& b : list.slots)
                            if (b && b->type == expected_type) {
                                REQUIRE(city->owner == owner);
                                const auto* type = w.types().find(b->type);
                                REQUIRE(type);
                                REQUIRE(type->prod.input_per_step.at(0) == (human ? 69 : 128));
                                REQUIRE(type->prod.output_per_step.at(0) == (human ? 17 : 51));
                                const auto& farm = std::get<sim::Farm>(b->payload);
                                REQUIRE(farm.raw == 0);
                                REQUIRE(farm.out == 0);
                                REQUIRE(farm.workers_out == 0);
                                Trip trip;
                                trip.home = b->id;
                                trip.island = is->index;
                                trip.x = b->x;
                                trip.y = b->y;
                                trips.push_back(trip);
                            }
    REQUIRE(trips.size() == (human ? std::size_t{1} : std::size_t{3}));
    return trips;
}

void run_canary(sim::World& w, std::vector<Trip>& trips, const std::string& map) {
    while (w.clock().tick() < kTripBound) {
        w.advance(kTurnMs);
        REQUIRE_MESSAGE(!w.faulted(), w.fault());
        for (auto& t : trips) {
            INFO("map=", map, " hut=", t.x, ",", t.y, " tick=", w.clock().tick());
            if (!t.food)
                observe(w, t);
        }
        if (std::all_of(trips.begin(), trips.end(), [](const Trip& t) { return t.food != 0; }))
            break;
        if (w.clock().tick() % 5000 == 0)
            for (const auto& t : trips) {
                const auto& farm = std::get<sim::Farm>(w.find_building(t.home)->payload);
                const auto* f = find_worker(w, t.worker);
                std::cout << "FISHER_PROGRESS hut=" << t.x << ',' << t.y
                          << " tick=" << w.clock().tick() << " workers=" << farm.workers_out
                          << " raw=" << farm.raw << " out=" << farm.out;
                if (f && f->harvest)
                    std::cout << " step=" << unsigned(f->harvest->step) << " pos=" << f->pos.x
                              << ',' << f->pos.y << " cargo=" << f->cargo_88
                              << " wait=" << f->harvest->wait << " next=" << f->harvest->next
                              << " along=" << f->harvest->along;
                std::cout << '\n';
            }
    }
    for (const auto& t : trips) {
        INFO("map=", map, " hut=", t.x, ",", t.y, " tick=", w.clock().tick(), " spawn=", t.spawn,
             " left=", t.left, " catch=", t.caught, " return=", t.returned, " food=", t.food);
        REQUIRE(t.spawn != 0);
        REQUIRE(t.left != 0);
        REQUIRE(t.caught != 0);
        REQUIRE(t.returned != 0);
        REQUIRE(t.food != 0);
    }
}
} // namespace

TEST_CASE("fisher real map: Venice 3978 three original huts complete trips and food without "
          "commands (game files)" *
          doctest::test_suite("heavy")) {
    const auto game = anno::test::game_dir();
    if (!game) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const std::string map = "0010SingleEndlos/01_1024_easy_256.sze";
    auto w = original_world(*game, map, true, false);
    auto trips = original_huts(w, false);
    run_canary(w, trips, map);
}

TEST_CASE("fisher real map: human 810 one prescribed Build completes trip and food (game files)" *
          doctest::test_suite("heavy")) {
    const auto game = anno::test::game_dir();
    if (!game) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const std::string map = "Tutorial/02Trade01.sze";
    auto w = original_world(*game, map, true, true);
    auto trips = original_huts(w, true);
    run_canary(w, trips, map);
}
