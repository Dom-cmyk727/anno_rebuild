// RENDER-DIRECT-2a: the corpus proof on data/maps/lan5.txt, the packet's third named scene
// (atlas.md §9). Lives here, not in anno_render_tests, for the same reason
// atlas_pages_lan5_test.cpp does: a fixed map's islands come from several .scp templates
// resolved by the app layer's own loader (app::load_play/load_placed_scene).
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

#include <doctest/doctest.h>

#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/render/chunk_instances.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/chunk_instances_check.hpp>
#include <anno/test/direct_class_counts_check.hpp>
#include <anno/test/game_dir.hpp>

namespace app = anno::app;
namespace render = anno::render;

TEST_CASE("RENDER-DIRECT-3 3a scope: depth-class counts on lan5.txt, every island (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const std::filesystem::path lan5 =
        std::filesystem::path(ANNO_REPO_DIR) / "data" / "maps" / "lan5.txt";
    auto play = app::load_play(*dir, lan5, app::FixedPlayOptions{});
    REQUIRE_MESSAGE(bool(play), "load_play: " << (play ? "" : play.error()));
    auto loaded = app::load_placed_scene(*dir, play.value().name, play.value().placed);
    REQUIRE_MESSAGE(bool(loaded), "load_placed_scene: " << (loaded ? "" : loaded.error()));
    anno::test::count_depth_classes(*loaded.value().scene, "lan5.txt");
}

TEST_CASE("resolve_static_instances on lan5.txt reproduces place_prepared's own full result, "
          "every island, every group (game files)" *
          doctest::test_suite("heavy")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const std::filesystem::path lan5 =
        std::filesystem::path(ANNO_REPO_DIR) / "data" / "maps" / "lan5.txt";
    std::error_code ec;
    // A repo file, always checked in; missing it is a broken checkout, a failure, not a skip
    // (CLAUDE.md - only an unset ANNO1503_GAME_DIR, checked above, skips).
    if (!std::filesystem::exists(lan5, ec)) {
        FAIL("lan5.txt is not at the expected repo path: " << lan5.string());
    }

    auto play = app::load_play(*dir, lan5, app::FixedPlayOptions{});
    REQUIRE_MESSAGE(bool(play), "load_play: " << (play ? "" : play.error()));
    auto loaded = app::load_placed_scene(*dir, play.value().name, play.value().placed);
    REQUIRE_MESSAGE(bool(loaded), "load_placed_scene: " << (loaded ? "" : loaded.error()));
    const render::Scene& scene = *loaded.value().scene;

    std::size_t static_total = 0, dynamic_total = 0;
    anno::test::check_resolve_static_instances(scene, "lan5.txt", static_total, dynamic_total);
    MESSAGE("static instances resolved: " << static_total << ", dynamic (live): " << dynamic_total);
    CHECK(static_total > 0);
}

TEST_CASE("RENDER-DIRECT-2c step 0: resolve_static_instances + bucket_by_chunk wall time per "
          "island on lan5.txt, worst case (provisional - PC not quiet, Release build, opt-in: "
          "ANNO_MEASURE_CHUNK_INSTANCE_TIME=1) (game files)" *
          doctest::test_suite("heavy")) {
    // A wall-clock measurement, not a correctness claim - opt-in, like
    // chunk_instances_corpus_test.cpp's sibling case (same env var, same reasoning).
    if (std::getenv("ANNO_MEASURE_CHUNK_INSTANCE_TIME") == nullptr) {
        MESSAGE("skipping: set ANNO_MEASURE_CHUNK_INSTANCE_TIME=1 to run this wall-clock "
                "measurement");
        return;
    }
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const std::filesystem::path lan5 =
        std::filesystem::path(ANNO_REPO_DIR) / "data" / "maps" / "lan5.txt";
    auto play = app::load_play(*dir, lan5, app::FixedPlayOptions{});
    REQUIRE_MESSAGE(bool(play), "load_play: " << (play ? "" : play.error()));
    auto loaded = app::load_placed_scene(*dir, play.value().name, play.value().placed);
    REQUIRE_MESSAGE(bool(loaded), "load_placed_scene: " << (loaded ? "" : loaded.error()));
    const render::Scene& scene = *loaded.value().scene;

    double worst_ms = 0.0;
    std::string worst_label;
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
                worst_label = "lan5.txt island " + std::to_string(island_i) + " group " +
                              std::to_string(group) + " (" + std::to_string(list.size()) +
                              " objects, " + std::to_string(buckets.size()) + " chunks)";
            }
        }
    }
    MESSAGE("RENDER-DIRECT-2c step 0 (provisional): worst island/group = " << worst_label << ", "
                                                                           << worst_ms << " ms");
    CHECK(worst_ms >= 0.0);
}
