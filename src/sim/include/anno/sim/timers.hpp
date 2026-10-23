// World-level time after the player table: the script timers, the monopoly table and
// the 7.5 minute event, steps 5, 6 and 7 of World::advance (spec: docs/sim/state.md
// §4.10, §5.3, §5.10, stage S12; evidence: AnnoWelt.dll SKTIMER tick 0x1009cd30,
// monopoly refresh 0x1009d2a0 with its owner query 0x1009d210, 7.5 minute event
// 0x100ae140).
//
// The functions here work on the state they are given and on read-only views of the
// rest; World::advance (scheduler.cpp) calls them in the original order and turns
// what they report into UI events. Nothing here keeps state of its own.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <anno/sim/config.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/settings.hpp>

namespace anno::sim {

/// D: SKTIMER, the object world +0x69c8 points to (vtable 0x100cb8c4, tick 0x1009cd30,
/// called at 0x100ae103; the offsets below are the object's). Timer 0 counts
/// down, the others count up; the counters are in ticks (seconds = counter / 500).
/// The script engine sets and reads them (BigBrother ops 0x10011ed0..0x10013dab, O-16,
/// OQ-054); until it is ported only a loaded or test-made state starts a timer.
struct ScriptTimers {
    /// D: +0x04, the tick of the last call.
    std::uint32_t last_tick = 0;
    /// D: bytes at +0x08; non-zero = running. Size SimConfig::script_timers.
    std::vector<std::uint8_t> active;
    /// D: u32 at +0x10. Size SimConfig::script_timers.
    std::vector<std::uint32_t> counter;
};

/// One entry of the monopoly table: when (game seconds) and whom it last named.
struct MonopolyEntry {
    std::uint32_t time_s = 0;
    /// D: a player pointer, 0 = none.
    std::optional<PlayerId> player;
};

/// D: MONOPOL, world +4 (entries of 8 bytes), accumulator +0xe8.
struct MonopolyTable {
    /// Size SimConfig::monopoly_entries.
    std::vector<MonopolyEntry> entries;
    /// D: +0xe8, milliseconds. Hashed and saved in the `ids` scope (docs/sim/state.md
    /// §9.3), not with the entries.
    std::uint32_t acc = 0;
};

/// SimConfig::script_timers stopped timers at 0, last_tick 0.
[[nodiscard]] ScriptTimers make_script_timers(const SimConfig& cfg);
/// SimConfig::monopoly_entries empty entries, accumulator 0.
[[nodiscard]] MonopolyTable make_monopoly_table(const SimConfig& cfg);

/// The timer tick (D 0x1009cd30), `t` the low 32 bits of the world tick: dt = t −
/// last_tick (u32); every running timer k > 0 counts up by dt (u32, wrapping like the
/// original's `add`); the running timer 0 counts down by dt and stops at 0. The original
/// stops it only when the subtraction gives exactly 0 (0x1009cd51 `sub` / `jne`): an
/// overshoot underflows to a huge u32 and keeps it running for about 99 days of game
/// time. Here an overshoot clamps at 0 and stops the timer (F5, a fix). last_tick
/// becomes t. Returns true when timer 0 stopped in this call (the world reports it as
/// a TimerExpired UI event). A table whose vectors differ in size (check_invariants
/// refuses it) is ticked over the shorter length.
bool timers_tick(ScriptTimers& timers, std::uint32_t t);

/// The owner the monopoly table credits an island to (D 0x1009d210): the owner of the
/// only city of the island, in city-number order, whose player has Volk 1; none when
/// no city qualifies or when a second one does (a second city of the same player
/// counts too: 0x1009d266 returns 0 as soon as a second qualifying city is found).
/// A city whose owner is not in the table does not qualify.
[[nodiscard]] std::optional<PlayerId> monopoly_owner(const Island& island,
                                                     const PlayerTable& players);

/// The body of the refresh (D 0x1009d2d4..0x1009d40a): for every raw slot s in 0 ..
/// monopoly_raw_slots − 1 and every island in table order (null slots skipped) whose
/// climate mask has bit s: without an owner (monopoly_owner) entry s is cleared and the
/// island scan for s ends; with an owner that differs from the entry's player the entry
/// becomes {game_seconds, owner}. An island whose climate has no mask (above the
/// table; check_invariants refuses it) is skipped like the original's `ja` at
/// 0x1009d32b. The second loop of the original (entries 20 .. 27 from island object
/// queries) is not modelled (O-24).
void monopoly_body(MonopolyTable& table, const std::vector<std::optional<Island>>& islands,
                   const PlayerTable& players, const SimConfig& cfg, std::uint32_t game_seconds);

/// The 30 s refresh with its gate (D 0x1009d2a0..0x1009d2ce). Faithful: the body runs
/// when the accumulator is 0 or at least monopoly_ms, and only the body adds dt_ms to
/// it, so from a fresh accumulator the body runs on the first call and then never
/// again unless that first step was itself monopoly_ms or longer (D5, OQ-040). With
/// Rules::monopoly_every_30s: acc += dt_ms; below monopoly_ms nothing happens, else
/// acc −= monopoly_ms and the body runs. Returns whether the body ran. No PRNG draw.
bool monopoly_refresh(MonopolyTable& table, std::uint32_t dt_ms, const SimConfig& cfg,
                      const Rules& rules, const std::vector<std::optional<Island>>& islands,
                      const PlayerTable& players, std::uint32_t game_seconds);

/// The 7.5 minute event (D 0x100ae140), in exactly this order: acc += dt_ms (u32);
/// below event750_ms nothing happens; acc becomes 0 (Rules::event750_keep_overshoot:
/// acc − event750_ms, D10, OQ-045); world property 6 bit 0x80 ends it without a draw;
/// draw #1 (Event750Gate) and r1 % event750_modulus != 0 ends it; the base game (no
/// add-on flag, property 14 bit 2) ends it with the draw consumed; draw #2
/// (Event750Island) picks the slot r2 % (island table length, null slots included);
/// an empty table ends it (F6: the original divides by zero) and a null slot ends it.
/// Returns the island hit, for which the world posts the message-manager event 0x15a
/// at the island's centre (a UI event; its consumer is the script engine, O-16).
[[nodiscard]] std::optional<IslandId> event750(std::uint32_t& acc, std::uint32_t dt_ms,
                                               const SimConfig& cfg, const Rules& rules,
                                               const WorldSettings& settings,
                                               const std::vector<std::optional<Island>>& islands,
                                               DrawSource& draw);

/// The structural facts of the timers (docs/sim/state.md §9.6): both vectors have
/// SimConfig::script_timers entries. Empty when they hold, else one message
/// ("timers.active: ...").
[[nodiscard]] std::string validate_timers(const ScriptTimers& timers, const SimConfig& cfg);

/// The structural facts of the monopoly table: SimConfig::monopoly_entries entries,
/// and every player an entry names is a slot of the player table (below
/// SimConfig::player_slots). Empty when they hold, else one message
/// ("monopoly.entries: ...").
[[nodiscard]] std::string validate_monopoly(const MonopolyTable& table, const SimConfig& cfg);

} // namespace anno::sim
