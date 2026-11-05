// Test-only original-data component fixtures. Never use this in production loading.
#pragma once
#include <anno/sim/scenario.hpp>
#include <anno/sim/world.hpp>

namespace original_component {
namespace sim = anno::sim;
struct Scenario {
    sim::ScenarioSpec spec;
    std::string diagnostic;
    bool isolated = false;
};
// Preserve supported whole programs. An unsupported whole program must demonstrably
// refuse normal creation before a NEW import/mechanism fixture is authored without it.
// Missing context deliberately keeps player construction unavailable in that fixture.
inline anno::formats::Result<Scenario, std::string>
import_only(const sim::ScenarioSpec& original,
            const std::shared_ptr<const sim::TypeRegistry>& types, const sim::SimConfig& cfg = {}) {
    Scenario out{original, "supported whole program retained", false};
    if (original.script_context != sim::ScriptContext::Pending)
        return out;
    std::vector<sim::TypeId> ids;
    for (const auto& type : types->types())
        ids.push_back(type.id);
    std::vector<std::string_view> goods;
    for (const auto& g : types->goods_table())
        goods.push_back(g.name);
    std::optional<std::string_view> ns;
    if (original.script_asset_namespace)
        ns = *original.script_asset_namespace;
    auto program = sim::compile_script(original.script_source,
                                       {ids, cfg.player_slots, cfg.script_timers, goods, ns},
                                       original.script_limits);
    if (program)
        return out;
    const auto refused = sim::World::create(original, types, cfg);
    if (refused || refused.error().find(program.error().message) == std::string::npos)
        return std::string("component setup did not observe the whole-program compile refusal");
    out.diagnostic = refused.error();
    out.isolated = true;
    out.spec.script_source.clear();
    out.spec.script_context = sim::ScriptContext::Missing;
    out.spec.script_limits = {};
    return out;
}
} // namespace original_component
