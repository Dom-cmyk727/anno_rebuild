#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include <anno/sim/wildlife.hpp>

namespace {
namespace sim = anno::sim;
using Fields = std::map<std::string, std::string>;
using Values = std::map<std::string, std::vector<std::int64_t>>;
// Supplied semantic id mappings from wildlife.md section 10, 0x100536b0–0x10053732.
// They belong to this vector caller, not the production helper or a World registry.
constexpr sim::TypeId short_list[]{sim::TypeId{117}, sim::TypeId{122}, sim::TypeId{129},
                                   sim::TypeId{135}, sim::TypeId{502}, sim::TypeId{504},
                                   sim::TypeId{505}};
constexpr sim::TypeId extended_list[]{sim::TypeId{118}, sim::TypeId{119}, sim::TypeId{128},
                                      sim::TypeId{130}, sim::TypeId{120}, sim::TypeId{123},
                                      sim::TypeId{127}, sim::TypeId{132}, sim::TypeId{137},
                                      sim::TypeId{141}, sim::TypeId{501}};
constexpr sim::TypeId extended_predators[]{sim::TypeId{116}, sim::TypeId{126}, sim::TypeId{131},
                                           sim::TypeId{201}, sim::TypeId{529}};
constexpr sim::WildlifePreyLists prey_lists{short_list, extended_list, extended_predators};

std::optional<std::uint32_t> integer(std::string_view s, std::uint32_t max = UINT32_MAX) {
    if (s.empty() || s.front() == '-' || s.front() == '+')
        return {};
    int base = 10;
    if (s.starts_with("0x") || s.starts_with("0X")) {
        base = 16;
        s.remove_prefix(2);
    }
    if (s.empty())
        return {};
    std::uint32_t value = 0;
    const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), value, base);
    if (error != std::errc{} || end != s.data() + s.size() || value > max)
        return {};
    return value;
}
std::optional<std::vector<std::uint32_t>> numbers(std::string_view s,
                                                  std::uint32_t max = UINT32_MAX) {
    std::vector<std::uint32_t> out;
    if (s == "-")
        return out;
    for (;;) {
        const auto comma = s.find(',');
        const auto n = integer(s.substr(0, comma), max);
        if (!n)
            return {};
        out.push_back(*n);
        if (comma == std::string_view::npos)
            return out;
        s.remove_prefix(comma + 1);
    }
}
std::optional<Fields> fields(std::istringstream& in) {
    Fields out;
    for (std::string word; in >> word;) {
        const auto at = word.find('=');
        if (at == 0 || at == std::string::npos || at + 1 == word.size() ||
            word.find('=', at + 1) != std::string::npos ||
            !out.emplace(word.substr(0, at), word.substr(at + 1)).second)
            return {};
    }
    return out;
}
std::vector<std::int64_t> widened(std::span<const std::uint32_t> values) {
    std::vector<std::int64_t> out;
    for (auto n : values)
        out.push_back(n);
    return out;
}
std::optional<Values> evaluate(const std::string& kind, const Fields& f) {
    const std::map<std::string, std::set<std::string>> schemas{
        {"density", {"n", "draws"}},
        {"habitat", {"center", "goods", "kind", "neighbors", "property7", "draws"}},
        {"replenish", {"candidates", "deficit", "occupied", "draws"}},
        {"create", {"cave", "entrances", "climate", "types", "goods", "saved", "draws"}},
        {"load_raw", {"raw", "draws"}},
        {"load", {"entry4", "base", "mode", "goods", "draws"}},
        {"animal", {"event", "state", "success", "quantity", "capacity", "variants", "draws"}},
        {"walk", {"attempt", "behavior", "facing", "draws"}},
        {"route_old", {"quantity", "draws"}},
        {"wolf", {"event", "state", "success", "quantity", "capacity", "variants", "draws"}},
        {"prey", {"predator", "prey", "kind", "draws"}}};
    if (!schemas.contains(kind))
        return {};
    std::set<std::string> keys;
    for (const auto& [k, v] : f) {
        (void)v;
        keys.insert(k);
    }
    if (keys != schemas.at(kind))
        return {};
    bool valid = true;
    const auto n = [&](const char* k, std::uint32_t max = UINT32_MAX) {
        const auto value = integer(f.at(k), max);
        valid = valid && value.has_value();
        return value.value_or(0);
    };
    const auto b = [&](const char* k) { return static_cast<std::uint8_t>(n(k, 255)); };
    const auto word = [&](const char* k) { return static_cast<std::uint16_t>(n(k, 65535)); };
    const auto list = [&](const char* k, std::uint32_t max = UINT32_MAX) {
        const auto v = numbers(f.at(k), max);
        valid = valid && v.has_value();
        return v.value_or(std::vector<std::uint32_t>{});
    };
    const auto draws = list("draws");
    Values out;
    const auto consumed = [&](const auto& r) {
        if (!r || !valid)
            return false;
        if (r.value().consumed > static_cast<std::size_t>(INT64_MAX))
            return false;
        out["consumed"] = {static_cast<std::int64_t>(r.value().consumed)};
        return true;
    };
    if (kind == "density") {
        const auto r = sim::wildlife_density(b("n"));
        if (!consumed(r))
            return {};
        out["desired"] = {r.value().value};
    } else if (kind == "habitat") {
        std::vector<sim::WildlifeHabitatFact> neighbors;
        std::istringstream input(f.at("neighbors"));
        for (std::string item; std::getline(input, item, ',');) {
            std::istringstream parts(item);
            std::vector<std::uint32_t> facts;
            for (std::string text; std::getline(parts, text, ':');) {
                const auto value = integer(text, 65535);
                if (!value)
                    return {};
                facts.push_back(*value);
            }
            if (facts.size() != 3 || item.ends_with(':'))
                return {};
            neighbors.push_back({sim::TypeId{facts[0]}, facts[1], facts[2]});
        }
        if (f.at("neighbors").ends_with(','))
            return {};
        const auto r =
            sim::wildlife_habitat({sim::TypeId{word("center")}, word("goods"), word("kind")},
                                  neighbors, word("property7"));
        if (!consumed(r))
            return {};
        out["passed"] = {r.value().value ? 1 : 0};
    } else if (kind == "replenish") {
        const auto candidates = list("candidates"), occupied = list("occupied");
        const auto r = sim::wildlife_replenish(candidates, b("deficit"), occupied, draws);
        if (!consumed(r))
            return {};
        out["chosen"] = widened(r.value().value.chosen);
        out["remaining"] = widened(r.value().value.remaining);
    } else if (kind == "create") {
        const auto types = list("types", 65535), goods = list("goods", 65535);
        (void)b("climate"); // Table is already resolved by this record's caller.
        if (types.size() != goods.size())
            return {};
        std::vector<sim::WildlifeSpecies> table;
        for (std::size_t i = 0; i < types.size(); ++i)
            table.push_back({sim::TypeId{types[i]}, goods[i]});
        const auto r =
            sim::wildlife_create(n("cave", 1) != 0, word("entrances"), table, word("saved"), draws);
        if (!consumed(r))
            return {};
        const auto& v = r.value().value;
        out["position"] = {v.position};
        out["index"] = {v.index};
        out["species"] = {v.species.v};
        out["inspected"] = {v.inspected};
    } else if (kind == "load_raw") {
        out["base"] = {sim::wildlife_load_base(word("raw"))};
        if (!valid)
            return {};
        out["consumed"] = {0};
    } else if (kind == "load") {
        const auto r =
            sim::wildlife_load(b("entry4"), n("base", 131), b("mode"), word("goods"), draws);
        if (!consumed(r))
            return {};
        out["timestamp"] = {r.value().value.timestamp};
        out["saved"] = {r.value().value.saved};
    } else if (kind == "animal" || kind == "wolf") {
        const auto& event = f.at("event");
        if (event != "complete" && event != "fallback" && event != "idle")
            return {};
        sim::WildlifeTransitionInput input{event == "complete"   ? sim::WildlifeEvent::Complete
                                           : event == "fallback" ? sim::WildlifeEvent::Fallback
                                                                 : sim::WildlifeEvent::Idle,
                                           b("state"),
                                           n("success", 1) != 0,
                                           word("quantity"),
                                           word("capacity"),
                                           b("variants")};
        const auto r =
            kind == "wolf" ? sim::wildlife_wolf(input, draws) : sim::wildlife_animal(input, draws);
        if (!consumed(r))
            return {};
        out["next"] = {r.value().value.next};
        if (r.value().value.variant)
            out["variant"] = {*r.value().value.variant};
    } else if (kind == "walk") {
        const auto r = sim::wildlife_walk(b("attempt"), b("behavior"), b("facing"), draws);
        if (!consumed(r))
            return {};
        const auto& v = r.value().value;
        out["radii"] = widened(v.radii);
        out["angles"] = widened(v.angles);
        out["attempt"] = {v.attempt};
    } else if (kind == "route_old") {
        const auto r = sim::wildlife_route_old(word("quantity"), draws);
        if (!consumed(r))
            return {};
        out["next"] = {r.value().value};
    } else {
        const auto predator = word("predator"), prey = word("prey");
        const auto r = sim::wildlife_prey(sim::TypeId{predator}, sim::TypeId{prey}, word("kind"),
                                          prey == 130, prey_lists);
        if (!consumed(r))
            return {};
        out["admitted"] = {r.value().value ? 1 : 0};
    }
    return out;
}
struct Record {
    std::string kind, label;
    Values actual, expected;
};
std::optional<std::vector<Record>> read_vectors(std::istream& in) {
    std::vector<Record> records;
    for (std::string line; std::getline(in, line);) {
        line = line.substr(0, line.find('#'));
        if (line.find_first_not_of(" \t\r") == std::string::npos)
            continue;
        const auto at = line.find("=>");
        if (at == std::string::npos || line.find("=>", at + 2) != std::string::npos)
            return {};
        std::istringstream left(line.substr(0, at)), right(line.substr(at + 2));
        std::string kind, label;
        if (!(left >> kind >> label) || label.find('=') != std::string::npos)
            return {};
        const auto given = fields(left), expected = fields(right);
        if (!given || !expected || !expected->contains("consumed"))
            return {};
        const auto actual = evaluate(kind, *given);
        if (!actual)
            return {};
        Values wanted;
        for (const auto& [key, text] : *expected) {
            if (!actual->contains(key))
                return {};
            if (key == "position" && text == "-1")
                wanted[key] = {-1};
            else {
                const auto v = numbers(text);
                if (!v)
                    return {};
                // Scalars may not use empty/multi-value lists, nor lists scalar spellings.
                const bool list_key =
                    key == "radii" || key == "angles" || key == "chosen" || key == "remaining";
                if (!list_key && v->size() != 1)
                    return {};
                wanted[key] = widened(*v);
            }
        }
        records.push_back({kind, label, *actual, std::move(wanted)});
    }
    if (in.bad() || records.empty())
        return {};
    return records;
}
} // namespace

TEST_CASE("wildlife vectors: all decision families and raw load conversion agree with every "
          "reference output") {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim/wildlife_vectors.txt");
    REQUIRE(in);
    const auto records = read_vectors(in);
    REQUIRE(records);
    std::map<std::string, std::size_t> counts;
    for (const auto& r : *records) {
        INFO(r.kind, " ", r.label);
        ++counts[r.kind];
        for (const auto& [key, value] : r.expected) {
            INFO("output ", key);
            CHECK(r.actual.at(key) == value);
        }
    }
    const std::map<std::string, std::size_t> expected{
        {"density", 4},   {"habitat", 5},  {"replenish", 3}, {"create", 3},
        {"load", 7},      {"load_raw", 5}, {"animal", 13},   {"walk", 4},
        {"route_old", 3}, {"wolf", 19},    {"prey", 8}};
    CHECK(counts == expected);
    CHECK(records->size() == 74);
    std::cout << "WILDLIFE_VECTORS executed=" << records->size();
    for (const auto& [kind, count] : counts)
        std::cout << ' ' << kind << '=' << count;
    std::cout << '\n';
}
TEST_CASE("wildlife controls: legal no-draw decisions remain valid") {
    const auto zero = sim::wildlife_density(0), full = sim::wildlife_density(16);
    REQUIRE(zero);
    REQUIRE(full);
    CHECK(zero.value().value == 0);
    CHECK(full.value().value == 5);
    CHECK(zero.value().consumed == 0);
    const std::array<std::uint32_t, 1> draws{1};
    const auto old = sim::wildlife_route_old(1, draws);
    REQUIRE(old);
    CHECK(old.value().value == 12);
    CHECK(old.value().consumed == 0);
    const auto prey = sim::wildlife_prey(sim::TypeId{503}, sim::TypeId{117}, 49, false, prey_lists);
    REQUIRE(prey);
    CHECK(prey.value().value);
    CHECK(prey.value().consumed == 0);
}
TEST_CASE("wildlife refusals: explicit OQs, invalid domains and exact exhausted draw counts") {
    const auto fault = [](const auto& r, sim::WildlifeFault want, std::size_t used = 0) {
        REQUIRE_FALSE(r.ok());
        CHECK(r.error().fault == want);
        CHECK(r.error().consumed == used);
    };
    using F = sim::WildlifeFault;
    const std::array<std::uint32_t, 2> cells{10, 20};
    const std::array<std::uint32_t, 1> one{0};
    fault(sim::wildlife_replenish(cells, 1, cells, one), F::DrawsExhausted, 1);
    fault(sim::wildlife_replenish(std::span(cells).first(1), 1, std::span(cells).first(1), one),
          F::Oq572CandidatesExhausted, 1);
    fault(sim::wildlife_replenish({}, 1, {}, {}), F::Oq572CandidatesExhausted);
    const std::array<std::uint32_t, 2> duplicates{10, 10};
    fault(sim::wildlife_replenish(duplicates, 0, {}, {}), F::InvalidInput);
    fault(sim::wildlife_density(17), F::InvalidInput);
    fault(sim::wildlife_habitat({}, {}, 5), F::InvalidInput);
    std::array<sim::WildlifeSpecies, 32> table{};
    fault(sim::wildlife_create(true, 0, table, 255, one), F::Oq574CaveWithoutEntrance);
    fault(sim::wildlife_create(false, 0, table, 255, one), F::DrawsExhausted, 1);
    fault(sim::wildlife_create(false, 0, {}, 255, {}), F::InvalidInput);
    fault(sim::wildlife_load(0, 65, 0, 17, {}), F::DrawsExhausted);
    fault(sim::wildlife_load(1, 132, 0, 17, {}), F::InvalidInput);
    for (std::size_t used = 0; used < 6; ++used) {
        const std::vector<std::uint32_t> draws(used, 0);
        fault(sim::wildlife_walk(0, 0, 0, draws), F::DrawsExhausted, used);
    }
    fault(sim::wildlife_walk(0, 7, 0, {}), F::DrawsExhausted);
    sim::WildlifeTransitionInput input{sim::WildlifeEvent::Idle, 28, true, 0, 9, 1};
    fault(sim::wildlife_animal(input, {}), F::DrawsExhausted);
    input.variants = 0;
    fault(sim::wildlife_wolf(input, one), F::InvalidInput);
    input = {sim::WildlifeEvent::Complete, 26, false, 0, 9, 1};
    fault(sim::wildlife_animal(input, one), F::InvalidInput);
    fault(sim::wildlife_wolf(input, one), F::InvalidInput);
    fault(sim::wildlife_prey(sim::TypeId{116}, sim::TypeId{130}, 49, true, prey_lists),
          F::Oq585ZeroHpPrey);
    // Mapping values are engine ids, not a new u16 content limit.
    table[0] = {sim::TypeId{100000}, 200000};
    const auto wide = sim::wildlife_create(true, 1, table, 255, one);
    REQUIRE(wide);
    CHECK(wide.value().value.species == sim::TypeId{100000});
}
TEST_CASE("wildlife vectors: malformed unknown missing and empty input is refused") {
    const auto parse = [](const std::string& s) {
        std::istringstream in(s);
        return read_vectors(in);
    };
    const std::string good = "density legal n=13 draws=- => desired=4 consumed=0\n";
    REQUIRE(parse(good));
    for (const auto& s : std::vector<std::string>{
             "", "# empty\n", "density bad => consumed=0\n", "unread x draws=- => consumed=0\n",
             "density x n=13 n=14 draws=- => consumed=0\n",
             "density x n=13.0 draws=- => consumed=0\n",
             "density x n=13 draws=4294967296 => consumed=0\n",
             "density x n=13 draws=- => bogus=0 consumed=0\n",
             "density x n=13 draws=- => desired=4\n", "density x n=13 draws=- => consumed=-\n",
             "density x n=13 draws=- => consumed=0,1\n", good + "broken\n"}) {
        INFO(s);
        CHECK_FALSE(parse(s));
    }
    const auto changed = parse("density bad n=13 draws=- => desired=5 consumed=0\n");
    REQUIRE(changed);
    CHECK(changed->front().actual.at("desired") != changed->front().expected.at("desired"));
}

TEST_CASE("wildlife load bounds: legal 66 and 131 refuse 132") {
    const std::array<std::uint32_t, 1> draws{63};
    const auto ordinary = sim::wildlife_load(0, 65, 0, 17, draws);
    REQUIRE(ordinary);
    CHECK(ordinary.value().value.timestamp == 96);
    CHECK(ordinary.value().consumed == 1);
    for (const std::uint32_t base : {66U, 131U}) {
        INFO("divided base " << base);
        const auto plain = sim::wildlife_load(255, base, 1, 17, draws);
        CHECK_MESSAGE(plain.ok(), "legal no-jitter base " << base);
        if (plain) {
            CHECK(plain.value().value.timestamp == base);
            CHECK(plain.value().value.saved == 255);
            CHECK(plain.value().consumed == 0);
        }
        const auto jitter = sim::wildlife_load(254, base, 0, 17, draws);
        CHECK_MESSAGE(jitter.ok(), "legal jitter base " << base);
        if (jitter) {
            CHECK(jitter.value().value.timestamp == base + 31);
            CHECK(jitter.value().value.saved == 17);
            CHECK(jitter.value().consumed == 1);
        }
        const auto exhausted = sim::wildlife_load(0, base, 0, 17, {});
        REQUIRE_FALSE(exhausted.ok());
        CHECK(exhausted.error().fault == sim::WildlifeFault::DrawsExhausted);
        CHECK(exhausted.error().consumed == 0);
    }
    const auto invalid = sim::wildlife_load(1, 132, 0, 17, {});
    REQUIRE_FALSE(invalid.ok());
    CHECK(invalid.error().fault == sim::WildlifeFault::InvalidInput);
    CHECK(invalid.error().consumed == 0);
}

TEST_CASE("wildlife raw load stamp: division agrees across the original u16 domain") {
    const std::array<std::pair<std::uint16_t, std::uint32_t>, 5> boundaries{
        {{499, 0}, {500, 1}, {1000, 2}, {33000, 66}, {65535, 131}}};
    for (const auto& [raw, base] : boundaries) {
        INFO("raw stamp ", raw);
        CHECK(sim::wildlife_load_base(raw) == base);
    }
    // AnnoWelt 0x100946e9–0x10094705: zero-extended u16, high product >> 5.
    // The signed correction is zero over this entire nonnegative input domain.
    for (std::uint32_t raw = 0; raw <= UINT16_MAX; ++raw) {
        const auto original = (std::uint64_t{raw} * 0x10624dd3U) >> 37;
        INFO("raw stamp ", raw);
        CHECK(sim::wildlife_load_base(static_cast<std::uint16_t>(raw)) == original);
    }
}
