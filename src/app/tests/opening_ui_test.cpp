// The opening's controls (W4-OPENING-1; docs/app/ship-opening.md): selection by stable id, the
// order gestures, the founding and unload modes, and the tracking of each command's own outcome.
// The controls are window-free; the projection here is a plain linear map (10 pixels a tile), so
// every expectation is hand-derived. The real window path is tested through the production window
// (tools/test/viewer_wiring.py, the opening cases) and the real-file cases of opening_test.cpp.
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>

#include <doctest/doctest.h>

#include <anno/app/opening_ui.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/types.hpp>

namespace app = anno::app;
namespace sim = anno::sim;

namespace {

constexpr double kPx = 10.0; // pixels per tile of the fake projection

/// pixel = tile * 10. The sea point is the exact Q24.8 of the pixel; ground is a set of tiles.
class FlatProjection final : public app::ScreenProjection {
  public:
    std::set<std::pair<int, int>> land;
    /// How much work the controls asked of the frame (a count, not a time).
    mutable std::size_t picks = 0, boxes = 0;
    std::optional<app::PickedPoint> pick(double px, double py) const override {
        ++picks;
        if (px < 0 || py < 0) {
            return std::nullopt;
        }
        app::PickedPoint p;
        p.x_q8 = static_cast<std::int32_t>(px / kPx * 256.0);
        p.y_q8 = static_cast<std::int32_t>(py / kPx * 256.0);
        p.tile =
            sim::TileXY{static_cast<std::int32_t>(px / kPx), static_cast<std::int32_t>(py / kPx)};
        p.ground = land.contains({p.tile.x, p.tile.y});
        return p;
    }
    std::optional<app::ScreenBox> ship_box(const app::ShipView& s) const override {
        ++boxes;
        const double cx = s.pos.x / 256.0 * kPx, cy = s.pos.y / 256.0 * kPx;
        app::ScreenBox b;
        b.x0 = cx - 8;
        b.y0 = cy - 8;
        b.x1 = cx + 8;
        b.y1 = cy + 8;
        b.cx = cx;
        b.cy = cy;
        return b;
    }
};

app::ShipView ship(std::uint64_t id, std::uint32_t owner, double tx, double ty,
                   std::vector<app::ShipHoldView> holds = {}) {
    app::ShipView s;
    s.id = sim::FigureId{id};
    s.owner = sim::PlayerId{owner};
    s.kind = 1502;
    s.pos = sim::FixedPos{static_cast<std::int32_t>(tx * 256), static_cast<std::int32_t>(ty * 256)};
    s.holds = std::move(holds);
    return s;
}

app::OpeningSetup setup() {
    app::OpeningSetup s;
    s.local = sim::PlayerId{0};
    s.founding_types = {sim::TypeId{1302}};
    s.cargo.dock_half_widths = {5, 5, 5, 4, 3, 2};
    s.cargo.warehouse_prodtypes = {35};
    s.islands = {app::IslandRect{0, 40, 40, 50, 50}};
    s.world_w = 200;
    s.world_h = 200;
    // A registry-free setup: the founding preview then has no type to read; the founding tests
    // that need one build a registry.
    return s;
}

std::shared_ptr<const sim::TypeRegistry> kontor_registry() {
    sim::TypeRegistry::Builder b;
    sim::BuildingType kontor;
    kontor.id = sim::TypeId{1302};
    kontor.prodtype = 35;
    kontor.width = 4;
    kontor.height = 4;
    kontor.cost_gold = 350;
    b.add(kontor);
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

struct Fixture {
    app::OpeningUi ui;
    FlatProjection proj;
    app::OpeningView view;
    std::function<app::CommandOutcome(app::CommandId)> outcomes;
    std::map<std::uint64_t, app::CommandOutcome> table;
    Fixture() : ui(setup()) {
        view.local = sim::PlayerId{0};
        view.ships = {ship(1, 0, 10, 10, {{42, 12800}, {26, 2560}}), ship(4, 1, 30, 10)};
        outcomes = [this](app::CommandId id) {
            const auto it = table.find(id.n);
            if (it == table.end()) {
                app::CommandOutcome o;
                o.id = id;
                o.state = app::CommandOutcome::State::Queued;
                return o;
            }
            return it->second;
        };
    }
    [[nodiscard]] app::UiFeed feed() const {
        app::UiFeed f;
        f.view = &view;
        f.outcome = outcomes;
        return f;
    }
    app::UiResult click(app::PointerButton b, double px, double py) {
        ui.pointer_down(b, px, py);
        return ui.pointer_up(b, px, py, &proj, feed());
    }
    app::UiResult key(app::OpeningKey k, bool shift = false) { return ui.key(k, shift, feed()); }
};

} // namespace

TEST_CASE(
    "controls: a left click selects the ship under it by its stable id, and nothing deselects") {
    Fixture f;
    CHECK_FALSE(f.ui.selected().has_value());
    const auto r = f.click(app::PointerButton::Left, 100, 100); // ship 1 at (10, 10) tiles
    CHECK(r.taken);
    CHECK(r.commands.empty());
    REQUIRE(f.ui.selected().has_value());
    CHECK(f.ui.selected()->v == 1);
    // The other participant's ship can be inspected, never controlled.
    f.click(app::PointerButton::Left, 300, 100);
    REQUIRE(f.ui.selected().has_value());
    CHECK(f.ui.selected()->v == 4);
    const auto order = f.click(app::PointerButton::Right, 200, 150);
    CHECK(order.commands.empty());
    CHECK(f.ui.message().find("inspect only") != std::string::npos);
    // Empty sea deselects.
    f.click(app::PointerButton::Left, 500, 500);
    CHECK_FALSE(f.ui.selected().has_value());
}

TEST_CASE("controls: of overlapping ships the front-most is picked, whatever the list order") {
    Fixture f;
    // Ship 2 stands in front of ship 1 (a larger x + y) and their boxes overlap at the click.
    f.view.ships = {ship(2, 0, 11, 11), ship(1, 0, 10, 10)};
    std::sort(f.view.ships.begin(), f.view.ships.end(),
              [](const app::ShipView& a, const app::ShipView& b) { return a.id < b.id; });
    f.click(app::PointerButton::Left, 105, 105);
    REQUIRE(f.ui.selected().has_value());
    CHECK(f.ui.selected()->v == 2);
    // The same click, the ships reversed in x + y: now ship 1 is in front.
    Fixture g;
    g.view.ships = {ship(1, 0, 11, 11), ship(2, 0, 10, 10)};
    g.click(app::PointerButton::Left, 105, 105);
    CHECK(g.ui.selected()->v == 1);
}

TEST_CASE("controls: a right click orders the selected own ship to the exact sea point, once") {
    Fixture f;
    f.click(app::PointerButton::Left, 100, 100);
    const auto r = f.click(app::PointerButton::Right, 200, 150); // tile (20, 15)
    REQUIRE(r.commands.size() == 1);
    CHECK(r.commands[0].issuer.v == 0);
    const auto* move = std::get_if<sim::MoveCmd>(&r.commands[0].body);
    REQUIRE(move != nullptr);
    CHECK(move->ship.v == 1);
    CHECK(move->target_x_q8 == 20 * 256);
    CHECK(move->target_y_q8 == 15 * 256);
    // The same release does not act again.
    const auto again = f.ui.pointer_up(app::PointerButton::Right, 200, 150, &f.proj, f.feed());
    CHECK(again.commands.empty());
    // Land, outside the world and no selection produce no command and say why.
    f.proj.land.insert({30, 30});
    CHECK(f.click(app::PointerButton::Right, 300, 300).commands.empty());
    CHECK(f.ui.message().find("land") != std::string::npos);
    CHECK(f.click(app::PointerButton::Right, 2500, 100).commands.empty());
    CHECK(f.ui.message().find("outside the world") != std::string::npos);
    f.click(app::PointerButton::Left, 800, 800); // deselect
    CHECK(f.click(app::PointerButton::Right, 200, 150).commands.empty());
    CHECK(f.ui.message().find("select one of your ships") != std::string::npos);
}

TEST_CASE(
    "controls: a pan is not an order, a cancelled press is not an order, a click after them is") {
    Fixture f;
    f.click(app::PointerButton::Left, 100, 100);
    // A right-button drag of 20 pixels: a pan.
    f.ui.pointer_down(app::PointerButton::Right, 200, 150);
    f.ui.pointer_move(220, 150, &f.proj, f.feed());
    CHECK(f.ui.pointer_up(app::PointerButton::Right, 220, 150, &f.proj, f.feed()).commands.empty());
    // A press that never moved, but focus was lost in between: cancelled.
    f.ui.pointer_down(app::PointerButton::Right, 200, 150);
    f.ui.abort_gestures();
    CHECK(f.ui.pointer_up(app::PointerButton::Right, 200, 150, &f.proj, f.feed()).commands.empty());
    // Esc during a press cancels it (the gesture is the first thing Esc cancels).
    f.ui.pointer_down(app::PointerButton::Right, 200, 150);
    CHECK(f.key(app::OpeningKey::Escape).taken);
    CHECK(f.ui.pointer_up(app::PointerButton::Right, 200, 150, &f.proj, f.feed()).commands.empty());
    CHECK(f.ui.selected().has_value()); // the selection survived the gesture's cancel
    // The press travelled, then came back to where it started: still a pan (the travel counts).
    f.ui.pointer_down(app::PointerButton::Right, 200, 150);
    f.ui.pointer_move(260, 150, &f.proj, f.feed());
    f.ui.pointer_move(200, 150, &f.proj, f.feed());
    CHECK(f.ui.pointer_up(app::PointerButton::Right, 200, 150, &f.proj, f.feed()).commands.empty());
    // A fresh, steady click orders.
    CHECK(f.click(app::PointerButton::Right, 200, 150).commands.size() == 1);
    // A left drag does not select (it pans).
    f.ui.pointer_down(app::PointerButton::Left, 100, 100);
    f.ui.pointer_move(140, 100, &f.proj, f.feed());
    static_cast<void>(f.ui.pointer_up(app::PointerButton::Left, 140, 100, &f.proj, f.feed()));
    CHECK(f.ui.selected().has_value());
}

TEST_CASE("controls: a stop is for the selected own ship, Esc cancels gesture, mode, selection, "
          "then nothing") {
    Fixture f;
    CHECK_FALSE(f.key(app::OpeningKey::Halt).taken == false); // taken: it answers with a message
    CHECK(f.key(app::OpeningKey::Halt).commands.empty());
    f.click(app::PointerButton::Left, 100, 100);
    const auto r = f.key(app::OpeningKey::Halt);
    REQUIRE(r.commands.size() == 1);
    CHECK(std::holds_alternative<sim::StopCmd>(r.commands[0].body));
    // Esc: a mode first, then the selection, then nothing (the window then quits).
    f.key(app::OpeningKey::Unload);
    CHECK(f.ui.mode() == app::OpeningMode::Unload);
    CHECK(f.key(app::OpeningKey::Escape).taken);
    CHECK(f.ui.mode() == app::OpeningMode::Select);
    CHECK(f.ui.selected().has_value());
    CHECK(f.key(app::OpeningKey::Escape).taken);
    CHECK_FALSE(f.ui.selected().has_value());
    CHECK_FALSE(f.key(app::OpeningKey::Escape).taken);
    CHECK_FALSE(f.ui.has_pending());
}

TEST_CASE("controls: founding derives the sim's origin from the click, carries the cursor's turn, "
          "and is one click") {
    Fixture f;
    auto s = setup();
    s.registry = kontor_registry();
    f.ui = app::OpeningUi(s);
    f.key(app::OpeningKey::Found);
    CHECK(f.ui.mode() == app::OpeningMode::Select); // no selected ship: no mode
    f.click(app::PointerButton::Left, 100, 100);
    f.key(app::OpeningKey::Found);
    REQUIRE(f.ui.mode() == app::OpeningMode::Found);
    f.proj.land.insert({50, 50});
    f.ui.pointer_move(500, 500, &f.proj, f.feed());
    const auto preview = f.ui.found_preview(f.feed());
    REQUIRE(preview.has_value());
    CHECK(preview->w == 4);
    CHECK(preview->origin.x == 49); // 50 - (4 - 1) / 2
    CHECK(preview->origin.y == 49);
    // The ship is at (10, 10): the click is 40 tiles away; the preview says so (advisory only).
    CHECK(preview->advisory.find("too far") != std::string::npos);
    const auto r = f.click(app::PointerButton::Left, 500, 500);
    REQUIRE(r.commands.size() == 1); // the sim decides: the advisory does not stop the command
    const auto* found = std::get_if<sim::FoundCmd>(&r.commands[0].body);
    REQUIRE(found != nullptr);
    CHECK(found->ship.v == 1);
    CHECK(found->placement.type.v == 1302);
    CHECK(found->placement.x == 49);
    CHECK(found->placement.y == 49);
    CHECK(found->placement.rotation == 0);
    CHECK(found->click_x == 50);
    CHECK(found->click_y == 50);
    // One click, one command: the mode ended; the next click selects instead of founding.
    CHECK(f.ui.mode() == app::OpeningMode::Select);
    CHECK(f.click(app::PointerButton::Left, 500, 500).commands.empty());
    // The cursor's turn (owner 2026-11-03, restored): T turns it on, `,` back; the command carries
    // it, the sim's trials start there and still turn the warehouse to the coast. The origin is
    // clamped at 0.
    f.click(app::PointerButton::Left, 100, 100);
    f.key(app::OpeningKey::Found);
    CHECK(f.key(app::OpeningKey::Rotate).taken);
    const auto rotated = f.click(app::PointerButton::Left, 5, 5); // tile (0, 0)
    REQUIRE(rotated.commands.size() == 1);
    const auto* edge = std::get_if<sim::FoundCmd>(&rotated.commands[0].body);
    REQUIRE(edge != nullptr);
    CHECK(edge->placement.rotation == 1);
    CHECK(edge->placement.x == 0);
    CHECK(edge->placement.y == 0);
    // Kept for the next founding: `,` turns it back, from 1 to 0, then from 0 to 3.
    for (const std::uint8_t want : {std::uint8_t{0}, std::uint8_t{3}}) {
        f.click(app::PointerButton::Left, 100, 100);
        f.key(app::OpeningKey::Found);
        CHECK(f.key(app::OpeningKey::RotateBack).taken);
        const auto back = f.click(app::PointerButton::Left, 500, 500);
        REQUIRE(back.commands.size() == 1);
        const auto* turned = std::get_if<sim::FoundCmd>(&back.commands[0].body);
        REQUIRE(turned != nullptr);
        CHECK(turned->placement.rotation == want);
    }
    // Esc cancels the mode without a command.
    f.click(app::PointerButton::Left, 100, 100);
    f.key(app::OpeningKey::Found);
    f.key(app::OpeningKey::Escape);
    CHECK(f.ui.mode() == app::OpeningMode::Select);
    CHECK(f.key(app::OpeningKey::Rotate).taken == false); // T is the building tool's again
}

// Where two turns fit (a convex corner: the beach on two edges, water opposite each), the cursor's
// turn decides, as the original's cursor byte does (owner 2026-11-03: restore it); elsewhere the
// warehouse still turns to the shore by itself.
TEST_CASE("controls: on a corner where two turns fit, the founding cursor's turn decides") {
    Fixture f;
    auto s = setup();
    sim::TypeRegistry::Builder b;
    sim::BuildingType kontor;
    kontor.id = sim::TypeId{1302};
    kontor.prodtype = 35;
    kontor.width = 4;
    kontor.height = 4;
    kontor.flags_word = 0x2007; // the coast class
    b.add(kontor);
    auto built = b.build();
    REQUIRE(built.ok());
    s.registry = built.value();
    f.ui = app::OpeningUi(s);
    f.click(app::PointerButton::Left, 100, 100); // ship 1
    f.key(app::OpeningKey::Found);
    REQUIRE(f.ui.mode() == app::OpeningMode::Found);
    // The click (60, 60) puts the origin at (59, 59). Beach (0x30) where dx + dy <= 4 from the
    // origin, water (0x10) beyond: turn 0 (beach edge y = 0, water at y = 4) and turn 1 (beach
    // edge x = 0, water at x = 4) both fit, 2 and 3 do not.
    app::UiFeed feed = f.feed();
    feed.vertex_height = [](std::int64_t x, std::int64_t y) {
        return std::optional<std::uint8_t>((x - 59) + (y - 59) <= 4 ? 0x30 : 0x10);
    };
    const auto preview = [&] {
        f.ui.pointer_move(605, 605, &f.proj, feed);
        return f.ui.found_preview(feed);
    };
    const std::array<std::uint8_t, 4> want{0, 1, 0, 0}; // trials from 2: 2, 3, 0; from 3: 3, 0
    for (std::uint8_t turn = 0; turn < 4; ++turn) {
        const auto p = preview();
        REQUIRE(p);
        CAPTURE(int{turn});
        CHECK(p->cursor_rotation == turn);
        CHECK(p->cmd.placement.rotation == turn);
        CHECK(p->coast_ok);
        CHECK(p->rotation == want[turn]);
        // The status line shows the cursor's turn next to the rotation the warehouse takes.
        CHECK(f.ui.status_line(feed).find("rot " + std::to_string(want[turn]) + " (turn " +
                                          std::to_string(turn) + ")") != std::string::npos);
        f.key(app::OpeningKey::Rotate);
    }
}

// The founding preview's branches (OpeningUi::make_found; review 2026-11-03: only the window script
// reached them): the island and footprint advisories, the coast rule turning the warehouse to the
// shore or finding no coast, and the sim thread's answer for exactly this command replacing the
// local guess. The click (60, 60) puts a 4 x 4 warehouse at (59, 59) on the island (40..89).
TEST_CASE("controls: the founding preview's advisories, the coast rule and the sim's answer") {
    Fixture f;
    auto s = setup();
    sim::TypeRegistry::Builder b;
    sim::BuildingType kontor;
    kontor.id = sim::TypeId{1302};
    kontor.prodtype = 35;
    kontor.width = 4;
    kontor.height = 4;
    kontor.flags_word = 0x2007; // the coast class (site::coast_class_supported)
    b.add(kontor);
    auto built = b.build();
    REQUIRE(built.ok());
    s.registry = built.value();
    f.ui = app::OpeningUi(s);
    f.click(app::PointerButton::Left, 100, 100); // ship 1
    f.key(app::OpeningKey::Found);
    REQUIRE(f.ui.mode() == app::OpeningMode::Found);
    const auto preview_at = [&](int tx, int ty, const app::UiFeed& feed) {
        f.ui.pointer_move(tx * kPx + 5, ty * kPx + 5, &f.proj, feed);
        return f.ui.found_preview(feed);
    };
    const auto with_heights =
        [&](std::function<std::optional<std::uint8_t>(std::int64_t, std::int64_t)> h) {
            app::UiFeed feed = f.feed();
            feed.vertex_height = std::move(h);
            return feed;
        };
    const auto has = [](const app::FoundPreview& p, const char* text) {
        return p.advisory.find(text) != std::string::npos;
    };
    // Where the click is.
    auto p = preview_at(20, 20, f.feed());
    REQUIRE(p);
    CHECK(has(*p, "on no island"));
    p = preview_at(89, 60, f.feed()); // origin (88, 59): the footprint reaches x = 91
    REQUIRE(p);
    CHECK(has(*p, "footprint leaves the island"));
    // The coast rule: the shore edge 0x30, water (0x10) on the opposite edge.
    p = preview_at(60, 60, with_heights([](std::int64_t, std::int64_t y) {
                       return std::optional<std::uint8_t>(y <= 61 ? 0x30 : 0x10);
                   }));
    REQUIRE(p);
    CHECK(p->coast_ok);
    CHECK(p->rotation == 0); // water at +y: rotation 0
    CHECK(has(*p, "it faces the shore"));
    p = preview_at(60, 60, with_heights([](std::int64_t x, std::int64_t) {
                       return std::optional<std::uint8_t>(x <= 61 ? 0x30 : 0x10);
                   }));
    REQUIRE(p);
    CHECK(p->coast_ok);
    CHECK(p->rotation == 1); // water at +x: the second trial
    CHECK(p->cmd.placement.rotation == app::kFoundingCursorRotation); // the command keeps its own
    p = preview_at(60, 60, with_heights([](std::int64_t, std::int64_t) {
                       return std::optional<std::uint8_t>(0x30);
                   }));
    REQUIRE(p);
    CHECK_FALSE(p->coast_ok);
    CHECK(has(*p, "not on a coast"));
    p = preview_at(60, 60, with_heights([](std::int64_t, std::int64_t) {
                       return std::optional<std::uint8_t>{};
                   }));
    REQUIRE(p);
    CHECK_FALSE(p->coast_ok);
    CHECK_FALSE(has(*p, "coast")); // unreadable: no verdict
    // The sim's answer for this very command replaces the local guess; one for another is ignored.
    p = preview_at(60, 60, f.feed());
    REQUIRE(p);
    CHECK_FALSE(p->sim_answer.has_value());
    const sim::FoundCmd cmd = p->cmd;
    const auto answered = [&](sim::Reject reason, std::optional<std::uint8_t> rotation) {
        f.view.found_probe = app::FoundingProbeView{cmd, reason, rotation};
        return preview_at(60, 60, f.feed());
    };
    p = answered(sim::Reject::None, 2);
    REQUIRE(p);
    CHECK(p->sim_answer == sim::Reject::None);
    CHECK(p->coast_ok);
    CHECK(p->rotation == 2);
    CHECK(has(*p, "the sim accepts it here"));
    CHECK_FALSE(has(*p, "too far")); // the local guesses are replaced
    for (const auto& [reason, text] :
         {std::pair{sim::Reject::NoRotationFits, "(NoRotationFits)"},
          std::pair{sim::Reject::OutOfRange, "(OutOfRange)"},
          std::pair{sim::Reject::OutOfIsland, "(OutOfIsland)"},
          std::pair{sim::Reject::UnknownIsland, "(UnknownIsland)"},
          std::pair{sim::Reject::SiteOwned, "the sim refuses it here: SiteOwned"}}) {
        p = answered(reason, std::nullopt);
        REQUIRE(p);
        CHECK(p->sim_answer == reason);
        CHECK_FALSE(p->coast_ok);
        CHECK_MESSAGE(has(*p, text), p->advisory);
    }
    // An answer for another click: the preview at (61, 60) does not take it.
    f.view.found_probe = app::FoundingProbeView{cmd, sim::Reject::None, 2};
    p = preview_at(61, 60, f.feed());
    REQUIRE(p);
    CHECK_FALSE(p->sim_answer.has_value());
}

TEST_CASE("controls: unloading asks for one hold and a quantity, from the stable ids of the view") {
    Fixture f;
    f.view.ships[0].holds = {{42, 12800}, {26, 2560}, {41, 0}, {41, 7680 + 128}};
    app::CityView city;
    city.ref = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    city.owner = sim::PlayerId{0};
    city.name = "Fairbenks";
    app::WarehouseView wh;
    wh.id = sim::BuildingId{(std::uint64_t{0} << 32) + 1};
    wh.type = sim::TypeId{1302};
    wh.tile = sim::TileXY{8, 8};
    wh.w = 4;
    wh.h = 4;
    city.warehouse = wh;
    city.stock_88.assign(64, 0);
    city.stock_88[41] = 50 * 256 - 3 * 256; // 47 t of goods 41: room for 3 t
    city.capacity_88 = 50 * 256;
    f.view.cities = {city};
    f.click(app::PointerButton::Left, 100, 100);
    f.key(app::OpeningKey::Unload);
    REQUIRE(f.ui.mode() == app::OpeningMode::Unload);
    auto plan = f.ui.unload_plan(f.feed());
    REQUIRE(plan.has_value());
    CHECK(plan->hold == 0);
    CHECK(plan->goods == 42);
    CHECK(plan->tons == 50);
    CHECK(plan->warehouse == wh.id);
    CHECK(plan->advisory.find("in range") != std::string::npos);
    // The quantity moves by one ton, or ten with Shift, within 1 .. the hold's tons.
    f.key(app::OpeningKey::QtyDown);
    f.key(app::OpeningKey::QtyDown, true);
    CHECK(f.ui.unload_plan(f.feed())->tons == 39);
    f.key(app::OpeningKey::QtyUp, true);
    f.key(app::OpeningKey::QtyUp, true);
    CHECK(f.ui.unload_plan(f.feed())->tons == 50); // never above what the hold holds
    // Tab skips an empty hold: 0 -> 1 -> 3 (hold 2 is empty), and the quantity follows the hold.
    f.key(app::OpeningKey::NextHold);
    CHECK(f.ui.unload_plan(f.feed())->hold == 1);
    f.key(app::OpeningKey::NextHold);
    plan = f.ui.unload_plan(f.feed());
    CHECK(plan->hold == 3);
    CHECK(plan->goods == 41);
    CHECK(plan->tons == 30);
    // A fraction of a ton is still a whole-ton request: the request is capped by the hold.
    f.key(app::OpeningKey::QtyUp, true);
    CHECK(f.ui.unload_plan(f.feed())->tons == 31); // 30.5 t aboard: 31 t may be asked
    // The advisory knows the store is nearly full: it would move 3 t, not 31.
    CHECK(f.ui.unload_plan(f.feed())->advisory.find("would move 3 t") != std::string::npos);
    const auto r = f.key(app::OpeningKey::Send);
    REQUIRE(r.commands.size() == 1);
    const auto* unload = std::get_if<sim::UnloadCmd>(&r.commands[0].body);
    REQUIRE(unload != nullptr);
    CHECK(unload->ship.v == 1);
    CHECK(unload->warehouse == wh.id);
    CHECK(unload->hold == 3);
    CHECK(unload->goods == 41);
    CHECK(unload->tons == 31);
    // Nothing is sent without a target: the warehouse vanishes from the view.
    f.view.cities.clear();
    f.ui.on_view(f.feed());
    const auto none = f.key(app::OpeningKey::Send);
    CHECK(none.commands.empty());
    CHECK(f.ui.message().find("no warehouse") != std::string::npos);
}

TEST_CASE("controls: a vanished ship drops the selection and the mode, modes exclude each other") {
    Fixture f;
    f.click(app::PointerButton::Left, 100, 100);
    f.key(app::OpeningKey::Unload);
    f.key(app::OpeningKey::Found);
    CHECK(f.ui.mode() == app::OpeningMode::Found); // entering one leaves the other
    f.view.ships.erase(f.view.ships.begin());
    f.ui.on_view(f.feed());
    CHECK_FALSE(f.ui.selected().has_value());
    CHECK(f.ui.mode() == app::OpeningMode::Select);
    CHECK(f.ui.message().find("gone") != std::string::npos);
    // The building tool taking the pointer leaves the modes.
    const app::ShipView other = f.view.ships.front();
    f.view.ships = {ship(1, 0, 10, 10), other};
    f.click(app::PointerButton::Left, 100, 100);
    f.key(app::OpeningKey::Found);
    f.ui.leave_modes();
    CHECK(f.ui.mode() == app::OpeningMode::Select);
}

TEST_CASE("controls: each command's own state: acceptance is not arrival, a replacement is not its "
          "predecessor") {
    Fixture f;
    f.click(app::PointerButton::Left, 100, 100);
    f.click(app::PointerButton::Right, 200, 150);
    f.ui.submitted(0, app::CommandId{app::CommandOrigin::Window, 1}, 5);
    f.click(app::PointerButton::Right, 300, 150);
    f.ui.submitted(0, app::CommandId{app::CommandOrigin::Window, 2}, 6);
    REQUIRE(f.ui.tracked().size() == 2);
    const auto& first = f.ui.tracked()[0];
    const auto& second = f.ui.tracked()[1];
    using S = app::TrackState;
    // Nothing recorded: queued.
    CHECK(f.ui.state_of(first, f.feed()) == S::Queued);
    // Command 1 accepted as order 7, command 2 as order 8 (the second replaced the first). The ship
    // carries order 8 and the first has no event: it is superseded, never "arrived".
    app::CommandOutcome a;
    a.id = first.receipt;
    a.state = app::CommandOutcome::State::Applied;
    a.order_id = 7;
    f.table[1] = a;
    a.id = second.receipt;
    a.order_id = 8;
    f.table[2] = a;
    f.view.ships[0].order_id = 8;
    CHECK(f.ui.state_of(first, f.feed()) == S::Superseded);
    CHECK(f.ui.state_of(second, f.feed()) == S::Planning);
    f.view.ships[0].routed = true;
    CHECK(f.ui.state_of(second, f.feed()) == S::Sailing);
    // An arrival is for ITS order: the event of order 7 does not end command 2.
    app::ShipEventView e;
    e.kind = app::ShipEventKind::Arrived;
    e.ship = sim::FigureId{1};
    e.order_id = 7;
    e.tick = 100;
    f.view.events = {e};
    CHECK(f.ui.state_of(first, f.feed()) == S::Arrived);
    CHECK(f.ui.state_of(second, f.feed()) == S::Sailing);
    e.order_id = 8;
    e.kind = app::ShipEventKind::RouteFailed;
    e.reason = 0;
    f.view.events = {e};
    CHECK(f.ui.state_of(second, f.feed()) == S::RouteFailed);
    e.kind = app::ShipEventKind::Stopped;
    f.view.events = {e};
    CHECK(f.ui.state_of(second, f.feed()) == S::Stopped);
    // The status line says "shown" only once a delivered frame drew the arrival's tick.
    e.kind = app::ShipEventKind::Arrived;
    f.view.events = {e};
    f.view.tick = 120;
    auto feed = f.feed();
    feed.shown_tick = 99;
    CHECK(f.ui.status_line(feed).find("not drawn yet") != std::string::npos);
    feed.shown_tick = 120;
    CHECK(f.ui.status_line(feed).find("(shown)") != std::string::npos);
    // A refusal is shown with the sim's reason and is not an order.
    app::CommandOutcome refused;
    refused.id = second.receipt;
    refused.state = app::CommandOutcome::State::Refused;
    refused.status = sim::Reject::NotShipOwner;
    f.table[2] = refused;
    CHECK(f.ui.state_of(second, f.feed()) == S::Refused);
    CHECK(f.ui.status_line(f.feed()).find("NotShipOwner") != std::string::npos);
    // The ship is gone: the order's state says so.
    a.id = second.receipt;
    a.order_id = 8;
    f.table[2] = a;
    f.view.events.clear();
    f.view.ships.erase(f.view.ships.begin());
    CHECK(f.ui.state_of(second, f.feed()) == S::ShipGone);
}

TEST_CASE("controls: the unload line reports the actual transfer, not the request") {
    Fixture f;
    f.view.ships[0].holds = {{42, 12800}};
    app::CityView city;
    city.ref = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    app::WarehouseView wh;
    wh.id = sim::BuildingId{1};
    wh.tile = sim::TileXY{8, 8};
    wh.w = wh.h = 4;
    city.warehouse = wh;
    city.stock_88.assign(64, 0);
    city.capacity_88 = 50 * 256;
    f.view.cities = {city};
    f.click(app::PointerButton::Left, 100, 100);
    f.key(app::OpeningKey::Unload);
    const auto r = f.key(app::OpeningKey::Send);
    REQUIRE(r.commands.size() == 1);
    f.ui.submitted(0, app::CommandId{app::CommandOrigin::Window, 1}, 1);
    app::CommandOutcome o;
    o.id = app::CommandId{app::CommandOrigin::Window, 1};
    o.state = app::CommandOutcome::State::Applied;
    o.transferred_88 = 5 * 256; // limited: 5 t of the 50 asked
    f.table[1] = o;
    f.view.ships[0].holds = {{42, 12800 - 5 * 256}};
    const std::string line = f.ui.status_line(f.feed());
    CHECK(line.find("moved 5 t of 50 requested (limited)") != std::string::npos);
    CHECK(line.find("hold 50 -> 45 t") != std::string::npos);
}

TEST_CASE("controls: tons are written exactly") {
    CHECK(app::tons_text(0) == "0");
    CHECK(app::tons_text(256) == "1");
    CHECK(app::tons_text(12800) == "50");
    CHECK(app::tons_text(128) == "0.50");
    CHECK(app::tons_text(256 + 64) == "1.25");
}

TEST_CASE("controls: a click's work is local to the ships of the view, and a move or a hover is "
          "one pick") {
    Fixture f;
    // A view with many ships and no buildings anywhere in sight: a selecting click asks for one box
    // per ship, once, and no ground pick; hovering and an order are one pick each.
    f.view.ships.clear();
    for (std::uint64_t i = 1; i <= 200; ++i) {
        f.view.ships.push_back(ship(i, static_cast<std::uint32_t>(i % 5),
                                    static_cast<double>(i % 40) * 3,
                                    static_cast<double>(i / 40) * 3));
    }
    f.proj.picks = f.proj.boxes = 0;
    f.click(app::PointerButton::Left, 5, 5);
    CHECK(f.proj.boxes == f.view.ships.size());
    CHECK(f.proj.picks == 0);
    f.proj.picks = f.proj.boxes = 0;
    f.ui.pointer_move(50, 50, &f.proj, f.feed());
    CHECK(f.proj.picks == 1);
    CHECK(f.proj.boxes == 0);
    // An order for an own selected ship: one pick, and no box is asked for.
    f.view.ships = {ship(1, 0, 10, 10)};
    f.click(app::PointerButton::Left, 100, 100);
    f.proj.picks = f.proj.boxes = 0;
    f.click(app::PointerButton::Right, 200, 150);
    CHECK(f.proj.picks == 1);
    CHECK(f.proj.boxes == 0);
}

TEST_CASE("controls: founding derives the origin per axis for a footprint that is not square, at "
          "each of the cursor's turns") {
    sim::TypeRegistry::Builder b;
    sim::BuildingType wide;
    wide.id = sim::TypeId{1302};
    wide.prodtype = 35;
    wide.width = 3;
    wide.height = 5;
    b.add(wide);
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    auto s = setup();
    s.registry = built.value();
    s.islands = {app::IslandRect{0, 0, 0, 200, 200}};
    // ox = max(0, cx - (w - 1) / 2), oy likewise, with the footprint of the command's rotation,
    // the cursor's turn after `rot` presses of T: 3 x 5 (half 1, 2) at even turns, 5 x 3 at odd.
    static_assert(app::kFoundingCursorRotation == 0);
    for (int rot = 0; rot < 4; ++rot) {
        const int expect_dx = rot % 2 == 0 ? 1 : 2, expect_dy = rot % 2 == 0 ? 2 : 1;
        Fixture f;
        f.ui = app::OpeningUi(s);
        f.click(app::PointerButton::Left, 100, 100);
        f.key(app::OpeningKey::Found);
        for (int i = 0; i < rot; ++i) {
            f.key(app::OpeningKey::Rotate);
        }
        const auto r = f.click(app::PointerButton::Left, 500, 700); // tile (50, 70)
        REQUIRE(r.commands.size() == 1);
        const auto* found = std::get_if<sim::FoundCmd>(&r.commands[0].body);
        REQUIRE(found != nullptr);
        CAPTURE(rot);
        CHECK(found->placement.rotation == rot);
        CHECK(found->placement.x == 50 - expect_dx);
        CHECK(found->placement.y == 70 - expect_dy);
        // Near the edge the origin is clamped at 0 on each axis separately.
        Fixture g;
        g.ui = app::OpeningUi(s);
        g.click(app::PointerButton::Left, 100, 100);
        g.key(app::OpeningKey::Found);
        for (int i = 0; i < rot; ++i) {
            g.key(app::OpeningKey::Rotate);
        }
        const auto edge = g.click(app::PointerButton::Left, 15, 500); // tile (1, 50)
        const auto* e = std::get_if<sim::FoundCmd>(&edge.commands[0].body);
        REQUIRE(e != nullptr);
        CHECK(e->placement.x == std::max(0, 1 - expect_dx));
        CHECK(e->placement.y == 50 - expect_dy);
    }
}

TEST_CASE("controls: a press whose release never comes does not swallow Esc for good") {
    Fixture f;
    f.click(app::PointerButton::Left, 100, 100);
    f.ui.pointer_down(app::PointerButton::Right, 200, 150);
    f.ui.abort_gestures(); // focus lost, the release goes elsewhere
    CHECK_FALSE(f.ui.pressed());
    CHECK(f.key(app::OpeningKey::Escape).taken); // clears the selection
    CHECK_FALSE(f.ui.selected().has_value());
    CHECK_FALSE(f.key(app::OpeningKey::Escape).taken); // nothing left: the window's own
    // Esc with the press still down cancels it and forgets it as well.
    f.click(app::PointerButton::Left, 100, 100);
    f.ui.pointer_down(app::PointerButton::Right, 200, 150);
    CHECK(f.key(app::OpeningKey::Escape).taken);
    CHECK_FALSE(f.ui.pressed());
    CHECK(f.ui.has_pending()); // the selection is still there, Esc reaches it next
}

TEST_CASE(
    "controls: an order replaced by a later one is 'replaced', also once the later one ended") {
    Fixture f;
    f.click(app::PointerButton::Left, 100, 100);
    f.click(app::PointerButton::Right, 200, 150);
    f.ui.submitted(0, app::CommandId{app::CommandOrigin::Window, 1}, 5);
    f.click(app::PointerButton::Right, 300, 150);
    f.ui.submitted(0, app::CommandId{app::CommandOrigin::Window, 2}, 6);
    app::CommandOutcome a;
    a.state = app::CommandOutcome::State::Applied;
    a.id = f.ui.tracked()[0].receipt;
    a.order_id = 7;
    f.table[1] = a;
    a.id = f.ui.tracked()[1].receipt;
    a.order_id = 8;
    f.table[2] = a;
    // The second order arrived: the ship carries no order, and the first has no event of its own.
    f.view.ships[0].order_id.reset();
    app::ShipEventView e;
    e.kind = app::ShipEventKind::Arrived;
    e.ship = sim::FigureId{1};
    e.order_id = 8;
    f.view.events = {e};
    CHECK(f.ui.state_of(f.ui.tracked()[0], f.feed()) == app::TrackState::Superseded);
    CHECK(f.ui.state_of(f.ui.tracked()[1], f.feed()) == app::TrackState::Arrived);
    // With nothing later to explain it, the order's end is simply no longer known.
    Fixture g;
    g.click(app::PointerButton::Left, 100, 100);
    g.click(app::PointerButton::Right, 200, 150);
    g.ui.submitted(0, app::CommandId{app::CommandOrigin::Window, 1}, 5);
    app::CommandOutcome one;
    one.state = app::CommandOutcome::State::Applied;
    one.id = g.ui.tracked()[0].receipt;
    one.order_id = 7;
    g.table[1] = one;
    CHECK(g.ui.state_of(g.ui.tracked()[0], g.feed()) == app::TrackState::Ended);
}

TEST_CASE("controls: an emptied hold is not sent") {
    Fixture f;
    f.view.ships[0].holds = {{42, 0}, {26, 2560}};
    app::CityView city;
    city.ref = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    app::WarehouseView wh;
    wh.id = sim::BuildingId{1};
    wh.tile = sim::TileXY{8, 8};
    wh.w = wh.h = 4;
    city.warehouse = wh;
    city.stock_88.assign(64, 0);
    city.capacity_88 = 50 * 256;
    f.view.cities = {city};
    f.click(app::PointerButton::Left, 100, 100);
    f.key(app::OpeningKey::Unload);
    // The mode opens on the first non-empty hold; the hold then empties under it.
    f.view.ships[0].holds = {{42, 0}, {26, 0}};
    const auto r = f.key(app::OpeningKey::Send);
    CHECK(r.commands.empty());
    CHECK(f.ui.message().find("empty") != std::string::npos);
}
