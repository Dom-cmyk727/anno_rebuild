// The iron pilot world: the original endless02 island 1, owner 8 (city 0), with the stage pass
// running (Rules::house_updates), shared by the iron mine and iron chain tests (MAP-EXTRACT-2).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/world.hpp>
#include <anno/test/game_dir.hpp>

#include "original_component.hpp"
#include "test_must.hpp"
#include "tutorial_town.hpp"
#include "world_access.hpp"

namespace iron_pilot {

namespace sim = anno::sim;

inline constexpr sim::PlayerId kPlayer{8};
inline constexpr sim::CityRef kCity{sim::IslandId{1}, sim::CityNo{0}};
inline constexpr std::uint64_t kAdvanceMs = 100; // 50 ticks; a production step is 10 advances

struct World {
    sim::World w;
    std::shared_ptr<const sim::TypeRegistry> types;
};

/// The pilot advanced `advances` times of 100 ms; nullopt without the original files.
inline std::optional<World> make(bool live_houses, bool atomic, int advances) {
    auto o = tutorial_town::load_original();
    if (!o) {
        return std::nullopt;
    }
    auto resolved = anno::sim_load::load_scenario_with_links(
        anno::test::read_file(o->dir / "Scenes" / "0011SingleEndlosAddon" / "endless02.sze"),
        o->templates, *o->types, o->cfg, {}, original_component::actor_free_units());
    REQUIRE(resolved);
    auto component =
        original_component::import_only(resolved.value().loaded.spec, o->types, o->cfg);
    REQUIRE_MESSAGE(component, (component ? "" : component.error()));
    sim::Rules rules;
    rules.build_atomic = atomic;
    rules.house_updates = live_houses;
    auto created = sim::World::create(component.value().spec, o->types, o->cfg, rules);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World world = std::move(created).value();
    if (world.script().state.context == sim::ScriptContext::Pending) {
        REQUIRE(world.initialize_scripts().empty());
    }
    for (int i = 0; i < advances; ++i) {
        world.advance(kAdvanceMs);
        REQUIRE_MESSAGE(!world.faulted(), world.fault());
    }
    return World{std::move(world), o->types};
}

inline sim::Command build_cmd(std::uint32_t type, std::int32_t x, std::int32_t y,
                              std::uint8_t rot = 0) {
    return sim::Command{
        kPlayer, sim::BuildCmd{kCity, {sim::PlacementWire{sim::TypeId{type}, x, y, rot, 0, 0}}}};
}
inline const sim::Island& isle(const sim::World& w) {
    return *w.islands()[1];
}
inline const sim::City& city_of(const sim::World& w) {
    return *test_must::must(isle(w).city(kCity.city));
}
inline std::int64_t gold(const sim::World& w) {
    return test_must::must(w.players().find(kPlayer))->money;
}
/// The first building of `type` in the city (other than `skip`), or nullptr.
inline const sim::Building* find_type(const sim::World& w, std::uint32_t type,
                                      sim::BuildingId skip = {}) {
    for (const auto& l : city_of(w).index.lists)
        for (const auto& b : l.slots)
            if (b && b->type.v == type && !(skip.valid() && b->id == skip))
                return &*b;
    return nullptr;
}

} // namespace iron_pilot
