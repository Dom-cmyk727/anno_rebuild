// The developer tool through the play window's own SDL mapping (W4.4 round ee, package B):
// synthetic SDL events, the same dev_tool_step the live window calls, and pointer frames built as
// the viewer builds them, against expectations derived from iso::tile_top (not from the picking
// code). A synthetic scene with one island at world tile (100, 40): no game files, flat ground.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/dev_tool.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/app/sdl_input.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/resample.hpp>
#include <anno/render/scene.hpp>
#include <anno/sim/types.hpp>
#include <anno/test/game_dir.hpp>

namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;

namespace {

constexpr std::int32_t kX0 = 100, kY0 = 40, kSize = 30;
const sim::TypeId kPlain{501}, kRoad{502};

std::shared_ptr<const render::Scene> scene(std::int32_t x0 = kX0, std::int32_t y0 = kY0) {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    bytes->push_back(std::vector<std::uint8_t>(64, 0));
    anno::formats::insel::Island island;
    island.head = (*bytes)[0];
    island.number = 0;
    island.x = static_cast<std::uint16_t>(x0);
    island.y = static_cast<std::uint16_t>(y0);
    island.width = kSize;
    island.height = kSize;
    return render::Scene::create("dev-tool", {island}, anno::assets::island::GroundTextures{},
                                 anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
}

std::shared_ptr<const sim::TypeRegistry> registry() {
    sim::TypeRegistry::Builder b;
    sim::BuildingType plain;
    plain.id = kPlain;
    plain.width = 2;
    plain.height = 3;
    sim::BuildingType road;
    road.id = kRoad;
    road.flags_word = sim::kRoadFlag;
    b.add(plain).add(road);
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

app::DevToolSetup setup() {
    app::DevToolSetup s;
    s.registry = registry();
    s.types = {kPlain, kRoad};
    s.city = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    s.island = app::TileRect{kX0, kY0, static_cast<std::uint32_t>(kSize),
                             static_cast<std::uint32_t>(kSize)};
    s.issuer = sim::PlayerId{1};
    return s;
}

/// A window of `w` x `h` drawable pixels showing zoom level `group`'s grid (tile `tw` x `th`)
/// centred on scene point (cx, cy): stepped (scale 0) as the viewer draws it (the view's origin
/// rounded), else continuous at `scale` (render::resampled_view).
struct Window {
    std::shared_ptr<const render::Scene> scene;
    double cx = 0, cy = 0, scale = 0, tw = 0, th = 0;
    int w = 0, h = 0;
    float density = 1.0F;
    /// The zoom level of a tile width: the built groups, else the first derived level.
    [[nodiscard]] std::size_t group() const {
        return tw == 84 ? 0 : tw == 42 ? 1 : tw == 20 ? 2 : 3;
    }
    [[nodiscard]] app::PointerFrame frame() const {
        if (scale == 0) {
            const render::Rect view{static_cast<int>(std::lround(cx - w / 2.0)),
                                    static_cast<int>(std::lround(cy - h / 2.0)), w, h};
            return app::pointer_frame(view, nullptr, group(), tw, th, scene, w, h);
        }
        const auto rv = render::resampled_view(cx, cy, static_cast<std::uint32_t>(w),
                                               static_cast<std::uint32_t>(h), scale,
                                               render::ResampleFilter::Bilinear);
        return app::pointer_frame(rv.view, &rv.resample, group(), tw, th, scene, w, h);
    }
    /// The window point (SDL's coordinates) over the drawn centre of world tile (x, y) on flat
    /// ground: tile_top's top vertex plus half a diamond, placed on the drawable without the
    /// frame's map (independently of pointer_frame): a stepped view's left edge is the viewer's
    /// rounded lround(cx - w / 2); a continuous view's is cx - w / (2 * scale) at the scale
    /// resampled_view settled on (its own definition). Drawable pixel = (scene - edge) * scale,
    /// less half a pixel for the pixel's centre.
    [[nodiscard]] std::pair<float, float> point_at(int x, int y) const {
        const double sx = (y - x) * tw / 2.0, sy = (x + y) * th / 2.0 + th / 2.0;
        double edge_x = 0, edge_y = 0, s = 1.0;
        if (scale == 0) {
            edge_x = static_cast<double>(std::lround(cx - w / 2.0));
            edge_y = static_cast<double>(std::lround(cy - h / 2.0));
        } else {
            s = render::resampled_view(cx, cy, static_cast<std::uint32_t>(w),
                                       static_cast<std::uint32_t>(h), scale,
                                       render::ResampleFilter::Bilinear)
                    .resample.scale;
            edge_x = cx - w / (2.0 * s);
            edge_y = cy - h / (2.0 * s);
        }
        const double px = (sx - edge_x) * s - 0.5;
        const double py = (sy - edge_y) * s - 0.5;
        return {static_cast<float>(px / density), static_cast<float>(py / density)};
    }
};

SDL_Event key(SDL_Keycode k) {
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    return e;
}

SDL_Event button(bool down, std::pair<float, float> at, std::uint8_t which = SDL_BUTTON_LEFT) {
    SDL_Event e{};
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.button = which;
    e.button.x = at.first;
    e.button.y = at.second;
    return e;
}

SDL_Event motion(std::pair<float, float> at) {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.x = at.first;
    e.motion.y = at.second;
    return e;
}

SDL_Event window_event(SDL_EventType type) {
    SDL_Event e{};
    e.type = type;
    return e;
}

/// Feeds `events` through the window's step; returns every command, in order.
std::vector<sim::Command> feed(app::DevTool& tool, const Window& win,
                               const std::vector<SDL_Event>& events) {
    std::vector<sim::Command> out;
    const app::PointerFrame frame = win.frame();
    for (const SDL_Event& e : events) {
        for (sim::Command& c : app::dev_tool_step(tool, e, win.density, &frame).commands) {
            out.push_back(std::move(c));
        }
    }
    return out;
}

std::vector<sim::TileXY> placements(const sim::Command& c) {
    std::vector<sim::TileXY> out;
    for (const sim::PlacementWire& p : std::get<sim::BuildCmd>(c.body).placements) {
        out.push_back({p.x, p.y});
    }
    return out;
}

} // namespace

TEST_CASE("dev tool: a click over a known tile builds there through the window's SDL mapping, at "
          "every zoom group, a derived level and continuous scales, at pixel densities 1 and 2" *
          doctest::test_suite("dev_tool")) {
    const auto sc = scene();
    struct Level {
        double tw, th, scale;
    };
    const std::vector<Level> levels{
        {84, 42, 0},    {42, 21, 0},    {20, 10, 0},   {10, 5, 0},  // built groups, a derived level
        {42, 21, 1.37}, {42, 21, 0.61}, {20, 10, 2.0}, {10, 5, 3.5} // continuous zoom
    };
    const std::vector<sim::TileXY> targets{
        {kX0 + 3, kY0 + 4}, {kX0 + 17, kY0 + 2}, {kX0 + 29, kY0 + 29}, {kX0, kY0}};
    for (const Level& lv : levels) {
        for (const float density : {1.0F, 2.0F}) {
            for (const sim::TileXY& t : targets) {
                CAPTURE(lv.tw);
                CAPTURE(lv.scale);
                CAPTURE(density);
                CAPTURE(t.x);
                CAPTURE(t.y);
                // The camera off the target by a few hundred drawable pixels (as a pan would
                // leave it), so the target is on screen at every level.
                const double off = 1.0 / std::max(lv.scale, 1.0);
                Window win{sc,
                           (t.y - t.x) * lv.tw / 2.0 - 230.3 * off,
                           (t.x + t.y) * lv.th / 2.0 + 170.7 * off,
                           lv.scale,
                           lv.tw,
                           lv.th,
                           1600,
                           1000,
                           density};
                app::DevTool tool(setup());
                const auto at = win.point_at(t.x, t.y);
                const auto cmds =
                    feed(tool, win, {key(SDLK_B), button(true, at), button(false, at)});
                REQUIRE(cmds.size() == 1);
                const auto& b = std::get<sim::BuildCmd>(cmds[0].body);
                REQUIRE(b.placements.size() == 1);
                CHECK(b.placements[0].x == t.x); // world tiles, as the command file's
                CHECK(b.placements[0].y == t.y);
                CHECK(b.placements[0].type == kPlain);
                CHECK(b.placements[0].rotation == 0);
                CHECK(b.city == *setup().city);
                CHECK(cmds[0].issuer == setup().issuer);
            }
        }
    }
}

TEST_CASE("dev tool: under continuous zoom a point just inside a tile's edge picks that tile: the "
          "view's sub-pixel offset counts" *
          doctest::test_suite("dev_tool")) {
    // The camera is placed so that the output's left and top edges lie 0.9 of a grid pixel past
    // the integer view origin (the Resample's offset). The pointer is the whole drawable pixel
    // nearest the point 0.05 of a tile inside the tile's -u edge (a pointer names a pixel; its
    // rounding moves the point at most about 0.04 of a tile): without the offset the pick moves
    // 0.06 of a tile or more back, into x - 1.
    const auto sc = scene();
    const sim::TileXY t{kX0 + 11, kY0 + 13};
    for (const auto& [tw, th, scale] :
         std::vector<std::array<double, 3>>{{42, 21, 1.37}, {20, 10, 2.0}, {10, 5, 3.5}}) {
        CAPTURE(tw);
        const int w = 1600, h = 1000;
        const double u = t.x + 0.05, v = t.y + 0.5;
        const double sx = (v - u) * tw / 2.0, sy = (u + v) * th / 2.0;
        // cx - w / (2 s) = floor(sx) - 300 + 0.9, and likewise in y.
        const double cx = std::floor(sx) - 300.0 + 0.9 + w / (2.0 * scale);
        const double cy = std::floor(sy) - 200.0 + 0.9 + h / (2.0 * scale);
        Window win{sc, cx, cy, scale, tw, th, w, h, 1.0F};
        const double s =
            render::resampled_view(cx, cy, w, h, scale, render::ResampleFilter::Bilinear)
                .resample.scale;
        REQUIRE(s == scale);
        const double edge_x = cx - w / (2.0 * s), edge_y = cy - h / (2.0 * s);
        const std::pair<float, float> at{static_cast<float>(std::round((sx - edge_x) * s - 0.5)),
                                         static_cast<float>(std::round((sy - edge_y) * s - 0.5))};
        app::DevTool tool(setup());
        const auto cmds = feed(tool, win, {key(SDLK_B), button(true, at), button(false, at)});
        REQUIRE(cmds.size() == 1);
        CHECK(placements(cmds[0]) == std::vector<sim::TileXY>{t});
    }
}

TEST_CASE(
    "dev tool: an island whose origin has an odd x + y is drawn half a pixel high at group 1, "
    "and a point just inside a tile's edge still picks that tile" *
    doctest::test_suite("dev_tool")) {
    // Review of package B (P2-1): the scene places an island at the integer iso::tile_top of its
    // origin, (x + y) * 21 / 2 at group 1, so an odd sum is drawn 0.5 px above the continuous
    // rule. The pointer is a whole pixel whose centre lies 0.25 px below tile T's upper-left
    // edge (v = T.y, from corner (T.x, T.y) to (T.x + 1, T.y)) as the island was drawn: the
    // integer offset plus the continuous local position. The continuous rule alone puts that
    // edge 0.5 px lower, above the pixel: tile T.y - 1.
    const std::int32_t x0 = kX0 + 1, y0 = kY0; // 141: odd
    const auto sc = scene(x0, y0);
    app::DevToolSetup st = setup();
    st.island.x0 = x0;
    const sim::TileXY t{x0 + 9, y0 + 12};
    for (const std::size_t group : {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
        CAPTURE(group);
        const auto [tw, th] = render::kTile[group];
        const auto top = render::tile_top(x0, y0, group);
        // The edge's midpoint as drawn, and a window around it.
        const double lu = t.x - x0 + 0.5, lv = t.y - y0;
        const double xe = top.first + (lv - lu) * tw / 2.0;
        const double ye = top.second + (lu + lv) * th / 2.0;
        Window win{sc,   xe + 100.3, ye - 80.6, 0, static_cast<double>(tw), static_cast<double>(th),
                   1600, 1000,       1.0F};
        const double edge_x = static_cast<double>(std::lround(win.cx - win.w / 2.0));
        const double edge_y = static_cast<double>(std::lround(win.cy - win.h / 2.0));
        // A column near the midpoint whose centre's edge height leaves the next pixel centre
        // below it by less than half a pixel (the edge rises half a pixel per pixel).
        std::optional<std::pair<float, float>> at;
        for (int dx = 0; dx < 2 && !at; ++dx) {
            const double px = std::floor(xe - edge_x) + dx;
            const double xc = edge_x + px + 0.5;
            const double y_edge = ye - (xc - xe) * th / tw;
            const double py = std::floor(y_edge - edge_y - 0.5) + 1.0; // first centre below
            const double d = edge_y + py + 0.5 - y_edge;
            if (d > 0.0 && d < 0.5) {
                at = std::pair{static_cast<float>(px), static_cast<float>(py)};
            }
        }
        REQUIRE(at.has_value());
        app::DevTool tool(st);
        const auto cmds = feed(tool, win, {key(SDLK_B), button(true, *at), button(false, *at)});
        REQUIRE(cmds.size() == 1);
        CHECK(placements(cmds[0]) == std::vector<sim::TileXY>{t});
    }
    // The rounding itself: half a pixel up at group 1, none at groups 0 and 2, and group 2's
    // scaled at a derived level (here there is none to scale).
    CHECK(app::placement_rounding(*sc, 0, 1, 42).second == doctest::Approx(-0.5));
    CHECK(app::placement_rounding(*sc, 0, 0, 84).second == doctest::Approx(0.0));
    CHECK(app::placement_rounding(*sc, 0, 2, 20).second == doctest::Approx(0.0));
}

TEST_CASE(
    "dev tool: on lan5's developer island, with its heights, every sampled ground pixel picks "
    "the tile the renderer drew there" *
    doctest::test_suite("dev_tool")) {
    // Review of package B (P2-2): elevation through the adapter, against the renderer's own record
    // of the tile at each pixel (draw_ground_grids' GroundProbe), at pixels away from tile edges
    // (an edge pixel's centre can lie on either side). The island's placement is the scene's,
    // with its own rounding.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto path = std::filesystem::path(ANNO_FIXTURES_DIR).parent_path().parent_path() /
                      "data" / "maps" / "lan5.txt";
    app::FixedPlayOptions fixed;
    fixed.stand_in_placement = true; // island-relative tiles, not chosen for terrain
    auto play = app::load_play(*dir, path, fixed);
    REQUIRE(play.ok());
    auto loaded = app::load_placed_scene(*dir, play.value().name, play.value().placed);
    REQUIRE(loaded.ok());
    const std::shared_ptr<const render::Scene> sc = loaded.value().scene;
    const app::DevToolSetup st = app::dev_tool_setup(play.value());
    REQUIRE(st.city.has_value());
    std::optional<std::size_t> index;
    for (std::size_t k = 0; k < sc->islands.size(); ++k) {
        if (sc->islands[k].x == st.island.x0 && sc->islands[k].y == st.island.y0) {
            index = k;
        }
    }
    REQUIRE(index.has_value());
    const auto& island = sc->islands[*index];
    const auto gfx = island.gfx_ids();
    const auto heights = island.heights();
    REQUIRE(gfx.ok());
    REQUIRE(heights.ok());
    std::size_t checked = 0, wrong = 0, compared = 0, differ = 0;
    for (const std::size_t group : {std::size_t{1}, std::size_t{2}}) {
        CAPTURE(group);
        const render::Placement* placed = nullptr;
        for (const render::Placement& p : sc->layouts[group]) {
            if (p.index == *index) {
                placed = &p;
            }
        }
        REQUIRE(placed != nullptr);
        const auto [tw, th] = render::kTile[group];
        // A 640 x 400 px window of the island around its middle, in island-local pixels.
        const auto mid = render::tile_top(island.width / 2, island.height / 2, group);
        render::Canvas local;
        local.width = 640;
        local.height = 400;
        local.origin = {mid.first - 320, mid.second - 200};
        local.pixels.assign(local.width * local.height * 4, 0);
        render::GroundProbe probe = render::GroundProbe::for_canvas(local);
        render::draw_ground_grids(local, island.width, island.height, gfx.value(), heights.value(),
                                  sc->textures, group, sc->lift, &probe);
        // The window shows exactly that region: scene pixel offset + local origin is drawable 0.
        const int vx = placed->offset.first + local.origin.first;
        const int vy = placed->offset.second + local.origin.second;
        const app::PointerFrame frame = app::pointer_frame(render::Rect{vx, vy, 640, 400}, nullptr,
                                                           group, tw, th, sc, 640, 400);
        app::DevTool tool(st);
        static_cast<void>(tool.handle({app::ToolInput::Kind::Toggle, 0, 0}, nullptr));
        for (std::size_t py = 0; py < local.height; py += 7) {
            for (std::size_t px = 0; px < local.width; px += 7) {
                const auto [u, v] = probe.pos[py * local.width + px];
                if (std::isnan(u)) {
                    continue;
                }
                const double fu = u - std::floor(u), fv = v - std::floor(v);
                if (fu < 0.06 || fu > 0.94 || fv < 0.06 || fv > 0.94) {
                    continue;
                }
                const sim::TileXY want{st.island.x0 + static_cast<std::int32_t>(std::floor(u)),
                                       st.island.y0 + static_cast<std::int32_t>(std::floor(v))};
                if (!st.island.contains(want)) {
                    continue;
                }
                const std::pair<float, float> at{static_cast<float>(px), static_cast<float>(py)};
                std::vector<sim::Command> cmds;
                for (const SDL_Event& e : {button(true, at), button(false, at)}) {
                    for (sim::Command& c : app::dev_tool_step(tool, e, 1.0F, &frame).commands) {
                        cmds.push_back(std::move(c));
                    }
                }
                ++checked;
                if (cmds.size() != 1 || placements(cmds[0]) != std::vector<sim::TileXY>{want}) {
                    ++wrong;
                    // The pixel, the two tiles, the drawn triangle (A-B-C below the B-C diagonal
                    // in tile space, C-B-D beyond) and the pixel centre's signed distance in px
                    // from the drawn B-C edge (iso.cpp draw_ground_grids).
                    const auto tx = static_cast<std::size_t>(std::floor(u));
                    const auto ty = static_cast<std::size_t>(std::floor(v));
                    const auto c =
                        render::corners(heights.value(), island.width, island.height, tx, ty);
                    const double ppu = static_cast<double>(sc->lift.num) /
                                       static_cast<double>(sc->lift.den) * tw /
                                       static_cast<double>(render::kTile[0].w);
                    auto screen = [&](double cx, double cy, std::int64_t h) {
                        return std::pair{(cy - cx) * tw / 2.0,
                                         (cx + cy) * th / 2.0 - static_cast<double>(h) * ppu};
                    };
                    const auto b =
                        screen(static_cast<double>(tx), static_cast<double>(ty) + 1.0, c[2]);
                    const auto cc =
                        screen(static_cast<double>(tx) + 1.0, static_cast<double>(ty), c[1]);
                    const double qx =
                        static_cast<double>(local.origin.first) + static_cast<double>(px) + 0.5;
                    const double qy =
                        static_cast<double>(local.origin.second) + static_cast<double>(py) + 0.5;
                    const double ex = cc.first - b.first, ey = cc.second - b.second;
                    const double dist =
                        (ex * (qy - b.second) - ey * (qx - b.first)) / std::hypot(ex, ey);
                    const auto picked =
                        tool.pick(frame, static_cast<int>(px), static_cast<int>(py));
                    MESSAGE("wrong pick: group "
                            << group << " pixel (" << px << ", " << py << "), drawn tile ("
                            << want.x << ", " << want.y << ") at (" << fu << ", " << fv
                            << "), triangle " << (fu + fv <= 1.0 ? "A-B-C" : "C-B-D") << ", picked "
                            << (picked
                                    ? std::to_string(picked->x) + ", " + std::to_string(picked->y)
                                    : std::string("none"))
                            << ", " << dist << " px from B-C");
                }
            }
        }
        // The pick asks only the islands its ray crosses (round ee, D6): at every third pixel,
        // edges included, it is the pick over Scene::ground_height (every island).
        const auto rounding = app::placement_rounding(*sc, *index, group, tw);
        for (int py = 0; py < 400; py += 3) {
            for (int px = 0; px < 640; px += 3) {
                const auto point = render::scene_point(frame.map, px, py);
                REQUIRE(point.has_value());
                const auto full = render::pick_tile(
                    point->first - rounding.first, point->second - rounding.second, frame.geometry,
                    [&sc](float u, float v) { return sc->ground_height(u, v); });
                std::optional<sim::TileXY> want;
                if (full && st.island.contains({full->first, full->second})) {
                    want = sim::TileXY{full->first, full->second};
                }
                ++compared;
                differ += tool.pick(frame, px, py) != want ? 1U : 0U;
            }
        }
    }
    MESSAGE(compared << " pixels: the crossed-island pick and the every-island pick differ at "
                     << differ);
    CHECK(differ == 0);
    MESSAGE("lan5 developer island: " << checked << " ground pixels checked, " << wrong
                                      << " picked another tile");
    CHECK(checked > 1000);
    CHECK(wrong == 0);
}

TEST_CASE("dev tool: a resized window maps the same tile with its new frame" *
          doctest::test_suite("dev_tool")) {
    const auto sc = scene();
    app::DevTool tool(setup());
    const sim::TileXY t{kX0 + 9, kY0 + 21};
    const double mx = kX0 + 9, my = kY0 + 21;
    for (const auto& [w, h] :
         std::vector<std::pair<int, int>>{{1920, 1080}, {801, 333}, {3456, 1944}}) {
        CAPTURE(w);
        Window win{sc, (my - mx) * 21.0, (mx + my) * 10.5, 0, 42, 21, w, h, 2.0F};
        const auto at = win.point_at(t.x, t.y);
        if (!tool.enabled()) {
            feed(tool, win, {key(SDLK_B)});
        }
        const auto cmds = feed(tool, win, {button(true, at), button(false, at)});
        REQUIRE(cmds.size() == 1);
        CHECK(placements(cmds[0]) == std::vector<sim::TileXY>{t});
    }
}

TEST_CASE("dev tool: off the city's island, outside the window and non-finite points make no "
          "command and say why" *
          doctest::test_suite("dev_tool")) {
    const auto sc = scene();
    const double mx = kX0 + 15, my = kY0 + 15;
    Window win{sc, (my - mx) * 21.0, (mx + my) * 10.5, 0, 42, 21, 1600, 1000, 1.0F};
    app::DevTool tool(setup());
    feed(tool, win, {key(SDLK_B)});
    // Open sea next to the island, and the tile just outside its far corner.
    for (const sim::TileXY& t :
         {sim::TileXY{kX0 - 1, kY0 + 3}, sim::TileXY{kX0 + kSize, kY0 + kSize}}) {
        const auto at = win.point_at(t.x, t.y);
        CHECK(feed(tool, win, {button(true, at), button(false, at)}).empty());
        CHECK(tool.message().find("not the city's island") != std::string::npos);
    }
    CHECK(feed(tool, win, {button(true, {-3.0F, 10.0F}), button(false, {-3.0F, 10.0F})}).empty());
    CHECK(tool.message().find("outside the window") != std::string::npos);
    CHECK(feed(tool, win, {button(true, {2000.0F, 10.0F})}).empty());
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(feed(tool, win, {button(true, {nan, 10.0F}), button(false, {nan, 10.0F})}).empty());
    // A press on the island and a release off it: nothing, and the press is gone.
    const auto on = win.point_at(kX0 + 5, kY0 + 5);
    const auto off = win.point_at(kX0 - 2, kY0 + 5);
    CHECK(feed(tool, win, {button(true, on), button(false, off)}).empty());
    CHECK(tool.message().find("nothing sent") != std::string::npos);
    CHECK_FALSE(tool.input().pressed());
    CHECK(feed(tool, win, {button(false, on)}).empty()); // a later release places nothing
    // No frame drawn yet: a press and a release do nothing.
    CHECK(tool.handle({app::ToolInput::Kind::Press, 10, 10}, nullptr).empty());
    CHECK(tool.handle({app::ToolInput::Kind::Release, 10, 10}, nullptr).empty());
}

TEST_CASE("dev tool: press, move, cancel, release and focus-loss sequences make exactly the "
          "intended commands, and motion alone makes none" *
          doctest::test_suite("dev_tool")) {
    const auto sc = scene();
    const double mx = kX0 + 15, my = kY0 + 15;
    const Window win{sc, (my - mx) * 21.0, (mx + my) * 10.5, 0, 42, 21, 1600, 1000, 1.0F};
    const auto at = [&win](int dx, int dy) { return win.point_at(kX0 + dx, kY0 + dy); };
    SUBCASE("the tool off: the window keeps its events") {
        app::DevTool tool(setup());
        const app::PointerFrame frame = win.frame();
        const auto step = app::dev_tool_step(tool, button(true, at(3, 3)), 1.0F, &frame);
        CHECK_FALSE(step.taken);
        CHECK(feed(tool, win, {button(true, at(3, 3)), button(false, at(3, 3))}).empty());
        CHECK_FALSE(app::dev_tool_step(tool, key(SDLK_ESCAPE), 1.0F, &frame).taken);
    }
    SUBCASE("a road drag: one command, its tiles in stream order") {
        app::DevTool tool(setup());
        const auto cmds = feed(tool, win,
                               {key(SDLK_B), key(SDLK_N), button(true, at(2, 6)), motion(at(4, 6)),
                                motion(at(7, 7)), button(false, at(8, 6))});
        REQUIRE(cmds.size() == 1);
        CHECK(placements(cmds[0]) == std::vector<sim::TileXY>{{kX0 + 2, kY0 + 6},
                                                              {kX0 + 3, kY0 + 6},
                                                              {kX0 + 4, kY0 + 6},
                                                              {kX0 + 5, kY0 + 6},
                                                              {kX0 + 6, kY0 + 6},
                                                              {kX0 + 7, kY0 + 6},
                                                              {kX0 + 8, kY0 + 6}});
        CHECK(std::get<sim::BuildCmd>(cmds[0].body).placements[0].type == kRoad);
    }
    SUBCASE("a drag of a building that is not a road is refused locally") {
        app::DevTool tool(setup());
        CHECK(feed(tool, win, {key(SDLK_B), button(true, at(2, 6)), button(false, at(8, 6))})
                  .empty());
        CHECK(tool.message().find("not one") != std::string::npos);
    }
    SUBCASE("rotation and demolition") {
        app::DevTool tool(setup());
        auto cmds = feed(
            tool, win, {key(SDLK_B), key(SDLK_T), button(true, at(5, 5)), button(false, at(5, 5))});
        REQUIRE(cmds.size() == 1);
        CHECK(std::get<sim::BuildCmd>(cmds[0].body).placements[0].rotation == 1);
        cmds = feed(tool, win, {key(SDLK_X), button(true, at(5, 5)), button(false, at(6, 5))});
        REQUIRE(cmds.size() == 1);
        const auto& d = std::get<sim::DemolishCmd>(cmds[0].body);
        CHECK(d.tiles == std::vector<sim::TileXY>{{kX0 + 5, kY0 + 5}, {kX0 + 6, kY0 + 5}});
        // Esc leaves the demolish mode; the next click builds again.
        const app::PointerFrame frame = win.frame();
        CHECK(app::dev_tool_step(tool, key(SDLK_ESCAPE), 1.0F, &frame).taken);
        CHECK_FALSE(tool.input().demolish_mode());
        CHECK_FALSE(app::dev_tool_step(tool, key(SDLK_ESCAPE), 1.0F, &frame).taken); // quit's
    }
    SUBCASE("a press dropped by focus loss, leaving the window, Esc or turning the tool off "
            "places nothing later") {
        for (const SDL_Event& drop :
             {window_event(SDL_EVENT_WINDOW_FOCUS_LOST), window_event(SDL_EVENT_WINDOW_MOUSE_LEAVE),
              window_event(SDL_EVENT_WINDOW_HIDDEN), key(SDLK_ESCAPE), key(SDLK_B)}) {
            CAPTURE(drop.type);
            app::DevTool tool(setup());
            feed(tool, win, {key(SDLK_B)});
            CHECK(feed(tool, win, {button(true, at(4, 4)), drop, button(false, at(4, 4))}).empty());
            CHECK_FALSE(tool.input().pressed());
        }
    }
    SUBCASE("motion alone, other buttons and key repeats make no command") {
        app::DevTool tool(setup());
        SDL_Event repeat = key(SDLK_B);
        repeat.key.repeat = true;
        CHECK(feed(tool, win,
                   {key(SDLK_B), motion(at(1, 1)), motion(at(9, 9)), motion(at(20, 3)),
                    button(true, at(4, 4), SDL_BUTTON_RIGHT),
                    button(false, at(4, 4), SDL_BUTTON_RIGHT), repeat})
                  .empty());
        CHECK(tool.enabled()); // the repeat did not turn it off
        CHECK(tool.hover() == std::optional<sim::TileXY>{sim::TileXY{kX0 + 20, kY0 + 3}});
    }
    SUBCASE("a held Esc cancels once and its repeats stay the tool's (never the window's quit)") {
        app::DevTool tool(setup());
        const app::PointerFrame frame = win.frame();
        feed(tool, win, {key(SDLK_B), key(SDLK_X)});
        REQUIRE(tool.input().demolish_mode());
        CHECK(app::dev_tool_step(tool, key(SDLK_ESCAPE), 1.0F, &frame).taken); // the cancel
        CHECK_FALSE(tool.input().demolish_mode());
        CHECK_FALSE(tool.has_pending());
        SDL_Event held = key(SDLK_ESCAPE);
        held.key.repeat = true;
        for (int i = 0; i < 3; ++i) {
            CHECK(app::dev_tool_step(tool, held, 1.0F, &frame).taken); // not the window's
        }
        for (const SDL_Keycode k : {SDLK_N, SDLK_P, SDLK_T, SDLK_X, SDLK_B}) {
            SDL_Event r = key(k);
            r.key.repeat = true;
            const auto before = tool.input().selected();
            CHECK(app::dev_tool_step(tool, r, 1.0F, &frame).taken);
            CHECK(tool.input().selected() == before);
            CHECK_FALSE(tool.input().demolish_mode());
            CHECK(tool.enabled());
        }
        // With the tool off, a held Esc is the window's (only B stays the tool's).
        feed(tool, win, {key(SDLK_B)});
        REQUIRE_FALSE(tool.enabled());
        CHECK_FALSE(app::dev_tool_step(tool, held, 1.0F, &frame).taken);
    }
}

TEST_CASE("dev tool: a drag longer than one record is refused before its path is made, and a "
          "tool without a city stays off" *
          doctest::test_suite("dev_tool")) {
    app::DevInput input(registry(), {kRoad}, sim::CityRef{}, sim::PlayerId{1});
    CHECK(input.handle({app::DevEvent::Kind::Press, {0, 0}}).empty());
    // 2^30 tiles: making the path first would allocate gigabytes.
    CHECK(input.handle({app::DevEvent::Kind::Release, {1 << 30, 0}}).empty());
    CHECK(input.message().find("over the wire limit") != std::string::npos);
    CHECK(app::DevInput::line_length({0, 0}, {1 << 30, -(1 << 30)}) == (1ULL << 30) + 1);
    CHECK(app::DevInput::line_length({-(1 << 30), 0}, {1 << 30, 0}) == (1ULL << 31) + 1);
    // Just over has_wire_form's limit (4 095 placements) but within the byte bound: refused after.
    CHECK(input.handle({app::DevEvent::Kind::Press, {0, 0}}).empty());
    CHECK(input.handle({app::DevEvent::Kind::Release, {4095, 0}}).empty());
    CHECK(input.message().find("4096 tiles is over the wire limit") != std::string::npos);
    CHECK(input.handle({app::DevEvent::Kind::Press, {0, 0}}).empty());
    CHECK(input.handle({app::DevEvent::Kind::Release, {4094, 0}}).size() == 1);

    app::DevToolSetup none = setup();
    none.city.reset();
    app::DevTool tool(none);
    CHECK(tool.handle({app::ToolInput::Kind::Toggle, 0, 0}, nullptr).empty());
    CHECK_FALSE(tool.enabled());
    CHECK(tool.message().find("no city") != std::string::npos);
}
