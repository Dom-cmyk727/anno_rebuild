// Per-pixel depth (docs/render/depth.md): the integer depth function, the per-format rules, the
// test and write of depth::test, and the depth replay of a chunk list. Synthetic, no game files.
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <anno/render/chunk_draws.hpp>
#include <anno/render/depth.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/frame_table.hpp>

namespace render = anno::render;
namespace depth = anno::render::depth;
namespace fig = anno::formats::figuren;
using anno::assets::Rgba;
using render::Px;
using render::Rect;

namespace {

/// A w x h opaque image of one colour, with `z` (all pixels) when given.
Rgba image(std::uint16_t w, std::uint16_t h, std::uint8_t alpha = 255, bool with_z = false,
           std::uint8_t z = 0) {
    Rgba img{w, h, {}, {}};
    for (std::size_t i = 0; i < std::size_t{w} * h; ++i) {
        img.pixels.insert(img.pixels.end(), {200, 100, 50, alpha});
    }
    if (with_z) {
        img.z.assign(std::size_t{w} * h, z);
    }
    return img;
}

/// A one-pixel target holding `d`.
depth::Target one(std::int32_t d) {
    depth::Target t{Rect{0, 0, 1, 1}, depth::params(0, render::kHeightLift), {d}, {0}};
    return t;
}

const depth::Rule kZ = depth::rule(fig::FrameFormat::RgbZ);
const depth::Rule kAZ = depth::rule(fig::FrameFormat::RgbAZ);
const depth::Rule kH = depth::rule(fig::FrameFormat::RgbH);
const depth::Rule kV = depth::rule(fig::FrameFormat::RgbV);

} // namespace

TEST_CASE("depth: the group constants from the original's camera (depth.md 2.1)") {
    // Hand-derived: A = 65536 * 0.00023663246 = 15.50794, B = 65536 * 0.00018957623 * 0.04 =
    // 0.4969627, L = 2.059 px per HEIGHT unit at group 0 (x 42/84, x 20/84 at groups 1, 2).
    // ca = round(65536 A / th), cb = round(1024 (2 A L / th + B)), sv = round(65536 B / L).
    CHECK(depth::params(0, render::kHeightLift) == depth::Params{24198, 2066, 15818, -48396});
    CHECK(depth::params(1, render::kHeightLift) == depth::Params{48397, 2066, 31636, -96794});
    CHECK(depth::params(2, render::kHeightLift) == depth::Params{101633, 2066, 66435, -203266});
    // The V step is the original's measured brush step at group 0 (0x3dc9 / 65536 = 0.24135 u16
    // per row, i.e. 0x3dc9 = 15817 in these units, annodisplay.md 9.1) within one unit: our lift
    // is 2.059 px per HEIGHT unit, the original's 51.477 * 0.04 = 2.05909 (15817.85 -> 15818).
    CHECK(std::abs(depth::params(0, render::kHeightLift).sv - 0x3dc9) <= 1);
}

TEST_CASE("depth: at() is nearer for higher points and lower rows, origin() splits it exactly") {
    const depth::Params p = depth::params(0, render::kHeightLift);
    // kZero at the origin row and height 0.
    CHECK(depth::at(p, 0, 0) == 8388608);
    // One row down (y2 + 2): floor((24198 * 2) / 256) = 189 units of 1/256 nearer.
    CHECK(depth::at(p, 2, 0) == 8388608 - 189);
    // One HEIGHT unit up at the same row: floor(2066 * 64 / 256) = 516 nearer (2.016 u16; the
    // original's 2.0455 with its 41.24 px tile).
    CHECK(depth::at(p, 0, 64) == 8388608 - 516);
    // Negative rows (scene y above 0) floor, not truncate: ca * -1 = -24198 -> -95 (not -94).
    CHECK(depth::at(p, -1, 0) == 8388608 + 95);
    for (std::int64_t y2o : {-30001, -2, 0, 1, 2048, 87001}) {
        const depth::Origin o = depth::origin(p, y2o);
        for (std::int64_t y2 : {0, 1, 2, 777, 1023}) {
            for (std::int64_t h : {0, 1, 1056, 3072, 13312, 0xFFFE}) {
                const std::int64_t split =
                    o.base - render::div_euclid<std::int64_t>(o.frac + p.ca * y2 + p.cb * h, 256);
                REQUIRE(split == depth::at(p, y2o + y2, h));
            }
        }
    }
}

TEST_CASE("depth: the format rules (D: AnnoDisplay.dll SelectBrush, depth-writing mode)") {
    CHECK(depth::rule(fig::FrameFormat::RgbZ) == depth::Rule{depth::Class::Z, true, false});
    CHECK(depth::rule(fig::FrameFormat::RgbZM) == depth::Rule{depth::Class::Z, true, false});
    CHECK(depth::rule(fig::FrameFormat::RgbAZ) == depth::Rule{depth::Class::Z, true, true});
    CHECK(depth::rule(fig::FrameFormat::RgbWZ) == depth::Rule{depth::Class::Z, false, false});
    CHECK(depth::rule(fig::FrameFormat::RgbV) == depth::Rule{depth::Class::V, true, false});
    CHECK(depth::rule(fig::FrameFormat::RgbAV) == depth::Rule{depth::Class::V, false, false});
    CHECK(depth::rule(fig::FrameFormat::RgbH) == depth::Rule{depth::Class::H, false, false});
    CHECK(depth::rule(fig::FrameFormat::RgbAH) == depth::Rule{depth::Class::H, false, false});
    CHECK(depth::rule(fig::FrameFormat::Alpha) == depth::Rule{});
    // The record flags round-trip.
    for (const auto f : {fig::FrameFormat::RgbZ, fig::FrameFormat::RgbAZ, fig::FrameFormat::RgbV,
                         fig::FrameFormat::RgbAH, fig::FrameFormat::Alpha}) {
        const depth::Rule r = depth::rule(f);
        CHECK(render::object_flags::depth_rule(render::object_flags::depth_bits(r)) == r);
    }
}

TEST_CASE("depth: test() draws at equal depth, adds the signed z, writes by the rule") {
    const Rgba z0 = image(1, 1, 255, true, 0);
    const Rgba zm1 = image(1, 1, 255, true, 0xFF);
    const Rgba zp1 = image(1, 1, 255, true, 1);
    SUBCASE("equal depth is drawn (the original's jb: skip only when the buffer is nearer)") {
        auto t = one(1000);
        CHECK(depth::test(t, 0, 0, z0, 0, 0, depth::Draw{1000, kZ}));
        CHECK(t.d[0] == 1000);
        CHECK(t.tested[0] == 1);
    }
    SUBCASE("z 0xFF is -1 (signed): nearer by 256, drawn and written") {
        auto t = one(1000);
        CHECK(depth::test(t, 0, 0, zm1, 0, 0, depth::Draw{1000, kZ}));
        CHECK(t.d[0] == 1000 - 256);
    }
    SUBCASE("z +1 is farther: hidden, nothing written or marked") {
        auto t = one(1000);
        CHECK_FALSE(depth::test(t, 0, 0, zp1, 0, 0, depth::Draw{1000, kZ}));
        CHECK(t.d[0] == 1000);
        CHECK(t.tested[0] == 0);
    }
    SUBCASE("RGBAZ: a level-1 pixel (alpha 128) is drawn but does not write, level 3 writes") {
        auto t = one(1000);
        const Rgba half = image(1, 1, 128, true, 0);
        CHECK(depth::test(t, 0, 0, half, 0, 0, depth::Draw{500, kAZ}));
        CHECK(t.d[0] == 1000);
        const Rgba full = image(1, 1, 255, true, 0);
        CHECK(depth::test(t, 0, 0, full, 0, 0, depth::Draw{500, kAZ}));
        CHECK(t.d[0] == 500);
        // Level 2 (alpha 191) writes too (only level 1 is the blended, non-writing one).
        auto t2 = one(1000);
        CHECK(depth::test(t2, 0, 0, image(1, 1, 191, true, 0), 0, 0, depth::Draw{400, kAZ}));
        CHECK(t2.d[0] == 400);
    }
    SUBCASE("V ramps from the frame's anchor row; H is the flat plane through the anchor") {
        // V at group 0, anchor row 4: row 10 is 6 rows below it, base + floor(15818 * 6 / 256) =
        // base + 370; row 0 is 4 rows above, base + floor(-63272 / 256) = base - 248 (floor).
        // Ramping from the top row instead (the wrong reading) would give base + 617 at row 10.
        const depth::Params& p = one(0).params;
        const depth::Draw v{1000, kV, 4, 0};
        CHECK(depth::pixel(p, v, 10, 0, 0) == 1000 + 370);
        CHECK(depth::pixel(p, v, 0, 0, 0) == 1000 - 248);
        CHECK(depth::pixel(p, v, 4, 0, 0) == 1000);
        auto tv = one(100000);
        CHECK(depth::test(tv, 0, 0, image(1, 11), 0, 10, v));
        CHECK(tv.d[0] == 1000 + 370);
        // H: at(y2, h64) of the anchor's height, whatever its base: at scene row 0, h 0 that is
        // kZero - floor(24198 / 256) = 8388608 - 94; it is tested and never written.
        const depth::Draw h{12345, kH, 21, 0};
        CHECK(depth::pixel(p, h, 3, 1, 0) == 8388608 - 94);
        auto th = one(8388608 - 94);
        CHECK(depth::test(th, 0, 0, image(1, 1), 0, 0, h));
        CHECK(th.d[0] == 8388608 - 94);
        auto th2 = one(8388608 - 95);
        CHECK_FALSE(depth::test(th2, 0, 0, image(1, 1), 0, 0, h));
    }
    SUBCASE("a pixel without depth (a unit) clears the mark; outside the target passes") {
        auto t = one(1000);
        CHECK(depth::test(t, 0, 0, z0, 0, 0, depth::Draw{1000, kZ}));
        depth::untest(t, 0, 0);
        CHECK(t.tested[0] == 0);
        CHECK(depth::test(t, 5, 5, zp1, 0, 0, depth::Draw{999999, kZ}));
    }
}

namespace {

render::FrameId add(render::FrameTable& ft, std::uint32_t id, Rgba img, std::int16_t ax,
                    std::int16_t ay) {
    fig::Frame f;
    f.width = img.width;
    f.height = img.height;
    f.anchor_x = ax;
    f.anchor_y = ay;
    auto shared = std::make_shared<Rgba>(std::move(img));
    return *ft.intern(
        render::FrameKey::unit({true, id, 0, render::UnitFrameId::kNoColour}, 0, false),
        [&] { return std::pair{f, shared}; });
}

/// A record of frame `id` (anchor ay) whose anchor stands at scene (ax_s, ay_s), height h, rule r.
render::ObjectRecord rec(render::FrameId id, int x, int y, int anchor_y, std::uint8_t h,
                         const depth::Rule& r) {
    const depth::Params p = depth::params(0, render::kHeightLift);
    const std::int32_t base = depth::at(p, 2 * std::int64_t{anchor_y}, 64 * std::int64_t{h});
    return {x, y, id, render::object_flags::depth_bits(r), 0, 0, base};
}

} // namespace

TEST_CASE("depth: replay hides a pile's rows below its foot in flat ground, the sea covers it") {
    // A 4 x 40 pile with z 0 whose anchor (its foot) is its bottom row, at scene (20, 50):
    // anchor_y = 39, frame top at row 11. Its pixel depth is the base, at(2 * 50, 64 h).
    // Flat terrain at height h everywhere: the terrain at row y is at(2y + 1, 64 h), nearer than
    // the base for 2y + 1 > 100, i.e. y >= 50. So rows 11..49 are drawn, none below: hand-derived
    // from the formula, and the depth-less replay (the negative control) draws all 40 rows.
    const Rect rect{0, 0, 64, 64};
    render::FrameTable ft;
    const auto pile = add(ft, 1, image(4, 40, 255, true, 0), 0, 39);
    for (const std::uint8_t h : {std::uint8_t{48}, std::uint8_t{0}}) {
        CAPTURE(int{h});
        render::ChunkDrawsBuilder b(rect, 0, false, 0, 16);
        b.begin_unclipped();
        b.add(rec(pile, 20, 11, 50, h, kZ));
        render::ChunkDraws d = b.finish(ft);
        const std::vector<std::uint16_t> terrain(64 * 64, static_cast<std::uint16_t>(64 * h));
        auto t = depth::Target::from_terrain(rect, depth::params(0, render::kHeightLift), terrain);
        const render::Canvas with = render::replay(d, ft, t);
        const render::Canvas without = render::replay(d, ft);
        auto drawn = [](const render::Canvas& c, int y) { return c.pixels[(y * 64 + 21) * 4 + 3]; };
        for (int y = 11; y < 51; ++y) {
            CAPTURE(y);
            CHECK(drawn(without, y) == 255);
            CHECK((drawn(with, y) == 255) == (y < 50));
        }
        // The submerged plane at the sea plane 16.5 (h64 1056): over ground at 48 nothing (the
        // ground is nearer than the plane, the pile's drawn pixels lie above it); over ground at 0
        // the pile's lowest rows: submerged where at(2y + 1, 1056) <= at(100, 0), i.e. where
        // floor((24198 (2y + 1) + 2066 * 1056) / 256) >= floor(24198 * 100 / 256) = 9452:
        // 2y + 1 >= 9452 * 256 / 24198 - 2066 * 1056 / 24198 = 99.99 - 90.16 -> y >= 5. All drawn
        // rows 11..49 then.
        const std::vector<std::uint8_t> sub = t.submerged(1056);
        if (h == 48) {
            CHECK(sub.empty());
        } else {
            REQUIRE(sub.size() == 64 * 64);
            for (int y = 11; y < 50; ++y) {
                CHECK(sub[static_cast<std::size_t>(y) * 64 + 21] == 1);
            }
            // A pixel no object covers is never submerged (the field rule handles the ground).
            CHECK(sub[static_cast<std::size_t>(30) * 64 + 40] == 0);
        }
    }
}

TEST_CASE("depth: an RGBV pile is cut at its anchor row, an H decal on its own ground is drawn") {
    // The pile of the test above as RGBV (4 x 40, anchor row 39 at scene row 50, flat ground at h
    // 0): its row y has depth base + floor(15818 (y - 50) / 256), the ground at(2y + 1, 0) =
    // kZero - floor(24198 (2y + 1) / 256), the base at(100, 0) = kZero - 9452. For y < 50 the pile
    // is nearer (it rises above the ground), at y = 50 the ground is nearer by 94: so rows 11..49
    // are drawn and none below. The top-row reading (base + 61.8 (y - 11)) would cut it at y >= 41.
    const Rect rect{0, 0, 64, 64};
    render::FrameTable ft;
    const auto pile = add(ft, 1, image(4, 40), 0, 39);
    const auto decal = add(ft, 2, image(8, 8), 4, 4);
    const std::vector<std::uint16_t> flat(64 * 64, 0);
    {
        render::ChunkDrawsBuilder b(rect, 0, false, 0, 16);
        b.begin_unclipped();
        auto r = rec(pile, 20, 11, 50, 0, kV);
        b.add(r);
        render::ChunkDraws d = b.finish(ft);
        auto t = depth::Target::from_terrain(rect, depth::params(0, render::kHeightLift), flat);
        const render::Canvas c = render::replay(d, ft, t);
        for (int y = 11; y < 51; ++y) {
            CAPTURE(y);
            CHECK((c.pixels[(y * 64 + 21) * 4 + 3] == 255) == (y < 50));
        }
    }
    // An H decal (8 x 8, anchor (4, 4)) on flat ground at its own height 10: every pixel equals
    // the ground's depth there (the same at() at the pixel centre), so all 64 are drawn; on ground
    // one HEIGHT unit higher (11) the ground is nearer everywhere and none is.
    for (const std::uint16_t g : {std::uint16_t{640}, std::uint16_t{704}}) {
        CAPTURE(g);
        render::ChunkDrawsBuilder b(rect, 0, false, 0, 16);
        b.begin_unclipped();
        auto r = rec(decal, 20, 20, 24, 10, kH);
        r.flags |= std::uint32_t{10} << render::object_flags::kHeightShift;
        b.add(r);
        render::ChunkDraws d = b.finish(ft);
        const std::vector<std::uint16_t> ground(64 * 64, g);
        auto t = depth::Target::from_terrain(rect, depth::params(0, render::kHeightLift), ground);
        const render::Canvas c = render::replay(d, ft, t);
        std::size_t drawn = 0;
        for (int y = 20; y < 28; ++y) {
            for (int x = 20; x < 28; ++x) {
                drawn += c.pixels[(y * 64 + x) * 4 + 3] == 255 ? 1U : 0U;
            }
        }
        CHECK(drawn == (g == 640 ? 64U : 0U));
    }
}

TEST_CASE("depth: draw order does not decide occlusion of opaque Z pixels, but does for H") {
    // Two 8 x 8 sprites on one spot: a near one (z -2) and a far one (z +2), both base B.
    // In either order the near one's pixels show (it writes); an H decal on the ground (height 0)
    // drawn after the sprite standing at height 10 is hidden where the sprite is.
    const Rect rect{0, 0, 32, 32};
    render::FrameTable ft;
    const auto nearf = add(ft, 1, image(8, 8, 255, true, 0xFE), 0, 8);
    const auto farf = add(ft, 2, image(8, 8, 255, true, 2), 0, 8);
    Rgba red = image(8, 8);
    for (std::size_t i = 0; i < 64; ++i) {
        red.pixels[i * 4] = 255;
        red.pixels[i * 4 + 1] = 0;
        red.pixels[i * 4 + 2] = 0;
    }
    const auto decal = add(ft, 3, std::move(red), 0, 8);
    auto build = [&](bool near_first) {
        render::ChunkDrawsBuilder b(rect, 0, false, 0, 16);
        b.begin_unclipped();
        auto r_near = rec(nearf, 4, 4, 12, 10, kZ);
        auto r_far = rec(farf, 4, 4, 12, 10, kZ);
        if (near_first) {
            b.add(r_near);
            b.add(r_far);
        } else {
            b.add(r_far);
            b.add(r_near);
        }
        return b.finish(ft);
    };
    for (const bool near_first : {true, false}) {
        CAPTURE(near_first);
        const render::ChunkDraws d = build(near_first);
        auto t = depth::Target::from_terrain(rect, depth::params(0, render::kHeightLift), {});
        const render::Canvas c = render::replay(d, ft, t);
        // The near sprite's colour, not the far one's: both are the same colour, so check the
        // written depth instead: base - 2 * 256 at every covered pixel.
        const std::int32_t base = depth::at(depth::params(0, render::kHeightLift), 24, 640);
        CHECK(t.d[static_cast<std::size_t>(6) * 32 + 6] == base - 512);
        CHECK(c.pixels[(6 * 32 + 6) * 4 + 3] == 255);
    }
    {
        // The decal at height 0 (no height bits: the flat plane at h 0) after the sprite: hidden.
        render::ChunkDrawsBuilder b(rect, 0, false, 0, 16);
        b.begin_unclipped();
        b.add(rec(nearf, 4, 4, 12, 10, kZ));
        b.add(rec(decal, 4, 4, 12, 10, kH));
        const render::ChunkDraws d = b.finish(ft);
        auto t = depth::Target::from_terrain(rect, depth::params(0, render::kHeightLift), {});
        const render::Canvas c = render::replay(d, ft, t);
        CHECK(c.pixels[(4 * 32 + 6) * 4] == 200); // the sprite's red channel, not the decal's 255
    }
}
