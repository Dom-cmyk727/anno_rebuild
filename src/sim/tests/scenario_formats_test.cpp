// The scenario adapter on synthetic scenarios (docs/sim/state.md §10, stage S14):
// the HEAD byte map, a player series, one INSEL5 island with terrain, tile objects
// and a building, a STADT2 city, every warning kind, and the refusals. The .sze
// files are built here byte by byte (our own data, nothing from the original files);
// every expected value follows from the bytes written and the evidence the adapter
// cites, never from the adapter.
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/insel.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

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
constexpr std::uint32_t kRoad = 100;    // PRODTYPE 193: no list, a tile object
constexpr std::uint32_t kHouse = 101;   // PRODTYPE 97: the house list, a building
constexpr std::uint32_t kTree = 102;    // PRODTYPE 225: no list, scenery
constexpr std::uint32_t kUnknown = 777; // not in the registry

std::shared_ptr<const sim::TypeRegistry> registry() {
    sim::TypeRegistry::Builder b;
    b.add_plain(sim::TypeId{kRoad}, 193);
    b.add_plain(sim::TypeId{kHouse}, 97);
    b.add_plain(sim::TypeId{kTree}, 225);
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
/// number 7 at (40, 60) with a road, a tree, a house and a city 2 of player 0.
Buf base_scene(const Buf& island_extra = {}, const Buf& state_records = {}) {
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
    // (a building) has no city on its tile (no owners decoded): not placed.
    REQUIRE(is.objects.size() == 3);
    CHECK(is.objects[0].index == 1 * 16 + 1);
    CHECK(is.objects[0].object == kRoad);
    CHECK(is.objects[1].index == 2 * 16 + 5);
    CHECK(is.objects[1].object == kTree);
    CHECK(is.objects[1].phase == 3);
    CHECK(is.objects[2].index == 6 * 16 + 9);
    CHECK(is.objects[2].phase == 1);
    CHECK(is.owner_runs.empty());
    CHECK(is.deposits.empty());
    CHECK(warnings_of(s, Kind::BuildingWithoutCity) == 1);
    CHECK(warnings_of(s, Kind::TileOwnersUndecoded) == 1);
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
    CHECK_FALSE(c.stock.has_value());
    CHECK(c.buildings.empty());
    CHECK(warnings_of(s, Kind::UndecodedCity) == 1);
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
    }
    Buf city_off = {1, 0};
    put16(city_off, 500); // no island there: CityOffIsland
    put16(city_off, 500);
    Buf state = record("STADT2", city_off);
    append(state, record("SCHIFF", Buf(16, 0))); // UnknownRecord (players)
    Buf file = base_scene(island_extra, state);
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
    CHECK(warnings_of(s, Kind::UnknownRecord) == 3); // OCCUR, SCHIFF, SKTIMER
    CHECK(warnings_of(s, Kind::UndecodedCity) == 1);
    CHECK(warnings_of(s, Kind::IslandLink) == 1);
    CHECK(warnings_of(s, Kind::UnknownObject) == 1);
    CHECK(warnings_of(s, Kind::BuildingWithoutCity) == 1);
    CHECK(warnings_of(s, Kind::TileOwnersUndecoded) == 2); // the skipped island has none
    CHECK(warnings_of(s, Kind::UnalignedIsland) == 2);     // (40, 60) and (205, 100)
    CHECK(warnings_of(s, Kind::UndecodedDeposits) == 1);
    CHECK(warnings_of(s, Kind::ObjectTileTaken) == 1);
    CHECK(warnings_of(s, Kind::DuplicateObject) == 1);
    CHECK(warnings_of(s, Kind::CityOffIsland) == 1);
    CHECK(warnings_of(s, Kind::UndecodedPeople) == 1);
    CHECK(warnings_of(s, Kind::ComputerNameNotDrawn) == 1);
    CHECK(warnings_of(s, Kind::UnknownClimate) == 1);
    // Every kind is listed, has a name, and is reached here.
    for (const Kind k : sim::all_warning_kinds()) {
        INFO(sim::warning_kind_name(k));
        CHECK(warnings_of(s, k) > 0);
        CHECK(sim::warning_kind_name(k) != "?");
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
    CHECK(c.buildings == 0);
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
    Buf st = record("STADT2", c);
    append(st, record("SCHIFF", Buf(16, 0)));
    append(kids, chunk("SPIELER", spieler(0, st)));                           // child 4
    append(kids, chunk("SPIELER", spieler(0, record("SCHIFF", Buf(16, 0))))); // child 5
    append(kids, chunk("SKTIMER", Buf(40, 0)));                               // child 6
    const Buf file = scene("ANNO", kids);
    const sim::LoadedScenario s = load(file);
    // One UnknownRecord per scope and name: SKTIMER, OCCUR, SCHIFF (twice in the file each).
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
    const sim::ScenarioWarning& city = the_warning(s, Kind::UndecodedCity);
    CHECK(city.path == "players[1].STADT2");
    CHECK(city.offset == offset_of(file, "STADT2"));
    const sim::ScenarioWarning& deposits = the_warning(s, Kind::TileOwnersUndecoded);
    CHECK(deposits.path == "islands[3]");
    CHECK(deposits.offset == offset_of(file, "INSEL5"));
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
    CHECK(warnings_of(old, Kind::UndecodedCity) == 1);
    const sim::LoadedScenario early = load(with("STADT2", true));
    CHECK(early.spec.islands[0].cities.empty());
    CHECK(warnings_of(early, Kind::CityOffIsland) == 1);
}
