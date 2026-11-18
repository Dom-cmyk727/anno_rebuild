#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/bytes.hpp>
#include <anno/formats/insel.hpp>
#include <anno/sim/resource_import.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

#include "alloc_probe.hpp"

namespace sim = anno::sim;
namespace ri = anno::sim::resource_import;
namespace {
using Fields = std::map<std::string, std::string>;
std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string s; std::getline(in, s, delimiter);)
        out.push_back(s);
    return out;
}
std::int64_t number(const std::string& text) {
    std::int64_t v = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), v);
    REQUIRE(result.ec == std::errc{});
    REQUIRE(result.ptr == text.data() + text.size());
    return v;
}
template <class T> std::vector<T> numbers(const std::string& text) {
    std::vector<T> out;
    if (text != "-")
        for (const auto& part : split(text, ','))
            out.push_back(static_cast<T>(number(part)));
    return out;
}
std::vector<sim::TypeId> ids(const std::string& text) {
    std::vector<sim::TypeId> out;
    for (const auto v : numbers<std::uint32_t>(text))
        out.emplace_back(v);
    return out;
}
Fields fields(const std::string& text) {
    Fields out;
    std::istringstream in(text);
    for (std::string part; in >> part;) {
        const auto at = part.find('=');
        REQUIRE(at != std::string::npos);
        REQUIRE(out.emplace(part.substr(0, at), part.substr(at + 1)).second);
    }
    return out;
}
struct Rig {
    sim::Island island;
    std::shared_ptr<const sim::TypeRegistry> types;
    std::vector<ri::TypeFacts> facts;
    ri::ClimateFacts climate;
    std::vector<ri::Record> records;
    std::vector<std::uint8_t> plane;
    std::vector<std::uint32_t> draws;
};
Rig rig(const Fields& f) {
    Rig r;
    REQUIRE(r.island.tiles.reset(0, 0, static_cast<std::uint32_t>(number(f.at("w"))),
                                 static_cast<std::uint32_t>(number(f.at("h"))), 4));
    r.island.terrain = sim::make_terrain(numbers<std::uint8_t>(f.at("heights")), {}, {});
    const auto objects = numbers<std::uint32_t>(f.at("objects"));
    REQUIRE(objects.size() == r.island.tiles.tile_count());
    for (std::uint64_t i = 0; i < objects.size(); ++i)
        r.island.tiles.set_object(i, objects[i]);
    sim::TypeRegistry::Builder builder;
    for (const auto& row : split(f.at("types"), ';')) {
        const auto v = split(row, ':');
        REQUIRE(v.size() == 7);
        sim::BuildingType type;
        type.id = sim::TypeId{static_cast<std::uint32_t>(number(v[0]))};
        type.prodtype = static_cast<std::uint32_t>(number(v[1]));
        type.width = static_cast<std::uint32_t>(number(v[2]));
        type.height = static_cast<std::uint32_t>(number(v[3]));
        type.flags_word = static_cast<std::uint32_t>(number(v[4]));
        builder.add(type);
        r.facts.push_back({type.id, static_cast<std::uint8_t>(number(v[5])),
                           static_cast<std::uint16_t>(number(v[6]))});
    }
    const auto registry = builder.build();
    REQUIRE_MESSAGE(registry, (registry.ok() ? "" : registry.error()));
    r.types = registry.value();
    for (const auto& row : split(f.at("records"), ';')) {
        const auto v = split(row, ':');
        REQUIRE(v.size() == 5);
        r.records.push_back(
            {sim::TypeId{static_cast<std::uint32_t>(number(v[0]))},
             static_cast<std::int32_t>(number(v[1])), static_cast<std::int32_t>(number(v[2])),
             static_cast<std::uint8_t>(number(v[3])), static_cast<std::uint16_t>(number(v[4]))});
    }
    r.climate = {ids(f.at("slots")), ids(f.at("map104")), ids(f.at("map201")), ids(f.at("map404"))};
    r.plane = numbers<std::uint8_t>(f.at("plane"));
    r.draws = numbers<std::uint32_t>(f.at("draws"));
    return r;
}
auto evaluate(const Rig& r) {
    return ri::evaluate(r.island.tiles, sim::VertexGrid::of(r.island), r.plane, *r.types, r.facts,
                        r.climate, r.records, r.draws);
}
std::string first_vector() {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim/resource_import_vectors.txt");
    for (std::string line; std::getline(in, line);)
        if (!line.empty() && line[0] != '#')
            return line.substr(0, line.find("=>"));
    FAIL("missing authored vector");
    return {};
}
std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    return {std::istreambuf_iterator<char>(in), {}};
}
// Test-only PE32 VA mapper: table data is runtime-read, never a shipped fixture.
std::vector<sim::TypeId> pe_words(const std::vector<std::uint8_t>& pe, std::uint32_t va,
                                  std::uint32_t count) {
    const auto b = anno::formats::Bytes{pe};
    REQUIRE(b.size() >= 64);
    const auto nt = anno::formats::u32_le(b, 0x3c);
    REQUIRE(nt + 24U <= b.size());
    const auto opt = nt + 24U;
    REQUIRE(anno::formats::u16_le(b, opt) == 0x10b);
    REQUIRE(opt + 32U <= b.size());
    const auto base = anno::formats::u32_le(b, opt + 28U);
    REQUIRE(va >= base);
    const auto rva = va - base;
    const auto table = opt + anno::formats::u16_le(b, nt + 20U);
    const auto sections = anno::formats::u16_le(b, nt + 6U);
    for (std::uint32_t i = 0; i < sections; ++i) {
        const auto at = table + 40U * i;
        REQUIRE(at + 40U <= b.size());
        const auto start = anno::formats::u32_le(b, at + 12U);
        const auto size = anno::formats::u32_le(b, at + 16U);
        if (rva < start || std::uint64_t{rva - start} + 2ULL * count > size)
            continue;
        const auto offset = anno::formats::u32_le(b, at + 20U) + (rva - start);
        REQUIRE(std::uint64_t{offset} + 2ULL * count <= b.size());
        std::vector<sim::TypeId> out;
        for (std::uint32_t j = 0; j < count; ++j)
            out.emplace_back(anno::formats::u16_le(b, offset + 2U * j));
        return out;
    }
    FAIL("selector row VA not in a PE raw section");
    return {};
}
} // namespace

TEST_CASE("resource import: authored ordered prefixes and draw-sensitive selectors") {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim/resource_import_vectors.txt");
    REQUIRE(in.good());
    std::uint32_t count = 0;
    for (std::string line; std::getline(in, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        INFO(line);
        const auto at = line.find("=>");
        REQUIRE(at != std::string::npos);
        const auto input = fields(line.substr(0, at)), expected = fields(line.substr(at + 2));
        const auto r = rig(input);
        const auto result = evaluate(r);
        REQUIRE(result);
        const auto& out = result.value();
        const auto obj = numbers<std::uint32_t>(expected.at("objects"));
        const auto phase = numbers<std::uint8_t>(expected.at("phase"));
        const auto height = numbers<std::uint8_t>(expected.at("height"));
        REQUIRE(obj.size() == out.tiles.tile_count());
        REQUIRE(phase.size() == obj.size());
        REQUIRE(height.size() == obj.size());
        for (std::uint64_t i = 0; i < obj.size(); ++i) {
            CHECK(out.tiles.object(i) == obj[i]);
            CHECK(out.tiles.phase(i) == phase[i]);
            CHECK(out.tiles.item_height(i) == height[i]);
            CHECK(r.island.tiles.object(i) == numbers<std::uint32_t>(input.at("objects"))[i]);
            CHECK(r.island.tiles.phase(i) == 0);
        }
        CHECK(out.variation == numbers<std::uint8_t>(expected.at("plane")));
        CHECK(r.plane == numbers<std::uint8_t>(input.at("plane")));
        CHECK(out.consumed == static_cast<std::uint64_t>(number(expected.at("consumed"))));
        std::vector<std::string> status;
        for (const auto d : out.records) {
            switch (d) {
            case ri::Disposition::Accepted:
                status.emplace_back("accepted");
                break;
            case ri::Disposition::Bounds:
                status.emplace_back("bounds");
                break;
            case ri::Disposition::Occupied:
                status.emplace_back("occupied");
                break;
            case ri::Disposition::Terrain:
                status.emplace_back("terrain");
                break;
            }
        }
        CHECK(status == split(expected.at("status"), ','));
        if (obj.size() == 4 && obj[0] == 104 && obj[1] == 104) {
            CHECK(out.tiles.foot_dx(0) == 0);
            CHECK(out.tiles.foot_dx(1) == 1);
            CHECK(out.tiles.foot_dy(1) == 0);
            CHECK(out.tiles.foot_anchor(0) == 1);
            CHECK(out.tiles.foot_anchor(1) == 0);
            CHECK(out.tiles.walk_code(1) == 4);
            CHECK(out.tiles.walk_mask(1) == 0);
        }
        CHECK(out.tiles.ignored_writes() == 0);
        ++count;
    }
    CHECK(count == 20);
}

TEST_CASE("resource import: errors expose no partial success or input mutation") {
    const auto original = rig(fields(first_vector()));
    for (std::uint32_t test = 0; test < 15; ++test) {
        auto r = original;
        auto expected = ri::ErrorKind::Facts;
        switch (test) {
        case 0:
            r.draws.clear();
            expected = ri::ErrorKind::Exhausted;
            break;
        case 1:
            r.facts.clear();
            break;
        case 2:
            r.plane.pop_back();
            expected = ri::ErrorKind::Geometry;
            break;
        case 3:
            r.climate.tag104.clear();
            break;
        case 4:
            r.facts.push_back(r.facts.front());
            break;
        case 5:
            r.records[0].rotation = 4;
            expected = ri::ErrorKind::Unsupported;
            break;
        case 6:
            r.island.tiles.set_object(0, 999);
            break;
        case 7:
            r.island.terrain = sim::make_terrain({48}, {}, {});
            expected = ri::ErrorKind::Geometry;
            break;
        case 8:
            r.climate.slots.pop_back();
            break;
        case 9:
            r.records[0].x = -1;
            expected = ri::ErrorKind::Unsupported;
            break;
        }
        if (test == 10) {
            auto type = *r.types->find(sim::TypeId{104});
            type.flags_word = 8;
            sim::TypeRegistry::Builder builder;
            builder.add(type);
            const auto built = builder.build();
            REQUIRE(built);
            r.types = built.value();
            expected = ri::ErrorKind::Unsupported;
        }
        if (test == 11)
            r.facts[0].id = sim::TypeId{};
        if (test == 12)
            r.climate.slots[0] = sim::TypeId{};
        if (test == 13)
            r.records[0].type = sim::TypeId{0};
        if (test == 14)
            r.records[0].type = sim::TypeId{999};
        const auto plane = r.plane;
        const auto before = r.island.tiles.object(0);
        const auto result = evaluate(r);
        REQUIRE_FALSE(result);
        CHECK(result.error().kind == expected);
        CHECK(result.error().record == 0);
        CHECK(result.error().consumed == ((test == 1 || test == 3) ? 1 : 0));
        CHECK(r.plane == plane);
        CHECK(r.island.tiles.object(0) == before);
        CHECK(r.island.tiles.phase(0) == 0);
    }
    auto r = original;
    r.records.push_back(r.records.front());
    const auto exhausted = evaluate(r);
    REQUIRE_FALSE(exhausted);
    CHECK(exhausted.error().record == 1);
    CHECK(exhausted.error().consumed == 1);
    CHECK(r.island.tiles.object(0) == 0);
    CHECK(r.plane == original.plane);
}

TEST_CASE("resource import: one prepared index handles unsorted unrelated and following facts") {
    auto input = fields(first_vector());
    input["types"] = "104:226:1:1:41:0:105;105:291:1:1:9:70:404";
    auto small = rig(input);
    std::reverse(small.facts.begin(), small.facts.end());
    const auto repeated = small.records.front();
    small.records.assign(16, repeated);
    small.draws.assign(16, 4);
    auto large = small;
    for (std::uint32_t id = 1000; id < 5096; ++id)
        large.facts.push_back({sim::TypeId{id}, 0, 0});
    std::reverse(large.facts.begin(), large.facts.end());
    const auto first_id = large.facts.front().id;
    const auto measured = [](const Rig& r) {
        anno::test::AllocScope allocations;
        auto result = evaluate(r);
        const auto bytes = allocations.bytes();
        return std::pair{std::move(result), bytes};
    };
    REQUIRE(small.records.front().type == sim::TypeId{104});
    REQUIRE(small.types->find(small.records.front().type) != nullptr);
    REQUIRE(small.facts.size() == 2);
    REQUIRE(small.facts[0].id == sim::TypeId{105});
    REQUIRE(small.facts[1].id == sim::TypeId{104});
    const auto baseline = measured(small);
    const auto indexed = measured(large);
    if (!baseline.first)
        std::cout << "INDEX_BASELINE_ERROR kind="
                  << static_cast<unsigned>(baseline.first.error().kind)
                  << " record=" << baseline.first.error().record
                  << " consumed=" << baseline.first.error().consumed << "\n";
    if (!indexed.first)
        std::cout << "INDEX_LARGE_ERROR kind=" << static_cast<unsigned>(indexed.first.error().kind)
                  << " record=" << indexed.first.error().record
                  << " consumed=" << indexed.first.error().consumed << "\n";
    REQUIRE(baseline.first);
    REQUIRE(indexed.first);
    CHECK(indexed.first.value().variation == std::vector<std::uint8_t>{75, 12, 13, 14});
    CHECK(indexed.first.value().variation == baseline.first.value().variation);
    CHECK(indexed.first.value().consumed == 16);
    CHECK(indexed.first.value().tiles.object(0) == 104);
    CHECK(indexed.first.value().tiles.phase(0) == 7);
    CHECK(indexed.first.value().records ==
          std::vector<ri::Disposition>(16, ri::Disposition::Accepted));
    CHECK(large.facts.front().id == first_id); // Sorting never mutates caller facts.
    // Test-only existing allocator probe: one owned index, not no index (old linear
    // scans) or a new index for every resource. No wall-clock threshold or public
    // instrumentation. sort/adjacent validation + lower_bound establish work bounds.
    const auto payload =
        static_cast<std::uint64_t>(large.facts.size() - small.facts.size()) * sizeof(ri::TypeFacts);
    REQUIRE(indexed.second >= baseline.second);
    const auto extra = indexed.second - baseline.second;
    CHECK(extra >= payload);
    CHECK(extra < 2 * payload);
    std::cout << "RESOURCE_INDEX records=16 required_facts=2 unrelated_facts=4096 extra_bytes="
              << extra << " one_index_payload=" << payload << "\n";
    const auto selected = ri::select(*large.types, large.facts, large.climate, sim::TypeId{104}, 4);
    REQUIRE(selected);
    CHECK(selected.value() == 4);
    auto duplicate = large;
    duplicate.facts.push_back(duplicate.facts.front()); // Nonadjacent in supplied order.
    const auto rejected = evaluate(duplicate);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().kind == ri::ErrorKind::Facts);
    CHECK(rejected.error().consumed == 0);
    const auto standalone =
        ri::select(*duplicate.types, duplicate.facts, duplicate.climate, sim::TypeId{104}, 4);
    REQUIRE_FALSE(standalone);
    CHECK(standalone.error() == ri::ErrorKind::Facts);
    large.facts.erase(
        std::remove_if(large.facts.begin(), large.facts.end(),
                       [](const ri::TypeFacts& f) { return f.id == sim::TypeId{105}; }),
        large.facts.end());
    const auto missing_following = evaluate(large);
    REQUIRE_FALSE(missing_following);
    CHECK(missing_following.error().kind == ri::ErrorKind::Facts);
    CHECK(missing_following.error().record == 0);
    CHECK(missing_following.error().consumed == 1); // Still after accepted placement/draw.
    CHECK(large.island.tiles.object(0) == 0);
    CHECK(large.plane == std::vector<std::uint8_t>{171, 12, 13, 14});
}

TEST_CASE("resource import: registered zero sentinel differs from legal sibling") {
    auto r = rig(fields(first_vector()));
    sim::TypeRegistry::Builder builder;
    sim::BuildingType ordinary;
    ordinary.id = sim::TypeId{0};
    ordinary.prodtype = 291;
    ordinary.flags_word = 9;
    builder.add(ordinary);
    ordinary.id = sim::TypeId{800};
    builder.add(ordinary);
    const auto types = builder.build();
    REQUIRE(types);
    r.types = types.value();
    REQUIRE(r.types->find(sim::TypeId{0}) != nullptr);
    REQUIRE(r.types->find(sim::TypeId{800}) != nullptr);
    r.facts.clear(); // Ordinary nonresource placement never needs selector metadata.
    r.draws.clear();
    r.records = {{sim::TypeId{800}, 0, 0, 0, 9}};
    const auto sibling = evaluate(r);
    REQUIRE(sibling);
    CHECK(sibling.value().tiles.object(0) == 800);
    CHECK(sibling.value().tiles.phase(0) == 9);
    CHECK(sibling.value().tiles.item_height(0) == 48);
    CHECK(sibling.value().variation == r.plane);
    CHECK(sibling.value().records == std::vector{ri::Disposition::Accepted});
    CHECK(sibling.value().consumed == 0);
    std::cout << "REGISTERED_ZERO lookup_zero=present legal_sibling=accepted object=800 draws=0\n";
    r.records[0].type = sim::TypeId{0};
    const auto zero = evaluate(r);
    std::cout << "REGISTERED_ZERO zero_result=" << (zero ? "accepted" : "refused") << "\n";
    REQUIRE_FALSE(zero); // Removing only the zero guard must fail HERE, after legal sibling.
    CHECK(zero.error().kind == ri::ErrorKind::Facts);
    CHECK(zero.error().record == 0);
    CHECK(zero.error().consumed == 0);
    CHECK(r.island.tiles.object(0) == 0);
    CHECK(r.island.tiles.phase(0) == 0);
    CHECK(r.plane == std::vector<std::uint8_t>{171, 12, 13, 14});
}

TEST_CASE("resource import: original Tutorial first resource prefix") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const auto registry =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    const auto data = read_file(*dir / "Scenes/Tutorial/01Discover01.sze");
    const auto islands = anno::formats::insel::islands(data);
    REQUIRE(islands);
    REQUIRE(islands.value().size() == 1);
    const auto& raw = islands.value().front();
    REQUIRE(raw.number == 11);
    REQUIRE(raw.x == 24);
    REQUIRE(raw.y == 24);
    const auto heights = raw.heights();
    REQUIRE(heights);
    std::vector<ri::Record> records;
    std::uint32_t header = 0;
    for (const auto& chunk : raw.records) {
        if (chunk.name() == "GELAENDE" || chunk.name() == "PROD2" || chunk.name() == "FLUESSE") {
            const auto block = anno::formats::insel::Block::parse(chunk.payload);
            REQUIRE(block);
            if (!block.value().data.empty()) {
                REQUIRE(chunk.name() == "GELAENDE");
                REQUIRE(header == 34);
                const auto item = block.value().data.first(6);
                // Original mask is 14 bits, offsets signed (0x100893xx), no dedup adapter.
                const auto id = anno::formats::u16_le(item, 0) & 0x3fffU;
                const auto local_x = static_cast<std::int32_t>(block.value().x) +
                                     (item[4] < 128 ? item[4] : static_cast<int>(item[4]) - 256);
                const auto local_y = static_cast<std::int32_t>(block.value().y) +
                                     (item[5] < 128 ? item[5] : static_cast<int>(item[5]) - 256);
                REQUIRE(id == 70);
                REQUIRE(local_x == 59);
                REQUIRE(local_y == 29);
                records.push_back({sim::TypeId{id}, raw.x + local_x, raw.y + local_y,
                                   static_cast<std::uint8_t>(anno::formats::u16_le(item, 0) >> 14),
                                   anno::formats::u16_le(item, 2)});
                break;
            }
        }
        ++header;
    }
    REQUIRE(records.size() == 1);
    const auto info = raw.record("INFO");
    REQUIRE(info);
    REQUIRE((*info)[0] == 2);
    sim::Island island;
    REQUIRE(island.tiles.reset(raw.x, raw.y, raw.width, raw.height, 8));
    island.terrain = sim::make_terrain(heights.value(), {}, {});
    const auto* type = registry.types->find(sim::TypeId{70});
    REQUIRE(type);
    REQUIRE(type->prodtype == 226);
    REQUIRE(type->flags_word == 0x29);
    REQUIRE(type->head_raw.size() >= 0x1c);
    REQUIRE(anno::formats::u16_le(type->head_raw, 0x1a) == 0);
    const auto* next = registry.types->find(sim::TypeId{71});
    REQUIRE(next);
    REQUIRE(next->output_goods.size() > 0);
    REQUIRE(next->output_goods[0] != 70);
    const auto anlagen_bytes = read_file(*dir / "Anlagen.dat");
    const auto anlagen = anno::formats::anlagen::AnlagenFile::parse(anlagen_bytes);
    REQUIRE(anlagen);
    const auto* raw_type = anlagen.value().entry(70);
    REQUIRE(raw_type);
    REQUIRE(raw_type->record_str("WARE") == std::optional<std::string>{"UNUSED"});
    // UNUSED = 0 is an explicit parsed fact (formats/anlagen.md), not empty-vector fallback.
    const std::vector<ri::TypeFacts> facts{
        {sim::TypeId{70}, 0, 0},
        {sim::TypeId{71}, static_cast<std::uint8_t>(next->output_goods[0]), 0}};
    const auto pe = read_file(*dir / "AnnoGame.dll");
    ri::ClimateFacts climate;
    climate.slots = pe_words(pe, 0x1002114c + 2 * 16 * 2, 16);
    REQUIRE(std::find(climate.slots.begin(), climate.slots.end(), sim::TypeId{70}) ==
            climate.slots.begin() + 6);
    const std::vector<std::uint8_t> plane(island.tiles.tile_count(), 11);
    const std::vector<std::uint32_t> draws{0, 0xffffffffU};
    const auto out = ri::evaluate(island.tiles, sim::VertexGrid::of(island), plane, *registry.types,
                                  facts, climate, records, draws);
    REQUIRE(out);
    REQUIRE(out.value().records == std::vector{ri::Disposition::Accepted});
    CHECK(out.value().consumed == 1);
    const auto index = island.tiles.index_of(83, 53);
    CHECK(out.value().tiles.object(index) == 70);
    CHECK(out.value().tiles.phase(index) == (records.front().raw_state & 63U));
    CHECK(out.value().tiles.item_height(index) == 48);
    CHECK(out.value().variation[index] == 107); // slot 6 high nibble + authored low 11
    CHECK(island.tiles.object(index) == 0);
    CHECK(plane[index] == 11);
    std::cout << "RESOURCE_PREFIX headers_before=34 supported=1 unsupported=0 accepted=1 draws=1 "
                 "variant=6 height=48\n";
}
