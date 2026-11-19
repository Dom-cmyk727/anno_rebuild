#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <utility>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/sha256.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/wildlife_first_call.hpp>
#include <anno/sim_load/wildlife_postload_fill.hpp>
#include <anno/test/game_dir.hpp>

namespace fmt = anno::formats;
namespace sim = anno::sim;
namespace load = anno::sim_load;
namespace {
using Buffer = fmt::ByteBuffer;
// Ordered fields in the hand-authored record, not a memory image.
using Cell = std::array<std::uint32_t, 10>;
std::vector<std::int64_t> numbers(const std::string& text) {
    std::istringstream in(text);
    std::string part;
    std::vector<std::int64_t> out;
    while (std::getline(in, part, ',')) {
        std::int64_t value = 0;
        const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
        REQUIRE(error == std::errc{});
        REQUIRE(end == part.data() + part.size());
        out.push_back(value);
    }
    return out;
}
template <class T> std::vector<T> column(const std::string& text) {
    std::vector<T> out;
    for (const auto value : numbers(text)) {
        REQUIRE(value >= std::numeric_limits<T>::min());
        REQUIRE(value <= std::numeric_limits<T>::max());
        out.push_back(static_cast<T>(value));
    }
    return out;
}
std::vector<Cell> cells(const std::string& text) {
    std::istringstream in(text);
    std::string part;
    std::vector<Cell> out;
    while (std::getline(in, part, ';')) {
        const auto row = column<std::uint32_t>(part);
        REQUIRE(row.size() == 10);
        Cell cell{};
        std::copy(row.begin(), row.end(), cell.begin());
        out.push_back(cell);
    }
    return out;
}
std::map<std::string, std::string> fields(std::istringstream& in) {
    std::map<std::string, std::string> result;
    std::string token;
    while (in >> token) {
        const auto split = token.find('=');
        REQUIRE(split != std::string::npos);
        REQUIRE(result.emplace(token.substr(0, split), token.substr(split + 1)).second);
    }
    return result;
}
Cell read(const sim::TileLayer& t, std::uint64_t i) {
    return {t.object(i),    t.phase(i),   t.owner(i),   t.aux(i),           t.walk_code(i),
            t.walk_mask(i), t.foot_dx(i), t.foot_dy(i), t.foot_rotation(i), t.foot_anchor(i)};
}
void write(sim::TileLayer& t, std::uint64_t i, const Cell& c) {
    t.set_object(i, c[0]);
    t.set_phase(i, static_cast<std::uint8_t>(c[1]));
    t.set_owner(i, c[2]);
    t.set_aux(i, c[3]);
    t.set_walk_code(i, static_cast<std::uint16_t>(c[4]));
    t.set_walk_mask(i, static_cast<std::uint16_t>(c[5]));
    t.set_foot_dx(i, static_cast<std::uint16_t>(c[6]));
    t.set_foot_dy(i, static_cast<std::uint16_t>(c[7]));
    t.set_foot_rotation(i, static_cast<std::uint8_t>(c[8]));
    t.set_foot_anchor(i, static_cast<std::uint8_t>(c[9]));
}
struct Case {
    std::string name;
    std::uint32_t w = 0, h = 0;
    std::int32_t x = 0, y = 0;
    sim::TileRect rect;
    Buffer heights, ground, raw;
    std::vector<std::uint16_t> gfx;
    std::vector<Cell> before, after;
    std::vector<std::uint32_t> grass, fish;
    std::uint64_t grass_count = 0, fish_count = 0;
};
std::vector<Case> cases() {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" /
                     "wildlife_postload_vectors.txt");
    REQUIRE(in);
    std::vector<Case> out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        const auto split = line.find("=>");
        REQUIRE(split != std::string::npos);
        std::istringstream left(line.substr(0, split)), right(line.substr(split + 2));
        std::string kind, name;
        left >> kind >> name;
        if (kind == "gfx")
            continue;
        REQUIRE(kind == "fill");
        const auto p = fields(left), e = fields(right);
        Case c;
        c.name = name;
        c.w = column<std::uint32_t>(p.at("w"))[0];
        c.h = column<std::uint32_t>(p.at("h"))[0];
        const auto origin = column<std::int32_t>(p.at("origin"));
        REQUIRE(origin.size() == 2);
        c.x = origin[0];
        c.y = origin[1];
        const auto rect = numbers(p.at("rect"));
        REQUIRE(rect.size() == 4);
        c.rect = {static_cast<std::int32_t>(rect[0]), static_cast<std::int32_t>(rect[1]),
                  static_cast<std::uint32_t>(rect[2]), static_cast<std::uint32_t>(rect[3])};
        c.heights = column<std::uint8_t>(p.at("height"));
        c.gfx = column<std::uint16_t>(p.at("gfx"));
        c.ground = column<std::uint8_t>(p.at("ground"));
        c.raw = column<std::uint8_t>(p.at("raw"));
        c.before = cells(p.at("cells"));
        c.after = cells(e.at("cells"));
        c.grass = column<std::uint32_t>(p.at("grass"));
        c.fish = column<std::uint32_t>(p.at("fish"));
        c.grass_count = column<std::uint32_t>(e.at("grass"))[0];
        c.fish_count = column<std::uint32_t>(e.at("fish"))[0];
        REQUIRE(p.at("height") == e.at("height"));
        REQUIRE(p.at("gfx") == e.at("gfx"));
        REQUIRE(p.at("ground") == e.at("ground"));
        REQUIRE(p.at("raw") == e.at("raw"));
        REQUIRE(p.at("draws") == e.at("draws"));
        REQUIRE(e.at("consumed") == "0");
        out.push_back(std::move(c));
    }
    REQUIRE(in.eof());
    REQUIRE(out.size() >= 19);
    return out;
}
struct Metadata {
    std::vector<Buffer> backing;
    std::vector<fmt::figuren::Entry> entries;
    std::shared_ptr<const sim::TypeRegistry> types;
    explicit Metadata(const Case& c) : backing(2) {
        sim::TypeRegistry::Builder builder;
        const std::array profiles{c.grass, c.fish};
        for (std::size_t i = 0; i < profiles.size(); ++i) {
            const auto& p = profiles[i];
            REQUIRE(p.size() == 4);
            auto& head = backing[i];
            head.assign(56, 0);
            head[0] = head[1] = 1;
            head[0x1c] = static_cast<std::uint8_t>(p[1]);
            head[0x1d] = static_cast<std::uint8_t>(p[1] >> 8);
            head[0x2f] = static_cast<std::uint8_t>(p[2]);
            head[0x30] = static_cast<std::uint8_t>(p[3]);
            fmt::figuren::Entry e;
            e.id = p[0];
            fmt::chunk::Chunk h;
            h.name_field = *fmt::chunk::name_field("HEAD");
            h.payload = fmt::Bytes(head).first(52);
            e.records.push_back(h);
            entries.push_back(e);
            sim::BuildingType t;
            t.id = sim::TypeId{p[0]};
            t.width = t.height = 1;
            t.prodtype = 225;
            t.flags_word = p[1];
            t.head_raw = head;
            builder.add(t);
        }
        const auto result = builder.build();
        REQUIRE_MESSAGE(result, (result ? "" : result.error()));
        types = result.value();
    }
};
Buffer packed(std::string_view name, const Buffer& bytes) {
    fmt::chunk::Chunk c;
    c.name_field = *fmt::chunk::name_field(name);
    c.payload = bytes;
    Buffer out;
    REQUIRE(fmt::chunk::write_chunk(c, out));
    return out;
}
void append(Buffer& a, const Buffer& b) {
    a.insert(a.end(), b.begin(), b.end());
}
Buffer block(std::uint32_t w, std::uint32_t h, const Buffer& payload) {
    REQUIRE(w <= 255);
    REQUIRE(h <= 255); // Authored single-block fixture, original block field width.
    Buffer b;
    fmt::put_u16_le(b, 0);
    fmt::put_u16_le(b, 0);
    b.push_back(static_cast<std::uint8_t>(w));
    b.push_back(static_cast<std::uint8_t>(h));
    append(b, payload);
    return b;
}
struct Fixture {
    Buffer scene;
    load::PreparedPrefix prefix;
    Fixture(const Case& c, bool with_ground = true) {
        Buffer b;
        fmt::put_u16_le(b, static_cast<std::uint16_t>(c.x));
        fmt::put_u16_le(b, static_cast<std::uint16_t>(c.y));
        fmt::put_u16_le(b, static_cast<std::uint16_t>(c.w));
        fmt::put_u16_le(b, static_cast<std::uint16_t>(c.h));
        fmt::put_u32_le(b, 9);
        append(b, packed("INFO", Buffer(12, 0)));
        append(b, packed("HEIGHT", block(c.w, c.h, c.heights)));
        Buffer gfx;
        for (const auto v : c.gfx)
            fmt::put_u16_le(gfx, v);
        append(b, packed("GFXID", block(c.w, c.h, gfx)));
        if (with_ground)
            append(b, packed("FRUCHTBAR", c.raw));
        const fmt::insel::HirschRecord spawn{0, 0, 0x80, 7, 10};
        append(b, packed("HIRSCH", fmt::insel::encode_hirsch(std::array{spawn})));
        scene = packed("ANNO", packed("SCENE", packed("INSEL5", b)));
        prefix.source_sha256 = fmt::sha256_hex(scene);
        prefix.complete_local_records = true;
        prefix.supplied.chunk_edge = 2;
        prefix.consumed = 37;
        load::PreparedIsland source;
        source.slot = 9;
        source.complete_local_records = true;
        source.terrain = sim::make_terrain(c.heights, c.gfx, c.raw);
        REQUIRE(source.resource.tiles.reset(c.x, c.y, c.w, c.h, 2));
        REQUIRE(c.before.size() == source.resource.tiles.tile_count());
        for (std::uint64_t i = 0; i < c.before.size(); ++i)
            write(source.resource.tiles, i, c.before[i]);
        for (std::uint64_t i = 0; i < source.resource.tiles.chunk_count(); ++i)
            static_cast<void>(source.resource.tiles.chunk_digest(i));
        source.resource.variation = c.ground;
        source.resource.consumed = 23;
        const auto islands = fmt::insel::islands(scene);
        REQUIRE(islands);
        const auto& records = islands.value()[0].records;
        for (std::uint64_t i = 0; i < records.size(); ++i) {
            load::PrefixHeader header;
            header.tag = std::string(records[i].name());
            header.source = {0, i, 0, records[i].offset, 9};
            if (header.tag == "FRUCHTBAR")
                header.fertility_updates = static_cast<std::uint64_t>(std::count_if(
                    c.raw.begin(), c.raw.end(), [](auto value) { return (value & 15U) != 0; }));
            if (header.tag == "HIRSCH")
                header.records = 1;
            source.headers.push_back(header);
        }
        source.spawns.push_back(
            {{0, records.size() - 1, 0, records.back().offset, 9}, spawn, 10, 8, 0, 36, {}});
        prefix.islands.push_back(std::move(source));
    }
};
load::WildlifeFillRoles roles(const Case& c) {
    return {sim::TypeId{c.grass[0]}, sim::TypeId{c.fish[0]}};
}
void equal(const load::PreparedIsland& a, const load::PreparedIsland& b) {
    REQUIRE(a.terrain);
    REQUIRE(b.terrain);
    CHECK(a.terrain->height == b.terrain->height);
    CHECK(a.terrain->gfx == b.terrain->gfx);
    CHECK(a.terrain->fertility == b.terrain->fertility);
    CHECK(a.terrain->digest == b.terrain->digest);
    CHECK(a.resource.variation == b.resource.variation);
    CHECK(a.resource.consumed == b.resource.consumed);
    CHECK(a.resource.tiles.tile_count() == b.resource.tiles.tile_count());
    for (std::uint64_t i = 0; i < a.resource.tiles.tile_count(); ++i)
        CHECK(read(a.resource.tiles, i) == read(b.resource.tiles, i));
    CHECK(a.headers.size() == b.headers.size());
    CHECK(a.spawns.size() == b.spawns.size());
    for (std::size_t i = 0; i < a.spawns.size(); ++i) {
        CHECK(a.spawns[i].raw == b.spawns[i].raw);
        CHECK(a.spawns[i].draw_before == b.spawns[i].draw_before);
        CHECK(a.spawns[i].timestamp == b.spawns[i].timestamp);
    }
}
std::uint64_t expected_digest(const std::vector<Cell>& expected, const sim::TileLayer& geometry,
                              std::uint64_t chunk) {
    // Independently stream literal expected fields in state.md §9.2 order.
    sim::StateHasher hash;
    const auto left = chunk % geometry.chunks_x() * geometry.chunk_edge();
    const auto top = chunk / geometry.chunks_x() * geometry.chunk_edge();
    for (auto y = top; y < std::min<std::uint64_t>(geometry.height(), top + geometry.chunk_edge());
         ++y)
        for (auto x = left;
             x < std::min<std::uint64_t>(geometry.width(), left + geometry.chunk_edge()); ++x) {
            const auto& c = expected[y * geometry.width() + x];
            hash.u32(c[0]);
            hash.u8(static_cast<std::uint8_t>(c[1]));
            hash.u32(c[2]);
            hash.u32(c[3]);
            for (std::size_t i = 4; i <= 7; ++i)
                hash.u16(static_cast<std::uint16_t>(c[i]));
            hash.u8(static_cast<std::uint8_t>(c[8]));
            hash.u8(static_cast<std::uint8_t>(c[9]));
        }
    return hash.value();
}
} // namespace

TEST_CASE("wildlife fill: hand vectors preserve full fields ownership and chunk digests") {
    for (const auto& c : cases()) {
        CAPTURE(c.name);
        Metadata meta(c);
        Fixture f(c);
        const auto original = f.prefix;
        const auto scene_before = f.scene;
        auto capability = load::prepare_wildlife_postload_fill(f.prefix, 0, f.scene, meta.entries,
                                                               *meta.types, roles(c));
        REQUIRE_MESSAGE(capability, (capability ? "" : capability.error()));
        const auto result =
            load::apply_wildlife_postload_fill(std::move(capability).value(), c.rect);
        REQUIRE(result);
        const auto& out = result.value();
        CHECK(out.grass == c.grass_count);
        CHECK(out.fish == c.fish_count);
        CHECK(out.consumed == 0);
        CHECK(out.prefix_consumed == 37);
        CHECK(out.island.resource.consumed == 23);
        REQUIRE(out.island.resource.tiles.tile_count() == c.after.size());
        for (std::uint64_t i = 0; i < c.after.size(); ++i)
            CHECK(read(out.island.resource.tiles, i) == c.after[i]);
        auto changed = out.island.resource.tiles;
        for (std::uint64_t k = 0; k < changed.chunk_count(); ++k) {
            CHECK(changed.chunk_digest(k) == expected_digest(c.after, changed, k));
            CHECK(changed.chunk_digest_uncached(k) == expected_digest(c.after, changed, k));
        }
        CHECK(changed.cache_consistent());
        CHECK(f.scene == scene_before);
        equal(f.prefix.islands[0], original.islands[0]);
        CHECK(f.prefix.islands[0].resource.tiles.dirty_chunks() == 0);
        CHECK(out.island.resource.variation == c.ground);
        CHECK(out.island.terrain->height == c.heights);
        CHECK(out.island.terrain->gfx == c.gfx);
        CHECK(out.island.terrain->fertility == c.raw);
        CHECK(out.island.terrain->digest == original.islands[0].terrain->digest);
        CHECK(out.island.spawns[0].raw == original.islands[0].spawns[0].raw);
        CHECK(out.island.spawns[0].draw_before == 36);
        auto repeated = load::prepare_wildlife_postload_fill(f.prefix, 0, f.scene, meta.entries,
                                                             *meta.types, roles(c));
        REQUIRE(repeated);
        auto again = load::apply_wildlife_postload_fill(std::move(repeated).value(), c.rect);
        REQUIRE(again);
        equal(out.island, again.value().island);
        f.prefix.islands[0] = out.island;
        auto idempotent = load::prepare_wildlife_postload_fill(f.prefix, 0, f.scene, meta.entries,
                                                               *meta.types, roles(c));
        REQUIRE(idempotent);
        auto twice = load::apply_wildlife_postload_fill(std::move(idempotent).value(), c.rect);
        REQUIRE(twice);
        CHECK(twice.value().fish == 0);
        CHECK(twice.value().grass == 0);
        equal(out.island, twice.value().island);
    }
}

TEST_CASE("wildlife fill: capability owns source and refuses moved reuse") {
    const auto c = cases()[0];
    auto capability = [&] {
        Metadata meta(c);
        Fixture f(c);
        auto prepared = load::prepare_wildlife_postload_fill(f.prefix, 0, f.scene, meta.entries,
                                                             *meta.types, roles(c));
        REQUIRE(prepared);
        // Mutate caller-owned objects after validation: the capability is a snapshot.
        f.prefix.islands[0].resource.tiles.set_object(0, 999);
        f.prefix.islands[0].resource.variation[0] = 0;
        f.prefix.islands[0].spawns.clear();
        f.prefix.islands[0].terrain = sim::make_terrain({}, {}, {});
        meta.entries.clear();
        f.scene.clear();
        return prepared;
    }(); // All source bytes, metadata, registry and prefix objects die here.
    REQUIRE(capability);
    auto result = load::apply_wildlife_postload_fill(std::move(capability).value(), c.rect);
    REQUIRE(result);
    CHECK(read(result.value().island.resource.tiles, 0) == c.after[0]);
    CHECK(sim::terrain_matches(*result.value().island.terrain));
    const auto moved = load::apply_wildlife_postload_fill(std::move(capability).value(), c.rect);
    REQUIRE_FALSE(moved);
    CHECK(moved.error() == "wildlife fill: capability already consumed");
}

TEST_CASE("wildlife fill: capability rejects inconsistent provenance and facts before effects") {
    const auto c = cases()[0];
    Metadata meta(c);
    Fixture baseline(c);
    auto control = load::prepare_wildlife_postload_fill(baseline.prefix, 0, baseline.scene,
                                                        meta.entries, *meta.types, roles(c));
    REQUIRE(control);
    auto legal = load::apply_wildlife_postload_fill(std::move(control).value(), c.rect);
    REQUIRE(legal);
    CHECK(legal.value().fish == 1);
    const auto reject = [&](const load::PreparedPrefix& prefix, fmt::Bytes scene,
                            std::span<const fmt::figuren::Entry> entries,
                            const sim::TypeRegistry& types, load::WildlifeFillRoles binding,
                            const std::string& why, std::uint64_t ordinal = 0) {
        const auto snapshot = prefix;
        const auto result =
            load::prepare_wildlife_postload_fill(prefix, ordinal, scene, entries, types, binding);
        REQUIRE_FALSE(result);
        CHECK(result.error() == "wildlife fill: " + why);
        equal(prefix.islands[0], snapshot.islands[0]);
        CHECK(prefix.consumed == snapshot.consumed);
    };
    const auto test = [&](const load::PreparedPrefix& prefix, const std::string& why) {
        reject(prefix, baseline.scene, meta.entries, *meta.types, roles(c), why);
    };
    auto p = baseline.prefix;
    p.complete_local_records = false;
    test(p, "complete local headers required");
    p = baseline.prefix;
    p.islands[0].complete_local_records = false;
    test(p, "complete local headers required");
    p = baseline.prefix;
    p.boundary = load::PrefixBoundary{};
    test(p, "complete local headers required");
    p = baseline.prefix;
    p.source_sha256 = "unrelated";
    test(p, "source scene identity mismatch");
    p = baseline.prefix;
    REQUIRE(p.islands[0].resource.tiles.reset(11, 20, c.w, c.h, 2));
    test(p, "source island geometry mismatch");
    p = baseline.prefix;
    p.islands[0].headers.pop_back();
    test(p, "header sequence mismatch");
    p = baseline.prefix;
    ++p.islands[0].headers[0].source.header_offset;
    test(p, "header provenance mismatch");
    p = baseline.prefix;
    p.islands[0].headers[3].fertility_updates.reset();
    test(p, "completed FRUCHTBAR required");
    p = baseline.prefix;
    p.islands[0].headers[0].fertility_updates = 0;
    test(p, "FRUCHTBAR completion on another header");
    p = baseline.prefix;
    p.islands[0].resource.variation[0] ^= 1;
    test(p, "effective ground low nibble mismatch");
    p = baseline.prefix;
    p.islands[0].resource.variation.pop_back();
    test(p, "terrain/ground plane shape mismatch");
    p = baseline.prefix;
    auto changed = std::make_shared<sim::Terrain>(*p.islands[0].terrain);
    changed->height[0] ^= 1;
    changed->digest = sim::terrain_digest(*changed);
    p.islands[0].terrain = changed;
    test(p, "source terrain provenance mismatch");
    p = baseline.prefix;
    changed = std::make_shared<sim::Terrain>(*p.islands[0].terrain);
    changed->fertility[0] ^= 0x10;
    changed->digest = sim::terrain_digest(*changed);
    p.islands[0].terrain = changed;
    test(p, "last FRUCHTBAR payload mismatch");
    p = baseline.prefix;
    changed = std::make_shared<sim::Terrain>(*p.islands[0].terrain);
    changed->digest ^= 1;
    p.islands[0].terrain = changed;
    test(p, "terrain digest mismatch");
    Fixture absent(c, false);
    reject(absent.prefix, absent.scene, meta.entries, *meta.types, roles(c),
           "completed FRUCHTBAR required");
    reject(baseline.prefix, baseline.scene, meta.entries, *sim::TypeRegistry::empty(), roles(c),
           "missing registry role");
    reject(baseline.prefix, baseline.scene, meta.entries, *meta.types,
           {sim::TypeId{0}, sim::TypeId{11}}, "distinct nonzero role ids required");
    p = baseline.prefix;
    changed = std::make_shared<sim::Terrain>(*p.islands[0].terrain);
    changed->gfx.pop_back();
    changed->digest = sim::terrain_digest(*changed);
    p.islands[0].terrain = changed;
    test(p, "terrain/ground plane shape mismatch");
    p = baseline.prefix;
    p.islands[0].terrain.reset();
    const auto no_terrain = load::prepare_wildlife_postload_fill(p, 0, baseline.scene, meta.entries,
                                                                 *meta.types, roles(c));
    REQUIRE_FALSE(no_terrain);
    CHECK(no_terrain.error() == "wildlife fill: terrain/ground plane shape mismatch");
    CHECK_FALSE(p.islands[0].terrain);
    auto entries = meta.entries;
    entries.erase(entries.begin());
    reject(baseline.prefix, baseline.scene, entries, *meta.types, roles(c), "missing role entry");
    entries = meta.entries;
    entries.push_back(entries[0]);
    reject(baseline.prefix, baseline.scene, entries, *meta.types, roles(c), "duplicate role entry");
    entries = meta.entries;
    entries[0].records.push_back(entries[0].records[0]);
    reject(baseline.prefix, baseline.scene, entries, *meta.types, roles(c),
           "unique role HEAD required");
    entries = meta.entries;
    entries[0].records[0].payload = fmt::Bytes(meta.backing[0]).first(29);
    reject(baseline.prefix, baseline.scene, entries, *meta.types, roles(c),
           "unsupported role HEAD length");
    Metadata mismatch(c);
    mismatch.backing[0][51] = 1;
    reject(baseline.prefix, baseline.scene, mismatch.entries, *mismatch.types, roles(c),
           "role HEAD/registry mismatch");
    auto unsupported = c;
    unsupported.grass[1] = 40;
    Metadata other(unsupported);
    reject(baseline.prefix, baseline.scene, other.entries, *other.types, roles(c),
           "unsupported role height profile");
    reject(baseline.prefix, baseline.scene, meta.entries, *meta.types,
           {sim::TypeId{2}, sim::TypeId{2}}, "distinct nonzero role ids required");
    reject(baseline.prefix, baseline.scene, meta.entries, *meta.types, roles(c),
           "complete local headers required", 1);
}

TEST_CASE("wildlife fill: completed zero ground and short HEAD retain proved defaults") {
    const auto all = cases();
    const auto zero = std::find_if(all.begin(), all.end(),
                                   [](const auto& c) { return c.name == "whole_island"; });
    REQUIRE(zero != all.end());
    Fixture f(*zero);
    Metadata meta(*zero);
    REQUIRE(f.prefix.islands[0].headers[3].fertility_updates.has_value());
    CHECK(f.prefix.islands[0].headers[3].fertility_updates == 0);
    auto prepared =
        load::prepare_wildlife_postload_fill(f.prefix, 0, f.scene, meta.entries, *meta.types);
    REQUIRE(prepared);
    const auto result = load::apply_wildlife_postload_fill(std::move(prepared).value(), zero->rect);
    REQUIRE(result);
    CHECK(result.value().fish == 6);
    const auto code4 = std::find_if(all.begin(), all.end(),
                                    [](const auto& c) { return c.name == "constructor_code4"; });
    REQUIRE(code4 != all.end());
    Fixture g(*code4);
    Metadata short_head(*code4);
    for (std::size_t i = 0; i < short_head.entries.size(); ++i) {
        // Both raw tails must really be zero to match the registry's zero-extended HEAD.
        short_head.backing[i][0x2f] = short_head.backing[i][0x30] = 0;
    }
    // Only the grass is shortened; the fish registry has first/shape 2/3.
    short_head.backing[1][0x2f] = 2;
    short_head.backing[1][0x30] = 3;
    short_head.entries[0].records[0].payload = fmt::Bytes(short_head.backing[0]).first(36);
    auto valid = load::prepare_wildlife_postload_fill(g.prefix, 0, g.scene, short_head.entries,
                                                      *short_head.types);
    REQUIRE_MESSAGE(valid, (valid ? "" : valid.error()));
    const auto filled = load::apply_wildlife_postload_fill(std::move(valid).value(), code4->rect);
    REQUIRE(filled);
    CHECK(filled.value().grass == 1);
    CHECK(filled.value().island.resource.tiles.walk_code(0) == 4);
}

TEST_CASE("wildlife first call: original Tutorial full fields under supplied prefix inputs") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const auto read_file = [](const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in);
        return Buffer(std::istreambuf_iterator<char>(in), {});
    };
    const auto scene = read_file(*dir / "Scenes/Tutorial/01Discover01.sze");
    REQUIRE(fmt::sha256_hex(scene) ==
            "225ed50b005743e00d8d3ffe18667d4565dc781ef45bd6d9b767fd2fe056c0f6");
    const auto original_anlagen = read_file(*dir / "Anlagen.dat");
    const auto anlagen = fmt::anlagen::AnlagenFile::parse(original_anlagen);
    REQUIRE(anlagen);
    const auto game = read_file(*dir / "AnnoGame.dll");
    const auto registry = load::load_original_registry(*dir, load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    const std::vector<std::uint32_t> draws(20000, 7); // Supplied diagnostic context, no generator.
    load::PrefixInputs supplied;
    supplied.sampled_stamps = {500};
    supplied.mode_bit = 0;
    supplied.stamp_provenance = "supplied diagnostic stamp 500, not MAIN";
    supplied.mode_provenance = "supplied query bit 0, not MAIN";
    const auto prepared = load::prepare_wildlife_prefix(scene, anlagen.value().entries, game,
                                                        *registry.types, supplied, draws);
    REQUIRE_MESSAGE(prepared, (prepared ? "" : prepared.error().reason));
    const auto& prefix = prepared.value();
    REQUIRE(prefix.complete_local_records);
    REQUIRE(prefix.islands.size() == 1);
    REQUIRE(prefix.consumed == 4745);
    const auto& before = prefix.islands[0];
    REQUIRE(before.resources.size() == 4760);
    for (const auto& record : before.resources)
        REQUIRE(record.effect.disposition == sim::resource_import::Disposition::Accepted);
    REQUIRE(before.spawns.size() == 156);
    const auto raw_islands = fmt::insel::islands(scene);
    REQUIRE(raw_islands);
    const auto& raw = raw_islands.value()[0];
    const auto heights = raw.heights();
    REQUIRE(heights);
    fmt::Bytes ground;
    for (const auto& record : raw.records)
        if (record.name() == "FRUCHTBAR")
            ground = record.payload;
    REQUIRE(ground.size() == std::uint64_t{raw.width} * raw.height);
    // Independent raw footprint union. Every source record is accepted in the
    // already verified prefix; overlays overwrite/skip nonzero cells, never clear.
    std::vector<std::uint8_t> occupied(ground.size(), 0);
    std::uint64_t raw_records = 0;
    for (const auto& record : raw.records) {
        if (record.name() != "GELAENDE")
            continue;
        const auto block = fmt::insel::Block::parse(record.payload);
        REQUIRE(block);
        REQUIRE(block.value().data.size() % 6 == 0);
        for (std::size_t at = 0; at < block.value().data.size(); at += 6) {
            const auto object = fmt::insel::decode_import_object(block.value().data.subspan(at, 6),
                                                                 block.value(), raw.x, raw.y);
            REQUIRE(object);
            const auto* type = registry.types->find(sim::TypeId{object.value().type});
            REQUIRE(type);
            const auto width = (object.value().rotation & 1U) ? type->height : type->width;
            const auto height = (object.value().rotation & 1U) ? type->width : type->height;
            const auto x = object.value().x - raw.x, y = object.value().y - raw.y;
            REQUIRE(x >= 0);
            REQUIRE(y >= 0);
            REQUIRE(std::uint64_t{static_cast<std::uint32_t>(x)} + width <= raw.width);
            REQUIRE(std::uint64_t{static_cast<std::uint32_t>(y)} + height <= raw.height);
            for (std::uint32_t dy = 0; dy < height; ++dy)
                for (std::uint32_t dx = 0; dx < width; ++dx)
                    occupied[(std::uint64_t{static_cast<std::uint32_t>(y)} + dy) * raw.width +
                             static_cast<std::uint32_t>(x) + dx] = 1;
            ++raw_records;
        }
    }
    REQUIRE(raw_records == 4760);
    CHECK(std::count(occupied.begin(), occupied.end(), 1) == 5001);
    const auto vertex = [&](std::uint32_t x, std::uint32_t y) -> std::uint32_t {
        return x < raw.width && y < raw.height ? heights.value()[std::uint64_t{y} * raw.width + x]
                                               : 0;
    };
    std::vector<Cell> expected;
    std::string action_rows;
    std::uint64_t fish = 0, grass = 0;
    for (std::uint32_t y = 0; y < raw.height; ++y)
        for (std::uint32_t x = 0; x < raw.width; ++x) {
            const auto i = std::uint64_t{y} * raw.width + x;
            REQUIRE((before.resource.tiles.object(i) != 0) == (occupied[i] != 0));
            auto cell = read(before.resource.tiles, i); // Prior prefix fields, not fill output.
            const auto mean =
                (vertex(x, y) + vertex(x + 1, y) + vertex(x, y + 1) + vertex(x + 1, y + 1)) / 4U;
            const std::uint32_t target = occupied[i]                                     ? 0
                                         : mean < 16                                     ? 11
                                         : (mean >= 32 && (ground[i] & 15U) * 10U > 50U) ? 2
                                                                                         : 0;
            if (target != 0) {
                action_rows += std::to_string(i) + "," + std::to_string(target) + "," +
                               std::to_string(mean) + "\n";
                cell[0] = target;
                cell[1] = 0;
                cell[3] = (cell[3] & (target == 2 ? 0xfe000000U : 0xffffff00U)) | mean;
                if (target == 2) {
                    cell[4] = 2; // Independently parsed original role HEAD, not computed output.
                    cell[5] = 0;
                    ++grass;
                } else {
                    ++fish;
                }
                cell[6] = cell[7] = cell[8] = 0;
            }
            expected.push_back(cell);
        }
    // Counts/action hash established privately from raw files by independent Python
    // before this canary; this is not a recorded world/state hash or a moved pin.
    REQUIRE(fish == 8486);
    REQUIRE(grass == 1559);
    const Buffer action_bytes(action_rows.begin(), action_rows.end());
    REQUIRE(fmt::sha256_hex(action_bytes) ==
            "a66cff880b8ca148261ba7b61a9e288d9f4a4a90717546014386c4c2f0971487");
    const auto snapshot = prefix;
    auto capability = load::prepare_wildlife_postload_fill(
        prefix, 0, scene, anlagen.value().entries, *registry.types);
    REQUIRE_MESSAGE(capability, (capability ? "" : capability.error()));
    const auto result = load::apply_wildlife_postload_fill(
        std::move(capability).value(), sim::TileRect{raw.x, raw.y, raw.width, raw.height});
    REQUIRE(result);
    CHECK(result.value().fish == fish);
    CHECK(result.value().grass == grass);
    CHECK(result.value().consumed == 0);
    CHECK(result.value().prefix_consumed == 4745);
    auto out = result.value().island.resource.tiles;
    for (std::uint64_t i = 0; i < out.tile_count(); ++i)
        CHECK(read(out, i) == expected[i]);
    for (std::uint64_t k = 0; k < out.chunk_count(); ++k)
        CHECK(out.chunk_digest(k) == expected_digest(expected, out, k));
    CHECK(out.cache_consistent());
    equal(prefix.islands[0], snapshot.islands[0]);
    CHECK(result.value().island.resource.variation == before.resource.variation);
    CHECK(result.value().island.terrain->digest == before.terrain->digest);
    CHECK(result.value().island.spawns.size() == 156);
    CHECK(prefix.consumed == 4745);
    const auto composed = load::prepare_wildlife_first_call(scene, {0, 11}, anlagen.value().entries,
                                                            game, *registry.types, supplied, draws);
    REQUIRE_MESSAGE(composed, (composed ? "" : composed.error().reason));
    CHECK(composed.value().source_sha256 == fmt::sha256_hex(scene));
    CHECK(composed.value().ordinal == 0);
    CHECK(composed.value().supplied.sampled_stamps == supplied.sampled_stamps);
    CHECK(composed.value().supplied.mode_provenance == supplied.mode_provenance);
    CHECK(composed.value().fill.fish == 8486);
    CHECK(composed.value().fill.grass == 1559);
    CHECK(composed.value().fill.prefix_consumed == 4745);
    CHECK(composed.value().fill.consumed == 0);
    auto composed_tiles = composed.value().fill.island.resource.tiles;
    for (std::uint64_t i = 0; i < composed_tiles.tile_count(); ++i)
        CHECK(read(composed_tiles, i) == expected[i]);
    for (std::uint64_t k = 0; k < composed_tiles.chunk_count(); ++k)
        CHECK(composed_tiles.chunk_digest(k) == expected_digest(expected, composed_tiles, k));
    CHECK(composed_tiles.cache_consistent());
    equal(composed.value().fill.island, result.value().island);
    CHECK(std::all_of(draws.begin(), draws.end(), [](auto v) { return v == 7; }));
    std::cout << "FIRST_CALL fish=" << fish << " grass=" << grass
              << " unchanged=" << out.tile_count() - fish - grass
              << " prefix_consumed=4745 new_draws=0 "
              << "scope=source_prefix_first_fill_only original_runtime=NOT_RUN\n";
}
