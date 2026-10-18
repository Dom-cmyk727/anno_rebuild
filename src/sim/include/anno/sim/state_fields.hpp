// The one field list of the persistent state (spec: docs/sim/state.md §9.1,
// §9.3). For every persistent type T there is exactly one function
//     template <class A, Same<T> S> void fields(A& a, S& s)
// naming its fields in canonical order; hash, save and load all run it, so a
// field cannot be saved and not hashed. Adding a field is one line at the end
// of its function plus a kStateHashVersion bump and re-baselined goldens in
// the same commit.
//
// Stream at kStateHashVersion 100 (stage S1), the scopes of §9.3 so far:
//   1 header     u32 kStateHashVersion, u32 kPrngTimelineVersion
//                (minimal: SimConfig, Rules, registry digest and seam names
//                join in stage S3)
//   2 clock_rng  u64 clock.ms, u64 clock.last_tick, u64 rng.size,
//                u64 rng.table_digest, u32 rng.cursor  (= core_hash)
// Not in the stream: the PRNG table itself (immutable, identified by its
// size and digest; a custom table travels in the save frame, §9.5).
#pragma once

#include <cstdint>

#include <anno/sim/archive.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

/// The header scope's fields: the versions the stream was made with. A
/// default-constructed value carries this build's versions; reading a stream
/// made with other versions fails.
struct StateHeader {
    std::uint32_t state_hash_version = kStateHashVersion;
    std::uint32_t prng_timeline_version = kPrngTimelineVersion;
};

template <class A, Same<StateHeader> S> void fields(A& a, S& s) {
    a.u32(s.state_hash_version);
    a.u32(s.prng_timeline_version);
    if constexpr (A::kReading) {
        if (!a.failed() && s.state_hash_version != kStateHashVersion) {
            a.fail("state hash version differs from this build's");
        } else if (!a.failed() && s.prng_timeline_version != kPrngTimelineVersion) {
            a.fail("PRNG timeline version differs from this build's");
        }
    }
}

template <class A, Same<WorldClock> S> void fields(A& a, S& s) {
    a.u64(s.ms);
    a.u64(s.last_tick);
}

/// The PRNG: table size and digest identify the (shared, immutable) table,
/// the cursor is the state. Reading checks the first two against the table
/// the world already has and rejects a cursor outside it.
template <class A, Same<WorldRng> S> void fields(A& a, S& s) {
    std::uint64_t size = s.size();
    std::uint64_t digest = s.table_digest();
    std::uint32_t cursor = s.cursor();
    a.u64(size);
    a.u64(digest);
    a.u32(cursor);
    if constexpr (A::kReading) {
        if (!a.failed()) {
            if (size != s.size() || digest != s.table_digest()) {
                a.fail("PRNG table differs from the world's table");
            } else if (cursor >= size) {
                a.fail("PRNG cursor outside the table");
            } else {
                s.set_cursor(cursor);
            }
        }
    }
}

/// The hierarchical hash of any persistent value, with the scopes down to
/// `depth` recorded in `tree` when it is not null. World::state_hash is this
/// over the world; it is a template so the depth rule can be tested on values
/// that nest scopes (the world itself only has top-level scopes until S2).
template <class T>
std::uint64_t hash_state(const T& value, HashTree* tree, std::uint32_t depth) {
    HashArchive a(HashOptions{tree, depth, false});
    fields(a, value);
    return a.finish();
}

/// The world so far: scopes header and clock_rng (the order of §9.3).
template <class A, Same<World> S> void fields(A& a, S& w) {
    StateHeader header;
    a.scope("header", [&] { fields(a, header); });
    a.scope("clock_rng", [&] {
        fields(a, w.clock);
        fields(a, w.rng);
    });
}

} // namespace anno::sim
