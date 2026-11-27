// RENDER-DIRECT-2a: resolve_static_instances + the shared Scene::object_record reproduce exactly
// what place_prepared + object_record give today, on every island's `other` list (the one
// objects_draws/emit_list reads - water lists are out of scope, RENDER-DIRECT-5's), at every
// group, on atlas.md §9's two .sze scenes (game files). lan5.txt is the third named scene, its
// own anno_app_tests case (chunk_instances_lan5_test.cpp) via the shared checker,
// anno/test/chunk_instances_check.hpp - the same split atlas_pages_check.hpp uses, for the same
// reason (a fixed map needs the app layer's loader).
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

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
