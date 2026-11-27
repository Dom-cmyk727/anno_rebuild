#include <anno/render/direct_instances.hpp>

namespace anno::render {

std::vector<ObjectRecord>
resolve_direct_records(const ChunkInstanceBuckets& buckets,
                       std::span<const std::pair<int, int>> visible_chunks, const Scene& scene,
                       const AtlasResidency& atlas, std::uint32_t track, bool depth_test,
                       const depth::Params& dparams, std::vector<FrameRef>& needed) {
    std::vector<ObjectRecord> resolved;
    for (const auto& chunk : visible_chunks) {
        const auto it = buckets.find(chunk);
        if (it == buckets.end()) {
            continue;
        }
        for (const StaticInstance& inst : it->second) {
            const auto rec = scene.object_record(inst.draw, {0, 0}, track, depth_test, dparams);
            if (!rec) {
                continue;
            }
            resolved.push_back(*rec);
            if (!atlas.resident(rec->frame)) {
                needed.push_back({rec->frame, inst.draw.frame->width, inst.draw.frame->height});
            }
        }
    }
    return resolved;
}

std::vector<DirectObjectInstance> place_direct_instances(std::span<const ObjectRecord> records,
                                                         const AtlasResidency& atlas) {
    std::vector<DirectObjectInstance> out;
    out.reserve(records.size());
    for (const ObjectRecord& rec : records) {
        if (const auto place = atlas.place(rec.frame)) {
            out.push_back({rec, *place});
        }
        // else: Overflow/Deferred this call - left out, the caller falls back for it.
    }
    return out;
}

} // namespace anno::render
