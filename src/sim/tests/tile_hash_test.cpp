// The tile layer and its chunk digests (docs/sim/state.md §4.8, §9.2, §11,
// stage S5): dimensions and reset, the dirty marks of the setters, the digest
// of a chunk (field order, clipped edge chunks), the cache assertion, write-order
// and executor independence, and the byte layout of the tiles scope.
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/executor.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>

#include "island_helpers.hpp"

namespace sim = anno::sim;
using island_test::Stream;
using island_test::ThreadExecutor;

namespace {

/// The digest of chunk (kx, ky) transcribed from the spec: FNV-1a 64 over (object
/// u32, phase u8, owner u32, aux u32) of the chunk's in-bounds tiles, row-major.
std::uint64_t expected_chunk(const sim::TileLayer& t, std::uint32_t kx, std::uint32_t ky) {
    sim::StateHasher h;
    const std::uint32_t edge = t.chunk_edge();
    for (std::uint32_t y = ky * edge; y < (ky + 1) * edge && y < t.height(); ++y) {
        for (std::uint32_t x = kx * edge; x < (kx + 1) * edge && x < t.width(); ++x) {
            const std::uint64_t i = std::uint64_t{y} * t.width() + x;
            h.u32(t.object(i));
            h.u8(t.phase(i));
            h.u32(t.owner(i));
            h.u32(t.aux(i));
        }
    }
    return h.value();
}

/// Distinct values in every column of every tile.
void fill_pattern(sim::TileLayer& t, std::uint32_t seed) {
    std::uint32_t x = seed * 2654435761U + 1U;
    for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
        x = x * 1664525U + 1013904223U;
        t.set_object(i, x >> 8);
        t.set_phase(i, static_cast<std::uint8_t>(x >> 3));
        t.set_owner(i, (x >> 5) % 17U);
        t.set_aux(i, x ^ 0x5a5a5a5aU);
    }
}

/// Through the non-const overload: the digests are cached as they are read.
std::vector<std::uint64_t> all_digests(sim::TileLayer& t) {
    std::vector<std::uint64_t> out;
    for (std::uint64_t k = 0; k < t.chunk_count(); ++k) {
        out.push_back(t.chunk_digest(k));
    }
    return out;
}

} // namespace

TEST_CASE("tile layer default constructed and reset") {
    const sim::TileLayer empty;
    CHECK(empty.width() == 0);
    CHECK(empty.height() == 0);
    CHECK(empty.chunk_edge() == 0);
    CHECK(empty.chunk_count() == 0);
    CHECK(empty.tile_count() == 0);
    CHECK(empty.object(0) == 0);
    CHECK(empty.chunk_digest(0) == 0);
    // chunk_of is total: an empty layer has no valid index, and the call neither
    // divides by its zero width nor indexes anything (UBSan would see either).
    CHECK(empty.chunk_of(0) == 0);
    CHECK(empty.chunk_of(1) == 0);
    CHECK(empty.chunk_of(0xFFFFFFFFFFFFFFFFULL) == 0);
    sim::TileLayer zero_wide(0, 0, 0, 5, 8);
    CHECK(zero_wide.chunk_of(0) == 0);
    zero_wide.set_object(0, 1); // the setters check the size first
    CHECK(zero_wide.ignored_writes() == 1);
    CHECK_FALSE(empty.contains(0, 0));
    CHECK(empty.index_of(0, 0) == sim::kNoSlot);

    sim::TileLayer t;
    REQUIRE(t.reset(-4, 7, 20, 13, 8));
    CHECK(t.x0() == -4);
    CHECK(t.y0() == 7);
    CHECK(t.width() == 20);
    CHECK(t.height() == 13);
    CHECK(t.chunk_edge() == 8);
    CHECK(t.tile_count() == 260);
    // ceil(20 / 8) = 3 by ceil(13 / 8) = 2 chunks.
    CHECK(t.chunks_x() == 3);
    CHECK(t.chunks_y() == 2);
    CHECK(t.chunk_count() == 6);
    for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
        REQUIRE(t.object(i) == 0);
        REQUIRE(t.phase(i) == 0);
        REQUIRE(t.owner(i) == 0);
        REQUIRE(t.aux(i) == 0);
    }
    CHECK(t.dirty_chunks() == 6);

    // The constructor is reset().
    const sim::TileLayer built(-4, 7, 20, 13, 8);
    CHECK(built.chunk_count() == 6);
    CHECK(built.x0() == -4);

    // A multiple of the edge has no clipped chunk; one more tile adds a chunk.
    REQUIRE(t.reset(0, 0, 16, 16, 8));
    CHECK(t.chunks_x() == 2);
    CHECK(t.chunks_y() == 2);
    REQUIRE(t.reset(0, 0, 17, 1, 8));
    CHECK(t.chunks_x() == 3);
    CHECK(t.chunks_y() == 1);
    // ceil(w / edge) is formed in u64: w + edge - 1 wraps in u32.
    REQUIRE(t.reset(0, 0, 2, 2, 0xFFFFFFFFU));
    CHECK(t.chunks_x() == 1);
    CHECK(t.chunks_y() == 1);
    // A width or height of 0 is a valid, empty layer without chunks.
    REQUIRE(t.reset(5, 5, 0, 9, 8));
    CHECK(t.tile_count() == 0);
    CHECK(t.chunk_count() == 0);
    REQUIRE(t.reset(5, 5, 9, 0, 8));
    CHECK(t.chunk_count() == 0);

    // Failures leave the layer unchanged.
    REQUIRE(t.reset(3, 4, 20, 13, 8));
    t.set_object(7, 99);
    const std::uint64_t before = t.chunk_digest(0);
    CHECK_FALSE(t.reset(0, 0, 100, 100, 0));
    CHECK(t.width() == 20);
    CHECK(t.x0() == 3);
    CHECK_FALSE(t.reset(0, 0, 0xFFFFFFFFU, 0xFFFFFFFFU, 8));
    CHECK(t.height() == 13);
    // The tile bound: exactly kIslandTileBound tiles are accepted by the check,
    // one more is not (shaped as 1 x n so the test allocates little: the bound is
    // on the product, and 1 x (bound + 1) is refused before any allocation).
    CHECK_FALSE(t.reset(0, 0, 1, static_cast<std::uint32_t>(sim::kIslandTileBound + 1), 8));
    CHECK_FALSE(t.reset(0, 0, 8192, 8193, 8));
    CHECK(t.object(7) == 99);
    CHECK(t.chunk_digest(0) == before);
    // A reset zero-fills.
    REQUIRE(t.reset(1, 2, 5, 5, 8));
    CHECK(t.object(7) == 0);
    CHECK(t.chunks_x() == 1);
}

TEST_CASE("the island tile bound is accepted exactly and refused one tile above") {
    // Every site that compares w * h with the bound calls island_tiles_within_bound
    // (TileLayer::reset, the scenario check, validate_islands and the stream
    // reader), so the edge is tested once, here, without allocating the 870 MB that
    // a layer of exactly the bound would take. The sites are tested with shapes that
    // cost nothing: 1 x bound is refused for another reason by the scenario (outside
    // the world, before anything is built) and by the stream reader (no bytes).
    static_assert(sim::island_tiles_within_bound(1, 1U << 26));
    static_assert(!sim::island_tiles_within_bound(1, (1U << 26) + 1));
    static_assert(sim::kIslandTileBound == (1ULL << 26));
    CHECK(sim::island_tiles_within_bound(8192, 8192));
    CHECK_FALSE(sim::island_tiles_within_bound(8192, 8193));
    CHECK_FALSE(sim::island_tiles_within_bound(8193, 8192));
    CHECK(sim::island_tiles_within_bound(1, 1U << 26));
    CHECK(sim::island_tiles_within_bound(2, 1U << 25));
    CHECK_FALSE(sim::island_tiles_within_bound(2, (1U << 25) + 1));
    CHECK(sim::island_tiles_within_bound(1U << 26, 1));
    CHECK_FALSE(sim::island_tiles_within_bound((1U << 26) + 1, 1));
    CHECK(sim::island_tiles_within_bound(0, 0xFFFFFFFFU));
    CHECK(sim::island_tiles_within_bound(0xFFFFFFFFU, 0));
    CHECK_FALSE(sim::island_tiles_within_bound(0xFFFFFFFFU, 0xFFFFFFFFU));
    // The refusal leaves a layer alone and counts nothing.
    sim::TileLayer t(0, 0, 4, 4, 8);
    CHECK_FALSE(t.reset(0, 0, 8192, 8193, 8));
    CHECK(t.width() == 4);
    CHECK(t.ignored_writes() == 0);
}

TEST_CASE("tile layer indexing and containment") {
    sim::TileLayer t(100, -20, 6, 4, 8);
    CHECK(t.contains(100, -20));
    CHECK(t.contains(105, -17));
    CHECK_FALSE(t.contains(106, -17));
    CHECK_FALSE(t.contains(105, -16));
    CHECK_FALSE(t.contains(99, -20));
    CHECK_FALSE(t.contains(100, -21));
    // Row-major from the top-left.
    CHECK(t.index_of(100, -20) == 0);
    CHECK(t.index_of(105, -20) == 5);
    CHECK(t.index_of(100, -19) == 6);
    CHECK(t.index_of(105, -17) == 23);
    CHECK(t.index_of(106, -17) == sim::kNoSlot);
    CHECK(t.index_of(100, -16) == sim::kNoSlot);
    // Coordinates near the ends of i32 do not overflow.
    sim::TileLayer far(2147483640, 0, 7, 1, 8);
    CHECK(far.contains(2147483646, 0));
    CHECK_FALSE(far.contains(-2147483647 - 1, 0));
    CHECK(far.index_of(2147483646, 0) == 6);
}

TEST_CASE("tile setters mark chunks dirty") {
    sim::TileLayer t(0, 0, 20, 13, 8);
    // chunk_of: row-major chunk order, 3 chunks per row.
    CHECK(t.chunk_of(0) == 0);
    CHECK(t.chunk_of(8) == 1);
    CHECK(t.chunk_of(19) == 2);
    CHECK(t.chunk_of(8 * 20) == 3);
    CHECK(t.chunk_of(9 * 20 + 9) == 4);
    CHECK(t.chunk_of(12 * 20 + 19) == 5);

    using Setter = void (*)(sim::TileLayer&, std::uint64_t);
    const std::vector<std::pair<const char*, Setter>> setters = {
        {"object", [](sim::TileLayer& l, std::uint64_t i) { l.set_object(i, 123); }},
        {"phase", [](sim::TileLayer& l, std::uint64_t i) { l.set_phase(i, 7); }},
        {"owner", [](sim::TileLayer& l, std::uint64_t i) { l.set_owner(i, 70000); }},
        {"aux", [](sim::TileLayer& l, std::uint64_t i) { l.set_aux(i, 0x80); }},
    };
    for (const auto& [name, set] : setters) {
        for (const std::uint64_t idx : {std::uint64_t{0}, std::uint64_t{9 * 20 + 9},
                                        std::uint64_t{12 * 20 + 19}, std::uint64_t{8 * 20 + 3}}) {
            INFO("setter " << name << " tile " << idx);
            t.refresh(sim::serial_executor());
            REQUIRE(t.dirty_chunks() == 0);
            const std::vector<std::uint64_t> before = all_digests(t);
            REQUIRE(t.dirty_chunks() == 0);
            set(t, idx);
            // Exactly the chunk of the tile is dirty, and only its digest moves.
            CHECK(t.dirty_chunks() == 1);
            const std::vector<std::uint64_t> after = all_digests(t);
            CHECK(t.dirty_chunks() == 0);
            for (std::uint64_t k = 0; k < t.chunk_count(); ++k) {
                CHECK((after[k] != before[k]) == (k == t.chunk_of(idx)));
            }
            CHECK(t.cache_consistent());
            // Undo, so the next row starts from the same state.
            t.reset(0, 0, 20, 13, 8);
        }
    }
    // A write beyond the layer is ignored and dirties nothing.
    t.refresh(sim::serial_executor());
    t.set_object(260, 5);
    t.set_phase(1ULL << 40, 5);
    t.set_owner(0xFFFFFFFFFFFFFFFFULL, 5);
    t.set_aux(260, 5);
    CHECK(t.dirty_chunks() == 0);
    CHECK(t.object(260) == 0);
    // ... and counted, so that a stray index of a later stage shows in a test.
    CHECK(t.ignored_writes() == 4);
    t.set_object(259, 1); // the last tile is a valid index
    CHECK(t.ignored_writes() == 4);
    t.set_owner(260, 1);
    CHECK(t.ignored_writes() == 5);
    // The counter is not state: a reset restarts it, and the digests do not see it.
    sim::TileLayer fresh(0, 0, 20, 13, 8);
    CHECK(fresh.ignored_writes() == 0);
    CHECK(t.reset(0, 0, 20, 13, 8));
    CHECK(t.ignored_writes() == 0);
    CHECK(all_digests(t) == all_digests(fresh));
}

TEST_CASE("chunk_of is total") {
    sim::TileLayer t(0, 0, 20, 13, 8);
    CHECK(t.chunk_count() == 6);
    // One beyond the last chunk for every index beyond the layer; the last valid
    // index is in the last chunk.
    CHECK(t.chunk_of(259) == 5);
    CHECK(t.chunk_of(260) == 6);
    CHECK(t.chunk_of(0xFFFFFFFFFFFFFFFFULL) == 6);
}

TEST_CASE("a const layer is hashed without writing its cache") {
    sim::TileLayer t(0, 0, 20, 13, 8);
    fill_pattern(t, 3);
    const sim::TileLayer& c = t;
    REQUIRE(t.dirty_chunks() == 6);
    // The const overload computes a dirty chunk and stores nothing.
    for (std::uint64_t k = 0; k < c.chunk_count(); ++k) {
        CHECK(c.chunk_digest(k) == c.chunk_digest_uncached(k));
    }
    CHECK(t.dirty_chunks() == 6);
    CHECK(c.chunk_digest(99) == 0);
    // The non-const overload fills the entry (one chunk at a time) ...
    const std::uint64_t d1 = t.chunk_digest(1);
    CHECK(d1 == c.chunk_digest_uncached(1));
    CHECK(t.dirty_chunks() == 5);
    CHECK(t.chunk_digest(99) == 0);
    // ... and the const overload then returns the cached value of a clean chunk.
    CHECK(c.chunk_digest(1) == d1);
    CHECK(c.cache_consistent());
}

TEST_CASE("several threads hash one const world at the same time") {
    const sim::World w = island_test::make_world(sim::synthetic::two_island_scenario());
    const std::uint64_t want = sim::World(w).state_hash();
    std::vector<std::uint64_t> got(4, 0);
    std::vector<std::thread> pool;
    for (std::size_t i = 0; i < got.size(); ++i) {
        pool.emplace_back([&w, &got, i] {
            for (int round = 0; round < 3; ++round) {
                got[i] = w.state_hash();
            }
        });
    }
    for (std::thread& th : pool) {
        th.join();
    }
    for (const std::uint64_t g : got) {
        CHECK(g == want);
    }
    // Hashing a const world left every cache as it was (all dirty): no thread wrote.
    for (const std::optional<sim::Island>& is : w.islands()) {
        if (is.has_value()) {
            CHECK(is->tiles.dirty_chunks() == is->tiles.chunk_count());
        }
    }
}

TEST_CASE("setters write the columns they name") {
    sim::TileLayer t(0, 0, 5, 3, 8);
    t.set_object(7, 0x01020304);
    t.set_phase(7, 0x55);
    t.set_owner(7, 0x0A0B0C0D);
    t.set_aux(7, 0xF0E0D0C0);
    CHECK(t.object(7) == 0x01020304);
    CHECK(t.phase(7) == 0x55);
    CHECK(t.owner(7) == 0x0A0B0C0D);
    CHECK(t.aux(7) == 0xF0E0D0C0);
    // The neighbours and the other columns are untouched; reads beyond the layer
    // are 0.
    CHECK(t.object(6) == 0);
    CHECK(t.object(8) == 0);
    CHECK(t.phase(6) == 0);
    CHECK(t.owner(8) == 0);
    CHECK(t.aux(6) == 0);
    CHECK(t.object(15) == 0);
    CHECK(t.phase(15) == 0);
    CHECK(t.owner(15) == 0);
    CHECK(t.aux(15) == 0);
}

TEST_CASE("chunk digest is the FNV of the in-bounds tiles") {
    sim::TileLayer t(0, 0, 20, 13, 8);
    fill_pattern(t, 1);
    for (std::uint32_t ky = 0; ky < 2; ++ky) {
        for (std::uint32_t kx = 0; kx < 3; ++kx) {
            INFO("chunk " << kx << "," << ky);
            const std::uint64_t k = std::uint64_t{ky} * 3 + kx;
            CHECK(t.chunk_digest(k) == expected_chunk(t, kx, ky));
            CHECK(t.chunk_digest_uncached(k) == expected_chunk(t, kx, ky));
        }
    }
    // The digest of an empty chunk is the FNV offset basis; a chunk beyond the
    // layer is 0.
    sim::TileLayer blank(0, 0, 8, 8, 8);
    CHECK(blank.chunk_digest(0) == expected_chunk(blank, 0, 0));
    CHECK(blank.chunk_digest(1) == 0);
    CHECK(blank.chunk_digest_uncached(1) == 0);
    // Each of the four columns, and nothing else, enters the digest.
    const std::uint64_t base = blank.chunk_digest(0);
    sim::TileLayer a = blank;
    a.set_object(10, 1);
    sim::TileLayer b = blank;
    b.set_phase(10, 1);
    sim::TileLayer c = blank;
    c.set_owner(10, 1);
    sim::TileLayer d = blank;
    d.set_aux(10, 1);
    const std::uint64_t da = a.chunk_digest(0), db = b.chunk_digest(0), dc = c.chunk_digest(0),
                        dd = d.chunk_digest(0);
    for (const std::uint64_t v : {da, db, dc, dd}) {
        CHECK(v != base);
    }
    // The four columns are not interchangeable (a swapped pair would hash equal).
    CHECK(da != dc);
    CHECK(da != dd);
    CHECK(dc != dd);
    CHECK(db != da);
    // The same value at another tile of the chunk differs (position matters).
    sim::TileLayer e = blank;
    e.set_object(11, 1);
    CHECK(e.chunk_digest(0) != da);
    // Values outside the chunk do not enter it.
    sim::TileLayer f(0, 0, 20, 13, 8);
    const std::uint64_t chunk0 = f.chunk_digest(0);
    f.set_object(9 * 20 + 15, 5);
    CHECK(f.chunk_digest(0) == chunk0);
    CHECK(f.chunk_digest(4) != 0);
}

TEST_CASE("edge chunks of a non-multiple-of-8 island are clipped") {
    // 10 x 3: two chunks; the second holds x = 8..9 of the three rows (6 tiles).
    sim::TileLayer t(0, 0, 10, 3, 8);
    fill_pattern(t, 4);
    REQUIRE(t.chunk_count() == 2);
    sim::StateHasher want;
    for (std::uint32_t y = 0; y < 3; ++y) {
        for (std::uint32_t x = 8; x < 10; ++x) {
            const std::uint64_t i = y * 10ULL + x;
            want.u32(t.object(i));
            want.u8(t.phase(i));
            want.u32(t.owner(i));
            want.u32(t.aux(i));
        }
    }
    CHECK(t.chunk_digest(1) == want.value());
    // The first chunk holds x = 0..7 of the three rows only, not the tiles of the
    // next chunk that an unclipped loop would reach by aliasing into the next row.
    sim::StateHasher first;
    for (std::uint32_t y = 0; y < 3; ++y) {
        for (std::uint32_t x = 0; x < 8; ++x) {
            const std::uint64_t i = y * 10ULL + x;
            first.u32(t.object(i));
            first.u8(t.phase(i));
            first.u32(t.owner(i));
            first.u32(t.aux(i));
        }
    }
    CHECK(t.chunk_digest(0) == first.value());

    // 9 x 9: four chunks, the last holds the single tile (8, 8).
    sim::TileLayer u(0, 0, 9, 9, 8);
    fill_pattern(u, 5);
    REQUIRE(u.chunk_count() == 4);
    sim::StateHasher corner;
    corner.u32(u.object(80));
    corner.u8(u.phase(80));
    corner.u32(u.owner(80));
    corner.u32(u.aux(80));
    CHECK(u.chunk_digest(3) == corner.value());
    // A write to the last column of the last row dirties only the corner chunk.
    u.refresh(sim::serial_executor());
    u.set_aux(80, 0);
    CHECK(u.dirty_chunks() == 1);
    CHECK(u.chunk_digest(3) != corner.value());
    // A write to (8, 0) belongs to chunk 1, one to (0, 8) to chunk 2.
    CHECK(u.chunk_of(8) == 1);
    CHECK(u.chunk_of(72) == 2);
}

TEST_CASE("chunk digest cached equals uncached") {
    sim::TileLayer t(10, 20, 37, 29, 8);
    std::uint32_t x = 12345;
    for (int round = 0; round < 40; ++round) {
        for (int w = 0; w < 25; ++w) {
            x = x * 1664525U + 1013904223U;
            const std::uint64_t idx = (x >> 4) % t.tile_count();
            switch ((x >> 2) & 3U) {
            case 0:
                t.set_object(idx, x);
                break;
            case 1:
                t.set_phase(idx, static_cast<std::uint8_t>(x));
                break;
            case 2:
                t.set_owner(idx, x >> 9);
                break;
            default:
                t.set_aux(idx, x ^ 0xFFU);
                break;
            }
        }
        // Read some chunks (filling the cache), then write again.
        for (std::uint64_t k = 0; k < t.chunk_count(); k += 1 + (x % 3U)) {
            REQUIRE(t.chunk_digest(k) == t.chunk_digest_uncached(k));
        }
        REQUIRE(t.cache_consistent());
    }
}

TEST_CASE("cache assertion finds a write that skipped the dirty mark") {
    sim::TileLayer t(0, 0, 9, 9, 8);
    fill_pattern(t, 2);
    for (std::uint64_t k = 0; k < t.chunk_count(); ++k) {
        static_cast<void>(t.chunk_digest(k));
    }
    REQUIRE(t.dirty_chunks() == 0);
    REQUIRE(t.cache_consistent());
    // Columns in order: object (81), phase (81), owner (81), aux (81). Corrupt one
    // value of each column; the cached digest of its chunk is now stale.
    for (const std::uint64_t at : {std::uint64_t{0}, std::uint64_t{81 + 80},
                                   std::uint64_t{162 + 40}, std::uint64_t{243 + 80}}) {
        INFO("value " << at);
        sim::TileLayer copy = t;
        island_test::CorruptingArchive a;
        a.at = at;
        sim::TileIO::columns(a, copy);
        CHECK_FALSE(copy.cache_consistent());
        // Refreshing does not repair a clean (cached) chunk: only a write through a
        // setter does, which is the rule.
        CHECK(copy.dirty_chunks() == 0);
    }
}

TEST_CASE("tile hash independent of write order") {
    sim::TileLayer a(0, 0, 21, 17, 8);
    sim::TileLayer b(0, 0, 21, 17, 8);
    struct Write {
        std::uint64_t idx;
        std::uint32_t value;
        std::uint32_t column;
    };
    std::vector<Write> writes;
    std::uint32_t x = 99;
    for (int i = 0; i < 400; ++i) {
        x = x * 1664525U + 1013904223U;
        // Distinct (tile, column) pairs, so the order cannot matter.
        writes.push_back({static_cast<std::uint64_t>(i) % 357, x,
                          static_cast<std::uint32_t>(i / 357 * 2 + i % 4) % 4});
    }
    // Make the pairs unique: later duplicates are dropped.
    std::vector<Write> unique;
    for (const Write& w : writes) {
        const bool dup = std::any_of(unique.begin(), unique.end(), [&](const Write& u) {
            return u.idx == w.idx && u.column == w.column;
        });
        if (!dup) {
            unique.push_back(w);
        }
    }
    auto apply = [](sim::TileLayer& t, const Write& w) {
        switch (w.column) {
        case 0:
            t.set_object(w.idx, w.value);
            break;
        case 1:
            t.set_phase(w.idx, static_cast<std::uint8_t>(w.value));
            break;
        case 2:
            t.set_owner(w.idx, w.value);
            break;
        default:
            t.set_aux(w.idx, w.value);
            break;
        }
    };
    for (const Write& w : unique) {
        apply(a, w);
    }
    for (auto it = unique.rbegin(); it != unique.rend(); ++it) {
        apply(b, *it);
        // Interleave reads, so the cache is filled at different moments.
        static_cast<void>(b.chunk_digest(static_cast<std::uint64_t>(it - unique.rbegin()) % 9));
    }
    CHECK(all_digests(a) == all_digests(b));
    sim::Island ia, ib;
    ia.tiles = a;
    ib.tiles = b;
    CHECK(island_test::hash_of(ia) == island_test::hash_of(ib));
    // And a different content differs.
    ib.tiles.set_aux(0, 0xDEADBEEF);
    CHECK(island_test::hash_of(ia) != island_test::hash_of(ib));
}

TEST_CASE("executors visit every job once and serial order is ascending") {
    std::vector<std::uint64_t> order;
    sim::parallel_for(sim::serial_executor(), 6, [&](std::uint64_t i) { order.push_back(i); });
    CHECK(order == std::vector<std::uint64_t>{0, 1, 2, 3, 4, 5});
    order.clear();
    sim::parallel_for(sim::serial_executor(), 0, [&](std::uint64_t i) { order.push_back(i); });
    CHECK(order.empty());
    for (const unsigned threads : {1U, 2U, 8U}) {
        ThreadExecutor ex(threads);
        std::vector<std::atomic<int>> seen(1000);
        sim::parallel_for(ex, 1000, [&](std::uint64_t i) { seen[i].fetch_add(1); });
        for (std::size_t i = 0; i < seen.size(); ++i) {
            REQUIRE(seen[i].load() == 1);
        }
        CHECK(ex.jobs() == 1000);
        CHECK(ex.calls() == 1);
    }
}

TEST_CASE("island hash parallel equals serial") {
    // An island of 200 x 150 tiles: 25 x 19 = 475 chunks.
    sim::ScenarioSpec spec = sim::synthetic::empty_scenario();
    sim::ScenarioSpec::IslandSpec is = sim::synthetic::plain_island(0, 10, 10, 200, 150);
    is.owner_runs = {{3, 5000}};
    for (std::uint32_t i = 0; i < 30000; i += 7) {
        is.objects.push_back({i, 1000 + i % 50, static_cast<std::uint8_t>(i)});
    }
    spec.islands.push_back(is);
    const sim::World base = island_test::make_world(spec);
    // The reference hash comes from a copy: hashing fills the digest cache of the
    // world it runs on, and `base` must stay unhashed (every chunk dirty).
    const std::uint64_t want = sim::World(base).state_hash();
    // The serial executor.
    {
        sim::World w = base;
        w.prepare_hash(sim::serial_executor());
        CHECK(w.state_hash() == want);
        CHECK(w.islands()[0]->tiles.dirty_chunks() == 0);
    }
    for (const unsigned threads : {1U, 2U, 8U}) {
        INFO("threads " << threads);
        sim::World w = base;
        REQUIRE(w.islands()[0]->tiles.dirty_chunks() == 475);
        ThreadExecutor ex(threads);
        w.prepare_hash(ex);
        // One job per chunk, on the executor that was given, and the caches are
        // filled by it (a refresh that ignored the executor would leave this at 0).
        CHECK(ex.jobs() == 475);
        CHECK(w.islands()[0]->tiles.dirty_chunks() == 0);
        CHECK(w.islands()[0]->tiles.cache_consistent());
        CHECK(w.state_hash() == want);
        // A write after the refresh is seen by the next hash.
        w.prepare_hash(ex);
        sim::WorldTestAccess::islands(w)[0]->tiles.set_aux(30000 - 1, 4);
        CHECK(w.state_hash() != want);
    }
    // Several islands, each refreshed.
    const sim::World two = island_test::make_world(sim::synthetic::two_island_scenario());
    sim::World w2 = two;
    ThreadExecutor ex(4);
    w2.prepare_hash(ex);
    CHECK(ex.calls() == 2);
    CHECK(w2.state_hash() == two.state_hash());
}

TEST_CASE("tiles scope byte layout and raw hash") {
    // A 3 x 2 layer at (5, 6): one chunk of edge 8. Columns object[], phase[],
    // owner[], aux[], 13 bytes per tile.
    sim::Island is;
    is.index = sim::IslandId{2};
    REQUIRE(is.tiles.reset(5, 6, 3, 2, 8));
    for (std::uint32_t i = 0; i < 6; ++i) {
        is.tiles.set_object(i, 0x11000000U + i);
        is.tiles.set_phase(i, static_cast<std::uint8_t>(0x40 + i));
        is.tiles.set_owner(i, 0x22000000U + i);
        is.tiles.set_aux(i, 0x33000000U + i);
    }
    is.climate = 4;
    is.next_serial = 9;
    is.sweep = {0x1111, 1, 2, 3, 4};
    is.event300_stamp = 0x2222;
    is.objmgr = {0x3333, 6};
    is.ring = {0x4444, 5};
    is.reserved_object_count = 0x0102030405060708ULL;

    Stream want;
    want.u32(2);
    want.i32(5);
    want.i32(6);
    want.u32(3);
    want.u32(2);
    want.u32(4);
    want.u32(9);
    want.u64(0); // no terrain
    want.u16(0x1111);
    want.u32(1);
    want.u32(2);
    want.u32(3);
    want.u32(4);
    want.u16(0x2222);
    want.u16(0x3333);
    want.u32(6);
    want.u16(0x4444);
    want.u32(5);
    want.u8(0);  // no drought
    want.u64(0); // no deposits
    want.u64(0x0102030405060708ULL);
    const std::size_t before_tiles = want.v.size();
    want.u32(3);
    want.u32(2);
    want.u32(8);
    for (std::uint32_t i = 0; i < 6; ++i) {
        want.u32(0x11000000U + i);
    }
    for (std::uint32_t i = 0; i < 6; ++i) {
        want.u8(0x40 + i);
    }
    for (std::uint32_t i = 0; i < 6; ++i) {
        want.u32(0x22000000U + i);
    }
    for (std::uint32_t i = 0; i < 6; ++i) {
        want.u32(0x33000000U + i);
    }
    CHECK(want.v.size() == before_tiles + 12 + 6 * sim::kTileBytes);
    sim::WriteArchive wa;
    fields(wa, std::as_const(is));
    CHECK(wa.data() == want.v);

    // The trace sees the same bytes, and the raw hash is the FNV of them.
    sim::TraceArchive trace;
    fields(trace, std::as_const(is));
    CHECK(trace.bytes_written() == want.v);
    sim::StateHasher fnv;
    fnv.bytes(want.v);
    sim::HashArchive raw(sim::HashOptions{nullptr, 0, true});
    fields(raw, std::as_const(is));
    CHECK(raw.finish() == fnv.value());

    // The hierarchical hash feeds the chunk digest in place of the columns: the
    // fields up to the tiles scope, then the u64 digest of the scope (dimensions
    // and the digest of every chunk).
    sim::StateHasher tiles;
    tiles.u32(3);
    tiles.u32(2);
    tiles.u32(8);
    tiles.u64(is.tiles.chunk_digest(0));
    sim::StateHasher island;
    island.bytes(std::span<const std::uint8_t>(want.v.data(), before_tiles));
    island.u64(tiles.value());
    CHECK(island_test::hash_of(is) == island.value());
}

TEST_CASE("tiles scope reads back and refuses what it cannot size") {
    sim::Island is;
    is.index = sim::IslandId{0};
    REQUIRE(is.tiles.reset(0, 0, 5, 4, 8));
    for (std::uint32_t i = 0; i < 20; ++i) {
        is.tiles.set_object(i, 1000 + i);
        is.tiles.set_owner(i, i % 3);
    }
    sim::WriteArchive wa;
    fields(wa, std::as_const(is));
    const std::vector<std::uint8_t> bytes = wa.take();
    {
        sim::Island back;
        sim::ReadArchive r(bytes);
        fields(r, back);
        REQUIRE_FALSE(r.failed());
        CHECK(r.remaining() == 0);
        CHECK(back.tiles.width() == 5);
        CHECK(back.tiles.height() == 4);
        CHECK(back.tiles.chunk_edge() == 8);
        for (std::uint64_t i = 0; i < 20; ++i) {
            REQUIRE(back.tiles.object(i) == 1000 + i);
            REQUIRE(back.tiles.owner(i) == i % 3);
        }
        // Freshly read: every chunk is dirty and the hash equals the original's.
        CHECK(back.tiles.dirty_chunks() == back.tiles.chunk_count());
        CHECK(island_test::hash_of(back) == island_test::hash_of(is));
    }
    // Every strict prefix fails cleanly.
    for (std::size_t n = 0; n < bytes.size(); ++n) {
        sim::Island back;
        sim::ReadArchive r(std::span<const std::uint8_t>(bytes.data(), n));
        fields(r, back);
        REQUIRE(r.failed());
    }
    // A stream that is short by less than one tile still fails on the size check
    // (the 13 bytes of a tile), not later inside a column.
    for (const std::size_t cut :
         {std::size_t{1}, std::size_t{5}, std::size_t{12}, std::size_t{13}, std::size_t{14},
          std::size_t{25}, std::size_t{26}, std::size_t{260}}) {
        INFO("cut " << cut);
        sim::Island back;
        sim::ReadArchive r(std::span<const std::uint8_t>(bytes.data(), bytes.size() - cut));
        fields(r, back);
        REQUIRE(r.failed());
        CHECK(r.error().find("tile columns need 20 x 13 bytes") != std::string::npos);
    }
    // The tiles scope starts 4+4+4+4+4+4+4+8 (to the sweep) + 14 + 2 + 6 + 6 + 1 + 8 +
    // 8 bytes into the island: find it from the end instead (3 x 4 header, then
    // 20 x 13 column bytes).
    const std::size_t tiles_at = bytes.size() - 12 - 20 * sim::kTileBytes;
    auto patched = [&](std::size_t at, std::uint32_t v) {
        std::vector<std::uint8_t> b = bytes;
        island_test::put32(b, at, v);
        sim::Island back;
        sim::ReadArchive r(b);
        fields(r, back);
        return r.failed() ? r.error() : std::string();
    };
    CHECK(patched(tiles_at, 6).find("differs from the island's") != std::string::npos);
    CHECK(patched(tiles_at + 4, 3).find("differs from the island's") != std::string::npos);
    CHECK(patched(tiles_at + 8, 0).find("chunk_edge is 0") != std::string::npos);
    // The island's own w and h (offsets 12 and 16) must equal the layer's.
    CHECK(patched(12, 7).find("differs from the island's") != std::string::npos);
    CHECK(patched(16, 5).find("differs from the island's") != std::string::npos);
    // Dimensions the bytes cannot back: rejected before anything is allocated.
    {
        std::vector<std::uint8_t> b = bytes;
        for (const std::size_t at : {std::size_t{12}, tiles_at}) {
            island_test::put32(b, at, 30000);
        }
        for (const std::size_t at : {std::size_t{16}, tiles_at + 4}) {
            island_test::put32(b, at, 2000);
        }
        sim::Island back;
        sim::ReadArchive r(b);
        fields(r, back);
        REQUIRE(r.failed());
        // 60 million tiles are within kIslandTileBound but 13 bytes each are not there.
        CHECK(r.error().find("tile columns need") != std::string::npos);
        CHECK(back.tiles.width() == 0);
    }
    // The edge of the tile bound on the reader: exactly kIslandTileBound tiles are
    // within it and fail on the byte count (a 1 x 2^26 layer is never allocated),
    // one more fails on the bound.
    for (const std::uint32_t h : {1U << 26, (1U << 26) + 1}) {
        std::vector<std::uint8_t> b = bytes;
        for (const std::size_t at : {std::size_t{12}, tiles_at}) {
            island_test::put32(b, at, 1);
        }
        for (const std::size_t at : {std::size_t{16}, tiles_at + 4}) {
            island_test::put32(b, at, h);
        }
        sim::Island back;
        sim::ReadArchive r(b);
        fields(r, back);
        REQUIRE(r.failed());
        INFO("h " << h);
        if (h == (1U << 26)) {
            CHECK(r.error().find("tile columns need 67108864 x 13 bytes") != std::string::npos);
            CHECK(r.error().find("more than") == std::string::npos);
        } else {
            CHECK(r.error().find("more than 67108864 tiles") != std::string::npos);
        }
        CHECK(back.tiles.width() == 0);
    }
    {
        std::vector<std::uint8_t> b = bytes;
        for (const std::size_t at : {std::size_t{12}, tiles_at, std::size_t{16}, tiles_at + 4}) {
            island_test::put32(b, at, 0xFFFFFFFFU);
        }
        sim::Island back;
        sim::ReadArchive r(b);
        fields(r, back);
        REQUIRE(r.failed());
        CHECK(r.error().find("more than") != std::string::npos);
        CHECK(back.tiles.width() == 0);
    }
}
