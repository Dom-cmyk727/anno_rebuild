// The scheduler building blocks against an independent model of the original's
// instruction sequence and hand-derived cases (docs/sim/state.md §5.1, §14.2;
// AnnoWelt.dll 0x10023780, 0x10023a50, 0x1009aa4c–0x1009aa77, 0x100add90).
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/events.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/scheduler.hpp>
#include <anno/sim/slot_list.hpp>

namespace sim = anno::sim;

namespace {

#if defined(__SANITIZE_ADDRESS__)
constexpr bool kSanitized = true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(undefined_behavior_sanitizer)
constexpr bool kSanitized = true;
#else
constexpr bool kSanitized = false;
#endif
#else
constexpr bool kSanitized = false;
#endif

/// Deterministic test generator (an LCG; the test owns it, the sim has none).
struct Lcg {
    std::uint64_t s;
    explicit Lcg(std::uint64_t seed) : s(seed) {}
    std::uint32_t next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>(s >> 33);
    }
    std::uint32_t below(std::uint32_t n) { return next() % n; }
};

/// One divisor of the original and its compiler idiom: signed multiply by a
/// magic number, keep the high half, shift right arithmetically (D 0x10023780
/// /500, 0x100969d0 /50, 0x1008b760 /15000, 0x10095d40 /5000; the figure
/// registry's phase A /250 at 0x1009a8d5..0x1009a907 and the island sweep /2000
/// at 0x1008b794 use the 0x10624dd3 multiplier with other shifts).
struct Magic {
    std::uint32_t div;
    std::int64_t magic;
    int shift;
};
constexpr std::array<Magic, 6> kMagics = {{
    {500, 0x10624dd3, 5},
    {50, 0x51eb851f, 4},
    {15000, 0x45e7b273, 12},
    {5000, 0x68db8bad, 11},
    {250, 0x10624dd3, 4},
    {2000, 0x10624dd3, 7},
}};

/// `imul r/m32` (edx:eax = eax · m), `sar edx, shift`, then the correction that
/// adds the sign bit of the quotient (`shr 31`, `add`).
std::int32_t ref_div(std::int32_t x, const Magic& m) {
    const std::int64_t prod = static_cast<std::int64_t>(x) * m.magic;
    std::int32_t edx = static_cast<std::int32_t>(prod >> 32);
    edx >>= m.shift;
    edx += static_cast<std::int32_t>(static_cast<std::uint32_t>(edx) >> 31);
    return edx;
}

/// The original's visit count: d = movzx (t - stamp); x = d · n in a signed
/// 32-bit register; q = x / div; count = q + 1; compare with n (signed).
std::int32_t ref_visit(std::uint32_t t, std::uint16_t stamp, std::int32_t n, const Magic& m) {
    const std::uint32_t d = static_cast<std::uint16_t>(t - stamp);
    const std::int32_t x = static_cast<std::int32_t>(d * static_cast<std::uint32_t>(n));
    const std::int32_t count = ref_div(x, m) + 1;
    return count < n ? count : n;
}

/// The registry variant: q = x / div, raised to 1, then capped at n.
std::int32_t ref_clamped(std::uint32_t t, std::uint16_t stamp, std::int32_t n, const Magic& m) {
    const std::uint32_t d = static_cast<std::uint16_t>(t - stamp);
    const std::int32_t x = static_cast<std::int32_t>(d * static_cast<std::uint32_t>(n));
    std::int32_t count = ref_div(x, m);
    if (count < 1) {
        count = 1;
    }
    return count < n ? count : n;
}

/// A list of `n` slots; slot i holds 10 · i + 1 unless `holes` names it.
sim::SlotList<int> make_list(std::uint32_t n, const std::vector<std::uint32_t>& holes = {}) {
    sim::SlotList<int> l(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        l.slots[i] = static_cast<int>(10 * i + 1);
    }
    for (const auto h : holes) {
        l.slots[h].reset();
    }
    return l;
}

} // namespace

TEST_CASE("visit_count equals the transcribed instruction sequence on its valid range") {
    const std::array<std::uint32_t, 27> sizes = {
        1,   2,   3,   5,   15,   16,   17,   31,   32,    33,    49,    50,    51,   100,
        499, 500, 501, 997, 1000, 4999, 5000, 5001, 10000, 14999, 15000, 32767, 32768};
    // The magic numbers divide exactly: spot check the transcription itself.
    for (const auto& m : kMagics) {
        CAPTURE(m.div);
        for (std::int32_t x = 0; x < 200000; x += 37) {
            REQUIRE(ref_div(x, m) == x / static_cast<std::int32_t>(m.div));
        }
        REQUIRE(ref_div(2147483647, m) == 2147483647 / static_cast<std::int32_t>(m.div));
    }
    std::uint64_t checked = 0;
    for (const auto& m : kMagics) {
        for (const auto n : sizes) {
            for (std::uint32_t d = 0; d <= 65535; ++d) {
                // The valid range of the original: d · n below 2^31.
                if (static_cast<std::uint64_t>(d) * n >= 0x80000000ULL) {
                    continue;
                }
                const std::uint16_t stamp = 12345;
                const std::uint32_t t = stamp + d;
                const std::uint64_t got = sim::visit_count(t, stamp, n, m.div);
                const auto want = ref_visit(t, stamp, static_cast<std::int32_t>(n), m);
                if (got != static_cast<std::uint64_t>(want)) {
                    FAIL_CHECK("div " << m.div << " n " << n << " d " << d << " got " << got);
                    return;
                }
                ++checked;
            }
        }
    }
    CHECK(checked > 6'000'000);
}

TEST_CASE("visit_count worked values") {
    // City ring: n = 16, div 50.
    CHECK(sim::visit_count(100, 100, 16, 50) == 1);  // d = 0
    CHECK(sim::visit_count(105, 100, 16, 50) == 2);  // 5 · 16 / 50 = 1, + 1
    CHECK(sim::visit_count(110, 100, 16, 50) == 4);  // 160 / 50 = 3, + 1
    CHECK(sim::visit_count(150, 100, 16, 50) == 16); // 800 / 50 = 16, + 1, capped
    // Building list: n = 40, div 500, d = 125: 5000 / 500 = 10, + 1.
    CHECK(sim::visit_count(125, 0, 40, 500) == 11);
    // Player table: n = 32, div 50; 5, 10 and 20 ms steps are d = 2, 5, 10
    // ticks: 64 / 50 = 1, 160 / 50 = 3, 320 / 50 = 6, each + 1.
    CHECK(sim::visit_count(2, 0, 32, 50) == 2);
    CHECK(sim::visit_count(5, 0, 32, 50) == 4);
    CHECK(sim::visit_count(10, 0, 32, 50) == 7);
    CHECK(sim::visit_count(49, 0, 32, 50) == 32); // 1568 / 50 = 31, + 1
    CHECK(sim::visit_count(48, 0, 32, 50) == 31); // 1536 / 50 = 30, + 1
    // Object manager: n = 5000 deposits, div 5000.
    CHECK(sim::visit_count(5000, 0, 5000, 5000) == 5000);
    CHECK(sim::visit_count(2500, 0, 5000, 5000) == 2501);
    // An empty list and a list of one.
    CHECK(sim::visit_count(60000, 0, 0, 500) == 0);
    CHECK(sim::visit_count(0, 0, 1, 500) == 1);
    CHECK(sim::visit_count(60000, 0, 1, 500) == 1);
}

TEST_CASE("visit_count and visit_count_clamped are defined for a zero divisor") {
    // The validation rejects every zero divisor (docs/sim/state.md §3.1); a
    // zero that got through must not trap: every slot is due.
    CHECK(sim::visit_count(5, 0, 40, 0) == 40);
    CHECK(sim::visit_count(0, 0, 1, 0) == 1);
    CHECK(sim::visit_count(5, 0, 0, 0) == 0);
    CHECK(sim::visit_count_clamped(5, 0, 40, 0) == 40);
    CHECK(sim::visit_count_clamped(5, 0, 0, 0) == 0);
    // A nonzero divisor is untouched by the guard.
    CHECK(sim::visit_count(5, 0, 40, 1) == 40);
    CHECK(sim::visit_count(0, 0, 40, 1) == 1);
}

TEST_CASE("visit_count compares the stamp as 16 bits") {
    // t is the low 32 bits of the tick; only (u16)(t - stamp) counts.
    CHECK(sim::visit_count(0x10010, 0xfff0, 500, 500) == 33); // d = 0x20 = 32: 32 + 1
    CHECK(sim::visit_count(0x20005, 5, 500, 500) == 1);       // d = 0 after 65536 ticks
    CHECK(sim::visit_count(0x20005 + 1, 5, 500, 500) == 2);   // d = 1
    CHECK(sim::visit_count(65540, 5, 500, 500) == 500);       // d = 65535: capped
    CHECK(sim::visit_count(3, 0xfffe, 500, 500) == 6);        // t below the stamp: d = 5
    // A tick below the stamp: (0 - 0xfff0) mod 2^16 = 16.
    CHECK(sim::visit_count(0, 0xfff0, 500, 500) == 17); // d = 16
    CHECK(sim::visit_count(0x0000fffe, 0xfffe, 500, 500) == 1);
}

TEST_CASE("visit_count is widened where the original overflows") {
    // D 0x10023780 multiplies d · n in a signed 32-bit register: from
    // d · n >= 2^31 the original's product goes negative (F3). n = 40 000
    // slots first reach that at d = 53 688.
    const auto& m = kMagics[0];
    const std::uint32_t n = 40000;
    // d = 53 687: still in range, both agree.
    const auto n_i32 = static_cast<std::int32_t>(n);
    CHECK(sim::visit_count(53687, 0, n, 500) == n);
    CHECK(ref_visit(53687, 0, n_i32, m) == n_i32);
    // d = 53 688: the original's count is negative (no visits), ours is the cap.
    CHECK(ref_visit(53688, 0, n_i32, m) < 0);
    CHECK(sim::visit_count(53688, 0, n, 500) == n);
    CHECK(sim::visit_count(65535, 0, n, 500) == n);
    // Monotone in d, and exact below the cap: d · n / div + 1.
    std::uint64_t prev = 0;
    for (std::uint32_t d = 0; d <= 65535; ++d) {
        const std::uint64_t c = sim::visit_count(d, 0, n, 500);
        REQUIRE(c >= prev);
        const std::uint64_t exact = static_cast<std::uint64_t>(d) * n / 500 + 1;
        REQUIRE(c == (exact < n ? exact : n));
        prev = c;
    }
    CHECK(sim::visit_count(5, 0, n, 500) == 401);
    // Nor is the product limited to 32 bits unsigned: a list of a million slots
    // with a modded divisor of 100 000 at d = 65 535 (d · n = 6.5e10).
    CHECK(sim::visit_count(65535, 0, 1'000'000, 100'000) == 655351);
    CHECK(sim::visit_count_clamped(65535, 0, 1'000'000, 100'000) == 655350);
}

TEST_CASE("visit_count_clamped has no plus one and is raised to one") {
    CHECK(sim::visit_count_clamped(0, 0, 32, 50) == 1);   // d = 0
    CHECK(sim::visit_count_clamped(1, 0, 32, 50) == 1);   // 32 / 50 = 0, raised to 1
    CHECK(sim::visit_count_clamped(2, 0, 32, 50) == 1);   // 64 / 50 = 1 (visit_count: 2)
    CHECK(sim::visit_count_clamped(10, 0, 32, 50) == 6);  // 320 / 50 = 6 (visit_count: 7)
    CHECK(sim::visit_count_clamped(48, 0, 32, 50) == 30); // 1536 / 50 = 30
    CHECK(sim::visit_count_clamped(49, 0, 32, 50) == 31); // 1568 / 50 = 31
    CHECK(sim::visit_count_clamped(50, 0, 32, 50) == 32); // 1600 / 50 = 32
    CHECK(sim::visit_count_clamped(60000, 0, 32, 50) == 32);
    CHECK(sim::visit_count_clamped(7, 0, 0, 50) == 0); // an empty list
    CHECK(sim::visit_count_clamped(7, 0, 1, 50) == 1);
    // Phase A's divisor (figure_a_div) with the same clamp.
    CHECK(sim::visit_count_clamped(250, 0, 250, 250) == 250);
    CHECK(sim::visit_count_clamped(125, 0, 100, 250) == 50);
    // One smaller than visit_count wherever 1 <= d · n / div < n.
    for (std::uint32_t d = 2; d < 49; ++d) {
        REQUIRE(sim::visit_count_clamped(d, 0, 32, 50) + 1 == sim::visit_count(d, 0, 32, 50));
    }
    // The stamp wraps as in visit_count.
    CHECK(sim::visit_count_clamped(0x10010, 0xfff0, 50, 50) == 32);
    // Against the transcription (imul, sar) over the valid range, for the
    // phase B divisor 50 (0x51eb851f, sar 4) and the other multipliers.
    for (const auto& m : kMagics) {
        for (const std::uint32_t n : {1U, 2U, 7U, 32U, 33U, 50U, 100U, 1000U, 32768U}) {
            for (std::uint32_t d = 0; d <= 65535; ++d) {
                if (static_cast<std::uint64_t>(d) * n >= 0x80000000ULL) {
                    continue;
                }
                const std::uint64_t got = sim::visit_count_clamped(d, 0, n, m.div);
                const auto want = ref_clamped(d, 0, static_cast<std::int32_t>(n), m);
                if (got != static_cast<std::uint64_t>(want)) {
                    FAIL_CHECK("div " << m.div << " n " << n << " d " << d << " got " << got);
                    return;
                }
            }
        }
    }
}

TEST_CASE("run_slot_list visits live slots from the cursor and moves the cursor") {
    // n = 8, div 500. d = 125 -> 125 · 8 / 500 = 2, + 1 = 3 slots reached.
    auto l = make_list(8, {2, 5});
    std::vector<int> seen;
    sim::run_slot_list(l, 0, 125, 500, [&](int& v) { seen.push_back(v); });
    CHECK(seen == std::vector<int>{1, 11}); // slots 0, 1; slot 2 is a hole
    CHECK(l.cursor == 3);

    // The next call continues at slot 3: d = 250 -> 4 + 1 = 5 slots: 3, 4, 5, 6, 7.
    seen.clear();
    sim::run_slot_list(l, 125, 375, 500, [&](int& v) { seen.push_back(v); });
    CHECK(seen == std::vector<int>{31, 41, 61, 71}); // slot 5 is a hole
    CHECK(l.cursor == 0);                            // 3 + 5 = 8 wraps to 0
}

TEST_CASE("run_slot_list counts holes and wraps round") {
    auto l = make_list(8, {2, 5});
    l.cursor = 6;
    std::vector<int> seen;
    // d = 500 -> 8 + 1, capped at n = 8: one full circle from slot 6.
    sim::run_slot_list(l, 0, 500, 500, [&](int& v) { seen.push_back(v); });
    CHECK(seen == std::vector<int>{61, 71, 1, 11, 31, 41});
    CHECK(l.cursor == 6);

    // A single visit on a hole moves the cursor but visits nothing.
    l.cursor = 2;
    seen.clear();
    sim::run_slot_list(l, 7, 7, 500, [&](int& v) { seen.push_back(v); }); // d = 0 -> 1
    CHECK(seen.empty());
    CHECK(l.cursor == 3);

    // n includes the holes: 40 slots with two live ones are still 11 steps at d = 125.
    auto sparse = make_list(40, {});
    for (std::uint32_t i = 0; i < 40; ++i) {
        if (i != 3 && i != 30) {
            sparse.slots[i].reset();
        }
    }
    seen.clear();
    sim::run_slot_list(sparse, 0, 125, 500, [&](int& v) { seen.push_back(v); });
    CHECK(sparse.cursor == 11);
    CHECK(seen == std::vector<int>{31});
}

TEST_CASE("run_slot_list on an empty or inconsistent list") {
    // n = 0: nothing, cursor untouched.
    sim::SlotList<int> none(0);
    int calls = 0;
    sim::run_slot_list(none, 0, 1000, 500, [&](int&) { ++calls; });
    CHECK(calls == 0);
    CHECK(none.cursor == 0);

    // A cursor at or beyond n is inconsistent (A8): defined, nothing visited,
    // the list is left as it is.
    auto l = make_list(8);
    l.cursor = 8;
    sim::run_slot_list(l, 0, 1000, 500, [&](int&) { ++calls; });
    CHECK(calls == 0);
    CHECK(l.cursor == 8);
    l.cursor = 4000000000U;
    sim::run_slot_list(l, 0, 1000, 500, [&](int&) { ++calls; });
    CHECK(calls == 0);
    CHECK(l.cursor == 4000000000U);

    // The visit gets the element itself.
    auto m = make_list(4);
    m.cursor = 3;
    sim::run_slot_list(m, 0, 0, 500, [&](int& v) { v = -v; });
    CHECK(*m.slots[3] == -31);
    CHECK(*m.slots[0] == 1);
}

TEST_CASE("run_slot_list on a list of 40000 slots") {
    // The original overflows its signed product for d · n >= 2^31 (F3).
    sim::SlotList<int> l(40000);
    for (const std::uint32_t s : {7U, 20000U, 39999U}) {
        l.slots[s] = static_cast<int>(s);
    }
    std::vector<int> seen;
    // d = 5: 5 · 40000 / 500 + 1 = 401 slots: only slot 7 is reached.
    sim::run_slot_list(l, 0, 5, 500, [&](int& v) { seen.push_back(v); });
    CHECK(seen == std::vector<int>{7});
    CHECK(l.cursor == 401);
    // d = 60000: past the original's overflow, a full circle here.
    seen.clear();
    l.cursor = 100;
    sim::run_slot_list(l, 0, 60000, 500, [&](int& v) { seen.push_back(v); });
    CHECK(seen == std::vector<int>{20000, 39999, 7});
    CHECK(l.cursor == 100);
}

TEST_CASE("run_slot_list matches a naive model on random lists and ticks") {
    Lcg rng(1503);
    const std::uint32_t rounds = kSanitized ? 3000 : 20000;
    for (std::uint32_t round = 0; round < rounds; ++round) {
        const std::uint32_t n = 1 + rng.below(60);
        sim::SlotList<int> l(n);
        std::vector<int> model(n, 0);
        for (std::uint32_t i = 0; i < n; ++i) {
            if (rng.below(3) != 0) {
                l.slots[i] = static_cast<int>(i + 1);
                model[i] = static_cast<int>(i + 1);
            }
        }
        l.cursor = rng.below(n);
        const std::uint32_t cursor = l.cursor;
        const std::uint16_t stamp = static_cast<std::uint16_t>(rng.next());
        // Ticks near the 16-bit wrap and beyond the 32-bit wrap included.
        const std::uint32_t t = rng.below(4) == 0 ? rng.below(40) : rng.next();
        const auto& m = kMagics[rng.below(static_cast<std::uint32_t>(kMagics.size()))];
        const std::int32_t count = ref_visit(t, stamp, static_cast<std::int32_t>(n), m);
        std::vector<int> want;
        for (std::int32_t k = 0; k < count; ++k) {
            const int v = model[(cursor + static_cast<std::uint32_t>(k)) % n];
            if (v != 0) {
                want.push_back(v);
            }
        }
        const std::uint32_t want_cursor = (cursor + static_cast<std::uint32_t>(count)) % n;
        std::vector<int> got;
        sim::run_slot_list(l, stamp, t, m.div, [&](int& v) { got.push_back(v); });
        REQUIRE(got == want);
        REQUIRE(l.cursor == want_cursor);
    }
}

TEST_CASE("run_scheduled_lists visits the scheduled lists in order with one shared stamp") {
    // Six lists of 40 slots, as a city has: L0 stalls are never scheduled.
    std::vector<sim::SlotList<int>> lists;
    for (std::uint32_t i = 0; i < 6; ++i) {
        sim::SlotList<int> l(40);
        for (std::uint32_t s = 0; s < 40; ++s) {
            l.slots[s] = static_cast<int>(100 * i + s);
        }
        lists.push_back(std::move(l));
    }
    const std::vector<sim::ListId> schedule = {5, 1, 4, 3, 2};
    std::uint16_t stamp = 0;
    std::vector<std::pair<sim::ListId, int>> seen;
    std::vector<std::uint16_t> stamps_seen;
    // d = 250: 250 · 40 / 500 + 1 = 21 slots per list.
    sim::run_scheduled_lists(lists, schedule, stamp, 250, 500, [&](sim::ListId lid, int& v) {
        seen.emplace_back(lid, v);
        stamps_seen.push_back(stamp);
    });
    REQUIRE(seen.size() == 5 * 21);
    // List order 5, 1, 4, 3, 2, 21 elements each, slots 0..20.
    for (std::size_t k = 0; k < seen.size(); ++k) {
        const sim::ListId want_list = schedule[k / 21];
        CHECK(seen[k].first == want_list);
        CHECK(seen[k].second == static_cast<int>(100 * want_list + k % 21));
    }
    // Every list started from the same stamp, and the stamp is written last.
    for (const auto s : stamps_seen) {
        CHECK(s == 0);
    }
    CHECK(stamp == 250);
    for (const sim::ListId lid : schedule) {
        CHECK(lists[lid].cursor == 21);
    }
    // The stall list was not touched.
    CHECK(lists[0].cursor == 0);

    // The next call has d = 0: one slot per list, from slot 21.
    seen.clear();
    sim::run_scheduled_lists(lists, schedule, stamp, 250, 500,
                             [&](sim::ListId lid, int& v) { seen.emplace_back(lid, v); });
    REQUIRE(seen.size() == 5);
    CHECK(seen[0] == std::pair<sim::ListId, int>{5, 521});
    CHECK(stamp == 250);
    CHECK(lists[5].cursor == 22);
}

TEST_CASE("run_scheduled_lists writes the 16-bit stamp and skips unknown lists") {
    std::vector<sim::SlotList<int>> lists(2, sim::SlotList<int>(4));
    for (auto& l : lists) {
        l.slots[0] = 1;
    }
    std::uint16_t stamp = 5;
    int calls = 0;
    // 2 is the boundary id (== lists.size()), 7 and 0xffffffff are far beyond.
    const std::vector<sim::ListId> schedule = {1, 2, 7, 0xffffffffU, 0};
    // t = 0x12345: the stamp is its low 16 bits; lists 2, 7 and 0xffffffff do
    // not exist.
    sim::run_scheduled_lists(lists, schedule, stamp, 0x12345, 500,
                             [&](sim::ListId, int&) { ++calls; });
    CHECK(stamp == 0x2345);
    // d = (u16)(0x12345 - 5) = 0x2340 = 9024: n = 4 -> capped at 4: every slot of
    // lists 1 and 0 once, only slot 0 is live.
    CHECK(calls == 2);
    // An empty schedule still writes the stamp.
    sim::run_scheduled_lists(lists, std::span<const sim::ListId>{}, stamp, 99, 500,
                             [&](sim::ListId, int&) { ++calls; });
    CHECK(stamp == 99);
    CHECK(calls == 2);
}

namespace {

/// A queue with an id counter.
struct Q {
    sim::EventQueue queue;
    sim::IdCounters ids;
    /// Queues an event falling due at `due`; `tag` travels in `value`, the
    /// tile is (tag, tag) so a handler can cancel it by tile.
    void add(std::uint64_t due, std::uint32_t tag) {
        sim::TimedEvent e;
        e.value = tag;
        e.tile = sim::TileXY{static_cast<std::int32_t>(tag), static_cast<std::int32_t>(tag)};
        queue.enqueue(ids, due, 0, e);
    }
    [[nodiscard]] std::vector<std::uint32_t> tags() const {
        std::vector<std::uint32_t> out;
        for (const auto& [key, e] : queue.q) {
            out.push_back(e.value);
        }
        return out;
    }
};

using Tags = std::vector<std::uint32_t>;

} // namespace

TEST_CASE("event queue enqueue keys by due tick and sequence") {
    sim::EventQueue queue;
    sim::IdCounters ids;
    sim::TimedEvent e;
    queue.enqueue(ids, 100, 50, e);
    queue.enqueue(ids, 100, 25000, e);
    queue.enqueue(ids, 120, 30, e); // due 150, as the first
    CHECK(ids.next_event_seq == 4);
    REQUIRE(queue.q.size() == 3);
    auto it = queue.q.begin();
    CHECK(it->first == sim::EventKey{150, 1}); // FIFO among equal due ticks
    ++it;
    CHECK(it->first == sim::EventKey{150, 3});
    ++it;
    CHECK(it->first == sim::EventKey{25100, 2});
    // A later event with an earlier due tick is served first.
    queue.enqueue(ids, 10, 0, e);
    CHECK(queue.q.begin()->first == sim::EventKey{10, 4});
}

TEST_CASE("event queue cancel_tile removes the first match") {
    sim::EventQueue queue;
    sim::IdCounters ids;
    sim::TimedEvent a;
    a.kind = sim::EventKind::TileCleanup;
    a.subtype = 4;
    a.tile = {3, 4};
    a.value = 1;
    sim::TimedEvent b = a;
    b.value = 2;
    sim::TimedEvent c = a;
    c.kind = sim::EventKind::DelayedHit;
    c.value = 3;
    sim::TimedEvent d = a;
    d.subtype = 2;
    d.value = 4;
    queue.enqueue(ids, 9, 0, c);
    queue.enqueue(ids, 5, 0, a);
    queue.enqueue(ids, 5, 0, b);
    queue.enqueue(ids, 7, 0, d);
    // Not a match: other tile, kind or subtype.
    CHECK_FALSE(queue.cancel_tile(sim::EventKind::TileCleanup, 4, sim::TileXY{9, 9}));
    CHECK_FALSE(queue.cancel_tile(sim::EventKind::DelayedHit, 2, sim::TileXY{3, 4}));
    CHECK(queue.q.size() == 4);
    // The first matching event in queue order (a, due 5 seq 2) goes, b stays.
    CHECK(queue.cancel_tile(sim::EventKind::TileCleanup, 4, sim::TileXY{3, 4}));
    std::vector<std::uint32_t> left;
    for (const auto& [key, e] : queue.q) {
        left.push_back(e.value);
    }
    CHECK(left == Tags{2, 4, 3});
    // The kind is part of the match; the subtype is 4 (kTileSubtype).
    CHECK(queue.cancel_tile(sim::EventKind::DelayedHit, 4, sim::TileXY{3, 4}));
    CHECK(queue.cancel_tile(sim::EventKind::TileCleanup, 4, sim::TileXY{3, 4}));
    // What is left is d (subtype 2): a tile cancel does not take it.
    CHECK(queue.q.size() == 1);
    CHECK_FALSE(queue.cancel_tile(sim::EventKind::TileCleanup, 4, sim::TileXY{3, 4}));
    CHECK(queue.q.size() == 1);
}

TEST_CASE("event queue cancel_tile takes only subtype 4 and both tile coordinates") {
    // D 0x100ade40: a cancel with a subtype other than 4 never removes anything
    // (0x100ade79), even when the stored event has that subtype and tile.
    sim::EventQueue queue;
    sim::IdCounters ids;
    sim::TimedEvent e;
    e.kind = sim::EventKind::DelayedHit;
    e.subtype = 2;
    e.tile = {3, 4};
    queue.enqueue(ids, 1, 0, e);
    CHECK_FALSE(queue.cancel_tile(sim::EventKind::DelayedHit, 2, sim::TileXY{3, 4}));
    CHECK_FALSE(queue.cancel_tile(sim::EventKind::DelayedHit, 0, sim::TileXY{3, 4}));
    CHECK(queue.q.size() == 1);

    // Both coordinates must match: one equal coordinate is not enough.
    sim::TimedEvent t;
    t.tile = {3, 4};
    queue.enqueue(ids, 2, 0, t);
    CHECK_FALSE(queue.cancel_tile(sim::EventKind::TileCleanup, 4, sim::TileXY{3, 9}));
    CHECK_FALSE(queue.cancel_tile(sim::EventKind::TileCleanup, 4, sim::TileXY{9, 4}));
    CHECK_FALSE(queue.cancel_tile(sim::EventKind::TileCleanup, 4, sim::TileXY{4, 3}));
    CHECK(queue.q.size() == 2);
    CHECK(queue.cancel_tile(sim::EventKind::TileCleanup, 4, sim::TileXY{3, 4}));
    CHECK(queue.q.size() == 1);
}

TEST_CASE("pop_due serves due events in queue order and leaves the rest") {
    Q q;
    q.add(7, 3);
    q.add(5, 1);
    q.add(5, 2);
    q.add(11, 4);
    Tags got;
    std::vector<std::size_t> sizes;
    sim::pop_due(q.queue, 10, false, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        sizes.push_back(q.queue.q.size()); // the event is out of the queue already
    });
    CHECK(got == Tags{1, 2, 3}); // FIFO among due 5, then due 7
    CHECK(sizes == std::vector<std::size_t>{3, 2, 1});
    CHECK(q.tags() == Tags{4});
    // due == now is due; nothing due leaves the queue as it is.
    got.clear();
    sim::pop_due(q.queue, 10, false, [&](const sim::TimedEvent& e) { got.push_back(e.value); });
    CHECK(got.empty());
    sim::pop_due(q.queue, 11, false, [&](const sim::TimedEvent& e) { got.push_back(e.value); });
    CHECK(got == Tags{4});
    CHECK(q.queue.q.empty());
    // An empty queue is fine.
    sim::pop_due(q.queue, 1000, false, [&](const sim::TimedEvent&) { FAIL("no event"); });
}

TEST_CASE("pop_due hands the payload to the handler") {
    sim::EventQueue queue;
    sim::IdCounters ids;
    sim::TimedEvent e;
    e.kind = sim::EventKind::DelayedHit;
    e.subtype = 2;
    e.value = 77;
    e.tile = {5, 6};
    e.target = sim::FigureId{12};
    e.attacker_kind = 2;
    e.attacker = sim::FigureId{13};
    queue.enqueue(ids, 3, 0, e);
    int calls = 0;
    sim::pop_due(queue, 3, false, [&](const sim::TimedEvent& got) {
        ++calls;
        CHECK(got.kind == sim::EventKind::DelayedHit);
        CHECK(got.subtype == 2);
        CHECK(got.value == 77);
        CHECK(got.tile == sim::TileXY{5, 6});
        CHECK(got.target == sim::FigureId{12});
        CHECK(got.attacker_kind == 2);
        CHECK(got.attacker == sim::FigureId{13});
    });
    CHECK(calls == 1);
}

TEST_CASE("pop_due does not see an event enqueued in front of the saved node") {
    // D3 / OQ-038: A (due 5) and B (due 7) are due at now = 10. While A runs it
    // enqueues N with due 6: in key order that is between A and the saved
    // node B, so it waits for the next advance.
    Q q;
    q.add(5, 1);
    q.add(7, 2);
    Tags got;
    sim::pop_due(q.queue, 10, false, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        if (e.value == 1) {
            q.add(6, 9);
        }
    });
    CHECK(got == Tags{1, 2});
    CHECK(q.tags() == Tags{9});
    got.clear();
    sim::pop_due(q.queue, 10, false, [&](const sim::TimedEvent& e) { got.push_back(e.value); });
    CHECK(got == Tags{9});
}

TEST_CASE("pop_due sees an event enqueued behind the saved node") {
    Q q;
    q.add(5, 1);
    q.add(7, 2);
    Tags got;
    sim::pop_due(q.queue, 10, false, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        if (e.value == 1) {
            q.add(8, 9); // behind B
        }
    });
    CHECK(got == Tags{1, 2, 9});
    CHECK(q.queue.q.empty());

    // The same due tick as the saved node falls behind it (FIFO by sequence);
    // the same due tick as the running event falls in front of the saved node.
    q.add(5, 1);
    q.add(7, 2);
    got.clear();
    sim::pop_due(q.queue, 10, false, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        if (e.value == 1) {
            q.add(7, 8); // equal due to B, later sequence: behind
            q.add(5, 9); // equal due to A, later sequence but before B: in front
        }
    });
    CHECK(got == Tags{1, 2, 8});
    CHECK(q.tags() == Tags{9});

    // Behind the saved node but not yet due: it stays.
    Q r;
    r.add(5, 1);
    r.add(7, 2);
    got.clear();
    sim::pop_due(r.queue, 10, false, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        if (e.value == 1) {
            r.add(30, 9);
        }
    });
    CHECK(got == Tags{1, 2});
    CHECK(r.tags() == Tags{9});
}

TEST_CASE("pop_due does not see an event enqueued when the saved node was the last") {
    Q q;
    q.add(5, 1);
    Tags got;
    sim::pop_due(q.queue, 10, false, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        q.add(5, 9); // due, but there was no node after A
    });
    CHECK(got == Tags{1});
    CHECK(q.tags() == Tags{9});

    // The last due event followed by a future one: the saved node is not due,
    // the loop ends there.
    Q r;
    r.add(5, 1);
    r.add(50, 2);
    got.clear();
    sim::pop_due(r.queue, 10, false, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        r.add(6, 9);
    });
    CHECK(got == Tags{1});
    CHECK(r.tags() == Tags{9, 2});
}

TEST_CASE("pop_due ends when the handler removes the saved node") {
    // F19: the original would follow a freed pointer; we stop. A handler
    // removing a node that is not the saved one does not end the loop.
    Q q;
    q.add(5, 1);
    q.add(6, 2);
    q.add(7, 3);
    Tags got;
    sim::pop_due(q.queue, 10, false, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        if (e.value == 1) {
            // Event 2 is the saved node.
            REQUIRE(q.queue.cancel_tile(sim::EventKind::TileCleanup, 4, sim::TileXY{2, 2}));
        }
    });
    CHECK(got == Tags{1});
    CHECK(q.tags() == Tags{3});

    Q r;
    r.add(5, 1);
    r.add(6, 2);
    r.add(7, 3);
    got.clear();
    sim::pop_due(r.queue, 10, false, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        if (e.value == 1) {
            // Event 3 is two nodes ahead, not the saved one.
            REQUIRE(r.queue.cancel_tile(sim::EventKind::TileCleanup, 4, sim::TileXY{3, 3}));
        }
    });
    CHECK(got == Tags{1, 2});
    CHECK(r.queue.q.empty());
}

TEST_CASE("pop_due with drain_new serves events enqueued in front too") {
    // Rules::events_drain_new: the first node is read again after every dispatch.
    Q q;
    q.add(5, 1);
    q.add(7, 2);
    Tags got;
    sim::pop_due(q.queue, 10, true, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        if (e.value == 1) {
            q.add(6, 9);
        }
    });
    CHECK(got == Tags{1, 9, 2});
    CHECK(q.queue.q.empty());

    // ... and when the saved node was the last.
    Q r;
    r.add(5, 1);
    got.clear();
    sim::pop_due(r.queue, 10, true, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        if (e.value == 1) {
            r.add(5, 9);
        }
    });
    CHECK(got == Tags{1, 9});
    CHECK(r.queue.q.empty());

    // A removed neighbour does not end it, a new event that is not due stays.
    Q s;
    s.add(5, 1);
    s.add(6, 2);
    s.add(7, 3);
    got.clear();
    sim::pop_due(s.queue, 10, true, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        if (e.value == 1) {
            REQUIRE(s.queue.cancel_tile(sim::EventKind::TileCleanup, 4, sim::TileXY{2, 2}));
            s.add(99, 9);
        }
    });
    CHECK(got == Tags{1, 3});
    CHECK(s.tags() == Tags{9});
}

TEST_CASE("pop_due unlinks the event before its handler runs") {
    // D 0x100addb3..0x100addf5: payload copied, node unlinked and freed, then
    // the handler is called, so a cancel of the running event's own tile finds
    // nothing.
    Q r;
    r.add(5, 7);
    bool found = true;
    sim::pop_due(r.queue, 10, false, [&](const sim::TimedEvent& e) {
        found = r.queue.cancel_tile(sim::EventKind::TileCleanup, 4, e.tile);
    });
    CHECK_FALSE(found);
    CHECK(r.queue.q.empty());

    // Two events on one tile: the running one is gone, so the cancel takes the
    // second, which is the saved next node (F19: the loop then ends).
    Q q;
    q.add(5, 1);
    q.add(6, 1);
    Tags got;
    bool cancelled_second = false;
    sim::pop_due(q.queue, 10, false, [&](const sim::TimedEvent& e) {
        got.push_back(e.value);
        cancelled_second = q.queue.cancel_tile(sim::EventKind::TileCleanup, 4, e.tile);
    });
    CHECK(cancelled_second);
    CHECK(got == Tags{1});
    CHECK(q.queue.q.empty());
}

TEST_CASE("pop_due stops at the dispatch cap and keeps the rest queued") {
    // A handler that always enqueues a delay-0 event: with drain_new the loop
    // would never end; the cap ends it and reports it.
    for (const bool drain : {true, false}) {
        CAPTURE(drain);
        Q q;
        q.add(5, 1);
        q.add(5, 2); // the saved node: without drain_new the chain follows it
        std::uint64_t calls = 0;
        const sim::PopResult r = sim::pop_due(
            q.queue, 10, drain,
            [&](const sim::TimedEvent&) {
                ++calls;
                q.add(10, 9); // due now, behind everything queued so far
            },
            100);
        CHECK(r.capped);
        CHECK(r.dispatched == 100);
        CHECK(calls == 100);
        // Everything after the cap stays queued for the next call.
        CHECK_FALSE(q.queue.q.empty());
        const std::size_t left = q.queue.q.size();
        const sim::PopResult next =
            sim::pop_due(q.queue, 10, drain, [&](const sim::TimedEvent&) {}, 1000);
        CHECK_FALSE(next.capped);
        CHECK(next.dispatched == left);
        CHECK(q.queue.q.empty());
    }
}

TEST_CASE("pop_due cap edge cases") {
    // Exactly as many due events as the cap: all served, not capped.
    Q q;
    for (std::uint32_t i = 0; i < 5; ++i) {
        q.add(1, i + 1);
    }
    const auto r = sim::pop_due(q.queue, 1, false, [&](const sim::TimedEvent&) {}, 5);
    CHECK(r.dispatched == 5);
    CHECK_FALSE(r.capped);

    // One more due event than the cap: capped, one left.
    Q s;
    for (std::uint32_t i = 0; i < 6; ++i) {
        s.add(1, i + 1);
    }
    const auto t = sim::pop_due(s.queue, 1, false, [&](const sim::TimedEvent&) {}, 5);
    CHECK(t.dispatched == 5);
    CHECK(t.capped);
    CHECK(s.tags() == Tags{6});

    // A cap of 0 serves nothing; nothing due is not capped.
    const auto z = sim::pop_due(s.queue, 1, false, [&](const sim::TimedEvent&) { FAIL("no"); }, 0);
    CHECK(z.dispatched == 0);
    CHECK(z.capped);
    const auto none =
        sim::pop_due(s.queue, 0, false, [&](const sim::TimedEvent&) { FAIL("no"); }, 0);
    CHECK_FALSE(none.capped);

    // The default argument is the default cap, far above a game's event count.
    CHECK(sim::kEventsMaxDispatchDefault >= 1024);
    const auto d = sim::pop_due(s.queue, 1, false, [&](const sim::TimedEvent&) {});
    CHECK(d.dispatched == 1);
    CHECK_FALSE(d.capped);
}
