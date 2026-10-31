// picture_check (round dx, DX1): see the header.
#include <algorithm>

#include <anno/app/picture_check.hpp>

namespace anno::app {

std::vector<BuildingItem> building_items(const formats::insel::Island& island) {
    std::vector<BuildingItem> out;
    for (const formats::chunk::Chunk& rec : island.records) {
        if (rec.name() != "PROD2") {
            continue;
        }
        auto parsed = formats::insel::Block::parse(rec.payload);
        if (!parsed) {
            continue;
        }
        const formats::insel::Block& block = parsed.value();
        for (std::size_t o = 0; o + formats::insel::kItemLen <= block.data.size();
             o += formats::insel::kItemLen) {
            const auto obj = formats::insel::PlacedObject::parse(
                block.data.subspan(o, formats::insel::kItemLen), block);
            out.push_back({block.x, block.y, obj.anlage, obj.rotation, obj.height(),
                           obj.state & 0xFFU, obj.x, obj.y});
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

ChunkDiff compare_chunks(const std::vector<render::ChunkKey>& keys, const ChunkLookup& candidate,
                         const ChunkLookup& reference) {
    ChunkDiff diff;
    for (const render::ChunkKey& key : keys) {
        const render::Chunk* want = reference(key);
        const render::Chunk* got = candidate(key);
        if (want == nullptr || got == nullptr) {
            ++diff.missing;
            continue;
        }
        ++diff.compared;
        diff.with_objects += want->objects.empty() ? 0U : 1U;
        diff.ground += want->ground == got->ground ? 0U : 1U;
        diff.objects += want->objects == got->objects ? 0U : 1U;
        diff.terrain += want->terrain == got->terrain ? 0U : 1U;
        diff.field += want->field == got->field ? 0U : 1U;
    }
    return diff;
}

} // namespace anno::app
