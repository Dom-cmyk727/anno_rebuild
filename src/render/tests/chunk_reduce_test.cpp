// reduce_chunks_2x (chunk_reduce.hpp): the CPU reference of a derived zoom
// level. Hand-computed 2 x 2 cases per plane.
#include <doctest/doctest.h>

#include <anno/render/chunk_reduce.hpp>

namespace {

using anno::render::Chunk;
using anno::render::ChunkQuad;
using anno::render::kChunk;
using anno::render::kNoTerrain;
using anno::render::Px;

constexpr std::size_t kN = static_cast<std::size_t>(kChunk);

/// A chunk with every plane, all open sea.
Chunk blank() {
    Chunk c;
    c.ground.assign(kN * kN, 0x102030U);
    c.objects.assign(kN * kN, Px{0, 0, 0, 0});
    c.field.assign(kN * kN * 4, 0);
    for (std::size_t i = 0; i < kN * kN; ++i) {
        c.field[i * 4 + 0] = 255;
        c.field[i * 4 + 1] = 255;
        c.field[i * 4 + 2] = 255;
        c.field[i * 4 + 3] = 1;
    }
    c.terrain.assign(kN * kN, kNoTerrain);
    return c;
}

std::size_t at(std::size_t x, std::size_t y) {
    return y * kN + x;
}

constexpr Px kWater{10, 20, 30, 255};

} // namespace

TEST_CASE("reduce_chunks_2x: no sources or empty planes give an empty chunk") {
    const Chunk none = anno::render::reduce_chunks_2x({nullptr, nullptr, nullptr, nullptr}, kWater);
    CHECK(none.ground.empty());
    CHECK(none.objects.empty());
    CHECK(none.field.empty());
    CHECK(none.terrain.empty());
    CHECK(none.base.empty());
    const Chunk empty;
    const Chunk also = anno::render::reduce_chunks_2x({&empty, nullptr, &empty, nullptr}, kWater);
    CHECK(also.ground.empty());
    CHECK(also.field.empty());
}

TEST_CASE("reduce_chunks_2x: terrain and ground average the texels that have ground") {
    Chunk a = blank();
    // Block (0, 0): terrain 100 and 101 at two texels, no ground at the others.
    a.terrain[at(0, 0)] = 100;
    a.terrain[at(1, 0)] = 101;
    a.ground[at(0, 0)] = 0x0A0000U | 0x0000C8U; // r 10, b 200
    a.ground[at(1, 0)] = 0x140000U | 0x000064U; // r 20, b 100
    // Block (1, 0): one texel with ground, the rest none.
    a.terrain[at(2, 1)] = 7;
    a.ground[at(2, 1)] = 0x010203U;
    const Chunk out = anno::render::reduce_chunks_2x({&a, nullptr, nullptr, nullptr}, kWater);
    REQUIRE(out.terrain.size() == kN * kN);
    // (100 + 101) / 2 = 100.5 rounds up.
    CHECK(out.terrain[at(0, 0)] == 101);
    CHECK(out.ground[at(0, 0)] == (0x0F0000U | 0x00000000U | 0x000096U)); // r 15, g 0, b 150
    // A lone texel with ground keeps its value, undiluted by the three without.
    CHECK(out.terrain[at(1, 0)] == 7);
    CHECK(out.ground[at(1, 0)] == 0x010203U);
    // No ground in the block: no terrain, the open-water colour.
    CHECK(out.terrain[at(2, 0)] == kNoTerrain);
    CHECK(out.ground[at(2, 0)] == (10U << 16U | 20U << 8U | 30U));
    // The other quadrants had no source.
    CHECK(out.terrain[at(300, 300)] == kNoTerrain);
}

TEST_CASE("reduce_chunks_2x: each source lands in its own quadrant") {
    Chunk q[4] = {blank(), blank(), blank(), blank()};
    for (std::size_t i = 0; i < 4; ++i) {
        q[i].terrain[at(0, 0)] = static_cast<std::uint16_t>(10 + i);
        q[i].terrain[at(1, 1)] = static_cast<std::uint16_t>(10 + i);
    }
    const Chunk out = anno::render::reduce_chunks_2x({&q[0], &q[1], &q[2], &q[3]}, kWater);
    CHECK(out.terrain[at(0, 0)] == 10);
    CHECK(out.terrain[at(256, 0)] == 11);
    CHECK(out.terrain[at(0, 256)] == 12);
    CHECK(out.terrain[at(256, 256)] == 13);
}

TEST_CASE("reduce_chunks_2x: field flags OR the two water bits and average the slope") {
    Chunk a = blank();
    // Block (0, 0): two water texels with slopes 3 and 7 (5 on average), one with
    // the river bit, one land texel (cover 0) whose slope bits must not count.
    auto set = [&](std::size_t x, std::size_t y, std::uint8_t cover, std::uint8_t flags) {
        a.field[at(x, y) * 4 + 0] = cover;
        a.field[at(x, y) * 4 + 3] = flags;
    };
    set(0, 0, 255, static_cast<std::uint8_t>(3U << 2U | 1U));
    set(1, 0, 101, static_cast<std::uint8_t>(7U << 2U | 2U));
    set(0, 1, 0, static_cast<std::uint8_t>(40U << 2U));
    set(1, 1, 255, static_cast<std::uint8_t>(3U << 2U | 1U));
    // Slopes 3, 7, 3 over three water texels: mean 13 / 3 = 4.33 -> 4.
    const Chunk out = anno::render::reduce_chunks_2x({&a, nullptr, nullptr, nullptr}, kWater);
    const std::uint8_t flags = out.field[at(0, 0) * 4 + 3];
    CHECK((flags & 3U) == 3U);
    CHECK((flags >> 2U) == 4U);
    // cover (255 + 101 + 0 + 255) / 4 = 152.75 -> 153; shore, depth stay 255.
    CHECK(out.field[at(0, 0) * 4 + 0] == 153);
    CHECK(out.field[at(0, 0) * 4 + 1] == 255);
    CHECK(out.field[at(0, 0) * 4 + 2] == 255);
    // Two different slopes only: 3 and 7 -> 5 (a maximum would give 7).
    Chunk b = blank();
    b.field[at(2, 0) * 4 + 3] = static_cast<std::uint8_t>(3U << 2U | 1U);
    b.field[at(3, 0) * 4 + 3] = static_cast<std::uint8_t>(7U << 2U | 1U);
    b.field[at(2, 1) * 4 + 3] = static_cast<std::uint8_t>(3U << 2U | 1U);
    b.field[at(3, 1) * 4 + 3] = static_cast<std::uint8_t>(7U << 2U | 1U);
    const Chunk ob = anno::render::reduce_chunks_2x({&b, nullptr, nullptr, nullptr}, kWater);
    CHECK((ob.field[at(1, 0) * 4 + 3] >> 2U) == 5U);
    // A block with no sea at all has slope 0 and keeps no bits.
    Chunk c = blank();
    for (std::size_t i = 0; i < 4; ++i) {
        c.field[at(i % 2, i / 2) * 4 + 0] = 0;
        c.field[at(i % 2, i / 2) * 4 + 3] = static_cast<std::uint8_t>(9U << 2U);
    }
    const Chunk oc = anno::render::reduce_chunks_2x({&c, nullptr, nullptr, nullptr}, kWater);
    CHECK(oc.field[at(0, 0) * 4 + 3] == 0);
}

TEST_CASE("reduce_chunks_2x: a missing field is open sea") {
    Chunk a = blank();
    a.field[at(0, 0) * 4 + 0] = 0;
    const Chunk with = anno::render::reduce_chunks_2x({&a, nullptr, nullptr, nullptr}, kWater);
    // The three quadrants without a source are the open-sea texel.
    CHECK(with.field[at(400, 400) * 4 + 0] == 255);
    CHECK(with.field[at(400, 400) * 4 + 3] == 1);
}

TEST_CASE("reduce_chunks_2x: objects are averaged premultiplied") {
    Chunk a = blank();
    a.objects[at(0, 0)] = Px{200, 0, 0, 255};
    a.objects[at(1, 0)] = Px{0, 0, 200, 85};
    const Chunk out = anno::render::reduce_chunks_2x({&a, nullptr, nullptr, nullptr}, kWater);
    // alpha (255 + 85) / 4 = 85; red 51000 / 340 = 150, blue 17000 / 340 = 50
    // (the premultiplied sums over the summed alpha, all exact).
    const Px p = out.objects[at(0, 0)];
    CHECK(p[3] == 85);
    CHECK(p[0] == 150);
    CHECK(p[1] == 0);
    CHECK(p[2] == 50);
    // A block with no alpha stays fully transparent black.
    CHECK(out.objects[at(5, 5)] == Px{0, 0, 0, 0});
    // A translucent pixel next to an opaque one is not dragged to the straight
    // mean (100 would be the straight mean of 200 and 0).
    CHECK(p[0] > 100);
}

TEST_CASE("reduce_chunks_2x: base and flow are not derived, the flags combine") {
    Chunk a = blank();
    a.base.assign(kN * kN, 3);
    a.water_below = true;
    a.ships = true;
    Chunk b = blank();
    b.water_above = true;
    b.ships = false;
    const Chunk out = anno::render::reduce_chunks_2x({&a, &b, nullptr, nullptr}, kWater);
    CHECK(out.base.empty());
    CHECK(out.flow.nx == 0);
    CHECK(out.flow_waiting.empty());
    CHECK(out.water_below);
    CHECK(out.water_above);
    CHECK_FALSE(out.ships);
}
