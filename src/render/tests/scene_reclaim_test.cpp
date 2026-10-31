// Final destruction of scene generations (round dw, DW3a; docs/render/gpu-chunks.md and
// docs/app/game-shell.md, "Retained work and reclamation"). The last owner of a generation may be
// the window thread, a frame's chunk job, or a worker; the destruction must run on the process
// reclaimer in every case. Synthetic: one 8x8 island with no records, no game files.
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>

namespace render = anno::render;

namespace {

std::shared_ptr<render::Scene> synthetic_scene() {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>(
        1, std::vector<std::uint8_t>(64, 0));
    anno::formats::insel::Island island;
    island.head = (*bytes)[0];
    island.number = 0;
    island.width = 8;
    island.height = 8;
    return render::Scene::create("reclaim", {island}, anno::assets::island::GroundTextures{},
                                 anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
}

/// Waits (bounded) until the reclaimer has finished `count` destructions beyond `before`.
bool wait_completed(std::uint64_t before, std::uint64_t count) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (render::SceneReclaimer::shared().stats().completed < before + count) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

} // namespace

TEST_CASE(
    "scene reclaim: the window's release of the last generation is destroyed on the reclaimer" *
    doctest::test_suite("scene_reclaim")) {
    const std::uint64_t before = render::SceneReclaimer::shared().stats().completed;
    std::shared_ptr<const render::Scene> scene = synthetic_scene();
    std::shared_ptr<const render::Scene> other = scene;
    scene.reset();
    CHECK(render::SceneReclaimer::shared().stats().completed ==
          before); // the release only enqueued
    other.reset();
    REQUIRE(wait_completed(before, 1));
    CHECK(render::SceneReclaimer::shared().last_thread() != std::this_thread::get_id());
}

TEST_CASE(
    "scene reclaim: the last generation released by a chunk worker is destroyed on the reclaimer" *
    doctest::test_suite("scene_reclaim")) {
    const std::uint64_t before = render::SceneReclaimer::shared().stats().completed;
    std::shared_ptr<const render::Scene> first = synthetic_scene();
    render::FrameRenderer frames(2);
    auto owned = std::make_unique<render::ChunkWorkers>(*first, 2);
    render::ChunkWorkers* workers = owned.get();
    frames.set_workers(std::move(owned));
    const render::Rect view{0, 0, render::kChunk, render::kChunk};
    frames.update_chunks(first, view,
                         2); // the chunk jobs and the renderer hold the first generation
    first.reset();
    // The renderer moves on: its own reference goes, and the jobs' references may be the last.
    std::shared_ptr<const render::Scene> second = synthetic_scene();
    frames.update_chunks(second, view, 2);
    workers->wait_idle();
    REQUIRE(wait_completed(before, 1));
    CHECK(render::SceneReclaimer::shared().last_thread() != std::this_thread::get_id());
}

TEST_CASE("scene reclaim: destructions are counted as queued, waiting, in flight and completed" *
          doctest::test_suite("scene_reclaim")) {
    const auto start = render::SceneReclaimer::shared().stats();
    std::vector<std::shared_ptr<const render::Scene>> scenes;
    for (int i = 0; i < 3; ++i) {
        scenes.push_back(synthetic_scene());
    }
    scenes.clear();
    REQUIRE(wait_completed(start.completed, 3));
    const auto end = render::SceneReclaimer::shared().stats();
    CHECK(end.queued == start.queued + 3);
    CHECK(end.completed == start.completed + 3);
    CHECK(end.waiting == 0);
    CHECK(end.in_flight == 0);
}
