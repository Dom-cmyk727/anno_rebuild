// The developer tool's preview and feedback (W4.4 round ee, package C) on lan5's developer start:
// the preview is the scene's own sprite for the type, at the hovered tile; what the preview knows
// comes from published data only; the feedback names the outcome of the tool's latest command.
// Skips when ANNO1503_GAME_DIR is unset.
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/dev_feedback.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/render/scene.hpp>
#include <anno/sim/building.hpp>
#include <anno/test/dev_lan5.hpp>
#include <anno/test/game_dir.hpp>

namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;

namespace {

using anno::test::free_tile;
using anno::test::lan5;
using anno::test::tool_at;

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
    // The tool's rotation reaches the sprite (round ee review of C, P3): at each rotation the
    // preview is Scene::preview_sprites at that rotation, and at least two rotations of the type
    // draw other frames (the scene's own drawing of every rotation is the next case's, on an
    // original scenario with many buildings).
    using FramePtr = const anno::formats::figuren::Frame*;
    std::set<std::vector<FramePtr>> distinct;
    for (int r = 0; r < 4; ++r) {
        const auto pv =
            app::make_preview(tool_at(*l, sim::TypeId{b->type}, tile, r), *l->scene, 1, 0);
        REQUIRE(pv.has_value());
        const auto direct = l->scene->preview_sprites(
            l->island, static_cast<std::uint16_t>(b->type), static_cast<std::uint8_t>(r),
            static_cast<std::uint16_t>(b->x), static_cast<std::uint16_t>(b->y),
            pv->object.first.height(), 1, 0);
        std::vector<FramePtr> frames, want;
        for (const render::SpriteDraw& d : pv->sprites) {
            frames.push_back(d.frame);
        }
        for (const render::SpriteDraw& d : direct) {
            want.push_back(d.frame);
        }
        CHECK(frames == want);
        if (!frames.empty()) {
            distinct.insert(frames);
        }
    }
    CHECK(distinct.size() >= 2);
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

TEST_CASE("dev feedback: on an original scenario, the preview sprites of placed objects' types and "
          "rotations are the scene's own drawing of them" *
          doctest::test_suite("dev_feedback")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto file = *dir / "Scenes" / "0200Single" / "Single_01.sze";
    app::FixedPlayOptions fixed;
    fixed.stand_in_placement = true; // island-relative tiles, not chosen for terrain
    auto play = app::load_play(*dir, file, fixed);
    REQUIRE(play);
    // Loaded as --view loads it (no SceneObjects): the scenario's own placed objects (a start
    // without buildings; its trees, rocks and scenery have rotations too, and preview_sprites
    // treats every Anlagen entry alike).
    auto loaded = app::load_scene(*dir, file, play.value().links, nullptr);
    REQUIRE(loaded);
    const render::Scene& scene = *loaded.value().scene;
    using FramePtr = const anno::formats::figuren::Frame*;
    std::map<std::uint16_t, std::map<std::uint8_t, std::set<FramePtr>>> by_type;
    std::size_t checked = 0, sprites = 0, skipped = 0;
    for (std::size_t k = 0; k < scene.islands.size(); ++k) {
        const render::Placement* placed = nullptr;
        for (const render::Placement& p : scene.layouts[1]) {
            if (p.index == k) {
                placed = &p;
            }
        }
        if (placed == nullptr) {
            continue;
        }
        const auto drawn = scene.sprites_in(placed->bounds, 1, 0, render::SpriteLayer::Objects,
                                            render::WaterStyle::Original);
        std::set<std::tuple<FramePtr, int, int>> drawn_at;
        for (const render::SpriteDraw& d : drawn) {
            drawn_at.insert({d.frame, d.anchor.first, d.anchor.second});
        }
        // The island's objects as the scene holds them (its own lists; the first 2 000 of each
        // island): each one's type, rotation, tile and height are what the preview is made from.
        const auto& others = scene.island_lists(k).other;
        for (std::size_t i = 0; i < others.size() && i < 2000; ++i) {
            const auto& placed_object = others[i].first;
            if ((placed_object.state & 0xFFU) != 0) {
                // The state's low byte (a variant or phase: with it, some of Single_01's
                // objects draw another frame) is not the preview's input: a placement starts at 0
                // (Scene::preview_sprites takes the height only).
                ++skipped;
                continue;
            }
            const auto pv = scene.preview_sprites(k, placed_object.anlage, placed_object.rotation,
                                                  placed_object.x, placed_object.y,
                                                  placed_object.height(), 1, 0);
            for (const render::SpriteDraw& d : pv) {
                CHECK(drawn_at.contains({d.frame, d.anchor.first, d.anchor.second}));
                by_type[placed_object.anlage][placed_object.rotation].insert(d.frame);
                ++sprites;
            }
            ++checked;
        }
    }
    // Rotations of one type that draw other frames: a preview ignoring the rotation fails above.
    std::size_t turned = 0;
    for (const auto& [type, rotations] : by_type) {
        for (auto i = rotations.begin(); i != rotations.end(); ++i) {
            for (auto j = std::next(i); j != rotations.end(); ++j) {
                turned += i->second != j->second ? 1U : 0U;
            }
        }
    }
    CHECK(checked > 100);
    CHECK(turned > 0);
    MESSAGE("Single_01: " << checked << " objects, " << sprites << " preview sprites, all at the "
                          << "scene's own frame and anchor (" << skipped
                          << " with a state low byte left out); " << turned
                          << " rotation pairs of one type draw other frames");
}

TEST_CASE("dev feedback: every buildable type of lan5 at every rotation has the registry's "
          "footprint in its Anlagen entry, and a preview or a reason for none" *
          doctest::test_suite("dev_feedback")) {
    // Round ee, D (review: the acceptance row "every original type"): data, not one type.
    auto l = lan5();
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto free = free_tile(*l, 8);
    REQUIRE(free.has_value());
    std::size_t drawn = 0, noted = 0, transparent = 0, compared = 0;
    for (const sim::TypeId t : l->setup.types) {
        const sim::BuildingType* type = l->setup.registry->find(t);
        REQUIRE(type != nullptr);
        for (int r = 0; r < 4; ++r) {
            CAPTURE(t.v);
            CAPTURE(r);
            // The registry's footprint (HEAD[0], HEAD[1], odd rotations swapped) is the Anlagen
            // entry's, which the scene draws with.
            const auto fp = l->scene->anlagen.footprint(t.v);
            if (fp) {
                const auto [w, h] = sim::footprint(*type, static_cast<std::uint8_t>(r));
                const bool odd = (r & 1) != 0;
                CHECK(w == (odd ? fp->second : fp->first));
                CHECK(h == (odd ? fp->first : fp->second));
                ++compared;
            }
            const app::DevTool tool = tool_at(*l, t, *free, r);
            const std::string note = app::preview_note(tool, *l->scene, 1);
            const auto pv = app::make_preview(tool, *l->scene, 1, 0);
            if (!note.empty()) {
                CHECK_FALSE(pv.has_value()); // a reason, and nothing drawn
                ++noted;
                continue;
            }
            REQUIRE(pv.has_value());
            // The object it draws is the type at the rotation and the hovered tile.
            CHECK(pv->object.first.anlage == t.v);
            CHECK(pv->object.first.rotation == r);
            CHECK(static_cast<std::int32_t>(pv->object.first.x) == free->x - l->setup.island.x0);
            CHECK(static_cast<std::int32_t>(pv->object.first.y) == free->y - l->setup.island.y0);
            if (pv->sprites.empty()) {
                ++transparent; // the entry's frames resolve to nothing at this rotation
            } else {
                ++drawn;
            }
        }
    }
    CHECK(drawn > 0);
    CHECK(compared == 4 * l->setup.types.size());
    MESSAGE(l->setup.types.size() << " buildable types x 4 rotations: footprints compared "
                                  << compared << ", previews with sprites " << drawn
                                  << ", without sprites " << transparent << ", with a reason "
                                  << noted);
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
    // A free tile of the island by the sim's own lookup: unchecked (the placement rule is the
    // sim's).
    const std::optional<sim::TileXY> free = free_tile(*l);
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

TEST_CASE("dev feedback: the composed title never lets an earlier command's refusal pass for the "
          "latest command's, and shows the money once" *
          doctest::test_suite("dev_feedback")) {
    auto l = lan5();
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // A type without goods costs, a free tile by the sim's lookup, and the tool there (all read
    // before the world moves into the runner).
    std::optional<sim::TypeId> plain;
    for (const sim::TypeId t : l->setup.types) {
        const sim::BuildingType* type = l->setup.registry->find(t);
        if (!plain && type != nullptr && type->cost_goods.empty() && type->width == 1 &&
            type->height == 1 && (type->flags_word & sim::kRoadFlag) == 0) {
            plain = t;
        }
    }
    REQUIRE(plain.has_value());
    const auto free = free_tile(*l);
    REQUIRE(free.has_value());
    const app::DevTool tool = tool_at(*l, *plain, *free);
    app::PlacementKnowledge knows(l->setup);
    const app::DevToolSetup setup = l->setup;
    app::PlayRunner runner(std::move(l->play.world));
    const auto build = [&] {
        sim::Command cmd;
        cmd.issuer = setup.issuer;
        sim::BuildCmd b;
        b.city = *setup.city;
        b.placements.push_back(sim::PlacementWire{*plain, free->x, free->y, 0, 0, 0});
        cmd.body = std::move(b);
        return cmd;
    };
    const app::CommandId first = runner.post_command(build());
    runner.run_n(1);
    REQUIRE(runner.outcome(first).state == app::CommandOutcome::State::Applied);
    const app::CommandId refused = runner.post_command(build()); // the same tile: Occupied
    runner.run_n(1);
    REQUIRE(runner.outcome(refused).state == app::CommandOutcome::State::Refused);
    sim::Command demolish;
    demolish.issuer = setup.issuer;
    demolish.body = sim::DemolishCmd{*setup.city, {*free}};
    const app::CommandId latest = runner.post_command(std::move(demolish));
    runner.run_n(1);
    REQUIRE(runner.outcome(latest).state == app::CommandOutcome::State::Applied);
    const auto snap = runner.snapshot();
    const std::string title =
        app::dev_title(*snap, tool, knows.check(tool, snap.get()), runner.outcome(latest));
    // The refusal is still in the runner's line (within kRejectionShowTurns), named by its
    // command; the latest command's own outcome is what the feedback names.
    const std::string refusal =
        "refused: Occupied (window command #" + std::to_string(refused.n) + ")";
    CHECK_MESSAGE(title.find(refusal) != std::string::npos, title);
    CHECK_MESSAGE(title.find("command #" + std::to_string(latest.n) + ": Applied") !=
                      std::string::npos,
                  title);
    std::size_t money = 0;
    for (std::size_t at = title.find("money "); at != std::string::npos;
         at = title.find("money ", at + 1)) {
        ++money;
    }
    CHECK_MESSAGE(money == 1, title);
    MESSAGE("title: " << title);
}

TEST_CASE("dev feedback: the developer start's resources are the fixed map's only, an original "
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
