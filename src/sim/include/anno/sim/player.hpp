// The player table: money, upkeep, statistics and bankruptcy (spec:
// docs/sim/state.md §4.5, §5.10; evidence: AnnoWelt.dll player table tick
// 0x1009c4d0, per-player tick 0x1001b900, upkeep 0x1001b840).
//
// Per advance the world visits `min(d·32/50 + 1, 32)` players round-robin
// (d = ticks since the table's own 16-bit stamp), not all of them, so upkeep
// and bankruptcy timing depend on the step size (D6, OQ-041;
// Rules::player_table_full_tick visits every slot instead). A visited player
// runs, in this order: the group-table pass (seam), the upkeep booking, the
// bankruptcy test and the defeat pass (seam).
//
// Everything here is integer arithmetic on state owned by the table; money is
// i64 (the original's i32, F15). The two per-player seams of the original (the
// group table 0x10062a40 and the defeat pass 0x10020550) need the TickContext
// and the SerialWorld of stage S7, so the table functions take them as
// PlayerHooks: plain function pointers on a Player that World::advance binds from
// Seams::player_groups and Seams::defeat (stage S7, scheduler.cpp); a seam without
// a function leaves its hook null, which is the documented default (it does
// nothing).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <anno/sim/config.hpp>
#include <anno/sim/ids.hpp>

namespace anno::sim {

/// D: player +0x28d, an open byte: the property setter (0x1001be0d..0x1001be1f)
/// stores any value, the record loader maps 7 to 8, and the tick only tests
/// "non-zero" (0x1001b925). Alive (0), Bankrupt (2) and Defeated (3) are the
/// names the sim knows; any other byte is kept as it is, read and written
/// verbatim, and counts as "not alive" (OQ-058 lists 1, 7 and 8). Bankrupt and
/// Defeated are final: nothing in the table tick leaves them.
enum class PlayerState : std::uint8_t { Alive = 0, Bankrupt = 2, Defeated = 3 };

/// The research table of one player (D: player +0xb0 object, 65 entries).
struct ResearchState {
    /// Bit 0 researchable, bit 1 researched (D: 0x1001da19–0x1001db4a).
    std::vector<std::uint8_t> state;
    /// Words at +0xb4 + 2i (progress << 8).
    std::vector<std::uint16_t> progress;
    [[nodiscard]] bool researched(std::uint32_t id) const {
        return id < state.size() && (state[id] & 2U) != 0;
    }
};

/// "No colour": the original's 0xff of the byte at player +0xa9, widened (the
/// importer of a scenario or save maps 0xff to this value).
inline constexpr std::uint32_t kNoColour = 0xFFFFFFFFU;

struct Player {
    /// The player's slot in the table (original u8).
    PlayerId index;
    /// D: +0x78. 8 and 11 human, 12 computer, 14 natives, 15 pirates, 16 Venice.
    std::uint32_t kind = 8;
    /// D: +0x7a (OQ-033, resolved): status icons only for Volk 1.
    std::uint32_t volk = 1;
    /// D: +0x74, i32 in the original, initial 10000 (F15).
    std::int64_t money = 10000;
    /// D: +0x88 / +0x8c, running per-minute sums of owned units.
    std::uint32_t upkeep_a = 0, upkeep_b = 0;
    /// D: +0xa2 (u16 in the original). u32 because upkeep_slices is a u32
    /// configuration value; the remainder is below it (F3), which
    /// validate_players enforces on a loaded state (a larger value is a state no
    /// play reaches and would book a different charge).
    std::uint32_t upkeep_rem = 0;
    /// D: +0xa4, a 16-bit stamp (kept: the gate compares a 16-bit difference).
    std::uint16_t upkeep_stamp = 0;
    /// D: +0x80 / +0x84, decaying statistics.
    std::uint32_t stat_a = 0, stat_b = 0;
    /// D: +0x90.
    std::uint32_t flags = 1;
    /// D: +0xa0 (bit 0x08: immune to random city events).
    std::uint32_t flags_a0 = 0;
    /// D: +0xa8 and +0xa9 (u8 in the original; the constructor at 0x10021600
    /// stores 0 at +0xa8 and 0xff at +0xa9). Widened to u32 because the player
    /// table size is configuration (up to kSlotTableBound slots), so a colour slot
    /// can exceed 255; kNoColour (the original's 0xff) means "none".
    std::uint32_t colour_slot = 0, default_colour = kNoColour;
    PlayerState state = PlayerState::Alive;
    /// D: +0x290; 0 = the grace period is not running.
    std::uint32_t bankrupt_deadline = 0;
    /// D: +0x295, set by Remove of a market-group building (D: 0x100255c0).
    bool defeat_pending = false;
    /// O-9: the owner slot +0x9c(7) waives the build cost.
    bool free_build = false;
    /// Bytes; no encoding is assumed.
    std::vector<std::uint8_t> name;
    ResearchState research;
};

struct PlayerTable {
    /// Size SimConfig::player_slots; a null slot holds no player. The slot is
    /// the player's index.
    std::vector<std::optional<Player>> slots;
    /// D: +0x88, a 16-bit stamp: the table's own visit count is driven by it.
    std::uint16_t stamp = 0;
    /// D: +0x8c, the round-robin cursor (widened from the original's byte);
    /// below slots.size() in every consistent state.
    std::uint32_t cursor = 0;
    // The +0x8a stamp and the +0x8e ring drive 0x10001020, a bare `ret 8`: no
    // behaviour, not modelled.

    /// The player in slot `id`, or nullptr (invalid id, slot out of range, empty
    /// slot): a caller treats that as "gone", never as an error.
    [[nodiscard]] const Player* find(PlayerId id) const {
        if (!id.valid() || id.v >= slots.size() || !slots[id.v].has_value()) {
            return nullptr;
        }
        return &*slots[id.v];
    }
    [[nodiscard]] Player* find(PlayerId id) {
        if (!id.valid() || id.v >= slots.size() || !slots[id.v].has_value()) {
            return nullptr;
        }
        return &*slots[id.v];
    }
};

/// A player with the original's initial values in slot `index`: 10000 gold,
/// Volk 1, kind 8, Alive, colour slot 0 and no default colour, the research
/// table sized from SimConfig::research_entries (all entries zero: the 65 initial
/// state bytes of the table at 0x100e0bf6 were read and are all zero; the
/// constructor's other stores, player +0xa6 = 0xffff, +0x178..+0x1aa and +0x28c =
/// 0x1f, are not modelled, OQ-059).
/// Precondition: `cfg` passed SimConfig::validate (research_entries is bounded by
/// kEntryTableBound); with an unvalidated size the allocation may throw. The
/// world builder (S5 World::create) validates the configuration first.
[[nodiscard]] Player make_player(PlayerId index, const SimConfig& cfg);

/// A table of SimConfig::player_slots empty slots, stamp and cursor 0.
/// Precondition: `cfg` passed SimConfig::validate (player_slots is bounded by
/// kSlotTableBound); with an unvalidated size the allocation may throw.
[[nodiscard]] PlayerTable make_player_table(const SimConfig& cfg);

/// The structural facts of a player table that the tick relies on (docs/sim/
/// state.md §9.6, the player part): the slot count is SimConfig::player_slots,
/// the cursor is inside the table, every player's index is its slot, its
/// research table has SimConfig::research_entries entries in both columns, and
/// its upkeep remainder is below SimConfig::upkeep_slices.
/// Empty when they hold, else one message ("players.slots: ..."). The state
/// stream runs it on what it just read.
[[nodiscard]] std::string validate_players(const PlayerTable& table, const SimConfig& cfg);

/// The two per-player passes of the original that this stage does not own: a
/// null function is the documented default (does nothing). `ctx` is the
/// caller's own context (not state).
struct PlayerHooks {
    using Fn = void (*)(void* ctx, Player&);
    /// Step 1 of a player tick, before the upkeep: the group table (30 slots
    /// with a 500-tick gate inside; D: 0x10019390 → 0x10062a40, O-20).
    Fn groups = nullptr;
    void* groups_ctx = nullptr;
    /// Step 4, for an Alive player of the table whose `defeat_pending` was set;
    /// the flag is cleared before the call (D: 0x10020550, O-22).
    Fn defeat = nullptr;
    void* defeat_ctx = nullptr;
};

/// One player's tick (D: 0x1001b900), `t` the low 32 bits of the world tick, in
/// this order:
///  1. hooks.groups;
///  2. upkeep (D: 0x1001b840): when the 16-bit difference t − upkeep_stamp has
///     reached upkeep_period, S = upkeep_a + upkeep_b + upkeep_rem in u32
///     (wrapping), the money drops by S / upkeep_slices, the remainder S %
///     upkeep_slices is kept, the stamp becomes t (NOT stamp + period), and
///     stat_a and stat_b become stat · stat_decay_num / stat_decay_den in u32
///     (the product wraps);
///  3. for a player whose index is not SimConfig::neutral_player and whose state
///     is Alive (the jump at 0x1001b927 skips steps 3 and 4 otherwise): at
///     money <= bankrupt_money the grace period starts (deadline = u32(t +
///     bankrupt_grace_ticks), 0 meaning "not running") and, once running, ends
///     with state Bankrupt when t > deadline (unsigned); above the limit the
///     deadline is cleared. The start of the period and the end test are
///     exclusive within one visit (the end is tested only when the period was
///     already running). Rules::bankrupt_deadline_wrap_safe compares the signed
///     32-bit difference instead and never stores 0 as a running deadline;
///     Rules::stat_decay_wide computes the decay of step 2 in 64 bits (OQ-057);
///  4. in the same block, defeat_pending is cleared and hooks.defeat runs.
/// A zero upkeep_slices or stat_decay_den (SimConfig::validate rejects them) is
/// defined here instead of a divide trap: the booking then charges nothing and
/// keeps the remainder, the decay keeps the statistics. The money subtraction
/// saturates at the lowest i64 (a loaded value near it cannot overflow; F28).
void player_tick(Player& p, std::uint32_t t, const SimConfig& cfg, const Rules& rules,
                 const PlayerHooks& hooks = {});

/// The table tick (D: 0x1009c4d0): visits `visit_count(t, stamp, n, player_div)`
/// = min(d·n/50 + 1, n) slots from the cursor (every slot with
/// Rules::player_table_full_tick), each non-null one with player_tick, advances
/// the cursor modulo n per slot visited, and stores the stamp t last. n is the
/// slot count read before the loop; an empty table or a cursor at or beyond n
/// (an inconsistent state, validate_players) visits nothing and leaves the
/// cursor. The AI of the original runs between the loop and the stamp (D:
/// Seams::ai, O-1); it is not called here (stage S8).
void player_table_tick(PlayerTable& table, std::uint32_t t, const SimConfig& cfg,
                       const Rules& rules, const PlayerHooks& hooks = {});

} // namespace anno::sim
