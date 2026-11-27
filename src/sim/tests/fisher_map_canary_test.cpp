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
#include <anno/sim/hash.hpp>
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
        REQUIRE(t.raw >= farm.raw);
        REQUIRE(t.raw - farm.raw == type->prod.input_per_step.at(0));
        t.food = tick;
        std::cout << "FISHER_FOOD hut=" << t.x << ',' << t.y << " tick=" << tick
                  << " output=" << farm.out << '\n';
    }
    t.raw = farm.raw;
    t.out = farm.out;
}
sim::World original_world(const std::filesystem::path& game, const std::string& map,
                          std::optional<bool> enabled, bool human, bool closed = false) {
    const auto registry =
        anno::sim_load::load_original_registry(game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    const auto templates = anno::sim_load::scan_island_templates(game);
    const auto loaded = anno::sim_load::load_scenario_file(game / "Scenes" / map, game, templates,
                                                           *registry.types, {}, {});
    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
    sim::ExtensionContext extensions;
    extensions.fisher_rules.enabled = enabled.value_or(false) ? std::uint8_t{1} : std::uint8_t{0};
    auto made = enabled ? sim::World::create(loaded.value().loaded.spec, registry.types, {}, {}, {},
                                             sim::serial_executor(), extensions)
                        : sim::World::create(loaded.value().loaded.spec, registry.types);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    auto w = std::move(made).value();
    auto session = sim::Recorder::start(w);
    REQUIRE_MESSAGE(session, (session ? "" : session.error()));
    if (human) {
        const auto result =
            w.apply({sim::PlayerId{2},
                     sim::BuildCmd{{sim::IslandId{3}, sim::CityNo{0}},
                                   {{sim::TypeId{810}, closed ? 242 : 243, closed ? 217 : 216,
                                     static_cast<std::uint8_t>(closed ? 1 : 0), 0, 0}}}});
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
                                REQUIRE(type->prod.input_per_step.at(0) == (human ? 69U : 128U));
                                REQUIRE(type->prod.output_per_step.at(0) == (human ? 17U : 51U));
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

void require_quiet(const sim::World& w, const std::vector<Trip>& trips) {
    REQUIRE_FALSE(w.fisher_enabled());
    REQUIRE_MESSAGE(!w.faulted(), w.fault());
    for (const auto& t : trips) {
        const auto* b = w.find_building(t.home);
        REQUIRE(b);
        const auto& farm = std::get<sim::Farm>(b->payload);
        REQUIRE(farm.workers_out == 0);
        REQUIRE(farm.raw == 0);
        REQUIRE(farm.out == 0);
    }
    for (const auto& f : w.figures().list.slots)
        REQUIRE_FALSE((f && f->kind == 142 && f->job == 45 && !f->expiry_stamp));
}
void run_canary(sim::World& w, std::vector<Trip>& trips, const std::string& map,
                const std::filesystem::path& game, bool human) {
    REQUIRE(w.fisher_enabled());
    // Independent normal creations, not copies of the reference state.
    auto duplicate = original_world(game, map, true, human);
    auto off = original_world(game, map, false, human);
    auto defaults = original_world(game, map, std::nullopt, human);
    const auto off_trips = original_huts(off, human);
    const auto default_trips = original_huts(defaults, human);
    const auto off_start = off.state_hash();
    REQUIRE(w.state_hash() == duplicate.state_hash());
    REQUIRE(w.state_hash() != off_start);
    REQUIRE(off.save() == defaults.save());
    std::optional<sim::World> restored;
    std::uint64_t cut_tick = 0, turns = 0, restored_turns = 0;
    sim::StateHasher on_trace, off_trace;
    while (w.clock().tick() < kTripBound) {
        w.advance(kTurnMs);
        duplicate.advance(kTurnMs);
        off.advance(kTurnMs);
        defaults.advance(kTurnMs);
        if (restored) {
            restored->advance(kTurnMs);
            ++restored_turns;
        }
        REQUIRE_MESSAGE(!w.faulted(), w.fault());
        REQUIRE_MESSAGE(!duplicate.faulted(), duplicate.fault());
        const auto hash = w.state_hash();
        REQUIRE(hash == duplicate.state_hash());
        if (restored) {
            REQUIRE_MESSAGE(!restored->faulted(), restored->fault());
            REQUIRE(hash == restored->state_hash());
        }
        require_quiet(off, off_trips);
        require_quiet(defaults, default_trips);
        REQUIRE(off.state_hash() == defaults.state_hash());
        on_trace.u64(hash);
        off_trace.u64(off.state_hash());
        ++turns;
        for (auto& t : trips) {
            INFO("map=", map, " hut=", t.x, ",", t.y, " tick=", w.clock().tick());
            if (!t.food)
                observe(w, t);
        }
        if (!restored) {
            for (const auto& t : trips) {
                const auto* f = find_worker(w, t.worker);
                if (!f || !f->harvest || f->harvest->step != sim::HarvestStep::Cut ||
                    f->harvest->wait == 0)
                    continue;
                const auto snapshot = w.save();
                const auto registry = anno::sim_load::load_original_registry(
                    game, anno::sim_load::FigureEntries::Required);
                REQUIRE_MESSAGE(registry.types, registry.error);
                auto loaded = sim::World::load(snapshot, registry.types);
                REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
                restored.emplace(std::move(loaded).value());
                REQUIRE(restored->fisher_enabled());
                REQUIRE(restored->state_hash() == hash);
                REQUIRE(restored->save() == snapshot);
                cut_tick = w.clock().tick();
                std::cout << "FISHER_NATIVE_CUT map=" << map << " tick=" << cut_tick
                          << " hut=" << t.x << ',' << t.y << " wait=" << f->harvest->wait << '\n';
                break;
            }
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
    REQUIRE(cut_tick != 0);
    REQUIRE(restored_turns > 0);
    REQUIRE(w.save() == duplicate.save());
    REQUIRE(w.save() == restored->save());
    REQUIRE(off.save() == defaults.save());
    std::cout << "FISHER_DETERMINISM map=" << map << " tick=" << w.clock().tick()
              << " turns=" << turns << " restored_turns=" << restored_turns
              << " on_trace=" << std::hex << on_trace.value() << std::dec << '\n';
    std::cout << "FISHER_OFF map=" << map << " tick=" << w.clock().tick() << " start=" << std::hex
              << off_start << " end=" << off.state_hash() << " trace=" << off_trace.value()
              << std::dec << '\n';
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
          "commands with per-turn reload and flag-off checks (game files)" *
          doctest::test_suite("heavy")) {
    const auto game = anno::test::game_dir();
    if (!game) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const std::string map = "0010SingleEndlos/01_1024_easy_256.sze";
    auto w = original_world(*game, map, true, false);
    auto trips = original_huts(w, false);
    run_canary(w, trips, map, *game, false);
}

TEST_CASE("fisher real map: human 810 one prescribed Build completes trip and food with per-turn "
          "reload and flag-off checks (game files)" *
          doctest::test_suite("heavy")) {
    const auto game = anno::test::game_dir();
    if (!game) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const std::string map = "Tutorial/02Trade01.sze";
    auto w = original_world(*game, map, true, true);
    auto trips = original_huts(w, true);
    run_canary(w, trips, map, *game, true);
}

TEST_CASE("fisher closed real map: human 810 leaves catches fails home waits releases resends and "
          "reloads deterministically with flag-off identity (game files)" *
          doctest::test_suite("heavy")) {
    const auto game = anno::test::game_dir();
    if (!game) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const std::string map = "Tutorial/02Trade01.sze";
    auto w = original_world(*game, map, true, true, true);
    auto duplicate = original_world(*game, map, true, true, true);
    auto off = original_world(*game, map, false, true, true);
    auto defaults = original_world(*game, map, std::nullopt, true, true);
    const auto registry =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    const auto huts = original_huts(w, true);
    REQUIRE(huts.size() == 1);
    const auto hid = huts[0].home;
    const auto* type = w.types().find(sim::TypeId{810});
    REQUIRE(type);
    const auto farm_at = [&](const sim::World& world) -> const sim::Farm& {
        const auto* b = world.find_building(hid);
        REQUIRE(b);
        return std::get<sim::Farm>(b->payload);
    };
    const auto& island = *w.islands()[3];
    const auto entrances = sim::entrance_walk_points(*w.find_building(hid), *type);
    REQUIRE(entrances.size() == 2);
    for (std::size_t n = 0; n < entrances.size(); ++n) {
        const auto cell = sim::fisher_cell_at(
            island.terrain->height, island.tiles.width(), island.tiles.height(),
            static_cast<std::uint32_t>(entrances[n].tile.x - island.tiles.x0()),
            static_cast<std::uint32_t>(entrances[n].tile.y - island.tiles.y0()));
        REQUIRE(cell);
        CHECK(cell->mean == (n == 0 ? 32 : 48));
        CHECK_FALSE(cell->walkable_water);
    }
    std::optional<sim::World> cut_reload, end_reload;
    sim::FigureId first, second;
    std::uint64_t spawn = 0, left = 0, caught = 0, released = 0, expiry = 0, removed = 0,
                  second_tick = 0;
    std::uint32_t cargo = 0, cuts = 0, due = 0;
    sim::FixedPos last_catch, stop;
    std::optional<sim::TileXY> cutting;
    sim::StateHasher on_trace, off_trace;
    while (w.clock().tick() < kTripBound) {
        w.advance(kTurnMs);
        duplicate.advance(kTurnMs);
        off.advance(kTurnMs);
        defaults.advance(kTurnMs);
        if (cut_reload)
            cut_reload->advance(kTurnMs);
        if (end_reload)
            end_reload->advance(kTurnMs);
        REQUIRE_MESSAGE(!w.faulted(), w.fault());
        REQUIRE_MESSAGE(!off.faulted(), off.fault());
        const auto hash = w.state_hash();
        REQUIRE(hash == duplicate.state_hash());
        if (cut_reload)
            REQUIRE(hash == cut_reload->state_hash());
        if (end_reload)
            REQUIRE(hash == end_reload->state_hash());
        REQUIRE(off.state_hash() == defaults.state_hash());
        on_trace.u64(hash);
        off_trace.u64(off.state_hash());
        const auto tick = w.clock().tick();
        const auto& farm = farm_at(w);
        REQUIRE(farm.raw == 0);
        REQUIRE(farm.out == 0);
        REQUIRE(farm_at(off).workers_out == 0);
        REQUIRE(farm_at(off).raw == 0);
        REQUIRE(farm_at(off).out == 0);
        for (const auto& slot : w.figures().list.slots)
            if (slot && slot->home == hid) {
                if (!first.valid()) {
                    first = slot->id;
                    spawn = tick;
                    REQUIRE(slot->kind == 142);
                    REQUIRE(slot->job == 45);
                    REQUIRE(slot->pos == sim::entrance_position(*w.find_building(hid), *type, 0));
                    std::cout << "FISHER_CLOSED spawn=" << spawn << " worker=" << first.v << '\n';
                } else if (slot->id != first && !second.valid()) {
                    second = slot->id;
                    second_tick = tick;
                    REQUIRE(released != 0);
                    std::cout << "FISHER_CLOSED second=" << second_tick << " worker=" << second.v
                              << '\n';
                }
            }
        const auto* f = find_worker(w, first);
        if (f && f->harvest) {
            const auto& h = *f->harvest;
            if (!left &&
                !sim::tile_in_footprint(*w.find_building(hid), *type, sim::tile_of_q8(f->pos.x),
                                        sim::tile_of_q8(f->pos.y))) {
                left = tick;
                std::cout << "FISHER_CLOSED left=" << left << '\n';
            }
            if (f->cargo_88 > cargo) {
                REQUIRE(cutting);
                const auto at = island.tile_index(cutting->x, cutting->y);
                REQUIRE(at != sim::kNoSlot);
                REQUIRE(island.tiles.object(at) == 10);
                REQUIRE_FALSE(island.tiles.claimed(at));
                REQUIRE(f->cargo_88 - cargo == 256);
                cargo = f->cargo_88;
                ++cuts;
                if (!caught)
                    caught = tick;
                last_catch = f->pos;
                std::cout << "FISHER_CLOSED catch=" << tick << " cargo=" << cargo << '\n';
            }
            if (h.step == sim::HarvestStep::Cut && h.wait) {
                const auto at = island.tile_index(h.target.x, h.target.y);
                REQUIRE(at != sim::kNoSlot);
                REQUIRE(island.tiles.object(at) == 11);
                REQUIRE(island.tiles.claimed(at));
                cutting = h.target;
                if (!cut_reload) {
                    auto loaded = sim::World::load(w.save(), registry.types);
                    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
                    cut_reload.emplace(std::move(loaded).value());
                    REQUIRE(cut_reload->state_hash() == hash);
                }
            }
            if (!f->home.valid() && !released) {
                released = tick;
                stop = f->pos;
                due = std::uint32_t{f->stamp} + h.wait;
                REQUIRE(left);
                REQUIRE(cuts == 3);
                REQUIRE(f->cargo_88 == 768);
                REQUIRE(farm.workers_out == 0);
                REQUIRE(h.step == sim::HarvestStep::Done);
                REQUIRE(h.route.empty());
                REQUIRE(h.wait > 0);
                REQUIRE_FALSE(f->expiry_stamp);
                REQUIRE(stop == last_catch);
                auto loaded = sim::World::load(w.save(), registry.types);
                REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
                end_reload.emplace(std::move(loaded).value());
                REQUIRE(end_reload->state_hash() == hash);
                std::cout << "FISHER_CLOSED released=" << released << " remaining=" << h.wait
                          << " nominal_due=" << due << " pos=" << stop.x << ',' << stop.y
                          << " raw=" << farm.raw << " workers=" << farm.workers_out << '\n';
            }
            if (released) {
                REQUIRE(f->pos == stop);
                REQUIRE(f->cargo_88 == 768);
                REQUIRE(h.route.empty());
                if (!f->expiry_stamp) {
                    REQUIRE(std::uint32_t{f->stamp} + h.wait == due);
                } else if (!expiry) {
                    expiry = tick;
                    REQUIRE(h.wait == 0);
                    REQUIRE(*f->expiry_stamp >= due);
                    std::cout << "FISHER_CLOSED expiry=" << expiry << '\n';
                }
            }
        } else if (first.valid() && !removed) {
            removed = tick;
            REQUIRE(expiry);
            std::cout << "FISHER_CLOSED removed=" << removed << '\n';
        }
    }
    REQUIRE(spawn);
    REQUIRE(left);
    REQUIRE(caught);
    REQUIRE(released);
    REQUIRE(second_tick);
    REQUIRE(expiry);
    REQUIRE(removed);
    REQUIRE(cut_reload);
    REQUIRE(end_reload);
    REQUIRE(w.save() == duplicate.save());
    REQUIRE(w.save() == cut_reload->save());
    REQUIRE(w.save() == end_reload->save());
    REQUIRE(off.save() == defaults.save());
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    std::cout << "FISHER_CLOSED_FINAL tick=" << w.clock().tick() << " on=" << std::hex
              << w.state_hash() << " on_trace=" << on_trace.value() << " off=" << off.state_hash()
              << " off_trace=" << off_trace.value() << std::dec << '\n';
}
