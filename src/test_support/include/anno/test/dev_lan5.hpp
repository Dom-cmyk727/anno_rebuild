// lan5's developer start for the developer tool's tests (W4.4 round ee): the play load, the scene
// as --play loads it (the sim's buildings, main.cpp), the tool's setup and the city's island in
// the scene; a tool hovering a tile through a real pick; a free tile by the sim's own occupancy.
// Header-only: included by test targets that link anno_app_core. Needs ANNO_FIXTURES_DIR.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/dev_tool.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

namespace anno::test {

struct Lan5 {
    app::PlayLoad play;
    std::shared_ptr<const render::Scene> scene;
    app::DevToolSetup setup;
    /// The city's island's index in the scene.
    std::size_t island = 0;
    std::vector<app::SceneBuilding> buildings;
};

/// lan5 loaded as --play loads it; nullopt when ANNO1503_GAME_DIR is unset. `rivers` false turns
/// the river flow off, as the GPU-versus-CPU verifiers do (the flow is asynchronous).
inline std::optional<Lan5> lan5(bool rivers = true) {
    const auto dir = game_dir();
    if (!dir) {
        return std::nullopt;
    }
    const auto path = std::filesystem::path(ANNO_FIXTURES_DIR).parent_path().parent_path() /
                      "data" / "maps" / "lan5.txt";
    app::FixedPlayOptions fixed;
    fixed.stand_in_placement = true; // these suites isolate the app layers from terrain (OQ-052)
    auto play = app::load_play(*dir, path, fixed);
    REQUIRE(play.ok());
    const std::vector<app::SceneBuilding> buildings = app::play_buildings(play.value().world);
    std::vector<bool> classes(app::kPictureAnlageIds, false);
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
    if (!rivers) {
        loaded.value().scene->river_cfg.enabled = false;
    }
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

/// The placement of the city's island at zoom level `group`.
inline const render::Placement& city_placement(const Lan5& l, std::size_t group) {
    const render::Placement* placed = nullptr;
    for (const render::Placement& p : l.scene->layouts[group]) {
        if (p.index == l.island) {
            placed = &p;
        }
    }
    REQUIRE(placed != nullptr);
    return *placed;
}

/// The scene point at group 1 over the drawn centre of world tile `tile` of the city's island
/// (the placement plus the local diamond's centre, lifted by the ground height there).
inline std::pair<int, int> tile_centre(const Lan5& l, sim::TileXY tile) {
    const auto [tw, th] = render::kTile[1];
    const render::Placement& placed = city_placement(l, 1);
    const int lx = tile.x - l.setup.island.x0, ly = tile.y - l.setup.island.y0;
    const double h =
        l.scene->ground_height(static_cast<float>(tile.x) + 0.5F, static_cast<float>(tile.y) + 0.5F)
            .value_or(0.0F);
    const double lift = h * 2.059 * tw / 84.0;
    return {placed.offset.first + (ly - lx) * tw / 2,
            placed.offset.second + (lx + ly) * th / 2 + th / 2 - static_cast<int>(lift)};
}

/// A tool selecting `type` (cycled to, as N would), turned `rotations` times, hovering world tile
/// `tile` at group 1 through a real pick: a frame whose drawable pixel (400, 300) is the tile's
/// drawn centre (the pick then finds the tile itself with the heights).
inline app::DevTool tool_at(const Lan5& l, sim::TypeId type, sim::TileXY tile, int rotations = 0) {
    app::DevTool tool(l.setup);
    static_cast<void>(tool.handle({app::ToolInput::Kind::Toggle, 0, 0}, nullptr));
    for (std::size_t i = 0; i < l.setup.types.size() && tool.input().selected() != type; ++i) {
        static_cast<void>(tool.handle({app::ToolInput::Kind::NextType, 0, 0}, nullptr));
    }
    REQUIRE(tool.input().selected() == type);
    for (int i = 0; i < rotations; ++i) {
        static_cast<void>(tool.handle({app::ToolInput::Kind::Rotate, 0, 0}, nullptr));
    }
    const auto [tw, th] = render::kTile[1];
    const auto [sx, sy] = tile_centre(l, tile);
    const app::PointerFrame frame = app::pointer_frame(render::Rect{sx - 400, sy - 300, 800, 600},
                                                       nullptr, 1, tw, th, l.scene, 800, 600);
    static_cast<void>(tool.handle({app::ToolInput::Kind::Move, 400, 300}, &frame));
    REQUIRE(tool.hover() == std::optional<sim::TileXY>{tile});
    return tool;
}

/// A tile of the city's island whose `size` x `size` square has no building of the city by the
/// sim's own lookup (World::building_at), at least `margin` tiles from the edge. Not land:
/// Scene::ground_height has a value on every tile of the island's rectangle, sea included (lan5's
/// first such tile, (1050, 442), is open sea; the sim's placement rule does not check terrain,
/// OQ-052).
inline std::optional<sim::TileXY> free_tile(const Lan5& l, std::int32_t size = 6,
                                            std::int32_t margin = 2) {
    const auto w = static_cast<std::int32_t>(l.setup.island.w);
    const auto h = static_cast<std::int32_t>(l.setup.island.h);
    for (std::int32_t y = margin; y + size + margin <= h; ++y) {
        for (std::int32_t x = margin; x + size + margin <= w; ++x) {
            const sim::TileXY t{l.setup.island.x0 + x, l.setup.island.y0 + y};
            bool clear = true;
            for (std::int32_t dy = 0; dy < size && clear; ++dy) {
                for (std::int32_t dx = 0; dx < size && clear; ++dx) {
                    clear = l.play.world.building_at(*l.setup.city, t.x + dx, t.y + dy) == nullptr;
                }
            }
            if (clear) {
                return t;
            }
        }
    }
    return std::nullopt;
}

} // namespace anno::test
