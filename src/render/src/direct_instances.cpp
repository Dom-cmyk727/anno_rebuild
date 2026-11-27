#include <anno/render/direct_instances.hpp>

namespace anno::render {

std::vector<DirectObjectInstance>
direct_object_instances(const ChunkInstanceBuckets& buckets,
                        std::span<const std::pair<int, int>> visible_chunks, const Scene& scene,
                        AtlasResidency& atlas, std::uint32_t track, bool depth_test,
                        const depth::Params& dparams, std::uint64_t max_upload_bytes) {
    // Resolve every visible instance first (object_record), collecting which frames still need
    // making resident, so ensure() is called once for the whole batch - not once per instance
    // (residency.hpp's own contract: it pins the layers it places for this frame).
    std::vector<ObjectRecord> resolved;
    std::vector<FrameRef> needed;
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
    if (!needed.empty()) {
        atlas.ensure(needed, AtlasClass::Static, max_upload_bytes);
    }
    std::vector<DirectObjectInstance> out;
    out.reserve(resolved.size());
    for (std::size_t i = 0; i < resolved.size(); ++i) {
        if (const auto place = atlas.place(resolved[i].frame)) {
            out.push_back({resolved[i], *place});
        }
        // else: Overflow/Deferred this call - left out, the caller falls back for it.
    }
    return out;
}

} // namespace anno::render
