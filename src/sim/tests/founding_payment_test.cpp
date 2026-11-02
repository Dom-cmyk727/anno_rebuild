// Founding payment preparation (task MAP-2 package C). Expected results are derived by hand from
// the rules in founding_payment.hpp (raw units of 1/256 ton: 1 t = 256). The mutants at the end are
// deliberately wrong implementations used as negative controls: the hand-derived cases must tell
// them apart from the real one.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/founding_payment.hpp>

namespace sim = anno::sim;
namespace pay = anno::sim::payment;

namespace {

constexpr sim::GoodsId kTools = 41; // 0x29 WERKZEUG
constexpr sim::GoodsId kWood = 42;  // 0x2a HOLZ
constexpr sim::GoodsId kBrick = 43; // 0x2b ZIEGEL
constexpr sim::GoodsId kFood = 50;  // unrelated goods
constexpr sim::GoodsId kCloth = 51;

using Holds = std::vector<sim::ShipHold>;
using Cost = std::vector<sim::CostEntry>;

pay::Costs costs(std::uint32_t gold, const Cost& goods) {
    return pay::Costs{gold, goods};
}

/// The plan, required to be a success.
pay::Plan ok(const Holds& h, std::int64_t money, const pay::Costs& c) {
    auto r = pay::prepare(h, money, c);
    REQUIRE(r.ok());
    return r.value();
}

pay::Refusal refused(const Holds& h, std::int64_t money, const pay::Costs& c) {
    auto r = pay::prepare(h, money, c);
    REQUIRE_FALSE(r.ok());
    return r.error();
}

} // namespace

TEST_CASE("payment: exact payment empties the holds and clears their goods") {
    const Holds h = {{kTools, 1024}, {kWood, 1536}};
    const Cost c = {{kTools, 1024}, {kWood, 1536}};
    const pay::Plan p = ok(h, 1000, costs(1000, c));
    CHECK(p.holds == Holds{{0, 0}, {0, 0}});
    CHECK(p.money_after == 0);
    CHECK(p.gold_charged == 1000);
}

TEST_CASE("payment: one raw unit short is a refusal naming the goods") {
    const Cost c = {{kTools, 1024}, {kWood, 1536}};
    // tools 1023 < 1024
    pay::Refusal r = refused({{kTools, 1023}, {kWood, 1536}}, 5000, costs(0, c));
    CHECK(r == pay::Refusal{pay::Status::InsufficientGoods, kTools, 1024, 1023});
    // wood 1535 < 1536 (tools suffice): the first goods in ascending order that fails is reported
    r = refused({{kTools, 1024}, {kWood, 1535}}, 5000, costs(0, c));
    CHECK(r == pay::Refusal{pay::Status::InsufficientGoods, kWood, 1536, 1535});
    // both short: tools (41) comes before wood (42)
    r = refused({{kTools, 1}, {kWood, 1}}, 5000, costs(0, c));
    CHECK(r.goods == kTools);
    // absent goods: available 0
    r = refused({{kTools, 1024}}, 5000, costs(0, c));
    CHECK(r == pay::Refusal{pay::Status::InsufficientGoods, kWood, 1536, 0});
}

TEST_CASE("payment: gold is checked only when non-zero and by one unit") {
    const Cost c = {{kTools, 256}};
    const Holds h = {{kTools, 256}};
    CHECK(refused(h, 999, costs(1000, c)) ==
          pay::Refusal{pay::Status::InsufficientGold, 0, 1000, 999});
    CHECK(ok(h, 1000, costs(1000, c)).money_after == 0);
    // Negative gold: available is floored at 0 in the refusal.
    CHECK(refused(h, -7, costs(1, c)) == pay::Refusal{pay::Status::InsufficientGold, 0, 1, 0});
    // A zero gold cost needs no gold, even for a negative balance, and charges nothing.
    const pay::Plan p = ok(h, -7, costs(0, c));
    CHECK(p.money_after == -7);
    CHECK(p.gold_charged == 0);
    // Goods are checked before gold: both short reports the goods.
    CHECK(refused({}, 0, costs(10, c)).status == pay::Status::InsufficientGoods);
}

TEST_CASE("payment: duplicate holds add for affordability and are consumed in index order") {
    // tools: holds 0 (500) and 2 (700); the cost is 1024. Index order: hold 0 gives 500 and is
    // emptied (cleared), hold 2 gives the remaining 524 and keeps 700 - 524 = 176.
    const Holds h = {{kTools, 500}, {kWood, 2000}, {kTools, 700}};
    const Cost c = {{kTools, 1024}};
    const pay::Plan p = ok(h, 0, costs(0, c));
    CHECK(p.holds == Holds{{0, 0}, {kWood, 2000}, {kTools, 176}});
    // The holds alone are 1200 >= 1024, neither hold alone is enough: duplicates did add.
    CHECK(refused({{kTools, 500}, {kTools, 523}}, 0, costs(0, c)).available == 1023);
}

TEST_CASE("payment: an earlier hold is emptied and cleared, a later surplus is untouched") {
    const Holds h = {{kTools, 300}, {kTools, 900}};
    const pay::Plan p = ok(h, 0, costs(0, {{kTools, 300}}));
    // Hold 0 is taken exactly (300 >= 300): amount 0 and goods 0. The need is met, hold 1
    // untouched.
    CHECK(p.holds == Holds{{0, 0}, {kTools, 900}});
    // A partial take keeps the goods id.
    CHECK(ok(h, 0, costs(0, {{kTools, 299}})).holds == Holds{{kTools, 1}, {kTools, 900}});
}

TEST_CASE("payment: zero costs change nothing") {
    const Holds h = {{kTools, 5}, {0, 0}, {kWood, 0}};
    pay::Plan p = ok(h, 42, costs(0, {}));
    CHECK(p.holds == h);
    CHECK(p.money_after == 42);
    // Entries with a zero amount are no cost: no hold is visited, not even an empty one of that
    // goods.
    p = ok(h, 42, costs(0, {{kWood, 0}, {kTools, 0}}));
    CHECK(p.holds == h);
}

TEST_CASE("payment: unrelated cargo is preserved around the paid holds") {
    const Holds h = {{kFood, 111}, {kTools, 300}, {kCloth, 222}, {kTools, 900}};
    const pay::Plan p = ok(h, 0, costs(0, {{kTools, 1000}}));
    // tools: hold 1 gives 300 (cleared), hold 3 gives 700 and keeps 200.
    CHECK(p.holds == Holds{{kFood, 111}, {0, 0}, {kCloth, 222}, {kTools, 200}});
}

TEST_CASE("payment: goods are processed in ascending id whatever the cost order, entries of one "
          "goods add") {
    const Holds h = {{kWood, 2000}, {kTools, 2000}, {kBrick, 2000}};
    const pay::Plan a = ok(h, 0, costs(0, {{kBrick, 100}, {kTools, 200}, {kWood, 300}}));
    const pay::Plan b = ok(h, 0, costs(0, {{kTools, 200}, {kWood, 300}, {kBrick, 100}}));
    CHECK(a == b);
    CHECK(a.holds == Holds{{kWood, 1700}, {kTools, 1800}, {kBrick, 1900}});
    // Two entries of one goods are one requirement of their sum: 400 + 624 = 1024.
    const Holds one = {{kTools, 1100}};
    CHECK(ok(one, 0, costs(0, {{kTools, 400}, {kTools, 624}})).holds == Holds{{kTools, 76}});
    CHECK(refused(one, 0, costs(0, {{kTools, 600}, {kTools, 600}})) ==
          pay::Refusal{pay::Status::InsufficientGoods, kTools, 1200, 1100});
}

TEST_CASE("payment: a matching empty hold is cleared only when it is visited with need remaining") {
    // AnnoWelt 0x1003b9a0 clears when take >= amount, so a visited empty hold of the paid goods is
    // cleared.
    CHECK(ok({{kTools, 0}, {kTools, 5}}, 0, costs(0, {{kTools, 3}})).holds ==
          Holds{{0, 0}, {kTools, 2}});
    // After the need is met the loop stops: a later empty hold of that goods is untouched.
    CHECK(ok({{kTools, 5}, {kTools, 0}}, 0, costs(0, {{kTools, 3}})).holds ==
          Holds{{kTools, 2}, {kTools, 0}});
    // Empty holds of other goods, and cleared holds, are never touched.
    CHECK(ok({{kWood, 0}, {0, 0}, {kTools, 5}}, 0, costs(0, {{kTools, 5}})).holds ==
          Holds{{kWood, 0}, {0, 0}, {0, 0}});
}

TEST_CASE("payment: goods id 0 supplies nothing") {
    CHECK(refused({{0, 5}}, 0, costs(0, {{0, 1}})) ==
          pay::Refusal{pay::Status::InsufficientGoods, 0, 1, 0});
}

TEST_CASE("payment: large amounts and overflow boundaries") {
    constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
    // A hold at the widest value pays one unit.
    CHECK(ok({{kTools, kMax}}, 0, costs(0, {{kTools, 1}})).holds == Holds{{kTools, kMax - 1}});
    // Two holds whose total does not fit 64 bits are an explicit refusal, not a wrapped small sum.
    CHECK(refused({{kTools, kMax}, {kTools, 1}}, 0, costs(0, {{kTools, 1}})) ==
          pay::Refusal{pay::Status::Overflow, kTools, 1, 0});
    // A total of exactly 2^64 - 1 fits: need 1 comes from hold 0 (take 1 < amount), hold 1
    // untouched.
    CHECK(ok({{kTools, kMax - 1}, {kTools, 1}}, 0, costs(0, {{kTools, 1}})).holds ==
          Holds{{kTools, kMax - 2}, {kTools, 1}});
    // Largest per-entry cost (u32), two of them summed.
    constexpr std::uint32_t big = std::numeric_limits<std::uint32_t>::max();
    const Holds h = {{kTools, std::uint64_t{big} * 2}};
    CHECK(ok(h, 0, costs(0, {{kTools, big}, {kTools, big}})).holds == Holds{{0, 0}});
    CHECK(refused(h, 0, costs(0, {{kTools, big}, {kTools, big}, {kTools, 1}})).required ==
          std::uint64_t{big} * 2 + 1);
    // Gold extremes: the largest gold cost from the largest balance, the smallest balance with no
    // cost.
    CHECK(ok({}, std::numeric_limits<std::int64_t>::max(), costs(big, {})).money_after ==
          std::numeric_limits<std::int64_t>::max() - big);
    CHECK(ok({}, std::numeric_limits<std::int64_t>::min(), costs(0, {})).money_after ==
          std::numeric_limits<std::int64_t>::min());
    CHECK(ok({}, big, costs(big, {})).money_after == 0);
    CHECK(refused({}, std::int64_t{big} - 1, costs(big, {})).status ==
          pay::Status::InsufficientGold);
}

TEST_CASE("payment: preparation is repeatable and never changes its inputs") {
    const Holds h = {{kFood, 9}, {kTools, 500}, {kTools, 700}, {kWood, 1536}};
    const Cost c = {{kTools, 1024}, {kWood, 1536}};
    const Holds h_before = h;
    const Cost c_before = c;
    const pay::Plan a = ok(h, 1000, costs(1000, c));
    const pay::Plan b = ok(h, 1000, costs(1000, c));
    CHECK(a == b);
    CHECK(h == h_before);
    CHECK(c.size() == c_before.size());
    CHECK(c[0].goods == c_before[0].goods);
    CHECK(c[0].amount_88 == c_before[0].amount_88);
    // A refusal leaves them as well, and is as repeatable.
    const pay::Refusal r1 = refused(h, 999, costs(1000, c));
    CHECK(r1 == refused(h, 999, costs(1000, c)));
    CHECK(h == h_before);
    // The result has the same length and order as the input holds.
    CHECK(a.holds.size() == h.size());
    CHECK(a.holds[0] == h[0]);
}

TEST_CASE("payment: the registry-type overload reads the type's costs") {
    sim::BuildingType t;
    t.cost_gold = 1000;
    t.cost_goods = {{kWood, 1536}, {kTools, 1024}}; // not in ascending order on purpose
    const Holds h = {{kTools, 1024}, {kWood, 2000}};
    auto r = pay::prepare(h, 1500, t);
    REQUIRE(r.ok());
    CHECK(r.value().holds == Holds{{0, 0}, {kWood, 464}});
    CHECK(r.value().money_after == 500);
    // Not special-cased to a type id: another type with other costs pays differently.
    sim::BuildingType other;
    other.id = sim::TypeId{1302};
    other.cost_gold = 0;
    other.cost_goods = {{kWood, 1}};
    CHECK(pay::prepare(h, 0, other).value().holds == Holds{{kTools, 1024}, {kWood, 1999}});
}

namespace {

// ---- negative controls: wrong implementations the cases above must tell apart ----

enum class Wrong { ReverseHoldOrder, KeepGoodsWhenEmptied, ClearOnlyBelowAmount };

Holds mutant(Holds holds, sim::GoodsId goods, std::uint64_t need, Wrong how) {
    const auto visit = [&](sim::ShipHold& h) {
        if (need == 0 || h.goods != goods) {
            return;
        }
        const std::uint64_t take = std::min(h.amount_88, need);
        const bool emptied =
            how == Wrong::ClearOnlyBelowAmount ? take > h.amount_88 : take >= h.amount_88;
        if (emptied) {
            h.amount_88 = 0;
            if (how != Wrong::KeepGoodsWhenEmptied) {
                h.goods = 0;
            }
        } else {
            h.amount_88 -= take;
        }
        need -= take;
    };
    if (how == Wrong::ReverseHoldOrder) {
        for (auto it = holds.rbegin(); it != holds.rend(); ++it) {
            visit(*it);
        }
    } else {
        for (auto& h : holds) {
            visit(h);
        }
    }
    return holds;
}

} // namespace

TEST_CASE("payment: the hand-derived cases distinguish reversed order and a missing clear") {
    const Holds h = {{kTools, 500}, {kWood, 2000}, {kTools, 700}};
    const Holds expected = ok(h, 0, costs(0, {{kTools, 1024}})).holds;
    REQUIRE(expected == Holds{{0, 0}, {kWood, 2000}, {kTools, 176}});
    CHECK(mutant(h, kTools, 1024, Wrong::ReverseHoldOrder) != expected); // {tools 176, wood, 0/0}
    CHECK(mutant(h, kTools, 1024, Wrong::KeepGoodsWhenEmptied) != expected); // {tools 0, ...}
    // The mutant framework itself agrees with the real code on a case with no emptied hold.
    const Holds easy = {{kTools, 900}};
    CHECK(mutant(easy, kTools, 100, Wrong::ReverseHoldOrder) ==
          ok(easy, 0, costs(0, {{kTools, 100}})).holds);
    // And the empty-hold-visited case separates the exact rule (take >= amount) from take > amount.
    const Holds visited = {{kTools, 0}, {kTools, 5}};
    CHECK(mutant(visited, kTools, 3, Wrong::ClearOnlyBelowAmount) !=
          ok(visited, 0, costs(0, {{kTools, 3}})).holds);
}
