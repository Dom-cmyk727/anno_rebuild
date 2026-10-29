// The fixed-map layout (docs/sim/fixed-map.md). Our own text, not an original
// format: line-oriented ASCII, fixed-width canonical form, FNV-1a 64 digest.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::fixed_map {

/// Reader cap on island lines and on start lines. A text file must not be able
/// to ask for an unbounded vector. The engine does not use this cap (island
/// count is SimConfig::island_slots; seats are the layout). Raise it here.
inline constexpr std::uint32_t kMaxRecords = 65536;
/// Reader cap on the integers of one raw or ore line. Same reason as
/// kMaxRecords. Raise it here.
inline constexpr std::uint32_t kMaxValues = 256;
/// WorldSettings::validate refuses a wider edge (config.hpp kWorldEdgeBound,
/// 2^14). Raise this together with that constant.
inline constexpr std::uint32_t kEdgeBound = 1U << 14;

struct Error {
    std::uint32_t line = 1;
    std::string message;
};

struct Island {
    std::uint32_t slot = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    /// Upper case, `\` already rewritten to `/`.
    std::string key;
    /// Parsed, not applied by the loader (docs/sim/fixed-map.md §7).
    std::vector<std::uint32_t> raw;
    std::vector<std::uint32_t> ore;
    std::uint32_t line = 0;
};

struct Start {
    std::uint32_t seat = 0;
    std::uint32_t island = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t rotation = 0;
    std::uint32_t line = 0;
};

struct FixedMap {
    std::uint32_t version = 1;
    std::string name;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::string base;
    std::uint32_t seed = 1;
    /// Empty when the file has no money record.
    std::optional<std::int32_t> money;
    /// Ascending slot.
    std::vector<Island> islands;
    /// Ascending seat.
    std::vector<Start> starts;
};

/// The text of one error, without the tool's `# path` line.
[[nodiscard]] std::string error_line(const Error& error);

/// Parses one layout. Islands come back sorted by slot, starts by seat.
[[nodiscard]] Result<FixedMap, Error> parse(Bytes bytes);

/// The canonical form of §2 of the spec.
[[nodiscard]] ByteBuffer canonical(const FixedMap& map);

/// FNV-1a 64 over canonical(map).
[[nodiscard]] std::uint64_t digest(const FixedMap& map);

/// The dump body of §3, without the `# path` line and with a trailing newline.
[[nodiscard]] std::string summary(const FixedMap& map);

} // namespace anno::formats::fixed_map
