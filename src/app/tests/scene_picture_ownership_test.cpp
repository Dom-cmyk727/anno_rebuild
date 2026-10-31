// ScenePicture ownership and accounting through actual destruction (round dy, A1/A5/A6;
// docs/app/game-shell.md, "Credits and byte accounting"). Synthetic islands with no Anlagen, so no
// game files are needed. Every gate is opened by an RAII opener declared after the picture, so a
// failed assertion releases the gate before the picture's destructor joins its threads.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/building_delta.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/picture_check.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>

namespace app = anno::app;
namespace render = anno::render;

namespace {

/// `count` synthetic islands numbered 0..count-1 of `size` x `size` tiles, each with a 64-byte
/// head and no records.
std::shared_ptr<render::Scene> islands_scene(std::size_t count, std::uint16_t size = 8) {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    for (std::size_t i = 0; i < count; ++i) {
        bytes->push_back(std::vector<std::uint8_t>(64, 0));
    }
    std::vector<anno::formats::insel::Island> islands;
    for (std::size_t i = 0; i < count; ++i) {
        anno::formats::insel::Island island;
        island.head = (*bytes)[i];
        island.number = static_cast<std::uint32_t>(i);
        island.width = size;
        island.height = size;
        islands.push_back(island);
    }
    return render::Scene::create("ownership", std::move(islands),
                                 anno::assets::island::GroundTextures{},
                                 anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
}

std::shared_ptr<const app::BuildingBatch> added(std::uint64_t turn, std::uint32_t island,
                                                std::uint16_t x, std::uint16_t y = 1) {
    auto b = std::make_shared<app::BuildingBatch>();
    b->turn = turn;
    b->deltas.push_back(
        {app::BuildingDelta::Kind::Added, app::SceneBuilding{island, 1, x, y, 0, 0}});
    return b;
}

bool drain(app::ScenePicture& picture, render::FrameRenderer& frames, int rounds) {
    for (int round = 0; round < rounds && picture.pending() > 0; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    return picture.pending() == 0;
}

/// Waits (bounded) until `done` holds; returns whether it did.
template <class F> bool eventually(F done, int seconds = 10) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (!done()) {
        if (std::chrono::steady_clock::now() > until) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

/// A gate a seam blocks in until it is opened (once entered, `entered` counts the threads in it).
struct Gate {
    std::atomic<bool> open{false};
    std::atomic<int> entered{0};
    void pass() {
        entered.fetch_add(1);
        while (!open.load()) {
            std::this_thread::yield();
        }
    }
};
/// Opens a gate when it goes out of scope (declared after the picture: it goes first).
struct Opener {
    Gate& gate;
    Opener(const Opener&) = delete;
    Opener& operator=(const Opener&) = delete;
    explicit Opener(Gate& g) : gate(g) {}
    ~Opener() { gate.open.store(true); }
};

/// The record payloads of island `index` in the committed generation.
std::vector<std::vector<std::uint8_t>> committed_payloads(const render::Scene& scene,
                                                          std::size_t index) {
    std::vector<std::vector<std::uint8_t>> out;
    for (const anno::formats::chunk::Chunk& rec : scene.islands[index].records) {
        out.emplace_back(rec.payload.begin(), rec.payload.end());
    }
    return out;
}

/// The record payloads a fresh build of island `index` from the desired state makes.
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

bool is(const char* a, const char* b) {
    return std::strcmp(a, b) == 0;
}

/// Clears the reclaimer's test observer when it goes out of scope (declared after what the
/// observer captures, so a failed assertion cannot leave it pointing at destroyed locals).
struct ObserverReset {
    ObserverReset() = default;
    ObserverReset(const ObserverReset&) = delete;
    ObserverReset& operator=(const ObserverReset&) = delete;
    ~ObserverReset() { render::SceneReclaimer::shared().set_test_observer({}); }
};

/// Waits (bounded) until the process reclaimer has nothing waiting and nothing in flight.
bool reclaimer_settled() {
    return eventually([] {
        const auto s = render::SceneReclaimer::shared().stats();
        return s.waiting == 0 && s.in_flight == 0;
    });
}

} // namespace

TEST_CASE("ownership: a dropped result's credit is held while its payload is destroyed" *
          doctest::test_suite("scene_picture_ownership")) {
    // Every build is over the byte budget alone, so any held credit keeps every other build out.
    // Island 0's finished result is dropped by a newer change; the reaper destroys it and the seam
    // stops inside that destruction, after the rest of the payload is gone. The credit must still
    // be held there: the replacement and a competing island stay deferred.
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    Gate gate;
    std::atomic<bool> armed{false};
    app::ScenePicture::Limits limits;
    limits.max_outstanding_bytes = 1;
    limits.payload_destroy_hook = [&gate, &armed](const char* what, std::size_t index) {
        bool expected = true;
        if (is(what, "result") && index == 0 && armed.compare_exchange_strong(expected, false)) {
            gate.pass();
        }
    };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
    Opener opener(gate);
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    picture.wait_idle();
    const std::size_t result_bytes = picture.stats().outstanding_bytes;
    REQUIRE(result_bytes > 0);
    CHECK(picture.stats().outstanding == 1);
    armed.store(true);
    picture.apply({added(2, 0, 2)}); // drops island 0's finished result to the reaper
    REQUIRE(eventually([&] { return gate.entered.load() == 1; }));
    // Inside the payload's destruction: the credit is still held, and still counted.
    CHECK(picture.stats().outstanding == 1);
    CHECK(picture.stats().outstanding_bytes == result_bytes);
    const auto deferrals = picture.stats().budget_deferrals;
    picture.apply({added(3, 1, 1)}); // a competing island
    CHECK(picture.stats().budget_deferrals > deferrals);
    picture.wait_idle();
    CHECK(picture.timings().build_ms.size() == 1); // nothing else was built
    gate.open.store(true);
    REQUIRE(eventually([&] { return picture.stats().outstanding == 0; }));
    CHECK(picture.stats().outstanding_bytes == 0);
    CHECK(drain(picture, frames, 40));
    CHECK(picture.error().empty());
    CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
    CHECK(committed_payloads(*picture.scene(), 1) == fresh_payloads(*scene, 1, picture));
    CHECK(picture.stats().outstanding_peak == 1);
}

TEST_CASE("ownership: a dropped queued job's credit is held while its payload is destroyed" *
          doctest::test_suite("scene_picture_ownership")) {
    // Two credits at most. Island 0's build is blocked on the one worker; island 1's job is queued
    // behind it. A newer change to island 1 drops the queued job; the seam stops inside its
    // destruction on the reaper. Both credits are still held there, so island 1's new job and a
    // competing island 2 stay deferred.
    auto scene = islands_scene(3);
    std::vector<app::SceneBuilding> none;
    Gate build_gate, destroy_gate;
    std::atomic<bool> armed{false};
    app::ScenePicture::Limits limits;
    limits.max_outstanding = 2;
    limits.build_fault = [&build_gate](std::size_t index) {
        if (index == 0) {
            build_gate.pass();
        }
    };
    limits.payload_destroy_hook = [&destroy_gate, &armed](const char* what, std::size_t index) {
        bool expected = true;
        if (is(what, "job") && index == 1 && armed.compare_exchange_strong(expected, false)) {
            destroy_gate.pass();
        }
    };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
    Opener open_destroy(destroy_gate);
    Opener open_build(build_gate);
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    REQUIRE(eventually([&] { return build_gate.entered.load() == 1; }));
    picture.apply({added(2, 1, 1)}); // queued behind island 0
    CHECK(picture.stats().outstanding == 2);
    armed.store(true);
    picture.apply({added(3, 1, 2)}); // drops the queued job
    REQUIRE(eventually([&] { return destroy_gate.entered.load() == 1; }));
    CHECK(picture.stats().outstanding == 2);
    const auto deferred = picture.stats().deferred;
    picture.apply({added(4, 2, 1)});
    CHECK(picture.stats().outstanding == 2);
    CHECK(picture.stats().deferred > deferred);
    destroy_gate.open.store(true);
    REQUIRE(eventually([&] { return picture.stats().outstanding == 1; }));
    build_gate.open.store(true);
    CHECK(drain(picture, frames, 40));
    CHECK(picture.error().empty());
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(committed_payloads(*picture.scene(), i) == fresh_payloads(*scene, i, picture));
    }
    CHECK(picture.stats().outstanding_peak <= 2);
}

TEST_CASE(
    "accounting: an incoming build much larger than the old records is reserved for its size" *
    doctest::test_suite("scene_picture_ownership")) {
    // Island 0 has a 64-byte head and no records; the change adds 500 buildings. The reservation
    // before the build runs covers the incoming payload (at least one PROD2 item and one block
    // header per building, and the copy of the buildings), not the 64 old bytes; the measured
    // result is at least the 500 items, and no larger than the reservation.
    auto scene = islands_scene(1, 64);
    std::vector<app::SceneBuilding> none;
    Gate gate;
    app::ScenePicture::Limits limits;
    limits.build_fault = [&gate](std::size_t) { gate.pass(); };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
    Opener opener(gate);
    render::FrameRenderer frames(1);
    auto batch = std::make_shared<app::BuildingBatch>();
    batch->turn = 1;
    for (std::uint16_t i = 0; i < 500; ++i) {
        batch->deltas.push_back({app::BuildingDelta::Kind::Added,
                                 app::SceneBuilding{0, 1, static_cast<std::uint16_t>(i % 50),
                                                    static_cast<std::uint16_t>(i / 50), 0, 0}});
    }
    picture.apply({batch});
    REQUIRE(eventually([&] { return gate.entered.load() == 1; }));
    const std::size_t reserved = picture.stats().outstanding_bytes;
    const std::size_t floor =
        500 * (anno::formats::insel::kItemLen + anno::formats::insel::kBlockHeaderLen +
               sizeof(app::SceneBuilding));
    CHECK(reserved >= floor);
    CHECK(reserved > 64 * 100);
    const auto before = render::payload_counts();
    gate.open.store(true);
    picture.wait_idle();
    const auto st = picture.stats();
    CHECK(st.estimate_shortfalls == 0);
    CHECK(st.result_bytes_peak >= 500 * anno::formats::insel::kItemLen);
    CHECK(st.result_bytes_peak <= reserved);
    CHECK(st.outstanding_bytes == st.result_bytes_peak); // resized to the measured result
    // The result's storage and lists are counted once, by their own allocations.
    const auto built = render::payload_counts();
    CHECK(built.of(render::PayloadKind::Incremental) -
              before.of(render::PayloadKind::Incremental) >=
          500 * anno::formats::insel::kItemLen);
    CHECK(drain(picture, frames, 10));
    CHECK(picture.stats().outstanding_bytes == 0); // handed to the generation
    CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
    CHECK(picture.scene()->referenced_payload_bytes() >= 500 * anno::formats::insel::kItemLen);
}

TEST_CASE("accounting: one oversized build is admitted alone and nothing runs beside it" *
          doctest::test_suite("scene_picture_ownership")) {
    auto scene = islands_scene(3);
    std::vector<app::SceneBuilding> none;
    Gate gate;
    app::ScenePicture::Limits limits;
    limits.max_outstanding_bytes = 1; // every build is over the budget on its own
    limits.build_fault = [&gate](std::size_t index) {
        if (index == 0) {
            gate.pass();
        }
    };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2, limits);
    Opener opener(gate);
    render::FrameRenderer frames(1);
    auto batch = std::make_shared<app::BuildingBatch>();
    batch->turn = 1;
    for (std::uint32_t island = 0; island < 3; ++island) {
        batch->deltas.push_back(
            {app::BuildingDelta::Kind::Added, app::SceneBuilding{island, 1, 1, 1, 0, 0}});
    }
    picture.apply({batch});
    REQUIRE(eventually([&] { return gate.entered.load() == 1; }));
    // Two workers are free, but the oversized build holds the only admission.
    CHECK(picture.stats().outstanding == 1);
    CHECK(picture.stats().outstanding_bytes > limits.max_outstanding_bytes);
    CHECK(picture.stats().budget_deferrals >= 2);
    CHECK(picture.timings().build_ms.empty());
    gate.open.store(true);
    CHECK(drain(picture, frames, 40));
    CHECK(picture.error().empty());
    CHECK(picture.stats().outstanding_peak == 1);
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(committed_payloads(*picture.scene(), i) == fresh_payloads(*scene, i, picture));
    }
}

TEST_CASE("accounting: one, two and four workers each hold a superseded generation at a barrier" *
          doctest::test_suite("scene_picture_ownership")) {
    for (const std::size_t workers : {std::size_t{1}, std::size_t{2}, std::size_t{4}}) {
        CAPTURE(workers);
        // Island 0's builds block at the barrier. Between two changes to island 0, island 1 is
        // built and committed, so each blocked build holds a different generation: the newest is
        // current, the others are superseded and retired, alive only through their jobs. Each
        // keeps its credit and its generation until it finishes.
        auto scene = islands_scene(2);
        std::vector<app::SceneBuilding> none;
        Gate gate;
        std::mutex seen_mutex;
        std::vector<std::uint64_t> destroyed;
        auto& reclaimer = render::SceneReclaimer::shared();
        reclaimer.set_test_observer([&seen_mutex, &destroyed](const render::Scene& s) {
            const std::lock_guard lock(seen_mutex);
            destroyed.push_back(s.generation());
        });
        const ObserverReset reset;
        {
            app::ScenePicture::Limits limits;
            limits.max_outstanding = workers + 1;
            limits.build_fault = [&gate](std::size_t index) {
                if (index == 0) {
                    gate.pass();
                }
            };
            app::ScenePicture picture(
                scene, none, [](std::uint16_t) { return true; }, workers, limits);
            Opener opener(gate);
            render::FrameRenderer frames(1);
            std::uint64_t turn = 1;
            std::vector<std::uint64_t> held;
            for (std::size_t i = 0; i < workers; ++i) {
                held.push_back(picture.scene()->generation());
                picture.apply({added(turn++, 0, static_cast<std::uint16_t>(i + 1))});
                REQUIRE(eventually([&] { return gate.entered.load() == static_cast<int>(i + 1); }));
                if (i + 1 < workers) {
                    // Island 1 is built by a free worker and committed: the next change to island 0
                    // starts from the new generation.
                    picture.apply({added(turn++, 1, static_cast<std::uint16_t>(i + 1))});
                    REQUIRE(eventually([&] { return picture.timings().build_ms.size() == i + 1; }));
                    REQUIRE(picture.commit(frames) == 1);
                }
            }
            CHECK(picture.stats().outstanding == workers);
            CHECK(picture.stats().discarded == 0);
            // The superseded generations are alive only through their jobs: none is destroyed yet.
            {
                const std::lock_guard lock(seen_mutex);
                for (std::size_t i = 0; i + 1 < held.size(); ++i) {
                    CHECK(std::find(destroyed.begin(), destroyed.end(), held[i]) ==
                          destroyed.end());
                }
            }
            gate.open.store(true);
            CHECK(drain(picture, frames, 40));
            CHECK(picture.error().empty());
            CHECK(picture.stats().outstanding == 0);
            CHECK(picture.stats().discarded == workers - 1);
            CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
            CHECK(committed_payloads(*picture.scene(), 1) == fresh_payloads(*scene, 1, picture));
            // Each superseded build released its generation when it finished: the generations
            // between the first (the test's own scene) and the last (retired by later commits).
            if (held.size() > 2) {
                REQUIRE(eventually([&] {
                    const std::lock_guard lock(seen_mutex);
                    return std::all_of(held.begin() + 1, held.end() - 1, [&](std::uint64_t g) {
                        return std::find(destroyed.begin(), destroyed.end(), g) != destroyed.end();
                    });
                }));
            }
        }
        // This iteration's generations are all destroyed before the next one starts counting.
        CHECK(reclaimer_settled());
    }
}

TEST_CASE("ownership: a frame-held generation outlives the bridge and is destroyed on the "
          "reclaimer" *
          doctest::test_suite("scene_picture_ownership")) {
    // A frame keeps the generation it last planned from (FrameRenderer::update_chunks), and the
    // caller keeps a handle to the same one. The bridge commits two more generations and is
    // destroyed; then the caller releases the frame and its handle. That generation, and no
    // other generation of this test, is destroyed on the reclaimer thread after that release,
    // and the accounted bytes return to what they were before the picture existed.
    auto scene = islands_scene(2);
    std::vector<app::SceneBuilding> none;
    struct Seen {
        const render::Scene* scene;
        std::uint64_t generation;
        std::thread::id thread;
    };
    std::mutex seen_mutex;
    std::vector<Seen> seen;
    auto& reclaimer = render::SceneReclaimer::shared();
    reclaimer.set_test_observer([&seen_mutex, &seen](const render::Scene& s) {
        const std::lock_guard lock(seen_mutex);
        seen.push_back({&s, s.generation(), std::this_thread::get_id()});
    });
    const ObserverReset reset;
    const auto before = render::payload_counts();
    const render::Scene* held_ptr = nullptr;
    std::shared_ptr<const render::Scene> held;
    auto frames = std::make_unique<render::FrameRenderer>(1);
    {
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, {});
        picture.apply({added(1, 0, 1)});
        REQUIRE(drain(picture, *frames, 10));
        held = picture.scene();
        held_ptr = held.get();
        REQUIRE(held->generation() == 1);
        const auto view = held->bounds(2);
        REQUIRE(view.has_value());
        frames->update_chunks(held, *view, 2); // the frame keeps generation 1
        picture.apply({added(2, 0, 2)});
        REQUIRE(drain(picture, *frames, 10));
        picture.apply({added(3, 1, 1)});
        REQUIRE(drain(picture, *frames, 10));
        REQUIRE(picture.scene()->generation() == 3);
    }
    // The bridge is gone; generations 2 and 3 are released by it. Generation 1 is still held.
    REQUIRE(eventually([&] {
        const std::lock_guard lock(seen_mutex);
        return std::count_if(seen.begin(), seen.end(),
                             [](const Seen& s) { return s.generation >= 2; }) == 2;
    }));
    {
        const std::lock_guard lock(seen_mutex);
        CHECK(std::none_of(seen.begin(), seen.end(),
                           [held_ptr](const Seen& s) { return s.scene == held_ptr; }));
    }
    frames.reset(); // the frame lets go on this thread; the handle still holds generation 1
    {
        const std::lock_guard lock(seen_mutex);
        CHECK(std::none_of(seen.begin(), seen.end(),
                           [held_ptr](const Seen& s) { return s.scene == held_ptr; }));
    }
    held.reset(); // the last owner, on this thread
    REQUIRE(eventually([&] {
        const std::lock_guard lock(seen_mutex);
        return std::any_of(seen.begin(), seen.end(),
                           [held_ptr](const Seen& s) { return s.scene == held_ptr; });
    }));
    {
        const std::lock_guard lock(seen_mutex);
        const auto it = std::find_if(seen.begin(), seen.end(),
                                     [held_ptr](const Seen& s) { return s.scene == held_ptr; });
        CHECK(it->generation == 1);
        CHECK(it->thread != std::this_thread::get_id());
    }
    // Destruction finished: the accounted bytes are back where they were.
    REQUIRE(eventually([&] {
        const auto now = render::payload_counts();
        return now.of(render::PayloadKind::Incremental) ==
                   before.of(render::PayloadKind::Incremental) &&
               now.of(render::PayloadKind::Generation) ==
                   before.of(render::PayloadKind::Generation) &&
               reclaimer.stats().in_flight == 0;
    }));
    CHECK(reclaimer.stats().bytes_in_flight == 0);
}

TEST_CASE("trace: an edit's steps are reported in order, a superseded version included" *
          doctest::test_suite("scene_picture_ownership")) {
    // Island 0's first build blocks; a newer change arrives; the first finishes superseded and the
    // second is committed. The seam reports every step with its island and version.
    auto scene = islands_scene(1);
    std::vector<app::SceneBuilding> none;
    Gate gate;
    std::mutex mutex;
    std::vector<std::pair<app::PictureEvent, std::uint64_t>> seen;
    std::atomic<bool> other_island{false};
    app::ScenePicture::Limits limits;
    limits.build_fault = [&gate](std::size_t) {
        if (gate.entered.load() == 0) {
            gate.pass();
        }
    };
    limits.trace = [&mutex, &seen, &other_island](app::PictureEvent e, std::size_t index,
                                                  std::uint64_t version) {
        // No assertion here: the seam runs on the workers too (doctest asserts on the test thread).
        if (index != 0) {
            other_island.store(true);
        }
        const std::lock_guard lock(mutex);
        seen.emplace_back(e, version);
    };
    app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 2, limits);
    Opener opener(gate);
    render::FrameRenderer frames(1);
    picture.apply({added(1, 0, 1)});
    REQUIRE(eventually([&] { return gate.entered.load() == 1; }));
    picture.apply({added(2, 0, 2)});
    // The second build runs on the other worker while the first is still held at the gate.
    REQUIRE(eventually([&] {
        const std::lock_guard lock(mutex);
        return std::find(seen.begin(), seen.end(),
                         std::make_pair(app::PictureEvent::Built, std::uint64_t{2})) != seen.end();
    }));
    gate.open.store(true);
    REQUIRE(drain(picture, frames, 20));
    // (No chunk key is invalidated here: with no Anlagen the objects draw nothing. The legal
    // workload's real-data case checks the keys.)
    CHECK(!other_island.load());
    using E = app::PictureEvent;
    const std::lock_guard lock(mutex);
    auto at = [&seen](E e, std::uint64_t v) {
        return std::find(seen.begin(), seen.end(), std::make_pair(e, v)) - seen.begin();
    };
    const auto n = static_cast<std::ptrdiff_t>(seen.size());
    CHECK(at(E::Desired, 1) < at(E::Admitted, 1));
    CHECK(at(E::Admitted, 1) < at(E::Started, 1));
    CHECK(at(E::Desired, 2) < at(E::Admitted, 2));
    CHECK(at(E::Started, 2) < at(E::Built, 2));
    CHECK(at(E::Built, 2) < at(E::Committed, 2));
    CHECK(at(E::Superseded, 1) < n);
    CHECK(at(E::Committed, 2) < n);
    CHECK(at(E::Committed, 1) == n); // version 1 is never committed on its own
}

TEST_CASE("progress: publish_progress publishes a superseded build that is ahead, and never goes "
          "back" *
          doctest::test_suite("scene_picture_ownership")) {
    // One worker. Version 1's build is held, version 2 arrives and is queued; version 1 finishes
    // and version 2's build is held. A commit then publishes version 1 with publish_progress (the
    // picture moves forward), and nothing without it (the accepted contract). Both converge to
    // version 2, and the committed versions only grow.
    for (const bool progress : {false, true}) {
        CAPTURE(progress);
        auto scene = islands_scene(1);
        std::vector<app::SceneBuilding> none;
        Gate first, second;
        std::atomic<int> builds{0};
        std::mutex mutex;
        std::vector<std::uint64_t> committed;
        app::ScenePicture::Limits limits;
        limits.publish_progress = progress;
        limits.build_fault = [&first, &second, &builds](std::size_t) {
            const int n = builds.fetch_add(1);
            if (n == 0) {
                first.pass();
            } else if (n == 1) {
                second.pass();
            }
        };
        limits.trace = [&mutex, &committed](app::PictureEvent e, std::size_t, std::uint64_t v) {
            if (e == app::PictureEvent::Committed) {
                const std::lock_guard lock(mutex);
                committed.push_back(v);
            }
        };
        app::ScenePicture picture(scene, none, [](std::uint16_t) { return true; }, 1, limits);
        Opener open_second(second);
        Opener open_first(first);
        render::FrameRenderer frames(1);
        picture.apply({added(1, 0, 1)});
        REQUIRE(eventually([&] { return first.entered.load() == 1; }));
        picture.apply({added(2, 0, 2)});
        first.open.store(true);
        REQUIRE(eventually([&] { return second.entered.load() == 1; }));
        picture.commit(frames);
        const std::size_t items = app::building_items(picture.scene()->islands[0]).size();
        CHECK(items == (progress ? 1U : 0U));
        CHECK(picture.pending() == 1);
        second.open.store(true);
        CHECK(drain(picture, frames, 40));
        CHECK(app::building_items(picture.scene()->islands[0]).size() == 2);
        CHECK(committed_payloads(*picture.scene(), 0) == fresh_payloads(*scene, 0, picture));
        const std::lock_guard lock(mutex);
        CHECK(std::is_sorted(committed.begin(), committed.end()));
        CHECK(std::adjacent_find(committed.begin(), committed.end()) == committed.end());
        CHECK(committed.front() == (progress ? 1U : 2U));
        CHECK(committed.back() == 2U);
    }
}
