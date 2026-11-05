#include <anno/sim_load/script_base.hpp>
namespace anno::sim_load {
std::string make_generated_script_free(sim::ScenarioSpec& spec, const sim::TypeRegistry& types,
                                       const sim::SimConfig& cfg) {
    if (!spec.script_source.empty()) {
        if (spec.script_context != sim::ScriptContext::Pending)
            return "invalid generated base script context";
        std::vector<sim::TypeId> ids;
        ids.reserve(types.types().size());
        for (const auto& type : types.types())
            ids.push_back(type.id);
        auto program = sim::compile_script(
            spec.script_source, {ids, cfg.player_slots, cfg.script_timers}, spec.script_limits);
        if (!program)
            return "generated layout requires a validated empty base script: " +
                   program.error().message;
        if (!program.value().kernel_empty)
            return "generated layout refuses a nonempty base script kernel";
    } else if (spec.script_context != sim::ScriptContext::Missing &&
               spec.script_context != sim::ScriptContext::ExplicitNone) {
        return "invalid empty generated base script context";
    }
    spec.script_source.clear();
    spec.script_context = sim::ScriptContext::ExplicitNone;
    spec.script_limits = {};
    return {};
}
} // namespace anno::sim_load
