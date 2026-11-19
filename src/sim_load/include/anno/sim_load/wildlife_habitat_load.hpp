// Bounded source admission, never a live sweep; docs/sim/wildlife-habitat-load.md.
#pragma once
#include <anno/sim_load/wildlife_final_facts.hpp>

namespace anno::sim_load {
struct WildlifeHabitatSourceCell {
    std::int32_t x = 0, y = 0;
    std::uint32_t original_word = 0, resolved_word = 0;
    sim::WildlifeHabitatFact fact; // Original object word, resolved output/kind.
};
struct WildlifeHabitatCell {
    std::uint8_t position = 0;
    std::size_t center = 0;
    std::array<std::size_t, 8> neighbors{}; // L,R,NW,N,NE,SW,S,SE in owned region.
    bool allowed = false;
};
struct WildlifeHabitatChunk {
    std::uint32_t chunk_x = 0, chunk_y = 0, property7 = 0;
    std::int32_t world_x = 0, world_y = 0; // Chunk origin, halo starts at -1,-1.
    std::string scene_sha256;
    std::uint64_t island_ordinal = 0, info_header = 0;
    WildlifeFinalTypes types;
    std::vector<WildlifeHabitatSourceCell> region; // Row-major 10 x 10, owned.
    std::vector<WildlifeHabitatCell> cells;        // Local packed positions 0..63.
    std::array<std::optional<bool>, 64> habitat{};
};
[[nodiscard]] formats::Result<WildlifeHabitatChunk, std::string>
bind_wildlife_habitat_chunk(const WildlifeFirstCallResult&, formats::Bytes scene,
                            std::uint32_t chunk_x, std::uint32_t chunk_y, GroundDescriptorProfile,
                            std::span<const formats::figuren::Entry>, formats::Bytes game,
                            const sim::TypeRegistry&);
// Source admission is completed separately. Caller supplies the ordered record,
// time, successful-allocation and raw-draw context; no native manager implied.
[[nodiscard]] formats::Result<WildlifeChunkPlan, WildlifeChunkFailure>
prepare_wildlife_habitat_maintenance(const WildlifeHabitatChunk&, WildlifeChunkSnapshot records,
                                     std::uint32_t time, WildlifeChunkAllocation,
                                     std::vector<std::uint64_t> fresh_tokens,
                                     std::span<const std::uint32_t> draws);
} // namespace anno::sim_load
