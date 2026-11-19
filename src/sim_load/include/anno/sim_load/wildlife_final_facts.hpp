// Source admission only; docs/sim/wildlife-final-facts.md. No live sweep/World.
#pragma once
#include <anno/sim_load/wildlife_first_call.hpp>
#include <anno/sim_load/wildlife_ground_facts.hpp>
#include <anno/sim_load/wildlife_materialize.hpp>

namespace anno::sim_load {
struct WildlifeFinalTypeSource {
    std::uint32_t word = 0;
    bool null_descriptor = false, ware_present = false;
    std::string source_sha256;
};
struct WildlifeFinalTypes {
    std::vector<WildlifeFinalTypeFact> types;
    std::vector<std::uint32_t> null_words;
    std::vector<WildlifeFinalTypeSource> provenance;
    std::string game_sha256;
};
[[nodiscard]] formats::Result<WildlifeFinalTypes, std::string>
bind_wildlife_final_types(GroundDescriptorProfile, std::span<const std::uint32_t> words,
                          std::span<const formats::figuren::Entry>, formats::Bytes game,
                          const sim::TypeRegistry&);
struct WildlifeFinalChunkFacts {
    WildlifeMaterializeFacts facts; // Cave counts remain explicitly caller supplied.
    WildlifeFinalTypes source;
    std::uint32_t chunk_x = 0, chunk_y = 0;
    std::int32_t world_x = 0, world_y = 0;
    std::string scene_sha256;
    std::uint64_t island_ordinal = 0;
};
// All 64 final cells of one complete original 8 x 8 chunk: SOURCE ADMISSION,
// not reached-field behavior of the original sweep. Consumes final post-fill
// resource tiles, never immutable Terrain::gfx (an earlier layer). Input is the
// completed first-call projection; this cannot authenticate a mutable C++ object.
[[nodiscard]] formats::Result<WildlifeFinalChunkFacts, std::string>
bind_wildlife_final_chunk(const WildlifeFirstCallResult&, std::uint32_t chunk_x,
                          std::uint32_t chunk_y, GroundDescriptorProfile,
                          std::span<const formats::figuren::Entry>, formats::Bytes game,
                          const sim::TypeRegistry&);
} // namespace anno::sim_load
