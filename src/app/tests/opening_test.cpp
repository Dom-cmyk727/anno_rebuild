// The ship-start opening's session pieces (W4-OPENING-1; docs/app/ship-opening.md): the save frame,
// the command file's ship-start forms, the CLI, the quick-save writer, and, with the original game
// files (ANNO1503_GAME_DIR; skipped with a message otherwise), the real loader, the published
// view and the correlation of an order's id and events through the runner.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/cli.hpp>
#include <anno/app/commands_file.hpp>
#include <anno/app/frame_projection.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/opening.hpp>
#include <anno/app/opening_session.hpp>
#include <anno/app/opening_ui.hpp>
#include <anno/app/opening_view.hpp>
#include <anno/app/play.hpp>
#include <anno/app/ship_layer.hpp>
#include <anno/app/zoom.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/route_world.hpp>
#include <anno/sim/ship_step.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>

#include "../../sim/tests/original_component.hpp"

namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;
namespace fs = std::filesystem;

namespace {

app::OpeningSave sample_save() {
    app::OpeningSave s;
    s.identity.manifest_hash = "ab12";
    s.identity.manifest_name = "lan5-ship-a";
    s.identity.seat = 2;
    s.identity.rules_digest = "00000000000000aa";
    s.identity.registry_digest = "00000000000000bb";
    s.turn = 41;
    s.tick = 4100;
    s.state_hash = 0x123456789abcdef0ULL;
    s.world = {1, 2, 3, 0, 255, 10, 10};
    return s;
}

fs::path scratch(const char* name) {
    const fs::path p = fs::temp_directory_path() / (std::string("anno_opening_") + name);
    std::error_code ec;
    fs::remove_all(p, ec);
    fs::create_directories(p);
    return p;
}

fs::path manifest_path() {
    return fs::path(ANNO_REPO_DIR) / "data" / "maps" / "candidates" / "ship-start-v2" /
           "lan5-ship-a.json";
}

} // namespace

TEST_CASE("opening save: the frame round-trips every identity field and the world bytes") {
    const app::OpeningSave s = sample_save();
    const auto bytes = app::format_opening_save(s);
    const auto back = app::parse_opening_save(bytes);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    CHECK(back.value().identity.manifest_hash == "ab12");
    CHECK(back.value().identity.manifest_name == "lan5-ship-a");
    CHECK(back.value().identity.seat == 2);
    CHECK(back.value().identity.rules_digest == "00000000000000aa");
    CHECK(back.value().identity.registry_digest == "00000000000000bb");
    CHECK(back.value().turn == 41);
    CHECK(back.value().tick == 4100);
    CHECK(back.value().state_hash == 0x123456789abcdef0ULL);
    CHECK(back.value().world == s.world);
}

TEST_CASE("opening save: a damaged frame is refused with its reason, never half read") {
    const auto good = app::format_opening_save(sample_save());
    const auto refused = [](std::vector<std::uint8_t> bytes, const char* what) {
        const auto r = app::parse_opening_save(bytes);
        CHECK_MESSAGE(!r.ok(), what);
        return r.ok() ? std::string() : r.error();
    };
    // A control: the undamaged bytes parse (the refusals below are about the damage).
    CHECK(app::parse_opening_save(good).ok());
    auto truncated = good;
    truncated.pop_back();
    CHECK(refused(truncated, "one byte short").find("truncated or extended") != std::string::npos);
    auto extended = good;
    extended.push_back(0);
    CHECK(refused(extended, "one byte long").find("truncated or extended") != std::string::npos);
    CHECK(refused({}, "empty").find("not an opening save") != std::string::npos);
    std::string text(good.begin(), good.end());
    const auto edit = [&](const std::string& from, const std::string& to) {
        std::string t = text;
        const auto at = t.find(from);
        REQUIRE(at != std::string::npos);
        t.replace(at, from.size(), to);
        return std::vector<std::uint8_t>(t.begin(), t.end());
    };
    CHECK(refused(edit("opening-save 1", "opening-save 2"), "another version").find("version") !=
          std::string::npos);
    CHECK(refused(edit("seat 2\n", "seat x\n"), "a seat that is no number").find("not a number") !=
          std::string::npos);
    CHECK(
        refused(edit("tick 4100\n", "tick 4100\ntick 1\n"), "a duplicate line").find("duplicate") !=
        std::string::npos);
    CHECK(refused(edit("tick 4100\n", "tick 4100\nbogus 1\n"), "an unknown line").find("unknown") !=
          std::string::npos);
    CHECK(refused(edit("manifest_hash ab12\n", ""), "a missing identity line").find("lacks") !=
          std::string::npos);
}

TEST_CASE(
    "command file: the ship-start forms round-trip, with their issuer, and old lines still parse") {
    using sim::Command;
    std::vector<app::ScriptedCommand> in;
    sim::FoundCmd found;
    found.ship = sim::FigureId{1};
    found.placement.type = sim::TypeId{1302};
    found.placement.rotation = 1;
    found.placement.x = 254;
    found.placement.y = 137;
    found.click_x = 255;
    found.click_y = 138;
    in.push_back({8, Command{sim::PlayerId{0}, found}});
    in.push_back({9, Command{sim::PlayerId{2}, sim::MoveCmd{sim::FigureId{7}, 79980, 170105}}});
    in.push_back({10, Command{sim::PlayerId{2}, sim::StopCmd{sim::FigureId{7}}}});
    sim::UnloadCmd unload;
    unload.ship = sim::FigureId{1};
    unload.warehouse = sim::BuildingId{1};
    unload.hold = 3;
    unload.goods = 41;
    unload.tons = 30;
    in.push_back({11, Command{sim::PlayerId{0}, unload}});
    sim::BuildCmd build;
    build.city = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    build.placements.push_back(sim::PlacementWire{sim::TypeId{1001}, 251, 137, 0, 0, 0});
    in.push_back({14, Command{sim::PlayerId{0}, build}});
    sim::DemolishCmd demolish;
    demolish.city = build.city;
    demolish.tiles = {{251, 137}, {252, 137}};
    in.push_back({15, Command{sim::PlayerId{0}, demolish}});
    const auto text = app::format_commands(in, true);
    REQUIRE_MESSAGE(text.ok(), (text.ok() ? std::string() : text.error().message));
    CHECK(text.value() == "turn 8 found 0 1 1302 1 254 137 255 138\n"
                          "turn 9 move 2 7 79980 170105\n"
                          "turn 10 stop 2 7\n"
                          "turn 11 unload 0 1 1 3 41 30\n"
                          "turn 14 buildc 0 0 0 1001 251 137 0\n"
                          "turn 15 demolishc 0 0 0 251 137 252 137\n");
    // Parsing needs no world for these lines: the world argument is not consulted.
    const sim::World world;
    const auto back = app::parse_commands(text.value(), world, sim::PlayerId{9});
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    REQUIRE(back.value().size() == in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        CHECK(back.value()[i].turn == in[i].turn);
        CHECK(back.value()[i].command == in[i].command); // the issuer included
    }
    // The old forms are unchanged byte for byte (no ship-start flag).
    const auto old_form = app::format_commands({{3, Command{sim::PlayerId{1}, build}}});
    REQUIRE(old_form.ok());
    CHECK(old_form.value() == "turn 3 build 1001 251 137 0\n");
    // A command with no line form is refused, not dropped.
    const auto speed =
        app::format_commands({{1, Command{sim::PlayerId{0}, sim::SetSpeedCmd{100}}}}, true);
    CHECK_FALSE(speed.ok());
    // Malformed lines name themselves.
    for (const char* bad :
         {"turn 1 found 0 1 1302 1 254 137 255\n", "turn 1 move 0 1 5\n",
          "turn 1 unload 0 1 1 3 41\n", "turn 1 stop x 1\n", "turn 1 buildc 0 0 0 1001 251 137\n",
          "turn 1 found 0 1 1302 4 1 2 3 4\n"}) {
        CAPTURE(bad);
        CHECK_FALSE(app::parse_commands(bad, world, sim::PlayerId{0}).ok());
    }
}

TEST_CASE("cli: --ship-start needs its seat, and the seat and the save mean nothing without it") {
    const auto good = app::parse_args({"--ship-start", "m.json", "--seat", "2"});
    REQUIRE(good.has_value());
    CHECK(good->ship_start);
    CHECK(good->play);
    CHECK(*good->seat == 2);
    const auto resumed = app::parse_args(
        {"--ship-start", "m.json", "--seat", "0", "--resume", "s.sav", "--speed", "200"});
    REQUIRE(resumed.has_value());
    CHECK(resumed->resume.has_value());
    CHECK(resumed->play_speed == 200);
    CHECK_FALSE(app::parse_args({"--ship-start", "m.json"}).has_value());
    CHECK_FALSE(app::parse_args({"--play", "x.sze", "--seat", "1"}).has_value());
    CHECK_FALSE(app::parse_args({"--play", "x.sze", "--resume", "s.sav"}).has_value());
    CHECK_FALSE(app::parse_args({"--ship-start", "m.json", "--seat", "x"}).has_value());
}

TEST_CASE(
    "quick-save writer: a file is replaced whole, a failed write says so and leaves the old file") {
    const fs::path dir = scratch("writer");
    const fs::path target = dir / "q.sav";
    CHECK(app::OpeningSaver::write_file(target, {1, 2, 3}).empty());
    std::ifstream in(target, std::ios::binary);
    std::vector<char> got{std::istreambuf_iterator<char>(in), {}};
    CHECK(got == std::vector<char>{1, 2, 3});
    in.close();
    CHECK(app::OpeningSaver::write_file(target, {9}).empty());
    CHECK_FALSE(fs::exists(fs::path(target.string() + ".tmp"))); // no temporary left behind
    // The target is a directory: the rename fails; the reason is named and the old state stays.
    const fs::path blocked = dir / "blocked.sav";
    fs::create_directories(blocked);
    const std::string error = app::OpeningSaver::write_file(blocked, {1});
    CHECK_FALSE(error.empty());
    CHECK(fs::is_directory(blocked));
    CHECK_FALSE(fs::exists(fs::path(blocked.string() + ".tmp")));
    // A directory that cannot be created (its parent is a file) is named too.
    const std::string nodir = app::OpeningSaver::write_file(target / "sub" / "x.sav", {1});
    CHECK_FALSE(nodir.empty());
}

TEST_CASE("opening, real files: the launch loads the start, every id comes from published values") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = app::load_opening(*dir, manifest_path(), 0);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    app::OpeningLoad& load = loaded.value();
    CHECK(load.identity.seat == 0);
    CHECK(load.identity.manifest_hash.size() == 64);
    CHECK(load.identity.rules_digest.size() == 16);
    CHECK(load.identity.start_hash.size() == 16);
    CHECK(load.setup.local.v == 0);
    REQUIRE(load.setup.founding_types.size() == 1);
    CHECK(load.setup.founding_types[0].v == 1302);
    CHECK_FALSE(load.setup.islands.empty());
    CHECK(load.play.world.players().find(sim::PlayerId{0}) != nullptr);
    // The camera opens on the participant's ship: seat 0 starts at tile (312.5, 680.5).
    CHECK(load.camera_x == doctest::Approx(312.5));
    CHECK(load.camera_y == doctest::Approx(680.5));
    // Bad input is a readable error, nothing started.
    const auto bad_seat = app::load_opening(*dir, manifest_path(), 9);
    REQUIRE_FALSE(bad_seat.ok());
    CHECK(bad_seat.error().find("not a seat") != std::string::npos);
    const auto missing = app::load_opening(*dir, manifest_path().parent_path() / "none.json", 0);
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.error().find("cannot read") != std::string::npos);
    const fs::path bad = scratch("badmanifest") / "m.json";
    std::ofstream(bad) << "{\"manifest\": 1}";
    const auto invalid = app::load_opening(*dir, bad, 0);
    REQUIRE_FALSE(invalid.ok());
    CHECK(invalid.error().find("invalid manifest") != std::string::npos);

    // The view: five ships and no city, the participant's gold, in FigureId order.
    app::ShipEventLog events;
    const app::OpeningView view =
        app::collect_opening_view(load.play.world, sim::PlayerId{0}, events, 0);
    REQUIRE(view.ships.size() == 5);
    CHECK(view.ships[0].id.v == 1);
    CHECK(view.ships[4].id.v == 13);
    CHECK(view.ships[0].owner.v == 0);
    CHECK(view.ships[0].holds.size() == 4);
    CHECK(view.cities.empty());
    CHECK(view.gold == 30000);
    CHECK(view.ship(sim::FigureId{7}) != nullptr);
    CHECK(view.ship(sim::FigureId{2}) == nullptr); // an embarked scout is not a ship
    CHECK_FALSE(view.ships[0].order_id.has_value());

    // The save of the loaded world resumes to the same state, and every identity mismatch is named.
    app::OpeningSave save;
    save.identity = load.identity;
    save.state_hash = load.play.world.state_hash();
    save.world = load.play.world.save();
    const fs::path sdir = scratch("resume");
    const fs::path file = sdir / "s.sav";
    REQUIRE(app::OpeningSaver::write_file(file, app::format_opening_save(save)).empty());
    auto resumed = app::load_opening(*dir, manifest_path(), 0, file);
    REQUIRE_MESSAGE(resumed.ok(), (resumed.ok() ? std::string() : resumed.error()));
    CHECK(resumed.value().identity.resumed_hash == load.identity.start_hash);
    CHECK(resumed.value().play.world.state_hash() == load.play.world.state_hash());
    const auto other_seat = app::load_opening(*dir, manifest_path(), 1, file);
    REQUIRE_FALSE(other_seat.ok());
    CHECK(other_seat.error().find("belongs to seat 0") != std::string::npos);
    app::OpeningSave wrong = save;
    wrong.identity.manifest_hash = std::string(64, 'f');
    REQUIRE(app::OpeningSaver::write_file(file, app::format_opening_save(wrong)).empty());
    const auto other_manifest = app::load_opening(*dir, manifest_path(), 0, file);
    REQUIRE_FALSE(other_manifest.ok());
    CHECK(other_manifest.error().find("another manifest") != std::string::npos);
    wrong = save;
    wrong.identity.rules_digest = "0000000000000001";
    REQUIRE(app::OpeningSaver::write_file(file, app::format_opening_save(wrong)).empty());
    CHECK(app::load_opening(*dir, manifest_path(), 0, file).error().find("other rules") !=
          std::string::npos);
    wrong = save;
    wrong.state_hash ^= 1;
    REQUIRE(app::OpeningSaver::write_file(file, app::format_opening_save(wrong)).empty());
    CHECK(app::load_opening(*dir, manifest_path(), 0, file).error().find("hash") !=
          std::string::npos);
    wrong = save;
    wrong.world.resize(wrong.world.size() / 2);
    REQUIRE(app::OpeningSaver::write_file(file, app::format_opening_save(wrong)).empty());
    CHECK_FALSE(app::load_opening(*dir, manifest_path(), 0, file).ok());
}

TEST_CASE("opening, real files: an order's id comes from the sim's own result, events end exactly "
          "that order") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = app::load_opening(*dir, manifest_path(), 0);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    app::PlayRunner runner(std::move(loaded.value().play.world), 100);
    runner.set_local_player(sim::PlayerId{0});
    runner.set_opening(true);
    const std::uint64_t before = runner.next_order_id();
    // Two orders for the same ship in one turn, then a refused one (another participant's ship).
    const auto q8 = [](double tiles) { return static_cast<std::int32_t>(tiles * 256); };
    const auto first = runner.post_command(
        sim::Command{sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{1}, q8(312.5), q8(668.5)}});
    const auto second = runner.post_command(
        sim::Command{sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{1}, q8(312.5), q8(664.5)}});
    const auto foreign = runner.post_command(
        sim::Command{sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{4}, q8(652.5), q8(670.5)}});
    runner.run_n(1);
    const auto o1 = runner.outcome(first), o2 = runner.outcome(second),
               o3 = runner.outcome(foreign);
    REQUIRE(o1.state == app::CommandOutcome::State::Applied);
    REQUIRE(o2.state == app::CommandOutcome::State::Applied);
    REQUIRE(o3.state == app::CommandOutcome::State::Refused);
    CHECK(o3.status == sim::Reject::NotShipOwner);
    REQUIRE(o1.order_id.has_value());
    REQUIRE(o2.order_id.has_value());
    CHECK(*o1.order_id == before);        // the sim's counter when the first was applied
    CHECK(*o2.order_id == before + 1);    // the second replaced it
    CHECK_FALSE(o3.order_id.has_value()); // a refused command made no order
    CHECK(runner.next_order_id() == before + 2);
    auto snap = runner.snapshot();
    REQUIRE(snap->opening != nullptr);
    const app::ShipView* ship = snap->opening->ship(sim::FigureId{1});
    REQUIRE(ship != nullptr);
    CHECK(ship->order_id == o2.order_id); // the ship carries the second order only
    // Sail until the ship arrives: exactly one Arrived event, for the second order's id.
    for (int i = 0; i < 4000 && snap->opening->events.empty(); ++i) {
        runner.run_n(1);
        snap = runner.snapshot();
    }
    REQUIRE_FALSE(snap->opening->events.empty());
    const app::ShipEventView& e = snap->opening->events.front();
    CHECK(e.kind == app::ShipEventKind::Arrived);
    CHECK(e.ship.v == 1);
    CHECK(e.order_id == *o2.order_id);
    CHECK(snap->opening->event_for(sim::FigureId{1}, *o1.order_id) == nullptr);
    // The ship stands where the second order sent it (the sim moved it; nothing was relocated).
    ship = snap->opening->ship(sim::FigureId{1});
    REQUIRE(ship != nullptr);
    CHECK(ship->pos.x == q8(312.5));
    CHECK(ship->pos.y == q8(664.5));
    CHECK_FALSE(ship->order_id.has_value());
    // A stop of an idle ship is accepted and ends no order: no Stopped event is invented.
    const std::size_t events = snap->opening->events.size();
    const auto stop =
        runner.post_command(sim::Command{sim::PlayerId{0}, sim::StopCmd{sim::FigureId{1}}});
    runner.run_n(1);
    CHECK(runner.outcome(stop).state == app::CommandOutcome::State::Applied);
    CHECK(runner.snapshot()->opening->events.size() == events);
    // A new order and a stop while it sails: the order ends with its own Stopped event.
    const auto third = runner.post_command(
        sim::Command{sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{1}, q8(312.5), q8(650.5)}});
    runner.run_n(3);
    const auto o4 = runner.outcome(third);
    REQUIRE(o4.order_id.has_value());
    static_cast<void>(
        runner.post_command(sim::Command{sim::PlayerId{0}, sim::StopCmd{sim::FigureId{1}}}));
    runner.run_n(1);
    const auto after = runner.snapshot();
    const app::ShipEventView* stopped = after->opening->event_for(sim::FigureId{1}, *o4.order_id);
    REQUIRE(stopped != nullptr);
    CHECK(stopped->kind == app::ShipEventKind::Stopped);
}

TEST_CASE(
    "live ships on the CPU renderer: blend_units draws what the scene's own units draw, in painter "
    "order (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded =
        app::load_scene(*dir, *dir / "Scenes" / "0200Single" / "Single_01.sze", {}, nullptr);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    anno::render::Scene& scene = *loaded.value().scene;
    std::vector<anno::render::UnitSprite> live;
    for (const auto& u : scene.units) {
        if (!u.z && live.empty()) {
            live.push_back(u);
        }
    }
    REQUIRE(live.size() == 1);
    live.push_back(live[0]);
    live[1].x += 0.5F;
    live[1].y += 0.4F; // in front of the first and overlapping it
    constexpr int kW = 640, kH = 400;
    for (const std::size_t group : {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
        const auto [ax, ay] = live[0].anchor(group, scene.lift);
        const anno::render::Rect view{ax - kW / 2, ay - kH / 2, kW, kH};
        // The reference: the ships in the scene, baked into the chunks. Then the bare view.
        scene.units = live;
        anno::render::FrameRenderer cpu(4);
        std::vector<std::uint32_t> baked(std::size_t{kW} * kH), bare(baked.size());
        cpu.render(scene, view, group, 300, anno::render::WaterStyle::Original, baked.data());
        scene.units.clear();
        anno::render::FrameRenderer none(4);
        none.render(scene, view, group, 300, anno::render::WaterStyle::Original, bare.data());
        std::size_t ship_pixels = 0;
        for (std::size_t i = 0; i < baked.size(); ++i) {
            ship_pixels += baked[i] != bare[i] ? 1U : 0U;
        }
        INFO("group " << group);
        REQUIRE(ship_pixels > 200); // the compared region holds the ships
        const auto max_diff = [&](const std::vector<std::uint32_t>& a,
                                  const std::vector<std::uint32_t>& b) {
            int m = 0;
            for (std::size_t i = 0; i < a.size(); ++i) {
                for (int k = 0; k < 3; ++k) {
                    m = std::max(m, std::abs(static_cast<int>((a[i] >> (8 * k)) & 0xFF) -
                                             static_cast<int>((b[i] >> (8 * k)) & 0xFF)));
                }
            }
            return m;
        };
        // The overlay over the bare frame: the same picture (a level of rounding in the blend).
        auto overlay = bare;
        app::blend_units(live, scene, group, view, overlay.data());
        CHECK(max_diff(overlay, baked) <= 2);
        // The list's order does not matter: the painter's order (x + y) decides.
        auto reversed = bare;
        const std::vector<anno::render::UnitSprite> flipped{live[1], live[0]};
        app::blend_units(flipped, scene, group, view, reversed.data());
        CHECK(reversed == overlay);
        // A wrong list is told apart: the ships a tile away give another picture.
        auto shifted = live;
        for (auto& u : shifted) {
            u.x += 1.0F;
            u.y += 1.0F;
        }
        auto wrong = bare;
        app::blend_units(shifted, scene, group, view, wrong.data());
        CHECK(max_diff(wrong, baked) > 8);
    }
}

namespace {

/// A linear map of 10 pixels a tile that calls every pixel open water: an order to land is then
/// sent (the sim's route planner is what fails it), as a window that misjudged the ground would.
class SeaProjection final : public app::ScreenProjection {
  public:
    std::optional<app::PickedPoint> pick(double px, double py) const override {
        app::PickedPoint p;
        p.x_q8 = static_cast<std::int32_t>(px / 10.0 * 256.0);
        p.y_q8 = static_cast<std::int32_t>(py / 10.0 * 256.0);
        p.tile =
            sim::TileXY{static_cast<std::int32_t>(px / 10.0), static_cast<std::int32_t>(py / 10.0)};
        return p;
    }
    std::optional<app::ScreenBox> ship_box(const app::ShipView& s) const override {
        app::ScreenBox b;
        b.cx = s.pos.x / 256.0 * 10.0;
        b.cy = s.pos.y / 256.0 * 10.0;
        b.x0 = b.cx - 8;
        b.y0 = b.cy - 8;
        b.x1 = b.cx + 8;
        b.y1 = b.cy + 8;
        return b;
    }
};

} // namespace

TEST_CASE("opening, real files: a route the sim cannot plan ends as its own 'route failed', the "
          "ship stays") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = app::load_opening(*dir, manifest_path(), 0);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    app::OpeningLoad& load = loaded.value();
    // A land tile with no water within eight tiles (the sim's goal radius is six): the order is
    // accepted (the target is inside the world) and its plan fails.
    const sim::WorldTileSource src(load.play.world, sim::ship_route_terrain_options());
    std::optional<std::pair<int, int>> inland;
    for (const auto& island : load.play.world.islands()) {
        if (!island || inland.has_value()) {
            continue;
        }
        const int x0 = island->tiles.x0(), y0 = island->tiles.y0();
        const int w = static_cast<int>(island->tiles.width()),
                  h = static_cast<int>(island->tiles.height());
        for (int y = y0 + 9; y < y0 + h - 9 && !inland; y += 2) {
            for (int x = x0 + 9; x < x0 + w - 9 && !inland; x += 2) {
                bool dry = true;
                for (int dy = -8; dy <= 8 && dry; ++dy) {
                    for (int dx = -8; dx <= 8 && dry; ++dx) {
                        dry = !src.tile_is_water(x + dx, y + dy);
                    }
                }
                if (dry) {
                    inland = std::pair{x, y};
                }
            }
        }
    }
    REQUIRE(inland.has_value());
    app::OpeningUi ui(load.setup);
    app::PlayRunner runner(std::move(load.play.world), 100);
    runner.set_local_player(sim::PlayerId{0});
    runner.set_opening(true);
    runner.run_n(1);
    SeaProjection proj;
    const auto feed_of = [&runner](std::shared_ptr<const app::PlaySnapshot> snap) {
        app::UiFeed feed;
        feed.view = snap->opening.get();
        feed.outcome = [&runner, snap](app::CommandId id) { return runner.outcome_at(id, *snap); };
        return feed;
    };
    auto snap = runner.snapshot();
    const app::ShipView* ship = snap->opening->ship(sim::FigureId{1});
    REQUIRE(ship != nullptr);
    const sim::FixedPos start = ship->pos;
    // Select the ship with a click on its box, then order a click on the inland tile.
    ui.pointer_down(app::PointerButton::Left, start.x / 256.0 * 10.0, start.y / 256.0 * 10.0);
    static_cast<void>(ui.pointer_up(app::PointerButton::Left, start.x / 256.0 * 10.0,
                                    start.y / 256.0 * 10.0, &proj, feed_of(snap)));
    REQUIRE(ui.selected().has_value());
    const double px = inland->first * 10.0 + 5, py = inland->second * 10.0 + 5;
    ui.pointer_down(app::PointerButton::Right, px, py);
    const app::UiResult sent =
        ui.pointer_up(app::PointerButton::Right, px, py, &proj, feed_of(snap));
    REQUIRE(sent.commands.size() == 1);
    ui.submitted(0, runner.post_command(sent.commands[0]), snap->opening->tick);
    bool failed = false;
    for (int i = 0; i < 400 && !failed; ++i) {
        runner.run_n(1);
        snap = runner.snapshot();
        failed = !snap->opening->events.empty();
    }
    REQUIRE(failed);
    const app::UiFeed feed = feed_of(snap);
    REQUIRE(ui.tracked().size() == 1);
    CHECK(ui.state_of(ui.tracked()[0], feed) == app::TrackState::RouteFailed);
    CHECK(snap->opening->events.front().kind == app::ShipEventKind::RouteFailed);
    CHECK(snap->opening->events.front().reason ==
          static_cast<std::uint32_t>(sim::NavFailReason::Unreachable));
    const std::string line = ui.status_line(feed);
    CHECK(line.find("route failed (unreachable)") != std::string::npos);
    // Acceptance was not arrival and the ship never left its tile.
    ship = snap->opening->ship(sim::FigureId{1});
    REQUIRE(ship != nullptr);
    CHECK(ship->pos.x / 256 == start.x / 256);
    CHECK(ship->pos.y / 256 == start.y / 256);
    CHECK_FALSE(ship->order_id.has_value());
}

TEST_CASE("opening, real files: the runner keeps only its newest view, a held one is released when "
          "dropped") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = app::load_opening(*dir, manifest_path(), 0);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    app::PlayRunner runner(std::move(loaded.value().play.world), 100);
    runner.set_local_player(sim::PlayerId{0});
    runner.set_opening(true);
    runner.run_n(3);
    // A window that holds a frame's snapshot while the sim goes on: the held view stays whole and
    // unchanged (it is a value), and the runner has not kept a reference of its own to older ones.
    std::shared_ptr<const app::PlaySnapshot> held = runner.snapshot();
    std::weak_ptr<const app::OpeningView> weak = held->opening;
    const std::uint64_t held_tick = held->opening->tick;
    const std::int32_t held_x = held->opening->ships[0].pos.x,
                       held_y = held->opening->ships[0].pos.y;
    const std::size_t held_holds = held->opening->ships[0].holds.size();
    runner.post_command(
        sim::Command{sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{1}, 80000, 170000}});
    runner.run_n(60);
    CHECK(runner.snapshot()->opening->tick > held_tick);
    // What the held frame draws did not change under it.
    CHECK(held->opening->tick == held_tick);
    CHECK(held->opening->ships[0].pos.x == held_x);
    CHECK(held->opening->ships[0].pos.y == held_y);
    CHECK(held->opening->ships[0].holds.size() == held_holds);
    CHECK(runner.snapshot()->opening->ships[0].pos.y != held_y); // the sim did sail on
    CHECK_FALSE(weak.expired());
    held.reset();
    CHECK(weak.expired()); // nothing else retained the old view
}

TEST_CASE("opening save: every header line is required and numbers are digits only") {
    const auto good = app::format_opening_save(sample_save());
    const std::string text(good.begin(), good.end());
    const auto edit = [&](const std::string& from, const std::string& to) {
        std::string t = text;
        const auto at = t.find(from);
        REQUIRE(at != std::string::npos);
        t.replace(at, from.size(), to);
        return std::vector<std::uint8_t>(t.begin(), t.end());
    };
    // A control: the unedited file reads.
    CHECK(app::parse_opening_save(good).ok());
    for (const char* line :
         {"seat 2\n", "turn 41\n", "tick 4100\n", "manifest_name lan5-ship-a\n"}) {
        CAPTURE(line);
        const auto r = app::parse_opening_save(edit(line, ""));
        REQUIRE_FALSE(r.ok());
        CHECK(r.error().find("required line") != std::string::npos);
    }
    for (const char* bad : {"seat -1\n", "seat +2\n", "seat  2\n", "seat 4294967296\n", "seat 2x\n",
                            "seat \n", "seat 99999999999999999999999\n"}) {
        CAPTURE(bad);
        const auto r = app::parse_opening_save(edit("seat 2\n", bad));
        REQUIRE_FALSE(r.ok());
        CHECK(r.error().find("not a number") != std::string::npos);
    }
    // A manifest name with a newline would end the header early: the writer keeps it on one line.
    app::OpeningSave s = sample_save();
    s.identity.manifest_name = "a\n\nb";
    const auto round = app::parse_opening_save(app::format_opening_save(s));
    REQUIRE_MESSAGE(round.ok(), (round.ok() ? std::string() : round.error()));
    CHECK(round.value().identity.manifest_name == "a  b");
    CHECK(round.value().world == s.world);
}

TEST_CASE("cli: --ship-start followed by --view is neither") {
    CHECK_FALSE(
        app::parse_args({"--ship-start", "m.json", "--seat", "0", "--view", "x.sze"}).has_value());
}

TEST_CASE("session identity: the lines read back as written, also with CRLF line ends, and name "
          "what is read") {
    app::OpeningIdentity id;
    id.manifest_path = "data/maps/m.json";
    id.manifest_hash = "abc123";
    id.seat = 3;
    id.rules_digest = "00ff00ff00ff00ff";
    id.registry_digest = "1111222233334444";
    id.start_hash = "deadbeefdeadbeef";
    const std::string text = app::format_opening_identity(id);
    const auto lf = app::parse_opening_identity(text);
    REQUIRE(lf.has_value());
    CHECK(lf->manifest_path == "data/maps/m.json");
    CHECK(lf->manifest_hash == "abc123");
    CHECK(lf->seat == 3);
    CHECK(lf->rules_digest == "00ff00ff00ff00ff");
    CHECK(lf->start_hash == "deadbeefdeadbeef");
    std::string crlf;
    for (const char c : text) {
        if (c == '\n') {
            crlf += '\r';
        }
        crlf += c;
    }
    const auto cr = app::parse_opening_identity(crlf);
    REQUIRE(cr.has_value());
    CHECK(cr->manifest_hash == lf->manifest_hash);
    CHECK(cr->rules_digest == lf->rules_digest);
    CHECK(cr->start_hash == lf->start_hash);
    // A session that is not a ship-start one names no identity.
    CHECK_FALSE(app::parse_opening_identity("map x.txt\nspeed 200\n").has_value());
}

// The founding preview's heights at an island's far column and row are the sim's boundary height
// (sim::VertexGrid::at reads kBoundaryVertexHeight there; review 2026-11-03: the window had none,
// so a warehouse whose water edge is the island's far edge stayed undecided in the preview). The
// island here has no HEIGHT grid: its inner vertices have no height, its far edge the boundary's,
// and a vertex off every island none.
TEST_CASE("founding preview heights: the island's far column and row read the sim's boundary") {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    bytes->push_back(std::vector<std::uint8_t>(64, 0));
    anno::formats::insel::Island island;
    island.head = (*bytes)[0];
    island.number = 0;
    island.x = 100;
    island.y = 40;
    island.width = 30;
    island.height = 20;
    const auto scene =
        render::Scene::create("heights", {island}, anno::assets::island::GroundTextures{},
                              anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
    CHECK_FALSE(app::scene_vertex_height(*scene, 110, 50).has_value()); // inside, no grid
    CHECK(app::scene_vertex_height(*scene, 130, 50) == sim::kBoundaryVertexHeight); // x0 + W
    CHECK(app::scene_vertex_height(*scene, 110, 60) == sim::kBoundaryVertexHeight); // y0 + H
    CHECK(app::scene_vertex_height(*scene, 130, 60) == sim::kBoundaryVertexHeight);
    CHECK_FALSE(app::scene_vertex_height(*scene, 131, 50).has_value()); // off the island
    CHECK_FALSE(app::scene_vertex_height(*scene, 99, 50).has_value());
    CHECK_FALSE(app::scene_vertex_height(*scene, 110, 61).has_value());
}

// Scene::shore_height reaches the wave depth (open sea away from every island is the shore's
// depth) and a new generation keeps it (the scene's own setting, set by the viewer's --shore).
TEST_CASE("the scene's shore: the wave depth of open sea and the next generation") {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    bytes->push_back(std::vector<std::uint8_t>(64, 0));
    anno::formats::insel::Island island;
    island.head = (*bytes)[0];
    island.number = 0;
    island.x = 100;
    island.y = 40;
    island.width = 30;
    island.height = 20;
    const auto scene =
        render::Scene::create("shore", {island}, anno::assets::island::GroundTextures{},
                              anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
    CHECK(scene->shore_height == render::kShoreHeight);
    CHECK(scene->fluid_depth(10.0F, 10.0F) == render::kShoreHeight);
    scene->shore_height = 16.0F;
    CHECK(scene->fluid_depth(10.0F, 10.0F) == 16.0F);
    const auto next = scene->with_islands({});
    CHECK(next->shore_height == 16.0F);
}

// Owner playtest 2026-11-03: "I could not place the Kontor at all". What decides it, island by
// island of the real start: the sim's coast rule on its own vertex grid, the same rule on the
// heights the window's preview reads (scene_vertex_height on the picture's scene, as the viewer's
// feed), and the sim's other checks of a rotation trial (bounds, ownership, occupancy). Review
// 2026-11-03: the preview's side clamped at the island's far edge, where the window had no height
// (now the sim's boundary height); both verdicts must now agree at every origin and rotation.
TEST_CASE(
    "founding sites, real files: the preview's coast verdict against the sim's rotation trial") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = app::load_opening(*dir, manifest_path(), 0);
    REQUIRE(loaded.ok());
    const sim::World& world = loaded.value().play.world;
    REQUIRE(!loaded.value().setup.founding_types.empty());
    const sim::BuildingType* type = world.types().find(loaded.value().setup.founding_types.front());
    REQUIRE(type != nullptr);
    auto picture =
        app::load_placed_scene(*dir, loaded.value().play.name, loaded.value().play.placed, nullptr);
    REQUIRE(picture.ok());
    const render::Scene& scene = *picture.value().scene;
    std::size_t sites = 0, coast_sim = 0, coast_preview = 0, verdict_differs = 0, occupied = 0,
                full = 0;
    for (const auto& isl : world.islands()) {
        if (!isl || !isl->terrain) {
            continue;
        }
        const sim::VertexGrid grid = sim::VertexGrid::of(*isl);
        const auto& tiles = isl->tiles;
        const std::int64_t x0 = tiles.x0(), y0 = tiles.y0();
        const std::int64_t w = tiles.width(), h = tiles.height();
        for (std::int64_t y = y0; y < y0 + h; ++y) {
            for (std::int64_t x = x0; x < x0 + w; ++x) {
                const sim::site::Origin o{x, y};
                bool any_sim = false, any_prev = false, any_full = false, coast_but_occ = false;
                for (std::uint8_t rot = 0; rot < 4; ++rot) {
                    const auto [fw, fh] = sim::footprint(*type, rot);
                    if (!sim::site::footprint_inside(tiles, o, fw, fh)) {
                        continue;
                    }
                    const auto s = sim::site::coast_height_check(grid, *type, rot, o);
                    const auto p = sim::site::coast_height_rule(
                        type->width, type->height, rot, [&](std::uint64_t dx, std::uint64_t dy) {
                            return app::scene_vertex_height(scene,
                                                            x + static_cast<std::int64_t>(dx),
                                                            y + static_cast<std::int64_t>(dy));
                        });
                    const bool sa = s.result == sim::site::TerrainResult::Accepted;
                    const bool pa = p.result == sim::site::TerrainResult::Accepted;
                    any_sim = any_sim || sa;
                    any_prev = any_prev || pa;
                    verdict_differs += sa != pa ? 1U : 0U;
                    const bool neutral =
                        sim::site::footprint_ownership(tiles, world.config().neutral_owner, o, fw,
                                                       fh) == sim::site::Ownership::AllNeutral;
                    const bool fits =
                        sim::site::footprint_occupancy(tiles, world.types(), *type, o, fw, fh) ==
                        sim::site::Compat::Compatible;
                    coast_but_occ = coast_but_occ || (sa && !fits);
                    any_full = any_full || (sa && fits && neutral);
                }
                ++sites;
                coast_sim += any_sim ? 1U : 0U;
                coast_preview += any_prev ? 1U : 0U;
                occupied += (coast_but_occ && !any_full) ? 1U : 0U;
                full += any_full ? 1U : 0U;
            }
        }
    }
    MESSAGE("origins " << sites << ": coast rule passes (sim) " << coast_sim
                       << ", (preview heights) " << coast_preview
                       << "; per-rotation verdicts that differ " << verdict_differs
                       << "; coast ok but occupied in every passing rotation " << occupied
                       << "; a rotation passes every check " << full);
    CHECK(full > 0);
    CHECK(coast_sim > 0);
    CHECK(verdict_differs == 0);
    CHECK(coast_preview == coast_sim);
}

// The founding preview asks the sim thread what Founding::apply would answer (probe_founding). Its
// answer must be apply's: the same reason, and on acceptance the rotation the warehouse is built
// at. Checked on the real start with the ship at the coast of the first island, for every click of
// the ring of tiles within its reach.
TEST_CASE("founding probe, real files: the preview's answer is what the sim does with the click") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = app::load_opening(*dir, manifest_path(), 0);
    REQUIRE(loaded.ok());
    sim::World world = std::move(loaded.value().play.world);
    const sim::TypeId type = loaded.value().setup.founding_types.front();
    const sim::BuildingType* bt = world.types().find(type);
    REQUIRE(bt != nullptr);
    // Sail ship 1 to the coast the session founds at (opening_sail.script).
    const sim::Timing timing;
    sim::Turn turn;
    turn.step_ms = 10;
    turn.commands.push_back(sim::Command{
        sim::PlayerId{0}, sim::MoveCmd{sim::FigureId{1}, 257 * 256 + 128, 134 * 256 + 128}});
    sim::run_turn(world, nullptr, turn, timing.mp_steps_per_turn);
    for (int k = 0; k < 3000; ++k) {
        const sim::Figure* ship = sim::find_figure(world.figures(), sim::FigureId{1});
        REQUIRE(ship != nullptr);
        if (ship->pos.x / 256 == 257 && ship->pos.y / 256 == 134) {
            break;
        }
        sim::Turn idle;
        idle.index = static_cast<std::uint64_t>(k + 1);
        idle.step_ms = 10;
        sim::run_turn(world, nullptr, idle, timing.mp_steps_per_turn);
    }
    const sim::Figure* ship = sim::find_figure(world.figures(), sim::FigureId{1});
    REQUIRE(ship != nullptr);
    REQUIRE(ship->pos.x / 256 == 257);
    std::size_t clicks = 0, agree = 0, accepted = 0, vetoed = 0;
    std::map<std::string, std::size_t> reasons;
    for (std::int32_t dy = -13; dy <= 13; ++dy) {
        for (std::int32_t dx = -13; dx <= 13; ++dx) {
            for (std::uint8_t rot = 0; rot < 4; rot += 3) {
                sim::FoundCmd f;
                f.ship = sim::FigureId{1};
                f.placement.type = type;
                f.placement.rotation = rot;
                f.click_x = 257 + dx;
                f.click_y = 134 + dy;
                const auto [w, h] = sim::footprint(*bt, rot);
                f.placement.x =
                    std::max<std::int32_t>(0, f.click_x - (static_cast<std::int32_t>(w) - 1) / 2);
                f.placement.y =
                    std::max<std::int32_t>(0, f.click_y - (static_cast<std::int32_t>(h) - 1) / 2);
                const app::FoundingProbeView probe =
                    app::probe_founding(world, sim::PlayerId{0}, f);
                sim::World copy = world;
                const sim::ApplyResult r = copy.apply(sim::Command{sim::PlayerId{0}, f});
                ++clicks;
                bool same = probe.reason == r.status;
                if (same && r.ok()) {
                    ++accepted;
                    // The warehouse stands at the probe's rotation.
                    std::optional<std::uint8_t> built;
                    for (const sim::RecordResult& rr : r.records) {
                        if (const sim::Building* b = copy.find_building(rr.building)) {
                            built = b->rotation;
                        }
                    }
                    same = built.has_value() && probe.rotation == built;
                }
                // The figure veto after the trials is not part of the probe (its doc): a ship lying
                // on the site is the sim's to refuse.
                const bool veto_only = !same && probe.reason == sim::Reject::None &&
                                       r.status == sim::Reject::FigureInTheWay;
                vetoed += veto_only ? 1U : 0U;
                same = same || veto_only;
                agree += same ? 1U : 0U;
                ++reasons[std::string(sim::reject_name(r.status))];
                if (!same) {
                    MESSAGE("click (" << f.click_x << "," << f.click_y << ") rot " << int(rot)
                                      << ": probe " << sim::reject_name(probe.reason) << ", sim "
                                      << sim::reject_name(r.status));
                }
            }
        }
    }
    std::string summary;
    for (const auto& [k, v] : reasons) {
        summary += k + " " + std::to_string(v) + "; ";
    }
    MESSAGE(clicks << " clicks: " << summary
                   << "the probe said yes and the figure veto no: " << vetoed);
    CHECK(accepted > 0);
    CHECK(agree == clicks);
    CHECK(vetoed <= 4);
}

// Owner playtest 2026-11-03, round 6: the viewer kept derived levels of at most 96 chunks built,
// assuming 91 at level 4; the map's level 4 has 98, so every zoom through it rebuilt it. The
// levels as the window sets them up (main.cpp: derived down to where the map fits 640 x 360).
TEST_CASE("the opening map's far levels down to level 4 stay built (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = app::load_opening(*dir, manifest_path(), 0);
    REQUIRE(loaded.ok());
    app::OpeningLoad& load = loaded.value();
    auto picture = app::load_placed_scene(*dir, load.play.name, load.play.placed, nullptr);
    REQUIRE(picture.ok());
    render::Scene& scene = *picture.value().scene;
    const auto b2 = scene.bounds(2);
    REQUIRE(b2.has_value());
    scene.set_levels(render::ZoomLevels::with_derived(
        render::ZoomLevels::derived_to_fit(b2->w, b2->h, 640.0, 360.0, 8)));
    const render::ZoomLevels& levels = scene.levels();
    REQUIRE(levels.size() > 5);
    std::vector<std::size_t> chunks;
    for (std::size_t g = 0; g < levels.size(); ++g) {
        const auto b = scene.bounds(g);
        REQUIRE(b.has_value());
        chunks.push_back(render::visible_chunks(*b).size());
    }
    MESSAGE("chunks per level: level 3 " << chunks[3] << ", level 4 " << chunks[4] << ", level 5 "
                                         << chunks[5]);
    for (std::size_t g = 4; g < levels.size(); ++g) {
        CHECK_MESSAGE(chunks[g] <= app::kOpeningResidentLevelChunks, "level " << g);
    }
    // Level 3 stays out (the memory measured for it bought nothing, zoom.hpp).
    CHECK(chunks[3] > app::kOpeningResidentLevelChunks);
}

// The original's own Kontors (owner playtest 2026-11-03: the founded warehouse looked turned and
// placed wrong). Every warehouse of the founding type that the original's scenario files place
// stands where the sim's coast rule accepts it, at its stored rotation and origin: the rule's
// rotation (the shore edge per rotation) is the files' rotation. Measured 2026-11-03: 62
// warehouses, each accepted at its stored rotation (a few also at a second one, at corners). What
// looked wrong was the picture: the renderer's projection was the mirror of the original's
// (docs/testing/ship-opening-playtest.md; fixed 2026-11-03, docs/render/iso.md "Handedness").
TEST_CASE(
    "founding sites, real files: the original's own warehouses pass the coast rule as stored") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // Play mode's strict loader (load_play) refuses most of the original scenario
    // files (the strict actor import and whole-script compile it requires are not
    // what every original file satisfies). This test is about the files' own
    // warehouse placements, not about play loading, so it reads scenes through the
    // sim tests' explicit, test-only actor-free path (original_component.hpp)
    // instead: actor records are kept as unprojected warnings rather than refused,
    // and an unsupported whole script is set aside only once the normal creation
    // path has actually demonstrated its refusal (original_component::import_only).
    const auto registry =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE(registry.types);
    const anno::sim_load::IslandTemplates templates = anno::sim_load::scan_island_templates(*dir);
    const sim::SimConfig cfg;
    std::size_t refused = 0;
    std::map<std::string, std::size_t> tally;
    std::size_t seen = 0, as_stored = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(*dir / "Scenes")) {
        if (entry.path().extension() != ".sze" || seen > 400) {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        if (!in) {
            continue;
        }
        const std::vector<std::uint8_t> buf{std::istreambuf_iterator<char>(in), {}};
        auto resolved = anno::sim_load::load_scenario_with_context(
            buf, entry.path(), *dir, templates, *registry.types, cfg, sim::Rules{},
            original_component::actor_free_units());
        if (!resolved) {
            ++refused;
            MESSAGE(entry.path().filename().string() << ": refused: " << resolved.error());
            continue;
        }
        auto prepared = original_component::import_only(resolved.value().loaded.spec,
                                                         registry.types, cfg);
        if (!prepared) {
            ++refused;
            MESSAGE(entry.path().filename().string() << ": refused: " << prepared.error());
            continue;
        }
        auto created = sim::World::create(prepared.value().spec, registry.types, cfg);
        if (!created) {
            ++refused;
            MESSAGE(entry.path().filename().string() << ": refused: " << created.error());
            continue;
        }
        const sim::World& world = created.value();
        for (const auto& isl : world.islands()) {
            if (!isl || !isl->terrain) {
                continue;
            }
            const sim::VertexGrid grid = sim::VertexGrid::of(*isl);
            for (const auto& city : isl->cities) {
                if (!city) {
                    continue;
                }
                for (const sim::BuildingList& list : city->index.lists) {
                    for (const auto& b : list.slots) {
                        if (!b || b->type.v != 1302) {
                            continue;
                        }
                        const sim::BuildingType* t = world.types().find(b->type);
                        REQUIRE(t != nullptr);
                        ++seen;
                        std::string key = "stored rot " + std::to_string(b->rotation) + ": ";
                        for (std::uint8_t r = 0; r < 4; ++r) {
                            const auto v = sim::site::coast_height_check(
                                grid, *t, r, sim::site::Origin{b->x, b->y});
                            key += v.result == sim::site::TerrainResult::Accepted ? "A" : "-";
                        }
                        ++tally[key];
                        const bool ok = sim::site::coast_height_check(grid, *t, b->rotation,
                                                                      sim::site::Origin{b->x, b->y})
                                            .result == sim::site::TerrainResult::Accepted;
                        as_stored += ok ? 1U : 0U;
                        if (!ok) {
                            MESSAGE(entry.path().filename().string()
                                    << ": warehouse at (" << b->x << "," << b->y << ") rot "
                                    << int(b->rotation) << " is not accepted as stored");
                        }
                    }
                }
            }
        }
    }
    std::string s;
    for (const auto& [k, v] : tally) {
        s += k + " x" + std::to_string(v) + "; ";
    }
    MESSAGE(seen << " warehouses 1302 (coast rule at rotations 0123 from the stored origin): "
                 << s);
    CHECK(seen >= 62);
    CHECK(as_stored == seen);
}
