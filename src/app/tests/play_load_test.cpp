// --play's loader (load_play): the route is the file's extension. A `.txt` is a
// fixed map (load_fixed_map, with the developer start and the developer's test
// resources); anything else is a scenario file. The lan5 case loads the file by path
// and does not depend on its islands (MAP.2 may change them).
#include <fstream>
#include <string>

#include <doctest/doctest.h>

#include <anno/app/commands_file.hpp>
#include <anno/app/play.hpp>
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

TEST_CASE("play loader: a .sze loads as a scenario file, as before") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path file = *dir / "Scenes" / "0200Single" / "Single_01.sze";
    if (!fs::exists(file)) {
        MESSAGE("skipping: Single_01.sze is not in this game directory");
        return;
    }
    auto loaded = anno::app::load_play(*dir, file);
    const bool ok = loaded.ok();
    REQUIRE_MESSAGE(ok, error_of(loaded));
    CHECK_FALSE(loaded.value().fixed_map);
    CHECK(loaded.value().placed.empty());
}
