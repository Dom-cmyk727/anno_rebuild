// Counts the bytes that operator new hands out while a scope is alive (stage S6
// review: "memory tracks the bytes read", docs/sim/state.md F33). The counting
// operator new/delete are replaced in alloc_probe.cpp for the whole test binary;
// outside a scope they only forward to malloc/free.
#pragma once

#include <cstdint>

namespace anno::test {

/// While alive, every allocation of any thread adds its size to bytes(). Scopes do
/// not nest (a test opens one at a time).
class AllocScope {
  public:
    AllocScope();
    ~AllocScope();
    AllocScope(const AllocScope&) = delete;
    AllocScope& operator=(const AllocScope&) = delete;

    /// The total size requested from operator new since the scope opened (frees are
    /// not subtracted: an upper bound of the peak).
    [[nodiscard]] std::uint64_t bytes() const;
};

/// Hands `p` back through an opaque call so that the optimiser cannot drop an
/// allocation that only a test of the probe itself makes.
void* keep(void* p);

} // namespace anno::test
