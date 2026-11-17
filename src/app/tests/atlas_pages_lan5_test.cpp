// RENDER-DIRECT-1b step 4 (review F2): the texel-exact proof on data/maps/lan5.txt, the packet's
// third named scene. Lives here, not in anno_render_tests (atlas_pages_corpus_test.cpp), because
// a fixed map's islands come from several .scp templates resolved by the app layer's own loader
// (app::load_play/load_placed_scene) - anno_render sits below anno_app_core (which depends on
// it), so anno_render_tests must not depend on anno_app_core to stay below it too
// (anno_render_gpu_tests's own link to anno_app_core is the one, pre-existing exception, not a
// precedent to extend); anno_app_tests already links both. The frame-gathering and the checker
// (collect_static_water, check_pages_against_source) are shared with atlas_pages_corpus_test.cpp
// via anno/test/atlas_pages_check.hpp (src/test_support, header-only) rather than copied.
#include <filesystem>

#include <doctest/doctest.h>

#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/render/atlas_pages.hpp>
#include <anno/render/frame_table.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/atlas_pages_check.hpp>
#include <anno/test/game_dir.hpp>

namespace app = anno::app;
namespace render = anno::render;

TEST_CASE("atlas pages: lan5.txt's static+water is texel exact (game files)" *
          doctest::test_suite("heavy")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const std::filesystem::path lan5 =
        std::filesystem::path(ANNO_REPO_DIR) / "data" / "maps" / "lan5.txt";
    std::error_code ec;
    // data/maps/lan5.txt is a repo file, always checked in; missing it is a broken checkout, a
    // failure, not a skip (the same rule as a named game file missing while ANNO1503_GAME_DIR is
    // set, CLAUDE.md - only an unset ANNO1503_GAME_DIR, checked above, skips).
    if (!std::filesystem::exists(lan5, ec)) {
        FAIL("lan5.txt is not at the expected repo path: " << lan5.string());
    }

    // The same two calls bench_frame's own load_any_scene uses for its ".txt" branch (no second
    // loading path): load_play resolves the layout's templates into placed islands,
    // load_placed_scene builds the render::Scene from them.
    auto play = app::load_play(*dir, lan5, app::FixedPlayOptions{});
    REQUIRE_MESSAGE(bool(play), "load_play: " << (play ? "" : play.error()));
    auto loaded = app::load_placed_scene(*dir, play.value().name, play.value().placed);
    REQUIRE_MESSAGE(bool(loaded), "load_placed_scene: " << (loaded ? "" : loaded.error()));
    const render::Scene& scene = *loaded.value().scene;

    // load_placed_scene does not populate scene.units (atlas.md section 5's own finding), so a
    // fixed map has no figures to check - only its static+water class.
    CHECK_MESSAGE(scene.units.empty(),
                  "lan5.txt: load_placed_scene does not populate units (expected)");

    for (std::uint32_t group = 0; group < render::kGroups; ++group) {
        render::SpriteCache cache;
        render::FrameTable table;
        const auto frames = anno::test::collect_static_water(scene, group, cache, table);
        const auto pages = render::build_atlas_pages(frames, 2048, 2, 4);
        anno::test::check_pages_against_source(frames, pages, "lan5.txt static+water");
    }
}
