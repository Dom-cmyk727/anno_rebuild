// The deterministic core against the documented facts and fixtures
// (docs/re/tick-loop.md §2–3, fixture F6; docs/sim/core.md).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/world.hpp>
#include <anno/test/game_dir.hpp>

#include "golden.hpp"
#include "world_access.hpp"

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
    sim::WorldTestAccess::rng(w1).next();
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

// ---------------------------------------------------------------------------
// Stage S1 of docs/sim/state.md: ids, draw sites, the hierarchical state
// hash over the field list, the shared PRNG table, the clock's begin/end.
// ---------------------------------------------------------------------------

TEST_CASE("ids: invalid by default, ordered, building ids pack island and serial") {
    sim::IslandId none;
    CHECK_FALSE(none.valid());
    CHECK(sim::IslandId{0}.valid());
    CHECK(sim::IslandId{0} != none);
    CHECK(sim::IslandId{1} < sim::IslandId{2});
    CHECK_FALSE(sim::BuildingId{}.valid());
    CHECK(sim::BuildingId{0}.valid());
    CHECK(sim::BuildingId{0xFFFFFFFFFFFFFFFFULL} == sim::BuildingId{});

    const sim::BuildingId b = sim::make_building_id(sim::IslandId{3}, 77);
    CHECK(b.v == (std::uint64_t{3} << 32) + 77);
    CHECK(sim::island_of(b) == sim::IslandId{3});
    CHECK(sim::serial_of(b) == 77);
    // A full serial does not bleed into the island half.
    const sim::BuildingId c = sim::make_building_id(sim::IslandId{1}, 0xFFFFFFFFU);
    CHECK(c.v == 0x1FFFFFFFFULL);
    CHECK(sim::island_of(c) == sim::IslandId{1});
    CHECK(sim::serial_of(c) == 0xFFFFFFFFU);
    // Ids of different islands are ordered by island first.
    CHECK(sim::make_building_id(sim::IslandId{1}, 9) < sim::make_building_id(sim::IslandId{2}, 1));
    CHECK(sim::make_building_id(sim::IslandId{2}, 1) < sim::make_building_id(sim::IslandId{2}, 2));

    // City order is island, then city number; building order is city, list, slot.
    const sim::CityRef c01{sim::IslandId{0}, sim::CityNo{1}};
    const sim::CityRef c10{sim::IslandId{1}, sim::CityNo{0}};
    CHECK(c01 < c10);
    CHECK(c01 == sim::CityRef{sim::IslandId{0}, sim::CityNo{1}});
    const sim::BuildingKey k1{c01, 2, 5};
    const sim::BuildingKey k2{c01, 3, 0};
    const sim::BuildingKey k3{c10, 0, 0};
    CHECK(k1 < k2);
    CHECK(k2 < k3);
    CHECK(sim::BuildingKey{}.slot == sim::kNoSlot);
    // Designated initialisers follow the member order, so they do not hide a
    // swapped order of the members (a positional initialiser would reorder
    // along with them). The two keys disagree between list and slot: the list
    // decides first, the slot only breaks a tie; the city beats both.
    const sim::BuildingKey low_list{.city = c01, .list = 1, .slot = 9};
    const sim::BuildingKey high_list{.city = c01, .list = 2, .slot = 0};
    const sim::BuildingKey same_list{.city = c01, .list = 2, .slot = 1};
    const sim::BuildingKey next_city{.city = c10, .list = 0, .slot = 0};
    CHECK(low_list < high_list);
    CHECK(high_list > low_list);
    CHECK(high_list < same_list);
    CHECK(same_list < next_city);
    CHECK(sim::BuildingKey{.city = c01, .list = 2, .slot = 1} == same_list);
    CHECK(sim::TileXY{1, 2} < sim::TileXY{1, 3});
    CHECK(sim::TileRect{1, 2, 3, 4} == sim::TileRect{1, 2, 3, 4});
    const sim::IdCounters ids;
    CHECK(ids.next_figure == 1);
    CHECK(ids.next_event_seq == 1);
}

namespace {

struct RecordingDraw final : sim::DrawSource {
    std::uint16_t next(sim::DrawSite site) override {
        sites.push_back(site);
        return static_cast<std::uint16_t>(100 + sites.size());
    }
    std::vector<sim::DrawSite> sites;
};

} // namespace

TEST_CASE("draw sources forward the site and the world stream") {
    sim::WorldRng rng;
    sim::GlobalDraw global(rng);
    sim::WorldRng reference;
    // The global source ignores the site and consumes the one world stream.
    CHECK(global.next(sim::DrawSite::SmallFarmAnimal) == reference.next());
    CHECK(global.next(sim::DrawSite::Event300) == reference.next());
    CHECK(global.next(sim::DrawSite::Event750Gate) == reference.next());
    CHECK(rng.cursor() == 3);
    CHECK(rng.cursor() == reference.cursor());

    // A SiteDraw gives production rules a DrawRef that names its site.
    RecordingDraw rec;
    sim::SiteDraw farm(rec, sim::DrawSite::SmallFarmAnimal);
    const sim::DrawRef ref = sim::DrawRef::of(farm);
    CHECK(ref() == 101);
    CHECK(ref() == 102);
    sim::SiteDraw ev(rec, sim::DrawSite::Event750Island);
    CHECK(sim::DrawRef::of(ev)() == 103);
    REQUIRE(rec.sites.size() == 3);
    CHECK(rec.sites[0] == sim::DrawSite::SmallFarmAnimal);
    CHECK(rec.sites[1] == sim::DrawSite::SmallFarmAnimal);
    CHECK(rec.sites[2] == sim::DrawSite::Event750Island);
    // Implemented sites first, the unported ones declared after.
    // Every enumerator value is pinned: the order is the PRNG timeline
    // manifest of docs/sim/state.md §6, and a reorder is a recorded bump.
    CHECK(static_cast<int>(sim::DrawSite::SmallFarmAnimal) == 0);
    CHECK(static_cast<int>(sim::DrawSite::Event300) == 1);
    CHECK(static_cast<int>(sim::DrawSite::Event750Gate) == 2);
    CHECK(static_cast<int>(sim::DrawSite::Event750Island) == 3);
    CHECK(static_cast<int>(sim::DrawSite::CityRandomEvents) == 4);
    CHECK(static_cast<int>(sim::DrawSite::WildlifeSweep) == 5);
    CHECK(static_cast<int>(sim::DrawSite::Ai) == 6);
    CHECK(static_cast<int>(sim::DrawSite::LinkRawMaterial) == 7);
    CHECK(static_cast<int>(sim::DrawSite::LinkTreasure) == 11);
    CHECK(static_cast<int>(sim::DrawSite::ComputerName) == 16);
    CHECK(static_cast<int>(sim::DrawSite::Count_) == 17);
    CHECK(sim::DrawRecord{}.count == 0);
}

namespace {

sim::World world_after(std::uint32_t seed, int steps, std::uint32_t step_ms) {
    sim::World w(seed);
    for (int i = 0; i < steps; ++i) {
        w.advance(step_ms);
    }
    return w;
}

} // namespace

TEST_CASE("core_hash equals the v1 golden") {
    // core.md's five-field FNV-1a. The values come from an independent Python
    // transcription (struct.pack of ms, last_tick, 2048, table digest, cursor).
    CHECK(sim::World().core_hash() == 0x977a5cb1c2ae725dULL);
    CHECK(world_after(1234, 1, 10).core_hash() == 0x65e3e2696c0396b0ULL);
    CHECK(world_after(1234, 1000, 10).core_hash() == 0xc3ae91a5ed6087ffULL);
    // The table digest itself (FNV-1a of the 2048 u16 values, little endian).
    CHECK(sim::WorldRng().table_digest() == 0x1bf8daace9f69fabULL);
    // The v1 hash is the five fields and nothing else.
    sim::World w;
    const std::uint64_t base = w.core_hash();
    sim::WorldTestAccess::clock(w).ms = 1;
    CHECK(w.core_hash() != base);
    w = sim::World();
    sim::WorldTestAccess::clock(w).last_tick = 1;
    CHECK(w.core_hash() != base);
    w = sim::World();
    sim::WorldTestAccess::rng(w).next();
    CHECK(w.core_hash() != base);
    w = sim::World();
    sim::WorldTestAccess::rng(w) = *sim::WorldRng::from_table(sim::msvc_rand_sequence(2048, 2));
    CHECK(w.core_hash() != base);
}

TEST_CASE("state_hash relational properties") {
    const sim::World base(1234);
    const std::uint64_t h = base.state_hash();
    const sim::World same(1234);
    CHECK(same.state_hash() == h);
    const sim::World copy = base;
    CHECK(copy.state_hash() == h);
    CHECK(h != base.core_hash());

    // Every field of the list moves the hash.
    sim::World w = base;
    sim::WorldTestAccess::clock(w).ms += 2;
    CHECK(w.state_hash() != h);
    w = base;
    sim::WorldTestAccess::clock(w).last_tick += 1;
    CHECK(w.state_hash() != h);
    w = base;
    sim::WorldTestAccess::rng(w).next();
    CHECK(w.state_hash() != h);
    w = base;
    sim::WorldTestAccess::rng(w) =
        *sim::WorldRng::from_table(sim::msvc_rand_sequence(2048, 2), 1234);
    CHECK(w.state_hash() != h);
    w = base;
    sim::WorldTestAccess::rng(w) =
        *sim::WorldRng::from_table(sim::msvc_rand_sequence(2047, 1), 1234);
    CHECK(w.state_hash() != h);
    // The hash depends on the state only, not on how it was reached. Two steps of 5 ms
    // and one of 10 ms reach the same clock, PRNG, player table and figure registry in
    // an empty world; they differ only in the monopoly accumulator (stage S12: the
    // refresh body runs on the first step and adds that step, 5 or 10 ms, D5), and
    // with that one field set equal the hashes are equal.
    sim::World a(1234), b(1234);
    a.advance(5);
    a.advance(5);
    b.advance(10);
    CHECK(a.monopoly().acc == 5);
    CHECK(b.monopoly().acc == 10);
    CHECK(a.state_hash() != b.state_hash());
    sim::WorldTestAccess::monopoly(b).acc = 5;
    CHECK(a.state_hash() == b.state_hash());

    // The scopes of stream 106: header, clock_rng, settings, ids, players, islands,
    // figures, events, timers_monopoly; the clock_rng scope is exactly the v1
    // five-field hash.
    sim::HashTree tree;
    CHECK(base.state_hash(tree, 0) == h);
    CHECK(tree.root.hash == h);
    REQUIRE(tree.root.children.size() == 9);
    CHECK(tree.root.children[0].name == "header");
    CHECK(tree.root.children[1].name == "clock_rng");
    CHECK(tree.root.children[2].name == "settings");
    CHECK(tree.root.children[3].name == "ids");
    CHECK(tree.root.children[4].name == "players");
    CHECK(tree.root.children[5].name == "islands");
    CHECK(tree.root.children[6].name == "figures");
    CHECK(tree.root.children[7].name == "events");
    CHECK(tree.root.children[8].name == "timers_monopoly");
    CHECK(tree.root.children[1].hash == base.core_hash());
    // The header scope: the two versions, SimConfig, Rules, the registry digest
    // and the seam names, in one FNV-1a stream (docs/sim/state.md §9.3).
    sim::WriteArchive config_bytes;
    fields(config_bytes, base.config());
    sim::WriteArchive rules_bytes;
    fields(rules_bytes, base.rules());
    sim::StateHasher header;
    header.u32(sim::kStateHashVersion);
    header.u32(sim::kPrngTimelineVersion);
    header.bytes(config_bytes.data());
    header.bytes(rules_bytes.data());
    header.u64(base.types().digest());
    header.u64(0);  // walk-table digest: the test world has no walk tables
    header.u64(22); // 8 city passes + 14 other seams
    for (int i = 0; i < 22; ++i) {
        header.u64(7);
        const std::uint8_t name[] = {'d', 'e', 'f', 'a', 'u', 'l', 't'};
        header.bytes(name);
    }
    CHECK(tree.root.children[0].hash == header.value());
    sim::WriteArchive settings_bytes;
    fields(settings_bytes, base.settings());
    sim::StateHasher settings;
    settings.bytes(settings_bytes.data());
    CHECK(tree.root.children[2].hash == settings.value());
    // The ids scope: u64 next_figure, u64 next_event_seq (both 1 in a new world), u32
    // event750_acc, u32 monopoly.acc (both 0) and bool faulted (false).
    sim::StateHasher ids;
    ids.u64(1);
    ids.u64(1);
    ids.u32(0);
    ids.u32(0);
    ids.u8(0);
    CHECK(tree.root.children[3].hash == ids.value());
    // The players scope: u64 slot count, one child scope per slot (an empty slot
    // is the single byte 0), u16 stamp, u32 cursor (docs/sim/state.md §9.3).
    sim::StateHasher absent;
    absent.u8(0);
    sim::StateHasher players;
    players.u64(32);
    for (int i = 0; i < 32; ++i) {
        players.u64(absent.value());
    }
    players.u16(0);
    players.u32(0);
    CHECK(tree.root.children[4].hash == players.value());
    // The islands scope of a world without islands: the u64 slot count 0.
    sim::StateHasher islands;
    islands.u64(0);
    CHECK(tree.root.children[5].hash == islands.value());
    // The figures scope of a new world: the registry list (i64 hint -1, u32 cursor, u64
    // slot count 1 and its child scope, an empty slot being the single byte 0), the
    // u16 stamp and the empty expiry list; the events scope: the u64 count 0.
    sim::StateHasher figures;
    figures.u64(0xffffffffffffffffULL); // hint -1
    figures.u32(0);
    figures.u64(1);
    figures.u64(absent.value());
    figures.u16(0);
    figures.u64(0);
    CHECK(tree.root.children[6].hash == figures.value());
    sim::StateHasher events;
    events.u64(0);
    CHECK(tree.root.children[7].hash == events.value());
    // The timers_monopoly scope: u32 last_tick 0, five stopped timers (count + u8
    // each, count + u32 each), 28 monopoly entries (u32 time 0, absent player).
    sim::StateHasher timers;
    timers.u32(0);
    timers.u64(5);
    for (int i = 0; i < 5; ++i) {
        timers.u8(0);
    }
    timers.u64(5);
    for (int i = 0; i < 5; ++i) {
        timers.u32(0);
    }
    timers.u64(28);
    for (int i = 0; i < 28; ++i) {
        timers.u32(0);
        timers.u8(0);
    }
    CHECK(tree.root.children[8].hash == timers.value());
    sim::StateHasher root;
    root.u64(header.value());
    root.u64(base.core_hash());
    root.u64(settings.value());
    root.u64(ids.value());
    root.u64(players.value());
    root.u64(islands.value());
    root.u64(figures.value());
    root.u64(events.value());
    root.u64(timers.value());
    CHECK(root.value() == h);
    // The empty world, from the independent Python transcription of the stream
    // (tools/sim_stream.py: header with SimConfig, Rules, registry digest and seam
    // names, clock_rng, settings, ids, players, islands, figures, events,
    // timers_monopoly).
    CHECK(sim::World().state_hash() == 0xf437905c6e27429eULL);
    CHECK(sim::kStateHashVersion == 116);
    CHECK(sim::kPrngTimelineVersion == 2);
}

namespace {

/// A value whose field list nests scopes three deep (the world itself only has
/// top-level scopes until S2).
struct Nested {
    std::uint32_t a = 1;
    std::uint8_t b = 2;
};

template <class A, sim::Same<Nested> S> void fields(A& a, S& s) {
    a.u32(s.a);
    a.scope("l0", [&] {
        a.u8(s.b);
        a.scope("l1", 4, [&] {
            a.u8(s.b);
            a.scope("l2", [&] { a.u8(s.b); });
        });
    });
}

} // namespace

TEST_CASE("hash_state records the scopes down to the requested depth") {
    const Nested value;
    const std::uint64_t flat = sim::hash_state(value, nullptr, 0);
    for (const std::uint32_t depth : {0U, 1U, 2U, 3U, 9U}) {
        sim::HashTree tree;
        INFO("depth " << depth);
        CHECK(sim::hash_state(value, &tree, depth) == flat);
        CHECK(tree.root.hash == flat);
        REQUIRE(tree.root.children.size() == 1);
        const sim::HashNode& l0 = tree.root.children[0];
        CHECK(l0.name == "l0");
        CHECK_FALSE(l0.indexed);
        if (depth == 0) {
            CHECK(l0.children.empty());
            continue;
        }
        REQUIRE(l0.children.size() == 1);
        const sim::HashNode& l1 = l0.children[0];
        CHECK(l1.name == "l1");
        CHECK(l1.indexed);
        CHECK(l1.index == 4);
        if (depth == 1) {
            CHECK(l1.children.empty());
            continue;
        }
        REQUIRE(l1.children.size() == 1);
        CHECK(l1.children[0].name == "l2");
        CHECK(l1.children[0].children.empty()); // the deepest scope has no children
    }
    // The hash of the world is the hash of the same helper.
    const sim::World w(77);
    sim::HashTree tree;
    CHECK(sim::hash_state(w, &tree, 5) == w.state_hash());
    CHECK(w.state_hash(tree, 5) == w.state_hash());
    // The world's top-level scopes are header, clock_rng, settings, ids, players,
    // islands, figures, events and timers_monopoly. Only players and figures have
    // children in a world
    // without islands: one scope `player` per slot, indexed by the slot (S4, the
    // first world scope that nests), and one scope `figure` per slot of the registry
    // (S7: one empty slot), recorded from depth 1 on and not at depth 0.
    REQUIRE(tree.root.children.size() == 9);
    for (const sim::HashNode& c : tree.root.children) {
        CHECK(c.children.size() == (c.name == "players" ? 32U : c.name == "figures" ? 1U : 0U));
    }
    const sim::HashNode& table = tree.root.children[4];
    for (std::size_t i = 0; i < table.children.size(); ++i) {
        CHECK(table.children[i].name == "player");
        CHECK(table.children[i].indexed);
        CHECK(table.children[i].index == i);
        CHECK(table.children[i].children.empty());
    }
    sim::HashTree shallow;
    CHECK(w.state_hash(shallow, 0) == w.state_hash());
    REQUIRE(shallow.root.children.size() == 9);
    CHECK(shallow.root.children[4].children.empty());
}

TEST_CASE("rng copy shares the table") {
    const sim::WorldRng a;
    sim::WorldRng b = a;
    REQUIRE(a.shared_table());
    CHECK(a.shared_table() == b.shared_table());
    CHECK(a.table().data() == b.table().data());
    // Default generators share one table; a custom table is its own.
    const sim::WorldRng c;
    CHECK(c.shared_table() == a.shared_table());
    const auto custom = sim::WorldRng::from_table({5, 6, 7});
    REQUIRE(custom);
    CHECK(custom->shared_table() != a.shared_table());
    const sim::WorldRng custom_copy = *custom;
    CHECK(custom_copy.shared_table() == custom->shared_table());
    // The cursor is per generator.
    b.next();
    b.next();
    CHECK(a.cursor() == 0);
    CHECK(b.cursor() == 2);
    // A copy of a World shares the table, and advancing the copy leaves the
    // original alone.
    sim::World w1(7);
    sim::World w2 = w1;
    CHECK(w1.rng().shared_table() == w2.rng().shared_table());
    const std::uint64_t before = w1.state_hash();
    w2.advance(100);
    sim::WorldTestAccess::rng(w2).next();
    CHECK(w1.state_hash() == before);
    CHECK(w2.state_hash() != before);
}

TEST_CASE("rng moved-from generator keeps its table") {
    sim::WorldRng src;
    const auto table = src.shared_table();
    const sim::WorldRng dst = std::move(src);
    CHECK(dst.shared_table() == table);
    CHECK(src.shared_table() == table); // NOLINT(bugprone-use-after-move): intended
    CHECK(src.size() == sim::kOriginalRngLength);
    sim::WorldRng assigned = *sim::WorldRng::from_table({1, 2});
    assigned = std::move(src);
    CHECK(assigned.shared_table() == table);
    CHECK(src.shared_table() == table); // NOLINT(bugprone-use-after-move): intended
    CHECK(src.next() == dst.table()[1]);
    // The same through a World move.
    sim::World w(5);
    const auto wt = w.rng().shared_table();
    const sim::World moved = std::move(w);
    CHECK(moved.rng().shared_table() == wt);
    CHECK(w.rng().shared_table() == wt); // NOLINT(bugprone-use-after-move): intended
}

TEST_CASE("rng stream cursor mix is the defined function") {
    // splitmix64 finaliser over seed + 0x9e3779b97f4a7c15 * (slot + 1), low 32
    // bits. mix(0, 0) is the low half of the well-known first output of
    // splitmix64 seeded with 0 (0xe220a8397b1dcdaf); the others are from an
    // independent Python transcription.
    CHECK(sim::rng_mix(0, 0) == 0x7b1dcdafU);
    CHECK(sim::rng_mix(1, 0) == 0x89025cc1U);
    CHECK(sim::rng_mix(1, 1) == 0x658eec67U);
    CHECK(sim::rng_mix(0x123456789abcdef0ULL, 7) == 0x0916c5cbU);
    CHECK(sim::rng_mix(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFU) == 0x2de810ecU);
    // The slot is part of the state step: neighbouring slots differ.
    CHECK(sim::rng_mix(1, 0) != sim::rng_mix(1, 1));
    CHECK(sim::rng_mix(1, 1) != sim::rng_mix(2, 1));

    const sim::WorldRng base;
    const sim::WorldRng s = base.stream(0, 0);
    CHECK(s.cursor() == 0x7b1dcdafU % 2048);
    CHECK(s.cursor() == 1455);
    CHECK(s.shared_table() == base.shared_table());
    CHECK(base.stream(1, 1).cursor() == 1127);
    CHECK(base.stream(0x123456789abcdef0ULL, 7).cursor() == 1483);
    // The stream continues like any generator from its start cursor.
    sim::WorldRng t = base.stream(0, 0);
    CHECK(t.next() == base.table()[1456]);
    // A pure function: the base generator is untouched and the result repeats.
    sim::WorldRng moving;
    moving.next();
    CHECK(moving.stream(9, 4).cursor() == base.stream(9, 4).cursor());
    CHECK(moving.cursor() == 1);
    // Any table length: the cursor is reduced modulo the length.
    const auto three = sim::WorldRng::from_table({10, 20, 30});
    REQUIRE(three);
    CHECK(three->stream(0, 0).cursor() == 0x7b1dcdafU % 3);
    CHECK(three->stream(1, 0).cursor() == 0x89025cc1U % 3);
}

TEST_CASE("clock begin end keeps last_tick") {
    sim::WorldClock c;
    // begin() adds the step and returns the new tick, last_tick stays: the
    // subsystems of an advance see the tick from before it (D: 0x100ae040).
    CHECK(c.begin(10) == 5);
    CHECK(c.ms == 10);
    CHECK(c.last_tick == 0);
    CHECK(c.tick() == 5);
    c.end();
    CHECK(c.last_tick == 5);
    CHECK(c.begin(3) == 6); // 13 ms -> tick 6
    CHECK(c.last_tick == 5);
    CHECK(c.begin(0) == 6); // a zero step is not special-cased
    CHECK(c.last_tick == 5);
    c.end();
    CHECK(c.last_tick == 6);
    // advance() is begin() then end().
    sim::WorldClock d;
    CHECK(d.advance(10) == 5);
    CHECK(d.last_tick == 5);
    CHECK(d.advance(0) == 5);
    // The counter is 64 bits wide.
    sim::WorldClock big;
    big.ms = 0xFFFFFFFFULL;
    CHECK(big.begin(2) == 0x80000000ULL);
    CHECK(big.ms == 0x100000001ULL);
}

TEST_CASE("field count canary") {
    // field_count<T>() is the number of members of an aggregate; fields(T)
    // must name exactly that many. A member added without a line in the field
    // list fails here (and the static_assert fails the build).
    static_assert(sim::field_count<sim::WorldClock>() == 2);
    static_assert(sim::field_count<sim::StateHeader>() == 2);
    static_assert(sim::field_count<sim::IdCounters>() == 2);
    static_assert(sim::field_count<sim::TileXY>() == 2);
    static_assert(sim::field_count<sim::TileRect>() == 4);
    static_assert(sim::field_count<sim::DrawRecord>() == 2);
    // The real payloads of production.hpp, with vector (also vector<bool>),
    // enum and nested-aggregate members: they get their fields() in S2, and
    // the canary then compares this count with the primitives visited. Run
    // on GCC and Clang by the CI matrix; a compiler that refuses the probe
    // shows up here, in S1, not later.
    static_assert(sim::field_count<sim::StatusTracker>() == 4);
    static_assert(sim::field_count<sim::StepResult>() == 9);
    static_assert(sim::field_count<sim::Workshop>() == 10);
    static_assert(sim::field_count<sim::WeaponsWorkshop>() == 11);
    static_assert(sim::field_count<sim::Farm>() == 10);
    static_assert(sim::field_count<sim::SmallFarm>() == 10);
    static_assert(sim::field_count<sim::SmallFarmAnimals>() == 3);
    // The probe bound is a template argument; a type beyond it is refused.
    static_assert(sim::field_count<sim::TileRect, 4>() == 4);
    // World and WorldRng are not aggregates: they canary themselves with a
    // structured binding next to their members (world.hpp, rng.hpp), which
    // fails to compile when a member is added without extending it.
    static_assert(
        std::is_void_v<decltype(sim::World::field_list_canary(std::declval<const sim::World&>()))>);
    static_assert(std::is_void_v<decltype(sim::WorldRng::field_list_canary(
                      std::declval<const sim::WorldRng&>()))>);

    // The probe counts what it should: scalars, enums and nested aggregates.
    struct Pair {
        std::uint32_t a;
        std::int32_t b;
    };
    struct Probe {
        bool flag;
        sim::DrawSite site;
        std::uint16_t small;
        Pair pair;
        std::int64_t big;
    };
    struct Empty {};
    static_assert(sim::field_count<Probe>() == 5);
    static_assert(sim::field_count<Pair>() == 2);
    static_assert(sim::field_count<Empty>() == 0);

    auto primitives = [](const sim::TraceArchive& t) {
        std::size_t n = 0;
        for (const sim::TraceEntry& e : t.entries()) {
            n += e.kind != sim::Prim::ScopeOpen && e.kind != sim::Prim::ScopeClose ? 1 : 0;
        }
        return n;
    };
    const sim::WorldClock clock;
    sim::TraceArchive clock_trace;
    fields(clock_trace, clock);
    CHECK(primitives(clock_trace) == sim::field_count<sim::WorldClock>());
    const sim::StateHeader header;
    sim::TraceArchive header_trace;
    fields(header_trace, header);
    CHECK(primitives(header_trace) == sim::field_count<sim::StateHeader>());
}

TEST_CASE("world core stream layout round trip and rejects") {
    sim::World w(1234);
    w.advance(10);
    w.advance(7);
    sim::WriteArchive wa;
    fields(wa, std::as_const(w));
    const std::vector<std::uint8_t> bytes = wa.take();
    // header: u32 116, u32 2, then SimConfig, Rules, registry digest and seam
    // names (config_test.cpp pins those bytes); clock_rng: u64 ms, u64
    // last_tick, u64 2048, u64 table digest, u32 cursor (36 bytes); settings
    // (38 bytes, config_test.cpp); ids: two u64, two u32 and a bool (25 bytes);
    // players: u64 32, 32 absent bytes, u16 stamp, u32 cursor (46 bytes); islands:
    // u64 0 (8 bytes); figures (31 bytes: no deferred list since T22), events (8 bytes) and
    // timers_monopoly (193 bytes), config_test.cpp. The clock_rng scope is found from the end.
    constexpr std::size_t kSettingsBytes = 38;
    constexpr std::size_t kIdsBytes = 25;
    constexpr std::size_t kPlayersBytes = 8 + 32 + 2 + 4;
    constexpr std::size_t kIslandsBytes = 8;
    constexpr std::size_t kTailBytes = 31 + 8 + 193;
    REQUIRE(bytes.size() >
            8 + 36 + kSettingsBytes + kIdsBytes + kPlayersBytes + kIslandsBytes + kTailBytes);
    const std::size_t at = bytes.size() - kTailBytes - kIslandsBytes - kPlayersBytes - kIdsBytes -
                           kSettingsBytes - 36; // clock_rng starts
    CHECK(std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + 8) ==
          std::vector<std::uint8_t>{116, 0, 0, 0, 2, 0, 0, 0});
    CHECK(bytes[at] == 17);    // ms = 17
    CHECK(bytes[at + 8] == 8); // last_tick = 8
    CHECK(bytes[at + 16] == 0);
    CHECK(bytes[at + 17] == 8); // 2048 = 0x0800
    CHECK(bytes[at + 32] == (1234 & 0xFF));
    CHECK(bytes[at + 33] == (1234 >> 8));

    auto read_into = [](sim::World& target, const std::vector<std::uint8_t>& in) {
        sim::ReadArchive r(in);
        fields(r, target);
        return r.failed() ? r.error() : (r.remaining() == 0 ? std::string() : "trailing");
    };
    SUBCASE("a fresh world reads it back to the same hash") {
        sim::World other;
        CHECK(read_into(other, bytes) == "");
        CHECK(other.state_hash() == w.state_hash());
        CHECK(other.clock().ms == 17);
        CHECK(other.clock().last_tick == 8);
        CHECK(other.rng().cursor() == 1234);
        // save(load(save(w))) == save(w)
        sim::WriteArchive again;
        fields(again, std::as_const(other));
        CHECK(again.data() == bytes);
    }
    SUBCASE("every prefix fails") {
        for (std::size_t n = 0; n < bytes.size(); ++n) {
            sim::World other;
            const std::vector<std::uint8_t> prefix(bytes.begin(),
                                                   bytes.begin() + static_cast<std::ptrdiff_t>(n));
            CHECK(read_into(other, prefix) != "");
        }
    }
    SUBCASE("another state hash version is refused") {
        auto bad = bytes;
        bad[0] = 111; // the build is 112, so 111 is a different version
        sim::World other;
        const std::string err = read_into(other, bad);
        CHECK(err.find("header") == 0);
        CHECK(err.find("version") != std::string::npos);
    }
    SUBCASE("another PRNG timeline version is refused") {
        auto bad = bytes;
        bad[4] = 3;
        sim::World other;
        const std::string err = read_into(other, bad);
        CHECK(err.find("header") == 0);
        CHECK(err.find("timeline") != std::string::npos);
    }
    SUBCASE("a table of another size or digest is refused") {
        auto bad = bytes;
        bad[at + 17] = 7; // size 0x0700
        sim::World other;
        std::string err = read_into(other, bad);
        CHECK(err.find("clock_rng") == 0);
        CHECK(err.find("table") != std::string::npos);
        bad = bytes;
        bad[at + 24] ^= 1; // digest
        err = read_into(other, bad);
        CHECK(err.find("table") != std::string::npos);
        // The same stream into a world with another table.
        sim::World custom;
        sim::WorldTestAccess::rng(custom) =
            *sim::WorldRng::from_table(sim::msvc_rand_sequence(2048, 2));
        CHECK(read_into(custom, bytes).find("table") != std::string::npos);
    }
    SUBCASE("a cursor outside the table is refused") {
        auto bad = bytes;
        bad[at + 32] = 0;
        bad[at + 33] = 8; // 2048
        sim::World other;
        CHECK(read_into(other, bad).find("cursor") != std::string::npos);
        bad[at + 33] = 7; // 1792: fine
        sim::World ok;
        CHECK(read_into(ok, bad) == "");
        CHECK(ok.rng().cursor() == 1792);
    }
}

TEST_CASE("golden G0 empty world seed 1234 advance 10 ms x 1000") {
    // The state hash after 1, 100 and 1000 steps, against
    // fixtures/synthetic/sim/goldens.txt (stage S1, re-baselined at development
    // version 101 by stage S3: the header gained the configuration and the
    // settings scope; at 102 by stage S4: the players scope, whose cursor and
    // stamp advance() moves).
    sim::World w(1234);
    int done = 0;
    for (const int checkpoint : {1, 100, 1000}) {
        while (done < checkpoint) {
            w.advance(10);
            ++done;
        }
        const std::string name = "g0_step" + std::to_string(checkpoint);
        std::string update_line;
        const std::string problem =
            anno::test::check_golden(name, "S1", w.state_hash(), update_line);
        if (anno::test::update_mode()) {
            MESSAGE("golden line: " << update_line);
        }
        INFO(name);
        CHECK(problem == "");
    }
    // The stage of the golden is compared too: a test naming another stage
    // than goldens.txt fails.
    if (!anno::test::update_requested()) {
        std::string line;
        CHECK(anno::test::check_golden("g0_step1000", "S2", w.state_hash(), line) != "");
        CHECK(anno::test::check_golden("g0_step1000", "S1", w.state_hash(), line) == "");
        CHECK(anno::test::check_golden("g0_missing", "S1", w.state_hash(), line) != "");
    }
    // The clock is where the scenario puts it, whatever the golden says.
    CHECK(w.clock().ms == 10000);
    CHECK(w.clock().last_tick == 5000);
    CHECK(w.rng().cursor() == 1234);
}
