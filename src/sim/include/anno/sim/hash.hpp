// Canonical state hashing for desync detection (spec: docs/sim/core.md,
// "State hash"). Every field is fed explicitly as fixed-width little-endian
// bytes in a documented order, never as memory dumps, so builds on any
// compiler and platform (and a future second implementation) agree.
#pragma once

#include <cstdint>
#include <span>

namespace anno::sim {

/// FNV-1a 64 (offset basis 0xcbf29ce484222325, prime 0x100000001b3) over
/// the fed bytes. Detects accidental divergence; not a security hash.
class StateHasher {
  public:
    void bytes(std::span<const std::uint8_t> b) {
        for (const std::uint8_t x : b) {
            h_ ^= x;
            h_ *= 0x100000001b3ULL;
        }
    }
    void u8(std::uint8_t v) { bytes({&v, 1}); }
    void u16(std::uint16_t v) { le(v, 2); }
    void u32(std::uint32_t v) { le(v, 4); }
    void u64(std::uint64_t v) { le(v, 8); }
    [[nodiscard]] std::uint64_t value() const { return h_; }

  private:
    void le(std::uint64_t v, int n) {
        for (int i = 0; i < n; ++i) {
            u8(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    std::uint64_t h_ = 0xcbf29ce484222325ULL;
};

} // namespace anno::sim
