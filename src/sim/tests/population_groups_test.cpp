// MAP-POP-IMPL-1 C: the derived population group tables (docs/sim/population-inputs.md
// 2.6-2.10, section 3 and 5 rows C1-C12; docs/sim/population-contract.md P-C1..P-C6).
// Shipped expectations: fixtures/synthetic/sim/group_derived.txt from tools/group_derived_ref.py (a
// second implementation over the raw BGruppen.dat). The synthetic rows are hand-derived below.
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/house_groups.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

namespace {

namespace sim = anno::sim;

// Goods ids of the original table (docs/sim/state.md Appendix A).
constexpr sim::GoodsId NAHRUNG = 26, LEDER = 27, SALZ = 28, ALKOHOL = 29, WIRTSHAUS = 76,
                       HOCHSCHULE = 81, KAPELLE = 82, KIRCHE = 83, KATHETRALE = 84;

sim::HouseGroup group_of(std::vector<std::pair<sim::GoodsId, bool>> needs) {
    sim::HouseGroup g;
    g.wohnmax = 8;
    for (const auto& [ware, force] : needs) {
        g.needs.push_back(sim::HouseNeed{ware, force, 41});
    }
    return g;
}

/// The original goods table with the shipped IdSec chain (KLEIDUNG excluded: not in a row below).
std::vector<sim::GoodsInfo> goods_with_chain() {
    std::vector<sim::GoodsInfo> g = sim::original_goods();
    g[KIRCHE].id_sec = KAPELLE;
    g[KATHETRALE].id_sec = KIRCHE;
    return g;
}

struct Row {
    std::string error;
    sim::HouseGroupDerived d;
};

Row derive(const sim::HouseGroup& g, const std::vector<sim::GoodsInfo>& goods,
           const sim::PopulationRoles& roles = {}) {
    Row r;
    r.error = sim::derive_group(g, goods, roles, r.d);
    return r;
}

std::uint32_t n_of(const Row& r) {
    return r.d.need_count;
}

} // namespace

TEST_CASE("population C: hand-derived rows C2-C12") {
    const auto goods = goods_with_chain();
    {
        // C2: duplicate wares, the last slot wins, n counts both.
        const Row r = derive(group_of({{NAHRUNG, false}, {SALZ, false}, {NAHRUNG, false}}), goods);
        REQUIRE(r.error == "");
        CHECK(n_of(r) == 3);
        CHECK(r.d.lookup.size() == goods.size()); // one entry per goods id, not per need
        CHECK(r.d.lookup[NAHRUNG] == 2);
        CHECK(r.d.lookup[SALZ] == 1);
        CHECK(r.d.lookup[LEDER] == 3); // the sentinel is n
        CHECK(r.d.goods_end == 3);
        CHECK(r.d.infra_end == 3);
    }
    {
        // C3: goods_end is the n of the last consumer-class need; the tavern is slot 0.
        const Row r = derive(group_of({{WIRTSHAUS, false}, {NAHRUNG, false}}), goods);
        REQUIRE(r.error == "");
        CHECK(n_of(r) == 2);
        CHECK(r.d.goods_end == 2);
        CHECK(r.d.infra_end == 2);
        CHECK(r.d.lookup[ALKOHOL] == 0);
        CHECK(r.d.lookup[WIRTSHAUS] == 0);
        CHECK(r.d.lookup[NAHRUNG] == 1);
    }
    {
        // C4: infra_end moves without Force (the case a Force-based rule fails).
        const Row r = derive(group_of({{NAHRUNG, true}, {WIRTSHAUS, false}}), goods);
        REQUIRE(r.error == "");
        CHECK(r.d.goods_end == 1);
        CHECK(r.d.infra_end == 2);
        for (const std::uint8_t f : r.d.forced) {
            CHECK(f == 0);
        }
        // and the forced tavern sets bit 0 of the set, not infra_end
        const Row f = derive(group_of({{NAHRUNG, false}, {WIRTSHAUS, true}}), goods);
        REQUIRE(f.error == "");
        CHECK(f.d.infra_end == 2);
        CHECK(f.d.forced[0] == 1);
    }
    {
        // C5: a later consumer need rewrites goods_end past a tavern; infra loop empty.
        const Row r = derive(group_of({{NAHRUNG, false}, {WIRTSHAUS, true}, {LEDER, true}}), goods);
        REQUIRE(r.error == "");
        CHECK(n_of(r) == 3);
        CHECK(r.d.goods_end == 3);
        CHECK(r.d.infra_end == 3);
        CHECK(r.d.forced[0] == 1);
        CHECK(r.d.forced[1] == 0);
    }
    {
        // C6: second tavern takes lookup[tavern]; the alias stays on the first.
        const Row r = derive(group_of({{WIRTSHAUS, false}, {WIRTSHAUS, false}}), goods);
        REQUIRE(r.error == "");
        CHECK(r.d.goods_end == 0);
        CHECK(r.d.infra_end == 2);
        CHECK(r.d.lookup[WIRTSHAUS] == 1);
        CHECK(r.d.lookup[ALKOHOL] == 0);
    }
    {
        // C7: a real alias-goods need blocks the alias.
        const Row r = derive(group_of({{ALKOHOL, false}, {WIRTSHAUS, false}}), goods);
        REQUIRE(r.error == "");
        CHECK(r.d.lookup[ALKOHOL] == 0);
        CHECK(r.d.lookup[WIRTSHAUS] == 1);
    }
    {
        // C8: one ascending pass: KIRCHE (83) before KATHETRALE (84), so 84 inherits slot 0.
        const Row r = derive(group_of({{KAPELLE, false}}), goods);
        REQUIRE(r.error == "");
        CHECK(r.d.lookup[KAPELLE] == 0);
        CHECK(r.d.lookup[KIRCHE] == 0);
        CHECK(r.d.lookup[KATHETRALE] == 0);
    }
    {
        // C9: a substitute with a higher id is not followed (no fixed point): HOCHSCHULE -> KIRCHE.
        auto g2 = goods;
        g2[HOCHSCHULE].id_sec = KIRCHE;
        const Row r = derive(group_of({{KAPELLE, false}}), g2);
        REQUIRE(r.error == "");
        CHECK(r.d.lookup[HOCHSCHULE] == 1); // still the sentinel n
        CHECK(r.d.lookup[KIRCHE] == 0);
        CHECK(r.d.lookup[KATHETRALE] == 0);
    }
    {
        // C10: goods 0, Menge 0 and a goods id outside the table are refused.
        CHECK(derive(group_of({{0, false}}), goods).error != "");
        sim::HouseGroup zero = group_of({{NAHRUNG, false}});
        zero.needs[0].menge = 0;
        CHECK(derive(zero, goods).error.find("Menge 0") != std::string::npos);
        CHECK(derive(group_of({{static_cast<sim::GoodsId>(goods.size()), false}}), goods).error !=
              "");
        CHECK(derive(group_of({{NAHRUNG, false}}), goods).error == ""); // the legal control
    }
    {
        // C11: an empty group still has a goods-sized lookup, all sentinel 0.
        const Row r = derive(group_of({}), goods);
        REQUIRE(r.error == "");
        CHECK(n_of(r) == 0);
        CHECK(r.d.goods_end == 0);
        CHECK(r.d.infra_end == 0);
        CHECK(r.d.lookup.size() == goods.size());
        for (const std::uint32_t s : r.d.lookup) {
            CHECK(s == 0);
        }
        CHECK(r.d.forced.size() == 0x5a - 0x4c + 1);
    }
    {
        // C12: the ware > infra_last arm: goods_end set only while it is still 0.
        const Row a = derive(group_of({{100, false}, {WIRTSHAUS, false}, {101, false}}), goods);
        REQUIRE(a.error == "");
        CHECK(a.d.goods_end == 1);
        CHECK(a.d.infra_end == 2);
        const Row b = derive(group_of({{LEDER, false}, {100, false}}), goods);
        REQUIRE(b.error == "");
        CHECK(b.d.goods_end == 1); // unchanged by the later ware above the range
    }
    {
        // Slot order is kept: a reordering changes the table (no hidden sort).
        const Row a = derive(group_of({{NAHRUNG, false}, {LEDER, false}}), goods);
        const Row b = derive(group_of({{LEDER, false}, {NAHRUNG, false}}), goods);
        CHECK(a.d.lookup[NAHRUNG] == 0);
        CHECK(b.d.lookup[NAHRUNG] == 1);
        // derivation is deterministic
        CHECK(derive(group_of({{NAHRUNG, false}, {LEDER, false}}), goods).d == a.d);
    }
}

TEST_CASE("population C: a registry larger than the original, with other role ids") {
    // 400 goods G0..G399, infrastructure range 100..140 (41 ids, more than 32), tavern 100, alias
    // 10; 301 needs (more than 255): 260 consumer needs with ware 1 + j % 99, then the 41
    // infrastructure goods once each, even ids forced. Closed forms (hand-derived):
    //  goods_end = 260, infra_end = 301, lookup[100 + i] = 260 + i,
    //  lookup[w] for a consumer ware w in 1..99: the last j < 260 with 1 + j % 99 == w, i.e.
    //  w - 1 + 198 for w <= 62 and w - 1 + 99 otherwise; the alias 10 is a real need so the tavern
    //  does not take it; forced[i] = 1 for even 100 + i.
    std::vector<sim::GoodsInfo> goods;
    for (int i = 0; i < 400; ++i) {
        goods.push_back(sim::GoodsInfo{i == 0 ? "UNUSED" : "G" + std::to_string(i), 0});
    }
    sim::PopulationRoles roles{100, 140, 100, 10};
    sim::HouseGroup g;
    g.wohnmax = 8;
    for (std::uint32_t j = 0; j < 260; ++j) {
        g.needs.push_back(sim::HouseNeed{1 + j % 99, false, 41});
    }
    for (std::uint32_t i = 0; i <= 40; ++i) {
        g.needs.push_back(sim::HouseNeed{100 + i, (100 + i) % 2 == 0, 41});
    }
    const Row r = derive(g, goods, roles);
    REQUIRE(r.error == "");
    CHECK(r.d.need_count == 301);
    CHECK(r.d.goods_end == 260);
    CHECK(r.d.infra_end == 301);
    CHECK(r.d.lookup.size() == 400);
    for (std::uint32_t i = 0; i <= 40; ++i) {
        CHECK(r.d.lookup[100 + i] == 260 + i);
        CHECK(r.d.forced[i] == ((100 + i) % 2 == 0 ? 1 : 0));
    }
    CHECK(r.d.forced.size() == 41);
    for (std::uint32_t w = 1; w <= 99; ++w) {
        CHECK(r.d.lookup[w] == (w <= 62 ? w - 1 + 198 : w - 1 + 99));
    }
    CHECK(r.d.lookup[10] == 9 + 198); // the real alias-goods need, not the tavern's slot 260
    CHECK(r.d.lookup[0] == 301);      // the sentinel is n
    CHECK(r.d.lookup[399] == 301);
    // The same group under the original roles (0x4c..0x5a fit this table) derives differently.
    const Row original_roles = derive(g, goods);
    REQUIRE(original_roles.error == "");
    CHECK_FALSE(original_roles.d == r.d);
}

TEST_CASE("population C: non-default tavern and alias roles are honoured") {
    // Roles: infrastructure 100..105, tavern 102, alias 10. Goods 10 is a plain id with no need.
    std::vector<sim::GoodsInfo> goods;
    for (int i = 0; i < 200; ++i) {
        goods.push_back(sim::GoodsInfo{i == 0 ? "UNUSED" : "G" + std::to_string(i), 0});
    }
    goods[104].id_sec = 101; // a non-original substitute at a non-original infrastructure id
    const sim::PopulationRoles roles{100, 105, 102, 10};
    // Two taverns: the first takes the alias, the second only its own slot.
    Row r = derive(group_of({{5, false}, {102, false}, {101, false}, {102, false}}), goods, roles);
    REQUIRE(r.error == "");
    CHECK(r.d.lookup[10] == 1);  // the first tavern's slot
    CHECK(r.d.lookup[102] == 3); // the last duplicate
    CHECK(r.d.lookup[104] == 2); // substituted from 101 (one ascending pass: 101 is already set)
    CHECK(r.d.goods_end == 1);
    CHECK(r.d.infra_end == 4);
    // The original tavern id 0x4c is just a plain goods id under these roles: no alias from it.
    Row plain = derive(group_of({{0x4c, false}}), goods, roles);
    REQUIRE(plain.error == "");
    CHECK(plain.d.lookup[10] == 1); // sentinel n
    CHECK(plain.d.goods_end == 1);
    // A real need on the alias goods blocks the alias.
    Row blocked = derive(group_of({{10, false}, {102, false}}), goods, roles);
    REQUIRE(blocked.error == "");
    CHECK(blocked.d.lookup[10] == 0);
    // Roles that break the contract are refused by the derivation too.
    CHECK(derive(group_of({{5, false}}), goods, sim::PopulationRoles{100, 105, 99, 10}).error !=
          "");
    CHECK(derive(group_of({{5, false}}), goods, sim::PopulationRoles{100, 105, 102, 101}).error !=
          "");
}

TEST_CASE("population C: shipped groups equal the independent oracle slot for slot") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::ifstream in(std::string(ANNO_FIXTURES_DIR) + "/sim/group_derived.txt");
    REQUIRE(in.good());
    const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    const auto derived = loaded.types->derived_groups();
    REQUIRE(derived.size() == 5);
    std::string line;
    std::size_t checked = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ss(line);
        std::string k, name, a;
        std::uint32_t n = 0, ge = 0, ie = 0;
        std::string forced, lookup;
        ss >> k >> name >> a >> n >> a >> ge >> a >> ie >> a >> forced >> a >> lookup;
        REQUIRE(checked < 5);
        const sim::HouseGroupDerived& d = derived[checked];
        INFO(name);
        CHECK(loaded.types->house_groups()[checked].number == checked);
        CHECK(d.need_count == n);
        CHECK(d.goods_end == ge);
        CHECK(d.infra_end == ie);
        // forced ids
        std::vector<std::uint32_t> want_forced;
        if (forced != "-") {
            std::stringstream fs(forced);
            for (std::string t; std::getline(fs, t, ',');) {
                want_forced.push_back(static_cast<std::uint32_t>(std::stoul(t)));
            }
        }
        std::vector<std::uint32_t> got_forced;
        for (std::size_t i = 0; i < d.forced.size(); ++i) {
            if (d.forced[i] != 0) {
                got_forced.push_back(static_cast<std::uint32_t>(0x4c + i));
            }
        }
        CHECK(got_forced == want_forced);
        // the lookup: every listed id has its slot, every other id is the sentinel n
        std::map<std::uint32_t, std::uint32_t> want_lookup;
        std::stringstream ls(lookup);
        for (std::string t; std::getline(ls, t, ',');) {
            const auto colon = t.find(':');
            want_lookup[static_cast<std::uint32_t>(std::stoul(t.substr(0, colon)))] =
                static_cast<std::uint32_t>(std::stoul(t.substr(colon + 1)));
        }
        REQUIRE(d.lookup.size() == loaded.types->goods_table().size());
        for (std::uint32_t id = 0; id < d.lookup.size(); ++id) {
            const auto it = want_lookup.find(id);
            CHECK(d.lookup[id] == (it == want_lookup.end() ? n : it->second));
        }
        ++checked;
    }
    CHECK(checked == 5);
}

TEST_CASE("population C: the derived tables are bound to the roles of their config") {
    const auto goods = goods_with_chain();
    sim::SimConfig cfg;
    cfg.house_groups = 2;
    sim::HouseGroup g0 = group_of({{NAHRUNG, false}, {WIRTSHAUS, false}});
    g0.wohnmax = 8;
    sim::HouseGroup g1 = group_of({{NAHRUNG, false}});
    g1.number = 1;
    g1.wohnmax = 12;
    sim::TypeRegistry::Builder b;
    b.goods(goods).house_groups({g0, g1}).config(cfg);
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    const auto reg = built.value();
    CHECK(reg->derived_groups().size() == 2);
    CHECK(reg->population_roles() == sim::TypeRegistry::roles_of(cfg));
    // The matching config: the view carries the group, its tables, group 0's missing predecessor.
    auto v0 = reg->population_view(0, cfg);
    REQUIRE(v0.ok());
    CHECK(v0.value().group == &reg->house_groups()[0]);
    CHECK(v0.value().derived == &reg->derived_groups()[0]);
    CHECK_FALSE(v0.value().previous_wohnmax.has_value());
    auto v1 = reg->population_view(1, cfg);
    REQUIRE(v1.ok());
    REQUIRE(v1.value().previous_wohnmax.has_value());
    CHECK(*v1.value().previous_wohnmax == 8);
    CHECK_FALSE(reg->population_view(2, cfg).ok());
    CHECK(reg->validate_against(cfg) == "");
    // Another set of roles: the stale table is refused by both entry points.
    sim::SimConfig other = cfg;
    other.population_infra_last = 0x5b;
    CHECK(other.validate() == "");
    CHECK_FALSE(reg->population_view(0, other).ok());
    CHECK(reg->validate_against(other).find("differ") != std::string::npos);
    sim::SimConfig alias = cfg;
    alias.population_tavern_alias_goods = 0x1e;
    CHECK_FALSE(reg->population_view(0, alias).ok());
    CHECK(reg->validate_against(alias) != "");
    // A registry built under the other roles derives different tables from the same groups.
    other.house_groups = 2;
    sim::TypeRegistry::Builder b2;
    b2.goods(goods).house_groups({g0, g1}).config(other);
    auto rebuilt = b2.build();
    REQUIRE_MESSAGE(rebuilt.ok(), (rebuilt.ok() ? std::string() : rebuilt.error()));
    CHECK(rebuilt.value()->population_view(0, other).ok());
    CHECK(rebuilt.value()->validate_against(other) == "");
    CHECK(rebuilt.value()->validate_against(cfg) != "");
    // The derived data is not hashed: the registry digest is a function of the hashed inputs only.
    CHECK(rebuilt.value()->digest() == reg->digest());
    // A registry without groups accepts any config (no binding to enforce).
    sim::TypeRegistry::Builder none;
    none.goods({sim::GoodsInfo{"UNUSED", 0}, sim::GoodsInfo{"A", 0}});
    auto empty = none.build();
    REQUIRE(empty.ok());
    CHECK(empty.value()->validate_against(other) == "");
    CHECK_FALSE(empty.value()->population_view(0, cfg).ok());
}
