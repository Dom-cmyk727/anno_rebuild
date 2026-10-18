// The simulated world (spec: docs/sim/core.md, docs/sim/state.md). For now
// the clock and the PRNG; islands, buildings, figures and players follow as
// their rules are recovered (PLAN M3, docs/sim/state.md §15). Once commands
// exist they become the only way to change it from outside; until then the
// members are public for tests.
#pragma once

#include <cstdint>

#include <anno/sim/clock.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/hash_tree.hpp>
#include <anno/sim/rng.hpp>

namespace anno::sim {

// Development counter until the stream is complete (stage S15), then FROZEN
// as 2 (the first released stream). Values 100, 101, ... are development
// versions: the counter is bumped with every change of the field list (every
// stage S1..S14 changes it), goldens made at a development version carry the
// stage and the version they were made at (docs/sim/state.md §14.3), and all
// of them are re-baselined in the one commit that sets the value to 2. 1 is
// the legacy core_hash. After S15 every field-list change is version 3, 4, ...
// Stream at 100 (stage S1): header (the two versions), clock_rng.
inline constexpr std::uint32_t kStateHashVersion = 100;
/// The PRNG timeline: which original draw sites exist in the sim (docs/sim/
/// state.md §6). Porting a site is a recorded bump, never a silent desync.
inline constexpr std::uint32_t kPrngTimelineVersion = 1;

class World {
  public:
    World() = default;
    /// A world whose PRNG cursor is `seed mod table size` (the host's game
    /// seed, D: AnnoFrame.dll 0x1009b978 → AnnoScene.dll 0x10002e40).
    explicit World(std::uint32_t seed) { rng.set_cursor(seed); }

    WorldClock clock;
    WorldRng rng;

    /// Compile-time canary (never called): binds every data member of World.
    /// A member added without extending this line fails to compile here,
    /// which is the reminder to give it a line in fields(World)
    /// (state_fields.hpp) and to bump kStateHashVersion, so it cannot be left
    /// outside hash and save (docs/sim/state.md §14.4).
    static constexpr void field_list_canary(const World& w) {
        [[maybe_unused]] const auto& [clock_member, rng_member] = w;
    }

    /// Advances the world by `game_ms` (original order, D: AnnoWelt.dll
    /// 0x100ae040: islands, timed events, sub-systems, 30 s and 7.5 min
    /// events; only the clock exists yet). Returns the new tick.
    std::uint64_t advance(std::uint32_t game_ms) { return clock.advance(game_ms); }

    /// Hash of the canonical state, version kStateHashVersion: hierarchical,
    /// over the one field list of state_fields.hpp (docs/sim/state.md §9).
    [[nodiscard]] std::uint64_t state_hash() const;
    /// The same value, with the scopes down to `depth` recorded in `tree`
    /// (docs/sim/state.md §9.4).
    [[nodiscard]] std::uint64_t state_hash(HashTree& tree, std::uint32_t depth) const;
    /// The legacy v1 hash (docs/sim/core.md): FNV-1a 64 of `clock.ms`,
    /// `clock.last_tick`, the PRNG table length, its digest and the cursor. Kept
    /// so the old golden stays checkable; state_hash() begins with the same
    /// five fields inside its clock_rng scope.
    [[nodiscard]] std::uint64_t core_hash() const;
};

} // namespace anno::sim
