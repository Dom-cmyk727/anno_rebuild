#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/annogame.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/tagged.hpp>
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

namespace {
Buffer bergwerk_base(std::uint16_t x = 16, std::uint16_t y = 24) {
    Buffer b;
    for (const auto v : std::vector<std::uint16_t>{x, y, 16, 16})
        fmt::put_u16_le(b, v);
    fmt::put_u32_le(b, 19);
    append(b, packed("INFO", Buffer(12, 0)));
    append(b, packed("HEIGHT", block(0, 0, 16, 16, Buffer(256, 48))));
    return b;
}
Buffer bergwerk_entry(std::uint32_t x, std::uint32_t y, std::uint32_t rotation,
                      bool long_form = false, std::uint32_t upper = 0) {
    Buffer word, out;
    fmt::put_u32_le(word, x | (y << 12) | (rotation << 24) | upper);
    REQUIRE(fmt::tagged::write_record("ENTRY", long_form, word, out));
    return out;
}
} // namespace

TEST_CASE(
    "wildlife prefix: bergwerk typed nodes preserve packed axes and duplicate prepend chronology") {
    const auto types = registry();
    Metadata meta;
    const auto pe = fake_pe();
    auto b = bergwerk_base();
    append(b, packed("GELAENDE", block(1, 0, 1, 1, object(104))));
    append(b, packed("BERGWERK2", {}));
    Buffer first;
    REQUIRE(fmt::tagged::write_record("OTHER", false, Buffer{9, 8, 7}, first));
    append(first, bergwerk_entry(7, 6, 1, false, 0xfc000000U));
    const auto parsed = fmt::tagged::parse_records(first);
    REQUIRE(parsed);
    Buffer encoded;
    REQUIRE(fmt::tagged::write_records(parsed.value(), encoded));
    CHECK(encoded == first); // Existing small-record format round-trip, including ignored bits.
    append(b, packed("BERGWERK2", first));
    append(b, packed("FRUCHTBAR", Buffer(256, 2)));
    append(b, packed("BERGWERK2", bergwerk_entry(7, 6, 1, true)));
    append(b, packed("HIRSCH", fmt::insel::encode_hirsch(
                                   std::vector<fmt::insel::HirschRecord>{{0, 0, 0x80, 7, 10}})));
    auto file = scene({packed("INSEL5", b)});
    const auto original = file;
    const auto result = load::prepare_wildlife_prefix(file, meta.entries, pe, *types, inputs(),
                                                      std::vector<std::uint32_t>{4, 7});
    REQUIRE(result);
    CHECK(file == original);
    file.clear();
    file.shrink_to_fit();
    b.clear();
    const auto& out = result.value();
    const auto& is = out.islands[0];
    REQUIRE(is.bergwerk_entries.size() == 2);
    REQUIRE(is.resource_list_nodes.size() == 8);
    REQUIRE(is.resource_list_chunks.size() == 4);
    CHECK(is.headers[3].bergwerk_nodes_added == 0);
    CHECK(is.headers[4].bergwerk_nodes_added == 4);
    CHECK(is.headers[6].bergwerk_nodes_added == 4);
    CHECK(is.headers[4].records == 1);
    CHECK(is.headers[6].records == 1);
    const std::vector<std::uint8_t> offsets{0x67, 0x6f, 0xe7, 0xef};
    for (std::uint64_t i = 0; i < 2; ++i) {
        const auto& e = is.bergwerk_entries[i];
        CHECK(e.local_x == 7);
        CHECK(e.local_y == 6);
        CHECK(e.world_x == 23);
        CHECK(e.world_y == 30);
        CHECK(e.rotation == 1);
        CHECK(e.source.header == (i == 0 ? 4 : 6));
        CHECK(e.source.record == (i == 0 ? 1 : 0));
        for (std::uint64_t c = 0; c < 4; ++c) {
            const auto& n = is.resource_list_nodes[i * 4 + c];
            CHECK(n.entry == i);
            CHECK(n.chunk == c);
            CHECK(n.offsets == offsets[c]);
            CHECK(n.rotation == 1);
            CHECK(n.mask == 0x10);
            CHECK(n.kind == 1);
            if (i == 0)
                CHECK_FALSE(n.next);
            else
                CHECK(n.next == c);
        }
    }
    for (std::uint64_t c = 0; c < 4; ++c) {
        CHECK(is.resource_list_chunks[c].head == c + 4);
        CHECK(is.resource_list_chunks[c].resource_flags == 2);
    }
    CHECK(out.consumed == 2);
    CHECK(is.resource.consumed == 1);
    CHECK(is.resource.variation[0] == 66);
    CHECK(is.resource.tiles.object(0) == 104);
    CHECK(is.resource.tiles.phase(0) == 7);
    CHECK(is.resource.tiles.item_height(0) == 48);
    REQUIRE(is.spawns.size() == 1);
    CHECK(is.spawns[0].draw_before == 1);
    CHECK(is.spawns[0].timestamp == 8);
    CHECK(is.terrain->fertility == Buffer(256, 2));
    CHECK(sim::terrain_matches(*is.terrain));
}

TEST_CASE(
    "wildlife prefix: bergwerk rotations signed chunk division and footprint ends are exact") {
    const auto types = registry();
    Metadata meta;
    const auto pe = fake_pe();
    for (std::uint32_t rotation = 0; rotation < 4; ++rotation) {
        auto b = bergwerk_base();
        append(b, packed("BERGWERK2", bergwerk_entry(5, 4, rotation)));
        const auto result = load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}),
                                                          meta.entries, pe, *types, inputs(), {});
        REQUIRE(result);
        const auto& is = result.value().islands[0];
        REQUIRE(is.resource_list_nodes.size() == ((rotation & 1U) ? 2 : 1));
        CHECK(is.resource_list_nodes[0].offsets == 0x45);
        CHECK(is.resource_list_nodes[0].chunk == 0);
        if (rotation & 1U) {
            CHECK(is.resource_list_nodes[1].offsets == 0x4d);
            CHECK(is.resource_list_nodes[1].chunk == 1);
        }
        CHECK(result.value().consumed == 0);
        CHECK(is.resource.variation == Buffer(256, 0));
        CHECK(is.resource.tiles.object(0) == 0);
    }
    auto b = bergwerk_base(7, 24);
    append(b, packed("BERGWERK2", bergwerk_entry(0, 0, 0)));
    const auto result = load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries,
                                                      pe, *types, inputs(), {});
    REQUIRE(result);
    const auto& is = result.value().islands[0];
    REQUIRE(is.resource_list_nodes.size() == 2);
    CHECK(is.resource_list_nodes[0].chunk == 0); // (-7)/8 truncates to 0, not floor -1.
    CHECK(is.resource_list_nodes[1].chunk == 0); // (8-7)/8 also 0; preserve original collision.
    CHECK(is.resource_list_nodes[0].offsets == 7);
    CHECK(is.resource_list_nodes[1].offsets == 15);
    CHECK(is.resource_list_nodes[1].next == 0);
    CHECK(is.resource_list_chunks[0].head == 1);
}

TEST_CASE(
    "wildlife prefix: bergwerk adapter contract refuses malformed entries before completion") {
    const auto types = registry();
    Metadata meta;
    const auto pe = fake_pe();
    Buffer short_entry, long_entry, oversized_entry;
    REQUIRE(fmt::tagged::write_record("ENTRY", false, Buffer(3, 0), short_entry));
    REQUIRE(fmt::tagged::write_record("ENTRY", false, Buffer(5, 0), oversized_entry));
    REQUIRE(fmt::tagged::write_record("ENTRY", true, Buffer(4, 0), long_entry));
    long_entry[6] = 255; // Extended length now claims 0xff0004, beyond the input.
    for (const auto& bad :
         {short_entry, oversized_entry, long_entry, Buffer{1}, bergwerk_entry(4095, 4095, 0)}) {
        auto b = bergwerk_base();
        append(b, packed("GELAENDE", block(1, 0, 1, 1, object(104))));
        append(b, packed("BERGWERK2", bergwerk_entry(7, 6, 1)));
        append(b, packed("BERGWERK2", bad));
        const auto result =
            load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries, pe, *types,
                                          inputs(), std::vector<std::uint32_t>{4});
        REQUIRE_FALSE(result);
        REQUIRE(result.error().diagnostic);
        const auto& out = *result.error().diagnostic;
        REQUIRE(out.boundary);
        CHECK(out.boundary->tag == "BERGWERK2");
        CHECK(out.boundary->source.header == 4);
        CHECK_FALSE(out.islands[0].headers.back().bergwerk_nodes_added);
        CHECK(out.islands[0].headers[3].bergwerk_nodes_added == 4);
        CHECK(out.islands[0].bergwerk_entries.size() == 1);
        CHECK(out.islands[0].resource_list_nodes.size() == 4);
        CHECK(out.islands[0].resource.variation[0] == 64);
        CHECK(out.consumed == 1);
    }
    // ENTRY validation is per source item; earlier entries in this header stay diagnostic.
    auto b = bergwerk_base();
    auto payload = bergwerk_entry(7, 6, 1);
    append(payload, short_entry);
    append(b, packed("BERGWERK2", payload));
    const auto result = load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries,
                                                      pe, *types, inputs(), {});
    REQUIRE_FALSE(result);
    REQUIRE(result.error().diagnostic);
    const auto& out = *result.error().diagnostic;
    REQUIRE(out.boundary);
    CHECK(out.boundary->source.record == 1);
    CHECK(out.islands[0].headers.back().records == 1);
    CHECK_FALSE(out.islands[0].headers.back().bergwerk_nodes_added);
    CHECK(out.islands[0].resource_list_nodes.size() == 4);
    CHECK_FALSE(out.complete_local_records);
}

namespace {
std::shared_ptr<const sim::TypeRegistry> ore_registry() {
    sim::TypeRegistry::Builder b;
    sim::BuildingType resource;
    resource.id = sim::TypeId{104};
    resource.prodtype = 225;
    resource.flags_word = 0x29;
    b.add(resource);
    for (const auto id : {501U, 502U, 503U, 504U, 1661U}) {
        sim::FigureType f;
        f.id = id;
        f.bewohntype = id == 503 ? 146 : 145;
        f.ware = id == 502 ? "SALZSTEIN" : id == 504 ? "WOLLE" : "EISENERZ";
        b.figure_type(f);
    }
    const auto built = b.build();
    REQUIRE(built);
    return built.value();
}
Buffer ore_entry(std::uint32_t type = 501, std::uint16_t tons = 400, std::uint32_t discovery = 0,
                 std::uint32_t x = 0x40d00000, std::uint32_t y = 0x40f00000,
                 std::size_t state_length = 8) {
    Buffer body;
    fmt::put_u32_le(body, (type << 14) | 0xfc003fffU); // Unused fields deliberately nonzero.
    fmt::put_u32_le(body, x);                          // Authored local 6.5.
    fmt::put_u32_le(body, y);                          // Authored local 7.5.
    fmt::put_u32_le(body, 0x3fc00000);                 // Authored height 1.5.
    if (state_length == 8) {
        fmt::put_u16_le(body, tons);
        fmt::put_u16_le(body, 0xabcd); // Padding is ignored.
        fmt::put_u32_le(body, discovery);
    } else {
        body.resize(16 + state_length, 255);
    }
    Buffer out;
    REQUIRE(fmt::tagged::write_record("ENTRY2", false, body, out));
    return out;
}
Buffer ore_base() {
    auto b = bergwerk_base();
    append(b, packed("GELAENDE", block(1, 0, 1, 1, object(104))));
    append(b, packed("BERGWERK2", bergwerk_entry(7, 6, 1)));
    return b;
}
} // namespace

TEST_CASE(
    "wildlife prefix: occur ore threshold equality preserves reserve and original load order") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = fake_pe();
    for (const auto tons : std::vector<std::uint16_t>{0, 1599, 1600, 1601, 65535}) {
        auto b = ore_base();
        append(b, packed("OCCUR", ore_entry(501, tons, 0x80000002U)));
        const auto result =
            load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries, pe, *types,
                                          inputs(), std::vector<std::uint32_t>{4});
        REQUIRE(result);
        const auto& is = result.value().islands[0];
        REQUIRE(is.occurrences.size() == 1);
        const auto& o = is.occurrences[0];
        CHECK(o.type == 501);
        CHECK(o.goods == 1);
        CHECK(o.world_x_bits == 0x41b40000U); // 22.5 after the original f32 store.
        CHECK(o.world_y_bits == 0x41fc0000U); // 31.5.
        CHECK(o.height_bits == 0x3fc00000U);
        CHECK(o.tile_x == 22);
        CHECK(o.tile_y == 31);
        CHECK(o.reserve == 1);
        CHECK(o.file_tons == tons);
        CHECK(o.quantity_88 == (tons < 1600 ? 512000 : std::uint32_t{tons} * 256));
        CHECK(o.discovery == 0x80000002U);
        REQUIRE(is.resource_list_nodes.size() == 5);
        const auto& node = is.resource_list_nodes[4];
        CHECK(node.kind == 6);
        CHECK(node.chunk == 0);
        CHECK(node.x_offset == 6);
        CHECK(node.y_offset == 7);
        CHECK(node.next == 0);
        CHECK(is.resource_list_chunks[0].head == 4);
        CHECK(is.resource_list_chunks[0].resource_flags == 0x42);
        for (std::uint64_t i = 0; i < 4; ++i)
            CHECK(is.resource_list_nodes[i].mask == 0x11);
        CHECK(is.occurrence_loads[0].mask_updates == 4);
        CHECK(is.headers.back().occurrences_added == 1);
        CHECK(result.value().consumed == 1);
        CHECK(is.resource.variation[0] == 64);
    }
    for (const std::uint32_t type : {501U, 1661U})
        for (const std::size_t length : {0U, 1U, 8U, 9U}) {
            auto b = ore_base();
            append(b, packed("OCCUR", ore_entry(type, 0, 99, 0x40d00000, 0x40f00000, length)));
            const auto result =
                load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries, pe,
                                              *types, inputs(), std::vector<std::uint32_t>{4});
            REQUIRE(result);
            const auto& o = result.value().islands[0].occurrences[0];
            CHECK(o.reserve == (type == 1661 ? 0 : 1));
            if (length == 8) {
                CHECK(o.quantity_88 == (type == 1661 ? 0 : 512000));
                CHECK(o.discovery == 99);
            } else {
                CHECK(o.quantity_88 == (type == 1661 ? 102400 : 512000));
                CHECK(o.discovery == 0);
                CHECK_FALSE(o.file_tons);
            }
            // A zero-Q deep source still refreshes every reached BERGWERK mask.
            CHECK(result.value().islands[0].resource_list_nodes[0].mask == 17);
        }
}

TEST_CASE(
    "wildlife prefix: occur shared list masks candidate head and duplicate chronology are owned") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = fake_pe();
    auto b = ore_base();
    append(b, packed("OCCUR", {}));
    auto first = ore_entry(501, 400, 1);
    append(first, ore_entry(502, 1601, 2, 0x40b00000)); // Different tile 5.5, same chunk.
    append(b, packed("OCCUR", first));
    append(b, packed("FRUCHTBAR", Buffer(256, 3)));
    auto second =
        ore_entry(501, 0, 99, 0x40b00000); // Same tile/class as current head, different type: skip.
    append(second,
           ore_entry(501, 1600, 4)); // Older identical position is hidden by that head: create.
    append(b, packed("OCCUR", second));
    append(b, packed("HIRSCH", fmt::insel::encode_hirsch(
                                   std::vector<fmt::insel::HirschRecord>{{0, 0, 0x80, 7, 10}})));
    auto file = scene({packed("INSEL5", b)});
    const auto original = file;
    const auto result = load::prepare_wildlife_prefix(file, meta.entries, pe, *types, inputs(),
                                                      std::vector<std::uint32_t>{4, 7});
    REQUIRE(result);
    CHECK(file == original);
    file.clear();
    file.shrink_to_fit();
    b.clear();
    first.clear();
    second.clear();
    const auto& out = result.value();
    const auto& is = out.islands[0];
    REQUIRE(is.occurrences.size() == 3);
    REQUIRE(is.occurrence_loads.size() == 4);
    CHECK(is.occurrence_loads[0].created);
    CHECK(is.occurrence_loads[1].created);
    CHECK_FALSE(is.occurrence_loads[2].created);
    CHECK(is.occurrence_loads[2].occurrence == 1);
    CHECK(is.occurrence_loads[2].mask_updates == 0);
    CHECK(is.occurrence_loads[3].created);
    CHECK(is.occurrences[1].type == 502);
    CHECK(is.occurrences[1].quantity_88 == 409856);
    CHECK(is.occurrences[1].discovery == 2);
    CHECK(is.occurrences[2].quantity_88 == 409600);
    CHECK(is.occurrences[2].discovery == 4);
    CHECK(is.headers[4].occurrences_added == 0);
    CHECK(is.headers[5].occurrences_added == 2);
    CHECK(is.headers[7].occurrences_added == 1);
    CHECK(is.headers[7].records == 2);
    REQUIRE(is.resource_list_nodes.size() == 7);
    CHECK(is.resource_list_nodes[4].next == 0);
    CHECK(is.resource_list_nodes[5].next == 4);
    CHECK(is.resource_list_nodes[6].next == 5);
    CHECK(is.resource_list_chunks[0].head == 6);
    for (std::uint64_t i = 0; i < 4; ++i)
        CHECK(is.resource_list_nodes[i].mask ==
              0x15); // goods 1/3 union, not tonnage or arithmetic sum.
    CHECK(is.resource.variation[0] == 67);
    CHECK(is.resource.tiles.object(0) == 104);
    CHECK(is.resource.tiles.phase(0) == 7);
    CHECK(is.resource.tiles.item_height(0) == 48);
    CHECK(is.terrain->fertility == Buffer(256, 3));
    CHECK(out.consumed == 2);
    REQUIRE(is.spawns.size() == 1);
    CHECK(is.spawns[0].draw_before == 1);
    CHECK(is.spawns[0].timestamp == 8);
}

TEST_CASE("wildlife prefix: occur adapter contract stops malformed unknown and unsafe entries "
          "diagnostically") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = fake_pe();
    Buffer short_entry;
    REQUIRE(fmt::tagged::write_record("ENTRY2", false, Buffer(15, 0), short_entry));
    for (const auto& bad : {short_entry, Buffer{1}, ore_entry(999), ore_entry(503), ore_entry(504),
                            ore_entry(501, 400, 0, 0x7f800000), ore_entry(501, 400, 0, 0x7fc00000),
                            ore_entry(501, 400, 0, 0x437f0000)}) {
        auto b = ore_base();
        append(b, packed("OCCUR", ore_entry()));
        append(b, packed("OCCUR", bad));
        const auto result =
            load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries, pe, *types,
                                          inputs(), std::vector<std::uint32_t>{4});
        REQUIRE_FALSE(result);
        REQUIRE(result.error().diagnostic);
        const auto& out = *result.error().diagnostic;
        REQUIRE(out.boundary);
        CHECK(out.boundary->tag == "OCCUR");
        CHECK(out.boundary->source.header == 5);
        CHECK_FALSE(out.islands[0].headers.back().occurrences_added);
        CHECK(out.islands[0].headers[4].occurrences_added == 1);
        CHECK(out.islands[0].occurrences.size() == 1);
        CHECK(out.islands[0].resource_list_nodes.size() == 5);
        CHECK(out.islands[0].resource_list_nodes[0].mask == 17);
        CHECK(out.consumed == 1);
        CHECK_FALSE(out.complete_local_records);
    }
    auto b = ore_base();
    auto payload = ore_entry();
    append(payload, ore_entry(503));
    append(b, packed("OCCUR", payload));
    const auto result =
        load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries, pe, *types,
                                      inputs(), std::vector<std::uint32_t>{4});
    REQUIRE_FALSE(result);
    REQUIRE(result.error().diagnostic);
    const auto& out = *result.error().diagnostic;
    CHECK(out.boundary->source.record == 1);
    CHECK(out.islands[0].headers.back().records == 1);
    CHECK_FALSE(out.islands[0].headers.back().occurrences_added);
    CHECK(out.islands[0].occurrences.size() == 1);
}

TEST_CASE(
    "wildlife prefix: occur mask radius boundary and u16 refresh edges preserve untouched nodes") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = fake_pe();
    for (const auto local_y : {0x40800000U, 0x40400000U}) { // 4.0 versus 3.0.
        auto b = ore_base();
        append(b, packed("OCCUR", ore_entry(501, 400, 0, 0, local_y)));
        const auto result =
            load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries, pe, *types,
                                          inputs(), std::vector<std::uint32_t>{4});
        REQUIRE(result);
        const auto& is = result.value().islands[0];
        // Anchor (23,30), source (16,28)/(16,27): row bounds 7 versus 6 at dx 7.
        const auto expected = local_y == 0x40800000U ? 17 : 16;
        CHECK(is.resource_list_nodes[0].mask == expected);
        CHECK(is.resource_list_nodes[2].mask == expected);
        // The actual 15-wide visitor excludes the right-column duplicate nodes.
        CHECK(is.resource_list_nodes[1].mask == 16);
        CHECK(is.resource_list_nodes[3].mask == 16);
        CHECK(is.occurrence_loads[0].mask_updates == (expected == 17 ? 2 : 0));
    }
    auto b = bergwerk_base(0, 0);
    append(b, packed("BERGWERK2", bergwerk_entry(0, 0, 0)));
    append(b, packed("OCCUR", ore_entry(501, 400, 0, 0x3f000000, 0x3f000000)));
    const auto result = load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries,
                                                      pe, *types, inputs(), {});
    REQUIRE(result);
    CHECK(result.value().islands[0].occurrences[0].tile_x == 0);
    CHECK(result.value().islands[0].resource_list_nodes[0].mask == 16);
    CHECK(result.value().islands[0].occurrence_loads[0].mask_updates == 0);
    CHECK(result.value().consumed == 0); // u16(0-7), not a fabricated signed-clipped refresh.
}

namespace {
std::vector<std::string> anbau_names() {
    std::vector<std::string> names;
    for (unsigned i = 0; i < 13; ++i)
        names.push_back("NAME" + std::to_string(i));
    names[0] = "AX";
    names[1] = "BY";
    names[12] = "CZ";
    return names;
}
Buffer anbau_pe(const std::vector<std::string>& names = anbau_names()) {
    REQUIRE(names.size() == 13);
    auto b = fake_pe();
    b.resize(0x2800, 0);
    put16(b, 0x46, 2);
    const auto sec = 0x58U + 0xe0U + 40;
    put32(b, sec + 12, 0x25000);
    put32(b, sec + 16, 0x2000);
    put32(b, sec + 20, 0x800);
    std::uint32_t pos = 0x25400;
    for (std::uint32_t i = 0; i < names.size(); ++i) {
        put32(b, 0x800 + 0x251f8 + (56 + i) * 4 - 0x25000, 0x10000000 + pos);
        for (const auto c : names[i])
            b.at(0x800 + pos++ - 0x25000) = static_cast<std::uint8_t>(c);
        b.at(0x800 + pos++ - 0x25000) = 0;
    }
    const std::string fallback = "FALL";
    for (std::uint32_t i = 0; i < fallback.size(); ++i)
        b.at(0x800 + 0x261e8 + i - 0x25000) = static_cast<std::uint8_t>(fallback[i]);
    return b;
}
Buffer anbau_payload(const std::vector<std::string>& names) {
    Buffer b;
    for (const auto& name : names) {
        b.insert(b.end(), name.begin(), name.end());
        b.push_back(0);
    }
    return b;
}
} // namespace

TEST_CASE("wildlife prefix: anbau replaces prior bitmap and owns token chronology without changing "
          "earlier effects") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = anbau_pe();
    auto b = ore_base();
    append(b, packed("OCCUR", ore_entry()));
    append(b, packed("ANBAU", anbau_payload({"AX", "CZ"})));
    append(b, packed("FRUCHTBAR", Buffer(256, 2)));
    append(b, packed("ANBAU", anbau_payload({"BY"})));
    append(b, packed("HIRSCH", fmt::insel::encode_hirsch(
                                   std::vector<fmt::insel::HirschRecord>{{0, 0, 0x80, 7, 10}})));
    append(b, packed("ANBAU", {}));
    auto file = scene({packed("INSEL5", b)});
    const auto original = file;
    const auto result = load::prepare_wildlife_prefix(file, meta.entries, pe, *types, inputs(),
                                                      std::vector<std::uint32_t>{4, 7});
    REQUIRE(result);
    CHECK(file == original);
    file.clear();
    file.shrink_to_fit();
    b.clear();
    const auto& out = result.value();
    const auto& is = out.islands[0];
    REQUIRE(is.headers[5].anbau);
    REQUIRE(is.headers[7].anbau);
    REQUIRE(is.headers[9].anbau);
    CHECK(is.headers[5].anbau->seed == 0);
    CHECK(is.headers[5].anbau->mask == 0x1001);
    CHECK(is.headers[7].anbau->mask == 2);
    CHECK(is.headers[9].anbau->mask == 0);
    CHECK(is.anbau_mask == 0);
    CHECK(is.headers[5].records == 2);
    CHECK(is.headers[7].records == 1);
    CHECK(is.headers[9].records == 0);
    REQUIRE(is.anbau_tokens.size() == 3);
    CHECK(is.anbau_tokens[0].name == "AX");
    CHECK(is.anbau_tokens[1].name == "CZ");
    CHECK(is.anbau_tokens[2].name == "BY");
    CHECK(is.anbau_tokens[0].goods == 56);
    CHECK(is.anbau_tokens[1].goods == 68);
    CHECK(is.anbau_tokens[2].goods == 57);
    CHECK(is.anbau_tokens[0].source.header == 5);
    CHECK(is.anbau_tokens[1].source.record == 1);
    CHECK(is.anbau_tokens[2].source.header == 7);
    Buffer rebuilt;
    for (const auto& token : is.anbau_tokens) {
        rebuilt.insert(rebuilt.end(), token.name.begin(), token.name.end());
        rebuilt.push_back(0);
    }
    CHECK(rebuilt == anbau_payload({"AX", "CZ", "BY"})); // Processed strings round-trip exactly.
    CHECK(out.consumed == 2);
    REQUIRE(is.spawns.size() == 1);
    CHECK(is.spawns[0].draw_before == 1);
    CHECK(is.spawns[0].timestamp == 8);
    CHECK(is.resource.variation[0] == 66);
    CHECK(is.resource.tiles.object(0) == 104);
    CHECK(is.resource.tiles.item_height(0) == 48);
    REQUIRE(is.occurrences.size() == 1);
    CHECK(is.occurrences[0].quantity_88 == 512000);
    for (std::uint64_t i = 0; i < 4; ++i)
        CHECK(is.resource_list_nodes[i].mask == 17);
    CHECK(is.resource_list_chunks[0].head == 4);
    CHECK(is.terrain->fertility == Buffer(256, 2));
    CHECK(sim::terrain_matches(*is.terrain));
}

TEST_CASE(
    "wildlife prefix: anbau image names exact bytes first match null fallback and climate seeds") {
    const auto types = registry();
    Metadata meta;
    auto names = anbau_names();
    names[2] = "AX";
    names[3] = std::string("\xC4Q", 2);
    names[4] = "";
    auto pe = anbau_pe(names);
    put32(pe, 0x800 + 0x251f8 + 61 * 4 - 0x25000, 0); // Getter fallback, not a missing row.
    const auto profile = fmt::annogame::read_anbau_names(pe);
    REQUIRE(profile);
    CHECK(profile.value().first_goods == 56);
    REQUIRE(profile.value().names.size() == 13);
    CHECK(profile.value().names[5] == "FALL");
    for (const std::uint8_t climate : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}}) {
        auto b = bergwerk_base();
        append(b, packed("ANBAU", anbau_payload({"AX", "ax", std::string("\xC4Q", 2),
                                                 std::string("\xC3\x84Q", 3), "", "FALL"})));
        auto file = change_header(scene({packed("INSEL5", b)}), "INFO", 0, Buffer(12, climate));
        const auto result =
            load::prepare_wildlife_prefix(file, meta.entries, pe, *types, inputs(), {});
        REQUIRE(result);
        const auto& is = result.value().islands[0];
        const std::uint32_t seed = climate == 0 ? 0 : 0xf009;
        CHECK(is.anbau_mask == (seed | 0x39));
        CHECK(is.headers.back().anbau->seed == seed);
        CHECK_FALSE(is.anbau_tokens[1].goods);
        CHECK_FALSE(is.anbau_tokens[3].goods);
        CHECK(is.anbau_tokens[0].goods == 56); // Not duplicate row 58.
        CHECK(is.anbau_tokens[2].goods == 59);
        CHECK(is.anbau_tokens[4].goods == 60);
        CHECK(is.anbau_tokens[5].goods == 61);
        CHECK(result.value().consumed == 0);
        file = change_header(file, "ANBAU", 2, {});
        const auto empty =
            load::prepare_wildlife_prefix(file, meta.entries, pe, *types, inputs(), {});
        REQUIRE(empty);
        CHECK(empty.value().islands[0].anbau_mask == seed);
        REQUIRE(empty.value().islands[0].headers.back().anbau);
        CHECK(empty.value().islands[0].anbau_tokens.empty());
        const auto no_name_data =
            load::prepare_wildlife_prefix(file, meta.entries, fake_pe(), *types, inputs(), {});
        REQUIRE(no_name_data); // Empty ANBAU never calls the Game name lookup.
        CHECK(no_name_data.value().islands[0].anbau_mask == seed);
    }
    const auto absent = load::prepare_wildlife_prefix(scene({packed("INSEL5", bergwerk_base())}),
                                                      meta.entries, pe, *types, inputs(), {});
    REQUIRE(absent);
    CHECK_FALSE(absent.value().islands[0].anbau_mask);
}

TEST_CASE("wildlife prefix: anbau original token limit ignores trailing bytes after the thirteenth "
          "string") {
    const auto types = registry();
    Metadata meta;
    const auto pe = anbau_pe();
    for (const bool match_last : {false, true}) {
        std::vector<std::string> names(13, "?");
        if (match_last)
            names.back() = "CZ";
        auto payload = anbau_payload(names);
        append(payload, anbau_payload({"AX"}));
        payload.push_back(255);
        auto b = bergwerk_base();
        append(b, packed("ANBAU", payload));
        const auto result = load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}),
                                                          meta.entries, pe, *types, inputs(), {});
        REQUIRE(result);
        const auto& is = result.value().islands[0];
        CHECK(is.anbau_mask == (match_last ? 0x1000 : 0));
        CHECK(is.headers.back().records == 13);
        REQUIRE(is.headers.back().anbau);
        CHECK(is.headers.back().anbau->ignored_bytes == 4);
        CHECK(is.anbau_tokens.size() == 13);
        CHECK(result.value().consumed == 0);
    }
}

TEST_CASE("wildlife prefix: anbau adapter contract refuses unterminated processed tokens and "
          "unsafe image metadata") {
    const auto types = registry();
    Metadata meta;
    const auto pe = anbau_pe();
    auto twelve = anbau_payload(std::vector<std::string>(12, "?"));
    twelve.push_back('A');
    for (const auto& bad : {Buffer{'B', 'Y'}, twelve}) {
        auto b = ore_base();
        append(b, packed("ANBAU", anbau_payload({"AX"})));
        append(b, packed("ANBAU", bad));
        const auto result =
            load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries, pe, *types,
                                          inputs(), std::vector<std::uint32_t>{4});
        REQUIRE_FALSE(result);
        REQUIRE(result.error().diagnostic);
        const auto& out = *result.error().diagnostic;
        REQUIRE(out.boundary);
        CHECK(out.boundary->tag == "ANBAU");
        CHECK(out.boundary->source.header == 5);
        CHECK_FALSE(out.islands[0].headers.back().anbau);
        CHECK(out.islands[0].headers.back().records == 0);
        CHECK(out.islands[0].anbau_mask == 1);
        CHECK(out.islands[0].anbau_tokens.size() == 1);
        CHECK(out.islands[0].resource.variation[0] == 64);
        CHECK(out.consumed == 1);
    }
    auto bad_pointer = pe;
    put32(bad_pointer, 0x800 + 0x251f8 + 56 * 4 - 0x25000, 0xffffffffU);
    auto no_string = pe;
    put32(no_string, 0x800 + 0x251f8 + 56 * 4 - 0x25000, 0x10026ff0);
    std::fill(no_string.begin() + 0x27f0, no_string.end(), std::uint8_t{'A'});
    auto short_table = pe;
    put32(short_table, 0x58 + 0xe0 + 40 + 16, 0x2d8);
    for (const auto& bad : {bad_pointer, no_string, short_table}) {
        CHECK_FALSE(fmt::annogame::read_anbau_names(bad));
        auto b = ore_base();
        append(b, packed("ANBAU", anbau_payload({"AX"})));
        const auto result =
            load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries, bad, *types,
                                          inputs(), std::vector<std::uint32_t>{4});
        REQUIRE_FALSE(result);
        REQUIRE(result.error().diagnostic);
        const auto& out = *result.error().diagnostic;
        REQUIRE(out.boundary);
        CHECK(out.boundary->tag == "ANBAU");
        CHECK_FALSE(out.islands[0].anbau_mask);
        CHECK_FALSE(out.islands[0].headers.back().anbau);
        CHECK(out.islands[0].anbau_tokens.empty());
        CHECK(out.consumed == 1);
    }
    auto wrong_identity = pe;
    put32(wrong_identity, 0x48, 0);
    CHECK_FALSE(fmt::annogame::read_anbau_names(wrong_identity));
    auto wrong_machine = pe;
    put16(wrong_machine, 0x44, 0x8664);
    CHECK_FALSE(fmt::annogame::read_anbau_names(wrong_machine));
}

TEST_CASE("wildlife prefix: discover replaces only the low word and owns ordered scalar effects") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = anbau_pe();
    auto b = ore_base();
    append(b, packed("OCCUR", ore_entry()));
    append(b, packed("ANBAU", anbau_payload({"AX"})));
    Buffer scalar(4);
    put32(scalar, 0, 0xabcd8001);
    append(b, packed("DISCOVER", scalar));
    append(b, packed("FRUCHTBAR", Buffer(256, 2)));
    put32(scalar, 0, 0x76540002);
    append(b, packed("DISCOVER", scalar));
    append(b, packed("HIRSCH", fmt::insel::encode_hirsch(
                                   std::vector<fmt::insel::HirschRecord>{{0, 0, 0x80, 7, 10}})));
    auto file = scene({packed("INSEL5", b)});
    const auto original = file;
    const auto result = load::prepare_wildlife_prefix(file, meta.entries, pe, *types, inputs(),
                                                      std::vector<std::uint32_t>{4, 7});
    REQUIRE(result);
    CHECK(file == original);
    file.clear();
    file.shrink_to_fit();
    b.clear();
    const auto& out = result.value();
    REQUIRE(out.islands.size() == 1);
    const auto& is = out.islands[0];
    REQUIRE(is.headers.size() == 10);
    CHECK(is.headers[6].discover == 32769);
    CHECK(is.headers[8].discover == 2);
    CHECK(is.discover_mask == 2);
    CHECK(is.headers[6].source.header == 6);
    CHECK(is.headers[8].source.header == 8);
    CHECK(is.headers[6].records == 1);
    CHECK(is.headers[8].records == 1);
    CHECK_FALSE(is.headers[7].discover);
    CHECK(is.headers[7].fertility_updates.has_value());
    CHECK(is.terrain->fertility == Buffer(256, 2));
    CHECK(sim::terrain_matches(*is.terrain));
    CHECK(is.anbau_mask == 1);
    REQUIRE(is.occurrences.size() == 1);
    CHECK(is.occurrences[0].quantity_88 == 512000);
    CHECK(is.occurrences[0].reserve == 1);
    CHECK(is.occurrences[0].discovery == 0);
    CHECK(is.occurrence_loads.size() == 1);
    CHECK(is.spawns.size() == 1);
    CHECK(is.spawns[0].draw_before == 1);
    CHECK(is.spawns[0].timestamp == 8);
    CHECK(out.consumed == 2);
    CHECK(is.resources.size() == 1);
    CHECK(is.resources[0].draw_before == 0);
    CHECK(is.resource_list_nodes.size() == 5);
    CHECK(is.resource_list_nodes[0].mask == 17);
    CHECK(is.resource.variation[0] == 66);
    CHECK(is.resource.tiles.object(0) == 104);
    CHECK(is.resource.tiles.item_height(0) == 48);
    CHECK(out.complete_local_records);
}

TEST_CASE(
    "wildlife prefix: discover contract distinguishes absence zero maximum and malformed framing") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = fake_pe();
    const auto run = [&](const Buffer& body) {
        return load::prepare_wildlife_prefix(scene({packed("INSEL5", body)}), meta.entries, pe,
                                             *types, inputs(), std::vector<std::uint32_t>{4});
    };
    const auto absent = run(ore_base());
    REQUIRE(absent);
    CHECK_FALSE(absent.value().islands[0].discover_mask);
    for (const auto word : {0U, 0x1234ffffU, 0xabcd8001U}) {
        auto b = ore_base();
        Buffer scalar(4);
        put32(scalar, 0, word);
        append(b, packed("DISCOVER", scalar));
        const auto result = run(b);
        REQUIRE(result);
        const auto& is = result.value().islands[0];
        REQUIRE(is.discover_mask);
        CHECK(*is.discover_mask == (word & 65535U));
        CHECK(is.headers.back().discover == is.discover_mask);
        CHECK(is.headers.back().records == 1);
        CHECK(result.value().consumed == 1);
    }
    for (const auto length : {0U, 1U, 2U, 3U, 5U, 8U}) {
        auto b = ore_base();
        Buffer scalar(4);
        put32(scalar, 0, 0xabcd8001);
        append(b, packed("DISCOVER", scalar));
        append(b, packed("DISCOVER", Buffer(length, 0xff)));
        const auto result = run(b);
        REQUIRE_FALSE(result);
        REQUIRE(result.error().diagnostic);
        const auto& out = *result.error().diagnostic;
        REQUIRE(out.boundary);
        CHECK(out.boundary->tag == "DISCOVER");
        CHECK(out.boundary->source.header == 5);
        CHECK(out.boundary->reason == "expected four-byte DISCOVER payload");
        const auto& is = out.islands[0];
        CHECK(is.discover_mask == 32769);
        CHECK(is.headers[4].discover == 32769);
        CHECK_FALSE(is.headers[5].discover);
        CHECK(is.headers[5].records == 0);
        CHECK_FALSE(is.complete_local_records);
        CHECK(out.consumed == 1);
        CHECK(is.resources.size() == 1);
    }
}

TEST_CASE("wildlife prefix: kontor counter replaces unsigned words and preserves earlier effects") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = anbau_pe();
    auto b = ore_base();
    append(b, packed("OCCUR", ore_entry()));
    Buffer discover(4);
    put32(discover, 0, 0xabcd8001);
    append(b, packed("DISCOVER", discover));
    append(b, packed("ANBAU", anbau_payload({"AX"})));
    append(b, packed("KONTORID", Buffer{2, 129})); // LE 0x8102 = 33026, not byte 2 or 641.
    append(b, packed("FRUCHTBAR", Buffer(256, 2)));
    append(b, packed("KONTORID", Buffer{7, 0}));
    append(b, packed("HIRSCH", fmt::insel::encode_hirsch(
                                   std::vector<fmt::insel::HirschRecord>{{0, 0, 0x80, 7, 10}})));
    auto file = scene({packed("INSEL5", b)});
    const auto original = file;
    const auto result = load::prepare_wildlife_prefix(file, meta.entries, pe, *types, inputs(),
                                                      std::vector<std::uint32_t>{4, 7});
    REQUIRE(result);
    CHECK(file == original);
    file.clear();
    file.shrink_to_fit();
    b.clear();
    const auto& out = result.value();
    REQUIRE(out.islands.size() == 1);
    const auto& is = out.islands[0];
    REQUIRE(is.headers.size() == 11);
    CHECK(is.headers[7].kontor_counter == 33026);
    CHECK(is.headers[9].kontor_counter == 7);
    CHECK(is.kontor_counter == 7);
    CHECK(is.headers[7].source.header == 7);
    CHECK(is.headers[9].source.header == 9);
    CHECK(is.headers[7].records == 1);
    CHECK(is.headers[9].records == 1);
    CHECK_FALSE(is.headers[8].kontor_counter);
    CHECK(is.headers[8].fertility_updates.has_value());
    CHECK(is.terrain->fertility == Buffer(256, 2));
    CHECK(sim::terrain_matches(*is.terrain));
    CHECK(is.anbau_mask == 1);
    CHECK(is.discover_mask == 32769);
    CHECK(is.headers[5].discover == 32769);
    REQUIRE(is.occurrences.size() == 1);
    CHECK(is.occurrences[0].quantity_88 == 512000);
    CHECK(is.occurrences[0].reserve == 1);
    CHECK(is.occurrences[0].discovery == 0);
    CHECK(is.occurrence_loads.size() == 1);
    CHECK(is.spawns.size() == 1);
    CHECK(is.spawns[0].draw_before == 1);
    CHECK(is.spawns[0].timestamp == 8);
    CHECK(out.consumed == 2);
    CHECK(is.resources.size() == 1);
    CHECK(is.resources[0].draw_before == 0);
    CHECK(is.resource_list_nodes.size() == 5);
    CHECK(is.resource_list_nodes[0].mask == 17);
    CHECK(is.resource.variation[0] == 66);
    CHECK(is.resource.tiles.object(0) == 104);
    CHECK(is.resource.tiles.item_height(0) == 48);
    CHECK(out.complete_local_records);
}

TEST_CASE("wildlife prefix: kontor counter contract distinguishes absent zero maximum and "
          "malformed framing") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = fake_pe();
    const auto run = [&](const Buffer& body) {
        return load::prepare_wildlife_prefix(scene({packed("INSEL5", body)}), meta.entries, pe,
                                             *types, inputs(), std::vector<std::uint32_t>{4});
    };
    const auto absent = run(ore_base());
    REQUIRE(absent);
    CHECK_FALSE(absent.value().islands[0].kontor_counter);
    for (const auto word : {0U, 65535U, 33026U}) {
        auto b = ore_base();
        append(b, packed("KONTORID", Buffer{2, 129}));
        Buffer scalar(2);
        put16(scalar, 0, static_cast<std::uint16_t>(word));
        append(b, packed("KONTORID", scalar));
        const auto result = run(b);
        REQUIRE(result);
        const auto& is = result.value().islands[0];
        REQUIRE(is.kontor_counter);
        CHECK(*is.kontor_counter == word);
        CHECK(is.headers.back().kontor_counter == is.kontor_counter);
        CHECK(is.headers.back().records == 1);
        CHECK(result.value().consumed == 1);
    }
    for (const auto length : {0U, 1U, 3U, 4U, 8U}) {
        auto b = ore_base();
        append(b, packed("KONTORID", Buffer{2, 129}));
        append(b, packed("KONTORID", Buffer(length, 0xff)));
        const auto result = run(b);
        REQUIRE_FALSE(result);
        REQUIRE(result.error().diagnostic);
        const auto& out = *result.error().diagnostic;
        REQUIRE(out.boundary);
        CHECK(out.boundary->tag == "KONTORID");
        CHECK(out.boundary->source.header == 5);
        CHECK(out.boundary->reason == "expected two-byte KONTORID payload");
        const auto& is = out.islands[0];
        CHECK(is.kontor_counter == 33026);
        CHECK(is.headers[4].kontor_counter == 33026);
        CHECK_FALSE(is.headers[5].kontor_counter);
        CHECK(is.headers[5].records == 0);
        CHECK_FALSE(is.complete_local_records);
        CHECK(out.consumed == 1);
        CHECK(is.resources.size() == 1);
    }
    auto market = ore_base();
    append(market, packed("KONTORID", Buffer{2, 129}));
    append(market, packed("KONTID", Buffer{7, 0})); // Different record, not silently imported.
    const auto result = run(market);
    REQUIRE_FALSE(result);
    REQUIRE(result.error().diagnostic);
    CHECK(result.error().diagnostic->boundary->tag == "KONTID");
    CHECK(result.error().diagnostic->islands[0].kontor_counter == 33026);
    CHECK_FALSE(result.error().diagnostic->islands[0].headers.back().kontor_counter);
}

TEST_CASE("wildlife prefix: prod2 empty blocks own chronology without terrain epilogues") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = anbau_pe();
    auto b = ore_base();
    append(b, packed("OCCUR", ore_entry()));
    Buffer discover(4);
    put32(discover, 0, 0xabcd8001);
    append(b, packed("DISCOVER", discover));
    append(b, packed("ANBAU", anbau_payload({"AX"})));
    append(b, packed("KONTORID", Buffer{2, 129}));
    append(b, packed("PROD2", block(1, 2, 7, 9, {})));
    append(b, packed("FRUCHTBAR", Buffer(256, 2)));
    append(b, packed("PROD2", block(65535, 32768, 255, 0, {})));
    append(b, packed("HIRSCH", fmt::insel::encode_hirsch(
                                   std::vector<fmt::insel::HirschRecord>{{0, 0, 0x80, 7, 10}})));
    auto file = scene({packed("INSEL5", b)});
    const auto original = file;
    const auto result = load::prepare_wildlife_prefix(file, meta.entries, pe, *types, inputs(),
                                                      std::vector<std::uint32_t>{4, 7});
    REQUIRE(result);
    CHECK(file == original);
    file.clear();
    file.shrink_to_fit();
    b.clear();
    const auto& out = result.value();
    REQUIRE(out.islands.size() == 1);
    const auto& is = out.islands[0];
    REQUIRE(is.headers.size() == 12);
    REQUIRE(is.headers[8].prod2_empty);
    REQUIRE(is.headers[10].prod2_empty);
    CHECK(is.headers[8].prod2_empty->x == 1);
    CHECK(is.headers[8].prod2_empty->y == 2);
    CHECK(is.headers[8].prod2_empty->width == 7);
    CHECK(is.headers[8].prod2_empty->height == 9);
    CHECK(is.headers[10].prod2_empty->x == 65535);
    CHECK(is.headers[10].prod2_empty->y == 32768);
    CHECK(is.headers[10].prod2_empty->width == 255);
    CHECK(is.headers[10].prod2_empty->height == 0);
    CHECK(is.headers[8].source.header == 8);
    CHECK(is.headers[10].source.header == 10);
    CHECK(is.headers[8].records == 0);
    CHECK(is.headers[10].records == 0);
    CHECK_FALSE(is.headers[8].water_mask_updates);
    CHECK_FALSE(is.headers[10].water_mask_updates);
    CHECK_FALSE(is.headers[9].prod2_empty);
    CHECK(is.headers[9].fertility_updates.has_value());
    CHECK(is.terrain->fertility == Buffer(256, 2));
    CHECK(sim::terrain_matches(*is.terrain));
    CHECK(is.anbau_mask == 1);
    CHECK(is.discover_mask == 32769);
    CHECK(is.kontor_counter == 33026);
    REQUIRE(is.occurrences.size() == 1);
    CHECK(is.occurrences[0].quantity_88 == 512000);
    CHECK(is.occurrences[0].reserve == 1);
    CHECK(is.occurrence_loads.size() == 1);
    REQUIRE(is.spawns.size() == 1);
    CHECK(is.spawns[0].draw_before == 1);
    CHECK(is.spawns[0].timestamp == 8);
    CHECK(out.consumed == 2);
    CHECK(is.resources.size() == 1);
    CHECK(is.resources[0].draw_before == 0);
    CHECK(is.resource_list_nodes.size() == 5);
    CHECK(is.resource_list_nodes[0].mask == 17);
    CHECK(is.resource.variation[0] == 66);
    CHECK(is.resource.tiles.object(0) == 104);
    CHECK(is.resource.tiles.item_height(0) == 48);
    CHECK(out.complete_local_records);

    // Water without GELAENDE: PROD2 must NOT run its unrelated water-mask epilogue.
    Buffer water;
    for (const auto v : std::vector<std::uint16_t>{7, 11, 3, 2})
        fmt::put_u16_le(water, v);
    fmt::put_u32_le(water, 17);
    append(water, packed("INFO", Buffer(12, 0)));
    append(water, packed("HEIGHT", block(0, 0, 3, 2, Buffer(6, 0))));
    const auto absent = load::prepare_wildlife_prefix(scene({packed("INSEL5", water)}),
                                                      meta.entries, pe, *types, inputs(), {});
    REQUIRE(absent);
    CHECK_FALSE(absent.value().islands[0].headers.back().prod2_empty);
    append(water, packed("PROD2", block(0, 0, 3, 2, {})));
    const auto empty = load::prepare_wildlife_prefix(scene({packed("INSEL5", water)}), meta.entries,
                                                     pe, *types, inputs(), {});
    REQUIRE(empty);
    CHECK(empty.value().consumed == 0);
    const auto& ew = empty.value().islands[0];
    REQUIRE(ew.headers.back().prod2_empty);
    CHECK_FALSE(ew.headers.back().water_mask_updates);
    for (std::uint64_t i = 0; i < ew.resource.tiles.tile_count(); ++i) {
        CHECK(ew.resource.tiles.walk_mask(i) == 0);
        CHECK(ew.resource.tiles.walk_mask(i) ==
              absent.value().islands[0].resource.tiles.walk_mask(i));
        CHECK(ew.resource.tiles.object(i) == 0);
    }
}

TEST_CASE("wildlife prefix: prod2 nonempty boundary keeps earlier empty completion") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = fake_pe();
    auto b = ore_base();
    append(b, packed("PROD2", block(1, 2, 7, 9, {})));
    append(b, packed("PROD2", block(1, 2, 7, 9, object(104, 0xa53f))));
    append(b,
           packed("MYSTERY", {})); // Mutant wrongly completing PROD2 reaches this later boundary.
    const auto result =
        load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries, pe, *types,
                                      inputs(), std::vector<std::uint32_t>{4});
    REQUIRE_FALSE(result);
    REQUIRE(result.error().diagnostic);
    const auto& out = *result.error().diagnostic;
    REQUIRE(out.boundary);
    REQUIRE(out.islands.size() == 1);
    const auto& is = out.islands[0];
    REQUIRE(is.headers.size() >= 6);
    REQUIRE(is.headers[4].prod2_empty);
    CHECK(is.headers[4].records == 0);
    CHECK(is.headers[4].prod2_empty->x == 1);
    CHECK((out.boundary->tag == "PROD2" && out.boundary->source.header == 5 &&
           out.boundary->source.record == 0 &&
           out.boundary->reason == "unsupported nonempty PROD2 item" &&
           !is.headers[5].prod2_empty.has_value()));
    CHECK_FALSE(is.complete_local_records);
    CHECK(out.consumed == 1);
    CHECK(is.resources.size() == 1);
    CHECK(is.resource.tiles.object(0) == 104);
    CHECK(is.resource.variation[0] == 64);
}

TEST_CASE("wildlife prefix: prod2 contract refuses short headers tails and all nonempty items") {
    const auto types = ore_registry();
    Metadata meta;
    const auto pe = fake_pe();
    const auto run = [&](const Buffer& payload) {
        auto b = ore_base();
        append(b, packed("PROD2", block(1, 2, 7, 9, {})));
        append(b, packed("PROD2", payload));
        return load::prepare_wildlife_prefix(scene({packed("INSEL5", b)}), meta.entries, pe, *types,
                                             inputs(), std::vector<std::uint32_t>{4});
    };
    const auto check = [&](const Buffer& payload, std::string_view reason) {
        const auto result = run(payload);
        REQUIRE_FALSE(result);
        REQUIRE(result.error().diagnostic);
        const auto& out = *result.error().diagnostic;
        REQUIRE(out.boundary);
        CHECK(out.boundary->tag == "PROD2");
        CHECK(out.boundary->source.header == 5);
        CHECK(out.boundary->source.record == 0);
        CHECK(out.boundary->reason == reason);
        const auto& is = out.islands[0];
        CHECK(is.headers[4].prod2_empty.has_value());
        CHECK_FALSE(is.headers[5].prod2_empty);
        CHECK(is.headers[5].records == 0);
        CHECK_FALSE(is.complete_local_records);
        CHECK(out.consumed == 1);
        CHECK(is.resources.size() == 1);
    };
    for (std::size_t length = 0; length < 6; ++length)
        check(Buffer(length, 255), "block header");
    for (std::size_t tail = 1; tail < 6; ++tail)
        check(block(1, 2, 7, 9, Buffer(tail, 255)),
              "partial PROD2 item tail cannot preserve stream framing");
    for (const auto id : std::vector<std::uint16_t>{104, 800, 16383, 0xc068}) {
        auto data = object(id, 0xa53f);
        check(block(1, 2, 7, 9, data), "unsupported nonempty PROD2 item");
        data.push_back(255);
        check(block(1, 2, 7, 9, data), "unsupported nonempty PROD2 item");
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
    CHECK(out.boundary->source.header == 72);
    CHECK(out.boundary->source.record == 0);
    CHECK(out.boundary->tag == "EVENTINSEL");
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
    std::cout << "FERTILITY_REAL header=49 bytes=16384 changed=8160\n";
    REQUIRE(raw.records[50].name() == "BERGWERK2");
    CHECK(raw.records[50].payload.size() == 108);
    REQUIRE(is.bergwerk_entries.size() == 9);
    REQUIRE(is.resource_list_nodes.size() == 21);
    CHECK(is.headers[50].records == 9);
    CHECK(is.headers[50].bergwerk_nodes_added == 20);
    CHECK_FALSE(is.headers[51].bergwerk_nodes_added);
    // The installed payload is independently read here without publishing its records.
    const auto payload = raw.records[50].payload;
    std::vector<std::optional<std::uint64_t>> heads(is.resource_list_chunks.size());
    std::uint64_t node_index = 0;
    for (std::uint64_t i = 0; i < is.bergwerk_entries.size(); ++i) {
        const auto packed_word = fmt::u32_le(payload, i * 12 + 8);
        const auto expected_x = (packed_word % 4096 + raw.x) % 65536;
        const auto expected_y = ((packed_word / 4096) % 4096 + raw.y) % 65536;
        const auto expected_rotation = (packed_word / 16777216) % 4;
        const auto& e = is.bergwerk_entries[i];
        CHECK(e.world_x == expected_x);
        CHECK(e.world_y == expected_y);
        CHECK(e.local_x == packed_word % 4096);
        CHECK(e.local_y == (packed_word / 4096) % 4096);
        CHECK(e.rotation == expected_rotation);
        CHECK(e.source.header == 50);
        CHECK(e.source.record == i);
        const auto width = expected_rotation % 2 ? 4U : 3U;
        const auto height = expected_rotation % 2 ? 3U : 4U;
        for (auto by = expected_y - expected_y % 8; by < expected_y + height; by += 8)
            for (auto bx = expected_x - expected_x % 8; bx < expected_x + width; bx += 8) {
                const auto chunk = ((by - raw.y) / 8) * ((raw.width + 7U) / 8) + (bx - raw.x) / 8;
                REQUIRE(node_index < is.resource_list_nodes.size());
                const auto& n = is.resource_list_nodes[node_index];
                CHECK(n.entry == i);
                CHECK(n.chunk == chunk);
                CHECK(n.offsets == ((expected_x - bx) % 16 | ((expected_y - by) % 16) * 16));
                CHECK(n.rotation == expected_rotation);
                CHECK(n.mask == 17);
                CHECK(n.kind == 1);
                CHECK(n.next == heads[chunk]);
                heads[chunk] = node_index++;
            }
    }
    CHECK(node_index == 20);
    REQUIRE(is.occurrences.size() == 1);
    REQUIRE(is.occurrence_loads.size() == 1);
    const auto& ore = is.occurrences[0];
    REQUIRE(raw.records[51].name() == "OCCUR");
    const auto occur_payload = raw.records[51].payload;
    REQUIRE(occur_payload.size() == 32);
    CHECK(ore.type == (fmt::u32_le(occur_payload, 8) >> 14 & 4095U));
    CHECK(ore.goods == 1);
    CHECK(ore.reserve == 1);
    CHECK(ore.quantity_88 == 512000);
    CHECK(ore.file_tons == fmt::u16_le(occur_payload, 24));
    CHECK(ore.discovery == fmt::u32_le(occur_payload, 28));
    const float expected_x = std::bit_cast<float>(fmt::u32_le(occur_payload, 12)) + raw.x;
    const float expected_y = std::bit_cast<float>(fmt::u32_le(occur_payload, 16)) + raw.y;
    CHECK(ore.world_x_bits == std::bit_cast<std::uint32_t>(expected_x));
    CHECK(ore.world_y_bits == std::bit_cast<std::uint32_t>(expected_y));
    CHECK(ore.height_bits == fmt::u32_le(occur_payload, 20));
    CHECK(ore.tile_x == static_cast<std::int32_t>(expected_x));
    CHECK(ore.tile_y == static_cast<std::int32_t>(expected_y));
    const auto local_x = static_cast<std::uint32_t>(ore.tile_x - raw.x);
    const auto local_y = static_cast<std::uint32_t>(ore.tile_y - raw.y);
    const auto chunk = (local_y / 8) * ((raw.width + 7U) / 8) + local_x / 8;
    const auto& occurrence_node = is.resource_list_nodes[20];
    CHECK(occurrence_node.kind == 6);
    CHECK(occurrence_node.chunk == chunk);
    CHECK(occurrence_node.entry == 0);
    CHECK(occurrence_node.next == heads[chunk]);
    CHECK(occurrence_node.x_offset == local_x % 8);
    CHECK(occurrence_node.y_offset == local_y % 8);
    heads[chunk] = 20;
    CHECK(is.occurrence_loads[0].mask_updates == 20);
    CHECK(is.headers[51].occurrences_added == 1);
    CHECK(is.headers[51].records == 1);
    CHECK_FALSE(is.headers[52].occurrences_added);
    std::cout << "OCCUR_REAL header=51 loaded=1 nodes_added=1 mask_changes=20 draws=0\n";
    std::uint64_t active = 0;
    for (std::uint64_t i = 0; i < heads.size(); ++i) {
        CHECK(is.resource_list_chunks[i].head == heads[i]);
        CHECK(is.resource_list_chunks[i].resource_flags ==
              ((heads[i] ? 2 : 0) | (i == chunk ? 64 : 0)));
        active += heads[i].has_value();
    }
    CHECK(active == 8);
    std::cout << "BERGWERK_REAL header=50 entries=9 nodes=20 chunks=8\n";
    REQUIRE(raw.records[52].name() == "ANBAU");
    const auto anbau_payload_raw = raw.records[52].payload;
    CHECK(anbau_payload_raw.size() == 135);
    REQUIRE(is.headers[52].anbau);
    CHECK(is.headers[52].anbau->seed == 0xf009);
    CHECK(is.headers[52].anbau->mask == 0xffff);
    CHECK(is.anbau_mask == 0xffff);
    CHECK(is.headers[52].records == 13);
    CHECK(is.headers[52].anbau->ignored_bytes == 0);
    CHECK_FALSE(is.headers[53].anbau);
    REQUIRE(is.anbau_tokens.size() == 13);
    std::size_t token_at = 0;
    for (std::uint64_t i = 0; i < is.anbau_tokens.size(); ++i) {
        const auto remaining = anbau_payload_raw.subspan(token_at);
        const auto end = std::find(remaining.begin(), remaining.end(), std::uint8_t{0});
        REQUIRE(end != remaining.end());
        CHECK(is.anbau_tokens[i].name == std::string(remaining.begin(), end));
        CHECK(is.anbau_tokens[i].goods == 56 + i);
        CHECK(is.anbau_tokens[i].source.header == 52);
        CHECK(is.anbau_tokens[i].source.record == i);
        token_at += static_cast<std::size_t>(end - remaining.begin()) + 1;
    }
    CHECK(token_at == anbau_payload_raw.size());
    std::cout << "ANBAU_REAL header=52 tokens=13 seed=61449 mask=65535 ignored=0 draws=0 "
                 "next=72/EVENTINSEL\n";
    REQUIRE(raw.records[53].name() == "DISCOVER");
    REQUIRE(raw.records[53].payload.size() == 4);
    const auto expected_discovery = fmt::u16_le(raw.records[53].payload, 0);
    CHECK(expected_discovery == 0);
    CHECK(is.discover_mask == expected_discovery);
    CHECK(is.headers[53].discover == expected_discovery);
    CHECK(is.headers[53].records == 1);
    CHECK_FALSE(is.headers[54].discover);
    CHECK(raw.records[54].name() == "KONTORID");
    std::cout << "DISCOVER_REAL header=53 bytes=4 mask=0 draws=0 next=72/EVENTINSEL\n";
    REQUIRE(raw.records[54].name() == "KONTORID");
    REQUIRE(raw.records[54].payload.size() == 2);
    const auto expected_counter = fmt::u16_le(raw.records[54].payload, 0);
    CHECK(expected_counter == 1);
    CHECK(is.kontor_counter == expected_counter);
    CHECK(is.headers[54].kontor_counter == expected_counter);
    CHECK(is.headers[54].records == 1);
    CHECK_FALSE(is.headers[55].kontor_counter);
    CHECK(raw.records[55].name() == "PROD2");
    std::cout << "KONTORID_REAL header=54 bytes=2 counter=1 draws=0 next=72/EVENTINSEL\n";
    for (std::uint64_t h = 55; h <= 70; ++h) {
        REQUIRE(raw.records[h].name() == "PROD2");
        const auto payload_raw = raw.records[h].payload;
        REQUIRE(payload_raw.size() == 6);
        REQUIRE(is.headers[h].prod2_empty);
        CHECK(is.headers[h].prod2_empty->x == fmt::u16_le(payload_raw, 0));
        CHECK(is.headers[h].prod2_empty->y == fmt::u16_le(payload_raw, 2));
        CHECK(is.headers[h].prod2_empty->width == payload_raw[4]);
        CHECK(is.headers[h].prod2_empty->height == payload_raw[5]);
        CHECK(is.headers[h].records == 0);
        CHECK_FALSE(is.headers[h].water_mask_updates);
    }
    REQUIRE(raw.records[71].name() == "HIRSCH");
    const auto spawn_raw = raw.records[71].payload;
    REQUIRE(spawn_raw.size() == 1248);
    REQUIRE(is.spawns.size() == 156);
    std::vector<std::optional<std::uint64_t>> spawn_heads(is.chunk_heads.size());
    std::uint64_t cursor = 4589;
    for (std::uint64_t i = 0; i < is.spawns.size(); ++i) {
        const auto& spawn = is.spawns[i];
        const auto offset = i * 8;
        const auto x_raw = fmt::u16_le(spawn_raw, offset);
        const auto y_raw = fmt::u16_le(spawn_raw, offset + 2);
        const auto flags = spawn_raw[offset + 4];
        CHECK(spawn.raw.x == x_raw);
        CHECK(spawn.raw.y == y_raw);
        CHECK(spawn.raw.flags == flags);
        CHECK(spawn.raw.raw5 == spawn_raw[offset + 5]);
        CHECK(spawn.goods == fmt::u16_le(spawn_raw, offset + 6)); // Supplied mode 0.
        CHECK(spawn.source.header == 71);
        CHECK(spawn.source.record == i);
        CHECK(spawn.draw_before == cursor);
        CHECK(spawn.timestamp == 1 + ((flags & 1U) == 0 ? 7 : 0)); // Supplied stamp 500/draw 7.
        cursor += (flags & 1U) == 0;
        const auto expected_chunk = (y_raw / 8U) * 16 + x_raw / 8U;
        CHECK(spawn.chunk == expected_chunk);
        CHECK(spawn.next_in_chunk == spawn_heads[expected_chunk]);
        spawn_heads[expected_chunk] = i;
    }
    CHECK(cursor == 4745);
    CHECK(is.chunk_heads == spawn_heads);
    CHECK(is.headers[71].records == 156);
    CHECK(raw.records[72].name() == "EVENTINSEL");
    CHECK_FALSE(is.headers[72].prod2_empty);
    std::cout << "PROD2_REAL headers=16 items=0 extra_epilogues=0 draws=0 "
                 "HIRSCH=156 load_draws=156 next=72/EVENTINSEL\n";
    CHECK(out.islands[0].resources.size() == 4760);
    CHECK(out.islands[0].spawns.size() == 156);
    CHECK(out.consumed == 4745);
    REQUIRE(out.islands[0].headers.size() == 73);
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
