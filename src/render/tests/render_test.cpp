// Synthetic tests of anno_render (no game files needed).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

#include <doctest/doctest.h>

#include <anno/render/frame.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/profile.hpp>
#include <anno/render/scene.hpp>
#include <anno/render/sea.hpp>

#include "water_field_fixture.hpp"

namespace render = anno::render;
using render::Rect;

TEST_CASE("corner heights and interpolation") {
    // 2 × 2 heights: corners of tile (0, 0) are the four values; tile (1, 1)
    // clamps to the island edge.
    const std::vector<std::uint8_t> h = {0, 10, 20, 30};
    CHECK(render::corners(h, 2, 2, 0, 0) == std::array<std::int64_t, 4>{0, 10, 20, 30});
    CHECK(render::corners(h, 2, 2, 1, 1) == std::array<std::int64_t, 4>{30, 30, 30, 30});
    // c = [A, C, B, D]; results scaled by n = 2.
    const std::array<std::int64_t, 4> c = {0, 10, 20, 30};
    CHECK(render::interpolate(c, 0, 0, 2) == 0);
    CHECK(render::interpolate(c, 2, 0, 2) == 20);
    CHECK(render::interpolate(c, 0, 2, 2) == 40);
    CHECK(render::interpolate(c, 2, 2, 2) == 60);
    // The centre lies on the B–C diagonal: (B + C) / 2 = 15.
    CHECK(render::interpolate(c, 1, 1, 2) == 30);
    // Off the diagonal the two triangles differ from bilinear blending.
    CHECK(render::interpolate({0, 0, 0, 40}, 3, 3, 4) == 4 * 20);
    CHECK(render::interpolate({40, 0, 0, 0}, 1, 1, 4) == 4 * 20);
    // terrain_height agrees with the integer form at the samples.
    CHECK(render::terrain_height(h, 2, 2, 0.5F, 0.5F) == doctest::Approx(15.0));
    // Lift: 1 px per unit at group 0, half at group 1 (tile width 42).
    const render::Lift lift{1, 1};
    CHECK(lift.scaled(30, 2, 0) == 15);
    CHECK(lift.scaled(30, 2, 1) == 7);
    CHECK(render::kNoLift.pixels(200, 0) == 0);
}

TEST_CASE("wave noise is deterministic and bounded") {
    for (int i = 0; i < 200; ++i) {
        auto f = [](int v) { return static_cast<float>(v) * 0.37F; };
        const float n = render::noise3(f(i), f(i * 3), f(i * 7));
        CHECK(n >= 0.0F);
        CHECK(n < 1.0F);
        CHECK(n == render::noise3(f(i), f(i * 3), f(i * 7)));
    }
    const render::Px water = {30, 90, 140, 255};
    auto enhance = [&](float shore, std::uint64_t t) {
        const auto s = render::wave_shade(shore, shore * 4.0F, 1.0F, {10.0F, 4.0F}, t, 1.0F);
        return std::pair{render::shade_apply(water, s), s.foam};
    };
    const auto a = enhance(0.3F, 1234);
    CHECK(a == enhance(0.3F, 1234));
    CHECK(a.second >= 0.0F);
    CHECK(a.second <= 1.0F);
    // Waves move: the same spot differs over time.
    bool moved = false;
    for (std::uint64_t k = 1; k < 20; ++k) {
        moved = moved || enhance(1.0F, k * 150).first != enhance(1.0F, (k - 1) * 150).first;
    }
    CHECK(moved);
}

TEST_CASE("projection") {
    CHECK(render::tile_top(0, 0, 0) == std::pair{0, 0});
    CHECK(render::tile_top(1, 0, 0) == std::pair{42, 21});
    CHECK(render::tile_top(0, 1, 0) == std::pair{-42, 21});
    CHECK(render::tile_top(2, 2, 1) == std::pair{0, 42});
}

TEST_CASE("layout: offsets, bounds and order") {
    // Island 0 lies further front (larger x + w + y + h) than island 1.
    const std::vector<render::IslandBox> boxes = {{10, 4, 8, 6}, {0, 0, 4, 4}};
    const auto p = render::layout(boxes, 0);
    REQUIRE(p.size() == 2);
    CHECK(p[0].index == 1);
    CHECK(p[1].index == 0);
    CHECK(p[1].offset == std::pair{(10 - 4) * 42, (10 + 4) * 21});
    CHECK(p[1].bounds == Rect{6 * 42 - 6 * 42, 14 * 21 - render::margin(0), (8 + 6) * 42,
                              (8 + 6) * 21 + render::margin(0)});
    CHECK(p[0].offset == std::pair{0, 0});
    CHECK(render::scene_bounds(p) == Rect{-4 * 42, -render::margin(0), 4 * 42 + 6 * 42 + 8 * 42,
                                          14 * 21 + 14 * 21 + render::margin(0)});
    CHECK_FALSE(render::scene_bounds({}));
    // The bounds rectangle equals Canvas::for_island shifted by the offset.
    const std::vector<render::IslandBox> one = {{3, 5, 7, 2}};
    const auto q = render::layout(one, 1)[0];
    const auto [tw, th] = render::kTile[1];
    CHECK(q.offset == render::tile_top(3, 5, 1));
    CHECK(q.bounds.x - q.offset.first == -2 * tw / 2);
    CHECK(q.bounds.w == 9 * tw / 2);
    CHECK(q.bounds.h == 9 * th / 2 + render::margin(1));
}

TEST_CASE("unit anchor projects the world point onto the sea surface") {
    render::UnitSprite u{10.5F, 4.5F, std::nullopt, {}, {}};
    // Group 2 tile 20 × 10: ((x − y)·10, (x + y)·5).
    CHECK(u.anchor(2, render::kNoLift) == std::pair{60, 75});
    // Sea surface 32 units at 2.059 px per unit and group 0, scaled by 20/84.
    CHECK(u.anchor(2, render::kHeightLift) == std::pair{60, 60});
    // On the ground at height 16: half the sea-surface lift.
    u.z = 16.0F;
    CHECK(u.anchor(2, render::kHeightLift) == std::pair{60, 68});
}

TEST_CASE("rect operations") {
    const Rect a{0, 0, 10, 10}, b{5, -5, 10, 10};
    CHECK(a.intersect(b) == Rect{5, 0, 5, 5});
    CHECK(a.unite(b) == Rect{0, -5, 15, 15});
    CHECK_FALSE(a.intersect(Rect{10, 0, 10, 10}));
}

namespace {
render::Px px(const render::Canvas& c, std::size_t x, std::size_t y) {
    const std::size_t o = (y * c.width + x) * 4;
    return {c.pixels[o], c.pixels[o + 1], c.pixels[o + 2], c.pixels[o + 3]};
}
} // namespace

TEST_CASE("composite places, clips and blends") {
    render::Canvas dst = render::blank(Rect{100, 50, 4, 3});
    for (std::size_t i = 0; i < dst.pixels.size(); i += 4) {
        dst.pixels[i + 2] = 200;
        dst.pixels[i + 3] = 255;
    }
    // 2 × 2 source with local origin (−1, −1): opaque, transparent,
    // half-transparent, opaque.
    render::Canvas src = render::blank(Rect{-1, -1, 2, 2});
    src.pixels = {255, 0, 0, 255, 9, 9, 9, 0, 200, 100, 0, 128, 0, 255, 0, 255};
    // Local (−1, −1) → scene (102, 50) = dst pixel (2, 0).
    render::composite(dst, src, {103, 51});
    CHECK(px(dst, 2, 0) == render::Px{255, 0, 0, 255});
    CHECK(px(dst, 3, 0) == render::Px{0, 0, 200, 255});
    CHECK(px(dst, 3, 1) == render::Px{0, 255, 0, 255});
    // 128/255 of (200, 100, 0) over (0, 0, 200).
    CHECK(px(dst, 2, 1) == render::Px{100, 50, 99, 255});
    CHECK(px(dst, 1, 1) == render::Px{0, 0, 200, 255});
    // Partly outside: only the overlapping pixel (dst (0, 2)) changes.
    render::composite(dst, src, {101, 53});
    CHECK(px(dst, 0, 2) == render::Px{255, 0, 0, 255});
    // Fully outside: nothing happens.
    const auto before = dst.pixels;
    render::composite(dst, src, {0, 0});
    CHECK(dst.pixels == before);
}

TEST_CASE("over onto transparent keeps the colour") {
    render::Px d = {0, 0, 0, 0};
    render::over(d, render::Px{10, 20, 30, 77});
    CHECK(d == render::Px{10, 20, 30, 77});
}

TEST_CASE("visible chunks") {
    const auto v = render::visible_chunks(Rect{-2, -1, 4, 3});
    CHECK(v == std::vector<std::pair<int, int>>{{-1, -1}, {0, -1}, {-1, 0}, {0, 0}});
    CHECK(render::zoomed_view(Rect{0, 0, 100, 100}, 0, 1) == Rect{-25, -25, 100, 100});
}

TEST_CASE("numeric helpers follow the documented semantics") {
    CHECK(render::sat_cast<int>(std::numeric_limits<float>::quiet_NaN()) == 0);
    CHECK(render::sat_cast<std::uint8_t>(-3.5F) == 0);
    CHECK(render::sat_cast<std::uint8_t>(300.0F) == 255);
    CHECK(render::sat_cast<int>(-2.7F) == -2);
    CHECK(render::div_euclid(-7, 4) == -2);
    CHECK(render::rem_euclid(-7, 4) == 1);
    CHECK(render::total_less(-0.0F, 0.0F));
    CHECK_FALSE(render::total_less(0.0F, -0.0F));
    CHECK(render::fmaxf(std::numeric_limits<float>::quiet_NaN(), 1.0F) == 1.0F);
}

TEST_CASE("div_euclid at exact multiples of kChunk, and where a chunk index of -2 comes from") {
    constexpr int k = render::kChunk;
    CHECK(render::div_euclid(-k, k) == -1);
    CHECK(render::rem_euclid(-k, k) == 0);
    CHECK(render::div_euclid(-k - 1, k) == -2);
    CHECK(render::rem_euclid(-k - 1, k) == k - 1);
    CHECK(render::div_euclid(-1, k) == -1);
    CHECK(render::div_euclid(0, k) == 0);
    CHECK(render::div_euclid(k - 1, k) == 0);
    CHECK(render::div_euclid(k, k) == 1);
    // The T1 note "-2 instead of -1 for -512 / 512" came from a stand-in source, not from
    // the division: the first pixel of destination chunk -1 (x = -512) reads the source
    // group at the scaled coordinate, and that lies in source chunk -2 (zoom-in: kTile
    // ratio 2, source = 2 * x + 1 = -1023). div_euclid(-1023, 512) == -2 is right.
    const int src = render::stand_in_source(-k, render::kTile[0].w, render::kTile[1].w);
    CHECK(src == -1023);
    CHECK(render::div_euclid(src, k) == -2);
    // The destination coordinate itself is chunk -1.
    CHECK(render::div_euclid(-k, k) == -1);
}

TEST_CASE("water field of an island without land: NaN distances become 0") {
    // All corners below the waterline: the shore distance is +inf everywhere,
    // and bilinear blending at a tile corner gives inf · 0 = NaN. Rust's
    // `as u8` turns that into 0; the C++ port must too (no undefined cast).
    render::sea::IslandWater iw;
    iw.w = iw.h = 8;
    iw.heights.assign(64, 0);
    iw.sea_chunk.assign(1, true);
    iw.open.assign(64, 0.0F);
    iw.shore.assign(64, std::numeric_limits<float>::infinity());
    iw.river.assign(64, std::numeric_limits<float>::quiet_NaN());
    render::sea::SceneProbe probe{{0}, {{4.0F, 4.0F}}};
    const auto f =
        render::sea::water_field(Rect{0, 0, 1, 1}, probe, [&iw](std::size_t) { return &iw; });
    CHECK(f.shore[0] == 0);
    CHECK(f.cover[0] == 1);
    CHECK((f.flags[0] & 1) == 1);
}

TEST_CASE("river bed keeps the floor of a one-corner-wide channel") {
    // River gaps (docs/render/iso.md, "River gaps"): a river piece's height byte
    // is its lowest corner, the banks lie 16 units higher and the surface 9
    // above the bed. The approximating B-spline of the sea's waterline
    // (smooth_height) lifts the floor of a channel one corner wide by a third
    // of the bank height (16 / 3 = 5.3 units; at a bend or a dead end, where
    // the neighbours are banks on two axes, by (1 - (2/3)^2) * 16 = 8.9 units,
    // the river depth: no water left); the bed surface passes through the
    // corners.
    constexpr std::size_t n = 12;
    std::vector<std::uint8_t> h(n * n, 16);
    for (std::size_t y = 0; y < n; ++y) {
        h[y * n + 6] = 0; // the channel runs along y at x = 6
    }
    const float level = 9.0F;
    for (float y = 3.0F; y < 8.0F; y += 0.5F) {
        const float bspline =
            render::sea::smooth_height(h, n, n, 6.0F, y) - render::sea::height_noise(6.0F, y);
        CHECK(bspline == doctest::Approx(16.0F / 3.0F).epsilon(0.01));
        CHECK(render::sea::river_bed_height(h, n, n, 6.0F, y) == doctest::Approx(0.0F));
    }
    {
        std::vector<std::uint8_t> pit(n * n, 16);
        pit[6 * n + 6] = 0;
        const float bspline = render::sea::smooth_height(pit, n, n, 6.0F, 6.0F) -
                              render::sea::height_noise(6.0F, 6.0F);
        CHECK(bspline > level - 0.5F); // the surface is at the floor: dry
        CHECK(render::sea::river_bed_height(pit, n, n, 6.0F, 6.0F) == doctest::Approx(0.0F));
    }
    // Through every corner, bounded by the four corners of the cell, and the
    // channel is wet only near its axis.
    for (std::size_t y = 2; y < 9; ++y) {
        for (std::size_t x = 2; x < 9; ++x) {
            CHECK(render::sea::river_bed_height(h, n, n, static_cast<float>(x),
                                                static_cast<float>(y)) ==
                  doctest::Approx(static_cast<float>(h[y * n + x])));
        }
    }
    for (float x = 2.0F; x < 9.0F; x += 0.25F) {
        const float bed = render::sea::river_bed_height(h, n, n, x, 5.5F);
        CHECK(bed >= 0.0F);
        CHECK(bed <= 16.0F);
        CHECK((bed < level) == (std::fabs(x - 6.0F) < 0.6F));
    }
}

TEST_CASE("canvas to image refuses sizes an Rgba cannot hold") {
    render::Canvas c = render::blank(Rect{0, 0, 70000, 1});
    CHECK_FALSE(c.to_rgba());
    c = render::blank(Rect{0, 0, 3, 2});
    const auto img = c.to_rgba();
    REQUIRE(img);
    CHECK(img->width == 3);
    CHECK(img->height == 2);
}

TEST_CASE("river gap patch: trench corners get depth, nearby pieces are joined (OQ-068)") {
    constexpr std::size_t n = 24;
    // Two 3 x 3 pieces of tier 32 on a plateau of 48, five tiles apart (corners
    // 2..5 and 10..13 along x), plus a trench corner at level + 1 = 42 inside the first.
    const std::vector<render::sea::RiverPiece> pieces = {{2, 8, 3, 3, 32}, {10, 8, 3, 3, 32}};
    auto make = [&](std::vector<std::uint8_t>& h, std::vector<float>& sum,
                    std::vector<float>& cnt) {
        h.assign(n * n, 48);
        sum.assign(n * n, 0.0F);
        cnt.assign(n * n, 0.0F);
        h[9 * n + 3] = 42;
        for (const auto& [x, y, w, hh, z] : pieces) {
            for (std::size_t dy = 0; dy <= hh; ++dy) {
                for (std::size_t dx = 0; dx <= w; ++dx) {
                    sum[(y + dy) * n + x + dx] += static_cast<float>(z) + render::sea::kRiverLevel;
                    cnt[(y + dy) * n + x + dx] += 1.0F;
                }
            }
        }
    };
    std::vector<std::uint8_t> h;
    std::vector<float> sum, cnt;
    make(h, sum, cnt);
    // The gap (5 to 10: 5 tiles) is wider than 4, so no link; the trench is deepened.
    render::sea::patch_river_gaps(pieces, n, n, 4.0F, h, sum, cnt);
    CHECK(h[9 * n + 3] == 36); // level 41 - kGapBedDepth
    CHECK(h[9 * n + 8] == 48); // the gap stays plateau
    CHECK(cnt[9 * n + 8] == 0.0F);
    // Within 6 tiles the pieces are joined: the corners between get a level
    // between the two (here equal) and a carved bed.
    make(h, sum, cnt);
    render::sea::patch_river_gaps(pieces, n, n, 6.0F, h, sum, cnt);
    for (std::size_t x = 6; x <= 9; ++x) {
        CHECK(cnt[9 * n + x] > 0.0F);
        CHECK(sum[9 * n + x] / cnt[9 * n + x] == doctest::Approx(41.0F));
        CHECK(h[9 * n + x] <= 36);
    }
    // Pieces that touch (a shared corner column) are one river already: no carve.
    const std::vector<render::sea::RiverPiece> touching = {{2, 8, 3, 3, 32}, {5, 8, 3, 3, 32}};
    std::vector<std::uint8_t> h2(n * n, 48);
    std::vector<float> s2(n * n, 0.0F), c2(n * n, 0.0F);
    render::sea::patch_river_gaps(touching, n, n, 6.0F, h2, s2, c2);
    CHECK(h2[9 * n + 4] == 48);
    CHECK(c2[9 * n + 4] == 0.0F);
}

namespace {

/// Busy-waits `us` microseconds (the steady clock, not sleep: no scheduling).
void spin(int us) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::microseconds(us);
    while (std::chrono::steady_clock::now() < end) {
    }
}

} // namespace

TEST_CASE("chunk profile: off without a sink, nested stages counted exclusively") {
    namespace profile = render::profile;
    profile::Times t;
    {
        // Not attached: a scope records nothing.
        const profile::Scope s(profile::Stage::objects);
        spin(200);
    }
    profile::attach(&t);
    const auto start = std::chrono::steady_clock::now();
    {
        const profile::Scope outer(profile::Stage::objects);
        spin(500);
        {
            const profile::Scope inner(profile::Stage::decode);
            spin(500);
        }
        spin(500);
    }
    const double elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    profile::attach(nullptr);
    CHECK(t[profile::Stage::objects] >= 0.9);
    CHECK(t[profile::Stage::decode] >= 0.45);
    // Exclusive: the decode is not also counted in the objects.
    CHECK(t[profile::Stage::objects] + t[profile::Stage::decode] <= elapsed);
    double others = 0;
    for (std::size_t i = 0; i < t.ms.size(); ++i) {
        if (i != static_cast<std::size_t>(profile::Stage::objects) &&
            i != static_cast<std::size_t>(profile::Stage::decode)) {
            others += t.ms[i];
        }
    }
    CHECK(others == 0.0);
    // Detached again: nothing more is counted.
    const double before = t[profile::Stage::objects];
    {
        const profile::Scope s(profile::Stage::objects);
        spin(200);
    }
    CHECK(t[profile::Stage::objects] == before);
}

TEST_CASE("water field: every position in an empty cell gets the empty texel") {
    namespace sea = render::sea;
    // A synthetic island: a ridge whose corners run through the waterline and
    // the threshold (kEmptyLandHeight) in small steps, sea around it, and a
    // river over high ground. Each condition of the proof is needed somewhere
    // here: without the river check the river tiles fail, with a lower height
    // threshold the tiles near the waterline fail.
    sea::IslandWater iw;
    iw.w = 48;
    iw.h = 40;
    iw.heights.resize(iw.w * iw.h);
    for (std::size_t y = 0; y < iw.h; ++y) {
        for (std::size_t x = 0; x < iw.w; ++x) {
            // 0 at the left edge to 47 at the right, plus a 0-3 jitter.
            const auto jitter = static_cast<std::uint8_t>((x * 7U + y * 13U) % 4U);
            iw.heights[y * iw.w + x] = static_cast<std::uint8_t>(x + jitter);
        }
    }
    // The 8 x 8 blocks up to x = 39 count as holding sea (an input here), the
    // last column (x 40-47) not.
    const std::size_t cw = (iw.w + 7) / 8;
    iw.sea_chunk.assign(cw * ((iw.h + 7) / 8), true);
    for (std::size_t by = 0; by < (iw.h + 7) / 8; ++by) {
        iw.sea_chunk[by * cw + cw - 1] = false;
    }
    iw.open.assign(iw.w * iw.h, 0.5F);
    iw.shore.assign(iw.w * iw.h, 1.0F);
    iw.river.assign(iw.w * iw.h, std::numeric_limits<float>::quiet_NaN());
    for (std::size_t x = 30; x < 46; ++x) {
        // A river along row 20 over the high ground: level above the bed.
        iw.river[20 * iw.w + x] = static_cast<float>(iw.heights[20 * iw.w + x]) + 9.0F;
    }
    iw.classify_cells();
    std::size_t empty = 0, by_height = 0, by_block = 0, checked = 0, wrong = 0;
    for (std::size_t ty = 0; ty < iw.h; ++ty) {
        for (std::size_t tx = 0; tx < iw.w; ++tx) {
            if (iw.empty_cell[ty * iw.w + tx] == 0) {
                continue;
            }
            ++empty;
            (iw.sea_chunk[(ty / 8) * cw + tx / 8] ? by_height : by_block) += 1;
            for (const float fy : {0.0F, 0.2F, 0.45F, 0.7F, 0.9999F}) {
                for (const float fx : {0.0F, 0.15F, 0.5F, 0.8F, 0.9999F}) {
                    ++checked;
                    const sea::FieldTexel t =
                        sea::field_at(iw, static_cast<float>(tx) + fx, static_cast<float>(ty) + fy);
                    wrong += t == sea::FieldTexel{} ? 0U : 1U;
                }
            }
        }
    }
    MESSAGE("empty cells " << empty << " (" << by_height << " by height, " << by_block
                           << " without sea), " << checked << " positions");
    CHECK(by_height > 0);
    CHECK(by_block > 0);
    CHECK(wrong == 0);
    // The sea, the waterline and the river are not empty.
    CHECK(iw.empty_cell[10 * iw.w + 2] == 0);
    CHECK(iw.empty_cell[10 * iw.w + 16] == 0);
    CHECK(iw.empty_cell[20 * iw.w + 35] == 0);
    // water_field: empty cells written without evaluation, the rest as field_at.
    render::Rect rect{0, 0, 64, 32};
    sea::SceneProbe probe{std::vector<std::uint32_t>(64 * 32, 0),
                          std::vector<std::array<float, 2>>(64 * 32)};
    for (std::size_t i = 0; i < probe.pos.size(); ++i) {
        probe.pos[i] = {0.3F + static_cast<float>(i % 64) * 0.73F,
                        0.1F + static_cast<float>(i / 64) * 1.21F};
    }
    const auto f = sea::water_field(rect, probe, [&](std::size_t) { return &iw; });
    std::size_t differ = 0;
    for (std::size_t i = 0; i < probe.pos.size(); ++i) {
        const sea::FieldTexel ref = sea::field_at(iw, probe.pos[i][0], probe.pos[i][1]);
        differ += ref == sea::FieldTexel{f.cover[i], f.shore[i], f.depth[i], f.flags[i]} ? 0U : 1U;
    }
    CHECK(differ == 0);
}
TEST_CASE("noise3_at equals noise3 at an integer z, bit for bit") {
    std::size_t differ = 0, n = 0;
    for (int iz : {-3, 0, 11, 23}) {
        for (int j = -200; j < 200; ++j) {
            for (int i = -200; i < 200; ++i) {
                const float x = static_cast<float>(i) * 0.137F + 0.01F;
                const float y = static_cast<float>(j) * 0.091F - 0.3F;
                const float a = render::noise3(x, y, static_cast<float>(iz));
                const float b = render::noise3_at(x, y, iz);
                differ += std::memcmp(&a, &b, sizeof a) == 0 ? 0U : 1U;
                ++n;
            }
        }
    }
    CHECK(n == 640000);
    CHECK(differ == 0);
}
TEST_CASE("SmoothSampler equals smooth_height and height_noise, bit for bit") {
    namespace sea = render::sea;
    sea::IslandWater iw;
    iw.w = 40;
    iw.h = 30;
    iw.heights.resize(iw.w * iw.h);
    for (std::size_t i = 0; i < iw.heights.size(); ++i) {
        iw.heights[i] = static_cast<std::uint8_t>((i * 53U + i / 7U) % 60U);
    }
    sea::SmoothSampler s(iw);
    std::size_t differ = 0, n = 0;
    auto same = [](float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; };
    // Rows of small steps (the cache holds), jumps between rows (it misses),
    // and positions outside the grid (clamped corners).
    for (int row = -3; row < 34; ++row) {
        for (int k = -20; k < 460; ++k) {
            const float x = static_cast<float>(k) * 0.091F + static_cast<float>(row % 5) * 0.37F;
            const float y = static_cast<float>(row) + static_cast<float>(k) * 0.013F;
            differ +=
                same(s.smooth(x, y), sea::smooth_height(iw.heights, iw.w, iw.h, x, y)) ? 0U : 1U;
            differ += same(s.noise(x, y), sea::height_noise(x, y)) ? 0U : 1U;
            n += 2;
        }
    }
    CHECK(n > 30000);
    CHECK(differ == 0);
}
TEST_CASE("water field: the synthetic scene's digest is the one before the T1f speedups") {
    // Recorded with the sources of 7479a2a (tests/water_field_fixture.hpp): every
    // shortcut must leave every byte as it was, classified or not.
    constexpr std::uint64_t kGolden = 0x51aadd2b3bf2a795ULL;
    namespace sea = render::sea;
    auto s = render::test::field_scene();
    auto water = [&](std::size_t i) -> const sea::IslandWater* {
        return i < s.islands.size() ? &s.islands[i] : nullptr;
    };
    CHECK(render::test::field_digest(sea::water_field(s.rect, s.probe, water)) == kGolden);
    std::size_t empty = 0;
    for (auto& iw : s.islands) {
        iw.classify_cells();
        empty +=
            static_cast<std::size_t>(std::count(iw.empty_cell.begin(), iw.empty_cell.end(), 1));
    }
    CHECK(empty > 0);
    const auto f = sea::water_field(s.rect, s.probe, water);
    CHECK(render::test::field_digest(f) == kGolden);
    // Per pixel against field_at of the pixel's island: island changes, pixels
    // without an island, the missing island, positions outside the grids.
    std::size_t differ = 0, outside = 0;
    for (std::size_t i = 0; i < s.probe.island.size(); ++i) {
        const std::uint32_t island = s.probe.island[i];
        sea::FieldTexel want{255, 255, 255, 1};
        if (island < s.islands.size()) {
            const auto& iw = s.islands[island];
            const auto [x, y] = s.probe.pos[i];
            want = sea::field_at(iw, x, y);
            outside += x < 0.0F || y < 0.0F || x >= static_cast<float>(iw.w) ||
                               y >= static_cast<float>(iw.h)
                           ? 1U
                           : 0U;
        }
        differ += want == sea::FieldTexel{f.cover[i], f.shore[i], f.depth[i], f.flags[i]} ? 0U : 1U;
    }
    CHECK(outside > 100);
    CHECK(differ == 0);
}

TEST_CASE("water field: a tile is empty exactly when its 16 spline corners clear the threshold") {
    namespace sea = render::sea;
    auto plateau = [](std::uint8_t height) {
        sea::IslandWater iw;
        iw.w = 24;
        iw.h = 16;
        iw.heights.assign(iw.w * iw.h, height);
        iw.sea_chunk.assign(((iw.w + 7) / 8) * ((iw.h + 7) / 8), true);
        iw.open.assign(iw.w * iw.h, 0.5F);
        iw.shore.assign(iw.w * iw.h, 1.0F);
        iw.river.assign(iw.w * iw.h, std::numeric_limits<float>::quiet_NaN());
        return iw;
    };
    auto marked = [](const sea::IslandWater& iw) {
        return static_cast<std::size_t>(std::count(iw.empty_cell.begin(), iw.empty_cell.end(), 1));
    };
    // The threshold: 31 < kEmptyLandHeight = 31.1 <= 32.
    auto low = plateau(31);
    low.classify_cells();
    CHECK(marked(low) == 0);
    auto high = plateau(32);
    high.classify_cells();
    CHECK(marked(high) == high.w * high.h);
    // One low corner (cx, cy) unmarks exactly the tiles whose smooth_height reads
    // it: tx in [cx - 2, cx + 1], ty in [cy - 2, cy + 1].
    auto pit = plateau(40);
    constexpr std::size_t cx = 10, cy = 7;
    pit.heights[cy * pit.w + cx] = 30;
    pit.classify_cells();
    for (std::size_t ty = 0; ty < pit.h; ++ty) {
        for (std::size_t tx = 0; tx < pit.w; ++tx) {
            const bool reads = tx + 2 >= cx && tx <= cx + 1 && ty + 2 >= cy && ty <= cy + 1;
            CHECK_MESSAGE((pit.empty_cell[ty * pit.w + tx] == 0) == reads, tx << ", " << ty);
        }
    }
    CHECK(marked(pit) == pit.w * pit.h - 16);
}

TEST_CASE("water field: islands one tile wide or high classify and evaluate like the rest") {
    // The spline reads 16 corners around a tile and clamps at the grid; on a
    // grid one tile wide every neighbour is a clamp. Classified or not, every
    // pixel must be field_at of its position (item 2 left this case out).
    namespace sea = render::sea;
    for (const bool tall : {true, false}) {
        for (const std::uint8_t base : {std::uint8_t{20}, std::uint8_t{45}}) {
            sea::IslandWater iw;
            iw.w = tall ? 1 : 18;
            iw.h = tall ? 18 : 1;
            iw.heights.resize(iw.w * iw.h);
            for (std::size_t i = 0; i < iw.heights.size(); ++i) {
                iw.heights[i] = static_cast<std::uint8_t>(base + (i % 5 == 0 ? 0U : 12U));
            }
            iw.sea_chunk.assign(((iw.w + 7) / 8) * ((iw.h + 7) / 8), true);
            iw.open.assign(iw.w * iw.h, 0.5F);
            iw.shore.assign(iw.w * iw.h, 1.0F);
            iw.river.assign(iw.w * iw.h, std::numeric_limits<float>::quiet_NaN());
            const render::Rect rect{0, 0, 48, 48};
            sea::SceneProbe probe{std::vector<std::uint32_t>(48 * 48, 0),
                                  std::vector<std::array<float, 2>>(48 * 48)};
            for (std::size_t i = 0; i < probe.pos.size(); ++i) {
                probe.pos[i] = {-1.0F + static_cast<float>(i % 48) * (tall ? 0.05F : 0.45F),
                                -1.0F + static_cast<float>(i / 48) * (tall ? 0.45F : 0.05F)};
            }
            auto water = [&](std::size_t) { return &iw; };
            const auto before = sea::water_field(rect, probe, water);
            iw.classify_cells();
            const auto after = sea::water_field(rect, probe, water);
            std::size_t differ = 0;
            for (std::size_t i = 0; i < probe.pos.size(); ++i) {
                const sea::FieldTexel want = sea::field_at(iw, probe.pos[i][0], probe.pos[i][1]);
                differ += want == sea::FieldTexel{after.cover[i], after.shore[i], after.depth[i],
                                                  after.flags[i]}
                              ? 0U
                              : 1U;
            }
            CHECK(differ == 0);
            CHECK(render::test::field_digest(before) == render::test::field_digest(after));
        }
    }
}

TEST_CASE("terrain_plane equals terrain_height per pixel across islands") {
    // Two islands whose pixels alternate in short runs on the same tiles: the
    // corner cache must not carry over from one island to the other.
    std::vector<std::uint8_t> a(12 * 10), b(12 * 10);
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<std::uint8_t>((i * 7U) % 50U);
        b[i] = static_cast<std::uint8_t>(200U - (i * 11U) % 90U);
    }
    render::sea::SceneProbe probe;
    for (std::size_t i = 0; i < 4000; ++i) {
        const std::size_t k = (i / 3) % 4;
        probe.island.push_back(k == 3 ? render::sea::kNoIsland : static_cast<std::uint32_t>(k));
        const float x = -1.0F + static_cast<float>(i % 160) * 0.09F;
        const float y = -0.5F + static_cast<float>(i / 160) * 0.47F;
        probe.pos.push_back({x, y});
    }
    const auto plane = render::terrain_plane(probe, [&](std::uint32_t island) {
        // Island 2 has no ground.
        return island == 0   ? render::TerrainGrid{&a, 12, 10}
               : island == 1 ? render::TerrainGrid{&b, 12, 10}
                             : render::TerrainGrid{};
    });
    REQUIRE(plane.size() == probe.island.size());
    std::size_t differ = 0, ground = 0;
    for (std::size_t i = 0; i < plane.size(); ++i) {
        std::uint16_t want = render::kNoTerrain;
        if (probe.island[i] < 2) {
            const auto& h = probe.island[i] == 0 ? a : b;
            const float z = render::terrain_height(h, 12, 10, probe.pos[i][0], probe.pos[i][1]);
            want = render::sat_cast<std::uint16_t>(
                render::clampf(roundf(z * render::kTerrainScale), 0.0F,
                               static_cast<float>(render::kNoTerrain - 1)));
            ++ground;
        }
        differ += plane[i] == want ? 0U : 1U;
    }
    CHECK(ground > 1000);
    CHECK(differ == 0);
}

TEST_CASE("NoisePlane equals noise3_at, also at negative z and coordinates") {
    std::size_t differ = 0;
    for (const int iz : {-7, 11}) {
        render::NoisePlane plane(iz);
        for (int i = -500; i < 500; ++i) {
            const float x = static_cast<float>(i) * 0.031F, y = static_cast<float>(i % 37) * -0.17F;
            const float a = plane.at(x, y), b = render::noise3_at(x, y, iz);
            differ += std::memcmp(&a, &b, sizeof a) == 0 ? 0U : 1U;
        }
    }
    CHECK(differ == 0);
}