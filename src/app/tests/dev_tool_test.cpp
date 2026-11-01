// The developer tool through the play window's own SDL mapping (W4.4 round ee, package B):
// synthetic SDL events, the same dev_tool_step the live window calls, and pointer frames built as
// the viewer builds them, against expectations derived from iso::tile_top (not from the picking
// code). A synthetic scene with one island at world tile (100, 40): no game files, flat ground.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/dev_tool.hpp>
#include <anno/app/sdl_input.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/resample.hpp>
#include <anno/render/scene.hpp>
#include <anno/sim/types.hpp>

namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;

namespace {

constexpr std::int32_t kX0 = 100, kY0 = 40, kSize = 30;
const sim::TypeId kPlain{501}, kRoad{502};

std::shared_ptr<const render::Scene> scene() {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    bytes->push_back(std::vector<std::uint8_t>(64, 0));
    anno::formats::insel::Island island;
    island.head = (*bytes)[0];
    island.number = 0;
    island.x = kX0;
    island.y = kY0;
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
    [[nodiscard]] app::PointerFrame frame() const {
        if (scale == 0) {
            const render::Rect view{static_cast<int>(std::lround(cx - w / 2.0)),
                                    static_cast<int>(std::lround(cy - h / 2.0)), w, h};
            return app::pointer_frame(view, nullptr, tw, th, scene, w, h);
        }
        const auto rv = render::resampled_view(cx, cy, static_cast<std::uint32_t>(w),
                                               static_cast<std::uint32_t>(h), scale,
                                               render::ResampleFilter::Bilinear);
        return app::pointer_frame(rv.view, &rv.resample, tw, th, scene, w, h);
    }
    /// The window point (SDL's coordinates) over the drawn centre of world tile (x, y) on flat
    /// ground: tile_top's top vertex plus half a diamond, placed on the drawable without the
    /// frame's map (independently of pointer_frame): a stepped view's left edge is the viewer's
    /// rounded lround(cx - w / 2); a continuous view's is cx - w / (2 * scale) at the scale
    /// resampled_view settled on (its own definition). Drawable pixel = (scene - edge) * scale,
    /// less half a pixel for the pixel's centre.
    [[nodiscard]] std::pair<float, float> point_at(int x, int y) const {
        const double sx = (x - y) * tw / 2.0, sy = (x + y) * th / 2.0 + th / 2.0;
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
                           (t.x - t.y) * lv.tw / 2.0 - 230.3 * off,
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
    // the integer view origin (the Resample's offset), and the point is 0.02 of a tile inside the
    // tile's -u edge: without the offset the pick moves about 0.06 of a tile back, into x - 1.
    const auto sc = scene();
    const sim::TileXY t{kX0 + 11, kY0 + 13};
    for (const auto& [tw, th, scale] :
         std::vector<std::array<double, 3>>{{42, 21, 1.37}, {20, 10, 2.0}, {10, 5, 3.5}}) {
        CAPTURE(tw);
        const int w = 1600, h = 1000;
        const double u = t.x + 0.02, v = t.y + 0.5;
        const double sx = (u - v) * tw / 2.0, sy = (u + v) * th / 2.0;
        // cx - w / (2 s) = floor(sx) - 300 + 0.9, and likewise in y.
        const double cx = std::floor(sx) - 300.0 + 0.9 + w / (2.0 * scale);
        const double cy = std::floor(sy) - 200.0 + 0.9 + h / (2.0 * scale);
        Window win{sc, cx, cy, scale, tw, th, w, h, 1.0F};
        const double s =
            render::resampled_view(cx, cy, w, h, scale, render::ResampleFilter::Bilinear)
                .resample.scale;
        REQUIRE(s == scale);
        const double edge_x = cx - w / (2.0 * s), edge_y = cy - h / (2.0 * s);
        const std::pair<float, float> at{static_cast<float>((sx - edge_x) * s - 0.5),
                                         static_cast<float>((sy - edge_y) * s - 0.5)};
        app::DevTool tool(setup());
        const auto cmds = feed(tool, win, {key(SDLK_B), button(true, at), button(false, at)});
        REQUIRE(cmds.size() == 1);
        CHECK(placements(cmds[0]) == std::vector<sim::TileXY>{t});
    }
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
        Window win{sc, (mx - my) * 21.0, (mx + my) * 10.5, 0, 42, 21, w, h, 2.0F};
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
    Window win{sc, (mx - my) * 21.0, (mx + my) * 10.5, 0, 42, 21, 1600, 1000, 1.0F};
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
    const Window win{sc, (mx - my) * 21.0, (mx + my) * 10.5, 0, 42, 21, 1600, 1000, 1.0F};
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
