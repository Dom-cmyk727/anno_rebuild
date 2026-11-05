#include "original_component.hpp"
// The corner-height view (task MAP-2 U1; docs/re/ship-founding.md section 12.3):
// the disk samples of Terrain::height (W x H, row-major) seen as the original's
// corner grid (stride W + 1; column W and rows H, H + 1 are 0).
//
// The synthetic cases use non-square islands at non-zero (also negative) origins with
// every row and column distinct, so a swapped axis or a wrong stride cannot pass. The
// real-data cases build the original's runtime grid independently from the file's HEIGHT
// BLOCKS with the copy rule of AnnoWelt.dll 0x10084870 (allocation (H + 2) * (W + 1)
// zeroes, block copy clamped to W + 1 / H + 1) and compare it with the view of the
// island the existing loader produced.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/insel.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

#include "island_helpers.hpp"

namespace sim = anno::sim;
namespace fs = std::filesystem;

namespace {

/// An island whose tile layer is (x0, y0, w, h) and whose HEIGHT column is `height`.
sim::Island island_with(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h,
                        std::vector<std::uint8_t> height) {
    sim::Island is;
    REQUIRE(is.tiles.reset(x0, y0, w, h, 8));
    is.terrain = sim::make_terrain(std::move(height), {}, {});
    return is;
}

/// height[y * w + x] = 17 * y + x + 1: distinct per cell for the sizes used here.
std::vector<std::uint8_t> ramp(std::uint32_t w, std::uint32_t h) {
    std::vector<std::uint8_t> v(std::size_t{w} * h);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            v[std::size_t{y} * w + x] = static_cast<std::uint8_t>(17 * y + x + 1);
        }
    }
    return v;
}

} // namespace

TEST_CASE("vertex grid: a non-square island at a non-zero origin") {
    constexpr std::uint32_t W = 5, H = 3;
    const sim::Island is = island_with(7, 11, W, H, ramp(W, H));
    const sim::VertexGrid g = sim::VertexGrid::of(is);
    REQUIRE(g.ok());
    CHECK(g.x0() == 7);
    CHECK(g.y0() == 11);
    CHECK(g.tiles_x() == W);
    CHECK(g.tiles_y() == H);
    CHECK(g.stride() == W + 1);
    // Every sample lands on its own vertex: HEIGHT[x, y] is vertex (x, y).
    for (std::uint32_t y = 0; y < H; ++y) {
        for (std::uint32_t x = 0; x < W; ++x) {
            INFO("tile (" << x << ", " << y << ")");
            REQUIRE(g.at(7 + x, 11 + y).has_value());
            CHECK(*g.at(7 + x, 11 + y) == static_cast<std::uint8_t>(17 * y + x + 1));
        }
    }
    // The boundary: column W (every row, the corners included) and row H read 0.
    for (std::uint32_t y = 0; y <= H; ++y) {
        REQUIRE(g.at(7 + W, 11 + y).has_value());
        CHECK(*g.at(7 + W, 11 + y) == 0);
    }
    for (std::uint32_t x = 0; x <= W; ++x) {
        REQUIRE(g.at(7 + x, 11 + H).has_value());
        CHECK(*g.at(7 + x, 11 + H) == 0);
    }
    // Outside the range there is no vertex (also the original's spare row H + 1, which no
    // placement reads).
    CHECK_FALSE(g.at(6, 11).has_value());
    CHECK_FALSE(g.at(7, 10).has_value());
    CHECK_FALSE(g.at(7 + W + 1, 11).has_value());
    CHECK_FALSE(g.at(7, 11 + H + 1).has_value());
    CHECK_FALSE(g.contains(7 + W + 1, 11));
    CHECK(g.contains(7 + W, 11 + H));
}

TEST_CASE("vertex grid: the original's runtime index") {
    constexpr std::uint32_t W = 5, H = 3;
    const sim::Island is = island_with(-4, 20, W, H, ramp(W, H));
    const sim::VertexGrid g = sim::VertexGrid::of(is);
    REQUIRE(g.ok());
    // (vy - y0) * (W + 1) + (vx - x0), the arithmetic of AnnoWelt.dll 0x1007cb52-0x1007cb85.
    CHECK(*g.runtime_index(-4, 20) == 0);
    CHECK(*g.runtime_index(-4 + 5, 20) == 5);
    CHECK(*g.runtime_index(-4, 21) == 6);
    CHECK(*g.runtime_index(-4 + 5, 20 + 3) == 3 * 6 + 5);
    CHECK_FALSE(g.runtime_index(-5, 20).has_value());
    CHECK_FALSE(g.runtime_index(-4, 24).has_value());
    // Negative origins are ordinary coordinates.
    CHECK(*g.at(-4, 20) == 1);
    CHECK(*g.at(-4 + 4, 20 + 2) == static_cast<std::uint8_t>(17 * 2 + 4 + 1));
    // Extreme coordinates are total (no signed overflow): the unsigned difference is exact.
    constexpr std::int64_t lo = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t hi = std::numeric_limits<std::int64_t>::max();
    CHECK_FALSE(g.contains(lo, 20));
    CHECK_FALSE(g.contains(hi, 20));
    CHECK_FALSE(g.contains(-4, lo));
    CHECK_FALSE(g.at(hi, hi).has_value());
    CHECK_FALSE(g.runtime_index(lo, lo).has_value());
    const sim::Island pos = island_with(7, 11, W, H, ramp(W, H));
    const sim::VertexGrid gp = sim::VertexGrid::of(pos);
    CHECK_FALSE(gp.contains(lo, 11));
    CHECK_FALSE(gp.contains(7, lo));
    CHECK_FALSE(gp.contains(hi, hi));
}

TEST_CASE("vertex grid: a one-tile island and an empty island") {
    {
        const sim::Island is = island_with(3, 4, 1, 1, {9});
        const sim::VertexGrid g = sim::VertexGrid::of(is);
        REQUIRE(g.ok());
        CHECK(*g.at(3, 4) == 9);
        CHECK(*g.at(4, 4) == 0);
        CHECK(*g.at(3, 5) == 0);
        CHECK(*g.at(4, 5) == 0);
        CHECK_FALSE(g.at(5, 4).has_value());
    }
    {
        // 0 x 0: the only vertex is the origin, a boundary vertex. The column is empty
        // and that is the right size.
        const sim::Island is = island_with(2, 2, 0, 0, {});
        const sim::VertexGrid g = sim::VertexGrid::of(is);
        REQUIRE(g.ok());
        CHECK(*g.at(2, 2) == 0);
        CHECK_FALSE(g.at(3, 2).has_value());
    }
}

TEST_CASE("vertex grid: missing, absent, placeholder and malformed terrain") {
    using S = sim::VertexGridStatus;
    {
        sim::Island is;
        REQUIRE(is.tiles.reset(0, 0, 4, 2, 8));
        const sim::VertexGrid g = sim::VertexGrid::of(is);
        CHECK(g.status() == S::NoTerrain);
        CHECK_FALSE(g.ok());
        CHECK_FALSE(g.at(0, 0).has_value());
        // The range question is independent of the status.
        CHECK(g.contains(4, 2));
    }
    {
        // A scenario without HEIGHT records: the other columns exist, HEIGHT is empty.
        sim::Island is;
        REQUIRE(is.tiles.reset(0, 0, 4, 2, 8));
        is.terrain = sim::make_terrain({}, std::vector<std::uint16_t>(8, 1), {});
        CHECK(sim::VertexGrid::of(is).status() == S::HeightColumnAbsent);
        // No columns at all.
        is.terrain = sim::make_terrain({}, {}, {});
        CHECK(sim::VertexGrid::of(is).status() == S::HeightColumnAbsent);
        CHECK_FALSE(sim::VertexGrid::of(is).at(0, 0).has_value());
    }
    {
        // The placeholder a world read from the flat state stream holds: empty columns,
        // the digest of real ones.
        sim::Island is;
        REQUIRE(is.tiles.reset(0, 0, 4, 2, 8));
        const std::shared_ptr<const sim::Terrain> real = sim::make_terrain(ramp(4, 2), {}, {});
        auto placeholder = std::make_shared<sim::Terrain>();
        placeholder->digest = real->digest;
        is.terrain = placeholder;
        const sim::VertexGrid g = sim::VertexGrid::of(is);
        CHECK(g.status() == S::HashOnlyPlaceholder);
        CHECK_FALSE(g.at(0, 0).has_value());
    }
    {
        // Truncated and over-long columns.
        std::vector<std::uint8_t> v = ramp(4, 2);
        v.pop_back();
        sim::Island is = island_with(0, 0, 4, 2, v);
        CHECK(sim::VertexGrid::of(is).status() == S::HeightColumnWrongSize);
        CHECK_FALSE(sim::VertexGrid::of(is).at(0, 0).has_value());
        v = ramp(4, 2);
        v.push_back(1);
        is = island_with(0, 0, 4, 2, v);
        CHECK(sim::VertexGrid::of(is).status() == S::HeightColumnWrongSize);
        // A column laid out for the corner grid, (W + 1) * (H + 1), is also refused: the
        // stored form is W x H.
        is = island_with(0, 0, 4, 2, std::vector<std::uint8_t>(5 * 3, 1));
        CHECK(sim::VertexGrid::of(is).status() == S::HeightColumnWrongSize);
    }
}

TEST_CASE("vertex grid: the view changes no state and survives save and load") {
    sim::ScenarioSpec spec = sim::synthetic::empty_scenario();
    sim::ScenarioSpec::IslandSpec is = sim::synthetic::plain_island(0, 40, 24, 6, 4);
    is.terrain = sim::make_terrain(ramp(6, 4), std::vector<std::uint16_t>(24, 3),
                                   std::vector<std::uint8_t>(24, 1));
    spec.islands.push_back(is);
    const sim::World w = island_test::make_world(spec);
    const std::uint64_t hash_before = w.state_hash();
    REQUIRE(w.islands().size() == 1);
    REQUIRE(w.islands()[0].has_value());
    const sim::VertexGrid g = sim::VertexGrid::of(*w.islands()[0]);
    REQUIRE(g.ok());
    CHECK(*g.at(40 + 5, 24 + 3) == static_cast<std::uint8_t>(17 * 3 + 5 + 1));
    CHECK(*g.at(40 + 6, 24 + 3) == 0);
    CHECK(w.state_hash() == hash_before); // reading the view changes no state

    // The save frame carries the columns, so a loaded world gives the same view.
    const std::vector<std::uint8_t> frame = w.save();
    const sim::World w2 = island_test::load_world(frame);
    CHECK(w2.state_hash() == hash_before);
    const sim::VertexGrid g2 = sim::VertexGrid::of(*w2.islands()[0]);
    REQUIRE(g2.ok());
    for (std::int64_t y = 24; y <= 28; ++y) {
        for (std::int64_t x = 40; x <= 46; ++x) {
            CHECK(g2.at(x, y) == g.at(x, y));
        }
    }
}

namespace {

/// The original's runtime corner grid, built from the file's HEIGHT blocks with the rule
/// of AnnoWelt.dll 0x1008c18a (allocation (H + 2) * (W + 1) zeroes) and 0x10084870 (the
/// block copy, clamped to W + 1 columns and H + 1 rows). Independent of Terrain::height and
/// of the view.
std::vector<std::uint8_t> original_runtime_grid(const anno::formats::insel::Island& isl) {
    const std::size_t W = isl.width, H = isl.height, stride = W + 1;
    std::vector<std::uint8_t> grid((H + 2) * stride, 0);
    auto blocks = isl.blocks("HEIGHT");
    REQUIRE(blocks);
    for (const anno::formats::insel::Block& b : blocks.value()) {
        const std::size_t bw = b.width, bh = b.height;
        REQUIRE(b.data.size() == bw * bh);
        const std::size_t cols = std::min(bw, stride - std::min<std::size_t>(b.x, stride));
        const std::size_t rows = std::min(bh, (H + 1) - std::min<std::size_t>(b.y, H + 1));
        for (std::size_t r = 0; r < rows; ++r) {
            for (std::size_t c = 0; c < cols; ++c) {
                grid[(b.y + r) * stride + (b.x + c)] = b.data[r * bw + c];
            }
        }
    }
    return grid;
}

} // namespace

TEST_CASE("vertex grid: shipped scenarios through the existing loader agree with the original's "
          "runtime grid") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto loaded_types =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded_types.error.empty(), loaded_types.error);
    std::vector<fs::path> files;
    for (const auto& e : fs::recursive_directory_iterator(*dir / "Scenes")) {
        if (e.is_regular_file() && e.path().extension() == ".sze") {
            files.push_back(e.path());
        }
    }
    std::sort(files.begin(), files.end());
    REQUIRE_FALSE(files.empty());
    std::uint64_t islands = 0, vertices = 0, boundary = 0, nonsquare = 0, unmatched = 0;
    for (const fs::path& f : files) {
        INFO(f.string());
        const auto buf = anno::test::read_file(f);
        auto scenario = sim::load_scenario(buf, *loaded_types.types, sim::SimConfig{},
                                           original_component::actor_free_units());
        REQUIRE(scenario);
        auto raw = anno::formats::insel::islands(buf);
        REQUIRE(raw);
        const auto& spec_islands = scenario.value().spec.islands;
        // Islands placed from links are not in the file's INSEL5 list; compare the file's own
        // islands by number, which both sides keep.
        for (const anno::formats::insel::Island& isl : raw.value()) {
            const sim::ScenarioSpec::IslandSpec* match = nullptr;
            for (const auto& s : spec_islands) {
                if (s.slot.v == isl.number && s.w == isl.width && s.h == isl.height) {
                    match = &s;
                    break;
                }
            }
            if (!isl.record("HEIGHT")) {
                continue;
            }
            if (match == nullptr) {
                ++unmatched;
                continue;
            }
            sim::Island is;
            REQUIRE(is.tiles.reset(match->x0, match->y0, match->w, match->h, 8));
            is.terrain = match->terrain;
            const sim::VertexGrid g = sim::VertexGrid::of(is);
            REQUIRE(g.ok());
            const std::vector<std::uint8_t> grid = original_runtime_grid(isl);
            const std::size_t W = isl.width, H = isl.height, stride = W + 1;
            for (std::size_t y = 0; y <= H; ++y) {
                for (std::size_t x = 0; x <= W; ++x) {
                    const auto v = g.at(match->x0 + static_cast<std::int64_t>(x),
                                        match->y0 + static_cast<std::int64_t>(y));
                    REQUIRE(v.has_value());
                    REQUIRE(*v == grid[y * stride + x]);
                    ++vertices;
                    boundary += (x == W || y == H) ? 1 : 0;
                }
            }
            ++islands;
            nonsquare += (W != H) ? 1 : 0;
        }
    }
    MESSAGE("islands compared: " << islands << ", vertices: " << vertices << ", boundary vertices: "
                                 << boundary << ", non-square islands: " << nonsquare
                                 << ", HEIGHT islands without a loaded match: " << unmatched);
    CHECK(islands > 0);
    // A loader regression that drops islands must not shrink the coverage silently.
    CHECK(unmatched == 0);
    CHECK(boundary > 0);
}
