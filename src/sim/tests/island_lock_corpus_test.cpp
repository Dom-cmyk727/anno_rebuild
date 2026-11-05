#include "original_component.hpp"
// The LOCKED record on the shipped data (task MAP-2 package D). The raw bytes are read with
// formats::insel, independently of the scenario adapter, and compared with the islands the
// adapter and World::create produce (docs/re/ship-founding.md section 11.5: 107 islands with
// LOCKED = 1 in 8 scenario files, 21 dummy templates with LOCKED = 0, no other record).
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/insel.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

namespace sim = anno::sim;
namespace fs = std::filesystem;

namespace {

std::vector<fs::path> files_with(const fs::path& root, const std::string& ext) {
    std::vector<fs::path> out;
    for (const auto& e : fs::recursive_directory_iterator(root)) {
        if (e.is_regular_file() && e.path().extension() == ext) {
            out.push_back(e.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

TEST_CASE("island lock: original-data components keep every shipped LOCKED byte through loader and "
          "world") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto loaded_types =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded_types.error.empty(), loaded_types.error);
    const std::vector<fs::path> files = files_with(*dir / "Scenes", ".sze");
    REQUIRE_FALSE(files.empty());
    std::uint64_t compared = 0, locked = 0, unmatched = 0, files_with_lock = 0;
    for (const fs::path& f : files) {
        INFO(f.string());
        const auto buf = anno::test::read_file(f);
        auto scenario = sim::load_scenario(buf, *loaded_types.types, sim::SimConfig{});
        REQUIRE(scenario);
        auto raw = anno::formats::insel::islands(buf);
        REQUIRE(raw);
        std::uint64_t locked_in_file = 0;
        for (const anno::formats::insel::Island& isl : raw.value()) {
            const auto rec = isl.record("LOCKED");
            std::uint8_t want = 0;
            if (rec) {
                REQUIRE(rec->size() == 1);
                want = (*rec)[0];
            }
            const sim::ScenarioSpec::IslandSpec* match = nullptr;
            for (const auto& s : scenario.value().spec.islands) {
                if (s.slot.v == isl.number && s.w == isl.width && s.h == isl.height) {
                    match = &s;
                    break;
                }
            }
            if (match == nullptr) {
                ++unmatched;
                continue;
            }
            CHECK(match->lock == want);
            ++compared;
            locked += want != 0 ? 1 : 0;
            locked_in_file += want != 0 ? 1 : 0;
        }
        if (locked_in_file == 0) {
            continue;
        }
        // A scenario with locked islands: the created world carries the bytes, and so does a
        // save/load round trip of it.
        ++files_with_lock;
        auto component = original_component::import_only(scenario.value().spec, loaded_types.types);
        REQUIRE_MESSAGE(component, (component ? "" : component.error()));
        MESSAGE("original-data LOCKED component: " << component.value().diagnostic);
        auto world =
            sim::World::create(component.value().spec, loaded_types.types, sim::SimConfig{});
        REQUIRE_MESSAGE(world.ok(), (world.ok() ? std::string() : world.error()));
        std::uint64_t in_world = 0;
        for (const auto& island : world.value().islands()) {
            in_world += island.has_value() && island->locked() ? 1 : 0;
        }
        CHECK(in_world == locked_in_file);
        auto reloaded = sim::World::load(world.value().save(), loaded_types.types);
        REQUIRE_MESSAGE(reloaded.ok(), (reloaded.ok() ? std::string() : reloaded.error()));
        CHECK(reloaded.value().state_hash() == world.value().state_hash());
        for (std::size_t i = 0; i < world.value().islands().size(); ++i) {
            if (world.value().islands()[i]) {
                CHECK(reloaded.value().islands()[i]->lock == world.value().islands()[i]->lock);
            }
        }
    }
    MESSAGE("islands compared: " << compared << ", locked: " << locked << " in " << files_with_lock
                                 << " files, unmatched: " << unmatched);
    CHECK(unmatched == 0);
    CHECK(compared ==
          668);           // the INSEL5 islands of the 50 scenarios (as the vertex-grid corpus case)
    CHECK(locked == 107); // section 11.5: 107 islands with LOCKED = 1 in the .sze files
    CHECK(files_with_lock == 8); // and 8 scenario files
}

TEST_CASE(
    "island lock: the 21 dummy island templates carry LOCKED 0, so link islands are unlocked") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::uint64_t records = 0, locked = 0;
    for (const fs::path& f : files_with(*dir / "Scenes" / "Islands", ".scp")) {
        INFO(f.string());
        const auto buf = anno::test::read_file(f);
        auto raw = anno::formats::insel::islands(buf);
        REQUIRE(raw);
        for (const anno::formats::insel::Island& isl : raw.value()) {
            if (const auto rec = isl.record("LOCKED")) {
                REQUIRE(rec->size() == 1);
                ++records;
                locked += (*rec)[0] != 0 ? 1 : 0;
            }
        }
    }
    CHECK(records == 21);
    CHECK(locked == 0);
}
