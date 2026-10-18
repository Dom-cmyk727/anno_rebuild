// The slot list: the original's per-city building group list as a reusable
// container (spec: docs/sim/state.md §4.6, §5.2; evidence: AnnoWelt.dll list
// constructor 0x100236a0, insert 0x10026a00 and 0x10026b30, remove 0x100255c0).
//
// A list is a vector of optional elements with null holes. It never shrinks
// or compacts, so the position of an element (its slot) and the length of the
// vector, which the round-robin scheduler divides by (scheduler.hpp), depend
// on the whole history of inserts and removals. A new element does not take
// the lowest free slot: the scan starts after the slot stored last (`hint`)
// and wraps round (decision D1, OQ-036; `SlotPolicy::LowestFree` is the
// opt-in). The same container serves the figure registry (A6, assumed).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <anno/sim/config.hpp>
#include <anno/sim/ids.hpp>

namespace anno::sim {

/// What an insert needs from the configuration (SimConfig::list_min_growth
/// and Rules::slot_policy).
struct SlotParams {
    /// Smallest length a full list grows to (D: 0x10026a00, 10). SimConfig::
    /// validate requires 1..kListSizeBound (valid_list_min_growth). Here a
    /// smaller value is still safe (a full list then grows by one element) and
    /// a larger one is cut to kListSizeBound, so a value that skipped the
    /// validation cannot ask for gigabytes; for every valid value the result
    /// is the spec's.
    std::uint32_t min_growth = 10;
    SlotPolicy policy = SlotPolicy::Original;
};

template <class T> struct SlotList {
    /// The slot stored last, -1 for none (D: list +0x00; the constructor
    /// writes -1). Removing an element leaves it alone.
    std::int64_t hint = -1;
    /// The scheduler's round-robin cursor (D: list +0x1c, a u16 in the
    /// original, widened). In a consistent state it is below slots.size().
    std::uint32_t cursor = 0;
    /// The elements including null holes (D: list vector at +0x10/+0x0c).
    std::vector<std::optional<T>> slots;

    /// `initial` null elements. The original's constructor creates exactly one
    /// (A7, D 0x100236a0), which is the default. SimConfig::validate keeps
    /// list_initial_size within 1..kListSizeBound (valid_list_initial_size);
    /// the constructor itself does not cut a larger value, because a loader
    /// builds lists of the saved length.
    explicit SlotList(std::uint32_t initial = 1) : slots(initial) {}

    /// Number of non-null elements (derived; the original keeps it at +0x08).
    [[nodiscard]] std::uint32_t live() const {
        std::uint32_t count = 0;
        for (const auto& s : slots) {
            if (s.has_value()) {
                ++count;
            }
        }
        return count;
    }

    /// The length a full list of `n` elements grows to, 0 if it cannot grow:
    /// the spec's n + n / 4, at least `min_growth` (cut to kListSizeBound),
    /// and at least n + 1 (F22, below). A slot is a u32 and kNoSlot is its
    /// maximum, so the new slot n must be below kNoSlot (n <= 0xFFFFFFFE) and
    /// the length stays at most 0xFFFFFFFF; a full list of kNoSlot or more
    /// elements returns 0 and `insert` then returns kNoSlot instead of
    /// truncating the slot (a limit of the slot type, not of the game: widen
    /// `Slot`, `cursor` and `live()` together to lift it).
    static constexpr std::uint64_t grown_size(std::uint64_t n, std::uint32_t min_growth) {
        if (n >= kNoSlot) {
            return 0;
        }
        std::uint64_t grown = n + n / 4;
        const std::uint64_t floor_to = min_growth < kListSizeBound ? min_growth : kListSizeBound;
        if (grown < floor_to) {
            grown = floor_to;
        }
        if (grown <= n) {
            grown = n + 1; // F22: always make room for the new element
        }
        return grown < kNoSlot ? grown : kNoSlot;
    }

    /// Stores `v` in a free slot, sets `hint` to it and returns it (D:
    /// 0x10026a00, 0x10026b30). The cursor is not touched (D: neither routine
    /// writes +0x1c), also not by a growth. Returns kNoSlot, leaving the list
    /// and `v` alone, when a full list cannot grow (see grown_size).
    ///
    /// Original policy: the scan visits slots (hint + 1) mod n, (hint + 2)
    /// mod n, ... for n steps and takes the first null one (after a removal
    /// that is the freed slot only if the scan reaches it before another
    /// hole). With no null slot the vector grows to n + n / 4, but at least
    /// `min_growth`, the new elements are null and `v` takes slot n. A fresh
    /// list grows 1, 10, 12, 15, 18, 22, 27, 33, 41, 51, 63, 78, 97, ...
    ///
    /// Beyond the spec (docs/sim/state.md §13, F22, fix): a length n below 4
    /// has n / 4 == 0, so with `min_growth` of 1 to 3 the spec's formula gives
    /// no growth at all and the slot n would lie outside the vector; the
    /// growth is raised to n + 1 then. For every min_growth the original
    /// uses (and any larger one up to kListSizeBound) the result is the
    /// spec's. A `policy` that is not an enumerator (valid_slot_policy; the
    /// validation rejects it) is read as Original.
    Slot insert(T v, const SlotParams& p) {
        const std::uint64_t n = slots.size();
        std::uint64_t slot = n;
        bool found = false;
        if (p.policy == SlotPolicy::LowestFree) {
            for (std::uint64_t i = 0; i < n && !found; ++i) {
                if (!slots[static_cast<std::size_t>(i)]) {
                    slot = i;
                    found = true;
                }
            }
        } else {
            // u64 arithmetic: hint = -1 wraps to 0 here, exactly hint + 1. The
            // start is reduced modulo n before the walk, so even a corrupt
            // hint outside the list (the loader rejects it, docs/sim/state.md
            // §9.6) is defined and the scan still visits every slot once.
            const std::uint64_t first = n == 0 ? 0 : (static_cast<std::uint64_t>(hint) + 1U) % n;
            for (std::uint64_t i = 0; i < n && !found; ++i) {
                const std::uint64_t idx = (first + i) % n;
                if (!slots[static_cast<std::size_t>(idx)]) {
                    slot = idx;
                    found = true;
                }
            }
        }
        if (!found) {
            const std::uint64_t grown = grown_size(n, p.min_growth);
            if (grown == 0) {
                return kNoSlot;
            }
            slots.resize(static_cast<std::size_t>(grown));
        }
        slots[static_cast<std::size_t>(slot)].emplace(std::move(v));
        hint = static_cast<std::int64_t>(slot);
        return static_cast<Slot>(slot);
    }

    /// Empties slot `s`; hint, cursor and the length stay (D: 0x100255c0). A
    /// slot beyond the list is ignored.
    void remove(Slot s) {
        if (s < slots.size()) {
            slots[s].reset();
        }
    }
};

} // namespace anno::sim
