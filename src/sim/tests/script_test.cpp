#include <array>
#include <utility>

#include <doctest/doctest.h>

#include <anno/sim/script.hpp>
using namespace anno::sim;
namespace {
std::string source(std::string kernel, std::string signals = {}, std::string mappings = {}) {
    return "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n" + mappings +
           "[SCRIPT_KERNEL]\n" + kernel + "\n[SIGNALS]\n" + signals + "[CAMARAPOS]\n0,0\n";
}
auto compile(std::string s, ScriptLimits limits = {}) {
    const std::array types{TypeId{90001}};
    return compile_script(std::move(s), {types, 128, 5}, limits);
}
struct Host final : ScriptHost {
    std::uint32_t value = 0, staged = 0, reads = 0;
    bool refuse = false, refuse_begin = false;
    std::uint32_t rollbacks = 0;
    bool begin() override {
        staged = value;
        return !refuse_begin;
    }
    void commit() override { value = staged; }
    void rollback() override {
        staged = value;
        ++rollbacks;
    }
    bool permission(std::uint32_t, TypeId, std::uint32_t v, bool, ScriptWork&) override {
        staged = v;
        return !refuse;
    }
    bool timer(ScriptAction, std::uint32_t, std::uint32_t v) override {
        staged = v;
        return !refuse;
    }
    std::uint32_t timer_seconds(std::uint32_t) const override { return staged; }
};
} // namespace
TEST_CASE("SCRIPT1 schedule is absolute signed wrapping quotient") {
    const std::array args{0u, 1u, 31u, 32u, 94u, 157u, 219u, 250u};
    const std::array expected{0u, 0u, 0u, 1u, 2u, 3u, 4u, 4u};
    for (std::size_t i = 0; i < args.size(); ++i)
        CHECK(script_visits(args[i], 4) == expected[i]);
    CHECK(script_visits(static_cast<std::uint32_t>(-126), 1) == 0);
    CHECK(script_visits(static_cast<std::uint32_t>(-375), 1) == 1);
    CHECK(script_visits(1073741793, 4) == 0);
}
TEST_CASE("SCRIPT1 grouping short circuit and read counts distinguish original") {
    auto p = compile(source("SET A = FALSE;\nSET B = TRUE;\nSET C = TRUE;\nSET X = FALSE;\n"
                            "IF A && B || C THEN { X = TRUE; } ELSE { C = FALSE; }\n"));
    REQUIRE_MESSAGE(p, (p ? "" : p.error().message));
    auto s = make_script_state(p.value());
    Host h;
    REQUIRE(visit_script(p.value(), s, 250, h));
    CHECK(s.signals[4] == 0);
    CHECK(s.signals[3] == 1);
    CHECK(s.last_work == 3);
    auto q = compile(source("SET A = TRUE;\nSET B = TRUE;\nIF A || B THEN {}\n"));
    REQUIRE(q);
    auto t = make_script_state(q.value());
    REQUIRE(visit_script(q.value(), t, 250, h));
    CHECK(t.last_work == 4);
    auto r = compile(source("SET A = TRUE;\nSET B = TRUE;\nIF A && B THEN {}\n"));
    REQUIRE(r);
    auto u = make_script_state(r.value());
    REQUIRE(visit_script(r.value(), u, 250, h));
    CHECK(u.last_work == 4);
}
TEST_CASE("SCRIPT1 INIT restoration cursor and same visit writes") {
    auto p = compile(source("SET X = FALSE;\nSET SEEN = TRUE;\nINITBLOCK(){ X = TRUE; }\n"
                            "IF X THEN { X = FALSE; }\nIF X THEN {} ELSE { SEEN = FALSE; }\n"));
    REQUIRE(p);
    auto s = make_script_state(p.value());
    Host h;
    REQUIRE(visit_script(p.value(), s, 0, h));
    CHECK(s.signals[1] == 1);
    REQUIRE(visit_script(p.value(), s, 1, h));
    CHECK(s.signals[1] == 1);
    REQUIRE(visit_script(p.value(), s, 63, h));
    CHECK(s.cursor == 1);
    CHECK(s.signals[1] == 0);
    auto saved = s;
    REQUIRE(visit_script(p.value(), s, 63, h));
    REQUIRE(visit_script(p.value(), saved, 63, h));
    CHECK(s.signals == saved.signals);
    CHECK(s.cursor == 0);
    CHECK(s.signals[2] == 0);
    auto restored =
        compile(source("SET X = FALSE;\nINITBLOCK(){ X = TRUE; }\n", "SCHUETTIINITSIG = FALSE\n"));
    REQUIRE(restored);
    auto t = make_script_state(restored.value());
    REQUIRE(visit_script(restored.value(), t, 0, h));
    CHECK(t.signals[1] == 0);
}
TEST_CASE("SCRIPT1 compiler refuses whole unsupported and forward symbols") {
    auto p = compile(
        source("INITBLOCK(){ SETCONSTRUCTION_DRAWSTATE(\"ME\",90001,TRUE); PLAYWAVE(\"x\"); }", {},
               "\"ME\" 0\n"));
    REQUIRE_FALSE(p);
    CHECK(p.error().message == "unsupported action: PLAYWAVE");
    CHECK_FALSE(compile(source("IF LATER THEN {}\nSET LATER = FALSE;\n")));
    CHECK_FALSE(compile(source("SET Foo = TRUE;\nIF FOO THEN {}\n")));
    CHECK(compile(source("SET Foo = TRUE;\nSET FOO = FALSE;\nIF Foo THEN {}\n")));
    CHECK(compile(source("INITBLOCK(){ SETCONSTRUCTION_DRAWSTATE(\"foo@x\",90001,TRUE); }", {},
                         "\"Foo\" 0\n")));
    CHECK_FALSE(compile(source("INITBLOCK(){ SETCONSTRUCTION_DRAWSTATE(\"foo@x\",90001,TRUE); }",
                               {}, "\"Foo@x\" 0\n")));
}
TEST_CASE("SCRIPT1 budget and host faults roll back entire visit") {
    auto p = compile(source("SET X = FALSE;\nINITBLOCK(){ X = TRUE; TIMER_SET(1,7); X = FALSE; }"),
                     {.work = 2});
    REQUIRE(p);
    auto s = make_script_state(p.value());
    Host h;
    REQUIRE_FALSE(visit_script(p.value(), s, 0, h));
    CHECK(s.context == ScriptContext::Fault);
    CHECK(s.fault == ScriptFault::Budget);
    CHECK(s.signals[1] == 0);
    CHECK(s.signals[0] == 1);
    CHECK(h.value == 0);
    REQUIRE_FALSE(visit_script(p.value(), s, 250, h));
    CHECK(h.value == 0);
    auto q = compile(source("INITBLOCK(){ TIMER_SET(1,7); }"));
    REQUIRE(q);
    auto t = make_script_state(q.value());
    h.refuse = true;
    REQUIRE_FALSE(visit_script(q.value(), t, 0, h));
    CHECK(t.fault == ScriptFault::Host);
    CHECK(h.value == 0);
}
TEST_CASE("SCRIPT1 read on losses and timer query visibility") {
    auto p = compile(source("SET X = FALSE;\nINITBLOCK(){\nX = TRUE\nX = "
                            "FALSE;\nTIMER_SET(1,7);\n}\nIF TIMER_1 == 7 THEN { X = FALSE; }\n"));
    REQUIRE(p);
    auto s = make_script_state(p.value());
    Host h;
    REQUIRE(visit_script(p.value(), s, 0, h));
    CHECK(s.signals[1] == 1);
    CHECK(h.value == 7);
    REQUIRE(visit_script(p.value(), s, 250, h));
    CHECK(s.signals[1] == 0);
}
TEST_CASE("SCRIPT1 duplicate kernels cannot hide unsupported dependencies") {
    auto text = source("INITBLOCK(){ PLAYWAVE(\"x\"); }\n");
    text.insert(text.find("[SIGNALS]"), "[SCRIPT_KERNEL]\n");
    auto p = compile(text);
    REQUIRE_FALSE(p);
    CHECK(p.error().message == "duplicate script section: [SCRIPT_KERNEL]");
    CHECK_FALSE(compile("[SCRIPT_KERNEL]\n"));
}
TEST_CASE("SCRIPT1 grammar folds keywords but builtins precede exact signals") {
    auto p =
        compile(source("set Foo = false;\nset PLAYTIME = TRUE;\n"
                       "initblock(){ Foo = true; }\nif PLAYTIME == 0 then { Foo = false; }\n"));
    REQUIRE_MESSAGE(p, (p ? "" : p.error().message));
    auto s = make_script_state(p.value());
    Host h;
    REQUIRE(visit_script(p.value(), s, 250, h));
    CHECK(s.signals[1] == 0);
    CHECK(s.signals[2] == 1);
}

TEST_CASE("SCRIPT1 host refusal on last unit is Host and failed begin rolls back") {
    auto p = compile(
        source("INITBLOCK(){ SETCONSTRUCTION_DRAWSTATE(\"ME\",90001,TRUE); }", {}, "\"ME\" 0\n"),
        {.work = 1});
    REQUIRE(p);
    auto s = make_script_state(p.value());
    s.bindings = {PlayerId{0}};
    Host h;
    h.refuse = true;
    REQUIRE_FALSE(visit_script(p.value(), s, 0, h));
    CHECK(s.fault == ScriptFault::Host);
    CHECK(h.value == 0);
    CHECK(h.rollbacks == 1);
    s = make_script_state(p.value());
    s.bindings = {PlayerId{0}};
    h.refuse_begin = true;
    REQUIRE_FALSE(visit_script(p.value(), s, 77, h));
    CHECK(s.fault == ScriptFault::Host);
    CHECK(s.last_arg == 77);
    CHECK(h.rollbacks == 2);
}

TEST_CASE("SCRIPT1 timer-like signal names remain exact symbols") {
    auto p = compile(source("SET TIMER_FLAG = TRUE;\nSET TIMER_01 = TRUE;\nSET X = FALSE;\nIF "
                            "TIMER_FLAG && TIMER_01 THEN { X = TRUE; }\n"));
    REQUIRE(p);
    auto s = make_script_state(p.value());
    Host h;
    REQUIRE(visit_script(p.value(), s, 250, h));
    CHECK(s.signals[3] == 1);
}
