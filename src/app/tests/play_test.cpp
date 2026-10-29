// Play mode (docs/app/game-shell.md). Cases that need the original files skip
// when ANNO1503_GAME_DIR is unset.
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/cli.hpp>
#include <anno/app/play.hpp>
#include <anno/render/iso.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/reject.hpp>
#include <anno/sim/replay.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;

namespace {

std::string upper_copy(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

fs::path single_01(const fs::path& dir) {
    return dir / "Scenes" / "0200Single" / "Single_01.sze";
}

fs::path endless_sze(const fs::path& dir) {
    const auto files = anno::test::files_with_ext(dir / "Scenes" / "0010SingleEndlos", "sze");
    return files.empty() ? fs::path{} : files.front();
}

anno::app::SceneObjects objects_of(const anno::sim::World& world,
                                   const std::vector<anno::app::SceneBuilding>& buildings) {
    anno::app::SceneObjects objects;
    objects.is_building = [&](std::uint16_t id) {
        return anno::app::anlage_is_building(world, id);
    };
    objects.buildings = buildings;
    return objects;
}

} // namespace

TEST_CASE("play: --play parses and --view does not") {
    const auto play = anno::app::parse_args({"--play", "Scenes/0200Single/Single_01.sze"});
    REQUIRE(play);
    CHECK(play->play);
    CHECK(play->file.filename() == "Single_01.sze");
    const auto view = anno::app::parse_args({"--view", "x.sze"});
    REQUIRE(view);
    CHECK_FALSE(view->play);
    const auto fast = anno::app::parse_args({"--play", "x.sze", "--speed", "200"});
    REQUIRE(fast);
    CHECK(fast->play_speed == 200);
    CHECK_FALSE(anno::app::parse_args({"--play", "x.sze", "--speed", "75"}));
}

TEST_CASE("play: islands are the ones the sim placed") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path file = single_01(*dir);
    if (!fs::exists(file)) {
        MESSAGE("skipping: Single_01.sze is not in this game directory");
        return;
    }
    auto opened = anno::app::load_play(*dir, file);
    REQUIRE(opened);
    const auto buildings = anno::app::play_buildings(opened.value().world);
    auto objects = objects_of(opened.value().world, buildings);
    auto scene = anno::app::load_scene(*dir, file, opened.value().links, &objects);
    REQUIRE(scene);
    const auto& islands = scene.value().scene->islands;
    REQUIRE(islands.size() == opened.value().islands.size());
    REQUIRE(scene.value().island_sources.size() == islands.size());
    std::map<std::uint32_t, const anno::app::PlayIsland*> by_slot;
    for (const auto& island : opened.value().islands) {
        by_slot[island.slot] = &island;
    }
    for (std::size_t i = 0; i < islands.size(); ++i) {
        const auto& drawn = islands[i];
        const auto it = by_slot.find(drawn.number & 0xFFFFu);
        REQUIRE(it != by_slot.end());
        const anno::app::PlayIsland& sim = *it->second;
        CHECK(drawn.x == sim.x);
        CHECK(drawn.y == sim.y);
        CHECK(drawn.width == sim.w);
        CHECK(drawn.height == sim.h);
        if (sim.template_key == "embedded") {
            CHECK(scene.value().island_sources[i].empty());
        } else {
            const std::string source =
                upper_copy(fs::path(scene.value().island_sources[i]).generic_string());
            CHECK(source.find(upper_copy(sim.template_key)) != std::string::npos);
        }
    }

    const fs::path linked = endless_sze(*dir);
    if (linked.empty()) {
        MESSAGE("no endless scenario; link templates were not checked");
        return;
    }
    auto endless = anno::app::load_play(*dir, linked);
    REQUIRE(endless);
    bool any_link = false;
    for (const auto& island : endless.value().islands) {
        any_link = any_link || island.template_key != "embedded";
    }
    CHECK(any_link);
    const auto link_buildings = anno::app::play_buildings(endless.value().world);
    auto link_objects = objects_of(endless.value().world, link_buildings);
    auto link_scene = anno::app::load_scene(*dir, linked, endless.value().links, &link_objects);
    REQUIRE(link_scene);
    CHECK(link_scene.value().notes.end() !=
          std::find_if(link_scene.value().notes.begin(), link_scene.value().notes.end(),
                       [](const std::string& n) {
                           return n.find("templates the sim placed") != std::string::npos;
                       }));
    std::map<std::uint32_t, const anno::app::PlayIsland*> linked_slots;
    for (const auto& island : endless.value().islands) {
        linked_slots[island.slot] = &island;
    }
    REQUIRE(link_scene.value().scene->islands.size() == endless.value().islands.size());
    for (std::size_t i = 0; i < link_scene.value().scene->islands.size(); ++i) {
        const auto& drawn = link_scene.value().scene->islands[i];
        const auto it = linked_slots.find(drawn.number & 0xFFFFu);
        REQUIRE(it != linked_slots.end());
        CHECK(drawn.x == it->second->x);
        CHECK(drawn.y == it->second->y);
        CHECK(drawn.width == it->second->w);
        CHECK(drawn.height == it->second->h);
        if (it->second->template_key != "embedded") {
            const std::string source =
                upper_copy(fs::path(link_scene.value().island_sources[i]).generic_string());
            CHECK(source.find(upper_copy(it->second->template_key)) != std::string::npos);
        }
    }
}

TEST_CASE("play: runner turns match run_turn and ignore the wall clock") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path file = single_01(*dir);
    if (!fs::exists(file)) {
        MESSAGE("skipping: Single_01.sze is not in this game directory");
        return;
    }
    auto through = anno::app::load_play(*dir, file);
    auto direct = anno::app::load_play(*dir, file);
    REQUIRE(through);
    REQUIRE(direct);
    anno::app::PlayRunner runner(std::move(through.value().world));
    CHECK(runner.step_ms() == 10);
    constexpr std::uint32_t kTurns = 3;
    runner.run_n(kTurns);
    anno::sim::Turn turn;
    turn.step_ms = runner.step_ms();
    for (std::uint32_t i = 0; i < kTurns; ++i) {
        turn.index = i;
        anno::sim::run_turn(direct.value().world, nullptr, turn,
                            anno::sim::Timing{}.mp_steps_per_turn);
    }
    CHECK(runner.state_hash() == direct.value().world.state_hash());

    auto delayed = anno::app::load_play(*dir, file);
    auto plain = anno::app::load_play(*dir, file);
    REQUIRE(delayed);
    REQUIRE(plain);
    anno::app::PlayRunner slow(std::move(delayed.value().world));
    slow.set_turn_delay(std::chrono::milliseconds(40));
    slow.run_n(1);
    anno::sim::Turn one;
    one.step_ms = slow.step_ms();
    anno::sim::run_turn(plain.value().world, nullptr, one, anno::sim::Timing{}.mp_steps_per_turn);
    CHECK(slow.state_hash() == plain.value().world.state_hash());

    auto double_speed = anno::app::load_play(*dir, file);
    REQUIRE(double_speed);
    anno::app::PlayRunner fast(std::move(double_speed.value().world), 200);
    CHECK(fast.step_ms() == 20);
}

TEST_CASE("play: a slow turn does not block a frame reading the snapshot") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path file = single_01(*dir);
    if (!fs::exists(file)) {
        MESSAGE("skipping: Single_01.sze is not in this game directory");
        return;
    }
    auto opened = anno::app::load_play(*dir, file);
    REQUIRE(opened);
    anno::app::PlayRunner runner(std::move(opened.value().world));
    runner.set_turn_delay(std::chrono::milliseconds(1500));
    runner.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const auto started = std::chrono::steady_clock::now();
    const auto snap = runner.snapshot();
    const auto title = runner.title_line();
    const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    runner.stop();
    CHECK(waited.count() < 50);
    REQUIRE(snap);
    CHECK(snap->tick == 0);
    CHECK(title.find("tick 0") != std::string::npos);
}

TEST_CASE("play: demolish before the scene drops that building") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path file = endless_sze(*dir);
    if (file.empty()) {
        MESSAGE("skipping: no endless scenario");
        return;
    }
    auto kept = anno::app::load_play(*dir, file);
    auto edited = anno::app::load_play(*dir, file);
    REQUIRE(kept);
    REQUIRE(edited);
    const anno::app::PlayIsland* origin = nullptr;
    anno::app::SceneBuilding target{};
    bool demolished = false;
    std::string last_reject;
    std::size_t city_count = 0;
    std::size_t building_count = 0;
    const auto& islands = edited.value().world.islands();
    for (std::size_t i = 0; i < islands.size() && !demolished; ++i) {
        if (!islands[i]) {
            continue;
        }
        for (const std::optional<anno::sim::City>& city : islands[i]->cities) {
            if (!city || demolished) {
                continue;
            }
            ++city_count;
            for (const anno::sim::BuildingList& list : city->index.lists) {
                for (const std::optional<anno::sim::Building>& building : list.slots) {
                    if (!building) {
                        continue;
                    }
                    ++building_count;
                    anno::sim::Command cmd;
                    cmd.issuer = city->owner;
                    cmd.body = anno::sim::DemolishCmd{
                        anno::sim::CityRef{anno::sim::IslandId{static_cast<std::uint32_t>(i)},
                                           city->number},
                        {anno::sim::TileXY{building->x, building->y}}};
                    const anno::sim::ApplyResult applied = edited.value().world.apply(cmd);
                    if (!applied.ok()) {
                        last_reject = std::string(anno::sim::reject_name(applied.status));
                        continue;
                    }
                    const std::int32_t local_x = building->x - islands[i]->tiles.x0();
                    const std::int32_t local_y = building->y - islands[i]->tiles.y0();
                    target.island = static_cast<std::uint32_t>(i);
                    target.type = static_cast<std::uint16_t>(building->type.v);
                    target.x = static_cast<std::uint16_t>(local_x);
                    target.y = static_cast<std::uint16_t>(local_y);
                    demolished = true;
                    break;
                }
                if (demolished) {
                    break;
                }
            }
        }
    }
    REQUIRE_MESSAGE(demolished, "cities " << city_count << " buildings " << building_count
                                          << " reject " << last_reject);
    for (const auto& island : edited.value().islands) {
        if (island.slot == target.island) {
            origin = &island;
        }
    }
    REQUIRE(origin != nullptr);
    const auto after = anno::app::play_buildings(edited.value().world);
    const auto still =
        std::find_if(after.begin(), after.end(), [&](const anno::app::SceneBuilding& b) {
            return b.island == target.island && b.type == target.type && b.x == target.x &&
                   b.y == target.y;
        });
    CHECK(still == after.end());

    const auto kept_buildings = anno::app::play_buildings(kept.value().world);
    auto kept_objects = objects_of(kept.value().world, kept_buildings);
    auto edited_buildings = anno::app::play_buildings(edited.value().world);
    auto edited_objects = objects_of(edited.value().world, edited_buildings);
    auto kept_scene = anno::app::load_scene(*dir, file, kept.value().links, &kept_objects);
    auto edited_scene = anno::app::load_scene(*dir, file, edited.value().links, &edited_objects);
    REQUIRE(kept_scene);
    REQUIRE(edited_scene);
    const bool listed =
        std::any_of(edited_scene.value().omitted.begin(), edited_scene.value().omitted.end(),
                    [&](const anno::app::OmittedObject& o) {
                        return o.island == target.island && o.type == target.type &&
                               o.x == target.x && o.y == target.y;
                    });
    REQUIRE_MESSAGE(listed, "target island " << target.island << " type " << target.type << " at "
                                             << target.x << "," << target.y << " omitted "
                                             << edited_scene.value().omitted.size());

    const auto [ax, ay] = anno::render::tile_top(origin->x + target.x, origin->y + target.y, 2);
    const anno::render::Rect box{ax - 96, ay - 48, 220, 180};
    anno::render::Layers layers = anno::render::Layers::all();
    const auto [kept_px, kept_stats] = kept_scene.value().scene->render(box, 2, layers);
    const auto [edited_px, edited_stats] = edited_scene.value().scene->render(box, 2, layers);
    (void)kept_stats;
    (void)edited_stats;
    REQUIRE(kept_px.pixels.size() == edited_px.pixels.size());
    CHECK(kept_px.pixels != edited_px.pixels);
}

TEST_CASE("play: tick 0 matches the viewer except objects the sim did not place") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto files = anno::test::files_with_ext(*dir / "Scenes" / "0200Single", "sze");
    if (files.size() < 10) {
        MESSAGE("fewer than ten 0200Single scenarios");
    }
    const std::size_t n = std::min<std::size_t>(files.size(), 10);
    anno::render::Layers layers = anno::render::Layers::all();
    for (std::size_t i = 0; i < n; ++i) {
        auto play = anno::app::load_play(*dir, files[i]);
        REQUIRE(play);
        const auto buildings = anno::app::play_buildings(play.value().world);
        auto objects = objects_of(play.value().world, buildings);
        auto played = anno::app::load_scene(*dir, files[i], play.value().links, &objects);
        auto viewed = anno::app::load_scene(*dir, files[i]);
        REQUIRE(played);
        REQUIRE(viewed);
        const auto& scene = *played.value().scene;
        const auto bounds = scene.bounds(2);
        REQUIRE(bounds);
        const auto bytes = std::uint64_t(bounds->w) * std::uint64_t(bounds->h) * 4;
        anno::render::Rect rect = *bounds;
        if (bytes > 48ull * 1024 * 1024) {
            rect = {bounds->x, bounds->y, std::min(bounds->w, 640), std::min(bounds->h, 360)};
            MESSAGE(files[i].filename().string()
                    << " is " << (bytes >> 20) << " MiB; compared a corner");
        }
        const auto [play_px, play_st] = scene.render(rect, 2, layers);
        const auto [view_px, view_st] = viewed.value().scene->render(rect, 2, layers);
        (void)play_st;
        (void)view_st;
        REQUIRE(play_px.pixels.size() == view_px.pixels.size());
        if (play_px.pixels == view_px.pixels) {
            continue;
        }
        std::vector<anno::render::Rect> boxes;
        for (const auto& o : played.value().omitted) {
            const auto [ax, ay] = anno::render::tile_top(static_cast<int>(o.origin_x) + o.x,
                                                         static_cast<int>(o.origin_y) + o.y, 2);
            boxes.push_back({ax - 160, ay - 80, 360, 280});
        }
        std::size_t outside = 0;
        for (std::size_t p = 0; p < play_px.pixels.size(); p += 4) {
            if (std::memcmp(play_px.pixels.data() + p, view_px.pixels.data() + p, 4) == 0) {
                continue;
            }
            const int x = rect.x + static_cast<int>((p / 4) % play_px.width);
            const int y = rect.y + static_cast<int>((p / 4) / play_px.width);
            const bool inside =
                std::any_of(boxes.begin(), boxes.end(), [&](const anno::render::Rect& b) {
                    return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h;
                });
            if (!inside) {
                ++outside;
            }
        }
        CHECK_MESSAGE(outside == 0, files[i].filename().string()
                                        << " " << outside << " pixels outside omitted objects");
    }
}

TEST_CASE("play: turn cost on the heavy endless scenario") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path file = *dir / "Scenes" / "0010SingleEndlos" / "06_1024_heavy_256.sze";
    if (!fs::exists(file)) {
        MESSAGE("skipping: 06_1024_heavy_256.sze is not in this game directory");
        return;
    }
    auto opened = anno::app::load_play(*dir, file);
    REQUIRE(opened);
    anno::app::PlayRunner runner(std::move(opened.value().world));
    std::vector<double> ms;
    ms.reserve(8);
    for (int i = 0; i < 8; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        runner.run_n(1);
        ms.push_back(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                .count());
    }
    std::sort(ms.begin(), ms.end());
    const auto at = [&](std::size_t numer, std::size_t denom) {
        std::size_t i = (ms.size() * numer + denom - 1) / denom;
        if (i == 0) {
            i = 1;
        }
        return ms[std::min(ms.size(), i) - 1];
    };
    MESSAGE("heavy 8 turns ms p50 " << at(1, 2) << " p95 " << at(95, 100) << " p99 " << at(99, 100)
                                    << " max " << ms.back());
    CHECK(runner.step_ms() == 10);
    CHECK(runner.snapshot()->tick > 0);
}
