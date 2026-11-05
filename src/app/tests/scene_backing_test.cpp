// Generation ownership of backing storage (round dv, R3; docs/app/game-shell.md): a scene loaded
// from game files owns the bytes its islands and Anlagen.dat view. A derived generation, and the
// chunk jobs that outlive the loader and the base generation, still read valid bytes. Run under
// ASan, a dangling view is a heap-use-after-free. Needs the game files (skips without them).
#include <memory>

#include <doctest/doctest.h>

#include <anno/app/loader.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

namespace render = anno::render;

TEST_CASE(
    "backing: a derived generation keeps reading its islands after the loaded scene is released "
    "(game files)" *
    doctest::test_suite("backing")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = anno::app::load_scene(*dir, *dir / "Scenes" / "0200Single" / "Single_01.sze");
    REQUIRE(loaded.ok());
    anno::app::LoadedScene ls = std::move(loaded).value();
    // The bytes moved into the scene: nothing is left in the loader's staging.
    REQUIRE(ls.buffers.empty());
    std::shared_ptr<const render::Scene> next;
    {
        std::shared_ptr<const render::Scene> base(std::move(ls.scene));
        // A generation that rebuilds one island's object lists (its records are the base's).
        render::Scene::IslandUpdate update;
        update.index = 0;
        update.island = base->islands.front();
        update.lists = render::Scene::build_island_lists(update.island, base->anlagen);
        std::vector<render::Scene::IslandUpdate> updates;
        updates.push_back(std::move(update));
        next = base->with_islands(std::move(updates));
    }
    // The base generation and the loader are gone; `next` is the only holder left.
    ls = anno::app::LoadedScene{};
    REQUIRE(next != nullptr);
    CHECK(next->generation() == 1);
    for (const auto& island : next->islands) {
        CHECK_FALSE(island.head.empty());
    }

    const auto view = next->bounds(2);
    REQUIRE(view.has_value());
    // Synchronous chunks read the island's tiles and the Anlagen sprites through `next`.
    render::FrameRenderer frames(1);
    frames.update_chunks(next, *view, 2);
    const auto keys = render::chunk_keys_touching(*next, *view, 2);
    REQUIRE_FALSE(keys.empty());
    CHECK(frames.chunk(keys.front()) != nullptr);
}

TEST_CASE("backing: queued chunk jobs read valid bytes after the base generation is released "
          "(game files)" *
          doctest::test_suite("backing")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = anno::app::load_scene(*dir, *dir / "Scenes" / "0200Single" / "Single_01.sze");
    REQUIRE(loaded.ok());
    anno::app::LoadedScene ls = std::move(loaded).value();
    std::shared_ptr<const render::Scene> scene(std::move(ls.scene));
    ls = anno::app::LoadedScene{};
    const auto view = scene->bounds(2);
    REQUIRE(view.has_value());
    const auto keys = render::chunk_keys_touching(*scene, *view, 2);
    REQUIRE_FALSE(keys.empty());

    // The chunk jobs queue on the workers; the renderer and the test drop the scene while they run.
    render::FrameRenderer frames(2);
    auto owned = std::make_unique<render::ChunkWorkers>(*scene, 2);
    render::ChunkWorkers* workers = owned.get();
    frames.set_workers(std::move(owned));
    frames.update_chunks(scene, *view, 2);
    scene.reset();
    // Every job finished with the bytes it read still valid (ASan reports a dangling view here).
    workers->wait_idle();
}
