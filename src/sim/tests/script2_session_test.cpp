#include <algorithm>
#include <fstream>
#include <iterator>

#include <doctest/doctest.h>

#include <anno/formats/bytes.hpp>
#include <anno/sim/archive.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>

#include "script_fixture.hpp"
#include "world_access.hpp"
using namespace anno::sim;
namespace {
auto session_world() {
    return script_test::make(false,
                             "INITBLOCK(){ SETCONSTRUCTION_DRAWSTATE(\"ME\",\"90001\",TRUE); "
                             "MONEY_ADD(\"ME\",7); PLAYWAVE(\"A.WAV\"); }",
                             {}, {}, "scenes/mod");
}
void observe(void* p, std::span<const SimEvent> events) {
    auto& out = *static_cast<std::vector<SimEvent>*>(p);
    out.insert(out.end(), events.begin(), events.end());
}
void checksum(std::vector<std::uint8_t>& bytes) {
    StateHasher h;
    h.bytes(std::span(bytes).first(bytes.size() - 8));
    for (std::size_t i = 0; i < 8; ++i)
        bytes[bytes.size() - 8 + i] = static_cast<std::uint8_t>(h.value() >> (8 * i));
}
} // namespace
TEST_CASE("SCRIPT2 normal recorded and unrecorded startup precedes first public BuildCmd") {
    using namespace command_test;
    auto a = session_world(), b = session_world();
    auto pending = World::load(a.world.save(), a.types.registry);
    REQUIRE(pending);
    const auto rng = a.world.rng().cursor();
    auto started = Recorder::start(a.world);
    REQUIRE(started);
    auto rec = std::move(started).value();
    REQUIRE(rec.log().entries.size() == 1);
    CHECK(rec.log().entries[0].kind == ReplayEntry::Kind::Startup);
    CHECK(a.world.script().state.context == ScriptContext::Ready);
    CHECK(a.world.clock().ms == 0);
    CHECK(a.world.rng().cursor() == rng);
    auto native_pending = World::load(rec.log().initial_save, a.types.registry);
    REQUIRE(native_pending);
    CHECK(native_pending.value().script().state.context == ScriptContext::Pending);
    const auto cmd = build(0, kCity0, {at(a.types.weaver, 120, 112)});
    REQUIRE(rec.submit(cmd).ok());
    rec.checkpoint();
    run_frame(b.world, nullptr, std::span(&cmd, 1), 0);
    CHECK(b.world.building_at(kCity0, 120, 112) != nullptr);
    CHECK(b.world.players().find(PlayerId{0})->money == a.world.players().find(PlayerId{0})->money);
    auto turn = session_world();
    run_turn(turn.world, nullptr, Turn{0, 0, {cmd}}, 0);
    CHECK(turn.world.save() == a.world.save());
    std::vector<SimEvent> expected;
    for (auto& e : a.world.drain_events())
        if (e.kind == SimEventKind::ScriptWave)
            expected.push_back(e);
    REQUIRE(expected.size() == 1);
    auto encoded = write_log(rec.log());
    REQUIRE(encoded);
    auto decoded = read_log(encoded.value());
    REQUIRE(decoded);
    std::vector<SimEvent> events;
    auto outcome = replay(pending.value(), decoded.value(), {&events, observe});
    REQUIRE(outcome);
    CHECK(outcome.value().reproduced());
    CHECK(pending.value().save() == a.world.save());
    std::vector<SimEvent> waves;
    for (auto& e : events)
        if (e.kind == SimEventKind::ScriptWave)
            waves.push_back(e);
    CHECK(waves == expected);
}
TEST_CASE("SCRIPT2 startup and replay reject repeated or nonfresh initialization entries") {
    auto f = session_world();
    auto rec = Recorder::start(f.world);
    REQUIRE(rec);
    auto repeated = rec.value().log();
    repeated.entries.push_back(repeated.entries[0]);
    auto w = World::load(repeated.initial_save, f.types.registry);
    REQUIRE(w);
    auto refused = replay(w.value(), repeated);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().find("fresh Pending") != std::string::npos);
    auto bad = repeated;
    bad.entries.resize(1);
    bad.entries[0].tick = 1;
    CHECK_FALSE(write_log(bad));
    auto encoded = write_log(rec.value().log());
    REQUIRE(encoded);
    REQUIRE(encoded.value().size() >= 9);
    const std::vector<std::uint8_t> startup{3, 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(std::vector<std::uint8_t>(encoded.value().end() - 9, encoded.value().end()) == startup);
    encoded.value().back() = 1;
    auto bad_tick = read_log(encoded.value());
    REQUIRE_FALSE(bad_tick);
    CHECK(bad_tick.error().ends_with("startup entry tick must be0"));
    auto ready = World::load(f.world.save(), f.types.registry);
    REQUIRE(ready);
    const auto saved = ready.value().save();
    auto ready_rec = Recorder::start(ready.value());
    REQUIRE(ready_rec);
    CHECK(ready_rec.value().log().entries.empty());
    CHECK(ready.value().save() == saved);
    CHECK(ready.value().drain_events().empty());
}
TEST_CASE("SCRIPT2 replay content identity distinguishes program namespace and limits") {
    auto f = session_world();
    auto rec = Recorder::start(f.world);
    REQUIRE(rec);
    for (int kind = 0; kind < 3; ++kind) {
        auto w = World::load(rec.value().log().initial_save, f.types.registry);
        REQUIRE(w);
        auto& r = WorldTestAccess::script(w.value());
        if (kind == 0)
            r.source.replace(r.source.find("A.WAV"), 5, "B.WAV");
        if (kind == 1)
            r.asset_namespace = "scenes/other";
        if (kind == 2)
            ++r.limits.asset_bytes;
        REQUIRE(
            restore_script(r, w.value().types(), w.value().players(), w.value().config()).empty());
        const auto before = w.value().state_hash();
        CHECK(before !=
              World::load(rec.value().log().initial_save, f.types.registry).value().state_hash());
        auto refused = replay(w.value(), rec.value().log());
        REQUIRE_FALSE(refused);
        CHECK(refused.error() == "replay refused: script content digest differs from the log's");
        CHECK(w.value().state_hash() == before);
    }
}
TEST_CASE("SCRIPT2 repaired native namespace and limits corruptions reach intended validators") {
    auto f = session_world();
    const auto bytes = f.world.save();
    for (int kind = 0; kind < 2; ++kind) {
        auto bad = bytes;
        if (kind == 0) {
            const std::string ns = "scenes/mod";
            auto it = std::search(bad.begin(), bad.end(), ns.begin(), ns.end());
            REQUIRE(it != bad.end());
            *it = '/';
        } else {
            REQUIRE(bad[20] == 0); // original RNG frame; then ten framed scopes
            std::size_t at = 21;
            for (int section = 0; section < 9; ++section)
                at += 8 + static_cast<std::size_t>(
                              (std::uint64_t{anno::formats::u32_le(bad, at)} |
                               (std::uint64_t{anno::formats::u32_le(bad, at + 4)} << 32)));
            const auto limit = at + 8 + 8 + f.world.script().source.size() + 16;
            REQUIRE(limit + 4 < bad.size());
            std::fill_n(bad.begin() + static_cast<std::ptrdiff_t>(limit), 4, 0);
        }
        checksum(bad);
        auto refused = World::load(bad, f.types.registry);
        REQUIRE_FALSE(refused);
        CHECK(refused.error().find(kind == 0 ? "invalid scenario asset namespace"
                                             : "invalid script limits") != std::string::npos);
        CHECK(refused.error().find("checksum") == std::string::npos);
    }
}
TEST_CASE("SCRIPT2 normal file loader supplies original namespace and unchanged complete source") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto registry =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.error.empty(), registry.error);
    const auto templates = anno::sim_load::scan_island_templates(*game);
    const auto file = *game / "Scenes/0011SingleEndlosAddon/endless01.sze";
    auto loaded = anno::sim_load::load_scenario_file(file, *game, templates, *registry.types,
                                                     SimConfig{}, Rules{});
    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
    CHECK(loaded.value().loaded.spec.script_asset_namespace == "scenes/0011SingleEndlosAddon");
    CHECK(loaded.value().loaded.spec.script_source.size() == 2488);
    auto made = World::create(loaded.value().loaded.spec, registry.types);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    REQUIRE(made.value().script().program);
    CHECK(made.value().script().program->areas.size() == 7);
    CHECK(made.value().script().program->branches.size() == 7);
    CHECK(made.value().script().program->assets.size() == 2);
    auto rec = Recorder::start(made.value());
    REQUIRE(rec);
    CHECK(made.value().script().state.context == ScriptContext::Ready);
    CHECK(made.value().clock().ms == 0);
    CHECK_FALSE(anno::sim_load::load_scenario_file(file, *game / "elsewhere", templates,
                                                   *registry.types, SimConfig{}, Rules{}));
}

TEST_CASE("SCRIPT2 framed oversized namespace refuses before allocation") {
    auto f = session_world();
    WorldTestAccess::script(f.world).asset_namespace = std::string(65537, 'n');
    auto invalid = f.world.save(); // serializer supplies valid section framing and checksum
    auto refused = World::load(invalid, f.types.registry);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().find("script namespace exceeds hard limit") != std::string::npos);
    CHECK(refused.error().find("checksum") == std::string::npos);
}

TEST_CASE("integration historical replay frame2 refuses before current parsing") {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "script1" / "g2.log",
                     std::ios::binary);
    REQUIRE(in.good());
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
    const auto log = read_log(bytes);
    REQUIRE_FALSE(log);
    CHECK(log.error().find("replay-log version 2 is not 4") != std::string::npos);
}
