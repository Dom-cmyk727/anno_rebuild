// The developer tool's preview and feedback (W4.4 round ee, package C) on lan5's developer start:
// the preview is the scene's own sprite for the type, at the hovered tile; what the preview knows
// comes from published data only; the feedback names the outcome of the tool's latest command.
// Skips when ANNO1503_GAME_DIR is unset.
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/dev_feedback.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;

namespace {

struct Lan5 {
    app::PlayLoad play;
    std::shared_ptr<const render::Scene> scene;
    app::DevToolSetup setup;
    std::size_t island = 0;
    std::vector<app::SceneBuilding> buildings;
};

std::optional<Lan5> lan5() {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return std::nullopt;
    }
    const auto path = std::filesystem::path(ANNO_FIXTURES_DIR).parent_path().parent_path() /
                      "data" / "maps" / "lan5.txt";
    auto play = app::load_play(*dir, path);
    REQUIRE(play.ok());
    // The scene as --play loads it: the sim's buildings, not the templates' (main.cpp).
    const std::vector<app::SceneBuilding> buildings = app::play_buildings(play.value().world);
    std::vector<bool> classes(0x1000, false);
    for (std::size_t id = 0; id < classes.size(); ++id) {
        classes[id] = app::anlage_is_building(play.value().world, static_cast<std::uint16_t>(id));
    }
    app::SceneObjects objects;
    objects.is_building = [&classes](std::uint16_t id) {
        return id < classes.size() && classes[id];
    };
    objects.buildings = buildings;
    auto loaded = app::load_placed_scene(*dir, play.value().name, play.value().placed, &objects);
    REQUIRE(loaded.ok());
    Lan5 l{std::move(play.value()), loaded.value().scene, {}, 0, {}};
    l.setup = app::dev_tool_setup(l.play);
    REQUIRE(l.setup.city.has_value());
    bool found = false;
    for (std::size_t k = 0; k < l.scene->islands.size(); ++k) {
        if (l.scene->islands[k].x == l.setup.island.x0 &&
            l.scene->islands[k].y == l.setup.island.y0) {
            l.island = k;
            found = true;
        }
    }
    REQUIRE(found);
    l.buildings = app::play_buildings(l.play.world);
    return l;
}

/// A tool selecting `type` (cycled to, as N would), hovering world tile `tile` at group 1.
app::DevTool tool_at(const Lan5& l, sim::TypeId type, sim::TileXY tile, int rotations = 0) {
    app::DevTool tool(l.setup);
    static_cast<void>(tool.handle({app::ToolInput::Kind::Toggle, 0, 0}, nullptr));
    for (std::size_t i = 0; i < l.setup.types.size() && tool.input().selected() != type; ++i) {
        static_cast<void>(tool.handle({app::ToolInput::Kind::NextType, 0, 0}, nullptr));
    }
    REQUIRE(tool.input().selected() == type);
    for (int i = 0; i < rotations; ++i) {
        static_cast<void>(tool.handle({app::ToolInput::Kind::Rotate, 0, 0}, nullptr));
    }
    // Hover the tile through a real pick: a frame whose drawable pixel (0, 0) is the tile's
    // drawn centre (the island's placement plus the local diamond's centre, flat-ground
    // approximation: the pick then finds the tile itself with the heights).
    const auto [tw, th] = render::kTile[1];
    const render::Placement* placed = nullptr;
    for (const render::Placement& p : l.scene->layouts[1]) {
        if (p.index == l.island) {
            placed = &p;
        }
    }
    REQUIRE(placed != nullptr);
    const int lx = tile.x - l.setup.island.x0, ly = tile.y - l.setup.island.y0;
    const double h =
        l.scene->ground_height(static_cast<float>(tile.x) + 0.5F, static_cast<float>(tile.y) + 0.5F)
            .value_or(0.0F);
    const double lift = h * 2.059 * tw / 84.0;
    const int sx = placed->offset.first + (lx - ly) * tw / 2;
    const int sy = placed->offset.second + (lx + ly) * th / 2 + th / 2 - static_cast<int>(lift);
    const app::PointerFrame frame = app::pointer_frame(render::Rect{sx - 400, sy - 300, 800, 600},
                                                       nullptr, 1, tw, th, l.scene, 800, 600);
    static_cast<void>(tool.handle({app::ToolInput::Kind::Move, 400, 300}, &frame));
    REQUIRE(tool.hover() == std::optional<sim::TileXY>{tile});
    return tool;
}

} // namespace

TEST_CASE("dev feedback: the preview is the scene's own sprite of the type at the hovered tile, "
          "and another rotation or type is drawn differently" *
          doctest::test_suite("dev_feedback")) {
    auto l = lan5();
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // A building of the developer start on the city's island, as the scene draws it.
    const std::uint32_t number = l->scene->islands[l->island].number;
    const app::SceneBuilding* b = nullptr;
    for (const app::SceneBuilding& s : l->buildings) {
        if (s.island == number && !b) {
            b = &s;
        }
    }
    REQUIRE(b != nullptr);
    const sim::TileXY tile{l->setup.island.x0 + b->x, l->setup.island.y0 + b->y};
    const app::DevTool tool = tool_at(*l, sim::TypeId{b->type}, tile, b->rotation);
    const auto preview = app::make_preview(tool, *l->scene, 1, 0);
    REQUIRE(preview.has_value());
    REQUIRE_FALSE(preview->sprites.empty());
    // The scene's sprites of that building at group 1 (its objects layer, in the island's
    // bounds): the same frames, anchored the same up to the height difference (the preview
    // stands on the ground; the building at the sim's height byte).
    const render::Placement* placed = nullptr;
    for (const render::Placement& p : l->scene->layouts[1]) {
        if (p.index == l->island) {
            placed = &p;
        }
    }
    REQUIRE(placed != nullptr);
    const auto drawn = l->scene->sprites_in(placed->bounds, 1, 0, render::SpriteLayer::Objects,
                                            render::WaterStyle::Original);
    const int raise_b = l->scene->lift.scaled(2 * std::int64_t{b->height}, 2, 1);
    const int raise_p =
        l->scene->lift.scaled(2 * std::int64_t{preview->object.first.height()}, 2, 1);
    std::size_t matched = 0;
    for (const render::SpriteDraw& p : preview->sprites) {
        CHECK(std::get<0>(p.key) == b->type); // the type's own ANLAGEN entry
        for (const render::SpriteDraw& d : drawn) {
            if (d.frame == p.frame && d.anchor.first == p.anchor.first &&
                d.anchor.second - raise_p == p.anchor.second - raise_b) {
                ++matched;
                break;
            }
        }
    }
    CHECK(matched == preview->sprites.size());
    MESSAGE("preview of type " << b->type << ": " << preview->sprites.size()
                               << " sprites, all found in the scene's own drawing");
    // Rotated once more: another footprint (odd turn of a non-square type) or other frames.
    const app::DevTool turned = tool_at(*l, sim::TypeId{b->type}, tile, b->rotation + 1);
    const auto rotated = app::make_preview(turned, *l->scene, 1, 0);
    REQUIRE(rotated.has_value());
    const bool other_frames = rotated->sprites.empty() || preview->sprites.empty() ||
                              rotated->sprites.front().frame != preview->sprites.front().frame;
    CHECK((other_frames || rotated->object.second != preview->object.second));
    // Another type at the same tile: its own entry.
    for (const sim::TypeId t : l->setup.types) {
        if (t.v != b->type) {
            const auto other = app::make_preview(tool_at(*l, t, tile), *l->scene, 1, 0);
            if (other && !other->sprites.empty()) {
                CHECK(std::get<0>(other->sprites.front().key) == t.v);
                break;
            }
        }
    }
    // No preview in the demolish mode, at a derived level, or with the tool off.
    app::DevTool demolish = tool;
    static_cast<void>(demolish.handle({app::ToolInput::Kind::ToggleDemolish, 0, 0}, nullptr));
    CHECK_FALSE(app::make_preview(demolish, *l->scene, 1, 0));
    CHECK_FALSE(app::make_preview(tool, *l->scene, render::kGroups, 0));
}

TEST_CASE("dev feedback: occupancy and costs are refusals only with a published basis, the rest is "
          "unchecked, and pointer moves never rebuild the index" *
          doctest::test_suite("dev_feedback")) {
    auto l = lan5();
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const std::uint32_t number = l->scene->islands[l->island].number;
    const app::SceneBuilding* b = nullptr;
    for (const app::SceneBuilding& s : l->buildings) {
        if (s.island == number && !b) {
            b = &s;
        }
    }
    REQUIRE(b != nullptr);
    app::PlacementKnowledge knows(l->setup);
    CHECK(knows.update(l->buildings, 7, *l->scene));
    // The building's own tile: occupied in the published picture.
    const sim::TileXY taken{l->setup.island.x0 + b->x, l->setup.island.y0 + b->y};
    CHECK(knows.occupied(taken));
    const sim::TypeId type = l->setup.types.front();
    app::PlaySnapshot snap;
    snap.turn = 12;
    snap.money = 10000000;
    snap.goods.assign(64, 100000);
    const auto on_taken = knows.check(tool_at(*l, type, taken), &snap);
    CHECK(on_taken.refused);
    CHECK(on_taken.text.find("occupied in the published picture") != std::string::npos);
    // A free tile of the island: unchecked (terrain and the rule are the sim's).
    std::optional<sim::TileXY> free;
    const app::DevTool probe(l->setup);
    for (std::int32_t y = 2; y < static_cast<std::int32_t>(l->setup.island.h) - 6 && !free; ++y) {
        for (std::int32_t x = 2; x < static_cast<std::int32_t>(l->setup.island.w) - 6 && !free;
             ++x) {
            const sim::TileXY t{l->setup.island.x0 + x, l->setup.island.y0 + y};
            bool clear = true;
            for (std::int32_t dy = 0; dy < 6; ++dy) {
                for (std::int32_t dx = 0; dx < 6; ++dx) {
                    clear = clear && !knows.occupied({t.x + dx, t.y + dy});
                }
            }
            if (clear && l->scene->ground_height(static_cast<float>(t.x) + 0.5F,
                                                 static_cast<float>(t.y) + 0.5F)) {
                free = t;
            }
        }
    }
    REQUIRE(free.has_value());
    const app::DevTool on_free = tool_at(*l, type, *free);
    const auto unchecked = knows.check(on_free, &snap);
    CHECK_FALSE(unchecked.refused);
    CHECK(unchecked.text.find("unchecked") != std::string::npos);
    CHECK(unchecked.text.find("turn 12") != std::string::npos);
    // Too little money: a refusal with the snapshot as its basis; waived costs: unchecked again.
    // A type that costs gold (the first buildable one may cost none).
    std::optional<sim::TypeId> priced;
    for (const sim::TypeId id : l->setup.types) {
        const sim::BuildingType* bt = l->setup.registry->find(id);
        if (bt != nullptr && bt->cost_gold > 0 && !priced) {
            priced = id;
        }
    }
    REQUIRE(priced.has_value());
    const sim::BuildingType* t = l->setup.registry->find(*priced);
    const app::DevTool priced_tool = tool_at(*l, *priced, *free);
    CHECK_FALSE(knows.check(priced_tool, &snap).refused);
    snap.money = static_cast<std::int64_t>(t->cost_gold) - 1;
    const auto poor = knows.check(priced_tool, &snap);
    CHECK(poor.refused);
    CHECK(poor.text.find("gold") != std::string::npos);
    snap.costs_waived = true;
    CHECK_FALSE(knows.check(priced_tool, &snap).refused);
    snap.costs_waived = false;
    snap.money = 10000000;
    // Ten thousand checks over the same published list: the index was built once.
    for (int i = 0; i < 10000; ++i) {
        CHECK_FALSE(knows.update(l->buildings, 7, *l->scene));
        static_cast<void>(knows.check(on_free, &snap));
    }
    CHECK(knows.rebuilds() == 1);
    // A new published list (a building gone): one rebuild, the tile is free.
    std::vector<app::SceneBuilding> fewer = l->buildings;
    fewer.erase(fewer.begin() + (b - l->buildings.data()));
    CHECK(knows.update(fewer, 8, *l->scene));
    CHECK(knows.rebuilds() == 2);
    CHECK_FALSE(knows.occupied(taken));
}

TEST_CASE("dev feedback: the line names the latest command's outcome by its receipt, not an "
          "earlier one's" *
          doctest::test_suite("dev_feedback")) {
    auto l = lan5();
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const app::DevTool tool =
        tool_at(*l, l->setup.types.front(), {l->setup.island.x0 + 5, l->setup.island.y0 + 5});
    app::PlaySnapshot snap;
    snap.money = 1234;
    app::CommandOutcome earlier;
    earlier.id = {app::CommandOrigin::Window, 4};
    earlier.state = app::CommandOutcome::State::Refused;
    earlier.status = sim::Reject::Occupied;
    app::CommandOutcome newer;
    newer.id = {app::CommandOrigin::Window, 5};
    newer.state = app::CommandOutcome::State::Queued;
    const std::string line = app::feedback_line(tool, {}, &snap, newer);
    CHECK(line.find("command #5: Queued") != std::string::npos);
    CHECK(line.find("Occupied") == std::string::npos);
    CHECK(line.find("money 1234") != std::string::npos);
    CHECK(app::feedback_line(tool, {}, &snap, earlier).find("command #4: Refused (Occupied)") !=
          std::string::npos);
    newer.state = app::CommandOutcome::State::Applied;
    newer.records = {sim::Reject::None, sim::Reject::Occupied, sim::Reject::None};
    CHECK(app::feedback_line(tool, {}, &snap, newer).find("Applied (2 of 3 records)") !=
          std::string::npos);
}

TEST_CASE("dev feedback: the developer start's resources are the fixed map's only; an original "
          "scenario keeps its own" *
          doctest::test_suite("dev_feedback")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto l = lan5();
    REQUIRE(l.has_value());
    const sim::Player* dev = l->play.world.players().find(app::developer_player());
    REQUIRE(dev != nullptr);
    CHECK(dev->money == app::FixedPlayOptions{}.gold);
    auto original = app::load_play(*dir, *dir / "Scenes" / "0200Single" / "Single_01.sze");
    REQUIRE(original.ok());
    std::size_t players = 0;
    for (const auto& player : original.value().world.players().slots) {
        if (player) {
            ++players;
            CHECK(player->money != app::FixedPlayOptions{}.gold);
        }
    }
    CHECK(players > 0);
}
