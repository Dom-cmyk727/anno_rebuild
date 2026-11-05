#include <algorithm>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/script_base.hpp>

#include "world_access.hpp"
using namespace anno::sim;
namespace {
std::string text(std::string kernel) {
    return "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n\"ME\" 0\n[SCRIPT_KERNEL]\n" +
           kernel + "\n[SIGNALS]\n[CAMARAPOS]\n0,0\n";
}
auto setup(std::string kernel, ScriptLimits limits = {}) {
    auto spec = synthetic::basic_scenario(77);
    spec.script_context = ScriptContext::Pending;
    spec.script_source = text(std::move(kernel));
    spec.script_limits = limits;
    return World::create(spec, TypeRegistry::empty());
}
void rechecksum(std::vector<std::uint8_t>& bytes) {
    StateHasher hash;
    hash.bytes(std::span(bytes).first(bytes.size() - 8));
    const auto value = hash.value();
    for (std::size_t k = 0; k < 8; ++k)
        bytes[bytes.size() - 8 + k] = static_cast<std::uint8_t>(value >> (8 * k));
}
} // namespace
TEST_CASE("SCRIPT1 native header and empty runtime have hand written bytes") {
    WriteArchive header;
    StateHeader h;
    fields(header, h);
    const std::vector<std::uint8_t> expected_header{129,  0,    0,    0, 0x53, 0x43, 0x52, 0x49,
                                                    0x50, 0x54, 0x31, 0, 5,    0,    0,    0};
    CHECK(header.data() == expected_header);
    auto wrong = expected_header;
    wrong[4] ^= 1;
    ReadArchive reader(wrong);
    StateHeader other;
    fields(reader, other);
    REQUIRE(reader.failed());
    CHECK(reader.error().find("script state schema differs") != std::string::npos);
    ScriptRuntime r;
    r.state.context = ScriptContext::ExplicitNone;
    WriteArchive out;
    fields(out, r);
    const std::vector<std::uint8_t> expected{
        0, 0, 0,  0, 0, 0, 0, 0,                                  // source length
        0, 0, 16, 0, 0, 0, 1, 0, 128, 0, 0, 0, 0xa0, 0x86, 1, 0,  // four limits
        1, 0, 0,  0,                                              // execution policy
        1, 0,                                                     // ExplicitNone, no fault
        0, 0, 0,  0, 0, 0, 0, 0, 0,   0, 0, 0, 0,    0,    0, 0,  // continuation
        0, 0, 0,  0, 0, 0, 0, 0, 0,   0, 0, 0, 0,    0,    0, 0}; // signals, bindings
    CHECK(out.data() == expected);
}
TEST_CASE("SCRIPT1 normal advance save reload preserves initialization and deferred IF") {
    auto made = setup("SET X = FALSE;\nSET SEEN = TRUE;\nINITBLOCK(){ X = TRUE; TIMER_SET(1,7); }\n"
                      "IF X THEN { X = FALSE; }\nIF X THEN {} ELSE { SEEN = FALSE; }\n");
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    auto w = std::move(made).value();
    CHECK(w.script().state.context == ScriptContext::Pending);
    auto pending = World::load(w.save(), TypeRegistry::empty());
    REQUIRE(pending);
    REQUIRE(w.advance(0) == 0);
    REQUIRE(pending.value().advance(0) == 0);
    CHECK(w.state_hash() == pending.value().state_hash());
    CHECK(w.script().state.signals[1] == 1);
    CHECK(w.timers().counter[1] == 3500);
    auto ready = World::load(w.save(), TypeRegistry::empty());
    REQUIRE(ready);
    REQUIRE(w.advance(126) == 63);
    REQUIRE(ready.value().advance(126) == 63); // oldarg0, no IF
    CHECK(w.script().state.cursor == 0);
    REQUIRE(w.advance(0) == 63);
    REQUIRE(ready.value().advance(0) == 63);
    CHECK(w.script().state.cursor == 1);
    CHECK(w.script().state.signals[1] == 0);
    auto deferred = World::load(w.save(), TypeRegistry::empty());
    REQUIRE(deferred);
    REQUIRE(w.advance(0) == 63);
    REQUIRE(deferred.value().advance(0) == 63);
    CHECK(w.script().state.signals[2] == 0);
    CHECK(w.state_hash() == deferred.value().state_hash());
    CHECK(w.check_invariants().empty());
}
TEST_CASE("SCRIPT1 runtime exhaustion persists and stops world time") {
    auto made = setup("INITBLOCK(){ TIMER_SET(1,7); TIMER_START(1); }", {.work = 1});
    REQUIRE(made);
    auto w = std::move(made).value();
    CHECK(w.advance(500) == 0);
    CHECK(w.faulted());
    CHECK(w.timers().counter[1] == 0);
    CHECK(w.script().state.fault == ScriptFault::Budget);
    CHECK(w.check_invariants() == "world fault: script runtime fault");
    auto restored = World::load(w.save(), TypeRegistry::empty());
    REQUIRE(restored);
    CHECK(restored.value().state_hash() == w.state_hash());
    CHECK(restored.value().advance(500) == 0);
    CHECK(restored.value().state_hash() == w.state_hash());
}
TEST_CASE("SCRIPT1 repaired checksum version and same version schema refusals are specific") {
    auto made = setup("");
    REQUIRE(made);
    const auto bytes = made.value().save();
    auto old = bytes;
    old[8] = 128;
    rechecksum(old);
    auto refused = World::load(old, TypeRegistry::empty());
    REQUIRE_FALSE(refused);
    CHECK(refused.error() ==
          "save: state stream version 128 is not supported (this build reads 129)");
    auto foreign = bytes;
    foreign[12] ^= 1;
    rechecksum(foreign);
    auto conflict = World::load(foreign, TypeRegistry::empty());
    REQUIRE_FALSE(conflict);
    CHECK(conflict.error() == "save: script state schema differs from this build's");
    auto other = setup("SET X = TRUE;");
    REQUIRE(other);
    CHECK(other.value().state_hash() != made.value().state_hash());
}
TEST_CASE("SCRIPT1 recorder and fresh decoded replay share normal visits") {
    auto made =
        setup("SET X = FALSE;\nINITBLOCK(){ X = TRUE; }\nIF PLAYTIME > 0 THEN { X = FALSE; }\n");
    REQUIRE(made);
    auto world = std::move(made).value();
    auto started = Recorder::start(world);
    REQUIRE(started);
    auto rec = std::move(started).value();
    rec.advance(0);
    rec.advance(1100);
    rec.advance(0);
    const auto encoded = write_log(rec.log());
    REQUIRE(encoded);
    auto decoded = read_log(encoded.value());
    REQUIRE(decoded);
    auto fresh = World::load(decoded.value().initial_save, TypeRegistry::empty());
    REQUIRE(fresh);
    auto replayed = replay(fresh.value(), decoded.value());
    REQUIRE_MESSAGE(replayed, (replayed ? "" : replayed.error()));
    CHECK(fresh.value().state_hash() == world.state_hash());
}

TEST_CASE("SCRIPT1 nonempty continuation and widened permission rows have literal bytes") {
    // Wire-only specimen: distinctive values expose swapped fields. It is not a playable runtime.
    ScriptRuntime r;
    r.source = "X";
    r.limits = {3, 4, 5, 6};
    r.policy = 0x01020304;
    r.state.context = ScriptContext::Fault;
    r.state.fault = ScriptFault::Host;
    r.state.cursor = 0x11223344;
    r.state.last_arg = 0x55667788;
    r.state.fault_offset = 0x12345678;
    r.state.last_work = 0x23456789;
    r.state.signals = {1, 0, 1};
    r.state.bindings = {PlayerId{0x10203}, PlayerId{70}};
    const std::vector<std::uint8_t> expected{
        1,    0,    0,    0,    0,    0,    0,    0,    'X',  3,    0,    0,    0,    4,    0,
        0,    0,    5,    0,    0,    0,    6,    0,    0,    0,    4,    3,    2,    1,    5,
        2,    0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55, 0x78, 0x56, 0x34, 0x12, 0x89, 0x67,
        0x45, 0x23, 3,    0,    0,    0,    0,    0,    0,    0,    1,    0,    1,    2,    0,
        0,    0,    0,    0,    0,    0,    3,    2,    1,    0,    70,   0,    0,    0};
    WriteArchive out;
    fields(out, r);
    CHECK(out.data() == expected);
    auto p = make_player(PlayerId{0}, SimConfig{});
    p.permissions = {{TypeId{70000}, 0}, {TypeId{90001}, 255}};
    WriteArchive player;
    fields(player, p);
    const std::vector<std::uint8_t> tail{2, 0, 0, 0, 0,    0,    0, 0, 0x70, 0x11, 1, 0,
                                         0, 0, 0, 0, 0x91, 0x5f, 1, 0, 255,  0,    0, 0};
    REQUIRE(player.data().size() >= tail.size());
    CHECK(std::vector<std::uint8_t>(player.data().end() - static_cast<std::ptrdiff_t>(tail.size()),
                                    player.data().end()) == tail);
}
TEST_CASE("SCRIPT1 native restore refuses malformed continuation and unrelated binding") {
    auto spec = synthetic::basic_scenario(77);
    auto other = spec.players[0];
    other.id = PlayerId{1};
    spec.players.push_back(other);
    spec.script_context = ScriptContext::Pending;
    spec.script_source = text("SET X = TRUE;\nIF X THEN {}\n");
    auto made = World::create(spec, TypeRegistry::empty());
    REQUIRE(made);
    const auto base = made.value();
    for (int mutant = 0; mutant < 5; ++mutant) {
        auto w = base;
        auto& r = WorldTestAccess::script(w);
        switch (mutant) {
        case 0:
            r.state.signals[1] = 2;
            break;
        case 1:
            r.state.cursor = 1;
            break;
        case 2:
            r.state.bindings[0] = PlayerId{1};
            break;
        case 3:
            r.policy = 2;
            break;
        case 4:
            r.limits.depth = 129;
            break;
        }
        auto refused = World::load(w.save(), TypeRegistry::empty());
        REQUIRE_FALSE(refused);
        CHECK(refused.error().find("script") != std::string::npos);
    }
}
TEST_CASE("SCRIPT1 generated adapter contract validates an inert base before clearing mappings") {
    ScenarioSpec s;
    s.script_context = ScriptContext::Pending;
    s.script_source = "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n\"ABSENT_NEUTRAL\" "
                      "31\n[SCRIPT_KERNEL]\n[SIGNALS]\n[CAMARAPOS]\n0,0\n";
    // Generated adapters will create other players; unused mappings must never be rebound.
    REQUIRE(
        anno::sim_load::make_generated_script_free(s, *TypeRegistry::empty(), SimConfig{}).empty());
    CHECK(s.script_context == ScriptContext::ExplicitNone);
    CHECK(s.script_source.empty());
    for (const auto& kernel : {std::string("SET X=TRUE;"), std::string("INITBLOCK(){}"),
                               std::string("IF SCHUETTIINITSIG THEN {}")}) {
        ScenarioSpec bad;
        bad.script_context = ScriptContext::Pending;
        bad.script_source = text(kernel);
        const auto before = bad.script_source;
        CHECK_FALSE(
            anno::sim_load::make_generated_script_free(bad, *TypeRegistry::empty(), SimConfig{})
                .empty());
        CHECK(bad.script_source == before);
        CHECK(bad.script_context == ScriptContext::Pending);
    }
    for (const auto& source : {text("") + "[UNKNOWN]\n", std::string("malformed"),
                               std::string("[OBJECT_DEFINE]\nOBJECT_FIGUR \"X\" "
                                           "1\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n[SCRIPT_"
                                           "KERNEL]\n[SIGNALS]\n[CAMARAPOS]\n0,0\n")}) {
        ScenarioSpec bad;
        bad.script_context = ScriptContext::Pending;
        bad.script_source = source;
        CHECK_FALSE(
            anno::sim_load::make_generated_script_free(bad, *TypeRegistry::empty(), SimConfig{})
                .empty());
        CHECK(bad.script_source == source);
    }
    ScenarioSpec area;
    area.script_context = ScriptContext::Pending;
    area.script_source = text("");
    area.script_source.insert(area.script_source.find("[PLAYERMAPPER_DEFINE]"),
                              "\"FIELD\" 1,2,3,4\n");
    const auto before = area.script_source;
    CHECK_FALSE(
        anno::sim_load::make_generated_script_free(area, *TypeRegistry::empty(), SimConfig{})
            .empty());
    CHECK(area.script_source == before);
}
