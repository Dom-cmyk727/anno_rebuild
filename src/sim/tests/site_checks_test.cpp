// Isolated founding-site helpers (task MAP-2 U2; docs/sim/ship-founding.md P7, P8, P10, P11,
// section 13). The coast-rule cases come from fixtures/synthetic/founding/coast_cases.txt, whose
// expected values were produced by emulating the original (tools/founding_fixtures.py); the
// geometry cases are derived by hand from the formulas of docs/re/ship-founding.md and use values
// chosen so that a swapped axis, a wrong bound or a wrong rounding direction fails.
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/site_checks.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/types.hpp>

namespace sim = anno::sim;
namespace site = anno::sim::site;

namespace {

sim::Island island_with(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h,
                        std::vector<std::uint8_t> height = {}) {
    sim::Island is;
    REQUIRE(is.tiles.reset(x0, y0, w, h, 8));
    is.terrain = sim::make_terrain(std::move(height), {}, {});
    return is;
}

sim::BuildingType type_of(std::uint32_t id, std::uint32_t prodtype, std::uint32_t flags,
                          std::uint32_t w = 1, std::uint32_t h = 1) {
    sim::BuildingType t;
    t.id = sim::TypeId{id};
    t.prodtype = prodtype;
    t.flags_word = flags;
    t.width = w;
    t.height = h;
    return t;
}

} // namespace

TEST_CASE("site: the click centres the footprint and clamps at zero") {
    using site::centred_origin;
    // (d - 1) / 2 truncates: a 4-wide footprint starts 1 tile left of the click, a 5-wide 2.
    CHECK(centred_origin(10, 20, 4, 4) == site::Origin{9, 19});
    CHECK(centred_origin(10, 20, 5, 5) == site::Origin{8, 18});
    CHECK(centred_origin(10, 20, 1, 1) == site::Origin{10, 20});
    // Axes are independent.
    CHECK(centred_origin(10, 20, 6, 2) == site::Origin{8, 20 - 0});
    CHECK(centred_origin(10, 20, 2, 6) == site::Origin{10 - 0, 18});
    // Clamp at 0 per axis.
    CHECK(centred_origin(1, 30, 6, 4) == site::Origin{0, 29});
    CHECK(centred_origin(0, 0, 4, 4) == site::Origin{0, 0});
    CHECK(centred_origin(-5, 3, 4, 4) == site::Origin{0, 2});
    // A zero size is the click itself; large values do not wrap.
    CHECK(centred_origin(7, 8, 0, 0) == site::Origin{7, 8});
    const std::int64_t big = std::int64_t{1} << 40;
    CHECK(centred_origin(big, big, 0xffffffffU, 3) ==
          site::Origin{big - (std::int64_t{0xffffffffU} - 1) / 2, big - 1});
}

TEST_CASE("site: the rotation trials are r, r+1, r+2, r+3 with the origin fixed") {
    CHECK(site::rotation_trials(0) == std::array<std::uint8_t, 4>{0, 1, 2, 3});
    CHECK(site::rotation_trials(1) == std::array<std::uint8_t, 4>{1, 2, 3, 0});
    CHECK(site::rotation_trials(2) == std::array<std::uint8_t, 4>{2, 3, 0, 1});
    CHECK(site::rotation_trials(3) == std::array<std::uint8_t, 4>{3, 0, 1, 2});
    // ADAPTATION: a rotation above 3 is reduced modulo 4.
    CHECK(site::rotation_trials(6) == site::rotation_trials(2));

    std::vector<std::uint8_t> seen;
    const auto r = site::first_accepted_rotation(2, [&](std::uint8_t rot) {
        seen.push_back(rot);
        return rot == 0;
    });
    REQUIRE(r.has_value());
    CHECK(*r == 0);
    CHECK(seen == std::vector<std::uint8_t>{2, 3, 0});
    seen.clear();
    CHECK_FALSE(site::first_accepted_rotation(1, [&](std::uint8_t rot) {
                    seen.push_back(rot);
                    return false;
                }).has_value());
    CHECK(seen == std::vector<std::uint8_t>{1, 2, 3, 0});
    // The first acceptable rotation wins even when a later one would also pass.
    CHECK(*site::first_accepted_rotation(3, [](std::uint8_t) { return true; }) == 3);
}

TEST_CASE("site: the footprint must lie inside the island on both axes") {
    // Non-square island 10 x 6 at (-3, 40).
    const sim::Island is = island_with(-3, 40, 10, 6);
    const sim::TileLayer& t = is.tiles;
    CHECK(site::footprint_inside(t, {-3, 40}, 10, 6)); // exactly the island
    CHECK(site::footprint_inside(t, {-3, 40}, 1, 1));
    CHECK(site::footprint_inside(t, {6, 45}, 1, 1));        // the last tile
    CHECK_FALSE(site::footprint_inside(t, {-4, 40}, 1, 1)); // lower x
    CHECK_FALSE(site::footprint_inside(t, {-3, 39}, 1, 1)); // lower y
    CHECK_FALSE(site::footprint_inside(t, {7, 40}, 1, 1));  // x = x0 + W
    CHECK_FALSE(site::footprint_inside(t, {-3, 46}, 1, 1)); // y = y0 + H
    CHECK_FALSE(site::footprint_inside(t, {5, 40}, 6, 1));  // right edge over by 1 (5+6 = 11 > 7)
    CHECK(site::footprint_inside(t, {4, 40}, 3, 1));        // right edge exact (4+3 = 7)
    CHECK_FALSE(site::footprint_inside(t, {-3, 43}, 1, 4)); // bottom edge over by 1
    CHECK(site::footprint_inside(t, {-3, 43}, 1, 3));
    // The two axes are not interchangeable: 10 wide fits, 10 tall does not.
    CHECK(site::footprint_inside(t, {-3, 40}, 10, 1));
    CHECK_FALSE(site::footprint_inside(t, {-3, 40}, 1, 10));
    // Huge sizes do not wrap.
    CHECK_FALSE(site::footprint_inside(t, {-3, 40}, 0xffffffffU, 1));
    CHECK_FALSE(
        site::footprint_inside(t, {std::numeric_limits<std::int64_t>::max() - 1, 40}, 1, 1));
}

TEST_CASE("site: the distance rule truncates toward zero and squares exactly") {
    const sim::FixedPos ship{10 * 256, 20 * 256}; // tile (10, 20)
    CHECK(site::within_range(10, 20, ship));
    CHECK(site::within_range(22, 20, ship));       // dx = 12
    CHECK_FALSE(site::within_range(23, 20, ship)); // dx = 13
    CHECK(site::within_range(-2, 20, ship));       // dx = -12
    CHECK_FALSE(site::within_range(-3, 20, ship));
    CHECK(site::within_range(10, 32, ship)); // dy = 12
    CHECK_FALSE(site::within_range(10, 33, ship));
    // d2 <= 168 accepted, 169 (= 13^2) rejected: 12^2 + 4^2 = 160, 12^2 + 5^2 = 169.
    CHECK(site::within_range(22, 24, ship));
    CHECK_FALSE(site::within_range(22, 25, ship));
    CHECK(site::within_range(10 + 8, 20 + 9, ship));        // 64 + 81 = 145
    CHECK(site::within_range(10 + 9, 20 + 9, ship));        // 162
    CHECK_FALSE(site::within_range(10 + 10, 20 + 9, ship)); // 181
    CHECK(site::within_range(10 + 12, 20 + 4, ship));
    CHECK(site::within_range(10 - 12, 20 - 4, ship));
    CHECK_FALSE(site::within_range(10 - 12, 20 - 5, ship));
    // A fractional ship position: the difference is truncated toward zero, not floored.
    const sim::FixedPos half{5 * 256 + 128, 0};      // x = 5.5
    CHECK(site::within_range(18, 0, half));          // 12.5 -> 12
    CHECK_FALSE(site::within_range(19, 0, half));    // 13.5 -> 13
    CHECK(site::within_range(-6, 0, half));          // -11.5 -> -11
    CHECK(site::within_range(-7, 0, half));          // -12.5 -> -12 (floor would give -13)
    CHECK_FALSE(site::within_range(-8, 0, half));    // -13.5 -> -13
    const sim::FixedPos just_under{13 * 256 - 1, 0}; // 12.996: dx = -12.996 -> -12, inside
    CHECK(site::within_range(0, 0, just_under));
    const sim::FixedPos exactly{13 * 256, 0};
    CHECK_FALSE(site::within_range(0, 0, exactly)); // dx = -13
    // The distance is data, not a constant.
    CHECK(site::within_range(13, 0, {0, 0}, site::DistanceRule{13}));
    CHECK_FALSE(site::within_range(14, 0, {0, 0}, site::DistanceRule{13}));
    CHECK(site::within_range(0, 0, {0, 0}, site::DistanceRule{0}));
    CHECK_FALSE(site::within_range(1, 0, {0, 0}, site::DistanceRule{0}));
    // Overflow guard: a click whose scaled value would overflow, and extreme ship positions.
    CHECK_FALSE(site::within_range(std::numeric_limits<std::int64_t>::max(), 0, {0, 0}));
    CHECK_FALSE(site::within_range(0, std::numeric_limits<std::int64_t>::min(), {0, 0}));
    CHECK_FALSE(site::within_range(0, 0, {std::numeric_limits<std::int32_t>::max(), 0}));
    CHECK_FALSE(site::within_range(0, 0, {0, std::numeric_limits<std::int32_t>::min()}));
    CHECK(site::within_range(0, 0, {-12 * 256, 0}, site::DistanceRule{12}));
    // A distance larger than any coordinate difference squares in 64 bits.
    CHECK(site::within_range(5, 5, {0, 0}, site::DistanceRule{0xffffffffU}));
}

TEST_CASE("site: ownership of the footprint, outside before owned") {
    sim::Island is = island_with(2, 3, 12, 8);
    sim::TileLayer& t = is.tiles;
    constexpr std::uint32_t neutral = 15;
    for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
        t.set_owner(i, neutral);
    }
    CHECK(site::footprint_ownership(t, neutral, {2, 3}, 4, 4) == site::Ownership::AllNeutral);
    CHECK(site::footprint_ownership(t, neutral, {10, 6}, 4, 5) == site::Ownership::AllNeutral);
    // One owned tile in the middle, own or rival alike.
    t.set_owner(t.index_of(5, 5), 3);
    CHECK(site::footprint_ownership(t, neutral, {4, 4}, 3, 3) == site::Ownership::OwnedTile);
    CHECK(site::footprint_ownership(t, neutral, {6, 4}, 3, 3) == site::Ownership::AllNeutral);
    CHECK(site::footprint_ownership(t, neutral, {2, 3}, 3, 2) == site::Ownership::AllNeutral);
    // The last row/column of the footprint is looked at (y = 5 is the third row of {4,3}+3x3).
    CHECK(site::footprint_ownership(t, neutral, {4, 3}, 3, 3) == site::Ownership::OwnedTile);
    CHECK(site::footprint_ownership(t, neutral, {4, 3}, 3, 2) == site::Ownership::AllNeutral);
    // A different neutral value is honoured (configuration, not a constant).
    CHECK(site::footprint_ownership(t, 3, {5, 5}, 1, 1) == site::Ownership::AllNeutral);
    CHECK(site::footprint_ownership(t, 14, {2, 3}, 1, 1) == site::Ownership::OwnedTile);
    // Outside the layer wins over everything.
    CHECK(site::footprint_ownership(t, neutral, {12, 3}, 4, 4) == site::Ownership::OutsideIsland);
    CHECK(site::footprint_ownership(t, neutral, {1, 3}, 1, 1) == site::Ownership::OutsideIsland);
}

TEST_CASE("site: overlap compatibility decides only what the original's slot decides here") {
    const sim::BuildingType warehouse = type_of(1302, 33, 0x2087, 4, 4);
    // Flag 0x20 (growing/raw objects, ruins, stones): compatible whatever the placed type is.
    CHECK(site::overlap_with_existing(warehouse, type_of(2, 81, 0x20)) == site::Compat::Compatible);
    CHECK(site::overlap_with_existing(type_of(9, 200, 0), type_of(2, 81, 0x20)) ==
          site::Compat::Compatible);
    // Placed group 0x21 (market/warehouse): another PRODTYPE is incompatible ...
    CHECK(site::overlap_with_existing(warehouse, type_of(3, 1, 0)) == site::Compat::Incompatible);
    // ... the same PRODTYPE needs bit 8 of the existing slot +0xd0, which is not read.
    CHECK(site::overlap_with_existing(warehouse, type_of(4, 33, 0)) == site::Compat::Unsupported);
    // Group 0x141 existing and a placed flag 0x10: compatible; without 0x10: incompatible.
    const sim::BuildingType g141 = type_of(5, 0x141, 0);
    CHECK(site::overlap_with_existing(type_of(6, 33, 0x2097), g141) == site::Compat::Compatible);
    CHECK(site::overlap_with_existing(warehouse, g141) == site::Compat::Incompatible);
    // Group of prodtype 32 is 1, of 33..64 is 0x21, of 65..96 is 0x41: groups 1/0x21/0x61 decided.
    CHECK(site::overlap_with_existing(type_of(7, 5, 0), type_of(3, 70, 0)) ==
          site::Compat::Incompatible);
    CHECK(site::overlap_with_existing(type_of(7, 97 + 32, 0), type_of(3, 70, 0)) ==
          site::Compat::Unsupported);
    // PRODTYPE 0 (UNUSED) as the placed type is a group the original has no row for here.
    CHECK(site::overlap_with_existing(type_of(8, 0, 0), type_of(3, 70, 0)) ==
          site::Compat::Unsupported);
}

TEST_CASE("site: footprint occupancy takes the worst tile") {
    sim::TypeRegistry::Builder b;
    const sim::BuildingType warehouse = type_of(1302, 33, 0x2087, 4, 4);
    b.add(warehouse);
    b.add(type_of(500, 81, 0x20)); // tree-like, overlappable
    b.add(type_of(501, 1, 0));     // production: not overlappable by a market type
    b.add(type_of(502, 33, 0));    // same PRODTYPE: unsupported (slot +0xd0 not read)
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    const sim::TypeRegistry& reg = *built.value();

    sim::Island is = island_with(0, 0, 8, 8);
    sim::TileLayer& t = is.tiles;
    CHECK(site::footprint_occupancy(t, reg, warehouse, {2, 2}, 4, 4) == site::Compat::Compatible);
    t.set_object(t.index_of(3, 3), 500);
    CHECK(site::footprint_occupancy(t, reg, warehouse, {2, 2}, 4, 4) == site::Compat::Compatible);
    t.set_object(t.index_of(5, 5), 502);
    CHECK(site::footprint_occupancy(t, reg, warehouse, {2, 2}, 4, 4) == site::Compat::Unsupported);
    t.set_object(t.index_of(2, 5), 501);
    CHECK(site::footprint_occupancy(t, reg, warehouse, {2, 2}, 4, 4) == site::Compat::Incompatible);
    // Tiles outside the footprint are not looked at.
    CHECK(site::footprint_occupancy(t, reg, warehouse, {0, 0}, 2, 2) == site::Compat::Compatible);
    // An id the registry does not know is unsupported, not accepted.
    t.set_object(t.index_of(0, 0), 9999);
    CHECK(site::footprint_occupancy(t, reg, warehouse, {0, 0}, 2, 2) == site::Compat::Unsupported);
    // A footprint leaving the layer is not decided here.
    CHECK(site::footprint_occupancy(t, reg, warehouse, {6, 6}, 4, 4) == site::Compat::Unsupported);
}

namespace {

struct CoastCase {
    std::string name;
    std::uint32_t w = 0, h = 0;
    std::uint8_t rot = 0;
    std::uint32_t flags = 0;
    std::uint32_t win_w = 0, win_h = 0;
    int expected = 0;
    std::vector<std::uint8_t> grid;
};

std::vector<CoastCase> load_coast_cases() {
    const std::filesystem::path p =
        std::filesystem::path(ANNO_FIXTURES_DIR) / "founding" / "coast_cases.txt";
    std::ifstream in(p);
    REQUIRE_MESSAGE(in.good(), "missing fixture " << p.string());
    std::vector<CoastCase> out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ss(line);
        CoastCase c;
        unsigned rot = 0;
        std::string flags, hex;
        ss >> c.name >> c.w >> c.h >> rot >> flags >> c.win_w >> c.win_h >> c.expected >> hex;
        REQUIRE_MESSAGE(!ss.fail(), "bad fixture line: " << line);
        c.rot = static_cast<std::uint8_t>(rot);
        c.flags = static_cast<std::uint32_t>(std::stoul(flags, nullptr, 16));
        REQUIRE(hex.size() == 2U * c.win_w * c.win_h);
        for (std::size_t i = 0; i < hex.size(); i += 2) {
            c.grid.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
        }
        out.push_back(std::move(c));
    }
    return out;
}

} // namespace

TEST_CASE("site: coast height rule equals the emulated original on the fixtures") {
    const std::vector<CoastCase> cases = load_coast_cases();
    REQUIRE(cases.size() > 100);
    std::size_t accepted = 0, rejected = 0, nonsquare_rot_odd = 0;
    for (const CoastCase& c : cases) {
        INFO(c.name);
        REQUIRE(site::coast_class_supported(c.flags));
        const bool odd = (c.rot & 1U) != 0;
        REQUIRE(c.win_w == (odd ? c.h : c.w) + 1);
        REQUIRE(c.win_h == (odd ? c.w : c.h) + 1);
        const site::TerrainVerdict v =
            site::coast_height_rule(c.w, c.h, c.rot, [&](std::uint64_t dx, std::uint64_t dy) {
                return dx < c.win_w && dy < c.win_h
                           ? std::optional<std::uint8_t>(c.grid[dy * c.win_w + dx])
                           : std::nullopt;
            });
        if (c.expected == -1) {
            CHECK(v.result == site::TerrainResult::Rejected);
            ++rejected;
        } else {
            CHECK(v.result == site::TerrainResult::Accepted);
            CHECK(v.result_byte == static_cast<std::uint8_t>(c.expected));
            ++accepted;
        }
        nonsquare_rot_odd += (c.w != c.h && odd) ? 1 : 0;
    }
    CHECK(accepted > 50);
    CHECK(rejected > 50);
    CHECK(nonsquare_rot_odd > 10);
}

TEST_CASE(
    "site: the non-square rotation-1 pair fails on an edge length taken from the rotated size") {
    const std::vector<CoastCase> cases = load_coast_cases();
    const CoastCase* ok = nullptr;
    const CoastCase* bad = nullptr;
    for (const CoastCase& c : cases) {
        ok = c.name == "nonsquare-4x3-rot1-valid" ? &c : ok;
        bad = c.name == "nonsquare-4x3-rot1-fifth-shore-vertex-wrong" ? &c : bad;
    }
    REQUIRE(ok != nullptr);
    REQUIRE(bad != nullptr);
    CHECK(ok->expected == 0x30);
    CHECK(bad->expected == -1);
    // The mistake the pair guards: n = rotated width + 1 = 4 looks at y = 0..3 only and so
    // accepts the second grid. Its fifth shore vertex (y = 4) is 0x10, not 0x30.
    CHECK(bad->grid[4 * bad->win_w + 0] == 0x10);
    for (std::uint32_t y = 0; y < 4; ++y) {
        CHECK(bad->grid[y * bad->win_w] == 0x30);
    }
}

TEST_CASE("site: the coast class is the decoded flag set only") {
    CHECK(site::coast_class_supported(0x2087));
    CHECK(site::coast_class_supported(0x2087 | 0x8)); // bits outside the mask
    CHECK_FALSE(site::coast_class_supported(0x4087)); // road class
    CHECK_FALSE(site::coast_class_supported(0x1087)); // mountain class
    CHECK_FALSE(site::coast_class_supported(0x3087));
    CHECK_FALSE(site::coast_class_supported(0x0087));       // plain
    CHECK_FALSE(site::coast_class_supported(0x2086));       // mode (f & 3) != 3
    CHECK_FALSE(site::coast_class_supported(0x2097));       // bit 0x10 set
    CHECK_FALSE(site::coast_class_supported(0x2007 ^ 0x4)); // bit 0x04 clear
}

TEST_CASE("site: the coast rule over an island's vertex grid") {
    // A 4x4 warehouse at rotation 0 needs shore row y = 0 = 0x30 and the row y = 4 below 0x20.
    // Island 10 x 8 at (3, -2); heights per tile (W x H); column W and row H read 0.
    constexpr std::uint32_t W = 10, H = 8;
    constexpr std::int32_t X0 = 3, Y0 = -2;
    std::vector<std::uint8_t> h(W * H, 0x20);
    // Window origin (5, 0) -> tile-local (2, 2): shore row local y = 2 (x = 2..6), water at y = 6.
    for (std::uint32_t x = 2; x <= 6; ++x) {
        h[2 * W + x] = 0x30;
        h[6 * W + x] = 0x10;
    }
    sim::Island is = island_with(X0, Y0, W, H, h);
    const sim::BuildingType warehouse = type_of(1302, 33, 0x2087, 4, 4);
    const sim::VertexGrid g = sim::VertexGrid::of(is);
    REQUIRE(g.ok());
    auto v = site::coast_height_check(g, warehouse, 0, {X0 + 2, Y0 + 2});
    CHECK(v.result == site::TerrainResult::Accepted);
    CHECK(v.result_byte == 0x30);
    // Rotation 2 expects the shore on the bottom row: rejected here.
    CHECK(site::coast_height_check(g, warehouse, 2, {X0 + 2, Y0 + 2}).result ==
          site::TerrainResult::Rejected);
    // One column to the right: the shore row has a 0x20 vertex at x = 7 ... still local x 3..7
    v = site::coast_height_check(g, warehouse, 0, {X0 + 3, Y0 + 2});
    CHECK(v.result == site::TerrainResult::Rejected);
    // A window reaching the corner vertices: tile (6..9) x (4..7) -> vertices up to (10, 8), the
    // boundary column and row read 0. All zero: the max is 0, not 0x30: rejected, readable.
    v = site::coast_height_check(g, warehouse, 0, {X0 + 6, Y0 + 4});
    CHECK(v.result == site::TerrainResult::Rejected);
    // A window one vertex beyond the grid is unreadable, never a guess.
    CHECK(site::coast_height_check(g, warehouse, 0, {X0 + 7, Y0 + 4}).result ==
          site::TerrainResult::Unreadable);
    CHECK(site::coast_height_check(g, warehouse, 0, {X0 - 1, Y0}).result ==
          site::TerrainResult::Unreadable);
    // Unsupported class is reported before any read.
    CHECK(site::coast_height_check(g, type_of(1, 33, 0x4087, 4, 4), 0, {X0 + 2, Y0 + 2}).result ==
          site::TerrainResult::Unsupported);
    // Missing terrain data: unreadable, not rejected.
    sim::Island none;
    REQUIRE(none.tiles.reset(0, 0, 8, 8, 8));
    const sim::VertexGrid gn = sim::VertexGrid::of(none);
    CHECK(site::coast_height_check(gn, warehouse, 0, {0, 0}).result ==
          site::TerrainResult::Unreadable);
}
