#include <doctest/doctest.h>

#include <anno/sim/replay.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>

#include "world_access.hpp"
using namespace anno::sim;
namespace {
std::string wave_source(std::string kernel) {
    return "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n\"ME\" 0\n[SCRIPT_KERNEL]\n" +
           kernel + "\n[SIGNALS]\n[CAMARAPOS]\n0,0\n";
}
auto wave_world(std::string kernel, ScriptLimits limits = {},
                std::optional<std::string> ns = "scenes/mod") {
    auto spec = synthetic::basic_scenario(77);
    spec.script_context = ScriptContext::Pending;
    spec.script_source = wave_source(kernel);
    spec.script_limits = limits;
    spec.script_asset_namespace = std::move(ns);
    auto made = World::create(spec, TypeRegistry::empty());
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    return std::move(made).value();
}
void event(const SimEvent& e, std::string expected, std::uint32_t offset, std::uint64_t tick = 0) {
    CHECK(e.kind == SimEventKind::ScriptWave);
    CHECK(e.code == 317);
    CHECK(e.tick == tick);
    CHECK_FALSE(e.player.valid());
    REQUIRE(e.wave);
    CHECK(e.wave->asset == expected);
    CHECK(e.wave->source_offset == offset);
    CHECK(e.wave->audience == WaveAudience::AllLocal);
}
} // namespace
TEST_CASE("SCRIPT2 PLAYWAVE exact namespace broadcast order repetition and startup output") {
    const auto source = wave_source(
        "INITBLOCK(){ PLAYWAVE(\"A.WAV\"); PLAYWAVE(\"B.WAV\"); PLAYWAVE(\"A.WAV\"); }");
    auto w =
        wave_world("INITBLOCK(){ PLAYWAVE(\"A.WAV\"); PLAYWAVE(\"B.WAV\"); PLAYWAVE(\"A.WAV\"); }");
    REQUIRE(w.initialize_scripts().empty());
    auto events = w.drain_events();
    REQUIRE(events.size() == 3);
    REQUIRE(w.last_output().events.size() == 3);
    auto at = source.find("PLAYWAVE");
    event(events[0], "scenes/mod/A.WAV", static_cast<std::uint32_t>(at));
    at = source.find("PLAYWAVE", at + 1);
    event(events[1], "scenes/mod/B.WAV", static_cast<std::uint32_t>(at));
    at = source.find("PLAYWAVE", at + 1);
    event(events[2], "scenes/mod/A.WAV", static_cast<std::uint32_t>(at));
    CHECK(events == w.last_output().events);
    REQUIRE(w.initialize_scripts().empty());
    CHECK(w.drain_events().empty());
}
TEST_CASE("SCRIPT2 PLAYWAVE failed transaction leaks neither wave nor money") {
    const auto bytes = std::string("scenes/mod/A.WAV").size();
    auto w = wave_world("INITBLOCK(){ MONEY_ADD(\"ME\",5); PLAYWAVE(\"A.WAV\"); TIMER_SET(1,7); }",
                        {.work = static_cast<std::uint32_t>(bytes + 2)});
    const auto money = w.players().find(PlayerId{0})->money;
    const auto rng = w.rng().cursor();
    CHECK_FALSE(w.initialize_scripts().empty());
    CHECK(w.script().state.fault == ScriptFault::Budget);
    CHECK(w.players().find(PlayerId{0})->money == money);
    CHECK(w.rng().cursor() == rng);
    CHECK(w.last_output().events.empty());
    CHECK(w.drain_events().empty());
    auto back = World::load(w.save(), TypeRegistry::empty());
    REQUIRE(back);
    CHECK(back.value().drain_events().empty());
}
TEST_CASE("SCRIPT2 PLAYWAVE bounded queue consumption is outside canonical state") {
    auto w =
        wave_world("INITBLOCK(){ PLAYWAVE(\"A.WAV\"); PLAYWAVE(\"B.WAV\"); PLAYWAVE(\"A.WAV\"); }");
    auto mute = w;
    w.set_event_capacity(2);
    mute.set_event_capacity(0);
    REQUIRE(w.initialize_scripts().empty());
    REQUIRE(mute.initialize_scripts().empty());
    CHECK(w.state_hash() == mute.state_hash());
    CHECK(w.events_dropped() == 1);
    CHECK(mute.events_dropped() == 3);
    auto events = w.drain_events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].wave->asset == "scenes/mod/B.WAV");
    CHECK(events[1].wave->asset == "scenes/mod/A.WAV");
    CHECK(mute.drain_events().empty());
    CHECK(w.save() == mute.save());
    auto back = World::load(w.save(), TypeRegistry::empty());
    REQUIRE(back);
    CHECK(back.value().drain_events().empty());
    CHECK(w.script().state.last_work == mute.script().state.last_work);
    w.advance(250);
    mute.advance(250);
    back.value().advance(250);
    CHECK(w.save() == mute.save());
    CHECK(w.save() == back.value().save());
}
TEST_CASE("SCRIPT2 PLAYWAVE native cuts preserve future emission and suppress past delivery") {
    auto w = wave_world("SET ONCE=TRUE; IF ONCE THEN { ONCE=FALSE; PLAYWAVE(\"A.WAV\"); }");
    REQUIRE(w.initialize_scripts().empty());
    w.advance(500);
    auto before = World::load(w.save(), TypeRegistry::empty());
    REQUIRE(before);
    w.advance(0);
    before.value().advance(0);
    REQUIRE(w.last_output().events.size() == 1);
    event(w.last_output().events[0], "scenes/mod/A.WAV",
          static_cast<std::uint32_t>(w.script().source.find("PLAYWAVE")), 250);
    CHECK(w.drain_events() == before.value().drain_events());
    CHECK(w.save() == before.value().save());
    auto after = World::load(w.save(), TypeRegistry::empty());
    REQUIRE(after);
    CHECK(after.value().drain_events().empty());
    w.advance(0);
    after.value().advance(0);
    CHECK(w.drain_events().empty());
    CHECK(after.value().drain_events().empty());
    CHECK(w.save() == after.value().save());
}
TEST_CASE("SCRIPT2 PLAYWAVE validates portable context assets and configurable payload bounds") {
    for (const auto asset : {"", "../A.WAV", "C:\\A.WAV", "/A.WAV", "a//b.WAV", "a/./b.WAV"}) {
        const auto source = wave_source("INITBLOCK(){ PLAYWAVE(\"" + std::string(asset) + "\"); }");
        CHECK_FALSE(compile_script(source, {{}, 32, 5, {}, "scenes/mod"}));
    }
    const auto source = wave_source("INITBLOCK(){ PLAYWAVE(\"A.WAV\"); }");
    CHECK_FALSE(compile_script(source, {{}, 32, 5}));
    CHECK_FALSE(compile_script(source, {{}, 32, 5, {}, "/host/absolute"}));
    CHECK_FALSE(compile_script(source, {{}, 32, 5, {}, "scenes/mod"}, {.asset_bytes = 1}));
    CHECK(compile_script(source, {{}, 32, 5, {}, "scenes/mod"}, {.asset_bytes = 65536}));
    CHECK_FALSE(compile_script(source, {{}, 32, 5, {}, "scenes/mod"}, {.asset_bytes = 65537}));
    auto w = wave_world("INITBLOCK(){ PLAYWAVE(\"sub\\A.WAV\"); }", {}, "scenes\\mod");
    REQUIRE(w.initialize_scripts().empty());
    auto events = w.drain_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].wave->asset == "scenes/mod/sub/A.WAV");
}
TEST_CASE("SCRIPT2 PLAYWAVE query Host failure discards staged output and prior money") {
    auto w = wave_world("INITBLOCK(){ MONEY_ADD(\"ME\",5); PLAYWAVE(\"A.WAV\"); }");
    auto source = w.script().source;
    source.insert(source.find("[PLAYERMAPPER_DEFINE]"), "\"FIELD\" 0,0,16,16\n");
    source.insert(source.find("[SIGNALS]"),
                  "IF AREA \"FIELD\" TOUCHED_BY_SHIP_FROM \"ME\" THEN {}\n");
    auto r =
        prepare_script(source, *TypeRegistry::empty(), w.players(), w.config(), {}, "scenes/mod");
    REQUIRE(r);
    auto players = w.players();
    auto timers = w.timers();
    std::vector<SimEvent> events;
    REQUIRE_FALSE(advance_script(r.value(), players, timers, *TypeRegistry::empty(), 250, nullptr,
                                 nullptr, &events, 17));
    CHECK(r.value().state.fault == ScriptFault::Host);
    CHECK(events.empty());
    CHECK(players.find(PlayerId{0})->money == w.players().find(PlayerId{0})->money);
}

TEST_CASE("SCRIPT2 PLAYWAVE compiled asset storage does not multiply a shared namespace") {
    const std::string ns(4000, 'n');
    std::string body = "INITBLOCK(){";
    for (int i = 0; i < 128; ++i)
        body += "PLAYWAVE(\"a\");";
    body += "}";
    const auto source = wave_source(body);
    auto compiled = compile_script(source, {{}, 32, 5, {}, ns}, {.work = 1000000});
    REQUIRE(compiled);
    CHECK(compiled.value().asset_namespace == ns);
    REQUIRE(compiled.value().assets.size() == 128);
    std::size_t stored = 0;
    for (const auto& asset : compiled.value().assets) {
        CHECK(asset == "a");
        stored += asset.size();
    }
    CHECK(stored == 128); // previous joined representation used512256bytes here
    CHECK(stored + compiled.value().asset_namespace.size() == 4128);
    auto w = wave_world(body, {.work = 1000000}, ns);
    REQUIRE(w.initialize_scripts().empty());
    auto events = w.drain_events();
    REQUIRE(events.size() == 128);
    for (const auto& event : events) {
        REQUIRE(event.wave);
        CHECK(event.wave->asset == ns + "/a");
    }
    CHECK(w.script().state.last_work ==
          128 * (4000 + 1 + 1 + 1)); // full payload plus statement, unchanged
}
