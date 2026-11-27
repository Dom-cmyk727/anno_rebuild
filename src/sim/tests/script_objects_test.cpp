#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include <doctest/doctest.h>

#include <anno/formats/bigbrother.hpp>
#include <anno/sim/city_creation.hpp>
#include <anno/sim/script_runtime.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

#include "script_fixture.hpp"
#include "world_access.hpp"
using namespace anno::sim;
namespace {
std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> result;
    std::istringstream in(text);
    for (std::string part; std::getline(in, part, delimiter);)
        result.push_back(part);
    return result;
}
std::string object_source(const std::string& definitions, const std::string& kernel) {
    auto s =
        std::string("[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n[SCRIPT_KERNEL]\n") +
        kernel + "\n[SIGNALS]\n[CAMARAPOS]\n0,0\n";
    s.insert(s.find("[AREA_DEFINE]"), definitions);
    return s;
}
std::string query_kernel(const std::string& name) {
    return "SET SEEN = FALSE;\nIF OBJECT \"" + name +
           "\" KILLED THEN { SEEN = TRUE; } ELSE { SEEN = FALSE; }\n";
}
void frame(std::vector<std::optional<Island>>& islands, const std::string& text) {
    for (auto& island : islands)
        if (island)
            for (auto& city : island->cities)
                city.reset();
    if (text == "-")
        return;
    for (const auto& item : split(text, ',')) {
        const auto fields = split(item, ':');
        REQUIRE(fields.size() == 4);
        auto& city = islands.at(std::stoul(fields[0]))->cities.at(std::stoul(fields[1]));
        city.emplace();
        city->number = CityNo{static_cast<std::uint32_t>(std::stoul(fields[1]))};
        city->owner = PlayerId{static_cast<std::uint32_t>(std::stoul(fields[2]))};
        // The oracle's building count is a distractor to nullness. Give the city an
        // actual occupied building list when nonzero; the query must never inspect it.
        const auto count = std::stoul(fields[3]);
        city->index.lists.resize(1);
        city->index.lists[0].slots.resize(count);
        for (auto& building : city->index.lists[0].slots)
            building.emplace();
    }
}
} // namespace
TEST_CASE("SO1b all accepted city object vectors execute through the C++ runtime") {
    std::ifstream in(std::string(ANNO_FIXTURES_DIR) + "/sim/script_objects_vectors.txt");
    REQUIRE(in.good());
    std::size_t vectors = 0;
    for (std::string line; std::getline(in, line);) {
        if (line.empty() || line.front() == '#')
            continue;
        std::istringstream row(line);
        std::string kind, label, token;
        row >> kind >> label;
        REQUIRE(kind == "query");
        INFO(label);
        std::map<std::string, std::string> input, expected;
        auto* fields = &input;
        while (row >> token) {
            if (token == "=>") {
                fields = &expected;
                continue;
            }
            const auto equals = token.find('=');
            REQUIRE(equals != std::string::npos);
            (*fields)[token.substr(0, equals)] = token.substr(equals + 1);
        }
        std::string definitions;
        for (const auto& item : split(input.at("objects"), ',')) {
            const auto pair = split(item, ':');
            REQUIRE(pair.size() == 2);
            definitions += "OBJECT_STADT \"" + pair[0] + "\" " + pair[1] + "\n";
        }
        auto source = object_source(definitions, query_kernel(input.at("ref")));
        auto types = TypeRegistry::empty();
        PlayerTable players;
        SimConfig cfg;
        // No world is supplied to the compiler: even the initial snapshot is installed later.
        auto runtime = prepare_script(source, *types, players, cfg);
        CHECK(static_cast<bool>(runtime) == (expected.at("compiled") == "1"));
        ++vectors;
        if (expected.at("compiled") == "0") {
            REQUIRE_FALSE(runtime);
            CHECK(runtime.error().find("unknown city object") != std::string::npos);
            CHECK(expected.at("city_reads") == "0");
            continue;
        }
        REQUIRE(runtime);
        const auto& expression = runtime.value().program->expressions.front();
        const auto& object = runtime.value().program->city_objects[expression.index];
        CHECK(object.island.v == std::stoul(expected.at("island")));
        CHECK(object.city.v == std::stoul(expected.at("city")));
        CHECK(expected.at("parse_reads") == "0");
        CHECK(expected.at("writes") == "0");
        std::vector<std::optional<Island>> islands;
        for (const auto& id : split(input.at("islands"), ',')) {
            const auto slot = static_cast<std::size_t>(std::stoul(id));
            islands.resize(std::max(islands.size(), slot + 1));
            islands[slot].emplace();
            islands[slot]->cities.resize(std::stoul(input.at("city_slots")));
        }
        frame(islands, input.at("initial"));
        const auto frames = split(input.at("frames"), '/');
        const auto killed = split(expected.at("killed"), ',');
        REQUIRE(frames.size() == killed.size());
        CHECK(frames.size() == std::stoul(expected.at("city_reads")));
        ScriptTimers timers;
        for (std::size_t k = 0; k < frames.size(); ++k) {
            frame(islands, frames[k]);
            REQUIRE(advance_script(runtime.value(), players, timers, *types, 250, nullptr, nullptr,
                                   nullptr, 0, islands));
            CHECK(runtime.value().state.signals.at(1) == std::stoul(killed[k]));
        }
    }
    CHECK(vectors == 13);
}
TEST_CASE("SO1b invalid city domains refuse live and roll back the entire visit") {
    for (const auto packed : {3, 4096}) {
        auto types = script_test::make().types;
        auto spec = command_test::two_city_spec(types);
        spec.script_context = ScriptContext::Pending;
        spec.script_source =
            object_source("OBJECT_STADT \"A\" " + std::to_string(packed) + "\n",
                          "SET SEEN = FALSE;\nINITBLOCK(){ MONEY_ADD(\"ME\",17); SEEN = TRUE; }\n"
                          "IF OBJECT \"A\" KILLED THEN {}\n");
        const std::string mapping_section = "[PLAYERMAPPER_DEFINE]\n";
        spec.script_source.insert(spec.script_source.find(mapping_section) + mapping_section.size(),
                                  "\"ME\" 0\n");
        // Use the normal world's configured ring and transaction host, not a mock.
        auto made = World::create(spec, types.registry);
        REQUIRE_MESSAGE(made, (made ? "" : made.error()));
        auto w = std::move(made).value();
        auto& runtime = WorldTestAccess::script(w);
        const auto money = w.players().find(PlayerId{0})->money;
        const auto rng = WorldTestAccess::rng(w).cursor();
        CHECK_FALSE(advance_script(runtime, WorldTestAccess::players(w), WorldTestAccess::timers(w),
                                   *types.registry, 250, &w.figures(), &w.settings(), nullptr, 0,
                                   w.islands()));
        CHECK(runtime.state.context == ScriptContext::Fault);
        CHECK(runtime.state.fault == ScriptFault::Host);
        CHECK(runtime.state.signals[0] == 1);
        CHECK(runtime.state.signals[1] == 0);
        CHECK(w.players().find(PlayerId{0})->money == money);
        CHECK(WorldTestAccess::rng(w).cursor() == rng);
    }
}
TEST_CASE("SO1b native reload rebuilds city definitions and observes a later city creation") {
    auto types = script_test::make().types;
    auto spec = command_test::two_city_spec(types);
    spec.script_context = ScriptContext::Pending;
    ScenarioSpec::PlayerSpec third;
    third.id = PlayerId{2};
    spec.players.push_back(third);
    spec.script_source = object_source("OBJECT_STADT \"A\" 512\n", query_kernel("a"));
    auto made = World::create(spec, types.registry);
    REQUIRE(made);
    auto w = std::move(made).value();
    const auto evaluate = [&](World& world) {
        const auto rng = WorldTestAccess::rng(world).cursor();
        CHECK(advance_script(WorldTestAccess::script(world), WorldTestAccess::players(world),
                             WorldTestAccess::timers(world), *types.registry, 250, &world.figures(),
                             &world.settings(), nullptr, 0, world.islands()));
        CHECK(WorldTestAccess::rng(world).cursor() == rng);
        return world.script().state.signals.at(1);
    };
    CHECK(evaluate(w) == 1);
    auto restored = World::load(w.save(), types.registry);
    REQUIRE_MESSAGE(restored, (restored ? "" : restored.error()));
    auto& r = restored.value();
    CHECK(evaluate(r) == 1);
    // Commit the existing city-creation operation into the previously empty slot.
    for (auto* world : {&w, &r}) {
        auto plan = CityCreation::prepare(*world, {IslandId{0}, PlayerId{2}, 0, {}});
        REQUIRE(plan);
        REQUIRE(plan.value().number == CityNo{2});
        REQUIRE(CityCreation::commit(*world, plan.value()));
    }
    auto& slots = WorldTestAccess::islands(w)[0]->cities;
    auto& other = WorldTestAccess::islands(r)[0]->cities;
    CHECK(evaluate(w) == 0);
    CHECK(evaluate(r) == 0);
    CHECK(w.state_hash() == r.state_hash());
    slots[2].reset();
    other[2].reset();
    CHECK(evaluate(w) == 1);
    CHECK(evaluate(r) == 1);
    CHECK(w.state_hash() == r.state_hash());
}
TEST_CASE("SO1b other object kinds queries and actions remain unsupported") {
    for (const auto& source :
         {object_source("OBJECT_STADT \"A\" 0\n", "IF OBJECT \"A\" ALIVE THEN {}\n"),
          object_source("OBJECT_STADT \"A\" 0\n",
                        "IF OBJECT \"A\" KILLED THEN { QUESTLOG_SHOW(1); }\n"),
          object_source("OBJECT_FIGUR \"A\" 0\n", query_kernel("A"))}) {
        CHECK_FALSE(compile_script(source, {}));
    }
}
TEST_CASE("SO1b S01_02_01 refuses at the next unsupported action (game files)") {
    auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    std::filesystem::path file;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(*game / "Scenes"))
        if (entry.path().filename() == "S01_02_01.sze")
            file = entry.path();
    REQUIRE_FALSE(file.empty());
    std::ifstream in(file, std::ios::binary);
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
    auto payload = anno::formats::bigbrother::find_payload(bytes);
    REQUIRE(payload);
    auto registry =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE(registry.error.empty());
    std::vector<TypeId> ids;
    for (const auto& type : registry.types->types())
        ids.push_back(type.id);
    std::vector<std::string_view> goods;
    for (const auto& good : registry.types->goods_table())
        goods.push_back(good.name);
    const auto data = payload.value();
    auto program = compile_script(std::string(data.begin(), data.end()),
                                  {ids, 32, 5, goods, "scenes/synthetic"});
    REQUIRE_FALSE(program);
    const auto line =
        "script offset " + std::to_string(program.error().offset) + ": " + program.error().message;
    MESSAGE(line);
    CHECK(line == "script offset 1339: unsupported action: QUESTLOG_SHOW");
}

TEST_CASE("SO1b identical duplicate city names retain the first declaration") {
    auto p = compile_script(
        object_source("OBJECT_STADT \"A\" 258\nOBJECT_STADT \"A\" 513\n", query_kernel("a")), {});
    REQUIRE(p);
    CHECK(p.value().city_objects[p.value().expressions.front().index].island == IslandId{2});
    CHECK(p.value().city_objects[p.value().expressions.front().index].city == CityNo{1});
}

TEST_CASE("SO1b scheduler supplies the live island table and configured large city ring") {
    auto types = script_test::make().types;
    auto spec = command_test::two_city_spec(types);
    spec.script_context = ScriptContext::Pending;
    spec.script_source = object_source("OBJECT_STADT \"A\" 65280\n", query_kernel("A"));
    SimConfig cfg;
    cfg.city_slots = 300; // data-defined ring exceeds the original byte's slot range
    auto made = World::create(spec, types.registry, cfg);
    REQUIRE(made);
    auto w = std::move(made).value();
    REQUIRE(w.islands()[0]->cities.size() == 300);
    w.advance(0);
    CHECK(w.script().state.signals.at(1) == 0);
    w.advance(500);
    w.advance(0);
    REQUIRE_FALSE(w.faulted());
    CHECK(w.script().state.signals.at(1) == 1);
}
