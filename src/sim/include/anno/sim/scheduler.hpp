// Scheduler building blocks of World::advance (spec: docs/sim/state.md §5.1;
// evidence: AnnoWelt.dll 0x10023780, 0x10023a50, 0x100969d0, 0x1009c4d0,
// 0x1009a8d5–0x1009a907, 0x1009aa4c–0x1009aa77, 0x10095d40, 0x100add90).
//
// Header-only and free of World, so they are proven (scheduler_blocks_test.cpp)
// before the world uses them. Everything here takes the world tick as its low
// 32 bits `t` and compares 16-bit stamps with the original's wrap
// (`static_cast<std::uint16_t>(t - stamp)`); counts and cursors are widened.
#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <anno/sim/config.hpp>
#include <anno/sim/events.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/slot_list.hpp>

namespace anno::sim {

/// Visits per call of a stamp-driven round-robin list: min(d·n/div + 1, n)
/// with d = (u16)(t − stamp). D 0x10023780: `imul 0x10624dd3`, `sar 5` (/500),
/// +1, compare with n; the same shape serves the city ring (n = 16, div 50, D
/// 0x100969d0), the player table (n = 32, div 50, D 0x1009c4d0), the island
/// sweeps (D 0x1008b760) and the island object manager (div 5000, D
/// 0x10095d40). The original multiplies in signed 32 bits, which is exact for
/// d·n < 2³¹ (lists of up to 32 768 slots); the product is u64 here (F3).
/// n = 0 gives 0 visits. Precondition: div != 0 (SimConfig::validate rejects
/// every zero divisor); a zero that got through is defined here (a hardware
/// divide trap is not): every slot is due, the result is n.
constexpr std::uint64_t visit_count(std::uint32_t t, std::uint16_t stamp, std::uint64_t n,
                                    std::uint32_t div) {
    if (div == 0) {
        return n;
    }
    const auto d = static_cast<std::uint64_t>(static_cast<std::uint16_t>(t - stamp));
    const std::uint64_t c = d * n / div + 1;
    return c < n ? c : n;
}

/// The unit registry's variant: clamp(d·n/div, 1, n) with no "+1". It is
/// phase B of the figure registry's tick (D 0x1009aa4c–0x1009aa77: `imul
/// 0x51eb851f`, `sar 4` = /50 = figure_b_div, n = (end − begin) >> 4 of 16-byte
/// entries, then compare with 1 and n). Phase A (D 0x1009a8d5–0x1009a907, `imul
/// 0x10624dd3`, `sar 4` = /250 = figure_a_div, n = word [obj+0x36]) has the
/// same clamp, but it is the sector re-bucketing, derived spatial data
/// (docs/sim/state.md §4.9), and does not use this function. Against
/// visit_count it is one smaller wherever d·n/div is at least 1 and below n,
/// and equal (1) where d·n/div is 0 (for n >= 1). n = 0 gives 0 (the registry
/// skips an empty list). Precondition: div != 0; a zero that got through gives
/// n, as in visit_count.
constexpr std::uint64_t visit_count_clamped(std::uint32_t t, std::uint16_t stamp, std::uint64_t n,
                                            std::uint32_t div) {
    if (div == 0) {
        return n;
    }
    const auto d = static_cast<std::uint64_t>(static_cast<std::uint16_t>(t - stamp));
    std::uint64_t c = d * n / div;
    if (c < 1) {
        c = 1;
    }
    return c < n ? c : n;
}

/// One scheduled list: `visit(T&)` is called for each non-null slot reached
/// and the cursor ends where the original's does (D: 0x10023810, 0x100238a0).
/// n is the vector length including the null holes, read before the loop; the
/// slot is looked up again in every iteration (the original re-reads the
/// vector start each time). A visit must not insert or remove elements of
/// this list (no tick does; docs/sim/state.md §5.5).
///
/// A cursor at or beyond n is an inconsistent state (A8: the original does not
/// clamp it, and indexes outside the vector). Nothing is visited then and the
/// list is left as it is, so the state stays visible to check_invariants and
/// the call is defined.
template <class T, class Visit>
void run_slot_list(SlotList<T>& l, std::uint16_t stamp, std::uint32_t t, std::uint32_t div,
                   Visit&& visit) {
    const std::uint64_t n = l.slots.size();
    if (n == 0 || l.cursor >= n) {
        return;
    }
    std::uint64_t cur = l.cursor;
    for (std::uint64_t i = visit_count(t, stamp, n, div); i > 0; --i) {
        if (cur < l.slots.size() && l.slots[static_cast<std::size_t>(cur)]) {
            visit(*l.slots[static_cast<std::size_t>(cur)]);
        }
        if (++cur >= n) {
            cur -= n;
        }
    }
    l.cursor = static_cast<std::uint32_t>(cur);
}

/// The lists of one city in scheduling order (D: 0x10023a50, which calls
/// 0x10023810 per list): every list named by `schedule`, in that order, gets
/// run_slot_list with the same stamp and `div`; `visit(ListId, T&)` is called
/// for the elements reached. The ONE shared `stamp` is written after the last
/// list (D: index +0x04), so every list sees the same d. A list that is not in
/// `schedule` (the original's stalls) is never visited, and an id beyond
/// `lists` is skipped (ListLayout::validate keeps schedules in range).
template <class T, class Visit>
void run_scheduled_lists(std::vector<SlotList<T>>& lists, std::span<const ListId> schedule,
                         std::uint16_t& stamp, std::uint32_t t, std::uint32_t div, Visit&& visit) {
    for (const ListId lid : schedule) {
        if (lid >= lists.size()) {
            continue;
        }
        run_slot_list(lists[lid], stamp, t, div, [&](T& item) { visit(lid, item); });
    }
    stamp = static_cast<std::uint16_t>(t);
}

/// What one pop_due call did.
struct PopResult {
    /// Events handed to the dispatch.
    std::uint64_t dispatched = 0;
    /// True when the loop stopped at `max_dispatch` with another due event
    /// still in reach; that event stays queued (it is served by the next
    /// call), so the stop is deterministic. World::check_invariants reports it
    /// (S12), a game never reaches it.
    bool capped = false;
};

/// Timed-event pop (D: 0x100add90). Events fall due when their due tick is at
/// most `now`. Each one is taken out of the queue and then `dispatch(const
/// TimedEvent&)` is called (D: 0x100addb3 copies the payload to the stack,
/// 0x100addc4 saves the next node, 0x100addd1..0x100addf2 unlink and 0x100addf5
/// frees the node, and only then the kind switch calls the handler; a handler
/// that cancels its own tile finds nothing). The next node is saved before the
/// dispatch, so
///   - an event the handler enqueues in front of the saved node (key order)
///     is not seen before the next advance,
///   - one behind it is (when it is due),
///   - an event enqueued when the saved node was the last is not seen, and
///   - a saved node the handler removed ends the loop (F19: the original would
///     follow a freed pointer).
/// With `drain_new` (Rules::events_drain_new, D3, OQ-038) the first node is
/// read again after every dispatch, so every due event is served, the new
/// ones too.
///
/// `max_dispatch` (fix, own design; Rules::events_max_dispatch, default
/// kEventsMaxDispatchDefault, OQ-038) bounds the events of one call. Without
/// it a handler that always enqueues a delay-0 event would never end the
/// drain_new loop (nor the saved-node loop when each handler enqueues behind
/// the saved node); the original has no such guard because only its
/// shipped handlers run. At the cap the loop stops and reports it.
template <class Dispatch>
PopResult pop_due(EventQueue& queue, std::uint64_t now, bool drain_new, Dispatch&& dispatch,
                  std::uint64_t max_dispatch = kEventsMaxDispatchDefault) {
    PopResult result;
    auto it = queue.q.begin();
    while (it != queue.q.end() && it->first.first <= now) {
        if (result.dispatched >= max_dispatch) {
            result.capped = true;
            break;
        }
        std::optional<EventKey> saved;
        const auto next = std::next(it);
        if (next != queue.q.end()) {
            saved = next->first;
        }
        const TimedEvent event = it->second;
        queue.q.erase(it);
        ++result.dispatched;
        dispatch(event);
        if (drain_new) {
            it = queue.q.begin();
        } else if (saved.has_value()) {
            it = queue.q.find(*saved);
        } else {
            break;
        }
    }
    return result;
}

} // namespace anno::sim
