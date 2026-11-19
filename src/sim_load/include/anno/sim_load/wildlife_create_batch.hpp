// Ordered conditional owned effects; docs/sim/wildlife-create-batch.md.
#pragma once
#include <anno/sim_load/wildlife_geometry.hpp>
#include <anno/sim_load/wildlife_release.hpp>

namespace anno::sim_load {
struct WildlifeManagerFact {
    std::uint64_t token = 0;
    std::optional<WildlifeManagerOutcome> outcome;
    std::optional<std::uint32_t> assigned_id, creation_time;
    std::string provenance;
};
enum class WildlifeCreateBatchDisposition { Unselected, Null, Created };
struct WildlifeCreateBatchEntry {
    std::uint64_t token = 0;
    WildlifeCreateBatchDisposition disposition = WildlifeCreateBatchDisposition::Unselected;
    WildlifeChunkRecord prior;
    std::optional<WildlifeManagerFact> manager;
    std::optional<WildlifeOrdinaryGeometry> geometry;
    std::optional<WildlifeCreationDescriptor> descriptor;
    std::optional<WildlifeCreationOutcomeEffects> effects;
};
struct WildlifeCreateBatchProgress {
    WildlifeChunkSnapshot chunk;
    std::map<std::uint64_t, WildlifeResourceSidecar> sidecars;
    std::vector<WildlifeCreateBatchEntry> entries;
    WildlifeImportDrawCounts unchanged_draws;
    std::vector<std::uint32_t> unused_maintenance, unused_selectors;
    std::string scene_sha256, corner_provenance;
    std::uint32_t origin_x = 0, origin_z = 0, columns = 0, rows = 0, chunk_x = 0, chunk_z = 0;
    [[nodiscard]] WildlifePartialReleaseIsland release_island() const {
        return {origin_x, origin_z, columns, rows, {{chunk_x, chunk_z, chunk}}};
    }
};
struct WildlifeCreateBatch {
    WildlifeCreateBatchProgress completed;
};
enum class WildlifeCreateBatchScope { Admission, ReachedEntry };
enum class WildlifeCreateBatchStage { Shape, Descriptor, Source, Manager, Geometry, Projection };
struct WildlifeCreateBatchFailure {
    WildlifeCreateBatchScope scope;
    WildlifeCreateBatchStage stage;
    std::optional<std::uint64_t> token;
    std::string reason;
    std::optional<WildlifeCreateBatchProgress> diagnostic_only;
    std::optional<WildlifeGeometryError> geometry_error;
};
[[nodiscard]] formats::Result<WildlifeCreateBatch, WildlifeCreateBatchFailure>
prepare_wildlife_create_batch(const WildlifeFirstCallResult&, const WildlifeImportPlan&,
                              const WildlifeNumericContext&, std::span<const WildlifeManagerFact>,
                              std::string corner_provenance);
} // namespace anno::sim_load
