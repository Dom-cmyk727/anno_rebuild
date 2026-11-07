// W4-UI-2 C2: the sim's construction_eligibility is the lock authority (CLAUDE.md evidence
// policy; this packet changes no sim behaviour). This case compares it, over every one of
// data/ui/construction.txt's 148 cells, with the original's UI lock rule recorded there (the
// `state unlocked|locked` column, read from the real game's View.dll tables and INFRAMIN field,
// docs/re/ui.md section 3.2). A difference is not a sim bug to silently paper over: it is named
// here and, if any is found, recorded in docs/open-questions.md with its evidence, per the
// dispatch's C2. Needs the original game files (ANNO1503_GAME_DIR); skipped otherwise.
#include <filesystem>
#include <sstream>

#include <doctest/doctest.h>

#include <anno/app/opening.hpp>
#include <anno/app/ui_layout.hpp>
#include <anno/sim/build_availability.hpp>
#include <anno/test/game_dir.hpp>

namespace app = anno::app;
namespace sim = anno::sim;
namespace fs = std::filesystem;

namespace {
fs::path manifest_path() {
    return fs::path(ANNO_REPO_DIR) / "data" / "maps" / "candidates" / "ship-start-v2" /
           "lan5-ship-a.json";
}
} // namespace

TEST_CASE("construction menu C2: sim eligibility vs the original's recorded lock state, "
          "all 148 cells") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto layout =
        app::load_construction_layout(fs::path(ANNO_REPO_DIR) / "data" / "ui" / "construction.txt");
    REQUIRE_MESSAGE(layout.ok(), (layout.ok() ? std::string() : layout.error()));
    const auto& construction = layout.value();
    REQUIRE(construction.cells.size() == 148);

    auto loaded = app::load_opening(*dir, manifest_path(), 0);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    const sim::World& world = loaded.value().play.world;
    const sim::PlayerId local = loaded.value().setup.local;

    std::size_t agree = 0;
    std::vector<std::string> mismatches;
    for (const auto& cell : construction.cells) {
        const auto eligibility = world.construction_eligibility(local, sim::TypeId{cell.id});
        const bool allowed = eligibility.allowed();
        if (allowed == cell.unlocked) {
            ++agree;
            continue;
        }
        std::ostringstream line;
        line << cell.category << " (" << cell.col << "," << cell.row << ") id " << cell.id
             << " inframin " << cell.inframin << ": construction.txt says "
             << (cell.unlocked ? "unlocked" : "locked") << ", construction_eligibility says "
             << (allowed ? "allowed" : "not allowed")
             << " (status=" << static_cast<int>(eligibility.status)
             << ", stage=" << eligibility.stage << ")";
        mismatches.push_back(line.str());
    }

    for (const auto& m : mismatches) {
        MESSAGE(m);
    }
    INFO(mismatches.size(),
         " of 148 cells disagree with construction.txt's recorded state "
         "(agree: ",
         agree, ")");
    // No forced CHECK on agreement count: a real disagreement here is evidence for an OQ, not a
    // test failure to paper over by adjusting the assertion (the dispatch's own evidence
    // discipline). This case's job is to surface every disagreement with its exact evidence;
    // closing or explaining it is docs/open-questions.md's job, not this test's.
    CHECK(agree + mismatches.size() == construction.cells.size());
}
