// Synthetic tests of anno_render (no game files needed).
#include <doctest/doctest.h>

#include <anno/render/frame.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>

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
