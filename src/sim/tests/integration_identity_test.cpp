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
    for (const auto& [name, version] :
         std::array<std::pair<const char*, unsigned>, 6>{{{"published128", 128},
                                                          {"supply129", 129},
                                                          {"script129", 129},
                                                          {"unit130", 130},
                                                          {"needs130", 130},
                                                          {"combined131", 131}}}) {
        INFO(std::string(name));
        std::ifstream file(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "integrate1" /
                               (std::string(name) + ".save"),
                           std::ios::binary);
        REQUIRE(file.good());
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
        const auto relative = std::string(name) == "script129"
                                  ? "unit130/fixtures/synthetic/sim/script1/g2.log"
                                  : (std::string(name) == "unit130"
                                         ? "unit130/fixtures/synthetic/sim/script2/g2.log"
                                         : std::string(name) + "/fixtures/synthetic/sim/g2.log");
        std::ifstream original_log(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "integrate1" /
                                       "legacy" / relative,
                                   std::ios::binary);
        REQUIRE(original_log.good());
        const std::vector<std::uint8_t> historical{std::istreambuf_iterator<char>(original_log),
                                                   {}};
        const auto decoded = sim::read_log(historical);
        if (std::string(name) == "combined131") {
            REQUIRE_FALSE(decoded); // Current framing, refused at its state identity.
            CHECK(decoded.error() ==
                  "stream (byte 24): replay state stream version differs from this build's");
        } else {
            REQUIRE_FALSE(decoded);
            CHECK(decoded.error().find(std::string("replay-log version ") +
                                       (std::string(name) == "unit130" ? "3" : "2") +
                                       " is not 4") != std::string::npos);
        }
        const auto refused = sim::World::load(bytes, f.types.registry);
        REQUIRE_FALSE(refused);
        CHECK(refused.error() == "save: state stream version " + std::to_string(version) +
                                     " is not supported (this build reads 134)");
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
    for (auto version : {128u, 129u, 130u, 131u}) {
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

TEST_CASE("expansion identity134: real133 archives and independent replay identity layers refuse") {
    const auto f = script_test::make();
    const auto bytes_of = [](const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        REQUIRE_MESSAGE(in.good(), path.string());
        return std::vector<std::uint8_t>{std::istreambuf_iterator<char>(in), {}};
    };
    const auto base = std::filesystem::path(ANNO_FIXTURES_DIR) / "sim";
    const auto old_native = bytes_of(base / "expand1/legacy133/g2.save");
    const auto old_recording = bytes_of(base / "econbook1/current133/g2.log");
    const auto old = sim::World::load(old_native, f.types.registry);
    REQUIRE_FALSE(old.ok());
    CHECK(old.error() == "save: state stream version 133 is not supported (this build reads 134)");
    const auto decoded = sim::read_log(old_recording);
    REQUIRE_FALSE(decoded.ok());
    CHECK(decoded.error() ==
          "stream (byte 24): replay state stream version differs from this build's");

    auto world = f.world;
    auto record = sim::Recorder::start(world);
    REQUIRE(record.ok());
    record.value().advance(20);
    record.value().checkpoint();
    const auto good = record.value().log();
    const auto encoded = sim::write_log(good);
    REQUIRE(encoded.ok());
    const auto read = sim::read_log(encoded.value());
    REQUIRE(read.ok());
    auto current = sim::World::load(good.initial_save, f.types.registry);
    REQUIRE(current.ok());
    const auto legal = sim::replay(current.value(), read.value());
    REQUIRE(legal.ok());
    REQUIRE(legal.value().reproduced());
    CHECK(current.value().save() == world.save());
    const auto before = current.value().save();

    // Each mismatch is isolated. Current native under old outer metadata cannot pass the outer
    // check; current outer metadata with a valid actual133 native must reach the embedded check.
    auto outer = good;
    outer.state_stream_version = 133;
    auto refused = sim::replay(current.value(), outer);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error() == "replay refused: state stream version differs from this build's");
    CHECK(current.value().save() == before);
    auto embedded = good;
    embedded.initial_save = old_native; // original checksum preserved, not a corrupt current blob
    const auto current_outer = sim::write_log(embedded);
    REQUIRE(current_outer.ok());
    const auto parsed = sim::read_log(current_outer.value());
    REQUIRE(parsed.ok()); // outer134 is actually accepted before the native identity rejects
    refused = sim::replay(current.value(), parsed.value());
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error() ==
          "replay refused: initial save state stream version differs from this build's");
    CHECK(current.value().save() == before);

    auto inner = good;
    REQUIRE(inner.initial_save.at(20) == 0);
    island_test::put32(inner.initial_save, 29, 133); // only inner header, outer native still134
    island_test::reseal(inner.initial_save);
    refused = sim::replay(current.value(), inner);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error() ==
          "replay refused: initial save header identity differs from this build's");
    CHECK(current.value().save() == before);
}
