// Isolated third post-island leaf, AnnoWelt 0x10085290–0x100853a9.
// UNWIRED: actual publication must remain fill -> navigation/raster -> cleanup.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <anno/formats/result.hpp>
#include <anno/sim/island.hpp>

namespace anno::sim_load {

struct FinalizedWildlifeGraphics {
    std::shared_ptr<const sim::Terrain> terrain; // New owned columns, even for no change.
    std::uint64_t cleared = 0;
};

// Positive TileLayer geometry; exact HEIGHT/GFX columns, absent or exact fertility,
// and a matching source digest. Uses raw Terrain's forced-zero W/H corner border;
// it cannot express arbitrary logical borders or physical H+1 padding (contract §9).
// O(W*H), at most 16 vertex reads per qualifying tile; no RNG or other writes.
[[nodiscard]] formats::Result<FinalizedWildlifeGraphics, std::string>
finalize_wildlife_graphics(const sim::Island& island);

} // namespace anno::sim_load
