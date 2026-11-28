// RENDER-DIRECT-2a: resolve_static_instances + the shared Scene::object_record reproduce exactly
// what place_prepared + object_record give today, on every island's `other` list (the one
// objects_draws/emit_list reads - water lists are out of scope, RENDER-DIRECT-5's), at every
// group, on atlas.md §9's two .sze scenes (game files). lan5.txt is the third named scene, its
// own anno_app_tests case (chunk_instances_lan5_test.cpp) via the shared checker,
// anno/test/chunk_instances_check.hpp - the same split atlas_pages_check.hpp uses, for the same
// reason (a fixed map needs the app layer's loader).
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/render/chunk_instances.hpp>
#include <anno/test/chunk_instances_check.hpp>
#include <anno/test/game_dir.hpp>

#include "corpus_scene.hpp"

namespace render = anno::render;
using render::test::Game;
using render::test::kScenes;

TEST_CASE("resolve_static_instances merged with the live draws of the rest reproduces "
          "place_prepared's own full result, every island, every group (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    const Game game(*dir);
    std::size_t static_total = 0, dynamic_total = 0;
    for (const auto& sf : kScenes) {
        if (std::string_view(sf.path).ends_with(".scp")) {
            continue; // keep to the two .sze scenes atlas.md §9 names here; lan5.txt is its own
                      // test, the canyon .scp was never one of the three named scenes.
        }
        const auto loaded = game.scene(*dir / sf.path);
        anno::test::check_resolve_static_instances(*loaded.scene, sf.name, static_total,
                                                   dynamic_total);
    }
    MESSAGE("static instances resolved: " << static_total << ", dynamic (live): " << dynamic_total);
    CHECK(static_total > 0);
}

TEST_CASE("RENDER-DIRECT-2c step 0: resolve_static_instances + bucket_by_chunk wall time per "
          "island, worst case (provisional - PC not quiet, Release build, opt-in: "
          "ANNO_MEASURE_CHUNK_INSTANCE_TIME=1) (game files)") {
    // A wall-clock measurement (ORCH III, 2026-11-27 - "measure first" before writing the patch
    // diff), not a correctness claim, so it is opt-in rather than run in every gate - the same
    // ctest_verdict.py OPT_IN pattern adapter_memory_diag_test.cpp's texture-ceiling case uses.
    if (std::getenv("ANNO_MEASURE_CHUNK_INSTANCE_TIME") == nullptr) {
        MESSAGE("skipping: set ANNO_MEASURE_CHUNK_INSTANCE_TIME=1 to run this wall-clock "
                "measurement (provisional - costly to run on every gate, and meaningless unless "
                "the PC happens to be quiet)");
        return;
    }
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    const Game game(*dir);
    double worst_ms = 0.0;
    std::string worst_label;
    for (const auto& sf : kScenes) {
        if (std::string_view(sf.path).ends_with(".scp")) {
            continue;
        }
        const auto loaded = game.scene(*dir / sf.path);
        const render::Scene& scene = *loaded.scene;
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
                    continue;
                }
                const auto t0 = std::chrono::steady_clock::now();
                const auto statics =
                    render::resolve_static_instances(list, scene.anlagen, group, scene.lift);
                const auto buckets = render::bucket_by_chunk(statics, offset);
                const auto t1 = std::chrono::steady_clock::now();
                const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                if (ms > worst_ms) {
                    worst_ms = ms;
                    worst_label = std::string(sf.name) + " island " + std::to_string(island_i) +
                                  " group " + std::to_string(group) + " (" +
                                  std::to_string(list.size()) + " objects, " +
                                  std::to_string(buckets.size()) + " chunks)";
                }
            }
        }
    }
    MESSAGE("RENDER-DIRECT-2c step 0 (provisional): worst island/group = " << worst_label << ", "
                                                                           << worst_ms << " ms");
    CHECK(worst_ms >= 0.0); // a measurement, not a correctness claim
}

TEST_CASE("resolve_static_instances: a mutated anchor makes the equality check above fail "
          "(the check is live, not vacuous) (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    const Game game(*dir);
    const auto loaded = game.scene(*dir / kScenes[0].path);
    const render::Scene& scene = *loaded.scene;
    const auto& list = scene.island_lists(0).other;
    REQUIRE_FALSE(list.empty());
    auto statics = render::resolve_static_instances(list, scene.anlagen, 0, scene.lift);
    REQUIRE_FALSE(statics.empty());
    statics[0].draw.anchor.first += 7; // a deliberately wrong anchor
    const render::depth::Params dp = render::depth::params(0, scene.lift);
    const auto wrong = scene.object_record(statics[0].draw, {0, 0}, 0, scene.depth_test, dp);
    std::vector<render::SpriteDraw> correct_draws;
    render::place_prepared(
        std::span{&list[statics[0].list_index], 1}, scene.anlagen, 0, scene.lift, 0,
        [](int, int, int, int) { return true; }, correct_draws);
    REQUIRE_FALSE(correct_draws.empty());
    const auto correct = scene.object_record(correct_draws[0], {0, 0}, 0, scene.depth_test, dp);
    REQUIRE(wrong);
    REQUIRE(correct);
    CHECK_FALSE(*wrong == *correct);
}

namespace {

/// Matches chunk_instances_test.cpp's own check: every chunk key present in both, with its
/// instances' keys equal in the same order (painter's order, not only as a set).
void check_buckets_equal_real(const render::ChunkInstanceBuckets& a,
                              const render::ChunkInstanceBuckets& b) {
    REQUIRE(a.size() == b.size());
    for (const auto& [chunk, insts] : a) {
        CAPTURE(chunk.first);
        CAPTURE(chunk.second);
        REQUIRE(b.count(chunk) == 1);
        const auto& other_insts = b.at(chunk);
        REQUIRE(insts.size() == other_insts.size());
        for (std::size_t i = 0; i < insts.size(); ++i) {
            CHECK(insts[i].key == other_insts[i].key);
            CHECK(insts[i].draw.anchor == other_insts[i].draw.anchor);
        }
    }
}

std::pair<int, int> offset_of(const render::Scene& scene, std::size_t island_i, std::size_t group) {
    for (const auto& p : scene.layouts[group]) {
        if (p.index == island_i) {
            return p.offset;
        }
    }
    FAIL("island has no placement at this group");
    return {0, 0};
}

} // namespace

TEST_CASE("patch_chunk_instances on a real island (Single_01.sze island 0): place, demolish "
          "and upgrade each match a fresh build element for element (RENDER-DIRECT-2c, game "
          "files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    const Game game(*dir);
    const auto loaded = game.scene(*dir / kScenes[0].path); // Single_01.sze
    const render::Scene& scene = *loaded.scene;
    REQUIRE_FALSE(scene.islands.empty());
    constexpr std::size_t kIsland = 0, kGroup = 1; // the worst case measured, step 0
    const auto& before = scene.island_lists(kIsland).other;
    REQUIRE(before.size() >= 2);
    const auto offset = offset_of(scene, kIsland, kGroup);
    // Two DIFFERENT, already-static anlage ids this island actually uses (no fabricated entry;
    // `other` is sorted by anlage first, so a run of the same type at the start is common -
    // scan for the first index whose anlage differs from before[0]'s).
    const std::uint32_t id_a = before[0].first.anlage;
    REQUIRE(render::is_static_instance(scene.anlagen, id_a));
    std::size_t j = 1;
    while (j < before.size() &&
           (before[j].first.anlage == id_a ||
            !render::is_static_instance(scene.anlagen, before[j].first.anlage))) {
        ++j;
    }
    REQUIRE_MESSAGE(j < before.size(),
                    "no second, different, static anlage id found on this island");
    const std::uint32_t id_b = before[j].first.anlage;

    const auto fresh = [&](const std::vector<render::Prepared>& list) {
        return render::bucket_by_chunk(
            render::resolve_static_instances(list, scene.anlagen, kGroup, scene.lift), offset);
    };

    SUBCASE("place") {
        auto after = before;
        auto placed = before[0].first; // copy a real, already-static object's shape
        placed.anlage = static_cast<std::uint16_t>(id_a);
        placed.x = static_cast<std::uint16_t>(before[0].first.x + 3);
        placed.y = static_cast<std::uint16_t>(before[0].first.y + 3);
        after.push_back({placed, before[0].second});
        auto buckets = fresh(before);
        render::patch_chunk_instances(buckets, before, after, scene.anlagen, kGroup, scene.lift,
                                      offset);
        check_buckets_equal_real(buckets, fresh(after));
    }
    SUBCASE("demolish") {
        std::vector<render::Prepared> after(before.begin() + 1, before.end());
        auto buckets = fresh(before);
        render::patch_chunk_instances(buckets, before, after, scene.anlagen, kGroup, scene.lift,
                                      offset);
        check_buckets_equal_real(buckets, fresh(after));
    }
    SUBCASE("upgrade") {
        auto after = before;
        after[0].first.anlage = static_cast<std::uint16_t>(id_b);
        auto buckets = fresh(before);
        render::patch_chunk_instances(buckets, before, after, scene.anlagen, kGroup, scene.lift,
                                      offset);
        check_buckets_equal_real(buckets, fresh(after));
    }
}

TEST_CASE("RENDER-DIRECT-2c: patch_chunk_instances wall time for one change on Single_01 island "
          "0 group 1, provisional (opt-in: ANNO_MEASURE_CHUNK_INSTANCE_TIME=1) (game files)") {
    if (std::getenv("ANNO_MEASURE_CHUNK_INSTANCE_TIME") == nullptr) {
        MESSAGE("skipping: set ANNO_MEASURE_CHUNK_INSTANCE_TIME=1 to run this wall-clock "
                "measurement");
        return;
    }
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return;
    }
    const Game game(*dir);
    const auto loaded = game.scene(*dir / kScenes[0].path);
    const render::Scene& scene = *loaded.scene;
    constexpr std::size_t kIsland = 0, kGroup = 1;
    const auto& before = scene.island_lists(kIsland).other;
    REQUIRE_FALSE(before.empty());
    const auto offset = offset_of(scene, kIsland, kGroup);
    auto after = before;
    after[0].first.y = static_cast<std::uint16_t>(after[0].first.y + 1); // one object moves
    auto buckets = render::bucket_by_chunk(
        render::resolve_static_instances(before, scene.anlagen, kGroup, scene.lift), offset);
    const auto t0 = std::chrono::steady_clock::now();
    render::patch_chunk_instances(buckets, before, after, scene.anlagen, kGroup, scene.lift,
                                  offset);
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    MESSAGE("RENDER-DIRECT-2c patch time (provisional), Single_01 island 0 group 1 ("
            << before.size() << " objects): " << ms << " ms");
    CHECK(ms >= 0.0);
}
