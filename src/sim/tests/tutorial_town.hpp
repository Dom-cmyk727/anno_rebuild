#include "original_component.hpp"
// Shared by the K2/K3 tests of MAP-POP-CLOSE-2: the Tutorial town on ORIGINAL data (needs
// ANNO1503_GAME_DIR).
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <doctest/doctest.h>

#include <anno/sim/city.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>

#include "world_access.hpp"

namespace tutorial_town {

namespace sim = anno::sim;
namespace fs = std::filesystem;

inline constexpr const char* kTutorial = "Tutorial/02Trade01.sze";
inline constexpr std::uint32_t kIsland = 3, kOwner = 2;
inline constexpr sim::TypeId kPioneer{501};
inline constexpr sim::GoodsId kLeder = 27, kSalz = 28, kStoffe = 33, kKapelle = 82;

struct Original {
    std::shared_ptr<const sim::TypeRegistry> types;
    sim::SimConfig cfg;
    anno::sim_load::IslandTemplates templates;
    fs::path dir;
};

inline std::optional<Original> load_original() {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return std::nullopt;
    }
    static const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    Original o;
    o.types = loaded.types;
    o.templates = anno::sim_load::scan_island_templates(*dir);
    o.dir = *dir;
    return o;
}

inline sim::World tutorial_world(const Original& o, bool live) {
    auto resolved = anno::sim_load::load_scenario_with_links(
        anno::test::read_file(o.dir / "Scenes" / kTutorial), o.templates, *o.types, o.cfg, {},
        original_component::actor_free_units());
    REQUIRE(resolved);
    sim::Rules rules;
    rules.house_updates = live;
    auto created = sim::World::create(resolved.value().loaded.spec, o.types, o.cfg, rules);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World world = std::move(created).value();
    // Run the complete supported kernel through the explicit normal startup boundary
    // before commands, without an extra world-subsystem visit.
    REQUIRE(world.script().state.context == sim::ScriptContext::Pending);
    REQUIRE(world.initialize_scripts().empty());
    REQUIRE(world.script().state.context == sim::ScriptContext::Ready);
    return world;
}

struct Town {
    sim::Island* island = nullptr;
    sim::City* city = nullptr;
    sim::CityRef ref;
};

inline Town find_town(sim::World& w) {
    Town t;
    for (auto& isl : sim::WorldTestAccess::islands(w)) {
        if (!isl || isl->index.v != kIsland) {
            continue;
        }
        for (auto& c : isl->cities) {
            if (c && c->owner.v == kOwner) {
                t.island = &*isl;
                t.city = &*c;
                t.ref = sim::CityRef{isl->index, c->number};
            }
        }
    }
    REQUIRE(t.city != nullptr);
    return t;
}

} // namespace tutorial_town
