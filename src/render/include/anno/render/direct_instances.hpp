// RENDER-DIRECT-3 stage 3a: the static instance buckets (2c), resolved to GPU-ready records for
// a direct instanced draw - no baked chunk texture, no compute bake pass. The CPU-side source of
// truth; the GPU wiring (vertex/fragment pair, instance buffer byte layout) is a later step, kept
// separate so this stays testable with no device open (anno_render, not anno_render_gpu).
#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <anno/render/chunk_draws.hpp>
#include <anno/render/chunk_instances.hpp>
#include <anno/render/depth.hpp>
#include <anno/render/residency.hpp>
#include <anno/render/scene.hpp>

namespace anno::render {

/// One direct-drawn static object: `Scene::object_record`'s own result (position, flags, depth -
/// the same fields and the same function 2a proved equal to today's baked-chunk path) plus where
/// its frame lives in the resident atlas (`AtlasPlace` - its `width`/`height` are also the quad
/// size, the frame's own, so no separate field is needed for that).
struct DirectObjectInstance {
    ObjectRecord record;
    AtlasPlace atlas;
};

/// `buckets`' entries in `visible_chunks` (duplicates across chunk boundaries are not
/// deduplicated here - a caller drawing every visible chunk already only visits a shared-reach
/// instance once per chunk it is bucketed under, same as today's chunk-by-chunk draw), resolved
/// via `scene.object_record` and `atlas`. Frames not yet resident are `ensure`d as one batch
/// first (`AtlasClass::Static`, `max_upload_bytes` the same budget rule the chunk-build compute
/// pass already uses) before any `place` lookup. An instance whose frame could not be made
/// resident this call (`AtlasResidency::ensure`'s `Overflow`/`Deferred`) is left out - the caller
/// falls back to the baked-chunk path for it, same as `objects_draws` already does for other
/// reasons (CLAUDE.md: a path, not a failure).
std::vector<DirectObjectInstance>
direct_object_instances(const ChunkInstanceBuckets& buckets,
                        std::span<const std::pair<int, int>> visible_chunks, const Scene& scene,
                        AtlasResidency& atlas, std::uint32_t track, bool depth_test,
                        const depth::Params& dparams, std::uint64_t max_upload_bytes);

} // namespace anno::render
