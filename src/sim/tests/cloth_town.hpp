// MAP-NEEDS-2 N3/N5: the shared harness of the cloth canary (Tutorial island-7 town, sheep -> wool
// -> cloth -> stall -> houses).
#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/harvest.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "island7_town.hpp"
#include "tutorial_town.hpp"
#include "world_access.hpp"

namespace cloth_town {

namespace sim = anno::sim;
using namespace tutorial_town;
using namespace island7;

constexpr sim::GoodsId kWolle = 9, kGras = 69;

struct PaySnap {
    std::int64_t money = 0;
    std::vector<std::uint32_t> store;
    std::vector<std::uint64_t> spent;
};

struct HouseW {
    std::uint32_t stoffe_slot = 0, stoffe0 = 0, prev = 0;
    bool rose = false;
    std::uint64_t first_rise_tick = 0;
};

struct Cloth {
    sim::World w;
    sim::TransportStats stats;
    sim::Island* isl = nullptr;
    sim::City* city = nullptr;
    sim::CityRef ref;
    std::vector<sim::BuildingId> sheep;
    sim::BuildingId weaver;
    sim::TypeId stall_type;
    std::optional<sim::TileXY> stall_at;
    std::optional<PaySnap> pay_before, pay_after; // around the stall BuildCmd
    std::map<sim::BuildingId, HouseW> houses;
    // per grass tile of the sheep farms' discs: the tick of the cut and of the regrowth
    std::map<std::pair<int, int>, std::vector<std::uint64_t>> cut_at, regrown_at;
    std::map<std::pair<int, int>, std::uint32_t> last_obj;
    std::uint64_t first_cloth_store_tick = 0;
    // the producers' starting inventory (recorded at tick 0)
    std::uint64_t farm_raw0 = 0, farm_out0 = 0, weaver_in0 = 0, weaver_out0 = 0;
    explicit Cloth(sim::World&& world) : w(std::move(world)) {}
    Cloth(const Cloth&) = delete;
    Cloth& operator=(const Cloth&) = delete;
};

/// Scope helpers (MAP-NEEDS-CLOSE-EXTRACT-1 C1). A building belongs to the TARGET CHAIN when it
/// is in the target CITY (the exact CityRef's building index, not merely on the island), and a
/// figure belongs to it when it is alive, its owner is the target city's owner and its home is a
/// building of that city. Everything that sums or selects figures goes through these.
inline bool in_city(const Cloth& c, sim::BuildingId id) {
    if (!id.valid()) {
        return false;
    }
    for (const auto& l : c.city->index.lists) {
        for (const auto& b : l.slots) {
            if (b && b->id == id) {
                return true;
            }
        }
    }
    return false;
}

inline bool in_chain(const Cloth& c, const sim::Figure& f) {
    return !f.expiry_stamp.has_value() && f.owner == c.city->owner && in_city(c, f.home);
}

/// The payment of a build: the owner's gold and the city's store of every cost good before, then
/// the exact debit after.
inline PaySnap pay_snap(const Cloth& c, const sim::BuildingType& t) {
    PaySnap p;
    p.money = c.w.players().find(c.city->owner)->money;
    for (const sim::CostEntry& e : t.cost_goods) {
        p.store.push_back(c.city->stock.stock[e.goods]);
        p.spent.push_back(c.city->build_goods[e.goods - c.w.config().build_goods_first].spent);
    }
    return p;
}

/// Asserts that the build between `a` and `b` was PAID: gold down by cost_gold, every cost good
/// down in the store by its amount and up in the city's `spent` ledger by the same (the BuildCmd
/// debits at apply.cpp, not free).
inline void check_paid(const Cloth& c, const sim::BuildingType& t, const PaySnap& a,
                       const PaySnap& b) {
    CHECK_FALSE(c.w.rules().free_build_cheat);
    CHECK(t.cost_gold > 0);
    CHECK(a.money - b.money == std::int64_t{t.cost_gold});
    CHECK_FALSE(t.cost_goods.empty());
    for (std::size_t i = 0; i < t.cost_goods.size(); ++i) {
        CHECK(a.store[i] - b.store[i] == t.cost_goods[i].amount_88);
        CHECK(b.spent[i] - a.spent[i] == t.cost_goods[i].amount_88);
    }
}

inline sim::Command cmd(const Cloth& c, sim::Body body) {
    sim::Command k;
    k.issuer = c.city->owner;
    k.body = std::move(body);
    return k;
}

/// Roles from the registry's goods, never from ids: a Farm-tick building with output WOLLE and
/// input GRAS, a Workshop with input WOLLE and output STOFFE, a MarketStall whose outputs include
/// STOFFE.
inline std::unique_ptr<Cloth> start_cloth(const Original& o, bool build_stall,
                                          std::uint32_t stoffe_store, bool producers) {
    auto c = std::make_unique<Cloth>(tutorial_world(o, true));
    for (auto& i : sim::WorldTestAccess::islands(c->w)) {
        if (i && i->index.v == kIsland7) {
            c->isl = &*i;
            for (auto& ci : i->cities) {
                if (ci && ci->number.v == kCity1) {
                    c->city = &*ci;
                }
            }
        }
    }
    REQUIRE(c->city != nullptr);
    c->ref = sim::CityRef{c->isl->index, c->city->number};
    const auto& T = *o.types;
    std::vector<sim::TileXY> demolish;
    for (auto& l : c->city->index.lists) {
        for (auto& b : l.slots) {
            if (!b) {
                continue;
            }
            const sim::BuildingType* t = T.find(b->type);
            if (t == nullptr || !t->has_prod || t->input_goods.empty() || t->output_goods.empty()) {
                continue;
            }
            const bool farm = T.class_info(t->cls).tick == sim::TickKind::Farm;
            if (farm && t->output_goods[0] == kWolle && t->input_goods[0] == kGras) {
                c->sheep.push_back(b->id);
                demolish.push_back({b->x, b->y});
            }
            if (t->cls == sim::RuntimeClass::Workshop && t->input_goods[0] == kWolle &&
                t->output_goods[0] == kStoffe) {
                c->weaver = b->id;
                demolish.push_back({b->x, b->y});
            }
        }
    }
    REQUIRE(c->sheep.size() == 2);
    REQUIRE(c->weaver.valid());
    for (const sim::BuildingType& t : T.types()) {
        if (t.cls == sim::RuntimeClass::MarketStall && !t.output_goods.empty() &&
            (t.output_goods[0] == kStoffe ||
             (t.output_goods.size() > 1 && t.output_goods[1] == kStoffe))) {
            c->stall_type = t.id;
        }
    }
    REQUIRE(c->stall_type.v != 0);
    // start state (asserted, recorded in the docs): nothing of the chain's goods in the store, no
    // figures
    CHECK(c->city->stock.stock[kWolle] == 0);
    CHECK(c->city->stock.stock[kStoffe] == 0);
    CHECK(c->city->stock.stock[kLeder] == 0);
    CHECK(c->city->stock.stock[18] == 0); // TIERHAEUTE
    std::size_t living = 0;
    for (const auto& s : c->w.figures().list.slots) {
        living += s.has_value() ? 1U : 0U;
    }
    CHECK(living == 0);
    for (const sim::BuildingId id : c->sheep) {
        const sim::Farm& f = std::get<sim::Farm>(c->isl->find_building(id)->payload);
        c->farm_raw0 += f.raw;
        c->farm_out0 += f.out;
    }
    {
        const sim::Workshop& w = std::get<sim::Workshop>(c->isl->find_building(c->weaver)->payload);
        c->weaver_in0 = w.in[0];
        c->weaver_out0 = w.out[0];
    }
    c->w.set_transport_stats(&c->stats);
    if (!producers) {
        REQUIRE(c->w.apply(cmd(*c, sim::DemolishCmd{c->ref, demolish})).ok());
    }
    if (build_stall) {
        // The nearest accepted site to the existing food stall, by rings of increasing distance (a
        // public BuildCmd; the first accepted site wins, deterministic).
        sim::TileXY centre{305, 339};
        for (auto& l : c->city->index.lists) {
            for (auto& b : l.slots) {
                if (b && T.find(b->type) != nullptr &&
                    T.find(b->type)->cls == sim::RuntimeClass::MarketStall) {
                    centre = {b->x, b->y};
                }
            }
        }
        bool done = false;
        c->pay_before = pay_snap(*c, *T.find(c->stall_type));
        for (int r = 1; r <= 8 && !done; ++r) {
            for (int dy = -r; dy <= r && !done; ++dy) {
                for (int dx = -r; dx <= r && !done; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) {
                        continue;
                    }
                    const auto res = c->w.apply(
                        cmd(*c, sim::BuildCmd{c->ref,
                                              {sim::PlacementWire{c->stall_type, centre.x + dx,
                                                                  centre.y + dy, 0, 0, 0}}}));
                    if (res.ok()) {
                        c->stall_at = sim::TileXY{centre.x + dx, centre.y + dy};
                        done = true;
                    }
                }
            }
        }
        REQUIRE(done);
        c->pay_after = pay_snap(*c, *T.find(c->stall_type));
    }
    if (stoffe_store != 0) {
        c->city->stock.stock[kStoffe] =
            stoffe_store; // LABELLED stock fixture (named setup, not a game command)
    }
    for (auto& l : c->city->index.lists) {
        for (auto& b : l.slots) {
            if (const auto* h = b ? std::get_if<sim::House>(&b->payload) : nullptr) {
                const sim::BuildingType* t = T.find(b->type);
                const auto g = sim::house_group(c->w.config(), t->prodtype);
                REQUIRE(g.has_value());
                const auto view = T.population_view(*g, c->w.config());
                REQUIRE(view.ok());
                HouseW hw;
                hw.stoffe_slot = view.value().derived->lookup[kStoffe];
                hw.stoffe0 = h->stock[hw.stoffe_slot];
                hw.prev = hw.stoffe0;
                c->houses[b->id] = hw;
            }
        }
    }
    // the grass tiles of the sheep farms' search squares (candidate tiles), for the regrowth
    // measurement
    for (const sim::BuildingId id : c->sheep) {
        const sim::Building* b = c->isl->find_building(id);
        if (b == nullptr) {
            continue; // demolished in the control arms
        }
        const sim::SearchArea a = sim::search_area(*b, *T.find(b->type));
        const sim::TileRect sq = a.square();
        for (int y = sq.y0; y < sq.y0 + static_cast<int>(sq.h); ++y) {
            for (int x = sq.x0; x < sq.x0 + static_cast<int>(sq.w); ++x) {
                const std::uint32_t obj = c->isl->tiles.object(c->isl->tile_index(x, y));
                if (sim::harvest_candidate_type(T, obj, kGras, true) ||
                    (obj != 0 && sim::harvest_candidate_type(T, obj + 1, kGras, true))) {
                    c->last_obj[{x, y}] = obj;
                }
            }
        }
    }
    return c;
}

/// One 100 ms step, with the per-step watchers.
inline void step(Cloth& c) {
    c.w.advance(100);
    REQUIRE_FALSE(c.w.faulted());
    const std::uint64_t t = c.w.clock().tick();
    for (auto& [id, hw] : c.houses) {
        const sim::Building* b = c.isl->find_building(id);
        const auto* h = b != nullptr ? std::get_if<sim::House>(&b->payload) : nullptr;
        if (h == nullptr) {
            continue;
        }
        const std::uint32_t now = h->stock[hw.stoffe_slot];
        if (now > hw.prev && !hw.rose) {
            hw.rose = true;
            hw.first_rise_tick = t;
        }
        hw.prev = now;
    }
    if (c.first_cloth_store_tick == 0 && c.city->stock.stock[kStoffe] > 0) {
        c.first_cloth_store_tick = t;
    }
    for (auto& [xy, last] : c.last_obj) {
        const std::uint32_t now = c.isl->tiles.object(c.isl->tile_index(xy.first, xy.second));
        if (now != last) {
            (now < last ? c.cut_at : c.regrown_at)[xy].push_back(t);
            last = now;
        }
    }
}

inline void run_to(Cloth& c, std::uint64_t tick) {
    while (c.w.clock().tick() < tick) {
        step(c);
    }
}

inline std::uint64_t taken(const Cloth& c) {
    const auto it = c.stats.residents.merged_taken_by_city.find({c.ref, kStoffe});
    return it != c.stats.residents.merged_taken_by_city.end() ? it->second : 0U;
}

inline std::uint32_t houses_reached(const Cloth& c) {
    std::uint32_t n = 0;
    for (const auto& [id, hw] : c.houses) {
        n += hw.rose ? 1U : 0U;
    }
    return n;
}

} // namespace cloth_town

namespace cloth_town {

/// The scoped ledger of one run (this town's two sheep farms and weaver only).
struct Ledger {
    std::uint64_t wool_made = 0, farm_out = 0, wool_loaded = 0;
    std::uint64_t weaver_consumed = 0, weaver_in = 0, weaver_out = 0, weaver_made = 0;
    std::uint64_t wool_store = 0, wool_cargo = 0;
    std::uint64_t cloth_loaded = 0, cloth_cart = 0, cloth_store = 0, cloth_taken = 0;
    std::uint64_t cart_lost = 0, raw_credited = 0, raw_in_farms = 0, raw_consumed = 0,
                  raw_cargo = 0;
    // the unexplained terms
    std::int64_t u_farm = 0, u_wool = 0, u_cloth_made = 0, u_cloth = 0, u_raw = 0;
};

inline Ledger ledger_of(const Cloth& c, std::uint64_t cloth_store0) {
    Ledger l;
    const sim::ForestryStats& fs = c.stats.forestry;
    for (const sim::BuildingId id : c.sheep) {
        const sim::Building* b = c.isl->find_building(id);
        if (b == nullptr) {
            continue;
        }
        const sim::Farm& f = std::get<sim::Farm>(b->payload);
        const auto it = fs.farms.find(id);
        if (it != fs.farms.end()) {
            l.wool_made += it->second.made_88;
            l.raw_consumed += it->second.consumed_88;
        }
        l.farm_out += f.out;
        l.raw_in_farms += f.raw;
        l.wool_loaded += c.stats.loaded_from.count(id) ? c.stats.loaded_from.at(id) : 0U;
    }
    const sim::Building* wb = c.isl->find_building(c.weaver);
    if (wb != nullptr) {
        const sim::Workshop& w = std::get<sim::Workshop>(wb->payload);
        l.weaver_in = w.in[0];
        l.weaver_out = w.out[0];
        const auto it = fs.workshops.find(c.weaver);
        if (it != fs.workshops.end()) {
            l.weaver_consumed = it->second.consumed_88[0];
            l.weaver_made = it->second.made_88[0];
        }
        l.cloth_loaded =
            c.stats.loaded_from.count(c.weaver) ? c.stats.loaded_from.at(c.weaver) : 0U;
    }
    for (const auto& s : c.w.figures().list.slots) {
        if (!s || s->expiry_stamp.has_value()) {
            continue;
        }
        // a cart's home is a market of the target city, a carrier's its workshop or farm: the
        // exact city and owner, not "somewhere on the island"
        const bool ours = in_chain(c, *s);
        if (s->cargo_goods == kWolle && ours && s->job != 0x2a && s->job != 0x2e) {
            l.wool_cargo += s->cargo_88;
        }
        if (s->cargo_goods == kStoffe && ours) {
            l.cloth_cart += s->cargo_88;
        }
        if (s->harvest.has_value() && s->cargo_goods == kGras && ours) {
            l.raw_cargo += s->cargo_88;
        }
    }
    l.wool_store = c.city->stock.stock[kWolle];
    l.cloth_store = c.city->stock.stock[kStoffe];
    l.cloth_taken = taken(c);
    // cart_lost is keyed by goods only (world-wide), see leather_town.hpp
    l.cart_lost = c.stats.cart_lost.count(kStoffe) ? c.stats.cart_lost.at(kStoffe) : 0U;
    // credited raw of this town's farms = consumed + in the farms + in the animals' hands (+ lost =
    // 0 asserted below)
    l.u_farm = std::int64_t(l.wool_made) - std::int64_t(l.farm_out) - std::int64_t(l.wool_loaded);
    l.u_wool = std::int64_t(l.wool_loaded) -
               (std::int64_t(l.wool_store) + std::int64_t(l.weaver_consumed) +
                std::int64_t(l.weaver_in) + std::int64_t(l.wool_cargo));
    l.u_cloth_made =
        std::int64_t(l.weaver_made) - std::int64_t(l.weaver_out) - std::int64_t(l.cloth_loaded);
    l.u_cloth =
        std::int64_t(l.cloth_loaded) - std::int64_t(l.cloth_cart) -
        (std::int64_t(l.cloth_store) - std::int64_t(cloth_store0) + std::int64_t(l.cloth_taken));
    return l;
}

} // namespace cloth_town

namespace cloth_town {

struct Regrowth {
    std::uint64_t n = 0, min = ~0ULL, max = 0;
    std::uint64_t cut_events = 0, tiles_cut = 0;
};

inline Regrowth regrowth_of(const Cloth& c) {
    Regrowth g;
    for (const auto& [xy, cuts] : c.cut_at) {
        g.cut_events += cuts.size();
        g.tiles_cut += 1;
        const auto it = c.regrown_at.find(xy);
        if (it == c.regrown_at.end()) {
            continue;
        }
        for (std::size_t i = 0; i < cuts.size() && i < it->second.size(); ++i) {
            const std::uint64_t d = it->second[i] - cuts[i];
            g.min = std::min(g.min, d);
            g.max = std::max(g.max, d);
            ++g.n;
        }
    }
    return g;
}

inline std::uint64_t grass_in_hands(const Cloth& c) {
    std::uint64_t n = 0;
    for (const auto& s : c.w.figures().list.slots) {
        if (s && !s->expiry_stamp.has_value() && s->harvest.has_value() &&
            s->cargo_goods == kGras &&
            std::find(c.sheep.begin(), c.sheep.end(), s->home) != c.sheep.end()) {
            n += s->cargo_88;
        }
    }
    return n;
}

/// Injects one figure into the world for a scope control (a NAMED test setup, not a game
/// command): the control worlds hold no figure at tick 0, so every figure in them is one of ours.
inline sim::FigureId inject(Cloth& c, sim::PlayerId owner, sim::BuildingId home, std::uint32_t job,
                            sim::GoodsId goods, std::uint32_t amount,
                            std::optional<sim::ResidentCargo> resident_cargo = std::nullopt) {
    static std::uint32_t next = 4000000; // far above any id the short runs of these controls use
    sim::Figure f;
    f.id = sim::FigureId{next++};
    f.owner = owner;
    f.home = home;
    f.job = job;
    f.cargo_goods = goods;
    f.cargo_88 = amount;
    if (resident_cargo.has_value()) {
        sim::ResidentState rs;
        rs.cargo.push_back(*resident_cargo);
        f.resident = rs;
    }
    const sim::FigureId id = f.id;
    const sim::Slot slot = sim::add_figure(sim::WorldTestAccess::figures(c.w), std::move(f),
                                           c.w.config(), c.w.rules());
    REQUIRE(slot != sim::kNoSlot);
    return id;
}

inline void remove_figures(Cloth& c) {
    std::vector<sim::FigureId> ids;
    for (const auto& s : c.w.figures().list.slots) {
        if (s) {
            ids.push_back(s->id);
        }
    }
    for (const sim::FigureId id : ids) {
        (void)sim::remove_figure(sim::WorldTestAccess::figures(c.w), id);
    }
}

} // namespace cloth_town
