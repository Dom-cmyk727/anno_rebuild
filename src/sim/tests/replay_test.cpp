// Stage S9: replay and session (docs/sim/state.md §8.7, §15 S9). The recorder that is
// the one door a session drives a World through, the log round trip, the divergence
// finder down to a scope path, the config-mismatch refusal, the command-before-steps
// order, the PRNG-timeline header, the framing helper and a hostile-log check.
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/world.hpp>

#include "building_helpers.hpp"
#include "island_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;

namespace {

namespace bt = building_test;

/// A SetSpeed to 100 % from the host player (accepted by the default config): a command
/// that changes hashed state when applied.
sim::Command host_speed(std::uint32_t percent = 100) {
    sim::Command c;
    c.issuer = sim::PlayerId{0};
    c.body = sim::SetSpeedCmd{percent};
    return c;
}

sim::Command build_cmd() {
    sim::BuildCmd b;
    b.city = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    b.placements.push_back(sim::PlacementWire{sim::TypeId{9001}, 101, 101, 0, 7, 3});
    b.placements.push_back(sim::PlacementWire{sim::TypeId{9002}, 103, 101, 1, 0, 0});
    sim::Command c;
    c.issuer = sim::PlayerId{1};
    c.body = std::move(b);
    return c;
}

sim::Command legacy_cmd() {
    sim::LegacyRawCmd raw;
    raw.type = 0x6a; // a ship command: not implemented, carried and replayed as-is
    raw.payload = {1, 2, 3, 4, 5};
    sim::Command c;
    c.issuer = sim::PlayerId{0};
    c.body = std::move(raw);
    return c;
}

} // namespace

TEST_CASE("recorder replay reproduces checkpoints") {
    const bt::Kit k = bt::make_kit();
    sim::World w = bt::all_classes_world(k);

    sim::Recorder rec(w);
    rec.checkpoint();
    rec.submit(host_speed(200));
    rec.advance(20);
    rec.advance(20);
    rec.checkpoint();
    rec.advance(100);
    rec.submit(host_speed(100));
    rec.advance(40);
    rec.checkpoint();
    const sim::ReplayLog log = rec.log();

    // A fresh world at the same initial state reproduces every checkpoint.
    sim::World fresh = bt::all_classes_world(k);
    auto outcome = sim::replay(fresh, log);
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(outcome.value().checkpoints_total == 3);
    CHECK(outcome.value().checkpoints_matched == 3);
    CHECK_FALSE(outcome.value().diverged_entry.has_value());

    // A real divergence is caught: if the fresh world is advanced one tick before the
    // replay, the very first checkpoint (recorded at tick 0) differs.
    sim::World skewed = bt::all_classes_world(k);
    skewed.advance(2);
    auto diverged = sim::replay(skewed, log);
    REQUIRE(diverged.ok());
    CHECK_FALSE(diverged.value().reproduced());
    REQUIRE(diverged.value().diverged_entry.has_value());
    CHECK(*diverged.value().diverged_entry == 0);
}

TEST_CASE("write_log read_log round trip") {
    const bt::Kit k = bt::make_kit();
    sim::World w = bt::all_classes_world(k);

    sim::Recorder rec(w);
    rec.checkpoint();
    rec.submit(host_speed(200));
    rec.submit(build_cmd());
    rec.submit(legacy_cmd());
    rec.advance(20);
    rec.checkpoint();
    sim::ReplayLog log = rec.log();
    // A Commands entry with several commands in one block (the framing helper's job).
    sim::ReplayEntry multi;
    multi.kind = sim::ReplayEntry::Kind::Commands;
    multi.commands = {host_speed(50), build_cmd(), legacy_cmd()};
    log.entries.push_back(multi);

    const std::vector<std::uint8_t> bytes = sim::write_log(log);
    auto rd = sim::read_log(bytes);
    REQUIRE_MESSAGE(rd.ok(), (rd.ok() ? std::string() : rd.error()));
    CHECK(rd.value() == log);

    // Re-writing the read-back log yields identical bytes.
    CHECK(sim::write_log(rd.value()) == bytes);
}

TEST_CASE("first divergent entry") {
    const std::vector<std::uint64_t> a{1, 2, 3, 4, 5};
    const std::vector<std::uint64_t> b{1, 2, 9, 4, 5};
    CHECK(sim::first_divergent_entry(a, b) == std::optional<std::size_t>{2});
    CHECK(sim::first_divergent_entry(a, a) == std::nullopt);
    // A prefix: they agree where both exist, so the shorter length is the divergence.
    const std::vector<std::uint64_t> prefix{1, 2};
    CHECK(sim::first_divergent_entry(a, prefix) == std::optional<std::size_t>{2});
    CHECK(sim::first_divergent_entry(prefix, a) == std::optional<std::size_t>{2});
    CHECK(sim::first_divergent_entry(std::span<const std::uint64_t>{},
                                     std::span<const std::uint64_t>{}) == std::nullopt);
}

TEST_CASE("diff_trees names the corrupted building") {
    const bt::Kit k = bt::make_kit();
    sim::World w = bt::all_classes_world(k);
    sim::HashTree before;
    static_cast<void>(w.state_hash(before, 8));

    // Corrupt one building's payload (the weaver in production list 4, slot 0).
    sim::World w2 = w;
    auto& slot = bt::city_of(w2, 0, 0).index.lists[4].slots[0];
    REQUIRE(slot.has_value());
    REQUIRE(std::holds_alternative<sim::Workshop>(slot->payload));
    std::get<sim::Workshop>(slot->payload).carriers_out ^= 1U;

    sim::HashTree after;
    static_cast<void>(w2.state_hash(after, 8));

    const std::string path = sim::diff_trees(before, after);
    CHECK(path == "world/islands/island[0]/cities/city[0]/index/list[4]/building[0]");

    // Identical trees name nothing.
    CHECK(sim::diff_trees(before, before).empty());
}

TEST_CASE("replay refuses a config mismatch") {
    const bt::Kit k = bt::make_kit();
    const sim::ScenarioSpec spec = bt::kit_scenario();

    sim::World logw = bt::make_world(k, spec);
    sim::Recorder rec(logw);
    rec.advance(20);
    rec.checkpoint();
    const sim::ReplayLog log = rec.log();

    // A world with different rules (a different rules digest) is refused before any
    // entry runs.
    sim::Rules rules;
    rules.events_drain_new = true;
    sim::World other = bt::make_world(k, spec, {}, rules);
    auto refused = sim::replay(other, log);
    CHECK_FALSE(refused.ok());
    CHECK(refused.error().find("rules") != std::string::npos);

    // A world on another registry is refused too (registry digest).
    sim::World empty_reg = island_test::make_world(spec);
    auto refused_reg = sim::replay(empty_reg, log);
    CHECK_FALSE(refused_reg.ok());
}

TEST_CASE("run_turn applies commands before steps") {
    const bt::Kit k = bt::make_kit();

    sim::World w1 = bt::all_classes_world(k);
    sim::Recorder rec(w1);
    sim::Turn turn;
    turn.step_ms = 20;
    turn.commands = {host_speed(200)};
    sim::run_turn(w1, &rec, turn, 3);

    // The log records the command block first, then one Advance per step.
    const std::vector<sim::ReplayEntry>& es = rec.log().entries;
    REQUIRE(es.size() == 4);
    CHECK(es[0].kind == sim::ReplayEntry::Kind::Commands);
    CHECK(es[1].kind == sim::ReplayEntry::Kind::Advance);
    CHECK(es[2].kind == sim::ReplayEntry::Kind::Advance);
    CHECK(es[3].kind == sim::ReplayEntry::Kind::Advance);

    // Same result as applying the command and then stepping by hand.
    sim::World w2 = bt::all_classes_world(k);
    w2.apply(host_speed(200));
    w2.advance(20);
    w2.advance(20);
    w2.advance(20);
    CHECK(w1.state_hash() == w2.state_hash());

    // run_frame (single player): the commands, then exactly one advance.
    sim::World w3 = bt::all_classes_world(k);
    sim::Recorder rec3(w3);
    const std::vector<sim::Command> cmds{host_speed(100)};
    sim::run_frame(w3, &rec3, cmds, 50);
    const std::vector<sim::ReplayEntry>& fs = rec3.log().entries;
    REQUIRE(fs.size() == 2);
    CHECK(fs[0].kind == sim::ReplayEntry::Kind::Commands);
    CHECK(fs[1].kind == sim::ReplayEntry::Kind::Advance);
}

TEST_CASE("log header records the prng timeline") {
    const bt::Kit k = bt::make_kit();
    sim::World w = bt::all_classes_world(k);
    sim::Recorder rec(w);
    const sim::ReplayLog& log = rec.log();

    CHECK(log.prng_timeline_version == sim::kPrngTimelineVersion);
    const std::vector<std::string> expected{"CityRandomEvents", "WildlifeSweep", "Ai"};
    CHECK(log.prng_timeline_missing == expected);

    // The digests pin the world the log was recorded against.
    CHECK(log.config_digest == w.config().digest());
    CHECK(log.rules_digest == w.rules().digest());
    CHECK(log.registry_digest == w.types().digest());
    CHECK(log.seams_digest == sim::seams_digest(w.seams()));
    CHECK_FALSE(log.initial_save.empty());
}

// ---- extra coverage ----------------------------------------------------------------

TEST_CASE("record_size and decode_turn frame a back-to-back stream") {
    const std::vector<sim::TurnEntry> entries{
        sim::TurnEntry{host_speed(100), 0},
        sim::TurnEntry{build_cmd(), 1},
        sim::TurnEntry{legacy_cmd(), 2},
    };
    std::vector<std::uint8_t> blob;
    for (const sim::TurnEntry& e : entries) {
        auto rec = sim::encode_turn_entry(e);
        REQUIRE(rec.ok());
        blob.insert(blob.end(), rec.value().begin(), rec.value().end());
    }

    std::vector<sim::TurnEntry> out;
    auto n = sim::decode_turn(blob, out);
    REQUIRE(n.ok());
    CHECK(n.value() == 3);
    CHECK(out == entries);

    // record_size frames the first ClientRecord (after the 8-byte issuer/seq header).
    auto rs = sim::record_size(std::span<const std::uint8_t>(blob).subspan(8));
    REQUIRE(rs.ok());
    CHECK(rs.value() >= 4);

    // A truncated stream is a clean error, never a crash or an over-read.
    for (std::size_t len = 0; len < blob.size(); ++len) {
        std::vector<sim::TurnEntry> partial;
        auto bad = sim::decode_turn(std::span<const std::uint8_t>(blob).first(len), partial);
        CHECK((bad.ok() || !bad.error().empty()));
    }
}

TEST_CASE("read_log rejects truncated and hostile input") {
    const bt::Kit k = bt::make_kit();
    sim::World w = bt::all_classes_world(k);
    sim::Recorder rec(w);
    rec.submit(host_speed(200));
    rec.advance(20);
    rec.checkpoint();
    const std::vector<std::uint8_t> good = sim::write_log(rec.log());
    REQUIRE(sim::read_log(good).ok());

    // Every prefix is rejected cleanly (not the whole log, which is valid).
    for (std::size_t len = 0; len < good.size(); ++len) {
        auto r = sim::read_log(std::span<const std::uint8_t>(good).first(len));
        CHECK_FALSE(r.ok());
    }
    // Trailing bytes are rejected.
    std::vector<std::uint8_t> trailing = good;
    trailing.push_back(0);
    CHECK_FALSE(sim::read_log(trailing).ok());

    // Garbage and an empty input are rejected, not crashed on.
    const std::vector<std::uint8_t> garbage{0xde, 0xad, 0xbe, 0xef, 1, 2, 3, 4, 5, 6, 7, 8};
    CHECK_FALSE(sim::read_log(garbage).ok());
    CHECK_FALSE(sim::read_log(std::span<const std::uint8_t>{}).ok());
}

TEST_CASE("PrintArchive dumps the stream and filters a scope") {
    const bt::Kit k = bt::make_kit();
    sim::World w = bt::all_classes_world(k);

    sim::PrintArchive full;
    fields(full, w);
    CHECK(full.text().find("header {") != std::string::npos);
    CHECK(full.text().find("islands {") != std::string::npos);

    // A filter restricts the printed primitives to one scope subtree.
    sim::PrintArchive filtered("islands/island[0]/cities/city[0]/index/list[4]/building[0]");
    fields(filtered, w);
    const std::string& t = filtered.text();
    CHECK(t.find("building[0] {") != std::string::npos);
    // The filter shows the chain down to the scope but not a sibling island scope.
    CHECK(t.find("settings {") == std::string::npos);
}

TEST_CASE("hash_per_entry and tree_at follow the log") {
    const bt::Kit k = bt::make_kit();
    sim::World w = bt::all_classes_world(k);
    sim::Recorder rec(w);
    rec.advance(20);
    rec.checkpoint();
    rec.advance(40);
    rec.checkpoint();
    const sim::ReplayLog log = rec.log();

    sim::World initial = bt::all_classes_world(k);
    const std::vector<std::uint64_t> hashes = sim::hash_per_entry(initial, log);
    REQUIRE(hashes.size() == log.entries.size());
    // The checkpoints' stored hashes equal the per-entry hashes at those positions.
    for (std::size_t i = 0; i < log.entries.size(); ++i) {
        if (log.entries[i].kind == sim::ReplayEntry::Kind::Checkpoint) {
            CHECK(hashes[i] == log.entries[i].hash);
        }
    }

    sim::World initial2 = bt::all_classes_world(k);
    const sim::HashTree tree = sim::tree_at(initial2, log, log.entries.size(), 0);
    CHECK(tree.root.hash == hashes.back());
}
