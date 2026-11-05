#include <algorithm>
#include <utility>

#include <anno/sim/script_runtime.hpp>
namespace anno::sim {
namespace {
auto compile_runtime(const ScriptRuntime& r, const TypeRegistry& types, const SimConfig& cfg) {
    std::vector<TypeId> ids;
    ids.reserve(types.types().size());
    for (const auto& t : types.types())
        ids.push_back(t.id);
    return compile_script(r.source, {ids, cfg.player_slots, cfg.script_timers}, r.limits);
}
struct Host final : ScriptHost {
    PlayerTable& players;
    ScriptTimers& timers;
    const TypeRegistry& types;
    const std::vector<PlayerId>& bindings;
    std::vector<std::pair<PlayerId, Player>> staged;
    std::optional<ScriptTimers> staged_timers;
    Host(PlayerTable& p, ScriptTimers& t, const TypeRegistry& registry,
         const std::vector<PlayerId>& b)
        : players(p), timers(t), types(registry), bindings(b) {}
    bool begin() override {
        staged.clear();
        staged_timers.reset();
        return true;
    }
    void commit() override {
        for (auto& [id, player] : staged)
            *players.find(id) = std::move(player);
        if (staged_timers)
            timers = std::move(*staged_timers);
        staged.clear();
        staged_timers.reset();
    }
    void rollback() override {
        staged.clear();
        staged_timers.reset();
    }
    bool permission(std::uint32_t mapping, TypeId id, std::uint32_t raw, bool all,
                    ScriptWork& work) override {
        if (mapping >= bindings.size())
            return false;
        const auto owner = bindings[mapping];
        const auto* player = players.find(owner);
        if (!player)
            return false;
        auto it = std::find_if(staged.begin(), staged.end(),
                               [&](const auto& row) { return row.first == owner; });
        if (it == staged.end()) {
            staged.emplace_back(owner, *player);
            it = staged.end() - 1;
        }
        return all ? set_all_construction_permissions(it->second, it->second.permissions, types,
                                                      raw, work)
                   : set_construction_permission(it->second, it->second.permissions, types, id,
                                                 raw);
    }
    bool timer(ScriptAction action, std::uint32_t index, std::uint32_t value) override {
        if (index >= timers.counter.size() || index >= timers.active.size())
            return false;
        if (!staged_timers)
            staged_timers = timers;
        if (action == ScriptAction::TimerSet)
            staged_timers->counter[index] = value * 500u;
        else if (action == ScriptAction::TimerStart)
            staged_timers->active[index] = 1;
        else if (action == ScriptAction::TimerStop)
            staged_timers->active[index] = 0;
        else
            return false;
        return true;
    }
    std::uint32_t timer_seconds(std::uint32_t index) const override {
        const auto& current = staged_timers ? *staged_timers : timers;
        return index < current.counter.size() ? current.counter[index] / 500u : 0;
    }
};
} // namespace
formats::Result<ScriptRuntime, std::string>
prepare_script(std::string source, const TypeRegistry& types, const PlayerTable& players,
               const SimConfig& cfg, ScriptLimits limits) {
    ScriptRuntime r;
    r.source = std::move(source);
    r.limits = limits;
    auto program = compile_runtime(r, types, cfg);
    if (!program)
        return "script offset " + std::to_string(program.error().offset) + ": " +
               program.error().message;
    r.state = make_script_state(program.value());
    for (const auto& m : program.value().mappings) {
        PlayerId id{m.slot};
        if (!players.find(id))
            id = PlayerId{cfg.neutral_player};
        if (!players.find(id))
            return std::string("script mapping has no player or neutral fallback: ") + m.name;
        r.state.bindings.push_back(id);
    }
    r.program = std::make_shared<const ScriptProgram>(std::move(program).value());
    return r;
}
std::string validate_script(const ScriptRuntime& r, const TypeRegistry& types,
                            const PlayerTable& players, const SimConfig& cfg) {
    const auto& s = r.state;
    if (r.policy != kScriptPolicy)
        return "script execution policy differs";
    if (r.limits.source_bytes == 0 || r.limits.source_bytes > 1048576 || r.limits.nodes == 0 ||
        r.limits.nodes > 65536 || r.limits.depth == 0 || r.limits.depth > 128 ||
        r.limits.work == 0 || r.limits.work > 10000000)
        return "invalid script limits";
    for (const auto& p : players.slots)
        if (p) {
            auto problem = validate_permissions(p->permissions, types);
            if (!problem.empty())
                return problem;
        }
    if (s.context == ScriptContext::Missing || s.context == ScriptContext::ExplicitNone) {
        if (!r.source.empty() || r.program || !s.signals.empty() || !s.bindings.empty() ||
            s.cursor || s.last_arg || s.last_work || s.fault != ScriptFault::None || s.fault_offset)
            return "script-free context carries execution state";
        return {};
    }
    if (s.context != ScriptContext::Pending && s.context != ScriptContext::Ready &&
        s.context != ScriptContext::Fault)
        return "unplayable script context";
    if (!r.program || r.program->source != r.source || r.program->policy != r.policy)
        return "script executable identity differs";
    const auto& p = *r.program;
    if (s.signals.size() != p.initial.size() || s.bindings.size() != p.mappings.size() ||
        s.cursor >= std::max<std::size_t>(1, p.branches.size()) || s.last_work > r.limits.work ||
        std::any_of(s.signals.begin(), s.signals.end(), [](auto b) { return b > 1; }))
        return "invalid script continuation";
    for (auto id : s.bindings)
        if (id.v >= cfg.player_slots || !players.find(id))
            return "invalid script binding";
    if (s.fault_offset > r.source.size() || (s.fault == ScriptFault::None && s.fault_offset != 0))
        return "invalid script fault location";
    if ((s.context == ScriptContext::Fault) != (s.fault != ScriptFault::None))
        return "invalid script fault state";
    if (s.context == ScriptContext::Pending &&
        (s.cursor || s.signals != p.initial || s.last_work || s.last_arg))
        return "invalid pending script state";
    return {};
}
std::string restore_script(ScriptRuntime& r, const TypeRegistry& types, const PlayerTable& players,
                           const SimConfig& cfg) {
    if (r.policy != kScriptPolicy)
        return "script execution policy differs";
    if (!r.source.empty()) {
        auto p = compile_runtime(r, types, cfg);
        if (!p)
            return "script offset " + std::to_string(p.error().offset) + ": " + p.error().message;
        r.program = std::make_shared<const ScriptProgram>(std::move(p).value());
    } else
        r.program.reset();
    return validate_script(r, types, players, cfg);
}
bool advance_script(ScriptRuntime& r, PlayerTable& players, ScriptTimers& timers,
                    const TypeRegistry& types, std::uint32_t arg) {
    if (!r.program)
        return r.state.context == ScriptContext::Missing ||
               r.state.context == ScriptContext::ExplicitNone;
    Host host(players, timers, types, r.state.bindings);
    return visit_script(*r.program, r.state, arg, host);
}
} // namespace anno::sim
