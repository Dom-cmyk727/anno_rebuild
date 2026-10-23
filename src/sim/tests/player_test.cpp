// The player table (docs/sim/state.md §4.5, §5.10, §9.3; stage S4): the visit
// counts, the upkeep booking, the bankruptcy and defeat passes, the read-only
// view, and the players scope of the state stream.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/world.hpp>

#include "golden.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;

namespace {

/// A little-endian byte stream written by hand: the expected players scope is
/// transcribed from docs/sim/state.md §9.3 independently of the field list.
struct Stream {
    std::vector<std::uint8_t> v;
    void u8(std::uint32_t x) { v.push_back(static_cast<std::uint8_t>(x)); }
    void u16(std::uint32_t x) {
        u8(x);
        u8(x >> 8);
    }
    void u32(std::uint32_t x) {
        u16(x);
        u16(x >> 16);
    }
    void u64(std::uint64_t x) {
        u32(static_cast<std::uint32_t>(x));
        u32(static_cast<std::uint32_t>(x >> 32));
    }
    void i64(std::int64_t x) { u64(static_cast<std::uint64_t>(x)); }
    void boolean(bool b) { u8(b ? 1U : 0U); }
};

bool starts_with(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

/// A table with a player in every slot (money 10000, all defaults).
sim::PlayerTable full_table(const sim::SimConfig& cfg = {}) {
    sim::PlayerTable t = sim::make_player_table(cfg);
    for (std::uint32_t i = 0; i < t.slots.size(); ++i) {
        t.slots[i] = sim::make_player(sim::PlayerId{i}, cfg);
    }
    return t;
}

/// Records the calls of the two per-player hooks, in order, with what the
/// player looked like at that moment.
struct HookLog {
    struct Call {
        char which = 0; // 'g' groups, 'd' defeat
        std::uint32_t player = 0;
        std::int64_t money = 0;
        std::uint32_t deadline = 0;
        bool defeat_pending = false;
        sim::PlayerState state = sim::PlayerState::Alive;
        std::uint16_t upkeep_stamp = 0;
    };
    std::vector<Call> calls;
    std::size_t count(char which) const {
        return static_cast<std::size_t>(std::count_if(
            calls.begin(), calls.end(), [&](const Call& c) { return c.which == which; }));
    }
    std::vector<std::uint32_t> players(char which) const {
        std::vector<std::uint32_t> out;
        for (const Call& c : calls) {
            if (c.which == which) {
                out.push_back(c.player);
            }
        }
        return out;
    }
};

void record_call(void* ctx, sim::Player& p, char which) {
    auto* log = static_cast<HookLog*>(ctx);
    log->calls.push_back(HookLog::Call{which, p.index.v, p.money, p.bankrupt_deadline,
                                       p.defeat_pending, p.state, p.upkeep_stamp});
}
void record_groups(void* ctx, sim::Player& p) {
    record_call(ctx, p, 'g');
}
void record_defeat(void* ctx, sim::Player& p) {
    record_call(ctx, p, 'd');
}

sim::PlayerHooks hooks_for(HookLog& log) {
    sim::PlayerHooks h;
    h.groups = &record_groups;
    h.groups_ctx = &log;
    h.defeat = &record_defeat;
    h.defeat_ctx = &log;
    return h;
}

sim::Player& at(sim::PlayerTable& t, std::uint32_t slot) {
    return *t.slots[slot];
}

} // namespace

TEST_CASE("player defaults and the table constructors") {
    const sim::SimConfig cfg;
    const sim::Player p = sim::make_player(sim::PlayerId{4}, cfg);
    CHECK(p.index == sim::PlayerId{4});
    CHECK(p.kind == 8);
    CHECK(p.volk == 1);
    CHECK(p.money == 10000);
    CHECK(p.upkeep_a == 0);
    CHECK(p.upkeep_b == 0);
    CHECK(p.upkeep_rem == 0);
    CHECK(p.upkeep_stamp == 0);
    CHECK(p.stat_a == 0);
    CHECK(p.stat_b == 0);
    CHECK(p.flags == 1);
    CHECK(p.flags_a0 == 0);
    // D: the constructor 0x10021600 stores 0 at +0xa8 and 0xff at +0xa9.
    CHECK(p.colour_slot == 0);
    CHECK(p.default_colour == sim::kNoColour);
    CHECK(sim::kNoColour == 0xFFFFFFFFU);
    CHECK(p.state == sim::PlayerState::Alive);
    CHECK(p.bankrupt_deadline == 0);
    CHECK_FALSE(p.defeat_pending);
    CHECK_FALSE(p.free_build);
    CHECK(p.name.empty());
    CHECK(p.research.state == std::vector<std::uint8_t>(65, 0));
    CHECK(p.research.progress == std::vector<std::uint16_t>(65, 0));
    // A default-constructed Player has no valid index (never aliases slot 0).
    CHECK_FALSE(sim::Player{}.index.valid());

    sim::SimConfig small;
    small.player_slots = 4;
    small.research_entries = 3;
    const sim::PlayerTable t = sim::make_player_table(small);
    CHECK(t.slots.size() == 4);
    CHECK(
        std::none_of(t.slots.begin(), t.slots.end(), [](const auto& s) { return s.has_value(); }));
    CHECK(t.stamp == 0);
    CHECK(t.cursor == 0);
    CHECK(sim::make_player(sim::PlayerId{1}, small).research.state.size() == 3);
    CHECK(sim::make_player(sim::PlayerId{1}, small).research.progress.size() == 3);
    CHECK(sim::make_player_table(cfg).slots.size() == 32);

    // The bare world holds the table of its configuration.
    const sim::World w;
    CHECK(w.players().slots.size() == 32);
    CHECK(w.players().stamp == 0);
    CHECK(w.players().cursor == 0);
    CHECK(sim::validate_players(w.players(), w.config()).empty());
}

TEST_CASE("research state researched needs bit 1 and a known id") {
    sim::ResearchState r;
    r.state = {0, 1, 2, 3};
    CHECK_FALSE(r.researched(0));
    CHECK_FALSE(r.researched(1)); // researchable only
    CHECK(r.researched(2));
    CHECK(r.researched(3));
    CHECK_FALSE(r.researched(4)); // beyond the table
    CHECK_FALSE(r.researched(0xFFFFFFFFU));
}

TEST_CASE("player table find is total") {
    sim::PlayerTable t = sim::make_player_table(sim::SimConfig{});
    t.slots[3] = sim::make_player(sim::PlayerId{3}, sim::SimConfig{});
    const sim::PlayerTable& ct = t;
    CHECK(ct.find(sim::PlayerId{3}) == &*t.slots[3]);
    CHECK(t.find(sim::PlayerId{3}) == &*t.slots[3]);
    CHECK(ct.find(sim::PlayerId{2}) == nullptr);  // empty slot
    CHECK(ct.find(sim::PlayerId{32}) == nullptr); // beyond the table
    CHECK(ct.find(sim::PlayerId{}) == nullptr);   // invalid id
    CHECK(t.find(sim::PlayerId{2}) == nullptr);
    CHECK(t.find(sim::PlayerId{32}) == nullptr);
    CHECK(t.find(sim::PlayerId{}) == nullptr);
}

TEST_CASE("player table visit counts for 5 10 20 ms and d 49") {
    // min(d*32/50 + 1, 32), the literal values of docs/sim/state.md §5.10: d is
    // the tick difference to the table's stamp (a 5, 10, 20 ms multiplayer step is
    // 2-3, 5, 10 ticks).
    const struct {
        std::uint32_t d;
        std::uint32_t expected;
    } rows[] = {{0, 1},   {1, 1},   {2, 2},   {3, 2},   {5, 4},     {10, 7},
                {25, 17}, {48, 31}, {49, 32}, {50, 32}, {1000, 32}, {65535, 32}};
    for (const auto& row : rows) {
        INFO("d " << row.d);
        sim::PlayerTable t = full_table();
        HookLog log;
        t.stamp = 0;
        sim::player_table_tick(t, row.d, sim::SimConfig{}, sim::Rules{}, hooks_for(log));
        CHECK(log.count('g') == row.expected);
        CHECK(t.cursor == row.expected % 32);
        CHECK(t.stamp == static_cast<std::uint16_t>(row.d));
        // They are slots 0 .. count-1 in order.
        std::vector<std::uint32_t> want;
        for (std::uint32_t i = 0; i < row.expected; ++i) {
            want.push_back(i);
        }
        CHECK(log.players('g') == want);
    }
    // d is the 16-bit difference: the stamp wraps like the original's word.
    sim::PlayerTable t = full_table();
    HookLog log;
    t.stamp = 65530;
    sim::player_table_tick(t, 65536 + 4, sim::SimConfig{}, sim::Rules{}, hooks_for(log));
    CHECK(log.count('g') == 7); // d = 10
    CHECK(t.stamp == 4);
}

TEST_CASE("player table visits follow the clock for the multiplayer steps") {
    // 5 ms steps are 2 or 3 ticks: 2 players per advance; 10 ms: 4; 20 ms: 7
    // (docs/sim/state.md §5.10). The cursor counts the visits.
    struct Row {
        std::uint32_t step_ms;
        std::vector<std::uint32_t> cursors;
        std::uint16_t stamp;
    };
    const Row rows[] = {{5, {2, 4, 6, 8}, 10}, {10, {4, 8, 12, 16}, 20}, {20, {7, 14, 21, 28}, 40}};
    for (const Row& row : rows) {
        INFO("step " << row.step_ms << " ms");
        sim::World w;
        sim::PlayerTable& table = sim::WorldTestAccess::players(w);
        table = full_table(w.config());
        for (const std::uint32_t cursor : row.cursors) {
            w.advance(row.step_ms);
            CHECK(w.players().cursor == cursor);
        }
        CHECK(w.players().stamp == row.stamp);
    }
    // 7 per advance wraps the 32 slots: 28 + 7 = 35 -> 3.
    sim::World w;
    sim::WorldTestAccess::players(w) = full_table(w.config());
    for (int i = 0; i < 5; ++i) {
        w.advance(20);
    }
    CHECK(w.players().cursor == 3);
}

TEST_CASE("player table cursor wraps and skips empty slots") {
    sim::PlayerTable t = sim::make_player_table(sim::SimConfig{});
    for (const std::uint32_t slot : {0U, 2U, 30U, 31U}) {
        t.slots[slot] = sim::make_player(sim::PlayerId{slot}, sim::SimConfig{});
    }
    t.cursor = 30;
    t.stamp = 0;
    HookLog log;
    sim::player_table_tick(t, 10, sim::SimConfig{}, sim::Rules{}, hooks_for(log)); // 7 visits
    // 30, 31, 0, 1, 2, 3, 4: the empty slots 1, 3 and 4 are passed over.
    CHECK(log.players('g') == std::vector<std::uint32_t>{30, 31, 0, 2});
    CHECK(t.cursor == 5);
}

TEST_CASE("player table full tick rule visits every slot once") {
    sim::Rules full;
    full.player_table_full_tick = true;
    sim::PlayerTable t = full_table();
    t.cursor = 9;
    HookLog log;
    sim::player_table_tick(t, 0, sim::SimConfig{}, full, hooks_for(log)); // d = 0
    CHECK(log.count('g') == 32);
    // From the cursor round the table once, the cursor ends where it started.
    std::vector<std::uint32_t> want;
    for (std::uint32_t i = 0; i < 32; ++i) {
        want.push_back((9 + i) % 32);
    }
    CHECK(log.players('g') == want);
    CHECK(t.cursor == 9);
    // The default rule visits one slot at d = 0.
    sim::PlayerTable d = full_table();
    HookLog once;
    sim::player_table_tick(d, 0, sim::SimConfig{}, sim::Rules{}, hooks_for(once));
    CHECK(once.count('g') == 1);
    CHECK(d.cursor == 1);
}

TEST_CASE("player table with a modded divisor and slot count") {
    sim::SimConfig cfg;
    cfg.player_slots = 8;
    cfg.player_div = 4;
    sim::PlayerTable t = full_table(cfg);
    HookLog log;
    // d = 5: min(5*8/4 + 1, 8) = 8 visits; d = 1: 1*8/4 + 1 = 3.
    sim::player_table_tick(t, 1, cfg, sim::Rules{}, hooks_for(log));
    CHECK(log.count('g') == 3);
    CHECK(t.cursor == 3);
    sim::player_table_tick(t, 6, cfg, sim::Rules{}, hooks_for(log)); // d = 5
    CHECK(log.count('g') == 3 + 8);
    CHECK(t.cursor == 3);
    // The count is clamped to the slot count: a divisor of 1 asks for more.
    cfg.player_div = 1;
    sim::PlayerTable u = full_table(cfg);
    HookLog all;
    sim::player_table_tick(u, 6, cfg, sim::Rules{}, hooks_for(all));
    CHECK(all.count('g') == 8);
    CHECK(u.cursor == 0);
}

TEST_CASE("player table with an empty table or a stray cursor visits nothing") {
    sim::SimConfig cfg;
    HookLog log;
    sim::PlayerTable empty;
    empty.stamp = 7;
    sim::player_table_tick(empty, 100, cfg, sim::Rules{}, hooks_for(log));
    CHECK(empty.stamp == 100); // the stamp is still written (the original's last step)
    CHECK(empty.cursor == 0);
    CHECK(log.calls.empty());
    // A cursor at or beyond the table is an inconsistent state: nothing is visited
    // and the cursor stays visible to validate_players.
    sim::PlayerTable stray = full_table(cfg);
    stray.cursor = 32;
    sim::player_table_tick(stray, 100, cfg, sim::Rules{}, hooks_for(log));
    CHECK(log.calls.empty());
    CHECK(stray.cursor == 32);
    CHECK(stray.stamp == 100);
    CHECK(starts_with(sim::validate_players(stray, cfg), "players.cursor"));
    stray.cursor = 31; // the last slot is fine
    sim::player_table_tick(stray, 300, cfg, sim::Rules{}, hooks_for(log));
    CHECK(log.count('g') > 0);
}

TEST_CASE("player upkeep gate opens at the period and compares 16 bits") {
    const sim::SimConfig cfg;
    struct Row {
        std::uint16_t stamp;
        std::uint32_t t;
        bool books;
    };
    const Row rows[] = {{0, 1999, false},
                        {0, 2000, true},
                        {0, 2001, true},
                        {100, 2099, false},
                        {100, 2100, true},
                        // the stamp wraps: 65000 + 2000 = 67000 > 65535
                        {65000, 66999, false},
                        {65000, 67000, true},
                        // t - stamp is 66536 in u32 but 1000 in 16 bits: closed; the
                        // 16-bit difference of the next row is 2000: open
                        {100, 66636, false},
                        {100, 67636, true},
                        // 65536 + 5: the difference is 5 in 16 bits
                        {0, 65541, false},
                        {5, 67541, true}};
    for (const Row& row : rows) {
        INFO("stamp " << row.stamp << " t " << row.t);
        sim::Player p = sim::make_player(sim::PlayerId{0}, cfg);
        p.upkeep_stamp = row.stamp;
        p.upkeep_a = 30;
        sim::player_tick(p, row.t, cfg, sim::Rules{});
        CHECK((p.money == 10000 - 2) == row.books);
        CHECK(p.upkeep_stamp == (row.books ? static_cast<std::uint16_t>(row.t) : row.stamp));
        // The rule is the 16-bit difference, not the 32-bit one (D: 0x1001b848
        // compares ax).
        CHECK(row.books == (static_cast<std::uint16_t>(row.t - row.stamp) >= 2000));
    }
    // Another period.
    sim::SimConfig fast;
    fast.upkeep_period = 100;
    sim::Player p = sim::make_player(sim::PlayerId{0}, fast);
    p.upkeep_a = 30;
    sim::player_tick(p, 99, fast, sim::Rules{});
    CHECK(p.money == 10000);
    sim::player_tick(p, 100, fast, sim::Rules{});
    CHECK(p.money == 9998);
}

TEST_CASE("player upkeep books the sum over 15 and keeps the remainder") {
    const sim::SimConfig cfg;
    sim::Player p = sim::make_player(sim::PlayerId{0}, cfg);
    // All three terms count: 100 + 50 + 7 = 157 = 10 * 15 + 7.
    p.upkeep_a = 100;
    p.upkeep_b = 50;
    p.upkeep_rem = 7;
    sim::player_tick(p, 2000, cfg, sim::Rules{});
    CHECK(p.money == 10000 - 10);
    CHECK(p.upkeep_rem == 7);
    // The remainder is carried: 14 per period gives 0, 1, 1, 2, ... charges.
    sim::Player q = sim::make_player(sim::PlayerId{0}, cfg);
    q.upkeep_a = 14;
    std::vector<std::int64_t> charged;
    std::vector<std::uint32_t> rems;
    std::uint32_t t = 0;
    for (int i = 0; i < 5; ++i) {
        t += 2000;
        const std::int64_t before = q.money;
        sim::player_tick(q, t, cfg, sim::Rules{});
        charged.push_back(before - q.money);
        rems.push_back(q.upkeep_rem);
    }
    // sums 14, 28, 27, 26, 25 -> 0 r14, 1 r13, 1 r12, 1 r11, 1 r10.
    CHECK(charged == std::vector<std::int64_t>{0, 1, 1, 1, 1});
    CHECK(rems == std::vector<std::uint32_t>{14, 13, 12, 11, 10});
    // upkeep_a and upkeep_b are interchangeable terms.
    sim::Player r = sim::make_player(sim::PlayerId{0}, cfg);
    r.upkeep_b = 90;
    sim::player_tick(r, 2000, cfg, sim::Rules{});
    CHECK(r.money == 10000 - 6);
    // The sum wraps in u32: 0xFFFFFFFF + 2 = 1.
    sim::Player w = sim::make_player(sim::PlayerId{0}, cfg);
    w.upkeep_a = 0xFFFFFFFFU;
    w.upkeep_b = 2;
    sim::player_tick(w, 2000, cfg, sim::Rules{});
    CHECK(w.money == 10000);
    CHECK(w.upkeep_rem == 1);
    // A balance may go below the original's i32 range (money is i64, F15).
    sim::Player big = sim::make_player(sim::PlayerId{0}, cfg);
    big.money = -3000000000LL;
    big.upkeep_a = 0xFFFFFFF0U;
    sim::player_tick(big, 2000, cfg, sim::Rules{});
    CHECK(big.money == -3000000000LL - 286331152); // 0xFFFFFFF0 / 15, no remainder
    CHECK(big.upkeep_rem == 0);
    // Another slice count.
    sim::SimConfig other;
    other.upkeep_slices = 4;
    sim::Player s = sim::make_player(sim::PlayerId{0}, other);
    s.upkeep_a = 23;
    sim::player_tick(s, 2000, other, sim::Rules{});
    CHECK(s.money == 10000 - 5);
    CHECK(s.upkeep_rem == 3);
}

TEST_CASE("player upkeep stamp becomes the tick not the old stamp plus the period") {
    const sim::SimConfig cfg;
    sim::Player p = sim::make_player(sim::PlayerId{0}, cfg);
    p.upkeep_a = 15;
    sim::player_tick(p, 2750, cfg, sim::Rules{}); // d = 2750, late
    CHECK(p.upkeep_stamp == 2750);
    CHECK(p.money == 9999);
    sim::player_tick(p, 4749, cfg, sim::Rules{}); // d = 1999 from 2750: closed
    CHECK(p.money == 9999);
    sim::player_tick(p, 4750, cfg, sim::Rules{}); // d = 2000: opens
    CHECK(p.money == 9998);
    CHECK(p.upkeep_stamp == 4750);
    // Past 16 bits the stamp is the low 16 bits of the tick.
    sim::player_tick(p, 70000, cfg, sim::Rules{});
    CHECK(p.upkeep_stamp == 70000 - 65536);
}

TEST_CASE("player statistics decay by 104 over 105 in u32 once per upkeep period") {
    const sim::SimConfig cfg;
    sim::Player p = sim::make_player(sim::PlayerId{0}, cfg);
    p.stat_a = 1050;
    p.stat_b = 210;
    sim::player_tick(p, 1999, cfg, sim::Rules{}); // gate closed: untouched
    CHECK(p.stat_a == 1050);
    CHECK(p.stat_b == 210);
    sim::player_tick(p, 2000, cfg, sim::Rules{});
    CHECK(p.stat_a == 1040);
    CHECK(p.stat_b == 208);
    // Truncating division: 100 * 104 / 105 = 99.
    p.stat_a = 100;
    p.stat_b = 1;
    sim::player_tick(p, 4000, cfg, sim::Rules{});
    CHECK(p.stat_a == 99);
    CHECK(p.stat_b == 0);
    // The product wraps in u32 like the original's register (docs/sim/state.md
    // §1.1): 50 000 000 * 104 = 5 200 000 000 -> 905 032 704, / 105 = 8 619 359.
    p.stat_a = 50000000;
    p.stat_b = 50000000;
    sim::player_tick(p, 6000, cfg, sim::Rules{});
    CHECK(p.stat_a == 8619359);
    CHECK(p.stat_b == 8619359);
    // Another ratio.
    sim::SimConfig other;
    other.stat_decay_num = 1;
    other.stat_decay_den = 2;
    sim::Player q = sim::make_player(sim::PlayerId{0}, other);
    q.stat_a = 9;
    q.stat_b = 20;
    sim::player_tick(q, 2000, other, sim::Rules{});
    CHECK(q.stat_a == 4);
    CHECK(q.stat_b == 10);
}

TEST_CASE("rules stat_decay_wide computes the decay in 64 bits and caps it") {
    const sim::SimConfig cfg;
    sim::Rules wide;
    wide.stat_decay_wide = true;
    sim::Player p = sim::make_player(sim::PlayerId{0}, cfg);
    p.stat_a = 50000000;
    p.stat_b = 1050;
    sim::player_tick(p, 2000, cfg, wide);
    CHECK(p.stat_a == 49523809); // 50 000 000 * 104 / 105, no wrap
    CHECK(p.stat_b == 1040);     // the same as the u32 rule below the wrap point
    // The faithful default wraps (the existing 50 000 000 case: 8 619 359).
    sim::Player q = sim::make_player(sim::PlayerId{0}, cfg);
    q.stat_a = 50000000;
    sim::player_tick(q, 2000, cfg, sim::Rules{});
    CHECK(q.stat_a == 8619359);
    // The last value that does not wrap in u32 is the same under both rules.
    sim::Player edge = sim::make_player(sim::PlayerId{0}, cfg);
    edge.stat_a = 41297762; // 41 297 762 * 104 = 4 294 967 248 < 2^32
    sim::Player edge_wide = edge;
    sim::player_tick(edge, 2000, cfg, sim::Rules{});
    sim::player_tick(edge_wide, 2000, cfg, wide);
    CHECK(edge.stat_a == edge_wide.stat_a);
    CHECK(edge.stat_a == 40904449);
    // The gate and the order are unchanged: nothing happens before the period.
    sim::Player closed = sim::make_player(sim::PlayerId{0}, cfg);
    closed.stat_a = 50000000;
    sim::player_tick(closed, 1999, cfg, wide);
    CHECK(closed.stat_a == 50000000);
    // A ratio above one is capped at 0xFFFFFFFF instead of wrapping.
    sim::SimConfig grow;
    grow.stat_decay_num = 3;
    grow.stat_decay_den = 1;
    sim::Player g = sim::make_player(sim::PlayerId{0}, grow);
    g.stat_a = 0xFFFFFFFFU;
    g.stat_b = 1000;
    sim::player_tick(g, 2000, grow, wide);
    CHECK(g.stat_a == 0xFFFFFFFFU);
    CHECK(g.stat_b == 3000);
    // A zero denominator keeps the statistics under the rule as well.
    sim::SimConfig zero;
    zero.stat_decay_den = 0;
    sim::Player z = sim::make_player(sim::PlayerId{0}, zero);
    z.stat_a = 77;
    sim::player_tick(z, 2000, zero, wide);
    CHECK(z.stat_a == 77);
}

TEST_CASE("rules bankrupt_deadline_wrap_safe keeps the grace period across the u32 wrap") {
    const sim::SimConfig cfg;
    sim::Rules safe;
    safe.bankrupt_deadline_wrap_safe = true;
    // Far from the wrap it behaves exactly like the original.
    sim::Player p = sim::make_player(sim::PlayerId{0}, cfg);
    p.money = -5000;
    sim::player_tick(p, 100, cfg, safe);
    CHECK(p.bankrupt_deadline == 150100);
    sim::player_tick(p, 150100, cfg, safe); // t == deadline: not yet
    CHECK(p.state == sim::PlayerState::Alive);
    sim::player_tick(p, 150101, cfg, safe);
    CHECK(p.state == sim::PlayerState::Bankrupt);

    // Across the wrap: the original declares the player bankrupt at the next
    // visit, the safe rule waits the whole period.
    sim::Player w = sim::make_player(sim::PlayerId{0}, cfg);
    w.money = -4000;
    sim::player_tick(w, 0xFFFF0000U, cfg, safe);
    CHECK(w.bankrupt_deadline == 84464);
    sim::player_tick(w, 0xFFFF0001U, cfg, safe);
    CHECK(w.state == sim::PlayerState::Alive);
    sim::player_tick(w, 0xFFFFFFFFU, cfg, safe);
    CHECK(w.state == sim::PlayerState::Alive);
    sim::player_tick(w, 0, cfg, safe); // the tick wrapped
    CHECK(w.state == sim::PlayerState::Alive);
    sim::player_tick(w, 84464, cfg, safe); // t == deadline
    CHECK(w.state == sim::PlayerState::Alive);
    sim::player_tick(w, 84465, cfg, safe);
    CHECK(w.state == sim::PlayerState::Bankrupt);

    // A deadline that would be exactly 2^32 is stored as 1, never as "not running".
    sim::Player z = sim::make_player(sim::PlayerId{0}, cfg);
    z.money = -4000;
    sim::player_tick(z, 0xFFFDB610U, cfg, safe);
    CHECK(z.bankrupt_deadline == 1);
    sim::player_tick(z, 0xFFFDB610U, cfg, safe); // the same tick again: not over
    CHECK(z.state == sim::PlayerState::Alive);
    CHECK(z.bankrupt_deadline == 1);
    sim::player_tick(z, 0xFFFFFFFFU, cfg, safe);
    CHECK(z.state == sim::PlayerState::Alive);
    sim::player_tick(z, 1, cfg, safe); // t == deadline
    CHECK(z.state == sim::PlayerState::Alive);
    sim::player_tick(z, 2, cfg, safe);
    CHECK(z.state == sim::PlayerState::Bankrupt);

    // The recovery still clears the deadline, and the option moves nothing else.
    sim::Player r = sim::make_player(sim::PlayerId{0}, cfg);
    r.money = -4000;
    sim::player_tick(r, 100, cfg, safe);
    r.money = -2999;
    sim::player_tick(r, 200, cfg, safe);
    CHECK(r.bankrupt_deadline == 0);
}

TEST_CASE("player upkeep is booked for the neutral and the bankrupt player too") {
    const sim::SimConfig cfg;
    sim::Player neutral = sim::make_player(sim::PlayerId{31}, cfg);
    neutral.upkeep_a = 150;
    sim::player_tick(neutral, 2000, cfg, sim::Rules{});
    CHECK(neutral.money == 10000 - 10);
    sim::Player bankrupt = sim::make_player(sim::PlayerId{2}, cfg);
    bankrupt.state = sim::PlayerState::Bankrupt;
    bankrupt.upkeep_a = 150;
    sim::player_tick(bankrupt, 2000, cfg, sim::Rules{});
    CHECK(bankrupt.money == 10000 - 10);
}

TEST_CASE("player upkeep saturates and zero divisors do not trap") {
    const sim::SimConfig cfg;
    const std::int64_t lowest = std::numeric_limits<std::int64_t>::min();
    sim::Player p = sim::make_player(sim::PlayerId{0}, cfg);
    p.money = lowest + 5;
    p.upkeep_a = 150; // 10 per period
    sim::player_tick(p, 2000, cfg, sim::Rules{});
    CHECK(p.money == lowest);
    sim::player_tick(p, 4000, cfg, sim::Rules{});
    CHECK(p.money == lowest);
    // Exactly the room left: no clamp needed.
    sim::Player q = sim::make_player(sim::PlayerId{0}, cfg);
    q.money = lowest + 10;
    q.upkeep_a = 150;
    sim::player_tick(q, 2000, cfg, sim::Rules{});
    CHECK(q.money == lowest);
    // A zero slice count or decay denominator (SimConfig::validate rejects both)
    // is defined: no charge, the remainder and the statistics are kept.
    sim::SimConfig zero = cfg;
    zero.upkeep_slices = 0;
    zero.stat_decay_den = 0;
    sim::Player z = sim::make_player(sim::PlayerId{0}, zero);
    z.upkeep_a = 150;
    z.upkeep_rem = 4;
    z.stat_a = 1050;
    z.stat_b = 7;
    sim::player_tick(z, 2000, zero, sim::Rules{});
    CHECK(z.money == 10000);
    CHECK(z.upkeep_rem == 4);
    CHECK(z.stat_a == 1050);
    CHECK(z.stat_b == 7);
    CHECK(z.upkeep_stamp == 2000);
}

TEST_CASE("player bankruptcy starts the grace period at the limit and ends it after") {
    const sim::SimConfig cfg;
    // -3000 is bankrupt ("<="), -2999 is not.
    sim::Player at_limit = sim::make_player(sim::PlayerId{0}, cfg);
    at_limit.money = -3000;
    sim::player_tick(at_limit, 100, cfg, sim::Rules{});
    CHECK(at_limit.bankrupt_deadline == 150100);
    CHECK(at_limit.state == sim::PlayerState::Alive);
    sim::Player above = sim::make_player(sim::PlayerId{0}, cfg);
    above.money = -2999;
    sim::player_tick(above, 100, cfg, sim::Rules{});
    CHECK(above.bankrupt_deadline == 0);

    // The deadline is unsigned t + 150000, and it ends strictly after it.
    sim::Player p = sim::make_player(sim::PlayerId{0}, cfg);
    p.money = -5000;
    sim::player_tick(p, 100, cfg, sim::Rules{});
    CHECK(p.bankrupt_deadline == 150100);
    sim::player_tick(p, 150099, cfg, sim::Rules{});
    CHECK(p.state == sim::PlayerState::Alive);
    sim::player_tick(p, 150100, cfg, sim::Rules{}); // t == deadline: not yet
    CHECK(p.state == sim::PlayerState::Alive);
    CHECK(p.bankrupt_deadline == 150100);
    sim::player_tick(p, 150101, cfg, sim::Rules{});
    CHECK(p.state == sim::PlayerState::Bankrupt);
    CHECK(p.bankrupt_deadline == 150100); // kept
    // Bankrupt is final: recovering money changes nothing.
    p.money = 100000;
    sim::player_tick(p, 150200, cfg, sim::Rules{});
    CHECK(p.state == sim::PlayerState::Bankrupt);
    CHECK(p.bankrupt_deadline == 150100);

    // Recovering above the limit during the grace period clears the deadline.
    sim::Player r = sim::make_player(sim::PlayerId{0}, cfg);
    r.money = -4000;
    sim::player_tick(r, 100, cfg, sim::Rules{});
    CHECK(r.bankrupt_deadline == 150100);
    r.money = -2999;
    sim::player_tick(r, 200, cfg, sim::Rules{});
    CHECK(r.bankrupt_deadline == 0);
    r.money = -4000;
    sim::player_tick(r, 300, cfg, sim::Rules{}); // a fresh period from 300
    CHECK(r.bankrupt_deadline == 150300);

    // The deadline is a u32: t + grace wraps like the original's register
    // (F-time, docs/sim/state.md §13).
    sim::Player w = sim::make_player(sim::PlayerId{0}, cfg);
    w.money = -4000;
    sim::player_tick(w, 0xFFFF0000U, cfg, sim::Rules{});
    CHECK(w.bankrupt_deadline == 84464); // 0xFFFF0000 + 150000 - 2^32
    // Starting the period and ending it are exclusive within one visit (D:
    // 0x1001b93f jumps over the compare): the wrapped deadline is below t, but the
    // player is still Alive after the visit that set it ...
    CHECK(w.state == sim::PlayerState::Alive);
    // ... and is declared bankrupt at the next visit (OQ-057, the original's wrap).
    sim::player_tick(w, 0xFFFF0001U, cfg, sim::Rules{});
    CHECK(w.state == sim::PlayerState::Bankrupt);
    CHECK(w.bankrupt_deadline == 84464);
    // A sum of exactly 2^32 lands on 0, which means "not running": the deadline
    // is set again at every visit, and the next tick lands on 1.
    sim::Player z = sim::make_player(sim::PlayerId{0}, cfg);
    z.money = -4000;
    sim::player_tick(z, 0xFFFDB610U, cfg, sim::Rules{}); // 0xFFFDB610 + 150000 = 2^32
    CHECK(z.bankrupt_deadline == 0);
    sim::player_tick(z, 0xFFFDB610U, cfg, sim::Rules{});
    CHECK(z.bankrupt_deadline == 0);
    CHECK(z.state == sim::PlayerState::Alive);
    sim::player_tick(z, 0xFFFDB611U, cfg, sim::Rules{});
    CHECK(z.bankrupt_deadline == 1);
    CHECK(z.state == sim::PlayerState::Alive);
    sim::player_tick(z, 0xFFFDB612U, cfg, sim::Rules{});
    CHECK(z.state == sim::PlayerState::Bankrupt);

    // Another limit and grace period.
    sim::SimConfig other;
    other.bankrupt_money = -10;
    other.bankrupt_grace_ticks = 50;
    sim::Player o = sim::make_player(sim::PlayerId{0}, other);
    o.money = -10;
    sim::player_tick(o, 7, other, sim::Rules{});
    CHECK(o.bankrupt_deadline == 57);
    sim::player_tick(o, 58, other, sim::Rules{});
    CHECK(o.state == sim::PlayerState::Bankrupt);
}

TEST_CASE("player bankruptcy and defeat skip the neutral player and any non-alive state") {
    sim::SimConfig cfg;
    for (const std::uint32_t neutral : {31U, 3U}) {
        INFO("neutral " << neutral);
        cfg.neutral_player = neutral;
        for (const std::uint32_t index : {3U, 31U, 5U}) {
            sim::Player p = sim::make_player(sim::PlayerId{index}, cfg);
            p.money = -9000;
            p.defeat_pending = true;
            HookLog log;
            sim::player_tick(p, 100, cfg, sim::Rules{}, hooks_for(log));
            if (index == neutral) {
                CHECK(p.bankrupt_deadline == 0);
                CHECK(p.defeat_pending);
                CHECK(log.count('d') == 0);
            } else {
                CHECK(p.bankrupt_deadline == 150100);
                CHECK_FALSE(p.defeat_pending);
                CHECK(log.count('d') == 1);
            }
        }
    }
    cfg.neutral_player = 31;
    // Any non-zero state byte is "not alive" (D: 0x1001b927 tests the byte against
    // zero), the named ones and the open values 1, 7, 8 and 255 alike.
    for (const sim::PlayerState state :
         {sim::PlayerState::Bankrupt, sim::PlayerState::Defeated, static_cast<sim::PlayerState>(1),
          static_cast<sim::PlayerState>(7), static_cast<sim::PlayerState>(8),
          static_cast<sim::PlayerState>(255)}) {
        sim::Player p = sim::make_player(sim::PlayerId{2}, cfg);
        p.state = state;
        p.money = -9000;
        p.defeat_pending = true;
        p.bankrupt_deadline = 77;
        HookLog log;
        sim::player_tick(p, 100000, cfg, sim::Rules{}, hooks_for(log));
        CHECK(p.state == state);
        CHECK(p.bankrupt_deadline == 77); // untouched
        CHECK(p.defeat_pending);          // stays set
        CHECK(log.count('d') == 0);
        CHECK(log.count('g') == 1); // the group pass is not behind the gate
    }
}

TEST_CASE("player defeat pending is cleared and the default seam does nothing") {
    const sim::SimConfig cfg;
    sim::Player p = sim::make_player(sim::PlayerId{1}, cfg);
    p.defeat_pending = true;
    sim::player_tick(p, 100, cfg, sim::Rules{}); // no hook installed
    CHECK_FALSE(p.defeat_pending);
    CHECK(p.state == sim::PlayerState::Alive); // not defeated by the default
    // Without the flag the hook is not called.
    HookLog log;
    sim::Player q = sim::make_player(sim::PlayerId{1}, cfg);
    sim::player_tick(q, 100, cfg, sim::Rules{}, hooks_for(log));
    CHECK(log.count('d') == 0);
    CHECK(log.count('g') == 1);
}

TEST_CASE("player hooks run groups first and defeat last with the context given") {
    const sim::SimConfig cfg;
    sim::Player p = sim::make_player(sim::PlayerId{1}, cfg);
    p.money = -4000;  // enters the grace period in this very tick
    p.upkeep_a = 150; // and books 10 upkeep
    p.defeat_pending = true;
    HookLog log;
    sim::player_tick(p, 2000, cfg, sim::Rules{}, hooks_for(log));
    REQUIRE(log.calls.size() == 2);
    const HookLog::Call& groups = log.calls[0];
    const HookLog::Call& defeat = log.calls[1];
    CHECK(groups.which == 'g');
    CHECK(defeat.which == 'd');
    CHECK(groups.player == 1);
    CHECK(defeat.player == 1);
    // The group pass sees the player before the upkeep and the bankruptcy test.
    CHECK(groups.money == -4000);
    CHECK(groups.upkeep_stamp == 0);
    CHECK(groups.deadline == 0);
    CHECK(groups.defeat_pending);
    // The defeat pass sees the booked upkeep, the started grace period and a
    // cleared flag.
    CHECK(defeat.money == -4010);
    CHECK(defeat.upkeep_stamp == 2000);
    CHECK(defeat.deadline == 152000);
    CHECK_FALSE(defeat.defeat_pending);
    CHECK(defeat.state == sim::PlayerState::Alive);
    // Each hook gets its own context.
    HookLog groups_log;
    HookLog defeat_log;
    sim::PlayerHooks split = hooks_for(groups_log);
    split.defeat_ctx = &defeat_log;
    sim::Player s = sim::make_player(sim::PlayerId{1}, cfg);
    s.defeat_pending = true;
    sim::player_tick(s, 100, cfg, sim::Rules{}, split);
    CHECK(groups_log.calls.size() == 1);
    CHECK(groups_log.calls[0].which == 'g');
    CHECK(defeat_log.calls.size() == 1);
    CHECK(defeat_log.calls[0].which == 'd');
    // A hook whose context is null is still called with that null.
    sim::PlayerHooks no_ctx;
    no_ctx.groups = [](void* ctx, sim::Player& player) {
        player.flags = ctx == nullptr ? 99U : 0U;
    };
    sim::Player r = sim::make_player(sim::PlayerId{1}, cfg);
    sim::player_tick(r, 1, cfg, sim::Rules{}, no_ctx);
    CHECK(r.flags == 99);
}

TEST_CASE("player table tick calls the hooks only for present players in visit order") {
    sim::PlayerTable t = sim::make_player_table(sim::SimConfig{});
    for (const std::uint32_t slot : {1U, 2U, 4U}) {
        t.slots[slot] = sim::make_player(sim::PlayerId{slot}, sim::SimConfig{});
        at(t, slot).defeat_pending = true;
    }
    HookLog log;
    sim::player_table_tick(t, 10, sim::SimConfig{}, sim::Rules{}, hooks_for(log)); // 7 visits
    // The calls alternate per player: groups then defeat, in slot order.
    REQUIRE(log.calls.size() == 6);
    const std::vector<std::pair<char, std::uint32_t>> want = {{'g', 1}, {'d', 1}, {'g', 2},
                                                              {'d', 2}, {'g', 4}, {'d', 4}};
    for (std::size_t i = 0; i < want.size(); ++i) {
        CHECK(log.calls[i].which == want[i].first);
        CHECK(log.calls[i].player == want[i].second);
    }
    // The table tick passes the ctx of the hooks to every call.
    CHECK(std::none_of(t.slots.begin(), t.slots.end(),
                       [](const auto& s) { return s.has_value() && s->defeat_pending; }));
}

TEST_CASE("players view answers total queries") {
    sim::SimConfig cfg;
    sim::PlayerTable t = sim::make_player_table(cfg);
    t.slots[1] = sim::make_player(sim::PlayerId{1}, cfg);
    t.slots[1]->volk = 3;
    t.slots[1]->free_build = true;
    t.slots[1]->state = sim::PlayerState::Bankrupt;
    t.slots[1]->research.state[7] = 2;
    t.slots[1]->research.state[8] = 1;
    t.slots[5] = sim::make_player(sim::PlayerId{5}, cfg);
    const sim::PlayersView view(t);
    CHECK(view.present(sim::PlayerId{1}));
    CHECK(view.present(sim::PlayerId{5}));
    CHECK_FALSE(view.present(sim::PlayerId{2}));
    CHECK_FALSE(view.present(sim::PlayerId{32}));
    CHECK_FALSE(view.present(sim::PlayerId{}));
    CHECK(view.volk(sim::PlayerId{1}) == 3);
    CHECK(view.volk(sim::PlayerId{5}) == 1);
    CHECK(view.volk(sim::PlayerId{2}) == 0);
    CHECK(view.volk(sim::PlayerId{}) == 0);
    CHECK(view.free_build(sim::PlayerId{1}));
    CHECK_FALSE(view.free_build(sim::PlayerId{5}));
    CHECK_FALSE(view.free_build(sim::PlayerId{2}));
    CHECK(view.researched(sim::PlayerId{1}, 7));
    CHECK_FALSE(view.researched(sim::PlayerId{1}, 8)); // researchable only
    CHECK_FALSE(view.researched(sim::PlayerId{1}, 9));
    CHECK_FALSE(view.researched(sim::PlayerId{1}, 65)); // beyond the table
    CHECK_FALSE(view.researched(sim::PlayerId{5}, 7));
    CHECK_FALSE(view.researched(sim::PlayerId{2}, 7));
    CHECK(view.state(sim::PlayerId{1}) == sim::PlayerState::Bankrupt);
    CHECK(view.state(sim::PlayerId{5}) == sim::PlayerState::Alive);
    CHECK(view.state(sim::PlayerId{2}) == sim::PlayerState::Defeated); // not there
    // The view looks at the table, it holds no copy.
    t.slots[5]->volk = 4;
    CHECK(view.volk(sim::PlayerId{5}) == 4);
    // A default view holds no player.
    const sim::PlayersView none;
    CHECK_FALSE(none.present(sim::PlayerId{1}));
    CHECK(none.volk(sim::PlayerId{1}) == 0);
    CHECK_FALSE(none.researched(sim::PlayerId{1}, 7));
    CHECK_FALSE(none.free_build(sim::PlayerId{1}));
    CHECK(none.state(sim::PlayerId{1}) == sim::PlayerState::Defeated);
}

TEST_CASE("world advance runs the player table and does not special-case zero") {
    sim::World w;
    sim::WorldTestAccess::players(w) = full_table(w.config());
    CHECK(w.advance(10) == 5);
    CHECK(w.players().stamp == 5);
    CHECK(w.players().cursor == 4); // d = 5 -> 4 visits
    // advance(0) adds nothing to the clock and still runs the subsystems: d = 0
    // visits one slot, the stamp is written.
    CHECK(w.advance(0) == 5);
    CHECK(w.players().cursor == 5);
    CHECK(w.players().stamp == 5);
    CHECK(w.clock().last_tick == 5);
    // The tick passed is the low 32 bits of the 64-bit tick: tick 2^32 + 70000 is
    // t = 70000 (stamp 4464 as a word, grace deadline 220000).
    sim::World late;
    sim::WorldTestAccess::players(late) = full_table(late.config());
    sim::WorldTestAccess::players(late).slots[0]->money = -9000;
    sim::WorldTestAccess::players(late).stamp = 69995 - 65536; // the word of tick 69995
    sim::WorldTestAccess::clock(late).ms = 2ULL * (0x100000000ULL + 70000);
    sim::WorldTestAccess::clock(late).last_tick = late.clock().tick();
    CHECK(late.advance(0) == 0x100000000ULL + 70000);
    CHECK(late.players().stamp == 70000 - 65536);
    CHECK(late.players().cursor == 4); // d = 5
    CHECK(late.players().slots[0]->bankrupt_deadline == 70000 + 150000);
}

TEST_CASE("world advance respects the rules and the configuration it holds") {
    sim::World w;
    sim::WorldTestAccess::players(w) = full_table(w.config());
    sim::WorldTestAccess::rules(w).player_table_full_tick = true;
    w.advance(0);
    CHECK(w.players().cursor == 0); // all 32 visited, back where it started
    // Upkeep through the world: a player in slot 0 is visited by every advance
    // with the full tick rule.
    sim::WorldTestAccess::players(w).slots[0]->upkeep_a = 300; // 20 per period
    sim::WorldTestAccess::players(w).slots[0]->money = 0;
    for (int i = 0; i < 400; ++i) { // t = 5 * i, 10 ms per advance
        w.advance(10);
    }
    CHECK(w.clock().tick() == 2000);
    CHECK(w.players().slots[0]->money == -20);
    CHECK(w.players().slots[0]->upkeep_stamp == 2000);
}

TEST_CASE("player table step size is not invariant") {
    // Three 2 ms advances are not one 6 ms advance (docs/sim/state.md §13, D6): the
    // visit count min(d*32/50 + 1, 32) is not additive.
    sim::World three;
    sim::World one;
    sim::WorldTestAccess::players(three) = full_table(three.config());
    sim::WorldTestAccess::players(one) = full_table(one.config());
    three.advance(2);
    three.advance(2);
    three.advance(2);
    one.advance(6);
    CHECK(three.clock().ms == one.clock().ms);
    CHECK(three.players().cursor == 3); // d = 1 each: one visit each
    CHECK(one.players().cursor == 2);   // d = 3: 3*32/50 + 1 = 2
    CHECK(three.players().stamp == one.players().stamp);
    CHECK(three.state_hash() != one.state_hash());
}

TEST_CASE("player upkeep and bankruptcy timeline through the world") {
    // The full tick rule visits slot 0 on every advance; 10 ms steps are 5 ticks.
    // Money -2990 with 20 booked per 2000 ticks: the first booking at t = 2000
    // brings it to -3010, which starts the grace period (deadline 152000); the
    // player is bankrupt at the first visit with t > 152000, t = 152005.
    sim::World w;
    sim::WorldTestAccess::players(w) = full_table(w.config());
    sim::WorldTestAccess::rules(w).player_table_full_tick = true;
    sim::Player& p = *sim::WorldTestAccess::players(w).slots[0];
    p.money = -2990;
    p.upkeep_a = 300;
    auto run_to = [&](std::uint64_t tick) {
        while (w.clock().tick() < tick) {
            w.advance(10);
        }
    };
    run_to(1995);
    CHECK(p.money == -2990);
    CHECK(p.bankrupt_deadline == 0);
    run_to(2000);
    CHECK(p.money == -3010);
    CHECK(p.upkeep_stamp == 2000);
    CHECK(p.bankrupt_deadline == 152000);
    run_to(152000);
    CHECK(p.state == sim::PlayerState::Alive);
    CHECK(p.money == -2990 - 76 * 20);
    run_to(152005);
    CHECK(p.state == sim::PlayerState::Bankrupt);
    CHECK(p.money == -2990 - 76 * 20);
    // The other players (10000 gold, no upkeep) are untouched.
    CHECK(w.players().slots[1]->money == 10000);
    CHECK(w.players().slots[1]->state == sim::PlayerState::Alive);
    // Saved and loaded back, the table goes on the same way.
    sim::WriteArchive wa;
    fields(wa, std::as_const(w));
    const std::vector<std::uint8_t> bytes = wa.take();
    sim::World copy;
    sim::ReadArchive ra(bytes);
    fields(ra, copy);
    REQUIRE_FALSE(ra.failed());
    CHECK(copy.state_hash() == w.state_hash());
    w.advance(10);
    copy.advance(10);
    CHECK(copy.state_hash() == w.state_hash());
}

namespace {

/// An independent model of one table advance, written from the pseudo-code of
/// docs/sim/state.md §5.10 in plain 64-bit arithmetic (the implementation uses
/// 32-bit types and shared helpers), to compare whole tables after every
/// advance of a random run.
struct ReferenceTable {
    std::vector<std::optional<sim::Player>> slots;
    std::uint64_t stamp = 0;
    std::uint64_t cursor = 0;
    sim::SimConfig cfg;
    bool full = false;

    void player(sim::Player& p, std::uint64_t t) {
        const std::uint64_t d = (t - p.upkeep_stamp) % 65536;
        if (d >= cfg.upkeep_period) {
            const std::uint64_t sum =
                (std::uint64_t{p.upkeep_a} + p.upkeep_b + p.upkeep_rem) % 4294967296ULL;
            p.money -= static_cast<std::int64_t>(sum / cfg.upkeep_slices);
            p.upkeep_rem = static_cast<std::uint32_t>(sum % cfg.upkeep_slices);
            p.upkeep_stamp = static_cast<std::uint16_t>(t % 65536);
            p.stat_a = static_cast<std::uint32_t>(
                (std::uint64_t{p.stat_a} * cfg.stat_decay_num % 4294967296ULL) /
                cfg.stat_decay_den);
            p.stat_b = static_cast<std::uint32_t>(
                (std::uint64_t{p.stat_b} * cfg.stat_decay_num % 4294967296ULL) /
                cfg.stat_decay_den);
        }
        if (p.index.v != cfg.neutral_player && p.state == sim::PlayerState::Alive) {
            if (p.money <= cfg.bankrupt_money) {
                if (p.bankrupt_deadline == 0) {
                    p.bankrupt_deadline =
                        static_cast<std::uint32_t>((t + cfg.bankrupt_grace_ticks) % 4294967296ULL);
                } else if (t > p.bankrupt_deadline) {
                    p.state = sim::PlayerState::Bankrupt;
                }
            } else {
                p.bankrupt_deadline = 0;
            }
            p.defeat_pending = false;
        }
    }

    void tick(std::uint64_t t) {
        const std::uint64_t n = slots.size();
        const std::uint64_t d = (t - stamp) % 65536;
        std::uint64_t count = d * n / cfg.player_div + 1;
        if (count > n) {
            count = n;
        }
        if (full) {
            count = n;
        }
        for (std::uint64_t i = 0; i < count; ++i) {
            const std::uint64_t slot = (cursor + i) % n;
            if (slots[slot].has_value()) {
                player(*slots[slot], t);
            }
        }
        cursor = (cursor + count) % n;
        stamp = t % 65536;
    }
};

bool same_player(const sim::Player& a, const sim::Player& b) {
    return a.index == b.index && a.money == b.money && a.upkeep_a == b.upkeep_a &&
           a.upkeep_b == b.upkeep_b && a.upkeep_rem == b.upkeep_rem &&
           a.upkeep_stamp == b.upkeep_stamp && a.stat_a == b.stat_a && a.stat_b == b.stat_b &&
           a.state == b.state && a.bankrupt_deadline == b.bankrupt_deadline &&
           a.defeat_pending == b.defeat_pending && a.flags == b.flags && a.kind == b.kind;
}

} // namespace

TEST_CASE("player table equals an independent model over random runs") {
    struct Variant {
        const char* name;
        bool full;
        void (*adjust)(sim::SimConfig&);
    };
    const Variant variants[] = {
        {"defaults", false, [](sim::SimConfig&) {}},
        {"full tick", true, [](sim::SimConfig&) {}},
        {"modded", false,
         [](sim::SimConfig& c) {
             c.upkeep_period = 700;
             c.upkeep_slices = 9;
             c.bankrupt_grace_ticks = 3000;
             c.neutral_player = 5;
             c.player_div = 7;
             c.stat_decay_num = 100;
             c.stat_decay_den = 101;
             c.bankrupt_money = -1500;
         }},
    };
    std::uint32_t lcg = 12345;
    auto rnd = [&](std::uint32_t bound) {
        lcg = lcg * 1664525U + 1013904223U;
        return (lcg >> 8) % bound;
    };
    for (const Variant& variant : variants) {
        INFO(variant.name);
        sim::World w;
        sim::SimConfig& cfg = sim::WorldTestAccess::config(w);
        variant.adjust(cfg);
        sim::WorldTestAccess::rules(w).player_table_full_tick = variant.full;
        sim::PlayerTable& table = sim::WorldTestAccess::players(w);
        table = full_table(cfg);
        // Remove a few players so that holes are passed over.
        for (const std::uint32_t hole : {6U, 13U, 14U}) {
            table.slots[hole].reset();
        }
        for (auto& slot : table.slots) {
            if (!slot) {
                continue;
            }
            sim::Player& p = *slot;
            p.money = static_cast<std::int64_t>(rnd(6000)) - 4000;
            p.upkeep_a = rnd(6000);
            p.upkeep_b = rnd(3000);
            p.upkeep_rem = rnd(9);
            p.upkeep_stamp = static_cast<std::uint16_t>(rnd(65536));
            p.stat_a = rnd(2) == 0 ? rnd(100000) : 40000000U + rnd(40000000);
            p.stat_b = rnd(100000);
            p.defeat_pending = rnd(4) == 0;
            p.state = rnd(12) == 0 ? sim::PlayerState::Defeated : sim::PlayerState::Alive;
        }
        ReferenceTable model;
        model.slots = table.slots;
        model.cfg = cfg;
        model.full = variant.full;

        std::size_t bankrupt = 0;
        std::size_t bookings = 0;
        for (int step = 0; step < 14000; ++step) {
            const std::uint32_t ms = rnd(9) == 0 ? 0U : 1 + rnd(100);
            const std::int64_t money_before = [&] {
                std::int64_t sum = 0;
                for (const auto& s : w.players().slots) {
                    sum += s ? s->money : 0;
                }
                return sum;
            }();
            const std::uint64_t tick = w.advance(ms);
            model.tick(tick & 0xFFFFFFFFU);
            bool equal = w.players().slots.size() == model.slots.size() &&
                         w.players().cursor == model.cursor && w.players().stamp == model.stamp;
            for (std::size_t i = 0; equal && i < model.slots.size(); ++i) {
                equal = w.players().slots[i].has_value() == model.slots[i].has_value() &&
                        (!model.slots[i] || same_player(*w.players().slots[i], *model.slots[i]));
            }
            INFO("step " << step << " ms " << ms);
            REQUIRE(equal);
            std::int64_t money_after = 0;
            for (const auto& s : w.players().slots) {
                money_after += s ? s->money : 0;
                bankrupt += (s && s->state == sim::PlayerState::Bankrupt) ? 1U : 0U;
            }
            bookings += money_after != money_before ? 1U : 0U;
        }
        // The run exercised the rules: money was booked, and players went bankrupt.
        CHECK(bookings > 100);
        CHECK(bankrupt > 0);
        CHECK(sim::validate_players(w.players(), cfg).empty());
    }
}

namespace {

/// The scenario of the golden G1, mirrored in tools/sim_stream.py (g1_slots): six
/// players in a world with the full-tick rule, run in steps of 500 ms (250 ticks).
sim::World g1_world() {
    sim::World w(1234);
    sim::WorldTestAccess::rules(w).player_table_full_tick = true;
    const sim::SimConfig cfg = w.config();
    sim::PlayerTable& t = sim::WorldTestAccess::players(w);
    t = sim::make_player_table(cfg);
    auto put = [&](std::uint32_t i) -> sim::Player& {
        t.slots[i] = sim::make_player(sim::PlayerId{i}, cfg);
        return *t.slots[i];
    };
    sim::Player& a = put(0);
    a.money = 500;
    a.upkeep_a = 300;
    a.upkeep_b = 30;
    a.stat_a = 1050;
    a.stat_b = 210;
    a.name = {'A', 'n', 'n', 'o'};
    sim::Player& b = put(1);
    b.money = -2990;
    b.upkeep_a = 600;
    b.defeat_pending = true;
    sim::Player& c = put(2);
    c.upkeep_a = 14;
    c.upkeep_rem = 7;
    c.stat_a = 50000000;
    c.stat_b = 40000000;
    c.upkeep_stamp = 1000;
    c.colour_slot = 3;
    sim::Player& d = put(4);
    d.money = 100;
    d.upkeep_a = 15;
    d.state = sim::PlayerState::Defeated;
    d.defeat_pending = true;
    sim::Player& e = put(7);
    e.money = -9999;
    e.state = static_cast<sim::PlayerState>(8);
    sim::Player& n = put(31);
    n.money = -9000;
    n.upkeep_a = 150;
    return w;
}

} // namespace

TEST_CASE("golden G1 populated player table with the full tick rule") {
    // The hash after 100 and 700 steps of 500 ms against goldens.txt, which
    // tools/sim_stream.py computes from its own transcription of the per-player
    // rules (upkeep gate, booking, decay in u32, bankruptcy grace). Player 1 enters
    // the grace period at t = 2000 and is bankrupt at the first visit after 152000.
    sim::World w = g1_world();
    int done = 0;
    for (const int checkpoint : {100, 700}) {
        while (done < checkpoint) {
            w.advance(500);
            ++done;
        }
        const std::string name = "g1_players_step" + std::to_string(checkpoint);
        std::string update_line;
        const std::string problem =
            anno::test::check_golden(name, "S4", w.state_hash(), update_line);
        if (anno::test::update_mode()) {
            MESSAGE("golden line: " << update_line);
        }
        INFO(name);
        CHECK(problem == "");
        const sim::PlayerTable& table = w.players();
        if (checkpoint == 100) {
            CHECK(table.slots[1]->state == sim::PlayerState::Alive);
            CHECK(table.slots[1]->bankrupt_deadline == 152000);
        } else {
            CHECK(table.slots[1]->state == sim::PlayerState::Bankrupt);
            CHECK(table.slots[31]->state == sim::PlayerState::Alive); // neutral
            CHECK(table.slots[31]->bankrupt_deadline == 0);
            CHECK(table.slots[7]->state == static_cast<sim::PlayerState>(8)); // untouched
            CHECK(table.slots[4]->defeat_pending);                            // not alive
        }
    }
    CHECK(w.clock().ms == 350000);
    CHECK(w.rng().cursor() == 1234);
}

namespace {

/// A populated table for the stream tests: players 0, 3 and 31 with distinct
/// values in every field.
sim::PlayerTable populated_table(const sim::SimConfig& cfg) {
    sim::PlayerTable t = sim::make_player_table(cfg);
    sim::Player a = sim::make_player(sim::PlayerId{0}, cfg);
    a.name = {'A', 'n', 'n', 'o'};
    a.research.state[2] = 3;
    a.research.progress[2] = 0x0102;
    t.slots[0] = a;
    sim::Player b = sim::make_player(sim::PlayerId{3}, cfg);
    b.kind = 12;
    b.volk = 2;
    b.money = -4000000000LL;
    b.upkeep_a = 11;
    b.upkeep_b = 22;
    b.upkeep_rem = 5;
    b.upkeep_stamp = 0x1234;
    b.stat_a = 0x01020304;
    b.stat_b = 0x05060708;
    b.flags = 0x11;
    b.flags_a0 = 0x08;
    b.colour_slot = 4;
    b.default_colour = 5;
    b.state = sim::PlayerState::Bankrupt;
    b.bankrupt_deadline = 150100;
    b.defeat_pending = true;
    b.free_build = false; // unlike defeat_pending, so a swap of the two bytes shows
    t.slots[3] = b;
    sim::Player c = sim::make_player(sim::PlayerId{31}, cfg);
    c.state = sim::PlayerState::Defeated;
    t.slots[31] = c;
    t.stamp = 0xBEEF;
    t.cursor = 9;
    return t;
}

void expected_player(Stream& s, const sim::Player& p) {
    s.u32(p.index.v);
    s.u32(p.kind);
    s.u32(p.volk);
    s.i64(p.money);
    s.u32(p.upkeep_a);
    s.u32(p.upkeep_b);
    s.u32(p.upkeep_rem);
    s.u16(p.upkeep_stamp);
    s.u32(p.stat_a);
    s.u32(p.stat_b);
    s.u32(p.flags);
    s.u32(p.flags_a0);
    s.u32(p.colour_slot);
    s.u32(p.default_colour);
    s.u8(static_cast<std::uint8_t>(p.state));
    s.u32(p.bankrupt_deadline);
    s.boolean(p.defeat_pending);
    s.boolean(p.free_build);
    s.u64(p.name.size());
    for (const std::uint8_t c : p.name) {
        s.u8(c);
    }
    s.u64(p.research.state.size());
    for (const std::uint8_t c : p.research.state) {
        s.u8(c);
    }
    s.u64(p.research.progress.size());
    for (const std::uint16_t c : p.research.progress) {
        s.u16(c);
    }
}

std::vector<std::uint8_t> save_table(const sim::PlayerTable& t) {
    sim::WriteArchive wa;
    fields(wa, t);
    return wa.take();
}

std::vector<std::uint8_t> save_world(const sim::World& w) {
    sim::WriteArchive wa;
    fields(wa, w);
    return wa.take();
}

std::string read_world(sim::World& target, const std::vector<std::uint8_t>& in) {
    sim::ReadArchive r(in);
    fields(r, target);
    return r.failed() ? r.error() : (r.remaining() == 0 ? std::string() : "trailing");
}

} // namespace

TEST_CASE("players scope stream layout") {
    const sim::SimConfig cfg;
    const sim::PlayerTable t = populated_table(cfg);
    // docs/sim/state.md §9.3: u64 slot count, per slot a presence byte and the
    // Player, then u16 stamp and u32 cursor.
    Stream expected;
    expected.u64(32);
    for (std::uint32_t i = 0; i < 32; ++i) {
        if (t.slots[i]) {
            expected.u8(1);
            expected_player(expected, *t.slots[i]);
        } else {
            expected.u8(0);
        }
    }
    expected.u16(0xBEEF);
    expected.u32(9);
    CHECK(save_table(t) == expected.v);
    // A player is 4+4+4+8+4+4+4+2+4+4+4+4+4+4+1+4+1+1 = 65 bytes, then the name
    // (8 + n), the 65 research bytes (8 + 65) and the 65 words (8 + 130).
    Stream one;
    expected_player(one, *t.slots[31]);
    CHECK(one.v.size() == 65 + 8 + 8 + 65 + 8 + 130);

    // The world stream has the table after the ids scope and before the islands scope
    // (stage S5: the u64 slot count 0 of a world without islands), then the figures
    // scope (39 bytes) and the events scope (8 bytes) of stage S7 and the
    // timers_monopoly scope (193 bytes) of stage S12.
    sim::World w;
    sim::WorldTestAccess::players(w) = t;
    const std::vector<std::uint8_t> world = save_world(w);
    constexpr std::ptrdiff_t kIslandsBytes = 8 + 39 + 8 + 193;
    REQUIRE(world.size() > expected.v.size() + static_cast<std::size_t>(kIslandsBytes));
    CHECK(std::equal(expected.v.begin(), expected.v.end(),
                     world.end() - kIslandsBytes - static_cast<std::ptrdiff_t>(expected.v.size())));
}

TEST_CASE("players scope hash tree names the player") {
    sim::World w;
    sim::WorldTestAccess::players(w) = populated_table(w.config());
    sim::HashTree tree;
    CHECK(w.state_hash(tree, 1) == w.state_hash());
    const sim::HashNode* players = nullptr;
    for (const sim::HashNode& c : tree.root.children) {
        if (c.name == "players") {
            players = &c;
        }
    }
    REQUIRE(players != nullptr);
    REQUIRE(players->children.size() == 32);
    // Corrupting player 3 moves exactly its child and the table above it.
    sim::World other = w;
    sim::WorldTestAccess::players(other).slots[3]->money += 1;
    sim::HashTree changed;
    CHECK(other.state_hash(changed, 1) != w.state_hash());
    const sim::HashNode* players2 = nullptr;
    for (const sim::HashNode& c : changed.root.children) {
        if (c.name == "players") {
            players2 = &c;
        }
    }
    REQUIRE(players2 != nullptr);
    CHECK(players2->hash != players->hash);
    for (std::size_t i = 0; i < 32; ++i) {
        INFO("slot " << i);
        CHECK((players2->children[i].hash != players->children[i].hash) == (i == 3));
    }
    // The other top-level scopes are the same.
    for (std::size_t i = 0; i < tree.root.children.size(); ++i) {
        if (tree.root.children[i].name != "players") {
            CHECK(changed.root.children[i].hash == tree.root.children[i].hash);
        }
    }
}

TEST_CASE("every player field moves the players scope hash") {
    using Mutation = void (*)(sim::Player&);
    // One mutation per member of Player except the index (checked below); the
    // research table is one member with two vectors.
    const std::vector<std::pair<const char*, Mutation>> mutations = {
        {"kind", [](sim::Player& p) { p.kind += 1; }},
        {"volk", [](sim::Player& p) { p.volk += 1; }},
        {"money", [](sim::Player& p) { p.money += 1; }},
        {"upkeep_a", [](sim::Player& p) { p.upkeep_a += 1; }},
        {"upkeep_b", [](sim::Player& p) { p.upkeep_b += 1; }},
        {"upkeep_rem", [](sim::Player& p) { p.upkeep_rem += 1; }},
        {"upkeep_stamp",
         [](sim::Player& p) { p.upkeep_stamp = static_cast<std::uint16_t>(p.upkeep_stamp + 1); }},
        {"stat_a", [](sim::Player& p) { p.stat_a += 1; }},
        {"stat_b", [](sim::Player& p) { p.stat_b += 1; }},
        {"flags", [](sim::Player& p) { p.flags += 1; }},
        {"flags_a0", [](sim::Player& p) { p.flags_a0 += 1; }},
        {"colour_slot", [](sim::Player& p) { p.colour_slot += 256; }},
        {"default_colour", [](sim::Player& p) { p.default_colour += 256; }},
        {"state",
         [](sim::Player& p) {
             p.state = p.state == sim::PlayerState::Alive ? sim::PlayerState::Bankrupt
                                                          : sim::PlayerState::Alive;
         }},
        {"bankrupt_deadline", [](sim::Player& p) { p.bankrupt_deadline += 1; }},
        {"defeat_pending", [](sim::Player& p) { p.defeat_pending = !p.defeat_pending; }},
        {"free_build", [](sim::Player& p) { p.free_build = !p.free_build; }},
        {"name", [](sim::Player& p) { p.name.push_back('x'); }},
        {"research.state",
         [](sim::Player& p) {
             p.research.state[64] = static_cast<std::uint8_t>(p.research.state[64] ^ 2U);
         }},
        {"research.progress",
         [](sim::Player& p) {
             p.research.progress[64] = static_cast<std::uint16_t>(p.research.progress[64] + 1);
         }},
    };
    static_assert(sim::field_count<sim::Player>() == 20);
    static_assert(sim::field_count<sim::ResearchState>() == 2);
    static_assert(sim::field_count<sim::PlayerTable>() == 3);
    CHECK(mutations.size() == sim::field_count<sim::Player>());

    sim::World base;
    sim::WorldTestAccess::players(base) = populated_table(base.config());
    sim::HashTree tree;
    const std::uint64_t base_hash = base.state_hash(tree, 0);
    auto scope_hashes = [](const sim::HashTree& t) {
        std::vector<std::uint64_t> out;
        for (const sim::HashNode& c : t.root.children) {
            out.push_back(c.hash);
        }
        return out;
    };
    const auto base_scopes = scope_hashes(tree);
    REQUIRE(base_scopes.size() == 9);
    for (const std::uint32_t slot : {0U, 3U, 31U}) {
        for (const auto& [name, mutate] : mutations) {
            INFO("slot " << slot << " field " << std::string(name));
            sim::World w = base;
            mutate(*sim::WorldTestAccess::players(w).slots[slot]);
            sim::HashTree t;
            CHECK(w.state_hash(t, 0) != base_hash);
            const auto s = scope_hashes(t);
            for (std::size_t k = 0; k < s.size(); ++k) {
                if (k == 4) {
                    CHECK(s[k] != base_scopes[k]); // players
                } else {
                    CHECK(s[k] == base_scopes[k]);
                }
            }
        }
    }
    // The index.
    sim::World w = base;
    sim::WorldTestAccess::players(w).slots[3]->index = sim::PlayerId{4};
    CHECK(w.state_hash() != base_hash);
    // The table's own fields and the presence of a slot.
    w = base;
    sim::WorldTestAccess::players(w).stamp += 1;
    CHECK(w.state_hash() != base_hash);
    w = base;
    sim::WorldTestAccess::players(w).cursor += 1;
    CHECK(w.state_hash() != base_hash);
    w = base;
    sim::WorldTestAccess::players(w).slots[31].reset();
    CHECK(w.state_hash() != base_hash);
    w = base;
    sim::WorldTestAccess::players(w).slots.push_back(std::nullopt);
    CHECK(w.state_hash() != base_hash);
    // Advancing moves the table's scope.
    w = base;
    w.advance(10);
    CHECK(w.state_hash() != base_hash);
}

TEST_CASE("players scope reads back and validates what it read") {
    sim::World w(77);
    sim::WorldTestAccess::players(w) = populated_table(w.config());
    const std::vector<std::uint8_t> bytes = save_world(w);

    SUBCASE("a fresh world reads it back to the same hash and bytes") {
        sim::World other;
        CHECK(read_world(other, bytes) == "");
        CHECK(other.state_hash() == w.state_hash());
        REQUIRE(other.players().slots[3].has_value());
        CHECK(other.players().slots[3]->money == -4000000000LL);
        CHECK(other.players().slots[3]->state == sim::PlayerState::Bankrupt);
        CHECK(other.players().slots[0]->name == std::vector<std::uint8_t>{'A', 'n', 'n', 'o'});
        CHECK(other.players().slots[0]->research.progress[2] == 0x0102);
        CHECK(other.players().stamp == 0xBEEF);
        CHECK(other.players().cursor == 9);
        CHECK(save_world(other) == bytes);
        // The loaded world continues like the live one.
        w.advance(10);
        other.advance(10);
        CHECK(other.state_hash() == w.state_hash());
    }
    SUBCASE("every prefix fails") {
        for (std::size_t n = 0; n < bytes.size(); ++n) {
            sim::World other;
            const std::vector<std::uint8_t> prefix(bytes.begin(),
                                                   bytes.begin() + static_cast<std::ptrdiff_t>(n));
            INFO("prefix " << n);
            CHECK(read_world(other, prefix) != "");
        }
    }
    SUBCASE("a table of the wrong size is refused") {
        sim::World bad = w;
        sim::WorldTestAccess::players(bad).slots.pop_back();
        sim::World other;
        const std::string err = read_world(other, save_world(bad));
        CHECK(starts_with(err, "players"));
        CHECK(err.find("SimConfig.player_slots") != std::string::npos);
        // One slot too many is refused as well.
        sim::World many = w;
        sim::WorldTestAccess::players(many).slots.push_back(std::nullopt);
        const std::string err_many = read_world(other, save_world(many));
        CHECK(starts_with(err_many, "players"));
        CHECK(err_many.find("has 33 entries") != std::string::npos);
        // The same table is fine for a configuration of 31 slots.
        sim::World fits = w;
        sim::WorldTestAccess::config(fits).player_slots = 31;
        sim::WorldTestAccess::players(fits).slots.pop_back();
        sim::World ok;
        CHECK(read_world(ok, save_world(fits)) == "");
        CHECK(ok.config().player_slots == 31);
        CHECK(ok.players().slots.size() == 31);
    }
    SUBCASE("a cursor outside the table is refused") {
        sim::World bad = w;
        sim::WorldTestAccess::players(bad).cursor = 32;
        sim::World other;
        const std::string err = read_world(other, save_world(bad));
        CHECK(starts_with(err, "players"));
        CHECK(err.find("cursor") != std::string::npos);
        sim::WorldTestAccess::players(bad).cursor = 31;
        CHECK(read_world(other, save_world(bad)) == "");
    }
    SUBCASE("a player whose index is not its slot is refused") {
        sim::World bad = w;
        sim::WorldTestAccess::players(bad).slots[3]->index = sim::PlayerId{4};
        sim::World other;
        const std::string err = read_world(other, save_world(bad));
        CHECK(starts_with(err, "players"));
        CHECK(err.find("index") != std::string::npos);
        sim::WorldTestAccess::players(bad).slots[3]->index = sim::PlayerId{};
        CHECK(read_world(other, save_world(bad)).find("index") != std::string::npos);
    }
    SUBCASE("a research table of the wrong size is refused") {
        sim::World bad = w;
        sim::WorldTestAccess::players(bad).slots[0]->research.state.pop_back();
        sim::World other;
        std::string err = read_world(other, save_world(bad));
        CHECK(starts_with(err, "players"));
        CHECK(err.find("research.state") != std::string::npos);
        bad = w;
        sim::WorldTestAccess::players(bad).slots[0]->research.progress.push_back(0);
        err = read_world(other, save_world(bad));
        CHECK(err.find("research.progress") != std::string::npos);
        // Another entry count in the configuration makes both fit.
        bad = w;
        sim::WorldTestAccess::config(bad).research_entries = 3;
        for (auto& slot : sim::WorldTestAccess::players(bad).slots) {
            if (slot) {
                slot->research.state.assign(3, 0);
                slot->research.progress.assign(3, 0);
            }
        }
        CHECK(read_world(other, save_world(bad)) == "");
    }
    SUBCASE("a player state byte is an open byte and is kept as it is") {
        // D: player +0x28d stores any value (the setter 0x1001be0d, the loader
        // 0x1001d37a); the stream neither rejects nor normalises one.
        sim::World other;
        sim::World bad = w;
        std::vector<std::uint64_t> hashes;
        for (const std::uint32_t raw : {0U, 1U, 2U, 3U, 4U, 7U, 8U, 255U}) {
            INFO("state " << raw);
            sim::WorldTestAccess::players(bad).slots[3]->state = static_cast<sim::PlayerState>(raw);
            REQUIRE(read_world(other, save_world(bad)) == "");
            CHECK(static_cast<std::uint32_t>(other.players().slots[3]->state) == raw);
            CHECK(other.state_hash() == bad.state_hash());
            hashes.push_back(other.state_hash());
        }
        std::sort(hashes.begin(), hashes.end());
        CHECK(std::adjacent_find(hashes.begin(), hashes.end()) == hashes.end());
    }
    SUBCASE("an upkeep remainder not below the slice count is refused") {
        sim::World bad = w;
        sim::WorldTestAccess::players(bad).slots[3]->upkeep_rem = 15;
        sim::World other;
        const std::string err = read_world(other, save_world(bad));
        CHECK(starts_with(err, "players"));
        CHECK(err.find("upkeep_rem") != std::string::npos);
        sim::WorldTestAccess::players(bad).slots[3]->upkeep_rem = 0xFFFFFFFFU;
        CHECK(read_world(other, save_world(bad)).find("upkeep_rem") != std::string::npos);
        sim::WorldTestAccess::players(bad).slots[3]->upkeep_rem = 14;
        CHECK(read_world(other, save_world(bad)) == "");
        // The configuration decides.
        sim::WorldTestAccess::config(bad).upkeep_slices = 14;
        CHECK(read_world(other, save_world(bad)).find("upkeep_rem") != std::string::npos);
    }
    SUBCASE("a presence byte that is not 0 or 1 is refused") {
        // The table alone: u64 count, slot 0 present (a presence byte and the
        // player), then slot 1's presence byte.
        std::vector<std::uint8_t> table = save_table(w.players());
        Stream slot0;
        expected_player(slot0, *w.players().slots[0]);
        const std::size_t slot1 = 8 + 1 + slot0.v.size();
        REQUIRE(table[slot1] == 0);
        table[slot1] = 2;
        sim::PlayerTable t;
        sim::ReadArchive r(table);
        fields(r, t);
        REQUIRE(r.failed());
        CHECK(starts_with(r.error(), "player[1]"));
        CHECK(r.error().find("bool") != std::string::npos);
    }
    SUBCASE("a count larger than the input is refused before anything is sized") {
        Stream s;
        s.u64(0xFFFFFFFFFFFFULL);
        sim::PlayerTable t;
        sim::ReadArchive r(s.v);
        fields(r, t);
        CHECK(r.failed());
        CHECK(t.slots.empty());
    }
    SUBCASE("a slot count above the table limit is refused before any slot is made") {
        // A valid header, then a count of 2^20 absent slots with the bytes to back
        // it: the remaining input would justify the count (one byte per absent
        // slot), but each slot is a whole Player in memory.
        const std::size_t table_size = save_table(w.players()).size();
        // bytes ends with the players scope, the islands scope (u64 count 0), the
        // figures and events scopes of a world without either (39 + 8 bytes) and the
        // timers_monopoly scope of the default configuration (193 bytes).
        constexpr std::ptrdiff_t kTail = 39 + 8 + 193;
        REQUIRE(bytes.size() > table_size + 8 + kTail);
        Stream s;
        s.v.assign(bytes.begin(),
                   bytes.end() - kTail - 8 - static_cast<std::ptrdiff_t>(table_size));
        s.u64(1ULL << 20);
        for (std::uint32_t i = 0; i < (1U << 20); ++i) {
            s.u8(0);
        }
        s.u16(0);
        s.u32(0);
        s.u64(0); // islands
        s.v.insert(s.v.end(), bytes.end() - kTail, bytes.end());
        sim::World target(5);
        sim::WorldTestAccess::players(target) = full_table(target.config());
        const std::uint64_t before = target.state_hash();
        const std::string err = read_world(target, s.v);
        CHECK(starts_with(err, "players"));
        CHECK(err.find("table limit") != std::string::npos);
        CHECK(target.state_hash() == before);
        // The table alone: nothing is sized, the target keeps no slots.
        Stream t;
        t.u64(1ULL << 20);
        for (std::uint32_t i = 0; i < (1U << 20); ++i) {
            t.u8(0);
        }
        sim::PlayerTable table;
        sim::ReadArchive r(t.v);
        fields(r, table);
        CHECK(r.failed());
        CHECK(table.slots.empty());
        // The limit itself is readable; one more is not.
        sim::PlayerTable limit;
        limit.slots.resize(sim::kSlotTableBound);
        sim::PlayerTable back;
        const std::vector<std::uint8_t> limit_bytes = save_table(limit);
        sim::ReadArchive ok(limit_bytes);
        fields(ok, back);
        CHECK_FALSE(ok.failed());
        CHECK(back.slots.size() == sim::kSlotTableBound);
        limit.slots.resize(sim::kSlotTableBound + 1U);
        const std::vector<std::uint8_t> over_bytes = save_table(limit);
        sim::ReadArchive over(over_bytes);
        sim::PlayerTable none;
        fields(over, none);
        CHECK(over.failed());
        CHECK(none.slots.empty());
    }
    SUBCASE("a failed read leaves the target's player table as it was") {
        sim::World target(5);
        sim::WorldTestAccess::players(target) = full_table(target.config());
        target.advance(10);
        const std::uint64_t before = target.state_hash();
        sim::World bad = w;
        sim::WorldTestAccess::players(bad).cursor = 99;
        CHECK(read_world(target, save_world(bad)) != "");
        CHECK(target.state_hash() == before);
        CHECK(target.players().slots[3].has_value());
        CHECK(target.players().slots[3]->money == 10000);
        CHECK(target.rng().cursor() == 5);
        // A good stream replaces everything.
        CHECK(read_world(target, bytes) == "");
        CHECK(target.state_hash() == w.state_hash());
        CHECK(target.players().slots[1].has_value() == false);
    }
}

TEST_CASE("validate players names the first broken fact") {
    const sim::SimConfig cfg;
    sim::PlayerTable t = populated_table(cfg);
    CHECK(sim::validate_players(t, cfg).empty());
    sim::PlayerTable few = t;
    few.slots.pop_back();
    CHECK(starts_with(sim::validate_players(few, cfg), "players.slots: has 31 entries"));
    sim::PlayerTable many = t;
    many.slots.push_back(std::nullopt);
    CHECK(starts_with(sim::validate_players(many, cfg), "players.slots: has 33 entries"));
    sim::PlayerTable cursor = t;
    cursor.cursor = 32;
    CHECK(starts_with(sim::validate_players(cursor, cfg), "players.cursor"));
    cursor.cursor = 31;
    CHECK(sim::validate_players(cursor, cfg).empty());
    sim::PlayerTable idx = t;
    idx.slots[3]->index = sim::PlayerId{2};
    CHECK(starts_with(sim::validate_players(idx, cfg), "players.slots[3].index"));
    sim::PlayerTable rs = t;
    rs.slots[0]->research.state.push_back(0);
    CHECK(starts_with(sim::validate_players(rs, cfg), "players.slots[0].research.state"));
    sim::PlayerTable rp = t;
    rp.slots[31]->research.progress.clear();
    CHECK(starts_with(sim::validate_players(rp, cfg), "players.slots[31].research.progress"));
    sim::PlayerTable rem = t;
    rem.slots[3]->upkeep_rem = 14; // the largest valid remainder
    CHECK(sim::validate_players(rem, cfg).empty());
    rem.slots[3]->upkeep_rem = 15;
    CHECK(starts_with(sim::validate_players(rem, cfg), "players.slots[3].upkeep_rem"));
    // The configuration decides the sizes.
    sim::SimConfig other = cfg;
    other.player_slots = 33;
    CHECK(starts_with(sim::validate_players(t, other), "players.slots: has 32 entries"));
    other = cfg;
    other.research_entries = 64;
    CHECK(starts_with(sim::validate_players(t, other), "players.slots[0].research.state"));
}
