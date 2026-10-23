// Stage S13 (docs/sim/state.md §5.4, §5.8, §15 S13): the growth sweep's chunk visit
// (grow_chunk, D AnnoWelt 0x1008b5f0) and the drought machine (drought_tick, D
// 0x1008aeb0). Every expected number is derived by hand from the spec (never from the
// code under test); the derivations are in the comments.
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/island_tick.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "slice_helpers.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using slice_test::island;
using slice_test::Tick;

namespace {

syn::SyntheticTypes growth_types() {
    auto t = syn::make_growth_types();
    REQUIRE_MESSAGE(t.ok(), (t.ok() ? std::string() : t.error()));
    return std::move(t).value();
}

sim::World make(const sim::ScenarioSpec& spec, const syn::SyntheticTypes& t, sim::Seams seams = {},
                sim::SimConfig cfg = {}, sim::Rules rules = {}) {
    auto r = sim::World::create(spec, t.registry, std::move(cfg), rules, std::move(seams));
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

/// One island of w x h tiles at (100, 100), every tile holding `object` (phase 0),
/// no player, no city.
sim::ScenarioSpec covered_island(std::uint32_t w, std::uint32_t h, sim::TypeId object) {
    sim::ScenarioSpec spec = syn::empty_scenario();
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 100, 100, w, h);
    for (std::uint32_t i = 0; i < w * h; ++i) {
        is.objects.push_back({i, object.v, 0});
    }
    spec.islands.push_back(std::move(is));
    return spec;
}

/// Every chunk digest of the island's tile layer, the cache brought up to date (so
/// dirty_chunks() counts the chunks written after this call).
std::vector<std::uint64_t> digests(sim::Island& is) {
    std::vector<std::uint64_t> out;
    for (std::uint64_t c = 0; c < is.tiles.chunk_count(); ++c) {
        out.push_back(is.tiles.chunk_digest(c));
    }
    return out;
}

/// The chunks whose digest differs between two snapshots.
std::vector<std::uint64_t> changed(const std::vector<std::uint64_t>& a,
                                   const std::vector<std::uint64_t>& b) {
    std::vector<std::uint64_t> out;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
        if (a[i] != b[i]) {
            out.push_back(i);
        }
    }
    return out;
}

std::uint8_t phase_at(const sim::Island& is, std::uint32_t lx, std::uint32_t ly) {
    return is.tiles.phase(std::uint64_t{ly} * is.tiles.width() + lx);
}

std::uint32_t object_at(const sim::Island& is, std::uint32_t lx, std::uint32_t ly) {
    return is.tiles.object(std::uint64_t{ly} * is.tiles.width() + lx);
}

// The grass (P 1 000, S 5, k 0) at t = 500: t / (P / 2) = 1, parity 1 against the stored
// 0, so a visit sets bit 6 and counts 1: phase 0x41 (1 < S << k = 5, no successor).
constexpr std::uint8_t kGrassOnce = 0x41;

} // namespace

// ------------------------------------------------------------------- growth

TEST_CASE("growth sweep visits chunks per sweep_chunks") {
    // 32 x 16 tiles = 4 x 2 chunks of grass. The island tick at t = 500 with the sweep
    // stamp at 500 - d visits min(8 d / 2000 + 1, 8) chunks from the cursor (0, 0), row
    // by row (A3): d = 250 -> 2 chunks (0 and 1), d = 1000 -> 5 (0 .. 4), d = 1749 -> 6 + 1
    // = 7, d = 2000 -> 8 + 1 clamped to 8 (all).
    const syn::SyntheticTypes t = growth_types();
    for (const auto& [d, n] : std::vector<std::pair<std::uint16_t, std::uint64_t>>{
             {250, 2}, {1000, 5}, {1749, 7}, {2000, 8}}) {
        INFO("d " << d);
        sim::World w = make(covered_island(32, 16, t.grass), t);
        sim::Island& is = island(w);
        is.sweep.stamp = static_cast<std::uint16_t>(500 - d);
        const std::vector<std::uint64_t> before = digests(is);
        Tick tick(w, 500);
        sim::Effects fx;
        sim::island_tick(tick.ctx, is, fx);
        std::vector<std::uint64_t> want;
        for (std::uint64_t c = 0; c < n; ++c) {
            want.push_back(c);
        }
        CHECK(changed(before, digests(is)) == want);
        // Chunk c is at (8 (c % 4), 8 (c / 4)); its corner tile was grown once.
        for (std::uint64_t c = 0; c < 8; ++c) {
            const auto lx = static_cast<std::uint32_t>(8 * (c % 4));
            const auto ly = static_cast<std::uint32_t>(8 * (c / 4));
            CHECK(phase_at(is, lx, ly) == (c < n ? kGrassOnce : 0));
        }
        // The cursor is at the chunk after the last one visited (wrapping), as S7 moved it.
        CHECK(is.sweep.grow_x == 8 * (n % 8 % 4));
        CHECK(is.sweep.grow_y == 8 * (n % 8 / 4));
    }
}

TEST_CASE("grow writes through the setters") {
    // One visited chunk of grass: its 64 tiles change phase through set_phase, which
    // marks exactly that chunk dirty; nothing is written outside the layer; the cache is
    // consistent after a refresh, and the digest of every other chunk is unchanged.
    const syn::SyntheticTypes t = growth_types();
    sim::World w = make(covered_island(32, 16, t.grass), t);
    sim::Island& is = island(w);
    const std::vector<std::uint64_t> before = digests(is);
    REQUIRE(is.tiles.dirty_chunks() == 0);
    Tick tick(w, 500);
    sim::grow_chunk(tick.ctx, is, 8, 8); // chunk 5
    CHECK(is.tiles.dirty_chunks() == 1);
    CHECK(is.tiles.ignored_writes() == 0);
    CHECK(changed(before, digests(is)) == std::vector<std::uint64_t>{5});
    CHECK(is.tiles.cache_consistent());
    for (std::uint32_t y = 8; y < 16; ++y) {
        for (std::uint32_t x = 8; x < 16; ++x) {
            CHECK(phase_at(is, x, y) == kGrassOnce);
            CHECK(object_at(is, x, y) == t.grass.v);
        }
    }
    // A visit at a tick of the same parity changes nothing and dirties nothing.
    const std::vector<std::uint64_t> after = digests(is);
    Tick same(w, 999); // 999 / 500 = 1: parity 1 = the stored bit
    sim::grow_chunk(same.ctx, is, 8, 8);
    CHECK(is.tiles.dirty_chunks() == 0);
    CHECK(digests(is) == after);
    // The successor is written through set_object: the grass needs S << k = 5 counts;
    // visits at t = 1000 (parity 0), 1500 (1), 2000 (0), 2500 (1) count 2 .. 5, and the
    // fifth count (5 % 1 == 0, 5 >= 5) makes it the next id with the phase cleared.
    for (const std::uint32_t at : {1000U, 1500U, 2000U, 2500U}) {
        Tick tk(w, at);
        sim::grow_chunk(tk.ctx, is, 8, 8);
    }
    CHECK(object_at(is, 8, 8) == t.ripe_grass.v);
    CHECK(phase_at(is, 8, 8) == 0);
    CHECK(object_at(is, 7, 8) == t.grass.v); // chunk 4, not visited
    CHECK(is.tiles.ignored_writes() == 0);
    // A ripe object does not grow (not a growing type): a later visit leaves it.
    Tick later(w, 3000);
    const std::vector<std::uint64_t> ripe = digests(is);
    sim::grow_chunk(later.ctx, is, 8, 8);
    CHECK(digests(is) == ripe);
}

TEST_CASE("grow skips tiles whose object is not a growing type") {
    // A tile with no object, an unknown id, a building type (the weaver) and a ripe
    // object: none of them is touched; the grass next to them grows.
    const syn::SyntheticTypes t = growth_types();
    sim::ScenarioSpec spec = covered_island(8, 8, t.grass);
    spec.islands[0].objects[0] = {0, 0, 0x55};
    spec.islands[0].objects[1] = {1, 4242, 0x55};
    spec.islands[0].objects[2] = {2, t.weaver.v, 0x55};
    spec.islands[0].objects[3] = {3, t.ripe_field.v, 0x55};
    sim::World w = make(spec, t);
    sim::Island& is = island(w);
    Tick tick(w, 500);
    sim::grow_chunk(tick.ctx, is, 0, 0);
    for (std::uint32_t x = 0; x < 4; ++x) {
        CHECK(phase_at(is, x, 0) == 0x55);
    }
    CHECK(object_at(is, 2, 0) == t.weaver.v);
    CHECK(phase_at(is, 4, 0) == kGrassOnce);
}

TEST_CASE("cursor wrap reading A3") {
    // 24 x 16 tiles = 3 x 2 chunks of grass, the growth cursor on the last chunk (16, 8).
    // d = 1000: min(6 * 1000 / 2000 + 1, 6) = 4 chunks: (16, 8), then the island wraps to
    // (0, 0), (8, 0), (16, 0); the cursor ends at (0, 8) after the row wrap of (16, 0).
    const syn::SyntheticTypes t = growth_types();
    sim::World w = make(covered_island(24, 16, t.grass), t);
    sim::Island& is = island(w);
    is.sweep.grow_x = 16;
    is.sweep.grow_y = 8;
    is.sweep.stamp = 1500;
    const std::vector<std::uint64_t> before = digests(is);
    // Chunk order: 0 (0,0), 1 (8,0), 2 (16,0), 3 (0,8), 4 (8,8), 5 (16,8). At t = 2500
    // the grass's parity is 2500 / 500 = 5 -> 1, so every visited chunk changes.
    Tick tick(w, 2500);
    sim::Effects fx;
    sim::island_tick(tick.ctx, is, fx);
    CHECK(changed(before, digests(is)) == std::vector<std::uint64_t>{0, 1, 2, 5});
    CHECK(is.sweep.grow_x == 0);
    CHECK(is.sweep.grow_y == 8);
}

TEST_CASE("edge chunks are clipped") {
    // A14 / F20: 30 x 20 tiles of grass = 4 x 3 chunks, the last column of chunks 6 tiles
    // wide and the last row 4 tiles high. The original's 8 x 8 block with index
    // y * width + x would reach (30, 0) = tile 30 = (0, 1) and (31, 0) = (1, 1) of chunk 0
    // from chunk (24, 0), and rows 20 .. 23 (beyond the 600 tiles) from chunk (24, 16).
    const syn::SyntheticTypes t = growth_types();
    sim::World w = make(covered_island(30, 20, t.grass), t);
    sim::Island& is = island(w);
    const std::vector<std::uint64_t> before = digests(is);
    Tick tick(w, 500);
    sim::grow_chunk(tick.ctx, is, 24, 0);
    sim::grow_chunk(tick.ctx, is, 24, 16);
    // Chunks 3 (24, 0) and 11 (24, 16) only.
    CHECK(changed(before, digests(is)) == std::vector<std::uint64_t>{3, 11});
    CHECK(is.tiles.ignored_writes() == 0);
    // The in-bounds tiles of both edge chunks grew once ...
    for (std::uint32_t y = 0; y < 8; ++y) {
        for (std::uint32_t x = 24; x < 30; ++x) {
            CHECK(phase_at(is, x, y) == kGrassOnce);
        }
    }
    for (std::uint32_t y = 16; y < 20; ++y) {
        for (std::uint32_t x = 24; x < 30; ++x) {
            CHECK(phase_at(is, x, y) == kGrassOnce);
        }
    }
    // ... and the tiles an unclipped block would alias did not.
    CHECK(phase_at(is, 0, 1) == 0);
    CHECK(phase_at(is, 1, 1) == 0);
    CHECK(phase_at(is, 0, 17) == 0);
    CHECK(phase_at(is, 23, 0) == 0);
}

TEST_CASE("growth tiles advance through World::advance") {
    // The exit condition: advance -> island_tick -> grow_chunk -> grow. One chunk of grass
    // (8 x 8): every advance of 1 000 ms (500 ticks) visits min(1 * 500 / 2000 + 1, 1) = 1
    // chunk. Visits at t = 500, 1000, 1500, 2000, 2500 alternate the parity 1, 0, 1, 0,
    // 1 against the stored bit: counts 1 .. 5, the fifth makes the grass ripe at t = 2500.
    const syn::SyntheticTypes t = growth_types();
    sim::World w = make(covered_island(8, 8, t.grass), t);
    for (int i = 1; i <= 4; ++i) {
        w.advance(1000);
        CHECK(phase_at(island(w), 3, 3) == (0x40 * (i % 2) + i));
    }
    w.advance(1000);
    CHECK(object_at(island(w), 3, 3) == t.ripe_grass.v);
    CHECK(phase_at(island(w), 3, 3) == 0);
    CHECK(w.last_output().draws.empty()); // the growth visit draws nothing (O-13)
    CHECK(w.check_invariants() == "");
}
