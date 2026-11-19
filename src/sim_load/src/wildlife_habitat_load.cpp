#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <utility>

#include <anno/formats/sha256.hpp>
#include <anno/sim_load/wildlife_habitat_load.hpp>

namespace anno::sim_load {
formats::Result<WildlifeHabitatChunk, std::string>
bind_wildlife_habitat_chunk(const WildlifeFirstCallResult& prepared, formats::Bytes scene,
                            std::uint32_t cx, std::uint32_t cy, GroundDescriptorProfile profile,
                            std::span<const formats::figuren::Entry> entries, formats::Bytes game,
                            const sim::TypeRegistry& registry) {
    const auto& island = prepared.fill.island;
    const auto& tiles = island.resource.tiles;
    const auto& terrain = island.terrain;
    if (formats::sha256_hex(scene) != prepared.source_sha256)
        return std::string("habitat source: scene identity mismatch");
    const auto raw = formats::insel::islands(scene);
    if (!raw || raw.value().size() != 1 || prepared.ordinal != 0)
        return std::string("habitat source: completed single-island profile required");
    const auto& source = raw.value()[0];
    if (source.version != formats::insel::Version::Insel5 || source.number != island.slot ||
        source.x != tiles.x0() || source.y != tiles.y0() || source.width != tiles.width() ||
        source.height != tiles.height())
        return std::string("habitat source: selected source geometry mismatch");
    std::optional<std::uint32_t> property7;
    std::uint64_t info_header = 0;
    bool leading = true;
    for (std::size_t i = 0; i < source.records.size(); ++i) {
        const auto& record = source.records[i];
        const auto name = record.name();
        if (name != "INFO" && name != "HEIGHT" && name != "GFXID")
            leading = false;
        if (name != "INFO")
            continue;
        if (!leading || record.payload.size() != 12)
            return std::string("habitat source: unsupported INFO profile");
        // Welt INFO 0x100897ee -> +0xc0; property 7 0x10087c4c reads SAME byte.
        property7 = std::uint32_t{record.payload[0]};
        info_header = i;
    }
    if (!property7 || *property7 != island.climate)
        return std::string("habitat source: INFO/property 7 unavailable or mismatched");
    // Original local geometry, not an engine island/content bound.
    constexpr std::uint64_t edge = 8, side = edge + 2;
    if (!island.complete_local_records || !terrain || tiles.chunk_edge() != edge ||
        tiles.x0() % 8 != 0 || tiles.y0() % 8 != 0 ||
        terrain->height.size() != tiles.tile_count() || terrain->gfx.size() != tiles.tile_count() ||
        terrain->fertility.size() != tiles.tile_count())
        return std::string("habitat source: unsupported completed geometry");
    const auto lx = std::uint64_t{cx} * edge, ly = std::uint64_t{cy} * edge;
    if (lx == 0 || ly == 0 || lx + edge >= tiles.width() || ly + edge >= tiles.height())
        return std::string("habitat source: complete interior chunk and halo required");
    const auto wx = std::int64_t{tiles.x0()} + static_cast<std::int64_t>(lx);
    const auto wy = std::int64_t{tiles.y0()} + static_cast<std::int64_t>(ly);
    if (wx - 1 < std::numeric_limits<std::int32_t>::min() ||
        wy - 1 < std::numeric_limits<std::int32_t>::min() ||
        wx + 8 > std::numeric_limits<std::int32_t>::max() ||
        wy + 8 > std::numeric_limits<std::int32_t>::max())
        return std::string("habitat source: halo world-coordinate overflow");
    std::vector<std::uint32_t> words;
    std::set<std::uint32_t> requested;
    for (std::uint64_t y = 0; y < side; ++y)
        for (std::uint64_t x = 0; x < side; ++x) {
            const auto word = tiles.object((ly - 1 + y) * tiles.width() + lx - 1 + x);
            words.push_back(word);
            // Original tests zero BEFORE lookup, even if type 0 is defined.
            if (word == 0)
                continue;
            requested.insert(word);
            const auto* type = registry.find(sim::TypeId{word});
            if (!type)
                return std::string("habitat source: missing actual registry type");
            // Discovery only: the source binder validates this kind before use.
            if (type->prodtype == 0xe2) {
                if (word == std::numeric_limits<std::uint32_t>::max())
                    return std::string("habitat source: successor overflow");
                requested.insert(word + 1);
            }
        }
    const std::vector<std::uint32_t> needed(requested.begin(), requested.end());
    auto bound = bind_wildlife_final_types(profile, needed, entries, game, registry);
    if (!bound)
        return "habitat source: " + bound.error();
    std::map<std::uint32_t, const WildlifeFinalTypeFact*> indexed;
    for (const auto& type : bound.value().types)
        indexed.emplace(type.object_word, &type);
    WildlifeHabitatChunk out;
    out.chunk_x = cx;
    out.chunk_y = cy;
    out.world_x = static_cast<std::int32_t>(wx);
    out.world_y = static_cast<std::int32_t>(wy);
    out.property7 = *property7;
    out.info_header = info_header;
    out.island_ordinal = prepared.ordinal;
    out.scene_sha256 = prepared.source_sha256;
    for (std::size_t i = 0; i < words.size(); ++i) {
        const auto word = words[i];
        auto resolved = word;
        sim::WildlifeHabitatFact fact{sim::TypeId{word}, 0, 0};
        if (word != 0) {
            const auto* actual = indexed.find(word)->second;
            if (*actual->prodtype == 0xe2)
                resolved = word + 1; // Checked and validated during source admission.
            const auto* type = indexed.find(resolved)->second;
            fact.goods = *type->output0;
            fact.kind = *type->prodtype;
        }
        out.region.push_back(
            {static_cast<std::int32_t>(wx - 1 + static_cast<std::int64_t>(i % side)),
             static_cast<std::int32_t>(wy - 1 + static_cast<std::int64_t>(i / side)), word,
             resolved, fact});
    }
    for (std::size_t y = 0; y < edge; ++y)
        for (std::size_t x = 0; x < edge; ++x) {
            const auto center = (y + 1) * side + x + 1;
            const std::array<std::size_t, 8> indices{
                center - 1,        center + 1,        center - side - 1, center - side,
                center - side + 1, center + side - 1, center + side,     center + side + 1};
            std::array<sim::WildlifeHabitatFact, 8> neighbors;
            for (std::size_t n = 0; n < indices.size(); ++n)
                neighbors[n] = out.region[indices[n]].fact;
            const auto decision =
                sim::wildlife_habitat(out.region[center].fact, neighbors, out.property7);
            if (!decision)
                return std::string("habitat source: numerical leaf refused");
            const auto position = static_cast<std::uint8_t>(y * edge + x);
            out.habitat[position] = decision.value().value;
            out.cells.push_back({position, center, indices, decision.value().value});
        }
    out.types = std::move(bound.value()); // Region facts no longer borrow descriptor pointers.
    return out;
}
formats::Result<WildlifeChunkPlan, WildlifeChunkFailure> prepare_wildlife_habitat_maintenance(
    const WildlifeHabitatChunk& habitat, WildlifeChunkSnapshot records, std::uint32_t time,
    WildlifeChunkAllocation allocation, std::vector<std::uint64_t> fresh_tokens,
    std::span<const std::uint32_t> draws) {
    WildlifeChunkInputs inputs;
    inputs.chunk = std::move(records);
    inputs.habitat = habitat.habitat;
    inputs.time = time;
    inputs.allocation = allocation;
    inputs.fresh_tokens = std::move(fresh_tokens);
    return prepare_wildlife_chunk(inputs, draws);
}
} // namespace anno::sim_load
