// ScenePicture recovery and convergence (round dw, DW3c/d and DW4c/f; docs/app/game-shell.md,
// "Retained work and reclamation"). Synthetic: islands of 8x8 with no Anlagen, so no game files are
// needed. The buildings are added to the islands' records, so a committed generation shows its
// changes as record counts: an island's record count must equal the number of buildings the desired
// state has on it.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/building_delta.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>

namespace app = anno::app;
namespace render = anno::render;

namespace {

/// `count` synthetic islands numbered 0..count-1, each with an empty record list.
std::shared_ptr<render::Scene> islands_scene(std::size_t count) {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    std::vector<anno::formats::insel::Island> islands;
    for (std::size_t i = 0; i < count; ++i) {
        bytes->push_back(std::vector<std::uint8_t>(64, 0));
    }
    for (std::size_t i = 0; i < count; ++i) {
        anno::formats::insel::Island island;
        island.head = (*bytes)[i];
        island.number = static_cast<std::uint32_t>(i);
        island.width = 8;
        island.height = 8;
        islands.push_back(island);
    }
    return render::Scene::create("recovery", std::move(islands),
                                 anno::assets::island::GroundTextures{},
                                 anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
}

/// The desired building list: `per_island[i]` buildings on island i, in batch order.
std::shared_ptr<const app::BuildingBatch> added(std::uint64_t turn, std::uint32_t island,
                                                std::uint16_t x) {
    auto b = std::make_shared<app::BuildingBatch>();
    b->turn = turn;
    b->deltas.push_back(
        {app::BuildingDelta::Kind::Added, app::SceneBuilding{island, 1, x, 1, 0, 0}});
    return b;
}

/// The record payloads of island `index` in the committed generation.
std::vector<std::vector<std::uint8_t>> committed_payloads(const render::Scene& scene,
                                                          std::size_t index) {
    std::vector<std::vector<std::uint8_t>> out;
    for (const anno::formats::chunk::Chunk& rec : scene.islands[index].records) {
        out.emplace_back(rec.payload.begin(), rec.payload.end());
    }
    return out;
}

/// The record payloads of island `index` as a fresh build from the desired state makes them: the
/// loader's per-island function on a copy of the base island (the same steps the picture's worker
/// takes).
std::vector<std::vector<std::uint8_t>> fresh_payloads(const render::Scene& base, std::size_t index,
                                                      const app::ScenePicture& picture) {
    const std::uint32_t number = base.islands[index].number;
    std::vector<app::SceneBuilding> mine;
    for (const app::SceneBuilding& b : picture.buildings()) {
        if (b.island == number) {
            mine.push_back(b);
        }
    }
    anno::formats::insel::Island island = base.islands[index];
    std::vector<std::vector<std::uint8_t>> storage;
    storage.push_back(std::vector<std::uint8_t>(island.head.begin(), island.head.end()));
    island.head = anno::formats::Bytes(storage.back().data(), storage.back().size());
    for (anno::formats::chunk::Chunk& rec : island.records) {
        storage.push_back(std::vector<std::uint8_t>(rec.payload.begin(), rec.payload.end()));
        rec.payload = anno::formats::Bytes(storage.back().data(), storage.back().size());
    }
    app::SceneObjects objects;
    objects.is_building = [](std::uint16_t id) { return id < 0x1000; };
    std::vector<std::string> notes;
    std::vector<app::OmittedObject> omitted;
    app::apply_island_buildings(island, mine, objects, storage, notes, omitted);
    std::vector<std::vector<std::uint8_t>> out;
    for (const anno::formats::chunk::Chunk& rec : island.records) {
        out.emplace_back(rec.payload.begin(), rec.payload.end());
    }
    return out;
}

/// The buildings of the desired state on island `number`.
std::size_t desired_on(const app::ScenePicture& picture, std::uint32_t number) {
    std::size_t n = 0;
    for (const app::SceneBuilding& b : picture.buildings()) {
        n += b.island == number ? 1U : 0U;
    }
    return n;
}

app::ScenePicture::Limits fault_once_limits(std::atomic<int>& failures, std::uint32_t island) {
    app::ScenePicture::Limits limits;
    limits.build_fault = [&failures, island](std::size_t index) {
        if (static_cast<std::uint32_t>(index) == island && failures.fetch_sub(1) > 0) {
            throw std::runtime_error("injected build failure");
        }
    };
    return limits;
}

/// Commits until nothing is pending, bounded (a hot loop or a stuck island fails the case).
bool drain(app::ScenePicture& picture, render::FrameRenderer& frames, int rounds) {
    for (int round = 0; round < rounds && picture.pending() > 0; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    return picture.pending() == 0;
}

} // namespace

TEST_CASE("picture recovery: a one-shot build failure is retried from the desired state with no "
          "new batch" *
          doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(2);
    std::atomic<int> failures{1};
    std::vector<app::SceneBuilding> none;
    app::ScenePicture picture(
        scene, none, [](std::uint16_t) { return true; }, 1, fault_once_limits(failures, 0));
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 0); // the failed build commits nothing
    CHECK(picture.pending() == 1);      // the change is still owed: no false idle
    // No new batch: the window's commit calls alone recover the island.
    CHECK(drain(picture, frames, 20));
    REQUIRE(!fresh_payloads(*scene, 0, picture).empty()); // the oracle is not two empty lists
    CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
    CHECK(picture.stats().build_failures == 1);
    CHECK(picture.stats().build_retries >= 1);
    CHECK(picture.stats().failed_islands == 0);
    CHECK(picture.error().empty());
}

TEST_CASE("picture recovery: a persistent failure stops after the attempts and is reported" *
          doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    app::ScenePicture::Limits limits;
    limits.max_attempts = 3;
    std::atomic<int> attempts{0};
    limits.build_fault = [&attempts](std::size_t) {
        attempts.fetch_add(1);
        throw std::runtime_error("injected persistent failure");
    };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    // Many commit calls: the builds stop at the attempt limit, the island stays owed and reported.
    for (int round = 0; round < 40; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    CHECK(attempts.load() == 3);
    CHECK(picture.stats().build_failures == 3);
    CHECK(picture.stats().failed_islands == 1);
    CHECK(picture.pending() == 1); // not idle: the change is owed
    CHECK(picture.error().find("injected persistent failure") != std::string::npos);
}

TEST_CASE("picture recovery: a newer change during a retry is built from the newest desired state" *
          doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(2);
    std::atomic<int> failures{1};
    std::vector<app::SceneBuilding> none;
    app::ScenePicture picture(
        scene, none, [](std::uint16_t) { return true; }, 1, fault_once_limits(failures, 0));
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    picture.wait_idle();
    picture.commit(frames); // the failure; the retry is waiting
    picture.apply({added(2, 0, 2)});
    CHECK(drain(picture, frames, 20));
    CHECK(committed_payloads(*picture.scene(), 0) ==
          fresh_payloads(*scene, 0, picture)); // both changes, from the newest desired state
    CHECK(desired_on(picture, 0) == 2);
}

TEST_CASE("picture recovery: shutdown with a retry waiting returns and releases the state" *
          doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    app::ScenePicture::Limits limits;
    limits.build_fault = [](std::size_t) { throw std::runtime_error("injected"); };
    {
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2, limits);
        render::FrameRenderer frames(1);
        picture.apply({added(1, 0, 1)});
        picture.wait_idle();
        picture.commit(frames); // a retry is now waiting
    }
    CHECK(true); // reaching this line: the destructor joined the workers and the retry did not run
}

TEST_CASE("picture recovery: a result of an older version is never published after a newer change" *
          doctest::test_suite("scene_picture_recovery")) {
    // The reaper is stalled, so commits are skipped while it is behind. A build finishes, a newer
    // change arrives while the reaper is still behind, and the reaper is released. The old result
    // must not be published: the commit drops it and the newer change is built.
    auto scene = islands_scene(2);
    std::atomic<bool> open{false};
    std::vector<app::SceneBuilding> none;
    app::ScenePicture::Limits limits;
    limits.max_reclaim_backlog = 0;
    limits.reclaim_gate = [&open] {
        while (!open.load()) {
            std::this_thread::yield();
        }
    };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
    render::FrameRenderer frames(1);
    // Generation 1 (island 0: one building); its replaced generation keeps the reaper busy.
    picture.apply({added(1, 0, 1)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 1);
    // Island 0 is built again (version 2, one building more) and the build finishes; the commit is
    // skipped while the reaper is behind, so the finished result waits.
    picture.apply({added(3, 0, 2)});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 0);
    CHECK(picture.stats().commit_skipped >= 1);
    // A newer change for island 0 arrives while its old result is waiting and the reaper is behind.
    picture.apply({added(4, 0, 3)});
    open.store(true); // the reaper catches up
    // Wait until the reaper has released everything it was handed (its own counters, bounded). A
    // commit is skipped while the reaper is behind, so the drain below must start after the
    // catch-up, not race it.
    const auto caught_up = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (picture.stats().reclaim_completed < picture.stats().reclaim_queued &&
           std::chrono::steady_clock::now() < caught_up) {
        std::this_thread::yield();
    }
    REQUIRE(picture.stats().reclaim_completed == picture.stats().reclaim_queued);
    CHECK(drain(picture, frames, 20));
    // The old result (two buildings) was never published as the newest state: island 0 has three.
    CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
    CHECK(desired_on(picture, 0) == 3);
    CHECK(picture.stats().discarded >= 1);
}

TEST_CASE(
    "picture recovery: sustained changes over many islands converge and stay within the limit" *
    doctest::test_suite("scene_picture_recovery")) {
    constexpr std::size_t kIslands = 12;
    auto scene = islands_scene(kIslands);
    std::vector<app::SceneBuilding> none;
    app::ScenePicture::Limits limits;
    limits.max_outstanding = 3;
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2, limits);
    render::FrameRenderer frames(1);
    std::map<std::uint32_t, std::size_t> counts;
    for (std::uint64_t turn = 1; turn <= 240; ++turn) {
        // Changes arrive faster than the commits: three islands per turn, rotating.
        std::vector<std::shared_ptr<const app::BuildingBatch>> batches;
        for (std::uint32_t k = 0; k < 3; ++k) {
            const auto island = static_cast<std::uint32_t>((turn * 3 + k) % kIslands);
            batches.push_back(added(turn, island, static_cast<std::uint16_t>(turn)));
            ++counts[island];
        }
        picture.apply(batches);
        if (turn % 4 == 0) {
            picture.wait_idle();
            picture.commit(frames);
        }
    }
    CHECK(drain(picture, frames, 400));
    const auto st = picture.stats();
    CHECK(st.outstanding_peak <= limits.max_outstanding);
    CHECK(st.committed >= kIslands);
    for (std::uint32_t island = 0; island < kIslands; ++island) {
        CHECK(committed_payloads(*picture.scene(), island) ==
              fresh_payloads(*scene, island, picture));
    }
    CHECK(st.dirty_islands == 0);
}

TEST_CASE(
    "picture recovery: one over-budget island still makes progress; the budget holds otherwise" *
    doctest::test_suite("scene_picture_recovery")) {
    auto scene = islands_scene(4);
    std::vector<app::SceneBuilding> none;
    app::ScenePicture::Limits limits;
    limits.max_outstanding_bytes = 1; // every island is over the budget on its own
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2, limits);
    render::FrameRenderer frames(1);
    for (std::uint32_t island = 0; island < 4; ++island) {
        picture.apply({added(island + 1, island, 1)});
    }
    CHECK(drain(picture, frames, 40));
    for (std::uint32_t island = 0; island < 4; ++island) {
        CHECK(committed_payloads(*picture.scene(), island) ==
              fresh_payloads(*scene, island, picture));
    }
    // One build at a time under the budget: the peak never has two builds in flight.
    CHECK(picture.stats().outstanding_peak == 1);
    CHECK(picture.stats().budget_deferrals >= 1);
}

TEST_CASE("picture recovery: the same convergence with one, two and four workers" *
          doctest::test_suite("scene_picture_recovery")) {
    for (std::size_t workers : {std::size_t{1}, std::size_t{2}, std::size_t{4}}) {
        auto scene = islands_scene(6);
        std::vector<app::SceneBuilding> none;
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, workers);
        render::FrameRenderer frames(1);
        for (std::uint64_t turn = 1; turn <= 30; ++turn) {
            picture.apply({added(turn, static_cast<std::uint32_t>(turn % 6),
                                 static_cast<std::uint16_t>(turn))});
        }
        CHECK(drain(picture, frames, 100));
        std::size_t total = 0;
        for (std::uint32_t island = 0; island < 6; ++island) {
            total += desired_on(picture, island);
            CHECK(committed_payloads(*picture.scene(), island) ==
                  fresh_payloads(*scene, island, picture));
        }
        CHECK(total == 30);
    }
}

TEST_CASE("picture recovery: a generation that outlives the bridge is destroyed on the reclaimer" *
          doctest::test_suite("scene_picture_recovery")) {
    // The bridge is destroyed while a commit's generation is still held by the test (a consumer
    // that outlived the picture). The final release is the test's, on this thread; the destruction
    // must run on the reclaimer, and the bridge's own references must be gone with the bridge.
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    std::shared_ptr<const render::Scene> kept;
    const std::uint64_t before = render::SceneReclaimer::shared().stats().completed;
    {
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2);
        render::FrameRenderer frames(1);
        picture.apply({added(1, 0, 1)});
        picture.wait_idle();
        REQUIRE(picture.commit(frames) == 1);
        kept = picture.scene();
    }
    scene.reset(); // the base generation and the bridge's references are gone; `kept` is the last
                   // owner
    CHECK(render::SceneReclaimer::shared().stats().completed == before);
    kept.reset();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (render::SceneReclaimer::shared().stats().completed < before + 2 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    // Both generations (the base and the committed one) were destroyed by the reclaimer.
    REQUIRE(render::SceneReclaimer::shared().stats().completed >= before + 2);
    CHECK(render::SceneReclaimer::shared().last_thread() != std::this_thread::get_id());
}
