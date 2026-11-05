#include <limits>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/state_fields.hpp>

#include "script_fixture.hpp"
#include "world_access.hpp"
using namespace anno::sim;
namespace {
std::string money_source(std::string body) {
    return "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n\"ME\" 0\n\"OTHER\" "
           "1\n\"ALIAS\" 0\n[SCRIPT_KERNEL]\nINITBLOCK(){" +
           body + "}\n[SIGNALS]\n[CAMARAPOS]\n0,0\n";
}
auto money_world(std::string body, std::int64_t a = 100, std::int64_t b = 200,
                 ScriptLimits limits = {}) {
    auto spec = synthetic::basic_scenario(77);
    spec.players[0].money = a;
    auto other = spec.players[0];
    other.id = PlayerId{1};
    other.money = b;
    spec.players.push_back(other);
    spec.script_context = ScriptContext::Pending;
    spec.script_source = money_source(body);
    spec.script_limits = limits;
    auto w = World::create(spec, TypeRegistry::empty());
    REQUIRE(w);
    return std::move(w).value();
}
std::vector<std::uint8_t> player_bytes(Player p) {
    WriteArchive a;
    fields(a, p);
    return a.take();
}
} // namespace
TEST_CASE("SCRIPT2 MONEY_ADD exact recipient signed ledger and unrelated fields") {
    auto w = money_world("MONEY_ADD(\"ME\",5); MONEY_ADD(\"OTHER\",-2); MONEY_ADD(\"ME\",0); "
                         "MONEY_ADD(\"ME\",-1); MONEY_ADD(\"aLiAs@suffix\",5);");
    auto p0 = *w.players().find(PlayerId{0}), p1 = *w.players().find(PlayerId{1});
    const auto rng = w.rng().cursor();
    REQUIRE(w.initialize_scripts().empty());
    CHECK(w.players().find(PlayerId{0})->money == 109);
    CHECK(w.players().find(PlayerId{1})->money == 198);
    p0.money = 109;
    p1.money = 198;
    CHECK(player_bytes(p0) == player_bytes(*w.players().find(PlayerId{0})));
    CHECK(player_bytes(p1) == player_bytes(*w.players().find(PlayerId{1})));
    CHECK(w.rng().cursor() == rng);
    const auto saved = w.save();
    REQUIRE(w.initialize_scripts().empty());
    CHECK(w.save() == saved);
    auto back = World::load(saved, TypeRegistry::empty());
    REQUIRE(back);
    REQUIRE(back.value().initialize_scripts().empty());
    CHECK(back.value().save() == saved);
}
TEST_CASE("SCRIPT2 MONEY_ADD i32 operand and established i64 saturating boundaries") {
    struct V {
        std::int64_t initial;
        std::int32_t delta;
        std::int64_t expected;
    };
    for (auto v : {V{10000, 99999, 109999}, V{0, -1, -1}, V{2147483647, 1, 2147483648LL},
                   V{INT64_MAX, 1, INT64_MAX}, V{INT64_MIN, -1, INT64_MIN},
                   V{0, INT32_MIN, INT32_MIN}, V{0, INT32_MAX, INT32_MAX}}) {
        auto w = money_world("MONEY_ADD(\"ME\"," + std::to_string(v.delta) + ");", v.initial);
        REQUIRE(w.initialize_scripts().empty());
        CHECK(w.players().find(PlayerId{0})->money == v.expected);
        CHECK(w.players().find(PlayerId{1})->money == 200);
    }
    for (auto value : {"2147483648", "-2147483649", "TRUE", "\"1\""})
        CHECK_FALSE(compile_script(money_source("MONEY_ADD(\"ME\"," + std::string(value) + ");"),
                                   {{}, 32, 5}));
    CHECK_FALSE(compile_script(money_source("MONEY_ADD(\"ABSENT\",1);"), {{}, 32, 5}));
}
TEST_CASE("SCRIPT2 MONEY_ADD failed visit never leaks credit or executes later timer writes") {
    auto w = money_world("MONEY_ADD(\"ME\",5); MONEY_ADD(\"OTHER\",7); TIMER_SET(1,7);", 100, 200,
                         {.work = 2});
    auto before0 = player_bytes(*w.players().find(PlayerId{0}));
    auto before1 = player_bytes(*w.players().find(PlayerId{1}));
    const auto timers = w.timers();
    CHECK_FALSE(w.initialize_scripts().empty());
    CHECK(w.script().state.fault == ScriptFault::Budget);
    CHECK(player_bytes(*w.players().find(PlayerId{0})) == before0);
    CHECK(player_bytes(*w.players().find(PlayerId{1})) == before1);
    CHECK(w.timers().counter == timers.counter);
    CHECK(w.timers().active == timers.active);
    CHECK(w.clock().ms == 0);
    auto loaded = World::load(w.save(), TypeRegistry::empty());
    REQUIRE(loaded);
    CHECK(loaded.value().save() == w.save());
}
TEST_CASE("SCRIPT2 MONEY_ADD and permission aliases preserve transaction order and commit once") {
    auto fixture = script_test::make(
        false,
        "INITBLOCK(){ MONEY_ADD(\"ME\",5); SETCONSTRUCTION_DRAWSTATE(\"ME\",\"90001\",TRUE); "
        "MONEY_ADD(\"ME\",-2); MONEY_ADD(\"OTHER\",7); }");
    auto control = script_test::make(
        false, "INITBLOCK(){ SETCONSTRUCTION_DRAWSTATE(\"ME\",\"90001\",TRUE); }");
    REQUIRE(fixture.world.initialize_scripts().empty());
    REQUIRE(control.world.initialize_scripts().empty());
    auto expected0 = *control.world.players().find(PlayerId{0});
    expected0.money += 3;
    auto expected1 = *control.world.players().find(PlayerId{1});
    expected1.money += 7;
    CHECK(player_bytes(expected0) == player_bytes(*fixture.world.players().find(PlayerId{0})));
    CHECK(player_bytes(expected1) == player_bytes(*fixture.world.players().find(PlayerId{1})));
}
TEST_CASE("SCRIPT2 MONEY_ADD sequential saturation is not saturation of a combined delta") {
    auto w = money_world("MONEY_ADD(\"ME\",2); MONEY_ADD(\"ME\",-1);", INT64_MAX - 1);
    REQUIRE(w.initialize_scripts().empty());
    CHECK(w.players().find(PlayerId{0})->money == INT64_MAX - 1);
}
TEST_CASE("SCRIPT2 MONEY_ADD query Host refusal rolls back prior credit") {
    auto w = money_world("");
    auto source = money_source("MONEY_ADD(\"ME\",5); TIMER_SET(1,7);");
    source.insert(source.find("[PLAYERMAPPER_DEFINE]"), "\"MOD_AREA\" 0,0,16,16\n");
    source.insert(source.find("[SIGNALS]"),
                  "IF AREA \"MOD_AREA\" TOUCHED_BY_SHIP_FROM \"ME\" THEN {}\n");
    auto r = prepare_script(source, *TypeRegistry::empty(), w.players(), w.config());
    REQUIRE(r);
    auto players = w.players();
    auto timers = w.timers();
    auto before = player_bytes(*players.find(PlayerId{0}));
    REQUIRE_FALSE(advance_script(r.value(), players, timers, *TypeRegistry::empty(), 250));
    CHECK(r.value().state.fault == ScriptFault::Host);
    CHECK(player_bytes(*players.find(PlayerId{0})) == before);
    CHECK(timers.counter == w.timers().counter);
    CHECK(timers.active == w.timers().active);
}
