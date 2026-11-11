// The selection decision (selection.hpp, ADR 0004 "Selection", W4-NEXT-2 K): every rule as a
// pure step, no window. Ids and tiles are made up; only equality matters to the rules.
#include <doctest/doctest.h>

#include <anno/app/selection.hpp>

namespace app = anno::app;
namespace sim = anno::sim;

namespace {

constexpr sim::TileXY kHouse{100, 100};
constexpr sim::TileXY kOther{104, 100};
constexpr sim::TileXY kGround{110, 100};
const sim::BuildingId kHouseId{7};
const sim::BuildingId kOtherId{9};

app::SelectionEvent click(std::optional<sim::TileXY> tile,
                          app::PointerMode mode = app::PointerMode::Select) {
    app::SelectionEvent e;
    e.kind = app::SelectionEventKind::Click;
    e.mode = mode;
    e.tile = tile;
    return e;
}

app::SelectionEvent escape() {
    app::SelectionEvent e;
    e.kind = app::SelectionEventKind::Escape;
    return e;
}

app::SelectionEvent answer(std::optional<sim::TileXY> tile, std::optional<sim::BuildingId> id) {
    app::SelectionEvent e;
    e.kind = app::SelectionEventKind::Answer;
    e.tile = tile;
    e.building = id;
    return e;
}

app::SelectionState selected_house() {
    app::SelectionState s;
    s.selected = app::SelectedBuilding{kHouseId, kHouse};
    return s;
}

} // namespace

TEST_CASE("selection: a click on a building asks, and the answer selects it") {
    const auto asked = app::selection_step({}, click(kHouse));
    CHECK(asked.consumed);
    CHECK(asked.state.pending == kHouse);
    CHECK_FALSE(asked.state.selected);
    CHECK(asked.inspect == kHouse);

    const auto answered = app::selection_step(asked.state, answer(kHouse, kHouseId));
    CHECK_FALSE(answered.consumed);
    CHECK_FALSE(answered.state.pending);
    REQUIRE(answered.state.selected);
    CHECK(answered.state.selected->id == kHouseId);
    CHECK(answered.state.selected->tile == kHouse);
    CHECK(answered.inspect == kHouse); // keeps checking the selected building
}

TEST_CASE("selection: an answer for another tile does not resolve a pending click") {
    // One turn behind: the snapshot still answers the previously selected tile.
    app::SelectionState s = selected_house();
    const auto asked = app::selection_step(s, click(kOther));
    CHECK(asked.state.selected == s.selected); // no flicker while pending
    const auto stale = app::selection_step(asked.state, answer(kHouse, kHouseId));
    CHECK(stale.state == asked.state);
    const auto fresh = app::selection_step(stale.state, answer(kOther, kOtherId));
    REQUIRE(fresh.state.selected);
    CHECK(fresh.state.selected->id == kOtherId);
}

TEST_CASE("selection: a click on empty ground deselects once answered") {
    const auto asked = app::selection_step(selected_house(), click(kGround));
    CHECK(asked.consumed);
    const auto answered = app::selection_step(asked.state, answer(kGround, std::nullopt));
    CHECK(answered.state == app::SelectionState{});
    CHECK_FALSE(answered.inspect);
}

TEST_CASE("selection: a click off the map deselects at once, and is consumed only when there "
          "was a selection") {
    const auto with = app::selection_step(selected_house(), click(std::nullopt));
    CHECK(with.consumed);
    CHECK(with.state == app::SelectionState{});
    const auto without = app::selection_step({}, click(std::nullopt));
    CHECK_FALSE(without.consumed);
}

TEST_CASE("selection: the tools keep their clicks in every other mode") {
    for (const app::PointerMode mode :
         {app::PointerMode::Build, app::PointerMode::Demolish, app::PointerMode::DevTool}) {
        CAPTURE(static_cast<int>(mode));
        const auto step = app::selection_step(selected_house(), click(kOther, mode));
        CHECK_FALSE(step.consumed);
        CHECK(step.state == selected_house()); // unchanged: not even pending
        CHECK(step.inspect == kHouse);
    }
}

TEST_CASE("selection: Escape deselects with a selection and is not ours without one") {
    const auto with = app::selection_step(selected_house(), escape());
    CHECK(with.consumed);
    CHECK(with.state == app::SelectionState{});
    CHECK_FALSE(with.inspect);

    app::SelectionState pending;
    pending.pending = kHouse;
    const auto with_pending = app::selection_step(pending, escape());
    CHECK(with_pending.consumed);
    CHECK(with_pending.state == app::SelectionState{});

    const auto without = app::selection_step({}, escape());
    CHECK_FALSE(without.consumed); // the window's own Escape (quit) still sees it
}

TEST_CASE("selection: a building demolished meanwhile drops the selection") {
    const auto gone = app::selection_step(selected_house(), answer(kHouse, std::nullopt));
    CHECK(gone.state == app::SelectionState{});
    const auto replaced = app::selection_step(selected_house(), answer(kHouse, kOtherId));
    CHECK(replaced.state == app::SelectionState{});
    // Negative control: the same building answered again keeps it.
    const auto same = app::selection_step(selected_house(), answer(kHouse, kHouseId));
    CHECK(same.state == selected_house());
    // An answer about another tile (the developer tool's hover) says nothing about it.
    const auto elsewhere = app::selection_step(selected_house(), answer(kOther, std::nullopt));
    CHECK(elsewhere.state == selected_house());
    // A snapshot without an answer (no city, nothing requested) changes nothing.
    const auto none = app::selection_step(selected_house(), answer(std::nullopt, std::nullopt));
    CHECK(none.state == selected_house());
}

TEST_CASE("selection: a press and release within the slop is a click, beyond it a drag") {
    CHECK(app::is_click(0.0, 0.0, 4.0));
    CHECK(app::is_click(4.0, -4.0, 4.0));
    CHECK_FALSE(app::is_click(4.5, 0.0, 4.0));
    CHECK_FALSE(app::is_click(0.0, -12.0, 4.0));
}
