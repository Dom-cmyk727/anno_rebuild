// ROADS-3 / UI-ROADS-1 unit tests: variant deck determinism + preview/commit parity,
// road_variant_id property 0x32 (HEAD[11]) against synthetic and original Anlagen.dat data,
// plan_road_ghost prefix/blink/refusal logic, and DevInput/DevTool L-drag & cap behaviour.
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/dev_input.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/road_drag_ui.hpp>
#include <anno/sim/extension_context.hpp>
#include <anno/sim/road_drag.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

namespace app = anno::app;
namespace sim = anno::sim;

namespace {

sim::BuildingType make_road_type(sim::TypeId id, std::uint8_t head11 = 0) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = 193; // STRASSE
    t.flags_word = 0x4186;
    t.width = 1;
    t.height = 1;
    t.cost_gold = 5;
    t.head_raw.assign(56, 0);
    t.head_raw[11] = head11;
    t.head_raw[0x2f] = 0;
    t.head_raw[0x30] = 3;
    return t;
}

std::shared_ptr<const sim::TypeRegistry> variant_registry() {
    sim::TypeRegistry::Builder b;
    // Arbitrary non-2520 ids proving data-driven behavior via is_land_road + HEAD[11]:
    // base id 4100, count=4, step=2 -> 0x84 (step=(0x84>>6)=2, count=4).
    const std::uint8_t head11 = static_cast<std::uint8_t>((2U << 6) | 4U);
    b.add(make_road_type(sim::TypeId{4100}, head11));
    b.add(make_road_type(sim::TypeId{4102}, 0));
    b.add(make_road_type(sim::TypeId{4104}, 0));
    b.add(make_road_type(sim::TypeId{4106}, 0));

    sim::BuildingType house;
    house.id = sim::TypeId{9001};
    house.prodtype = 97;
    house.flags_word = 0x0084;
    house.width = 2;
    house.height = 2;
    b.add(house);

    auto res = b.build();
    REQUIRE_MESSAGE(res.ok(), (res.ok() ? std::string() : res.error()));
    return res.value();
}

} // namespace

TEST_CASE("road_variant_id: property 0x32 (HEAD byte 11) count & step on synthetic registry") {
    const auto reg = variant_registry();
    CHECK(app::road_variant_id(*reg, sim::TypeId{4100}, 0).v == 4100);
    CHECK(app::road_variant_id(*reg, sim::TypeId{4100}, 1).v == 4102);
    CHECK(app::road_variant_id(*reg, sim::TypeId{4100}, 2).v == 4104);
    CHECK(app::road_variant_id(*reg, sim::TypeId{4100}, 3).v == 4106);
    CHECK(app::road_variant_id(*reg, sim::TypeId{4100}, 4).v == 4100);
    // Single-variant type falls back cleanly to base id.
    CHECK(app::road_variant_id(*reg, sim::TypeId{4102}, 3).v == 4102);
}

TEST_CASE("road_variant_id: original Anlagen.dat 2520 (5 variants) and 2500 (4 variants)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto reg =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(reg.types != nullptr, reg.error);
    for (std::uint8_t b = 0; b < 10; ++b) {
        CHECK(app::road_variant_id(*reg.types, sim::TypeId{2520}, b).v == 2520U + (b % 5U));
        CHECK(app::road_variant_id(*reg.types, sim::TypeId{2500}, b).v == 2500U + (b % 4U));
    }
}

TEST_CASE("RoadVariantDeck: deterministic per-index caching and post-commit reroll") {
    const auto reg = variant_registry();
    app::RoadVariantDeck deck;
    std::vector<sim::TypeId> first;
    for (std::size_t i = 0; i < 8; ++i) {
        first.push_back(deck.variant_for(*reg, sim::TypeId{4100}, i));
    }
    // Re-evaluating the same drag indices yields the exact same sequence.
    for (std::size_t i = 0; i < 8; ++i) {
        CHECK(deck.variant_for(*reg, sim::TypeId{4100}, i) == first[i]);
    }
    CHECK(deck.generation() == 0);
    deck.reroll();
    CHECK(deck.generation() == 1);
    bool any_diff = false;
    for (std::size_t i = 0; i < 8; ++i) {
        if (deck.variant_for(*reg, sim::TypeId{4100}, i) != first[i]) {
            any_diff = true;
        }
    }
    CHECK(any_diff);
}

TEST_CASE(
    "plan_road_ghost: valid prefix, unaffordable tail blink, refusal box, never radius/fert") {
    using app::GhostState;
    // Pending keeps previous state.
    const auto p0 = app::plan_road_ghost({true, false, 5, std::nullopt, GhostState::Hidden, false});
    CHECK(p0.state == GhostState::Hidden);
    CHECK_FALSE(p0.draw_refusal_box);
    CHECK_FALSE(p0.draw_radius);
    CHECK_FALSE(p0.draw_fertility);

    // Chrome hides immediately even with a valid verdict.
    const auto p_chrome = app::plan_road_ghost(
        {true, true, 5, sim::RoadPathCheck{5, 5, sim::Reject::None}, GhostState::Valid, false});
    CHECK(p_chrome.state == GhostState::Hidden);
    CHECK(p_chrome.draw_count == 0);
    CHECK_FALSE(p_chrome.draw_refusal_box);

    // Refused (valid_prefix == 0) -> Refused + refusal box only, zero sprites.
    const auto p_ref =
        app::plan_road_ghost({true, false, 5, sim::RoadPathCheck{0, 0, sim::Reject::Occupied},
                              GhostState::Valid, false});
    CHECK(p_ref.state == GhostState::Refused);
    CHECK(p_ref.draw_count == 0);
    CHECK(p_ref.draw_refusal_box);
    CHECK_FALSE(p_ref.draw_radius);
    CHECK_FALSE(p_ref.draw_fertility);

    // Partially affordable: phase 0 draws all valid_count; phase 1 draws only affordable_count.
    const sim::RoadPathCheck check{7, 4, sim::Reject::CannotAfford};
    const auto p_u0 = app::plan_road_ghost({true, false, 7, check, GhostState::Hidden, false});
    CHECK(p_u0.state == GhostState::Unaffordable);
    CHECK(p_u0.valid_count == 7);
    CHECK(p_u0.affordable_count == 4);
    CHECK(p_u0.draw_count == 7);
    CHECK_FALSE(p_u0.draw_refusal_box);

    const auto p_u1 = app::plan_road_ghost({true, false, 7, check, GhostState::Unaffordable, true});
    CHECK(p_u1.state == GhostState::Unaffordable);
    CHECK(p_u1.draw_count == 4);
    CHECK_FALSE(p_u1.draw_refusal_box);

    // All valid & affordable: never draws refusal box, radius, or fertility regardless of blink.
    const auto p_ok = app::plan_road_ghost(
        {true, false, 7, sim::RoadPathCheck{7, 7, sim::Reject::None}, GhostState::Hidden, true});
    CHECK(p_ok.state == GhostState::Valid);
    CHECK(p_ok.draw_count == 7);
    CHECK_FALSE(p_ok.draw_refusal_box);
    CHECK_FALSE(p_ok.draw_radius);
    CHECK_FALSE(p_ok.draw_fertility);
}

TEST_CASE("DevInput road L-drag: data-driven, L-path, cap, preview==commit, stays armed") {
    const auto reg = variant_registry();
    const sim::TypeId road_id{4100};
    const sim::TypeId house_id{9001};
    sim::RoadRules rr;
    rr.enabled = 1;
    rr.drag_cap = 10;
    app::DevInput input(reg, {house_id, road_id}, sim::CityRef{sim::IslandId{0}, sim::CityNo{0}},
                        sim::PlayerId{1}, rr);
    CHECK(input.arm(road_id));
    CHECK(input.land_road_selected());
    CHECK(input.road_drag_active());

    // Hover before press gives 1-tile preview.
    const auto single = input.road_preview_placements({10, 20});
    REQUIRE(single.size() == 1);
    CHECK(single[0].x == 10);
    CHECK(single[0].y == 20);

    // Tie drag (|dx| == |dy| == 2): y first, 5 tiles; preview variants match commit variants.
    CHECK(input.handle({app::DevEvent::Kind::Press, {10, 20}}).empty());
    CHECK(input.handle({app::DevEvent::Kind::Move, {12, 22}}).empty());
    const auto preview = input.road_preview_placements({12, 22});
    REQUIRE(preview.size() == 5);
    CHECK(sim::TileXY{preview[0].x, preview[0].y} == sim::TileXY{10, 20});
    CHECK(sim::TileXY{preview[1].x, preview[1].y} == sim::TileXY{10, 21});
    CHECK(sim::TileXY{preview[2].x, preview[2].y} == sim::TileXY{10, 22});
    CHECK(sim::TileXY{preview[3].x, preview[3].y} == sim::TileXY{11, 22});
    CHECK(sim::TileXY{preview[4].x, preview[4].y} == sim::TileXY{12, 22});

    const auto cmds = input.handle({app::DevEvent::Kind::Release, {12, 22}});
    REQUIRE(cmds.size() == 1);
    const auto& build = std::get<sim::BuildCmd>(cmds[0].body);
    REQUIRE(build.placements.size() == preview.size());
    for (std::size_t i = 0; i < preview.size(); ++i) {
        CHECK(build.placements[i] == preview[i]);
    }
    CHECK(input.selected() == road_id); // stays armed
    CHECK(input.variant_generation() == 1);

    // Custom cap = 10 cuts a 21-tile drag to 10 tiles cleanly without local refusal.
    CHECK(input.handle({app::DevEvent::Kind::Press, {10, 20}}).empty());
    const auto capped = input.road_preview_placements({30, 20});
    CHECK(capped.size() == 10);
    const auto cmds_cap = input.handle({app::DevEvent::Kind::Release, {30, 20}});
    REQUIRE(cmds_cap.size() == 1);
    CHECK(std::get<sim::BuildCmd>(cmds_cap[0].body).placements.size() == 10);
}
