#pragma once
#include <anno/sim/replay.hpp>

#include "command_helpers.hpp"
namespace script_test {
namespace sim = anno::sim;
struct Fixture {
    sim::synthetic::SyntheticTypes types;
    sim::World world;
};
inline std::string source(std::string kernel) {
    return "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n\"ME\" 0\n\"OTHER\" "
           "1\n[SCRIPT_KERNEL]\n" +
           kernel + "\n[SIGNALS]\n[CAMARAPOS]\n0,0\n";
}
inline std::string permission(std::string owner, sim::TypeId id, bool value) {
    return "SETCONSTRUCTION_DRAWSTATE(\"" + owner + "\"," + std::to_string(id.v) + "," +
           (value ? "TRUE" : "FALSE") + ");";
}
inline Fixture make(bool atomic = false, std::optional<std::string> kernel = {},
                    sim::Seams seams = {}) {
    auto types = command_test::build_types();
    sim::TypeRegistry::Builder builder;
    builder.walk_tables(types.registry->walk_tables());
    for (const auto& t : types.registry->types())
        builder.add(t);
    auto custom = *types.registry->find(types.weaver);
    custom.id = sim::TypeId{90001};
    custom.construction.stage = 3;
    custom.permissions.folks = {1};
    builder.add(custom);
    auto registry = builder.build();
    REQUIRE(registry);
    types.registry = registry.value();
    types.weaver = custom.id;
    auto spec = command_test::two_city_spec(types);
    // Explicitly stocked synthetic fixture, unrelated to unchanged ship-start cargo.
    for (auto& city : spec.islands[0].cities)
        (*city.stock)[43] = 20 * sim::kTon;
    spec.script_context = sim::ScriptContext::Pending;
    if (!kernel)
        kernel = "INITBLOCK(){" + permission("ME", types.weaver, false) +
                 permission("OTHER", types.weaver, true) + permission("ME", types.road, false) +
                 "}\nIF PLAYTIME == 1 THEN {" + permission("ME", types.weaver, true) +
                 "}\nIF PLAYTIME > 1 THEN {" + permission("ME", types.weaver, false) + "}\n";
    spec.script_source = source(*kernel);
    sim::Rules rules;
    rules.build_atomic = atomic;
    auto world = sim::World::create(spec, types.registry, {}, rules, seams);
    REQUIRE_MESSAGE(world, (world ? "" : world.error()));
    return {types, std::move(world).value()};
}
inline void unlock(sim::World& w) {
    w.advance(0);
    w.advance(1000);
    w.advance(0);
}
} // namespace script_test
