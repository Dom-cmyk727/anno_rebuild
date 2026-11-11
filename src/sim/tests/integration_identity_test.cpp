// MAP-INTEGRATE-1 I2: each predecessor and each compatibility predicate has its own control.
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
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
                                     " is not supported (this build reads 138)");
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

TEST_CASE("expansion identity: real133 archives and independent replay identity layers refuse "
          "(checked at 135)") {
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
    CHECK(old.error() == "save: state stream version 133 is not supported (this build reads 138)");
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

TEST_CASE("salt identity135: real133 and real134 archives refuse, each with its own diagnostic") {
    const auto f = script_test::make();
    const auto bytes_of = [](const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        REQUIRE_MESSAGE(in.good(), path.string());
        return std::vector<std::uint8_t>{std::istreambuf_iterator<char>(in), {}};
    };
    const auto base = std::filesystem::path(ANNO_FIXTURES_DIR) / "sim";
    // actual native states of the two predecessor identities
    const auto n133 = bytes_of(base / "expand1/legacy133/g2.save");
    const auto n134 = bytes_of(base / "salt1/legacy134/g2.save");
    for (const auto& [bytes, version] : {std::pair{&n133, 133}, std::pair{&n134, 134}}) {
        const auto refused = sim::World::load(*bytes, f.types.registry);
        REQUIRE_FALSE(refused.ok());
        CHECK(refused.error() == "save: state stream version " + std::to_string(version) +
                                     " is not supported (this build reads 138)");
    }
    // actual predecessor recordings: the replay header carries the identity
    for (const char* rel : {"econbook1/current133/g2.log", "expand1/current134/g2.log"}) {
        const auto decoded = sim::read_log(bytes_of(base / rel));
        REQUIRE_FALSE(decoded.ok());
        CHECK(decoded.error() ==
              "stream (byte 24): replay state stream version differs from this build's");
    }
    // the two diagnostics differ (a 134 native is not reported as 133 and vice versa)
    CHECK(sim::World::load(n133, f.types.registry).error() !=
          sim::World::load(n134, f.types.registry).error());
}

TEST_CASE("combined identity138: valid native and replay artifacts 134 through 137 refuse at "
          "their exact boundary, beside a valid 138 control") {
    const auto f = script_test::make();
    const auto bytes_of = [](const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        REQUIRE_MESSAGE(in.good(), path.string());
        return std::vector<std::uint8_t>{std::istreambuf_iterator<char>(in), {}};
    };
    // Extract the counted native blob without asking read_log to accept the old outer identity.
    // This is deliberately just the documented replay envelope, not a compatibility reader.
    const auto initial_save_of = [&](const std::filesystem::path& path) {
        const auto bytes = bytes_of(path);
        sim::ReadArchive r(bytes);
        std::array<std::uint8_t, 4> magic{};
        r.bytes(magic);
        std::uint32_t u32 = 0;
        std::uint64_t u64 = 0;
        r.u32(u32); // replay frame
        r.u32(u32); // command wire
        r.u32(u32); // state stream
        for (int i = 0; i < 7; ++i)
            r.u64(u64); // schema and the six content/configuration digests
        r.u32(u32);     // PRNG timeline
        std::uint64_t missing = 0;
        r.count(missing, 8);
        for (std::uint64_t i = 0; i < missing; ++i) {
            std::string name;
            r.str(name);
        }
        std::uint64_t length = 0;
        r.u64(length);
        REQUIRE_FALSE(r.failed());
        REQUIRE(magic == std::array<std::uint8_t, 4>{'A', '3', 'R', 'P'});
        REQUIRE(length <= r.remaining());
        const auto first = bytes.begin() + static_cast<std::ptrdiff_t>(r.position());
        return std::vector<std::uint8_t>(first, first + static_cast<std::ptrdiff_t>(length));
    };
    const auto base = std::filesystem::path(ANNO_FIXTURES_DIR) / "sim";
    const std::array replay_paths{"expand1/current134/g2.log", "salt1/current135/g2.log",
                                  "freight/current136/g2.log", "iron2/current137/g2.log"};
    std::vector<std::pair<unsigned, std::vector<std::uint8_t>>> natives;
    natives.emplace_back(134, bytes_of(base / "freight/legacy134/g2.save"));
    natives.emplace_back(135, bytes_of(base / "salt1/legacy135/g2.save"));
    natives.emplace_back(136, initial_save_of(base / replay_paths[2]));
    natives.emplace_back(137, initial_save_of(base / replay_paths[3]));
    for (const auto& [version, bytes] : natives) {
        INFO(version);
        const auto refused = sim::World::load(bytes, f.types.registry);
        REQUIRE_FALSE(refused.ok());
        CHECK(refused.error() == "save: state stream version " + std::to_string(version) +
                                     " is not supported (this build reads 138)");
    }
    for (const char* rel : replay_paths) {
        const auto decoded = sim::read_log(bytes_of(base / rel));
        REQUIRE_FALSE(decoded.ok());
        CHECK(decoded.error() ==
              "stream (byte 24): replay state stream version differs from this build's");
    }

    // A native 138 and an outer replay 138 are accepted immediately beside the four refusals.
    auto world = f.world;
    auto recorder = sim::Recorder::start(world);
    REQUIRE(recorder.ok());
    const auto good = recorder.value().log();
    REQUIRE(sim::World::load(good.initial_save, f.types.registry).ok());
    const auto current_bytes = sim::write_log(good);
    REQUIRE(current_bytes.ok());
    REQUIRE(sim::read_log(current_bytes.value()).ok());

    auto target = sim::World::load(good.initial_save, f.types.registry);
    REQUIRE(target.ok());
    const auto before = target.value().save();
    for (const auto& [version, bytes] : natives) {
        INFO(version);
        auto embedded = good;
        embedded.initial_save = bytes; // valid old envelope and checksum, current outer metadata
        const auto outer = sim::write_log(embedded);
        REQUIRE(outer.ok());
        const auto parsed = sim::read_log(outer.value());
        REQUIRE(parsed.ok());
        const auto refused = sim::replay(target.value(), parsed.value());
        REQUIRE_FALSE(refused.ok());
        CHECK(refused.error() ==
              "replay refused: initial save state stream version differs from this build's");
        CHECK(target.value().save() == before);
    }

    // Corruption wins over identity: a damaged old envelope is not misreported as merely old.
    auto damaged = good;
    damaged.initial_save = natives.front().second;
    damaged.initial_save.back() ^= 1;
    const auto corrupt = sim::replay(target.value(), damaged);
    REQUIRE_FALSE(corrupt.ok());
    CHECK(corrupt.error() == "replay refused: initial save checksum differs");
    CHECK(target.value().save() == before);
}
