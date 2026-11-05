#include <doctest/doctest.h>

#include <anno/sim/scenario.hpp>
#include <anno/sim/script_runtime.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"
using namespace anno::sim;
namespace {
std::string area_source(std::string rect = "30,10,20,40", std::string query = "SHIP") {
    return "[OBJECT_DEFINE]\n[AREA_DEFINE]\n\"PATCH\" " + rect +
           "\n[PLAYERMAPPER_DEFINE]\n\"ME\" 0\n[SCRIPT_KERNEL]\nSET HIT=FALSE;\n"
           "IF AREA \"PATCH\" TOUCHED_BY_" +
           query +
           "_FROM \"ME\" THEN { HIT=TRUE; }\n"
           "[SIGNALS]\n[CAMARAPOS]\n0,0\n";
}
auto figure_types() {
    TypeRegistry::Builder b;
    for (auto k : {97u, 107u, 108u, 112u, 113u, 128u, 129u}) {
        FigureType t;
        t.id = k + 70000; // classification is data, never the engine id
        t.bewohntype = k;
        b.figure_type(t);
    }
    auto r = b.build();
    REQUIRE(r);
    return r.value();
}
struct AreaFixture {
    std::shared_ptr<const TypeRegistry> types = figure_types();
    SimConfig cfg;
    PlayerTable players = make_player_table(cfg);
    ScriptTimers timers = make_script_timers(cfg);
    FigureRegistry figures = make_figure_registry(cfg);
    WorldSettings settings;
    AreaFixture() {
        players.slots[0] = make_player(PlayerId{0}, cfg);
        settings.world_width = settings.world_height = 1024;
    }
    ScriptRuntime runtime(std::string source = area_source(), ScriptLimits limits = {}) {
        auto r = prepare_script(source, *types, players, cfg, limits);
        REQUIRE_MESSAGE(r, (r ? "" : r.error()));
        return std::move(r).value();
    }
    void figure(std::uint32_t k = 113, FixedPos pos = {2560, 7680}, PlayerId owner = PlayerId{0},
                std::uint32_t state = 0) {
        Figure f;
        f.id = FigureId{figures.by_id.size() + 1};
        f.kind = k + 70000;
        f.pos = pos;
        f.owner = owner;
        f.state_word = state;
        REQUIRE(add_figure(figures, f, cfg, Rules{}) != kNoSlot);
    }
    bool visit(ScriptRuntime& r) {
        return advance_script(r, players, timers, *types, 250, &figures, &settings);
    }
};
} // namespace
TEST_CASE("SCRIPT2 area half open point owner classification and state vectors") {
    for (auto pos : {FixedPos{2560, 7680},
                     {5119, 10239},
                     {2559, 7680},
                     {5120, 7680},
                     {2560, 7679},
                     {2560, 10240}}) {
        AreaFixture f;
        f.figure(113, pos);
        auto r = f.runtime();
        REQUIRE(f.visit(r));
        CHECK(bool(r.state.signals[1]) ==
              (pos == FixedPos{2560, 7680} || pos == FixedPos{5119, 10239}));
    }
    for (auto kind : {97u, 107u, 108u, 112u, 113u, 128u, 129u})
        for (bool ship : {false, true}) {
            AreaFixture f;
            f.figure(kind);
            auto r = f.runtime(area_source("30,10,20,40", ship ? "SHIP" : "SOLDIER"));
            REQUIRE(f.visit(r));
            CHECK(bool(r.state.signals[1]) ==
                  (ship ? kind >= 113 && kind <= 128 : kind == 97 || kind == 107));
        }
    for (auto state : {0u, 4u, 8192u, 128u}) {
        AreaFixture f;
        f.figure(113, {2560, 7680}, PlayerId{0}, state);
        auto r = f.runtime();
        REQUIRE(f.visit(r));
        CHECK(bool(r.state.signals[1]) == (state == 0 || state == 128));
    }
    AreaFixture f;
    f.figure(113, {2560, 7680}, PlayerId{1});
    auto r = f.runtime();
    REQUIRE(f.visit(r));
    CHECK(r.state.signals[1] == 0); // HUMAN0 is exact owner, never wildcard
}
TEST_CASE("SCRIPT2 original sector wrap partial world and complete candidate controls") {
    for (auto width : {1u, 17u, 32u}) {
        AreaFixture f;
        f.settings.world_width = f.settings.world_height = width;
        f.figure(113, {width == 1 ? 0 : 4096, width == 1 ? 0 : 4096});
        auto r = f.runtime(area_source(width == 1 ? "0,0,1,1" : "16,16,17,17"));
        REQUIRE(f.visit(r));
        CHECK(bool(r.state.signals[1]) == (width == 32));
    }
    for (auto end : {2147483647, 2147483632}) {
        AreaFixture f;
        f.settings.world_width = f.settings.world_height = 32;
        f.figure(113, {0, 0});
        auto r = f.runtime(area_source("0,0," + std::to_string(end) + ",32"));
        REQUIRE(f.visit(r));
        CHECK(bool(r.state.signals[1]) == (end == 2147483632));
    }
    AreaFixture f;
    for (int i = 0; i < 512; ++i)
        f.figure(113, {2560, 7680}, PlayerId{1});
    f.figure();
    auto r = f.runtime();
    REQUIRE(f.visit(r));
    CHECK(r.state.signals[1] == 1);
    CHECK(r.query_stats.candidates == 513);
    CHECK(r.query_stats.inserted == 513);
    CHECK(r.query_stats.slots == f.figures.list.slots.size());
}
TEST_CASE("SCRIPT2 area movement removal carried crew and derived rebuild") {
    AreaFixture f;
    f.figure(108);
    auto r = f.runtime(area_source("30,10,20,40", "SOLDIER"));
    REQUIRE(f.visit(r));
    CHECK(r.state.signals[1] == 0);
    f.figure(97);
    auto* child = find_figure(f.figures, FigureId{2});
    child->parent = FigureId{1};
    child->crew_slot = 3;
    r = f.runtime(area_source("30,10,20,40", "SOLDIER"));
    REQUIRE(f.visit(r));
    CHECK(r.state.signals[1] == 1);
    auto* parent = find_figure(f.figures, FigureId{1});
    parent->pos.x = 5120;
    r = f.runtime(area_source("30,10,20,40", "SOLDIER"));
    REQUIRE(f.visit(r));
    CHECK(r.state.signals[1] == 0);
    parent->pos.x = 2560;
    r = f.runtime(area_source("30,10,20,40", "SOLDIER"));
    REQUIRE(f.visit(r));
    CHECK(r.state.signals[1] == 1);
    parent->expiry_stamp = 0;
    r = f.runtime(area_source("30,10,20,40", "SOLDIER"));
    REQUIRE(f.visit(r));
    CHECK(r.state.signals[1] == 0);
}
TEST_CASE("SCRIPT2 area charged build once per visit and rollback on budget") {
    AreaFixture f;
    for (int i = 0; i < 1000; ++i)
        f.figure(129, {200000, 200000});
    f.figure();
    auto s = area_source();
    const auto at = s.find("[SIGNALS]");
    s.insert(at, "IF AREA \"PATCH\" TOUCHED_BY_SHIP_FROM \"ME\" THEN {}\n");
    auto r = f.runtime(s);
    REQUIRE(f.visit(r));
    CHECK(r.query_stats.slots == f.figures.list.slots.size());
    CHECK(r.query_stats.candidates == 2);
    CHECK(r.query_stats.inserted == 1);
    auto limited = f.runtime(s, {.work = 50});
    REQUIRE_FALSE(f.visit(limited));
    CHECK(limited.state.fault == ScriptFault::Budget);
    CHECK(limited.state.signals[1] == 0);
    CHECK(limited.state.cursor == 0);
    CHECK(limited.state.last_work == 50);
}
TEST_CASE("SCRIPT2 area native reload reconstructs view and bindings") {
    auto types = figure_types();
    auto spec = synthetic::basic_scenario(77);
    spec.settings.world_width = spec.settings.world_height = 1024;
    spec.script_context = ScriptContext::Pending;
    spec.script_source = area_source();
    ScenarioSpec::FigureSpec f;
    f.kind = 70113;
    f.owner = PlayerId{0};
    f.pos = {2560, 7680};
    spec.figures.push_back(f);
    auto made = World::create(spec, types);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    auto w = std::move(made).value();
    auto pending = World::load(w.save(), types);
    REQUIRE(pending);
    REQUIRE(w.initialize_scripts().empty());
    REQUIRE(pending.value().initialize_scripts().empty());
    w.advance(500);
    pending.value().advance(500);
    auto before = World::load(w.save(), types);
    REQUIRE(before);
    w.advance(0);
    before.value().advance(0);
    pending.value().advance(0);
    CHECK(w.script().state.signals[1] == 1);
    CHECK(w.save() == before.value().save());
    CHECK(w.save() == pending.value().save());
    CHECK(w.script().query_stats.inserted == 1);
    auto after = World::load(w.save(), types);
    REQUIRE(after);
    CHECK(after.value().script().query_stats.inserted == 0);
    w.advance(0);
    after.value().advance(0);
    CHECK(w.save() == after.value().save());
}
TEST_CASE("SCRIPT2 area validation and source order bindings remain strict") {
    auto s = area_source();
    CHECK(compile_script(s, {{}, 32, 5}));
    auto bad = s;
    bad.replace(bad.find("TOUCHED_BY_SHIP_FROM"), 20, "TOUCHED_BY_OTHER_FROM");
    CHECK_FALSE(compile_script(bad, {{}, 32, 5}));
    bad = s;
    bad.replace(bad.find("AREA \"PATCH\""), 12, "AREA \"OTHER\"");
    CHECK_FALSE(compile_script(bad, {{}, 32, 5}));
    bad = s;
    bad.replace(bad.find("30,10,20,40"), 11, "30,10,2147483648,40");
    CHECK_FALSE(compile_script(bad, {{}, 32, 5}));
}

TEST_CASE("SCRIPT2 area plain mapper casefold exact owner and cargo qualifier refusal") {
    AreaFixture f;
    f.figure(113, {2560, 7680}, PlayerId{1});
    f.players.slots[1] = make_player(PlayerId{1}, f.cfg);
    auto source = area_source();
    source.insert(source.find("[SCRIPT_KERNEL]"), "\"OTHER\" 1\n");
    source.replace(source.find("FROM \"ME\""), 9, "FROM \"oThEr\"");
    auto r = f.runtime(source);
    REQUIRE(f.visit(r));
    CHECK(r.state.signals[1] == 1);
    for (const auto name : {"OTHER@WOOD", "ME@anything", "UNKNOWN@WOOD"}) {
        auto bad = source;
        bad.replace(bad.find("oThEr"), 5, name);
        auto refused = compile_script(bad, {{}, 32, 5});
        REQUIRE_FALSE(refused);
        CHECK(refused.error().message == "unsupported area cargo qualifier");
    }
}
TEST_CASE("SCRIPT2 Ready runtime observes changed authoritative membership without touch history") {
    AreaFixture f;
    f.figure(108);
    auto source = area_source("30,10,20,40", "SOLDIER");
    source.insert(source.find("[SIGNALS]"), "");
    source.replace(source.find("HIT=TRUE; }"), 11, "HIT=TRUE; } ELSE { HIT=FALSE; }");
    auto r = f.runtime(source);
    auto check = [&](bool expected) {
        REQUIRE(f.visit(r));
        CHECK(bool(r.state.signals[1]) == expected);
    };
    check(false);
    f.figure(97);
    auto* child = find_figure(f.figures, FigureId{2});
    child->parent = FigureId{1};
    child->crew_slot = 7;
    check(true);
    auto* parent = find_figure(f.figures, FigureId{1});
    parent->pos.x = 5120;
    check(false);
    parent->pos.x = 2560;
    check(true);
    parent->owner = PlayerId{1};
    check(false);
    parent->owner = PlayerId{0};
    check(true);
    child->crew_slot.reset();
    check(false);
    child->crew_slot = 7;
    check(true);
    parent->parent = FigureId{2};
    check(false);
    parent->parent = FigureId{};
    check(true);
    parent->expiry_stamp = 0;
    check(false);
    parent->expiry_stamp.reset();
    check(true);
    remove_figure(f.figures, FigureId{1});
    check(false);
}
TEST_CASE("SCRIPT2 false AND area guard does not construct an unrelated large index") {
    AreaFixture f;
    for (int i = 0; i < 1000; ++i)
        f.figure(113);
    auto source = area_source();
    source.insert(source.find("IF AREA"), "SET NEVER=FALSE;\n");
    source.replace(source.find("IF AREA"), 7, "IF NEVER && AREA");
    auto r = f.runtime(source, {.work = 10});
    REQUIRE(f.visit(r));
    CHECK(r.state.signals[1] == 0);
    CHECK(r.query_stats.slots == 0);
    CHECK(r.query_stats.inserted == 0);
    CHECK(r.query_stats.candidates == 0);
    CHECK(r.state.fault == ScriptFault::None);
}

TEST_CASE("SCRIPT2 missing spatial context faults Host and rolls back prior writes") {
    AreaFixture f;
    auto source = area_source();
    source.insert(source.find("IF AREA"), "INITBLOCK(){ HIT=TRUE; TIMER_SET(1,7); }\n");
    auto r = f.runtime(source);
    const auto before = f.timers;
    const auto money = f.players.find(PlayerId{0})->money;
    REQUIRE_FALSE(advance_script(r, f.players, f.timers, *f.types, 250));
    CHECK(r.state.fault == ScriptFault::Host);
    CHECK(r.state.signals[1] == 0);
    CHECK(r.state.cursor == 0);
    CHECK(f.timers.counter == before.counter);
    CHECK(f.timers.active == before.active);
    CHECK(f.players.find(PlayerId{0})->money == money);
}

TEST_CASE("SCRIPT2 plain goods name collision refuses cargo semantics with registry context") {
    const std::vector<std::string_view> names{"UNUSED", "HOLZ", "MOD_GOODS"};
    for (auto name : {"HOLZ", "hOlZ", "MOD_GOODS"}) {
        auto source = area_source();
        source.replace(source.find("\"ME\""), 4, "\"" + std::string(name) + "\"");
        source.replace(source.find("FROM \"ME\""), 9, "FROM \"" + std::string(name) + "\"");
        auto r = compile_script(source, {{}, 32, 5, names});
        REQUIRE_FALSE(r);
        CHECK(r.error().message == "unsupported area cargo qualifier");
    }
    CHECK(compile_script(area_source(), {{}, 32, 5, names}));
    // The authoritative World compiler receives its actual data-defined goods table.
    auto spec = synthetic::basic_scenario(77);
    spec.script_context = ScriptContext::Pending;
    spec.script_source = area_source();
    auto& source = spec.script_source;
    source.replace(source.find("\"ME\""), 4, "\"HOLZ\"");
    source.replace(source.find("FROM \"ME\""), 9, "FROM \"HOLZ\"");
    auto made = World::create(spec, TypeRegistry::empty());
    REQUIRE_FALSE(made);
    CHECK(made.error().find("unsupported area cargo qualifier") != std::string::npos);
}
