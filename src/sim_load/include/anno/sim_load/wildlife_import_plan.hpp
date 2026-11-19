// Owned shared-list projection only; docs/sim/wildlife-import-plan.md.
#pragma once
#include <map>
#include <variant>

#include <anno/sim_load/wildlife_habitat_load.hpp>

namespace anno::sim_load {
struct WildlifeImportFlags {
    std::optional<std::uint16_t> non_presence;
    std::string provenance;
};
struct WildlifeBergwerkSidecar {
    PrefixBergwerkEntry entry;
    PrefixResourceListNode node_projection;
};
struct WildlifeOccurrenceSidecar {
    PrefixOccurrence occurrence;
    PrefixResourceListNode node_projection;
    std::vector<PrefixOccurrenceLoad> loads; // Includes later non-creating loads.
};
using WildlifeResourceSidecar = std::variant<WildlifeBergwerkSidecar, WildlifeOccurrenceSidecar>;
struct WildlifeImportNodeSource {
    std::uint64_t token = 0;
    std::uint32_t category = 0;
    PrefixSource source;
    std::uint64_t projected_index = 0, insertion_ordinal = 0;
};
struct WildlifeImportedChunk {
    WildlifeChunkSnapshot snapshot;
    std::vector<WildlifeImportNodeSource> order; // Actual shared head order.
    std::map<std::uint64_t, WildlifeResourceSidecar> sidecars;
    WildlifeImportFlags supplied_flags;
    std::uint16_t derived_presence = 0;
    std::uint64_t prefix_draws = 0, source_token_count = 0;
    std::uint32_t chunk_x = 0, chunk_y = 0;
    std::string scene_sha256;
};
enum class WildlifeImportStage { Source, Flags, Chronology, Payload, SpawnLinks, ResourceLinks };
struct WildlifeImportFailure {
    WildlifeImportStage stage;
    std::string reason;
    WildlifeImportFlags supplied_flags;
    std::uint64_t prefix_draws = 0;
    std::optional<PrefixSource> source;
};
[[nodiscard]] formats::Result<WildlifeImportedChunk, WildlifeImportFailure>
reconstruct_wildlife_import(const WildlifeFirstCallResult&, formats::Bytes scene,
                            std::uint32_t chunk_x, std::uint32_t chunk_y,
                            const WildlifeImportFlags&);
struct WildlifeImportRuntime {
    std::uint32_t time = 0;
    WildlifeChunkAllocation allocation = WildlifeChunkAllocation::NormalSuccess;
    std::vector<std::uint64_t> fresh_tokens;
    WildlifeImportFlags flags;
    std::string provenance;
};
struct WildlifeImportDrawCounts {
    std::uint64_t imported_prefix = 0;
    std::size_t maintenance = 0, selector_only = 0;
};
struct WildlifeImportPlan {
    WildlifeImportRuntime supplied;
    WildlifeImportedChunk imported;
    WildlifeHabitatChunk habitat;
    WildlifeFinalChunkFacts final_facts;
    WildlifeChunkPlan maintenance;
    WildlifeMaterializePlan materialization;
    WildlifeImportDrawCounts draws;
    std::vector<std::uint32_t> unused_maintenance;
};
enum class WildlifeImportPlanStage {
    Runtime,
    Import,
    Habitat,
    Maintenance,
    FinalFacts,
    Materialization
};
struct WildlifeImportPlanFailure {
    WildlifeImportPlanStage stage;
    std::string reason;
    WildlifeImportRuntime supplied;
    WildlifeImportDrawCounts draws;
    std::optional<WildlifeImportedChunk> imported;
    std::optional<WildlifeHabitatChunk> habitat;
    std::optional<WildlifeChunkPlan> maintenance;
    std::optional<WildlifeFinalChunkFacts> final_facts;
    std::optional<WildlifeImportFailure> import_error;
    std::optional<WildlifeChunkFailure> maintenance_error;
    std::optional<WildlifeMaterializeFailure> materialization_error;
    std::vector<std::uint32_t> unused_maintenance;
};
// Separate local draw domains; no native allocation/callback/global-history claim.
[[nodiscard]] formats::Result<WildlifeImportPlan, WildlifeImportPlanFailure>
prepare_wildlife_import_plan(const WildlifeFirstCallResult&, formats::Bytes scene,
                             std::uint32_t chunk_x, std::uint32_t chunk_y,
                             const WildlifeImportRuntime&, GroundDescriptorProfile,
                             std::span<const formats::figuren::Entry>, formats::Bytes game,
                             const sim::TypeRegistry&, const WildlifeSpeciesSource&,
                             std::span<const std::uint32_t> maintenance_draws,
                             std::span<const std::uint32_t> selector_draws,
                             std::span<const WildlifeCaveFact> supplied_caves = {});
} // namespace anno::sim_load
