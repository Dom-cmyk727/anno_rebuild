#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <utility>

#include <anno/formats/annogame.hpp>
#include <anno/formats/sha256.hpp>
#include <anno/sim_load/wildlife_final_facts.hpp>

namespace anno::sim_load {
formats::Result<WildlifeFinalTypes, std::string>
bind_wildlife_final_types(GroundDescriptorProfile profile, std::span<const std::uint32_t> words,
                          std::span<const formats::figuren::Entry> entries, formats::Bytes game,
                          const sim::TypeRegistry& registry) {
    // Reuse proven fresh kind/HEAD/null admission, NOT its HEAD[0x19] value.
    const auto admitted = bind_wildlife_ground_facts(profile, words, entries, registry);
    if (!admitted)
        return admitted.error();
    const auto names = formats::annogame::read_wildlife_names(game);
    if (!names)
        return names.error();
    const std::set<std::uint32_t> needed(words.begin(), words.end());
    std::map<std::uint32_t, const formats::figuren::Entry*> indexed;
    for (const auto& entry : entries)
        if (needed.contains(entry.id))
            indexed.emplace(entry.id, &entry); // Duplicate needed keys already refused.
    WildlifeFinalTypes out;
    out.game_sha256 = names.value().source_sha256;
    for (const auto& fact : admitted.value()) {
        const auto word = fact.object;
        if (fact.property == GroundProperty::NullDescriptor) {
            out.null_words.push_back(word);
            out.provenance.push_back({word, true, false, {}});
            continue;
        }
        const auto& entry = *indexed.find(word)->second;
        const auto prefix = "final facts object " + std::to_string(word) + ": ";
        const auto count = std::count_if(entry.records.begin(), entry.records.end(),
                                         [](const auto& r) { return r.name() == "WARE"; });
        if (count > 1)
            return prefix + "duplicate WARE";
        // PRODTYPE input +0x34 must not overlap query output header +0x84,
        // Game 0x1000974f..0x1000977e. Existing binder checks its termination.
        if (entry.record("PRODTYPE")->size() > 0x84 - 0x34)
            return prefix + "unsafe PRODTYPE length";
        const auto ware = entry.record("WARE");
        std::vector<sim::GoodsId> outputs;
        if (ware) {
            // Game stack +0x124..+0x564, not an engine name/content bound.
            constexpr std::size_t safe_payload = 0x564 - 0x124;
            if (ware->size() > safe_payload)
                return prefix + "unsafe WARE length";
            std::size_t at = 0;
            // Original WARE storage/loop has two bytes, Game 0x1000a1f4.
            for (std::size_t slot = 0; slot < 2 && at < ware->size(); ++slot) {
                const auto begin = ware->begin() + static_cast<std::ptrdiff_t>(at);
                const auto end = std::find(begin, ware->end(), std::uint8_t{0});
                if (end == ware->end())
                    return prefix + "unterminated WARE";
                const std::string name(begin, end);
                const auto found =
                    std::find(names.value().goods.begin(), names.value().goods.end(), name);
                if (found == names.value().goods.end())
                    return prefix + "unsupported WARE name";
                const auto ordinal = static_cast<sim::GoodsId>(found - names.value().goods.begin());
                const auto goods = registry.goods_table();
                if (ordinal >= goods.size() || goods[ordinal].name != name)
                    return prefix + "goods vocabulary/registry mismatch";
                outputs.push_back(ordinal);
                at = static_cast<std::size_t>(end - ware->begin()) + 1;
            }
        }
        // Game 0x1000b61e zeros output bytes. Absent/empty WARE leaves them zero.
        const sim::GoodsId output0 = outputs.empty() ? 0 : outputs[0];
        // Modern registry drops trailing UNUSED; cross-check only, never use its
        // first vector element as the raw getter or treat missing metadata as 0.
        while (!outputs.empty() && outputs.back() == 0)
            outputs.pop_back();
        if (registry.find(sim::TypeId{word})->output_goods != outputs)
            return prefix + "output/registry mismatch";
        out.types.push_back({word, output0, fact.kind});
        const auto bytes = entry.to_bytes();
        out.provenance.push_back({word, false, ware.has_value(), formats::sha256_hex(bytes)});
    }
    return out;
}
formats::Result<WildlifeFinalChunkFacts, std::string>
bind_wildlife_final_chunk(const WildlifeFirstCallResult& prepared, std::uint32_t cx,
                          std::uint32_t cy, GroundDescriptorProfile profile,
                          std::span<const formats::figuren::Entry> entries, formats::Bytes game,
                          const sim::TypeRegistry& registry) {
    const auto& island = prepared.fill.island;
    const auto& tiles = island.resource.tiles;
    const auto& terrain = island.terrain;
    // This is a completed-input geometry/profile check, not original query order.
    if (!island.complete_local_records || !terrain || tiles.width() == 0 || tiles.height() == 0 ||
        tiles.chunk_edge() != 8 || tiles.x0() % 8 != 0 || tiles.y0() % 8 != 0 ||
        terrain->height.size() != tiles.tile_count() || terrain->gfx.size() != tiles.tile_count() ||
        terrain->fertility.size() != tiles.tile_count())
        return std::string("final chunk: unsupported completed geometry");
    const auto local_x = std::uint64_t{cx} * 8, local_y = std::uint64_t{cy} * 8;
    if (local_x + 8 > tiles.width() || local_y + 8 > tiles.height())
        return std::string("final chunk: full chunk outside prepared island");
    const auto wx = std::int64_t{tiles.x0()} + static_cast<std::int64_t>(local_x);
    const auto wy = std::int64_t{tiles.y0()} + static_cast<std::int64_t>(local_y);
    if (wx < std::numeric_limits<std::int32_t>::min() ||
        wx + 7 > std::numeric_limits<std::int32_t>::max() ||
        wy < std::numeric_limits<std::int32_t>::min() ||
        wy + 7 > std::numeric_limits<std::int32_t>::max())
        return std::string("final chunk: world coordinate overflow");
    std::vector<std::uint32_t> words;
    for (std::uint64_t y = 0; y < 8; ++y)
        for (std::uint64_t x = 0; x < 8; ++x)
            words.push_back(tiles.object((local_y + y) * tiles.width() + local_x + x));
    auto bound = bind_wildlife_final_types(profile, words, entries, game, registry);
    if (!bound)
        return bound.error();
    WildlifeFinalChunkFacts out;
    out.source = std::move(bound.value());
    out.chunk_x = cx;
    out.chunk_y = cy;
    out.world_x = static_cast<std::int32_t>(wx);
    out.world_y = static_cast<std::int32_t>(wy);
    out.scene_sha256 = prepared.source_sha256;
    out.island_ordinal = prepared.ordinal;
    out.facts.types = out.source.types;
    for (std::size_t position = 0; position < words.size(); ++position) {
        const auto word = words[position];
        const auto null =
            std::binary_search(out.source.null_words.begin(), out.source.null_words.end(), word);
        out.facts.tiles.push_back(
            {static_cast<std::uint8_t>(position),
             null ? WildlifeTileResolution::Null : WildlifeTileResolution::Present, word});
    }
    return out;
}
} // namespace anno::sim_load
