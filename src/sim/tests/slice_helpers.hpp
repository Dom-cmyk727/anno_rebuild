// Helpers shared by the stage S7 tests (scheduler_test.cpp, slice_test.cpp): a tick
// context over a world's parts so the island-phase functions can be called
// directly, recording seams, scenarios with many cities or a production list full of
// workshops, and a registry whose types have figure ids.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/draw.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/island_tick.hpp>
#include <anno/sim/scheduler.hpp>
#include <anno/sim/tick.hpp>

#include "building_helpers.hpp"

namespace slice_test {

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using building_test::Kit;

/// What a recording seam writes to (through its ctx pointer).
struct Log {
    std::vector<std::string> lines;
    void add(std::string s) { lines.push_back(std::move(s)); }
    [[nodiscard]] std::size_t count(const std::string& prefix) const {
        std::size_t n = 0;
        for (const std::string& l : lines) {
            if (l.rfind(prefix, 0) == 0) {
                ++n;
            }
        }
        return n;
    }
};

/// A tick context built over a world's own parts, for calling the island-phase
/// functions directly at a chosen tick. The world's PRNG is behind the context's
/// DrawSource (counted per site).
struct Tick {
    sim::GlobalDraw global;
    sim::CountingDraw draw;
    sim::TickContext ctx;
    Tick(sim::World& w, std::uint64_t tick)
        : global(sim::WorldTestAccess::rng(w)), draw(global), ctx{tick,
                                                                  static_cast<std::uint32_t>(tick),
                                                                  w.config(),
                                                                  w.rules(),
                                                                  w.settings(),
                                                                  w.types(),
                                                                  sim::PlayersView(w.players()),
                                                                  sim::IslandsView(w.islands()),
                                                                  draw,
                                                                  w.seams()} {}
};

inline sim::Island& island(sim::World& w, std::size_t slot = 0) {
    return *sim::WorldTestAccess::islands(w)[slot];
}

inline sim::City& city(sim::World& w, std::size_t no, std::size_t slot = 0) {
    return *island(w, slot).cities[no];
}

/// A seam name for tests (never "default" or empty).
inline const char* seam_name() {
    return "test_recorder";
}

/// A scenario of one player, one island (64 x 64 at 100,100, climate NORD) and
/// `cities` empty cities 0 .. cities-1 owned by player 0.
inline sim::ScenarioSpec cities_scenario(std::uint32_t cities, std::uint32_t seed = 1234) {
    sim::ScenarioSpec spec = syn::basic_scenario(seed);
    spec.islands[0].cities.clear();
    for (std::uint32_t c = 0; c < cities; ++c) {
        spec.islands[0].cities.push_back(syn::plain_city(c, 0));
    }
    // Tile owners must be cities of the island: leave every tile neutral.
    spec.islands[0].owner_runs.clear();
    return spec;
}

/// A weaver (the kit's type) at tile (x, y) with 5 t of wool and the given `last`.
inline sim::ScenarioSpec::BuildingSpec weaver_at(const Kit& k, std::int32_t x, std::int32_t y,
                                                 std::uint16_t last = 0) {
    sim::ScenarioSpec::BuildingSpec b = building_test::place(k.weaver, x, y);
    sim::Workshop w = sim::make_workshop(k.registry->find(k.weaver)->prod);
    w.in[0] = 5 * sim::kTon;
    w.last = last;
    b.payload = w;
    return b;
}

/// The first `n` slots of the production list of a city.
inline std::vector<std::optional<sim::Building>>& production_slots(sim::City& c) {
    return c.index.lists[4].slots;
}

/// The kit's registry with the first figure id of the weaver and the farm set, so a
/// spawn carries a kind that tells which type sent it.
inline Kit kit_with_figure_ids(std::uint32_t weaver_kind, std::uint32_t farm_kind) {
    Kit k = building_test::make_kit();
    sim::TypeRegistry::Builder b;
    for (const sim::BuildingType& t : k.registry->types()) {
        sim::BuildingType copy = t;
        if (copy.id == k.weaver) {
            copy.prod.figure_ids[0] = weaver_kind;
        } else if (copy.id == k.farm) {
            copy.prod.figure_ids[0] = farm_kind;
        }
        b.add(copy);
    }
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    k.registry = std::move(built.value());
    return k;
}

/// World::create with seams on the kit's registry, required to succeed.
inline sim::World make_world_with(const Kit& k, const sim::ScenarioSpec& spec, sim::Seams seams,
                                  sim::SimConfig cfg = {}, sim::Rules rules = {}) {
    auto r = sim::World::create(spec, k.registry, std::move(cfg), rules, std::move(seams));
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

/// A seam table (default config) with every city pass named, so a test can install
/// functions one by one.
inline sim::Seams named_seams() {
    sim::Seams s;
    for (auto& p : s.city_pass) {
        p.name = seam_name();
    }
    return s;
}

} // namespace slice_test
