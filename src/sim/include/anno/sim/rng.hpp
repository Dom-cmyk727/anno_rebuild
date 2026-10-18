// The simulation's random numbers: the original world PRNG (spec:
// docs/sim/core.md, "World PRNG"; docs/sim/state.md §4.2; evidence:
// docs/re/tick-loop.md §3).
//
// The original draws from a table of 2048 u16 values with a cursor:
// next() = T[(cursor + 1) mod len], and the cursor moves on (D: AnnoWelt.dll
// world slot +0xb4 = 0x100a1740). The table is exactly the first 2048
// outputs of the MSVC CRT rand() from its default seed 1 (verified over all
// entries of the table at AnnoWelt.dll 0x100cb8f8), so it is generated here
// instead of copied. The table is immutable and shared by every copy of a
// generator (a World copy does not copy 4 KiB); the whole mutable state is
// the cursor.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace anno::sim {

/// Length of the original table (D: AnnoWelt.dll 0x100a3980 sets 0x800).
/// The default only: WorldRng takes any non-empty table (PLAN §0.1).
inline constexpr std::size_t kOriginalRngLength = 2048;

/// The first `n` outputs of the MSVC CRT rand() after srand(seed):
/// x = x · 214013 + 2531011 mod 2³², output (x >> 16) & 0x7fff.
std::vector<std::uint16_t> msvc_rand_sequence(std::size_t n, std::uint32_t seed = 1);

/// An immutable draw table and its FNV-1a 64 digest (u16 LE per value).
struct RngTable {
    std::vector<std::uint16_t> values;
    std::uint64_t digest = 0;
};

/// The cursor start of a per-slot stream (docs/sim/state.md §4.2, N: own
/// design, defined here so every implementation agrees; unused by M3 until
/// PrngMode::PerIsland exists). The splitmix64 finaliser over
/// `seed + 0x9e3779b97f4a7c15 · (slot + 1)` in u64 arithmetic, truncated to
/// 32 bits. Integer only.
std::uint32_t rng_mix(std::uint64_t seed, std::uint32_t slot);

/// The world PRNG: a shared table and a cursor.
class WorldRng {
  public:
    /// The original generator: msvc_rand_sequence(2048, 1), cursor 0. All
    /// default generators share one table.
    WorldRng();
    /// A generator over `table`, cursor `cursor mod size`; nullopt for an
    /// empty table.
    static std::optional<WorldRng> from_table(std::vector<std::uint16_t> table,
                                              std::uint32_t cursor = 0);
    // Moves copy: a moved-from generator must keep a non-empty table (the
    // draw divides by its length). Copies share the table.
    WorldRng(const WorldRng&) = default;
    WorldRng& operator=(const WorldRng&) = default;
    WorldRng(WorldRng&& o) : WorldRng(static_cast<const WorldRng&>(o)) {}
    WorldRng& operator=(WorldRng&& o) { return *this = static_cast<const WorldRng&>(o); }
    ~WorldRng() = default;

    /// T[(cursor + 1) mod len]; the cursor moves to that index.
    std::uint16_t next();
    [[nodiscard]] std::uint32_t cursor() const { return cursor_; }
    /// Sets the cursor to `value mod len`, as the original's seeding through
    /// world property 8 (D: AnnoWelt.dll 0x100a3230; u32 value, `divl` by
    /// the length at 0x100a32ac).
    void set_cursor(std::uint32_t value);
    [[nodiscard]] std::size_t size() const { return table_->values.size(); }
    [[nodiscard]] std::span<const std::uint16_t> table() const { return table_->values; }
    /// FNV-1a 64 of the table (u16 LE), for the state hash.
    [[nodiscard]] std::uint64_t table_digest() const { return table_->digest; }
    /// The shared table (never null; copies and moved-from generators hold
    /// the same pointer).
    [[nodiscard]] const std::shared_ptr<const RngTable>& shared_table() const { return table_; }
    /// A generator over the same table whose cursor is
    /// `rng_mix(seed, slot) mod size`.
    [[nodiscard]] WorldRng stream(std::uint64_t seed, std::uint32_t slot) const;

    /// Compile-time canary (never called): binds every data member. A member
    /// added without extending this line fails to compile here, the reminder
    /// to give it a line in fields(WorldRng) (state_fields.hpp) and to bump
    /// kStateHashVersion (docs/sim/state.md §14.4).
    static constexpr void field_list_canary(const WorldRng& r) {
        [[maybe_unused]] const auto& [table_member, cursor_member] = r;
    }

  private:
    explicit WorldRng(std::shared_ptr<const RngTable> table) : table_(std::move(table)) {}

    std::shared_ptr<const RngTable> table_;
    std::uint32_t cursor_ = 0;
};

} // namespace anno::sim
