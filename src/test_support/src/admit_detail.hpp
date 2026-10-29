// Admission decision shared by the in-memory budget and the host coordinator.
#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <anno/test/budget.hpp>
#include <anno/test/parallel.hpp>

namespace anno::test::detail {

/// How often a waiting request re-reads memory (ANNO_TEST_BUDGET_RECHECK_MS,
/// default 200). A recheck only re-decides; it never admits by itself.
inline std::chrono::milliseconds recheck_period() {
    const std::uint64_t n = env_count("ANNO_TEST_BUDGET_RECHECK_MS").value_or(200);
    return std::chrono::milliseconds(n == 0 ? 200 : (n > 60000 ? 60000 : n));
}

/// The longest a blocking reserve or CPU acquire waits before it is refused
/// with "wait deadline" (ANNO_TEST_ADMIT_WAIT_S, default 1800 s). The bound is
/// a refusal; it never starts work that was not admitted.
inline std::chrono::milliseconds admit_deadline() {
    const std::uint64_t s = env_count("ANNO_TEST_ADMIT_WAIT_S").value_or(1800);
    return std::chrono::milliseconds((s == 0 ? 1800 : (s > 86400 ? 86400 : s)) * 1000);
}

inline std::uint64_t sat_add(std::uint64_t a, std::uint64_t b) {
    return a > std::numeric_limits<std::uint64_t>::max() - b
               ? std::numeric_limits<std::uint64_t>::max()
               : a + b;
}

inline std::uint64_t sat_sub(std::uint64_t a, std::uint64_t b) {
    return a > b ? a - b : 0;
}

/// NotReady: the coordinator has no reading taken after the last event that
/// lowered the pending bytes; the request stays queued until it has one.
enum class Decision { Admit, Wait, Refuse, NotReady };

/// One memory domain the request allocates from (docs/perf/README.md, T10c
/// F2). A Windows or plain Linux request has one (the host); a WSL request
/// has its VM and, behind it, the host.
struct Constraint {
    /// Something can measure this domain at all. Without a meter the request
    /// is refused: no reading will ever arrive.
    bool meter = false;
    /// The reading postdates every event that lowered `pending`. A reading
    /// taken before a release (or before a holder's allocation was credited
    /// as resident) does not show those bytes as used.
    bool ready = true;
    std::optional<std::uint64_t> free;
    /// The most this domain can ever hold (physical memory and the commit
    /// limit, or the VM size). Unknown: no request is called over capacity;
    /// one that does not fit waits until its deadline.
    std::optional<std::uint64_t> total;
    /// Reserved bytes not yet visible in `free`, and those already allocated.
    std::uint64_t pending = 0;
    std::uint64_t resident = 0;
};

struct View {
    std::uint64_t headroom = 0;
    std::uint64_t stop_below = 0;
    std::uint64_t oldest_bytes = 0;
    std::uint64_t oldest_seq = 0;
    std::uint64_t self_seq = 0;
    /// [0] is the host (or the only) domain; it fills the reported numbers.
    std::vector<Constraint> constraints;
};

inline Decision decide(const View& view, std::uint64_t bytes, Admit& info) {
    info = Admit{};
    info.estimate = bytes;
    info.headroom = view.headroom;
    auto refuse = [&](const char* why) {
        info.refused = true;
        info.reason = why;
        return Decision::Refuse;
    };
    if (view.constraints.empty()) {
        return refuse("free memory unknown");
    }
    const Constraint& first = view.constraints.front();
    info.pending = first.pending;
    info.resident = first.resident;
    info.free = first.free.value_or(0);
    info.total = first.total.value_or(0);
    for (const Constraint& c : view.constraints) {
        if (!c.meter) {
            return refuse("free memory unknown");
        }
    }
    for (const Constraint& c : view.constraints) {
        if (!c.ready) {
            info.reason = "waiting for a fresh reading";
            return Decision::NotReady;
        }
    }
    for (const Constraint& c : view.constraints) {
        if (!c.free) {
            // A meter exists but gave no number: wait for the next reading
            // (bounded by the caller's deadline). Never an admission.
            info.reason = "free memory unknown";
            return Decision::Wait;
        }
    }
    for (const Constraint& c : view.constraints) {
        if (view.stop_below > 0 && *c.free < view.stop_below) {
            return refuse("low-memory stop");
        }
    }
    // Permanent: more than the domain can ever give. Everything else that does
    // not fit is temporary and waits: reservations end, and memory used outside
    // the budget can come back.
    for (const Constraint& c : view.constraints) {
        if (c.total && bytes > sat_sub(*c.total, view.headroom)) {
            return refuse("exceeds capacity");
        }
    }
    std::uint64_t available = std::numeric_limits<std::uint64_t>::max();
    for (const Constraint& c : view.constraints) {
        const std::uint64_t here = sat_sub(*c.free, sat_add(view.headroom, c.pending));
        available = here < available ? here : available;
    }
    info.available = available;
    bool allowed = bytes <= available;
    if (allowed && view.oldest_bytes > 0 && view.self_seq != view.oldest_seq) {
        allowed = available - bytes >= view.oldest_bytes;
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
