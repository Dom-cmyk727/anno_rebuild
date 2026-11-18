#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/annogame.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/wildlife_prefix.hpp>
#include <anno/test/game_dir.hpp>
namespace fmt = anno::formats;
namespace sim = anno::sim;
namespace load = anno::sim_load;
namespace {
using Buffer = fmt::ByteBuffer;
void put16(Buffer& b, std::size_t at, std::uint16_t v) {
    b.at(at) = static_cast<std::uint8_t>(v);
    b.at(at + 1) = static_cast<std::uint8_t>(v >> 8);
}
void put32(Buffer& b, std::size_t at, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        b.at(at + i) = static_cast<std::uint8_t>(v >> (8U * i));
}
Buffer packed(std::string_view name, const Buffer& bytes) {
    fmt::chunk::Chunk c;
    const auto field = fmt::chunk::name_field(name);
    REQUIRE(field);
    c.name_field = *field;
    c.payload = bytes;
    Buffer out;
    REQUIRE(fmt::chunk::write_chunk(c, out));
    return out;
}
void append(Buffer& out, const Buffer& bytes) {
    out.insert(out.end(), bytes.begin(), bytes.end());
}
Buffer fake_pe() {
    // Authored PE/data only; profile constants and RVA geometry, no original bytes/table.
    Buffer b(0x600, 0);
    b.at(0) = 'M';
    b.at(1) = 'Z';
    put32(b, 0x3c, 0x40);
    b.at(0x40) = 'P';
    b.at(0x41) = 'E';
    put16(b, 0x44, 0x14c);
    put16(b, 0x46, 1);
    put32(b, 0x48, 0x44c0c30f);
    put16(b, 0x54, 0xe0);
    put16(b, 0x58, 0x10b);
    put32(b, 0x58 + 0x1c, 0x10000000);
    put32(b, 0x58 + 0x38, 0x2e000);
    const auto sec = 0x58U + 0xe0U;
    put32(b, sec + 12, 0x21000);
    put32(b, sec + 16, 0x400);
    put32(b, sec + 20, 0x200);
    for (unsigned climate = 0; climate < 7; ++climate) {
        for (unsigned i = 0; i < 16; ++i)
            put16(b, 0x200 + 0x14c + climate * 32 + i * 2,
                  static_cast<std::uint16_t>(100 + climate * 16 + i));
        for (unsigned base : {0x22cU, 0x29cU, 0x30cU})
            for (unsigned i = 0; i < 8; ++i)
                put16(b, 0x200 + base + climate * 16 + i * 2,
                      static_cast<std::uint16_t>((base == 0x29c   ? 100U
                                                  : base == 0x30c ? 200U
                                                                  : 300U) +
                                                 climate * 16 + i));
    }
    return b;
}
struct Metadata {
    std::vector<Buffer> storage;
    std::vector<fmt::figuren::Entry> entries;
    Metadata(std::uint32_t id = 104, std::string ware = "BAUM", std::size_t head_size = 28) {
        Buffer head(head_size, 0);
        if (head_size >= 28)
            put16(head, 26, 104);
        Buffer w(ware.begin(), ware.end());
        w.push_back(0);
        storage.push_back(std::move(head));
        storage.push_back(std::move(w));
        fmt::figuren::Entry e;
        e.id = id;
        fmt::chunk::Chunk h;
        h.name_field = *fmt::chunk::name_field("HEAD");
        h.payload = storage[0];
        fmt::chunk::Chunk a;
        a.name_field = *fmt::chunk::name_field("WARE");
        a.payload = storage[1];
        e.records = {h, a};
        entries.push_back(e);
    }
};
std::shared_ptr<const sim::TypeRegistry> registry(std::uint32_t id = 104) {
    sim::BuildingType t;
    t.id = sim::TypeId{id};
    t.prodtype = 225;
    t.flags_word = 0x29;
    sim::TypeRegistry::Builder b;
    b.add(t);
    sim::BuildingType ordinary;
    ordinary.id = sim::TypeId{800};
    ordinary.prodtype = 291;
    ordinary.flags_word = 9;
    b.add(ordinary);
    const auto built = b.build();
    REQUIRE(built);
    return built.value();
}
Buffer block(std::uint16_t x, std::uint16_t y, std::uint8_t w, std::uint8_t h, Buffer data) {
    Buffer b;
    fmt::put_u16_le(b, x);
    fmt::put_u16_le(b, y);
    b.push_back(w);
    b.push_back(h);
    append(b, data);
    return b;
}
Buffer object(std::uint16_t type, std::uint16_t state = 0x507) {
    Buffer b;
    fmt::put_u16_le(b, type);
    fmt::put_u16_le(b, state);
    b.push_back(255);
    b.push_back(0);
    return b;
}
Buffer island(std::uint32_t slot, std::uint16_t type = 104, std::string extra = "",
              bool hirsch_only = false) {
    Buffer b;
    fmt::put_u16_le(b, 0);
    fmt::put_u16_le(b, 0);
    fmt::put_u16_le(b, 16);
    fmt::put_u16_le(b, 2);
    fmt::put_u32_le(b, slot);
    append(b, packed("INFO", Buffer(12, 0)));
    append(b, packed("HEIGHT", block(0, 0, 16, 2, Buffer(32, 48))));
    append(b, packed("HIRSCH", {})); // Present-empty, distinct source identity.
    if (!hirsch_only)
        append(b, packed("GELAENDE", block(1, 0, 1, 1, object(type)))); // signed -1 -> x 0
    append(b, packed("HIRSCH", fmt::insel::encode_hirsch(
                                   std::vector<fmt::insel::HirschRecord>{{0, 0, 0x80, 7, 10}})));
    if (!extra.empty())
        append(b, packed(extra, {}));
    if (!hirsch_only)
        append(b,
               packed("GELAENDE", block(1, 0, 1, 1, object(type, 0x709)))); // Duplicate replaces.
    append(b, packed("HIRSCH", fmt::insel::encode_hirsch(std::vector<fmt::insel::HirschRecord>{
                                   {8, 0, 0, 9, 20}, {1, 0, 1, 3, 30}})));
    append(b, packed("HIRSCH", {}));
    return packed("INSEL5", b);
}
Buffer scene(const std::vector<Buffer>& islands) {
    Buffer body;
    for (const auto& b : islands)
        append(body, b);
    return packed("ANNO", packed("SCENE", body));
}
Buffer change_header(const Buffer& file, std::string_view tag, std::uint64_t wanted,
                     const Buffer& replacement, bool drop_all = false) {
    const auto parsed = fmt::insel::islands(file);
    REQUIRE(parsed);
    std::vector<Buffer> islands;
    for (const auto& raw : parsed.value()) {
        Buffer body(raw.head.begin(), raw.head.end());
        std::uint64_t h = 0;
        for (const auto& record : raw.records) {
            const auto name = record.name();
            if (!(drop_all && name == tag)) {
                Buffer bytes(record.payload.begin(), record.payload.end());
                if (name == tag && h == wanted)
                    bytes = replacement;
                append(body, packed(name, bytes));
            }
            ++h;
        }
        islands.push_back(packed("INSEL5", body));
    }
    return scene(islands);
}
load::PrefixInputs inputs(std::vector<std::uint16_t> stamps = {500}, std::uint8_t mode = 0) {
    return {std::move(stamps), mode, "supplied synthetic/raw-sample input",
            "supplied mode query bit", 8};
}
Buffer read_file(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    REQUIRE(f.good());
    return {std::istreambuf_iterator<char>(f), {}};
}
} // namespace

TEST_CASE("wildlife prefix: raw fields keep high ids signed offsets and checked narrowing") {
    Buffer bytes;
    fmt::put_u16_le(bytes, 0xf123);
    fmt::put_u16_le(bytes, 0xabcd);
    bytes.push_back(255);
    bytes.push_back(128);
    fmt::insel::Block b{5, 7, 1, 1, {}};
    const auto parsed = fmt::insel::decode_import_object(bytes, b, 10, 200);
    REQUIRE(parsed);
    CHECK(parsed.value().type == 0x3123);
    CHECK(parsed.value().rotation == 3);
    CHECK(parsed.value().x == 14);
    CHECK(parsed.value().y == 79);
    CHECK(parsed.value().state == 0xabcd);
    const auto legacy = fmt::insel::PlacedObject::parse(bytes, b);
    CHECK(legacy.anlage == 0x123);
    CHECK(legacy.reserved == 3);
    CHECK(legacy.x == 260);
    CHECK(legacy.y == 135);
    CHECK_FALSE(fmt::insel::decode_import_object(std::span(bytes).first(5), b, 10, 200));
    CHECK_FALSE(
        fmt::insel::decode_import_object(bytes, b, std::numeric_limits<std::int32_t>::max(), 200));
    const auto types = registry(0x3123);
    Metadata meta(0x3123);
    const auto file = scene({island(7, 0x3123)});
    const auto pe = fake_pe();
    const auto full = load::prepare_wildlife_prefix(file, meta.entries, pe, *types, inputs(),
                                                    std::vector<std::uint32_t>{4, 7, 2, 31});
    REQUIRE(full);
    REQUIRE(full.value().complete_local_records);
    CHECK(full.value().islands[0].resource.tiles.object(0) == 0x3123);
}

TEST_CASE("wildlife prefix: serial interleaving stamps modes repeated headers and prepend order") {
    const auto types = registry();
    Metadata meta;
    const auto pe = fake_pe();
    const auto file = scene({island(7)});
    for (const std::uint16_t stamp : {std::uint16_t{499}, std::uint16_t{500}, std::uint16_t{65535}})
        for (const std::uint8_t mode : {std::uint8_t{0}, std::uint8_t{1}}) {
            const auto result =
                load::prepare_wildlife_prefix(file, meta.entries, pe, *types, inputs({stamp}, mode),
                                              std::vector<std::uint32_t>{4, 7, 2, 31});
            REQUIRE(result);
            REQUIRE(result.value().complete_local_records);
            REQUIRE_FALSE(result.value().boundary);
            const auto& out = result.value();
            const auto& is = out.islands[0];
            CHECK(out.consumed == 4);
            REQUIRE(is.resources.size() == 2);
            REQUIRE(is.spawns.size() == 3);
            CHECK(is.resources[0].effect.variant == 4);
            CHECK(is.resources[0].draw_before == 0);
            CHECK(is.resources[0].effect.height == 48);
            CHECK(is.resources[1].effect.variant == 2);
            CHECK(is.resources[1].draw_before == 2);
            // Hand-derived: /500 bases 0,1,131. Draw 4 is resource selection, NEXT draw 7 is first
            // jitter. Resources-first would give first timestamp base+2; omitted advance gives
            // base+4, never base+7.
            const std::uint8_t base = stamp == 499 ? 0 : stamp == 500 ? 1 : 131;
            CHECK(is.spawns[0].timestamp == base + 7);
            CHECK(is.spawns[1].timestamp == base + 31);
            CHECK(is.spawns[2].timestamp == base);
            CHECK(is.spawns[0].goods == (mode ? 255 : 10));
            CHECK(is.spawns[1].goods == (mode ? 255 : 20));
            CHECK(is.spawns[2].goods == (mode ? 255 : 30));
            CHECK(is.spawns[0].raw.flags == 0x80);
            CHECK(is.spawns[0].raw.raw5 == 7);
            CHECK(is.resource.variation[0] == 32);
            CHECK(is.resource.tiles.phase(0) == 9);
            CHECK(is.resource.tiles.item_height(0) == 48);
            CHECK(is.headers[2].tag == "HIRSCH");
            CHECK(is.headers[2].records == 0);
            CHECK(is.headers.back().records == 0);
            CHECK(is.spawns[0].source.header == 4);
            CHECK(is.spawns[1].source.header == 6);
            CHECK(is.spawns[2].source.record == 1);
            CHECK(is.chunk_heads[0] == 2);
            CHECK(is.spawns[2].next_in_chunk == 0);
            CHECK_FALSE(is.spawns[0].next_in_chunk);
            CHECK(is.chunk_heads[1] == 1);
            std::cout << "PREFIX_INTERLEAVE supplied_stamp=" << stamp << " mode=" << unsigned(mode)
                      << " first_timestamp=" << unsigned(is.spawns[0].timestamp)
                      << " consumed=" << out.consumed << "\n";
        }
    const auto ordered = load::prepare_wildlife_prefix(
        scene({island(7), island(2)}), meta.entries, pe, *types, inputs({500, 499}),
        std::vector<std::uint32_t>{4, 7, 2, 31, 4, 7, 2, 31});
    REQUIRE(ordered);
    REQUIRE(ordered.value().complete_local_records);
    CHECK(ordered.value().islands[0].slot == 7);
    CHECK(ordered.value().islands[1].slot == 2);
    CHECK(ordered.value().islands[1].spawns[0].timestamp == 7);
    CHECK(ordered.value().islands[1].spawns[0].source.island == 1);
}

TEST_CASE("wildlife prefix: empty GELAENDE header executes its clipped mask callback") {
    const auto types = registry();
    Metadata metadata;
    Buffer body;
    fmt::put_u16_le(body, 0);
    fmt::put_u16_le(body, 0);
    fmt::put_u16_le(body, 2);
    fmt::put_u16_le(body, 2);
    fmt::put_u32_le(body, 7);
    append(body, packed("INFO", Buffer(12, 0)));
    append(body, packed("HEIGHT", block(0, 0, 2, 2, Buffer(4, 0))));
    append(body, packed("GELAENDE", block(1, 1, 9, 9, {}))); // Empty items, clipped to last cell.
    append(body, packed("GELAENDE", block(1, 1, 9, 9, {}))); // Repeated OR changes nothing further.
    const auto result = load::prepare_wildlife_prefix(
        scene({packed("INSEL5", body)}), metadata.entries, fake_pe(), *types, inputs(), {});
    REQUIRE(result);
    REQUIRE(result.value().complete_local_records);
    const auto& is = result.value().islands[0];
    CHECK(is.headers[2].records == 0);
    REQUIRE(is.headers[2].water_mask_updates);
    CHECK(*is.headers[2].water_mask_updates == 1);
    REQUIRE(is.headers[3].water_mask_updates);
    CHECK(*is.headers[3].water_mask_updates == 0);
    CHECK(is.resource.tiles.walk_mask(0) == 0);
    CHECK(is.resource.tiles.walk_mask(3) == 511);
    CHECK(is.resource.tiles.object(3) == 0);
    CHECK(is.resource.tiles.walk_code(3) == 2);
    CHECK(result.value().consumed == 0);
    CHECK(is.spawns.empty());
    CHECK(is.resources.empty());
}

TEST_CASE("wildlife prefix: ordinary and rejected objects preserve the jitter cursor") {
    const auto types = registry();
    Metadata resource, ordinary(800, "UNUSED");
    auto entries = resource.entries;
    entries.insert(entries.end(), ordinary.entries.begin(), ordinary.entries.end());
    const auto file =
        change_header(scene({island(7)}), "GELAENDE", 3, block(1, 0, 1, 1, object(800)));
    const auto result = load::prepare_wildlife_prefix(file, entries, fake_pe(), *types, inputs(),
                                                      std::vector<std::uint32_t>{4, 7});
    REQUIRE(result);
    REQUIRE(result.value().complete_local_records);
    const auto& out = result.value();
    const auto& is = out.islands[0];
    REQUIRE(is.resources.size() == 2);
    REQUIRE(is.spawns.size() == 3);
    CHECK(is.resources[0].effect.disposition == sim::resource_import::Disposition::Accepted);
    CHECK(is.resources[1].effect.disposition == sim::resource_import::Disposition::Occupied);
    CHECK(is.resources[0].effect.draws == 0);
    CHECK(is.resources[1].effect.draws == 0);
    CHECK_FALSE(is.resources[0].effect.variant);
    CHECK_FALSE(is.resources[1].effect.variant);
    CHECK(is.resource.tiles.object(0) == 800);
    CHECK(is.resource.tiles.phase(0) == 7);
    CHECK(is.spawns[0].timestamp == 5);
    CHECK(is.spawns[1].timestamp == 8);
    CHECK(out.consumed == 2);
}

TEST_CASE(
    "wildlife prefix: adapter contract refuses malformed prerequisites and incomplete prefixes") {
    const auto types = registry();
    const auto pe = fake_pe();
    const auto file = scene({island(7)});
    const std::vector<std::uint32_t> draws{4, 7, 2, 31};
    Metadata baseline;
    const auto valid =
        load::prepare_wildlife_prefix(file, baseline.entries, pe, *types, inputs(), draws);
    REQUIRE(valid);
    REQUIRE(valid.value().complete_local_records);
    Metadata unused(104, "UNUSED");
    const auto explicit_zero = load::resource_metadata(unused.entries, *types);
    REQUIRE(explicit_zero);
    CHECK(explicit_zero.value()[0].goods0 == 0);
    Metadata missing(104, "UNUSED");
    missing.entries[0].records.pop_back();
    CHECK_FALSE(load::resource_metadata(missing.entries, *types));
    Metadata unknown(104, "NOT_A_GOOD");
    CHECK_FALSE(load::resource_metadata(unknown.entries, *types));
    Metadata truncated(104, "UNUSED", 27);
    CHECK_FALSE(load::resource_metadata(truncated.entries, *types));
    Metadata unterminated;
    unterminated.storage[1].back() = 'X';
    CHECK_FALSE(load::resource_metadata(unterminated.entries, *types));
    auto bad_pe = pe;
    bad_pe.at(0x48) ^= 1;
    CHECK_FALSE(
        load::prepare_wildlife_prefix(file, baseline.entries, bad_pe, *types, inputs(), draws));
    bad_pe = pe;
    bad_pe.resize(0x200 + 0x14c + 31);
    CHECK_FALSE(fmt::annogame::read_resource_tables(bad_pe, 0));
    const auto last_rows = fmt::annogame::read_resource_tables(pe, 6);
    REQUIRE(last_rows);
    CHECK(last_rows.value().slots.front() == 196);
    CHECK(last_rows.value().slots.back() == 211);
    CHECK(last_rows.value().tag104.back() == 203);
    CHECK(last_rows.value().tag201.front() == 296);
    CHECK(last_rows.value().tag404.back() == 403);
    bad_pe = pe;
    put32(bad_pe, 0x58 + 0xe0 + 12, 0x22000);
    CHECK_FALSE(fmt::annogame::read_resource_tables(bad_pe, 0));
    CHECK_FALSE(fmt::annogame::read_resource_tables(pe, 7));
    auto provenance = inputs();
    provenance.stamp_provenance.clear();
    CHECK_FALSE(
        load::prepare_wildlife_prefix(file, baseline.entries, pe, *types, provenance, draws));
    const auto stopped = load::prepare_wildlife_prefix(
        scene({island(7, 104, "MYSTERY")}), baseline.entries, pe, *types, inputs(), draws);
    REQUIRE_FALSE(stopped);
    REQUIRE(stopped.error().diagnostic);
    REQUIRE_FALSE((*stopped.error().diagnostic).complete_local_records);
    REQUIRE((*stopped.error().diagnostic).boundary);
    CHECK((*stopped.error().diagnostic).boundary->source.header == 5);
    CHECK((*stopped.error().diagnostic).boundary->tag == "MYSTERY");
    CHECK((*stopped.error().diagnostic).consumed == 2);
    CHECK((*stopped.error().diagnostic).islands[0].spawns.size() == 1);
    const auto exhausted = load::prepare_wildlife_prefix(file, baseline.entries, pe, *types,
                                                         inputs(), std::vector<std::uint32_t>{4});
    REQUIRE_FALSE(exhausted);
    REQUIRE(exhausted.error().diagnostic);
    CHECK_FALSE((*exhausted.error().diagnostic).complete_local_records);
    REQUIRE((*exhausted.error().diagnostic).boundary);
    CHECK((*exhausted.error().diagnostic).boundary->tag == "HIRSCH");
    CHECK((*exhausted.error().diagnostic).boundary->source.header == 4);
    CHECK((*exhausted.error().diagnostic).consumed == 1);
    Metadata repeated;
    repeated.entries[0].records.push_back(repeated.entries[0].records.back());
    CHECK_FALSE(load::resource_metadata(repeated.entries, *types));
    const auto absent =
        load::prepare_wildlife_prefix(change_header(file, "HIRSCH", 0, {}, true), baseline.entries,
                                      pe, *types, inputs(), std::vector<std::uint32_t>{4, 2});
    REQUIRE(absent);
    REQUIRE(absent.value().complete_local_records);
    CHECK(absent.value().islands[0].spawns.empty());
    CHECK(absent.value().islands[0].headers.size() == 4);
    CHECK(absent.value().consumed == 2);
    const auto bad_hirsch =
        load::prepare_wildlife_prefix(change_header(file, "HIRSCH", 2, Buffer(7, 0)),
                                      baseline.entries, pe, *types, inputs(), draws);
    REQUIRE_FALSE(bad_hirsch);
    REQUIRE(bad_hirsch.error().diagnostic);
    CHECK_FALSE((*bad_hirsch.error().diagnostic).complete_local_records);
    REQUIRE((*bad_hirsch.error().diagnostic).boundary);
    CHECK((*bad_hirsch.error().diagnostic).boundary->source.header == 2);
    CHECK((*bad_hirsch.error().diagnostic).consumed == 0);
    auto bad_object = block(1, 0, 1, 1, object(104));
    bad_object.push_back(1);
    const auto bad_block =
        load::prepare_wildlife_prefix(change_header(file, "GELAENDE", 3, bad_object),
                                      baseline.entries, pe, *types, inputs(), draws);
    REQUIRE_FALSE(bad_block);
    REQUIRE(bad_block.error().diagnostic);
    CHECK_FALSE((*bad_block.error().diagnostic).complete_local_records);
    REQUIRE((*bad_block.error().diagnostic).boundary);
    CHECK((*bad_block.error().diagnostic).boundary->source.header == 3);
    CHECK((*bad_block.error().diagnostic).consumed == 0);
    const auto outside_spawn = load::prepare_wildlife_prefix(
        change_header(
            file, "HIRSCH", 4,
            fmt::insel::encode_hirsch(std::vector<fmt::insel::HirschRecord>{{16, 0, 0, 0, 10}})),
        baseline.entries, pe, *types, inputs(), draws);
    REQUIRE_FALSE(outside_spawn);
    REQUIRE(outside_spawn.error().diagnostic);
    CHECK_FALSE((*outside_spawn.error().diagnostic).complete_local_records);
    REQUIRE((*outside_spawn.error().diagnostic).boundary);
    CHECK((*outside_spawn.error().diagnostic).boundary->source.header == 4);
    CHECK((*outside_spawn.error().diagnostic).consumed == 1);
    const auto reused = load::prepare_wildlife_prefix(
        scene({island(7), island(7)}), baseline.entries, pe, *types, inputs({500, 500}),
        std::vector<std::uint32_t>{4, 7, 2, 31, 4, 7, 2, 31});
    REQUIRE_FALSE(reused);
    REQUIRE(reused.error().diagnostic);
    CHECK_FALSE((*reused.error().diagnostic).complete_local_records);
    REQUIRE((*reused.error().diagnostic).boundary);
    CHECK((*reused.error().diagnostic).boundary->source.island == 1);
}

namespace {
Buffer fertility_island(const Buffer& first, const std::optional<Buffer>& second,
                        bool occupied = true, bool stop_after = false) {
    Buffer b;
    for (const auto v : std::vector<std::uint16_t>{7, 11, 3, 2})
        fmt::put_u16_le(b, v);
    fmt::put_u32_le(b, 17);
    append(b, packed("INFO", Buffer(12, 0)));
    append(b, packed("HEIGHT", block(0, 0, 3, 2, Buffer(6, occupied ? 48 : 0))));
    if (occupied)
        append(b, packed("GELAENDE", block(1, 0, 1, 1, object(104))));
    append(b, packed("FRUCHTBAR", first));
    if (second) {
        if (occupied)
            append(b, packed("GELAENDE", block(1, 0, 1, 1, object(104, 0x709))));
        append(b, packed("FRUCHTBAR", *second));
    }
    if (occupied)
        append(b, packed("HIRSCH", fmt::insel::encode_hirsch(std::vector<fmt::insel::HirschRecord>{
                                       {0, 0, 0x80, 7, 10}})));
    if (stop_after)
        append(b, packed("MYSTERY", {}));
    return packed("INSEL5", b);
}
} // namespace

TEST_CASE("wildlife prefix: fertility nibble chronology owns raw terrain and preserves resource "
          "high bits") {
    const auto types = registry();
    Metadata meta;
    const auto pe = fake_pe();
    const Buffer f1{0, 17, 2, 255, 128, 15}, f2{255, 7, 128, 16, 254, 18};
    auto file = scene({fertility_island(f1, f2)});
    const auto before = file;
    const auto result = load::prepare_wildlife_prefix(file, meta.entries, pe, *types, inputs(),
                                                      std::vector<std::uint32_t>{4, 2, 7});
    REQUIRE(result);
    CHECK(file == before);
    file.clear();
    file.shrink_to_fit();
    const auto& out = result.value();
    const auto& is = out.islands[0];
    REQUIRE(is.headers.size() == 7);
    CHECK(is.headers[3].fertility_updates == 4);
    CHECK(is.headers[5].fertility_updates == 6);
    CHECK_FALSE(is.headers[4].fertility_updates);
    CHECK(is.headers[3].source.header == 3);
    CHECK(is.headers[5].source.header == 5);
    CHECK(is.resource.variation == Buffer{47, 7, 0, 0, 14, 2});
    CHECK(is.terrain->fertility == f2);
    const auto expected = sim::make_terrain(Buffer(6, 48), std::vector<std::uint16_t>(6, 0), f2);
    CHECK(is.terrain->digest == expected->digest);
    CHECK(sim::terrain_matches(*is.terrain));
    CHECK(is.terrain->digest !=
          sim::make_terrain(Buffer(6, 48), std::vector<std::uint16_t>(6, 0), {})->digest);
    REQUIRE(is.resources.size() == 2);
    CHECK(is.resources[1].draw_before == 1);
    CHECK(is.resources[1].effect.variant == 2);
    CHECK(is.resources[1].effect.height == 48); // Borrowed HEIGHT remains valid after first header.
    CHECK(is.resource.tiles.object(0) == 104);
    CHECK(is.resource.tiles.phase(0) == 9);
    CHECK(is.resource.tiles.item_height(0) == 48);
    CHECK(is.resource.tiles.foot_anchor(0) == 1);
    for (std::uint64_t i = 1; i < 6; ++i)
        CHECK(is.resource.tiles.object(i) == 0);
    REQUIRE(is.spawns.size() == 1);
    CHECK(is.spawns[0].draw_before == 2);
    CHECK(is.spawns[0].timestamp == 8);
    CHECK(out.consumed == 3);
    // First payload alone proves a different high nibble and local row-major values.
    const auto one =
        load::prepare_wildlife_prefix(scene({fertility_island(f1, {})}), meta.entries, pe, *types,
                                      inputs(), std::vector<std::uint32_t>{4, 7});
    REQUIRE(one);
    CHECK(one.value().islands[0].resource.variation == Buffer{64, 1, 2, 15, 0, 15});
}

TEST_CASE("wildlife prefix: fertility adapter contract refuses lengths and preserves prior "
          "diagnostic effects") {
    const auto types = registry();
    Metadata meta;
    const auto pe = fake_pe();
    const Buffer f1{255, 7, 128, 16, 254, 18};
    for (const std::size_t n : {0U, 5U, 7U}) {
        const auto result = load::prepare_wildlife_prefix(
            scene({fertility_island(f1, Buffer(n, 3))}), meta.entries, pe, *types, inputs(),
            std::vector<std::uint32_t>{4, 2, 7});
        REQUIRE_FALSE(result);
        REQUIRE(result.error().diagnostic);
        const auto& out = *result.error().diagnostic;
        REQUIRE(out.boundary);
        CHECK(out.boundary->tag == "FRUCHTBAR");
        CHECK(out.boundary->source.header == 5);
        CHECK_FALSE(out.complete_local_records);
        const auto& is = out.islands[0];
        CHECK_FALSE(is.headers.back().fertility_updates);
        CHECK(is.headers[3].fertility_updates == 4);
        CHECK(is.resource.variation == Buffer{47, 7, 0, 0, 14, 2});
        CHECK(is.terrain->fertility == f1);
        CHECK(sim::terrain_matches(*is.terrain));
        CHECK(out.consumed == 2);
        CHECK(is.spawns.empty());
    }
    // Existing TileLayer overflow bound is checked before any map-sized allocation.
    Buffer huge;
    for (const auto v : std::vector<std::uint16_t>{0, 0, 65535, 65535})
        fmt::put_u16_le(huge, v);
    fmt::put_u32_le(huge, 1);
    append(huge, packed("INFO", Buffer(12, 0)));
    append(huge, packed("FRUCHTBAR", {}));
    const auto result = load::prepare_wildlife_prefix(scene({packed("INSEL5", huge)}), meta.entries,
                                                      pe, *types, inputs(), {});
    REQUIRE_FALSE(result);
    CHECK(result.error().reason == "prefix: invalid island geometry");
}

TEST_CASE("wildlife prefix: fertility empty low terrain has no implicit fill and zero changes "
          "still complete") {
    const auto types = registry();
    Metadata meta;
    const auto pe = fake_pe();
    for (const Buffer& raw : {Buffer(6, 0), Buffer{255, 7, 128, 16, 254, 18}}) {
        const auto result =
            load::prepare_wildlife_prefix(scene({fertility_island(raw, {}, false, true)}),
                                          meta.entries, pe, *types, inputs(), {});
        REQUIRE_FALSE(result);
        REQUIRE(result.error().diagnostic);
        const auto& out = *result.error().diagnostic;
        REQUIRE(out.boundary);
        CHECK(out.boundary->tag == "MYSTERY");
        const auto& is = out.islands[0];
        CHECK(is.headers[2].fertility_updates == (raw[0] == 0 ? 0 : 4));
        CHECK_FALSE(is.headers.back().fertility_updates);
        CHECK(is.terrain->fertility == raw);
        CHECK(sim::terrain_matches(*is.terrain));
        for (std::uint64_t i = 0; i < 6; ++i) {
            CHECK(is.resource.tiles.object(i) == 0);
            CHECK(is.resource.tiles.walk_mask(i) == 0);
            CHECK(is.resource.tiles.item_height(i) == 0);
            CHECK(is.resource.tiles.phase(i) == 0);
            CHECK(is.resource.variation[i] == (raw[i] & 15));
        }
        CHECK(out.consumed == 0);
        CHECK(is.resources.empty());
        CHECK(is.spawns.empty());
    }
}

TEST_CASE("wildlife prefix: original Tutorial stops at its first unsupported boundary") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const auto bytes = read_file(*dir / "Scenes/Tutorial/01Discover01.sze");
    const auto ab = read_file(*dir / "Anlagen.dat");
    const auto pe = read_file(*dir / "AnnoGame.dll");
    const auto anlagen = fmt::anlagen::AnlagenFile::parse(ab);
    REQUIRE(anlagen);
    const auto types =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(types.types, types.error);
    const std::vector<std::uint32_t> draws(20000,
                                           7); // Supplied, not a generated original world sequence.
    auto supplied = inputs({500}, 0);
    supplied.stamp_provenance = "explicit test raw sample 500, not MAIN";
    supplied.mode_provenance = "explicit test query bit 0, not MAIN";
    const auto result = load::prepare_wildlife_prefix(bytes, anlagen.value().entries, pe,
                                                      *types.types, supplied, draws);
    const bool inspectable = result.ok() || result.error().diagnostic.has_value();
    REQUIRE_MESSAGE(inspectable, (result.ok() ? "" : result.error().reason));
    const auto& out = result ? result.value() : *result.error().diagnostic;
    REQUIRE(out.islands.size() == 1);
    REQUIRE(out.islands[0].resources.size() > 0);
    REQUIRE_FALSE(result); // A diagnostic prefix is not a complete prepared-island success.
    REQUIRE(out.boundary);
    CHECK_FALSE(out.complete_local_records);
    CHECK_FALSE(out.islands[0].complete_local_records);
    CHECK(out.boundary->source.island == 0);
    CHECK(out.boundary->source.slot == 11);
    CHECK(out.boundary->source.header == 50);
    CHECK(out.boundary->source.record == 0);
    CHECK(out.boundary->tag == "BERGWERK2");
    CHECK(out.boundary->reason.find("unsupported intervening header") != std::string::npos);
    const auto raw_islands = fmt::insel::islands(bytes);
    REQUIRE(raw_islands);
    const auto& raw = raw_islands.value()[0];
    REQUIRE(raw.records[49].name() == "FRUCHTBAR");
    const auto fertility = raw.records[49].payload;
    REQUIRE(fertility.size() == 16384);
    const auto& is = out.islands[0];
    CHECK(is.headers[49].fertility_updates == 8160);
    CHECK_FALSE(is.headers[50].fertility_updates);
    CHECK(is.terrain->fertility == Buffer(fertility.begin(), fertility.end()));
    CHECK(sim::terrain_matches(*is.terrain));
    Buffer high(fertility.size(), 0);
    for (const auto& r : is.resources)
        if (r.effect.variant) {
            const auto i = is.resource.tiles.index_of(r.raw.x, r.raw.y);
            high[i] = static_cast<std::uint8_t>(*r.effect.variant << 4);
        }
    for (std::size_t i = 0; i < fertility.size(); ++i) {
        CHECK((is.resource.variation[i] & 15U) == (fertility[i] & 15U));
        CHECK((is.resource.variation[i] & 240U) == high[i]);
    }
    std::cout << "FERTILITY_REAL header=49 bytes=16384 changed=8160 next=50/BERGWERK2\n";
    CHECK(out.islands[0].resources.size() == 4760);
    CHECK(out.islands[0].spawns.empty());
    CHECK(out.consumed == 4589);
    REQUIRE(out.islands[0].headers.size() == 51);
    CHECK(out.islands[0].headers[33].records == 0);
    REQUIRE(out.islands[0].headers[33].water_mask_updates);
    REQUIRE(out.islands[0].headers[34].water_mask_updates);
    // Independent raw HEIGHT count: first two disjoint 32x32 blocks have 1024/977 water cells.
    CHECK(*out.islands[0].headers[33].water_mask_updates == 1024);
    CHECK(*out.islands[0].headers[34].water_mask_updates == 977);
    CHECK(out.islands[0].resource.tiles.walk_mask(0) == 511);
    REQUIRE(out.islands[0].headers[35].water_mask_updates);
    REQUIRE(out.islands[0].headers[36].water_mask_updates);
    for (std::uint64_t h = 37; h <= 48; ++h)
        REQUIRE(out.islands[0].headers[h].water_mask_updates);
    CHECK_FALSE(
        out.islands[0].headers[49].water_mask_updates); // Unknown header has no proved effect.
    const auto old_boundary =
        std::find_if(out.islands[0].resources.begin(), out.islands[0].resources.end(),
                     [](const auto& r) { return r.source.header == 35 && r.source.record == 22; });
    REQUIRE(old_boundary != out.islands[0].resources.end());
    CHECK(old_boundary->raw.type == sim::TypeId{1558});
    CHECK(old_boundary->draw_before == 37);
    CHECK(old_boundary->effect.disposition == sim::resource_import::Disposition::Accepted);
    CHECK(old_boundary->effect.height == 48);
    CHECK(old_boundary->effect.draws == 0);
    CHECK_FALSE(old_boundary->effect.variant);
    // Independent HHbb item-layout parse plus original place(): all four
    // cells retain the stone. The earlier private HbbH parse is preserved as failed evidence.
    CHECK(old_boundary->raw.x == 107);
    CHECK(old_boundary->raw.y == 43);
    const auto& tiles = out.islands[0].resource.tiles;
    for (std::int32_t y = 43; y <= 44; ++y)
        for (std::int32_t x = 107; x <= 108; ++x) {
            const auto i = tiles.index_of(x, y);
            CHECK(tiles.object(i) == 1558);
            CHECK(tiles.phase(i) == 0);
            CHECK(tiles.item_height(i) == 48);
            CHECK(tiles.foot_dx(i) == x - 107);
            CHECK(tiles.foot_dy(i) == y - 43);
            CHECK(tiles.foot_rotation(i) == 0);
            CHECK(tiles.foot_anchor(i) == (x == 107 && y == 43));
        }
    const auto former_cliff =
        std::find_if(out.islands[0].resources.begin(), out.islands[0].resources.end(),
                     [](const auto& r) { return r.source.header == 37 && r.source.record == 1; });
    REQUIRE(former_cliff != out.islands[0].resources.end());
    CHECK(former_cliff->raw.type == sim::TypeId{1552});
    CHECK(former_cliff->raw.x == 54);
    CHECK(former_cliff->raw.y == 61);
    CHECK(former_cliff->raw.rotation == 3);
    CHECK(former_cliff->draw_before == 255);
    CHECK(former_cliff->effect.disposition == sim::resource_import::Disposition::Accepted);
    CHECK(former_cliff->effect.height == 32);
    CHECK(former_cliff->effect.draws == 0);
    CHECK_FALSE(former_cliff->effect.variant);
    // Independent HHbb/source-order reference expectations, not adapter output.
    // Later partial overlays retain some anchors and replace other cells.
    const std::vector<std::uint32_t> cliff_ids{1552, 1552, 1552, 1554, 1554,
                                               1552, 1554, 1554, 1549};
    const std::vector<std::uint8_t> cliff_heights{32, 32, 32, 16, 16, 32, 16, 16, 48};
    const std::vector<std::uint8_t> cliff_rotations{3, 3, 3, 2, 2, 3, 2, 2, 0};
    const std::vector<std::uint8_t> cliff_dx{0, 1, 2, 1, 2, 2, 1, 2, 0};
    const std::vector<std::uint8_t> cliff_dy{0, 0, 0, 0, 0, 1, 1, 1, 1};
    const std::vector<std::uint8_t> cliff_anchors{1, 1, 1, 1, 0, 1, 0, 0, 1};
    for (std::int32_t dy = 0; dy < 3; ++dy)
        for (std::int32_t dx = 0; dx < 3; ++dx) {
            const auto at = static_cast<std::uint64_t>(dy * 3 + dx);
            const auto i = tiles.index_of(54 + dx, 61 + dy);
            CHECK(tiles.object(i) == cliff_ids[at]);
            CHECK(tiles.phase(i) == 0);
            CHECK(tiles.item_height(i) == cliff_heights[at]);
            CHECK(tiles.foot_dx(i) == cliff_dx[at]);
            CHECK(tiles.foot_dy(i) == cliff_dy[at]);
            CHECK(tiles.foot_rotation(i) == cliff_rotations[at]);
            CHECK(tiles.foot_anchor(i) == cliff_anchors[at]);
        }
    const auto former_rock =
        std::find_if(out.islands[0].resources.begin(), out.islands[0].resources.end(),
                     [](const auto& r) { return r.source.header == 42 && r.source.record == 39; });
    REQUIRE(former_rock != out.islands[0].resources.end());
    CHECK(former_rock->raw.type == sim::TypeId{1512});
    CHECK(former_rock->raw.x == 59);
    CHECK(former_rock->raw.y == 90);
    CHECK(former_rock->raw.rotation == 0);
    CHECK(former_rock->draw_before == 2333);
    CHECK(former_rock->effect.disposition == sim::resource_import::Disposition::Accepted);
    CHECK(former_rock->effect.height == 48); // Actual quad minima 32/48 distinguish mode 2.
    CHECK(former_rock->effect.draws == 0);
    CHECK_FALSE(former_rock->effect.variant);
    for (std::int32_t x = 59; x <= 60; ++x) {
        const auto i = tiles.index_of(x, 90);
        CHECK(tiles.object(i) == 1512);
        CHECK(tiles.phase(i) == 0);
        CHECK(tiles.item_height(i) == 48);
        CHECK(tiles.foot_dx(i) == x - 59);
        CHECK(tiles.foot_dy(i) == 0);
        CHECK(tiles.foot_rotation(i) == 0);
        // Later partial placements retain first anchor and anchor the second cell.
        CHECK(tiles.foot_anchor(i) == 1);
    }
    std::uint64_t stones = 0, cliffs = 0, rocks = 0;
    for (const auto& record : out.islands[0].resources) {
        const auto* type = types.types->find(record.raw.type);
        REQUIRE(type);
        stones += type->flags_word == 0x3a;
        cliffs += type->flags_word == 0x12;
        rocks += type->flags_word == 0x8018;
    }
    CHECK(stones == 84);
    CHECK(cliffs == 63);
    CHECK(rocks == 24);
    std::uint64_t epilogues = 0;
    for (const auto& h : out.islands[0].headers)
        epilogues += h.water_mask_updates.has_value();
    CHECK(epilogues == 16);

    std::uint64_t accepted = 0, rejected = 0, resource_class = 0, resource_draws = 0;
    for (const auto& record : out.islands[0].resources) {
        accepted += record.effect.disposition == sim::resource_import::Disposition::Accepted;
        rejected += record.effect.disposition != sim::resource_import::Disposition::Accepted;
        const auto* original_type = types.types->find(record.raw.type);
        REQUIRE(original_type);
        resource_class += original_type->prodtype >= 225 && original_type->prodtype <= 256;
        resource_draws += record.effect.draws;
    }
    CHECK(accepted == 4760);
    CHECK(rejected == 0);
    CHECK(resource_class == 4589);
    CHECK(resource_draws == 4589);
    const auto* rock_type = types.types->find(sim::TypeId{1512});
    REQUIRE(rock_type);
    CHECK(rock_type->flags_word == 0x8018);
    CHECK(rock_type->prodtype == 321);
    CHECK(rock_type->width == 2);
    CHECK(rock_type->height == 1);
    const auto& first = out.islands[0].resources.front();
    CHECK(first.source.header == 34);
    CHECK(first.source.record == 0);
    CHECK(first.raw.type == sim::TypeId{70});
    CHECK(first.effect.variant == 6);
    CHECK(first.effect.height == 48);
    CHECK(first.effect.draws == 1);
    std::cout << "PREFIX_REAL supplied_stamp=500 supplied_mode=0 complete_local="
              << out.complete_local_records << " resources=" << out.islands[0].resources.size()
              << " accepted=" << accepted << " rejected=" << rejected
              << " resource_class=" << resource_class << " resource_draws=" << resource_draws
              << " spawns=" << out.islands[0].spawns.size() << " consumed=" << out.consumed
              << " stones=" << stones << " cliffs=" << cliffs << " rocks=" << rocks
              << " epilogues=" << epilogues;
    if (out.boundary)
        std::cout << " first_boundary_island=" << out.boundary->source.island
                  << " slot=" << out.boundary->source.slot
                  << " header=" << out.boundary->source.header
                  << " record=" << out.boundary->source.record << " tag=" << out.boundary->tag
                  << " reason=" << out.boundary->reason;
    std::cout << "\n";
    // The predecessor 35/22 boundary is preserved in PREFIX evidence; this is
    // the same unfiltered stream, now stopping at the independently parsed next class.
}
