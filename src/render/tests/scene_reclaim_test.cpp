// Final destruction of scene generations (round dw, DW3a; docs/render/gpu-chunks.md and
// docs/app/game-shell.md, "Retained work and reclamation"). The last owner of a generation may be
// the window thread, a frame's chunk job, or a worker; the destruction must run on the process
// reclaimer in every case. Synthetic: one 8x8 island with no records, no game files.
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/reclaim_identity.hpp>

namespace render = anno::render;
using anno::test::settle_reclaimer;

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

} // namespace

TEST_CASE(
    "scene reclaim: the window's release of the last generation is destroyed on the reclaimer" *
    doctest::test_suite("scene_reclaim")) {
    auto& reclaimer = render::SceneReclaimer::shared();
    REQUIRE(settle_reclaimer());
    const auto before = reclaimer.stats();
    std::shared_ptr<const render::Scene> scene = synthetic_scene();
    std::shared_ptr<const render::Scene> other = scene;
    scene.reset();
    CHECK(reclaimer.stats().queued == before.queued); // the other owner still holds it
    CHECK(reclaimer.stats().completed == before.completed);
    other.reset();
    REQUIRE(settle_reclaimer());
    CHECK(reclaimer.stats().completed == before.completed + 1);
    CHECK(reclaimer.last_thread() != std::this_thread::get_id());
}

TEST_CASE(
    "scene reclaim: the last generation released by a chunk worker is destroyed on the reclaimer" *
    doctest::test_suite("scene_reclaim")) {
    auto& reclaimer = render::SceneReclaimer::shared();
    REQUIRE(settle_reclaimer());
    const auto before = reclaimer.stats();
    auto scene = synthetic_scene();
    std::mutex gate;
    std::promise<void> entered;
    auto started = entered.get_future();
    {
        render::ChunkWorkers workers(1);
        std::unique_lock hold_worker(gate);
        workers.set_test_gate([&](const render::ChunkKey&, std::uint64_t) {
            entered.set_value();
            const std::lock_guard lock(gate);
        });
        REQUIRE(workers.submit({render::ChunkJob{{2, 0, 0}, {}, {}}}, scene));
        REQUIRE(started.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
        scene.reset(); // the worker has taken the only job and still owns its scene
        CHECK(reclaimer.stats().queued == before.queued);
        hold_worker.unlock();
        workers.wait_idle();
    } // join: wait_idle reports completed builds before their local handles are released
    REQUIRE(settle_reclaimer());
    CHECK(reclaimer.stats().completed == before.completed + 1);
    CHECK(reclaimer.last_thread() != std::this_thread::get_id());
}

TEST_CASE("scene reclaim: destructions are counted as queued, waiting, in flight and completed" *
          doctest::test_suite("scene_reclaim")) {
    auto& reclaimer = render::SceneReclaimer::shared();
    REQUIRE(settle_reclaimer());
    const auto start = reclaimer.stats();
    std::vector<std::shared_ptr<const render::Scene>> scenes;
    for (int i = 0; i < 3; ++i) {
        scenes.push_back(synthetic_scene());
    }
    scenes.clear();
    REQUIRE(settle_reclaimer());
    const auto end = reclaimer.stats();
    CHECK(end.queued == start.queued + 3);
    CHECK(end.completed == start.completed + 3);
    CHECK(end.waiting == 0);
    CHECK(end.in_flight == 0);
}
