// Stage S13 (docs/sim/state.md §5.4, §5.8, §15 S13): the growth sweep's chunk visit
// (grow_chunk, D AnnoWelt 0x1008b5f0) and the drought machine (drought_tick, D
// 0x1008aeb0). Every expected number is derived by hand from the spec (never from the
// code under test); the derivations are in the comments.
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/hash.hpp>
#include <anno/sim/island_tick.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "golden.hpp"
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

/// The MSVC rand() table of the world PRNG (docs/sim/core.md) and the k-th draw after
/// the cursor `cursor` (next() moves the cursor first, then reads).
const std::vector<std::uint16_t>& table() {
    static const std::vector<std::uint16_t> t = sim::msvc_rand_sequence(2048, 1);
    return t;
}

std::uint16_t draw_after(std::uint32_t cursor, std::uint32_t k) {
    return table()[(cursor + k) % table().size()];
}

/// One player and one island of 64 x 64 tiles at (100, 100), climate 2, no city: the
/// 300 s event never starts a drought here (it needs a city), so a test starts one with
/// start_drought_at.
sim::ScenarioSpec bare_island(std::uint32_t seed = 1234) {
    sim::ScenarioSpec spec = syn::empty_scenario(seed);
    sim::ScenarioSpec::PlayerSpec p;
    p.id = sim::PlayerId{0};
    spec.players.push_back(p);
    spec.islands.push_back(syn::plain_island(0, 100, 100, 64, 64));
    return spec;
}

/// The record after n strips of one direction (docs/sim/state.md §5.8, the timeline): n
/// / (phases · steps) rings done, then the phase and the step of the next strip.
struct StripState {
    std::int64_t rings = 0;
    std::uint32_t phase = 0, step = 0;
};

StripState after_strips(std::uint64_t n, const sim::SimConfig& c) {
    const std::uint64_t per_ring = std::uint64_t{c.drought_phases} * c.drought_steps;
    StripState s;
    s.rings = static_cast<std::int64_t>(n / per_ring);
    s.phase = static_cast<std::uint32_t>(n % per_ring / c.drought_steps);
    s.step = static_cast<std::uint32_t>(n % c.drought_steps * c.chunk_edge);
    return s;
}

/// The strip number of the record's next strip in its direction (the inverse of
/// after_strips for an expanding record).
std::uint64_t strips_done(const sim::Drought& d, const sim::SimConfig& c) {
    const std::uint64_t per_ring = std::uint64_t{c.drought_phases} * c.drought_steps;
    return static_cast<std::uint64_t>(d.ring) * per_ring +
           std::uint64_t{d.phase} * c.drought_steps + d.step / c.chunk_edge;
}

/// What the drought tiles seam saw, one entry per call.
struct StripLog {
    std::vector<sim::DroughtStrip> strips;
    std::vector<std::uint64_t> ticks;
    std::vector<std::vector<std::uint32_t>> masks;
    std::vector<std::uint16_t> stamps;
};

void install_strip_log(sim::Seams& s, StripLog* log) {
    s.drought_tiles.name = "rec_drought_tiles";
    s.drought_tiles.ctx = log;
    s.drought_tiles.fn = [](void* c, const sim::TickContext& ctx, sim::Island& is,
                            const sim::DroughtStrip& strip, sim::Effects&) {
        auto& l = *static_cast<StripLog*>(c);
        l.strips.push_back(strip);
        l.ticks.push_back(ctx.tick);
        l.masks.push_back(is.drought->city_mask);
        l.stamps.push_back(is.drought->stamp);
    };
}

sim::TileRect rect(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h) {
    return sim::TileRect{x0, y0, w, h};
}

sim::World loaded(const std::vector<std::uint8_t>& bytes, const syn::SyntheticTypes& t,
                  sim::Seams seams = {}) {
    auto r = sim::World::load(bytes, t.registry, std::move(seams));
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

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
    // A type with growth data but not of the growing class is not grown either (the
    // visit asks find_growing, not find).
    sim::TypeRegistry::Builder b;
    for (const sim::BuildingType& type : t.registry->types()) {
        b.add(type);
    }
    sim::BuildingType odd;
    odd.id = sim::TypeId{9020};
    odd.prodtype = 225;
    odd.has_prod = true;
    odd.prod.kind = sim::BuildingClass::Other;
    odd.prod.growth_period = 1000;
    odd.prod.stages = 5;
    b.add(odd);
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    syn::SyntheticTypes t2 = t;
    t2.registry = std::move(built.value());
    sim::ScenarioSpec spec2 = covered_island(8, 8, t.grass);
    spec2.islands[0].objects[0] = {0, 9020, 0x15}; // bit 6 clear: a visit at t = 500 would flip it
    sim::World w2 = make(spec2, t2);
    Tick tick2(w2, 500);
    sim::grow_chunk(tick2.ctx, island(w2), 0, 0);
    CHECK(phase_at(island(w2), 0, 0) == 0x15);
    CHECK(phase_at(island(w2), 1, 0) == kGrassOnce);
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

// ------------------------------------------------------------------- the drought

TEST_CASE("drought centre is the first market building") {
    // Through advance: seed 2, the first 300 s draw (cursor 2 + 1) is 26 500, a multiple
    // of 20; one advance of 300 000 ms reaches tick 150 000 = game second 300, the gate
    // opens, the island has climate 2 and city 0, so the wrapper starts a drought at the
    // first building of the market list of the lowest city slot that has one: city 0's
    // Kontor at (131, 123), floored by the constructor to (128, 120). Its stamp is
    // u16(150 000) = 18 928; the drought tick of the same island tick sees a difference of
    // 0 and runs no strip.
    const syn::SyntheticTypes t = growth_types();
    REQUIRE(draw_after(2, 1) == 26500);
    {
        sim::ScenarioSpec spec = syn::g5_scenario(t, 2);
        // A Kontor in city 3 too (on its rows 40 .. 43): city 0 comes first.
        spec.islands[0].cities[1].buildings = {building_test::place(t.kontor, 150, 141)};
        sim::World w = make(spec, t);
        w.advance(300000);
        REQUIRE(w.clock().tick() == 150000);
        REQUIRE(island(w).drought.has_value());
        const sim::Drought& d = *island(w).drought;
        CHECK(d.x == 128);
        CHECK(d.y == 120);
        CHECK(d.target_ring == 6); // 0x32 / 8
        CHECK(d.state == 1);
        CHECK(d.ring == 0);
        CHECK(d.step == 0);
        CHECK(d.phase == 0);
        CHECK(d.stamp == 150000 - 131072);
        CHECK(d.city_mask == std::vector<std::uint32_t>{0});
        CHECK(w.check_invariants() == "");
    }
    {
        // Without city 0's Kontor the centre is city 3's, (150, 141) -> (144, 136).
        sim::ScenarioSpec spec = syn::g5_scenario(t, 2);
        spec.islands[0].cities[0].buildings.clear();
        spec.islands[0].cities[1].buildings = {building_test::place(t.kontor, 150, 141)};
        sim::World w = make(spec, t);
        w.advance(300000);
        REQUIRE(island(w).drought.has_value());
        CHECK(island(w).drought->x == 144);
        CHECK(island(w).drought->y == 136);
    }
}

TEST_CASE("drought machine timeline") {
    // §5.8, derived by hand before the code: started at T0 = 0 and ticked every 50 ticks
    // (advances of 100 ms), strip n runs at 50 n; after n strips the record has ring n /
    // (phases · steps), phase n % (phases · steps) / steps, step (n % steps) · edge. The
    // last of the (target + 1) · phases · steps expanding strips turns ring target + 1
    // into the plateau at the target; 120 counts of 1 000 ticks; then the same number of
    // strips back from ring target to ring 0, the last one removes the record. The
    // original: 224 strips, 11 200 ticks out, 120 000 of plateau, 11 200 back, gone at
    // 142 400. Modded: drought_steps 4, drought_phases 2, chunk_edge 16 (target 50 / 16 =
    // 3: 32 strips each way); chunk_edge 4 with drought_phases 5 (target 12, the fifth
    // phase an empty strip: 13 · 40 = 520 strips each way).
    const syn::SyntheticTypes t = growth_types();
    struct Mod {
        const char* name;
        std::uint32_t steps, phases, edge;
        std::uint64_t gone;
    };
    for (const Mod& m : {Mod{"original", 8, 4, 8, 142400},
                         Mod{"steps 4 phases 2 edge 16", 4, 2, 16, 120000 + 2 * 50 * 32},
                         Mod{"edge 4 phases 5", 8, 5, 4, 120000 + 2 * 50 * 520}}) {
        INFO(m.name);
        sim::SimConfig cfg;
        cfg.drought_steps = m.steps;
        cfg.drought_phases = m.phases;
        cfg.chunk_edge = m.edge;
        sim::World w = make(bare_island(), t, {}, cfg);
        sim::start_drought_at(w.config(), island(w), 128, 128, cfg.drought_radius, 0);
        const std::int64_t target = cfg.drought_radius / cfg.chunk_edge;
        const std::uint64_t strips =
            static_cast<std::uint64_t>(target + 1) * cfg.drought_phases * cfg.drought_steps;
        const std::uint64_t expand_end = 50 * strips;
        const std::uint64_t plateau_end = expand_end + 120000;
        REQUIRE(plateau_end + 50 * strips == m.gone);
        std::uint64_t mismatches = 0;
        for (std::uint64_t tick = 50; tick <= m.gone + 1000; tick += 50) {
            w.advance(100);
            REQUIRE(w.clock().tick() == tick);
            const std::optional<sim::Drought>& dr = island(w).drought;
            if (tick >= m.gone) {
                if (dr.has_value()) {
                    ++mismatches;
                }
                continue;
            }
            REQUIRE(dr.has_value());
            std::uint8_t state = 1;
            std::int64_t ring = 0;
            std::uint32_t phase = 0, step = 0, counter = 0;
            std::uint64_t stamp = tick;
            if (tick < expand_end) {
                const StripState s = after_strips(tick / 50, cfg);
                ring = s.rings;
                phase = s.phase;
                step = s.step;
            } else if (tick < plateau_end) {
                state = 0;
                ring = target;
                counter = static_cast<std::uint32_t>((tick - expand_end) / 1000);
                stamp = expand_end + 1000 * counter;
            } else {
                state = 0xff;
                const StripState s = after_strips((tick - plateau_end) / 50, cfg);
                ring = target - s.rings;
                phase = s.phase;
                step = s.step;
            }
            const bool ok = dr->state == state && dr->ring == ring && dr->phase == phase &&
                            dr->step == step && dr->counter == counter &&
                            dr->stamp == static_cast<std::uint16_t>(stamp) &&
                            dr->target_ring == target;
            if (!ok && mismatches++ == 0) {
                // The first mismatch with its numbers.
                CHECK(tick == 0);
                CHECK(dr->state == state);
                CHECK(dr->ring == ring);
                CHECK(dr->phase == phase);
                CHECK(dr->step == step);
                CHECK(dr->counter == counter);
                CHECK(dr->stamp == static_cast<std::uint16_t>(stamp));
            }
        }
        CHECK(mismatches == 0);
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("drought gate catches up") {
    // §5.8: the gate is an `if`: one strip per drought tick at most, and the stamp moves
    // by 50, never to t. A drought started at tick 0 and an advance of 5 000 ms reach tick
    // 2 500 with one strip (stamp 50). Advances of 2 ms (one tick) then run one strip each
    // while (2 500 + k) - 50 (k + 1) >= 50, i.e. for k = 1 .. 50: 51 strips by tick 2 550,
    // stamp 2 550; tick 2 551 runs none, and from there one strip per 50 ticks: 52 strips
    // at tick 2 600, which is 2 600 / 50, caught up.
    const syn::SyntheticTypes t = growth_types();
    sim::World w = make(bare_island(), t);
    sim::start_drought_at(w.config(), island(w), 128, 128, 50, 0);
    const sim::SimConfig& c = w.config();
    w.advance(5000);
    REQUIRE(w.clock().tick() == 2500);
    CHECK(strips_done(*island(w).drought, c) == 1);
    CHECK(island(w).drought->stamp == 50);
    for (std::uint64_t k = 1; k <= 50; ++k) {
        w.advance(2);
        REQUIRE(w.clock().tick() == 2500 + k);
        CHECK(strips_done(*island(w).drought, c) == k + 1);
    }
    CHECK(island(w).drought->stamp == 2550);
    w.advance(2); // tick 2 551
    CHECK(strips_done(*island(w).drought, c) == 51);
    for (int i = 0; i < 49; ++i) {
        w.advance(2); // ticks 2 552 .. 2 600
    }
    REQUIRE(w.clock().tick() == 2600);
    CHECK(strips_done(*island(w).drought, c) == 52);
    CHECK(island(w).drought->stamp == 2600);
    // The plateau's gate catches up the same way: a record in the plateau whose stamp is
    // 5 500 ticks behind counts once per tick call until the lag is below 1 000: 5 counts.
    sim::Drought& d = *island(w).drought;
    d.state = 0;
    d.ring = 6;
    d.phase = 0;
    d.step = 0;
    d.counter = 0;
    d.stamp = static_cast<std::uint16_t>(2600 - 5500 + 65536);
    for (int i = 0; i < 8; ++i) {
        w.advance(0); // the same tick, the island tick runs again
    }
    CHECK(island(w).drought->counter == 5);
    CHECK(island(w).drought->stamp == static_cast<std::uint16_t>(2600 - 500 + 65536));
    CHECK(w.check_invariants() == "");
}

TEST_CASE("drought tiles seam called per strip") {
    // The seam (§5.8, O-15) gets every strip, once, after the stamp moved and before the
    // owner samples: 224 expanding strips at ticks 50 n, none in the plateau, 224
    // retreating strips at 131 200 + 50 m. The drought of the G5 island at (128, 120)
    // (60 x 44 tiles at (100, 100): x in [100, 160), y in [100, 144)), radius 0x32.
    const syn::SyntheticTypes t = growth_types();
    StripLog log;
    sim::Seams seams;
    install_strip_log(seams, &log);
    sim::World w = make(syn::g5_scenario(t), t, seams);
    sim::start_drought_at(w.config(), island(w), 131, 123, 50, 0);
    for (int i = 0; i < 142400 / 50 + 10; ++i) {
        w.advance(100);
    }
    CHECK_FALSE(island(w).drought.has_value());
    REQUIRE(log.strips.size() == 448);
    for (std::size_t i = 0; i < 448; ++i) {
        const std::uint64_t want = i < 224 ? 50 * (i + 1) : 131200 + 50 * (i - 223);
        if (log.ticks[i] != want) {
            CHECK(log.ticks[i] == want);
            break;
        }
        CHECK(log.stamps[i] == static_cast<std::uint16_t>(want)); // moved before the call
        CHECK(log.strips[i].expanding == (i < 224));
    }
    // Ring 0: the centre chunk for all four phases, steps 0, 8, .., 56.
    for (std::size_t i = 0; i < 32; ++i) {
        CHECK(log.strips[i].rect == rect(128, 120, 8, 8));
        CHECK(log.strips[i].ring == 0);
        CHECK(log.strips[i].phase == i / 8);
        CHECK(log.strips[i].step == i % 8 * 8);
    }
    // Ring 1 (r = 8): top [120, 136) x [112, 120); right [136, 144) x [112, 128); bottom
    // [128, 144) x [128, 136); left [120, 128) x [120, 136).
    CHECK(log.strips[32].rect == rect(120, 112, 16, 8));
    CHECK(log.strips[40].rect == rect(136, 112, 8, 16));
    CHECK(log.strips[48].rect == rect(128, 128, 16, 8));
    CHECK(log.strips[56].rect == rect(120, 120, 8, 16));
    CHECK(log.strips[56].ring == 1);
    CHECK(log.strips[56].phase == 3);
    // Ring 2 right (r = 16): [144, 152) x [104, 136).
    CHECK(log.strips[72].rect == rect(144, 104, 8, 32));
    // Ring 3 (r = 24) top: [104, 152) x [96, 104) clipped to y >= 100: 48 x 4; left:
    // [104, 112) x [104, 152) clipped to y < 144: 8 x 40.
    CHECK(log.strips[96].rect == rect(104, 100, 48, 4));
    CHECK(log.strips[120].rect == rect(104, 104, 8, 40));
    // Ring 6 (r = 48): top y [72, 80), right x [176, 184), bottom y [168, 176), left x
    // [80, 88): all outside the island, empty strips.
    for (const std::size_t i : {192U, 200U, 208U, 216U}) {
        CHECK(log.strips[i].rect == sim::TileRect{});
    }
    // Ring 5 bottom (r = 40): [96, 176) x [160, 168): outside (y >= 144), empty; ring 5
    // left: [88, 96) x [88, 168): x outside, empty; ring 4 right: [160, 168): empty.
    CHECK(log.strips[160 + 16].rect == sim::TileRect{});
    CHECK(log.strips[160 + 24].rect == sim::TileRect{});
    CHECK(log.strips[128 + 8].rect == sim::TileRect{});
    // The retreat starts at ring 6 phase 0 and ends with the centre chunk.
    CHECK(log.strips[224].ring == 6);
    CHECK(log.strips[224].phase == 0);
    CHECK(log.strips[224].step == 0);
    CHECK(log.strips[447].rect == rect(128, 120, 8, 8));
    CHECK(log.strips[447].ring == 0);
    CHECK(log.strips[447].phase == 3);
    CHECK(log.strips[447].step == 56);
    // The seam sees the mask before the strip's samples: empty at the first strip, bit 0
    // (city 0 owns (130, 122)) from the second. City 3 (rows 140 .. 143) is first sampled
    // at strip 81 (ring 2, phase 2, step 0: the bottom row [120, 152) x [136, 144), samples
    // at y = 142), so the seam sees bit 3 from the 82nd call on.
    CHECK(log.masks[0] == std::vector<std::uint32_t>{0});
    CHECK(log.masks[1] == std::vector<std::uint32_t>{1});
    CHECK(log.masks[80] == std::vector<std::uint32_t>{1});
    CHECK(log.masks[81] == std::vector<std::uint32_t>{9});
    CHECK(log.masks[447] == std::vector<std::uint32_t>{9});
    // drought_strip gives the same rectangles for a record built by hand.
    sim::Drought d;
    d.x = 128;
    d.y = 120;
    d.ring = 3;
    d.phase = 3;
    CHECK(sim::drought_strip(w.config(), island(w), d).rect == rect(104, 104, 8, 40));
    d.phase = 4; // no rectangle in the original (only with drought_phases > 4)
    CHECK(sim::drought_strip(w.config(), island(w), d).rect == sim::TileRect{});
}

TEST_CASE("expanding strip at step 0 samples four owners per anchor") {
    // §5.8 (0x1008b0c2..0x1008b195): the owners of (ax + 2, ay + 2), (ax + 2, ay + 6),
    // (ax + 6, ay + 2), (ax + 6, ay + 6) of each anchor, only when expanding and at step
    // 0. The drought at (128, 128): ring 0's anchor is (128, 128).
    const syn::SyntheticTypes t = growth_types();
    sim::ScenarioSpec spec = bare_island();
    for (std::uint32_t c = 1; c <= 4; ++c) {
        spec.islands[0].cities.push_back(syn::plain_city(c, 0));
    }
    sim::World w = make(spec, t);
    sim::Island& is = island(w);
    const auto own = [&](std::int32_t x, std::int32_t y, std::uint32_t c) {
        is.tiles.set_owner(is.tile_index(x, y), c);
    };
    own(130, 130, 1); // (ax + 2, ay + 2)
    own(134, 134, 2); // (ax + 6, ay + 6)
    own(131, 131, 3); // not a sample
    sim::start_drought_at(w.config(), is, 128, 128, 50, 0);
    w.advance(100); // strip 1: ring 0, phase 0, step 0
    CHECK(is.drought->city_mask == std::vector<std::uint32_t>{0b110});
    own(130, 134, 4); // (ax + 2, ay + 6): steps 8 .. 56 do not sample
    for (int i = 0; i < 7; ++i) {
        w.advance(100);
    }
    CHECK(is.drought->city_mask == std::vector<std::uint32_t>{0b110});
    w.advance(100); // strip 9: phase 1, step 0, the centre chunk again
    CHECK(is.drought->city_mask == std::vector<std::uint32_t>{0b10110});
    // Retreating strips do not sample (and the mask is never cleared).
    own(134, 130, 3);
    is.drought->state = 0xff;
    is.drought->phase = 0;
    is.drought->step = 0;
    w.advance(100);
    CHECK(is.drought->city_mask == std::vector<std::uint32_t>{0b10110});
    CHECK(w.check_invariants() == "");
}

TEST_CASE("drought samples outside the island are skipped (F36)") {
    // An island of 20 x 20 at (101, 101) owned by city 1. A drought centred at (103, 103)
    // is floored to (96, 96): ring 0 is the unclipped chunk [96, 104) x [96, 104) (the
    // original's anchor), whose samples (98, 98), (98, 102), (102, 98) lie outside the
    // island and are skipped (the original reads them unchecked); (102, 102) gives bit 1.
    const syn::SyntheticTypes t = growth_types();
    sim::ScenarioSpec spec = syn::empty_scenario();
    sim::ScenarioSpec::PlayerSpec p;
    p.id = sim::PlayerId{0};
    spec.players.push_back(p);
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 101, 101, 20, 20);
    is.owner_runs = {{1, 400}};
    is.cities = {syn::plain_city(1, 0)};
    spec.islands.push_back(std::move(is));
    sim::World w = make(spec, t);
    sim::start_drought_at(w.config(), island(w), 103, 103, 50, 0);
    CHECK(sim::drought_strip(w.config(), island(w), *island(w).drought).rect == rect(96, 96, 8, 8));
    w.advance(100);
    CHECK(island(w).drought->city_mask == std::vector<std::uint32_t>{0b10});
    // A centre chunk across the far edge: (120, 120) -> [120, 128), samples at 122 and 126
    // are outside the island ([101, 121)): no bit.
    sim::start_drought_at(w.config(), island(w), 120, 120, 50,
                          static_cast<std::uint32_t>(w.clock().tick()));
    w.advance(100);
    CHECK(island(w).drought->city_mask == std::vector<std::uint32_t>{0});
    CHECK(island(w).tiles.ignored_writes() == 0);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("the 300 s event sees a drought that ends in the same island tick") {
    // The island tick runs the 300 s event before the drought tick (D 0x1008b8a2 slot
    // +0x190, then 0x1008b8ab). A drought started at tick 7 600 ends at 7 600 + 142 400 =
    // 150 000, the tick of the first 300 s gate; with seed 2 that gate's draw (26 500)
    // passes, but the event still sees the record and starts nothing; the drought tick then
    // removes it. Run the other way round, a new drought would start at 150 000.
    const syn::SyntheticTypes t = growth_types();
    REQUIRE(draw_after(2, 1) == 26500);
    sim::World w = make(syn::g5_scenario(t, 2), t);
    w.advance(15200);
    REQUIRE(w.clock().tick() == 7600);
    sim::start_drought_at(w.config(), island(w), 131, 123, 50, 7600);
    std::uint32_t draws = 0;
    for (int i = 0; i < 142400 / 50; ++i) {
        w.advance(100);
        for (const sim::DrawRecord& r : w.last_output().draws) {
            draws += r.count;
        }
        if (w.clock().tick() < 150000) {
            REQUIRE(island(w).drought.has_value());
        }
    }
    REQUIRE(w.clock().tick() == 150000);
    CHECK(draws == 1); // the gate drew once
    CHECK_FALSE(island(w).drought.has_value());
    CHECK(island(w).event300_stamp == 300);
}

TEST_CASE("start_drought_at is the constructor the scripted door will use") {
    // O-51: a scripted drought calls the constructor with its own position and length, on
    // an island without cities or a market building, and the machine runs it like any
    // other. (147, 133) is floored to (144, 128); radius 20 gives target ring 2, so 3 rings
    // of 32 strips: the plateau at tick 4 800. No PRNG draw is involved.
    const syn::SyntheticTypes t = growth_types();
    sim::World w = make(bare_island(), t);
    sim::start_drought_at(w.config(), island(w), 1, 2, 8, 0);
    sim::start_drought_at(w.config(), island(w), 147, 133, 20, 0); // replaces the record
    REQUIRE(island(w).drought.has_value());
    CHECK(island(w).drought->x == 144);
    CHECK(island(w).drought->y == 128);
    CHECK(island(w).drought->target_ring == 2);
    std::uint64_t draws = 0;
    for (int i = 0; i < 96; ++i) {
        w.advance(100);
        draws += w.last_output().draws.size();
    }
    CHECK(draws == 0);
    REQUIRE(island(w).drought.has_value());
    CHECK(island(w).drought->state == 0);
    CHECK(island(w).drought->ring == 2);
    CHECK(island(w).drought->stamp == 4800);
    CHECK(w.check_invariants() == "");
    // A radius beyond the i32 ring (edge 1: 2^32 - 1 rings) is cut to 2^31 - 1, so the
    // ring can never overflow on the way out.
    sim::SimConfig wide = w.config();
    wide.chunk_edge = 1;
    sim::Island spare;
    sim::start_drought_at(wide, spare, 5, 5, 0xFFFFFFFFU, 0);
    REQUIRE(spare.drought.has_value());
    CHECK(spare.drought->target_ring == sim::kMaxDroughtRing);
    sim::start_drought_at(wide, spare, 5, 5, sim::kMaxDroughtRing, 0);
    CHECK(spare.drought->target_ring == sim::kMaxDroughtRing);
}

TEST_CASE("city mask with more than 32 city slots") {
    // O-55: with city_slots 33 and 64 the mask has 2 words. The only city on the tiles is
    // city 32 (33 slots) or 63 (64 slots), with the Kontor; city 0 has no tile and no
    // building. Seed
    // 2: the first draw 26 500 passes, the drought starts at tick 150 000 at (128, 120);
    // the first strip (tick 150 050) samples the owner of the centre chunk: bit 0 of word 1
    // for city 32, bit 31 of word 1 for city 63. The loader accepts the record.
    const syn::SyntheticTypes t = growth_types();
    for (const std::uint32_t slots : {33U, 64U}) {
        INFO("city_slots " << slots);
        sim::SimConfig cfg;
        cfg.city_slots = slots;
        // A tile owner above neutral_owner is refused by the scenario loader: the
        // neutral value moves up to the table size (the gate then scans every slot).
        cfg.neutral_owner = slots;
        const std::uint32_t c = slots - 1;
        sim::ScenarioSpec spec = syn::g5_scenario(t, 2);
        sim::ScenarioSpec::IslandSpec& is = spec.islands[0];
        is.owner_runs = {{c, 60U * 44U}};
        sim::ScenarioSpec::CitySpec rich = syn::plain_city(c, 0);
        rich.buildings = {building_test::place(t.kontor, 131, 123)};
        is.cities = {syn::plain_city(0, 0), std::move(rich)};
        sim::World w = make(spec, t, {}, cfg);
        w.advance(300000);
        REQUIRE(island(w).drought.has_value());
        CHECK(island(w).drought->city_mask == std::vector<std::uint32_t>{0, 0});
        w.advance(100);
        const std::vector<std::uint32_t> want{0, std::uint32_t{1} << (c - 32)};
        CHECK(island(w).drought->city_mask == want);
        CHECK(w.check_invariants() == "");
        const sim::World back = loaded(w.save(), t);
        CHECK(back.state_hash() == w.state_hash());
        CHECK(back.islands()[0]->drought->city_mask == want);
    }
}

TEST_CASE("drought save load and hash in each of the three states") {
    // A drought started at tick 0 on the G5 island: expanding at tick 5 000 (100 strips:
    // ring 3), the plateau at 50 000, retreating at 135 000. In each state a save loads to
    // the same hash, the record is the same, and both worlds run on to the same hash; a
    // perturbation of each state's moving field moves the hash.
    const syn::SyntheticTypes t = growth_types();
    sim::World w = make(syn::g5_scenario(t), t);
    sim::start_drought_at(w.config(), island(w), 131, 123, 50, 0);
    std::uint64_t tick = 0;
    for (const auto& [until, state] : std::vector<std::pair<std::uint64_t, std::uint8_t>>{
             {5000, 1}, {50000, 0}, {135000, 0xff}}) {
        while (tick < until) {
            w.advance(100);
            tick += 50;
        }
        INFO("tick " << tick);
        REQUIRE(island(w).drought.has_value());
        CHECK(island(w).drought->state == state);
        sim::World back = loaded(w.save(), t);
        CHECK(back.state_hash() == w.state_hash());
        REQUIRE(back.islands()[0]->drought.has_value());
        CHECK(back.islands()[0]->drought->state == state);
        CHECK(back.islands()[0]->drought->city_mask == island(w).drought->city_mask);
        CHECK(back.save() == w.save());
        sim::World ahead = w;
        for (int i = 0; i < 300; ++i) {
            ahead.advance(100);
            back.advance(100);
        }
        CHECK(back.state_hash() == ahead.state_hash());
        sim::World moved = w;
        if (state == 0) {
            island(moved).drought->counter =
                static_cast<std::uint16_t>(island(moved).drought->counter + 1U);
        } else {
            island(moved).drought->step =
                static_cast<std::uint8_t>(island(moved).drought->step ^ 8);
        }
        CHECK(moved.state_hash() != w.state_hash());
    }
}

// ------------------------------------------------------------------- G5

TEST_CASE("G5 growth and the 300 s drought through 1000000 ticks equal golden") {
    // G5 (docs/sim/state.md §14.3): synthetic::g5_scenario (seed 514) on make_growth_types,
    // 20 000 single-player advances of 100 ms (tick 1 000 000, 2 000 game seconds), a
    // checkpoint before the first and after every 1 000th advance, recorded as
    // fixtures/synthetic/sim/g5.log. Every number below is derived by hand.
    const syn::SyntheticTypes t = growth_types();
    sim::World w = make(syn::g5_scenario(t), t);
    const std::uint32_t cursor0 = w.rng().cursor();
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    rec.checkpoint();
    sim::StateHasher checkpoints;
    std::map<sim::DrawSite, std::uint32_t> draws;
    std::vector<sim::SimEvent> ui;
    std::optional<std::uint64_t> field_ripe, forest_ripe;
    std::vector<std::uint64_t> starts, ends, bit3_at;
    bool had = false;
    std::vector<std::uint32_t> last_mask;
    std::uint8_t field_phase_100k = 0xff, forest_phase_200k = 0xff;
    std::optional<sim::Drought> at305k, at350k, at440k;
    for (int i = 1; i <= 20000; ++i) {
        rec.advance(100);
        ui.insert(ui.end(), w.last_output().events.begin(), w.last_output().events.end());
        for (const sim::DrawRecord& r : w.last_output().draws) {
            draws[r.site] += r.count;
        }
        const std::uint64_t tick = w.clock().tick();
        const sim::Island& is = island(w);
        if (!field_ripe && object_at(is, 2, 3) == t.ripe_field.v) {
            field_ripe = tick;
        }
        if (!forest_ripe && object_at(is, 58, 42) == t.ripe_forest.v) {
            forest_ripe = tick;
        }
        if (tick == 100000) {
            field_phase_100k = phase_at(is, 2, 3);
        }
        if (tick == 200000) {
            forest_phase_200k = phase_at(is, 58, 42);
        }
        if (is.drought.has_value() != had) {
            (had ? ends : starts).push_back(tick);
            had = is.drought.has_value();
            last_mask.clear();
        }
        if (had) {
            if (is.drought->city_mask.size() == 1 && (is.drought->city_mask[0] & 8U) != 0 &&
                (last_mask.empty() || (last_mask[0] & 8U) == 0)) {
                bit3_at.push_back(tick);
            }
            last_mask = is.drought->city_mask;
        }
        if (tick == 305000) {
            at305k = is.drought;
        } else if (tick == 350000) {
            at350k = is.drought;
        } else if (tick == 440000) {
            at440k = is.drought;
        }
        if (i % 1000 == 0) {
            rec.checkpoint();
            checkpoints.u64(w.state_hash());
            REQUIRE(w.check_invariants() == "");
        }
    }
    REQUIRE(w.clock().tick() == 1000000);
    CHECK(w.events_dropped() == 0);
    CHECK(ui.empty()); // nothing in G5 posts a UI event (no production, no timer)

    // Growth. 48 chunks (8 x 6), d = 50 per advance: min(48 · 50 / 2000 + 1, 48) = 2
    // chunks per advance from the cursor, a full sweep every 24 advances = 1 200 ticks.
    // Advance k visits chunks 2 (k - 1) and 2 (k - 1) + 1 (mod 48): chunk 0 at ticks 50 +
    // 1 200 j, chunk 47 at 1 200 (j + 1). Both P / 2 (7 750, 12 000) exceed 1 200, so every
    // parity flip of t / (P / 2) is counted once: after a visit at tick T the counter is T
    // / (P / 2) and the parity bit its low bit, until the counter reaches S << k = 20 at a
    // multiple of 2^k = 4, which makes the object ripe.
    // The field (P / 2 = 7 750) in chunk 0: its 84th visit, at 99 650, the last before the
    // checkpoint at 100 000, leaves the counter 99 650 / 7 750 = 12, parity 0; ripe at the
    // first visit at or after 20 · 7 750 = 155 000: 50 + 1 200 · 130 = 156 050.
    CHECK(field_phase_100k == 12);
    CHECK(field_ripe == std::uint64_t{156050});
    // The forest (P / 2 = 12 000) in chunk 47: its 166th visit, at 199 200, leaves 16,
    // parity 0; ripe at 20 · 12 000 = 240 000, which is a visit (1 200 · 200).
    CHECK(forest_phase_200k == 16);
    CHECK(forest_ripe == std::uint64_t{240000});

    // The 300 s gates at game seconds 300 · k = ticks 150 000 · k, k = 1 .. 6 (the 7th, at
    // 2 100 s, is beyond the end): one draw each, the only draws of G5 (the 7.5 minute event
    // is off). Cursor 514: 21 763, 4 940, 20 851, 18 662, 13 829, 30 900; % 20: 3, 0, 11, 2,
    // 9, 0. The gate also needs climate != 0 and a city below slot 15 (city 0): droughts
    // start at the second gate (300 000) and at the sixth (900 000); the first is gone at
    // 300 000 + 142 400 = 442 400, before the third gate, so no draw meets a running one.
    CHECK(cursor0 == 514);
    CHECK(draw_after(cursor0, 1) == 21763);
    CHECK(draw_after(cursor0, 2) == 4940);
    CHECK(draw_after(cursor0, 3) == 20851);
    CHECK(draw_after(cursor0, 4) == 18662);
    CHECK(draw_after(cursor0, 5) == 13829);
    CHECK(draw_after(cursor0, 6) == 30900);
    CHECK(draws[sim::DrawSite::Event300] == 6);
    CHECK(draws.size() == 1);
    CHECK(w.rng().cursor() == cursor0 + 6);
    CHECK(starts == std::vector<std::uint64_t>{300000, 900000});
    CHECK(ends == std::vector<std::uint64_t>{442400});
    // The states at three checkpoints of the first drought (T0 = 300 000, the timeline of
    // §5.8): at 305 000, 100 strips: ring 3, phase 0, step 32, expanding; at 350 000 the
    // plateau since 311 200: 38 counts; at 440 000, 176 retreating strips since 431 200:
    // ring 6 - 5 = 1, phase 2, step 0.
    REQUIRE(at305k.has_value());
    CHECK(at305k->state == 1);
    CHECK(at305k->ring == 3);
    CHECK(at305k->phase == 0);
    CHECK(at305k->step == 32);
    REQUIRE(at350k.has_value());
    CHECK(at350k->state == 0);
    CHECK(at350k->ring == 6);
    CHECK(at350k->counter == 38);
    REQUIRE(at440k.has_value());
    CHECK(at440k->state == 0xff);
    CHECK(at440k->ring == 1);
    CHECK(at440k->phase == 2);
    CHECK(at440k->step == 0);
    // Both droughts centre on the Kontor's chunk (128, 120). City 0 is in the mask from the
    // first strip, city 3 (rows 140 .. 143) from strip 81 (ring 2, phase 2, step 0, samples
    // at y = 142): T0 + 4 050.
    CHECK(bit3_at == std::vector<std::uint64_t>{304050, 904050});
    // The end: the second drought still runs, in its plateau since 911 200 (88 counts by
    // 999 200, the 89th would be at 1 000 200), so it is not gone: it would retreat from
    // 1 031 200 and end at 1 042 400.
    REQUIRE(island(w).drought.has_value());
    const sim::Drought& d = *island(w).drought;
    CHECK(d.x == 128);
    CHECK(d.y == 120);
    CHECK(d.state == 0);
    CHECK(d.ring == 6);
    CHECK(d.counter == 88);
    CHECK(d.stamp == static_cast<std::uint16_t>(999200));
    CHECK(d.city_mask == std::vector<std::uint32_t>{9});

    // The log is byte-identical to the committed one (ANNO_UPDATE_GOLDEN=1 rewrites it).
    const sim::ReplayLog log = rec.log();
    auto written = sim::write_log(log);
    REQUIRE_MESSAGE(written.ok(), (written.ok() ? std::string() : written.error()));
    const std::filesystem::path path = std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "g5.log";
    if (anno::test::update_mode()) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(written.value().data()),
                  static_cast<std::streamsize>(written.value().size()));
        MESSAGE("wrote " << path.string());
    }
    std::ifstream in(path, std::ios::binary);
    const std::vector<std::uint8_t> committed((std::istreambuf_iterator<char>(in)),
                                              std::istreambuf_iterator<char>());
    CHECK_MESSAGE(committed == written.value(), "the recorded G5 log differs from "
                                                    << path.string()
                                                    << " (ANNO_UPDATE_GOLDEN=1 rewrites it)");
    // The committed log replays on a world rebuilt from its initial save: 21 of 21.
    auto read = sim::read_log(committed);
    REQUIRE_MESSAGE(read.ok(), (read.ok() ? std::string() : read.error()));
    sim::World replayed = loaded(read.value().initial_save, t);
    auto outcome = sim::replay(replayed, read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(outcome.value().checkpoints_total == 21);
    CHECK(outcome.value().checkpoints_matched == 21);
    CHECK(replayed.state_hash() == w.state_hash());

    // Goldens (self-baselined, goldens.txt; the asserts above do not depend on them).
    for (const auto& [name, value] : std::vector<std::pair<std::string, std::uint64_t>>{
             {"s13_g5_checkpoints", checkpoints.value()}, {"s13_g5_end", w.state_hash()}}) {
        std::string line;
        const std::string problem = anno::test::check_golden(name, "S13", value, line);
        if (anno::test::update_mode()) {
            MESSAGE("golden line: " << line);
        }
        CHECK_MESSAGE(problem.empty(), problem);
    }
}

// ------------------------------------------------------------- review B's cases

TEST_CASE("drought strip gate across the 16-bit wrap of the tick") {
    // The gate is a u16 difference (0x1008aee2 `cmp dx`): a drought started at tick 65 000
    // and ticked every 5 ticks (advances of 10 ms) runs one strip per 50 ticks across tick
    // 65 536, where the stamp wraps from 65 500 + 50 to 14: 20 strips by tick 66 000, the
    // stamp u16(66 000) = 464.
    const syn::SyntheticTypes t = growth_types();
    sim::World w = make(bare_island(), t);
    w.advance(130000);
    REQUIRE(w.clock().tick() == 65000);
    sim::start_drought_at(w.config(), island(w), 128, 128, 50, 65000);
    for (int i = 0; i < 200; ++i) {
        w.advance(10);
        const std::uint64_t tick = w.clock().tick();
        if (strips_done(*island(w).drought, w.config()) != (tick - 65000) / 50) {
            CHECK(strips_done(*island(w).drought, w.config()) == (tick - 65000) / 50);
            break;
        }
    }
    REQUIRE(w.clock().tick() == 66000);
    CHECK(strips_done(*island(w).drought, w.config()) == 20);
    CHECK(island(w).drought->stamp == 464);
}

TEST_CASE("owner samples cover every sample of every anchor of a strip") {
    // Ring 1 right of a drought at (128, 128) is [136, 144) x [120, 136): anchors (136, 120)
    // and (136, 128). City 1 owns (142, 122) = the (b, a) sample of the first anchor, city 2
    // (138, 130) = the (a, a) sample of the second, city 3 (138, 134) = the second anchor's
    // (a, b) and city 4 (142, 134) its (b, b). The strip is ring 1, phase 1, step 0: strip
    // 32 + 8 + 1 = 41 at tick 2 050. Before it the mask is empty (ring 0 and ring 1's top
    // row sample none of these tiles), after it bits 1 .. 4.
    const syn::SyntheticTypes t = growth_types();
    sim::ScenarioSpec spec = bare_island();
    for (std::uint32_t c = 1; c <= 4; ++c) {
        spec.islands[0].cities.push_back(syn::plain_city(c, 0));
    }
    sim::World w = make(spec, t);
    sim::Island& is = island(w);
    const auto own = [&](std::int32_t x, std::int32_t y, std::uint32_t c) {
        is.tiles.set_owner(is.tile_index(x, y), c);
    };
    own(142, 122, 1);
    own(138, 130, 2);
    own(138, 134, 3);
    own(142, 134, 4);
    sim::start_drought_at(w.config(), is, 128, 128, 50, 0);
    for (int i = 0; i < 40; ++i) {
        w.advance(100);
    }
    CHECK(is.drought->city_mask == std::vector<std::uint32_t>{0});
    w.advance(100);
    CHECK(strips_done(*is.drought, w.config()) == 41);
    CHECK(is.drought->city_mask == std::vector<std::uint32_t>{0b11110});
}

TEST_CASE("drought strips and samples follow chunk_edge") {
    // The rectangles and the sample offsets scale with the edge e: ring 0 is [x, x + e)^2,
    // ring 1 top [x - e, x + e) x [y - e, y), the samples at e / 4 and 3 e / 4 (1 and 3 for
    // e = 4, 4 and 12 for e = 16). A drought at (128, 128) on the 64 x 64 island at
    // (100, 100); city 1 owns (x + a, y + a), city 2 (x + b, y + b).
    const syn::SyntheticTypes t = growth_types();
    for (const std::uint32_t e : {4U, 16U}) {
        INFO("chunk_edge " << e);
        sim::SimConfig cfg;
        cfg.chunk_edge = e;
        sim::ScenarioSpec spec = bare_island();
        spec.islands[0].cities = {syn::plain_city(1, 0), syn::plain_city(2, 0)};
        StripLog log;
        sim::Seams seams;
        install_strip_log(seams, &log);
        sim::World w = make(spec, t, seams, cfg);
        sim::Island& is = island(w);
        const auto a = static_cast<std::int32_t>(e / 4);
        const auto b = static_cast<std::int32_t>(3 * e / 4);
        is.tiles.set_owner(is.tile_index(128 + a, 128 + a), 1);
        is.tiles.set_owner(is.tile_index(128 + b, 128 + b), 2);
        sim::start_drought_at(w.config(), is, 128, 128, 50, 0);
        for (int i = 0; i < 33; ++i) {
            w.advance(100);
        }
        REQUIRE(log.strips.size() == 33);
        const auto ei = static_cast<std::int32_t>(e);
        CHECK(log.strips[0].rect == rect(128, 128, e, e));
        CHECK(log.strips[0].step == 0);
        CHECK(log.strips[1].step == e);
        CHECK(log.strips[32].ring == 1);
        CHECK(log.strips[32].rect == rect(128 - ei, 128 - ei, 2 * e, e));
        CHECK(is.drought->city_mask == std::vector<std::uint32_t>{0b110});
    }
}

TEST_CASE("drought clip uses the island origin on each axis") {
    // An island of 60 x 44 at (100, 108): x in [100, 160), y in [108, 152). A drought at
    // (128, 120), ring 3 (r = 24): the top row [104, 152) x [96, 104) lies above y = 108
    // (empty); the left column [104, 112) x [104, 152) is clipped to y >= 108: 8 x 44.
    const syn::SyntheticTypes t = growth_types();
    sim::ScenarioSpec spec = syn::empty_scenario();
    spec.islands.push_back(syn::plain_island(0, 100, 108, 60, 44));
    sim::World w = make(spec, t);
    sim::Drought d;
    d.x = 128;
    d.y = 120;
    d.ring = 3;
    d.phase = 0;
    CHECK(sim::drought_strip(w.config(), island(w), d).rect == sim::TileRect{});
    d.phase = 3;
    CHECK(sim::drought_strip(w.config(), island(w), d).rect == rect(104, 108, 8, 44));
    d.phase = 1; // right column [152, 160) x [96, 144) -> y from 108: 8 x 36
    CHECK(sim::drought_strip(w.config(), island(w), d).rect == rect(152, 108, 8, 36));
}

TEST_CASE("the plateau's last count and ring 0 with a fifth phase") {
    // The plateau's last count ends the call: state 0xff, counter 0, the stamp moved by
    // 1 000, no strip in that call even when the lag would allow one (0x1008b5c5..0x1008b5d5
    // returns). And ring 0 is the centre chunk for every phase, also a fifth one
    // (drought_phases 5: strip 33 = ring 0, phase 4), while ring >= 1 has no fifth side.
    const syn::SyntheticTypes t = growth_types();
    sim::SimConfig cfg;
    cfg.drought_phases = 5;
    StripLog log;
    sim::Seams seams;
    install_strip_log(seams, &log);
    sim::World w = make(bare_island(), t, seams, cfg);
    sim::start_drought_at(w.config(), island(w), 128, 128, 50, 0);
    for (int i = 0; i < 48; ++i) {
        w.advance(100);
    }
    REQUIRE(log.strips.size() == 48);
    CHECK(log.strips[32].ring == 0);
    CHECK(log.strips[32].phase == 4);
    CHECK(log.strips[32].rect == rect(128, 128, 8, 8));
    CHECK(log.strips[47].ring == 1); // strip 48: 47 strips done, ring 1, phase 0, step 56
    CHECK(log.strips[47].phase == 0);
    CHECK(log.strips[47].step == 56);
    // The plateau's last count with a lag of 1 100 ticks.
    sim::Drought& d = *island(w).drought;
    d.state = 0;
    d.ring = 6;
    d.phase = 0;
    d.step = 0;
    d.counter = 119;
    const auto now = static_cast<std::uint16_t>(w.clock().tick());
    d.stamp = static_cast<std::uint16_t>(now - 1100);
    const std::size_t calls = log.strips.size();
    w.advance(0);
    CHECK(island(w).drought->state == 0xff);
    CHECK(island(w).drought->counter == 0);
    CHECK(island(w).drought->stamp == static_cast<std::uint16_t>(now - 100));
    CHECK(island(w).drought->step == 0);
    CHECK(log.strips.size() == calls);
    // The next call runs the first retreating strip (lag 100 >= 50).
    w.advance(0);
    CHECK(log.strips.size() == calls + 1);
    CHECK_FALSE(log.strips.back().expanding);
}

TEST_CASE("a drought tiles seam that removes or edits the record") {
    // The seam may write the island; one that removes the record ends the strip (no
    // samples, no step advance on a gone record), one that writes the step is followed by
    // the machine from the seam's value.
    const syn::SyntheticTypes t = growth_types();
    struct Ctl {
        int calls = 0;
        int remove_at = -1;
    } ctl;
    sim::Seams seams;
    seams.drought_tiles.name = "test_drought_edit";
    seams.drought_tiles.ctx = &ctl;
    seams.drought_tiles.fn = [](void* c, const sim::TickContext&, sim::Island& is,
                                const sim::DroughtStrip&, sim::Effects&) {
        auto& k = *static_cast<Ctl*>(c);
        if (k.calls++ == k.remove_at) {
            is.drought.reset();
        } else {
            is.drought->step = 48; // skip ahead to the last step group of the phase
        }
    };
    ctl.remove_at = 2;
    sim::World w = make(bare_island(), t, seams);
    sim::start_drought_at(w.config(), island(w), 128, 128, 50, 0);
    w.advance(100); // strip 1: the seam sets step 48, the machine advances it to 56
    CHECK(island(w).drought->step == 56);
    w.advance(100); // strip 2: step 48 -> 56 again
    CHECK(island(w).drought->step == 56);
    w.advance(100); // strip 3: the seam removes the record
    CHECK_FALSE(island(w).drought.has_value());
    w.advance(100);
    CHECK(ctl.calls == 3);
    CHECK(w.check_invariants() == "");
}
