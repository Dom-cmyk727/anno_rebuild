// The slot list against hand-derived slot sequences and a naive transliteration
// of the original's loop (docs/sim/state.md §5.2, §14.2; AnnoWelt.dll
// 0x10026a00, 0x100255c0).
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/config.hpp>
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

/// Inserts into a fresh list until it has exactly `size` slots, all occupied
/// (the sizes the growth sequence passes through: 10, 12, 15, ...); the value
/// of slot s is 1000 + s and the hint ends on the last slot.
sim::SlotList<int> full_list(std::uint32_t size, const sim::SlotParams& p = {}) {
    sim::SlotList<int> l;
    while (l.slots.size() < size || l.live() < l.slots.size()) {
        const sim::Slot s = l.insert(0, p);
        l.slots[s] = static_cast<int>(1000 + s);
    }
    return l;
}

/// The original's insert transliterated step by step: a counter walks from
/// hint + 1, wrapping by comparison (no modulo), growth by the explicit rule.
struct Naive {
    std::vector<int> v{0};
    int hint = -1;
    sim::SlotPolicy policy = sim::SlotPolicy::Original;
    std::uint32_t min_growth = 10;
    /// The scheduler's cursor: the original's insert never writes it, so the
    /// model keeps it fixed and any write by SlotList::insert or remove shows.
    std::uint32_t cursor = 0;

    int insert(int value) {
        const int n = static_cast<int>(v.size());
        int found = -1;
        if (policy == sim::SlotPolicy::LowestFree) {
            for (int i = 0; i < n; ++i) {
                if (v[static_cast<std::size_t>(i)] == 0) {
                    found = i;
                    break;
                }
            }
        } else {
            int idx = hint + 1;
            for (int k = 0; k < n; ++k) {
                if (idx >= n) {
                    idx = 0;
                }
                if (v[static_cast<std::size_t>(idx)] == 0) {
                    found = idx;
                    break;
                }
                ++idx;
            }
        }
        if (found < 0) {
            int grown = n + n / 4;
            if (grown < static_cast<int>(min_growth)) {
                grown = static_cast<int>(min_growth);
            }
            if (grown <= n) {
                grown = n + 1;
            }
            v.resize(static_cast<std::size_t>(grown), 0);
            found = n;
        }
        v[static_cast<std::size_t>(found)] = value;
        hint = found;
        return found;
    }
    void remove(int slot) {
        if (slot >= 0 && slot < static_cast<int>(v.size())) {
            v[static_cast<std::size_t>(slot)] = 0;
        }
    }
};

/// Whether the list holds exactly the naive model's state.
bool same_state(const sim::SlotList<int>& l, const Naive& m) {
    if (l.slots.size() != m.v.size() || l.hint != m.hint || l.cursor != m.cursor) {
        return false;
    }
    for (std::size_t i = 0; i < m.v.size(); ++i) {
        const int got = l.slots[i] ? *l.slots[i] : 0;
        if (got != m.v[i]) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("slot list fresh state is one null element") {
    // D 0x100236a0, A7: the constructor creates one null element; hint -1.
    const sim::SlotList<int> l;
    CHECK(l.slots.size() == 1);
    CHECK_FALSE(l.slots[0].has_value());
    CHECK(l.hint == -1);
    CHECK(l.cursor == 0);
    CHECK(l.live() == 0);
    const sim::SlotList<int> five(5);
    CHECK(five.slots.size() == 5);
    CHECK(five.live() == 0);
    const sim::SlotList<int> none(0);
    CHECK(none.slots.empty());
    CHECK(none.hint == -1);
}

TEST_CASE("slot list growth sequence is 1 10 12 15 18 22 27 33 41 51 63 78 97") {
    sim::SlotList<int> l;
    const sim::SlotParams p;
    std::vector<std::size_t> sizes{l.slots.size()};
    for (int i = 0; i < 97; ++i) {
        const std::size_t before = l.slots.size();
        const sim::Slot s = l.insert(i, p);
        if (l.slots.size() != before) {
            sizes.push_back(l.slots.size());
            // The new element takes the old length as its slot; the rest of
            // the new elements are null.
            CHECK(s == before);
            CHECK(l.live() == before + 1);
        }
        CHECK(l.hint == static_cast<std::int64_t>(s));
        CHECK(l.slots[s].value_or(-1) == i);
    }
    CHECK(sizes == std::vector<std::size_t>{1, 10, 12, 15, 18, 22, 27, 33, 41, 51, 63, 78, 97});
    CHECK(l.live() == 97);
    // The 98th insert grows to 97 + 24 = 121.
    CHECK(l.insert(97, p) == 97);
    CHECK(l.slots.size() == 121);
}

TEST_CASE("slot list takes the first hole after the hint and wraps") {
    const sim::SlotParams p;
    auto l = full_list(10);
    REQUIRE(l.slots.size() == 10);
    REQUIRE(l.hint == 9);
    l.remove(2);
    l.remove(6);
    // The scan starts at (9 + 1) % 10 = 0 and finds slot 2.
    CHECK(l.insert(1, p) == 2);
    CHECK(l.hint == 2);
    // Free slot 1: the lowest hole is now 1, but the scan from 3 reaches 6 first.
    l.remove(1);
    CHECK(l.insert(2, p) == 6);
    CHECK(l.hint == 6);
    // From 7 the scan wraps round and reaches slot 1.
    CHECK(l.insert(3, p) == 1);
    CHECK(l.hint == 1);
    CHECK(l.slots.size() == 10);
    CHECK(l.live() == 10);
    // Full: the vector grows 10 -> 12 and the new element takes slot 10.
    CHECK(l.insert(4, p) == 10);
    CHECK(l.slots.size() == 12);
    CHECK_FALSE(l.slots[11].has_value());
}

TEST_CASE("slot list scan from the last slot wraps to slot 0") {
    const sim::SlotParams p;
    auto l = full_list(10);
    REQUIRE(l.hint == 9);
    l.remove(0);
    l.remove(9);
    // hint 9: the scan starts at 10 % 10 = 0.
    CHECK(l.insert(1, p) == 0);
    // hint 0: the scan starts at 1 and reaches 9 after nine steps.
    CHECK(l.insert(2, p) == 9);
}

TEST_CASE("slot list reuse after remove only when the scan reaches the slot") {
    const sim::SlotParams p;
    auto l = full_list(10);
    // hint is 9. A hole at 4 and a later one at 8; the scan starts at 0.
    l.remove(4);
    l.remove(8);
    CHECK(l.insert(1, p) == 4);
    // Now hint 4; a new hole at 3 is behind the hint, the hole at 8 is ahead.
    l.remove(3);
    CHECK(l.insert(2, p) == 8);
    CHECK(l.insert(3, p) == 3);
}

TEST_CASE("slot list remove leaves hint cursor and length") {
    auto l = full_list(10);
    l.cursor = 7;
    const std::int64_t hint = l.hint;
    l.remove(5);
    CHECK(l.hint == hint);
    CHECK(l.cursor == 7);
    CHECK(l.slots.size() == 10);
    CHECK(l.live() == 9);
    // Removing an empty slot or one beyond the list changes nothing.
    l.remove(5);
    l.remove(10);
    l.remove(sim::kNoSlot);
    CHECK(l.slots.size() == 10);
    CHECK(l.live() == 9);
    CHECK(l.hint == hint);
    // The hole is not compacted away.
    CHECK_FALSE(l.slots[5].has_value());
    CHECK(l.slots[6].has_value());
}

TEST_CASE("slot list LowestFree takes the lowest hole") {
    sim::SlotParams p;
    p.policy = sim::SlotPolicy::LowestFree;
    auto l = full_list(10, p);
    l.remove(6);
    l.remove(2);
    l.remove(1);
    CHECK(l.insert(1, p) == 1);
    CHECK(l.hint == 1);
    CHECK(l.insert(2, p) == 2);
    CHECK(l.insert(3, p) == 6);
    // With a hint at 3 the original policy scans from 4 and takes 6, where
    // LowestFree still takes 1.
    sim::SlotParams orig;
    auto o = full_list(10, orig);
    o.remove(6);
    o.remove(2);
    o.remove(1);
    o.hint = 3;
    CHECK(o.insert(1, orig) == 6); // starts at 4
    auto lf = full_list(10, p);
    lf.remove(6);
    lf.remove(2);
    lf.remove(1);
    lf.hint = 3;
    CHECK(lf.insert(1, p) == 1);
    // A full list grows exactly as under the original policy.
    auto g = full_list(10, p);
    CHECK(g.insert(5, p) == 10);
    CHECK(g.slots.size() == 12);
}

TEST_CASE("slot list growth never leaves the new slot outside the vector") {
    // F22: with min_growth 1..3 the spec's n + n/4 does not grow a list below
    // 4 elements; the growth is raised to n + 1. The original's 10 and any
    // larger value follow the spec formula exactly.
    struct Row {
        std::uint32_t min_growth;
        std::vector<std::size_t> sizes;
    };
    const std::vector<Row> rows = {
        {0, {1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 15}},
        {1, {1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 15}},
        {2, {1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 15}},
        {3, {1, 3, 4, 5, 6, 7, 8, 10, 12, 15}},
        {4, {1, 4, 5, 6, 7, 8, 10, 12, 15}},
        {10, {1, 10, 12, 15, 18}},
        {25, {1, 25, 31, 38, 47}},
    };
    for (const auto& row : rows) {
        CAPTURE(row.min_growth);
        sim::SlotParams p;
        p.min_growth = row.min_growth;
        sim::SlotList<int> l;
        std::vector<std::size_t> sizes{1};
        for (int i = 0; i < 200; ++i) {
            const sim::Slot s = l.insert(i, p);
            REQUIRE(s < l.slots.size());
            REQUIRE(l.slots[s].value_or(-1) == i);
            if (sizes.back() != l.slots.size()) {
                sizes.push_back(l.slots.size());
            }
        }
        REQUIRE(sizes.size() > row.sizes.size());
        sizes.resize(row.sizes.size());
        CHECK(sizes == row.sizes);
        CHECK(l.live() == 200);
    }
    // An empty vector (a list loaded with no element) also takes an element.
    sim::SlotList<int> none(0);
    CHECK(none.insert(7, sim::SlotParams{}) == 0);
    CHECK(none.slots.size() == 10);
}

TEST_CASE("slot list insert leaves the scheduler cursor alone") {
    // D 0x10026a00, 0x10026b30 never write +0x1c: neither in a hole fill nor in
    // a growth.
    const sim::SlotParams p;
    auto l = full_list(10);
    l.cursor = 7;
    l.remove(3);
    CHECK(l.insert(1, p) == 3); // a hole fill
    CHECK(l.cursor == 7);
    CHECK(l.insert(2, p) == 10); // a growth, 10 -> 12
    CHECK(l.slots.size() == 12);
    CHECK(l.cursor == 7);
    sim::SlotParams lowest;
    lowest.policy = sim::SlotPolicy::LowestFree;
    l.remove(5);
    CHECK(l.insert(3, lowest) == 5);
    CHECK(l.cursor == 7);
    // A fresh list: the first insert grows 1 -> 10 and keeps cursor 0.
    sim::SlotList<int> f;
    CHECK(f.insert(1, p) == 0);
    CHECK(f.cursor == 0);
    CHECK(f.insert(2, p) == 1);
    CHECK(f.cursor == 0);
}

TEST_CASE("slot list growth is bounded for a hostile min_growth") {
    // A config or save with list_min_growth = 0xFFFFFFFF must not ask for 4 G
    // elements: the validation rejects it (valid_list_min_growth) and insert
    // cuts it to kListSizeBound anyway.
    CHECK(sim::SlotList<int>::grown_size(1, 0xFFFFFFFFU) == sim::kListSizeBound);
    CHECK(sim::SlotList<int>::grown_size(1, sim::kListSizeBound) == sim::kListSizeBound);
    CHECK(sim::SlotList<int>::grown_size(1, sim::kListSizeBound + 1) == sim::kListSizeBound);
    CHECK(sim::SlotList<int>::grown_size(1, sim::kListSizeBound - 1) == sim::kListSizeBound - 1);
    // Valid values: the spec's rule, F22 included.
    CHECK(sim::SlotList<int>::grown_size(1, 10) == 10);
    CHECK(sim::SlotList<int>::grown_size(10, 10) == 12);
    CHECK(sim::SlotList<int>::grown_size(97, 10) == 121);
    CHECK(sim::SlotList<int>::grown_size(0, 0) == 1);
    CHECK(sim::SlotList<int>::grown_size(2, 1) == 3);
    // The growth of a long list is the spec's n + n/4 even above the bound.
    CHECK(sim::SlotList<int>::grown_size(100'000'000, 10) == 125'000'000);
    // A real insert with a hostile min_growth allocates the bounded size (the
    // element type is small so the test stays cheap).
    sim::SlotList<std::uint8_t> l;
    sim::SlotParams p;
    p.min_growth = 0xFFFFFFFFU;
    CHECK(l.insert(std::uint8_t{1}, p) == 0); // the fresh list's one null element
    CHECK(l.insert(std::uint8_t{2}, p) == 1); // full: grows to the bounded size
    CHECK(l.slots.size() == sim::kListSizeBound);
}

TEST_CASE("slot list refuses to grow past the slot type") {
    // Slot is a u32 and kNoSlot its maximum: the new slot n must be below it.
    constexpr std::uint64_t kMaxLen = sim::kNoSlot; // 0xFFFFFFFF elements
    CHECK(sim::SlotList<int>::grown_size(kMaxLen - 1, 10) == kMaxLen);
    CHECK(sim::SlotList<int>::grown_size(kMaxLen - 2, 10) == kMaxLen);
    CHECK(sim::SlotList<int>::grown_size(kMaxLen - 1000, 10) == kMaxLen);
    CHECK(sim::SlotList<int>::grown_size(kMaxLen, 10) == 0);
    CHECK(sim::SlotList<int>::grown_size(kMaxLen + 1, 10) == 0);
    CHECK(sim::SlotList<int>::grown_size(std::uint64_t{1} << 40, 10) == 0);
}

TEST_CASE("list size configuration predicates") {
    // SimConfig::validate (S3) uses these; one row per bound.
    CHECK_FALSE(sim::valid_list_min_growth(0));
    CHECK(sim::valid_list_min_growth(1));
    CHECK(sim::valid_list_min_growth(10));
    CHECK(sim::valid_list_min_growth(sim::kListSizeBound));
    CHECK_FALSE(sim::valid_list_min_growth(sim::kListSizeBound + 1));
    CHECK_FALSE(sim::valid_list_min_growth(0xFFFFFFFFU));
    CHECK_FALSE(sim::valid_list_initial_size(0));
    CHECK(sim::valid_list_initial_size(1));
    CHECK(sim::valid_list_initial_size(sim::kListSizeBound));
    CHECK_FALSE(sim::valid_list_initial_size(sim::kListSizeBound + 1));
    CHECK_FALSE(sim::valid_list_initial_size(0xFFFFFFFFU));
    // The original's values are valid.
    CHECK(sim::valid_list_min_growth(10));
    CHECK(sim::valid_list_initial_size(1));
}

TEST_CASE("slot policy predicate accepts only the enumerators") {
    CHECK(sim::valid_slot_policy(sim::SlotPolicy::Original));
    CHECK(sim::valid_slot_policy(sim::SlotPolicy::LowestFree));
    CHECK_FALSE(sim::valid_slot_policy(static_cast<sim::SlotPolicy>(2)));
    CHECK_FALSE(sim::valid_slot_policy(static_cast<sim::SlotPolicy>(255)));
    // An out-of-range value is read as Original by insert (defined, not a
    // trap): the scan starts after the hint.
    sim::SlotParams p;
    p.policy = static_cast<sim::SlotPolicy>(7);
    auto l = full_list(10);
    l.remove(2);
    l.remove(6);
    l.hint = 3;
    CHECK(l.insert(1, p) == 6);
}

TEST_CASE("slot list survives a corrupt hint") {
    // Defined behaviour for hints outside the list (the loader rejects them):
    // the scan still visits every slot once.
    const sim::SlotParams p;
    auto l = full_list(10);
    l.remove(4);
    l.hint = 1'000'000;
    CHECK(l.insert(1, p) == 4);
    l.remove(7);
    l.hint = -5;
    CHECK(l.insert(2, p) == 7);
}

TEST_CASE("slot list holds move-only elements") {
    sim::SlotList<std::unique_ptr<int>> l;
    const sim::SlotParams p;
    for (int i = 0; i < 25; ++i) {
        const sim::Slot s = l.insert(std::make_unique<int>(i), p);
        REQUIRE(l.slots[s].has_value());
        CHECK(**l.slots[s] == i);
    }
    CHECK(l.live() == 25);
    l.remove(3);
    CHECK(l.live() == 24);
}

TEST_CASE("slot list matches the naive model over random inserts and removes") {
    const std::uint32_t ops = kSanitized ? 20'000 : 100'000;
    for (const auto policy : {sim::SlotPolicy::Original, sim::SlotPolicy::LowestFree}) {
        for (const std::uint32_t min_growth : {10U, 3U}) {
            CAPTURE(static_cast<int>(policy));
            CAPTURE(min_growth);
            Lcg rng(0x1503 + min_growth);
            sim::SlotParams p;
            p.policy = policy;
            p.min_growth = min_growth;
            sim::SlotList<int> l;
            Naive m;
            m.policy = policy;
            m.min_growth = min_growth;
            l.cursor = 7;
            m.cursor = 7;
            int next_value = 1;
            for (std::uint32_t op = 0; op < ops; ++op) {
                const std::uint32_t live = l.live();
                // Keep the list a few hundred long: removals dominate above 150.
                const bool insert = rng.below(100) < (live > 150 ? 20U : 65U);
                if (insert) {
                    const int value = next_value++;
                    const sim::Slot got = l.insert(value, p);
                    const int want = m.insert(value);
                    REQUIRE(static_cast<int>(got) == want);
                } else {
                    // Mostly live-looking slots, sometimes holes or beyond the end.
                    const sim::Slot s = rng.below(static_cast<std::uint32_t>(l.slots.size()) + 3);
                    l.remove(s);
                    m.remove(static_cast<int>(s));
                }
                REQUIRE(same_state(l, m));
            }
            // The list went through growth and wrap-around many times.
            CHECK(l.slots.size() > 100);
        }
    }
}
