#include <algorithm>
#include <iterator>
#include <set>

#include <anno/render/chunk_instances.hpp>
#include <anno/render/frame.hpp>

namespace anno::render {

bool is_static_instance(const formats::anlagen::AnlagenFile& anlagen, std::uint32_t anlage_id) {
    const formats::figuren::Entry* entry = anlagen.entry(anlage_id);
    if (entry == nullptr || entry->gfx.empty()) {
        return true; // place_prepared always skips these too (failed/invisible), never time-varying
    }
    const auto [n, ms] = animation(entry->gfx.front());
    return n == 1 || ms == 0;
}

std::vector<StaticInstance> resolve_static_instances(std::span<const Prepared> list,
                                                     const formats::anlagen::AnlagenFile& anlagen,
                                                     std::size_t group, Lift lift) {
    std::vector<StaticInstance> out;
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (!is_static_instance(anlagen, list[i].first.anlage)) {
            continue;
        }
        std::vector<SpriteDraw> draws;
        // One object at a time, as Scene::object_frame already does: place_prepared's visible()
        // is only a culling filter, never part of the resolved result.
        place_prepared(
            list.subspan(i, 1), anlagen, group, lift, 0, [](int, int, int, int) { return true; },
            draws);
        const std::pair<std::uint32_t, std::uint16_t> painter_key{
            std::uint32_t{list[i].first.x} + list[i].second.first + list[i].first.y +
                list[i].second.second,
            list[i].first.x};
        for (SpriteDraw& d : draws) {
            out.push_back({i, list[i].first, painter_key, std::move(d)});
        }
    }
    return out;
}

void bucket_by_chunk_into(const std::vector<StaticInstance>& instances, std::pair<int, int> offset,
                          ChunkInstanceBuckets& buckets) {
    std::set<std::pair<int, int>> touched;
    for (const StaticInstance& inst : instances) {
        StaticInstance global = inst;
        global.draw.anchor.first += offset.first;
        global.draw.anchor.second += offset.second;
        const auto r = global.draw.reach();
        for (const auto& chunk : visible_chunks(Rect{r[0], r[1], r[2], r[3]})) {
            buckets[chunk].push_back(global);
            touched.insert(chunk);
        }
    }
    // A chunk a patch inserted into may now have a newly added instance out of painter's order
    // relative to the ones already there (appended at the end, above) - re-sort it back into
    // painter's order (iso.cpp's own key, StaticInstance::painter_key), stable so instances with
    // an equal key (the same tile stack) keep their existing relative order. A no-op resort of
    // already-sorted data on a from-scratch build (every chunk here is "touched").
    for (const auto& chunk : touched) {
        // Tie-break by `key` (PlacedObject's own order): prepare_objects' stable_sort ties
        // preserve the std::set's PlacedObject order, which `key` reproduces exactly, so a tie
        // here resolves the same way regardless of whether the instance survived the patch or
        // was just added.
        std::stable_sort(buckets[chunk].begin(), buckets[chunk].end(),
                         [](const StaticInstance& a, const StaticInstance& b) {
                             return std::pair{a.painter_key, a.key} <
                                    std::pair{b.painter_key, b.key};
                         });
    }
}

ChunkInstanceBuckets bucket_by_chunk(const std::vector<StaticInstance>& instances,
                                     std::pair<int, int> offset) {
    ChunkInstanceBuckets out;
    bucket_by_chunk_into(instances, offset, out);
    return out;
}

namespace {

/// `list` sorted by its own `PlacedObject` key - a real sort, needed: `prepare_objects` dedups
/// and sorts by `PlacedObject` via a `std::set` first, but then re-filters and `std::stable_sort`s
/// the result by painter's order (`x + footprint.w + y + footprint.h, x` - iso.cpp), so `other`
/// itself is in painter's order, not `PlacedObject` order (`std::set_difference` needs both
/// inputs sorted by the same comparator it is given, here `PlacedObject`'s own `<`).
std::vector<formats::insel::PlacedObject> sorted_keys(std::span<const Prepared> list) {
    std::vector<formats::insel::PlacedObject> keys;
    keys.reserve(list.size());
    for (const Prepared& p : list) {
        keys.push_back(p.first);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

} // namespace

std::vector<formats::insel::PlacedObject>
removed_static_instance_keys(std::span<const Prepared> old_list,
                             std::span<const Prepared> new_list) {
    const auto old_keys = sorted_keys(old_list);
    const auto new_keys = sorted_keys(new_list);
    std::vector<formats::insel::PlacedObject> removed;
    std::set_difference(old_keys.begin(), old_keys.end(), new_keys.begin(), new_keys.end(),
                        std::back_inserter(removed));
    return removed;
}

std::vector<StaticInstance> added_static_instances(std::span<const Prepared> old_list,
                                                   std::span<const Prepared> new_list,
                                                   const formats::anlagen::AnlagenFile& anlagen,
                                                   std::size_t group, Lift lift) {
    const auto old_keys = sorted_keys(old_list);
    const auto new_keys = sorted_keys(new_list);
    std::vector<formats::insel::PlacedObject> added_keys;
    std::set_difference(new_keys.begin(), new_keys.end(), old_keys.begin(), old_keys.end(),
                        std::back_inserter(added_keys));
    std::vector<Prepared> added_list;
    added_list.reserve(added_keys.size());
    for (const Prepared& p : new_list) {
        if (std::binary_search(added_keys.begin(), added_keys.end(), p.first)) {
            added_list.push_back(p);
        }
    }
    return resolve_static_instances(added_list, anlagen, group, lift);
}

void remove_static_instances(ChunkInstanceBuckets& buckets,
                             const std::vector<formats::insel::PlacedObject>& removed) {
    if (removed.empty()) {
        return;
    }
    for (auto it = buckets.begin(); it != buckets.end();) {
        std::erase_if(it->second, [&](const StaticInstance& inst) {
            return std::binary_search(removed.begin(), removed.end(), inst.key);
        });
        it = it->second.empty() ? buckets.erase(it) : std::next(it);
    }
}

void patch_chunk_instances(ChunkInstanceBuckets& buckets, std::span<const Prepared> old_list,
                           std::span<const Prepared> new_list,
                           const formats::anlagen::AnlagenFile& anlagen, std::size_t group,
                           Lift lift, std::pair<int, int> offset) {
    // Sorted once, shared by both diffs below - removed_static_instance_keys/
    // added_static_instances each sort their own copy too (for standalone testing/use), but a
    // patch call sorts the (potentially large) lists only once.
    const auto old_keys = sorted_keys(old_list);
    const auto new_keys = sorted_keys(new_list);
    std::vector<formats::insel::PlacedObject> removed;
    std::set_difference(old_keys.begin(), old_keys.end(), new_keys.begin(), new_keys.end(),
                        std::back_inserter(removed));
    remove_static_instances(buckets, removed);

    std::vector<formats::insel::PlacedObject> added_keys;
    std::set_difference(new_keys.begin(), new_keys.end(), old_keys.begin(), old_keys.end(),
                        std::back_inserter(added_keys));
    std::vector<Prepared> added_list;
    added_list.reserve(added_keys.size());
    for (const Prepared& p : new_list) {
        if (std::binary_search(added_keys.begin(), added_keys.end(), p.first)) {
            added_list.push_back(p);
        }
    }
    bucket_by_chunk_into(resolve_static_instances(added_list, anlagen, group, lift), offset,
                         buckets);
}

} // namespace anno::render
