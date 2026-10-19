// The timed-event queue (spec: docs/sim/state.md §4.10, §5.7; evidence:
// AnnoWelt.dll queue at world +0x6a94, enqueue 0x100adef0, cancel 0x100ade40,
// pop 0x100add90, docs/re/tick-loop.md).
//
// Stage S2 declares the queue and its two operations because the scheduler
// block `pop_due` (scheduler.hpp) is proven against it; the dispatch inside
// World::advance, the seams and the script timers follow in stage S12.
#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <utility>

#include <anno/sim/ids.hpp>

namespace anno::sim {

/// Exactly two kinds exist (D: 0x100add90; enqueue sites 0x10063cf1,
/// 0x100943d5, 0x1003593e); the original silently drops any other.
enum class EventKind : std::uint8_t { TileCleanup = 0, DelayedHit = 1 };

/// The subtype of a tile event: kind 0 always, kind 1 for a hit on a tile (D:
/// 0x10063cf1, 0x100943d5; the only subtype cancel_tile accepts, 0x100ade79).
inline constexpr std::uint32_t kTileSubtype = 4;

struct TimedEvent {
    EventKind kind = EventKind::TileCleanup;
    /// Kind 0: 4. Kind 1: 2 (unit) or 4 (tile). The enqueue at 0x100adef0 also
    /// accepts subtype 1 (three f32 stored at payload +0x18/+0x1c/+0x20, D
    /// 0x100adf92..0x100adfa4), which this struct cannot represent: the three
    /// known enqueue sites pass 4 (kind 0, 0x10063cf1 and 0x100943d5) and a
    /// variable (kind 1, 0x1003593e); whether subtype 1 is ever enqueued is
    /// open (OQ-054), and a payload field is added if it is.
    std::uint32_t subtype = kTileSubtype;
    /// Kind 1: damage.
    std::uint32_t value = 0;
    /// Kind 0 and kind 1 subtype 4.
    TileXY tile;
    /// Kind 1 subtype 2.
    FigureId target;
    /// Payload +0x1c (2 = unit).
    std::uint32_t attacker_kind = 0;
    /// Payload +0x20.
    FigureId attacker;
};

/// The queue key: due tick, then the sequence number of the enqueue.
using EventKey = std::pair<std::uint64_t, std::uint64_t>;

struct EventQueue {
    /// Ordered by (due, seq): the original inserts a node after every node
    /// with `due <=` its own (D: 0x100adef0), so equal due ticks are served
    /// first come, first served.
    std::map<EventKey, TimedEvent> q;

    /// Queues `e` to fall due at `tick + delay` (u64 arithmetic; the original
    /// keeps 32-bit ticks, F-time) with the next sequence number of `ids`. The
    /// sequence numbers are assigned in the serial merge phase only, in island
    /// order (docs/sim/state.md §2.1). The counter must exceed every queued
    /// sequence number (the loader checks it): a clash would drop the event.
    /// A sum that does not fit in u64 is cut at 2^64 - 1 ("never, or last"; F35:
    /// a wrapped sum would fire the event at once). When the sequence numbers are
    /// used up (`next_event_seq` at 2^64 - 1, a state the loader accepts) nothing
    /// is queued and false is returned; the counter never wraps.
    bool enqueue(IdCounters& ids, std::uint64_t tick, std::uint64_t delay, TimedEvent e) {
        if (ids.next_event_seq == std::numeric_limits<std::uint64_t>::max()) {
            return false;
        }
        const std::uint64_t seq = ids.next_event_seq++;
        const std::uint64_t due = delay > std::numeric_limits<std::uint64_t>::max() - tick
                                      ? std::numeric_limits<std::uint64_t>::max()
                                      : tick + delay;
        q.emplace(EventKey{due, seq}, std::move(e));
        return true;
    }

    /// Removes the first event, in queue order, of this kind with this tile;
    /// false if there is none (D: 0x100ade40). The original matches only when
    /// the stored kind equals the first argument, the stored subtype equals the
    /// second AND the second is 4 (0x100ade79 `cmp ebx, 4 / jne`), and then
    /// compares the tile as two u16 words (0x100ade7e..0x100ade95; here the
    /// whole TileXY). So a cancel with a subtype other than 4 never removes
    /// anything; no original caller uses another one. Kept as it is: the
    /// subtype argument stays for the call shape, and a subtype other than 4
    /// is a no-op returning false (a generalisation would be a Rules option,
    /// OQ-056, not a silent change). The event being dispatched is already out
    /// of the queue (pop_due), so a handler cancelling its own tile finds
    /// nothing.
    bool cancel_tile(EventKind kind, std::uint32_t subtype, TileXY tile) {
        if (subtype != kTileSubtype) {
            return false;
        }
        for (auto it = q.begin(); it != q.end(); ++it) {
            const TimedEvent& e = it->second;
            if (e.kind == kind && e.subtype == subtype && e.tile == tile) {
                q.erase(it);
                return true;
            }
        }
        return false;
    }
};

} // namespace anno::sim
