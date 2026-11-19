#include <limits>
#include <set>
#include <utility>

#include <anno/sim_load/wildlife_release.hpp>

namespace anno::sim_load {
formats::Result<WildlifeEndEffects, std::string>
project_wildlife_end(const WildlifeCreationDescriptor& descriptor, const WildlifeEndState& input,
                     const WildlifeReleaseIsland& island, WildlifeEndOperation operation) {
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
    for (const auto& chunk : island.chunks)
        if (chunk.x >= island.columns || chunk.z >= island.rows ||
            !seen.emplace(chunk.x, chunk.z).second)
            return std::string("wildlife end: invalid chunk snapshot");
    WildlifeEndEffects out{input, island, {}};
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
        for (auto& chunk : out.island.chunks) {
            if (chunk.x != cx || chunk.z != cz || (chunk.categories & 1U) == 0)
                continue;
            for (auto& record : chunk.records) {
                if (record.category != 0 || (record.spawn.packed & 7U) != x % 8 ||
                    ((record.spawn.packed >> 3) & 7U) != z % 8)
                    continue;
                // Welt 0x1008bdd9: no saved-ID ownership guard, no other field write.
                record.spawn.packed = static_cast<std::uint8_t>(record.spawn.packed & 0xbfU);
                out.stages.push_back(S::ClearOccupancy);
                break; // First category/local-position match in original list order.
            }
            break;
        }
        out.state.attached = false;
        out.stages.push_back(S::DetachIsland);
    }
    out.state.ended = true; // Projection bookkeeping, not an invented original flag write.
    return out;
}
} // namespace anno::sim_load
