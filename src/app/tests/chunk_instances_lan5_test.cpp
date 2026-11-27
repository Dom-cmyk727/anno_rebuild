// RENDER-DIRECT-2a: the corpus proof on data/maps/lan5.txt, the packet's third named scene
// (atlas.md §9). Lives here, not in anno_render_tests, for the same reason
// atlas_pages_lan5_test.cpp does: a fixed map's islands come from several .scp templates
// resolved by the app layer's own loader (app::load_play/load_placed_scene).
#include <cstddef>
#include <filesystem>

#include <doctest/doctest.h>

#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/chunk_instances_check.hpp>
#include <anno/test/game_dir.hpp>

namespace app = anno::app;
namespace render = anno::render;

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
