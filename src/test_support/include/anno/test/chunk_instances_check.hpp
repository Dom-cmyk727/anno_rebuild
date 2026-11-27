// Shared between anno_render_tests (chunk_instances_corpus_test.cpp, the two atlas.md §9 .sze
// scenes) and anno_app_tests (the data/maps/lan5.txt fixed-map case, which needs the app layer's
// own loader, same reason atlas_pages_check.hpp is split this way). Header-only: test_support
// stays free of a render dependency; only a TU that already links anno_render/anno_assets
// includes this.
#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/render/chunk_instances.hpp>
#include <anno/render/scene.hpp>

namespace anno::test {

namespace detail {

/// `object_record` applied to `draws`, in `draws`' own order (place_prepared never drops/reorders
/// an input, only skips an undecodable frame inside object_record - `rec` is nullopt then, same
/// as `objects_draws` does, so the skip is identical on both sides of the comparison below).
inline std::vector<render::ObjectRecord>
chunk_instances_records_of(const render::Scene& scene, const std::vector<render::SpriteDraw>& draws,
                           std::pair<int, int> offset, const render::depth::Params& dp) {
    std::vector<render::ObjectRecord> out;
    for (const auto& d : draws) {
        if (const auto rec = scene.object_record(d, offset, 0, scene.depth_test, dp)) {
            out.push_back(*rec);
        }
    }
    return out;
}

} // namespace detail

/// RENDER-DIRECT-2a's corpus proof: for every island of `scene` with a non-empty `other` list, at
/// every group it is placed at, `resolve_static_instances`'s entries merged with a fresh
/// `place_prepared` of the rest (by list index, restoring the list's own order) must give the
/// exact same `ObjectRecord`s as resolving the whole list live - the claim "a resolved-once-at-
/// load SpriteDraw and one resolved fresh by place_prepared produce an identical record through
/// the same code" (Scene::object_record), checked at the island-list granularity (chunk
/// bucketing itself is checked separately, against frame.hpp's own visible_chunks, not
/// re-derived here - ORCH III, 2026-11-27, ADR this avoids a second per-chunk rasteriser).
/// Adds to `static_total`/`dynamic_total` so a caller can report counts across scenes.
inline void check_resolve_static_instances(const render::Scene& scene, std::string_view scene_name,
                                           std::size_t& static_total, std::size_t& dynamic_total) {
    for (std::size_t island_i = 0; island_i < scene.islands.size(); ++island_i) {
        const auto& list = scene.island_lists(island_i).other;
        if (list.empty()) {
            continue;
        }
        for (std::size_t group = 0; group < render::kGroups; ++group) {
            std::pair<int, int> offset{0, 0};
            bool placed = false;
            for (const auto& p : scene.layouts[group]) {
                if (p.index == island_i) {
                    offset = p.offset;
                    placed = true;
                    break;
                }
            }
            if (!placed) {
                continue; // the island has no placement at this group (set_levels)
            }
            const render::depth::Params dp = render::depth::params(group, scene.lift);

            // "Old": place_prepared on the whole list, as emit_list does per chunk, with no
            // visibility filter (the filter never changes which object resolves to what - only
            // whether it is kept; dropping it compares the resolution itself).
            std::vector<render::SpriteDraw> live;
            render::place_prepared(
                list, scene.anlagen, group, scene.lift, 0, [](int, int, int, int) { return true; },
                live);
            const auto old_records = detail::chunk_instances_records_of(scene, live, offset, dp);

            // "New": resolve_static_instances for the eligible entries (used as-is - this is
            // what the comparison is actually checking), one-at-a-time place_prepared for the
            // rest, merged back into the list's own order by list_index.
            const auto statics =
                render::resolve_static_instances(list, scene.anlagen, group, scene.lift);
            static_total += statics.size();
            std::vector<bool> is_static(list.size(), false);
            for (const auto& s : statics) {
                is_static[s.list_index] = true;
            }
            std::vector<std::pair<std::size_t, render::SpriteDraw>> merged;
            for (const auto& s : statics) {
                merged.emplace_back(s.list_index, s.draw);
            }
            for (std::size_t i = 0; i < list.size(); ++i) {
                if (is_static[i]) {
                    continue;
                }
                ++dynamic_total;
                std::vector<render::SpriteDraw> one;
                render::place_prepared(
                    std::span{&list[i], 1}, scene.anlagen, group, scene.lift, 0,
                    [](int, int, int, int) { return true; }, one);
                for (auto& d : one) {
                    merged.emplace_back(i, std::move(d));
                }
            }
            // stable: a static object's shadow+main pair (both tagged with the same list_index,
            // resolve_static_instances/place_prepared emit shadow first) must keep that relative
            // order, matching "old"'s per-object order exactly.
            std::stable_sort(merged.begin(), merged.end(),
                             [](const auto& a, const auto& b) { return a.first < b.first; });
            std::vector<render::SpriteDraw> new_draws;
            new_draws.reserve(merged.size());
            for (auto& [idx, d] : merged) {
                new_draws.push_back(std::move(d));
            }
            const auto new_records =
                detail::chunk_instances_records_of(scene, new_draws, offset, dp);

            CAPTURE(scene_name);
            CAPTURE(island_i);
            CAPTURE(group);
            REQUIRE(new_records.size() == old_records.size());
            for (std::size_t k = 0; k < new_records.size(); ++k) {
                CHECK(new_records[k] == old_records[k]);
            }
        }
    }
}

} // namespace anno::test
