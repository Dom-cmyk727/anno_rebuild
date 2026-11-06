// Stream 135 (docs/sim/salt-extraction.md sections 11 and 13): the HIGHMAP tables of the original
// class-0x3000 types enter the registry; no other type carries one. Needs ANNO1503_GAME_DIR.
#include <algorithm>
#include <array>
#include <iostream>

#include <doctest/doctest.h>

#include "tutorial_town.hpp"

TEST_CASE("salt registry: the original HIGHMAP tables of the twelve class-0x3000 types") {
    const auto o = tutorial_town::load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    namespace sim = anno::sim;
    std::size_t with_table = 0, class3000 = 0;
    for (const sim::BuildingType& t : o->types->types()) {
        const bool mountain = (t.flags_word & 0x7000U) == 0x3000U;
        class3000 += mountain ? 1U : 0U;
        if (!mountain) {
            CHECK_MESSAGE(t.highmap.empty(), "type " << t.id.v << " is not class 0x3000");
            continue;
        }
        // every shipped class-0x3000 record is the 2h x 2w table of its footprint
        CHECK_MESSAGE(t.highmap.size() == std::size_t{4} * t.width * t.height, "type " << t.id.v);
        with_table += t.highmap.empty() ? 0U : 1U;
    }
    CHECK(class3000 == 12);
    CHECK(with_table == 12);
    // the salt mine 1203, 3x4: the bytes of the Anlagen.dat HIGHMAP record (docs section 11)
    const sim::BuildingType* salt = o->types->find(sim::TypeId{1203});
    REQUIRE(salt != nullptr);
    const std::array<std::uint8_t, 48> want{
        255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 32,  32,  255, 255, 255, 255, 16,  16,  255, 255, 255, 255,
        16,  16,  16,  16,  255, 16,  12,  12,  16,  16,  255, 255, 4,   4,   255, 255};
    REQUIRE(salt->highmap.size() == 48);
    CHECK(std::equal(want.begin(), want.end(), salt->highmap.begin()));
    // the quarry 1201 carries ITS Anlagen.dat record (Data/Highmap.dat differs for it: unread which
    // wins)
    const sim::BuildingType* quarry = o->types->find(sim::TypeId{1201});
    REQUIRE(quarry != nullptr);
    CHECK(quarry->highmap.size() == 48);
    CHECK(quarry->highmap != salt->highmap);
}
