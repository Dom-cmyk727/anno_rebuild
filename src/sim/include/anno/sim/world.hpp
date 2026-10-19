// The simulated world (spec: docs/sim/core.md, docs/sim/state.md). For now
// the clock, the PRNG, the immutable context a world is created with
// (configuration, rules, settings, seams and the type registry; stage S3) and
// the player table (stage S4). Islands, buildings and figures follow as their
// rules are recovered (PLAN M3, docs/sim/state.md §15). Once commands exist
// they become the only way to change it from outside; until then `clock` and
// `rng` are public for tests.
#pragma once

#include <cstdint>
#include <memory>

#include <anno/sim/clock.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/hash_tree.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/seams.hpp>
#include <anno/sim/settings.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

// Development counter until the stream is complete (stage S15), then FROZEN
// as 2 (the first released stream). Values 100, 101, ... are development
// versions: the counter is bumped with every change of the field list (every
// stage S1..S14 changes it), goldens made at a development version carry the
// stage and the version they were made at (docs/sim/state.md §14.3), and all
// of them are re-baselined in the one commit that sets the value to 2. 1 is
// the legacy core_hash. After S15 every field-list change is version 3, 4, ...
// Stream at 100 (stage S1): header (the two versions), clock_rng.
// Stream at 101 (stage S3): header gains SimConfig, Rules, the registry digest
// and the seam names; new scope settings.
// Stream at 102 (stage S4): new scope players (after settings).
inline constexpr std::uint32_t kStateHashVersion = 102;
/// The PRNG timeline: which original draw sites exist in the sim (docs/sim/
/// state.md §6). Porting a site is a recorded bump, never a silent desync.
inline constexpr std::uint32_t kPrngTimelineVersion = 1;

struct StateIO;

class World {
  public:
    /// An empty world: the original configuration, default rules and settings,
    /// no seams installed, and the empty type registry (docs/sim/state.md
    /// §4.14). World::create (S5) builds the real ones.
    World();
    /// A world whose PRNG cursor is `seed mod table size` (the host's game
    /// seed, D: AnnoFrame.dll 0x1009b978 → AnnoScene.dll 0x10002e40).
    explicit World(std::uint32_t seed);

    WorldClock clock;
    WorldRng rng;

    [[nodiscard]] const WorldSettings& settings() const { return settings_; }
    [[nodiscard]] const SimConfig& config() const { return config_; }
    [[nodiscard]] const Rules& rules() const { return rules_; }
    [[nodiscard]] const Seams& seams() const { return seams_; }
    [[nodiscard]] const TypeRegistry& types() const { return *types_; }
    [[nodiscard]] const PlayerTable& players() const { return players_; }

    /// Compile-time canary (never called): binds every data member of World.
    /// A member added without extending this line fails to compile here,
    /// which is the reminder to give it a line in fields(World)
    /// (state_fields.hpp) and to bump kStateHashVersion, so it cannot be left
    /// outside hash and save (docs/sim/state.md §14.4).
    static constexpr void field_list_canary(const World& w) {
        [[maybe_unused]] const auto& [clock_member, rng_member, settings_member, config_member,
                                      rules_member, seams_member, types_member, players_member] = w;
    }

    /// Advances the world by `game_ms` (original order, D: AnnoWelt.dll
    /// 0x100ae040: islands, timed events, figures, players, timers, 30 s and
    /// 7.5 min events; only the clock and the player table (step 4) exist yet).
    /// `game_ms` == 0 is not special-cased: the original adds 0 and still runs
    /// every subsystem. Returns the new tick.
    std::uint64_t advance(std::uint32_t game_ms);

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

  private:
    friend struct StateIO;
    friend struct WorldTestAccess;

    WorldSettings settings_;
    SimConfig config_;
    Rules rules_;
    Seams seams_;
    RegistryRef types_; // never null, also after a move (types.hpp)
    PlayerTable players_;
};

} // namespace anno::sim
