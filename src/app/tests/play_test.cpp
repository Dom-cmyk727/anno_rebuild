// Play mode (docs/app/game-shell.md). Cases that need the original files skip
// when ANNO1503_GAME_DIR is unset.
#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/cli.hpp>
#include <anno/app/play.hpp>
#include <anno/formats/fixed_map.hpp>
#include <anno/render/iso.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/reject.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim_load/fixed_map.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>
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

void expect_play_islands(const std::string& label, const anno::app::LoadedScene& scene,
                         const std::vector<anno::app::PlayIsland>& sim_islands) {
    const auto& islands = scene.scene->islands;
    REQUIRE_MESSAGE(islands.size() == sim_islands.size(), label);
    REQUIRE_MESSAGE(scene.island_sources.size() == islands.size(), label);
    std::map<std::uint32_t, const anno::app::PlayIsland*> by_slot;
    for (const auto& island : sim_islands) {
        by_slot[island.slot] = &island;
    }
    for (std::size_t i = 0; i < islands.size(); ++i) {
        const auto& drawn = islands[i];
        // The island table slot is the link number's low 16 bits
        // (IslandLinkSpec, D AnnoScene 0x10008503).
        const auto it = by_slot.find(drawn.number & 0xFFFFu);
        REQUIRE_MESSAGE(it != by_slot.end(), label << " island " << drawn.number);
        const anno::app::PlayIsland& sim = *it->second;
        CHECK(drawn.x == sim.x);
        CHECK(drawn.y == sim.y);
        CHECK(drawn.width == sim.w);
        CHECK(drawn.height == sim.h);
        if (sim.template_key == "embedded") {
            CHECK(scene.island_sources[i].empty());
        } else {
            const std::string source =
                upper_copy(fs::path(scene.island_sources[i]).generic_string());
            CHECK_MESSAGE(source.find(upper_copy(sim.template_key)) != std::string::npos,
                          label << " " << sim.template_key);
        }
    }
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
    const auto files = anno::test::files_with_ext(*dir / "Scenes", "sze");
    if (files.empty()) {
        MESSAGE("skipping: no scenario files");
        return;
    }
    bool any_link = false;
    for (const fs::path& file : files) {
        const std::string label = fs::relative(file, *dir / "Scenes").generic_string();
        auto opened = anno::app::load_play(*dir, file);
        if (!opened) {
            const std::string err = opened.error();
            REQUIRE_MESSAGE(false, label << ": " << err);
        }
        bool file_link = false;
        for (const auto& island : opened.value().islands) {
            file_link = file_link || island.template_key != "embedded";
        }
        any_link = any_link || file_link;
        const auto buildings = anno::app::play_buildings(opened.value().world);
        auto objects = objects_of(opened.value().world, buildings);
        auto scene = anno::app::load_scene(*dir, file, opened.value().links, &objects);
        if (!scene) {
            const std::string err = scene.error();
            REQUIRE_MESSAGE(false, label << ": " << err);
        }
        if (file_link) {
            const bool noted = std::any_of(
                scene.value().notes.begin(), scene.value().notes.end(), [](const std::string& n) {
                    return n.find("templates the sim placed") != std::string::npos;
                });
            CHECK_MESSAGE(noted, label);
        }
        expect_play_islands(label, scene.value(), opened.value().islands);
    }
    CHECK(any_link);
    MESSAGE(files.size() << " scenarios");
}

TEST_CASE("play: lan5 islands are the ones the sim placed") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path path =
        fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        const std::string shown = path.string();
        REQUIRE_MESSAGE(false, "cannot read " << shown);
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    auto parsed = anno::formats::fixed_map::parse(bytes);
    if (!parsed) {
        const std::string err = parsed.error().message;
        REQUIRE_MESSAGE(false, path.filename().string() << ": " << err);
    }
    const auto registry =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    auto loaded = anno::sim_load::load_fixed_map(
        parsed.value(),
        std::vector<anno::sim::Seat>{anno::sim::Seat{anno::sim::SeatKind::Human, "P0", 0, 0},
                                     anno::sim::Seat{anno::sim::SeatKind::Human, "P1", 1, 0}},
        *dir, *registry.types, {});
    if (!loaded) {
        const std::string err = loaded.error();
        REQUIRE_MESSAGE(false, err);
    }
    auto created = anno::sim::World::create(loaded.value().spec, registry.types);
    if (!created) {
        const std::string err = created.error();
        REQUIRE_MESSAGE(false, err);
    }

    const anno::sim_load::IslandTemplates templates = anno::sim_load::scan_island_templates(*dir);
    std::map<std::string, std::size_t> by_name;
    for (std::size_t i = 0; i < templates.summaries.size(); ++i) {
        by_name.emplace(templates.summaries[i].name, i);
    }
    std::vector<anno::app::PlacedIsland> placed;
    std::map<std::uint32_t, std::string> key_by_slot;
    for (const anno::sim::IslandLinkSpec& link : loaded.value().links) {
        const auto found = by_name.find(upper_copy(link.path));
        REQUIRE_MESSAGE(found != by_name.end(), link.path);
        anno::app::PlacedIsland item;
        item.number = link.number;
        item.x = static_cast<std::int32_t>(link.x);
        item.y = static_cast<std::int32_t>(link.y);
        item.file = templates.files[found->second];
        placed.push_back(std::move(item));
        key_by_slot.emplace(link.number & 0xFFFFu, link.path);
    }
    auto scene = anno::app::load_placed_scene(*dir, "lan5", placed, nullptr);
    if (!scene) {
        const std::string err = scene.error();
        REQUIRE_MESSAGE(false, err);
    }

    std::vector<anno::app::PlayIsland> sim_islands;
    for (const auto& island : created.value().islands()) {
        if (!island) {
            continue;
        }
        anno::app::PlayIsland row;
        row.slot = island->index.v;
        row.x = island->rect().x0;
        row.y = island->rect().y0;
        row.w = island->rect().w;
        row.h = island->rect().h;
        const auto key = key_by_slot.find(row.slot);
        row.template_key = key == key_by_slot.end() ? "embedded" : key->second;
        sim_islands.push_back(std::move(row));
    }
    expect_play_islands("lan5", scene.value(), sim_islands);
    CHECK_FALSE(sim_islands.empty());
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

namespace {

bool screen_in(const anno::render::Rect& r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

/// Pixels of `play` that differ from `view` and sit outside the omitted-object
/// boxes. Equal canvases return 0 without a scan.
std::size_t diffs_outside(const anno::render::Canvas& play, const anno::render::Canvas& view,
                          const anno::render::Rect& rect,
                          const std::vector<anno::render::Rect>& boxes) {
    if (play.pixels == view.pixels) {
        return 0;
    }
    std::size_t outside = 0;
    for (std::size_t p = 0; p < play.pixels.size(); p += 4) {
        if (std::memcmp(play.pixels.data() + p, view.pixels.data() + p, 4) == 0) {
            continue;
        }
        const int x = rect.x + static_cast<int>((p / 4) % play.width);
        const int y = rect.y + static_cast<int>((p / 4) / play.width);
        const bool inside =
            std::any_of(boxes.begin(), boxes.end(),
                        [&](const anno::render::Rect& b) { return screen_in(b, x, y); });
        if (!inside) {
            ++outside;
        }
    }
    return outside;
}

} // namespace

TEST_CASE("play: tick 0 matches the viewer except objects the sim did not place") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto files = anno::test::files_with_ext(*dir / "Scenes" / "0200Single", "sze");
    // The ten campaign scenarios. A short directory is a missing install, not a pass.
    REQUIRE(files.size() == 10);
    // One window is 512² RGBA, 1 MiB. A full 0200Single frame is far larger, which
    // is why the previous check kept a 640×360 corner. Test bound only: the windows
    // tile each island, so raising this constant compares the same pixels in fewer
    // pieces.
    constexpr int kWin = 512;
    anno::render::Layers layers = anno::render::Layers::all();
    std::size_t windows = 0;
    std::size_t building_count = 0;
    for (const fs::path& file : files) {
        auto play = anno::app::load_play(*dir, file);
        REQUIRE(play);
        const auto buildings = anno::app::play_buildings(play.value().world);
        auto objects = objects_of(play.value().world, buildings);
        auto played = anno::app::load_scene(*dir, file, play.value().links, &objects);
        auto viewed = anno::app::load_scene(*dir, file);
        REQUIRE(played);
        REQUIRE(viewed);
        const auto& scene = *played.value().scene;
        const auto& layout = scene.layouts[2];
        REQUIRE(layout.size() == scene.islands.size());
        std::vector<anno::render::Rect> boxes;
        for (const auto& o : played.value().omitted) {
            const auto [ax, ay] = anno::render::tile_top(static_cast<int>(o.origin_x) + o.x,
                                                         static_cast<int>(o.origin_y) + o.y, 2);
            boxes.push_back({ax - 160, ay - 80, 360, 280});
        }
        std::vector<anno::render::Rect> compared;
        auto compare_window = [&](anno::render::Rect rect) {
            if (rect.w <= 0 || rect.h <= 0) {
                return;
            }
            const auto [play_px, play_st] = scene.render(rect, 2, layers);
            const auto [view_px, view_st] = viewed.value().scene->render(rect, 2, layers);
            (void)play_st;
            (void)view_st;
            REQUIRE(play_px.pixels.size() == view_px.pixels.size());
            const std::size_t outside = diffs_outside(play_px, view_px, rect, boxes);
            CHECK_MESSAGE(outside == 0, file.filename().string()
                                            << " " << outside
                                            << " pixels outside omitted objects at " << rect.x
                                            << "," << rect.y);
            compared.push_back(rect);
            ++windows;
        };
        auto tile = [&](const anno::render::Rect& area) {
            for (int y = area.y; y < area.y + area.h; y += kWin) {
                for (int x = area.x; x < area.x + area.w; x += kWin) {
                    const int w = std::min(kWin, area.x + area.w - x);
                    const int h = std::min(kWin, area.y + area.h - y);
                    compare_window({x, y, w, h});
                }
            }
        };
        for (const anno::render::Placement& place : layout) {
            CHECK(place.bounds.w > 0);
            CHECK(place.bounds.h > 0);
            tile(place.bounds);
        }
        auto covered = [&](int x, int y) {
            return std::any_of(compared.begin(), compared.end(),
                               [&](const anno::render::Rect& r) { return screen_in(r, x, y); });
        };
        for (const anno::app::SceneBuilding& b : buildings) {
            ++building_count;
            const auto host =
                std::find_if(scene.islands.begin(), scene.islands.end(), [&](const auto& island) {
                    return (island.number & 0xFFFFu) == b.island;
                });
            REQUIRE_MESSAGE(host != scene.islands.end(), file.filename().string()
                                                             << " building island " << b.island);
            const auto [sx, sy] = anno::render::tile_top(static_cast<int>(host->x) + b.x,
                                                         static_cast<int>(host->y) + b.y, 2);
            if (!covered(sx, sy)) {
                compare_window({sx - kWin / 2, sy - kWin / 2, kWin, kWin});
            }
            CHECK_MESSAGE(covered(sx, sy), file.filename().string()
                                               << " building " << b.type << " at " << sx << ","
                                               << sy << " is outside the compared windows");
        }
        for (const anno::app::OmittedObject& o : played.value().omitted) {
            const auto [sx, sy] = anno::render::tile_top(static_cast<int>(o.origin_x) + o.x,
                                                         static_cast<int>(o.origin_y) + o.y, 2);
            if (!covered(sx, sy)) {
                compare_window({sx - kWin / 2, sy - kWin / 2, kWin, kWin});
            }
            CHECK_MESSAGE(covered(sx, sy), file.filename().string()
                                               << " omitted " << o.type << " at " << sx << "," << sy
                                               << " is outside the compared windows");
        }
    }
    CHECK(windows > 0);
    CHECK(building_count > 0);
    MESSAGE(files.size() << " scenarios, " << windows << " windows, " << building_count
                         << " buildings");
}

/// Virtual clock. `sleep_until` and `sleep_for` both wake `overshoot` late, which
/// is the Windows timer missing a short sleep. Absolute deadlines absorb that;
/// a sleep aimed at now + period accumulates it. Sleeps are counted so the test
/// can release one and wait for the next without missing a fast thread.
class ManualClock final : public anno::app::PlaySchedule {
  public:
    explicit ManualClock(std::chrono::milliseconds overshoot) : overshoot_(overshoot) {}

    time_point now() const override {
        std::lock_guard lock(mu_);
        return cur_;
    }

    void sleep_until(time_point deadline) override { block(deadline); }

    void sleep_for(duration d) override {
        time_point target;
        {
            std::lock_guard lock(mu_);
            target = cur_ + d;
        }
        block(target);
    }

    void wake() override {
        std::lock_guard lock(mu_);
        abort_ = true;
        cv_.notify_all();
    }

    [[nodiscard]] bool wait_until_sleeping(std::chrono::milliseconds budget) {
        std::unique_lock lock(mu_);
        return cv_.wait_for(lock, budget, [&] { return entered_ > released_ || abort_; });
    }

    /// Lets the sim thread leave its current sleep, then waits until it blocks
    /// in the next one. Returns false on abort or timeout.
    [[nodiscard]] bool release_and_wait(std::chrono::milliseconds budget) {
        std::unique_lock lock(mu_);
        if (entered_ <= released_ &&
            !cv_.wait_for(lock, budget, [&] { return entered_ > released_ || abort_; })) {
            return false;
        }
        if (abort_) {
            return false;
        }
        const auto generation = entered_;
        released_ = generation;
        cv_.notify_all();
        if (!cv_.wait_for(lock, budget, [&] { return completed_ >= generation || abort_; })) {
            return false;
        }
        if (abort_) {
            return false;
        }
        return cv_.wait_for(lock, budget, [&] { return entered_ > generation || abort_; });
    }

    [[nodiscard]] std::int64_t millis() const {
        std::lock_guard lock(mu_);
        return std::chrono::duration_cast<std::chrono::milliseconds>(cur_.time_since_epoch())
            .count();
    }

  private:
    void block(time_point target) {
        std::unique_lock lock(mu_);
        if (abort_) {
            return;
        }
        const auto generation = ++entered_;
        cv_.notify_all();
        cv_.wait(lock, [&] { return released_ >= generation || abort_; });
        if (!abort_) {
            if (cur_ < target) {
                cur_ = target;
            }
            cur_ += overshoot_;
        }
        completed_ = generation;
        cv_.notify_all();
    }

    mutable std::mutex mu_;
    std::condition_variable cv_;
    time_point cur_{};
    duration overshoot_{};
    std::uint64_t entered_ = 0;
    std::uint64_t released_ = 0;
    std::uint64_t completed_ = 0;
    bool abort_ = false;
};

anno::sim::World empty_world() {
    auto made =
        anno::sim::World::create(anno::sim::ScenarioSpec{}, anno::sim::TypeRegistry::empty());
    REQUIRE(made);
    return std::move(made).value();
}

TEST_CASE("play: absolute deadlines do not accumulate sleep overshoot") {
    // 5 ms is the size of the drift the orchestrator measured (~4.7 ms/turn).
    constexpr int kOvershootMs = 5;
    constexpr std::uint64_t kEarlyTurn = 100;
    constexpr std::uint64_t kLateTurn = 600;
    const auto period = static_cast<std::uint64_t>(anno::sim::Timing{}.mp_turn_real_ms);
    const auto budget = std::chrono::seconds(2);
    for (const unsigned speed : {50u, 100u, 200u}) {
        ManualClock clock{std::chrono::milliseconds(kOvershootMs)};
        anno::app::PlayRunner runner(empty_world(), speed, &clock);
        const auto per_turn = static_cast<std::uint64_t>(runner.step_ms()) *
                              static_cast<std::uint64_t>(anno::sim::Timing{}.mp_steps_per_turn);
        REQUIRE(per_turn > 0);
        runner.start();
        REQUIRE_MESSAGE(clock.wait_until_sleeping(budget), "speed " << speed << " never slept");
        auto turns_of = [&] { return runner.snapshot()->game_ms / per_turn; };
        auto lateness = [&](std::uint64_t turns) {
            const auto scheduled = (turns - 1) * period;
            const auto stamp = clock.millis();
            if (stamp <= 0) {
                return std::uint64_t{0};
            }
            const auto real = static_cast<std::uint64_t>(stamp);
            return real > scheduled ? real - scheduled : std::uint64_t{0};
        };
        std::uint64_t seen = turns_of();
        std::uint64_t max_late = seen == 0 ? 0 : lateness(seen);
        struct Mark {
            bool set = false;
            std::uint64_t real = 0;
            std::uint64_t game = 0;
        };
        Mark early;
        Mark late;
        auto note = [&](std::uint64_t turns) {
            const auto game = runner.snapshot()->game_ms;
            const auto stamp = clock.millis();
            const auto real = stamp > 0 ? static_cast<std::uint64_t>(stamp) : std::uint64_t{0};
            if (!early.set && turns >= kEarlyTurn) {
                early = {true, real, game};
            }
            if (!late.set && turns >= kLateTurn) {
                late = {true, real, game};
            }
        };
        note(seen);
        int pumps = 0;
        while (seen < kLateTurn) {
            REQUIRE_MESSAGE(clock.release_and_wait(budget), "speed " << speed << " stalled");
            const auto now_turns = turns_of();
            if (now_turns != seen) {
                seen = now_turns;
                max_late = std::max(max_late, lateness(seen));
                note(seen);
            }
            REQUIRE_MESSAGE(++pumps < 20000, "speed " << speed << " did not reach " << kLateTurn
                                                      << " turns (" << seen << ")");
        }
        const auto snap = runner.snapshot();
        CHECK_MESSAGE(max_late <= static_cast<std::uint64_t>(kOvershootMs),
                      "speed " << speed << " lateness " << max_late << " ms");
        CHECK_MESSAGE(snap->behind_ms == 0, "speed " << speed << " behind " << snap->behind_ms);
        REQUIRE(early.set);
        REQUIRE(late.set);
        REQUIRE(late.real > early.real);
        const auto delta_game = late.game - early.game;
        const auto delta_real = late.real - early.real;
        const auto lhs = delta_game * period;
        const auto rhs = per_turn * delta_real;
        const auto diff = lhs > rhs ? lhs - rhs : rhs - lhs;
        // 0.5 % of the expected game/real ratio: |got - want| / want <= 1/200.
        CHECK_MESSAGE(diff * 200 <= rhs,
                      "speed " << speed << " game " << delta_game << " real " << delta_real);
        runner.stop();
    }
}

TEST_CASE("play: a late sim catches up at most five turns") {
    ManualClock clock{std::chrono::milliseconds(0)};
    anno::app::PlayRunner runner(empty_world(), 100, &clock);
    CHECK(runner.catchup_turns() == 5);
    runner.set_catchup_turns(5);
    runner.set_turn_delay(std::chrono::milliseconds(1000));
    runner.start();
    const auto budget = std::chrono::seconds(2);
    int pumps = 0;
    while (runner.snapshot()->behind_ms == 0) {
        REQUIRE_MESSAGE(clock.wait_until_sleeping(budget), "never slept");
        REQUIRE_MESSAGE(clock.release_and_wait(budget), "stalled");
        REQUIRE_MESSAGE(++pumps < 10000, "behind_ms stayed 0 after " << pumps << " sleeps, game "
                                                                     << runner.snapshot()->game_ms);
    }
    const auto snap = runner.snapshot();
    const auto per_turn = static_cast<std::uint64_t>(runner.step_ms()) *
                          static_cast<std::uint64_t>(anno::sim::Timing{}.mp_steps_per_turn);
    CHECK(snap->game_ms == 5 * per_turn);
    // Five 1000 ms turns end at t = 5000. The first deadline strictly after
    // that is index 51. Slots 5..50 were not run.
    const auto period = static_cast<std::uint64_t>(anno::sim::Timing{}.mp_turn_real_ms);
    const std::uint64_t ended = 5ull * 1000ull;
    const std::uint64_t next = ended / period + 1;
    CHECK(snap->behind_ms == (next - 5) * period);
    runner.stop();
}

TEST_CASE("play: the sim thread matches run_turn") {
    ManualClock clock{std::chrono::milliseconds(0)};
    auto origin = empty_world();
    anno::sim::World direct = origin;
    const auto before = direct.state_hash();
    anno::app::PlayRunner runner(std::move(origin), 100, &clock);
    constexpr std::uint64_t kTurns = 50;
    const auto per_turn = static_cast<std::uint64_t>(runner.step_ms()) *
                          static_cast<std::uint64_t>(anno::sim::Timing{}.mp_steps_per_turn);
    runner.start();
    const auto budget = std::chrono::seconds(2);
    REQUIRE_MESSAGE(clock.wait_until_sleeping(budget), "never slept");
    int pumps = 0;
    while (runner.snapshot()->game_ms < kTurns * per_turn) {
        REQUIRE_MESSAGE(clock.release_and_wait(budget), "stalled");
        REQUIRE_MESSAGE(++pumps < 5000, "stopped at game " << runner.snapshot()->game_ms);
    }
    runner.stop();
    CHECK(runner.snapshot()->game_ms == kTurns * per_turn);
    anno::sim::Turn turn;
    turn.step_ms = runner.step_ms();
    for (std::uint32_t i = 0; i < kTurns; ++i) {
        turn.index = i;
        anno::sim::run_turn(direct, nullptr, turn, anno::sim::Timing{}.mp_steps_per_turn);
    }
    CHECK(runner.state_hash() != before);
    CHECK(runner.state_hash() == direct.state_hash());
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
