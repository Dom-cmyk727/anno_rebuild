// The simulated world (spec: docs/sim/core.md). For now the clock and the
// PRNG; islands, buildings, figures and players follow as their rules are
// recovered (PLAN M3). Once commands exist they become the only way to change
// it from outside; until then the members are public for tests.
#pragma once

#include <cstdint>

#include <anno/sim/clock.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/rng.hpp>

namespace anno::sim {

class World {
  public:
    World() = default;
    /// A world whose PRNG cursor is `seed mod table size` (the host's game
    /// seed, D: AnnoFrame.dll 0x1009b978 → AnnoScene.dll 0x10002e40).
    explicit World(std::uint32_t seed) { rng.set_cursor(seed); }

    WorldClock clock;
    WorldRng rng;

    /// Advances the world by `game_ms` (original order, D: AnnoWelt.dll
    /// 0x100ae040: islands, timed events, sub-systems, 30 s and 7.5 min
    /// events; only the clock exists yet). Returns the new tick.
    std::uint64_t advance(std::uint32_t game_ms) { return clock.advance(game_ms); }

    /// Hash of the canonical state (docs/sim/core.md, "State hash").
    [[nodiscard]] std::uint64_t state_hash() const;
};

} // namespace anno::sim
