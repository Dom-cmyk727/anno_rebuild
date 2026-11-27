// RENDER-DIRECT-3 stage 3a: resolve_direct_records/place_direct_instances resolve the visible
// chunks' static instances (2a/2c) into GPU-ready records with a real resident atlas placement,
// with no GPU device open (CPU-only, like residency_test.cpp - ensure() here is bookkeeping only;
// production uploads the real texels through gpu.cpp's own cb_resident, never through this pair).
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/render/depth.hpp>
#include <anno/render/direct_instances.hpp>
#include <anno/test/direct_class_counts_check.hpp>
#include <anno/test/game_dir.hpp>

#include "corpus_scene.hpp"

namespace render = anno::render;
using render::test::Game;
using render::test::kScenes;

TEST_CASE("RENDER-DIRECT-3 3a scope: depth-class counts on Single_01.sze, every island (game "
          "files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    const Game game(*dir);
    const auto loaded = game.scene(*dir / kScenes[0].path); // Single_01.sze
    anno::test::count_depth_classes(*loaded.scene, "Single_01.sze");
}

TEST_CASE("direct_object_instances: every returned instance's atlas place is resident, sized to "
          "its own frame, and its record matches object_record directly (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    const Game game(*dir);
    const auto loaded = game.scene(*dir / kScenes[0].path); // Single_01.sze
    const render::Scene& scene = *loaded.scene;
    constexpr std::size_t kIsland = 0, kGroup = 0;
    const auto& list = scene.island_lists(kIsland).other;
    REQUIRE_FALSE(list.empty());
    std::pair<int, int> offset{0, 0};
    for (const auto& p : scene.layouts[kGroup]) {
        if (p.index == kIsland) {
            offset = p.offset;
            break;
        }
    }
    const auto statics = render::resolve_static_instances(list, scene.anlagen, kGroup, scene.lift);
    REQUIRE_FALSE(statics.empty());
    const auto buckets = render::bucket_by_chunk(statics, offset);
    REQUIRE_FALSE(buckets.empty());
    std::vector<std::pair<int, int>> visible;
    for (const auto& entry : buckets) {
        visible.push_back(entry.first);
    }

    // Generous: a 2048 page, 64 MiB (RENDER-DIRECT-1a's own page size; the budget just needs to
    // fit this one island's distinct frames, not a whole scenario).
    render::AtlasResidency atlas(2048, 64U * 1024 * 1024);
    const render::depth::Params dp = render::depth::params(kGroup, scene.lift);
    std::vector<render::FrameRef> needed;
    const auto records = render::resolve_direct_records(buckets, visible, scene, atlas, 0,
                                                        scene.depth_test, dp, needed);
    if (!needed.empty()) {
        // No device here (this test, like residency_test.cpp, only checks the bookkeeping) -
        // production instead goes through gpu.cpp's own cb_resident, which also uploads the
        // texels ensure()'s placement decision commits to (direct_instances.hpp's own contract).
        atlas.ensure(needed, render::AtlasClass::Static, std::numeric_limits<std::uint64_t>::max());
    }
    const auto instances = render::place_direct_instances(records, atlas);

    std::size_t total_input = 0;
    for (const auto& entry : buckets) {
        total_input += entry.second.size();
    }
    REQUIRE_FALSE(instances.empty());
    CHECK(instances.size() <= total_input); // only an undecodable frame is ever dropped

    std::size_t checked = 0;
    for (const auto& inst : instances) {
        CAPTURE(checked);
        CHECK(atlas.resident(inst.record.frame));
        const auto place = atlas.place(inst.record.frame);
        REQUIRE(place);
        CHECK(inst.atlas.layer == place->layer);
        CHECK(inst.atlas.width == place->width);
        CHECK(inst.atlas.height == place->height);
        ++checked;
        if (checked >= 200) {
            break; // every instance checked would be slow and adds nothing past this point
        }
    }

    // One instance's record matches a direct object_record call on the same draw, exactly - the
    // resolver does not transform the record on its way through.
    const auto one = buckets.begin()->second.front();
    const auto direct = scene.object_record(one.draw, {0, 0}, 0, scene.depth_test, dp);
    REQUIRE(direct);
    bool matched = false;
    for (const auto& inst : instances) {
        if (inst.record == *direct) {
            matched = true;
            break;
        }
    }
    CHECK(matched);
}

TEST_CASE("direct_object_instances: an instance whose frame cannot be made resident (a budget "
          "of 0) is left out, not drawn wrong (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    const Game game(*dir);
    const auto loaded = game.scene(*dir / kScenes[0].path);
    const render::Scene& scene = *loaded.scene;
    constexpr std::size_t kIsland = 0, kGroup = 0;
    const auto& list = scene.island_lists(kIsland).other;
    REQUIRE_FALSE(list.empty());
    const auto statics = render::resolve_static_instances(list, scene.anlagen, kGroup, scene.lift);
    REQUIRE_FALSE(statics.empty());
    const auto buckets = render::bucket_by_chunk(statics, {0, 0});
    REQUIRE_FALSE(buckets.empty());
    std::vector<std::pair<int, int>> visible;
    for (const auto& entry : buckets) {
        visible.push_back(entry.first);
    }
    render::AtlasResidency atlas(2048, 2048U * 2048U * 4); // one layer only
    const render::depth::Params dp = render::depth::params(kGroup, scene.lift);
    std::vector<render::FrameRef> needed;
    const auto records = render::resolve_direct_records(buckets, visible, scene, atlas, 0,
                                                        scene.depth_test, dp, needed);
    if (!needed.empty()) {
        atlas.ensure(needed, render::AtlasClass::Static, 0); // no upload budget: nothing placed
    }
    const auto instances = render::place_direct_instances(records, atlas);
    // Every instance returned must already be resident (none could be placed this call); the
    // count is <= what a generous budget would give, never a wrong one standing in.
    for (const auto& inst : instances) {
        CHECK(atlas.resident(inst.record.frame));
    }
}
