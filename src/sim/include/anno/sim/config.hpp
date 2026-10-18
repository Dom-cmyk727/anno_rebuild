// Simulation configuration (spec: docs/sim/state.md §3).
//
// Stage S2 holds only what the slot list and the scheduler blocks need: the
// free-slot policy enum and the sanity bounds below. Stage S3 adds SimConfig,
// Rules, ListLayout and their validation to this file; the enum stays where the
// spec declares it (§3), so no include changes, and SimConfig::validate calls
// the predicates below instead of repeating their bounds.
#pragma once

#include <cstdint>

namespace anno::sim {

/// Sanity bound for SimConfig::list_min_growth and SimConfig::list_initial_size
/// (S3 validates both against it). Reason: both reach `vector::resize` on the
/// first insert, so a hostile config or save with 0xFFFFFFFF would ask for
/// about 4 G elements (bad_alloc, which sim code must not throw, or an OOM
/// kill). 2^24 slots is far above any list a game builds (the original's lists
/// stop at 32 768, docs/sim/state.md §5.1). Path to raise it: S3 turns this
/// into the field SimConfig::list_size_bound (hashed with the rest); it is
/// never a type limit (slot ids are u32, see SlotList::grown_size).
inline constexpr std::uint32_t kListSizeBound = 1U << 24;

/// SimConfig::list_min_growth is valid when it is in 1..kListSizeBound (zero
/// would stall the growth rule of SlotList::insert, F22).
constexpr bool valid_list_min_growth(std::uint32_t v) {
    return v >= 1 && v <= kListSizeBound;
}

/// SimConfig::list_initial_size is valid when it is in 1..kListSizeBound (at
/// least one element: check_invariants requires n >= 1, A7).
constexpr bool valid_list_initial_size(std::uint32_t v) {
    return v >= 1 && v <= kListSizeBound;
}

/// Default of the per-call dispatch cap of pop_due (S3: Rules::
/// events_max_dispatch, hashed). Reason: with Rules::events_drain_new a handler
/// that always enqueues a delay-0 event would keep the loop running forever;
/// 2^20 events in one advance is far above anything a game produces (the
/// queue holds a few hundred). Reaching the cap is reported, not an error
/// that stops the world.
inline constexpr std::uint64_t kEventsMaxDispatchDefault = 1ULL << 20;

/// Which null slot of a building list a new element takes (decision D1, filed
/// as OQ-036; docs/sim/state.md §5.2). `Original` is the faithful default.
enum class SlotPolicy : std::uint8_t {
    /// The first null slot at or after (last stored slot + 1), wrapping round
    /// (D: AnnoWelt.dll 0x10026a00): a freed slot is reused only when this scan
    /// reaches it.
    Original,
    /// The lowest-numbered null slot (opt-in; the list still never compacts).
    LowestFree,
};

/// Whether `p` is one of the enumerators (a value cast from a save or a config
/// file may be anything; Rules::validate rejects the others, so the policy test in
/// SlotList::insert never sees one; any other value is read as Original there).
constexpr bool valid_slot_policy(SlotPolicy p) {
    return p == SlotPolicy::Original || p == SlotPolicy::LowestFree;
}

} // namespace anno::sim
