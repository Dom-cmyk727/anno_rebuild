#include <algorithm>
#include <array>
#include <utility>

#include <doctest/doctest.h>

#include <anno/sim/wildlife.hpp>
#include <anno/sim_load/wildlife_chunk.hpp>

namespace load = anno::sim_load;
namespace sim = anno::sim;
namespace {
using Record = load::WildlifeChunkRecord;
Record record(std::uint64_t token, std::uint8_t position, bool occupied, bool high,
              std::uint8_t time, std::uint16_t id, std::uint16_t goods,
              std::optional<std::uint8_t> byte11 = {}) {
    return {token, 0, position, occupied, high, time, id, goods, byte11};
}
Record nonspawn() {
    Record r;
    r.token = 90;
    r.category = 3;
    r.high_bit = true;
    r.timestamp = std::uint8_t{33};
    r.goods = std::uint16_t{999};
    r.byte11 = std::uint8_t{1};
    return r;
}
Record fresh(std::uint64_t token, std::uint8_t position, std::uint8_t timestamp) {
    Record r;
    r.token = token;
    r.position = position;
    r.occupied = false;
    r.timestamp = timestamp;
    r.goods = std::uint16_t{255};
    return r; // Unknown saved ID/high bit/byte11 remain absent.
}
load::WildlifeChunkInputs empty_input() {
    load::WildlifeChunkInputs in;
    in.habitat.fill(false); // Supplied known-false outcomes, never missing facts.
    return in;
}
void candidates(load::WildlifeChunkInputs& in, unsigned count) {
    unsigned index = 0;
    for (unsigned z = 1; z < 8; z += 2)
        for (unsigned x = 1; x < 8; x += 2)
            in.habitat[x + 8 * z] = index++ < count;
}
load::WildlifeChunkInputs main_input() {
    auto in = empty_input();
    in.chunk.flags = 0x8101;
    in.chunk.records = {record(10, 2, false, true, 11, 101, 7, std::uint8_t{9}), nonspawn(),
                        record(20, 9, false, false, 12, 202, 8),
                        record(30, 4, false, true, 13, 303, 9, std::uint8_t{204}),
                        record(40, 18, true, false, 14, 404, 10, std::uint8_t{77})};
    candidates(in, 14);
    in.time = 259000;
    in.fresh_tokens = {50};
    return in;
}
load::WildlifeChunkSnapshot final_snapshot() {
    return {0x8101,
            {fresh(50, 57, 3), record(10, 11, false, true, 3, 101, 255, std::uint8_t{9}),
             record(30, 59, false, true, 3, 303, 255, std::uint8_t{204}), nonspawn(),
             record(20, 9, false, false, 12, 202, 8),
             record(40, 18, true, false, 14, 404, 10, std::uint8_t{77})}};
}
constexpr std::array<std::uint32_t, 5> draws{0, 0, 1, 0, 999};
} // namespace
TEST_CASE("wildlife chunk: complete ordered maintenance matches literal records and draw trace") {
    const auto input = main_input();
    const auto before = input;
    const auto result = load::prepare_wildlife_chunk(input, draws);
    REQUIRE(result);
    const auto& out = result.value().completed;
    CHECK(out.chunk == final_snapshot());
    CHECK(out.recycled_order ==
          std::vector<Record>{input.chunk.records[3], input.chunk.records[0]});
    CHECK(out.retained_before_replenish == 2);
    CHECK(out.retained == 5);
    CHECK(out.desired == 5);
    CHECK(out.consumed == 4);
    CHECK(out.fresh_used == 1);
    CHECK(out.candidates == std::vector<std::uint8_t>{45, 47, 13, 15, 25, 27, 29, 31, 41, 43});
    CHECK(out.reuse.empty());
    CHECK(out.discarded.empty());
    CHECK_FALSE(out.reserved);
    CHECK(out.inserted ==
          std::vector<Record>{record(30, 59, false, true, 3, 303, 255, std::uint8_t{204}),
                              record(10, 11, false, true, 3, 101, 255, std::uint8_t{9}),
                              fresh(50, 57, 3)});
    Record before_fresh_insert;
    before_fresh_insert.token = 50;
    before_fresh_insert.position = std::uint8_t{57};
    before_fresh_insert.goods = std::uint16_t{255};
    CHECK(out.attempts ==
          std::vector<load::WildlifeChunkAttempt>{
              {0, 14, 9, 20, record(30, 9, false, true, 13, 303, 255, std::uint8_t{204})},
              {0, 13, 59, {}, record(30, 59, false, true, 13, 303, 255, std::uint8_t{204})},
              {1, 12, 11, {}, record(10, 11, false, true, 11, 101, 255, std::uint8_t{9})},
              {0, 11, 57, {}, before_fresh_insert}});
    CHECK_FALSE(out.chunk.records[0].saved_id);
    CHECK_FALSE(out.chunk.records[0].high_bit);
    CHECK_FALSE(out.chunk.records[0].byte11);
    CHECK(out.chunk.records[1].saved_id == 101);
    CHECK(out.chunk.records[2].saved_id == 303);
    CHECK(input == before);
    CHECK(draws == std::array<std::uint32_t, 5>{0, 0, 1, 0, 999});
    const auto again = load::prepare_wildlife_chunk(input, draws);
    REQUIRE(again);
    CHECK(again.value() == result.value());
}
TEST_CASE(
    "wildlife chunk: partial draw failures expose reserved state without cleanup or rollback") {
    const auto input = main_input();
    for (std::size_t count = 0; count < 4; ++count) {
        const auto result = load::prepare_wildlife_chunk(input, std::span(draws).first(count));
        REQUIRE_FALSE(result);
        CHECK(result.error().fault == load::WildlifeChunkFault::DrawsExhausted);
        CHECK(result.error().stage == load::WildlifeChunkStage::SelectCandidate);
        const auto& partial = result.error().diagnostic_only;
        CHECK(partial.consumed == count);
        CHECK(partial.attempts.size() == count);
        CHECK(partial.discarded.empty());
        CHECK(partial.chunk.flags == 0x8101);
        REQUIRE(partial.reserved);
        if (count < 2) {
            CHECK(partial.chunk.records ==
                  std::vector<Record>{nonspawn(), input.chunk.records[2], input.chunk.records[4]});
            CHECK(partial.reuse == std::vector<Record>{input.chunk.records[0]});
            CHECK(partial.retained == 2);
            CHECK(partial.fresh_used == 0);
            auto expected = input.chunk.records[3];
            if (count == 1) {
                expected.position = std::uint8_t{9};
                expected.goods = std::uint16_t{255};
            }
            CHECK(*partial.reserved == expected);
            CHECK(partial.candidates == (count == 0
                                             ? std::vector<std::uint8_t>{9, 11, 13, 15, 25, 27, 29,
                                                                         31, 41, 43, 45, 47, 57, 59}
                                             : std::vector<std::uint8_t>{59, 11, 13, 15, 25, 27, 29,
                                                                         31, 41, 43, 45, 47, 57}));
        } else if (count == 2) {
            CHECK(partial.chunk.records ==
                  std::vector<Record>{record(30, 59, false, true, 3, 303, 255, std::uint8_t{204}),
                                      nonspawn(), input.chunk.records[2], input.chunk.records[4]});
            CHECK(*partial.reserved == input.chunk.records[0]);
            CHECK(partial.reuse.empty());
            CHECK(partial.retained == 3);
            CHECK(partial.fresh_used == 0);
            CHECK(partial.candidates ==
                  std::vector<std::uint8_t>{57, 11, 13, 15, 25, 27, 29, 31, 41, 43, 45, 47});
        } else {
            CHECK(partial.chunk.records ==
                  std::vector<Record>{record(10, 11, false, true, 3, 101, 255, std::uint8_t{9}),
                                      record(30, 59, false, true, 3, 303, 255, std::uint8_t{204}),
                                      nonspawn(), input.chunk.records[2], input.chunk.records[4]});
            Record expected;
            expected.token = 50;
            CHECK(*partial.reserved ==
                  expected); // No candidate write yet, no invented fresh zero ID.
            CHECK(partial.reuse.empty());
            CHECK(partial.retained == 4);
            CHECK(partial.fresh_used == 1);
            CHECK(partial.candidates ==
                  std::vector<std::uint8_t>{57, 47, 13, 15, 25, 27, 29, 31, 41, 43, 45});
        }
    }
    CHECK(input == main_input());
}
TEST_CASE("wildlife chunk: excess records zero candidates and successful leftover discard") {
    auto input = empty_input();
    input.chunk.flags = 0xf001;
    const auto a = record(10, 2, false, true, 11, 101, 7, std::uint8_t{9});
    const auto b = record(30, 4, false, true, 13, 303, 9, std::uint8_t{204});
    input.chunk.records = {a, nonspawn(), b};
    auto result = load::prepare_wildlife_chunk(input, {});
    REQUIRE(result);
    const auto& cleared = result.value().completed;
    CHECK(cleared.chunk == load::WildlifeChunkSnapshot{0xf000, {nonspawn()}});
    CHECK(cleared.recycled_order == std::vector<Record>{b, a});
    CHECK(cleared.discarded == std::vector<Record>{b, a});
    CHECK(cleared.retained == 0);
    CHECK(cleared.desired == 0);
    CHECK(cleared.consumed == 0);
    CHECK(cleared.fresh_used == 0);
    CHECK(cleared.attempts.empty());
    CHECK(cleared.candidates.empty());
    CHECK_FALSE(cleared.reserved);
    input.chunk.records = {record(40, 18, true, false, 14, 404, 10, std::uint8_t{77}),
                           record(41, 20, true, true, 15, 405, 11), nonspawn()};
    input.chunk.flags = 0xab01;
    candidates(input, 1);
    // Neither occupied tile has a habitat fact: retention must not query it.
    input.habitat[18].reset();
    input.habitat[20].reset();
    result = load::prepare_wildlife_chunk(input, {});
    REQUIRE(result);
    CHECK(result.value().completed.chunk == input.chunk);
    CHECK(result.value().completed.retained == 2);
    CHECK(result.value().completed.desired == 1);
    CHECK(result.value().completed.consumed == 0);
    CHECK(result.value().completed.reuse.empty());
}
TEST_CASE("wildlife chunk: category availability and first unoccupied matching record control "
          "collision") {
    auto input = empty_input();
    candidates(input, 8); // Desired 3; two duplicate-position records count separately.
    input.chunk.flags = 0x8401;
    const auto k = record(20, 9, false, false, 12, 202, 8);
    const auto duplicate = record(21, 9, false, true, 13, 203, 9);
    input.chunk.records = {nonspawn(), k, duplicate};
    input.fresh_tokens = {50};
    input.time = 999;
    const std::array<std::uint32_t, 3> local_draws{0, 0, 999};
    const auto result = load::prepare_wildlife_chunk(input, local_draws);
    REQUIRE(result);
    const auto& out = result.value().completed;
    CHECK(out.chunk ==
          load::WildlifeChunkSnapshot{0x8401, {fresh(50, 31, 0), nonspawn(), k, duplicate}});
    REQUIRE(out.attempts.size() == 2);
    CHECK(out.attempts[0].collision == 20); // Not occupied, first of duplicate positions.
    CHECK(out.attempts[0].reserved_after_probe.token == 50);
    CHECK_FALSE(out.attempts[0].reserved_after_probe.timestamp);
    CHECK_FALSE(out.attempts[0].reserved_after_probe.saved_id);
    CHECK_FALSE(out.attempts[0].reserved_after_probe.occupied);
    CHECK(out.attempts[1].reserved_after_probe.token == 50);
    CHECK(out.consumed == 2);
    input.chunk.flags = 0x8400; // Same physical records, but original category gate skips lookup.
    const auto no_category = load::prepare_wildlife_chunk(input, local_draws);
    REQUIRE(no_category);
    CHECK(no_category.value().completed.chunk ==
          load::WildlifeChunkSnapshot{0x8401, {fresh(50, 9, 0), nonspawn(), k, duplicate}});
    CHECK(no_category.value().completed.consumed == 1);
    input = empty_input();
    candidates(input, 1);
    auto wrong_category = record(90, 9, true, true, 88, 99, 100);
    wrong_category.category = 3;
    input.chunk = {0x4201, {wrong_category}};
    input.fresh_tokens = {50};
    const auto wrong = load::prepare_wildlife_chunk(input, local_draws);
    REQUIRE(wrong);
    CHECK(wrong.value().completed.chunk ==
          load::WildlifeChunkSnapshot{0x4201, {fresh(50, 9, 0), wrong_category}});
    CHECK(wrong.value().completed.consumed == 1);
}
TEST_CASE(
    "wildlife chunk: invalid facts and missing allocation are typed noncommittable diagnostics") {
    const auto legal = main_input();
    REQUIRE(load::prepare_wildlife_chunk(legal, draws));
    for (unsigned variant = 0; variant < 8; ++variant) {
        auto bad = legal;
        if (variant == 0)
            bad.habitat[2].reset(); // Unoccupied retention query missing.
        if (variant == 1)
            bad.habitat[63].reset(); // Sixteenth candidate query missing, even though false.
        if (variant == 2)
            bad.chunk.records[1].token = 10;
        if (variant == 3)
            bad.fresh_tokens.push_back(50);
        if (variant == 4)
            bad.chunk.records[0].position = std::uint8_t{64};
        if (variant == 5)
            bad.chunk.records[0].occupied.reset();
        if (variant == 6)
            bad.allocation = static_cast<load::WildlifeChunkAllocation>(99);
        if (variant == 7)
            bad.fresh_tokens[0] = 20;
        const auto before = bad;
        const auto result = load::prepare_wildlife_chunk(bad, draws);
        REQUIRE_FALSE(result);
        CHECK(result.error().fault == load::WildlifeChunkFault::InvalidFacts);
        CHECK(result.error().stage == load::WildlifeChunkStage::Validate);
        CHECK(result.error().diagnostic_only.consumed == 0);
        CHECK(result.error().diagnostic_only.chunk == bad.chunk);
        CHECK(bad == before);
    }
    auto missing_allocation = legal;
    missing_allocation.fresh_tokens.clear();
    const auto result = load::prepare_wildlife_chunk(missing_allocation, draws);
    REQUIRE_FALSE(result);
    CHECK(result.error().fault == load::WildlifeChunkFault::InvalidFacts);
    CHECK(result.error().stage == load::WildlifeChunkStage::Reserve);
    CHECK(result.error().diagnostic_only.consumed == 3);
    CHECK(result.error().diagnostic_only.retained == 4);
    CHECK_FALSE(result.error().diagnostic_only.reserved);
    CHECK(result.error().diagnostic_only.discarded.empty());
}
TEST_CASE("wildlife chunk: complete result survives owners and empty valid chunk needs no work") {
    std::optional<load::WildlifeChunkPlan> owned;
    {
        auto input = main_input();
        std::vector<std::uint32_t> local_draws(draws.begin(), draws.end());
        auto result = load::prepare_wildlife_chunk(input, local_draws);
        REQUIRE(result);
        owned = std::move(result.value());
    }
    REQUIRE(owned);
    CHECK(owned->completed.chunk == final_snapshot());
    CHECK(owned->completed.recycled_order.size() == 2);
    CHECK(owned->completed.attempts.size() == 4);
    CHECK(owned->completed.attempts[0].reserved_after_probe.saved_id == 303);
    auto empty = empty_input();
    empty.chunk.flags = 0xfffe;
    const auto result = load::prepare_wildlife_chunk(empty, {});
    REQUIRE(result);
    CHECK(result.value().completed.chunk == empty.chunk);
    CHECK(result.value().completed.consumed == 0);
    CHECK(result.value().completed.fresh_used == 0);
}
TEST_CASE("wildlife chunk: density domain tight legal collision and lower-level exhaustion scope") {
    const std::array<std::uint32_t, 17> expected{0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 5, 5, 5};
    for (std::uint32_t n = 0; n < expected.size(); ++n) {
        const auto density = sim::wildlife_density(n);
        REQUIRE(density);
        CHECK(density.value().value == expected[n]);
        CHECK(density.value().value <= n);
        CHECK(density.value().consumed == 0);
    }
    // N = 4 is the smallest density domain allowing R = 1 and a positive deficit.
    auto input = empty_input();
    candidates(input, 4);
    const auto k = record(0, 9, false, false, 0, 0, 0);
    input.chunk = {0x8801, {k}};
    input.fresh_tokens = {50};
    const std::array<std::uint32_t, 2> collision_draws{0, 0};
    const auto result = load::prepare_wildlife_chunk(input, collision_draws);
    REQUIRE(result);
    const auto& out = result.value().completed;
    CHECK(out.chunk == load::WildlifeChunkSnapshot{0x8801, {fresh(50, 15, 0), k}});
    CHECK(out.retained_before_replenish == 1);
    CHECK(out.retained == 2);
    CHECK(out.desired == 2);
    CHECK(out.consumed == 2);
    CHECK(out.fresh_used == 1);
    CHECK(out.candidates == std::vector<std::uint8_t>{13, 11});
    REQUIRE(out.attempts.size() == 2);
    CHECK(out.attempts[0].collision == 0); // Known token 0 is a collision, not absence.
    REQUIRE(out.chunk.records[1].saved_id);
    CHECK(out.chunk.records[1].saved_id == 0);
    CHECK(out.attempts[0].reserved_after_probe.token == 50);
    CHECK(out.attempts[1].reserved_after_probe.token == 50);
    // Lower-level API accepts an independent deficit/collision set. This proves
    // its named error only, NOT reachability in the consistent complete chunk.
    const std::array<std::uint32_t, 1> cells{9}, one_draw{0};
    const auto legal = sim::wildlife_replenish(cells, 1, {}, one_draw);
    REQUIRE(legal);
    CHECK(legal.value().value.chosen == std::vector<std::uint32_t>{9});
    CHECK(legal.value().value.remaining.empty());
    CHECK(legal.value().consumed == 1);
    const auto exhausted = sim::wildlife_replenish(cells, 1, cells, one_draw);
    REQUIRE_FALSE(exhausted);
    CHECK(exhausted.error().fault == sim::WildlifeFault::Oq572CandidatesExhausted);
    CHECK(exhausted.error().consumed == 1);
}
