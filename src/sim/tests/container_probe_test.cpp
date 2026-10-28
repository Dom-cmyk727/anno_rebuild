// Probe of the container shapes the state model builds on (docs/sim/state.md
// §14.1, §15 S2): optional<variant<...>> of the real production payloads,
// vectors of optionals of them, the slot list of those, and the real TileLayer
// (stage S5: default constructor, several vectors including `mutable` ones,
// `reset(...)`) inside the real Island (stage S6: a nested optional<struct>, a
// shared_ptr<const>, vectors of optionals, the cities with their building lists
// and the derived index cache).
//
// The point is the compiler, not the values: GCC 12/13 release builds under
// `-Werror -Wnull-dereference -Wmaybe-uninitialized` are known to report false
// positives on the first copies of these shapes, and this file finds them (and
// the one place to work around them) before the stages that depend on the
// shapes are written. The cases also run, so ASan/UBSan see the copies, moves
// and growth. Stage S5 pointed the file at the real `TileLayer`, `Terrain`,
// `SweepState`, `Drought` and `CityRing`; stage S6 at the real `Inert`, `Payload`,
// `Building`, `BuildingIndex`, `City` and `Island`.
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/city.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/slot_list.hpp>

namespace sim = anno::sim;

namespace {

/// FNV-1a style accumulator over 64-bit words: a value digest of a shape, so
/// a copy can be compared with its source without operator==.
struct Fp {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    void add(std::uint64_t v) {
        h ^= v;
        h *= 0x100000001b3ULL;
    }
};

// ------------------------------------------------------------ building shapes

using InertLike = sim::Inert;
using PayloadLike = sim::Payload;
using BuildingLike = sim::Building;

void fp_status(Fp& f, const sim::StatusTracker& s) {
    f.add(static_cast<std::uint64_t>(s.code));
    f.add(s.icons);
    f.add(s.shows_icons ? 1U : 0U);
    f.add(s.time);
}

template <class V> void fp_vec(Fp& f, const V& v) {
    f.add(v.size());
    for (const auto e : v) {
        f.add(static_cast<std::uint64_t>(e));
    }
}

void fp_payload(Fp& f, const InertLike& p) {
    f.add(p.last);
    f.add(p.blocked ? 1U : 0U);
    f.add(p.suspended ? 1U : 0U);
    fp_status(f, p.status);
}
void fp_payload(Fp& f, const sim::Workshop& p) {
    f.add(p.last);
    f.add(p.switched_on ? 1U : 0U);
    f.add(p.suspended ? 1U : 0U);
    f.add(p.blocked ? 1U : 0U);
    fp_vec(f, p.out);
    fp_vec(f, p.in);
    fp_vec(f, p.activity);
    fp_vec(f, p.reserved);
    f.add(p.carriers_out);
    fp_status(f, p.status);
}
void fp_payload(Fp& f, const sim::WeaponsWorkshop& p) {
    f.add(p.last);
    fp_vec(f, p.line_on);
    fp_vec(f, p.unlocked);
    f.add(p.suspended ? 1U : 0U);
    f.add(p.blocked ? 1U : 0U);
    fp_vec(f, p.out);
    fp_vec(f, p.in);
    fp_vec(f, p.activity);
    fp_vec(f, p.reserved);
    f.add(p.carriers_out);
    fp_status(f, p.status);
}
void fp_payload(Fp& f, const sim::Farm& p) {
    f.add(p.last);
    f.add(p.switched_on ? 1U : 0U);
    f.add(p.suspended ? 1U : 0U);
    f.add(p.blocked ? 1U : 0U);
    f.add(p.out);
    f.add(p.raw);
    f.add(p.activity);
    f.add(p.reserved);
    f.add(p.workers_out);
    fp_status(f, p.status);
}
void fp_payload(Fp& f, const sim::SmallFarm& p) {
    f.add(p.last);
    fp_vec(f, p.line_on);
    f.add(p.suspended ? 1U : 0U);
    f.add(p.blocked ? 1U : 0U);
    fp_vec(f, p.out);
    fp_vec(f, p.raw);
    fp_vec(f, p.activity);
    fp_vec(f, p.reserved);
    fp_vec(f, p.figures_out);
    fp_status(f, p.status);
}
void fp_payload(Fp& f, const sim::House& p) {
    f.add(p.last);
    f.add(static_cast<std::uint32_t>(p.residents_64));
    f.add(p.e);
    fp_vec(f, p.stock);
}
void fp_payload(Fp& f, const sim::Market& p) {
    f.add(p.last);
    f.add(p.blocked ? 1U : 0U);
    f.add(p.suspended ? 1U : 0U);
    fp_status(f, p.status);
    f.add(p.carts_out);
}
void fp_payload(Fp& f, const sim::Mine& p) {
    f.add(p.last);
    f.add(p.switched_on ? 1U : 0U);
    f.add(p.deep ? 1U : 0U);
    f.add(p.blocked ? 1U : 0U);
    f.add(p.reserved);
    f.add(p.out);
    f.add(p.activity);
    f.add(p.deposit.has_value() ? 1U + static_cast<std::uint64_t>(*p.deposit) : 0U);
    fp_status(f, p.status);
}

std::uint64_t fingerprint(const BuildingLike& b) {
    Fp f;
    f.add(b.id.v);
    f.add(b.type.v);
    f.add(static_cast<std::uint32_t>(b.x));
    f.add(static_cast<std::uint32_t>(b.y));
    f.add(b.rotation);
    f.add(b.byte0e);
    f.add(b.byte0f);
    f.add(b.payload.index());
    std::visit([&f](const auto& p) { fp_payload(f, p); }, b.payload);
    return f.h;
}

/// A building whose payload is alternative `k % 7`, filled with values that
/// depend on `k` (vectors of different lengths, so the copies allocate).
BuildingLike make_building(std::uint32_t k) {
    BuildingLike b;
    b.id = sim::make_building_id(sim::IslandId{k % 3}, k + 1);
    b.type = sim::TypeId{1000 + k};
    b.x = static_cast<std::int32_t>(k) * 3;
    b.y = static_cast<std::int32_t>(k) * 5 + 1;
    b.rotation = static_cast<std::uint8_t>(k % 4);
    b.byte0f = static_cast<std::uint8_t>(k);
    const std::uint32_t len = 1 + k % 5;
    switch (k % 7) {
    case 0: {
        auto& p = b.payload.emplace<InertLike>();
        p.last = static_cast<std::uint16_t>(k);
        p.blocked = (k & 1U) != 0;
        p.status.code = sim::Status::MissingInput;
        p.status.icons = 2;
        p.status.time = static_cast<std::uint16_t>(100 + k);
        break;
    }
    case 1: {
        auto& p = b.payload.emplace<sim::Workshop>();
        p.last = static_cast<std::uint16_t>(k);
        p.out.assign(len, 256 + k);
        p.in.assign(len + 1, 512 + k);
        p.activity.assign(len, k);
        p.reserved.assign(len, k / 2);
        p.carriers_out = k;
        break;
    }
    case 2: {
        auto& p = b.payload.emplace<sim::WeaponsWorkshop>();
        p.last = static_cast<std::uint16_t>(k);
        p.line_on.assign(len, true);
        p.line_on[0] = (k & 1U) != 0;
        p.unlocked.assign(len, (k & 2U) != 0);
        p.out.assign(len, k);
        p.in.assign(len, k + 1);
        p.activity.assign(len, static_cast<std::uint16_t>(k));
        break;
    }
    case 3: {
        auto& p = b.payload.emplace<sim::Farm>();
        p.last = static_cast<std::uint16_t>(k);
        p.out = k * 7;
        p.raw = k * 11;
        p.workers_out = k % 4;
        break;
    }
    case 4: {
        auto& p = b.payload.emplace<sim::SmallFarm>();
        p.last = static_cast<std::uint16_t>(k);
        // Not `= {true, flag}` or assign({..}): GCC 12 at -O2 reports a null
        // dereference inside vector<bool>'s range insert for those shapes.
        p.line_on.assign(2, true);
        p.line_on[1] = (k & 1U) != 0;
        p.out = {k, k + 1};
        p.raw = {k + 2, k + 3};
        p.activity = {static_cast<std::uint16_t>(k), 7};
        p.figures_out.assign(5, k % 3);
        break;
    }
    case 5: {
        auto& p = b.payload.emplace<sim::Mine>();
        p.last = static_cast<std::uint16_t>(k);
        p.out = k * 13;
        p.reserved = k;
        if ((k & 1U) != 0) {
            p.deposit = static_cast<std::size_t>(k);
        }
        break;
    }
    default: {
        auto& p = b.payload.emplace<sim::Market>();
        p.last = static_cast<std::uint16_t>(k);
        p.carts_out = k % 3;
        p.status.time = static_cast<std::uint16_t>(k);
        break;
    }
    }
    return b;
}

std::uint64_t fingerprint(const std::optional<BuildingLike>& b) {
    return b ? fingerprint(*b) : 0;
}

std::uint64_t fingerprint(const std::vector<std::optional<BuildingLike>>& v) {
    Fp f;
    f.add(v.size());
    for (const auto& e : v) {
        f.add(fingerprint(e));
    }
    return f.h;
}

std::uint64_t fingerprint(const sim::SlotList<BuildingLike>& l) {
    Fp f;
    f.add(static_cast<std::uint64_t>(l.hint));
    f.add(l.cursor);
    f.add(fingerprint(l.slots));
    return f.h;
}

std::uint64_t fingerprint(const std::vector<sim::SlotList<BuildingLike>>& lists) {
    Fp f;
    f.add(lists.size());
    for (const auto& l : lists) {
        f.add(fingerprint(l));
    }
    return f.h;
}

// ------------------------------------------------- tile layer and island shapes

// Since stage S6 the whole island is the real type: tile layer, terrain, sweep,
// drought, ring, cities (stock, settings, building index with its lists) and the
// derived index cache.

using CityLike = sim::City;
using IslandLike = sim::Island;

std::uint64_t fingerprint(const sim::TileLayer& t) {
    Fp f;
    f.add(t.width());
    f.add(t.height());
    const std::uint64_t chunks = t.chunk_count();
    for (std::uint64_t c = 0; c < chunks; ++c) {
        f.add(t.chunk_digest(c));
    }
    return f.h;
}

std::uint64_t fingerprint(const CityLike& c) {
    Fp f;
    f.add(c.number.v);
    f.add(c.owner.v);
    fp_vec(f, c.stock.stock);
    f.add(c.stock.capacity);
    f.add(c.index.stamp);
    f.add(fingerprint(c.index.lists));
    fp_vec(f, c.index.service_counters);
    fp_vec(f, c.index.house_counts);
    f.add(c.tick_stamp);
    f.add(c.goods.size());
    for (const auto& g : c.goods) {
        f.add(g.buy ? 1U : 0U);
        f.add(static_cast<std::uint32_t>(g.list_index));
    }
    for (const auto& b : c.build_goods) {
        f.add(b.setting);
        f.add(b.spent);
    }
    for (const auto& list : c.trade_lists) {
        f.add(list.size());
        for (const auto& e : list) {
            f.add(e.goods);
            f.add(e.amount_88);
            f.add(e.price);
        }
    }
    return f.h;
}

std::uint64_t fingerprint(const IslandLike& isl) {
    Fp f;
    f.add(isl.index.v);
    f.add(fingerprint(isl.tiles));
    f.add(isl.terrain ? isl.terrain->digest : 0);
    f.add(isl.climate);
    for (const auto& d : isl.deposits) {
        f.add(d.remaining);
        f.add(static_cast<std::uint32_t>(d.x));
        f.add(d.reserve ? 1U : 0U);
    }
    f.add(isl.next_serial);
    f.add(isl.sweep.stamp);
    f.add(isl.sweep.grow_x);
    f.add(isl.event300_stamp);
    if (isl.drought) {
        f.add(1);
        f.add(isl.drought->x);
        f.add(isl.drought->ring < 0 ? 0U : static_cast<std::uint32_t>(isl.drought->ring));
        f.add(isl.drought->stamp);
    } else {
        f.add(0);
    }
    f.add(isl.ring.cursor);
    f.add(isl.cities.size());
    for (const auto& c : isl.cities) {
        f.add(c ? fingerprint(*c) : 0);
    }
    f.add(isl.reserved_object_count);
    f.add(isl.next_serial);
    f.add(isl.objmgr.cursor);
    for (const auto& [id, key] : isl.cache.by_id) {
        f.add(id.v);
        f.add(key.list);
        f.add(key.slot);
    }
    f.add(isl.cache.grids.size());
    for (const auto& g : isl.cache.grids) {
        fp_vec(f, g);
    }
    return f.h;
}

/// An island with a 20 x 13 tile layer (chunks of 8: 3 x 2, the edge ones
/// clipped), a drought, deposits and `cities` occupied city slots of 16, each
/// with `per_list` buildings in each of six lists.
IslandLike make_island(std::uint32_t seed, std::uint32_t cities, std::uint32_t per_list) {
    IslandLike isl;
    isl.index = sim::IslandId{seed};
    REQUIRE(isl.tiles.reset(100, 100, 20, 13, 8));
    for (std::uint32_t i = 0; i < 20U * 13U; i += 3) {
        isl.tiles.set_object(i, 700 + i % 9);
        isl.tiles.set_phase(i, static_cast<std::uint8_t>(i));
        isl.tiles.set_owner(i, i % 16);
    }
    auto terrain = std::make_shared<sim::Terrain>();
    terrain->height.assign(20U * 13U, 4);
    terrain->digest = 0xabcdef00U + seed;
    isl.terrain = terrain;
    isl.climate = seed % 7;
    isl.deposits.resize(3);
    isl.deposits[1].remaining = 5 * sim::kTon;
    isl.drought.emplace();
    isl.drought->x = 8 * seed;
    isl.drought->stamp = static_cast<std::uint16_t>(seed);
    isl.cities.resize(16);
    std::uint32_t k = seed * 100;
    for (std::uint32_t c = 0; c < cities; ++c) {
        CityLike& city = isl.cities[c].emplace();
        city.number = sim::CityNo{c};
        city.owner = sim::PlayerId{c};
        city.stock.stock.assign(sim::kOriginalWarehouseGoods, c);
        city.index.lists.resize(6);
        city.index.service_counters.assign(32, c);
        city.index.house_counts.assign(5, c + 1);
        city.goods.resize(sim::kOriginalWarehouseGoods);
        city.goods[3].buy = true;
        city.build_goods.resize(5);
        city.build_goods[1].spent = 77 + c;
        city.trade_lists.resize(2);
        city.trade_lists[1].push_back({5, 256, 9});
        for (auto& list : city.index.lists) {
            for (std::uint32_t i = 0; i < per_list; ++i) {
                const sim::Slot s = list.insert(make_building(k++), sim::SlotParams{});
                const BuildingLike& b = *list.slots[s];
                isl.cache.by_id[b.id] =
                    sim::BuildingKey{sim::CityRef{isl.index, city.number}, 0, s};
            }
        }
    }
    isl.cache.grids.resize(6);
    isl.cache.grids[2].assign(20U * 13U, 0);
    isl.cache.grids[2][17] = 3;
    return isl;
}

} // namespace

TEST_CASE("container shapes copy move emplace") {
    // The payload variant: every alternative, copied, moved and assigned over
    // every other alternative.
    std::vector<std::optional<BuildingLike>> all;
    for (std::uint32_t k = 0; k < 12; ++k) {
        all.emplace_back(make_building(k));
    }
    for (std::uint32_t k = 0; k < 12; ++k) {
        CAPTURE(k);
        std::optional<BuildingLike> a;
        a.emplace(make_building(k));
        REQUIRE(a.has_value());
        CHECK(a->payload.index() == k % 7);
        const std::uint64_t want = fingerprint(a);
        const std::optional<BuildingLike> copy(a);
        CHECK(fingerprint(copy) == want);
        std::optional<BuildingLike> moved(std::move(a));
        CHECK(fingerprint(moved) == want);
        // Copy-assign over a different alternative, then move-assign.
        std::optional<BuildingLike> target;
        target.emplace(make_building(k + 1));
        CHECK(fingerprint(target) != want);
        target = copy;
        CHECK(fingerprint(target) == want);
        target.emplace(make_building(k + 2));
        target = std::move(moved);
        CHECK(fingerprint(target) == want);
        // Copies are independent of their source.
        std::optional<BuildingLike> other(copy);
        other->x += 1;
        other->payload = InertLike{};
        CHECK(fingerprint(copy) == want);
        CHECK(fingerprint(other) != want);
        // Self-assignment through a reference leaves the value alone.
        const std::optional<BuildingLike>& self = target;
        target = self;
        CHECK(fingerprint(target) == want);
        target.reset();
        CHECK_FALSE(target.has_value());
        CHECK(fingerprint(target) == 0);
        target.emplace();
        CHECK(target->payload.index() == 0);
        CHECK(target->byte0e == 0x86);
    }

    // A vector of optionals grows by copying or moving its elements.
    std::vector<std::optional<BuildingLike>> v;
    for (std::uint32_t k = 0; k < 100; ++k) {
        v.push_back(k % 7 == 3 ? std::nullopt : std::optional<BuildingLike>(make_building(k)));
    }
    const std::uint64_t want_v = fingerprint(v);
    v.resize(250);
    v.resize(100);
    CHECK(fingerprint(v) == want_v);
    std::vector<std::optional<BuildingLike>> v2(v);
    CHECK(fingerprint(v2) == want_v);
    v2[5].reset();
    v2[3].emplace(make_building(3));
    CHECK(fingerprint(v2) != want_v);
    CHECK(fingerprint(v) == want_v);
    v2 = v;
    CHECK(fingerprint(v2) == want_v);
    std::vector<std::optional<BuildingLike>> v3(std::move(v2));
    CHECK(fingerprint(v3) == want_v);

    // The slot list of them: inserts that grow, removals, copies, moves.
    sim::SlotList<BuildingLike> list;
    for (std::uint32_t k = 0; k < 120; ++k) {
        list.insert(make_building(k), sim::SlotParams{});
        if (k % 5 == 4) {
            list.remove(k / 2);
        }
    }
    list.cursor = 17;
    const std::uint64_t want_list = fingerprint(list);
    sim::SlotList<BuildingLike> list_copy(list);
    CHECK(fingerprint(list_copy) == want_list);
    list_copy.insert(make_building(500), sim::SlotParams{});
    CHECK(fingerprint(list_copy) != want_list);
    CHECK(fingerprint(list) == want_list);
    sim::SlotList<BuildingLike> list_moved(std::move(list_copy));
    list_copy = list;
    CHECK(fingerprint(list_copy) == want_list);
    list_moved = std::move(list_copy);
    list_copy = list_moved;
    CHECK(fingerprint(list_copy) == fingerprint(list_moved));

    // Six lists per city, as a BuildingIndex holds them.
    std::vector<sim::SlotList<BuildingLike>> lists(6);
    for (std::uint32_t k = 0; k < 60; ++k) {
        lists[k % 6].insert(make_building(k), sim::SlotParams{});
    }
    const std::uint64_t want_lists = fingerprint(lists);
    std::vector<sim::SlotList<BuildingLike>> lists_copy(lists);
    CHECK(fingerprint(lists_copy) == want_lists);
    lists_copy.resize(9);
    lists_copy.resize(6);
    CHECK(fingerprint(lists_copy) == want_lists);
    std::vector<sim::SlotList<BuildingLike>> lists_moved(std::move(lists_copy));
    CHECK(fingerprint(lists_moved) == want_lists);
}

TEST_CASE("container shapes of a tile layer stand-in") {
    sim::TileLayer empty;
    CHECK(empty.width() == 0);
    CHECK(empty.chunk_digest(0) == 0);
    CHECK(fingerprint(empty) == fingerprint(sim::TileLayer()));

    sim::TileLayer t;
    REQUIRE(t.reset(-4, 7, 20, 13, 8));
    CHECK(t.chunks_x() == 3);
    CHECK(t.chunks_y() == 2);
    CHECK(t.contains(-4, 7));
    CHECK_FALSE(t.contains(16, 7));
    const std::uint64_t blank = fingerprint(t);
    t.set_object(19, 700);
    t.set_owner(5 * 20 + 19, 3);
    const std::uint64_t written = fingerprint(t);
    CHECK(written != blank);
    // The cache agrees with the reference after writes and after copies.
    const sim::TileLayer copy(t);
    for (std::uint32_t c = 0; c < 6; ++c) {
        CHECK(copy.chunk_digest(c) == copy.chunk_digest_uncached(c));
        CHECK(t.chunk_digest(c) == t.chunk_digest_uncached(c));
    }
    sim::TileLayer moved(std::move(t));
    CHECK(fingerprint(moved) == written);
    t = copy;
    CHECK(fingerprint(t) == written);
    t.set_phase(0, 9);
    CHECK(fingerprint(t) != written);
    CHECK(fingerprint(copy) == written);
    // A failed reset leaves the layer unchanged.
    CHECK_FALSE(t.reset(0, 0, 100, 100, 0));
    CHECK(t.width() == 20);
    CHECK_FALSE(t.reset(0, 0, 0xffffffffU, 0xffffffffU, 8));
    CHECK(t.height() == 13);
    // Re-dimension and zero-fill.
    CHECK(t.reset(1, 2, 5, 5, 8));
    CHECK(t.chunks_x() == 1);
    CHECK(fingerprint(t) == fingerprint(sim::TileLayer(1, 2, 5, 5, 8)));
}

TEST_CASE("container shapes of an island stand-in") {
    // optional<IslandLike>: default construction, emplace, fill, copy, move, reset.
    std::optional<IslandLike> a;
    a.emplace();
    CHECK(a->cities.empty());
    CHECK_FALSE(a->drought.has_value());
    a.emplace(make_island(2, 3, 4));
    const std::uint64_t want = fingerprint(*a);
    std::optional<IslandLike> copy(a);
    REQUIRE(copy.has_value());
    CHECK(fingerprint(*copy) == want);
    // The copy owns its state; the terrain is shared (immutable).
    CHECK(copy->terrain == a->terrain);
    copy->tiles.set_object(30, 12345);
    copy->cities[1]->index.lists[2].insert(make_building(900), sim::SlotParams{});
    copy->drought->ring = 4;
    CHECK(fingerprint(*copy) != want);
    CHECK(fingerprint(*a) == want);
    std::optional<IslandLike> moved(std::move(copy));
    CHECK(fingerprint(*moved) != want);
    moved = a;
    CHECK(fingerprint(*moved) == want);
    moved.reset();
    CHECK_FALSE(moved.has_value());

    // The island table: a vector of optionals that grows and is snapshotted.
    std::vector<std::optional<IslandLike>> islands;
    islands.emplace_back(make_island(0, 1, 2));
    islands.emplace_back(std::nullopt);
    islands.emplace_back(make_island(2, 3, 4));
    const auto table_fp = [](const std::vector<std::optional<IslandLike>>& t) {
        Fp f;
        for (const auto& i : t) {
            f.add(i ? fingerprint(*i) : 0);
        }
        return f.h;
    };
    const std::uint64_t want_table = table_fp(islands);
    const std::vector<std::optional<IslandLike>> snapshot(islands);
    islands.resize(20);
    islands[7].emplace(make_island(7, 2, 3));
    CHECK(table_fp(snapshot) == want_table);
    CHECK(table_fp(islands) != want_table);
    islands[7].reset();
    islands.resize(3);
    CHECK(table_fp(islands) == want_table);
    std::swap(islands[0], islands[2]);
    CHECK(table_fp(islands) != want_table);
    std::swap(islands[0], islands[2]);
    std::vector<std::optional<IslandLike>> assigned;
    assigned = islands;
    CHECK(table_fp(assigned) == want_table);
    assigned = std::move(islands);
    CHECK(table_fp(assigned) == want_table);
}
