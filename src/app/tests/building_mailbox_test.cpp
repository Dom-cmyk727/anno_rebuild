// The building mailbox (docs/app/game-shell.md, "Building changes after tick 0"):
// the bound and the resync, and the picture's apply. No game files, no threads.
#include <cstdint>
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/building_delta.hpp>

namespace {

anno::app::SceneBuilding at(std::uint16_t x, std::uint16_t y, std::uint32_t type) {
    anno::app::SceneBuilding b;
    b.x = x;
    b.y = y;
    b.type = type;
    return b;
}

// A batch for `turn` that removes `from` and adds `to`, as the diff would.
std::shared_ptr<const anno::app::BuildingBatch> batch_moving(std::uint64_t turn,
                                                             const anno::app::SceneBuilding& from,
                                                             const anno::app::SceneBuilding& to) {
    auto b = std::make_shared<anno::app::BuildingBatch>();
    b->turn = turn;
    b->deltas.push_back({anno::app::BuildingDelta::Kind::Removed, from});
    b->deltas.push_back({anno::app::BuildingDelta::Kind::Added, to});
    return b;
}

} // namespace

TEST_CASE("mailbox: batches up to the bound are kept in order") {
    anno::app::BuildingMailbox box(3);
    std::vector<anno::app::SceneBuilding> current{at(1, 1, 5)};
    box.push(batch_moving(1, at(0, 0, 1), at(1, 1, 5)), current);
    box.push(batch_moving(2, at(1, 1, 5), at(2, 2, 5)), current);
    box.push(batch_moving(3, at(2, 2, 5), at(3, 3, 5)), current);
    REQUIRE(box.pending() == 3);
    const auto taken = box.take();
    REQUIRE(taken.size() == 3);
    CHECK(taken[0]->turn == 1);
    CHECK(taken[2]->turn == 3);
    CHECK_FALSE(taken[0]->resync);
    CHECK(box.pending() == 0);
}

TEST_CASE("mailbox: past the bound the batches collapse into one resync") {
    anno::app::BuildingMailbox box(3);
    std::vector<anno::app::SceneBuilding> after = {at(1, 1, 5)};
    // Four pushes with a bound of 3: the fourth collapses the three waiting ones.
    box.push(batch_moving(1, at(0, 0, 1), at(0, 1, 1)), after);
    box.push(batch_moving(2, at(0, 1, 1), at(0, 2, 1)), after);
    box.push(batch_moving(3, at(0, 2, 1), at(0, 3, 1)), after);
    box.push(batch_moving(4, at(0, 3, 1), at(1, 1, 5)), after);
    REQUIRE(box.pending() == 1);
    const auto taken = box.take();
    REQUIRE(taken.size() == 1);
    CHECK(taken[0]->resync);
    CHECK(taken[0]->turn == 4);
    CHECK(taken[0]->full == after);
}

TEST_CASE("mailbox: a picture that takes once after a collapse equals the world's list") {
    // The picture starts from the list at load; the world moves through 20 turns,
    // the mailbox is never taken during them (bound 4), then taken once.
    std::vector<anno::app::SceneBuilding> world = {at(0, 0, 1), at(5, 5, 2)};
    anno::app::sort_buildings(world);
    std::vector<anno::app::SceneBuilding> picture = world;

    anno::app::BuildingMailbox box(4);
    for (std::uint64_t turn = 1; turn <= 20; ++turn) {
        // Each turn moves the building at x = turn % 6 one tile east in the world.
        std::vector<anno::app::SceneBuilding> before = world;
        anno::app::SceneBuilding moved = world.front();
        anno::app::SceneBuilding next = moved;
        next.x = static_cast<std::uint16_t>(moved.x + 1);
        world.front() = next;
        anno::app::sort_buildings(world);
        const auto deltas = anno::app::diff_sorted_buildings(before, world);
        auto batch = std::make_shared<anno::app::BuildingBatch>();
        batch->turn = turn;
        batch->deltas = deltas;
        box.push(batch, world);
    }
    CHECK(box.pending() <= 4);
    const auto result = anno::app::apply_batches(picture, box.take());
    CHECK(result.consistent);
    CHECK(picture == world);
}

TEST_CASE("mailbox: applying every batch in order reaches the same list as a resync") {
    std::vector<anno::app::SceneBuilding> start = {at(0, 0, 1), at(1, 1, 2)};
    anno::app::sort_buildings(start);
    std::vector<anno::app::SceneBuilding> end = {at(0, 3, 1), at(4, 4, 9)};
    anno::app::sort_buildings(end);
    const auto deltas = anno::app::diff_sorted_buildings(start, end);

    auto batch = std::make_shared<anno::app::BuildingBatch>();
    batch->turn = 7;
    batch->deltas = deltas;

    std::vector<anno::app::SceneBuilding> normal = start;
    const auto applied = anno::app::apply_batches(normal, {batch});
    CHECK(applied.consistent);
    CHECK(normal == end);
    CHECK(applied.deltas == deltas);

    auto resync = std::make_shared<anno::app::BuildingBatch>();
    resync->turn = 7;
    resync->resync = true;
    resync->full = end;
    std::vector<anno::app::SceneBuilding> by_resync = start;
    const auto r = anno::app::apply_batches(by_resync, {resync});
    CHECK(by_resync == normal);
    CHECK(r.deltas == deltas);
}

TEST_CASE("mailbox: a delta for a building the picture does not hold is reported") {
    std::vector<anno::app::SceneBuilding> picture = {at(0, 0, 1)};
    auto batch = std::make_shared<anno::app::BuildingBatch>();
    batch->turn = 1;
    batch->deltas.push_back({anno::app::BuildingDelta::Kind::Removed, at(9, 9, 9)});
    const auto result = anno::app::apply_batches(picture, {batch});
    CHECK_FALSE(result.consistent);
    CHECK(picture.size() == 1);
}
