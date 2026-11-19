// Transient original-file prefix only. No normal loader/World publication.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include <anno/formats/annogame.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/sim/resource_import.hpp>
#include <anno/sim/wildlife.hpp>
namespace anno::sim_load {
struct PrefixInputs {
    std::vector<std::uint16_t> sampled_stamps; // FILE island order, never numeric slot order.
    std::uint8_t mode_bit = 0;
    std::string stamp_provenance, mode_provenance;
    std::uint32_t chunk_edge =
        8; // Original geometry; caller configurable, no island/population cap.
};
struct PrefixSource {
    std::uint64_t island = 0, header = 0, record = 0, header_offset = 0;
    std::uint32_t slot = 0;
};
struct PrefixBoundary {
    PrefixSource source;
    std::string tag, reason;
};
struct PrefixHeader {
    PrefixSource source;
    std::string tag;
    std::uint64_t records = 0; // Completed typed items; setup headers have zero.
    std::optional<std::uint64_t> water_mask_updates; // Engaged only after GELAENDE epilogue.
    std::optional<std::uint64_t> fertility_updates;  // Engaged after FRUCHTBAR, even if unchanged.
    std::optional<std::uint64_t> bergwerk_nodes_added; // Engaged after the whole header.
};
struct PrefixResource {
    PrefixSource source;
    sim::resource_import::Record raw;
    sim::resource_import::Effect effect;
    std::uint64_t draw_before = 0;
};
struct PrefixSpawn {
    PrefixSource source;
    formats::insel::HirschRecord raw;
    sim::GoodsId goods = 0;
    std::uint8_t timestamp = 0;
    std::uint64_t chunk = 0, draw_before = 0;
    std::optional<std::uint64_t> next_in_chunk;
    // Successful load domain: category 0, unoccupied. NO guessed figure id.
};
// BERGWERK2 resource-list records, NOT OCCUR ore occurrences or tonnage.
// Contract/primary closure: docs/sim/wildlife-bergwerk.md.
struct PrefixBergwerkEntry {
    PrefixSource source;
    std::uint32_t local_x = 0, local_y = 0, world_x = 0, world_y = 0;
    std::uint8_t rotation = 0;
};
struct PrefixBergwerkNode {
    std::uint64_t entry = 0, chunk = 0;
    std::uint8_t offsets = 0, rotation = 0, mask = 0x10, kind = 1;
    std::optional<std::uint64_t> next;
};
struct PrefixBergwerkChunk {
    // Only the represented resource-presence bit; not the full original cache.
    std::uint8_t resource_flags = 0;
    std::optional<std::uint64_t> head;
};
struct PreparedIsland {
    std::uint32_t slot = 0, climate = 0;
    std::shared_ptr<const sim::Terrain> terrain;
    sim::resource_import::Output resource;
    std::vector<PrefixHeader> headers;
    std::vector<PrefixResource> resources;
    std::vector<PrefixSpawn> spawns; // Source chronology, never prepend visit order.
    std::vector<std::optional<std::uint64_t>> chunk_heads;
    std::vector<PrefixBergwerkEntry> bergwerk_entries;
    std::vector<PrefixBergwerkNode> bergwerk_nodes;
    std::vector<PrefixBergwerkChunk> bergwerk_chunks;
    bool complete_local_records = false;
};
struct PreparedPrefix {
    PrefixInputs supplied;
    std::string source_sha256;
    std::vector<PreparedIsland> islands; // Source order.
    std::uint64_t consumed = 0;
    bool complete_local_records = false;
    std::optional<PrefixBoundary> boundary; // Diagnostic prefix, not resumable success.
};
struct PrefixFailure {
    std::string reason;
    std::optional<PreparedPrefix> diagnostic; // Never returned as successful completion.
};
// Strict parsed metadata, present UNUSED is distinct from absent/unknown/truncated.
formats::Result<std::vector<sim::resource_import::TypeFacts>, std::string>
resource_metadata(std::span<const formats::figuren::Entry>, const sim::TypeRegistry&);
// Scene bytes supply embedded local-island child sequences, not scene/world
// execution. Player/actor and post-island callbacks are outside this adapter;
// the supplied stream is isolated local-header input, never a whole-world cursor.
// Only successful original allocations and fresh embedded islands. Unsupported
// headers/classes stop with a labelled diagnostic partial. Malformed prerequisites
// may refuse without any prefix. MAIN is never used for stamp or mode.
formats::Result<PreparedPrefix, PrefixFailure>
prepare_wildlife_prefix(formats::Bytes scene, std::span<const formats::figuren::Entry> entries,
                        formats::Bytes annogame, const sim::TypeRegistry&, PrefixInputs,
                        std::span<const std::uint32_t> draws);
} // namespace anno::sim_load
