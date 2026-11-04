// Picking (render/picking.hpp): the forward rule matches iso::tile_top, and the
// inverse picks the tile a point lies on, at the three zoom groups and at
// continuous zoom, with ground heights and open sea.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/render/iso.hpp>
#include <anno/render/picking.hpp>

namespace {

// A sloped ground, 4 HEIGHT units per tile in u and -3 in v, with a bump. Heights
// stay between 0 and 150 on the grid used by the test.
std::optional<float> slope(float u, float v) {
    return 60.0F + 4.0F * u - 3.0F * v + 6.0F * std::sin(0.5F * u);
}

// Sea beyond u = 18: no ground height (picks use height 0 there).
std::optional<float> slope_with_sea(float u, float v) {
    if (u >= 18.0F) {
        return std::nullopt;
    }
    return slope(u, v);
}

} // namespace

TEST_CASE("picking: the forward rule at height 0 is iso::tile_top at each built group") {
    for (std::size_t group = 0; group < anno::render::kGroups; ++group) {
        const auto g = anno::render::pick_geometry(group);
        for (int x = -4; x <= 20; ++x) {
            for (int y = -4; y <= 20; ++y) {
                const auto [tx, ty] = anno::render::tile_top(x, y, group);
                const auto [px, py] = anno::render::corner_point(x, y, 0.0, g);
                // iso::tile_top truncates in integer arithmetic (the odd tile height of
                // group 1 gives a half pixel); picking keeps the exact value.
                CHECK(std::abs(px - static_cast<double>(tx)) < 1.0);
                CHECK(std::abs(py - static_cast<double>(ty)) < 1.0);
            }
        }
    }
}

TEST_CASE("picking: a tile centre at its ground height picks that tile, per group") {
    for (std::size_t group = 0; group < anno::render::kGroups; ++group) {
        const auto g = anno::render::pick_geometry(group);
        for (int x = 0; x < 16; ++x) {
            for (int y = 0; y < 16; ++y) {
                const double u = x + 0.5, v = y + 0.5;
                const double h = slope(static_cast<float>(u), static_cast<float>(v)).value();
                const auto [px, py] = anno::render::corner_point(u, v, h, g);
                const auto picked = anno::render::pick_tile(px, py, g, slope);
                REQUIRE(picked.has_value());
                CHECK(picked->first == x);
                CHECK(picked->second == y);
            }
        }
    }
}

TEST_CASE("picking: continuous zoom picks with the same rule") {
    for (const double scale : {1.0, 0.45, 0.7, 1.3, 2.0}) {
        for (std::size_t group = 0; group < anno::render::kGroups; ++group) {
            const auto g = anno::render::pick_geometry(group, scale);
            for (int x = 0; x < 12; ++x) {
                for (int y = 0; y < 12; ++y) {
                    const double u = x + 0.25, v = y + 0.75;
                    const double h = slope(static_cast<float>(u), static_cast<float>(v)).value();
                    const auto [px, py] = anno::render::corner_point(u, v, h, g);
                    const auto picked = anno::render::pick_tile(px, py, g, slope);
                    REQUIRE(picked.has_value());
                    CHECK_MESSAGE(picked->first == x, "scale " << scale << " group " << group);
                    CHECK_MESSAGE(picked->second == y, "scale " << scale << " group " << group);
                }
            }
        }
    }
}

TEST_CASE("picking: open sea picks at height 0") {
    for (std::size_t group = 0; group < anno::render::kGroups; ++group) {
        const auto g = anno::render::pick_geometry(group);
        for (int x = 19; x < 24; ++x) {
            for (int y = 2; y < 8; ++y) {
                const double u = x + 0.5, v = y + 0.5;
                const auto [px, py] = anno::render::corner_point(u, v, 0.0, g);
                const auto picked = anno::render::pick_tile(px, py, g, slope_with_sea);
                REQUIRE(picked.has_value());
                CHECK(picked->first == x);
                CHECK(picked->second == y);
            }
        }
    }
}

TEST_CASE("picking: the lift scales with the tile width, as Lift does") {
    for (std::size_t group = 0; group < anno::render::kGroups; ++group) {
        const auto g = anno::render::pick_geometry(group);
        for (const int h : {0, 1, 16, 32, 200}) {
            const auto height = static_cast<std::uint8_t>(h);
            const int lifted = anno::render::kHeightLift.pixels(height, group);
            const double lift = g.lift * h;
            CHECK(std::abs(lift - lifted) < 1.0);
        }
    }
}

// ---- round ee, package B: expectations derived without pick_tile's own formula ----------------

namespace {

/// The centre of tile (x, y) on flat ground of height `h`, from the renderer's integer rule
/// (iso::tile_top: the diamond's top vertex) plus half a diamond down, lifted by the height at
/// Lift's rate (kHeightLift: 2.059 px per HEIGHT unit at a tile width of 84 px, proportional).
std::pair<double, double> drawn_centre(double h, double tw, double th, std::pair<int, int> top) {
    const double lift = h * 2059.0 / 1000.0 * tw / 84.0;
    return {static_cast<double>(top.first), static_cast<double>(top.second) + th / 2.0 - lift};
}

} // namespace

TEST_CASE("picking: hand-derived tile centres from tile_top and the lift pick their tile, and a "
          "point past an edge picks the neighbour" *
          doctest::test_suite("picking")) {
    // A literal case first: group 0, tile (10, 5), flat ground at height 0. tile_top is
    // ((5 - 10) * 42, (10 + 5) * 21) = (-210, 315) (the original's handedness, iso.md
    // "Handedness"); the centre is 21 px below it.
    {
        const auto g = anno::render::pick_geometry(0);
        const auto flat = [](float, float) { return std::optional<float>(0.0F); };
        const auto picked = anno::render::pick_tile(-210.0, 336.0, g, flat);
        REQUIRE(picked.has_value());
        CHECK(picked->first == 10);
        CHECK(picked->second == 5);
    }
    for (const double h : {0.0, 37.0, 120.0}) {
        const auto flat = [h](float, float) { return std::optional<float>(static_cast<float>(h)); };
        for (std::size_t group = 0; group < anno::render::kGroups; ++group) {
            const auto [tw, th] = anno::render::kTile[group];
            const auto g = anno::render::pick_geometry(group);
            for (const auto& [x, y] :
                 std::vector<std::pair<int, int>>{{0, 0}, {10, 5}, {3, 17}, {41, 40}}) {
                CAPTURE(group);
                CAPTURE(h);
                CAPTURE(x);
                CAPTURE(y);
                const auto [cx, cy] = drawn_centre(h, tw, th, anno::render::tile_top(x, y, group));
                const auto picked = anno::render::pick_tile(cx, cy, g, flat);
                REQUIRE(picked.has_value());
                CHECK(picked->first == x);
                CHECK(picked->second == y);
                // One step of +u is (-tw / 2, th / 2) on screen (the original's handedness):
                // 0.6 of it crosses into x + 1, 0.4 stays (the diamond's edge is at 0.5); -v is
                // (-tw / 2, -th / 2).
                const auto across =
                    anno::render::pick_tile(cx - 0.6 * tw / 2.0, cy + 0.6 * th / 2.0, g, flat);
                const auto inside =
                    anno::render::pick_tile(cx - 0.4 * tw / 2.0, cy + 0.4 * th / 2.0, g, flat);
                const auto back_v =
                    anno::render::pick_tile(cx - 0.6 * tw / 2.0, cy - 0.6 * th / 2.0, g, flat);
                REQUIRE(across);
                REQUIRE(inside);
                REQUIRE(back_v);
                CHECK(*across == std::pair{x + 1, y});
                CHECK(*inside == std::pair{x, y});
                CHECK(*back_v == std::pair{x, y - 1});
            }
        }
    }
    // A derived level (a continuous zoom below group 2): its own tile size, the same rule.
    const auto g = anno::render::pick_geometry_of(10.0, 5.0);
    const auto flat = [](float, float) { return std::optional<float>(50.0F); };
    const double lift = 50.0 * 2.059 * 10.0 / 84.0;
    const auto picked = anno::render::pick_tile((2 - 7) * 5.0, (7 + 2) * 2.5 + 2.5 - lift, g, flat);
    REQUIRE(picked.has_value());
    CHECK(*picked == std::pair{7, 2});
}

TEST_CASE("picking: a raised tile hides the lower ground behind it, and the pick is the raised "
          "tile" *
          doctest::test_suite("picking")) {
    // Tile (6, 6) is 100 HEIGHT units high, everything else is at 0. The screen point at the
    // centre of its raised top lies, on flat ground, over a tile further back: that ground is
    // drawn first and hidden by the raised tile (the painter's order is x + y).
    const auto raised = [](float u, float v) {
        return std::optional<float>(std::floor(u) == 6.0F && std::floor(v) == 6.0F ? 100.0F : 0.0F);
    };
    const auto flat = [](float, float) { return std::optional<float>(0.0F); };
    for (std::size_t group = 0; group < anno::render::kGroups; ++group) {
        CAPTURE(group);
        const auto [tw, th] = anno::render::kTile[group];
        const auto g = anno::render::pick_geometry(group);
        const auto [cx, cy] = drawn_centre(100.0, tw, th, anno::render::tile_top(6, 6, group));
        const auto behind = anno::render::pick_tile(cx, cy, g, flat);
        REQUIRE(behind.has_value());
        CHECK(*behind != std::pair{6, 6});
        CHECK(behind->first + behind->second < 12); // further back
        const auto picked = anno::render::pick_tile(cx, cy, g, raised);
        REQUIRE(picked.has_value());
        CHECK(*picked == std::pair{6, 6});
    }
}

TEST_CASE("picking: non-finite input, a degenerate geometry and points beyond the safe range pick "
          "nothing, and the screen map is a pixel's centre" *
          doctest::test_suite("picking")) {
    const auto g = anno::render::pick_geometry(1);
    const auto flat = [](float, float) { return std::optional<float>(0.0F); };
    const double nan = std::nan("");
    const double inf = std::numeric_limits<double>::infinity();
    CHECK_FALSE(anno::render::pick_tile(nan, 0.0, g, flat));
    CHECK_FALSE(anno::render::pick_tile(0.0, inf, g, flat));
    CHECK_FALSE(anno::render::pick_tile(1e12, 1e12, g, flat)); // beyond 2^30 tiles
    anno::render::PickGeometry zero = g;
    zero.tile_w = 0.0;
    CHECK_FALSE(anno::render::pick_tile(0.0, 0.0, zero, flat));
    // A stepped zoom: drawable pixel (10, 20) of a view at (-100, 50) shows scene point
    // (-89.5, 70.5). A continuous zoom at scale 2 with the view's left edge at -100.25:
    // (-100.25 + 10.5 / 2, 50 + 20.5 / 2) = (-95, 60.25).
    const auto stepped = anno::render::scene_point({-100.0, 50.0, 1.0}, 10.0, 20.0);
    REQUIRE(stepped);
    CHECK(stepped->first == doctest::Approx(-89.5));
    CHECK(stepped->second == doctest::Approx(70.5));
    const auto scaled = anno::render::scene_point({-100.25, 50.0, 2.0}, 10.0, 20.0);
    REQUIRE(scaled);
    CHECK(scaled->first == doctest::Approx(-95.0));
    CHECK(scaled->second == doctest::Approx(60.25));
    // A pointer with a fraction (a high-density display) is in the pixel its integer part names.
    const auto fraction = anno::render::scene_point({-100.25, 50.0, 2.0}, 10.7, 20.2);
    REQUIRE(fraction);
    CHECK(fraction->first == doctest::Approx(-95.0));
    CHECK(fraction->second == doctest::Approx(60.25));
    CHECK_FALSE(anno::render::scene_point({0.0, 0.0, 0.0}, 1.0, 1.0));
    CHECK_FALSE(anno::render::scene_point({0.0, 0.0, 1.0}, nan, 1.0));
}

TEST_CASE("picking: in front of a crest, the thin part of a triangle that a one-unit march "
          "stepped over picks the tile the painter drew there" *
          doctest::test_suite("picking")) {
    // The corners of lan5's developer island where the pick took the tile behind
    // (W4-OPENING-1-CLOSE): tile (1, 1) rises 16 units from A to C, so its A-B-C triangle faces
    // away on screen and its C-B-D triangle reaches over tile (0, 1); in front of the B-C crest
    // the ray is under the ground for less than one HEIGHT unit. The oracle is the painter:
    // every triangle in the renderer's order (x + y, then x; A-B-C before C-B-D), the last one
    // that holds the pixel centre wins, and pixels within 0.01 px of that triangle's edges are
    // left out (where the rasteriser's tolerance decides).
    constexpr std::size_t w = 4, h = 4;
    const std::vector<std::uint8_t> heights = {24, 32, 48, 48, //
                                               24, 32, 48, 48, //
                                               24, 40, 48, 48, //
                                               24, 40, 48, 48};
    const auto ground = [&](float u, float v) -> std::optional<float> {
        if (u < 0.0F || v < 0.0F || u >= static_cast<float>(w) || v >= static_cast<float>(h)) {
            return std::nullopt;
        }
        return anno::render::terrain_height(heights, w, h, u, v);
    };
    using P = std::pair<double, double>;
    std::size_t checked = 0, wrong = 0;
    for (std::size_t group = 0; group < anno::render::kGroups; ++group) {
        CAPTURE(group);
        const auto g = anno::render::pick_geometry(group);
        auto corner = [&](std::size_t x, std::size_t y) {
            const auto c = std::min(x, w - 1), r = std::min(y, h - 1);
            return anno::render::corner_point(static_cast<double>(x), static_cast<double>(y),
                                              static_cast<double>(heights[r * w + c]), g);
        };
        const int x0 = static_cast<int>(-g.tile_w * 2.5), x1 = static_cast<int>(g.tile_w * 2.5);
        const int y0 = static_cast<int>(-g.lift * 60.0), y1 = static_cast<int>(g.tile_h * 4.5);
        for (int py = y0; py < y1; ++py) {
            for (int px = x0; px < x1; ++px) {
                const P q{px + 0.5, py + 0.5};
                std::optional<std::pair<int, int>> drawn;
                double margin = 0.0;
                for (std::size_t v = 0; v + 1 < w + h; ++v) {
                    for (std::size_t x = 0; x <= v && x < w; ++x) {
                        const std::size_t y = v - x;
                        if (y >= h) {
                            continue;
                        }
                        const P a = corner(x, y), cc = corner(x + 1, y), b = corner(x, y + 1),
                                d = corner(x + 1, y + 1);
                        for (const std::array<P, 3>& t :
                             {std::array<P, 3>{a, b, cc}, std::array<P, 3>{cc, b, d}}) {
                            auto cross = [](P o, P p, P r) {
                                return (p.first - o.first) * (r.second - o.second) -
                                       (p.second - o.second) * (r.first - o.first);
                            };
                            const double area = cross(t[0], t[1], t[2]);
                            if (std::fabs(area) < 1e-9) {
                                continue;
                            }
                            double m = 1e30;
                            for (std::size_t e = 0; e < 3; ++e) {
                                const P p0 = t[e], p1 = t[(e + 1) % 3];
                                const double len =
                                    std::hypot(p1.first - p0.first, p1.second - p0.second);
                                m = std::min(m, cross(p0, p1, q) * (area > 0 ? 1.0 : -1.0) / len);
                            }
                            if (m >= -0.01) {
                                drawn = std::pair{static_cast<int>(x), static_cast<int>(y)};
                                margin = m;
                            }
                        }
                    }
                }
                if (!drawn || margin < 0.01) {
                    continue;
                }
                const auto picked = anno::render::pick_tile(q.first, q.second, g, ground);
                ++checked;
                if (picked != drawn) {
                    ++wrong;
                    MESSAGE("pixel (" << px << ", " << py << "): drawn (" << drawn->first << ", "
                                      << drawn->second << "), margin " << margin);
                }
            }
        }
    }
    MESSAGE(checked << " pixels checked, " << wrong << " picked another tile");
    CHECK(checked > 1000);
    CHECK(wrong == 0);
}
