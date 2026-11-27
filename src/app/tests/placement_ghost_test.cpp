// The placement ghost (placement_ghost.hpp, UI-GHOST-1): the state choice, the blink clock, the
// radius table and both outlines, each against values derived by hand from the GOG code
// (docs/BUILDINGSOBJECTS.md part B section 5.1), with the wrong reading each one rules out.
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/placement_ghost.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/scene.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/types.hpp>

namespace app = anno::app;
namespace sim = anno::sim;

namespace {

app::GhostInputs armed(std::optional<sim::Reject> verdict) {
    app::GhostInputs in;
    in.armed = true;
    in.verdict = verdict;
    return in;
}

} // namespace

TEST_CASE("ghost state: hidden > refused > unaffordable > valid") {
    using S = app::GhostState;
    // The sim's verdict decides: None valid, CannotAfford unaffordable, anything else refused.
    CHECK(app::ghost_state(armed(sim::Reject::None), S::Hidden) == S::Valid);
    CHECK(app::ghost_state(armed(sim::Reject::CannotAfford), S::Hidden) == S::Unaffordable);
    for (const sim::Reject r :
         {sim::Reject::TerrainRefused, sim::Reject::Occupied, sim::Reject::TileNotOwned,
          sim::Reject::OutOfIsland, sim::Reject::Unsupported, sim::Reject::ConstructionDenied}) {
        CAPTURE(sim::reject_name(r));
        CHECK(app::ghost_state(armed(r), S::Valid) == S::Refused);
    }
    // Hidden wins over every verdict: nothing armed, an empty footprint, the pointer on chrome.
    for (const sim::Reject r :
         {sim::Reject::None, sim::Reject::CannotAfford, sim::Reject::TerrainRefused}) {
        app::GhostInputs in = armed(r);
        in.armed = false;
        CHECK(app::ghost_state(in, S::Valid) == S::Hidden);
        in = armed(r);
        in.footprint_empty = true;
        CHECK(app::ghost_state(in, S::Valid) == S::Hidden);
        in = armed(r);
        in.pointer_on_chrome = true;
        CHECK(app::ghost_state(in, S::Valid) == S::Hidden);
    }
    // Pending (no verdict for this record yet): the state shown before stays; hidden stays hidden.
    for (const S s : {S::Hidden, S::Refused, S::Unaffordable, S::Valid}) {
        CHECK(app::ghost_state(armed(std::nullopt), s) == s);
    }
    CHECK(app::ghost_state_name(S::Hidden) == "hidden");
    CHECK(app::ghost_state_name(S::Refused) == "refused");
    CHECK(app::ghost_state_name(S::Unaffordable) == "unaffordable");
    CHECK(app::ghost_state_name(S::Valid) == "valid");
}

TEST_CASE("ghost blink: acc > 699 flips and keeps the surplus (AnnoFrame 0x100038c8)") {
    SUBCASE("zero elapsed time changes nothing") {
        app::GhostBlink b;
        for (int i = 0; i < 10; ++i) {
            CHECK_FALSE(b.advance(0));
        }
        CHECK(b.accumulator() == 0);
        CHECK_FALSE(b.hidden());
        CHECK(b.ms_to_flip() == 700);
    }
    SUBCASE("the smallest step: 1 ms frames") {
        app::GhostBlink b;
        // 699 ms is not above 699 (a >= reading would flip here).
        for (int i = 0; i < 699; ++i) {
            CHECK_FALSE(b.advance(1));
        }
        CHECK(b.accumulator() == 699);
        CHECK_FALSE(b.hidden());
        CHECK(b.ms_to_flip() == 1);
        // The 700th: 700 > 699, subtract 699 (not reset to 0): 1 left, phase 1.
        CHECK(b.advance(1));
        CHECK(b.hidden());
        CHECK(b.accumulator() == 1);
        // From then on a phase is 699 ms: 1 + 698 = 699 holds, the next millisecond flips.
        for (int i = 0; i < 698; ++i) {
            CHECK_FALSE(b.advance(1));
        }
        CHECK(b.advance(1));
        CHECK_FALSE(b.hidden());
        CHECK(b.accumulator() == 1);
        CHECK(b.flips() == 2);
    }
    SUBCASE("a partitioned delay equals one step") {
        app::GhostBlink a, b;
        CHECK_FALSE(a.advance(350));
        CHECK(a.advance(350));
        CHECK(b.advance(700));
        CHECK(a.accumulator() == b.accumulator());
        CHECK(a.hidden() == b.hidden());
        CHECK(a.accumulator() == 1);
    }
    SUBCASE("a surplus beyond two phases flips once per frame until spent") {
        app::GhostBlink b;
        CHECK(b.advance(2000)); // 2000 - 699 = 1301
        CHECK(b.accumulator() == 1301);
        CHECK(b.advance(0)); // 1301 - 699 = 602
        CHECK(b.accumulator() == 602);
        CHECK_FALSE(b.advance(0));
        CHECK(b.flips() == 2);
        CHECK_FALSE(b.hidden());
        CHECK(b.ms_to_flip() == 98);
    }
}

TEST_CASE("radius half-widths (AnnoFrame 0x100443d1-0x10044439), derived by hand") {
    // r = 0: the loop writes w[0] = 0 (the unsigned compare skips), the r <= 3 rule adds 1.
    CHECK(app::radius_half_widths(0) == std::vector<std::uint32_t>{1});
    // r = 1: w[0] = 1, w[1] = 0, then w[1] + 1.
    CHECK(app::radius_half_widths(1) == std::vector<std::uint32_t>{1, 1});
    // r = 4 (esi = 16, edx = 12, s = 8): esi 15 > 12: w0 = 4; esi 12, not > 12: w1 = 4, then
    // s = 6, w4 = 1, eax = 3, edx = 6; esi 7 > 6: w2 = 3; esi 0, not > 6: w3 = 3, then w3 = 3,
    // eax = 2, and the loop ends (4 > 2). No r <= 3 rule.
    CHECK(app::radius_half_widths(4) == std::vector<std::uint32_t>{4, 4, 3, 3, 1});
    CHECK(app::radius_half_widths(24) ==
          std::vector<std::uint32_t>{24, 24, 24, 24, 24, 23, 23, 23, 23, 22, 22, 21, 21,
                                     20, 19, 19, 18, 17, 16, 15, 13, 12, 10, 8,  4});
}

TEST_CASE("footprint outline: one edge per tile side along the corners") {
    const auto e = app::footprint_outline({10, 20, 2, 3});
    CHECK(e.size() == 2U * 2U + 3U * 2U);
    const std::vector<app::CornerEdge> expected{
        {10, 20, 11, 20}, {10, 23, 11, 23}, {11, 20, 12, 20}, {11, 23, 12, 23}, {10, 20, 10, 21},
        {12, 20, 12, 21}, {10, 21, 10, 22}, {12, 21, 12, 22}, {10, 22, 10, 23}, {12, 22, 12, 23}};
    CHECK(e == expected);
    CHECK(app::footprint_outline({10, 20, 0, 3}).empty());
}

TEST_CASE("radius outline: the disc's boundary around the footprint centre") {
    // r = 0, a 1 x 1 footprint at (10, 10): w[0] = 1, so the row 10 spans tiles 9..11: a 3 x 1
    // strip, 3 + 3 horizontal and 1 + 1 vertical edges.
    const auto e = app::radius_outline({10, 10, 1, 1}, 0);
    CHECK(e.size() == 8U);
    CHECK(std::find(e.begin(), e.end(), app::CornerEdge{9, 10, 9, 11}) != e.end());
    CHECK(std::find(e.begin(), e.end(), app::CornerEdge{12, 10, 12, 11}) != e.end());
    CHECK(std::find(e.begin(), e.end(), app::CornerEdge{9, 10, 10, 10}) != e.end());
    // The centre of a 4 x 4 footprint is x0 + 1, y0 + 1 ((w - 1) / 2): r = 1 gives rows 0, 1, 2
    // relative to y0 with half-width 1, a 3 x 3 square from (x0, y0): 12 edges.
    const auto sq = app::radius_outline({100, 200, 4, 4}, 1);
    CHECK(sq.size() == 12U);
    CHECK(std::find(sq.begin(), sq.end(), app::CornerEdge{100, 200, 101, 200}) != sq.end());
    CHECK(std::find(sq.begin(), sq.end(), app::CornerEdge{103, 202, 103, 203}) != sq.end());
    // Clipped at the map's near edge: nothing with a negative corner.
    for (const auto& c : app::radius_outline({0, 0, 1, 1}, 4)) {
        CHECK(c.x0 >= 0);
        CHECK(c.y0 >= 0);
    }
    // A closed outline: every corner is shared by an even number of edges.
    std::map<std::pair<std::int64_t, std::int64_t>, int> degree;
    for (const auto& c : app::radius_outline({500, 500, 3, 3}, 24)) {
        ++degree[{c.x0, c.y0}];
        ++degree[{c.x1, c.y1}];
    }
    REQUIRE_FALSE(degree.empty());
    for (const auto& [corner, d] : degree) {
        CHECK(d % 2 == 0);
    }
}

TEST_CASE("outline pixels: exact colour, one pixel wide, under the chrome") {
    app::OutlineProjection p;
    p.geometry.tile_w = 4.0;
    p.geometry.tile_h = 2.0;
    p.geometry.lift = 1.0;
    p.map.x0 = -20.0;
    p.map.y0 = -10.0;
    p.map.scale = 1.0;
    p.height = [](double, double) { return 0.0; };
    // Corner (0, 0) at height 0.08: scene (0, -0.08) -> pixel (20, 9); corner (1, 0): scene
    // (-2, 0.92) -> pixel (18, 10). Bresenham: (20, 9), (19, 9 or 10), (18, 10).
    const auto a = app::project_corner(p, 0.0, 0.0);
    CHECK(a.first == doctest::Approx(20.0));
    CHECK(a.second == doctest::Approx(9.92));
    const std::vector<app::CornerEdge> edge{{0, 0, 1, 0}};
    std::vector<std::uint32_t> frame(40U * 30U, 0x123456U);
    CHECK(app::draw_outline_xrgb(edge, p, frame.data(), 40, 30, app::kGhostOutlineRgb) == 3U);
    CHECK(frame[9U * 40U + 20U] == 0xFFFF00U);
    CHECK(frame[10U * 40U + 18U] == 0xFFFF00U);
    std::size_t yellow = 0;
    for (const std::uint32_t px : frame) {
        yellow += px == 0xFFFF00U ? 1U : 0U;
    }
    CHECK(yellow == 3U);
    // Into an RGBA layer: opaque yellow where it is transparent; a chrome pixel stays.
    anno::assets::Rgba layer;
    layer.width = 40;
    layer.height = 30;
    layer.pixels.assign(40U * 30U * 4U, 0);
    std::uint8_t* chrome = &layer.pixels[(9U * 40U + 20U) * 4U];
    chrome[0] = 200;
    chrome[3] = 255;
    CHECK(app::draw_outline_rgba_under(edge, p, layer, app::kGhostOutlineRgb) == 2U);
    CHECK(chrome[0] == 200);
    const std::uint8_t* end = &layer.pixels[(10U * 40U + 18U) * 4U];
    CHECK(end[0] == 255);
    CHECK(end[1] == 255);
    CHECK(end[2] == 0);
    CHECK(end[3] == 255);
    // Off the frame: clipped, nothing written, no fault.
    p.map.x0 = 1000.0;
    CHECK(app::draw_outline_xrgb(edge, p, frame.data(), 40, 30, app::kGhostOutlineRgb) == 0U);
}

namespace {

anno::formats::chunk::Chunk make_head_chunk(std::uint8_t w, std::uint8_t h, std::uint16_t flags,
                                            std::vector<std::uint8_t>& buf) {
    buf.assign(0x1eU, 0);
    buf[0] = w;
    buf[1] = h;
    buf[0x1c] = static_cast<std::uint8_t>(flags & 0xFFU);
    buf[0x1d] = static_cast<std::uint8_t>(flags >> 8U);
    anno::formats::chunk::Chunk c;
    c.name_field = {'H', 'E', 'A', 'D', 0, 0, 0, 0, 0, 0, 0, 0};
    c.payload = buf;
    return c;
}

anno::formats::figuren::Entry make_anlagen_entry(std::uint32_t id, std::vector<std::uint8_t>& hbuf,
                                                 std::uint8_t w, std::uint8_t h,
                                                 std::uint16_t flags) {
    anno::formats::figuren::Entry e;
    e.id = id;
    e.records.push_back(make_head_chunk(w, h, flags, hbuf));
    return e;
}

std::vector<std::uint8_t> make_gelaende_payload(
    std::uint16_t bx, std::uint16_t by, std::uint8_t bw, std::uint8_t bh,
    std::initializer_list<std::tuple<std::uint16_t, std::uint8_t, std::uint8_t>> items) {
    std::vector<std::uint8_t> out(6U + items.size() * 6U, 0);
    out[0] = static_cast<std::uint8_t>(bx & 0xFFU);
    out[1] = static_cast<std::uint8_t>(bx >> 8U);
    out[2] = static_cast<std::uint8_t>(by & 0xFFU);
    out[3] = static_cast<std::uint8_t>(by >> 8U);
    out[4] = bw;
    out[5] = bh;
    std::size_t off = 6;
    for (const auto& [id, x, y] : items) {
        out[off] = static_cast<std::uint8_t>(id & 0xFFU);
        out[off + 1] = static_cast<std::uint8_t>(id >> 8U);
        out[off + 4] = static_cast<std::uint8_t>(x - bx);
        out[off + 5] = static_cast<std::uint8_t>(y - by);
        off += 6;
    }
    return out;
}

} // namespace

TEST_CASE("ghost suppression: hides flag-0x20 trees, preserves flag-0x10 rock under flag-0x10 "
          "mine, and restores on cancel") {
    constexpr std::uint16_t kTree = 70;
    constexpr std::uint16_t kRock = 150;
    constexpr std::uint16_t kHouse = 501;
    constexpr std::uint16_t kMine = 601;

    auto backing = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    std::vector<std::uint8_t> h_tree, h_rock, h_house, h_mine;
    anno::formats::anlagen::AnlagenFile anlagen;
    anlagen.entries.push_back(make_anlagen_entry(kTree, h_tree, 1, 1, 0x0029));
    anlagen.entries.push_back(make_anlagen_entry(kRock, h_rock, 1, 1, 0x8018));
    anlagen.entries.push_back(make_anlagen_entry(kHouse, h_house, 2, 2, 0x0000));
    anlagen.entries.push_back(make_anlagen_entry(kMine, h_mine, 2, 2, 0x0010));

    backing->push_back(std::vector<std::uint8_t>(64, 0));
    backing->push_back(
        make_gelaende_payload(0, 0, 20, 20, {{kTree, 4, 4}, {kRock, 5, 4}, {kTree, 7, 7}}));
    anno::formats::insel::Island island;
    island.head = (*backing)[0];
    island.number = 1;
    island.x = 1000;
    island.y = 400;
    island.width = 20;
    island.height = 20;
    anno::formats::chunk::Chunk gel;
    gel.name_field = {'G', 'E', 'L', 'A', 'E', 'N', 'D', 'E', 0, 0, 0, 0};
    gel.payload = (*backing)[1];
    island.records.push_back(gel);

    auto scene = anno::render::Scene::create(
        "ghost2-suppress", {island}, anno::assets::island::GroundTextures{}, std::move(anlagen),
        anno::render::kHeightLift, backing);
    REQUIRE(scene->island_lists(0).other.size() == 3);

    // Non-0x10 house at (4, 4) hides the covered tree at (4, 4), leaves outside tree (7, 7), and
    // leaves non-0x20 rock at (5, 4).
    const auto house_cover = app::cover_of(scene->anlagen, kHouse, 4, 4, 0);
    REQUIRE(house_cover);
    const std::vector<app::Cover> house_covers{*house_cover};
    const auto suppressed_house = app::collect_suppressed_object_indices(*scene, 0, house_covers);
    REQUIRE(suppressed_house.size() == 1);
    CHECK(scene->island_lists(0).other[suppressed_house[0]].first.anlage == kTree);
    CHECK(scene->island_lists(0).other[suppressed_house[0]].first.x == 4);

    // Flag-0x10 mine covering (4, 4)..(5, 5): tree at (4, 4) hides, flag-0x10 rock at (5, 4) stays!
    const auto mine_cover = app::cover_of(scene->anlagen, kMine, 4, 4, 0);
    REQUIRE(mine_cover);
    CHECK(mine_cover->overlay);
    const std::vector<app::Cover> mine_covers{*mine_cover};
    const auto suppressed_mine = app::collect_suppressed_object_indices(*scene, 0, mine_covers);
    REQUIRE(suppressed_mine.size() == 1);
    CHECK(scene->island_lists(0).other[suppressed_mine[0]].first.anlage == kTree);

    // Negative island-relative origin clips cleanly without uint16 underflow.
    const auto edge_cover = app::cover_of(scene->anlagen, kHouse, -1, -1, 0);
    REQUIRE(edge_cover);
    CHECK(app::collect_suppressed_object_indices(*scene, 0,
                                                 std::span<const app::Cover>(&*edge_cover, 1))
              .empty());

    // Cancel / empty covers: nothing suppressed (objects immediately restored).
    CHECK(app::collect_suppressed_object_indices(*scene, 0, {}).empty());
}

TEST_CASE("occupied blocking outline (ADAPTATION): outlines blocking building footprint when "
          "refused Occupied") {
    sim::TypeRegistry::Builder b;
    sim::BuildingType kontor;
    kontor.id = sim::TypeId{1302};
    kontor.width = 4;
    kontor.height = 4;
    b.add(kontor);
    sim::BuildingType hall;
    hall.id = sim::TypeId{777};
    hall.width = 3;
    hall.height = 2;
    b.add(hall);
    auto reg = b.build();
    REQUIRE(reg.ok());

    const std::vector<app::SceneBuilding> buildings{
        {1, 1302, 10, 10, 0, 16}, // world (1010..1013, 410..413)
        {1, 777, 30, 30, 1, 16},  // odd rotation -> 2x3 at world (1030..1031, 430..432)
    };
    // Target 3x3 at (1008..1010, 408..410) clips corner (1010, 410) of 4x4 Kontor.
    app::BlockingQuery q;
    q.target = {1008, 408, 3, 3};
    q.island_x0 = 1000;
    q.island_y0 = 400;
    q.buildings = buildings;
    q.registry = reg.value().get();

    const auto boxes = app::occupied_blocking_boxes(q);
    REQUIRE(boxes.size() == 1);
    CHECK(boxes[0].x0 == 1010);
    CHECK(boxes[0].y0 == 410);
    CHECK(boxes[0].w == 4);
    CHECK(boxes[0].h == 4);
    const auto edges = app::blocking_footprint_outline(boxes);
    CHECK(edges.size() == 16); // 4x4 perimeter

    // Overlap with rotated non-square building swaps w and h (2 wide x 3 high).
    q.target = {1031, 432, 1, 1};
    const auto odd_boxes = app::occupied_blocking_boxes(q);
    REQUIRE(odd_boxes.size() == 1);
    CHECK(odd_boxes[0].w == 2);
    CHECK(odd_boxes[0].h == 3);

    // Clear target overlaps nothing.
    q.target = {1020, 420, 2, 2};
    CHECK(app::occupied_blocking_boxes(q).empty());
}
