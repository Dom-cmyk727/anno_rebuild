// The resident/supplier transactions (resident.hpp; docs/sim/population-live.md section 2).
#include <algorithm>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <anno/sim/resident.hpp>

namespace anno::sim {

namespace {

constexpr std::int64_t kU32Max = std::numeric_limits<std::uint32_t>::max();

/// trunc(a / b) toward zero for b != 0: C++ integer division already truncates toward zero.
std::int64_t tdiv(std::int64_t a, std::int64_t b) {
    return a / b;
}

/// The need slot of a goods id, need_count when there is none (a goods id beyond the lookup).
std::size_t slot_of(const HouseFacts& h, GoodsId g) {
    return g < h.lookup.size() ? h.lookup[g] : h.needs.size();
}

struct Satisfaction {
    std::int64_t s = 0;
    bool fault = false;
};

/// s = min(trunc(1536 * stock / (E * Menge)), 160); a zero divisor is the fault.
Satisfaction satisfaction(const HouseFacts& h, std::size_t slot) {
    const std::int64_t d = static_cast<std::int64_t>(h.e) * h.needs[slot].menge;
    if (d == 0) {
        return {0, true};
    }
    const std::int64_t stock = slot < h.stock.size() ? h.stock[slot] : 0;
    return {std::min(tdiv(kUrgencyScale * stock, d), kUrgencySaturation), false};
}

/// The urgency of a need slot: 128 - s, raised to the maximum along the IdSec chain of its ware
/// (D helper 0x10076960), at most goods.size() steps. false = a divisor was zero.
bool urgency(const HouseFacts& h, std::size_t slot, std::int64_t& out) {
    Satisfaction first = satisfaction(h, slot);
    if (first.fault) {
        return false;
    }
    std::int64_t urg = kUrgencyBase - first.s;
    std::size_t cur = slot;
    for (std::size_t step = 0; step < h.goods.size(); ++step) {
        const GoodsId ware = h.needs[cur].ware;
        const GoodsId sec = ware < h.goods.size() ? h.goods[ware].id_sec : 0;
        if (sec == 0) {
            break;
        }
        const std::size_t next = slot_of(h, sec);
        if (next >= h.needs.size()) {
            break;
        }
        const Satisfaction n = satisfaction(h, next);
        if (n.fault) {
            return false;
        }
        urg = std::max(urg, kUrgencyBase - n.s);
        cur = next;
    }
    out = urg;
    return true;
}

} // namespace

HouseWalkerFacts house_walker_facts(const BuildingType& type) {
    HouseWalkerFacts f;
    if (!type.has_prod) {
        return f;
    }
    if (!type.prod.max_figures.empty()) {
        f.walker_max = type.prod.max_figures[0];
    }
    if (!type.prod.figure_type_count.empty()) {
        f.variants = type.prod.figure_type_count[0] & 0xffU; // D: masked to a byte (10a)
    }
    const std::size_t listed = std::min<std::size_t>(f.variants, type.prod.figure_ids.size());
    f.figure_ids = std::span<const std::uint32_t>(type.prod.figure_ids.data(), listed);
    return f;
}

std::string house_walker_problem(const BuildingType& type) {
    const HouseWalkerFacts f = house_walker_facts(type);
    if (f.walker_max > 0 && f.variants == 0) {
        return "house type " + std::to_string(type.id.v) + ": walker maximum " +
               std::to_string(f.walker_max) + " with no figure type (population-live.md 10a)";
    }
    if (f.figure_ids.size() < f.variants) {
        return "house type " + std::to_string(type.id.v) + ": " + std::to_string(f.variants) +
               " figure types, only " + std::to_string(f.figure_ids.size()) + " ids listed";
    }
    return {};
}

const char* resident_fault_name(ResidentFault f) {
    switch (f) {
    case ResidentFault::None:
        return "none";
    case ResidentFault::ChoiceDivisor:
        return "choice_divisor";
    case ResidentFault::GoodsOutOfTable:
        return "goods_out_of_table";
    case ResidentFault::BilledOverflow:
        return "billed_overflow";
    case ResidentFault::StockOverflow:
        return "stock_overflow";
    case ResidentFault::CargoOverflow:
        return "overflow";
    }
    return "";
}

std::int64_t house_demand(std::uint32_t menge, std::uint32_t e, std::uint32_t stock) {
    return tdiv(static_cast<std::int64_t>(menge) * e * 256, kDemandDivisor) -
           static_cast<std::int64_t>(stock / 16);
}

ChoiceResult house_choice(const HouseFacts& house, const SupplierOffer& offer,
                          std::span<const ResidentCargo> carried,
                          std::span<const ResidentCargo> carried_more) {
    GoodsId cand[2] = {offer.goods[0], offer.goods[1]};
    // Skip rule (D): amount != 0 first, then goods == A / B. A and B are tested independently.
    for (const std::span<const ResidentCargo> list : {carried, carried_more}) {
        for (const ResidentCargo& c : list) {
            if (c.amount_88 != 0) {
                for (GoodsId& g : cand) {
                    if (g == c.goods) {
                        g = 0;
                    }
                }
            }
        }
    }
    ChoiceResult r;
    const std::int64_t residents = tdiv(house.residents_64, 64);
    for (int k = 0; k < 2; ++k) {
        const GoodsId g = cand[k];
        if (g == 0) {
            continue;
        }
        const std::size_t slot = slot_of(house, g);
        if (slot >= house.needs.size()) {
            continue;
        }
        std::int64_t urg = 0;
        if (!urgency(house, slot, urg)) {
            ChoiceResult f;
            f.fault = ResidentFault::ChoiceDivisor;
            return f;
        }
        const std::int64_t thr =
            tdiv(residents * house.needs[slot].menge * kThresholdNum, kThresholdDen);
        if (urg >= 0 && static_cast<std::int64_t>(offer.avail[k]) > thr && offer.enabled[k]) {
            // A is accepted at once; B only when strictly better than what was accepted (best
            // starts at -1, so B alone is accepted too).
            if (k == 0 || urg > r.score) {
                r.score = urg;
                r.goods = g;
            }
        }
    }
    return r;
}

AvailResult supplier_available(const SupplierModel& s, std::span<const std::uint32_t> city_store,
                               GoodsId g) {
    AvailResult r;
    switch (s.cls) {
    case RuntimeClass::MarketStall:
        if (g >= city_store.size()) {
            r.fault = ResidentFault::GoodsOutOfTable;
        } else {
            r.amount = city_store[g];
        }
        break;
    case RuntimeClass::PublicService:
    case RuntimeClass::School:
    case RuntimeClass::Doctor:
        r.amount = kServiceAvailability;
        break;
    case RuntimeClass::FireBrigade:
        r.amount = kFireBrigadeAvailability;
        break;
    case RuntimeClass::Inn:
        r.amount = s.tavern_stock;
        break;
    default:
        break;
    }
    return r;
}

bool supplier_enabled(const SupplierModel& s, GoodsId g) {
    if (s.cls == RuntimeClass::MarketStall) {
        // D: the bytes test `out0 != g`: output 0 reads flag +0x18, every other goods flag +0x19.
        return g == s.out[0] ? s.flag[0] : s.flag[1];
    }
    return true;
}

BillResult bill(BillLedger& ledger, GoodsId g, std::uint32_t amount, bool take) {
    BillResult r;
    if (g < ledger.first || g > ledger.last) {
        r.returned = amount;
        return r;
    }
    if (g >= ledger.billed.size() || (take && g >= ledger.store.size())) {
        r.fault = ResidentFault::GoodsOutOfTable;
        return r;
    }
    if (static_cast<std::int64_t>(ledger.billed[g]) + amount > kU32Max) {
        r.fault = ResidentFault::BilledOverflow;
        return r;
    }
    ledger.billed[g] += amount;
    if (!take) {
        r.returned = amount;
        return r;
    }
    const std::uint32_t t = std::min(amount, ledger.store[g]);
    ledger.store[g] -= t;
    r.returned = t;
    return r;
}

BillResult supplier_bill(SupplierModel& s, BillLedger& ledger, GoodsId g, std::uint32_t amount) {
    switch (s.cls) {
    case RuntimeClass::MarketStall:
        return bill(ledger, g, amount, true);
    case RuntimeClass::PublicService:
    case RuntimeClass::School:
        return bill(ledger, g, amount, false);
    case RuntimeClass::Inn: {
        // D 0x100756e0: the input good is billed with the CLIPPED amount t (take = 0), even t = 0.
        const std::uint32_t t = std::min(s.tavern_stock, amount);
        BillResult r = bill(ledger, s.inn_input, t, false);
        if (r.fault != ResidentFault::None) {
            r.returned = 0;
            return r;
        }
        s.tavern_stock -= t;
        r.returned = t;
        return r;
    }
    default:
        return {};
    }
}

DeliverResult house_deliver(const HouseFacts& house, std::span<std::uint32_t> stock, GoodsId g,
                            std::uint32_t amount) {
    DeliverResult r;
    const std::size_t n = house.needs.size();
    const std::size_t slot = slot_of(house, g);
    if (slot >= n || slot >= stock.size()) {
        return r;
    }
    const std::uint64_t add = std::uint64_t{amount} * kStockPerDelivered;
    // D 0x10076610: the replay is a recursive deliver of the IdSec substitute (house vtable slot
    // 0xc0 is the function itself): every substitute that has a need slot is raised and replays its
    // own IdSec; a substitute without a slot ends the chain. P-3: at most goods.size() replays.
    std::vector<std::size_t> chain{slot};
    GoodsId cur = g;
    for (std::size_t step = 0; step < house.goods.size(); ++step) {
        const GoodsId sec = cur < house.goods.size() ? house.goods[cur].id_sec : 0;
        if (sec == 0) {
            break;
        }
        const std::size_t next = slot_of(house, sec);
        if (next >= n || next >= stock.size()) {
            break;
        }
        chain.push_back(next);
        cur = sec;
    }
    // Check every add of the chain (slots may repeat) before any is made.
    std::map<std::size_t, std::uint64_t> total;
    for (const std::size_t k : chain) {
        total[k] += add;
    }
    for (const auto& [k, v] : total) {
        if (stock[k] + v > static_cast<std::uint64_t>(kU32Max)) {
            r.fault = ResidentFault::StockOverflow;
            return r;
        }
    }
    for (const auto& [k, v] : total) {
        stock[k] += static_cast<std::uint32_t>(v);
    }
    r.touched = static_cast<std::uint32_t>(chain.size());
    return r;
}

BuyResult resident_buy(const HouseFacts& house, SupplierModel& supplier, BillLedger& ledger,
                       std::uint32_t capacity_88, std::span<const ResidentCargo> others,
                       std::span<ResidentCargo> cargo) {
    BuyResult r;
    const std::size_t n = cargo.size();
    std::size_t i = 0;
    while (i < n && cargo[i].goods != 0) {
        ++i;
    }
    if (i >= n) {
        return r;
    }
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1 && !(i < n && cargo[i].goods == 0)) {
            break;
        }
        SupplierOffer offer;
        for (int k = 0; k < 2; ++k) {
            offer.goods[k] = supplier.out[k];
            const AvailResult a = supplier_available(supplier, ledger.store, supplier.out[k]);
            if (a.fault != ResidentFault::None && supplier.out[k] != 0) {
                r.fault = a.fault;
                return r;
            }
            offer.avail[k] = a.amount;
            offer.enabled[k] = supplier_enabled(supplier, supplier.out[k]);
        }
        const ChoiceResult c = house_choice(house, offer, cargo, others);
        if (c.fault != ResidentFault::None) {
            r.fault = c.fault;
            return r;
        }
        if (c.score < kPurchaseScoreGate) {
            break;
        }
        const int k = c.goods == offer.goods[0] ? 0 : 1;
        const std::size_t slot = slot_of(house, c.goods);
        const std::int64_t amt =
            std::min<std::int64_t>({static_cast<std::int64_t>(offer.avail[k]),
                                    house_demand(house.needs[slot].menge, house.e,
                                                 slot < house.stock.size() ? house.stock[slot] : 0),
                                    static_cast<std::int64_t>(capacity_88)});
        if (amt > 0) {
            const BillResult b =
                supplier_bill(supplier, ledger, c.goods, static_cast<std::uint32_t>(amt));
            if (b.fault != ResidentFault::None) {
                r.fault = b.fault;
                return r;
            }
            if (static_cast<std::int64_t>(cargo[i].amount_88) + b.returned > kU32Max) {
                r.fault = ResidentFault::CargoOverflow;
                return r;
            }
            cargo[i].goods = c.goods;
            cargo[i].amount_88 += b.returned;
            ++i;
            ++r.purchases;
        }
    }
    return r;
}

} // namespace anno::sim
