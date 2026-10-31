// The neutral-tile territory claim (task MAP-2 U3; docs/re/ship-founding.md section 11.2).
// The fixtures (fixtures/synthetic/founding/territory_cases.txt) were produced by emulating
// the original island slot +0xe8 (tools/territory_fixtures.py); the remaining cases check
// the properties of the contract by hand: neutral-only mutation, radius 0, configured
// neutral owner, rejection without change, widened arithmetic, dirty/hash accounting.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/island.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/territory.hpp>
#include <anno/sim/world.hpp>

#include "founding_fixture.hpp"
#include "island_helpers.hpp"

namespace sim = anno::sim;
namespace tr = anno::sim::territory;

namespace {

constexpr std::uint32_t kNeutral = 15;
constexpr std::uint32_t kMaxRadius =
    4096; // the tests' own bound (the contract's callers pass theirs)

sim::TileLayer make_layer(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h,
                          std::uint32_t fill = kNeutral) {
    sim::TileLayer t;
    REQUIRE(t.reset(x0, y0, w, h, 8));
    for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
        t.set_owner(i, fill);
    }
    return t;
}

std::uint64_t fnv_owners(const sim::TileLayer& t) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
        h = (h ^ static_cast<std::uint8_t>(t.owner(i))) * 0x100000001b3ULL;
    }
    return h;
}

std::string hex_rows(const sim::TileLayer& t) {
    std::string out;
    for (std::uint32_t y = 0; y < t.height(); ++y) {
        if (y != 0) {
            out += '/';
        }
        for (std::uint32_t x = 0; x < t.width(); ++x) {
            out += "0123456789abcdef"[t.owner(std::uint64_t{y} * t.width() + x) & 15U];
        }
    }
    return out;
}

std::vector<std::int64_t> ints(const std::string& s) {
    std::vector<std::int64_t> v;
    std::istringstream in(s);
    std::string tok;
    while (std::getline(in, tok, ',')) {
        v.push_back(std::stoll(tok));
    }
    return v;
}

std::string field(const std::string& line, const std::string& key) {
    const std::string k = " " + key + "=";
    const std::size_t p = line.find(k);
    REQUIRE_MESSAGE(p != std::string::npos, "no " << key << " in " << line);
    const std::size_t b = p + k.size();
    return line.substr(b, line.find(' ', b) - b);
}

/// Applies a fixture case the way the founding order does: the disc (radius > 0), then, for
/// pair cases, the footprint rectangle; a radius-0 case is the rectangle call alone.
tr::Result apply_case(sim::TileLayer& t, const tr::Footprint& f, std::uint32_t city,
                      std::uint32_t radius, bool pair) {
    tr::Result r = tr::claim_neutral_disc(t, kNeutral, city, f, radius, kMaxRadius);
    REQUIRE(r.status == tr::Status::Ok);
    if (pair) {
        const tr::Result r2 = tr::claim_neutral_rect(t, kNeutral, city, f);
        REQUIRE(r2.status == tr::Status::Ok);
        r.claimed += r2.claimed;
    }
    return r;
}

} // namespace

TEST_CASE("territory: the disc profile of the original") {
    // R = 24 (the warehouse's action radius): the table printed from the original's loop.
    const std::vector<std::uint32_t> want = {24, 24, 24, 24, 24, 23, 23, 23, 23, 22, 22, 21, 21,
                                             20, 19, 19, 18, 17, 16, 15, 13, 12, 10, 8,  4};
    CHECK(tr::disc_profile(24) == want);
    CHECK(tr::disc_profile(0).empty());
    // Shape properties for every radius up to a few thousand: R + 1 entries, non-increasing for
    // R > 3, the first entry is R, nothing wraps at a byte (values above 255 occur).
    for (std::uint32_t r = 4; r <= 3000; r += (r < 40 ? 1 : 97)) {
        const std::vector<std::uint32_t> t = tr::disc_profile(r);
        REQUIRE(t.size() == std::size_t{r} + 1);
        CHECK(t[0] == r);
        CHECK(std::is_sorted(t.begin(), t.end(), std::greater<>()));
        CHECK(t[r] > 0);
    }
    CHECK(tr::disc_profile(1000)[0] == 1000);
}

TEST_CASE("territory: the emulated original's owner grids") {
    const std::vector<std::string> lines = founding_fixture::read_lines("territory_cases.txt");
    std::map<std::string, std::string> grids;
    std::size_t cases = 0, with_pre = 0, pairs = 0, radius0 = 0, edge_clipped = 0;
    std::uint64_t total_claimed = 0;
    for (const std::string& line : lines) {
        if (line.rfind("grid ", 0) == 0) {
            std::istringstream ss(line);
            std::string tag, name, rows;
            ss >> tag >> name >> rows;
            grids[name] = rows;
        }
    }
    for (const std::string& line : lines) {
        if (line.rfind("case ", 0) != 0) {
            continue;
        }
        std::istringstream ss(line);
        std::string tag, name;
        ss >> tag >> name;
        INFO(name);
        const std::vector<std::int64_t> isl = ints(field(line, "isl"));
        const std::vector<std::int64_t> rect = ints(field(line, "rect"));
        const auto city = static_cast<std::uint32_t>(std::stoul(field(line, "city")));
        const auto radius = static_cast<std::uint32_t>(std::stoul(field(line, "radius")));
        const bool pair = field(line, "pair") == "1";
        const std::string pre = field(line, "pre");
        sim::TileLayer t =
            make_layer(static_cast<std::int32_t>(isl[0]), static_cast<std::int32_t>(isl[1]),
                       static_cast<std::uint32_t>(isl[2]), static_cast<std::uint32_t>(isl[3]));
        if (pre != "-") {
            ++with_pre;
            std::istringstream ps(pre);
            std::string tok;
            while (std::getline(ps, tok, ';')) {
                std::int64_t x = 0, y = 0, o = 0;
                char c1 = 0, c2 = 0;
                std::istringstream ts(tok);
                ts >> x >> c1 >> y >> c2 >> o;
                t.set_owner(t.index_of(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)),
                            static_cast<std::uint32_t>(o));
            }
        }
        const tr::Footprint f{rect[0], rect[1], static_cast<std::uint32_t>(rect[2]),
                              static_cast<std::uint32_t>(rect[3])};
        const tr::Result r = apply_case(t, f, city, radius, pair);
        CHECK(r.claimed == std::stoull(field(line, "claimed")));
        char buf[17];
        std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(fnv_owners(t)));
        CHECK(std::string(buf) == field(line, "fnv"));
        const auto g = grids.find(name);
        if (g != grids.end()) {
            CHECK(hex_rows(t) == g->second);
        }
        CHECK(t.ignored_writes() == 0);
        ++cases;
        pairs += pair ? 1 : 0;
        radius0 += radius == 0 ? 1 : 0;
        const std::int64_t reach = radius;
        edge_clipped += (rect[0] - reach < isl[0] || rect[1] - reach < isl[1] ||
                         rect[0] + rect[2] + reach > isl[0] + isl[2] ||
                         rect[1] + rect[3] + reach > isl[1] + isl[3])
                            ? 1
                            : 0;
        total_claimed += r.claimed;
    }
    // The set exercises what the contract names.
    CHECK(cases > 150);
    CHECK(with_pre > 30);
    CHECK(pairs > 20);
    CHECK(radius0 > 15);
    CHECK(edge_clipped > 40);
    CHECK(total_claimed > 10000);
    CHECK(grids.size() > 10);
}

TEST_CASE("territory: only neutral tiles change; own and rival tiles survive") {
    sim::TileLayer t = make_layer(0, 0, 30, 30);
    // Own city 4 block and rival city 9 block, both inside the disc of R = 6 around (12, 12) 4x4.
    for (std::int32_t y = 10; y < 13; ++y) {
        for (std::int32_t x = 8; x < 11; ++x) {
            t.set_owner(t.index_of(x, y), 4);
        }
    }
    for (std::int32_t y = 14; y < 17; ++y) {
        for (std::int32_t x = 14; x < 17; ++x) {
            t.set_owner(t.index_of(x, y), 9);
        }
    }
    const tr::Footprint f{11, 11, 4, 4};
    const tr::Result r = tr::claim_neutral_disc(t, kNeutral, 4, f, 6, kMaxRadius);
    REQUIRE(r.status == tr::Status::Ok);
    for (std::int32_t y = 14; y < 17; ++y) {
        for (std::int32_t x = 14; x < 17; ++x) {
            CHECK(t.owner(t.index_of(x, y)) == 9);
        }
    }
    std::uint64_t own = 0, rival = 0, neutral = 0;
    for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
        own += t.owner(i) == 4;
        rival += t.owner(i) == 9;
        neutral += t.owner(i) == kNeutral;
    }
    CHECK(rival == 9);
    CHECK(own == 9 + r.claimed);
    CHECK(neutral == 900 - 9 - own);
    // Idempotent: a second claim finds nothing neutral inside the disc.
    CHECK(tr::claim_neutral_disc(t, kNeutral, 4, f, 6, kMaxRadius).claimed == 0);
    // A different city does not take over the first city's tiles.
    const std::uint64_t before = fnv_owners(t);
    CHECK(tr::claim_neutral_disc(t, kNeutral, 7, f, 6, kMaxRadius).claimed == 0);
    CHECK(fnv_owners(t) == before);
}

TEST_CASE("territory: radius 0 is the footprint rectangle, also for the disc entry") {
    sim::TileLayer a = make_layer(5, -3, 11, 9);
    sim::TileLayer b = make_layer(5, -3, 11, 9);
    sim::TileLayer c = make_layer(5, -3, 11, 9);
    const tr::Footprint f{7, -1, 3, 2};
    CHECK(tr::claim_neutral_rect(a, kNeutral, 2, f).claimed == 6);
    CHECK(tr::claim_neutral_disc(b, kNeutral, 2, f, 0, kMaxRadius).claimed == 6);
    CHECK(fnv_owners(a) == fnv_owners(b));
    CHECK(a.owner(a.index_of(7, -1)) == 2);
    CHECK(a.owner(a.index_of(9, 0)) == 2);
    CHECK(a.owner(a.index_of(10, 0)) == kNeutral);
    CHECK(a.owner(a.index_of(7, 1)) == kNeutral);
    // The disc with radius 1 around the same footprint is strictly larger.
    CHECK(tr::claim_neutral_disc(c, kNeutral, 2, f, 1, kMaxRadius).claimed > 6);
    // max_radius is not consulted for radius 0 beyond the comparison: 0 <= 0.
    sim::TileLayer d = make_layer(0, 0, 4, 4);
    CHECK(tr::claim_neutral_disc(d, kNeutral, 1, {1, 1, 1, 1}, 0, 0).claimed == 1);
}

TEST_CASE("territory: the configured neutral owner decides what is claimable") {
    // Same layout with neutral 0 / city 5 as with neutral 15 / city 5 after relabelling.
    sim::TileLayer a = make_layer(0, 0, 24, 24, kNeutral);
    sim::TileLayer b = make_layer(0, 0, 24, 24, 0);
    for (std::int32_t y = 9; y < 12; ++y) {
        for (std::int32_t x = 6; x < 9; ++x) {
            a.set_owner(a.index_of(x, y), 9);
            b.set_owner(b.index_of(x, y), 9);
        }
    }
    const tr::Footprint f{10, 10, 4, 4};
    const tr::Result ra = tr::claim_neutral_disc(a, kNeutral, 5, f, 8, kMaxRadius);
    const tr::Result rb = tr::claim_neutral_disc(b, 0, 5, f, 8, kMaxRadius);
    CHECK(ra.claimed == rb.claimed);
    for (std::uint64_t i = 0; i < a.tile_count(); ++i) {
        const std::uint32_t va = a.owner(i) == kNeutral ? 0 : a.owner(i);
        CHECK(va == b.owner(i));
    }
    // A layer whose tiles are owner 15 but whose neutral owner is 0: nothing is claimable.
    sim::TileLayer c = make_layer(0, 0, 8, 8, 15);
    CHECK(tr::claim_neutral_rect(c, 0, 5, {2, 2, 2, 2}).claimed == 0);
    // The city owner may be any value other than the neutral one (here above a nibble).
    sim::TileLayer d = make_layer(0, 0, 8, 8, 15);
    CHECK(tr::claim_neutral_rect(d, 15, 300, {2, 2, 2, 2}).claimed == 4);
    CHECK(d.owner(d.index_of(2, 2)) == 300);
}

TEST_CASE("territory: odd and even footprints, centre-row visits counted once") {
    for (std::uint32_t w = 1; w <= 6; ++w) {
        for (std::uint32_t h = 1; h <= 6; ++h) {
            sim::TileLayer t = make_layer(2, 3, 41, 37);
            const tr::Footprint f{17, 15, w, h};
            const tr::Result r = tr::claim_neutral_disc(t, kNeutral, 1, f, 11, kMaxRadius);
            REQUIRE(r.status == tr::Status::Ok);
            std::uint64_t city = 0;
            for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
                city += t.owner(i) == 1;
            }
            INFO(w << "x" << h);
            CHECK(r.claimed == city); // a row visited twice (py == 0, k == 0) is not counted twice
            // The footprint itself is always inside the disc.
            for (std::uint32_t dy = 0; dy < h; ++dy) {
                for (std::uint32_t dx = 0; dx < w; ++dx) {
                    CHECK(t.owner(t.index_of(static_cast<std::int32_t>(17 + dx),
                                             static_cast<std::int32_t>(15 + dy))) == 1);
                }
            }
        }
    }
}

TEST_CASE("territory: clipping at the island's edges and a disc larger than the island") {
    // A 6 x 4 island with a huge radius: everything neutral is claimed, nothing outside written.
    sim::TileLayer t = make_layer(-2, 7, 6, 4);
    const tr::Result r = tr::claim_neutral_disc(t, kNeutral, 3, {0, 8, 2, 2}, 4000, kMaxRadius);
    REQUIRE(r.status == tr::Status::Ok);
    CHECK(r.claimed == 24);
    CHECK(t.ignored_writes() == 0);
    // Footprint in the corner: columns clamp at x0, rows above y0 are skipped.
    sim::TileLayer c = make_layer(10, 20, 30, 30);
    CHECK(tr::claim_neutral_disc(c, kNeutral, 3, {10, 20, 1, 1}, 5, kMaxRadius).status ==
          tr::Status::Ok);
    CHECK(c.owner(c.index_of(10, 20)) == 3);
    CHECK(c.owner(c.index_of(15, 20)) == 3);
    CHECK(c.owner(c.index_of(10, 25)) == 3);
    CHECK(c.owner(c.index_of(16, 20)) == kNeutral);
    CHECK(c.owner(c.index_of(10, 26)) == kNeutral);
    CHECK(c.ignored_writes() == 0);
}

TEST_CASE("territory: refusals change nothing") {
    sim::TileLayer t = make_layer(4, 4, 10, 10);
    const std::uint64_t digest0 = t.chunk_digest(0);
    const std::uint64_t hash0 = fnv_owners(t);
    const auto dirty0 = t.dirty_chunks();
    using S = tr::Status;
    CHECK(tr::claim_neutral_rect(t, kNeutral, 2, {3, 4, 2, 2}).status == S::BadFootprint); // west
    CHECK(tr::claim_neutral_rect(t, kNeutral, 2, {13, 4, 2, 2}).status ==
          S::BadFootprint); // east over
    CHECK(tr::claim_neutral_rect(t, kNeutral, 2, {4, 13, 1, 2}).status ==
          S::BadFootprint); // south over
    CHECK(tr::claim_neutral_rect(t, kNeutral, 2, {4, 4, 0, 2}).status == S::BadFootprint); // empty
    CHECK(tr::claim_neutral_rect(t, kNeutral, 2, {4, 4, 2, 0}).status == S::BadFootprint);
    CHECK(tr::claim_neutral_disc(t, kNeutral, 2, {4, 4, 11, 1}, 3, kMaxRadius).status ==
          S::BadFootprint);
    CHECK(tr::claim_neutral_rect(t, kNeutral, kNeutral, {4, 4, 2, 2}).status == S::InvalidOwner);
    CHECK(tr::claim_neutral_disc(t, kNeutral, kNeutral, {4, 4, 2, 2}, 3, kMaxRadius).status ==
          S::InvalidOwner);
    CHECK(tr::claim_neutral_disc(t, kNeutral, 2, {4, 4, 2, 2}, kMaxRadius + 1, kMaxRadius).status ==
          S::RadiusTooLarge);
    CHECK(tr::claim_neutral_disc(t, kNeutral, 2, {4, 4, 2, 2},
                                 std::numeric_limits<std::uint32_t>::max(), kMaxRadius)
              .status == S::RadiusTooLarge);
    // Extreme positions are refused without arithmetic on them.
    const std::int64_t lo = std::numeric_limits<std::int64_t>::min();
    const std::int64_t hi = std::numeric_limits<std::int64_t>::max();
    CHECK(tr::claim_neutral_rect(t, kNeutral, 2, {lo, 4, 1, 1}).status == S::BadFootprint);
    CHECK(tr::claim_neutral_rect(t, kNeutral, 2, {4, hi, 1, 1}).status == S::BadFootprint);
    CHECK(tr::claim_neutral_rect(t, kNeutral, 2, {hi, hi, 0xffffffffU, 0xffffffffU}).status ==
          S::BadFootprint);
    CHECK(fnv_owners(t) == hash0);
    CHECK(t.chunk_digest(0) == digest0);
    CHECK(t.dirty_chunks() == dirty0);
    CHECK(t.ignored_writes() == 0);
    // A radius at the bound is accepted (the bound is the caller's, not a constant here).
    CHECK(tr::claim_neutral_disc(t, kNeutral, 2, {4, 4, 2, 2}, kMaxRadius, kMaxRadius).status ==
          S::Ok);
}

TEST_CASE("territory: dirty chunks, cached digests and the layer digest follow the writes") {
    sim::TileLayer t = make_layer(0, 0, 40, 40);
    // Bring every chunk's cache up to date, then claim a small area.
    for (std::uint64_t c = 0; c < t.chunk_count(); ++c) {
        (void)t.chunk_digest(c);
    }
    REQUIRE(t.dirty_chunks() == 0);
    REQUIRE(t.cache_consistent());
    const tr::Result r = tr::claim_neutral_rect(t, kNeutral, 6, {1, 1, 3, 3});
    REQUIRE(r.claimed == 9);
    CHECK(t.dirty_chunks() == 1); // only the chunk holding (1..3, 1..3)
    CHECK(t.cache_consistent());
    for (std::uint64_t c = 0; c < t.chunk_count(); ++c) {
        CHECK(t.chunk_digest(c) == t.chunk_digest_uncached(c));
    }
    CHECK(t.dirty_chunks() == 0);
    // A claim of nothing writes nothing: no dirty chunk.
    CHECK(tr::claim_neutral_rect(t, kNeutral, 6, {1, 1, 3, 3}).claimed == 0);
    CHECK(t.dirty_chunks() == 0);
    // A disc crossing several chunks dirties them and the digests agree afterwards.
    CHECK(tr::claim_neutral_disc(t, kNeutral, 7, {18, 18, 4, 4}, 12, kMaxRadius).claimed > 100);
    CHECK(t.dirty_chunks() > 4);
    CHECK(t.cache_consistent());
    for (std::uint64_t c = 0; c < t.chunk_count(); ++c) {
        CHECK(t.chunk_digest(c) == t.chunk_digest_uncached(c));
    }
    // The same owners written one by one give the same chunk digests (the claim is only set_owner).
    sim::TileLayer u = make_layer(0, 0, 40, 40);
    for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
        u.set_owner(i, t.owner(i));
    }
    for (std::uint64_t c = 0; c < t.chunk_count(); ++c) {
        CHECK(u.chunk_digest(c) == t.chunk_digest(c));
    }
}

TEST_CASE("territory: a claim changes the world hash and survives save and load") {
    sim::ScenarioSpec spec = sim::synthetic::empty_scenario();
    spec.islands.push_back(sim::synthetic::plain_island(0, 40, 24, 30, 20));
    // The state refuses a tile owner that is not a city of the island: give the island city 2.
    sim::ScenarioSpec::PlayerSpec player;
    player.id = sim::PlayerId{0};
    spec.players.push_back(player);
    spec.islands.back().cities.push_back(sim::synthetic::plain_city(2, 0));
    sim::World w = island_test::make_world(spec);
    const std::uint64_t before = w.state_hash();
    sim::Island* is = w.island_at(45, 30);
    REQUIRE(is != nullptr);
    const tr::Result r =
        tr::claim_neutral_disc(is->tiles, w.config().neutral_owner, 2, {50, 34, 4, 4}, 6, 64);
    REQUIRE(r.status == tr::Status::Ok);
    REQUIRE(r.claimed > 0);
    const std::uint64_t after = w.state_hash();
    CHECK(after != before);
    CHECK(w.state_hash() == after); // reading the hash again changes nothing
    const std::vector<std::uint8_t> frame = w.save();
    const sim::World w2 = island_test::load_world(frame);
    CHECK(w2.state_hash() == after);
    CHECK(w2.save() == frame);
}
