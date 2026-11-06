// UnloadCmd: a ship's hold into the owner's warehouse city (MAP-CARGO-1 U2); LoadCmd: the owner's
// warehouse city into a ship's hold (MAP-CARGO-2 L2). Contract:
// docs/sim/ship-cargo.md (sections 1-11 unload, 12 load); evidence: docs/re/ship-cargo.md;
// independent reference: tools/cargo_ref.py.
//
// Shape (as Founding): validation reads the world and decides, the transfer plan is computed from
// the live hold and store before any write, and the commit is two stores of precomputed values
// that cannot fail. A refusal changes no serialized byte, no PRNG draw, no gold, cargo or stock;
// its only side effect is the host-facing CommandRejected event of Commands::apply.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <anno/sim/apply.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/reject.hpp>

namespace anno::sim {

class World;

struct CargoDestination {
    Reject status = Reject::None;
    CityRef city;
};
struct ShipCargo {
    /// Shared read-only warehouse/city/owner/range validation in manual failure
    /// order. Only assignment omits range; each actual transfer checks it live.
    static CargoDestination destination(const World&, const Figure&, BuildingId, bool range);
    /// Validates (atomically) and applies one UnloadCmd. Total. On success `records[0]` carries the
    /// warehouse and `transferred_88`.
    static ApplyResult unload(World& w, const Command& command, const UnloadCmd& cmd,
                              const InAdvance* in_advance = nullptr);

    /// Validates (atomically) and applies one LoadCmd (docs/sim/ship-cargo.md section 12.4): the
    /// plan (hold, amount) is computed from the live hold and store before any write; the commit
    /// debits the store and credits the hold by that same amount. Total. On success `records[0]`
    /// carries the warehouse and `transferred_88`.
    static ApplyResult load(World& w, const Command& command, const LoadCmd& cmd,
                            const InAdvance* in_advance = nullptr);

    /// The hold a load goes to (section 12.4 step 12): the preferred hold when it holds `goods`
    /// and has at least one whole ton free, else the same-goods hold with the most room (strictly
    /// greater, lowest index on ties) or, when none has room, the first empty hold (goods 0 and
    /// amount 0), over the first `limit` holds. nullopt when there is none. Pure.
    [[nodiscard]] static std::optional<std::size_t>
    resolve_hold(const std::vector<ShipHold>& holds, std::size_t limit, std::uint32_t capacity_88,
                 std::size_t preferred, GoodsId goods);

    /// The amount a request moves: min(tons * 256, hold amount, free), all in 1/256 ton, formed in
    /// u64 (tons < 2^32 so the product is below 2^40). Pure; the caller has checked that each of
    /// the three is non-zero for a transfer to happen.
    [[nodiscard]] static std::uint64_t amount_88(std::uint32_t tons, std::uint64_t hold_amount_88,
                                                 std::uint64_t free_88);

    /// The range test of section 5 for a ship tile (tx, ty) and a footprint {x0, y0, w, h}: per
    /// axis the distance to the nearer edge tile (0 inside), in range iff dy <= r and
    /// dx <= half[dy]. Pure.
    [[nodiscard]] static bool footprint_in_range(std::int64_t tx, std::int64_t ty, std::int64_t x0,
                                                 std::int64_t y0, std::int64_t w, std::int64_t h,
                                                 const CargoRules& rules);
};

} // namespace anno::sim
