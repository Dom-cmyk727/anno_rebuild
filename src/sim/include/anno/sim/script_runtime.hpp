#pragma once
#include <memory>

#include <anno/sim/permissions.hpp>
#include <anno/sim/timers.hpp>
namespace anno::sim {
inline constexpr std::uint64_t kScriptSchema = 0x0031545049524353ULL; // "SCRIPT1\0"
struct ScriptRuntime {
    std::string source;
    ScriptLimits limits;
    std::uint32_t policy = kScriptPolicy;
    ScriptState state;
    std::shared_ptr<const ScriptProgram> program; // derived once, never serialized
};
[[nodiscard]] formats::Result<ScriptRuntime, std::string>
prepare_script(std::string source, const TypeRegistry&, const PlayerTable&, const SimConfig&,
               ScriptLimits = {});
// Native reload rebuilds immutable instructions, preserving and validating execution
// state/bindings.
[[nodiscard]] std::string restore_script(ScriptRuntime&, const TypeRegistry&, const PlayerTable&,
                                         const SimConfig&);
[[nodiscard]] std::string validate_script(const ScriptRuntime&, const TypeRegistry&,
                                          const PlayerTable&, const SimConfig&);
bool advance_script(ScriptRuntime&, PlayerTable&, ScriptTimers&, const TypeRegistry&,
                    std::uint32_t);
} // namespace anno::sim
