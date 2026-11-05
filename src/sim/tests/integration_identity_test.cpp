// MAP-INTEGRATE-1 I2: each predecessor and each compatibility predicate has its own control.
#include <array>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/replay.hpp>

#include "island_helpers.hpp"
#include "script_fixture.hpp"
namespace sim = anno::sim;
TEST_CASE("integration identity: actual predecessor saves and logs refuse at intended boundary") {
    auto f = script_test::make();
    const auto current = f.world.save();
    REQUIRE(sim::World::load(current, f.types.registry));
    for (const auto& [name, version] : std::array<std::pair<const char*, unsigned>, 4>{
             {{"published128", 128}, {"supply129", 129}, {"script129", 129}, {"unit130", 130}}}) {
        INFO(name);
        std::ifstream file(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "integrate1" /
                               (std::string(name) + ".save"),
                           std::ios::binary);
        REQUIRE(file.good());
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
        const auto refused = sim::World::load(bytes, f.types.registry);
        REQUIRE_FALSE(refused);
        CHECK(refused.error() == "save: state stream version " + std::to_string(version) +
                                     " is not supported (this build reads 131)");
    }
    auto rec = sim::Recorder::start(f.world);
    REQUIRE(rec);
    auto log = rec.value().log();
    log.entries.clear(); // No later checkpoint can accidentally be the rejection mechanism.
    auto w = sim::World::load(log.initial_save, f.types.registry);
    REQUIRE(w);
    const auto before = w.value().save();
    REQUIRE(sim::replay(w.value(), log));
    CHECK(w.value().save() == before);
    for (auto version : {128u, 129u, 130u}) {
        auto old = log;
        old.state_stream_version = version;
        auto out = sim::replay(w.value(), old);
        REQUIRE_FALSE(out);
        CHECK(out.error() == "replay refused: state stream version differs from this build's");
        CHECK(w.value().save() == before);
        CHECK(sim::hash_per_entry(w.value(), old).empty());
    }
    auto schema = log;
    schema.state_schema ^= 1;
    auto refused = sim::replay(w.value(), schema);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == "replay refused: state schema differs from this build's");
    for (bool wrong_schema : {false, true}) {
        auto forged = log;
        if (wrong_schema)
            forged.initial_save[12] ^= 1;
        else
            island_test::put32(forged.initial_save, 8, 130);
        island_test::reseal(forged.initial_save);
        refused = sim::replay(w.value(), forged);
        REQUIRE_FALSE(refused);
        CHECK(refused.error() == std::string("replay refused: initial save state ") +
                                     (wrong_schema ? "schema differs from this build's"
                                                   : "stream version differs from this build's"));
        CHECK(w.value().save() == before);
    }
    // Current outer frame cannot disguise an incompatible inner state header.
    auto inner = log;
    REQUIRE(inner.initial_save[20] == 0); // original PRNG table, first section starts at29
    island_test::put32(inner.initial_save, 29, 130);
    island_test::reseal(inner.initial_save);
    auto inner_refused = sim::replay(w.value(), inner);
    REQUIRE_FALSE(inner_refused);
    CHECK(inner_refused.error() ==
          "replay refused: initial save header identity differs from this build's");
    auto encoded = sim::write_log(log);
    REQUIRE(encoded);
    REQUIRE(sim::read_log(encoded.value()));
    for (auto version : {2u, 3u}) {
        auto old = encoded.value();
        island_test::put32(old, 4, version);
        const auto out = sim::read_log(old);
        REQUIRE_FALSE(out);
        CHECK(out.error().find("replay-log version " + std::to_string(version) + " is not 4") !=
              std::string::npos);
    }
    auto bad_schema = current;
    bad_schema[12] ^= 1;
    island_test::reseal(bad_schema);
    auto native_refused = sim::World::load(bad_schema, f.types.registry);
    REQUIRE_FALSE(native_refused);
    CHECK(native_refused.error() == "save: script state schema differs from this build's");
}
