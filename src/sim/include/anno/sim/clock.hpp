// Game time: the world clock and how much game time a step advances (spec:
// docs/sim/core.md, "Time"; evidence: docs/re/tick-loop.md §1–2, §6).
//
// The original advances a 64-bit millisecond counter by a variable amount
// per step; one world tick is 2 ms of game time (tick = ms >> 1; D:
// AnnoWelt.dll Advance 0x100ae040). Game speed is a percentage; the speeds
// the original lets a player set (50, 100, 200, 400 %) round to exact binary
// fractions in f32 under the default round-to-nearest mode (assumed, not
// traced; tick-loop.md §6), so the original's x87 step-size formulas have
// exact integer equivalents, used here.
#pragma once

#include <cstdint>
#include <optional>

namespace anno::sim {

/// The world clock.
struct WorldClock {
    /// Game milliseconds since the start (D: world +0x100/+0x104).
    std::uint64_t ms = 0;
    /// The tick as of the end of the last advance: Advance copies the new
    /// tick to world +0x108 after running everything (D: 0x100ae126,
    /// 0x100ae12d), so between advances it equals tick(); during an advance it
    /// still holds the tick from before it. The original stores ticks as 32
    /// bits; widened here (no wrap after 99 days of game time).
    std::uint64_t last_tick = 0;

    /// One tick = 2 ms of game time (D: world +0x10c = ms >> 1).
    [[nodiscard]] std::uint64_t tick() const { return ms >> 1; }
    /// Whole game seconds as the original reports them (world property 7:
    /// tick / 500 + offset; D: AnnoWelt.dll 0x100a3380, offset at +0x69d4).
    [[nodiscard]] std::uint64_t game_seconds(std::uint64_t offset = 0) const {
        return tick() / 500 + offset;
    }
    /// Starts an advance: adds `game_ms` to the counter and returns the new
    /// tick. The original adds the sign-extended i32 step; steps are never
    /// negative here. `last_tick` keeps the tick from before the advance, as
    /// the original's +0x108 does while the subsystems run (D: 0x100ae040
    /// stores the new tick at +0x10c first and +0x108 last).
    std::uint64_t begin(std::uint32_t game_ms) {
        ms += game_ms;
        return tick();
    }
    /// Ends an advance: `last_tick` becomes the current tick (D: 0x100ae126,
    /// 0x100ae12d).
    void end() { last_tick = tick(); }
    /// begin(), then end(), with no subsystem in between: advances by
    /// `game_ms` and returns the new tick. World::advance runs the subsystems
    /// between the two calls (docs/sim/state.md §5.3).
    std::uint64_t advance(std::uint32_t game_ms) {
        const std::uint64_t new_tick = begin(game_ms);
        end();
        return new_tick;
    }
};

/// The original's timing constants, as defaults (PLAN §0.1: configuration).
struct Timing {
    /// Longest frame the main loop passes on (D: AnnoFrame.dll 0x100087f0).
    std::uint32_t max_frame_ms = 1000;
    /// Largest single-player step (D: AnnoFrame.dll 0x1005b110, cap at
    /// 0x1005b12b). The original then advances in a loop of pieces of at most
    /// 100 ms (0x1005b13c–0x1005b172); with the cap that loop runs once. A
    /// rules option that raises the cap must keep advancing in ≤ 100 ms pieces.
    std::uint32_t max_single_step_ms = 100;
    /// Multiplayer: real milliseconds per step, steps per turn, real
    /// milliseconds per turn (D: 0x1005fe10, server 0x100559c0).
    std::uint32_t mp_step_real_ms = 10;
    std::uint32_t mp_steps_per_turn = 10;
    std::uint32_t mp_turn_real_ms = 100;
};

/// Whether `percent` is a speed the original steps the world at: the speeds a
/// player can set (keys F5–F8, likely; D: AnnoFrame.dll 0x100071b0), and 0
/// (paused) in single-player, where a paused frame advances nothing. In
/// multiplayer 0 is no step speed: the host pauses by stopping turns
/// (0x1005e230), so no step runs. 400 % only in single-player (the key
/// handler refuses it when net client slot +0xfc is non-zero, 0x100609c0 for
/// tNetClientNet).
bool is_original_speed(std::uint32_t percent, bool multiplayer);

/// Game milliseconds of a single-player step for a frame of `frame_ms` real
/// milliseconds at `percent` speed: trunc(dt · speed + 0.5), capped at
/// max_single_step_ms; nullopt when it is below 1 ms (nothing advances, e.g.
/// paused). The original computes this in x87 floats (D: AnnoFrame.dll
/// 0x1005b110); for the original speeds the integer form
/// (dt · percent + 50) / 100 is exact. nullopt also for any other speed
/// (not original; a rules option may allow more later); is_original_speed
/// tells the two cases apart. frame_ms is clamped to max_frame_ms as the main
/// loop does (with the 100 ms cap this never changes the result).
std::optional<std::uint32_t> single_player_step_ms(std::uint32_t frame_ms, std::uint32_t percent,
                                                   const Timing& timing = {});

/// Game milliseconds of one multiplayer step: trunc(speed · 10) = percent /
/// 10 for the original multiplayer speeds 50, 100, 200 (D: AnnoFrame.dll
/// 0x1005fef9, 0x1005ff81, 0x1008641c = 10.0f); nullopt for any other speed,
/// including 0 (pause stops turns instead).
std::optional<std::uint32_t> multiplayer_step_ms(std::uint32_t percent);

/// Chunks an incremental island sweep processes in one call:
/// min(chunks · Δtick / period + 1, chunks), with Δtick the 16-bit tick
/// difference since the previous call (D: AnnoWelt.dll 0x1008b760, cap at
/// 0x1008b7b3 and 0x1008b82a; period 2000 ticks for 0x1008b5f0, 15000 for
/// island slot +0x194). The original computes in signed 32 bits (overflow
/// above about 32,768 chunks); widened here. period 0 is not original and
/// processes all chunks.
std::uint64_t sweep_chunks(std::uint32_t chunks, std::uint16_t delta_tick, std::uint32_t period);

} // namespace anno::sim
