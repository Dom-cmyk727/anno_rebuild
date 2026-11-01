// The developer tool against registry data it was not written for (W4.4 round ee, package D): a
// second definition with another id (wider than an original Anlagen id), a non-square footprint,
// other costs and no sprite in the scene; an empty buildable list; malformed definitions; pointer
// moves that rebuild nothing; and the per-move work as unrelated world content grows. A synthetic
// scene (flat ground, no Anlagen entries) and registry: no game files.
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/dev_feedback.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/types.hpp>

namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;

namespace {

constexpr std::int32_t kX0 = 100, kY0 = 40, kSize = 30;
/// An original-style type, a road, scenery (no list in the layout), a PRODTYPE outside the
/// layout's blocks, and a mod's type: an id no Anlagen entry has and a PROD2 item cannot hold.
const sim::TypeId kHouse{501}, kRoad{502}, kScenery{503}, kOutside{504}, kMod{0x12345};
/// PRODTYPE 161 (a market stall, building_helpers.hpp) has a list in the original layout and
/// does not tick; 193 is outside the layout's blocks (1..192).
constexpr std::uint32_t kListed = 161, kUnlisted = 193;
/// HOLZ (goods 0x2a, a build cost goods of the original configuration).
constexpr sim::GoodsId kWood = 0x2a;

/// `islands` synthetic islands of kSize tiles in a row from world tile (kX0, kY0), island k with
/// number k: the city's island is number 0.
std::shared_ptr<const render::Scene> scene(std::size_t islands = 1) {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    std::vector<anno::formats::insel::Island> list;
    for (std::size_t k = 0; k < islands; ++k) {
        bytes->push_back(std::vector<std::uint8_t>(64, 0));
    }
    for (std::size_t k = 0; k < islands; ++k) {
        anno::formats::insel::Island island;
        island.head = (*bytes)[k];
        island.number = static_cast<std::uint32_t>(k);
        island.x = static_cast<std::uint16_t>(kX0 + static_cast<std::int32_t>(k) * (kSize + 2));
        island.y = static_cast<std::uint16_t>(kY0);
        island.width = kSize;
        island.height = kSize;
        list.push_back(island);
    }
    return render::Scene::create("dev-tool-mod", list, anno::assets::island::GroundTextures{},
                                 anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
}

sim::BuildingType type(sim::TypeId id, std::uint32_t prodtype, std::uint32_t w, std::uint32_t h,
                       std::uint32_t gold = 0, std::uint32_t wood = 0) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = prodtype;
    t.width = w;
    t.height = h;
    t.cost_gold = gold;
    if (wood > 0) {
        t.cost_goods.push_back(sim::CostEntry{kWood, wood});
    }
    return t;
}

std::shared_ptr<const sim::TypeRegistry> registry(std::vector<sim::BuildingType> extra = {}) {
    sim::TypeRegistry::Builder b;
    sim::BuildingType road = type(kRoad, 0, 1, 1);
    road.flags_word = sim::kRoadFlag;
    // Added in another order than their ids: the list is in TypeId order, not insertion order.
    b.add(type(kMod, kListed, 3, 1, 777, 300))
        .add(type(kHouse, kListed, 2, 2, 50))
        .add(road)
        .add(type(kScenery, 0, 1, 1))
        .add(type(kOutside, kUnlisted, 1, 1));
    for (sim::BuildingType& t : extra) {
        b.add(std::move(t));
    }
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

app::DevToolSetup setup(std::vector<sim::TypeId> types,
                        std::shared_ptr<const sim::TypeRegistry> reg = registry()) {
    app::DevToolSetup s;
    s.registry = std::move(reg);
    s.types = std::move(types);
    s.city = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    s.island = app::TileRect{kX0, kY0, static_cast<std::uint32_t>(kSize),
                             static_cast<std::uint32_t>(kSize)};
    s.issuer = sim::PlayerId{1};
    return s;
}

/// A group-1 frame of 1600 x 1000 drawable pixels (the whole 30 x 30 island) whose centre pixel is
/// over world tile (cx, cy) on flat ground, and the drawable pixel over the centre of world tile
/// (x, y) in it.
struct Frame {
    app::PointerFrame frame;
    render::Rect view;
    [[nodiscard]] std::pair<double, double> at(std::int32_t x, std::int32_t y) const {
        const auto [tw, th] = render::kTile[1];
        const double sx = (x - y) * tw / 2.0, sy = (x + y) * th / 2.0 + th / 2.0;
        return {sx - view.x, sy - view.y};
    }
};

Frame frame_over(std::shared_ptr<const render::Scene> s, std::int32_t cx, std::int32_t cy) {
    const auto [tw, th] = render::kTile[1];
    const int sx = (cx - cy) * tw / 2, sy = (cx + cy) * th / 2 + th / 2;
    Frame f;
    f.view = render::Rect{sx - 800, sy - 500, 1600, 1000};
    f.frame = app::pointer_frame(f.view, nullptr, 1, tw, th, std::move(s), 1600, 1000);
    return f;
}

std::vector<sim::Command> click(app::DevTool& tool, const Frame& f, std::int32_t x,
                                std::int32_t y) {
    const auto [px, py] = f.at(x, y);
    auto a = tool.handle({app::ToolInput::Kind::Press, px, py}, &f.frame);
    auto b = tool.handle({app::ToolInput::Kind::Release, px, py}, &f.frame);
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

void move(app::DevTool& tool, const Frame& f, std::int32_t x, std::int32_t y) {
    const auto [px, py] = f.at(x, y);
    static_cast<void>(tool.handle({app::ToolInput::Kind::Move, px, py}, &f.frame));
}

void key(app::DevTool& tool, app::ToolInput::Kind kind) {
    static_cast<void>(tool.handle({kind, 0, 0}, nullptr));
}

} // namespace

TEST_CASE("dev tool data: the buildable list is the registry's list-backed and road types in id "
          "order, of any id width" *
          doctest::test_suite("dev_tool_mod")) {
    const auto reg = registry();
    const std::vector<sim::TypeId> got = app::buildable_types(*reg, sim::ListLayout::original());
    const std::vector<sim::TypeId> want{kHouse, kRoad, kMod};
    CHECK(got == want);
    // A modded layout without the market stall's block: the stall types are no longer buildable,
    // the road still is (data, not code).
    const auto none = app::buildable_types(*reg, sim::ListLayout{});
    CHECK(none == std::vector<sim::TypeId>{kRoad});
}

TEST_CASE("dev tool data: a second definition (another id, a non-square footprint, other costs, no "
          "sprite) is driven by its data alone" *
          doctest::test_suite("dev_tool_mod")) {
    const auto s = scene();
    app::DevTool tool(setup({kHouse, kMod}));
    const Frame f = frame_over(s, kX0 + 15, kY0 + 15);
    key(tool, app::ToolInput::Kind::Toggle);
    key(tool, app::ToolInput::Kind::NextType);
    REQUIRE(tool.input().selected() == std::optional<sim::TypeId>{kMod});
    // Its commands carry its id and the rotation; its footprint turns (3x1, then 1x3).
    auto cmds = click(tool, f, kX0 + 10, kY0 + 10);
    REQUIRE(cmds.size() == 1);
    const auto& build = std::get<sim::BuildCmd>(cmds.front().body);
    REQUIRE(build.placements.size() == 1);
    CHECK(build.placements[0].type == kMod);
    CHECK(build.placements[0].rotation == 0);
    CHECK(tool.input().footprint_at({kX0 + 10, kY0 + 10}, 16).size() == 3);
    CHECK(app::dev_tool_status(tool).find("build 74565 (3x1, 777 gold) rotation 0") !=
          std::string::npos);
    key(tool, app::ToolInput::Kind::Rotate);
    const auto turned = tool.input().footprint_at({kX0 + 10, kY0 + 10}, 16);
    REQUIRE(turned.size() == 3);
    CHECK(turned.back() == sim::TileXY{kX0 + 10, kY0 + 12}); // along y now
    CHECK(app::dev_tool_status(tool).find("(1x3, 777 gold) rotation 1") != std::string::npos);
    cmds = click(tool, f, kX0 + 10, kY0 + 10);
    REQUIRE(cmds.size() == 1);
    CHECK(std::get<sim::BuildCmd>(cmds.front().body).placements[0].rotation == 1);

    // Occupancy by its rotated footprint: a published mod building at local (5, 5), rotation 0,
    // covers local x 5..7 on row 5. The turned mod at (7, 3) covers rows 3..5: occupied.
    app::PlacementKnowledge knows(tool.setup());
    // Before an index and without a snapshot, the text names both as unchecked.
    move(tool, f, kX0 + 8, kY0 + 3);
    const auto blind = knows.check(tool, nullptr);
    CHECK_FALSE(blind.refused);
    CHECK(blind.text.find("occupancy (no published index)") != std::string::npos);
    CHECK(blind.text.find("costs (no snapshot)") != std::string::npos);
    CHECK(blind.text.find("does not check terrain yet, OQ-052") != std::string::npos);
    const std::vector<app::SceneBuilding> published{
        app::SceneBuilding{0, kMod.v, 5, 5, 0, 0},
    };
    CHECK(knows.update(published, 1, *s));
    CHECK(knows.occupied({kX0 + 7, kY0 + 5}));
    CHECK_FALSE(knows.occupied({kX0 + 8, kY0 + 5}));
    move(tool, f, kX0 + 7, kY0 + 3);
    app::PlaySnapshot snap;
    snap.turn = 3;
    snap.money = 100000;
    snap.goods.assign(0x40, 100000);
    auto v = knows.check(tool, &snap);
    CHECK(v.refused);
    CHECK(v.text.find("occupied") != std::string::npos);
    // One column further: free; its costs: gold 777, then its wood (300).
    move(tool, f, kX0 + 8, kY0 + 3);
    CHECK_FALSE(knows.check(tool, &snap).refused);
    snap.money = 500;
    v = knows.check(tool, &snap);
    CHECK(v.refused);
    CHECK(v.text.find("gold 777 needed, 500 held") != std::string::npos);
    snap.money = 1000;
    snap.goods[kWood] = 299;
    v = knows.check(tool, &snap);
    CHECK(v.refused);
    CHECK(v.text.find("300 needed, 299 held") != std::string::npos);
    // At the island's lower edge the turned footprint leaves it.
    move(tool, f, kX0 + 8, kY0 + kSize - 2);
    v = knows.check(tool, &snap);
    CHECK(v.refused);
    CHECK(v.text.find("leaves the city's island") != std::string::npos);

    // No sprite: the id does not fit a PROD2 item, the picture does not draw it; said, not shown.
    move(tool, f, kX0 + 8, kY0 + 3);
    CHECK_FALSE(app::make_preview(tool, *s, 1, 0));
    const std::string note = app::preview_note(tool, *s, 1);
    CHECK(note.find("74565 does not fit a PROD2 item") != std::string::npos);
    const std::string line = app::feedback_line(tool, knows.check(tool, &snap), &snap, {}, note);
    CHECK(line.find(note) != std::string::npos);
    CHECK(line.find("/300") != std::string::npos); // its wood: held / needed
    // The house fits a PROD2 item but this scene's Anlagen has no entry for it.
    key(tool, app::ToolInput::Kind::PrevType);
    CHECK(app::preview_note(tool, *s, 1).find("no entry 501") != std::string::npos);
}

TEST_CASE(
    "dev tool data: an empty buildable list only demolishes, and a malformed definition sends "
    "nothing" *
    doctest::test_suite("dev_tool_mod")) {
    const auto s = scene();
    const Frame f = frame_over(s, kX0 + 15, kY0 + 15);
    {
        app::DevTool tool(setup({}));
        key(tool, app::ToolInput::Kind::Toggle);
        key(tool, app::ToolInput::Kind::NextType); // nothing to cycle
        CHECK(click(tool, f, kX0 + 4, kY0 + 4).empty());
        CHECK(tool.message() == "no buildable type");
        CHECK(app::dev_tool_status(tool).find("no buildable type") != std::string::npos);
        CHECK_FALSE(app::make_preview(tool, *s, 1, 0));
        key(tool, app::ToolInput::Kind::ToggleDemolish);
        const auto cmds = click(tool, f, kX0 + 4, kY0 + 4);
        REQUIRE(cmds.size() == 1);
        CHECK(std::holds_alternative<sim::DemolishCmd>(cmds.front().body));
    }
    {
        // A listed id the registry lacks: nothing is sent, the id is named.
        app::DevTool tool(setup({sim::TypeId{999}}));
        key(tool, app::ToolInput::Kind::Toggle);
        CHECK(click(tool, f, kX0 + 4, kY0 + 4).empty());
        CHECK(tool.message().find("type 999 is not in the registry") != std::string::npos);
        app::PlacementKnowledge knows(tool.setup());
        static_cast<void>(knows.update({}, 1, *s));
        CHECK(knows.check(tool, nullptr).text == "type 999 is not in the registry");
        CHECK(app::dev_tool_status(tool).find("type 999 is not in the registry") !=
              std::string::npos);
    }
    {
        // A zero footprint: the registry builder refuses it (the sim's rule, unchanged).
        sim::TypeRegistry::Builder b;
        b.add(type(sim::TypeId{600}, kListed, 0, 2));
        const auto built = b.build();
        REQUIRE_FALSE(built.ok());
        CHECK(built.error().find("is empty") != std::string::npos);
    }
    {
        // A footprint wider than the island: a known refusal, nothing guessed.
        const sim::TypeId wide{601};
        app::DevTool tool(setup({wide}, registry({type(wide, kListed, 40, 1)})));
        key(tool, app::ToolInput::Kind::Toggle);
        move(tool, f, kX0 + 1, kY0 + 1);
        app::PlacementKnowledge knows(tool.setup());
        static_cast<void>(knows.update({}, 1, *s));
        const auto v = knows.check(tool, nullptr);
        CHECK(v.refused);
        CHECK(v.text.find("leaves the city's island") != std::string::npos);
    }
}

TEST_CASE(
    "dev tool data: pointer moves rebuild nothing, and an unpublished list is not re-indexed" *
    doctest::test_suite("dev_tool_mod")) {
    const auto s = scene();
    app::DevTool tool(setup({kHouse, kMod}));
    const Frame f = frame_over(s, kX0 + 15, kY0 + 15);
    key(tool, app::ToolInput::Kind::Toggle);
    app::PlacementKnowledge knows(tool.setup());
    std::vector<app::SceneBuilding> published{app::SceneBuilding{0, kHouse.v, 3, 3, 0, 0}};
    CHECK(knows.update(published, 5, *s));
    app::PlaySnapshot snap;
    snap.money = 1000;
    std::size_t commands = 0;
    for (int i = 0; i < 100000; ++i) {
        const auto [px, py] = f.at(kX0 + 2 + i % 25, kY0 + 2 + (i / 25) % 25);
        commands += tool.handle({app::ToolInput::Kind::Move, px, py}, &f.frame).size();
        static_cast<void>(knows.check(tool, &snap));
        static_cast<void>(app::make_preview(tool, *s, 1, 0));
        // The window's update after each frame: the same published version.
        CHECK_FALSE(knows.update(published, 5, *s));
    }
    CHECK(commands == 0);
    CHECK(knows.rebuilds() == 1);
    // Commands queued, nothing published yet: the version is the same, so no rebuild.
    static_cast<void>(click(tool, f, kX0 + 10, kY0 + 10));
    CHECK_FALSE(knows.update(published, 5, *s));
    // The next published version: one rebuild.
    published.push_back(app::SceneBuilding{0, kHouse.v, 10, 10, 0, 0});
    CHECK(knows.update(published, 6, *s));
    CHECK(knows.rebuilds() == 2);
}

TEST_CASE("dev tool data: per-move work against growing unrelated content (a measurement)" *
          doctest::test_suite("dev_tool_mod")) {
    // Unrelated content: published buildings on other islands (any number) and islands in the
    // scene. Per move: the pick, DevTool::handle, make_preview, preview_note,
    // PlacementKnowledge::check and feedback_line, as the window does on a tile change.
    struct Row {
        std::size_t islands, buildings;
        double rebuild_ms, move_us, pick_us;
        std::size_t indexed;
    };
    std::vector<Row> rows;
    for (const std::size_t islands : {std::size_t{1}, std::size_t{16}, std::size_t{64}}) {
        const auto s = scene(islands);
        for (const std::size_t n :
             {std::size_t{0}, std::size_t{1000}, std::size_t{10000}, std::size_t{100000}}) {
            app::DevTool tool(setup({kHouse, kMod}));
            const Frame f = frame_over(s, kX0 + 15, kY0 + 15);
            key(tool, app::ToolInput::Kind::Toggle);
            std::vector<app::SceneBuilding> published;
            published.reserve(n + 4);
            // Four on the city's island; the rest on islands numbered 1.. (other islands).
            for (std::uint16_t k = 0; k < 4; ++k) {
                published.push_back(
                    app::SceneBuilding{0, kHouse.v, static_cast<std::uint16_t>(4 * k), 20, 0, 0});
            }
            for (std::size_t i = 0; i < n; ++i) {
                published.push_back(app::SceneBuilding{static_cast<std::uint32_t>(1 + i % 1000),
                                                       kHouse.v, static_cast<std::uint16_t>(i % 28),
                                                       static_cast<std::uint16_t>((i / 28) % 28), 0,
                                                       0});
            }
            app::PlacementKnowledge knows(tool.setup());
            const auto r0 = std::chrono::steady_clock::now();
            static_cast<void>(knows.update(published, 1, *s));
            const auto r1 = std::chrono::steady_clock::now();
            app::PlaySnapshot snap;
            snap.money = 1000;
            constexpr int kMoves = 20000;
            std::size_t sink = 0;
            const auto m0 = std::chrono::steady_clock::now();
            for (int i = 0; i < kMoves; ++i) {
                const auto [px, py] = f.at(kX0 + 2 + i % 25, kY0 + 2 + (i / 25) % 25);
                static_cast<void>(tool.handle({app::ToolInput::Kind::Move, px, py}, &f.frame));
                const auto p = app::make_preview(tool, *s, 1, 0);
                const auto note = app::preview_note(tool, *s, 1);
                const auto v = knows.check(tool, &snap);
                sink += app::feedback_line(tool, v, &snap, {}, note).size() + (p ? 1 : 0);
            }
            const auto m1 = std::chrono::steady_clock::now();
            // The pick alone (DevTool::pick, render::pick_tile), the same pointer path.
            const auto k0 = std::chrono::steady_clock::now();
            for (int i = 0; i < kMoves; ++i) {
                const auto [px, py] = f.at(kX0 + 2 + i % 25, kY0 + 2 + (i / 25) % 25);
                sink += tool.pick(f.frame, px, py) ? 1U : 0U;
            }
            const auto k1 = std::chrono::steady_clock::now();
            CHECK(sink > 0);
            CHECK(knows.rebuilds() == 1);
            std::size_t indexed = 0;
            for (std::int32_t y = 0; y < kSize; ++y) {
                for (std::int32_t x = 0; x < kSize; ++x) {
                    indexed += knows.occupied({kX0 + x, kY0 + y}) ? 1U : 0U;
                }
            }
            CHECK(indexed == 16); // the four city houses (2x2) only
            rows.push_back(
                Row{islands, n, std::chrono::duration<double, std::milli>(r1 - r0).count(),
                    std::chrono::duration<double, std::micro>(m1 - m0).count() / kMoves,
                    std::chrono::duration<double, std::micro>(k1 - k0).count() / kMoves, indexed});
        }
    }
    for (const Row& r : rows) {
        MESSAGE("islands " << r.islands << ", published buildings " << r.buildings + 4
                           << " (unrelated " << r.buildings << "): index rebuild " << r.rebuild_ms
                           << " ms, " << r.move_us << " us per move (the pick alone " << r.pick_us
                           << " us), " << r.indexed << " tiles indexed");
    }
}

TEST_CASE("dev tool data: another player's city on another island: its commands carry that issuer "
          "and city, and the index keys on that island" *
          doctest::test_suite("dev_tool_mod")) {
    // Integration review of round ee (P2-2): nothing assumes player 1, city 0 or island 0.
    const auto s = scene(3); // islands 0, 1, 2 in a row; the city is on island 1
    const std::int32_t x1 = kX0 + kSize + 2;
    app::DevToolSetup st = setup({kHouse});
    st.issuer = sim::PlayerId{3};
    st.city = sim::CityRef{sim::IslandId{7}, sim::CityNo{2}};
    st.island = app::TileRect{x1, kY0, static_cast<std::uint32_t>(kSize),
                              static_cast<std::uint32_t>(kSize)};
    app::DevTool tool(st);
    const Frame f = frame_over(s, x1 + 15, kY0 + 15);
    key(tool, app::ToolInput::Kind::Toggle);
    auto cmds = click(tool, f, x1 + 6, kY0 + 7);
    REQUIRE(cmds.size() == 1);
    CHECK(cmds[0].issuer == sim::PlayerId{3});
    const auto& build = std::get<sim::BuildCmd>(cmds[0].body);
    CHECK(build.city == sim::CityRef{sim::IslandId{7}, sim::CityNo{2}});
    CHECK(build.placements[0].x == x1 + 6);
    CHECK(build.placements[0].y == kY0 + 7);
    // A click on island 0 (another island) is not the city's: nothing is sent.
    CHECK(click(tool, f, kX0 + 20, kY0 + 7).empty());
    key(tool, app::ToolInput::Kind::ToggleDemolish);
    cmds = click(tool, f, x1 + 6, kY0 + 7);
    REQUIRE(cmds.size() == 1);
    CHECK(cmds[0].issuer == sim::PlayerId{3});
    CHECK(std::get<sim::DemolishCmd>(cmds[0].body).city ==
          sim::CityRef{sim::IslandId{7}, sim::CityNo{2}});
    // The occupancy index takes the buildings of the city's island (scene island number 1) and
    // places them at that island's origin; island 0's building at the same local tile is not it.
    app::PlacementKnowledge knows(st);
    CHECK(knows.update({app::SceneBuilding{1, kHouse.v, 3, 4, 0, 0},
                        app::SceneBuilding{0, kHouse.v, 10, 10, 0, 0}},
                       1, *s));
    CHECK(knows.occupied({x1 + 3, kY0 + 4}));
    CHECK(knows.occupied({x1 + 4, kY0 + 5})); // 2x2
    CHECK_FALSE(knows.occupied({x1 + 10, kY0 + 10}));
    CHECK_FALSE(knows.occupied({kX0 + 10, kY0 + 10}));
    // The preview looks for the city's island in the scene by its origin.
    key(tool, app::ToolInput::Kind::ToggleDemolish);
    move(tool, f, x1 + 6, kY0 + 7);
    CHECK(app::preview_note(tool, *s, 1).find("no entry 501") != std::string::npos);
}
