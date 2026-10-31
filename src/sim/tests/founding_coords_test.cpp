// Coordinate-indexing regressions of the founding helpers (task MAP-2 package A).
//
// TileLayer::reset accepts an origin up to INT32_MAX, so a layer can hold tiles whose world
// coordinate is above INT32_MAX (x0 = INT32_MAX, width 2 reaches INT32_MAX + 1). The helpers take
// footprints in 64-bit world coordinates and must address such tiles by their LOCAL offset from
// the layer's origin, never by a narrowed 32-bit world coordinate. Each case below first checks
// the layer really contains the tile, then that the helper answers for that very tile.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/island.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/territory.hpp>
#include <anno/sim/types.hpp>

namespace sim = anno::sim;
namespace site = anno::sim::site;
namespace tr = anno::sim::territory;

namespace {

constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();
constexpr std::uint32_t kNeutral = 15;

sim::TileLayer layer(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h) {
    sim::TileLayer t;
    REQUIRE(t.reset(x0, y0, w, h, 8));
    for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
        t.set_owner(i, kNeutral);
    }
    return t;
}

std::vector<std::uint32_t> owners(const sim::TileLayer& t) {
    std::vector<std::uint32_t> v;
    for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
        v.push_back(t.owner(i));
    }
    return v;
}

sim::BuildingType type_of(std::uint32_t id, std::uint32_t prodtype, std::uint32_t flags) {
    sim::BuildingType t;
    t.id = sim::TypeId{id};
    t.prodtype = prodtype;
    t.flags_word = flags;
    return t;
}

struct Case {
    const char* name;
    std::int32_t x0, y0;
    std::uint32_t w, h;
};

// Both axes across the boundary, one axis each, the far corner, and a layer ending exactly at
// INT32_MAX (no tile above it) as the control.
const Case kCases[] = {
    {"both axes cross", kMax, kMax, 2, 2},
    {"x crosses", kMax - 1, 100, 4, 3},
    {"y crosses", -7, kMax - 2, 3, 5},
    {"both cross, larger", kMax - 2, kMax - 1, 5, 4},
    {"ends exactly at INT32_MAX", kMax - 3, kMax - 3, 4, 4},
};

} // namespace

TEST_CASE("founding coordinates: the layers really hold tiles above INT32_MAX") {
    const sim::TileLayer t = layer(kMax, kMax, 2, 2);
    CHECK(t.tile_count() == 4);
    // The tile above INT32_MAX has no 32-bit world coordinate; its local offset (1, 1) is index 3.
    CHECK(t.contains(kMax, kMax));
    CHECK(t.index_of(kMax, kMax) == 0);
}

TEST_CASE("founding coordinates: ownership answers for every tile of the footprint") {
    for (const Case& c : kCases) {
        INFO(std::string(c.name));
        // All neutral: the whole layer as one footprint is AllNeutral.
        sim::TileLayer t = layer(c.x0, c.y0, c.w, c.h);
        CHECK(site::footprint_ownership(t, kNeutral, {c.x0, c.y0}, c.w, c.h) ==
              site::Ownership::AllNeutral);
        // One non-neutral tile anywhere (including the tiles above INT32_MAX) is found, for
        // the whole-layer footprint and for the 1x1 footprint at that tile.
        for (std::uint32_t ly = 0; ly < c.h; ++ly) {
            for (std::uint32_t lx = 0; lx < c.w; ++lx) {
                sim::TileLayer u = layer(c.x0, c.y0, c.w, c.h);
                u.set_owner(std::uint64_t{ly} * c.w + lx, 4);
                CHECK(site::footprint_ownership(u, kNeutral, {c.x0, c.y0}, c.w, c.h) ==
                      site::Ownership::OwnedTile);
                CHECK(site::footprint_ownership(u, kNeutral,
                                                {std::int64_t{c.x0} + lx, std::int64_t{c.y0} + ly},
                                                1, 1) == site::Ownership::OwnedTile);
                // A neighbouring 1x1 footprint is neutral.
                const std::uint32_t nx = (lx + 1) % c.w;
                const std::uint32_t ny = (nx == 0) ? (ly + 1) % c.h : ly;
                if (nx != lx || ny != ly) {
                    CHECK(site::footprint_ownership(
                              u, kNeutral, {std::int64_t{c.x0} + nx, std::int64_t{c.y0} + ny}, 1,
                              1) == site::Ownership::AllNeutral);
                }
            }
        }
        // One past the layer in either axis is outside, also at the 32-bit boundary.
        CHECK(site::footprint_ownership(t, kNeutral, {std::int64_t{c.x0} + c.w, c.y0}, 1, 1) ==
              site::Ownership::OutsideIsland);
        CHECK(site::footprint_ownership(t, kNeutral, {c.x0, std::int64_t{c.y0} + c.h}, 1, 1) ==
              site::Ownership::OutsideIsland);
    }
}

TEST_CASE(
    "founding coordinates: occupancy sees an object on every tile, including above INT32_MAX") {
    sim::TypeRegistry::Builder b;
    const sim::BuildingType warehouse = type_of(1302, 33, 0x2087);
    b.add(warehouse);
    b.add(type_of(500, 81, 0x20)); // overlappable
    b.add(type_of(501, 1, 0));     // not overlappable by a market type
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    const sim::TypeRegistry& reg = *built.value();
    for (const Case& c : kCases) {
        INFO(std::string(c.name));
        for (std::uint32_t ly = 0; ly < c.h; ++ly) {
            for (std::uint32_t lx = 0; lx < c.w; ++lx) {
                const std::uint64_t idx = std::uint64_t{ly} * c.w + lx;
                const site::Origin whole{c.x0, c.y0};
                sim::TileLayer t = layer(c.x0, c.y0, c.w, c.h);
                CHECK(site::footprint_occupancy(t, reg, warehouse, whole, c.w, c.h) ==
                      site::Compat::Compatible);
                t.set_object(idx, 501);
                CHECK(site::footprint_occupancy(t, reg, warehouse, whole, c.w, c.h) ==
                      site::Compat::Incompatible);
                t.set_object(idx, 500);
                CHECK(site::footprint_occupancy(t, reg, warehouse, whole, c.w, c.h) ==
                      site::Compat::Compatible);
                t.set_object(idx, 9999); // unknown id
                CHECK(site::footprint_occupancy(t, reg, warehouse, whole, c.w, c.h) ==
                      site::Compat::Unsupported);
            }
        }
    }
}

TEST_CASE("founding coordinates: rectangle claims change exactly the footprint's tiles") {
    for (const Case& c : kCases) {
        INFO(std::string(c.name));
        // The whole layer, then every 1x1 footprint, then the 2x2 footprints that fit.
        {
            sim::TileLayer t = layer(c.x0, c.y0, c.w, c.h);
            const tr::Result r = tr::claim_neutral_rect(t, kNeutral, 3, {c.x0, c.y0, c.w, c.h});
            CHECK(r.status == tr::Status::Ok);
            CHECK(r.claimed == t.tile_count());
            for (const std::uint32_t o : owners(t)) {
                CHECK(o == 3);
            }
            CHECK(t.ignored_writes() == 0);
        }
        for (std::uint32_t ly = 0; ly < c.h; ++ly) {
            for (std::uint32_t lx = 0; lx < c.w; ++lx) {
                sim::TileLayer t = layer(c.x0, c.y0, c.w, c.h);
                std::vector<std::uint32_t> want = owners(t);
                want[std::size_t{ly} * c.w + lx] = 3;
                const tr::Result r = tr::claim_neutral_rect(
                    t, kNeutral, 3, {std::int64_t{c.x0} + lx, std::int64_t{c.y0} + ly, 1, 1});
                CHECK(r.status == tr::Status::Ok);
                CHECK(r.claimed == 1);
                CHECK(owners(t) == want);
                CHECK(t.ignored_writes() == 0);
            }
        }
        // A non-neutral tile survives, and the count is of changed tiles only.
        sim::TileLayer t = layer(c.x0, c.y0, c.w, c.h);
        t.set_owner(t.tile_count() - 1, 9); // the last tile (above INT32_MAX in the crossing cases)
        const tr::Result r = tr::claim_neutral_rect(t, kNeutral, 3, {c.x0, c.y0, c.w, c.h});
        CHECK(r.claimed == t.tile_count() - 1);
        CHECK(t.owner(t.tile_count() - 1) == 9);
        std::uint64_t three = 0;
        for (const std::uint32_t o : owners(t)) {
            three += o == 3;
        }
        CHECK(three == r.claimed);
        CHECK(t.ignored_writes() == 0);
    }
}

TEST_CASE("founding coordinates: disc claims reach tiles above INT32_MAX and count what changed") {
    for (const Case& c : kCases) {
        INFO(std::string(c.name));
        for (const std::uint32_t radius : {1U, 2U, 5U}) {
            INFO("radius " << radius);
            // A footprint in the layer's last row/column corner (the tile above INT32_MAX when it
            // crosses), and one at the origin corner.
            const std::int64_t fx[2] = {std::int64_t{c.x0} + c.w - 1, c.x0};
            const std::int64_t fy[2] = {std::int64_t{c.y0} + c.h - 1, c.y0};
            for (int k = 0; k < 2; ++k) {
                sim::TileLayer t = layer(c.x0, c.y0, c.w, c.h);
                const std::vector<std::uint32_t> before = owners(t);
                const tr::Result r =
                    tr::claim_neutral_disc(t, kNeutral, 3, {fx[k], fy[k], 1, 1}, radius, 64);
                REQUIRE(r.status == tr::Status::Ok);
                const std::vector<std::uint32_t> after = owners(t);
                std::uint64_t changed = 0;
                for (std::size_t i = 0; i < after.size(); ++i) {
                    changed += after[i] != before[i];
                    CHECK((after[i] == before[i] || after[i] == 3));
                }
                CHECK(r.claimed == changed);
                CHECK(changed >= 1);
                // The footprint tile itself is claimed.
                const std::uint64_t own = (static_cast<std::uint64_t>(fy[k] - c.y0)) * c.w +
                                          static_cast<std::uint64_t>(fx[k] - c.x0);
                CHECK(t.owner(own) == 3);
                CHECK(t.ignored_writes() == 0);
            }
        }
    }
    // Every tile of a 2x2 layer at (INT32_MAX, INT32_MAX) is within a radius-2 disc of any tile.
    sim::TileLayer t = layer(kMax, kMax, 2, 2);
    CHECK(tr::claim_neutral_disc(t, kNeutral, 3, {kMax, kMax, 1, 1}, 2, 64).claimed == 4);
    for (const std::uint32_t o : owners(t)) {
        CHECK(o == 3);
    }
}

TEST_CASE("founding coordinates: controls at ordinary, negative and out-of-bounds origins") {
    // Ordinary.
    sim::TileLayer a = layer(10, 20, 6, 4);
    CHECK(tr::claim_neutral_rect(a, kNeutral, 3, {11, 21, 2, 2}).claimed == 4);
    CHECK(a.owner(a.index_of(11, 21)) == 3);
    CHECK(a.owner(a.index_of(10, 20)) == kNeutral);
    // Negative origin and a layer at INT32_MIN.
    sim::TileLayer b = layer(-5, -9, 4, 4);
    CHECK(tr::claim_neutral_rect(b, kNeutral, 3, {-4, -8, 2, 2}).claimed == 4);
    sim::TileLayer m = layer(kMin, kMin, 3, 3);
    CHECK(tr::claim_neutral_disc(m, kNeutral, 3,
                                 {std::int64_t{kMin} + 1, std::int64_t{kMin} + 1, 1, 1}, 1, 64)
              .claimed >= 1);
    CHECK(m.ignored_writes() == 0);
    // Out of bounds at the 32-bit boundary: refused before any mutation.
    sim::TileLayer c = layer(kMax, kMax, 2, 2);
    const std::vector<std::uint32_t> before = owners(c);
    CHECK(tr::claim_neutral_rect(c, kNeutral, 3, {kMax, kMax, 3, 1}).status ==
          tr::Status::BadFootprint);
    CHECK(tr::claim_neutral_rect(c, kNeutral, 3, {std::int64_t{kMax} + 2, kMax, 1, 1}).status ==
          tr::Status::BadFootprint);
    CHECK(tr::claim_neutral_disc(c, kNeutral, 3, {kMax, std::int64_t{kMax} + 2, 1, 1}, 2, 64)
              .status == tr::Status::BadFootprint);
    CHECK(owners(c) == before);
    CHECK(c.ignored_writes() == 0);
    CHECK_FALSE(site::footprint_inside(c, {kMax, kMax}, 3, 1));
    CHECK(site::footprint_inside(c, {kMax, kMax}, 2, 2));
}
