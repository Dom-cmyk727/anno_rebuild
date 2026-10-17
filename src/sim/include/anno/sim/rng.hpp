// The simulation's random numbers: the original world PRNG (spec:
// docs/sim/core.md, "World PRNG"; evidence: docs/re/tick-loop.md §3).
//
// The original draws from a table of 2048 u16 values with a cursor:
// next() = T[(cursor + 1) mod len], and the cursor moves on (D: AnnoWelt.dll
// world slot +0xb4 = 0x100a1740). The table is exactly the first 2048
// outputs of the MSVC CRT rand() from its default seed 1 (verified over all
// entries of the table at AnnoWelt.dll 0x100cb8f8), so it is generated here
// instead of copied. The whole generator state is the cursor.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace anno::sim {

/// Length of the original table (D: AnnoWelt.dll 0x100a3980 sets 0x800).
/// The default only: WorldRng takes any non-empty table (PLAN §0.1).
inline constexpr std::size_t kOriginalRngLength = 2048;

/// The first `n` outputs of the MSVC CRT rand() after srand(seed):
/// x = x · 214013 + 2531011 mod 2³², output (x >> 16) & 0x7fff.
std::vector<std::uint16_t> msvc_rand_sequence(std::size_t n, std::uint32_t seed = 1);

/// The world PRNG: a table and a cursor.
class WorldRng {
  public:
    /// The original generator: msvc_rand_sequence(2048, 1), cursor 0.
    WorldRng();
    /// A generator over `table`, cursor `cursor mod size`; nullopt for an
    /// empty table.
    static std::optional<WorldRng> from_table(std::vector<std::uint16_t> table,
                                              std::uint32_t cursor = 0);

    /// T[(cursor + 1) mod len]; the cursor moves to that index.
    std::uint16_t next();
    [[nodiscard]] std::uint32_t cursor() const { return cursor_; }
    /// Sets the cursor to `value mod len`, as the original's seeding through
    /// world property 8 (D: AnnoWelt.dll 0x100a3230; u32 value, `divl` by
    /// the length at 0x100a32ac).
    void set_cursor(std::uint32_t value);
    [[nodiscard]] std::size_t size() const { return table_.size(); }
    [[nodiscard]] std::span<const std::uint16_t> table() const { return table_; }
    /// FNV-1a 64 of the table (u16 LE), for the state hash.
    [[nodiscard]] std::uint64_t table_digest() const { return digest_; }

  private:
    explicit WorldRng(std::vector<std::uint16_t> table);

    std::vector<std::uint16_t> table_;
    std::uint64_t digest_ = 0;
    std::uint32_t cursor_ = 0;
};

} // namespace anno::sim
