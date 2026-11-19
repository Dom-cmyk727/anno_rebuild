// UNWIRED primary Wachstum source binding; docs/sim/wildlife-growth-load.md.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/result.hpp>

namespace anno::sim_load {

struct GrowthRecord {
    std::int32_t id = 0;
    std::uint32_t width = 0, height = 0;
    std::uint64_t line = 0;
};
struct GrowthShortRow {
    std::uint64_t line = 0;
    std::int32_t id = 0;
    std::uint32_t row = 0, parsed = 0, width = 0;
    bool stopped = false;
};
struct GrowthLookup {
    std::vector<std::uint8_t> values;
    std::vector<GrowthRecord> records;
    std::vector<GrowthShortRow> short_rows;
    std::uint64_t sections = 0, bridge_rows = 0, climate_rows = 0;
    std::string source_sha256;
};

// Immutable source, entirely owned result. Bounded original packed lookup profile,
// not an engine type/island cap. No I/O, sidecar outputs, registry, city/kernel
// certification, FRUCHTBAR marker or World integration. Unknown/unsafe grammar
// refuses without publishing a partial lookup; see the documented subset.
[[nodiscard]] formats::Result<GrowthLookup, std::string>
load_wildlife_growth(formats::Bytes source);

} // namespace anno::sim_load
