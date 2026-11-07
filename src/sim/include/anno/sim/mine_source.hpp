// The source of a player-built mine (stream 135; docs/sim/salt-extraction.md sections 12 and 13).
//
// The original's deposits are island objects created from the OCCUR ore objects; this engine keeps
// the ore objects (Island::ore_objects) and the deposits a mine actually uses (Island::deposits,
// Mine::deposit) apart. A mine placed by a command binds one ore object here: a Deposit is made
// from it once (deduplicated by goods and tile, so every mine that reaches the same ore shares the
// one deposit and a teardown or rebuild never refills it), and the mine's `deposit` points at it.
// The loader never calls this: imported mines keep their (absent) deposit, by decision.
#pragma once

#include <cstdint>
#include <optional>

#include <anno/sim/building.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/ore_lookup_stats.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// The initial state of a deposit loaded from an OCCUR ENTRY2 (AnnoWelt deposit class
/// 0x100637a0, load method 0x100634e0): remaining = (u16 amount in tons) << 8; for an ore goods
/// id 1..8 with the reserve flag set and remaining < 0x64000 (1,600 t) it is reset to 0x7d000
/// (2,000 t). The flag is set by the constructor and never cleared by the load. Reference:
/// tools/mine_terrain_ref.py deposit_load, vectors fixtures/synthetic/sim/mine_terrain_vectors.txt.
struct DepositInit {
    std::uint32_t remaining = 0;
    bool reserve = true;
};
[[nodiscard]] DepositInit deposit_load(std::uint32_t amount_tons, GoodsId ore_goods,
                                       bool reserve = true);

/// The ore object a mine of type `t` at `area` would work: of the ore objects of its output ware in
/// reach (`deposit_in_reach`, the radius-7 table of the production step), the one with the largest
/// `deposit_remaining(deep)` of the prospective deposit (an existing deposit at the ore's tile
/// counts with its CURRENT remaining), the first in ore_objects order on a tie (a stand-in: the
/// original's query order is unread; no effect for salt). nullopt when none lies in reach or `t`
/// has no output ware. Read-only. ORIGINAL (re-verified 2026-11-07 from AnnoWelt 0x100787c0): the
/// reach is the object rectangle grown by 7 tiles with the circle half-widths {7,7,7,6,6,5,4,2} by
/// row distance (a distance above 7 is out), the ware match, and a later object replaces the best
/// only when its remaining amount is strictly larger. Discovery is not consulted (OQ-553: the
/// original's lookup does not read the discovery mask). The original runs this lookup once, when
/// the city is attached to a new mine (0x1006e299), and again only from the production step when a
/// bound deposit is spent or short (0x1006dfb4).
[[nodiscard]] std::optional<std::size_t>
find_ore_in_reach(const Island& island, const TypeRegistry& types, const BuildingType& t,
                  const MineArea& area, bool deep, OreLookupStats* stats = nullptr);

/// Binds the mine `b` (type `t`, payload Mine) to the ore found by find_ore_in_reach: appends the
/// deposit if the ore has none yet and returns its index. Nullopt when nothing is in reach: the
/// mine then stays unbound and idle (the original discards the result of its attach-time lookup and
/// the production step never searches without a bound deposit, 0x1006def0 to 0x1006e178). Sets
/// nothing: the caller stores the result in Mine::deposit.
[[nodiscard]] std::optional<std::size_t>
bind_mine_deposit(Island& island, const TypeRegistry& types, const Building& b,
                  const BuildingType& t, bool deep, OreLookupStats* stats = nullptr);

} // namespace anno::sim
