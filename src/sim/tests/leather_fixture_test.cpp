// MAP-NEEDS-2 N5: the SYNTHETIC MECHANICS FIXTURE hide -> leather -> stall -> households
// (docs/sim/early-needs.md section 13). It claims NOTHING about hunters producing hides: the
// hides are a labelled stock written into the city store by the test; everything after the
// store is the ordinary engine (a tannery built by a public BuildCmd, its carrier, the store, a
// cart to the 1314 stall, the households). Kept out of every real-file ledger.
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>

#include <anno/sim/replay.hpp>
#include <anno/sim/state_fields.hpp>

#include "evidence_out.hpp"
#include "leather_town.hpp"

namespace {

namespace sim = anno::sim;
using namespace leather_town;

constexpr std::uint32_t kHides = 8192; // the LABELLED hide stock (store units)
constexpr std::uint64_t kEnd = 100000; // ticks of the main run
constexpr std::uint64_t kControlEnd = 60000;

std::string hex64(std::uint64_t v) {
    std::ostringstream o;
    o << std::hex << v;
    return o.str();
}

struct Result {
    std::uint64_t first_store = 0, made = 0, taken = 0, store = 0, hide_store = 0, hide_in = 0,
                  consumed = 0, out = 0;
    std::uint32_t houses = 0;
    std::uint64_t hash = 0, rng_cursor = 0;
};

Result run_arm(const Original& o, std::uint32_t hides, bool tannery, bool stall,
               std::uint64_t end) {
    auto L = start_leather(o, hides, tannery, stall);
    Cloth& c = *L->t;
    Result r;
    while (c.w.clock().tick() < end) {
        step(c);
        if (r.first_store == 0 && c.city->stock.stock[kLeder] > 0) {
            r.first_store = c.w.clock().tick();
        }
    }
    const LeatherLedger l = leather_ledger(*L);
    CHECK(l.u_hides == 0);
    CHECK(l.u_made == 0);
    CHECK(l.u_loaded == 0);
    r.made = l.leather_made;
    r.taken = l.leather_taken;
    r.store = l.leather_store;
    r.hide_store = l.hide_store;
    r.hide_in = l.hide_in;
    r.consumed = l.hide_consumed;
    r.out = l.leather_out;
    r.houses = houses_reached(c);
    r.hash = c.w.state_hash();
    r.rng_cursor = c.w.rng().cursor();
    return r;
}

} // namespace

TEST_CASE("synthetic mechanics fixture: labelled hides become leather and households take it") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto L = start_leather(*o, kHides, true, true);
    Cloth& c = *L->t;
    const sim::BuildingType& tt = *o->types->find(L->tannery_type);
    // setup (asserted): a public BuildCmd of the stall and of the tannery, both PAID; the hide
    // stock is the only labelled thing; the zero-start guard of a real-file proof refuses it
    REQUIRE(c.stall_at.has_value());
    REQUIRE(L->tannery_at.has_value());
    CHECK(L->tannery_at->x == 279);
    CHECK(L->tannery_at->y == 350);
    check_paid(c, *o->types->find(c.stall_type), *c.pay_before, *c.pay_after);
    check_paid(c, tt, *L->pay_before, *L->pay_after);
    CHECK_FALSE(chain_goods_zero(*c.city));
    CHECK(c.city->stock.stock[kHaeute] == kHides);
    CHECK(c.city->stock.stock[kLeder] == 0);
    CHECK(c.city->stock.stock[kStoffe] == 0);
    CHECK(c.city->stock.stock[kWolle] == 0);
    std::size_t living = 0;
    for (const auto& s : c.w.figures().list.slots) {
        living += s.has_value() ? 1U : 0U;
    }
    CHECK(living == 0);
    REQUIRE(tt.input_goods[0] == kHaeute);
    REQUIRE(tt.output_goods[0] == kLeder);
    const std::uint64_t in_step = tt.prod.input_per_step[0], out_step = tt.prod.output_per_step[0];
    std::cerr << "PIN recipe per step in " << in_step << " out " << out_step << "\n";

    std::uint64_t first_store = 0, first_cart = 0, first_hide_cargo = 0;
    std::uint64_t checks = 0;
    while (c.w.clock().tick() < kEnd) {
        step(c);
        const std::uint64_t t = c.w.clock().tick();
        if (first_store == 0 && c.city->stock.stock[kLeder] > 0) {
            first_store = t;
        }
        if (t % 5000 == 0) {
            const LeatherLedger l = leather_ledger(*L);
            CHECK(l.u_hides == 0);
            CHECK(l.u_made == 0);
            CHECK(l.u_loaded == 0);
            CHECK(l.cart_lost == 0);
            // the recipe, exactly: leather made / consumed hides = out per step / in per step
            CHECK(l.leather_made * in_step == l.hide_consumed * out_step);
            if (first_cart == 0 && l.leather_cart > 0) {
                first_cart = t;
            }
            if (first_hide_cargo == 0 && l.hide_cargo > 0) {
                first_hide_cargo = t;
            }
            ++checks;
        }
    }
    CHECK(checks == kEnd / 5000);
    const LeatherLedger l = leather_ledger(*L);
    CHECK(l.u_hides == 0);
    CHECK(l.u_made == 0);
    CHECK(l.u_loaded == 0);
    CHECK(l.leather_made * in_step == l.hide_consumed * out_step);
    CHECK(l.leather_made > 0);
    CHECK(l.leather_taken > 0);
    CHECK(l.leather_taken <= l.leather_loaded);
    CHECK(houses_reached(c) > 0);
    // measured pins at stream 130 (hand-checked: 5120 + 780 + 2292 = 8192 hides; 1337 made =
    // 28 + 1309 loaded; 1309 loaded = 0 + 504 + 805; 2292 * 7 = 1337 * 12)
    CHECK(in_step == 12);
    CHECK(out_step == 7);
    CHECK(first_store == 26250);
    CHECK(l.hide_store == 5120);
    CHECK(l.hide_in == 780);
    CHECK(l.hide_consumed == 2292);
    CHECK(l.leather_made == 1337);
    CHECK(l.leather_out == 28);
    CHECK(l.leather_loaded == 1309);
    CHECK(l.leather_cart == 0);
    CHECK(l.leather_store == 504);
    CHECK(l.leather_taken == 805);
    CHECK(houses_reached(c) == 8);
    CHECK(c.w.state_hash() == 0xc6db3b32c5649d2eULL);
    std::cerr << "PIN main first_store " << first_store << " hides_store " << l.hide_store << " in "
              << l.hide_in << " consumed " << l.hide_consumed << " made " << l.leather_made
              << " out " << l.leather_out << " loaded " << l.leather_loaded << " cart "
              << l.leather_cart << " store " << l.leather_store << " taken " << l.leather_taken
              << " houses " << houses_reached(c) << " hash " << hex64(c.w.state_hash()) << "\n";
}

TEST_CASE("synthetic mechanics fixture: controls name each cause") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const Result main_arm = run_arm(*o, kHides, true, true, kControlEnd);
    // (a) vacuous: a tannery and a stall but NO hides -> nothing is made, stored or taken
    const Result no_hides = run_arm(*o, 0, true, true, kControlEnd);
    // (b) hides and a stall but NO tannery -> the hides stay in the store untouched
    const Result no_tannery = run_arm(*o, kHides, false, true, kControlEnd);
    // (c) hides and a tannery but NO stall -> leather is made and stored, nobody takes it
    const Result no_stall = run_arm(*o, kHides, true, false, kControlEnd);
    std::cerr << "PIN controls main made " << main_arm.made << " taken " << main_arm.taken
              << " houses " << main_arm.houses << " | no_hides made " << no_hides.made << " store "
              << no_hides.store << " taken " << no_hides.taken << " | no_tannery "
              << "hide_store " << no_tannery.hide_store << " made " << no_tannery.made
              << " | no_stall made " << no_stall.made << " store " << no_stall.store << " taken "
              << no_stall.taken << " houses " << no_stall.houses << "\n";
    CHECK(main_arm.made == 784); // measured at 60 000 ticks
    CHECK(main_arm.taken == 574);
    CHECK(main_arm.houses == 4);
    CHECK(no_hides.made == 0);
    CHECK(no_hides.store == 0);
    CHECK(no_hides.taken == 0);
    CHECK(no_hides.hide_in == 0);
    CHECK(no_hides.out == 0);
    CHECK(no_hides.houses == 0);
    CHECK(no_tannery.hide_store == kHides);
    CHECK(no_tannery.made == 0);
    CHECK(no_tannery.store == 0);
    CHECK(no_tannery.taken == 0);
    CHECK(no_tannery.houses == 0);
    CHECK(no_stall.made ==
          main_arm.made);         // the stall changes who holds the leather, not the recipe
    CHECK(no_stall.store == 574); // without a stall the same leather stays in the store
    CHECK(no_stall.taken == 0);
    CHECK(no_stall.houses == 0);
}

namespace {

/// Every differing leaf scope of two hash trees, as paths without the leading slash.
void diff_leaves(const sim::HashNode& x, const sim::HashNode& y, std::string path,
                 std::vector<std::string>& out) {
    if (x.hash == y.hash) {
        return;
    }
    path += (path.empty() ? "" : "/") + std::string(x.name) +
            (x.indexed ? "[" + std::to_string(x.index) + "]" : "");
    bool deeper = false;
    for (std::size_t j = 0; j < x.children.size() && j < y.children.size(); ++j) {
        if (x.children[j].hash != y.children[j].hash) {
            deeper = true;
            diff_leaves(x.children[j], y.children[j], path, out);
        }
    }
    if (!deeper) {
        out.push_back(path);
    }
}

struct OtherTown {
    std::vector<std::uint32_t> stock;
    std::int64_t money = 0;
};

OtherTown other_town(Cloth& c, std::uint32_t island) {
    OtherTown t;
    for (auto& isl : sim::WorldTestAccess::islands(c.w)) {
        if (!isl || isl->index.v != island) {
            continue;
        }
        for (auto& ci : isl->cities) {
            if (ci) {
                t.stock.assign(ci->stock.stock.begin(), ci->stock.stock.end());
                t.money = c.w.players().find(ci->owner)->money;
            }
        }
    }
    REQUIRE_FALSE(t.stock.empty());
    return t;
}

/// Removes every Farm-tick building of the OTHER towns with their owners' PUBLIC DemolishCmd.
std::size_t demolish_other_farms(Cloth& c, const Original& o) {
    std::size_t n = 0;
    for (auto& isl : sim::WorldTestAccess::islands(c.w)) {
        if (!isl) {
            continue;
        }
        for (auto& ci : isl->cities) {
            if (!ci || (isl->index == c.isl->index && ci->number == c.city->number)) {
                continue;
            }
            std::vector<sim::TileXY> tiles;
            for (auto& l : ci->index.lists) {
                for (auto& b : l.slots) {
                    const sim::BuildingType* t = b ? o.types->find(b->type) : nullptr;
                    if (t != nullptr && o.types->class_info(t->cls).tick == sim::TickKind::Farm) {
                        tiles.push_back({b->x, b->y});
                    }
                }
            }
            if (tiles.empty()) {
                continue;
            }
            sim::Command k;
            k.issuer = ci->owner;
            k.body = sim::DemolishCmd{sim::CityRef{isl->index, ci->number}, tiles};
            REQUIRE(c.w.apply(k).ok());
            n += tiles.size();
        }
    }
    return n;
}

} // namespace

TEST_CASE("synthetic mechanics fixture: three owners, three towns, shared stream and dispatch") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // The scene holds three towns of three owners: player 0 island 7 (this fixture), player 2
    // island 3 (3 Farm-tick buildings, 2 workshops, 2 small farms, its own stall and 7,680 cloth
    // and leather) and player 1 island 4 (3 Farm-tick buildings, wool 12,800). Arm X: the fixture
    // runs. Arm Y: the same without hides. Arm Z: the fixture with the other towns' farms
    // demolished by their owners (their dispatch draws leave the shared stream).
    auto X = start_leather(*o, kHides, true, true);
    auto Y = start_leather(*o, 0, true, true);
    auto Z = start_leather(*o, kHides, true, true);
    const std::size_t removed = demolish_other_farms(*Z->t, *o);
    CHECK(removed == 6);
    const OtherTown b0 = other_town(*X->t, 3), c0 = other_town(*X->t, 4);
    for (Leather* arm : {X.get(), Y.get(), Z.get()}) {
        run_to(*arm->t, kEnd);
    }
    Cloth& cx = *X->t;
    Cloth& cy = *Y->t;
    Cloth& cz = *Z->t;
    const LeatherLedger lx = leather_ledger(*X), ly = leather_ledger(*Y), lz = leather_ledger(*Z);
    // (1) per-town ledgers balance in every arm; the leather exists only where hides were
    for (const LeatherLedger* l : {&lx, &ly, &lz}) {
        CHECK(l->u_hides == 0);
        CHECK(l->u_made == 0);
        CHECK(l->u_loaded == 0);
    }
    CHECK(lx.leather_made > 0);
    CHECK(ly.leather_made == 0);
    // (2) goods and money are partitioned by owner and town: no hide reaches another town, no
    // leather of this fixture is added to another town's store (their leather only goes DOWN, by
    // their own households), and their money is the same in every arm. The ONE good that differs
    // between the arms in another town is NAHRUNG (26) of the town with working farms: the shared
    // stream moves its farms' timing (3 below)
    const OtherTown bx = other_town(cx, 3), by = other_town(cy, 3), cxo = other_town(cx, 4),
                    cyo = other_town(cy, 4);
    std::set<std::size_t> differing_goods;
    for (std::size_t g = 0; g < bx.stock.size(); ++g) {
        if (bx.stock[g] != by.stock[g]) {
            differing_goods.insert(g);
        }
        if (cxo.stock[g] != cyo.stock[g]) {
            differing_goods.insert(g);
        }
    }
    CHECK(differing_goods == std::set<std::size_t>{26});
    CHECK(bx.money == by.money);
    CHECK(cxo.money == cyo.money);
    CHECK(bx.money == b0.money);
    CHECK(cxo.money == c0.money);
    CHECK(bx.stock[kHaeute] == 0);
    CHECK(cxo.stock[kHaeute] == 0);
    CHECK(bx.stock[kLeder] <= b0.stock[kLeder]); // island 3's own leather only goes down
    CHECK(bx.stock[kLeder] == by.stock[kLeder]);
    CHECK(cxo.stock[kLeder] == c0.stock[kLeder]); // island 4 has none and gets none
    // (3) the shared stream IS a coupling, measured: the two arms' worlds differ in the shared draw
    // cursor, the figure ids, island 3's and 4's tiles (their farms pick other grass tiles) and
    // their buildings' timers, and in NO player, no city stock other than the food above
    sim::HashTree tx, ty;
    (void)cx.w.state_hash(tx, 8);
    (void)cy.w.state_hash(ty, 8);
    std::vector<std::string> leaves;
    diff_leaves(tx.root, ty.root, "", leaves);
    std::map<std::string, std::size_t> categories;
    for (const std::string& p : leaves) {
        const bool island7 = p.find("islands/island[7]") != std::string::npos;
        std::string cat = "other";
        if (p == "world/clock_rng") {
            cat = "clock_rng";
        } else if (p == "world/ids") {
            cat = "ids";
        } else if (p.rfind("world/figures/", 0) == 0) {
            cat = "figures";
        } else if (island7) {
            cat = "island7";
        } else if (p.find("/tiles") != std::string::npos) {
            cat = "other-island tiles";
        } else if (p.find("/building[") != std::string::npos) {
            cat = "other-island building";
        }
        ++categories[cat];
        CHECK_MESSAGE(cat != "other", p);
        CHECK_MESSAGE(p.find("players") == std::string::npos, p);
        CHECK_MESSAGE((island7 || p.find("/stock") == std::string::npos), p);
    }
    for (const auto& [k, n] : categories) {
        std::cerr << "PIN leaf category " << k << " " << n << "\n";
    }
    CHECK(categories["clock_rng"] == 1);
    CHECK(categories["ids"] == 1);
    CHECK(categories["figures"] > 0);
    CHECK(categories["other-island tiles"] == 1); // island 3's grass tiles (measured)
    CHECK(categories["other-island building"] == 11);
    // (4) arm Z: with the other towns' farms gone their dispatch draws leave the shared stream (the
    // cursor differs); the recipe-bound total (leather made) is the same, the split between store,
    // output and carts is not (measured)
    std::cerr << "PIN iso rng_cursor X " << cx.w.rng().cursor() << " Z " << cz.w.rng().cursor()
              << " Y " << cy.w.rng().cursor() << " | X made " << lx.leather_made << " taken "
              << lx.leather_taken << " store " << lx.leather_store << " houses "
              << houses_reached(cx) << " | Z made " << lz.leather_made << " taken "
              << lz.leather_taken << " store " << lz.leather_store << " houses "
              << houses_reached(cz) << "\n";
    CHECK(cx.w.rng().cursor() == 1968);
    CHECK(cz.w.rng().cursor() == 1881);
    CHECK(cy.w.rng().cursor() == 2040);
    CHECK(lx.leather_made == 1337);
    CHECK(lx.leather_taken == 805);
    CHECK(lx.leather_store == 504);
    CHECK(lz.leather_made == 1337);
    CHECK(lz.leather_taken == 805);
    CHECK(lz.leather_store == 441); // the split between store and cart moved with the stream
    CHECK(houses_reached(cz) == 8);
    // the demolished towns: their farms are gone and no fixture good reached them
    const OtherTown bz = other_town(cz, 3), cz4 = other_town(cz, 4);
    CHECK(bz.stock[kHaeute] == 0);
    CHECK(cz4.stock[kHaeute] == 0);
    CHECK(bz.stock[kLeder] <= b0.stock[kLeder]);
}

TEST_CASE(
    "synthetic mechanics fixture: a rival owner's tannery and hides stay in the rival's town") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // This town (player 0, island 7) has a tannery and a stall but NO hides. The rival (player 2,
    // island 3) gets a tannery by its OWN public BuildCmd and the same LABELLED hide stock in its
    // own store. The rival's hides become the rival's leather; nothing of it reaches this town.
    auto A = start_leather(*o, 0, true, true);
    Cloth& c = *A->t;
    sim::Island* rival_isl = nullptr;
    sim::City* rival = nullptr;
    for (auto& isl : sim::WorldTestAccess::islands(c.w)) {
        if (isl && isl->index.v == kIsland) {
            for (auto& ci : isl->cities) {
                if (ci && ci->owner.v == kOwner) {
                    rival_isl = &*isl;
                    rival = &*ci;
                }
            }
        }
    }
    REQUIRE(rival != nullptr);
    const sim::CityRef rref{rival_isl->index, rival->number};
    // the site: the nearest accepted site in rings around the rival's first workshop
    sim::TileXY centre{0, 0};
    for (auto& l : rival->index.lists) {
        for (auto& b : l.slots) {
            const sim::BuildingType* t = b ? o->types->find(b->type) : nullptr;
            if (t != nullptr && t->cls == sim::RuntimeClass::Workshop && centre.x == 0) {
                centre = {b->x, b->y};
            }
        }
    }
    REQUIRE(centre.x != 0);
    sim::BuildingId rival_tannery;
    for (int r = 1; r <= 14 && !rival_tannery.valid(); ++r) {
        for (int dy = -r; dy <= r && !rival_tannery.valid(); ++dy) {
            for (int dx = -r; dx <= r && !rival_tannery.valid(); ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) {
                    continue;
                }
                sim::Command k;
                k.issuer = rival->owner;
                k.body = sim::BuildCmd{
                    rref,
                    {sim::PlacementWire{A->tannery_type, centre.x + dx, centre.y + dy, 0, 0, 0}}};
                if (c.w.apply(k).ok()) {
                    const sim::Building* b = rival_isl->building_at(
                        c.w.config().layout, rival->number, centre.x + dx, centre.y + dy);
                    REQUIRE(b != nullptr);
                    rival_tannery = b->id;
                }
            }
        }
    }
    REQUIRE(rival_tannery.valid());
    const std::uint32_t rival_leather0 = rival->stock.stock[kLeder];
    rival->stock.stock[kHaeute] = kHides; // LABELLED stock, in the RIVAL's store
    // every step: each figure that carries hides or leather belongs to the rival's owner; the count
    // of such observations proves the check is not vacuous
    std::uint64_t observed = 0;
    while (c.w.clock().tick() < kControlEnd) {
        step(c);
        for (const auto& f : c.w.figures().list.slots) {
            if (f && !f->expiry_stamp.has_value() &&
                (f->cargo_goods == kHaeute || f->cargo_goods == kLeder) && f->cargo_88 > 0) {
                ++observed;
                CHECK(f->owner == rival->owner);
            }
        }
    }
    CHECK(observed > 0);
    std::cerr << "PIN rival observations of carried hides or leather " << observed << "\n";
    const LeatherLedger la = leather_ledger(*A);
    const auto it = c.stats.forestry.workshops.find(rival_tannery);
    REQUIRE(it != c.stats.forestry.workshops.end());
    const std::uint64_t rival_made = it->second.made_88[0];
    const std::uint64_t rival_loaded =
        c.stats.loaded_from.count(rival_tannery) ? c.stats.loaded_from.at(rival_tannery) : 0U;
    std::cerr << "PIN rival made " << rival_made << " loaded " << rival_loaded << " rival hides "
              << rival->stock.stock[kHaeute] << " rival leather " << rival_leather0 << " -> "
              << rival->stock.stock[kLeder] << " | this town made " << la.leather_made << " hides "
              << la.hide_store << " leather store " << la.leather_store << " taken "
              << la.leather_taken << " houses " << houses_reached(c) << "\n";
    // the rival's tannery works, in the rival's town (measured at 60 000 ticks, stream 130)
    CHECK(rival_made == 665);
    CHECK(rival_loaded == 658);
    CHECK(rival->stock.stock[kLeder] == 7936);
    CHECK(rival->stock.stock[kHaeute] < kHides);
    // this town stays at zero in every term: no hide, no leather made, stored or taken, no house
    CHECK(la.hide_store == 0);
    CHECK(la.hide_in == 0);
    CHECK(la.hide_cargo == 0);
    CHECK(la.leather_made == 0);
    CHECK(la.leather_out == 0);
    CHECK(la.leather_loaded == 0);
    CHECK(la.leather_store == 0);
    CHECK(la.leather_taken == 0);
    CHECK(houses_reached(c) == 0);
    CHECK(c.city->stock.stock[kLeder] == 0);
    CHECK(c.city->stock.stock[kHaeute] == 0);
    CHECK(la.u_hides == 0);
    CHECK(la.u_made == 0);
    CHECK(la.u_loaded == 0);
}
