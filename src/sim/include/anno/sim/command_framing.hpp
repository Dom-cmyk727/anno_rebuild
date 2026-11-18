// ClientRecord payload framing (docs/sim/state.md 8.5; ship-trade-routes.md R1).
// These are wire widths, not engine world capacities or the native save layout.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace anno::sim {
/// The longest payload a record can carry: its length field is a u16.
enum : std::size_t { kMaxRecordPayload = 0xffff };
namespace freight_wire {
inline constexpr std::uint64_t head_bytes = 12;   // u64 ship, u32 stop count
inline constexpr std::uint64_t stop_bytes = 20;   // u64 warehouse, i32 x/y, u32 action count
inline constexpr std::uint64_t action_bytes = 13; // u8 direction, u32 goods, u64 tons
inline constexpr std::uint64_t max_stops = (kMaxRecordPayload - head_bytes) / stop_bytes;
inline constexpr std::uint64_t max_actions =
    (kMaxRecordPayload - head_bytes - stop_bytes) / action_bytes;

// Total counts only; callers separately require a nonempty definition. Check
// divisions before multiplying so even hostile u64 counts cannot wrap or allocate.
[[nodiscard]] constexpr std::optional<std::uint64_t> payload_size(std::uint64_t stops,
                                                                  std::uint64_t actions) {
    if (stops > max_stops)
        return std::nullopt;
    const auto base = head_bytes + stops * stop_bytes;
    if (actions > (kMaxRecordPayload - base) / action_bytes)
        return std::nullopt;
    return base + actions * action_bytes;
}
} // namespace freight_wire
} // namespace anno::sim
