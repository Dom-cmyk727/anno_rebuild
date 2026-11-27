#pragma once

#include <cstddef>
#include <map>
#include <span>
#include <utility>
#include <vector>

#include <anno/formats/insel.hpp>
#include <anno/render/iso.hpp>

namespace anno::render {

/// One object of an island's `other` list (RENDER-DIRECT-2a), resolved once at load instead of on
/// every chunk build, because its appearance cannot change over time - see is_static_instance.
/// `draw.anchor` is already in scene (global) screen space, the island's placement offset applied.
/// `list_index` is `IslandLists::other`'s own index at the time it was resolved: a test merges this
/// with the (rare) time-varying entries' live draws, in the list's own order - NOT a stable key
/// across a building change (adding/removing one object shifts every later index, since `other` is
/// built from a sorted `std::set`). `key` is the stable identity `prepare_objects`' own
/// `std::set<PlacedObject>` already uses (RENDER-DIRECT-2c's patch matches by this, never by
/// `list_index`). `painter_key` is `prepare_objects`' own final sort key (`x + footprint.w + y +
/// footprint.h`, then `x` - iso.cpp), carried along so a patch can re-sort a bucket it inserted
/// into back into painter's order (RENDER-DIRECT-2c: appending at the end is not enough, a newly
/// added instance may belong before surviving ones in the same chunk).
struct StaticInstance {
    std::size_t list_index;
    formats::insel::PlacedObject key;
    std::pair<std::uint32_t, std::uint16_t> painter_key;
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

using ChunkInstanceBuckets = std::map<std::pair<int, int>, std::vector<StaticInstance>>;

/// `instances`' entries added into `buckets` (an existing map, appended to - not cleared), bucketed
/// by the chunk(s) their `draw.reach()` overlaps (`frame.hpp`'s own `visible_chunks`, so the chunk
/// grid matches the real chunk build exactly), `offset` (the island's placement at this group,
/// `Placement::offset`) applied to every anchor first. An instance whose reach spans more than one
/// chunk is listed under each.
void bucket_by_chunk_into(const std::vector<StaticInstance>& instances, std::pair<int, int> offset,
                          ChunkInstanceBuckets& buckets);

/// `bucket_by_chunk_into` into a fresh map.
ChunkInstanceBuckets bucket_by_chunk(const std::vector<StaticInstance>& instances,
                                     std::pair<int, int> offset);

/// RENDER-DIRECT-2c: `new_list`'s static-eligible entries that are not in `old_list` (by key -
/// `PlacedObject`'s own `operator<=>`, the same identity `prepare_objects`'
/// `std::set<PlacedObject>` already uses to dedup/sort `other`), already resolved and ready to
/// bucket.
std::vector<StaticInstance> added_static_instances(std::span<const Prepared> old_list,
                                                   std::span<const Prepared> new_list,
                                                   const formats::anlagen::AnlagenFile& anlagen,
                                                   std::size_t group, Lift lift);

/// RENDER-DIRECT-2c: `old_list`'s entries that are not in `new_list` (by key), as the key alone -
/// what `remove_static_instances` (below) needs to drop them from `buckets`.
std::vector<formats::insel::PlacedObject>
removed_static_instance_keys(std::span<const Prepared> old_list,
                             std::span<const Prepared> new_list);

/// Drops every `StaticInstance` in `buckets` whose `key` is one of `removed` (there may be more
/// than one bucket per key, when the instance's reach spanned a chunk boundary - every bucket is
/// checked, not just one). An empty bucket left behind is erased, so `buckets` only ever holds
/// chunks that still have at least one instance (matching what a fresh `bucket_by_chunk` would
/// give).
void remove_static_instances(ChunkInstanceBuckets& buckets,
                             const std::vector<formats::insel::PlacedObject>& removed);

/// `removed_static_instance_keys` then `remove_static_instances`, then `added_static_instances`
/// then `bucket_by_chunk_into` - the whole patch for one building change (place, demolish or
/// upgrade; upgrade is exactly a removed old key plus an added new key at the same tile, no special
/// case). `old_list`/`new_list` are the changed island's `IslandLists::other` before/after.
void patch_chunk_instances(ChunkInstanceBuckets& buckets, std::span<const Prepared> old_list,
                           std::span<const Prepared> new_list,
                           const formats::anlagen::AnlagenFile& anlagen, std::size_t group,
                           Lift lift, std::pair<int, int> offset);

} // namespace anno::render
