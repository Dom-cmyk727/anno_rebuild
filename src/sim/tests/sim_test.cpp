// The deterministic core against the documented facts and fixtures
// (docs/re/tick-loop.md §2–3, fixture F6; docs/sim/core.md).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/clock.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/world.hpp>
#include <anno/test/game_dir.hpp>

namespace sim = anno::sim;

TEST_CASE("MSVC rand table equals the original's") {
    const auto t = sim::msvc_rand_sequence(sim::kOriginalRngLength);
    REQUIRE(t.size() == 2048);
    // First outputs of MSVC rand() after srand(1).
    CHECK(std::vector<std::uint16_t>(t.begin(), t.begin() + 6) ==
          std::vector<std::uint16_t>{41, 18467, 6334, 26500, 19169, 15724});
    // Facts of the table at AnnoWelt.dll 0x100cb8f8 (docs/re/tick-loop.md §3);
    // the sum is our own check value (docs/sim/core.md).
    CHECK(*std::min_element(t.begin(), t.end()) == 21);
    CHECK(*std::max_element(t.begin(), t.end()) == 32757);
    CHECK(std::set<std::uint16_t>(t.begin(), t.end()).size() == 1991);
    std::uint64_t sum = 0;
    for (const auto v : t) {
        sum += v;
    }
    CHECK(sum == 33419328);
}

TEST_CASE("world PRNG: fixture F6") {
    sim::WorldRng r;
    CHECK(r.cursor() == 0);
    CHECK(r.next() == 18467);
    CHECK(r.next() == 6334);
    CHECK(r.next() == 26500);
    CHECK(r.next() == 19169);
    CHECK(r.next() == 15724);
    CHECK(r.cursor() == 5);
    r.set_cursor(2047);
    CHECK(r.next() == 41);
    CHECK(r.next() == 18467);
    CHECK(r.next() == 6334);
    r.set_cursor(1000);
    CHECK(r.next() == 1528);
    CHECK(r.next() == 26679);
    CHECK(r.next() == 7864);
    // Seeding through property 8 takes the value mod 2048; the period is 2048.
    r.set_cursor(2048 + 7);
    CHECK(r.cursor() == 7);
    sim::WorldRng a, b;
    for (int i = 0; i < 2048; ++i) {
        a.next();
    }
    CHECK(a.cursor() == b.cursor());
    CHECK(a.next() == b.next());
    // Any table length works (not limited to the original's); empty is refused.
    auto small = sim::WorldRng::from_table({5, 6, 7}, 4);
    REQUIRE(small);
    CHECK(small->cursor() == 1);
    CHECK(small->next() == 7);
    CHECK(small->next() == 5);
    auto one = sim::WorldRng::from_table({9});
    REQUIRE(one);
    CHECK(one->next() == 9);
    CHECK(one->next() == 9);
    CHECK_FALSE(sim::WorldRng::from_table({}));
    // A different table of the same length changes the state hash.
    auto other = sim::WorldRng::from_table(sim::msvc_rand_sequence(2048, 2));
    REQUIRE(other);
    CHECK(other->table_digest() != sim::WorldRng().table_digest());
    // A moved-from generator still works (moves copy).
    sim::WorldRng src;
    const sim::WorldRng dst = std::move(src);
    CHECK(src.next() == dst.table()[1]); // NOLINT(bugprone-use-after-move): intended
    sim::WorldRng assigned;
    assigned = std::move(src);
    CHECK(src.size() == sim::kOriginalRngLength);
}

TEST_CASE("world clock: one tick is 2 ms") {
    sim::WorldClock c;
    CHECK(c.advance(5) == 2);
    // +0x108 holds the new tick after an advance (D: 0x100ae126, 0x100ae12d).
    CHECK(c.last_tick == 2);
    CHECK(c.last_tick == c.tick());
    CHECK(c.advance(1) == 3);
    CHECK(c.ms == 6);
    for (int i = 0; i < 1000; ++i) {
        c.advance(1);
    }
    CHECK(c.tick() == c.ms >> 1);
    CHECK(c.game_seconds() == 1); // 1006 ms → tick 503 → 1 s
    CHECK(c.game_seconds(10) == 11);
}

TEST_CASE("single-player step size equals the original's x87 formula") {
    // The original's chain (tick-loop.md §6): speed = f32(pct · 0.01f) (fild,
    // fmul by the f32 at 0x10086354, fstp to f32, default round-to-nearest),
    // then trunc(dt · speed + 0.5). Both products are exact in double, so
    // double reproduces the x87 values here.
    for (const std::uint32_t pct : {50U, 100U, 200U, 400U}) {
        const auto speed =
            static_cast<float>(static_cast<double>(pct) * static_cast<double>(0.01F));
        for (std::uint32_t dt = 0; dt <= 1000; ++dt) {
            const auto x87 = static_cast<std::int64_t>(
                std::trunc(static_cast<double>(dt) * static_cast<double>(speed) + 0.5));
            const auto got = sim::single_player_step_ms(dt, pct);
            if (x87 < 1) {
                CHECK_FALSE(got);
            } else {
                REQUIRE(got);
                CHECK(*got == std::min<std::int64_t>(x87, 100));
            }
        }
    }
    // Examples from docs/re/tick-loop.md §2.
    // 50 % at 1 ms frames runs at full speed.
    CHECK(sim::single_player_step_ms(1, 50) == 1U);
    CHECK(sim::single_player_step_ms(3, 50) == 2U);
    CHECK(sim::single_player_step_ms(16, 100) == 16U);
    // Capped: time is lost on slow frames.
    CHECK(sim::single_player_step_ms(150, 100) == 100U);
    CHECK(sim::single_player_step_ms(30, 400) == 100U);
    CHECK(sim::single_player_step_ms(5000, 100) == 100U);
    // Paused; not an original speed.
    CHECK_FALSE(sim::single_player_step_ms(16, 0));
    CHECK_FALSE(sim::single_player_step_ms(16, 75));
}

TEST_CASE("multiplayer step size and speeds") {
    // trunc(f32 speed · 10.0f) for the speeds a player can set.
    for (const std::uint32_t pct : {50U, 100U, 200U}) {
        const auto speed =
            static_cast<float>(static_cast<double>(pct) * static_cast<double>(0.01F));
        const auto x87 = static_cast<std::uint32_t>(std::trunc(static_cast<double>(speed) * 10.0));
        CHECK(sim::multiplayer_step_ms(pct) == x87);
    }
    CHECK(sim::multiplayer_step_ms(100) == 10U);
    // Pause stops turns; 400 % is refused in multiplayer.
    CHECK_FALSE(sim::multiplayer_step_ms(0));
    CHECK_FALSE(sim::multiplayer_step_ms(400));
    CHECK(sim::is_original_speed(400, false));
    CHECK(sim::is_original_speed(0, false));
    CHECK_FALSE(sim::is_original_speed(0, true));
    // A turn: 10 steps of 10 real ms; game time per turn at 100 % = 100 ms.
    const sim::Timing t;
    CHECK(t.mp_steps_per_turn * t.mp_step_real_ms == t.mp_turn_real_ms);
    CHECK(t.mp_steps_per_turn * *sim::multiplayer_step_ms(100) == 100);
}

TEST_CASE("island sweep scheduling") {
    // One full sweep of the chunks per 2000 ticks (4 s), at most all chunks
    // per call (D: AnnoWelt.dll 0x1008b7b3).
    CHECK(sim::sweep_chunks(64, 0, 2000) == 1);
    CHECK(sim::sweep_chunks(64, 31, 2000) == 1);
    CHECK(sim::sweep_chunks(64, 32, 2000) == 2);
    CHECK(sim::sweep_chunks(64, 1968, 2000) == 63);
    CHECK(sim::sweep_chunks(64, 1969, 2000) == 64);
    CHECK(sim::sweep_chunks(64, 2000, 2000) == 64);
    CHECK(sim::sweep_chunks(100, 65535, 15000) == 100);
    CHECK(sim::sweep_chunks(0, 500, 2000) == 0);
    CHECK(sim::sweep_chunks(7, 1, 0) == 7);
}

TEST_CASE("state hash: canonical and sensitive") {
    sim::StateHasher h;
    CHECK(h.value() == 0xcbf29ce484222325ULL);
    h.u8('a');
    CHECK(h.value() == 0xaf63dc4c8601ec8cULL); // FNV-1a 64 of "a"
    sim::StateHasher le;
    le.u32(0x04030201U);
    sim::StateHasher bytes;
    const std::uint8_t b[] = {1, 2, 3, 4};
    bytes.bytes(b);
    CHECK(le.value() == bytes.value());
    sim::World w1(1234), w2(1234), w3(1235);
    CHECK(w1.state_hash() == w2.state_hash());
    CHECK(w1.state_hash() != w3.state_hash());
    w1.advance(10);
    CHECK(w1.state_hash() != w2.state_hash());
    w2.advance(10);
    CHECK(w1.state_hash() == w2.state_hash());
    w1.rng.next();
    CHECK(w1.state_hash() != w2.state_hash());
}

namespace {

/// The bytes at virtual address `va` of a PE32 image (section table
/// lookup; PE/COFF specification), or empty if the file is not a PE32 image or
/// the range is not fully inside one section's raw data. Every read is
/// bounds-checked.
std::vector<std::uint8_t> pe_bytes(const std::vector<std::uint8_t>& pe, std::uint32_t va,
                                   std::size_t n) {
    bool ok = true;
    auto u16 = [&](std::uint64_t o) -> std::uint32_t {
        if (o + 2 > pe.size()) {
            ok = false;
            return 0;
        }
        return static_cast<std::uint32_t>(pe[o] | pe[o + 1] << 8);
    };
    auto u32 = [&](std::uint64_t o) { return u16(o) | u16(o + 2) << 16; };
    if (u16(0) != 0x5A4D) { // "MZ"
        return {};
    }
    const std::uint64_t nt = u32(0x3c);
    if (u32(nt) != 0x00004550 || u16(nt + 24) != 0x10b || !ok) { // "PE\0\0", PE32
        return {};
    }
    const std::uint32_t sections = u16(nt + 6);
    const std::uint64_t opt = nt + 24;
    const std::uint64_t base = u32(opt + 28);
    const std::uint64_t table = opt + u16(nt + 20);
    if (!ok || va < base) {
        return {};
    }
    const std::uint64_t rva = va - base;
    for (std::uint32_t k = 0; k < sections; ++k) {
        const std::uint64_t h = table + 40 * std::uint64_t{k};
        const std::uint64_t vaddr = u32(h + 12);
        const std::uint64_t size = std::min(u32(h + 8), u32(h + 16)); // virtual, raw
        const std::uint64_t raw = u32(h + 20);
        if (!ok) {
            return {};
        }
        if (rva >= vaddr && rva + n <= vaddr + size) {
            const std::uint64_t o = raw + (rva - vaddr);
            if (o + n > pe.size()) {
                return {};
            }
            return {pe.begin() + static_cast<std::ptrdiff_t>(o),
                    pe.begin() + static_cast<std::ptrdiff_t>(o + n)};
        }
    }
    return {};
}

} // namespace

TEST_CASE("pe_bytes refuses truncated and non-PE input") {
    CHECK(pe_bytes({}, 0x10001000U, 4).empty());
    CHECK(pe_bytes({'M', 'Z'}, 0x10001000U, 4).empty());
    std::vector<std::uint8_t> junk(0x200, 0xFF);
    junk[0] = 'M';
    junk[1] = 'Z';
    CHECK(pe_bytes(junk, 0x10001000U, 4).empty());
}

TEST_CASE("the generated table equals the one in AnnoWelt.dll") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::ifstream in(*dir / "AnnoWelt.dll", std::ios::binary);
    REQUIRE(in);
    const std::vector<std::uint8_t> pe(std::istreambuf_iterator<char>(in), {});
    const auto bytes = pe_bytes(pe, 0x100cb8f8U, 4096);
    REQUIRE(bytes.size() == 4096);
    const auto t = sim::msvc_rand_sequence(sim::kOriginalRngLength);
    for (std::size_t i = 0; i < t.size(); ++i) {
        REQUIRE(static_cast<std::uint16_t>(bytes[2 * i] | bytes[2 * i + 1] << 8) == t[i]);
    }
}
