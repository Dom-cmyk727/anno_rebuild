#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

#include <doctest/doctest.h>

#include <anno/formats/bigbrother.hpp>
#include <anno/formats/sze.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/route_world.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>

#include "world_access.hpp"
using namespace anno::sim;
namespace fs = std::filesystem;
namespace {
std::vector<std::uint8_t> read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    return {std::istreambuf_iterator<char>(in), {}};
}
void write(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.good());
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    REQUIRE(out.good());
}
struct Original {
    std::shared_ptr<const TypeRegistry> types;
    ScenarioSpec spec;
    World world;
};
Original original(const fs::path& game) {
    auto registry =
        anno::sim_load::load_original_registry(game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.error.empty(), registry.error);
    auto templates = anno::sim_load::scan_island_templates(game);
    auto loaded =
        anno::sim_load::load_scenario_file(game / "Scenes/0011SingleEndlosAddon/endless01.sze",
                                           game, templates, *registry.types, {}, {});
    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
    auto spec = std::move(loaded).value().loaded.spec;
    auto made = World::create(spec, registry.types);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    return {registry.types, std::move(spec), std::move(made).value()};
}
World synthetic_program(const Original& input) {
    auto spec = synthetic::basic_scenario(77);
    spec.settings.world_width = spec.settings.world_height = 1024;
    spec.script_context = ScriptContext::Pending;
    spec.script_source = input.spec.script_source; // entire unchanged program, runtime input only
    spec.script_asset_namespace = input.spec.script_asset_namespace;
    auto neutral = spec.players[0];
    neutral.id = PlayerId{31};
    spec.players.push_back(neutral);
    auto& island = spec.islands[0];
    island.x0 = 32;
    island.y0 = 176;
    island.cities.clear();
    island.deposits.clear();
    island.objects.clear();
    island.owner_runs.clear();
    island.ore_objects.clear();
    synthetic::give_flat_terrain(island, 0x30);
    auto other = island;
    other.slot = IslandId{1};
    other.x0 = 400;
    other.y0 = 16;
    spec.islands.push_back(other);
    // Authored synthetic starting positions, not the original scenario or a relocation.
    for (auto [kind, pos] : {std::pair{1503u, FixedPos{220 * 256, 700 * 256}},
                             std::pair{717u, FixedPos{65 * 256, 198 * 256}},
                             std::pair{717u, FixedPos{426 * 256, 46 * 256}}}) {
        ScenarioSpec::FigureSpec figure;
        figure.kind = kind;
        figure.owner = PlayerId{0};
        figure.pos = pos;
        spec.figures.push_back(figure);
    }
    auto made = World::create(spec, input.types);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    return std::move(made).value();
}
void observe(void* ctx, std::span<const SimEvent> events) {
    auto& out = *static_cast<std::vector<SimEvent>*>(ctx);
    for (const auto& e : events)
        if (e.kind == SimEventKind::ScriptWave)
            out.push_back(e);
}
void wave_ledger(const std::vector<SimEvent>& waves) {
    REQUIRE(waves.size() == 1);
    CHECK(waves[0].code == 317);
    CHECK(waves[0].tick == 18);
    CHECK_FALSE(waves[0].player.valid());
    REQUIRE(waves[0].wave);
    CHECK(waves[0].wave->asset == "scenes/0011SingleEndlosAddon/HAWAIMUCKE.WAV");
    CHECK(waves[0].wave->audience == WaveAudience::AllLocal);
}
void final_ledger(const World& w) {
    CHECK(w.script().state.signals == std::vector<std::uint8_t>{0, 0, 1, 1, 1, 1, 0, 0});
    CHECK(w.players().find(PlayerId{0})->money == 209998); // 10000+99999+99999, no building debit
    CHECK(w.players().find(PlayerId{31})->money == 10000);
    CHECK(w.players().find(PlayerId{0})->permissions.size() == 462);
    CHECK(w.check_invariants().empty());
}
struct Canary {
    ReplayLog normal, wrong_command, wrong_program;
    std::uint64_t final_hash = 0;
};
Canary produce(const Original& input) {
    auto w = synthetic_program(input);
    const auto source = w.script().source;
    const auto pending = w.save();
    auto started = Recorder::start(w);
    REQUIRE(started);
    auto rec = std::move(started).value();
    rec.checkpoint();
    CHECK(w.players().find(PlayerId{0})->money == 10000);
    REQUIRE(rec.submit(Command{PlayerId{0}, SetSpeedCmd{100}}).ok());
    rec.checkpoint();
    rec.advance(36);
    rec.checkpoint(); // oldtick0 visits no IF; nowtick18 visits exactly1 each call
    std::vector<std::size_t> cuts{0, 1, rec.log().entries.size()};
    std::vector<SimEvent> waves;
    for (std::uint32_t branch = 0; branch < 7; ++branch) {
        rec.advance(0);
        const auto events = w.drain_events();
        observe(&waves, events);
        const auto expected = branch < 5 ? 10000 : branch == 5 ? 109999 : 209998;
        CHECK(w.players().find(PlayerId{0})->money == expected);
        CHECK(w.script().state.cursor == (branch + 1) % 7);
        rec.checkpoint();
        if (branch == 0 || branch >= 5)
            cuts.push_back(rec.log().entries.size());
    }
    wave_ledger(waves);
    final_ledger(w);
    CHECK(w.script().source == source);
    rec.advance(0);
    rec.checkpoint(); // next visit has no repeat event/reward
    CHECK(w.drain_events().empty());
    final_ledger(w);
    const auto final = w.save();
    for (const auto cut : cuts) {
        auto prefix = rec.log();
        prefix.entries.resize(cut);
        auto before = World::load(pending, input.types);
        REQUIRE(before);
        std::vector<SimEvent> prefix_waves;
        auto a = replay(before.value(), prefix, {&prefix_waves, observe});
        REQUIRE(a);
        REQUIRE(a.value().reproduced());
        auto restored = World::load(before.value().save(), input.types);
        REQUIRE(restored);
        auto suffix = rec.log();
        suffix.entries.erase(suffix.entries.begin(),
                             suffix.entries.begin() + static_cast<std::ptrdiff_t>(cut));
        CHECK(restored.value().drain_events().empty());
        std::vector<SimEvent> suffix_waves;
        auto b = replay(restored.value(), suffix, {&suffix_waves, observe});
        REQUIRE(b);
        CHECK(b.value().reproduced());
        CHECK(restored.value().save() == final);
        CHECK(prefix_waves.size() == (cut > 6 ? 1u : 0u));
        CHECK(suffix_waves.size() == (cut <= 6 ? 1u : 0u));
        prefix_waves.insert(prefix_waves.end(), suffix_waves.begin(), suffix_waves.end());
        wave_ledger(prefix_waves);
    }
    auto wrong = rec.log();
    REQUIRE(wrong.entries[2].kind == ReplayEntry::Kind::Commands);
    std::get<SetSpeedCmd>(wrong.entries[2].commands[0].body).percent = 200;
    auto legal = World::load(pending, input.types);
    REQUIRE(legal);
    REQUIRE(legal.value().initialize_scripts().empty());
    REQUIRE(legal.value().apply(wrong.entries[2].commands[0]).ok());
    auto changed_program = rec.log();
    auto changed = World::load(pending, input.types);
    REQUIRE(changed);
    auto& runtime = WorldTestAccess::script(changed.value());
    runtime.source.replace(runtime.source.find("HAWAIMUCKE.WAV"), 13, "WRONG.WAV");
    REQUIRE(
        restore_script(runtime, *input.types, changed.value().players(), changed.value().config())
            .empty());
    changed_program.initial_save =
        changed.value().save(); // valid native frame/checksum, different content
    return {rec.log(), std::move(wrong), std::move(changed_program), w.state_hash()};
}
void verify(const Canary& canary, const std::shared_ptr<const TypeRegistry>& types) {
    auto w = World::load(canary.normal.initial_save, types);
    REQUIRE(w);
    std::vector<SimEvent> waves;
    auto replayed = replay(w.value(), canary.normal, {&waves, observe});
    REQUIRE(replayed);
    CHECK(replayed.value().reproduced());
    CHECK(w.value().state_hash() == canary.final_hash);
    final_ledger(w.value());
    wave_ledger(waves);
    auto wrong = World::load(canary.wrong_command.initial_save, types);
    REQUIRE(wrong);
    auto divergent = replay(wrong.value(), canary.wrong_command);
    REQUIRE(divergent);
    CHECK(divergent.value().diverged_entry == std::optional<std::uint64_t>{3});
    auto program = World::load(canary.wrong_program.initial_save, types);
    REQUIRE(program);
    auto refused = replay(program.value(), canary.wrong_program);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == "replay refused: script content digest differs from the log's");
}
} // namespace
TEST_CASE("SCRIPT2 original imported actors retain source and startup") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto path = *game / "Scenes/0011SingleEndlosAddon/endless01.sze";
    const auto raw = read(path);
    auto parsed = anno::formats::sze::Scene::parse(raw);
    REQUIRE(parsed);
    auto players = parsed.value().players();
    REQUIRE(players);
    std::uint32_t ships = 0, human_cities = 0;
    for (const auto& player : players.value())
        if (player.index == 0) {
            auto units = player.units("SCHIFF");
            REQUIRE(units);
            for (const auto& unit : units.value()) {
                ++ships;
                CHECK(unit.figure == 1503);
                CHECK(unit.x_bits == 0x43afc000);
                CHECK(unit.y_bits == 0x43bfc000);
            }
            for (const auto& record : player.records)
                human_cities += (record.name == "STADT" || record.name == "STADT2") ? 1u : 0u;
        }
    CHECK(ships == 1);
    CHECK(human_cities == 0);
    auto input = original(*game);
    CHECK(input.spec.figures.size() == 5);
    CHECK(input.world.figures().by_id.size() == 5);
    const auto* imported = find_figure(input.world.figures(), FigureId{1});
    REQUIRE(imported);
    CHECK(imported->kind == 1503);
    CHECK(imported->pos == FixedPos{89984, 98176});
    CHECK(imported->holds.size() == 8);
    auto payload = anno::formats::bigbrother::find_payload(raw);
    REQUIRE(payload);
    CHECK(input.spec.script_source == std::string(payload.value().begin(), payload.value().end()));
    CHECK(input.world.script().source == input.spec.script_source);
    const auto before = *input.world.players().find(PlayerId{0});
    const auto initial_hash = input.world.state_hash();
    const auto initial_rng = input.world.rng().cursor();
    const auto researched = [](const Player& p) {
        std::uint32_t count = 0;
        for (std::uint32_t i = 0; i < p.research.state.size(); ++i)
            count += p.research.researched(i) ? 1u : 0u;
        return count;
    };
    auto rec = Recorder::start(input.world);
    REQUIRE(rec);
    const auto& after = *input.world.players().find(PlayerId{0});
    CHECK(after.money == before.money);
    CHECK(after.permissions.size() == 462);
    CHECK(input.world.rng().cursor() == initial_rng);
    CHECK(input.world.clock().ms == 0);
    MESSAGE("ORIGINAL_INIT_LEDGER gold_before="
            << before.money << " gold_after=" << after.money
            << " permissions_before=" << before.permissions.size()
            << " permissions_after=" << after.permissions.size() << " stage_before=" << before.flags
            << " stage_after=" << after.flags << " researched_before=" << researched(before)
            << " researched_after=" << researched(after) << " initial_state_hash_u64="
            << initial_hash << " ready_state_hash_u64=" << input.world.state_hash()
            << " rng_before_after=" << initial_rng
            << " init_work=" << input.world.script().state.last_work);
    const auto& program = *input.world.script().program;
    REQUIRE(program.mappings.size() == input.world.script().state.bindings.size());
    for (std::size_t i = 0; i < program.mappings.size(); ++i)
        MESSAGE("ORIGINAL_MAPPING name="
                << program.mappings[i].name << " slot=" << program.mappings[i].slot
                << " actual_player=" << input.world.script().state.bindings[i].v);

    CHECK(input.world.script().state.context == ScriptContext::Ready);
    CHECK(input.world.script().program->branches.size() == 7);
    rec.value().advance(36);
    for (int i = 0; i < 7; ++i)
        rec.value().advance(0);
    CHECK(input.world.script().state.signals == std::vector<std::uint8_t>{0, 1, 1, 1, 1, 1, 1, 1});
    CHECK(input.world.drain_events().empty());
    MESSAGE("Original import prerequisite resolved: 1 raw Human ship, 5 imported actors, "
            "0 Human cities. Compile+INIT+false visits alone do not establish F2.");
}

TEST_CASE("SCRIPT2 original canary route and founding prerequisites") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    auto input = original(*game);
    auto& w = input.world;
    auto recorder = Recorder::start(w);
    REQUIRE(recorder);
    const auto before = w.state_hash();
    const auto* ship = find_figure(w.figures(), FigureId{1});
    REQUIRE(ship);
    REQUIRE(ship->nav);
    REQUIRE(ship->holds.size() == 8);
    const auto* warehouse = w.types().find(TypeId{1302});
    REQUIRE(warehouse);
    const WorldTileSource source(w, ship_route_terrain_options());
    const CoastIndex index(source, {});
    RouteQuery query;
    query.start_x = ship->pos.x / 256;
    query.start_z = ship->pos.y / 256;
    bool area_route = false;
    for (int y = 672; y <= 720 && !area_route; y += 8)
        for (int x = 224; x <= 272; x += 8) {
            if (!source.tile_is_water(x, y))
                continue;
            query.goal_x_q8 = x * 256 + 128;
            query.goal_z_q8 = y * 256 + 128;
            const auto route = plan_route(index, query, {}, RouteCostTable::original());
            if (route.status != RouteStatus::Planned || route.end_x < 218 || route.end_x >= 285 ||
                route.end_z < 671 || route.end_z >= 736)
                continue;
            MESSAGE("ORIGINAL_CANARY_AREA goal=" << x << "," << y << " endpoint=" << route.end_x
                                                 << "," << route.end_z);
            area_route = true;
            break;
        }
    REQUIRE(area_route);
    struct Candidate {
        std::int32_t x, y;
        std::int64_t distance;
    };
    std::vector<Candidate> candidates;
    for (const auto& slot : w.islands()) {
        if (!slot || slot->locked())
            continue;
        const auto& is = *slot;
        const auto grid = VertexGrid::of(is);
        if (!grid.ok())
            continue;
        for (std::int32_t y = is.tiles.y0() + 1;
             std::int64_t{y} < std::int64_t{is.tiles.y0()} + is.tiles.height(); ++y)
            for (std::int32_t x = is.tiles.x0() + 1;
                 std::int64_t{x} < std::int64_t{is.tiles.x0()} + is.tiles.width(); ++x) {
                const site::Origin origin{x, y};
                const auto rotation = site::first_accepted_rotation(0, [&](std::uint8_t r) {
                    const auto [fw, fh] = footprint(*warehouse, r);
                    return site::footprint_inside(is.tiles, origin, fw, fh) &&
                           site::footprint_ownership(is.tiles, w.config().neutral_owner, origin, fw,
                                                     fh) == site::Ownership::AllNeutral &&
                           site::footprint_occupancy(is.tiles, w.types(), *warehouse, origin, fw,
                                                     fh) == site::Compat::Compatible &&
                           site::coast_height_check(grid, *warehouse, r, origin).result ==
                               site::TerrainResult::Accepted;
                });
                if (rotation) {
                    const auto dx = std::int64_t{x + 1} - query.start_x;
                    const auto dy = std::int64_t{y + 1} - query.start_z;
                    candidates.push_back({x + 1, y + 1, dx * dx + dy * dy});
                }
            }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const auto& a, const auto& b) { return a.distance < b.distance; });
    REQUIRE_FALSE(candidates.empty());
    bool found = false;
    // Planning copies only: no relocation is ever applied to the executed canary world.
    // This checks the actual transaction/resource prerequisites before an expensive voyage.
    std::size_t tried = 0;
    for (const auto& c : candidates) {
        if (++tried > 128)
            break; // bounded planning search; refusal is not proof that no site exists
        for (const auto [dx, dy] :
             {std::pair{6, 0}, std::pair{-6, 0}, std::pair{0, 6}, std::pair{0, -6}}) {
            if (!source.tile_is_water(c.x + dx, c.y + dy))
                continue;
            auto trial = w;
            WorldTestAccess::relocate(trial, FigureId{1},
                                      {(c.x + dx) * 256 + 128, (c.y + dy) * 256 + 128});
            FoundCmd cmd{FigureId{1}, {TypeId{1302}, c.x - 1, c.y - 1}, c.x, c.y};
            if (!trial.apply({PlayerId{0}, cmd}).ok())
                continue;
            query.goal_x_q8 = (c.x + dx) * 256 + 128;
            query.goal_z_q8 = (c.y + dy) * 256 + 128;
            const auto planned = plan_route(index, query, {}, RouteCostTable::original());
            if (planned.status != RouteStatus::Planned || planned.end_x != c.x + dx ||
                planned.end_z != c.y + dy)
                continue;
            MESSAGE("ORIGINAL_CANARY_PLAN click=" << c.x << "," << c.y << " goal=" << c.x + dx
                                                  << "," << c.y + dy << " neutral_coast_candidates="
                                                  << candidates.size());
            found = true;
            break;
        }
        if (found)
            break;
    }
    REQUIRE(found);
    CHECK(w.state_hash() == before);
    CHECK(find_figure(w.figures(), FigureId{1})->pos == FixedPos{89984, 98176});
}

TEST_CASE("SCRIPT2 original public voyage founding and construction") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    auto input = original(*game);
    auto& w = input.world;
    REQUIRE(find_figure(w.figures(), FigureId{1})->pos == FixedPos{89984, 98176});
    auto started = Recorder::start(w);
    REQUIRE(started);
    auto rec = std::move(started).value();
    rec.checkpoint();
    REQUIRE(rec.submit({PlayerId{0}, SetSpeedCmd{100}}).ok());
    std::vector<SimEvent> waves;
    const auto source_holds = find_figure(w.figures(), FigureId{1})->holds;
    std::vector<std::size_t> cuts{0, 1};
    const auto sail = [&](int x, int y) {
        const auto ordered =
            rec.submit({PlayerId{0}, MoveCmd{FigureId{1}, x * 256 + 128, y * 256 + 128}});
        REQUIRE_MESSAGE(ordered.ok(), reject_name(ordered.status));
        const auto order_id = find_figure(w.figures(), FigureId{1})->nav->order->order_id;
        bool arrived = false;
        std::uint32_t advances = 0;
        // 1000ms =500ticks, well inside the supported u16 visit interval. Bound4000s
        // exceeds this route's distance at the source ship's evidenced minimum speed.
        for (; advances < 4000 && !arrived; ++advances) {
            const auto old_tick = w.clock().tick();
            const auto signal = w.script().state.signals[1];
            const auto point = find_figure(w.figures(), FigureId{1})->pos;
            const auto before_visit = rec.log().entries.size();
            rec.advance(1000);
            REQUIRE_FALSE(w.faulted());
            const auto events = w.drain_events();
            for (const auto& e : events) {
                REQUIRE(e.kind != SimEventKind::ShipRouteFailed);
                if (e.kind == SimEventKind::ShipArrived) {
                    CHECK(e.figure == FigureId{1});
                    CHECK(e.order_id == order_id);
                    arrived = true;
                }
                if (e.kind == SimEventKind::ScriptWave) {
                    CHECK(point.x / 256 >= 218);
                    CHECK(point.x / 256 < 285);
                    CHECK(point.y / 256 >= 671);
                    CHECK(point.y / 256 < 736);
                    CHECK(w.script().state.cursor == 0); // old tick >=250 visits all seven branches
                    CHECK(signal == 1);
                    CHECK(w.script().state.signals[1] == 0);
                    CHECK(e.tick == old_tick);
                    waves.push_back(e);
                }
            }
            rec.checkpoint();
            if (signal != w.script().state.signals[1]) {
                cuts.push_back(before_visit);
                cuts.push_back(rec.log().entries.size());
            }
        }
        REQUIRE(arrived);
        CHECK(find_figure(w.figures(), FigureId{1})->pos.x / 256 == x);
        CHECK(find_figure(w.figures(), FigureId{1})->pos.y / 256 == y);
        CHECK(find_figure(w.figures(), FigureId{1})->holds == source_holds);
        MESSAGE("ORIGINAL_VOYAGE goal=" << x << "," << y << " advances=" << advances
                                        << " tick=" << w.clock().tick());
        rec.checkpoint();
    };
    sail(224, 672);
    // Additional bounded visits at the arrived position must not repeat the
    // one-shot effect. At this old tick each call visits all seven branches.
    for (int i = 0; i < 7; ++i) {
        const auto tick = w.clock().tick();
        rec.advance(0);
        const auto events = w.drain_events();
        for (const auto& e : events)
            if (e.kind == SimEventKind::ScriptWave) {
                CHECK(e.tick == tick);
                waves.push_back(e);
            }
    }
    REQUIRE(waves.size() == 1);
    CHECK(waves[0].code == 317);
    CHECK(waves[0].tick == 48000);
    REQUIRE(waves[0].wave);
    CHECK(waves[0].wave->asset == "scenes/0011SingleEndlosAddon/HAWAIMUCKE.WAV");
    CHECK(waves[0].wave->audience == WaveAudience::AllLocal);
    CHECK(waves[0].wave->source_offset == 1068);
    CHECK_FALSE(waves[0].player.valid());
    CHECK(w.script().state.signals == std::vector<std::uint8_t>{0, 0, 1, 1, 1, 1, 1, 1});
    CHECK(w.players().find(PlayerId{0})->money == 10000000);
    sail(350, 405);
    CHECK(waves.size() == 1);
    CHECK(w.players().find(PlayerId{0})->money == 10000000);
    FoundCmd founding{FigureId{1}, {TypeId{1302}, 343, 404}, 344, 405};
    const auto before_holds = find_figure(w.figures(), FigureId{1})->holds;
    const auto founded = rec.submit({PlayerId{0}, founding});
    REQUIRE_MESSAGE(founded.ok(), reject_name(founded.status));
    REQUIRE(founded.records.size() == 1);
    const auto warehouse_id = founded.records[0].building;
    CHECK(w.players().find(PlayerId{0})->money == 9999650);
    const auto& holds = find_figure(w.figures(), FigureId{1})->holds;
    REQUIRE(holds.size() == before_holds.size());
    CHECK(holds[0].amount_88 == before_holds[0].amount_88 - 1280);
    CHECK(holds[3].amount_88 == before_holds[3].amount_88 - 3072);
    auto expected_holds = before_holds;
    expected_holds[0].amount_88 -= 1280;
    expected_holds[3].amount_88 -= 3072;
    CHECK(holds == expected_holds);
    rec.checkpoint();
    cuts.push_back(rec.log().entries.size());
    std::optional<CityRef> city;
    for (std::uint32_t i = 0; i < w.islands().size(); ++i)
        if (w.islands()[i])
            for (const auto& c : w.islands()[i]->cities)
                if (c && c->owner == PlayerId{0}) {
                    REQUIRE_FALSE(city);
                    city = CityRef{IslandId{i}, c->number};
                }
    REQUIRE(city);
    const auto& initial_stock = w.islands()[city->island.v]->city(city->city)->stock.stock;
    CHECK(std::all_of(initial_stock.begin(), initial_stock.end(), [](auto n) { return n == 0; }));
    for (std::uint32_t h = 0; h < holds.size(); ++h) {
        const auto hold = find_figure(w.figures(), FigureId{1})->holds[h];
        if (!hold.goods || hold.amount_88 < 256)
            continue;
        const auto& store = w.islands()[city->island.v]->city(city->city)->stock;
        const auto old_stock = store.stock[hold.goods];
        const auto room = store.capacity > old_stock ? store.capacity - old_stock : 0;
        const auto expected = std::min<std::uint64_t>(hold.amount_88, room);
        const auto old_hash = w.state_hash();
        REQUIRE(hold.amount_88 / 256 <= std::numeric_limits<std::uint32_t>::max());
        const auto unloaded =
            rec.submit({PlayerId{0}, UnloadCmd{FigureId{1}, warehouse_id, h, hold.goods,
                                               static_cast<std::uint32_t>(hold.amount_88 / 256)}});
        if (!expected) {
            CHECK(unloaded.status == Reject::StockFull);
            CHECK(w.state_hash() == old_hash);
            continue;
        }
        REQUIRE_MESSAGE(unloaded.ok(), "hold " << h << ": " << reject_name(unloaded.status));
        REQUIRE(unloaded.records.size() == 1);
        CHECK(unloaded.records[0].transferred_88 == expected);
        CHECK(store.stock[hold.goods] == old_stock + expected);
        CHECK(find_figure(w.figures(), FigureId{1})->holds[h].amount_88 ==
              hold.amount_88 - expected);
        CHECK(w.players().find(PlayerId{0})->money == 9999650);
    }
    rec.checkpoint();
    cuts.push_back(rec.log().entries.size());
    const auto& is = *w.islands()[city->island.v];
    const auto* handwerk = w.types().find(TypeId{1001});
    REQUIRE(handwerk);
    std::optional<PlacementWire> placement;
    std::optional<PlacementWire> alternate;
    for (std::int32_t y = is.tiles.y0();
         std::int64_t{y} < std::int64_t{is.tiles.y0()} + is.tiles.height() && !alternate; ++y)
        for (std::int32_t x = is.tiles.x0();
             std::int64_t{x} < std::int64_t{is.tiles.x0()} + is.tiles.width(); ++x) {
            const PlacementRecord p{TypeId{1001}, x, y};
            if (footprint_owner_rule(is, w.config(), *handwerk, p, city->city) == Reject::None &&
                placement_rule(is, w.types(), *handwerk, p).reject == Reject::None) {
                if (!placement)
                    placement = PlacementWire{TypeId{1001}, x, y};
                else {
                    alternate = PlacementWire{TypeId{1001}, x, y};
                    break;
                }
            }
        }
    REQUIRE(placement);
    REQUIRE(alternate);
    const Command build{PlayerId{0}, BuildCmd{*city, {*placement}}};
    // Explicitly separate permission-denied fixture. Only this copy changes a
    // permission; the unchanged original canary still has its actual INIT rows.
    auto denied = w;
    auto& permissions = WorldTestAccess::players(denied).slots[0]->permissions;
    const auto permission = std::find_if(permissions.begin(), permissions.end(),
                                         [](const auto& p) { return p.type == TypeId{1001}; });
    REQUIRE(permission != permissions.end());
    REQUIRE(permission->value != 0);
    permission->value = 0;
    const auto denied_save = denied.save();
    CHECK(denied.apply(build).status == Reject::ConstructionDenied);
    CHECK(denied.save() == denied_save);
    const Command changed_build{PlayerId{0}, BuildCmd{*city, {*alternate}}};
    auto legal_changed = w;
    REQUIRE(legal_changed.apply(changed_build).ok());
    const auto stock = is.city(city->city)->stock.stock;
    const auto build_entry = rec.log().entries.size();
    const auto built = rec.submit(build);
    REQUIRE_MESSAGE(built.ok(), reject_name(built.status));
    REQUIRE(built.records.size() == 1);
    CHECK(built.records[0].building.valid());
    const auto* building = w.find_building(built.records[0].building);
    REQUIRE(building);
    CHECK(building->type == TypeId{1001});
    CHECK(building->x == placement->x);
    CHECK(building->y == placement->y);
    CHECK(w.building_at(*city, placement->x, placement->y) == building);
    CHECK(w.players().find(PlayerId{0})->money == 9999350);
    for (std::size_t g = 0; g < stock.size(); ++g) {
        std::uint64_t cost = 0;
        for (const auto& c : handwerk->cost_goods)
            if (c.goods == g)
                cost += c.amount_88;
        CHECK(is.city(city->city)->stock.stock[g] + cost == stock[g]);
    }
    CHECK(w.script().source == input.spec.script_source);
    CHECK(w.check_invariants().empty());
    rec.checkpoint();
    cuts.push_back(rec.log().entries.size());
    (void)w.drain_events();
    rec.advance(0); // next visit must not repeat the completed one-shot effect
    std::vector<SimEvent> next_waves;
    observe(&next_waves, w.drain_events());
    CHECK(next_waves.empty());
    rec.checkpoint();
    const auto final_save = w.save();
    cuts.push_back(rec.log().entries.size());
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    auto continued = World::load(rec.log().initial_save, input.types);
    REQUIRE(continued);
    std::vector<SimEvent> continued_waves;
    std::size_t previous = 0;
    for (const auto cut : cuts) {
        auto segment = rec.log();
        segment.entries = {rec.log().entries.begin() + static_cast<std::ptrdiff_t>(previous),
                           rec.log().entries.begin() + static_cast<std::ptrdiff_t>(cut)};
        const auto replayed = replay(continued.value(), segment, {&continued_waves, observe});
        REQUIRE(replayed);
        REQUIRE(replayed.value().reproduced());
        const auto native = continued.value().save();
        auto reloaded = World::load(native, input.types);
        REQUIRE(reloaded);
        CHECK(reloaded.value().save() == native);
        CHECK(reloaded.value().drain_events().empty());
        continued = std::move(reloaded);
        previous = cut;
    }
    CHECK(continued.value().save() == final_save);
    CHECK(continued_waves == waves);
    auto wrong_command = rec.log();
    REQUIRE(wrong_command.entries[build_entry].commands.size() == 1);
    wrong_command.entries[build_entry].commands[0] = changed_build;
    auto wrong_program = rec.log();
    auto changed = World::load(rec.log().initial_save, input.types);
    REQUIRE(changed);
    auto& runtime = WorldTestAccess::script(changed.value());
    const std::string asset = "HAWAIMUCKE.WAV";
    const auto at = runtime.source.find(asset);
    REQUIRE(at != std::string::npos);
    runtime.source.replace(at, asset.size(), "WRONG.WAV");
    REQUIRE(
        restore_script(runtime, *input.types, changed.value().players(), changed.value().config())
            .empty());
    wrong_program.initial_save =
        changed.value().save(); // valid framing, different program identity
    if (const char* dir = std::getenv("ANNO_SCRIPT2_ORIGINAL_CANARY_DIR"); dir && *dir) {
        const std::array<std::pair<const char*, const ReplayLog*>, 3> logs{
            {{"normal.log", &rec.log()},
             {"wrong-command.log", &wrong_command},
             {"wrong-program.log", &wrong_program}}};
        for (const auto& [name, log] : logs) {
            auto bytes = write_log(*log);
            REQUIRE(bytes);
            write(fs::path(dir) / name, bytes.value());
        }
        const auto expected =
            std::to_string(w.state_hash()) + " " + std::to_string(waves[0].tick) + " " +
            std::to_string(build_entry + 1) + " " + std::to_string(built.records[0].building.v) +
            " " + std::to_string(city->island.v) + " " + std::to_string(city->city.v) + " " +
            std::to_string(placement->x) + " " + std::to_string(placement->y) + "\n";
        write(fs::path(dir) / "expected.txt", {expected.begin(), expected.end()});
    }
    MESSAGE("ORIGINAL_CONSTRUCTION type=1001 site="
            << placement->x << "," << placement->y << " wave_tick=" << waves[0].tick
            << " gold=10000000->9999650->9999350 final_hash=" << w.state_hash());
}
TEST_CASE("SCRIPT2 original encoded canary replay worker") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    const char* dir = std::getenv("ANNO_SCRIPT2_ORIGINAL_CANARY_DIR");
    if (!dir || !*dir) {
        MESSAGE("skipped: set ANNO_SCRIPT2_ORIGINAL_CANARY_DIR=path for the encoded worker");
        return;
    }
    const auto registry =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE(registry.error.empty());
    auto normal = read_log(read(fs::path(dir) / "normal.log"));
    auto wrong = read_log(read(fs::path(dir) / "wrong-command.log"));
    auto program = read_log(read(fs::path(dir) / "wrong-program.log"));
    REQUIRE(normal);
    REQUIRE(wrong);
    REQUIRE(program);
    const auto expected = read(fs::path(dir) / "expected.txt");
    std::istringstream values(std::string(expected.begin(), expected.end()));
    std::uint64_t hash = 0, wave_tick = 0, divergence = 0, building_id = 0;
    std::uint32_t island = 0, city = 0;
    std::int32_t x = 0, y = 0;
    values >> hash >> wave_tick >> divergence >> building_id >> island >> city >> x >> y;
    REQUIRE_FALSE(values.fail());
    values >> std::ws;
    REQUIRE(values.eof());
    REQUIRE(wave_tick == 48000);
    REQUIRE(divergence > 0);
    REQUIRE(divergence < normal.value().entries.size());
    REQUIRE(normal.value().entries[divergence].kind == ReplayEntry::Kind::Checkpoint);
    REQUIRE(normal.value().entries[divergence - 1].kind == ReplayEntry::Kind::Commands);
    REQUIRE(std::holds_alternative<BuildCmd>(
        normal.value().entries[divergence - 1].commands.at(0).body));
    auto w = World::load(normal.value().initial_save, registry.types);
    REQUIRE(w);
    const auto raw = read(*game / "Scenes/0011SingleEndlosAddon/endless01.sze");
    const auto payload = anno::formats::bigbrother::find_payload(raw);
    REQUIRE(payload);
    CHECK(w.value().script().source == std::string(payload.value().begin(), payload.value().end()));
    CHECK(find_figure(w.value().figures(), FigureId{1})->pos == FixedPos{89984, 98176});
    std::vector<SimEvent> waves;
    const auto replayed = replay(w.value(), normal.value(), {&waves, observe});
    REQUIRE(replayed);
    REQUIRE(replayed.value().reproduced());
    CHECK(w.value().state_hash() == hash);
    CHECK(w.value().players().find(PlayerId{0})->money == 9999350);
    CHECK(w.value().script().state.signals == std::vector<std::uint8_t>{0, 0, 1, 1, 1, 1, 1, 1});
    REQUIRE(waves.size() == 1);
    CHECK(waves[0].tick == wave_tick);
    CHECK(waves[0].code == 317);
    REQUIRE(waves[0].wave);
    CHECK(waves[0].wave->asset == "scenes/0011SingleEndlosAddon/HAWAIMUCKE.WAV");
    CHECK(waves[0].wave->audience == WaveAudience::AllLocal);
    CHECK_FALSE(waves[0].player.valid());
    const auto* building = w.value().find_building(BuildingId{building_id});
    REQUIRE(building);
    CHECK(building->type == TypeId{1001});
    CHECK(building->x == x);
    CHECK(building->y == y);
    CHECK(w.value().building_at({IslandId{island}, CityNo{city}}, x, y) == building);
    CHECK(w.value().check_invariants().empty());
    auto changed = World::load(wrong.value().initial_save, registry.types);
    REQUIRE(changed);
    const auto divergent = replay(changed.value(), wrong.value());
    REQUIRE(divergent);
    CHECK(divergent.value().diverged_entry == std::optional<std::uint64_t>{divergence});
    auto different_program = World::load(program.value().initial_save, registry.types);
    REQUIRE(different_program);
    const auto refused = replay(different_program.value(), program.value());
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == "replay refused: script content digest differs from the log's");
    MESSAGE("ORIGINAL_ENCODED_REPLAY final_hash=" << hash << " wave_tick=" << wave_tick
                                                  << " wrong_build_checkpoint=" << divergence);
}
TEST_CASE("SCRIPT2 synthetic complete original program canary producer") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto input = original(*game);
    const auto canary = produce(input);
    verify(canary, input.types);
    if (const char* dir = std::getenv("ANNO_SCRIPT2_CANARY_DIR"); dir && *dir) {
        for (const auto& [name, log] : {std::pair{"normal.log", &canary.normal},
                                        std::pair{"wrong-command.log", &canary.wrong_command},
                                        std::pair{"wrong-program.log", &canary.wrong_program}}) {
            auto bytes = write_log(*log);
            REQUIRE(bytes);
            write(fs::path(dir) / name, bytes.value());
        }
        const auto expected = std::to_string(canary.final_hash) + "\n";
        write(fs::path(dir) / "expected.txt", {expected.begin(), expected.end()});
    }
}
TEST_CASE("SCRIPT2 synthetic encoded canary replay worker") {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto registry =
        anno::sim_load::load_original_registry(*game, anno::sim_load::FigureEntries::Required);
    REQUIRE(registry.error.empty());
    if (const char* dir = std::getenv("ANNO_SCRIPT2_CANARY_DIR"); dir && *dir) {
        auto normal = read_log(read(fs::path(dir) / "normal.log"));
        REQUIRE(normal);
        auto wrong = read_log(read(fs::path(dir) / "wrong-command.log"));
        REQUIRE(wrong);
        auto program = read_log(read(fs::path(dir) / "wrong-program.log"));
        REQUIRE(program);
        const auto expected = read(fs::path(dir) / "expected.txt");
        verify({normal.value(), wrong.value(), program.value(),
                std::stoull(std::string(expected.begin(), expected.end()))},
               registry.types);
    } else {
        auto input = original(*game);
        verify(produce(input), input.types); // in-process only
    }
}
