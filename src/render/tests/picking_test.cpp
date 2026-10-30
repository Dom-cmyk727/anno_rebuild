// Picking (render/picking.hpp): the forward rule matches iso::tile_top, and the
// inverse picks the tile a point lies on, at the three zoom groups and at
// continuous zoom, with ground heights and open sea.
#include <cmath>
#include <optional>

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
