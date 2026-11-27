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
/// via `scene.object_record`. Any frame not already `atlas.resident()` is appended to `needed`
/// (`FrameRef`, deduplicated by the caller if it cares - a repeated frame across instances is
/// harmless to pass twice to `ensure`, which is itself idempotent per call). **This function never
/// calls `AtlasResidency::ensure` itself and takes no upload budget**: `ensure`'s real job is
/// uploading GPU texels (`write_layer` in `gpu.cpp`'s own `cb_resident`), which needs the device/
/// queue this GPU-free library does not have - calling `ensure` without that upload step would
/// mark frames resident in the bookkeeping with nothing actually written to the atlas texture.
/// The caller (gpu.cpp) collects `needed` across every visible chunk, calls its own residency
/// wrapper once for the whole frame's batch, then calls `place_direct_instances` below.
std::vector<ObjectRecord>
resolve_direct_records(const ChunkInstanceBuckets& buckets,
                       std::span<const std::pair<int, int>> visible_chunks, const Scene& scene,
                       const AtlasResidency& atlas, std::uint32_t track, bool depth_test,
                       const depth::Params& dparams, std::vector<FrameRef>& needed);

/// Looks up each record's current placement in `atlas` (after the caller has made every frame
/// `resolve_direct_records` listed in `needed` resident). A record whose frame still is not
/// resident (`AtlasResidency::ensure`'s `Overflow`/`Deferred` this call) is left out - the caller
/// falls back to the baked-chunk path for it, same as `objects_draws` already does for other
/// reasons (CLAUDE.md: a path, not a failure).
std::vector<DirectObjectInstance> place_direct_instances(std::span<const ObjectRecord> records,
                                                         const AtlasResidency& atlas);

} // namespace anno::render
