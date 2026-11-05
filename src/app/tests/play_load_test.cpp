// --play's loader (load_play): the route is the file's extension. A `.txt` is a
// fixed map (load_fixed_map, with the developer start and the developer's test
// resources); anything else is a scenario file. The lan5 case loads the file by path
// and does not depend on its islands (MAP.2 may change them).
#include <algorithm>
#include <fstream>
#include <string>

#include <doctest/doctest.h>

#include <anno/app/commands_file.hpp>
#include <anno/app/play.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;

namespace {

fs::path lan5_path() {
    return fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
}

// The result's error text, or "" when it loaded (for the REQUIRE_MESSAGE-free checks).
std::string error_of(const anno::formats::Result<anno::app::PlayLoad, std::string>& r) {
    return r.ok() ? std::string() : r.error();
}

} // namespace

TEST_CASE("play loader: lan5.txt gives player 1 its start island with the test resources") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = anno::app::load_play(*dir, lan5_path());
    const bool ok = loaded.ok();
    REQUIRE_MESSAGE(ok, error_of(loaded));
    anno::app::PlayLoad& play = loaded.value();
    CHECK(play.fixed_map);
    CHECK(play.name == "lan5");
    CHECK(!play.placed.empty());

    // Player 1 (the developer player) has a city on an island: the start island.
    bool owns_start = false;
    for (const auto& island : play.world.islands()) {
        if (!island) {
            continue;
        }
        for (const auto& city : island->cities) {
            if (city && city->owner == anno::app::developer_player()) {
                owns_start = true;
            }
        }
    }
    CHECK_MESSAGE(owns_start, "player 1 has a city on a start island");

    // The test resources: money of the developer player at the round-dr amount.
    const anno::sim::Player* dev = play.world.players().find(anno::app::developer_player());
    REQUIRE(dev != nullptr);
    CHECK(dev->money == 10000000);
}

TEST_CASE("play loader: a malformed .txt names the file and the line") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path bad = fs::temp_directory_path() / "anno-play-malformed.txt";
    {
        std::ofstream out(bad, std::ios::binary);
        out << "this is not a fixed map\n";
    }
    auto loaded = anno::app::load_play(*dir, bad);
    CHECK_FALSE(loaded.ok());
    const std::string why = error_of(loaded);
    CHECK_MESSAGE(why.find("anno-play-malformed.txt") != std::string::npos, why);
    CHECK_MESSAGE(why.find("parse_fixed_map") != std::string::npos, why);
    CHECK_MESSAGE(why.find("line") != std::string::npos, why);
    std::error_code ec;
    fs::remove(bad, ec);
}

TEST_CASE("play loader: original endless scenario keeps script context and starting actors") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path file = *dir / "Scenes/0011SingleEndlosAddon/endless01.sze";
    if (!fs::exists(file)) {
        MESSAGE("skipping: endless01.sze is not in this game directory");
        return;
    }
    auto loaded = anno::app::load_play(*dir, file);
    const bool ok = loaded.ok();
    REQUIRE_MESSAGE(ok, error_of(loaded));
    CHECK_FALSE(loaded.value().fixed_map);
    CHECK(loaded.value().placed.empty());
    CHECK(loaded.value().world.script().asset_namespace == "scenes/0011SingleEndlosAddon");
    CHECK(loaded.value().world.script().state.context == anno::sim::ScriptContext::Pending);
    CHECK(std::count_if(loaded.value().world.figures().list.slots.begin(),
                        loaded.value().world.figures().list.slots.end(),
                        [](const auto& slot) { return slot.has_value(); }) == 5);
}

TEST_CASE("play: startup fault pauses and repeated synchronous turns cannot consume commands") {
    using namespace anno::sim;
    auto spec = synthetic::basic_scenario(77);
    spec.script_context = ScriptContext::Pending;
    spec.script_source = "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n\"ME\" 0\n"
                         "[SCRIPT_KERNEL]\nINITBLOCK(){ TIMER_SET(1,7); TIMER_START(1); }\n"
                         "[SIGNALS]\n[CAMARAPOS]\n0,0\n";
    spec.script_limits.work = 1;
    auto made = World::create(spec, TypeRegistry::empty());
    REQUIRE(made);
    anno::app::PlayRunner runner(std::move(made).value());
    auto first = runner.post_command({PlayerId{0}, SetSpeedCmd{200}});
    auto second = runner.post_command({PlayerId{0}, SetSpeedCmd{300}});
    runner.run_n(3);
    const auto frozen = runner.state_hash();
    REQUIRE(runner.snapshot());
    CHECK(runner.snapshot()->paused);
    CHECK(runner.snapshot()->game_ms == 0);
    CHECK_FALSE(runner.snapshot()->last_rejection.empty());
    for (auto id : {first, second}) {
        CHECK(runner.outcome(id).state == anno::app::CommandOutcome::State::Refused);
        CHECK(runner.outcome(id).status == Reject::PermissionContext);
    }
    runner.set_paused(false);
    runner.run_n(2);
    CHECK(runner.state_hash() == frozen);
}

TEST_CASE("play loader: unsupported original actors are refused instead of silently omitted") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = anno::app::load_play(*dir, *dir / "Scenes/0200Single/Single_01.sze");
    REQUIRE_FALSE(loaded);
    CHECK(loaded.error().find("unsupported actor") != std::string::npos);
}
