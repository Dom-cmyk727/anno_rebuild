// MAP-POP-LIVE-1 L1: the resident/supplier transactions (src/sim/src/resident.cpp) against
// fixtures/synthetic/sim/resident_vectors.txt. The expectations in that file were derived by hand
// from docs/sim/population-live.md section 2 and 10 before this module existed;
// tools/resident_ref.py checks the same file independently. The module is not called from the
// driver (the House dispatch is still a no-op in L1).
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/resident.hpp>

namespace {

namespace sim = anno::sim;

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(s);
    while (std::getline(is, cur, sep)) {
        out.push_back(cur);
    }
    return out;
}

using Kv = std::map<std::string, std::string>;

Kv parse_kv(const std::vector<std::string>& t, std::size_t from, std::size_t to) {
    Kv m;
    for (std::size_t i = from; i < to; ++i) {
        const auto eq = t[i].find('=');
        REQUIRE_MESSAGE(eq != std::string::npos, t[i]);
        m[t[i].substr(0, eq)] = t[i].substr(eq + 1);
    }
    return m;
}

std::vector<std::int64_t> ints(const std::string& s) {
    std::vector<std::int64_t> v;
    if (s == "none") {
        return v;
    }
    for (const std::string& t : split(s, ',')) {
        v.push_back(std::stoll(t, nullptr, 0));
    }
    return v;
}

std::vector<std::pair<std::int64_t, std::int64_t>> pairs(const std::string& s) {
    std::vector<std::pair<std::int64_t, std::int64_t>> v;
    if (s == "none") {
        return v;
    }
    for (const std::string& t : split(s, ',')) {
        const auto c = t.find(':');
        REQUIRE(c != std::string::npos);
        v.emplace_back(std::stoll(t.substr(0, c)), std::stoll(t.substr(c + 1)));
    }
    return v;
}

std::string fmt_list(const std::vector<std::uint32_t>& v) {
    if (v.empty()) {
        return "none";
    }
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) {
        s += (i ? "," : "") + std::to_string(v[i]);
    }
    return s;
}

std::string fmt_sparse(const std::vector<std::uint32_t>& v) {
    std::string s;
    for (std::size_t g = 0; g < v.size(); ++g) {
        if (v[g] != 0) {
            s += (s.empty() ? "" : ",") + std::to_string(g) + ":" + std::to_string(v[g]);
        }
    }
    return s.empty() ? "none" : s;
}

std::string fmt_cargo(const std::vector<sim::ResidentCargo>& c) {
    std::string s;
    for (std::size_t i = 0; i < c.size(); ++i) {
        s += (i ? "," : "") + std::to_string(c[i].goods) + ":" + std::to_string(c[i].amount_88);
    }
    return s.empty() ? "none" : s;
}

std::vector<std::uint32_t> sparse(const std::string& s, std::size_t count) {
    std::vector<std::uint32_t> v(count, 0);
    for (const auto& [g, a] : pairs(s)) {
        REQUIRE(static_cast<std::size_t>(g) < count);
        v[static_cast<std::size_t>(g)] = static_cast<std::uint32_t>(a);
    }
    return v;
}

std::vector<sim::ResidentCargo> cargo_of(const std::string& s) {
    std::vector<sim::ResidentCargo> v;
    for (const auto& [g, a] : pairs(s)) {
        v.push_back({static_cast<sim::GoodsId>(g), static_cast<std::uint32_t>(a)});
    }
    return v;
}

struct Goods {
    std::vector<sim::GoodsInfo> table;
};
struct House {
    std::vector<sim::HouseNeed> needs;
    std::map<std::int64_t, std::int64_t> lookup;
};

bool flag_list(const std::string& s, int k) {
    return ints(s)[static_cast<std::size_t>(k)] != 0;
}

sim::RuntimeClass class_of(const std::string& name) {
    static const char* const names[] = {
        "Plain",       "Workshop",    "DualWorkshop",  "WeaponsWorkshop", "Farm",
        "FurTrapper",  "Mine",        "SmallFarm",     "Quarry",          "Hunter",
        "Whaler",      "Shipyard",    "Cannonworks",   "Market",          "MarketCombi",
        "MarketStall", "Inn",         "PublicService", "Doctor",          "School",
        "Castle",      "FireBrigade", "House",         "Tower",           "CannonTower"};
    for (std::size_t i = 0; i < std::size(names); ++i) {
        if (name == names[i]) {
            return static_cast<sim::RuntimeClass>(i);
        }
    }
    FAIL("unknown class " << name);
    return sim::RuntimeClass::Plain;
}

/// The dynamic House view of a vector (the lookup vector has one entry per goods id).
struct HouseView {
    std::vector<std::uint32_t> lookup;
    sim::HouseFacts facts;
};

HouseView make_view(const Goods& g, const House& h, std::uint32_t e, std::int32_t res,
                    const std::vector<std::uint32_t>& stock) {
    HouseView v;
    v.lookup.assign(g.table.size(), static_cast<std::uint32_t>(h.needs.size()));
    for (const auto& [goods, slot] : h.lookup) {
        v.lookup[static_cast<std::size_t>(goods)] = static_cast<std::uint32_t>(slot);
    }
    v.facts.needs = h.needs;
    v.facts.goods = g.table;
    v.facts.e = e;
    v.facts.residents_64 = res;
    v.facts.stock = stock;
    return v;
}

Kv run_vector(const std::string& kind, const Kv& kv, const std::map<std::string, Goods>& goods,
              const std::map<std::string, House>& houses) {
    Kv out;
    auto num = [&](const char* k) { return std::stoll(kv.at(k), nullptr, 0); };
    if (kind == "demand") {
        out["demand"] = std::to_string(sim::house_demand(static_cast<std::uint32_t>(num("menge")),
                                                         static_cast<std::uint32_t>(num("e")),
                                                         static_cast<std::uint32_t>(num("stock"))));
    } else if (kind == "amt") {
        out["amount"] = std::to_string(std::min({num("avail"), num("demand"), num("cap")}));
    } else if (kind == "choice") {
        const Goods& g = goods.at(kv.at("goods"));
        const House& h = houses.at(kv.at("house"));
        std::vector<std::uint32_t> stock;
        for (const auto x : ints(kv.at("stock"))) {
            stock.push_back(static_cast<std::uint32_t>(x));
        }
        HouseView hv = make_view(g, h, static_cast<std::uint32_t>(num("e")),
                                 static_cast<std::int32_t>(num("res")), stock);
        hv.facts.lookup = hv.lookup;
        sim::SupplierOffer offer;
        const auto o = ints(kv.at("out"));
        const auto a = ints(kv.at("avail"));
        const std::string en = kv.count("enabled") ? kv.at("enabled") : "1,1";
        for (int k = 0; k < 2; ++k) {
            offer.goods[k] = static_cast<sim::GoodsId>(o[static_cast<std::size_t>(k)]);
            offer.avail[k] = static_cast<std::uint64_t>(a[static_cast<std::size_t>(k)]);
            offer.enabled[k] = flag_list(en, k);
        }
        const auto carried = cargo_of(kv.count("carried") ? kv.at("carried") : "none");
        const sim::ChoiceResult r = sim::house_choice(hv.facts, offer, carried);
        if (r.fault != sim::ResidentFault::None) {
            out["status"] = "fault";
            out["fault"] = sim::resident_fault_name(r.fault);
        } else {
            out["status"] = r.score >= 0 ? "chosen" : "none";
        }
        out["score"] = std::to_string(r.score);
        out["goods"] = std::to_string(r.goods);
    } else if (kind == "avail") {
        sim::SupplierModel m;
        m.cls = class_of(kv.at("cls"));
        const auto o = ints(kv.count("out") ? kv.at("out") : "0,0");
        m.out[0] = static_cast<sim::GoodsId>(o[0]);
        m.out[1] = static_cast<sim::GoodsId>(o[1]);
        const std::string fl = kv.count("flags") ? kv.at("flags") : "1,1";
        m.flag[0] = flag_list(fl, 0);
        m.flag[1] = flag_list(fl, 1);
        m.tavern_stock = kv.count("tavern") ? static_cast<std::uint32_t>(num("tavern")) : 0;
        const std::size_t count = kv.count("count") ? static_cast<std::size_t>(num("count")) : 96;
        auto city = sparse(kv.count("city") ? kv.at("city") : "none", count);
        const auto gid = static_cast<sim::GoodsId>(num("goods"));
        const sim::AvailResult r = sim::supplier_available(m, city, gid);
        if (r.fault != sim::ResidentFault::None) {
            out["fault"] = sim::resident_fault_name(r.fault);
        } else {
            out["avail"] = std::to_string(r.amount);
            out["enabled"] = sim::supplier_enabled(m, gid) ? "1" : "0";
        }
    } else if (kind == "bill") {
        const auto rng = ints(kv.count("range") ? kv.at("range") : "26,40");
        const std::size_t count = kv.count("count") ? static_cast<std::size_t>(num("count")) : 96;
        auto store = sparse(kv.at("store"), count);
        auto billed = sparse(kv.at("billed"), count);
        sim::BillLedger l{store, billed, static_cast<sim::GoodsId>(rng[0]),
                          static_cast<sim::GoodsId>(rng[1])};
        const sim::BillResult r =
            sim::bill(l, static_cast<sim::GoodsId>(num("goods")),
                      static_cast<std::uint32_t>(num("amount")), num("take") != 0);
        out["status"] = r.fault == sim::ResidentFault::None
                            ? "ok"
                            : (r.fault == sim::ResidentFault::GoodsOutOfTable
                                   ? "out_of_table"
                                   : sim::resident_fault_name(r.fault));
        out["returned"] = std::to_string(r.returned);
        out["store"] = fmt_sparse(store);
        out["billed"] = fmt_sparse(billed);
    } else if (kind == "sbill") {
        const auto rng = ints(kv.count("range") ? kv.at("range") : "26,40");
        const std::size_t count = kv.count("count") ? static_cast<std::size_t>(num("count")) : 96;
        auto store = sparse(kv.at("store"), count);
        auto billed = sparse(kv.at("billed"), count);
        sim::BillLedger l{store, billed, static_cast<sim::GoodsId>(rng[0]),
                          static_cast<sim::GoodsId>(rng[1])};
        sim::SupplierModel m;
        m.cls = class_of(kv.at("cls"));
        m.tavern_stock = kv.count("tavern") ? static_cast<std::uint32_t>(num("tavern")) : 0;
        m.inn_input = kv.count("input") ? static_cast<sim::GoodsId>(num("input")) : 0;
        const sim::BillResult r = sim::supplier_bill(m, l, static_cast<sim::GoodsId>(num("goods")),
                                                     static_cast<std::uint32_t>(num("amount")));
        out["status"] = r.fault == sim::ResidentFault::None
                            ? "ok"
                            : (r.fault == sim::ResidentFault::GoodsOutOfTable
                                   ? "out_of_table"
                                   : sim::resident_fault_name(r.fault));
        out["returned"] = std::to_string(r.returned);
        out["store"] = fmt_sparse(store);
        out["billed"] = fmt_sparse(billed);
        out["tavern"] = std::to_string(m.tavern_stock);
    } else if (kind == "deliver") {
        const Goods& g = goods.at(kv.at("goods"));
        const House& h = houses.at(kv.at("house"));
        std::vector<std::uint32_t> stock;
        for (const auto x : ints(kv.at("stock"))) {
            stock.push_back(static_cast<std::uint32_t>(x));
        }
        HouseView hv = make_view(g, h, 0, 0, stock);
        hv.facts.lookup = hv.lookup;
        const sim::DeliverResult r =
            sim::house_deliver(hv.facts, stock, static_cast<sim::GoodsId>(num("goods_id")),
                               static_cast<std::uint32_t>(num("amount")));
        out["status"] = r.fault == sim::ResidentFault::None ? "ok" : "refused";
        out["stock"] = fmt_list(stock);
        out["touched"] = std::to_string(r.touched);
    } else if (kind == "buy") {
        const Goods& g = goods.at(kv.at("goods"));
        const House& h = houses.at(kv.at("house"));
        std::vector<std::uint32_t> stock;
        for (const auto x : ints(kv.at("stock"))) {
            stock.push_back(static_cast<std::uint32_t>(x));
        }
        HouseView hv = make_view(g, h, static_cast<std::uint32_t>(num("e")),
                                 static_cast<std::int32_t>(num("res")), stock);
        hv.facts.lookup = hv.lookup;
        sim::SupplierModel m;
        m.cls = class_of(kv.at("cls"));
        const auto o = ints(kv.at("out"));
        m.out[0] = static_cast<sim::GoodsId>(o[0]);
        m.out[1] = static_cast<sim::GoodsId>(o[1]);
        const std::string fl = kv.count("flags") ? kv.at("flags") : "1,1";
        m.flag[0] = flag_list(fl, 0);
        m.flag[1] = flag_list(fl, 1);
        m.tavern_stock = kv.count("tavern") ? static_cast<std::uint32_t>(num("tavern")) : 0;
        m.inn_input = kv.count("input") ? static_cast<sim::GoodsId>(num("input")) : 0;
        const auto rng = ints(kv.count("range") ? kv.at("range") : "26,40");
        const std::size_t count = kv.count("count") ? static_cast<std::size_t>(num("count")) : 96;
        auto city = sparse(kv.count("city") ? kv.at("city") : "none", count);
        auto billed = sparse(kv.count("billed") ? kv.at("billed") : "none", count);
        sim::BillLedger l{city, billed, static_cast<sim::GoodsId>(rng[0]),
                          static_cast<sim::GoodsId>(rng[1])};
        auto cargo = cargo_of(kv.at("cargo"));
        const auto others = cargo_of(kv.count("others") ? kv.at("others") : "none");
        const sim::BuyResult r = sim::resident_buy(
            hv.facts, m, l, static_cast<std::uint32_t>(num("cap")), others, cargo);
        out["status"] = r.fault == sim::ResidentFault::None ? "ok" : "fault";
        if (r.fault != sim::ResidentFault::None) {
            out["fault"] = sim::resident_fault_name(r.fault);
        }
        out["cargo"] = fmt_cargo(cargo);
        out["city"] = fmt_sparse(city);
        out["billed"] = fmt_sparse(billed);
        out["purchases"] = std::to_string(r.purchases);
        out["tavern"] = std::to_string(m.tavern_stock);
    } else {
        FAIL("unknown record " << kind);
    }
    return out;
}

/// Runs the whole file; returns {vectors, disagreements}. Messages go to doctest.
std::pair<int, int> run_file(const std::string& path) {
    std::ifstream in(path);
    REQUIRE_MESSAGE(in.good(), "cannot read " << path);
    std::map<std::string, Goods> goods;
    std::map<std::string, House> houses;
    int nvec = 0, bad = 0, lineno = 0;
    std::string raw;
    while (std::getline(in, raw)) {
        ++lineno;
        const auto hash = raw.find('#');
        if (hash != std::string::npos) {
            raw.resize(hash);
        }
        std::istringstream ls(raw);
        std::vector<std::string> t;
        for (std::string w; ls >> w;) {
            t.push_back(w);
        }
        if (t.empty()) {
            continue;
        }
        if (t[0] == "goods") {
            const Kv k = parse_kv(t, 2, t.size());
            Goods g;
            g.table.resize(static_cast<std::size_t>(std::stoll(k.at("count"))));
            if (k.count("sec")) {
                for (const auto& [a, b] : pairs(k.at("sec"))) {
                    g.table.at(static_cast<std::size_t>(a)).id_sec = static_cast<sim::GoodsId>(b);
                }
            }
            goods[t[1]] = std::move(g);
        } else if (t[0] == "house") {
            const Kv k = parse_kv(t, 2, t.size());
            House h;
            for (const auto& [w, m] : pairs(k.at("needs"))) {
                sim::HouseNeed n;
                n.ware = static_cast<sim::GoodsId>(w);
                n.menge = static_cast<std::uint16_t>(m);
                h.needs.push_back(n);
            }
            if (k.count("lookup")) {
                for (const auto& [a, b] : pairs(k.at("lookup"))) {
                    h.lookup[a] = b;
                }
            }
            houses[t[1]] = std::move(h);
        } else {
            const auto arrow = std::find(t.begin(), t.end(), "=>");
            REQUIRE_MESSAGE(arrow != t.end(), path << ":" << lineno << " no =>");
            const auto k = static_cast<std::size_t>(arrow - t.begin());
            const Kv in_kv = parse_kv(t, 2, k);
            const Kv exp = parse_kv(t, k + 1, t.size());
            const Kv got = run_vector(t[0], in_kv, goods, houses);
            ++nvec;
            for (const auto& [key, want] : exp) {
                const auto it = got.find(key);
                const std::string have = it == got.end() ? "<absent>" : it->second;
                if (have != want) {
                    ++bad;
                    MESSAGE(path << ":" << lineno << " " << t[1] << ": " << key << " expected "
                                 << want << ", computed " << have);
                }
            }
        }
    }
    return {nvec, bad};
}

} // namespace

TEST_CASE("resident vectors: the C++ module agrees with the hand-derived file") {
    const auto [nvec, bad] = run_file(std::string(ANNO_FIXTURES_DIR) + "/sim/resident_vectors.txt");
    CHECK(nvec >= 100);
    CHECK(bad == 0);
}
