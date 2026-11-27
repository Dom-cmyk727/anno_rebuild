#pragma once

#include <cstddef>
#include <map>
#include <span>
#include <utility>
#include <vector>

#include <anno/render/iso.hpp>

namespace anno::render {

/// One object of an island's `other` list (RENDER-DIRECT-2a), resolved once at load instead of on
/// every chunk build, because its appearance cannot change over time - see is_static_instance.
/// `draw.anchor` is already in scene (global) screen space, the island's placement offset applied.
/// `list_index` is `IslandLists::other`'s own index: RENDER-DIRECT-2c patches one entry by it, and
/// a test merges this with the (rare) time-varying entries' live draws, in the list's own order.
struct StaticInstance {
    std::size_t list_index;
    SpriteDraw draw;
};

/// Whether place_prepared's frame choice for anlage `id` can never change with time_ms:
/// frame_index steps `(time_ms / ms + phase) % n` when `ms != 0`, `phase % n` when `ms == 0` - so
/// `n == 1` or `ms == 0` both make every call choose the same frame. A missing/empty entry is also
/// time-invariant (place_prepared always skips it, counted failed/invisible, same as here).
bool is_static_instance(const formats::anlagen::AnlagenFile& anlagen, std::uint32_t anlage_id);

/// The static-eligible entries of `list` (place_prepared's own per-object resolution for each, one
/// at a time, as `Scene::object_frame` already does), in `list`'s own order, before any chunk
/// split.
std::vector<StaticInstance> resolve_static_instances(std::span<const Prepared> list,
                                                     const formats::anlagen::AnlagenFile& anlagen,
                                                     std::size_t group, Lift lift);

/// `instances`' entries bucketed by the chunk(s) their `draw.reach()` overlaps (`frame.hpp`'s own
/// `visible_chunks`, so the chunk grid matches the real chunk build exactly), `offset` (the
/// island's placement at this group, `Placement::offset`) applied to every anchor first. An
/// instance whose reach spans more than one chunk is listed under each.
std::map<std::pair<int, int>, std::vector<StaticInstance>>
bucket_by_chunk(const std::vector<StaticInstance>& instances, std::pair<int, int> offset);

} // namespace anno::render
