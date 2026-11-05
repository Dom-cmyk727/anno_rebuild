#include <algorithm>
#include <filesystem>

#include <doctest/doctest.h>

#include <anno/sim/replay.hpp>
#include <anno/sim/route.hpp>
#include <anno/sim/route_world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
#include <anno/test/game_dir.hpp>
using namespace anno::sim;
namespace {
struct Original {
    std::shared_ptr<const TypeRegistry> types;
    World world;
};
Original original(const std::filesystem::path& game) {
    auto registry =
        anno::sim_load::load_original_registry(game, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.error.empty(), registry.error);
    auto templates = anno::sim_load::scan_island_templates(game);
    auto loaded =
        anno::sim_load::load_scenario_file(game / "Scenes/0011SingleEndlosAddon/endless01.sze",
                                           game, templates, *registry.types, {}, {});
    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
    auto made = World::create(loaded.value().loaded.spec, registry.types);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    return {registry.types, std::move(made).value()};
}
bool contains(const ScriptArea& area, FixedPos pos) {
    return area.x0 <= pos.x / 256 && pos.x / 256 < area.x1 && area.y0 <= pos.y / 256 &&
           pos.y / 256 < area.y1;
}
} // namespace
TEST_CASE("SCRIPT2 unchanged original conditional canary producer") {
    auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("skipped: ANNO1503_GAME_DIR not set");
        return;
    }
    auto f = original(*game);
    auto& w = f.world;
    const auto source = w.script().source;
    REQUIRE(source.size() == 2488);
    const Figure* ship = nullptr;
    for (const auto& slot : w.figures().list.slots)
        if (slot && slot->owner == PlayerId{0} && slot->nav && !slot->parent.valid()) {
            ship = &*slot;
            break;
        }
    REQUIRE(ship);
    const auto ship_id = ship->id;
    MESSAGE("ORIGINAL_SHIP id=" << ship->id.v << " kind=" << ship->kind
                                << " owner=" << ship->owner.v << " xq8=" << ship->pos.x
                                << " yq8=" << ship->pos.y);
    for (const auto& h : ship->holds)
        MESSAGE("ORIGINAL_HOLD goods=" << h.goods << " raw=" << h.amount_88);
    for (const auto& is : w.islands())
        if (is)
            for (const auto& c : is->cities)
                if (c)
                    MESSAGE("ORIGINAL_CITY island=" << is->tiles.x0() << " city=" << c->number.v
                                                    << " owner=" << c->owner.v);
    const auto initial_gold = w.players().find(PlayerId{0})->money;
    auto started = Recorder::start(w);
    REQUIRE(started);
    auto rec = std::move(started).value();
    rec.checkpoint();
    CHECK(w.players().find(PlayerId{0})->permissions.size() == 462);
    CHECK(w.players().find(PlayerId{0})->money == initial_gold);
    CHECK(w.drain_events().empty());
    const auto it = std::find_if(w.script().program->areas.begin(), w.script().program->areas.end(),
                                 [](const auto& a) { return a.name == "NEGER"; });
    REQUIRE(it != w.script().program->areas.end());
    const auto area = *it;
    WorldTileSource terrain(w, ship_route_terrain_options());
    CoastIndex coast(terrain, {});
    std::optional<FixedPos> goal;
    for (auto y = area.y0 + 7; y < area.y1 - 7 && !goal; y += 8)
        for (auto x = area.x0 + 7; x < area.x1 - 7 && !goal; x += 8) {
            if (!terrain.tile_is_water(x, y))
                continue;
            RouteQuery q;
            q.start_x = ship->pos.x / 256;
            q.start_z = ship->pos.y / 256;
            q.goal_x_q8 = x * 256 + 128;
            q.goal_z_q8 = y * 256 + 128;
            auto route = plan_route(coast, q, {}, RouteCostTable::original());
            if ((route.status == RouteStatus::Planned || route.status == RouteStatus::Arrived) &&
                contains(area, {route.end_x * 256, route.end_z * 256}))
                goal = FixedPos{q.goal_x_q8, q.goal_z_q8};
        }
    REQUIRE_MESSAGE(goal, "no public-navigation goal found inside the authored NEGER area");
    MESSAGE("ORIGINAL_GOAL xq8=" << goal->x << " yq8=" << goal->y);
    REQUIRE(rec.submit(Command{PlayerId{0}, MoveCmd{ship_id, goal->x, goal->y}}).ok());
    rec.checkpoint();
    std::uint32_t steps = 0;
    while (!contains(area, find_figure(w.figures(), ship_id)->pos) && steps < 1000) {
        rec.advance(1000);
        ++steps;
        REQUIRE_FALSE(w.faulted());
    }
    REQUIRE(steps < 1000);
    CHECK(w.script().state.signals[1] == 1); // ONETIME, before the next script visit
    auto before = World::load(w.save(), f.types);
    REQUIRE(before);
    rec.advance(0);
    before.value().advance(0);
    CHECK(w.script().state.signals[1] == 0);
    CHECK(w.save() == before.value().save());
    auto events = w.drain_events();
    std::vector<SimEvent> waves;
    for (auto& e : events)
        if (e.kind == SimEventKind::ScriptWave)
            waves.push_back(e);
    REQUIRE(waves.size() == 1);
    CHECK(waves[0].code == 317);
    REQUIRE(waves[0].wave);
    CHECK(waves[0].wave->asset == "scenes/0011SingleEndlosAddon/HAWAIMUCKE.WAV");
    CHECK(waves[0].wave->audience == WaveAudience::AllLocal);
    CHECK(w.players().find(PlayerId{0})->money == initial_gold);
    CHECK(w.script().source == source);
    MESSAGE("ORIGINAL_CONDITION steps=" << steps << " ms=" << w.clock().ms
                                        << " gold=" << initial_gold << " waves=" << waves.size());
}
