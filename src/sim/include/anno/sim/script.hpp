// SCRIPT1: compiled deterministic BIGBROTHER subset. No world/renderer access.
#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/ids.hpp>
namespace anno::sim {
inline constexpr std::uint32_t kScriptPolicy = 2;
struct ScriptLimits {
    std::uint32_t source_bytes = 1048576, nodes = 65536, depth = 128, work = 100000;
    std::uint32_t asset_bytes = 4096; // configurable presentation-reference bound, hard65536
};
enum class ScriptContext : std::uint8_t {
    Missing,
    ExplicitNone,
    Pending,
    Ready,
    Unsupported,
    Fault
};
enum class ScriptFault : std::uint8_t { None, Budget, Host, InvalidState };
struct ScriptError {
    std::uint32_t offset = 0;
    std::string message;
};
struct ScriptEnvironment {
    std::span<const TypeId> types;
    std::uint32_t player_slots = 32, timer_count = 5;
    std::span<const std::string_view> goods_names{}; // registry order, index0 is UNUSED
    std::optional<std::string_view> asset_namespace{};
};
struct ScriptMapping {
    std::string name;
    std::uint32_t slot = 0, offset = 0;
};
struct ScriptArea {
    std::string name;
    std::int32_t y0 = 0, x0 = 0, x1 = 0, y1 = 0;
    std::uint32_t offset = 0;
};
enum class ScriptQuery : std::uint8_t { Signal, Playtime, Timer, ShipTouch, SoldierTouch };
enum class ScriptCompare : std::uint8_t { Truth, Equal, Less, Greater };
struct ScriptExpr {
    enum class Kind : std::uint8_t { Leaf, And, Or } kind = Kind::Leaf;
    ScriptQuery query = ScriptQuery::Signal;
    ScriptCompare compare = ScriptCompare::Truth;
    std::uint32_t index = 0, left = 0, right = 0, offset = 0;
    std::int32_t value = 0;
    std::uint32_t mapping = 0;
};
enum class ScriptAction : std::uint8_t {
    Assign,
    Permission,
    PermissionAll,
    TimerSet,
    TimerStart,
    TimerStop,
    MoneyAdd,
    PlayWave
};
struct ScriptInstruction {
    ScriptAction action = ScriptAction::Assign;
    std::uint32_t index = 0, value = 0, offset = 0;
    TypeId type;
};
struct ScriptIf {
    std::uint32_t condition = 0, offset = 0;
    std::vector<ScriptInstruction> yes, no;
};
struct ScriptProgram {
    std::string source;
    ScriptLimits limits;
    std::uint32_t policy = kScriptPolicy;
    std::vector<std::string> symbols;
    std::vector<std::uint8_t> initial;
    std::vector<ScriptMapping> mappings;
    std::vector<ScriptArea> areas;
    std::string asset_namespace;     // one normalized derived prefix per program
    std::vector<std::string> assets; // relative names; total bytes bounded by source bytes
    std::vector<ScriptExpr> expressions;
    std::vector<ScriptInstruction> init;
    std::vector<ScriptIf> branches;
    bool kernel_empty = true; // derived syntax fact; includes empty INIT/IF/SET presence
};
struct ScriptState {
    ScriptContext context = ScriptContext::Missing;
    ScriptFault fault = ScriptFault::None;
    std::uint32_t cursor = 0, last_arg = 0, fault_offset = 0, last_work = 0;
    std::vector<std::uint8_t> signals;
    // Actual resolved player ids, in declaration order. Never rebound on native reload.
    std::vector<PlayerId> bindings;
};
struct ScriptWork {
    std::uint32_t remaining = 0, used = 0;
    bool exhausted = false;
    bool consume();
};
// Host stages only typed mutable state. Rollback is required after any refusal;
// all effects are visible to later instructions inside the same transaction.
struct ScriptHost {
    virtual ~ScriptHost() = default;
    virtual bool begin() = 0;
    virtual void commit() = 0;
    virtual void rollback() = 0;
    virtual bool permission(std::uint32_t mapping, TypeId, std::uint32_t value, bool all,
                            ScriptWork&) = 0;
    virtual bool timer(ScriptAction, std::uint32_t index, std::uint32_t value) = 0;
    virtual bool wave(std::string_view, std::string_view, std::uint32_t, ScriptWork&) {
        return false;
    }
    virtual bool money(std::uint32_t, std::int32_t) { return false; }
    virtual bool touch(const ScriptArea&, std::uint32_t, bool, ScriptWork&, bool&) { return false; }
    virtual std::uint32_t timer_seconds(std::uint32_t index) const = 0;
};
[[nodiscard]] formats::Result<ScriptProgram, ScriptError>
compile_script(std::string source, const ScriptEnvironment&, ScriptLimits = {});
[[nodiscard]] ScriptState make_script_state(const ScriptProgram&);
[[nodiscard]] std::uint32_t script_visits(std::uint32_t arg, std::uint32_t count);
// Returns false on existing or newly recorded fault. Caller must stop world time.
bool visit_script(const ScriptProgram&, ScriptState&, std::uint32_t arg, ScriptHost&);
} // namespace anno::sim
