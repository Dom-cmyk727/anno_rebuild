#include <limits>
#include <set>
#include <utility>

#include <anno/sim_load/wildlife_release.hpp>

namespace anno::sim_load {
namespace {
struct EndGeometry {
    std::uint32_t origin_x, origin_z, columns, rows;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> chunks;
};
template <class Island> EndGeometry geometry_headers(const Island& island) {
    EndGeometry out{island.origin_x, island.origin_z, island.columns, island.rows, {}};
    for (const auto& chunk : island.chunks)
        out.chunks.emplace_back(chunk.x, chunk.z);
    return out;
}
struct HomeLookup {
    std::uint32_t cx, cz;
    std::uint16_t x, z;
};
struct EndTransition {
    WildlifeEndState state;
    std::vector<WildlifeEndStage> stages;
    std::optional<HomeLookup> lookup;
};
bool position_matches(std::uint8_t position, const HomeLookup& home) {
    return (position & 7U) == std::uint32_t{home.x} % 8U &&
           ((position >> 3) & 7U) == std::uint32_t{home.z} % 8U;
}
formats::Result<EndTransition, std::string>
end_transition(const WildlifeCreationDescriptor& descriptor, const WildlifeEndState& input,
               const EndGeometry& island, WildlifeEndOperation operation) {
    if (operation != WildlifeEndOperation::NormalEnd &&
        operation != WildlifeEndOperation::DirectRemovalContrast)
        return std::string("wildlife end: unsupported operation");
    if (descriptor.kind() != 49 && descriptor.kind() != 51)
        return std::string("wildlife end: unsupported kind");
    if (input.animal.type != descriptor.id())
        return std::string("wildlife end: descriptor/type mismatch");
    // Empty profile closes only the skipped branches at Welt 0x10052b40.
    // Query 12 mask 2 is tested at 0x1008bd21; configuration 4 skips it.
    if (input.animal.configuration != 4 || input.followers != 0 || input.motion_entries != 0 ||
        input.motion_cursor || input.route || input.animal.current != 0 ||
        input.animal.first_action_started)
        return std::string("wildlife end: unsupported live cleanup profile");
    if (input.ended && input.attached)
        return std::string("wildlife end: inconsistent ended attachment");
    // This adapter bounds original-width projections, not engine island dimensions.
    if (island.origin_x > std::numeric_limits<std::uint16_t>::max() ||
        island.origin_z > std::numeric_limits<std::uint16_t>::max() || island.columns == 0 ||
        island.rows == 0)
        return std::string("wildlife end: unsupported island geometry");
    std::set<std::pair<std::uint32_t, std::uint32_t>> seen;
    for (const auto& [x, z] : island.chunks)
        if (x >= island.columns || z >= island.rows || !seen.emplace(x, z).second)
            return std::string("wildlife end: invalid chunk snapshot");
    EndTransition out{input, {}, {}};
    using S = WildlifeEndStage;
    if (operation == WildlifeEndOperation::DirectRemovalContrast) {
        out.stages = {S::DirectRemovalNoRelease};
        return out; // Not actual manager deletion, destruction or pointer detachment.
    }
    auto& animal = out.state.animal;
    // Welt 0x10051a90–0x10051afa, called with 1: no active action cleanup here.
    animal.current = 0;
    animal.requested = 0;
    animal.flags = static_cast<std::uint16_t>(animal.flags & 0xf7ffU);
    animal.second_position_words = animal.position_words;
    out.stages = {S::EmptyFollowers, S::ResetAction, S::EmptyMotion, S::EmptyRoute};
    if (input.attached) {
        out.stages.push_back(S::IslandRelease);
        // Property 0x2f reads original HOME words, not current position or widened home.
        const auto x = std::int64_t{animal.original_home[0]};
        const auto z = std::int64_t{animal.original_home[1]};
        const auto cx = (x - island.origin_x) / 8;
        const auto cz = (z - island.origin_z) / 8;
        if (cx < 0 || cz < 0 || cx >= island.columns || cz >= island.rows)
            return std::string("wildlife end: home outside supplied chunk grid");
        out.stages.push_back(S::HomeLookup);
        out.lookup = HomeLookup{static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cz),
                                animal.original_home[0], animal.original_home[1]};
        out.state.attached = false;
    }
    out.state.ended = true; // Projection bookkeeping, not an invented original flag write.
    return out;
}
} // namespace
formats::Result<WildlifeEndEffects, std::string>
project_wildlife_end(const WildlifeCreationDescriptor& descriptor, const WildlifeEndState& input,
                     const WildlifeReleaseIsland& island, WildlifeEndOperation operation) {
    auto result = end_transition(descriptor, input, geometry_headers(island), operation);
    if (!result)
        return result.error();
    WildlifeEndEffects out{std::move(result.value().state), island,
                           std::move(result.value().stages)};
    if (result.value().lookup) {
        const auto& home = *result.value().lookup;
        for (auto& chunk : out.island.chunks) {
            if (chunk.x != home.cx || chunk.z != home.cz || (chunk.categories & 1U) == 0)
                continue;
            for (auto& record : chunk.records) {
                if (record.category != 0 || !position_matches(record.spawn.packed, home))
                    continue;
                record.spawn.packed = static_cast<std::uint8_t>(record.spawn.packed & 0xbfU);
                out.stages.push_back(WildlifeEndStage::ClearOccupancy);
                break;
            }
            break;
        }
        out.stages.push_back(WildlifeEndStage::DetachIsland);
    }
    return out;
}
formats::Result<WildlifePartialEndEffects, std::string> project_wildlife_partial_end(
    const WildlifeCreationDescriptor& descriptor, const WildlifeEndState& input,
    const WildlifePartialReleaseIsland& island, WildlifeEndOperation operation) {
    // Share only the actually needed geometry headers; unknown record data stays in its own type.
    auto result = end_transition(descriptor, input, geometry_headers(island), operation);
    if (!result)
        return result.error();
    WildlifePartialEndEffects out{std::move(result.value().state), island,
                                  std::move(result.value().stages)};
    if (result.value().lookup) {
        const auto& home = *result.value().lookup;
        for (auto& chunk : out.island.chunks) {
            if (chunk.x != home.cx || chunk.z != home.cz || (chunk.chunk.flags & 1U) == 0)
                continue;
            for (auto& record : chunk.chunk.records) {
                if (record.category != 0)
                    continue;
                if (!record.position || *record.position > 63)
                    return std::string(
                        "partial wildlife end: unavailable local position (adapter admission)");
                if (!position_matches(*record.position, home))
                    continue;
                // Same first home match, no saved-ID/occupancy guard; only mask0x40 clears.
                record.occupied = false;
                out.stages.push_back(WildlifeEndStage::ClearOccupancy);
                break;
            }
            break;
        }
        out.stages.push_back(WildlifeEndStage::DetachIsland);
    }
    return out;
}

} // namespace anno::sim_load
