// OBJECTPOS sub-records (anno::formats::objectpos): synthetic cases everywhere, the Anlagen.dat
// corpus with ANNO1503_GAME_DIR. Spec: docs/formats/anlagen.md "OBJECTPOS".
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/objectpos.hpp>
#include <anno/test/game_dir.hpp>

namespace op = anno::formats::objectpos;

TEST_CASE("objectpos: sub-records, CONPO2 arrows and the round trip") {
    // FIRE with 4 bytes, CONPO2 with two arrows (the second with side 215, kept), CONPOS kept raw.
    const std::vector<std::uint8_t> rec = {'F', 'I', 'R', 'E', 0,   0,   4,   0,   1, 2, 3, 4,
                                           'C', 'O', 'N', 'P', 'O', '2', 6,   0,   0, 2, 1, 4,
                                           4,   215, 'C', 'O', 'N', 'P', 'O', 'S', 2, 0, 0, 4};
    const auto parsed = op::ObjectPos::parse(rec);
    REQUIRE(parsed.ok());
    const auto& o = parsed.value();
    REQUIRE(o.records.size() == 3);
    CHECK(o.records[0].name_str() == "FIRE");
    CHECK(o.records[0].data == std::vector<std::uint8_t>{1, 2, 3, 4});
    CHECK(o.records[2].name_str() == "CONPOS");
    CHECK(o.conpo2() == std::vector<op::Arrow>{{0, 2, 1}, {4, 4, 215}});
    CHECK(o.to_bytes() == rec);
    // No CONPO2: no arrows. Empty record: no sub-records.
    const std::vector<std::uint8_t> fire = {'F', 'I', 'R', 'E', 0, 0, 0, 0};
    CHECK(op::ObjectPos::parse(fire).value().conpo2().empty());
    CHECK(op::ObjectPos::parse(std::vector<std::uint8_t>{}).value().records.empty());
}

TEST_CASE("objectpos: malformed records are errors") {
    // A header cut short.
    CHECK_FALSE(op::ObjectPos::parse(std::vector<std::uint8_t>{'F', 'I', 'R', 'E', 0, 0, 4}).ok());
    // Data past the end.
    CHECK_FALSE(
        op::ObjectPos::parse(std::vector<std::uint8_t>{'F', 'I', 'R', 'E', 0, 0, 4, 0, 1, 2}).ok());
    // A CONPO2 size that is not a multiple of 3.
    CHECK_FALSE(op::ObjectPos::parse(
                    std::vector<std::uint8_t>{'C', 'O', 'N', 'P', 'O', '2', 4, 0, 1, 2, 3, 4})
                    .ok());
}

TEST_CASE("objectpos corpus") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping objectpos corpus: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = anno::test::read_file(*dir / "Anlagen.dat");
    const auto file = anno::formats::anlagen::AnlagenFile::parse(buf);
    REQUIRE(file.ok());
    std::size_t with_record = 0, with_conpo2 = 0, arrows = 0;
    std::map<std::string, std::size_t> names;
    std::map<std::uint32_t, std::size_t> odd_side;
    for (const auto& e : file.value().entries) {
        const auto rec = e.record("OBJECTPOS");
        if (!rec)
            continue;
        ++with_record;
        const auto parsed = op::ObjectPos::parse(*rec);
        REQUIRE_MESSAGE(parsed.ok(), "entry " << e.id << ": " << parsed.error());
        const auto& o = parsed.value();
        const auto back = o.to_bytes();
        CHECK_MESSAGE(std::equal(back.begin(), back.end(), rec->begin(), rec->end()),
                      "entry " << e.id << " does not write back byte for byte");
        for (const auto& s : o.records)
            ++names[s.name_str()];
        const auto a = o.conpo2();
        if (!a.empty())
            ++with_conpo2;
        arrows += a.size();
        for (const auto& r : a)
            if (r.side > 3)
                ++odd_side[e.id];
    }
    CHECK(with_record == 426);
    CHECK(with_conpo2 == 184);
    CHECK(arrows == 852);
    CHECK(names == std::map<std::string, std::size_t>{{"AUSPOS", 15},
                                                      {"CONPO2", 184},
                                                      {"CONPOS", 1},
                                                      {"FIRE", 281},
                                                      {"HIMAP", 11},
                                                      {"SCHUSS", 102},
                                                      {"WPLANE", 60}});
    // The odd side bytes are kept (types 261 and 1571), as is CONPOS (1542, counted above).
    CHECK(odd_side == std::map<std::uint32_t, std::size_t>{{261, 1}, {1571, 1}});
}
