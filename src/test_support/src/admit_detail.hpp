// Admission decision shared by the in-memory budget and the host coordinator.
#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <anno/test/budget.hpp>

namespace anno::test::detail {

inline std::uint64_t sat_add(std::uint64_t a, std::uint64_t b) {
    return a > std::numeric_limits<std::uint64_t>::max() - b
               ? std::numeric_limits<std::uint64_t>::max()
               : a + b;
}

enum class Decision { Admit, Wait, Refuse };

struct View {
    bool free_known = false;
    std::uint64_t free = 0;
    std::uint64_t headroom = 0;
    std::uint64_t stop_below = 0;
    std::uint64_t pending = 0;
    std::uint64_t resident = 0;
    std::uint64_t oldest_bytes = 0;
    std::uint64_t oldest_seq = 0;
    std::uint64_t self_seq = 0;
};

inline Decision decide(const View& view, std::uint64_t bytes, Admit& info) {
    info = Admit{};
    info.estimate = bytes;
    info.headroom = view.headroom;
    info.pending = view.pending;
    info.resident = view.resident;
    if (!view.free_known) {
        // An unknown reading must not authorize the request. The caller can
        // retry once a measurement exists; it is not admitted in the meantime.
        info.refused = true;
        info.reason = "free memory unknown";
        return Decision::Refuse;
    }
    const std::uint64_t blocked = sat_add(view.headroom, view.pending);
    info.available = view.free > blocked ? view.free - blocked : 0;
    const std::uint64_t with_release = sat_add(view.free, view.resident);
    const std::uint64_t max_possible =
        with_release > view.headroom ? with_release - view.headroom : 0;
    if (view.stop_below > 0 && view.free < view.stop_below) {
        info.refused = true;
        info.reason = "low-memory stop";
        return Decision::Refuse;
    }
    if (bytes > max_possible) {
        info.refused = true;
        info.reason = "exceeds capacity";
        return Decision::Refuse;
    }
    bool allowed = bytes <= info.available;
    if (allowed && view.oldest_bytes > 0 && view.self_seq != view.oldest_seq) {
        allowed = info.available - bytes >= view.oldest_bytes;
    }
    if (allowed) {
        info.admitted = true;
        info.reason = "admitted";
        return Decision::Admit;
    }
    info.reason = "waiting";
    return Decision::Wait;
}

struct Hold {
    std::uint64_t id = 0;
    std::uint64_t bytes = 0;
    /// Bytes of this hold already visible in the process private total.
    /// The estimate is a ceiling, so a hold can be partly resident: the rest
    /// stays pending until the private total catches up. Marking the whole
    /// hold pending after the allocation has already lowered free memory
    /// counts those bytes twice and refuses a later job that would fit once
    /// this hold is released.
    std::uint64_t resident_bytes = 0;
};

/// Private and free readings taken while this client held nothing, or folded
/// forward by the resident bytes of a hold that has since been released.
/// Retained allocations stay in the baseline, so a later unallocated job is
/// not marked resident just because the process still has the old cache.
struct ReadingBase {
    bool set = false;
    std::uint64_t private_bytes = 0;
    bool free_set = false;
    std::uint64_t free_bytes = 0;
};

inline void snap_base(ReadingBase& base, std::uint64_t private_bytes,
                      const std::optional<std::uint64_t>& free_bytes) {
    base.private_bytes = private_bytes;
    base.set = true;
    if (free_bytes) {
        base.free_bytes = *free_bytes;
        base.free_set = true;
    }
}

inline void observe(std::vector<Hold>& holds, ReadingBase& base, const BudgetHooks& hooks) {
    if (hooks.process_private == nullptr) {
        return;
    }
    const std::optional<std::uint64_t> private_bytes = hooks.process_private();
    if (!private_bytes) {
        for (Hold& hold : holds) {
            hold.resident_bytes = 0;
        }
        return;
    }
    std::optional<std::uint64_t> free_bytes;
    if (hooks.free_bytes != nullptr) {
        free_bytes = hooks.free_bytes();
    }
    // No live hold: the current readings are the new baseline. Memory the
    // process kept after the previous job is not residency of the next one.
    if (holds.empty()) {
        snap_base(base, *private_bytes, free_bytes);
        return;
    }
    if (!base.set) {
        snap_base(base, *private_bytes, free_bytes);
    }
    const std::uint64_t growth =
        *private_bytes > base.private_bytes ? *private_bytes - base.private_bytes : 0;
    // Residency is only the part that shows up in both the private total and
    // the free-memory drop. Private commit alone is not physical residency,
    // and a missing free reading credits nothing.
    std::uint64_t credit = 0;
    if (free_bytes && base.free_set) {
        const std::uint64_t drop =
            base.free_bytes > *free_bytes ? base.free_bytes - *free_bytes : 0;
        credit = growth < drop ? growth : drop;
    }
    for (Hold& hold : holds) {
        if (credit >= hold.bytes) {
            hold.resident_bytes = hold.bytes;
            credit -= hold.bytes;
        } else {
            hold.resident_bytes = credit;
            credit = 0;
        }
    }
}

inline void fold_released(ReadingBase& base, const Hold& hold) {
    if (!base.set || hold.resident_bytes == 0) {
        return;
    }
    base.private_bytes = sat_add(base.private_bytes, hold.resident_bytes);
    if (base.free_set) {
        base.free_bytes =
            base.free_bytes > hold.resident_bytes ? base.free_bytes - hold.resident_bytes : 0;
    }
}

inline std::uint64_t sum_reserved(const std::vector<Hold>& holds) {
    std::uint64_t sum = 0;
    for (const Hold& hold : holds) {
        sum = sat_add(sum, hold.bytes);
    }
    return sum;
}

inline std::uint64_t sum_resident(const std::vector<Hold>& holds) {
    std::uint64_t sum = 0;
    for (const Hold& hold : holds) {
        sum = sat_add(sum, hold.resident_bytes);
    }
    return sum;
}

} // namespace anno::test::detail
