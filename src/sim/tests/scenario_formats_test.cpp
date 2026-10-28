// The scenario adapter on synthetic scenarios (docs/sim/state.md §10, stage S14):
// the HEAD byte map, a player series, one INSEL5 island with terrain, tile objects
// and building items, a STADT2 city with its records (T23: HEAD2, WARE, ID3,
// PROD/ENTRY), every warning kind, and the refusals. The .sze
// files are built here byte by byte (our own data, nothing from the original files);
// every expected value follows from the bytes written and the evidence the adapter
// cites, never from the adapter.
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/insel.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using Buf = std::vector<std::uint8_t>;
using Kind = sim::ScenarioWarningKind;

namespace {

void put16(Buf& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void put32(Buf& b, std::uint32_t v) {
    put16(b, v & 0xffffU);
    put16(b, v >> 16);
}
void append(Buf& b, const Buf& more) {
    b.insert(b.end(), more.begin(), more.end());
}

/// A chunk: 12-byte NUL-padded name, u32 length, payload (chunk-container.md).
Buf chunk(std::string_view name, const Buf& payload) {
    Buf b(name.begin(), name.end());
    b.resize(12, 0);
    put32(b, static_cast<std::uint32_t>(payload.size()));
    append(b, payload);
    return b;
}

/// A short-form tagged record: 6-byte name, u16 length, payload (sze.md).
Buf record(std::string_view name, const Buf& payload) {
    Buf b(name.begin(), name.end());
    b.resize(6, 0);
    put16(b, static_cast<std::uint32_t>(payload.size()));
    append(b, payload);
    return b;
}

/// The 32-byte SCENE/HEAD with every byte the handler reads set to a distinct value.
Buf head_bytes(std::uint32_t w = 300, std::uint32_t h = 200, std::uint8_t b0a = 7) {
    Buf b;
    put32(b, w);
    put32(b, h);
    b.push_back(2);     // [8] -> prop0
    b.push_back(5);     // [9] -> prop2
    b.push_back(b0a);   // [0xa] -> prop1 (0 -> 1)
    b.push_back(1);     // [0xb] -> prop10
    put16(b, 0x1234);   // [0xc] -> prop3
    put16(b, 0xeeee);   // [0xe] not read
    put16(b, 0x8401);   // [0x10] -> prop6
    b.resize(32, 0xcc); // [0x12..0x1f] not read
    return b;
}

/// A player HEAD (108 bytes): type, money, name, colour, flags at 0x4E.
Buf player_head(std::uint32_t kind, std::uint32_t money, std::string_view name, std::uint8_t colour,
                std::uint16_t flags4e = 0) {
    Buf b;
    put32(b, kind);
    put32(b, money);
    Buf n(name.begin(), name.end());
    n.resize(48, 0);
    append(b, n);
    b.resize(0x4c, 0);
    b.push_back(colour);
    b.push_back(0);
    put16(b, flags4e);
    b.resize(108, 0);
    return b;
}

Buf spieler(std::uint32_t index, const Buf& records) {
    Buf b;
    put32(b, 0);
    put32(b, index);
    append(b, records);
    return b;
}

/// An INSEL5 of w × h tiles (w, h <= 32: one block) at (x, y), number `no`, with
/// INFO (climate), HEIGHT, GFXID, FRUCHTBAR and the given extra records.
Buf insel5(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h, std::uint32_t no,
           std::uint8_t climate, const Buf& extra, bool with_info = true) {
    Buf p;
    put16(p, x);
    put16(p, y);
    put16(p, w);
    put16(p, h);
    put32(p, no);
    if (with_info) {
        Buf info(12, 0);
        info[0] = climate;
        append(p, chunk("INFO", info));
    }
    Buf height = {0, 0, 0, 0, static_cast<std::uint8_t>(w), static_cast<std::uint8_t>(h)};
    Buf gfx = height;
    for (std::uint32_t i = 0; i < w * h; ++i) {
        height.push_back(static_cast<std::uint8_t>(i % 7));
        put16(gfx, 0x1100 + (i % 5));
    }
    append(p, chunk("HEIGHT", height));
    append(p, chunk("GFXID", gfx));
    append(p, chunk("FRUCHTBAR", Buf(w * h, 3)));
    append(p, extra);
    return chunk("INSEL5", p);
}

/// One 6-byte placed item (insel.md): id | rotation << 14, state, x, y.
void item(Buf& b, std::uint32_t id, std::uint32_t rotation, std::uint32_t height,
          std::uint32_t phase, std::uint8_t x, std::uint8_t y) {
    put16(b, id | (rotation << 14));
    put16(b, (height << 8) | phase);
    b.push_back(x);
    b.push_back(y);
}

/// A PROD/ENTRY record: u16 x, y (island-relative), then the class's own bytes.
Buf entry(std::uint32_t x, std::uint32_t y, const Buf& rest = {}) {
    Buf e;
    put16(e, x);
    put16(e, y);
    append(e, rest);
    return record("ENTRY", e);
}

/// The owner grid of `n` tiles from row-major runs {owner, length}; the tiles after
/// the last run are neutral.
std::vector<std::uint32_t>
owner_grid(const std::vector<std::pair<std::uint32_t, std::uint32_t>>& runs, std::size_t n,
           std::uint32_t neutral) {
    std::vector<std::uint32_t> g;
    for (const auto& [owner, length] : runs) {
        g.insert(g.end(), length, owner);
    }
    g.resize(n, neutral);
    return g;
}

/// A block record (GELAENDE, PROD2, FLUESSE) at block origin (0, 0) holding `items`.
Buf block_record(std::string_view name, const Buf& items, std::uint8_t w = 32,
                 std::uint8_t h = 32) {
    Buf b = {0, 0, 0, 0, w, h};
    append(b, items);
    return chunk(name, b);
}

Buf scene(std::string_view root, const Buf& children) {
    return chunk(root, chunk("SCENE", children));
}

// The registry of the unit cases: ids that fit the 12 bits of an item.
constexpr std::uint32_t kRoad = 100;        // PRODTYPE 193: no list, a tile object
constexpr std::uint32_t kHouse = 101;       // PRODTYPE 97: the house list, a building
constexpr std::uint32_t kTree = 102;        // PRODTYPE 225: no list, scenery
constexpr std::uint32_t kUnknown = 777;     // not in the registry
constexpr std::uint32_t kWeaver = 103;      // a HANDWERK workshop: STOFFE from WOLLE
constexpr std::uint32_t kFarm = 104;        // a PLANTAGE farm: KORN from GETREIDE
constexpr std::uint32_t kBigHouse = 105;    // PRODTYPE 97 with a 2 x 2 footprint
constexpr std::uint32_t kMine = 106;        // a MINE of EISENERZ
constexpr std::uint32_t kSmallFarm = 107;   // UNIVERSAL: KORN from GETREIDE, WOLLE from GRAS
constexpr std::uint32_t kWeapons = 108;     // HANDWERKWAFFEN: BOGEN, ARMBRUST from HOLZ, SEILE
constexpr std::uint32_t kTrapper = 109;     // PELZJAEGER: PELZE from PELZTIER
constexpr std::uint32_t kInn = 110;         // PRODTYPE 66 WIRTSHAUS: class Inn, Load not mapped
constexpr std::uint32_t kMarketCombi = 111; // PRODTYPE 36 MARKTKOMBI: class MarketCombi (KONTID)

std::shared_ptr<const sim::TypeRegistry> registry() {
    sim::TypeRegistry::Builder b;
    b.add_plain(sim::TypeId{kRoad}, 193);
    b.add_plain(sim::TypeId{kHouse}, 97);
    b.add_plain(sim::TypeId{kTree}, 225);
    b.add_workshop(sim::TypeId{kWeaver}, "STOFFE", "WOLLE", 6, 11, 5);
    b.add_farm(sim::TypeId{kFarm}, "KORN", "GETREIDE", 37, 80, 5);
    b.add_mine(sim::TypeId{kMine}, "EISENERZ", 4, 6);
    b.add_small_farm(sim::TypeId{kSmallFarm}, "KORN", "WOLLE", "GETREIDE", "GRAS", 13, 4, 128, 50,
                     4);
    b.add_weapons_workshop(sim::TypeId{kWeapons}, "BOGEN", "ARMBRUST", "HOLZ", "SEILE", 9, 5);
    b.add_fur_trapper(sim::TypeId{kTrapper}, "PELZE", "PELZTIER", 26, 25, 5);
    b.add_plain(sim::TypeId{kInn},
                66); // class Inn (Inert, Load not mapped -> UndecodedBuildingRecord)
    b.add_plain(sim::TypeId{kMarketCombi}, 36); // class MarketCombi (the market KONTID reader)
    {
        sim::TypeRegistry::Builder one;
        one.add_plain(sim::TypeId{kBigHouse}, 97);
        auto r = one.build();
        REQUIRE(r.ok());
        sim::BuildingType big = *r.value()->find(sim::TypeId{kBigHouse});
        big.width = 2;
        big.height = 2;
        b.add(big);
    }
    auto r = b.build();
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return r.value();
}

sim::LoadedScenario load(const Buf& file) {
    auto r = sim::load_scenario(file, *registry(), sim::SimConfig{});
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

std::string refusal(const Buf& file) {
    auto r = sim::load_scenario(file, *registry(), sim::SimConfig{});
    REQUIRE_FALSE(r.ok());
    return r.error();
}

std::size_t warnings_of(const sim::LoadedScenario& s, Kind k) {
    std::size_t n = 0;
    for (const sim::ScenarioWarning& w : s.warnings) {
        n += w.kind == k ? 1U : 0U;
    }
    return n;
}

/// The scenario most cases use: player 0 (human) and 31 (type 2), one 16 × 8 island
/// number 7 at (40, 60) with a road, a tree, a house (a PROD2 building item at (8, 4))
/// and a city 2 of player 0 whose records are `city_records`.
Buf base_scene(const Buf& island_extra = {}, const Buf& state_records = {},
               const Buf& city_records = {}) {
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 25000, "Ada", 3)))));
    append(kids, chunk("SPIELER", spieler(31, record("HEAD", player_head(2, 10000, "", 0)))));
    Buf items;
    item(items, kRoad, 0, 4, 0, 1, 1);
    item(items, kTree, 1, 4, 3, 5, 2);
    Buf prod;
    item(prod, kHouse, 0, 4, 0, 8, 4);
    item(prod, kRoad, 2, 4, 1, 9, 6);
    Buf extra = block_record("GELAENDE", items);
    append(extra, block_record("PROD2", prod));
    append(extra, island_extra);
    append(kids, insel5(40, 60, 16, 8, 7, 2, extra));
    Buf city = {2, 0};
    put16(city, 50);
    put16(city, 63);
    append(city, city_records);
    Buf st = record("STADT2", city);
    append(st, state_records);
    append(kids, chunk("SPIELER", spieler(0, st)));
    return scene("ANNO", kids);
}

} // namespace

TEST_CASE("scenario HEAD byte map") {
    // D AnnoScene 0x10009cc0 (HEAD case): width u32 [0], height u32 [4]; property 0 =
    // [8], 1 = [0xa] (0 -> 1), 2 = [9], 3 = u16 [0xc], 6 = u16 [0x10], 10 = [0xb], 14 =
    // 1 for ANNO, 3 for ANNOADDON1.
    const sim::LoadedScenario s = load(base_scene());
    const sim::WorldSettings& w = s.spec.settings;
    CHECK(w.world_width == 300);
    CHECK(w.world_height == 200);
    CHECK(w.prop0 == 2);
    CHECK(w.prop2 == 5);
    CHECK(w.prop1 == 7);
    CHECK(w.prop10 == 1);
    CHECK(w.prop3 == 0x1234);
    CHECK(w.prop6 == 0x8401); // a u16: the high byte counts
    CHECK(w.prop14 == 1);
    // [0xa] = 0 reads as 1; the add-on root gives property 14 = 3.
    Buf kids = chunk("HEAD", head_bytes(64, 64, 0));
    const sim::LoadedScenario z = load(scene("ANNOADDON1", kids));
    CHECK(z.spec.settings.prop1 == 1);
    CHECK(z.spec.settings.prop14 == 3);
    CHECK(z.spec.settings.world_width == 64);
    // The handler reads HEAD into a zero-filled 32-byte buffer: a shorter record reads
    // as zeros after its end (17 bytes: prop6 = u16 {0x01, 0}); a longer one is refused.
    Buf h17 = head_bytes();
    h17.resize(17);
    CHECK(load(scene("ANNO", chunk("HEAD", h17))).spec.settings.prop6 == 0x0001);
    CHECK(load(scene("ANNO", chunk("HEAD", Buf{}))).spec.settings.prop1 == 1);
    Buf h33 = head_bytes();
    h33.push_back(0);
    CHECK(refusal(scene("ANNO", chunk("HEAD", h33))) ==
          "scene.HEAD: 33 bytes, longer than the reader's 32-byte buffer");
    Buf h18 = head_bytes(70000, 5);
    h18.resize(18);
    const sim::LoadedScenario e = load(scene("ANNO", chunk("HEAD", h18)));
    CHECK(e.spec.settings.world_width == 70000); // u32, not u16
    CHECK(e.spec.settings.prop6 == 0x8401);
    // No HEAD at all.
    CHECK(refusal(scene("ANNO", chunk("SKTIMER", Buf(40, 0)))) == "scene.HEAD: missing");
}

TEST_CASE("scenario player series") {
    // HEAD (D AnnoWelt 0x1001d2b9–0x1001d430): type (u32 read as u16), money +0x74 (i32),
    // name up to the NUL, colour [0x4c], flags u16 [0x4e] (& 0xFFBF, >= 0x100 read as 0);
    // VOLK (D 0x1001d657–0x1001d753): people 1 for 8/11/12, 11 for 15, 12 for 16 (15 and 16
    // also set bits 0x19 and colour 0); other types: the untraced name list (a warning).
    Buf kids = chunk("HEAD", head_bytes());
    Buf p0 = record("HEAD", player_head(0x10008, 25000, std::string_view("Ada\0xy", 6), 3, 0x00ff));
    append(p0, record("VOLK", Buf{'X', 0}));
    append(kids, chunk("SPIELER", spieler(0, p0)));
    Buf p1 = record("HEAD", player_head(11, 0xffffff9cU, "B", 2, 0x0100));
    append(p1, record("VOLK", Buf{'X', 0}));
    append(kids, chunk("SPIELER", spieler(1, p1)));
    Buf p2 = record("HEAD", player_head(12, 7, "C", 0xff, 0x00ff));
    append(p2, record("VOLK", Buf{'X', 0}));
    append(kids, chunk("SPIELER", spieler(2, p2)));
    Buf p4 = record("HEAD", player_head(15, 1000, "Pir", 5, 0x0120));
    append(p4, record("VOLK", Buf{'P', 0}));
    append(kids, chunk("SPIELER", spieler(4, p4)));
    Buf p9 = record("HEAD", player_head(16, 1, "V", 6, 0x40));
    append(p9, record("VOLK", Buf{'V', 0}));
    append(kids, chunk("SPIELER", spieler(9, p9)));
    append(kids, chunk("SPIELER", spieler(10, record("HEAD", player_head(16, 1, "W", 6, 0x40)))));
    Buf p11 = record("HEAD", player_head(14, 1, "N", 0));
    append(p11, record("VOLK", Buf{'N', 0}));
    append(kids, chunk("SPIELER", spieler(11, p11)));
    const sim::LoadedScenario s = load(scene("ANNO", kids));
    REQUIRE(s.spec.players.size() == 7);
    const auto& a = s.spec.players[0];
    CHECK(a.id == sim::PlayerId{0});
    CHECK(a.kind == 8); // 0x10008 read as u16
    CHECK(a.money == 25000);
    CHECK(a.name == std::vector<std::uint8_t>{'A', 'd', 'a'}); // up to the NUL
    CHECK(a.colour == 3);
    CHECK(a.flags_a0 == 0xbf); // 0xff & 0xffbf
    CHECK(a.volk == 1);
    const auto& b = s.spec.players[1];
    CHECK(b.kind == 11);
    CHECK(b.money == -100); // 0xffffff9c as i32
    CHECK(b.flags_a0 == 0); // 0x100 >= 0x100 reads as 0
    CHECK(b.volk == 1);
    const auto& c = s.spec.players[2];
    CHECK(c.kind == 12);
    CHECK(c.colour == 0xff);
    CHECK(c.volk == 1);
    const auto& p = s.spec.players[3];
    CHECK(p.id == sim::PlayerId{4});
    CHECK(p.flags_a0 == 0x19); // 0x120 >= 0x100 reads as 0, then | 0x19
    CHECK(p.volk == 11);
    CHECK(p.colour == 0); // VOLK sets colour 0 for type 15
    const auto& v = s.spec.players[4];
    CHECK(v.volk == 12);
    CHECK(v.flags_a0 == 0x19); // 0x40 & 0xffbf = 0, then | 0x19
    CHECK(v.colour == 0);      // VOLK sets colour 0 for type 16
    const auto& w = s.spec.players[5];
    CHECK(w.volk == 1);     // no VOLK record: the constructor's value stays
    CHECK(w.flags_a0 == 0); // 0x40 & 0xffbf
    CHECK(w.colour == 6);
    // Only the type-14 player's VOLK is undecoded; 8, 11, 12, 15, 16 are not.
    CHECK(warnings_of(s, Kind::UndecodedPeople) == 1);
    CHECK(s.spec.players[6].volk == 1);
    CHECK(warnings_of(s, Kind::ComputerNameNotDrawn) == 0); // the computer has a name
}

TEST_CASE("scenario island with terrain, tile objects and a building") {
    const sim::LoadedScenario s = load(base_scene());
    REQUIRE(s.spec.islands.size() == 1);
    const auto& is = s.spec.islands[0];
    CHECK(is.slot == sim::IslandId{7}); // the island number (D AnnoWelt 0x100a19a0)
    CHECK(is.x0 == 40);
    CHECK(is.y0 == 60);
    CHECK(is.w == 16);
    CHECK(is.h == 8);
    CHECK(is.climate == 2); // INFO[0]
    REQUIRE(is.terrain != nullptr);
    CHECK(is.terrain->height.size() == 128);
    CHECK(is.terrain->height[9] == 9 % 7);
    CHECK(is.terrain->gfx.size() == 128);
    CHECK(is.terrain->gfx[3] == 0x1103);
    CHECK(is.terrain->fertility == std::vector<std::uint8_t>(128, 3));
    // GELAENDE road at (1, 1) and tree at (5, 2); PROD2 road at (9, 6). The house
    // (a building item) is named by no city's PROD/ENTRY: not placed.
    REQUIRE(is.objects.size() == 3);
    CHECK(is.objects[0].index == 1 * 16 + 1);
    CHECK(is.objects[0].object == kRoad);
    CHECK(is.objects[1].index == 2 * 16 + 5);
    CHECK(is.objects[1].object == kTree);
    CHECK(is.objects[1].phase == 3);
    CHECK(is.objects[2].index == 6 * 16 + 9);
    CHECK(is.objects[2].phase == 1);
    CHECK(is.owner_runs.empty()); // the city has no ID3: every tile neutral
    CHECK(is.deposits.empty());
    REQUIRE(is.cities.size() == 1);
    CHECK(is.cities[0].buildings.empty());
    CHECK(warnings_of(s, Kind::BuildingItemWithoutEntry) == 1);
    CHECK(warnings_of(s, Kind::TileOwnersUndecoded) == 0);
}

TEST_CASE("scenario STADT2 city") {
    // STADT2 prefix (D AnnoWelt 0x1001e294): u8 city number, u8 0, u16 x, u16 y (a
    // world tile of the city); the owner is the enclosing block's player.
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, chunk("SPIELER", spieler(5, record("HEAD", player_head(8, 1, "B", 1)))));
    append(kids, insel5(40, 60, 16, 8, 7, 2, {}));
    Buf city = {2, 0};
    put16(city, 55);
    put16(city, 67);
    append(kids, chunk("SPIELER", spieler(5, record("STADT2", city))));
    const sim::LoadedScenario s = load(scene("ANNO", kids));
    REQUIRE(s.spec.islands.size() == 1);
    REQUIRE(s.spec.islands[0].cities.size() == 1);
    const auto& c = s.spec.islands[0].cities[0];
    CHECK(c.no == sim::CityNo{2});
    CHECK(c.owner == sim::PlayerId{5});
    CHECK_FALSE(c.stock.has_value());    // no WARE: World::create's empty stock
    CHECK_FALSE(c.capacity.has_value()); // no HEAD2: 50 t
    CHECK(c.buildings.empty());
    CHECK(s.spec.islands[0].owner_runs.empty());
    CHECK(warnings_of(s, Kind::UnknownRecord) == 0);
}

TEST_CASE("scenario city records: HEAD2, WARE and ID3") {
    // HEAD2 (D AnnoWelt 0x1002e383–0x1002e3e5): a zero-filled 64-byte buffer, u16 [0x24]
    // << 8 is the capacity, 0 -> 50 t. WARE (0x1002e7ca–0x1002e84b): length / 8 entries
    // {u16 goods in the file's numbering, u16 tons, 4 bytes not read}, the goods mapped
    // by its WARETYPE name (0x100a18d0), a mapped 0 skipped, stock = tons << 8. ID3
    // (0x1002e709–0x1002e7c5, island +0x100 = 0x10085620): u32 x | y << 15, island-
    // relative; bit 30: 8 mask bytes follow, row j bit i = tile (x + i, y + j); without
    // a mask the whole 8 x 8 chunk.
    auto with = [](const Buf& city_records) {
        Buf waretype;
        auto name = [&waretype](std::uint32_t number, std::string_view n) {
            put16(waretype, number);
            waretype.insert(waretype.end(), n.begin(), n.end());
            waretype.push_back(0);
        };
        name(0, "UNUSED");
        name(7, "WOLLE");
        name(9, "NOSUCHGOODS");
        Buf kids = chunk("HEAD", head_bytes());
        append(kids, chunk("WARETYPE", waretype));
        append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
        append(kids, insel5(40, 60, 16, 8, 7, 2, {}));
        Buf c = {2, 0};
        put16(c, 41);
        put16(c, 61);
        append(c, city_records);
        append(kids, chunk("SPIELER", spieler(0, record("STADT2", c))));
        return scene("ANNO", kids);
    };
    Buf head2(0x26, 0);
    head2[0x24] = 80;
    Buf ware;
    auto goods = [&ware](std::uint32_t number, std::uint32_t tons) {
        put16(ware, number);
        put16(ware, tons);
        put32(ware, 0xababababU);
    };
    goods(7, 12);                       // WOLLE: 12 t
    goods(0, 99);                       // UNUSED: skipped without a warning
    goods(9, 5);                        // a name the registry does not know: UnknownGoods
    goods(12, 5);                       // a number without a WARETYPE name: UnknownGoods
    ware.resize(ware.size() + 7, 0xcd); // a tail shorter than an entry: not read
    Buf id3;
    put32(id3, 0);               // the whole chunk at (0, 0)
    put32(id3, 8 | 0x40000000U); // (8, 0) with a mask
    append(id3, Buf{0x01, 0x03, 0, 0, 0, 0, 0, 0x80});
    Buf recs = record("HEAD2", head2);
    append(recs, record("WARE", ware));
    append(recs, record("ID3", id3));
    const sim::LoadedScenario s = load(with(recs));
    REQUIRE(s.spec.islands.size() == 1);
    REQUIRE(s.spec.islands[0].cities.size() == 1);
    const auto& c = s.spec.islands[0].cities[0];
    CHECK(c.capacity == std::optional<std::uint32_t>{80U << 8});
    REQUIRE(c.stock.has_value());
    CHECK(c.stock->size() == sim::SimConfig{}.warehouse_goods);
    const auto wolle = registry()->goods_by_name("WOLLE");
    REQUIRE(wolle.has_value());
    for (std::size_t g = 0; g < c.stock->size(); ++g) {
        INFO(g);
        CHECK((*c.stock)[g] == (g == *wolle ? 12U << 8 : 0U));
    }
    CHECK(warnings_of(s, Kind::UnknownGoods) == 2);
    CHECK(warnings_of(s, Kind::UnknownRecord) == 0); // WARETYPE is read
    // The owner grid: the chunk (0..7, 0..7) and the masked tiles (8, 0), (8, 1), (9, 1),
    // (15, 7) belong to city 2; the trailing neutral tiles are left out of the runs.
    const std::uint32_t neutral = sim::SimConfig{}.neutral_owner;
    std::vector<std::uint32_t> want(16 * 8, neutral);
    for (std::size_t y = 0; y < 8; ++y) {
        for (std::size_t x = 0; x < 8; ++x) {
            want[y * 16 + x] = 2;
        }
    }
    want[8] = 2;
    want[16 + 8] = 2;
    want[16 + 9] = 2;
    want[7 * 16 + 15] = 2;
    const auto& runs = s.spec.islands[0].owner_runs;
    CHECK(owner_grid(runs, want.size(), neutral) == want);
    REQUIRE_FALSE(runs.empty());
    CHECK(runs.front() == std::pair<std::uint32_t, std::uint32_t>{2, 9});
    CHECK(runs.back() == std::pair<std::uint32_t, std::uint32_t>{2, 1});
    // The world takes the capacity, the stock and the owners.
    auto w = sim::World::create(s.spec, registry());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    CHECK(w.value().check_invariants() == "");
    REQUIRE(w.value().islands()[7].has_value());
    const sim::Island& island = *w.value().islands()[7];
    const sim::City* city = island.city(sim::CityNo{2});
    REQUIRE(city != nullptr);
    CHECK(city->stock.capacity == 80U << 8);
    CHECK(city->stock.stock[*wolle] == 12U << 8);
    CHECK(island.tiles.owner(7 * 16 + 15) == 2);
    CHECK(island.tiles.owner(7 * 16 + 14) == neutral);
    // HEAD2 [0x24] = 0 and a short HEAD2 (zero-filled) give 50 t.
    for (const Buf& h : {Buf(0x26, 0), Buf(4, 0xff)}) {
        const sim::LoadedScenario z = load(with(record("HEAD2", h)));
        CHECK(z.spec.islands[0].cities[0].capacity == std::optional<std::uint32_t>{50U * 256U});
    }
    // ID2 is ID3 in world tiles: (48, 60) is island tile (8, 0).
    Buf id2;
    put32(id2, 48 | (60U << 15) | 0x40000000U);
    append(id2, Buf{0x01, 0, 0, 0, 0, 0, 0, 0});
    const sim::LoadedScenario v = load(with(record("ID2", id2)));
    CHECK(v.spec.islands[0].owner_runs ==
          std::vector<std::pair<std::uint32_t, std::uint32_t>>{{neutral, 8}, {2, 1}});
    // A truncated entry or mask is refused.
    CHECK(refusal(with(record("ID3", Buf{1, 2, 3}))) ==
          "players[1].STADT2.ID3: a truncated entry at byte 0");
    Buf cut;
    put32(cut, 0x40000000U);
    append(cut, Buf{1, 2, 3, 4});
    CHECK(refusal(with(record("ID3", cut))) == "players[1].STADT2.ID3: a truncated mask at byte 4");
}

TEST_CASE("scenario city PROD/ENTRY creates the buildings") {
    // D AnnoWelt 0x100264a0: per ENTRY u16 x, y island-relative; island +0x84
    // (0x10083a90) gives the building whose footprint holds that tile, created with
    // its type, origin, rotation and height byte (+0x0f, 0x10071c40) of the item; the
    // bytes after the position go to the class's Load (slot +0x7c). Workshop
    // 0x10068640: length <= 10, [1] & 1 the switch, [2] output, [4] input 0, [6]
    // input 1, [8] activity. Farm 0x1006ef70: exactly 10 bytes, [1] & 1, [2] output,
    // [4] raw stock, [8] activity ([6] not read); another length skips the record
    // whole. Mine 0x1006e310: [1] & 1, [2] output, [8] activity, over Create's default (the deep
    // flag of the type attach stays). Then the footprint claim (0x10026685): the
    // building's tiles still neutral get the city.
    Buf prod2;
    item(prod2, kHouse, 0, 4, 0, 8, 4);
    item(prod2, kWeaver, 1, 6, 0, 12, 2);
    item(prod2, kFarm, 0, 3, 0, 13, 5);
    item(prod2, kFarm, 0, 2, 0, 14, 6);
    item(prod2, kBigHouse, 0, 5, 0, 2, 5);
    item(prod2, kHouse, 0, 1, 0, 15, 0); // named by no ENTRY
    item(prod2, kMine, 0, 0, 0, 10, 7);
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
    append(kids, insel5(40, 60, 16, 8, 7, 2, block_record("PROD2", prod2)));
    Buf id3;
    put32(id3, 0); // the chunk (0..7, 0..7)
    Buf prod = entry(8, 4);
    append(prod, entry(12, 2, Buf{0, 0, 0x80, 0x01, 0x40, 0x02, 0x77, 0x77, 5, 0}));
    append(prod, record("XYZ", Buf(2, 0))); // not an ENTRY: skipped (0x1002671d)
    append(prod, entry(13, 5, Buf{0, 0, 0x00, 0x03, 0x00, 0x01, 0x77, 0x77, 9, 0}));
    append(prod, entry(14, 6, Buf(11, 0x55))); // 11 bytes: the farm skips them whole
    append(prod, entry(3, 6));                 // a tile of the big house: its origin (2, 5)
    append(prod, entry(10, 7, Buf{0, 0, 0x00, 0x02, 0x11, 0x11, 0x22, 0x22, 3, 0}));
    Buf c = {2, 0};
    put16(c, 41);
    put16(c, 61);
    append(c, record("ID3", id3));
    append(c, record("STAT2", Buf(4, 0))); // a city record the adapter does not map
    append(c, record("PROD", prod));
    append(kids, chunk("SPIELER", spieler(0, record("STADT2", c))));
    const sim::LoadedScenario s = load(scene("ANNO", kids));
    REQUIRE(s.spec.islands.size() == 1);
    REQUIRE(s.spec.islands[0].cities.size() == 1);
    const auto& b = s.spec.islands[0].cities[0].buildings;
    REQUIRE(b.size() == 6); // in ENTRY order
    auto at = [&b](std::size_t i, std::uint32_t type, std::int32_t x, std::int32_t y,
                   std::uint8_t rotation, std::uint8_t byte0f) {
        INFO(i);
        CHECK(b[i].type == sim::TypeId{type});
        CHECK(b[i].x == x);
        CHECK(b[i].y == y);
        CHECK(b[i].rotation == rotation);
        CHECK(b[i].byte0f == byte0f);
    };
    at(0, kHouse, 48, 64, 0, 4);
    at(1, kWeaver, 52, 62, 1, 6);
    at(2, kFarm, 53, 65, 0, 3);
    at(3, kFarm, 54, 66, 0, 2);
    at(4, kBigHouse, 42, 65, 0, 5);
    at(5, kMine, 50, 67, 0, 0);
    CHECK_FALSE(b[0].payload.has_value()); // no bytes: World::create's default
    REQUIRE(b[1].payload.has_value());
    REQUIRE(b[2].payload.has_value());
    REQUIRE(b[3].payload.has_value());
    CHECK_FALSE(b[4].payload.has_value());
    const auto* weaver = std::get_if<sim::Workshop>(&*b[1].payload);
    REQUIRE(weaver != nullptr);
    CHECK_FALSE(weaver->switched_on);
    CHECK(weaver->out == std::vector<std::uint32_t>{0x0180});
    CHECK(weaver->in == std::vector<std::uint32_t>{0x0240}); // input 1: the weaver has one
    CHECK(weaver->activity == std::vector<std::uint32_t>{5});
    const auto* farm = std::get_if<sim::Farm>(&*b[2].payload);
    REQUIRE(farm != nullptr);
    CHECK_FALSE(farm->switched_on);
    CHECK(farm->out == 0x0300);
    CHECK(farm->raw == 0x0100);
    CHECK(farm->activity == 9);
    const auto* skipped = std::get_if<sim::Farm>(&*b[3].payload);
    REQUIRE(skipped != nullptr);
    CHECK(skipped->switched_on);
    CHECK(skipped->out == 0);
    CHECK(skipped->raw == 0);
    CHECK(skipped->activity == 0);
    REQUIRE(b[5].payload.has_value());
    const auto* mine = std::get_if<sim::Mine>(&*b[5].payload);
    REQUIRE(mine != nullptr);
    CHECK_FALSE(mine->switched_on);
    CHECK(mine->out == 0x0200);
    CHECK(mine->activity == 3);
    const auto types = registry(); // keeps the type alive
    const sim::BuildingType& mt = *types->find(sim::TypeId{kMine});
    CHECK(mine->deep == sim::mine_is_deep(mt.prod, mt.research_id));
    CHECK(warnings_of(s, Kind::BuildingItemWithoutEntry) == 1); // the house at (15, 0)
    CHECK(warnings_of(s, Kind::UnknownRecord) == 1);            // PROD.XYZ (STAT2 is its own kind)
    CHECK(warnings_of(s, Kind::StatisticsNotModelled) == 1);    // STAT2 (T26)
    CHECK(warnings_of(s, Kind::UndecodedBuildingRecord) == 0);
    CHECK(warnings_of(s, Kind::BuildingRefused) == 0);
    CHECK(warnings_of(s, Kind::EntryWithoutBuildingItem) == 0);
    // The owners: the ID3 chunk plus the footprints outside it.
    const std::uint32_t neutral = sim::SimConfig{}.neutral_owner;
    std::vector<std::uint32_t> want(16 * 8, neutral);
    for (std::size_t y = 0; y < 8; ++y) {
        for (std::size_t x = 0; x < 8; ++x) {
            want[y * 16 + x] = 2;
        }
    }
    for (const auto& [x, y] : {std::pair{8, 4}, std::pair{12, 2}, std::pair{13, 5},
                               std::pair{14, 6}, std::pair{10, 7}}) {
        want[static_cast<std::size_t>(y * 16 + x)] = 2;
    }
    CHECK(owner_grid(s.spec.islands[0].owner_runs, want.size(), neutral) == want);
    // The world holds the six buildings.
    auto w = sim::World::create(s.spec, registry());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    CHECK(w.value().check_invariants() == "");
    REQUIRE(w.value().islands()[7].has_value());
    const sim::City* city = w.value().islands()[7]->city(sim::CityNo{2});
    REQUIRE(city != nullptr);
    std::size_t placed = 0;
    for (const sim::BuildingList& l : city->index.lists) {
        for (const auto& slot : l.slots) {
            placed += slot.has_value() ? 1U : 0U;
        }
    }
    CHECK(placed == 6);
}

TEST_CASE("scenario city class records: two-line classes and the length rules") {
    // Small farm 0x1006fc80 (own length into 14 bytes, no test): per line i [2 + 2i]
    // output, [6 + 2i] raw stock, [10 + 2i] activity, ([1] >> i) & 1 the switch.
    // Weapons workshop 0x10069140 (the same buffer): [2 + 2i] output, [6 + 2i] input,
    // [10 + 2i] activity, [1] & (1 << i). Fur trapper: the farm's Load 0x1006ef70.
    // Length rules: a farm record of another length than 10 and a workshop record
    // over 10 are skipped whole (the default stays, no warning); a mine record over
    // 10 or a small farm record over 14 would overrun the original's buffer
    // (UndecodedBuildingRecord, once per class).
    Buf prod2;
    item(prod2, kSmallFarm, 0, 0, 0, 0, 0);
    item(prod2, kWeapons, 0, 0, 0, 2, 0);
    item(prod2, kTrapper, 0, 0, 0, 4, 0);
    item(prod2, kFarm, 0, 0, 0, 6, 0);
    item(prod2, kWeaver, 0, 0, 0, 8, 0);
    item(prod2, kMine, 0, 0, 0, 10, 0);
    item(prod2, kSmallFarm, 0, 0, 0, 12, 0);
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
    append(kids, insel5(40, 60, 16, 8, 7, 2, block_record("PROD2", prod2)));
    Buf prod =
        entry(0, 0, Buf{0, 0x02, 0x10, 0x01, 0x20, 0x01, 0x30, 0x01, 0x40, 0x01, 7, 0, 8, 0});
    append(prod,
           entry(2, 0, Buf{0, 0x01, 0x10, 0x02, 0x20, 0x02, 0x30, 0x02, 0x40, 0x02, 3, 0, 4, 0}));
    append(prod, entry(4, 0, Buf{0, 0, 0x50, 0, 0x60, 0, 0x77, 0x77, 6, 0}));
    append(prod, entry(6, 0, Buf{0, 0, 1, 1, 1, 1, 1, 1, 1}));       // 9 bytes: skipped
    append(prod, entry(8, 0, Buf{0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1})); // 11 bytes: skipped
    append(prod, entry(10, 0, Buf(11, 1)));                          // over the mine's 10
    append(prod, entry(12, 0, Buf(15, 1)));                          // over the small farm's 14
    Buf c = {2, 0};
    put16(c, 41);
    put16(c, 61);
    append(c, record("PROD", prod));
    append(kids, chunk("SPIELER", spieler(0, record("STADT2", c))));
    const sim::LoadedScenario s = load(scene("ANNO", kids));
    const auto& b = s.spec.islands[0].cities[0].buildings;
    REQUIRE(b.size() == 7);
    for (const auto& bs : b) {
        REQUIRE(bs.payload.has_value());
    }
    const auto* sf = std::get_if<sim::SmallFarm>(&*b[0].payload);
    REQUIRE(sf != nullptr);
    CHECK(sf->out == std::vector<std::uint32_t>{0x0110, 0x0120});
    CHECK(sf->raw == std::vector<std::uint32_t>{0x0130, 0x0140});
    CHECK(sf->activity == std::vector<std::uint16_t>{7, 8});
    CHECK(sf->line_on == std::vector<bool>{false, true});
    const auto* ww = std::get_if<sim::WeaponsWorkshop>(&*b[1].payload);
    REQUIRE(ww != nullptr);
    CHECK(ww->out == std::vector<std::uint32_t>{0x0210, 0x0220});
    CHECK(ww->in == std::vector<std::uint32_t>{0x0230, 0x0240});
    CHECK(ww->activity == std::vector<std::uint16_t>{3, 4});
    CHECK(ww->line_on == std::vector<bool>{true, false});
    const auto* tr = std::get_if<sim::Farm>(&*b[2].payload);
    REQUIRE(tr != nullptr);
    CHECK_FALSE(tr->switched_on);
    CHECK(tr->out == 0x50);
    CHECK(tr->raw == 0x60);
    CHECK(tr->activity == 6);
    const auto* farm = std::get_if<sim::Farm>(&*b[3].payload);
    REQUIRE(farm != nullptr);
    CHECK(farm->switched_on);
    CHECK(farm->out == 0);
    CHECK(farm->activity == 0);
    const auto* weaver = std::get_if<sim::Workshop>(&*b[4].payload);
    REQUIRE(weaver != nullptr);
    CHECK(weaver->switched_on);
    CHECK(weaver->out == std::vector<std::uint32_t>{0});
    const auto* mine = std::get_if<sim::Mine>(&*b[5].payload);
    REQUIRE(mine != nullptr);
    CHECK(mine->switched_on);
    CHECK(mine->out == 0);
    const auto* sf2 = std::get_if<sim::SmallFarm>(&*b[6].payload);
    REQUIRE(sf2 != nullptr);
    CHECK(sf2->out == std::vector<std::uint32_t>{0, 0});
    CHECK(warnings_of(s, Kind::UndecodedBuildingRecord) == 2); // the mine and the small farm
    auto w = sim::World::create(s.spec, registry());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    CHECK(w.value().check_invariants() == "");
}

TEST_CASE("house record reading (pure functions, T26)") {
    // The house population formula (D AnnoWelt 0x1007702b–0x10077119, population.md §2):
    // stock = trunc(Menge · s · residents · 128 / 1536); 128 = 100 %. Hand-derived:
    // Menge 82 (2.00 t stored × 40.96), residents 5, s 128 -> 82·128·5·128/1536 = 4373;
    // s 64 -> 2186; s 0 -> 0.
    CHECK(sim::house_need_stock(82, 128, 5) == 4373);
    CHECK(sim::house_need_stock(82, 64, 5) == 2186);
    CHECK(sim::house_need_stock(82, 0, 5) == 0);
    CHECK(sim::house_need_stock(82, 128, 0) == 0);
    // The satisfaction bytes by version (0x10076f34 / 0x10076f69 / 0x1007702b): version 2
    // one byte per need, version 1 the SECOND byte of each pair, version 0 none.
    const Buf v2 = {128, 64, 0, 160, 5, 6};
    CHECK(sim::house_satisfaction(2, 6, v2) == std::vector<std::uint32_t>{128, 64, 0, 160, 5, 6});
    const Buf v1 = {1, 128, 2, 64, 3, 0}; // pairs: second bytes 128, 64, 0
    CHECK(sim::house_satisfaction(1, 3, v1) == std::vector<std::uint32_t>{128, 64, 0});
    CHECK(sim::house_satisfaction(0, 6, v2).empty());
    // A byte past the block reads as 0.
    CHECK(sim::house_satisfaction(2, 3, Buf{7}) == std::vector<std::uint32_t>{7, 0, 0});
    // The reset rule (0x10076e9e–0x10076ecb): group above pioneers and residents below
    // half the lower group's Wohnmax.
    CHECK_FALSE(sim::house_reset_applies(0, 1, 20));  // pioneers never reset
    CHECK(sim::house_reset_applies(1, 5, 20));        // 5 < 10
    CHECK_FALSE(sim::house_reset_applies(1, 10, 20)); // 10 < 10 is false
    CHECK_FALSE(sim::house_reset_applies(1, 11, 20));
}

// A scene with a WARETYPE goods table and one city 2 (player 0) of island 7 at (40, 60),
// holding `city_records`. Used by the HEAD2/BAUMAT/VK/EK and the market cases.
namespace {
Buf t26_city_scene(const std::vector<std::pair<std::uint32_t, std::string_view>>& waretype,
                   const Buf& city_records, const Buf& prod2 = {}) {
    Buf wt;
    for (const auto& [number, n] : waretype) {
        put16(wt, number);
        wt.insert(wt.end(), n.begin(), n.end());
        wt.push_back(0);
    }
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, chunk("WARETYPE", wt));
    append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
    append(kids, insel5(40, 60, 16, 8, 7, 2, prod2.empty() ? Buf{} : block_record("PROD2", prod2)));
    Buf c = {2, 0};
    put16(c, 50); // island tile (10, 3)
    put16(c, 63);
    append(c, city_records);
    append(kids, chunk("SPIELER", spieler(0, record("STADT2", c))));
    return scene("ANNO", kids);
}
} // namespace

TEST_CASE("scenario city HEAD2 slot counts, statistics and fields, BAUMAT, VK and EK (T26)") {
    const auto wolle = registry()->goods_by_name("WOLLE");
    const auto holz = registry()->goods_by_name("HOLZ");
    REQUIRE(wolle.has_value());
    REQUIRE(holz.has_value());
    const auto wg = static_cast<std::uint32_t>(*wolle);
    const auto hg = static_cast<std::uint32_t>(*holz);
    const sim::SimConfig cfg;
    REQUIRE(hg >= cfg.build_goods_first);
    REQUIRE(hg - cfg.build_goods_first < cfg.build_goods_count); // HOLZ is a build good
    REQUIRE(wg < cfg.build_goods_first);                         // WOLLE is not

    Buf head2(64, 0);
    head2[0x24] = 0;    // capacity 0 -> 50 t
    head2[0x26] = 1;    // VK slot count 1
    head2[0x27] = 2;    // EK slot count 2
    head2[0x28] = 5;    // statistic 0 (× 7): not modelled
    head2[0x30] = 0x09; // owner bit (player 0 = bit 0) + a foreign bit (player 3): not mapped

    Buf baumat;
    auto bm = [&baumat](std::uint32_t number, std::uint32_t value) {
        put16(baumat, number);
        put16(baumat, value);
        put32(baumat, 0);
    };
    bm(200, 7);  // -> HOLZ (in the build block): setting 7 << 8
    bm(100, 10); // -> WOLLE (not in the build block): BuildGoodsOutOfRange

    auto trade = [](std::uint32_t price, std::uint32_t goods_no, std::uint32_t amount) {
        Buf e;
        put16(e, price);
        put16(e, goods_no);
        put16(e, amount);
        put16(e, 0);
        return e;
    };
    Buf vk; // list 1, slot count 1: index 0 WOLLE, index 1 skipped (goods 0), index 2 HOLZ
    append(vk, trade(3, 100, 5));
    append(vk, trade(0, 0, 0));
    append(vk, trade(9, 200, 6));
    Buf ek; // list 0: index 0 WOLLE (read after VK, so it wins the goods setting)
    append(ek, trade(2, 100, 4));

    Buf recs = record("HEAD2", head2);
    append(recs, record("BAUMAT", baumat));
    append(recs, record("VK", vk));
    append(recs, record("EK", ek));
    const sim::LoadedScenario s = load(t26_city_scene({{100, "WOLLE"}, {200, "HOLZ"}}, recs));
    REQUIRE(s.spec.islands.size() == 1);
    REQUIRE(s.spec.islands[0].cities.size() == 1);
    const auto& city = s.spec.islands[0].cities[0];

    REQUIRE(city.build_goods.has_value());
    CHECK((*city.build_goods)[hg - cfg.build_goods_first].setting == (7U << 8));
    CHECK(warnings_of(s, Kind::BuildGoodsOutOfRange) == 1);

    REQUIRE(city.trade_lists.has_value());
    REQUIRE(city.trade_lists->size() == cfg.trade_lists);
    const auto& vklist = (*city.trade_lists)[1];
    const auto& eklist = (*city.trade_lists)[0];
    REQUIRE(vklist.size() == 3); // the entries read (3)
    CHECK(vklist[0].goods == static_cast<sim::GoodsId>(wg));
    CHECK(vklist[0].amount_88 == (5U << 8));
    CHECK(vklist[0].price == 3);
    CHECK(vklist[1].goods == 0); // the skipped entry keeps its index, stays default
    CHECK(vklist[2].goods == static_cast<sim::GoodsId>(hg));
    REQUIRE(eklist.size() == 1); // the entries read (1); the slot count 2 is not kept
    CHECK(eklist[0].goods == static_cast<sim::GoodsId>(wg));
    CHECK(eklist[0].amount_88 == (4U << 8));
    CHECK(eklist[0].price == 2);
    CHECK(warnings_of(s, Kind::TradeEntryBeyondSlots) == 1); // VK index 1/2 beyond slot 1

    REQUIRE(city.goods.has_value());
    CHECK((*city.goods)[wg].buy == false); // EK (list 0) read after VK, it wins
    CHECK((*city.goods)[wg].list_index == 0);
    CHECK((*city.goods)[hg].buy == true); // only in VK (list 1)
    CHECK((*city.goods)[hg].list_index == 2);

    CHECK(warnings_of(s, Kind::StatisticsNotModelled) == 1); // HEAD2 [0x28]
    CHECK(warnings_of(s, Kind::Head2FieldNotMapped) == 1);   // [0x30] bit, slots 1/2: one per city
    CHECK(warnings_of(s, Kind::UnknownRecord) == 0);

    auto w = sim::World::create(s.spec, registry());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    CHECK(w.value().check_invariants() == "");
}

TEST_CASE("scenario HEAD2 player-mask with only the owner's bit does not warn (T26)") {
    Buf head2(64, 0);
    head2[0x30] = 0x01; // only the owner's bit (player 0): the default, no warning
    const sim::LoadedScenario s = load(t26_city_scene({}, record("HEAD2", head2)));
    CHECK(warnings_of(s, Kind::Head2FieldNotMapped) == 0);
    CHECK(warnings_of(s, Kind::StatisticsNotModelled) == 0);
    // The default slot counts 4/6 are no warning; another one is (not kept, no City field).
    head2[0x26] = 4;
    head2[0x27] = 6;
    CHECK(warnings_of(load(t26_city_scene({}, record("HEAD2", head2))),
                      Kind::Head2FieldNotMapped) == 0);
    head2[0x27] = 7;
    CHECK(warnings_of(load(t26_city_scene({}, record("HEAD2", head2))),
                      Kind::Head2FieldNotMapped) == 1);
}

TEST_CASE("scenario market KONTID is read but not kept (T26)") {
    // A MARKTKOMBI (class MarketCombi) entry with one nested KONTID: read and consumed
    // (not UndecodedBuildingRecord), its number not kept (the Market payload has none).
    Buf prod2;
    item(prod2, kMarketCombi, 0, 0, 0, 5, 3);
    Buf kontid = record("KONTID", [] {
        Buf b;
        put16(b, 7);
        return b;
    }());
    Buf prod = entry(5, 3, kontid);
    Buf recs = record("PROD", prod);
    const sim::LoadedScenario s = load(t26_city_scene({}, recs, prod2));
    CHECK(warnings_of(s, Kind::MarketNumberNotKept) == 1);
    CHECK(warnings_of(s, Kind::UndecodedBuildingRecord) == 0);
    REQUIRE(s.spec.islands[0].cities.size() == 1);
    CHECK(s.spec.islands[0].cities[0].buildings.size() == 1); // the market is created
    auto w = sim::World::create(s.spec, registry());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    CHECK(w.value().check_invariants() == "");
}

TEST_CASE("scenario VOLK people names map through the 14-name table (T26)") {
    // D AnnoWelt 0x1001d6fc–0x1001d753: types 8/11/12 -> 1, 15 -> 11, 16 -> 12; every
    // other type -> the index of the VOLK name (GOTT 0 … BANDE 13), with the bits 0x19
    // and colour 0 for indices 2..12. A name not in the table stays undecoded.
    auto player = [](std::uint32_t idx, std::uint32_t kind, std::string_view volk,
                     std::uint8_t colour) {
        Buf p = record("HEAD", player_head(kind, 1, "N", colour));
        if (!volk.empty()) {
            Buf vb(volk.begin(), volk.end());
            vb.push_back(0);
            append(p, record("VOLK", vb));
        }
        return chunk("SPIELER", spieler(idx, p));
    };
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, player(0, 8, "SPIELER", 3));   // type 8 -> 1, name ignored
    append(kids, player(1, 15, "PIRATEN", 5));  // 15 -> 11, bits, colour 0
    append(kids, player(2, 14, "INDIANER", 4)); // 14, INDIANER index 8, bits, colour 0
    append(kids, player(3, 17, "GOTT", 6));     // 17, GOTT index 0, no bits
    append(kids, player(4, 17, "BANDE", 6));    // 17, BANDE index 13, no bits
    append(kids, player(5, 2, "SPIELER", 6));   // 2, SPIELER index 1, no bits
    append(kids, player(6, 2, "MAUREN", 6));    // 2, MAUREN index 2, bits, colour 0
    append(kids, player(7, 14, "NOTANAME", 6)); // not in the table: UndecodedPeople
    const sim::LoadedScenario s = load(scene("ANNO", kids));
    REQUIRE(s.spec.players.size() == 8);
    const auto& pl = s.spec.players;
    CHECK(pl[0].volk == 1);
    CHECK(pl[1].volk == 11);
    CHECK(pl[1].colour == 0);
    CHECK((pl[1].flags_a0 & 0x19U) == 0x19U);
    CHECK(pl[2].volk == 8); // INDIANER
    CHECK(pl[2].colour == 0);
    CHECK((pl[2].flags_a0 & 0x19U) == 0x19U);
    CHECK(pl[3].volk == 0); // GOTT, no bits
    CHECK(pl[3].colour == 6);
    CHECK((pl[3].flags_a0 & 0x19U) == 0);
    CHECK(pl[4].volk == 13); // BANDE, no bits
    CHECK(pl[4].colour == 6);
    CHECK(pl[5].volk == 1); // SPIELER, index 1, no bits
    CHECK(pl[5].colour == 6);
    CHECK(pl[6].volk == 2); // MAUREN, bits
    CHECK(pl[6].colour == 0);
    CHECK((pl[6].flags_a0 & 0x19U) == 0x19U);
    CHECK(pl[7].volk == 1); // unchanged default
    CHECK(warnings_of(s, Kind::UndecodedPeople) == 1);
    // The table is as documented.
    CHECK(sim::volk_name(0) == "GOTT");
    CHECK(sim::volk_name(13) == "BANDE");
    CHECK(sim::volk_name(14).empty());
    CHECK(sim::volk_name_index("INDIANER") == 8);
    CHECK(sim::volk_name_index("NOPE") == -1);
}

TEST_CASE("scenario city footprints: the origin wins, another city's tile stays") {
    // An entry on a tile that is one item's origin and inside another item's footprint
    // names the item of that origin (0x10083a90 reads the tile's own record). The
    // footprint claim (0x10026685, no force) takes only neutral tiles, so a tile that
    // another city's ID3 holds stays that city's. A second equal item on a building
    // item's origin is a DuplicateObject and does not stand for a second building.
    Buf prod2;
    item(prod2, kBigHouse, 0, 0, 0, 0, 4); // covers (0..1, 4..5)
    item(prod2, kHouse, 0, 0, 0, 1, 5);    // its origin inside the big house's footprint
    item(prod2, kHouse, 0, 0, 0, 9, 1);
    item(prod2, kHouse, 0, 0, 0, 9, 1); // the same item again
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
    append(kids, insel5(40, 60, 16, 8, 7, 2, block_record("PROD2", prod2)));
    Buf id3;
    put32(id3, 8); // city 3: the chunk (8..15, 0..7)
    Buf c3 = {3, 0};
    put16(c3, 41);
    put16(c3, 61);
    append(c3, record("ID3", id3));
    Buf prod = entry(1, 5);
    append(prod, entry(9, 1));
    Buf c2 = {2, 0};
    put16(c2, 41);
    put16(c2, 61);
    append(c2, record("PROD", prod));
    Buf st = record("STADT2", c3);
    append(st, record("STADT2", c2));
    append(kids, chunk("SPIELER", spieler(0, st)));
    const sim::LoadedScenario s = load(scene("ANNO", kids));
    REQUIRE(s.spec.islands[0].cities.size() == 2);
    const auto& cities = s.spec.islands[0].cities;
    const auto& city2 = cities[0].no == sim::CityNo{2} ? cities[0] : cities[1];
    REQUIRE(city2.buildings.size() == 2);
    CHECK(city2.buildings[0].type == sim::TypeId{kHouse});
    CHECK(city2.buildings[0].x == 41);
    CHECK(city2.buildings[0].y == 65);
    CHECK(city2.buildings[1].x == 49);
    CHECK(warnings_of(s, Kind::BuildingItemWithoutEntry) == 1); // the big house
    CHECK(warnings_of(s, Kind::DuplicateObject) == 1);
    const std::uint32_t neutral = sim::SimConfig{}.neutral_owner;
    std::vector<std::uint32_t> want(16 * 8, neutral);
    for (std::size_t y = 0; y < 8; ++y) {
        for (std::size_t x = 8; x < 16; ++x) {
            want[y * 16 + x] = 3;
        }
    }
    want[5 * 16 + 1] = 2; // the house's own footprint, neutral before
    CHECK(owner_grid(s.spec.islands[0].owner_runs, want.size(), neutral) == want);
}

TEST_CASE("scenario city entries: refused buildings and entries without an item") {
    // create_building's rules (F32: the original creates blindly): a footprint that
    // leaves the island or overlaps an earlier building is refused with the reason. An
    // entry whose tile holds no building item is skipped (the original's type lookup
    // fails). A house record with bytes is applied to the house payload (P1; a registry
    // without a group table has no stocks).
    Buf prod2;
    item(prod2, kBigHouse, 0, 0, 0, 15, 3); // 2 x 2 from (15, 3): leaves the 16-wide island
    item(prod2, kHouse, 0, 0, 0, 8, 4);
    item(prod2, kHouse, 0, 0, 0, 10, 4);
    item(prod2, kHouse, 0, 0, 0, 11, 4);
    item(prod2, kRoad, 0, 0, 0, 5, 5); // a PROD2 item without a list: no building item
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
    append(kids, insel5(40, 60, 16, 8, 7, 2, block_record("PROD2", prod2)));
    Buf prod = entry(15, 3);         // BuildingRefused: not inside
    append(prod, entry(8, 4));       // placed
    append(prod, entry(8, 4));       // BuildingRefused: overlaps
    append(prod, entry(0, 0));       // EntryWithoutBuildingItem: no item
    append(prod, entry(99, 0));      // EntryWithoutBuildingItem: off the island
    append(prod, entry(5, 5));       // EntryWithoutBuildingItem: the road (OQ-144)
    append(prod, entry(10, 4, {1})); // placed; the house record
    append(prod, entry(11, 4, {2})); // placed; the house record
    Buf c = {2, 0};
    put16(c, 41);
    put16(c, 61);
    append(c, record("PROD", prod));
    append(kids, chunk("SPIELER", spieler(0, record("STADT2", c))));
    const sim::LoadedScenario s = load(scene("ANNO", kids));
    const auto& b = s.spec.islands[0].cities[0].buildings;
    REQUIRE(b.size() == 3);
    CHECK(b[0].x == 48);
    CHECK(b[1].x == 50);
    CHECK(b[2].x == 51);
    CHECK(warnings_of(s, Kind::BuildingRefused) == 2);
    CHECK(warnings_of(s, Kind::EntryWithoutBuildingItem) == 3);
    CHECK(warnings_of(s, Kind::UndecodedBuildingRecord) == 0); // the houses are their own kind
    CHECK(warnings_of(s, Kind::HouseResetNotApplied) == 0); // the two house records: no group table
    CHECK(warnings_of(s, Kind::BuildingItemWithoutEntry) == 0); // every item is named
    std::vector<std::string> reasons, skipped;
    for (const sim::ScenarioWarning& w : s.warnings) {
        if (w.kind == Kind::BuildingRefused) {
            reasons.push_back(w.message);
        }
        if (w.kind == Kind::EntryWithoutBuildingItem) {
            skipped.push_back(w.message);
        }
    }
    CHECK(skipped == std::vector<std::string>{
                         "tile (0, 0) holds no PROD2 building item: skipped",
                         "tile (99, 0) holds no PROD2 building item: skipped",
                         "tile (5, 5) holds no PROD2 building item (a PROD2 item of PRODTYPE "
                         "193, no list in the layout: OQ-144): skipped"});
    CHECK(reasons ==
          std::vector<std::string>{"building 105: the footprint is not inside the island",
                                   "building 101: the footprint overlaps another building"});
    auto w = sim::World::create(s.spec, registry());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    CHECK(w.value().check_invariants() == "");
    // An ENTRY shorter than its position is refused.
    Buf short_prod = record("ENTRY", Buf{1, 0, 2});
    Buf c2 = {2, 0};
    put16(c2, 41);
    put16(c2, 61);
    append(c2, record("PROD", short_prod));
    Buf k2 = chunk("HEAD", head_bytes());
    append(k2, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
    append(k2, insel5(40, 60, 16, 8, 7, 2, {}));
    append(k2, chunk("SPIELER", spieler(0, record("STADT2", c2))));
    CHECK(refusal(scene("ANNO", k2)) ==
          "players[1].STADT2.PROD.ENTRY[0]: the 4-byte position is missing");
}

TEST_CASE("scenario warning kinds") {
    // One scenario that reaches every kind once (or a known number of times).
    Buf island_extra;
    {
        Buf items;
        item(items, kUnknown, 0, 0, 0, 3, 3); // UnknownObject
        item(items, kRoad, 0, 0, 0, 1, 1);    // tile (1, 1) holds the GELAENDE road: taken
        item(items, kRoad, 0, 0, 0, 3, 5);
        item(items, kRoad, 0, 0, 0, 3, 5); // the same item again: DuplicateObject
        append(island_extra, block_record("FLUESSE", items, 8, 8));
        append(island_extra, chunk("BERGWERK2", Buf(8, 1))); // UndecodedDeposits
        append(island_extra, chunk("OCCUR", Buf(4, 0)));     // UnknownRecord
        Buf more;
        item(more, kHouse, 0, 0, 0, 14, 0); // BuildingItemWithoutEntry
        item(more, kInn, 0, 0, 0, 6, 6);    // an Inn: its Load is not mapped
        append(island_extra, block_record("PROD2", more));
    }
    // City 2's records.
    Buf city_records = record("ID", Buf(8, 0)); // TileOwnersUndecoded
    {
        Buf id3;
        put32(id3, 12 | (4U << 15)); // the chunk at (12, 4): 48 of its tiles lie outside
        append(city_records, record("ID3", id3));
        Buf ware;
        put16(ware, 12); // no WARETYPE in the scene: UnknownGoods
        put16(ware, 3);
        put32(ware, 0);
        append(city_records, record("WARE", ware));
        Buf prod = entry(0, 7);               // no item there: EntryWithoutBuildingItem
        append(prod, entry(8, 4, Buf{1, 2})); // the house with bytes
        append(prod, entry(8, 4));            // the house again: BuildingRefused
        append(prod, entry(6, 6, Buf{1, 2})); // the Inn with bytes: UndecodedBuildingRecord
        append(city_records, record("PROD", prod));
    }
    Buf city_off = {1, 0};
    put16(city_off, 500); // no island there: CityOffIsland
    put16(city_off, 500);
    Buf state = record("STADT2", city_off);
    append(state, record("SCHIFF", Buf(16, 0))); // UnknownRecord (players)
    Buf file = base_scene(island_extra, state, city_records);
    // Add: an INSELLNK, an unknown SCENE child, a native player with VOLK, an unnamed
    // computer, an island without a climate mask.
    Buf kids;
    {
        Buf link;
        put16(link, 400);
        put16(link, 400);
        put16(link, 64);
        put16(link, 64);
        put32(link, 9);
        const std::string path = "islands\\polar\\po_064x064_dummy.scp";
        link.insert(link.end(), path.begin(), path.end());
        link.push_back(0);
        kids = chunk("INSELLNK", link);
        append(kids, chunk("SKTIMER", Buf(40, 0)));
        Buf n = record("HEAD", player_head(14, 0, "N", 0));
        append(n, record("VOLK", Buf{'I', 0}));
        append(kids, chunk("SPIELER", spieler(6, n)));
        append(kids, chunk("SPIELER", spieler(7, record("HEAD", player_head(12, 1, "", 0xff)))));
        append(kids, insel5(200, 200, 8, 8, 8, 9, {}));
        append(kids, insel5(205, 100, 8, 8, 12, 1, {})); // UnalignedIsland
    }
    // Splice the extra children into the SCENE (after its own children).
    Buf scene_kids(file.begin() + 32, file.end());
    append(scene_kids, kids);
    const sim::LoadedScenario s = load(scene("ANNO", scene_kids));
    CHECK(warnings_of(s, Kind::UnknownRecord) == 3);  // OCCUR, SCHIFF, SKTIMER
    CHECK(warnings_of(s, Kind::LinkUnresolved) == 1); // no ERZE: the selection never runs
    CHECK(warnings_of(s, Kind::UnknownObject) == 1);
    CHECK(warnings_of(s, Kind::BuildingItemWithoutEntry) == 1); // the house at (14, 0)
    CHECK(warnings_of(s, Kind::TileOwnersUndecoded) == 1);      // the ID record
    CHECK(warnings_of(s, Kind::UnalignedIsland) == 2);          // (40, 60) and (205, 100)
    CHECK(warnings_of(s, Kind::UndecodedDeposits) == 1);
    CHECK(warnings_of(s, Kind::ObjectTileTaken) == 1);
    CHECK(warnings_of(s, Kind::DuplicateObject) == 1);
    CHECK(warnings_of(s, Kind::CityOffIsland) == 1);
    CHECK(warnings_of(s, Kind::UndecodedPeople) == 1);
    CHECK(warnings_of(s, Kind::ComputerNameNotDrawn) == 1);
    CHECK(warnings_of(s, Kind::UnknownClimate) == 1);
    CHECK(warnings_of(s, Kind::EntryWithoutBuildingItem) == 1);
    CHECK(warnings_of(s, Kind::BuildingRefused) == 1);
    CHECK(warnings_of(s, Kind::UndecodedBuildingRecord) == 1); // the Inn
    CHECK(warnings_of(s, Kind::HouseResetNotApplied) == 0); // the house with bytes: no group table
    CHECK(warnings_of(s, Kind::UnknownGoods) == 1);
    CHECK(warnings_of(s, Kind::OwnerTileOutsideIsland) == 1);
    for (const sim::ScenarioWarning& w : s.warnings) {
        if (w.kind == Kind::OwnerTileOutsideIsland) {
            CHECK(w.message.find("48 tiles outside the island") == 0);
        }
    }
    // Every kind is listed, has a name, and is reached here. The T26 city-record kinds
    // that need a WARETYPE goods table or a market/trade list have their own cases
    // below (the market, the trade lists, the build goods, the HEAD2 fields, the
    // statistics); only their reachability is checked there, not here.
    static constexpr Kind kT26Dedicated[] = {
        Kind::MarketNumberNotKept, Kind::BuildGoodsOutOfRange,  Kind::TradeEntryBeyondSlots,
        Kind::Head2FieldNotMapped, Kind::StatisticsNotModelled, Kind::StatisticsHistoryNotModelled,
        Kind::HouseResetNotApplied};
    for (const Kind k : sim::all_warning_kinds()) {
        INFO(sim::warning_kind_name(k));
        CHECK(sim::warning_kind_name(k) != "?");
        if (sim::warning_kind_name(k).starts_with("Link") && k != Kind::LinkUnresolved) {
            continue; // reached by the link cases (scenario_links_test.cpp)
        }
        if (std::find(std::begin(kT26Dedicated), std::end(kT26Dedicated), k) !=
            std::end(kT26Dedicated)) {
            continue; // reached by the dedicated T26 cases below
        }
        CHECK(warnings_of(s, k) > 0);
    }
    // Paths and offsets name the record.
    for (const sim::ScenarioWarning& w : s.warnings) {
        INFO(w.path << " " << w.message);
        CHECK_FALSE(w.path.empty());
        CHECK_FALSE(w.record.empty());
        CHECK(w.offset < scene_kids.size() + 64);
    }
    // The counts.
    const sim::ScenarioCounts c = sim::count(s);
    CHECK(c.islands == 2);
    CHECK(c.links == 1);
    CHECK(c.cities == 1);
    CHECK(c.buildings == 2);    // the house at (8, 4) and the Inn at (6, 6)
    CHECK(c.tile_objects == 4); // road, tree, PROD2 road, FLUESSE road at (3, 5)
    CHECK(c.players == 4);
}

TEST_CASE("scenario contradictions are refused with their path") {
    CHECK(refusal(Buf{1, 2, 3}).find("not a scenario") == 0);
    {
        Buf kids = chunk("HEAD", head_bytes());
        append(kids, insel5(0, 0, 8, 8, 4, 1, {}));
        append(kids, insel5(100, 0, 8, 8, 4, 1, {}));
        const std::string e = refusal(scene("ANNO", kids));
        CHECK(e.find("islands[") == 0);
        CHECK(e.find("island number 4 is used twice") != std::string::npos);
    }
    {
        Buf kids = chunk("HEAD", head_bytes());
        append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
        append(kids, chunk("SPIELER", spieler(1, record("HEAD", player_head(8, 1, "B", 2)))));
        append(kids, insel5(0, 0, 8, 8, 4, 1, {}));
        Buf c = {3, 0};
        put16(c, 1);
        put16(c, 1);
        append(kids, chunk("SPIELER", spieler(0, record("STADT2", c))));
        append(kids, chunk("SPIELER", spieler(1, record("STADT2", c))));
        const std::string e = refusal(scene("ANNO", kids));
        CHECK(e.find("players[3].STADT2") == 0);
        CHECK(e.find("city number 3 is used twice") != std::string::npos);
    }
    {
        Buf kids = chunk("HEAD", head_bytes());
        append(kids, chunk("SPIELER", spieler(40, record("HEAD", player_head(8, 1, "A", 1)))));
        CHECK(refusal(scene("ANNO", kids)).find("players[0]: player index 40") == 0);
    }
    {
        Buf kids = chunk("HEAD", head_bytes());
        append(kids, chunk("SPIELER", spieler(2, record("STADT2", Buf(6, 0)))));
        CHECK(refusal(scene("ANNO", kids)).find("players[0]: a state block") == 0);
    }
}

TEST_CASE("a loaded synthetic scenario creates a world") {
    const sim::LoadedScenario s = load(base_scene());
    auto w = sim::World::create(s.spec, registry());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    CHECK(w.value().check_invariants() == "");
}

TEST_CASE("scenario edges of an island: items and cities") {
    // An item's tile is its block origin + item x/y; it must lie inside the w × h island.
    // A STADT2 tile (x, y) belongs to the island when x0 <= x < x0 + w and y0 <= y < y0 + h.
    auto with = [](std::uint8_t ix, std::uint8_t iy, std::uint32_t cx, std::uint32_t cy) {
        Buf kids = chunk("HEAD", head_bytes());
        append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
        Buf items;
        item(items, kRoad, 0, 0, 0, ix, iy);
        append(kids, insel5(40, 60, 16, 8, 7, 2, block_record("GELAENDE", items)));
        Buf c = {3, 0};
        put16(c, cx);
        put16(c, cy);
        append(kids, chunk("SPIELER", spieler(0, record("STADT2", c))));
        return scene("ANNO", kids);
    };
    {
        const sim::LoadedScenario s = load(with(15, 7, 40, 60)); // last tile; first city tile
        REQUIRE(s.spec.islands[0].objects.size() == 1);
        CHECK(s.spec.islands[0].objects[0].index == 7 * 16 + 15);
        CHECK(s.spec.islands[0].cities.size() == 1);
    }
    CHECK(load(with(0, 0, 55, 67)).spec.islands[0].cities.size() == 1); // the last city tile
    for (const auto& [cx, cy] :
         {std::pair{56U, 60U}, std::pair{40U, 68U}, std::pair{39U, 60U}, std::pair{40U, 59U}}) {
        INFO(cx << ", " << cy);
        const sim::LoadedScenario s = load(with(0, 0, cx, cy));
        CHECK(s.spec.islands[0].cities.empty());
        CHECK(warnings_of(s, Kind::CityOffIsland) == 1);
    }
    // Child 2 is the INSEL5 (HEAD, SPIELER, INSEL5, SPIELER).
    CHECK(refusal(with(16, 0, 40, 60)) ==
          "islands[2].GELAENDE[0]: the item's tile (16, 0) is outside the island");
    CHECK(refusal(with(0, 8, 40, 60)) ==
          "islands[2].GELAENDE[0]: the item's tile (0, 8) is outside the island");
}

TEST_CASE("scenario overlapping items keep the first") {
    // OQ-142 stand-in: the first item on a tile stays; an equal item of the same record is
    // a DuplicateObject, anything else on a taken tile an ObjectTileTaken.
    Buf g;
    item(g, kRoad, 0, 4, 0, 1, 1);
    item(g, kRoad, 0, 4, 0, 1, 1); // equal, same record: duplicate
    Buf f;
    item(f, kRoad, 0, 4, 0, 1, 1); // equal item, other record: taken
    item(f, kTree, 0, 4, 0, 2, 2);
    item(f, kRoad, 0, 4, 0, 2, 2); // other item, same record, taken tile
    Buf extra = block_record("GELAENDE", g);
    append(extra, block_record("FLUESSE", f, 8, 8));
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, insel5(40, 60, 16, 8, 7, 2, extra));
    const sim::LoadedScenario s = load(scene("ANNO", kids));
    CHECK(warnings_of(s, Kind::DuplicateObject) == 1);
    CHECK(warnings_of(s, Kind::ObjectTileTaken) == 2);
    REQUIRE(s.spec.islands[0].objects.size() == 2);
    CHECK(s.spec.islands[0].objects[1].object == kTree); // the first on (2, 2)
}

TEST_CASE("scenario climate bound and a missing INFO") {
    // INFO[0] is the climate; the monopoly masks cover 0 .. 6 (SimConfig default: 7).
    auto with = [](std::uint8_t climate, bool info) {
        Buf kids = chunk("HEAD", head_bytes());
        append(kids, insel5(40, 60, 8, 8, 7, climate, {}, info));
        return scene("ANNO", kids);
    };
    REQUIRE(sim::SimConfig{}.monopoly_climate_mask.size() == 7);
    const sim::LoadedScenario ok = load(with(6, true));
    REQUIRE(ok.spec.islands.size() == 1);
    CHECK(ok.spec.islands[0].climate == 6);
    for (const auto& [climate, info] :
         {std::pair{std::uint8_t{7}, true}, std::pair{std::uint8_t{0}, false}}) {
        const sim::LoadedScenario s = load(with(climate, info));
        CHECK(s.spec.islands.empty());
        CHECK(warnings_of(s, Kind::UnknownClimate) == 1);
    }
    // An empty INFO record.
    Buf p;
    put16(p, 0);
    put16(p, 0);
    put16(p, 8);
    put16(p, 8);
    put32(p, 3);
    append(p, chunk("INFO", Buf{}));
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, chunk("INSEL5", p));
    CHECK(warnings_of(load(scene("ANNO", kids)), Kind::UnknownClimate) == 1);
}

namespace {

/// The offset of the first occurrence of `name` in `file` (a record or chunk name).
std::uint64_t offset_of(const Buf& file, std::string_view name) {
    const auto it = std::search(file.begin(), file.end(), name.begin(), name.end());
    REQUIRE(it != file.end());
    return static_cast<std::uint64_t>(it - file.begin());
}

const sim::ScenarioWarning& the_warning(const sim::LoadedScenario& s, Kind k) {
    for (const sim::ScenarioWarning& w : s.warnings) {
        if (w.kind == k) {
            return w;
        }
    }
    FAIL("no warning of that kind");
    return s.warnings.front();
}

} // namespace

TEST_CASE("scenario warnings name their path and the record's offset, unknown records once") {
    Buf kids = chunk("HEAD", head_bytes());     // child 0
    append(kids, chunk("SKTIMER", Buf(40, 0))); // child 1
    append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1))))); // 2
    append(kids, insel5(40, 60, 8, 8, 7, 2, chunk("OCCUR", Buf(4, 0)))); // child 3
    Buf c = {3, 0};
    put16(c, 41);
    put16(c, 61);
    append(c, record("STAT2", Buf(4, 0)));  // a city record the adapter does not map
    append(c, record("PROD", entry(2, 2))); // no building item at (2, 2)
    Buf st = record("STADT2", c);
    append(st, record("SCHIFF", Buf(16, 0)));
    append(kids, chunk("SPIELER", spieler(0, st)));                           // child 4
    append(kids, chunk("SPIELER", spieler(0, record("SCHIFF", Buf(16, 0))))); // child 5
    append(kids, chunk("SKTIMER", Buf(40, 0)));                               // child 6
    const Buf file = scene("ANNO", kids);
    const sim::LoadedScenario s = load(file);
    // One UnknownRecord per scope and name: SKTIMER, OCCUR, SCHIFF (twice in the file
    // each). STAT2 is its own kind since T26 (StatisticsNotModelled).
    REQUIRE(warnings_of(s, Kind::UnknownRecord) == 3);
    std::map<std::string, const sim::ScenarioWarning*> by_record;
    for (const sim::ScenarioWarning& w : s.warnings) {
        if (w.kind == Kind::UnknownRecord) {
            by_record[w.record] = &w;
        }
    }
    CHECK(by_record.at("SKTIMER")->path == "scene.SKTIMER");
    CHECK(by_record.at("SKTIMER")->offset == offset_of(file, "SKTIMER"));
    CHECK(by_record.at("OCCUR")->path == "islands[3].OCCUR");
    CHECK(by_record.at("OCCUR")->offset == offset_of(file, "OCCUR"));
    CHECK(by_record.at("SCHIFF")->path == "players[1].SCHIFF");
    CHECK(by_record.at("SCHIFF")->offset == offset_of(file, "SCHIFF"));
    const sim::ScenarioWarning& st2 = the_warning(s, Kind::StatisticsNotModelled);
    CHECK(st2.path == "players[1].STADT2.STAT2");
    CHECK(st2.offset == offset_of(file, "STAT2"));
    const sim::ScenarioWarning& e = the_warning(s, Kind::EntryWithoutBuildingItem);
    CHECK(e.path == "players[1].STADT2.PROD.ENTRY[0]");
    CHECK(e.offset == offset_of(file, "ENTRY"));
}

TEST_CASE("scenario refusals at their bounds") {
    auto head_only = [] { return chunk("HEAD", head_bytes()); };
    {
        // island_slots 256: number 255 is a slot, 256 is not.
        Buf kids = head_only();
        append(kids, insel5(0, 0, 8, 8, 255, 1, {}));
        CHECK(load(scene("ANNO", kids)).spec.islands[0].slot == sim::IslandId{255});
        Buf bad = head_only();
        append(bad, insel5(0, 0, 8, 8, 256, 1, {}));
        CHECK(refusal(scene("ANNO", bad)) ==
              "islands[1]: island number 256 is not below island_slots (256)");
    }
    {
        // player_slots 32: index 31 is a slot, 32 is not.
        Buf kids = head_only();
        append(kids, chunk("SPIELER", spieler(31, record("HEAD", player_head(2, 1, "", 0)))));
        CHECK(load(scene("ANNO", kids)).spec.players.size() == 1);
        Buf bad = head_only();
        append(bad, chunk("SPIELER", spieler(32, record("HEAD", player_head(2, 1, "", 0)))));
        CHECK(refusal(scene("ANNO", bad)).find("players[0]: player index 32 is not below") == 0);
    }
    {
        Buf short_head = player_head(8, 1, "A", 1);
        short_head.resize(107);
        Buf kids = head_only();
        append(kids, chunk("SPIELER", spieler(0, record("HEAD", short_head))));
        CHECK(refusal(scene("ANNO", kids)) == "players[0].HEAD: 107 bytes, the reader needs 108");
    }
    {
        Buf kids = head_only();
        append(kids, chunk("SPIELER", spieler(3, record("HEAD", player_head(8, 1, "A", 1)))));
        append(kids, chunk("SPIELER", spieler(3, record("HEAD", player_head(8, 1, "B", 2)))));
        CHECK(refusal(scene("ANNO", kids)) == "players[1]: player index 3 has two header blocks");
    }
    auto city = [&](Buf prefix) {
        Buf kids = head_only();
        append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
        append(kids, insel5(0, 0, 8, 8, 4, 1, {}));
        append(kids, chunk("SPIELER", spieler(0, record("STADT2", prefix))));
        return scene("ANNO", kids);
    };
    CHECK(refusal(city(Buf{1, 0, 1, 0, 1})) == "players[1].STADT2: the 6-byte prefix is missing");
    CHECK(load(city(Buf{14, 0, 1, 0, 1, 0})).spec.islands[0].cities.size() == 1);
    CHECK(refusal(city(Buf{15, 0, 1, 0, 1, 0})).find("players[1].STADT2: city number 15 is not") ==
          0);
    CHECK(refusal(city(Buf{16, 0, 1, 0, 1, 0})).find("players[1].STADT2: city number 16 is not") ==
          0);
    // A city number that is no slot is refused even off every island.
    CHECK(refusal(city(Buf{16, 0, 99, 0, 99, 0})).find("city number 16 is not") !=
          std::string::npos);
    {
        // An INSELLNK path without its NUL does not parse; a link number an INSEL5 reuses
        // is refused (numbers are unique across both, P sze.md "Island links").
        Buf link;
        put16(link, 400);
        put16(link, 400);
        put16(link, 64);
        put16(link, 64);
        put32(link, 4);
        const std::string path = "islands\\polar\\po_064x064_dummy.scp";
        Buf bad = link;
        bad.insert(bad.end(), path.begin(), path.end());
        Buf kids = head_only();
        append(kids, chunk("INSELLNK", bad));
        CHECK(refusal(scene("ANNO", kids)).find("scene[1].INSELLNK: ") == 0);
        Buf good = bad;
        good.push_back(0);
        Buf both = head_only();
        append(both, chunk("INSELLNK", good));
        append(both, insel5(0, 0, 8, 8, 4, 1, {}));
        CHECK(refusal(scene("ANNO", both)) == "islands[2]: island number 4 is used twice");
    }
    {
        // A HEIGHT block that does not cover the island, and items that are not 6 bytes.
        Buf p;
        put16(p, 0);
        put16(p, 0);
        put16(p, 8);
        put16(p, 8);
        put32(p, 3);
        Buf info(12, 0);
        append(p, chunk("INFO", info));
        append(p, chunk("HEIGHT", Buf{0, 0, 0, 0, 4,  4,  1,  2,  3,  4,  5,
                                      6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}));
        Buf kids = head_only();
        append(kids, chunk("INSEL5", p));
        CHECK(refusal(scene("ANNO", kids)).find("islands[1].HEIGHT: ") == 0);
        // floor((len - 6) / 6) items (D 0x100895cf): a 5-byte tail after a whole item is
        // ignored.
        Buf items;
        item(items, kRoad, 0, 0, 0, 2, 3);
        items.resize(items.size() + 5, 0xab);
        Buf k2 = head_only();
        append(k2, insel5(0, 0, 8, 8, 3, 1, block_record("PROD2", items)));
        const sim::LoadedScenario s = load(scene("ANNO", k2));
        REQUIRE(s.spec.islands[0].objects.size() == 1);
        CHECK(s.spec.islands[0].objects[0].index == 3 * 8 + 2);
    }
}

TEST_CASE("scenario cities: the old name STADT, and file order") {
    // The player reader also takes the older record name STADT (D AnnoWelt 0x1001d240);
    // the SCENE children are read in file order, so a city before its island finds none.
    auto with = [](std::string_view name, bool city_first) {
        Buf kids = chunk("HEAD", head_bytes());
        append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
        Buf c = {5, 0};
        put16(c, 41);
        put16(c, 61);
        Buf head2(0x26, 0);
        head2[0x24] = 7;
        append(c, record("HEAD2", head2)); // the same city reader
        const Buf state = chunk("SPIELER", spieler(0, record(name, c)));
        if (city_first) {
            append(kids, state);
        }
        append(kids, insel5(40, 60, 8, 8, 7, 2, {}));
        if (!city_first) {
            append(kids, state);
        }
        return scene("ANNO", kids);
    };
    const sim::LoadedScenario old = load(with("STADT", false));
    REQUIRE(old.spec.islands[0].cities.size() == 1);
    CHECK(old.spec.islands[0].cities[0].no == sim::CityNo{5});
    CHECK(old.spec.islands[0].cities[0].capacity == std::optional<std::uint32_t>{7U << 8});
    const sim::LoadedScenario early = load(with("STADT2", true));
    CHECK(early.spec.islands[0].cities.empty());
    CHECK(warnings_of(early, Kind::CityOffIsland) == 1);
}

TEST_CASE("scenario link template: its building items are reported, never placed") {
    // A nord link of 16 x 16 (climate 2 from the n_ prefix) and one template whose
    // PROD2 holds a house (a building item) at (8, 4) and a road at (9, 6). No city of
    // the file names a link island's tiles, so the house is a BuildingItemWithoutEntry
    // and the road the island's one tile object; every tile stays neutral.
    Buf info(12, 0);
    info[0] = 2;
    info[5] = 9;
    Buf prod;
    item(prod, kHouse, 0, 4, 0, 8, 4);
    item(prod, kRoad, 2, 4, 1, 9, 6);
    Buf extra = chunk("INFO", info);
    append(extra, block_record("PROD2", prod));
    const Buf scp = chunk("ANNO", insel5(0, 0, 16, 16, 0, 2, extra, false));
    const std::optional<sim::TemplateSummary> sum = sim::template_summary(scp, "NORD/A.SCP");
    REQUIRE(sum.has_value());

    Buf kids = chunk("HEAD", head_bytes());
    append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 0, "Ada", 3)))));
    Buf link;
    put16(link, 64);
    put16(link, 64);
    put16(link, 16);
    put16(link, 16);
    put32(link, 4);
    const std::string path = "islands\\nord\\n_016x016_dummy.scp";
    link.insert(link.end(), path.begin(), path.end());
    link.push_back(0);
    append(kids, chunk("INSELLNK", link));
    append(kids, chunk("ERZE", Buf(10, 0)));
    sim::LoadedScenario s = load(scene("ANNO", kids));
    const std::vector<sim::TemplateSummary> cands = {*sum};
    auto r = sim::resolve_scenario_links(s, cands, sim::SimConfig{}, sim::Rules{});
    REQUIRE(r.ok());
    REQUIRE(r.value().links.size() == 1);
    REQUIRE(r.value().links[0].candidate == std::optional<std::uint32_t>{0});
    const std::vector<anno::formats::Bytes> views = {scp};
    auto e = sim::place_island_links(s, r.value(), views, *registry(), sim::SimConfig{});
    REQUIRE_MESSAGE(!e.has_value(), (e ? *e : std::string()));
    REQUIRE(s.spec.islands.size() == 1);
    const auto& is = s.spec.islands[0];
    CHECK(is.slot == sim::IslandId{4});
    REQUIRE(is.objects.size() == 1);
    CHECK(is.objects[0].index == 6 * 16 + 9);
    CHECK(is.objects[0].object == kRoad);
    CHECK(is.cities.empty());
    CHECK(is.owner_runs.empty());
    CHECK(warnings_of(s, Kind::TileOwnersUndecoded) == 0);
    REQUIRE(warnings_of(s, Kind::BuildingItemWithoutEntry) == 1);
    const sim::ScenarioWarning& w = the_warning(s, Kind::BuildingItemWithoutEntry);
    CHECK(w.path == "links[0].PROD2[0]");
    CHECK(w.message.find("of the link's template") != std::string::npos);
    auto world = sim::World::create(s.spec, registry(), sim::SimConfig{});
    REQUIRE_MESSAGE(world.ok(), (world.ok() ? std::string() : world.error()));
    CHECK(world.value().check_invariants().empty());
}

// ---- P1: the house record is applied to the house payload (docs/sim/state.md §4.7c,
// docs/re/population.md §11.2). The group table here is built in code: two small groups.

namespace {

constexpr std::uint32_t kHouse1 = 112; // PRODTYPE 98: the second group's house

sim::SimConfig house_config() {
    sim::SimConfig cfg;
    cfg.house_groups = 2;
    return cfg;
}

/// Two groups: group 0 Wohnmax 8 with the needs (Menge 41, 16); group 1 Wohnmax 15 with
/// (37, 12, 20). The Menge values are the loader's integers (Menge * 40.96 + 0.5).
std::vector<sim::HouseGroup> house_groups_two() {
    sim::HouseGroup g0;
    g0.number = 0;
    g0.wohnmax = 8;
    g0.needs = {{26, true, 41}, {27, true, 16}};
    sim::HouseGroup g1;
    g1.number = 1;
    g1.wohnmax = 15;
    g1.needs = {{26, true, 37}, {27, true, 12}, {33, false, 20}};
    return {g0, g1};
}

std::shared_ptr<const sim::TypeRegistry> house_registry(bool with_groups = true) {
    sim::TypeRegistry::Builder b;
    b.config(house_config());
    b.add_plain(sim::TypeId{kHouse}, 97);
    b.add_plain(sim::TypeId{kHouse1}, 98);
    if (with_groups) {
        b.house_groups(house_groups_two());
    }
    auto r = b.build();
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return r.value();
}

struct HouseAt {
    std::uint32_t type;
    std::uint8_t x, y;
    Buf rec;
};

/// A scenario with the given houses on one 16 x 8 island and one city.
sim::LoadedScenario load_houses(const std::vector<HouseAt>& houses, bool with_groups = true) {
    Buf prod2, prod;
    for (const HouseAt& h : houses) {
        item(prod2, h.type, 0, 0, 0, h.x, h.y);
        append(prod, entry(h.x, h.y, h.rec));
    }
    Buf kids = chunk("HEAD", head_bytes());
    append(kids, chunk("SPIELER", spieler(0, record("HEAD", player_head(8, 1, "A", 1)))));
    append(kids, insel5(40, 60, 16, 8, 7, 2, block_record("PROD2", prod2)));
    Buf c = {2, 0};
    put16(c, 41);
    put16(c, 61);
    append(c, record("PROD", prod));
    append(kids, chunk("SPIELER", spieler(0, record("STADT2", c))));
    auto r = sim::load_scenario(scene("ANNO", kids), *house_registry(with_groups), house_config());
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

/// A house record: [1] residents, [2] n, [3] version, then the bytes.
Buf house_rec(std::uint8_t residents, std::uint8_t n, std::uint8_t version, const Buf& bytes) {
    Buf b = {0, residents, n, version, 0, 0, 0, 0};
    append(b, bytes);
    return b;
}

const sim::House& house_of(const sim::LoadedScenario& s, std::size_t i) {
    const auto& bs = s.spec.islands[0].cities[0].buildings[i];
    REQUIRE(bs.payload.has_value());
    const auto* h = std::get_if<sim::House>(&*bs.payload);
    REQUIRE(h != nullptr);
    return *h;
}

} // namespace

TEST_CASE("P1 import: a v2 house record sets residents, E and the stocks of its group") {
    // 10 residents, group 0 (needs with Menge 41 and 16), s = 128 and 64. D 0x1007702b:
    // stock = trunc(Menge * s * residents * 128 / 1536): 41*128*10*128/1536 = 4373.33,
    // 16*64*10*128/1536 = 853.33.
    const auto s = load_houses({{kHouse, 8, 4, house_rec(10, 2, 2, {128, 64})}});
    REQUIRE(s.spec.islands[0].cities[0].buildings.size() == 1);
    const sim::House& h = house_of(s, 0);
    CHECK(h.residents_64 == 10 * 64);
    CHECK(h.e == 10U * 128U);
    CHECK(h.last == 0);
    REQUIRE(h.stock.size() == 2);
    CHECK(h.stock[0] == 4373);
    CHECK(h.stock[1] == 853);
    CHECK(warnings_of(s, Kind::HouseResetNotApplied) == 0);
    CHECK(s.houses == 1);
    CHECK(s.resident_sum == 10);
}

TEST_CASE("P1 import: version 1 reads the second byte of each pair, version 0 no bytes") {
    const auto s = load_houses({{kHouse, 8, 4, house_rec(10, 2, 1, {7, 128, 9, 64})},
                                {kHouse, 10, 4, house_rec(10, 2, 0, {128, 64})}});
    CHECK(house_of(s, 0).stock == std::vector<std::uint32_t>{4373, 853});
    CHECK(house_of(s, 1).stock == std::vector<std::uint32_t>{0, 0}); // the constructor's 0
    CHECK(house_of(s, 1).residents_64 == 10 * 64);
}

TEST_CASE("P1 import: n above and below the group's need count") {
    // Group 0 has two needs. n = 4 drops the bytes beyond them; n = 1 leaves need 1 at 0
    // (the constructor zeroes every slot).
    const auto s = load_houses({{kHouse, 8, 4, house_rec(10, 4, 2, {128, 64, 255, 255})},
                                {kHouse, 10, 4, house_rec(10, 1, 2, {128, 64})}});
    CHECK(house_of(s, 0).stock == std::vector<std::uint32_t>{4373, 853});
    CHECK(house_of(s, 1).stock == std::vector<std::uint32_t>{4373, 0});
    // A record shorter than its n reads the missing bytes as 0.
    const auto t = load_houses({{kHouse, 8, 4, house_rec(10, 2, 2, {128})}});
    CHECK(house_of(t, 0).stock == std::vector<std::uint32_t>{4373, 0});
}

TEST_CASE("P1 import: the reset predicate is warned, never applied") {
    // Group 1 above group 0 (Wohnmax 8): residents below 8 / 2 = 4 fire the reset.
    // The original would set 15 residents and draw per forced need; here the record is
    // loaded as stored and HouseResetNotApplied is added once per record.
    const auto fires = load_houses({{kHouse1, 8, 4, house_rec(3, 3, 2, {128, 128, 128})},
                                    {kHouse1, 10, 4, house_rec(0, 0, 2, {})}});
    CHECK(warnings_of(fires, Kind::HouseResetNotApplied) == 2);
    CHECK(house_of(fires, 0).residents_64 == 3 * 64);
    CHECK(house_of(fires, 0).e == 3U * 128U);
    // 4 residents is not below 4; group 0 never resets, whatever the residents.
    const auto quiet = load_houses({{kHouse1, 8, 4, house_rec(4, 3, 2, {128, 128, 128})},
                                    {kHouse, 10, 4, house_rec(0, 2, 2, {128, 128})}});
    CHECK(warnings_of(quiet, Kind::HouseResetNotApplied) == 0);
    // Without a group table there is no Wohnmax to compare with: nothing is evaluated.
    const auto none = load_houses({{kHouse1, 8, 4, house_rec(3, 3, 2, {128, 128, 128})}}, false);
    CHECK(warnings_of(none, Kind::HouseResetNotApplied) == 0);
    CHECK(house_of(none, 0).stock.empty());
    CHECK(house_of(none, 0).residents_64 == 3 * 64);
}

TEST_CASE("P1 import: created houses sum to resident_sum, save then load is exact") {
    const auto s = load_houses({{kHouse, 8, 4, house_rec(10, 2, 2, {128, 64})},
                                {kHouse1, 10, 4, house_rec(9, 3, 2, {128, 100, 50})}});
    CHECK(s.resident_sum == 19);
    auto made = sim::World::create(s.spec, house_registry(), house_config());
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string() : made.error()));
    sim::World w = std::move(made).value();
    CHECK(w.check_invariants() == "");
    std::uint32_t residents = 0;
    // Named before the walk: GCC 13 -O3 -Wnull-dereference treats optional::operator->
    // on the chained expression as a possible null iterator.
    REQUIRE(w.islands().size() > 7);
    REQUIRE(w.islands()[7].has_value());
    const auto* city = w.islands()[7]->city(sim::CityNo{2});
    REQUIRE(city != nullptr);
    for (const auto& list : city->index.lists) {
        for (const auto& slot : list.slots) {
            if (slot.has_value()) {
                if (const auto* h = std::get_if<sim::House>(&slot->payload)) {
                    residents += static_cast<std::uint32_t>(h->residents_64 >> 6);
                }
            }
        }
    }
    CHECK(residents == s.resident_sum);
    // A fraction (residents_64 not a multiple of 64) survives a save: our own save is the
    // runtime form (P1 decision 4), unlike the original's integer record.
    auto& mutable_islands = sim::WorldTestAccess::islands(w);
    REQUIRE(mutable_islands.size() > 7);
    REQUIRE(mutable_islands[7].has_value());
    auto* mutable_city = mutable_islands[7]->city(sim::CityNo{2});
    REQUIRE(mutable_city != nullptr);
    for (auto& list : mutable_city->index.lists) {
        for (auto& slot : list.slots) {
            if (slot.has_value()) {
                if (auto* h = std::get_if<sim::House>(&slot->payload)) {
                    h->residents_64 += 37;
                    h->e += 5;
                }
            }
        }
    }
    const std::uint64_t before = w.state_hash();
    auto back = sim::World::load(w.save(), house_registry());
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    CHECK(back.value().state_hash() == before);
    CHECK(back.value().check_invariants() == "");
}
