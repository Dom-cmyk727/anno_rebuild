// Authored original-format records and primary-derived expectations. Never copy original assets.
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/sha256.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/ship.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>

namespace sim = anno::sim;
namespace {
using Buf = std::vector<std::uint8_t>;
constexpr std::uint32_t ship_kind = 60001, scout_kind = 60002, donkey_kind = 60003;
void u16(Buf& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void u32(Buf& b, std::uint32_t v) {
    u16(b, v & 0xffffU);
    u16(b, v >> 16);
}
void append(Buf& b, const Buf& part) {
    b.insert(b.end(), part.begin(), part.end());
}
Buf tagged(std::string_view name, const Buf& body) {
    REQUIRE(body.size() < 0xff00);
    Buf b(name.begin(), name.end());
    b.resize(6, 0);
    u16(b, static_cast<std::uint32_t>(body.size()));
    append(b, body);
    return b;
}
Buf chunk(std::string_view name, const Buf& body) {
    Buf b(name.begin(), name.end());
    b.resize(12, 0);
    u32(b, static_cast<std::uint32_t>(body.size()));
    append(b, body);
    return b;
}
Buf head(std::uint8_t ratio = 128, std::uint16_t walker_hp = 350, std::uint8_t sign = 255,
         std::uint8_t flags = 0) {
    Buf h(16, 0);
    h[1] = sign;
    h[8] = static_cast<std::uint8_t>(walker_hp);
    h[9] = ratio;
    h[10] = flags;
    return h;
}
Buf walker_head(std::uint16_t hp = 350, std::uint8_t sign = 255) {
    Buf h(16, 0);
    h[1] = sign;
    h[8] = static_cast<std::uint8_t>(hp);
    h[9] = static_cast<std::uint8_t>(hp >> 8);
    return h;
}
Buf holds(std::uint16_t tons = 3) {
    Buf b;
    u16(b, 0xab29); // Only byte0 is goods; byte1 deliberately nonzero.
    u16(b, tons);
    u32(b, 0xf00ff00f); // Ignored original tail is not cargo state.
    b.resize(16, 0);    // A second, empty hold.
    return b;
}
Buf scout_records(std::uint16_t hp = 350, const Buf& priv = {}) {
    Buf records = tagged("HEAD", walker_head(hp));
    append(records, tagged("WAREN", Buf(16, 0)));
    if (!priv.empty())
        append(records, tagged("PRIVAT", priv));
    return records;
}
Buf crew(std::uint16_t id = 2, std::uint8_t slot = 1, std::uint16_t hp = 350,
         const Buf& priv = {}) {
    Buf b;
    u16(b, id);
    u16(b, scout_kind);
    b.push_back(slot);
    b.push_back(0);
    append(b, scout_records(hp, priv));
    return tagged("CREW", tagged("ENTRY", b));
}
Buf ship_records(const Buf& priv = {}, std::uint8_t ratio = 64) {
    Buf b = tagged("HEAD", head(ratio, 0));
    append(b, tagged("WAREN", holds()));
    if (!priv.empty())
        append(b, tagged("PRIVAT", priv));
    return b;
}
Buf unit(std::uint16_t id, std::uint16_t kind, const Buf& records,
         std::uint32_t x_bits = 0x3f8ccccd, std::uint32_t y_bits = 0x40200000,
         std::uint8_t facing = 4, std::uint8_t prefix_owner = 255) {
    Buf b;
    u16(b, id);
    u16(b, kind);
    b.push_back(prefix_owner);
    b.push_back(facing);
    u16(b, 0);
    u32(b, x_bits);
    u32(b, y_bits);
    append(b, records);
    return b;
}
Buf source(const Buf& actors, std::uint32_t slots = 100, std::uint32_t width = 512) {
    Buf scene;
    Buf h;
    u32(h, width);
    u32(h, 512);
    h.resize(32, 0);
    append(scene, chunk("HEAD", h));
    Buf main;
    u32(main, 0);
    u32(main, slots);
    main.resize(16, 0);
    append(scene, chunk("MAIN", main));
    Buf ware;
    u16(ware, 41);
    const std::string name = "WERKZEUG";
    ware.insert(ware.end(), name.begin(), name.end());
    ware.push_back(0);
    append(scene, chunk("WARETYPE", ware));
    Buf player;
    u32(player, 0);
    u32(player, 7);
    Buf ph;
    u32(ph, 8);
    u32(ph, 5000);
    ph.resize(108, 0);
    append(player, tagged("HEAD", ph));
    append(scene, chunk("SPIELER", player));
    Buf state;
    u32(state, 0);
    u32(state, 7);
    append(state, actors);
    append(scene, chunk("SPIELER", state));
    // A complete authored empty kernel, not a stripped original program.
    const std::string script = "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n"
                               "[SCRIPT_KERNEL]\n[SIGNALS]\n[CAMARAPOS]\n0,0\n";
    append(scene, chunk("BIGBROTHER", Buf(script.begin(), script.end())));
    return chunk("ANNO", chunk("SCENE", scene));
}
std::shared_ptr<const sim::TypeRegistry> registry(std::uint32_t scout_max = 350,
                                                  bool metadata = true,
                                                  bool navigating_scout = false,
                                                  bool remapped_goods = false) {
    sim::TypeRegistry::Builder b;
    if (remapped_goods) {
        auto goods = sim::original_goods();
        goods[41].name = "OLD_TOOL_NAME";
        goods.resize(90002);
        goods[90001].name = "WERKZEUG";
        b.goods(std::move(goods));
    }
    sim::FigureType ship{ship_kind, 0, 2560, 0, 0};
    ship.bewohntype = 113;
    ship.nav = sim::NavFacts{154618816, 472446400, 12000, 2, 2, 0, 1280};
    sim::FigureType scout{scout_kind, 0, 2560, 0, 0};
    scout.bewohntype = 97;
    if (navigating_scout) {
        scout.nav = ship.nav; // Legal authored type, unsupported as an original foot-role import.
    }
    sim::FigureType donkey{donkey_kind, 0, 768, 0, 0};
    donkey.bewohntype = 53;
    b.figure_type(ship).figure_type(scout).figure_type(donkey);
    if (metadata) {
        b.unit_import_facts(ship_kind, {12000, 2, 2, 15})
            .unit_import_facts(scout_kind, {scout_max, 2, 0, 1})
            .unit_import_facts(donkey_kind, {400, 0, 0, 0});
    }
    auto built = b.build();
    REQUIRE_MESSAGE(built, (built ? "" : built.error()));
    return built.value();
}
sim::World made(const sim::ScenarioSpec& spec, std::shared_ptr<const sim::TypeRegistry> types) {
    auto world = sim::World::create(spec, std::move(types));
    REQUIRE_MESSAGE(world, (world ? "" : world.error()));
    return std::move(world).value();
}
} // namespace

TEST_CASE("UNIT1 explicit source slots preserve holes, creation hint and stable identities") {
    const auto types = registry();
    const auto bytes = source(tagged("SCHIFF", unit(37, ship_kind, ship_records(crew()))));
    auto loaded = sim::load_scenario(bytes, *types, {});
    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
    REQUIRE(loaded.value().spec.figures.size() == 2);
    CHECK(loaded.value().spec.figures[0].registry_slot == 37);
    CHECK(loaded.value().spec.figures[1].registry_slot == 2);
    auto w = made(loaded.value().spec, types);
    CHECK(w.figures().list.slots.size() == 100);
    CHECK(w.figures().list.hint == 2);
    CHECK(w.figures().list.cursor == 0);
    CHECK(w.figures().by_id.at(sim::FigureId{1}) == 37);
    CHECK(w.figures().by_id.at(sim::FigureId{2}) == 2);
    CHECK(w.figures().list.slots[2]->parent == sim::FigureId{1});
    CHECK(w.figures().list.slots[2]->crew_slot == 1);
    CHECK(w.figures().list.slots[37]->owner == sim::PlayerId{7});
    CHECK(w.figures().list.slots[2]->state_word == 0x88);
    CHECK(w.figures().list.slots[37]->state_word == 0x8000);
    CHECK(w.figures().list.slots[2]->hp == 350);
    REQUIRE(w.figures().list.slots[37]->nav);
    CHECK(w.figures().list.slots[37]->nav->health == 6000);
    CHECK(w.figures().list.slots[37]->pos == sim::FixedPos{281, 640});
    CHECK(w.figures().list.slots[37]->nav->pos_u[0] == 2362232064000LL);
    CHECK(w.figures().list.slots[37]->holds == std::vector<sim::ShipHold>{{41, 768}, {0, 0}});
    CHECK(w.check_invariants().empty());
    auto copy = w.figures();
    sim::Figure next;
    next.id = sim::FigureId{3};
    CHECK(sim::add_figure(copy, next, w.config(), w.rules()) == 3);
    CHECK(copy.list.hint == 3);
    const auto saved = w.save();
    auto restored = sim::World::load(saved, types);
    REQUIRE(restored);
    CHECK(restored.value().save() == saved);
    CHECK(restored.value().figures().list.hint == 2);
    CHECK(restored.value().check_invariants().empty());
}

TEST_CASE("UNIT1 import metadata changes imported health, never native runtime identity") {
    auto a = registry(350), b = registry(351), absent = registry(350, false);
    CHECK(a->digest() == b->digest());
    CHECK(a->digest() == absent->digest());
    auto bytes = source(tagged("SOLDAT", unit(0, scout_kind, scout_records(0))));
    auto first = sim::load_scenario(bytes, *a, {});
    auto second = sim::load_scenario(bytes, *b, {});
    REQUIRE(first);
    REQUIRE(second);
    auto x = made(first.value().spec, a), y = made(second.value().spec, b);
    CHECK(sim::find_figure(x.figures(), sim::FigureId{1})->hp == 350);
    CHECK(sim::find_figure(y.figures(), sim::FigureId{1})->hp == 351);
    CHECK(x.state_hash() != y.state_hash());
    auto loaded = sim::World::load(x.save(), b);
    REQUIRE(loaded);
    CHECK(loaded.value().save() == x.save());
    CHECK(loaded.value().initialize_scripts().empty());
    CHECK(x.initialize_scripts().empty());
    x.advance(10);
    loaded.value().advance(10);
    CHECK(loaded.value().save() == x.save());
    auto missing = sim::load_scenario(bytes, *absent, {});
    REQUIRE_FALSE(missing);
    CHECK(missing.error().find("import facts") != std::string::npos);
}

TEST_CASE("UNIT1 original endless01 imported starting prerequisites") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto file = *game / "Scenes/0011SingleEndlosAddon/endless01.sze";
    std::ifstream in(file, std::ios::binary);
    REQUIRE(in.good());
    Buf bytes{std::istreambuf_iterator<char>(in), {}};
    CHECK(anno::formats::sha256_hex(bytes) ==
          "b289be7fe906264b1d0fe0b2be75c0e68813ed6691128c84cfebb6e4fd4cc8c0");
    auto original =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(original.error.empty(), original.error);
    const auto* ship_facts = original.types->unit_import_facts(1503);
    const auto* scout_facts = original.types->unit_import_facts(717);
    REQUIRE(ship_facts);
    REQUIRE(scout_facts);
    CHECK(ship_facts->max_hp == 12000);
    CHECK(ship_facts->cargo_slots == 8);
    CHECK(ship_facts->crew_slots == 3);
    CHECK(ship_facts->other_crew == 6);  // LOGIC26, not cannon-capacity byte15 (15).
    CHECK(scout_facts->other_crew == 0); // LOGIC15 is 1, a different property.
    auto templates = anno::sim_load::scan_island_templates(*game);
    auto loaded =
        anno::sim_load::load_scenario_file(file, *game, templates, *original.types, {}, {});
    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
    const auto& spec = loaded.value().loaded.spec;
    REQUIRE(spec.figures.size() == 5);
    CHECK(spec.original_figure_slots == 100);
    CHECK(anno::formats::sha256_hex(
              anno::formats::Bytes(reinterpret_cast<const std::uint8_t*>(spec.script_source.data()),
                                   spec.script_source.size())) ==
          "cac0aeb9c14dfd5c0f9b18c664e5282dff57096e2938f93c67391e3c3f8a47d0");
    auto w = made(spec, original.types);
    CHECK(w.figures().list.slots.size() == 100);
    CHECK(w.figures().list.hint == 4);
    CHECK(w.figures().list.cursor == 0);
    CHECK(w.ids().next_figure == 6);
    const std::array<std::uint32_t, 5> kinds{1503, 717, 718, 717, 718};
    const std::array<std::uint32_t, 5> hp{0, 350, 400, 350, 400};
    const std::array<std::optional<std::uint32_t>, 5> parents{std::nullopt, 0, 1, 0, 3};
    const std::array<std::optional<std::uint32_t>, 5> crew_slots{std::nullopt, 0, std::nullopt, 1,
                                                                 std::nullopt};
    for (std::uint32_t i = 0; i < 5; ++i) {
        REQUIRE(w.figures().list.slots[i]);
        const auto& f = *w.figures().list.slots[i];
        CHECK(f.id == sim::FigureId{std::uint64_t{i} + 1});
        CHECK(f.kind == kinds[i]);
        CHECK(f.owner == sim::PlayerId{0});
        CHECK(f.hp == hp[i]);
        CHECK(f.state_word == (i == 0 ? 0x8000U : 0x88U));
        CHECK(f.pos == sim::FixedPos{89984, 98176});
        CHECK(f.dir16 == (i == 0 ? 4 : 0));
        CHECK(f.crew_slot == crew_slots[i]);
        CHECK(f.parent ==
              (parents[i] ? sim::FigureId{std::uint64_t{*parents[i]} + 1} : sim::FigureId{}));
    }
    const auto& ship = *w.figures().list.slots[0];
    REQUIRE(ship.nav);
    CHECK(ship.nav->health == 12000);
    CHECK(ship.holds == std::vector<sim::ShipHold>{{41, 12800},
                                                   {41, 12800},
                                                   {26, 12800},
                                                   {42, 12800},
                                                   {42, 12800},
                                                   {41, 12800},
                                                   {43, 12800},
                                                   {26, 12800}});
    CHECK(w.players().find(sim::PlayerId{0})->money == 10000000);
    CHECK(w.clock().tick() == 0);
    CHECK(w.check_invariants().empty());
    const auto saved = w.save();
    auto restored = sim::World::load(saved, original.types);
    REQUIRE(restored);
    CHECK(restored.value().save() == saved);
    MESSAGE("UNIT1 early prerequisite: five original actors; source slots0..4 in100; "
            "hint4; original health/cargo/position and complete source retained; no movement yet");
}

TEST_CASE("UNIT1 scouts do not qualify as the ship HEAD abstract crew") {
    auto types = registry();
    for (bool abstract_crew : {false, true}) {
        // A carried class97 scout is present in both controls; only HEAD8 distinguishes them.
        Buf h = head(128, 0);
        h[8] = abstract_crew ? 1 : 0;
        Buf records = tagged("HEAD", h);
        append(records, tagged("WAREN", holds()));
        append(records, tagged("PRIVAT", crew()));
        auto loaded =
            sim::load_scenario(source(tagged("SCHIFF", unit(37, ship_kind, records))), *types, {});
        REQUIRE(loaded);
        REQUIRE(loaded.value().spec.figures.size() == 2);
        CHECK(loaded.value().spec.figures[0].state_word == (abstract_crew ? 0U : 0x8000U));
        CHECK(loaded.value().spec.figures[1].state_word == 0x88);
        auto w = made(loaded.value().spec, types);
        CHECK(w.figures().list.slots[37]->state_word == (abstract_crew ? 0U : 0x8000U));
        CHECK(w.check_invariants().empty());
    }
}

TEST_CASE("UNIT1 original foot roles refuse navigating metadata without changing authored types") {
    const auto bytes = source(tagged("SOLDAT", unit(0, scout_kind, scout_records())));
    auto foot = registry();
    auto nav = registry(350, true, true);
    auto legal = sim::load_scenario(bytes, *foot, {});
    REQUIRE(legal);
    CHECK(legal.value().spec.figures[0].hp == 350);
    CHECK_FALSE(legal.value().spec.figures[0].health.has_value());
    auto rejected = sim::load_scenario(bytes, *nav, {});
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().find("unsupported actor") != std::string::npos);
    CHECK(nav->find_figure_type(scout_kind)->nav.has_value());
}

namespace {
Buf crew_entry(std::uint16_t raw, std::uint8_t slot, std::uint16_t kind = scout_kind,
               const Buf& priv = {}) {
    Buf b;
    u16(b, raw);
    u16(b, kind);
    b.push_back(slot);
    b.push_back(0);
    append(b, scout_records(350, priv));
    return tagged("ENTRY", b);
}
Buf esel(std::uint16_t raw = 4, std::uint16_t kind = donkey_kind) {
    Buf b;
    u16(b, kind);
    u16(b, raw);
    u16(b, 6000);
    u16(b, 7000); // Held coordinates are ignored, even outside this world's bounds.
    append(b, tagged("HEAD", walker_head(400)));
    return tagged("ESEL", b);
}
void refused(const Buf& file, std::string_view reason, sim::UnitImportOptions options = {}) {
    const auto types = registry();
    auto loaded = sim::load_scenario(file, *types, {}, options);
    CAPTURE(reason);
    REQUIRE_FALSE(loaded);
    CAPTURE(loaded.error());
    CHECK(loaded.error().find(reason) != std::string::npos);
}
} // namespace

TEST_CASE("UNIT1 authored health and independent HEAD state vectors") {
    const auto types = registry();
    const std::array<std::pair<std::uint8_t, std::uint32_t>, 4> ratios{
        std::pair{std::uint8_t{0}, 12000U}, {1, 93}, {64, 6000}, {128, 12000}};
    for (const auto& [ratio, hp] : ratios) {
        auto loaded = sim::load_scenario(
            source(tagged("SCHIFF", unit(0, ship_kind, ship_records({}, ratio)))), *types, {});
        REQUIRE(loaded);
        CHECK(loaded.value().spec.figures[0].health == hp);
    }
    refused(source(tagged("SCHIFF", unit(0, ship_kind, ship_records({}, 129)))), "health exceeds");
    const std::array<std::pair<std::uint16_t, std::uint32_t>, 5> health{
        std::pair{std::uint16_t{175}, 175U}, {0, 350}, {0xffff, 350}, {350, 350}, {351, 350}};
    for (const auto& [raw, hp] : health) {
        auto loaded = sim::load_scenario(
            source(tagged("SOLDAT", unit(0, scout_kind, scout_records(raw)))), *types, {});
        REQUIRE(loaded);
        CHECK(loaded.value().spec.figures[0].hp == hp);
        CHECK(loaded.value().spec.figures[0].state_word == 0);
        CHECK(loaded.value().spec.figures[0].dir16 == 8);
    }
    for (std::uint8_t sign :
         {std::uint8_t{0}, std::uint8_t{127}, std::uint8_t{128}, std::uint8_t{255}}) {
        Buf r = tagged("HEAD", walker_head(350, sign));
        append(r, tagged("WAREN", Buf(16, 0)));
        auto loaded =
            sim::load_scenario(source(tagged("SOLDAT", unit(0, scout_kind, r))), *types, {});
        REQUIRE(loaded);
        CHECK(loaded.value().spec.figures[0].state_word == (sign < 128 ? 0x20U : 0U));
    }
    Buf h = head(128, 0, 0, 3);
    Buf r = tagged("HEAD", h);
    append(r, tagged("WAREN", holds()));
    auto flags = sim::load_scenario(source(tagged("SCHIFF", unit(0, ship_kind, r))), *types, {});
    REQUIRE(flags);
    CHECK(flags.value().spec.figures[0].state_word == 0x8120);
}

TEST_CASE("UNIT1 authored positions retain Q32 fractions and round ties to even") {
    const auto types = registry();
    struct Point {
        std::uint32_t bits;
        std::int32_t q8;
        std::int64_t q32;
    };
    const std::array<Point, 6> points{{{0x3f8ccccd, 281, 4724464128LL},
                                       {0x2f000000, 0, 0},
                                       {0x2fc00000, 0, 2},
                                       {0x2f800000, 0, 1},
                                       {0, 0, 0},
                                       {0x80000000, 0, 0}}};
    for (const auto p : points) {
        auto loaded = sim::load_scenario(
            source(tagged("SCHIFF", unit(0, ship_kind, ship_records(), p.bits))), *types, {});
        REQUIRE(loaded);
        const auto& f = loaded.value().spec.figures[0];
        CHECK(f.pos.x == p.q8);
        CHECK(f.pos.y == 640);
        CHECK(f.dir16 == 4);
        REQUIRE(f.nav_position_q32);
        CHECK((*f.nav_position_q32)[0] == p.q32);
        auto w = made(loaded.value().spec, types);
        const auto* actor = sim::find_figure(w.figures(), sim::FigureId{1});
        REQUIRE(actor);
        REQUIRE(actor->nav);
        CHECK(actor->nav->pos_u[0] == p.q32 * 500);
        auto copy = sim::World::load(w.save(), types);
        REQUIRE(copy);
        CHECK(copy.value().save() == w.save());
    }
    for (const auto bits :
         {0xbf800000U, 0x80000001U, 0x7f800000U, 0x7fc00000U, 0x7f7fffffU, 0x44000000U}) {
        refused(source(tagged("SCHIFF", unit(0, ship_kind, ship_records(), bits))), "position");
    }
}

TEST_CASE("UNIT1 cargo remaps the low goods byte and preserves ordered empty and named slots") {
    const auto custom = registry(350, true, false, true);
    for (const std::uint8_t high : {std::uint8_t{0}, std::uint8_t{0xab}}) {
        CAPTURE(high);
        auto cargo = holds();
        cargo[1] = high;
        Buf records = tagged("HEAD", head(64, 0));
        append(records, tagged("WAREN", cargo));
        auto decoded =
            sim::load_scenario(source(tagged("SCHIFF", unit(0, ship_kind, records))), *custom, {});
        REQUIRE(decoded);
        CHECK(decoded.value().spec.figures[0].holds ==
              std::vector<sim::ShipHold>{{90001, 768}, {0, 0}});
        auto w = made(decoded.value().spec, custom);
        CHECK(sim::find_figure(w.figures(), sim::FigureId{1})->holds[0].goods == 90001);
    }
    Buf r = tagged("HEAD", head(128, 0));
    append(r, tagged("WAREN", holds(50)));
    auto clamped = sim::load_scenario(source(tagged("SCHIFF", unit(0, ship_kind, r))), *custom, {});
    REQUIRE(clamped);
    CHECK(clamped.value().spec.figures[0].holds[0].amount_88 == 2560);
    r = tagged("HEAD", head(128, 0));
    append(r, tagged("WAREN", holds(0)));
    auto zero = sim::load_scenario(source(tagged("SCHIFF", unit(0, ship_kind, r))), *custom, {});
    REQUIRE(zero);
    CHECK(zero.value().spec.figures[0].holds[0].goods == 90001);
    CHECK(zero.value().spec.figures[0].holds[0].amount_88 == 0);
    Buf unknown = holds(3);
    unknown[0] = 42;
    r = tagged("HEAD", head(128, 0));
    append(r, tagged("WAREN", unknown));
    refused(source(tagged("SCHIFF", unit(0, ship_kind, r))), "unknown/unmapped goods");
    unknown = holds(0);
    unknown[0] = 42;
    r = tagged("HEAD", head(128, 0));
    append(r, tagged("WAREN", unknown));
    auto empty = sim::load_scenario(source(tagged("SCHIFF", unit(0, ship_kind, r))), *custom, {});
    REQUIRE(empty);
    CHECK(empty.value().spec.figures[0].holds[0].goods == 0);
    CHECK(empty.value().spec.figures[0].holds[0].amount_88 == 0);
    CHECK(std::any_of(
        empty.value().warnings.begin(), empty.value().warnings.end(),
        [](const auto& x) { return x.message.find("empty unknown") != std::string::npos; }));
}

TEST_CASE("UNIT1 held donkey hierarchy uses the nonspatial parent cache through native reload") {
    const auto types = registry();
    auto decoded = sim::load_scenario(
        source(tagged("SCHIFF", unit(37, ship_kind, ship_records(crew(2, 1, 350, esel()))))),
        *types, {});
    REQUIRE(decoded);
    REQUIRE(decoded.value().spec.figures.size() == 3);
    auto& spec = decoded.value().spec;
    CHECK(spec.figures[2].parent == 1);
    CHECK_FALSE(spec.figures[2].crew_slot);
    CHECK(spec.figures[2].pos == spec.figures[1].pos);
    CHECK(spec.figures[2].hp == 400);
    CHECK(spec.figures[2].state_word == 0x88);
    CHECK(spec.figures[2].dir16 == 0);
    auto w = made(spec, types);
    CHECK(w.figures().by_id.at(sim::FigureId{3}) == 4);
    CHECK(w.figures().children.at(sim::FigureId{1}) ==
          std::vector<sim::FigureId>{sim::FigureId{2}});
    CHECK(w.figures().children.at(sim::FigureId{2}) ==
          std::vector<sim::FigureId>{sim::FigureId{3}});
    CHECK(w.figures().list.hint == 4);
    auto players = w.players();
    auto timers = sim::make_script_timers(w.config());
    const std::string query =
        "[OBJECT_DEFINE]\n[AREA_DEFINE]\n\"PATCH\" 2,1,2,3\n"
        "[PLAYERMAPPER_DEFINE]\n\"ME\" 7\n[SCRIPT_KERNEL]\nSET HIT=FALSE;\n"
        "IF AREA \"PATCH\" TOUCHED_BY_SOLDIER_FROM \"ME\" THEN { HIT=TRUE; }\n"
        "[SIGNALS]\n[CAMARAPOS]\n0,0\n";
    auto runtime = sim::prepare_script(query, *types, players, w.config());
    REQUIRE(runtime);
    REQUIRE(sim::advance_script(runtime.value(), players, timers, *types, 250, &w.figures(),
                                &w.settings()));
    CHECK_FALSE(runtime.value().state.signals[1]);
    CHECK(runtime.value().query_stats.inserted ==
          1); // Only unparented ship, not its cached children.
    auto restored = sim::World::load(w.save(), types);
    REQUIRE(restored);
    CHECK(restored.value().save() == w.save());
    CHECK(restored.value().check_invariants().empty());
}

TEST_CASE("UNIT1 malformed and unsupported actors refuse the complete import") {
    const auto legal = source(tagged("SCHIFF", unit(37, ship_kind, ship_records(crew()))));
    auto positive = sim::load_scenario(legal, *registry(), {});
    REQUIRE(positive);
    struct Bad {
        std::string name;
        Buf records;
        std::string reason;
    };
    std::vector<Bad> cases;
    cases.push_back({"short prefix", tagged("SCHIFF", Buf(15, 0)), "prefix"});
    cases.push_back({"duplicate raw across parent",
                     tagged("SCHIFF", unit(37, ship_kind, ship_records(crew(37)))),
                     "duplicate raw"});
    Buf entries = crew_entry(2, 1);
    append(entries, crew_entry(3, 1));
    cases.push_back({"duplicate crew slot",
                     tagged("SCHIFF", unit(37, ship_kind, ship_records(tagged("CREW", entries)))),
                     "crew slot"});
    cases.push_back({"unavailable crew slot",
                     tagged("SCHIFF", unit(37, ship_kind, ship_records(crew(2, 2)))), "crew slot"});
    cases.push_back({"ship in SOLDAT", tagged("SOLDAT", unit(37, ship_kind, ship_records())),
                     "unsupported actor"});
    cases.push_back(
        {"ship in CREW",
         tagged("SCHIFF",
                unit(37, ship_kind, ship_records(tagged("CREW", crew_entry(2, 1, ship_kind))))),
         "unsupported actor"});
    cases.push_back(
        {"scout in ESEL",
         tagged("SCHIFF", unit(37, ship_kind, ship_records(crew(2, 1, 350, esel(4, scout_kind))))),
         "unsupported actor"});
    cases.push_back({"unheld ESEL",
                     tagged("SOLDAT", unit(2, scout_kind, scout_records(350, esel()))),
                     "unheld ESEL"});
    Buf r = ship_records();
    append(r, tagged("HEAD", head(128, 0)));
    cases.push_back({"duplicate HEAD", tagged("SCHIFF", unit(37, ship_kind, r)), "HEAD"});
    cases.push_back({"missing HEAD",
                     tagged("SCHIFF", unit(37, ship_kind, tagged("WAREN", holds()))),
                     "missing required"});
    r = tagged("HEAD", Buf(15, 0));
    append(r, tagged("WAREN", holds()));
    cases.push_back({"short HEAD", tagged("SCHIFF", unit(37, ship_kind, r)), "record length"});
    cases.push_back({"missing WAREN",
                     tagged("SCHIFF", unit(37, ship_kind, tagged("HEAD", head(128, 0)))),
                     "missing required"});
    for (std::size_t bytes : {15U, 24U}) {
        r = tagged("HEAD", head(128, 0));
        append(r, tagged("WAREN", Buf(bytes, 0)));
        cases.push_back(
            {"wrong WAREN length", tagged("SCHIFF", unit(37, ship_kind, r)), "hold record length"});
    }
    r = ship_records();
    append(r, tagged("WAREN", holds()));
    cases.push_back({"duplicate WAREN", tagged("SCHIFF", unit(37, ship_kind, r)), "WAREN"});
    r = ship_records();
    append(r, tagged("PRIVAT", {}));
    append(r, tagged("PRIVAT", {}));
    cases.push_back(
        {"duplicate PRIVAT", tagged("SCHIFF", unit(37, ship_kind, r)), "duplicate record"});
    r = ship_records();
    append(r, tagged("TASK", Buf{1}));
    cases.push_back({"task not silently omitted", tagged("SCHIFF", unit(37, ship_kind, r)),
                     "unsupported unit record"});
    cases.push_back({"route not silently omitted",
                     tagged("SCHIFF", unit(37, ship_kind, ship_records(tagged("ROUTE", Buf{1})))),
                     "unsupported required"});
    cases.push_back({"nested role depth",
                     tagged("SOLDAT", unit(2, scout_kind, scout_records(350, crew()))),
                     "unsupported required"});
    auto prefix = unit(37, ship_kind, ship_records());
    prefix[6] = 1;
    cases.push_back({"unported activation", tagged("SCHIFF", prefix), "activation flags"});
    cases.push_back(
        {"bad ship facing",
         tagged("SCHIFF", unit(37, ship_kind, ship_records(), 0x3f8ccccd, 0x40200000, 16)),
         "facing"});
    cases.push_back(
        {"bad scout facing",
         tagged("SOLDAT", unit(37, scout_kind, scout_records(), 0x3f8ccccd, 0x40200000, 8)),
         "facing"});
    cases.push_back({"unknown required type", tagged("SOLDAT", unit(37, 59999, scout_records())),
                     "unsupported actor"});
    for (const auto& bad : cases) {
        CAPTURE(bad.name);
        refused(source(bad.records), bad.reason);
    }
    // Zero lies outside a zero-width world too; canonical -0 must not bypass bounds.
    refused(source(tagged("SCHIFF", unit(0, ship_kind, ship_records(), 0)), 100, 0), "position");
}

TEST_CASE("UNIT1 import allocation budgets refuse required growth without clipping") {
    const auto legal = source(tagged("SCHIFF", unit(37, ship_kind, ship_records())), 100);
    sim::UnitImportOptions options;
    options.max_registry_slots = 100;
    auto loaded = sim::load_scenario(legal, *registry(), {}, options);
    REQUIRE(loaded);
    refused(source(tagged("SCHIFF", unit(100, ship_kind, ship_records())), 100), "growth exceeds",
            options);
    options.max_registry_slots = 124;
    refused(source(tagged("SCHIFF", unit(100, ship_kind, ship_records())), 100), "growth exceeds",
            options);
    options.max_registry_slots = 125;
    auto exact =
        sim::load_scenario(source(tagged("SCHIFF", unit(100, ship_kind, ship_records())), 100),
                           *registry(), {}, options);
    REQUIRE(exact);
    auto w = made(exact.value().spec, registry());
    CHECK(w.figures().list.slots.size() == 125);
    refused(source({}, UINT32_MAX), "MAIN registry length");
    options = {};
    options.max_actors = 1;
    refused(source(tagged("SCHIFF", unit(37, ship_kind, ship_records(crew())))), "actor budget",
            options);
    options = {};
    options.max_registry_slots = 0;
    refused(legal, "invalid allocation", options);
    options = {};
    options.max_actors = sim::kListSizeBound + 1;
    refused(legal, "invalid allocation", options);
}

TEST_CASE("UNIT1 actor-free component projection is explicit and never a refusal fallback") {
    const auto bytes = source(tagged("SOLDAT", unit(0, 59999, scout_records())));
    refused(bytes, "unsupported actor");
    sim::UnitImportOptions options;
    options.projection = sim::OriginalActorProjection::ActorFreeComponent;
    auto component = sim::load_scenario(bytes, *registry(), {}, options);
    REQUIRE(component);
    CHECK(component.value().spec.figures.empty());
    CHECK_FALSE(component.value().spec.original_figure_slots);
    CHECK(std::any_of(component.value().warnings.begin(), component.value().warnings.end(),
                      [](const auto& x) { return x.record == "SOLDAT"; }));
    CHECK(std::any_of(component.value().warnings.begin(), component.value().warnings.end(),
                      [](const auto& x) { return x.record == "MAIN"; }));
    options.projection = static_cast<sim::OriginalActorProjection>(255);
    refused(bytes, "unknown actor projection", options);
}

TEST_CASE("UNIT1 import catalogs reject orphan and duplicate type metadata") {
    sim::TypeRegistry::Builder orphan;
    orphan.unit_import_facts(12345, {350, 2, 0, 1});
    auto missing = orphan.build();
    REQUIRE_FALSE(missing);
    CHECK(missing.error().find("unknown figure") != std::string::npos);
    sim::TypeRegistry::Builder duplicate;
    sim::FigureType scout{scout_kind, 0, 2560, 0, 0};
    scout.bewohntype = 97;
    duplicate.figure_type(scout)
        .unit_import_facts(scout_kind, {350, 2, 0, 1})
        .unit_import_facts(scout_kind, {351, 2, 0, 1});
    auto repeated = duplicate.build();
    REQUIRE_FALSE(repeated);
    CHECK(repeated.error().find("duplicate unit import facts") != std::string::npos);
}

TEST_CASE("UNIT1 zero positions require a nonempty world extent before canonicalization") {
    const auto types = registry();
    for (auto bits : {0U, 0x80000000U}) {
        CAPTURE(bits);
        const auto actor = tagged("SCHIFF", unit(0, ship_kind, ship_records(), bits));
        auto legal = sim::load_scenario(source(actor, 100, 512), *types, {});
        REQUIRE(legal);
        CHECK(legal.value().spec.figures[0].pos.x == 0);
        auto empty = sim::load_scenario(source(actor, 100, 0), *types, {});
        REQUIRE_FALSE(empty);
        CHECK(empty.error().find("nonempty world extent") != std::string::npos);
    }
}

TEST_CASE("UNIT1 raw figure LOGIC distinguishes abstract crew from cannon capacity") {
    // First run an equal-fields legal control, then a value that separates the two getters.
    for (auto counts : {std::pair{std::uint8_t{6}, std::uint8_t{6}},
                        std::pair{std::uint8_t{15}, std::uint8_t{0}}}) {
        CAPTURE(counts.first);
        CAPTURE(counts.second);
        Buf type_head;
        u32(type_head, 0x3de147ae);
        u32(type_head, 0x3d1374bc);
        type_head.resize(24, 0);
        Buf name{'S', 'H', 'I', 'P', 0};
        Buf logic(44, 0);
        logic[0x11] = 50;
        logic[0x13] = 2;
        logic[0x14] = 2;
        logic[0x15] = counts.first;  // Original +0x101, separate cannon getter.
        logic[0x26] = counts.second; // Original +0xf8, slot+0x88(mode2).
        logic[0x1e] = 0xe0;
        logic[0x1f] = 0x2e; // 12000 HP.
        const auto rec = [](std::string_view label, const Buf& data) {
            anno::formats::chunk::Chunk r;
            std::copy(label.begin(), label.end(), r.name_field.begin());
            r.payload = data;
            return r;
        };
        anno::formats::figuren::Entry e;
        e.id = ship_kind;
        e.records = {rec("HEAD", type_head), rec("BEWOHNTYPE", name), rec("LOGIC", logic)};
        auto built = sim::TypeRegistry::build({}, sim::original_goods(), {}, std::span(&e, 1),
                                              sim::synthetic_walk_tables());
        REQUIRE_MESSAGE(built, (built ? "" : built.error()));
        const auto* facts = built.value()->unit_import_facts(ship_kind);
        REQUIRE(facts);
        CHECK(facts->max_hp == 12000);
        CHECK(facts->cargo_slots == 2);
        CHECK(facts->crew_slots == 2);
        CHECK(facts->other_crew == counts.second);
        Buf h = head(128, 0);
        h[8] = 1;
        Buf records = tagged("HEAD", h);
        append(records, tagged("WAREN", holds()));
        auto loaded = sim::load_scenario(source(tagged("SCHIFF", unit(0, ship_kind, records))),
                                         *built.value(), {});
        REQUIRE(loaded);
        CHECK(loaded.value().spec.figures[0].state_word == (counts.second == 0 ? 0x8000U : 0U));
        auto w = made(loaded.value().spec, built.value());
        CHECK(w.figures().list.slots[0]->state_word == (counts.second == 0 ? 0x8000U : 0U));
    }
}

TEST_CASE("UNIT1 original corpus imports complete actor counts or explicitly refuses") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto registry =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE(registry.error.empty());
    std::ifstream inventory(std::filesystem::path(ANNO_FIXTURES_DIR) /
                            "sim/unit_import1/corpus_counts.txt");
    REQUIRE(inventory.good());
    std::set<std::string> expected_files;
    std::uint64_t actors = 0, imported_actors = 0;
    std::uint32_t accepted = 0, refused = 0;
    bool endless01 = false;
    for (std::string line; std::getline(inventory, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        std::string file, hash;
        std::uint64_t count = 0;
        row >> file >> hash >> count;
        REQUIRE_FALSE(row.fail());
        row >> std::ws;
        REQUIRE(row.eof());
        REQUIRE(expected_files.insert(file).second);
        INFO(file);
        std::ifstream in(*game / file, std::ios::binary);
        REQUIRE(in.good());
        const Buf bytes{std::istreambuf_iterator<char>(in), {}};
        REQUIRE(anno::formats::sha256_hex(bytes) == hash);
        actors += count;
        const auto loaded = sim::load_scenario(bytes, *registry.types, {});
        if (!loaded) {
            ++refused;
            CHECK(loaded.error().find("players[") != std::string::npos);
            CHECK(loaded.error().find("unsupported") != std::string::npos);
            MESSAGE("UNIT1_CORPUS " << file << " accepted=0 raw_actors=" << count
                                    << " dependency=" << loaded.error());
            continue;
        }
        ++accepted;
        CHECK(loaded.value().spec.figures.size() == count);
        imported_actors += loaded.value().spec.figures.size();
        if (file == "Scenes/0011SingleEndlosAddon/endless01.sze") {
            endless01 = true;
            CHECK(count == 5);
        }
        MESSAGE("UNIT1_CORPUS " << file << " accepted=1 raw_actors=" << count
                                << " imported=" << loaded.value().spec.figures.size());
    }
    REQUIRE(inventory.eof());
    std::set<std::string> actual_files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(*game / "Scenes"))
        if (entry.is_regular_file() && entry.path().extension() == ".sze")
            actual_files.insert(std::filesystem::relative(entry.path(), *game).generic_string());
    CHECK(expected_files == actual_files);
    CHECK(expected_files.size() == 50);
    CHECK(actors == 3473);
    CHECK(accepted + refused == 50);
    CHECK(endless01);
    MESSAGE("UNIT1_CORPUS_TOTAL accepted=" << accepted << " refused=" << refused << " raw_actors="
                                           << actors << " imported_actors=" << imported_actors);
}
