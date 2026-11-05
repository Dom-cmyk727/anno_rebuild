#pragma once
#include <memory>

#include <anno/sim/figure.hpp>
#include <anno/sim/permissions.hpp>
#include <anno/sim/settings.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/timers.hpp>
namespace anno::sim {
inline constexpr std::uint64_t kScriptSchema = 0x0032545049524353ULL; // "SCRIPT2\0"
struct ScriptQueryStats {
    std::uint32_t slots = 0, inserted = 0, rows = 0, cells = 0, candidates = 0, crew = 0;
};
struct ScriptRuntime {
    std::string source;
    ScriptLimits limits;
    std::uint32_t policy = kScriptPolicy;
    ScriptState state;
    std::optional<std::string>
        asset_namespace;          // persisted content identity, no host absolute paths
    ScriptQueryStats query_stats; // derived diagnostic, never saved/hashed
    std::shared_ptr<const ScriptProgram> program; // derived once, never serialized
};
[[nodiscard]] formats::Result<ScriptRuntime, std::string>
prepare_script(std::string source, const TypeRegistry&, const PlayerTable&, const SimConfig&,
               ScriptLimits = {}, std::optional<std::string> asset_namespace = {});
[[nodiscard]] std::uint64_t script_content_digest(const ScriptRuntime&);
// Native reload rebuilds immutable instructions, preserving and validating execution
// state/bindings.
[[nodiscard]] std::string restore_script(ScriptRuntime&, const TypeRegistry&, const PlayerTable&,
                                         const SimConfig&);
[[nodiscard]] std::string validate_script(const ScriptRuntime&, const TypeRegistry&,
                                          const PlayerTable&, const SimConfig&);
bool advance_script(ScriptRuntime&, PlayerTable&, ScriptTimers&, const TypeRegistry&, std::uint32_t,
                    const FigureRegistry* = nullptr, const WorldSettings* = nullptr,
                    std::vector<SimEvent>* output = nullptr, std::uint64_t event_tick = 0);
} // namespace anno::sim
