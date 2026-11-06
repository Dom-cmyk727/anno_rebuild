// MAP-NEEDS-2 N5: the SYNTHETIC MECHANICS FIXTURE hide -> leather -> stall -> households on the
// Tutorial island-7 town. It claims NOTHING about hunters producing hides (the wildlife
// lifecycle is not ported; docs/sim/early-needs.md section 11). The hides are a LABELLED stock
// written into the city store by the test (a named setup step, not a game command); everything
// after the store is the ordinary engine: a tannery (the Workshop type found by its goods,
// TIERHAEUTE -> LEDER) built by a public BuildCmd, its carrier, the store, a cart to the 1314
// stall, the households' LEDER need.
#pragma once

#include "cloth_town.hpp"

namespace leather_town {

namespace sim = anno::sim;
using namespace tutorial_town;
using namespace island7;
using namespace cloth_town;

inline constexpr sim::GoodsId kHaeute = 18; // TIERHAEUTE

struct Leather {
    std::unique_ptr<Cloth> t; // the cloth town with its producers removed (the stall is built)
    sim::TypeId tannery_type;
    sim::BuildingId tannery;
    std::optional<sim::TileXY> tannery_at;
    std::optional<PaySnap> pay_before, pay_after;
    std::uint64_t hides0 = 0; // the labelled stock
};

/// The zero-start guard of a real-file proof (C18): none of the chain's goods in the store. A
/// labelled stock fails it by construction, so a labelled fixture can never pass as a real-file
/// proof.
inline bool chain_goods_zero(const sim::City& city) {
    return city.stock.stock[kWolle] == 0 && city.stock.stock[kStoffe] == 0 &&
           city.stock.stock[kLeder] == 0 && city.stock.stock[18] == 0;
}

/// After World::load the island and city pointers of the harness are stale: re-find them.
inline void rebind(Cloth& c) {
    c.isl = nullptr;
    c.city = nullptr;
    for (auto& i : sim::WorldTestAccess::islands(c.w)) {
        if (i && i->index == c.ref.island) {
            c.isl = &*i;
            for (auto& ci : i->cities) {
                if (ci && ci->number == c.ref.city) {
                    c.city = &*ci;
                }
            }
        }
    }
    REQUIRE(c.city != nullptr);
    c.w.set_transport_stats(&c.stats);
}

/// The sum of the households' LEDER stock of the fixture's city in any world (a loaded one too).
inline std::uint64_t house_leather(sim::World& w, const sim::TypeRegistry& types) {
    std::uint64_t sum = 0;
    for (auto& isl : sim::WorldTestAccess::islands(w)) {
        if (!isl || isl->index.v != kIsland7) {
            continue;
        }
        for (auto& ci : isl->cities) {
            if (!ci || ci->number.v != kCity1) {
                continue;
            }
            for (auto& l : ci->index.lists) {
                for (auto& b : l.slots) {
                    const auto* h = b ? std::get_if<sim::House>(&b->payload) : nullptr;
                    if (h == nullptr) {
                        continue;
                    }
                    const auto g = sim::house_group(w.config(), types.find(b->type)->prodtype);
                    const auto view = types.population_view(*g, w.config());
                    sum += h->stock[view.value().derived->lookup[kLeder]];
                }
            }
        }
    }
    return sum;
}

/// The tannery type by its goods (never by id): a Workshop whose first input is TIERHAEUTE and
/// whose first output is LEDER.
inline sim::TypeId find_tannery_type(const Original& o) {
    for (const sim::BuildingType& t : o.types->types()) {
        if (t.cls == sim::RuntimeClass::Workshop && t.has_prod && !t.input_goods.empty() &&
            !t.output_goods.empty() && t.input_goods[0] == kHaeute && t.output_goods[0] == kLeder) {
            return t.id;
        }
    }
    return sim::TypeId{};
}

/// One cloth town without its sheep farms and weaver (no cloth), the 1314 stall built, the
/// households' watcher re-pointed to LEDER. `hides` = the LABELLED hide stock (0: none);
/// `tannery`: whether the tannery is built by BuildCmd.
inline std::unique_ptr<Leather> start_leather(const Original& o, std::uint32_t hides, bool tannery,
                                              bool stall = true) {
    auto L = std::make_unique<Leather>();
    L->t = start_cloth(o, stall, 0, false);
    Cloth& c = *L->t;
    const auto& T = *o.types;
    L->tannery_type = find_tannery_type(o);
    REQUIRE(L->tannery_type.v != 0);
    if (tannery) {
        // the site: the nearest accepted site in rings around the demolished weaver (279,350)
        const sim::TileXY centre{279, 350};
        L->pay_before = pay_snap(c, *T.find(L->tannery_type));
        bool done = false;
        for (int r = 0; r <= 10 && !done; ++r) {
            for (int dy = -r; dy <= r && !done; ++dy) {
                for (int dx = -r; dx <= r && !done; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) {
                        continue;
                    }
                    const auto res = c.w.apply(
                        cmd(c, sim::BuildCmd{c.ref,
                                             {sim::PlacementWire{L->tannery_type, centre.x + dx,
                                                                 centre.y + dy, 0, 0, 0}}}));
                    if (res.ok()) {
                        L->tannery_at = sim::TileXY{centre.x + dx, centre.y + dy};
                        done = true;
                    }
                }
            }
        }
        REQUIRE(done);
        L->pay_after = pay_snap(c, *T.find(L->tannery_type));
        const sim::Building* b = c.isl->building_at(c.w.config().layout, c.city->number,
                                                    L->tannery_at->x, L->tannery_at->y);
        REQUIRE(b != nullptr);
        REQUIRE(b->type == L->tannery_type);
        L->tannery = b->id;
    }
    if (hides != 0) {
        c.city->stock.stock[kHaeute] = hides; // LABELLED stock fixture (named setup, no command)
        L->hides0 = hides;
    }
    // the households' watcher follows LEDER instead of STOFFE
    for (auto& [id, hw] : c.houses) {
        const sim::Building* b = c.isl->find_building(id);
        const auto* h = std::get_if<sim::House>(&b->payload);
        const sim::BuildingType* t = T.find(b->type);
        const auto g = sim::house_group(c.w.config(), t->prodtype);
        REQUIRE(g.has_value());
        const auto view = T.population_view(*g, c.w.config());
        REQUIRE(view.ok());
        hw.stoffe_slot = view.value().derived->lookup[kLeder];
        hw.stoffe0 = h->stock[hw.stoffe_slot];
        hw.prev = hw.stoffe0;
        hw.rose = false;
    }
    return L;
}

inline std::uint64_t taken_of(const Cloth& c, sim::GoodsId g) {
    const auto it = c.stats.residents.merged_taken_by_city.find({c.ref, g});
    return it != c.stats.residents.merged_taken_by_city.end() ? it->second : 0U;
}

/// The scoped ledger of the hides and the leather of one run (this town's tannery only).
struct LeatherLedger {
    std::uint64_t hide_store = 0, hide_in = 0, hide_cargo = 0, hide_consumed = 0;
    std::uint64_t leather_made = 0, leather_out = 0, leather_loaded = 0, leather_cart = 0,
                  leather_store = 0, leather_taken = 0, cart_lost = 0;
    std::int64_t u_hides = 0, u_made = 0, u_loaded = 0;
};

inline LeatherLedger leather_ledger(const Leather& L) {
    const Cloth& c = *L.t;
    LeatherLedger l;
    const sim::ForestryStats& fs = c.stats.forestry;
    const sim::Building* tb = L.tannery.valid() ? c.isl->find_building(L.tannery) : nullptr;
    if (tb != nullptr) {
        const sim::Workshop& w = std::get<sim::Workshop>(tb->payload);
        l.hide_in = w.in[0];
        l.leather_out = w.out[0];
        const auto it = fs.workshops.find(L.tannery);
        if (it != fs.workshops.end()) {
            l.hide_consumed = it->second.consumed_88[0];
            l.leather_made = it->second.made_88[0];
        }
        l.leather_loaded =
            c.stats.loaded_from.count(L.tannery) ? c.stats.loaded_from.at(L.tannery) : 0U;
    }
    for (const auto& s : c.w.figures().list.slots) {
        if (!s || s->expiry_stamp.has_value()) {
            continue;
        }
        const bool ours = in_chain(c, *s);
        if (s->cargo_goods == kHaeute && ours) {
            l.hide_cargo += s->cargo_88;
        }
        if (s->cargo_goods == kLeder && ours) {
            l.leather_cart += s->cargo_88;
        }
    }
    l.hide_store = c.city->stock.stock[kHaeute];
    l.leather_store = c.city->stock.stock[kLeder];
    l.leather_taken = taken_of(c, kLeder);
    // cart_lost is keyed by goods only (world-wide): a zero here is a world-wide zero, which
    // implies zero for the target chain; a non-zero would need the rival's activity excluded first
    l.cart_lost = c.stats.cart_lost.count(kLeder) ? c.stats.cart_lost.at(kLeder) : 0U;
    // hides: the labelled stock is all there is: store + tannery input + in carriers' hands +
    // consumed = the labelled stock
    l.u_hides =
        std::int64_t(L.hides0) - (std::int64_t(l.hide_store) + std::int64_t(l.hide_in) +
                                  std::int64_t(l.hide_cargo) + std::int64_t(l.hide_consumed));
    l.u_made =
        std::int64_t(l.leather_made) - std::int64_t(l.leather_out) - std::int64_t(l.leather_loaded);
    l.u_loaded = std::int64_t(l.leather_loaded) - std::int64_t(l.leather_cart) -
                 (std::int64_t(l.leather_store) + std::int64_t(l.leather_taken));
    return l;
}

} // namespace leather_town
