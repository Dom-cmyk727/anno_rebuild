#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#include <doctest/doctest.h>

#include <anno/formats/sze.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>

#include "world_access.hpp"
using namespace anno::sim;
namespace fs = std::filesystem;
namespace {
std::vector<std::uint8_t> read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    return {std::istreambuf_iterator<char>(in), {}};
}
void write(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    REQUIRE(out.good());
}
struct Original {
    std::shared_ptr<const TypeRegistry> types;
    ScenarioSpec spec;
    World world;
};
Original original(const fs::path& game) {
    auto registry =
        anno::sim_load::load_original_registry(game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.error.empty(), registry.error);
    auto templates = anno::sim_load::scan_island_templates(game);
    auto loaded =
        anno::sim_load::load_scenario_file(game / "Scenes/0011SingleEndlosAddon/endless01.sze",
                                           game, templates, *registry.types, {}, {});
    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
    auto spec = std::move(loaded).value().loaded.spec;
    auto made = World::create(spec, registry.types);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    return {registry.types, std::move(spec), std::move(made).value()};
}
World synthetic_program(const Original& input) {
    auto spec = synthetic::basic_scenario(77);
    spec.settings.world_width = spec.settings.world_height = 1024;
    spec.script_context = ScriptContext::Pending;
    spec.script_source = input.spec.script_source; // entire unchanged program, runtime input only
    spec.script_asset_namespace = input.spec.script_asset_namespace;
    auto neutral = spec.players[0];
    neutral.id = PlayerId{31};
    spec.players.push_back(neutral);
    auto& island = spec.islands[0];
    island.x0 = 32;
    island.y0 = 176;
    island.cities.clear();
    island.deposits.clear();
    island.objects.clear();
    island.owner_runs.clear();
    island.ore_objects.clear();
    synthetic::give_flat_terrain(island, 0x30);
    auto other = island;
    other.slot = IslandId{1};
    other.x0 = 400;
    other.y0 = 16;
    spec.islands.push_back(other);
    // Authored synthetic starting positions, not the original scenario or a relocation.
    for (auto [kind, pos] : {std::pair{1503u, FixedPos{220 * 256, 700 * 256}},
                             std::pair{717u, FixedPos{65 * 256, 198 * 256}},
                             std::pair{717u, FixedPos{426 * 256, 46 * 256}}}) {
        ScenarioSpec::FigureSpec figure;
        figure.kind = kind;
        figure.owner = PlayerId{0};
        figure.pos = pos;
        spec.figures.push_back(figure);
    }
    auto made = World::create(spec, input.types);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    return std::move(made).value();
}
void observe(void* ctx, std::span<const SimEvent> events) {
    auto& out = *static_cast<std::vector<SimEvent>*>(ctx);
    for (const auto& e : events)
        if (e.kind == SimEventKind::ScriptWave)
            out.push_back(e);
}
void wave_ledger(const std::vector<SimEvent>& waves) {
    REQUIRE(waves.size() == 1);
    CHECK(waves[0].code == 317);
    CHECK(waves[0].tick == 18);
    CHECK_FALSE(waves[0].player.valid());
    REQUIRE(waves[0].wave);
    CHECK(waves[0].wave->asset == "scenes/0011SingleEndlosAddon/HAWAIMUCKE.WAV");
    CHECK(waves[0].wave->audience == WaveAudience::AllLocal);
}
void final_ledger(const World& w) {
    CHECK(w.script().state.signals == std::vector<std::uint8_t>{0, 0, 1, 1, 1, 1, 0, 0});
    CHECK(w.players().find(PlayerId{0})->money == 209998); // 10000+99999+99999, no building debit
    CHECK(w.players().find(PlayerId{31})->money == 10000);
    CHECK(w.players().find(PlayerId{0})->permissions.size() == 462);
    CHECK(w.check_invariants().empty());
}
struct Canary {
    ReplayLog normal, wrong_command, wrong_program;
    std::uint64_t final_hash = 0;
};
Canary produce(const Original& input) {
    auto w = synthetic_program(input);
    const auto source = w.script().source;
    const auto pending = w.save();
    auto started = Recorder::start(w);
    REQUIRE(started);
    auto rec = std::move(started).value();
    rec.checkpoint();
    CHECK(w.players().find(PlayerId{0})->money == 10000);
    REQUIRE(rec.submit(Command{PlayerId{0}, SetSpeedCmd{100}}).ok());
    rec.checkpoint();
    rec.advance(36);
    rec.checkpoint(); // oldtick0 visits no IF; nowtick18 visits exactly1 each call
    std::vector<std::size_t> cuts{0, 1, rec.log().entries.size()};
    std::vector<SimEvent> waves;
    for (std::uint32_t branch = 0; branch < 7; ++branch) {
        rec.advance(0);
        const auto events = w.drain_events();
        observe(&waves, events);
        const auto expected = branch < 5 ? 10000 : branch == 5 ? 109999 : 209998;
        CHECK(w.players().find(PlayerId{0})->money == expected);
        CHECK(w.script().state.cursor == (branch + 1) % 7);
        rec.checkpoint();
        if (branch == 0 || branch >= 5)
            cuts.push_back(rec.log().entries.size());
    }
    wave_ledger(waves);
    final_ledger(w);
    CHECK(w.script().source == source);
    rec.advance(0);
    rec.checkpoint(); // next visit has no repeat event/reward
    CHECK(w.drain_events().empty());
    final_ledger(w);
    const auto final = w.save();
    for (const auto cut : cuts) {
        auto prefix = rec.log();
        prefix.entries.resize(cut);
        auto before = World::load(pending, input.types);
        REQUIRE(before);
        std::vector<SimEvent> prefix_waves;
        auto a = replay(before.value(), prefix, {&prefix_waves, observe});
        REQUIRE(a);
        REQUIRE(a.value().reproduced());
        auto restored = World::load(before.value().save(), input.types);
        REQUIRE(restored);
        auto suffix = rec.log();
        suffix.entries.erase(suffix.entries.begin(),
                             suffix.entries.begin() + static_cast<std::ptrdiff_t>(cut));
        CHECK(restored.value().drain_events().empty());
        std::vector<SimEvent> suffix_waves;
        auto b = replay(restored.value(), suffix, {&suffix_waves, observe});
        REQUIRE(b);
        CHECK(b.value().reproduced());
        CHECK(restored.value().save() == final);
        CHECK(prefix_waves.size() == (cut > 6 ? 1u : 0u));
        CHECK(suffix_waves.size() == (cut <= 6 ? 1u : 0u));
        prefix_waves.insert(prefix_waves.end(), suffix_waves.begin(), suffix_waves.end());
        wave_ledger(prefix_waves);
    }
    auto wrong = rec.log();
    REQUIRE(wrong.entries[2].kind == ReplayEntry::Kind::Commands);
    std::get<SetSpeedCmd>(wrong.entries[2].commands[0].body).percent = 200;
    auto legal = World::load(pending, input.types);
    REQUIRE(legal);
    REQUIRE(legal.value().initialize_scripts().empty());
    REQUIRE(legal.value().apply(wrong.entries[2].commands[0]).ok());
    auto changed_program = rec.log();
    auto changed = World::load(pending, input.types);
    REQUIRE(changed);
    auto& runtime = WorldTestAccess::script(changed.value());
    runtime.source.replace(runtime.source.find("HAWAIMUCKE.WAV"), 13, "WRONG.WAV");
    REQUIRE(
        restore_script(runtime, *input.types, changed.value().players(), changed.value().config())
            .empty());
    changed_program.initial_save =
        changed.value().save(); // valid native frame/checksum, different content
    return {rec.log(), std::move(wrong), std::move(changed_program), w.state_hash()};
}
void verify(const Canary& canary, const std::shared_ptr<const TypeRegistry>& types) {
    auto w = World::load(canary.normal.initial_save, types);
    REQUIRE(w);
    std::vector<SimEvent> waves;
    auto replayed = replay(w.value(), canary.normal, {&waves, observe});
    REQUIRE(replayed);
    CHECK(replayed.value().reproduced());
    CHECK(w.value().state_hash() == canary.final_hash);
    final_ledger(w.value());
    wave_ledger(waves);
    auto wrong = World::load(canary.wrong_command.initial_save, types);
    REQUIRE(wrong);
    auto divergent = replay(wrong.value(), canary.wrong_command);
    REQUIRE(divergent);
    CHECK(divergent.value().diverged_entry == std::optional<std::uint64_t>{3});
    auto program = World::load(canary.wrong_program.initial_save, types);
    REQUIRE(program);
    auto refused = replay(program.value(), canary.wrong_program);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == "replay refused: script content digest differs from the log's");
}
} // namespace
TEST_CASE("SCRIPT2 original unit import dependency is explicit") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto path = *game / "Scenes/0011SingleEndlosAddon/endless01.sze";
    auto parsed = anno::formats::sze::Scene::parse(read(path));
    REQUIRE(parsed);
    auto players = parsed.value().players();
    REQUIRE(players);
    std::uint32_t ships = 0, human_cities = 0;
    for (const auto& player : players.value())
        if (player.index == 0) {
            auto units = player.units("SCHIFF");
            REQUIRE(units);
            for (const auto& unit : units.value()) {
                ++ships;
                CHECK(unit.figure == 1503);
                CHECK(unit.x_bits == 0x43afc000);
                CHECK(unit.y_bits == 0x43bfc000);
            }
            for (const auto& record : player.records)
                human_cities += (record.name == "STADT" || record.name == "STADT2") ? 1u : 0u;
        }
    CHECK(ships == 1);
    CHECK(human_cities == 0);
    auto input = original(*game);
    CHECK(input.spec.figures.empty());
    CHECK(input.world.figures().by_id.empty());
    auto rec = Recorder::start(input.world);
    REQUIRE(rec);
    CHECK(input.world.script().state.context == ScriptContext::Ready);
    CHECK(input.world.script().program->branches.size() == 7);
    rec.value().advance(36);
    for (int i = 0; i < 7; ++i)
        rec.value().advance(0);
    CHECK(input.world.script().state.signals == std::vector<std::uint8_t>{0, 1, 1, 1, 1, 1, 1, 1});
    CHECK(input.world.drain_events().empty());
    MESSAGE("OPEN F2: original SCHIFF/SOLDAT/CREW import is absent; 1 raw Human ship, 0 imported "
            "figures, 0 Human cities. Compile+INIT+false visits are not real "
            "conditional/construction proof.");
}
TEST_CASE("SCRIPT2 synthetic complete original program canary producer") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto input = original(*game);
    const auto canary = produce(input);
    verify(canary, input.types);
    if (const char* dir = std::getenv("ANNO_SCRIPT2_CANARY_DIR"); dir && *dir) {
        for (const auto& [name, log] : {std::pair{"normal.log", &canary.normal},
                                        std::pair{"wrong-command.log", &canary.wrong_command},
                                        std::pair{"wrong-program.log", &canary.wrong_program}}) {
            auto bytes = write_log(*log);
            REQUIRE(bytes);
            write(fs::path(dir) / name, bytes.value());
        }
        const auto expected = std::to_string(canary.final_hash) + "\n";
        write(fs::path(dir) / "expected.txt", {expected.begin(), expected.end()});
    }
}
TEST_CASE("SCRIPT2 synthetic encoded canary replay worker") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto registry =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE(registry.error.empty());
    if (const char* dir = std::getenv("ANNO_SCRIPT2_CANARY_DIR"); dir && *dir) {
        auto normal = read_log(read(fs::path(dir) / "normal.log"));
        REQUIRE(normal);
        auto wrong = read_log(read(fs::path(dir) / "wrong-command.log"));
        REQUIRE(wrong);
        auto program = read_log(read(fs::path(dir) / "wrong-program.log"));
        REQUIRE(program);
        const auto expected = read(fs::path(dir) / "expected.txt");
        verify({normal.value(), wrong.value(), program.value(),
                std::stoull(std::string(expected.begin(), expected.end()))},
               registry.types);
    } else {
        auto input = original(*game);
        verify(produce(input), input.types); // in-process only
    }
}
