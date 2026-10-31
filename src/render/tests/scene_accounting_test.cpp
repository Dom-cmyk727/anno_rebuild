// Retained-byte accounting by ownership (round dy, A4/A5; docs/app/game-shell.md, "Retained work
// and reclamation"). Hand-counted synthetic cases: a buffer shared by two generations is counted
// once; a replacement adds exactly its own buffer; a generation dequeued into a stalled destruction
// stays counted until its destruction ends. The counters are process-wide, so each case compares
// against a snapshot taken at its start (each case runs in its own process under CTest).
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/scene.hpp>

namespace render = anno::render;
using render::PayloadKind;

namespace {

/// One island whose 64-byte head is the backing's only buffer (outer capacity 1).
std::shared_ptr<render::Scene>
one_island_scene(const std::shared_ptr<std::vector<std::vector<std::uint8_t>>>& backing) {
    std::vector<anno::formats::insel::Island> islands(1);
    islands[0].head = (*backing)[0];
    islands[0].number = 0;
    islands[0].width = 8;
    islands[0].height = 8;
    return render::Scene::create(
        "accounting", std::move(islands), anno::assets::island::GroundTextures{},
        anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, backing);
}

std::shared_ptr<std::vector<std::vector<std::uint8_t>>> one_buffer_backing() {
    auto backing = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    backing->reserve(1);
    backing->push_back(std::vector<std::uint8_t>(64, 0));
    return backing;
}

std::size_t delta(const render::PayloadCounts& after, const render::PayloadCounts& before,
                  PayloadKind k) {
    return after.of(k) - before.of(k);
}

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

/// The reclaimer has nothing waiting and nothing in flight.
bool settled() {
    return eventually([] {
        const auto s = render::SceneReclaimer::shared().stats();
        return s.waiting == 0 && s.in_flight == 0;
    });
}

/// One 64-byte buffer in an outer vector of capacity 1: 64 plus one buffer header.
constexpr std::size_t kOneBuffer = 64 + sizeof(std::vector<std::uint8_t>);

} // namespace

TEST_CASE("accounting: a buffer shared by two generations is counted once" *
          doctest::test_suite("scene_accounting")) {
    REQUIRE(settled());
    const auto before = render::payload_counts();
    auto backing = one_buffer_backing();
    REQUIRE(backing->capacity() == 1);
    auto base = one_island_scene(backing);
    const auto with_base = render::payload_counts();
    // The backing (one 64-byte head) is counted once, by the family's assets.
    CHECK(delta(with_base, before, PayloadKind::Backing) == kOneBuffer);
    CHECK(delta(with_base, before, PayloadKind::Generation) == base->own_bytes());
    CHECK(delta(with_base, before, PayloadKind::Incremental) == 0);
    CHECK(base->own_bytes() > sizeof(render::Scene));
    // An unchanged derivation shares the head, the backing and the lists: only its own metadata.
    auto derived = base->with_islands({});
    const auto with_derived = render::payload_counts();
    CHECK(delta(with_derived, with_base, PayloadKind::Backing) == 0);
    CHECK(delta(with_derived, with_base, PayloadKind::Baseline) == 0);
    CHECK(delta(with_derived, with_base, PayloadKind::Incremental) == 0);
    CHECK(delta(with_derived, with_base, PayloadKind::Generation) == derived->own_bytes());
    CHECK(derived->referenced_payload_bytes() == base->referenced_payload_bytes());
    // Both generations go: everything they counted is gone once the reclaimer has finished.
    derived.reset();
    base.reset();
    backing.reset();
    REQUIRE(settled());
    const auto after = render::payload_counts();
    for (std::size_t k = 0; k < render::kPayloadKinds; ++k) {
        CAPTURE(k);
        CHECK(after.live[k] == before.live[k]);
    }
}

TEST_CASE("accounting: a replacement adds exactly its own buffer, shared by later generations" *
          doctest::test_suite("scene_accounting")) {
    REQUIRE(settled());
    auto base = one_island_scene(one_buffer_backing());
    const auto with_base = render::payload_counts();
    const std::size_t base_refs = base->referenced_payload_bytes();
    render::Buffers buffers;
    buffers.reserve(1);
    buffers.push_back(std::vector<std::uint8_t>(64, 7));
    auto storage = render::accounted_buffers(std::move(buffers), PayloadKind::Incremental);
    const auto with_storage = render::payload_counts();
    CHECK(delta(with_storage, with_base, PayloadKind::Incremental) == kOneBuffer);
    render::Scene::IslandUpdate update;
    update.index = 0;
    update.island = base->islands[0];
    update.island.head = anno::formats::Bytes((*storage)[0].data(), (*storage)[0].size());
    update.lists = base->island_lists_handle(0); // the lists are shared, unchanged
    update.storage = storage;
    std::vector<render::Scene::IslandUpdate> updates;
    updates.push_back(std::move(update));
    auto changed = base->with_islands(std::move(updates));
    auto shared_again = changed->with_islands({});
    const auto with_all = render::payload_counts();
    // The two derivations allocate no payload: the buffer was counted once, when it was made.
    CHECK(delta(with_all, with_storage, PayloadKind::Incremental) == 0);
    CHECK(delta(with_all, with_storage, PayloadKind::Generation) ==
          changed->own_bytes() + shared_again->own_bytes());
    CHECK(changed->referenced_payload_bytes() == base_refs + kOneBuffer);
    CHECK(shared_again->referenced_payload_bytes() == base_refs + kOneBuffer);
    // The buffer is freed only with its last owner, then it leaves the count.
    storage.reset();
    changed.reset();
    REQUIRE(settled());
    CHECK(delta(render::payload_counts(), with_base, PayloadKind::Incremental) == kOneBuffer);
    shared_again.reset();
    REQUIRE(settled());
    CHECK(render::payload_counts().of(PayloadKind::Incremental) ==
          with_base.of(PayloadKind::Incremental));
}

TEST_CASE(
    "accounting: a generation dequeued into a stalled destruction stays counted until it ends" *
    doctest::test_suite("scene_accounting")) {
    REQUIRE(settled());
    auto& reclaimer = render::SceneReclaimer::shared();
    auto open = std::make_shared<std::atomic<bool>>(false);
    struct GateReset {
        std::shared_ptr<std::atomic<bool>> open;
        GateReset(const GateReset&) = delete;
        GateReset& operator=(const GateReset&) = delete;
        explicit GateReset(std::shared_ptr<std::atomic<bool>> o) : open(std::move(o)) {}
        ~GateReset() {
            open->store(true);
            render::SceneReclaimer::shared().set_test_gate({});
        }
    } reset(open);
    auto base = one_island_scene(one_buffer_backing());
    auto first = base->with_islands({});
    auto second = base->with_islands({});
    const std::size_t own_first = first->own_bytes();
    const std::size_t own_second = second->own_bytes();
    reclaimer.set_test_gate([open] {
        while (!open->load()) {
            std::this_thread::yield();
        }
    });
    const auto before = reclaimer.stats();
    const auto meta = render::payload_counts().of(PayloadKind::Generation);
    first.reset(); // dequeued at once, then stalled in the gate before its destruction
    REQUIRE(eventually([&] { return reclaimer.stats().in_flight == 1; }));
    second.reset(); // waits behind it
    REQUIRE(eventually([&] { return reclaimer.stats().waiting == before.waiting + 1; }));
    const auto stalled = reclaimer.stats();
    CHECK(stalled.bytes_in_flight == before.bytes_in_flight + own_first);
    CHECK(stalled.bytes_waiting == before.bytes_waiting + own_second);
    // Neither scene has been destroyed: both are still counted as live generation metadata.
    CHECK(render::payload_counts().of(PayloadKind::Generation) == meta);
    open->store(true);
    REQUIRE(settled());
    const auto done = reclaimer.stats();
    CHECK(done.bytes_in_flight == before.bytes_in_flight);
    CHECK(done.bytes_waiting == before.bytes_waiting);
    CHECK(done.completed == before.completed + 2);
    CHECK(render::payload_counts().of(PayloadKind::Generation) == meta - own_first - own_second);
}
