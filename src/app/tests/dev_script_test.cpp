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
#include <tuple>
#include <utility>
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
    const int cx = (kY0 - kX0) * tw / 2;
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
        app::FixedPlayOptions fixed;
        fixed.stand_in_placement = true; // island-relative tiles, not chosen for terrain
        auto p = app::load_play(*dir, path, fixed);
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
    // A wire-representable absent type is refused before occupancy. Roads are now
    // supported, so a road is no longer a valid Unsupported negative control.
    constexpr std::uint32_t absent_type = 65535;
    REQUIRE(st.registry->find(sim::TypeId{absent_type}) == nullptr);
    auto changed = session;
    changed[1] = {4, build(absent_type, *free)};
    app::PlayRunner changed_live(load());
    changed_live.set_script(changed);
    changed_live.set_record_hashes(true);
    changed_live.run_n(40);
    CHECK(app::format_hashes(changed_live.recorded_hashes()) ==
          app::format_hashes(live.recorded_hashes()));
    CHECK(app::format_outcomes(changed_live.outcomes_since(1)).find("turn 4 Refused UnknownType") !=
          std::string::npos);
    const auto c = app::replay_session(load(), 100, changed, hashes.value(), outcomes);
    CHECK_FALSE(c.agree);
    CHECK_MESSAGE(c.message.find("outcome 2 differs") != std::string::npos, c.message);
    MESSAGE("replay: " << same.message << "; moved: " << m.message << "; changed: " << c.message);
}

TEST_CASE("dev script: wait drawn ends only when the change is settled on its way to the screen "
          "(round ee review, R2)" *
          doctest::test_suite("dev_script")) {
    using V = app::Visibility;
    const auto latest = [](app::CommandOutcome::State state) {
        app::CommandOutcome o;
        o.id = {app::CommandOrigin::Window, 1};
        o.state = state;
        o.turn = 7;
        return o;
    };
    const auto run = [&](app::CommandOutcome::State state, V visibility) {
        auto script = app::DevScript::parse("wait drawn\nexpect visibility " +
                                            std::string(app::visibility_name(visibility)) + "\n");
        REQUIRE(script.ok());
        app::DevScriptState st;
        st.latest = latest(state);
        st.visibility = visibility;
        st.now_ms = 1.0;
        static_cast<void>(script.value().step(st));
        return script.value().done() && !script.value().failed();
    };
    // Every earlier boundary keeps waiting: the sim applied it, a picture update is not display.
    for (const V waiting : {V::NotInPicture, V::Committing, V::AwaitingFrame, V::ChunksStale,
                            V::NotOnGpu, V::NotPresented, V::OutOfView}) {
        CAPTURE(app::visibility_name(waiting));
        CHECK_FALSE(run(app::CommandOutcome::State::Applied, waiting));
    }
    // Settled: shown, or classified as not shown at all (and the expectation names which).
    for (const V settled : {V::Visible, V::Offscreen, V::NothingToDraw, V::NoVisualChange}) {
        CAPTURE(app::visibility_name(settled));
        CHECK(run(app::CommandOutcome::State::Applied, settled));
    }
    // A command that was not applied has no change to wait for; a queued one is not over.
    CHECK(run(app::CommandOutcome::State::Refused, V::NoVisualChange));
    CHECK_FALSE(run(app::CommandOutcome::State::Queued, V::NoVisualChange));
    // The expectation fails when the change is somewhere else than named.
    auto script = app::DevScript::parse("expect visibility Visible\n");
    REQUIRE(script.ok());
    app::DevScriptState st;
    st.latest = latest(app::CommandOutcome::State::Applied);
    st.visibility = V::Offscreen;
    static_cast<void>(script.value().step(st));
    CHECK(script.value().failed());
}

TEST_CASE("dev script: hold, release, note and the consistency expectations (W4-WIN-CLOSE C1)" *
          doctest::test_suite("dev_script")) {
    const auto step_once = [](const std::string& text, const app::DevScriptState& st) {
        auto script = app::DevScript::parse(text);
        REQUIRE(script.ok());
        const app::DevScriptAction act = script.value().step(st);
        return std::pair{std::move(script).value(), act.hold};
    };
    // hold/release need a hold in the session, and each asks the window for exactly one action.
    app::DevScriptState with;
    with.hold_available = true;
    {
        auto [s, hold] = step_once("hold publication\nrelease publication\n", with);
        CHECK_FALSE(s.failed());
        CHECK(hold == app::DevScriptAction::Hold::Arm); // one input step per call
        CHECK_FALSE(s.done());
    }
    {
        auto [s, hold] = step_once("release publication\n", with);
        CHECK(hold == app::DevScriptAction::Hold::Release);
    }
    {
        auto [s, hold] = step_once("hold publication\n", app::DevScriptState{});
        CHECK(s.failed());
        CHECK(hold == app::DevScriptAction::Hold::None);
    }
    // wait held: over only while a publication waits; a watchdog ending or no hold fails it.
    {
        auto [s, h] = step_once("wait held\n", with);
        CHECK_FALSE(s.done());
        app::DevScriptState held = with;
        held.held = true;
        auto [s2, h2] = step_once("wait held\n", held);
        CHECK(s2.done());
        CHECK_FALSE(s2.failed());
        app::DevScriptState expired = with;
        expired.hold_expired = true;
        auto [s3, h3] = step_once("wait held\n", expired);
        CHECK(s3.failed());
    }
    // expect published: an Applied or Refused outcome newer than the snapshot is the old,
    // inconsistent view; a Queued (withheld) one and an older one pass.
    const auto snap = [](std::uint64_t outcomes_next, std::int64_t money) {
        auto s = std::make_shared<app::PlaySnapshot>();
        s->outcomes_next = outcomes_next;
        s->money = money;
        return s;
    };
    const auto outcome = [](app::CommandOutcome::State state, std::uint64_t seq) {
        app::CommandOutcome o;
        o.id = {app::CommandOrigin::Window, 1};
        o.state = state;
        o.seq = seq;
        return o;
    };
    using S = app::CommandOutcome::State;
    for (const auto& [state, seq, next, passes] :
         {std::tuple{S::Applied, 5ULL, 5ULL, false}, std::tuple{S::Refused, 6ULL, 5ULL, false},
          std::tuple{S::Applied, 4ULL, 5ULL, true}, std::tuple{S::Queued, 9ULL, 5ULL, true}}) {
        app::DevScriptState st;
        st.snapshot = snap(next, 0);
        st.latest = outcome(state, seq);
        auto [s, h] = step_once("expect published\n", st);
        CAPTURE(static_cast<int>(state));
        CHECK(s.failed() == !passes);
    }
    // note money / expect money same|less: the note is taken, a wait lets the state move, and the
    // expectation reads the later snapshot.
    const auto money_script = [&](const std::string& expectation, std::int64_t later) {
        auto script =
            app::DevScript::parse("note money\nwait frames 1\nexpect money " + expectation + "\n");
        REQUIRE(script.ok());
        app::DevScriptState st;
        st.snapshot = snap(1, 1000);
        st.frames = 10;
        static_cast<void>(script.value().step(st)); // notes 1000, then waits for a frame
        st.snapshot = snap(1, later);
        st.frames = 11;
        static_cast<void>(script.value().step(st));
        REQUIRE(script.value().done());
        return !script.value().failed();
    };
    CHECK(money_script("same", 1000));
    CHECK_FALSE(money_script("same", 900));
    CHECK(money_script("less", 900));
    CHECK_FALSE(money_script("less", 1000));
    // title-lacks, and occupancy counters need the tool's index.
    {
        app::DevScriptState st;
        st.title = "anno1503 - command #1: Queued";
        auto [ok, h] = step_once("expect title-lacks Applied\n", st);
        CHECK_FALSE(ok.failed());
        auto [bad, h2] = step_once("expect title-lacks Queued\n", st);
        CHECK(bad.failed());
        auto [none, h3] = step_once("expect work rebuilds == 0\n", st);
        CHECK(none.failed()); // no index to read
    }
    // A step that is not a step is named at parse time.
    CHECK_FALSE(app::DevScript::parse("hold\n").ok());
    CHECK_FALSE(app::DevScript::parse("note\n").ok());
}

TEST_CASE(
    "dev script: stage holds, wait visibility, wait blocked and expect held (W4-WIN-CLOSE C2)" *
    doctest::test_suite("dev_script")) {
    using Stage = app::DevScriptAction::Stage;
    const auto once = [](const std::string& text, const app::DevScriptState& st) {
        auto script = app::DevScript::parse(text);
        REQUIRE(script.ok());
        const app::DevScriptAction act = script.value().step(st);
        return std::pair{std::move(script).value(), act};
    };
    app::DevScriptState gpu;
    gpu.stage_holds_available = true;
    // Each hold/release asks the window for exactly one stage action; without the GPU window the
    // step fails.
    for (const auto& [text, want] : {std::pair{"hold chunks\n", Stage::ArmChunks},
                                     std::pair{"release chunks\n", Stage::ReleaseChunks},
                                     std::pair{"hold uploads\n", Stage::ArmUploads},
                                     std::pair{"release uploads\n", Stage::ReleaseUploads},
                                     std::pair{"hold presents\n", Stage::FailPresents},
                                     std::pair{"release presents\n", Stage::PassPresents}}) {
        CAPTURE(text);
        auto [s, act] = once(text, gpu);
        CHECK_FALSE(s.failed());
        CHECK(act.stage == want);
        auto [s2, act2] = once(text, app::DevScriptState{});
        CHECK(s2.failed());
        CHECK(act2.stage == Stage::None);
    }
    // wait blocked: over only while a builder waits in the gate.
    {
        auto [s, act] = once("wait blocked\n", gpu);
        CHECK_FALSE(s.done());
        app::DevScriptState blocked = gpu;
        blocked.chunks_blocked = 2;
        auto [s2, act2] = once("wait blocked\n", blocked);
        CHECK(s2.done());
        CHECK_FALSE(s2.failed());
    }
    // expect held NAME: in effect, or the step fails and names it.
    for (const char* name : {"chunks", "uploads", "presents", "publication"}) {
        CAPTURE(name);
        auto [none, a] = once(std::string("expect held ") + name + "\n", gpu);
        CHECK(none.failed());
        app::DevScriptState on = gpu;
        on.chunks_armed = on.uploads_held = on.presents_failing = on.held = true;
        auto [yes, b] = once(std::string("expect held ") + name + "\n", on);
        CHECK_FALSE(yes.failed());
    }
    // wait visibility NAME: only for an applied command whose change is at NAME.
    {
        app::CommandOutcome o;
        o.id = {app::CommandOrigin::Window, 1};
        o.state = app::CommandOutcome::State::Applied;
        app::DevScriptState st;
        st.latest = o;
        st.visibility = app::Visibility::ChunksStale;
        auto [wrong, a] = once("wait visibility NotOnGpu\n", st);
        CHECK_FALSE(wrong.done());
        auto [right, b] = once("wait visibility ChunksStale\n", st);
        CHECK(right.done());
        CHECK_FALSE(right.failed());
        st.latest->state = app::CommandOutcome::State::Queued;
        auto [queued, c] = once("wait visibility ChunksStale\n", st);
        CHECK_FALSE(queued.done());
    }
    // resize needs a size.
    {
        auto [s, act] = once("resize 1100 640\n", app::DevScriptState{});
        REQUIRE(act.resize.has_value());
        CHECK(act.resize->first == 1100);
        CHECK(act.resize->second == 640);
        CHECK_FALSE(app::DevScript::parse("resize 1100\n").ok());
        auto [bad, none] = once("resize 0 640\n", app::DevScriptState{});
        CHECK(bad.failed());
    }
}

TEST_CASE("dev script: fail and pass a CPU delivery stage, expect failing (W4-WIN-FINAL F2)" *
          doctest::test_suite("dev_script")) {
    const auto once = [](const std::string& text, const app::DevScriptState& st) {
        auto script = app::DevScript::parse(text);
        REQUIRE(script.ok());
        const app::DevScriptAction act = script.value().step(st);
        return std::pair{std::move(script).value(), act};
    };
    app::DevScriptState cpu;
    cpu.cpu_fail_available = true;
    for (const auto& [name, bit] :
         {std::pair{"update", 1}, std::pair{"submit", 2}, std::pair{"present", 4}}) {
        CAPTURE(name);
        auto [s, act] = once(std::string("fail ") + name + "\n", cpu);
        CHECK_FALSE(s.failed());
        CHECK(act.cpu_fail_set == bit);
        CHECK(act.cpu_fail_clear == 0);
        auto [p, act2] = once(std::string("pass ") + name + "\n", cpu);
        CHECK(act2.cpu_fail_clear == bit);
        // Without the CPU seam (the GPU window) the step fails.
        auto [g, act3] = once(std::string("fail ") + name + "\n", app::DevScriptState{});
        CHECK(g.failed());
        CHECK(act3.cpu_fail_set == 0);
        // expect failing: only while the stage is in effect.
        app::DevScriptState on = cpu;
        on.cpu_failing = static_cast<std::uint8_t>(bit);
        auto [yes, a4] = once(std::string("expect failing ") + name + "\n", on);
        CHECK_FALSE(yes.failed());
        auto [no, a5] = once(std::string("expect failing ") + name + "\n", cpu);
        CHECK(no.failed());
    }
    auto [bad, act] = once("fail frobnicate\n", cpu);
    CHECK(bad.failed());
}

TEST_CASE("dev script: the log opens with the identity of the whole parsed script and a ran record "
          "keeps every argument (W4-WIN-VERIFY V2)" *
          doctest::test_suite("dev_script")) {
    // The expected values are derived apart from the code: FNV-1a 64 (offset basis
    // cbf29ce484222325, prime 100000001b3) over the canonical text, one `LINE VERB ARG...` line per
    // step after comments and whitespace, wait steps included. tools/test/viewer_session.py
    // computes the same value from the script file; viewer_wiring_contract_test.py asserts these
    // same scripts and values (test_the_python_identity_agrees_with_the_cpp_vectors), so a change
    // to either implementation alone fails a cheap test.
    const std::string base =
        "# comment\nexpect title-lacks a  b # trailing\n\twait frames 3\nquit\n";
    const auto parse = [](const std::string& text) {
        auto script = app::DevScript::parse(text);
        REQUIRE(script.ok());
        return std::move(script).value();
    };
    const auto first = [](const app::DevScript& s) {
        REQUIRE_FALSE(s.log().empty());
        return s.log().front();
    };
    CHECK(first(parse(base)) == "script identity: steps=3 fnv1a64=8d702b850fad24d4");
    // Whitespace and comments do not change it, a moved line does.
    CHECK(first(parse("# other\nexpect   title-lacks a b\nwait\tframes 3  # x\nquit")) ==
          "script identity: steps=3 fnv1a64=8d702b850fad24d4");
    CHECK(first(parse("expect title-lacks a b\nwait frames 3\nquit\n")) !=
          "script identity: steps=3 fnv1a64=8d702b850fad24d4");
    // A later argument of an expectation, and a wait's value, each change it; so does a step
    // removed.
    CHECK(first(parse("# comment\nexpect title-lacks a c\nwait frames 3\nquit\n")) ==
          "script identity: steps=3 fnv1a64=6d327550d5e6e183");
    CHECK(first(parse("# comment\nexpect title-lacks a b\nwait frames 4\nquit\n")) ==
          "script identity: steps=3 fnv1a64=7f40b977c080e275");
    CHECK(first(parse("# comment\nexpect title-lacks a b\nquit\n")).find("steps=2") !=
          std::string::npos);
    // tools/test/viewer_wiring_fixtures/tiny.script as the wrapper's contract uses it.
    CHECK(first(parse("# a tiny script\nlook 1 2\nkey b\nwait frames 2\ncheckpoint a\nquit\n")) ==
          "script identity: steps=5 fnv1a64=c0292a5964fabae3");
    // Bytes that are not ASCII whitespace belong to an argument (U+0085 and U+2028 in UTF-8).
    CHECK(first(parse("note a\xc2\x85"
                      "b\nnote c\xe2\x80\xa8"
                      "d\nquit\n")) == "script identity: steps=3 fnv1a64=7c2889893a0c95db");
    // A run records every argument of every step it executes (a wait ends silently).
    app::DevScript s = parse("expect title-lacks zz yy\nnote frames\nquit\n");
    const app::DevScriptAction act = s.step(app::DevScriptState{});
    CHECK(act.quit);
    CHECK_FALSE(s.failed());
    const auto& log = s.log();
    REQUIRE(log.size() == 4);
    CHECK(log[1] == "ran line 1: expect title-lacks zz yy");
    CHECK(log[2] == "ran line 2: note frames");
    CHECK(log[3] == "ran line 3: quit");
}

TEST_CASE("dev script: expect fact's value can hold a space, like title/title-lacks "
          "(W4-SETTLEMENT-1 W7 review, finding 9)" *
          doctest::test_suite("dev_script")) {
    // Before this fix, a fact value with a space parsed into more than 4 args for "expect fact",
    // which no case in step()'s dispatch matches ("unknown expectation") — so a script could not
    // pin an exact token boundary and had to settle for a loose `contains` that also matches a
    // longer value sharing the same prefix (settlement_activity.script's "residents=5" matching
    // "residents=50").
    app::DevScript s =
        app::DevScript::parse("expect fact a == one two three\nexpect fact a contains one two\n"
                              "quit\n")
            .value();
    app::DevScriptState st;
    st.facts["a"] = "one two three";
    const app::DevScriptAction act = s.step(st);
    CHECK(act.quit);
    CHECK_FALSE(s.failed());
    const auto& log = s.log();
    REQUIRE(log.size() == 4);
    // The joined value is logged back exactly as it was written, not collapsed to one word.
    CHECK(log[1] == "ran line 1: expect fact a == one two three");
    CHECK(log[2] == "ran line 2: expect fact a contains one two");
    CHECK(log[3] == "ran line 3: quit");

    // The bug this closes: a single-word value accidentally matches a longer one sharing its
    // prefix. "residents=5" is itself a substring of "residents=50 ...", so a loose check wrongly
    // passes against the wrong count.
    app::DevScript loose =
        app::DevScript::parse("expect fact a contains residents=5\nquit\n").value();
    app::DevScriptState loose_st;
    loose_st.facts["a"] = "residents=50 stock=0";
    CHECK(loose.step(loose_st).quit); // wrongly passes: this is the bug, not the fix
    CHECK_FALSE(loose.failed());

    // Pinning the token boundary through the space this fix now allows ("residents=5 stock=")
    // tells the two counts apart: "residents=50 stock=0" does not contain that exact text.
    app::DevScript pinned =
        app::DevScript::parse("expect fact a contains residents=5 stock=\nquit\n").value();
    app::DevScriptState pinned_st;
    pinned_st.facts["a"] = "residents=50 stock=0";
    CHECK_FALSE(pinned.step(pinned_st).quit);
    CHECK(pinned.failed());

    // The same pinned check correctly accepts the real residents=5 case.
    app::DevScript pinned_ok =
        app::DevScript::parse("expect fact a contains residents=5 stock=\nquit\n").value();
    app::DevScriptState pinned_ok_st;
    pinned_ok_st.facts["a"] = "residents=5 stock=0,0,0,0,0,0";
    CHECK(pinned_ok.step(pinned_ok_st).quit);
    CHECK_FALSE(pinned_ok.failed());
}

TEST_CASE("dev session: a plain --play recording's invocation carries its --rules profile "
          "(PLAY-LIVE-1a Do item 5)") {
    // main.cpp's own round trip: format_invocation writes session.txt (map, speed, rules);
    // replay_hashes reads it back with parse_invocation_rules and refuses before turn 1 when
    // the replay's own --rules differs. This is that round trip's own proof; the refusal
    // itself (a SIGPIPE-free free function local to main.cpp, not linked into this test
    // binary) is read in main.cpp's replay_hashes, not re-run here.
    const std::string original_text =
        app::format_invocation("x.sze", 100, app::RulesProfile::Original);
    CHECK(original_text == "map x.sze\nspeed 100\nrules original\n");
    const auto original_back = app::parse_invocation_rules(original_text);
    REQUIRE(original_back.has_value());
    CHECK(*original_back == app::RulesProfile::Original);

    const std::string living_text = app::format_invocation("x.sze", 100, app::RulesProfile::Living);
    CHECK(living_text == "map x.sze\nspeed 100\nrules living\n");
    const auto living_back = app::parse_invocation_rules(living_text);
    REQUIRE(living_back.has_value());
    CHECK(*living_back == app::RulesProfile::Living);

    // An older recording's session.txt (before this packet) has no "rules" line at all: not
    // refused, since it makes no claim to check against (Do item 5: "otherwise refuse a
    // differing flag" only applies once a claim can actually be read).
    CHECK_FALSE(app::parse_invocation_rules("map x.sze\nspeed 100\n").has_value());
    CHECK_FALSE(app::parse_invocation_rules("map x.sze\nspeed 100\nrules unknown\n").has_value());
}
