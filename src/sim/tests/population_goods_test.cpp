// MAP-POP-IMPL-1 B: the [WAREN] BauFirst / IdSec metadata, the population goods roles and their
// identity (docs/sim/population-contract.md P-B1..P-B5, docs/sim/population-inputs.md B1-B6).
// Shipped expectations come from fixtures/synthetic/sim/waren_metadata.txt, written by
// tools/waren_ref.py from the raw BGruppen.dat and the stage-name pointers of AnnoGame.dll (no
// C++).
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/bytes.hpp>
#include <anno/sim/goods_metadata.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/house_groups.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

namespace {

namespace sim = anno::sim;

std::vector<std::uint8_t> bytes_of(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

/// A three-goods table that is not the original: UNUSED, ALPHA, BETA.
std::vector<sim::GoodsInfo> tiny_goods() {
    return {sim::GoodsInfo{"UNUSED", 0}, sim::GoodsInfo{"ALPHA", 0}, sim::GoodsInfo{"BETA", 0}};
}

std::string read_waren(const std::string& text, std::vector<sim::GoodsInfo>& goods) {
    const std::vector<std::uint8_t> b = bytes_of(text);
    return sim::apply_goods_metadata(b, goods);
}

} // namespace

TEST_CASE("population B: BauFirst and IdSec of a hand-written WAREN section") {
    // Legal control first: a section with both keys on a non-original goods table.
    std::vector<sim::GoodsInfo> g = tiny_goods();
    CHECK(
        read_waren("[WAREN]\r\n  Id: ALPHA\r\n  Preis: 4\r\n  BauFirst: INFRA_STUFE_2B\r\n"
                   "  IdSec: BETA\r\n\r\n  Id: BETA\r\n  BauFirst: INFRA_STUFE_FORSCH\r\n[END]\r\n",
                   g) == "");
    CHECK(g[1].bau_first == 5); // INFRA_STUFE_2B
    CHECK(g[1].id_sec == 2);
    CHECK(g[2].bau_first == 31); // the last accepted stage
    CHECK(g[2].id_sec == 0);
    CHECK(g[0].bau_first == 0);

    // B2: absent and explicit INFRA_EVER are both 0 (indistinguishable at run time).
    g = tiny_goods();
    CHECK(read_waren(
              "[WAREN]\r\n  Id: ALPHA\r\n  Preis: 4\r\n  Id: BETA\r\n  BauFirst: INFRA_EVER\r\n",
              g) == "");
    CHECK(g[1].bau_first == 0);
    CHECK(g[2].bau_first == 0);
    // No [WAREN] section at all changes nothing.
    g = tiny_goods();
    CHECK(read_waren("[BGRUPPE]\r\n  BGruppeNr: PIONIER\r\n", g) == "");
    CHECK(g[1].bau_first == 0);

    // B3/B4: the declared domain. Each refusal names the line.
    const std::pair<const char*, const char*> bad[] = {
        {"  Id: ALPHA\r\n  BauFirst: INFRA_STUFE_9Z\r\n", "line 3"},
        {"  Id: ALPHA\r\n  BauFirst: GOTT\r\n", "GOTT"},
        {"  Id: ALPHA\r\n  BauFirst: SPIELER\r\n", "SPIELER"},
        {"  Id: ALPHA\r\n  BauFirst: infra_ever\r\n", "infra_ever"},
        {"  Id: NOSUCH\r\n  BauFirst: INFRA_EVER\r\n", "NOSUCH"},
        {"  Id: ALPHA\r\n  IdSec: NOSUCH\r\n", "NOSUCH"}};
    for (const auto& [body, want] : bad) {
        std::vector<sim::GoodsInfo> h = tiny_goods();
        const std::string err = read_waren(std::string("[WAREN]\r\n") + body, h);
        CAPTURE(body);
        CHECK(err != "");
        CHECK(err.find(want) != std::string::npos);
    }
}

TEST_CASE("population B: the stage-name mapping covers exactly 0..31") {
    CHECK(sim::stage_index_of("INFRA_EVER") == 0U);
    CHECK(sim::stage_index_of("INFRA_STUFE_1A") == 1U);
    CHECK(sim::stage_index_of("INFRA_STUFE_5F") == 28U);
    CHECK(sim::stage_index_of("INFRA_STUFE_TRIUMPH") == 29U);
    CHECK(sim::stage_index_of("INFRA_STUFE_FORSCH") == 31U);
    CHECK_FALSE(sim::stage_index_of("GOTT").has_value());
    CHECK_FALSE(sim::stage_index_of("SPIELER").has_value());
    CHECK_FALSE(sim::stage_index_of("").has_value());
}

TEST_CASE("population B: shipped BauFirst / IdSec equal the independent oracle") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::ifstream in(std::string(ANNO_FIXTURES_DIR) + "/sim/waren_metadata.txt");
    REQUIRE(in.good());
    std::map<std::string, std::uint32_t> bau;
    std::map<std::string, std::string> idsec;
    std::vector<std::string> stages;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string kind, a, b;
        ss >> kind >> a >> b;
        if (kind == "stage") {
            stages.push_back(b);
            CHECK(sim::stage_index_of(b) == std::stoul(a));
        } else if (kind == "not_stage") {
            CHECK_FALSE(sim::stage_index_of(b).has_value());
        } else if (kind == "bau") {
            bau[a] = static_cast<std::uint32_t>(std::stoul(b));
        } else if (kind == "idsec") {
            idsec[a] = b;
        }
    }
    CHECK(stages.size() == 32);
    CHECK(bau.size() == 23);
    CHECK(idsec.size() == 4);

    const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    const auto goods = loaded.types->goods_table();
    std::size_t checked_bau = 0, checked_sec = 0;
    for (std::size_t i = 0; i < goods.size(); ++i) {
        INFO(goods[i].name);
        // Names repeat only for UNUSED, which no record names.
        const bool named = goods[i].name != "UNUSED";
        const auto b = bau.find(goods[i].name);
        CHECK(goods[i].bau_first == (named && b != bau.end() ? b->second : 0U));
        checked_bau += named && b != bau.end() ? 1U : 0U;
        const auto s = idsec.find(goods[i].name);
        if (named && s != idsec.end()) {
            REQUIRE(goods[i].id_sec < goods.size());
            CHECK(goods[goods[i].id_sec].name == s->second);
            ++checked_sec;
        } else {
            CHECK(goods[i].id_sec == 0U);
        }
    }
    CHECK(checked_bau == 23);
    CHECK(checked_sec == 4);
}

TEST_CASE("population B: registry validation of the metadata and of the roles") {
    // A small registry without groups builds under the default config although the default
    // population ids (0x4c.. , 0x1d) are not in its goods table.
    sim::TypeRegistry::Builder plain;
    plain.goods(tiny_goods());
    auto ok = plain.build();
    REQUIRE_MESSAGE(ok.ok(), (ok.ok() ? std::string() : ok.error()));
    CHECK(ok.value()->validate_against(sim::SimConfig{}) == "");

    // The metadata must be in the domain even without groups.
    auto g = tiny_goods();
    g[1].bau_first = 32;
    sim::TypeRegistry::Builder bad_stage;
    bad_stage.goods(g);
    auto r1 = bad_stage.build();
    REQUIRE_FALSE(r1.ok());
    CHECK(r1.error().find("bau_first") != std::string::npos);
    g = tiny_goods();
    g[1].id_sec = 3;
    sim::TypeRegistry::Builder bad_sec;
    bad_sec.goods(g);
    auto r2 = bad_sec.build();
    REQUIRE_FALSE(r2.ok());
    CHECK(r2.error().find("id_sec") != std::string::npos);

    // With house groups the roles must lie in the goods domain: the original ids do not fit the
    // tiny table (a registry with groups and config roles outside it is refused).
    sim::HouseGroup grp;
    grp.number = 0;
    grp.wohnmax = 8;
    grp.needs = {sim::HouseNeed{1, false, 41}};
    sim::SimConfig cfg;
    cfg.house_groups = 1;
    sim::TypeRegistry::Builder with_groups;
    with_groups.goods(tiny_goods()).house_groups({grp}).config(cfg);
    auto r3 = with_groups.build();
    REQUIRE_FALSE(r3.ok());
    CHECK(r3.error().find("population_infra_last") != std::string::npos);
    // The same registry with roles inside its table builds, and validate_against agrees with it.
    cfg.population_infra_first = 1;
    cfg.population_infra_last = 1;
    cfg.population_tavern_goods = 1;
    cfg.population_tavern_alias_goods = 2;
    sim::TypeRegistry::Builder fitted;
    fitted.goods(tiny_goods()).house_groups({grp}).config(cfg);
    auto r4 = fitted.build();
    REQUIRE_MESSAGE(r4.ok(), (r4.ok() ? std::string() : r4.error()));
    CHECK(r4.value()->validate_against(cfg) == "");
    sim::SimConfig other = cfg;
    other.population_infra_last = 7; // outside the goods table
    CHECK(r4.value()->validate_against(other).find("population_infra_last") != std::string::npos);
}

TEST_CASE("population B: the metadata is in the registry digest") {
    const auto make = [](std::uint32_t bau, sim::GoodsId sec) {
        auto g = tiny_goods();
        g[1].bau_first = bau;
        g[1].id_sec = sec;
        sim::TypeRegistry::Builder b;
        b.goods(g);
        auto r = b.build();
        REQUIRE(r.ok());
        return r.value()->digest();
    };
    const std::uint64_t base = make(0, 0);
    CHECK(make(0, 0) == base);
    CHECK(make(1, 0) != base);
    CHECK(make(0, 2) != base);
    CHECK(make(1, 0) != make(0, 2));
}

TEST_CASE("population B: the digest layout of nonzero metadata, hand-written") {
    // bau_first then id_sec, u32 each, after forsch, per goods (stream 127). The expected value is
    // the FNV-1a 64 of an independent byte transcription, not TypeRegistry::digest of itself.
    auto goods = sim::original_goods();
    goods[26].bau_first = 5;
    goods[28].id_sec = 33;
    sim::TypeRegistry::Builder b;
    b.goods(goods);
    auto built = b.build();
    REQUIRE(built.ok());
    sim::StateHasher h;
    h.u64(0); // no types
    h.u64(goods.size());
    for (const sim::GoodsInfo& g : goods) {
        h.u64(g.name.size());
        for (const char c : g.name) {
            h.u8(static_cast<std::uint8_t>(c));
        }
        h.u32(g.forsch);
        h.u32(g.bau_first);
        h.u32(g.id_sec);
    }
    const auto classes = sim::original_class_table();
    h.u64(classes.size());
    for (const sim::ClassInfo& c : classes) {
        h.u8(static_cast<std::uint8_t>(c.cls));
        h.u8(static_cast<std::uint8_t>(c.tick));
        h.u8(static_cast<std::uint8_t>(c.payload));
        h.u32(c.gate_period);
    }
    h.u64(0); // no figure types
    h.u64(0); // no group table
    h.u64(0); // no stage table (stream 128, L3)
    CHECK(built.value()->digest() == h.value());
    // A swapped order (id_sec first) would hash differently, so the order above is pinned.
    sim::StateHasher swapped;
    swapped.u32(33);
    swapped.u32(5);
    sim::StateHasher straight;
    straight.u32(5);
    straight.u32(33);
    CHECK(swapped.value() != straight.value());
}

TEST_CASE("population L3: shipped BAUSTUFEN equals the independent oracle and is in the registry") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::ifstream in(std::string(ANNO_FIXTURES_DIR) + "/sim/baustufen_table.txt");
    REQUIRE(in.good());
    std::vector<sim::StageRecord> want;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ss(line);
        std::string kind, idx, name, g, w, c;
        ss >> kind >> idx >> name >> g >> w >> c;
        REQUIRE(kind == "stage");
        CHECK(sim::stage_index_of(name) == std::stoul(idx));
        want.push_back(sim::StageRecord{static_cast<std::uint32_t>(std::stoul(idx)),
                                        static_cast<std::uint32_t>(std::stoul(g.substr(6))),
                                        static_cast<std::uint32_t>(std::stoul(w.substr(7))),
                                        static_cast<std::uint32_t>(std::stoul(c.substr(6)))});
    }
    CHECK(want.size() == 31);
    const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    const auto got = loaded.types->stage_table();
    REQUIRE(got.size() == want.size());
    for (std::size_t i = 0; i < want.size(); ++i) {
        CHECK(got[i] == want[i]);
    }
    CHECK(got[5].wissen == 20); // INFRA_STUFE_2B
}

TEST_CASE("population L3: the stage table parser names its errors") {
    const auto parse = [](const char* text) {
        const std::string s = text;
        return sim::parse_stage_table(
            anno::formats::Bytes(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()));
    };
    CHECK(parse("[BAUSTUFEN]\n Stufe: INFRA_EVER, 0, 0\n Stufe: INFRA_STUFE_1A, 1, 30\n Wissen: "
                "INFRA_STUFE_1A, 7\n[END]\n")
              .value() == std::vector<sim::StageRecord>{{0, 0, 0, 0}, {1, 1, 7, 30}});
    CHECK(parse("[WAREN]\n").value().empty());
    CHECK_FALSE(parse("[BAUSTUFEN]\n Stufe: GOTT, 0, 0\n").ok());
    CHECK_FALSE(parse("[BAUSTUFEN]\n Stufe: INFRA_EVER, 0\n").ok());
    CHECK_FALSE(parse("[BAUSTUFEN]\n Stufe: INFRA_EVER, -1, 0\n").ok());
    CHECK_FALSE(parse("[BAUSTUFEN]\n Stufe: INFRA_EVER, 0, 0\n Stufe: INFRA_EVER, 0, 1\n").ok());
    CHECK_FALSE(parse("[BAUSTUFEN]\n Wissen: INFRA_EVER, 3\n").ok());
    CHECK_FALSE(parse("[BAUSTUFEN]\n Stufe: INFRA_EVER, 0, 0\n Wissen: INFRA_EVER, 3\n Wissen: "
                      "INFRA_EVER, 4\n")
                    .ok());
}

TEST_CASE("population L3: the registry checks and hashes the stage table") {
    const auto build = [](std::vector<sim::StageRecord> t) {
        return sim::TypeRegistry::Builder().stage_table(std::move(t)).build();
    };
    const auto none = build({});
    REQUIRE(none.ok());
    const auto one = build({{1, 0, 0, 30}});
    REQUIRE(one.ok());
    CHECK(one.value()->digest() != none.value()->digest());
    CHECK(build({{1, 0, 0, 31}}).value()->digest() != one.value()->digest());
    CHECK(build({{1, 0, 5, 30}}).value()->digest() != one.value()->digest());
    CHECK(build({{2, 0, 0, 30}}).value()->digest() != one.value()->digest());
    CHECK(build({{1, 0, 0, 30}, {31, 0, 0, 5}}).ok());
    CHECK_FALSE(build({{32, 0, 0, 1}}).ok());              // not a bit of the 32-bit mask
    CHECK_FALSE(build({{3, 0, 0, 1}, {3, 0, 0, 2}}).ok()); // repeated
    CHECK_FALSE(build({{4, 0, 0, 1}, {3, 0, 0, 2}}).ok()); // not ascending
}
