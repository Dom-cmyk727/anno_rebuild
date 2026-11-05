// The Inn (MAP-POP-CLOSE-2 K1): its tick, take, unload and the priority it presents to the
// workshop carrier. D: AnnoWelt.dll vtable 0x100c96d8; tick 0x100755f0, take 0x100756e0, unload
// 0x100756d0, availability 0x10075940, base priority 0x10072870. Evidence: docs/re/population.md
// section 18. Units are 8.8 tons (256 = 1 t). `tavern` and `taken` are the original's u16 values:
// every operation wraps modulo 65536 as the original's 16-bit adds do (widened storage, same
// arithmetic; a value above 0xffff is an invariant violation).
#pragma once

#include <cstdint>
#include <optional>

#include <anno/sim/building.hpp>

namespace anno::sim {

/// The tick's gate period (the compare constant 0x1388 of `cmp cx, 0x1388`).
inline constexpr std::uint32_t kInnGatePeriod = 5000;
/// The stock below which a carrier is always sent (`cmp ax, 0x100`): 1 t.
inline constexpr std::uint32_t kInnRefillStock = 0x100;

struct InnTickResult {
    /// The 5000-tick gate opened (the stamp advanced and the statistic decayed).
    bool stepped = false;
    /// The Inn sends an alcohol carrier (job 0x33) now.
    bool send = false;
};

/// D 0x100755f0. `tick` is the world tick (only its low 16 bits matter, as in the original's 16-bit
/// compare); `limit` is the type's max figures of group 0 (`+0xb8(0)`), `inn.carriers_out` the
/// carriers on their way. Order: gate, `last += 5000` (u16), the statistic decays by
/// (31 * taken) / 32 BEFORE the switched-off test, flags bits 4/5 (`inn.suspended`) stop, a signed
/// `limit > carriers_out` is needed, then send iff `tavern < 0x100 || tavern < (taken >> 1)` with
/// the decayed statistic. Does not touch `carriers_out` (the caller raises it when it spawns).
[[nodiscard]] InnTickResult inn_step(Inn& inn, std::uint32_t tick, std::int32_t limit);

/// D 0x100756e0 (the part on the Inn): n = min(request, stock) as SIGNED values (a negative request
/// stays negative), `taken += n`, `tavern -= n`, both wrapped to 16 bits. Returns n.
std::int64_t inn_take(Inn& inn, std::int64_t request);

/// D 0x100756d0: `tavern += amount` as a 16-bit add (no clip, no x16).
void inn_unload(Inn& inn, std::uint32_t amount);

/// D 0x10072870, the BASE priority of a sender whose stock proxy is its tavern stock (`+0xbc`):
/// steps = stock / need; < 30 -> 0x80, < 60 -> 0x60, else -1. nullopt when `need` is 0 (the
/// original traps in `idiv`; the registry refuses such an Inn type, types.cpp, and the carrier
/// treats a missing value as "no priority").
[[nodiscard]] std::optional<std::int32_t> base_priority(std::uint32_t stock, std::uint32_t need);

} // namespace anno::sim
