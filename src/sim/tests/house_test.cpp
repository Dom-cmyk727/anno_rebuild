// P1, population stage 1 (docs/sim/state.md §4.7c): the group table parsed from the text of
// Data/BGruppen.dat and held by the registry, and the house payload. The synthetic groups
// are written here, in code and in text of our own; the shipped file is read through
// anno::test::game_dir and skipped without it. Every expected value follows from the text
// written or from docs/re/population.md §1, never from the parser.
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/house_groups.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
#include <anno/test/game_dir.hpp>

#include "golden.hpp"

namespace sim = anno::sim;

namespace {

using Bytes = std::vector<std::uint8_t>;

Bytes bytes_of(std::string_view text) {
    return Bytes(text.begin(), text.end());
}

constexpr std::string_view kTab21 = "-2,-2,-2,-2,-1,-1,-1,-1,0,0,0,0,0,0,1,1,2,2,2,2,2";

/// One [BGRUPPE] record in the file's own layout (CR LF, `Key: value`, comments).
std::string group_text(std::string_view name, int wohnmax, std::string_view extra_head,
                       std::string_view needs, std::string_view vermehr = "-0.25,-0.15,0,0.2,0.5") {
    std::string t =
        "  BGruppeNr:  " + std::string(name) + "\r\n  Wohnmax:    " + std::to_string(wohnmax) +
        "\r\n  BauFirst:   INFRA_EVER\r\n" + std::string(extra_head) +
        "  NahrungTab: " + std::string(kTab21) + "\r\n  BedarfTab:  " + std::string(kTab21) +
        "\r\n  InfraTab:   " + std::string(kTab21) + "\r\n  VermehrTab: " + std::string(vermehr) +
        "\r\n;=====\r\n" + std::string(needs);
    return t;
}

const std::string kNeedsA =
    "  Ware:       NAHRUNG\r\n  Menge:      1.0\r\n  Priority:   HIGH\r\n  Force:      TRUE\r\n\r\n"
    "  Ware:       LEDER    ; Aufstieg\r\n  Menge:      0.4\r\n  Priority:   LOW\r\n  Force:      "
    "FALSE\r\n";
const std::string kNeedsB =
    "  Ware:       NAHRUNG\r\n  Menge:      0.9\r\n  Force:      TRUE\r\n"
    "  Ware:       WIRTSHAUS\r\n  Menge:      0.5\r\n  Force:      TRUE\r\n";

std::string two_groups() {
    return "[WAREN]\r\n  Id: NAHRUNG\r\n[BGRUPPE]\r\n" +
           group_text("PIONIER", 8, "  NoDowngrade: TRUE\r\n", kNeedsA) +
           group_text("SIEDLER", 15, "", kNeedsB, "-0.4,-0.2,0,0.4,0.8");
}

std::string parse_error(const std::string& text) {
    const auto goods = sim::original_goods();
    const Bytes b = bytes_of(text);
    auto r = sim::parse_house_groups(b, goods);
    return r.ok() ? std::string() : r.error();
}

bool contains(const std::string& s, std::string_view part) {
    return s.find(part) != std::string::npos;
}

} // namespace

TEST_CASE("house groups: two synthetic groups parse to exact values") {
    const auto goods = sim::original_goods();
    const Bytes b = bytes_of(two_groups());
    auto r = sim::parse_house_groups(b, goods);
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    const auto& g = r.value();
    REQUIRE(g.size() == 2);
    CHECK(g[0].number == 0);
    CHECK(g[0].wohnmax == 8);
    CHECK(g[0].no_downgrade);
    CHECK_FALSE(g[0].no_upgrade);
    CHECK(g[1].number == 1);
    CHECK(g[1].wohnmax == 15);
    CHECK_FALSE(g[1].no_downgrade);
    // Menge = trunc(v * 40.96 + 0.5): 1.0 -> 41, 0.4 -> 16, 0.9 -> 37, 0.5 -> 20 (population.md
    // §1).
    REQUIRE(g[0].needs.size() == 2);
    CHECK(g[0].needs[0].ware == 26); // NAHRUNG
    CHECK(g[0].needs[0].force);
    CHECK(g[0].needs[0].menge == 41);
    CHECK(g[0].needs[1].ware == 27); // LEDER
    CHECK_FALSE(g[0].needs[1].force);
    CHECK(g[0].needs[1].menge == 16);
    REQUIRE(g[1].needs.size() == 2);
    CHECK(g[1].needs[0].menge == 37);
    CHECK(g[1].needs[1].ware == 0x4c); // WIRTSHAUS
    CHECK(g[1].needs[1].menge == 20);
    // VermehrTab = trunc(v * 64) toward zero: -0.25 -> -16, -0.15 -> -9, 0.2 -> 12, 0.5 -> 32;
    // -0.4 -> -25, -0.2 -> -12, 0.4 -> 25, 0.8 -> 51.
    CHECK(g[0].vermehr == std::array<std::int8_t, 5>{-16, -9, 0, 12, 32});
    CHECK(g[1].vermehr == std::array<std::int8_t, 5>{-25, -12, 0, 25, 51});
    CHECK(g[0].nahrung[0] == -2);
    CHECK(g[0].nahrung[8] == 0);
    CHECK(g[0].nahrung[20] == 2);
    CHECK(g[0].bedarf == g[0].infra);
    // Groups come back in slot order whatever the order in the file.
    const std::string swapped = "[BGRUPPE]\r\n" + group_text("SIEDLER", 15, "", kNeedsB) +
                                group_text("PIONIER", 8, "", kNeedsA);
    auto s = sim::parse_house_groups(bytes_of(swapped), goods);
    REQUIRE(s.ok());
    CHECK(s.value()[0].wohnmax == 8);
    CHECK(s.value()[1].wohnmax == 15);
}

TEST_CASE("house groups: malformed files are refused with a message naming the line") {
    const auto replace = [](std::string text, std::string_view from, std::string_view to) {
        const auto at = text.find(from);
        REQUIRE(at != std::string::npos);
        text.replace(at, from.size(), to);
        return text;
    };
    const std::string good = two_groups();
    CHECK(parse_error(good).empty());
    // A missing field.
    const std::string no_wohnmax = replace(good, "  Wohnmax:    8\r\n", "");
    CHECK(contains(parse_error(no_wohnmax), "without Wohnmax"));
    CHECK(contains(parse_error(replace(good, "  VermehrTab: -0.25,-0.15,0,0.2,0.5\r\n", "")),
                   "without VermehrTab"));
    CHECK(contains(parse_error(replace(good, "  Force:      TRUE\r\n\r\n", "\r\n")),
                   "without Force"));
    CHECK(contains(parse_error(replace(good, "  Menge:      1.0\r\n", "")), "without Menge"));
    // A non-integer value.
    CHECK(contains(parse_error(replace(good, "Wohnmax:    8", "Wohnmax:    eight")),
                   "Wohnmax 'eight'"));
    CHECK(contains(parse_error(replace(good, "Menge:      0.4", "Menge:      lots")),
                   "Menge 'lots'"));
    CHECK(contains(parse_error(replace(good, "-0.25,", "-0.255,")), "VermehrTab value"));
    CHECK(contains(parse_error(replace(good, "NoDowngrade: TRUE", "NoDowngrade: maybe")),
                   "not TRUE or FALSE"));
    // A table of the wrong length.
    CHECK(contains(
        parse_error(replace(good, "InfraTab:   -2,-2,-2,-2,-1", "InfraTab:   -2,-2,-2,-1")),
        "InfraTab has 20 values"));
    CHECK(contains(
        parse_error(replace(good, "0.2,0.5\r\n;=====\r\n  Ware:       NAHRUNG\r\n  Menge:      1.0",
                            "0.2\r\n;=====\r\n  Ware:       NAHRUNG\r\n  Menge:      1.0")),
        "VermehrTab has 4 values"));
    // A name the goods table does not know, a group name that is none of the five.
    CHECK(contains(parse_error(replace(good, "Ware:       LEDER", "Ware:       NICHTS")),
                   "'NICHTS' is not in the goods table"));
    CHECK(contains(parse_error(replace(good, "BGruppeNr:  SIEDLER", "BGruppeNr:  HELDEN")),
                   "five group names"));
    // A slot defined twice, a slot skipped, no group at all.
    CHECK(contains(parse_error(replace(good, "BGruppeNr:  SIEDLER", "BGruppeNr:  PIONIER")),
                   "defined twice"));
    CHECK(contains(parse_error(replace(good, "BGruppeNr:  SIEDLER", "BGruppeNr:  BUERGER")),
                   "slot 1 is missing"));
    CHECK(contains(parse_error("[WAREN]\r\n  Id: NAHRUNG\r\n"), "no [BGRUPPE]"));
}

TEST_CASE("house groups: the shipped Data/BGruppen.dat") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping the shipped BGruppen.dat: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto buf = anno::test::read_file(*dir / "Data" / "BGruppen.dat");
    const auto goods = sim::original_goods();
    auto r = sim::parse_house_groups(buf, goods);
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    const auto& g = r.value();
    REQUIRE(g.size() == 5);
    std::size_t needs = 0;
    for (std::size_t i = 0; i < g.size(); ++i) {
        INFO("group " << i);
        CHECK(g[i].number == i);
        needs += g[i].needs.size();
        // population.md §1: NAHRUNG is need slot 0 of every group; every Menge is positive.
        REQUIRE_FALSE(g[i].needs.empty());
        CHECK(goods[g[i].needs[0].ware].name == "NAHRUNG");
        for (const sim::HouseNeed& n : g[i].needs) {
            CHECK(n.menge > 0);
        }
    }
    CHECK(needs == 47); // the keyed-records corpus case of the formats tests counts the same
    // population.md §4: the downgrade floors are 3/4 of the lower group's Wohnmax
    // (settlers < 6, citizens < 11.25): pioneers 8, settlers 15, citizens 28.
    CHECK(g[0].wohnmax == 8);
    CHECK(g[1].wohnmax == 15);
    CHECK(g[2].wohnmax == 28);
    CHECK(g[0].no_downgrade); // population.md §4: pioneers do not downgrade
}

namespace {

sim::HouseGroup small_group(std::uint32_t number, std::uint16_t wohnmax, std::size_t needs) {
    sim::HouseGroup g;
    g.number = number;
    g.wohnmax = wohnmax;
    for (std::size_t i = 0; i < needs; ++i) {
        g.needs.push_back(
            {static_cast<sim::GoodsId>(26 + i), i == 0, static_cast<std::uint16_t>(10 + i)});
    }
    g.vermehr = {-1, 0, 0, 1, 2};
    return g;
}

sim::SimConfig two_group_config() {
    sim::SimConfig cfg;
    cfg.house_groups = 2;
    return cfg;
}

} // namespace

TEST_CASE("registry: a non-empty group table must have exactly SimConfig::house_groups groups") {
    {
        sim::TypeRegistry::Builder b; // the default config has 5 groups
        b.house_groups({small_group(0, 8, 1), small_group(1, 15, 2)});
        auto r = b.build();
        REQUIRE_FALSE(r.ok());
        CHECK(contains(r.error(), "the group table has 2 groups, SimConfig::house_groups is 5"));
    }
    {
        sim::TypeRegistry::Builder b;
        b.config(two_group_config());
        b.house_groups({small_group(0, 8, 1)});
        CHECK_FALSE(b.build().ok()); // too few
    }
    {
        sim::TypeRegistry::Builder b;
        b.config(two_group_config());
        b.house_groups({small_group(0, 8, 1), small_group(1, 15, 2)});
        auto r = b.build();
        REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
        CHECK(r.value()->house_groups().size() == 2);
    }
    {
        sim::TypeRegistry::Builder b; // an empty table is allowed whatever house_groups says
        CHECK(b.build().ok());
        CHECK(sim::TypeRegistry::empty()->house_groups().empty());
    }
}

TEST_CASE("registry digest: the group table is part of it, every field moves it") {
    const auto digest = [](std::vector<sim::HouseGroup> groups) {
        sim::TypeRegistry::Builder b;
        b.config(two_group_config());
        b.house_groups(std::move(groups));
        auto r = b.build();
        REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
        return r.value()->digest();
    };
    const std::vector<sim::HouseGroup> base = {small_group(0, 8, 1), small_group(1, 15, 2)};
    const std::uint64_t d = digest(base);
    CHECK(digest(base) == d);
    sim::TypeRegistry::Builder none;
    none.config(two_group_config());
    CHECK(none.build().value()->digest() != d); // the table is in the digest
    std::vector<std::pair<const char*, std::vector<sim::HouseGroup>>> changed;
    const auto with = [&](const char* name, auto&& edit) {
        auto g = base;
        edit(g);
        changed.emplace_back(name, std::move(g));
    };
    with("wohnmax", [](auto& g) { g[1].wohnmax += 1; });
    with("no_upgrade", [](auto& g) { g[0].no_upgrade = true; });
    with("no_downgrade", [](auto& g) { g[0].no_downgrade = true; });
    with("need ware", [](auto& g) { g[1].needs[1].ware += 1; });
    with("need force", [](auto& g) { g[1].needs[1].force = true; });
    with("need menge", [](auto& g) { g[1].needs[0].menge += 1; });
    with("need added", [](auto& g) { g[0].needs.push_back({30, false, 1}); });
    with("nahrung", [](auto& g) { g[0].nahrung[20] = 1; });
    with("bedarf", [](auto& g) { g[0].bedarf[0] = 1; });
    with("infra", [](auto& g) { g[1].infra[10] = -1; });
    with("vermehr", [](auto& g) { g[1].vermehr[4] += 1; });
    with("number", [](auto& g) {
        g[0].number = 1;
        g[1].number = 0;
    });
    std::vector<std::uint64_t> seen = {d};
    for (const auto& [name, groups] : changed) {
        INFO(name);
        const std::uint64_t x = digest(groups);
        CHECK(x != d);
        for (const std::uint64_t s : seen) {
            CHECK(x != s);
        }
        seen.push_back(x);
    }
}

TEST_CASE("house payload: constructor values, the stock sized by the group, payload_problem") {
    sim::TypeRegistry::Builder b;
    b.config(two_group_config());
    b.add_plain(sim::TypeId{1}, 97); // group 0
    b.add_plain(sim::TypeId{2}, 98); // group 1
    b.add_plain(sim::TypeId{3}, 35); // a market (not a house)
    b.house_groups({small_group(0, 8, 2), small_group(1, 15, 5)});
    auto r = b.build();
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    const auto reg = r.value();
    const sim::SimConfig cfg = two_group_config();
    const sim::BuildingType& h0 = *reg->find(sim::TypeId{1});
    const sim::BuildingType& h1 = *reg->find(sim::TypeId{2});
    const sim::ClassInfo& ci = reg->class_info(sim::RuntimeClass::House);
    CHECK(ci.tick == sim::TickKind::House);
    CHECK(ci.payload == sim::PayloadTag::House);
    CHECK(ci.gate_period == 2500);
    CHECK(sim::house_need_count(h0, cfg, *reg) == 2);
    CHECK(sim::house_need_count(h1, cfg, *reg) == 5);
    CHECK(sim::house_need_count(*reg->find(sim::TypeId{3}), cfg, *reg) == 0);
    const sim::Payload p0 = sim::make_payload(h0, ci, cfg, *reg);
    const auto* house = std::get_if<sim::House>(&p0);
    REQUIRE(house != nullptr);
    // population.md §11.2: the constructor writes residents * 64 = 5 * 64 = 0x140, E = 5 * 128 =
    // 0x280, zeroes every stock; `last` is 0 (P1 decision 8).
    CHECK(house->residents_64 == 0x140);
    CHECK(house->e == 0x280);
    CHECK(house->last == 0);
    CHECK(house->stock == std::vector<std::uint32_t>{0, 0});
    CHECK(std::get<sim::House>(sim::make_payload(h1, ci, cfg, *reg)).stock.size() == 5);
    CHECK(sim::payload_problem(p0, h0, ci, cfg, *reg).empty());
    // A stock vector of another size is rejected; so is another alternative.
    sim::Payload wrong = p0;
    std::get<sim::House>(wrong).stock.push_back(0);
    CHECK(contains(sim::payload_problem(wrong, h0, ci, cfg, *reg),
                   "stock has 3 entries, the type needs 2"));
    CHECK(contains(sim::payload_problem(p0, h1, ci, cfg, *reg),
                   "stock has 2 entries, the type needs 5"));
    CHECK(contains(sim::payload_problem(sim::Inert{}, h0, ci, cfg, *reg),
                   "holds Inert, the class needs House"));
    // A registry without a group table: no stocks.
    sim::TypeRegistry::Builder plain;
    plain.add_plain(sim::TypeId{1}, 97);
    const auto preg = plain.build().value();
    CHECK(std::get<sim::House>(
              sim::make_payload(*preg->find(sim::TypeId{1}), ci, sim::SimConfig{}, *preg))
              .stock.empty());
}

TEST_CASE("house payload: the stream layout") {
    sim::House h;
    h.last = 0x0102;
    h.residents_64 = -2;
    h.e = 0x0a0b0c0d;
    h.stock = {7, 0x11223344};
    sim::WriteArchive w;
    fields(w, h);
    const std::vector<std::uint8_t> expect = {
        0x02, 0x01,                                     // last
        0xfe, 0xff, 0xff, 0xff,                         // residents_64 (i32, little endian)
        0x0d, 0x0c, 0x0b, 0x0a,                         // e
        2,    0,    0,    0,    0,    0,    0,    0,    // stock count
        7,    0,    0,    0,    0x44, 0x33, 0x22, 0x11, // stock
    };
    CHECK(w.data() == expect);
    sim::House back;
    sim::ReadArchive r(w.data());
    fields(r, back);
    CHECK_FALSE(r.failed());
    CHECK(back.last == h.last);
    CHECK(back.residents_64 == h.residents_64);
    CHECK(back.e == h.e);
    CHECK(back.stock == h.stock);
}

namespace {

/// The house payload of the one house of a G7 world.
const sim::House& g7_house(const sim::World& w) {
    for (const auto& slot : w.islands()[0]->cities[0]->index.lists[3].slots) {
        if (slot.has_value()) {
            return std::get<sim::House>(slot->payload);
        }
    }
    FAIL("no house in the G7 world");
    return std::get<sim::House>(w.islands()[0]->cities[0]->index.lists[3].slots[0]->payload);
}

} // namespace

TEST_CASE("G7 (P1): the created population world, and the House tick changes nothing") {
    namespace syn = sim::synthetic;
    auto types = syn::make_population_types();
    REQUIRE_MESSAGE(types.ok(), (types.ok() ? std::string() : types.error()));
    const auto& t = types.value();
    REQUIRE(t.registry->house_groups().size() == 2);
    auto made = sim::World::create(syn::g7_scenario(t), t.registry, syn::population_config());
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string() : made.error()));
    sim::World w = std::move(made).value();
    CHECK(w.check_invariants() == "");
    // The created house: 5 residents, E = 5 * 128, the food stock of s = 128, the second need 0.
    const sim::House before = g7_house(w);
    CHECK(before.residents_64 == 5 * 64);
    CHECK(before.e == 5U * 128U);
    CHECK(before.last == 0);
    CHECK(before.stock == std::vector<std::uint32_t>{2186, 0}); // trunc(41 * 128 * 5 * 128 / 1536)
    const std::uint64_t created = w.state_hash();
    // 2000 steps of 10 ms = 20 000 ticks: eight gate periods of the house class (2500), all
    // of them visits of tick_building. TickKind::House does nothing: the payload, `last`
    // included, is unchanged. (The city and the store are touched by other rules only.)
    for (int i = 0; i < 2000; ++i) {
        w.advance(10);
    }
    const sim::House after = g7_house(w);
    CHECK(after.last == before.last);
    CHECK(after.residents_64 == before.residents_64);
    CHECK(after.e == before.e);
    CHECK(after.stock == before.stock);
    CHECK(w.check_invariants() == "");
    CHECK(w.state_hash() != created); // the clock moved
    std::string line;
    const std::string problem = anno::test::check_golden("p1_g7_create", "P1", created, line);
    if (anno::test::update_mode()) {
        MESSAGE("golden line: " << line);
    }
    CHECK_MESSAGE(problem.empty(), problem);
}
