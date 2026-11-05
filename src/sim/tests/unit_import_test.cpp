// Authored original-format records and primary-derived expectations. Never copy original assets.
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/sha256.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/ship.hpp>
#include <anno/sim/synthetic.hpp>
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
Buf source(const Buf& actors, std::uint32_t slots = 100) {
    Buf scene;
    Buf h;
    u32(h, 512);
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
std::shared_ptr<const sim::TypeRegistry>
registry(std::uint32_t scout_max = 350, bool metadata = true, bool navigating_scout = false) {
    sim::TypeRegistry::Builder b;
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
