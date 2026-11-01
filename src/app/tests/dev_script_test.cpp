// The scripted session and its replay without a window (W4.4 round ee, review of E3): the
// script's parser, the foreign-input filter, a click step's events through the window's own SDL
// mapping (dev_tool_step) onto the tile it names, the waits and their limit, and the replay's
// comparison of hashes and outcomes on lan5 (game files) with a moved command and a changed
// refusal as controls.
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/dev_script.hpp>
#include <anno/app/dev_session.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/play.hpp>
#include <anno/app/sdl_input.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>
#include <anno/sim/types.hpp>
#include <anno/test/dev_lan5.hpp>

namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;

namespace {

constexpr std::int32_t kX0 = 100, kY0 = 40, kSize = 30;

std::shared_ptr<const render::Scene> scene() {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    bytes->push_back(std::vector<std::uint8_t>(64, 0));
    anno::formats::insel::Island island;
    island.head = (*bytes)[0];
    island.x = static_cast<std::uint16_t>(kX0);
    island.y = static_cast<std::uint16_t>(kY0);
    island.width = kSize;
    island.height = kSize;
    return render::Scene::create("dev-script", {island}, anno::assets::island::GroundTextures{},
                                 anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
}

app::DevToolSetup setup() {
    sim::TypeRegistry::Builder b;
    sim::BuildingType a;
    a.id = sim::TypeId{501};
    a.prodtype = 161;
    sim::BuildingType c = a;
    c.id = sim::TypeId{502};
    b.add(a).add(c);
    auto built = b.build();
    REQUIRE(built.ok());
    app::DevToolSetup s;
    s.registry = built.value();
    s.types = {sim::TypeId{501}, sim::TypeId{502}};
    s.city = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    s.island = app::TileRect{kX0, kY0, static_cast<std::uint32_t>(kSize),
                             static_cast<std::uint32_t>(kSize)};
    s.issuer = sim::PlayerId{1};
    return s;
}

/// A group-1 frame of 1600 x 1000 drawable pixels centred on the island's middle tile.
app::PointerFrame frame(const std::shared_ptr<const render::Scene>& s) {
    const auto [tw, th] = render::kTile[1];
    const int cx = (kX0 - kY0) * tw / 2;
    const int cy = (kX0 + kY0 + kSize) * th / 2 + th / 2;
    return app::pointer_frame(render::Rect{cx - 800, cy - 500, 1600, 1000}, nullptr, 1, tw, th, s,
                              1600, 1000);
}

struct Driver {
    std::shared_ptr<const render::Scene> s = scene();
    app::DevTool tool{setup()};
    app::PointerFrame f;
    float density = 1.0F;
    std::vector<sim::Command> commands;
    app::DevScriptState state() const {
        app::DevScriptState st;
        st.tool = &tool;
        st.frame = &f;
        st.density = density;
        st.window_w = static_cast<int>(1600 / density); // window points
        st.window_h = static_cast<int>(1000 / density);
        return st;
    }
    /// The events through the window's own mapping, as the viewer handles them.
    void feed(const app::DevScriptAction& act) {
        for (const SDL_Event& e : act.events) {
            CHECK_FALSE(app::foreign_input(e));
            for (sim::Command& c : app::dev_tool_step(tool, e, density, &f).commands) {
                commands.push_back(std::move(c));
            }
        }
    }
};

} // namespace

TEST_CASE("dev script: the parser takes the steps and names a bad line" *
          doctest::test_suite("dev_script")) {
    const auto ok = app::DevScript::parse("# a comment\nkey b\nselect 501 # trailing\n"
                                          "click 1 2\ndrag 1 2 3 4\nexpect title tool: build 501\n"
                                          "expect outcome none\nwait outcome\nquit\n");
    REQUIRE(ok.ok());
    for (const char* bad : {"jump 1 2", "click 1", "drag 1 2 3", "key", "quit now"}) {
        CAPTURE(bad);
        const auto r = app::DevScript::parse(std::string("key b\n") + bad + "\n");
        REQUIRE_FALSE(r.ok());
        CHECK(r.error().find("line 2") != std::string::npos);
    }
}

TEST_CASE("dev script: only the script's own input is taken" * doctest::test_suite("dev_script")) {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.which = 0; // a person's mouse (SDL's id, or 0)
    CHECK(app::foreign_input(e));
    e.motion.which = app::kScriptDevice;
    CHECK_FALSE(app::foreign_input(e));
    SDL_Event k{};
    k.type = SDL_EVENT_KEY_DOWN;
    k.key.which = 7;
    CHECK(app::foreign_input(k));
    SDL_Event w{};
    w.type = SDL_EVENT_MOUSE_WHEEL;
    CHECK(app::foreign_input(w));
    SDL_Event b{};
    b.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    CHECK(app::foreign_input(b));
    SDL_Event q{};
    q.type = SDL_EVENT_QUIT; // not input: the window keeps it
    CHECK_FALSE(app::foreign_input(q));
}

TEST_CASE("dev script: a click step's events land on the tile it names, at density 1 and 2, and "
          "the waits end or fail" *
          doctest::test_suite("dev_script")) {
    for (const float density : {1.0F, 2.0F}) {
        CAPTURE(density);
        Driver d;
        d.density = density;
        d.f = frame(d.s);
        auto script = app::DevScript::parse("key b\nselect 502\nclick 110 50\nclick 103 61\n"
                                            "wait outcome\nexpect outcome none\n");
        REQUIRE(script.ok());
        app::DevScript& sc = script.value();
        // One input step per call; the window handles it before the next call.
        for (int i = 0; i < 8 && d.commands.size() < 2; ++i) {
            d.feed(sc.step(d.state()));
        }
        REQUIRE(d.commands.size() == 2);
        const auto& first = std::get<sim::BuildCmd>(d.commands[0].body);
        CHECK(first.placements[0].type == sim::TypeId{502});
        CHECK(first.placements[0].x == 110);
        CHECK(first.placements[0].y == 50);
        const auto& second = std::get<sim::BuildCmd>(d.commands[1].body);
        CHECK(second.placements[0].x == 103);
        CHECK(second.placements[0].y == 61);
        // `wait outcome` does not end without one; past the limit it fails the script.
        auto st = d.state();
        st.now_ms = 0;
        CHECK(sc.step(st).events.empty());
        CHECK_FALSE(sc.done());
        st.now_ms = app::DevScript::kWaitLimitMs + 1.0;
        static_cast<void>(sc.step(st));
        CHECK(sc.failed());
    }
    // `expect outcome none` fails when the latest click did send a command.
    Driver d;
    d.f = frame(d.s);
    auto script = app::DevScript::parse("expect outcome none\n");
    REQUIRE(script.ok());
    auto st = d.state();
    st.latest = app::CommandOutcome{};
    static_cast<void>(script.value().step(st));
    CHECK(script.value().failed());
    // A tile that is not drawn fails the click, it does not guess.
    auto off = app::DevScript::parse("click 5000 5000\n");
    REQUIRE(off.ok());
    CHECK(off.value().step(d.state()).events.empty());
    CHECK(off.value().failed());
}

TEST_CASE("dev session: the replay agrees with a recorded run, and a moved command or a changed "
          "refusal does not (game files)" *
          doctest::test_suite("dev_script")) {
    auto l = anno::test::lan5();
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto free = anno::test::free_tile(*l);
    REQUIRE(free.has_value());
    const app::DevToolSetup st = l->setup;
    auto build = [&st](std::uint32_t type, sim::TileXY at) {
        sim::Command c;
        c.issuer = st.issuer;
        sim::BuildCmd b;
        b.city = *st.city;
        b.placements.push_back(sim::PlacementWire{sim::TypeId{type}, at.x, at.y, 0, 0, 0});
        c.body = std::move(b);
        return c;
    };
    sim::Command demolish;
    demolish.issuer = st.issuer;
    demolish.body = sim::DemolishCmd{*st.city, {*free}};
    // The session: a build, the same tile again (Occupied), a demolish, at turns 2, 4 and 30.
    const std::vector<app::ScriptedCommand> session{
        {2, build(1307, *free)}, {4, build(1307, *free)}, {30, demolish}};
    const auto path = std::filesystem::path(ANNO_FIXTURES_DIR).parent_path().parent_path() /
                      "data" / "maps" / "lan5.txt";
    const auto dir = anno::test::game_dir();
    auto load = [&] {
        auto p = app::load_play(*dir, path);
        REQUIRE(p.ok());
        return std::move(p.value().world);
    };
    app::PlayRunner live(load());
    live.set_script(session);
    live.set_record_hashes(true);
    live.run_n(40);
    const auto hashes = app::parse_hashes(app::format_hashes(live.recorded_hashes()));
    REQUIRE(hashes.ok());
    const std::string outcomes = app::format_outcomes(live.outcomes_since(1));
    CHECK(outcomes.find("turn 4 Refused Occupied") != std::string::npos);
    const auto same = app::replay_session(load(), 100, session, hashes.value(), outcomes);
    CHECK_MESSAGE(same.agree, same.message);
    CHECK(same.outcomes == 3);
    // A command moved by one tile: a hash differs.
    auto moved = session;
    moved[0] = {2, build(1307, {free->x + 1, free->y})};
    const auto m = app::replay_session(load(), 100, moved, hashes.value(), outcomes);
    CHECK_FALSE(m.agree);
    CHECK(m.message.find("differs") != std::string::npos);
    // The refused command with another type: the state is the same (still refused), the
    // outcome's reason is not (a road type is Unsupported before the occupancy check).
    auto changed = session;
    changed[1] = {4, build(2520, *free)};
    const auto c = app::replay_session(load(), 100, changed, hashes.value(), outcomes);
    CHECK_FALSE(c.agree);
    CHECK_MESSAGE(c.message.find("outcome 2 differs") != std::string::npos, c.message);
    MESSAGE("replay: " << same.message << "; moved: " << m.message << "; changed: " << c.message);
}
