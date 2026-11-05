#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <doctest/doctest.h>

#include <anno/formats/bigbrother.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

#include "script_fixture.hpp"
namespace sim = anno::sim;
namespace bb = anno::formats::bigbrother;
namespace fs = std::filesystem;
namespace {
std::vector<std::uint8_t> read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void write(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    REQUIRE(out.good());
}
std::optional<fs::path> directory() {
    const char* p = std::getenv("ANNO_SCRIPT_CANARY_DIR");
    return p && *p ? std::optional<fs::path>{p} : std::nullopt;
}
struct Canary {
    sim::ReplayLog log, wrong;
    std::shared_ptr<const sim::TypeRegistry> types;
    std::uint64_t final_hash = 0;
};
Canary produce() {
    using namespace command_test;
    auto f = script_test::make();
    auto& w = f.world;
    auto started = sim::Recorder::start(w);
    REQUIRE(started);
    auto rec = std::move(started).value();
    rec.checkpoint(); // 0: Pending native identity
    rec.advance(0);   // 1: all INIT
    const auto command = build(0, kCity0, {at(f.types.weaver, 120, 112)});
    auto hash = w.state_hash();
    REQUIRE(rec.submit(command).status == sim::Reject::ConstructionDenied); // 2
    CHECK(w.state_hash() == hash);
    rec.checkpoint(); // 3
    rec.advance(1000);
    rec.advance(0);
    rec.checkpoint(); // 4,5,6: allow, before build
    auto saved = sim::World::load(w.save(), f.types.registry);
    REQUIRE(saved);
    CHECK(saved.value().state_hash() == w.state_hash());
    const auto gold = w.players().find(sim::PlayerId{0})->money;
    const auto tools = city(w, kCity0).stock.stock[41];
    REQUIRE(rec.submit(command).ok()); // 7: changed command diverges here
    CHECK(w.players().find(sim::PlayerId{0})->money == gold - 300);
    CHECK(city(w, kCity0).stock.stock[41] == tools - 768);
    rec.checkpoint(); // 8
    rec.advance(1000);
    rec.advance(0); // 9,10: deny despite retained stage3 grant
    hash = w.state_hash();
    REQUIRE(rec.submit(build(0, kCity0, {at(f.types.weaver, 121, 112)})).status ==
            sim::Reject::ConstructionDenied); // 11
    CHECK(w.state_hash() == hash);
    rec.checkpoint(); // 12
    REQUIRE(rec.log().entries.size() == 14);
    auto normal = rec.log();
    auto wrong = normal;
    std::get<sim::BuildCmd>(wrong.entries[8].commands[0].body).placements[0].x = 121;
    auto initial = sim::World::load(normal.initial_save, f.types.registry);
    REQUIRE(initial);
    const auto hashes = sim::hash_per_entry(initial.value(), normal);
    const auto wrong_hashes = sim::hash_per_entry(initial.value(), wrong);
    REQUIRE(hashes.size() == 14);
    CHECK(sim::first_divergent_entry(hashes, wrong_hashes) == std::optional<std::size_t>{8});
    return {std::move(normal), std::move(wrong), f.types.registry, w.state_hash()};
}
void verify(const sim::ReplayLog& log, const sim::ReplayLog& wrong,
            std::shared_ptr<const sim::TypeRegistry> types, std::uint64_t expected) {
    auto initial = sim::World::load(log.initial_save, types);
    REQUIRE(initial);
    auto result = sim::replay(initial.value(), log);
    REQUIRE(result);
    CHECK(result.value().reproduced());
    CHECK(result.value().checkpoints_matched == 5);
    CHECK(initial.value().state_hash() == expected);
    auto changed = sim::World::load(wrong.initial_save, types);
    REQUIRE(changed);
    auto divergent = sim::replay(changed.value(), wrong);
    REQUIRE(divergent);
    CHECK(divergent.value().diverged_entry == std::optional<std::uint64_t>{9});
    CHECK(divergent.value().checkpoints_matched == 3);
    // Native cuts at pending, afterINIT, afterpermission change, afterlegalbuild and afterdeny.
    for (std::size_t cut : {0u, 2u, 6u, 8u, 11u}) {
        auto prefix = log;
        prefix.entries.resize(cut + 1);
        auto fresh = sim::World::load(log.initial_save, types);
        REQUIRE(fresh);
        auto before = sim::replay(fresh.value(), prefix);
        REQUIRE(before);
        REQUIRE(before.value().reproduced());
        auto restored = sim::World::load(fresh.value().save(), types);
        REQUIRE(restored);
        auto suffix = log;
        suffix.entries.erase(suffix.entries.begin(),
                             suffix.entries.begin() + static_cast<std::ptrdiff_t>(cut + 1));
        auto after = sim::replay(restored.value(), suffix);
        REQUIRE(after);
        CHECK(after.value().reproduced());
        CHECK(restored.value().state_hash() == expected);
    }
}
} // namespace
TEST_CASE("SCRIPT1 mod canary producer") {
    const auto c = produce();
    auto encoded = sim::write_log(c.log);
    auto wrong = sim::write_log(c.wrong);
    REQUIRE(encoded);
    REQUIRE(wrong);
    auto decoded = sim::read_log(encoded.value());
    auto negative = sim::read_log(wrong.value());
    REQUIRE(decoded);
    REQUIRE(negative);
    verify(decoded.value(), negative.value(), c.types, c.final_hash);
    if (auto dir = directory()) {
        write(*dir / "normal.log", encoded.value());
        write(*dir / "wrong-command.log", wrong.value());
        const auto expected = std::to_string(c.final_hash) + "\n";
        write(*dir / "expected.txt", {expected.begin(), expected.end()});
    }
}
TEST_CASE("SCRIPT1 encoded canary replay worker") {
    if (auto dir = directory()) {
        auto normal = sim::read_log(read(*dir / "normal.log"));
        auto wrong = sim::read_log(read(*dir / "wrong-command.log"));
        REQUIRE(normal);
        REQUIRE(wrong);
        auto f = script_test::make();
        auto bytes = read(*dir / "expected.txt");
        const std::string expected(bytes.begin(), bytes.end());
        verify(normal.value(), wrong.value(), f.types.registry, std::stoull(expected));
    } else {
        // Ordinary isolated control. Only script1_fresh_process establishes distinct-process
        // execution.
        const auto c = produce();
        verify(c.log, c.wrong, c.types, c.final_hash);
    }
}
TEST_CASE("SCRIPT1 original capability inventory distinguishes parsing compilation and execution "
          "(game files)") {
    auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("SKIP: ANNO1503_GAME_DIR is unset");
        return;
    }
    const auto loaded =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE(loaded.error.empty());
    std::vector<sim::TypeId> ids;
    for (const auto& t : loaded.types->types())
        ids.push_back(t.id);
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(*game / "Scenes"))
        if (entry.is_regular_file() && entry.path().extension() == ".sze")
            files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    REQUIRE(files.size() == 50);
    std::size_t parsed = 0, compiled = 0, permission_programs = 0, permission_compiled = 0;
    for (const auto& file : files) {
        auto bytes = read(file);
        auto payload = bb::find_payload(bytes);
        REQUIRE(payload);
        auto ast = bb::parse(payload.value());
        REQUIRE(ast);
        ++parsed;
        bool permission = false;
        const auto block = [&](const auto& body) {
            for (const auto& statement : body)
                if (const auto* call = std::get_if<bb::Call>(&statement))
                    permission = permission || call->name == "SETCONSTRUCTION_DRAWSTATE" ||
                                 call->name == "SETCONSTRUCTION_DRAWSTATE_ALL";
        };
        for (const auto& item : ast.value().kernel) {
            if (const auto* init = std::get_if<bb::InitBlock>(&item))
                block(init->body);
            if (const auto* branch = std::get_if<bb::If>(&item)) {
                block(branch->then_block);
                block(branch->else_block);
            }
        }
        const auto& p = payload.value();
        std::string source(p.begin(), p.end());
        auto program = sim::compile_script(source, {ids, 32, 5});
        compiled += program ? 1u : 0u;
        permission_programs += permission ? 1u : 0u;
        permission_compiled += (program && permission) ? 1u : 0u;
        MESSAGE("CAPABILITY " << fs::relative(file, *game).generic_string() << " parsed=1 compiled="
                              << (program ? 1 : 0) << " executed=0 permission=" << permission
                              << " dependency=" << (program ? "none" : program.error().message));
        if (permission)
            CHECK_FALSE(program); // honest full-program blocker; fragments are separate below
    }
    CHECK(parsed == 50);
    CHECK(permission_programs == 6);
    CHECK(permission_compiled == 0);
    MESSAGE("CAPABILITY_TOTAL parsed=" << parsed << " compiled=" << compiled
                                       << " executed_full_scenario=0 permission_compiled="
                                       << permission_compiled);
}
TEST_CASE(
    "SCRIPT1 original INIT fragment allows actual build in labelled stocked harness (game files)") {
    auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("SKIP: ANNO1503_GAME_DIR is unset");
        return;
    }
    const auto loaded =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE(loaded.error.empty());
    const auto bytes = read(*game / "Scenes/0011SingleEndlosAddon/endless01.sze");
    auto payload = bb::find_payload(bytes);
    REQUIRE(payload);
    auto ast = bb::parse(payload.value());
    REQUIRE(ast);
    REQUIRE_FALSE(ast.value().kernel.empty());
    const auto* init = std::get_if<bb::InitBlock>(&ast.value().kernel[0]);
    REQUIRE(init);
    REQUIRE(init->body.size() == 2);
    const std::string original(payload.value().begin(), payload.value().end());
    const auto end = original.find('}', bb::offset_of(init->body.back()));
    REQUIRE(end != std::string::npos);
    const auto fragment = original.substr(init->offset, end - init->offset + 1);
    const auto wrapper =
        "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n\"HUMAN\" 0\n[SCRIPT_KERNEL]\n" +
        fragment + "\n[SIGNALS]\n[CAMARAPOS]\n0,0\n";
    auto spec = sim::synthetic::basic_scenario(77);
    spec.players[0].money = 30000;
    spec.script_context = sim::ScriptContext::Pending;
    spec.script_source = wrapper;
    sim::synthetic::give_flat_terrain(spec.islands[0], 0x30);
    auto& city = spec.islands[0].cities[0];
    city.stock = std::vector<std::uint32_t>(sim::SimConfig{}.warehouse_goods, 50u * sim::kTon);
    auto made = sim::World::create(spec, loaded.types);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    auto& w = made.value();
    CHECK_FALSE(sim::build_eligibility(w.types(), w.players(), sim::PlayerId{0}, sim::TypeId{1335})
                    .allowed());
    w.advance(0);
    CHECK(w.players().find(sim::PlayerId{0})->permissions.size() == 462);
    auto result = w.apply(command_test::build(0, command_test::kCity0,
                                              {command_test::at(sim::TypeId{1335}, 112, 112)}));
    REQUIRE_MESSAGE(result.ok(), sim::reject_name(result.status));
    REQUIRE(result.records[0].building.valid());
    auto restored = sim::World::load(w.save(), loaded.types);
    REQUIRE(restored);
    CHECK(restored.value().state_hash() == w.state_hash());
    MESSAGE("FRAGMENT_ONLY endless01 INIT offset="
            << init->offset << " length=" << fragment.size()
            << " retained SCRIPT1 fragment regression; complete scenario proof belongs to SCRIPT2");
}
TEST_CASE("SCRIPT1 changed permission action diverges at its content identity and action visit") {
    auto original = script_test::make();
    auto kernel = original.world.script().source;
    const std::string header = "[SCRIPT_KERNEL]\n";
    const auto first = kernel.find(header) + header.size();
    const auto last = kernel.find("[SIGNALS]");
    kernel = kernel.substr(first, last - first);
    const auto allow = script_test::permission("ME", original.types.weaver, true);
    const auto found = kernel.find(allow);
    REQUIRE(found != std::string::npos);
    kernel.replace(found, allow.size(),
                   script_test::permission("ME", original.types.weaver, false));
    auto changed = script_test::make(false, kernel);
    CHECK(original.world.state_hash() !=
          changed.world.state_hash()); // distinct content identity at entry0
    original.world.advance(0);
    changed.world.advance(0);
    CHECK(original.world.players().find(sim::PlayerId{0})->permissions[0].value == 0);
    CHECK(changed.world.players().find(sim::PlayerId{0})->permissions[0].value == 0);
    original.world.advance(1000);
    changed.world.advance(1000);
    original.world.advance(0);
    changed.world.advance(0); // old tick500, PLAYTIME1, first causal action visit
    CHECK(
        original.world.construction_eligibility(sim::PlayerId{0}, original.types.weaver).allowed());
    CHECK_FALSE(
        changed.world.construction_eligibility(sim::PlayerId{0}, changed.types.weaver).allowed());
    const auto command = command_test::build(0, command_test::kCity0,
                                             {command_test::at(original.types.weaver, 120, 112)});
    CHECK(original.world.apply(command).ok());
    const auto hash = changed.world.state_hash();
    CHECK(changed.world.apply(command).status == sim::Reject::ConstructionDenied);
    CHECK(changed.world.state_hash() == hash);
}

TEST_CASE("SCRIPT1 published128 recordings stay unchanged and reach version refusal") {
    for (const auto* name :
         {"g2.log", "g3.log", "g4.log", "g5.log", "g6.log", "m2_found.log", "m2_found_wrong.log",
          "cargo_load.log", "cargo_load_wrong.log", "cargo_unload.log", "cargo_unload_wrong.log"}) {
        auto log = sim::read_log(read(fs::path(ANNO_FIXTURES_DIR) / "sim" / name));
        REQUIRE_MESSAGE(log, (log ? "" : log.error()));
        REQUIRE(log.value().initial_save.size() > 12);
        CHECK(anno::formats::u32_le(log.value().initial_save, 8) == 128);
        auto refused = sim::World::load(log.value().initial_save, sim::TypeRegistry::empty());
        REQUIRE_FALSE(refused);
        CHECK(refused.error() ==
              "save: state stream version 128 is not supported (this build reads 130)");
    }
}
