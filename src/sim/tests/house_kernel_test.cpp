// MAP-POP-IMPL-1 D: the pure House kernel against fixtures/synthetic/sim/house_kernel_vectors.txt.
// The expectations in that file were derived by hand from the instruction spans before this kernel
// existed (docs/sim/population-contract.md P-D1..P-D13); tools/house_kernel_ref.py checks the same
// file independently. Not a live tick: the driver is not involved (see the last case).
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/house_kernel.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>

namespace {

namespace sim = anno::sim;

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(s);
    while (std::getline(is, cur, sep)) {
        out.push_back(cur);
    }
    if (!s.empty() && s.back() == sep) {
        out.emplace_back();
    }
    return out;
}

std::vector<std::int64_t> ints(const std::string& s) {
    std::vector<std::int64_t> v;
    for (const std::string& t : split(s, ',')) {
        if (!t.empty()) {
            v.push_back(std::stoll(t, nullptr, 0));
        }
    }
    return v;
}

std::map<std::string, std::string> kv(const std::vector<std::string>& tokens, std::size_t from,
                                      std::size_t to) {
    std::map<std::string, std::string> m;
    for (std::size_t i = from; i < to; ++i) {
        const auto eq = tokens[i].find('=');
        REQUIRE(eq != std::string::npos);
        m[tokens[i].substr(0, eq)] = tokens[i].substr(eq + 1);
    }
    return m;
}

struct VectorGroup {
    sim::HouseGroup group;
    std::map<sim::GoodsId, std::uint32_t> bau;
    sim::PopulationRoles roles{76, 90, 76, 29};
    std::shared_ptr<const sim::TypeRegistry> registry;
    sim::SimConfig cfg;
    std::optional<sim::PopulationView> view;
};

std::string absent() {
    return "<absent>";
}

template <class T> std::string num(const std::optional<T>& v) {
    return v.has_value() ? std::to_string(static_cast<std::int64_t>(*v)) : absent();
}

std::string reason_name(sim::NotEvaluatedReason r) {
    using R = sim::NotEvaluatedReason;
    switch (r) {
    case R::None:
        return "none";
    case R::GatePeriodOutOfDomain:
        return "gate_period_out_of_domain";
    case R::MissingGroup:
        return "missing_group";
    case R::StockSizeMismatch:
        return "stock_size_mismatch";
    case R::StageIndexOutOfDomain:
        return "stage_index_out_of_domain";
    case R::NegativeSmoothedE:
        return "negative_smoothed_e";
    case R::ZeroDivisor:
        return "zero_divisor";
    case R::MissingFirstNeed:
        return "missing_first_need";
    case R::MoodOutOfRange:
        return "mood_out_of_range";
    case R::Overflow:
        return "overflow";
    case R::PreviousCapacityUnavailable:
        return "previous_capacity_unavailable";
    }
    return "?";
}

/// The result as the reference prints it (key -> text).
std::map<std::string, std::string> render(const sim::HouseKernelResult& r) {
    std::map<std::string, std::string> m;
    m["status"] = r.status == sim::HouseKernelStatus::Skipped     ? "skipped"
                  : r.status == sim::HouseKernelStatus::Evaluated ? "evaluated"
                                                                  : "not_evaluated";
    m["reason"] = r.reason == sim::NotEvaluatedReason::None ? absent() : reason_name(r.reason);
    m["last"] = num(r.last);
    m["e"] = num(r.e);
    if (r.lowest.has_value()) {
        std::string s;
        for (std::size_t i = 0; i < r.stock.size(); ++i) {
            s += (i ? "," : "") + std::to_string(r.stock[i]);
        }
        m["stock"] = s;
    } else {
        m["stock"] = absent();
    }
    m["lowest"] = num(r.lowest);
    m["spawn"] = r.spawn_condition.has_value() ? (*r.spawn_condition ? "1" : "0") : absent();
    m["mood"] = num(r.mood);
    m["res"] = num(r.residents_64);
    m["csub"] = num(r.counter_sub);
    m["cadd"] = num(r.counter_add);
    m["capsub"] = num(r.cap_sub);
    m["b64"] = num(r.below64_add);
    m["cap"] = r.cap_crossed.has_value() ? (*r.cap_crossed ? "1" : "0") : absent();
    if (r.counter_add.has_value()) {
        m["arm"] = r.arm == sim::HouseArm::None        ? "none"
                   : r.arm == sim::HouseArm::Downgrade ? "downgrade"
                                                       : "remove";
        m["effects"] = r.effect == sim::DeferredEffect::None           ? "none"
                       : r.effect == sim::DeferredEffect::UpgradeCheck ? "upgrade_check"
                       : r.effect == sim::DeferredEffect::Downgrade    ? "downgrade"
                                                                       : "remove";
    } else {
        m["arm"] = absent();
        m["effects"] = absent();
    }
    return m;
}

} // namespace

TEST_CASE("house kernel: every hand-derived vector") {
    std::ifstream in(std::string(ANNO_FIXTURES_DIR) + "/sim/house_kernel_vectors.txt");
    REQUIRE(in.good());
    std::map<std::string, std::vector<std::int8_t>> tables;
    std::map<std::string, VectorGroup> groups;
    std::size_t vectors = 0, evaluated = 0, skipped = 0, refused = 0;
    std::string raw;
    while (std::getline(in, raw)) {
        const std::string line = raw.substr(0, raw.find('#'));
        std::istringstream ls(line);
        std::vector<std::string> t;
        for (std::string w; ls >> w;) {
            t.push_back(w);
        }
        if (t.empty()) {
            continue;
        }
        if (t[0] == "table") {
            std::vector<std::int8_t> v;
            for (const auto x : ints(t[2])) {
                v.push_back(static_cast<std::int8_t>(x));
            }
            REQUIRE(v.size() == 21);
            tables[t[1]] = v;
        } else if (t[0] == "group") {
            const auto m = kv(t, 2, t.size());
            VectorGroup g;
            g.group.number = 0;
            g.group.wohnmax = static_cast<std::uint16_t>(std::stoul(m.at("wohnmax")));
            g.group.no_downgrade = m.at("nodown") == "1";
            const auto vm = ints(m.at("vermehr"));
            REQUIRE(vm.size() == 5);
            for (std::size_t i = 0; i < 5; ++i) {
                g.group.vermehr[i] = static_cast<std::int8_t>(vm[i]);
            }
            const auto copy = [&](const std::string& name, std::array<std::int8_t, 21>& to) {
                const auto& v = tables.at(m.at(name));
                std::copy(v.begin(), v.end(), to.begin());
            };
            copy("nahrung", g.group.nahrung);
            copy("bedarf", g.group.bedarf);
            copy("infra", g.group.infra);
            for (const std::string& need : split(m.at("needs"), ',')) {
                if (need.empty()) {
                    continue;
                }
                const auto f = split(need, ':');
                g.group.needs.push_back(
                    sim::HouseNeed{static_cast<sim::GoodsId>(std::stoul(f[0])), f[1] == "1",
                                   static_cast<std::uint16_t>(std::stoul(f[2]))});
            }
            if (m.contains("bau")) {
                for (const std::string& b : split(m.at("bau"), ',')) {
                    const auto f = split(b, ':');
                    g.bau[static_cast<sim::GoodsId>(std::stoul(f[0]))] =
                        static_cast<std::uint32_t>(std::stoul(f[1]));
                }
            }
            if (m.contains("roles")) {
                const auto r = ints(m.at("roles"));
                g.roles = sim::PopulationRoles{
                    static_cast<sim::GoodsId>(r[0]), static_cast<sim::GoodsId>(r[1]),
                    static_cast<sim::GoodsId>(r[2]), static_cast<sim::GoodsId>(r[3])};
            }
            std::vector<sim::GoodsInfo> goods = sim::original_goods();
            for (const auto& [ware, stage] : g.bau) {
                goods[ware].bau_first = stage;
            }
            g.cfg.house_groups = 1;
            g.cfg.population_infra_first = g.roles.infra_first;
            g.cfg.population_infra_last = g.roles.infra_last;
            g.cfg.population_tavern_goods = g.roles.tavern_goods;
            g.cfg.population_tavern_alias_goods = g.roles.tavern_alias_goods;
            sim::TypeRegistry::Builder b;
            b.goods(goods).house_groups({g.group}).config(g.cfg);
            auto built = b.build();
            REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
            g.registry = built.value();
            auto view = g.registry->population_view(0, g.cfg);
            REQUIRE(view.ok());
            g.view = view.value();
            groups[t[1]] = std::move(g);
        } else if (t[0] == "mood") {
            const auto arrow =
                static_cast<std::size_t>(std::find(t.begin(), t.end(), "=>") - t.begin());
            const auto in_kv = kv(t, 2, arrow);
            const auto exp = kv(t, arrow + 1, t.size());
            const int got = sim::house_mood_combine(
                std::stoi(in_kv.at("f")), std::stoi(in_kv.at("b")), std::stoi(in_kv.at("i")),
                static_cast<std::uint8_t>(std::stoi(in_kv.at("flags"))));
            INFO(t[1]);
            CHECK(std::to_string(got) == exp.at("mood"));
            ++vectors;
        } else if (t[0] == "vec") {
            const auto arrow =
                static_cast<std::size_t>(std::find(t.begin(), t.end(), "=>") - t.begin());
            const auto m = kv(t, 2, arrow);
            const auto exp = kv(t, arrow + 1, t.size());
            VectorGroup& g = groups.at(m.at("group"));
            std::vector<std::uint32_t> stock;
            for (const auto x : ints(m.at("stock"))) {
                stock.push_back(static_cast<std::uint32_t>(x));
            }
            sim::HouseKernelInputs inp;
            inp.view = &*g.view;
            inp.now = static_cast<std::uint32_t>(std::stoull(m.at("now")));
            inp.last = static_cast<std::uint16_t>(std::stoul(m.at("last")));
            inp.gate_period = static_cast<std::uint32_t>(std::stoull(m.at("period")));
            inp.player_mask = static_cast<std::uint32_t>(std::stoull(m.at("mask"), nullptr, 0));
            inp.residents_64 = static_cast<std::int32_t>(std::stoll(m.at("res")));
            inp.e = static_cast<std::uint32_t>(std::stoull(m.at("e")));
            inp.stock = stock;
            if (m.contains("flags")) {
                inp.mood_flags = static_cast<std::uint8_t>(std::stoul(m.at("flags"), nullptr, 0));
            }
            if (m.contains("prev")) {
                inp.previous_wohnmax = static_cast<std::uint32_t>(std::stoul(m.at("prev")));
            }
            if (m.contains("walkers")) {
                const auto w = split(m.at("walkers"), '/');
                inp.walkers = sim::WalkerFacts{static_cast<std::uint32_t>(std::stoul(w[0])),
                                               static_cast<std::uint32_t>(std::stoul(w[1]))};
            }
            const sim::HouseKernelResult r = sim::house_kernel(inp);
            const auto got = render(r);
            for (const auto& [key, want] : exp) {
                INFO(t[1]);
                INFO(key);
                CHECK(got.at(key) == want);
            }
            ++vectors;
            evaluated += r.status == sim::HouseKernelStatus::Evaluated ? 1U : 0U;
            skipped += r.status == sim::HouseKernelStatus::Skipped ? 1U : 0U;
            refused += r.status == sim::HouseKernelStatus::NotEvaluated ? 1U : 0U;
        }
    }
    // The file really exercised every status (a function that returned one status for all fails).
    CHECK(vectors >= 60);
    CHECK(evaluated >= 30);
    CHECK(skipped >= 5);
    CHECK(refused >= 8);
}

TEST_CASE("house kernel: a partition of calls and a saved partial last") {
    // A house 5000 behind needs two calls: the first leaves last = 2500, the second 5000, the third
    // skips. The only partial state is `last`; it is a u16 that a save keeps (House::last, stream
    // 127).
    const auto types = sim::synthetic::make_synthetic_types();
    REQUIRE(types.ok());
    sim::HouseGroup g;
    g.wohnmax = 8;
    g.needs = {sim::HouseNeed{26, false, 16}};
    sim::SimConfig cfg;
    cfg.house_groups = 1;
    sim::TypeRegistry::Builder b;
    b.goods(sim::original_goods()).house_groups({g}).config(cfg);
    auto built = b.build();
    REQUIRE(built.ok());
    auto view = built.value()->population_view(0, cfg);
    REQUIRE(view.ok());
    std::uint16_t last = 0;
    std::vector<std::uint32_t> stock{0};
    int taken = 0;
    for (int call = 0; call < 3; ++call) {
        sim::HouseKernelInputs in;
        in.view = &view.value();
        in.now = 5000;
        in.last = last;
        in.gate_period = 2500;
        in.player_mask = 1;
        in.residents_64 = 64;
        in.e = 128;
        in.stock = stock;
        in.previous_wohnmax = 8;
        const auto r = sim::house_kernel(in);
        taken += r.status == sim::HouseKernelStatus::Evaluated ? 1 : 0;
        REQUIRE(r.last.has_value());
        last = *r.last;
    }
    CHECK(taken == 2);
    CHECK(last == 5000);
    // The kernel is not wired into the tick: an empty-kernel invocation changes nothing in a world.
    // (the House tick dispatch stays `return;`, driver.cpp; checked by population_canary_test.)
}
