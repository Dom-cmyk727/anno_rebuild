// Owned empty-cleanup projection; docs/sim/wildlife-release-effects.md.
#pragma once
#include <anno/sim_load/wildlife_create.hpp>

namespace anno::sim_load {
struct WildlifeReleaseRecord {
    std::uint8_t category = 0;
    WildlifeSpawnProjection spawn;
    bool operator==(const WildlifeReleaseRecord&) const = default;
};
struct WildlifeReleaseChunk {
    std::uint32_t x = 0, z = 0;
    std::uint16_t categories = 0;
    std::vector<WildlifeReleaseRecord> records; // Original traversal order, not sorted IDs.
    bool operator==(const WildlifeReleaseChunk&) const = default;
};
struct WildlifeReleaseIsland {
    std::uint32_t origin_x = 0, origin_z = 0, columns = 0, rows = 0;
    std::vector<WildlifeReleaseChunk> chunks; // Sparse owned snapshot, no live links.
    bool operator==(const WildlifeReleaseIsland&) const = default;
};
struct WildlifeEndState {
    WildlifeCreatedProjection animal;
    bool attached = true, ended = false;
    std::uint32_t followers = 0, motion_entries = 0;
    bool motion_cursor = false, route = false;
    bool operator==(const WildlifeEndState&) const = default;
};
enum class WildlifeEndOperation { NormalEnd, DirectRemovalContrast };
enum class WildlifeEndStage {
    DirectRemovalNoRelease,
    EmptyFollowers,
    ResetAction,
    EmptyMotion,
    EmptyRoute,
    IslandRelease,
    HomeLookup,
    ClearOccupancy,
    DetachIsland
};
struct WildlifeEndEffects {
    WildlifeEndState state;
    WildlifeReleaseIsland island;
    std::vector<WildlifeEndStage> stages;
    bool operator==(const WildlifeEndEffects&) const = default;
};
// Pure effects on supplied owned facts only. DirectRemovalContrast does not delete
// anything; it exposes the absence of a normal-end release in that separate path.
[[nodiscard]] formats::Result<WildlifeEndEffects, std::string>
project_wildlife_end(const WildlifeCreationDescriptor&, const WildlifeEndState&,
                     const WildlifeReleaseIsland&,
                     WildlifeEndOperation = WildlifeEndOperation::NormalEnd);
} // namespace anno::sim_load
