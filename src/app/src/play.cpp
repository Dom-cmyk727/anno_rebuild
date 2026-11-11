#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <variant>

#include <anno/app/cli.hpp>
#include <anno/app/opening.hpp>
#include <anno/app/play.hpp>
#include <anno/formats/fixed_map.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/game_setup.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/reject.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim_load/fixed_map.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>

namespace anno::app {

namespace {

namespace fs = std::filesystem;

std::string path_text(const fs::path& p) {
    return to_utf8(p);
}

// The extension with a leading dot, lower case ("" for none).
std::string extension_lower(const fs::path& p) {
    std::string ext = p.extension().string();
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

class SteadySchedule final : public PlaySchedule {
  public:
    time_point now() const override { return clock::now(); }
    void sleep_until(time_point deadline) override { std::this_thread::sleep_until(deadline); }
    void sleep_for(duration d) override {
        if (d > duration::zero()) {
            std::this_thread::sleep_for(d);
        }
    }
};

PlaySchedule::time_point deadline_at(PlaySchedule::time_point origin, std::uint64_t index,
                                     std::uint64_t period_ms) {
    const auto ms = static_cast<std::chrono::milliseconds::rep>(index * period_ms);
    return origin + std::chrono::milliseconds(ms);
}

std::uint64_t percentile_95(std::vector<std::uint64_t> values) {
    if (values.empty()) {
        return 0;
    }
    std::sort(values.begin(), values.end());
    std::size_t idx = (values.size() * 95 + 99) / 100;
    if (idx == 0) {
        idx = 1;
    }
    if (idx > values.size()) {
        idx = values.size();
    }
    return values[idx - 1];
}

} // namespace

namespace {

// A fixed map (the layout text) with the developer start and the developer's test
// resources on (DeveloperResources): the world, the placed islands for the scene
// (the same construction as the lan5 test), and the island rows.
formats::Result<PlayLoad, std::string>
load_fixed_play(const fs::path& game_dir, const formats::fixed_map::FixedMap& map,
                const std::shared_ptr<const sim::TypeRegistry>& types,
                const sim_load::IslandTemplates& templates, const fs::path& file,
                const FixedPlayOptions& fixed) {
    // Human seats P0, P1, ... with colours 0, 1, ... (two by default, as the lan5 test and
    // the developer start use them).
    std::vector<sim::Seat> seats;
    for (std::size_t i = 0; i < fixed.seats; ++i) {
        seats.push_back(sim::Seat{sim::SeatKind::Human, "P" + std::to_string(i),
                                  static_cast<std::uint32_t>(i), 0});
    }
    sim_load::FixedMapOptions options;
    options.resources.enabled = true;
    options.resources.gold = fixed.gold;
    options.resources.goods_tons = fixed.goods_tons;
    const sim::SimConfig cfg;
    auto loaded = sim_load::load_fixed_map(map, seats, game_dir, *types, cfg, options);
    if (!loaded) {
        return loaded.error();
    }
    sim::Seams seams;
    if (fixed.stand_in_placement) {
        seams.placement_rule = sim::stand_in_placement_seam();
    }
    auto created = sim::World::create(loaded.value().spec, types, cfg, sim::Rules{}, seams);
    if (!created) {
        return created.error();
    }

    std::map<std::string, std::size_t> by_name;
    for (std::size_t i = 0; i < templates.summaries.size(); ++i) {
        by_name.emplace(templates.summaries[i].name, i);
    }
    PlayLoad out;
    std::map<std::uint32_t, std::string> key_by_slot;
    for (const sim::IslandLinkSpec& link : loaded.value().links) {
        std::string upper = link.path;
        for (char& c : upper) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        const auto found = by_name.find(upper);
        if (found == by_name.end()) {
            return "link template " + link.path + " is not in Scenes/Islands";
        }
        PlacedIsland item;
        item.number = link.number;
        item.x = static_cast<std::int32_t>(link.x);
        item.y = static_cast<std::int32_t>(link.y);
        item.file = templates.files[found->second];
        out.placed.push_back(std::move(item));
        key_by_slot.emplace(link.number & 0xFFFFu, link.path);
    }
    for (const auto& island : created.value().islands()) {
        if (!island) {
            continue;
        }
        PlayIsland row;
        row.slot = island->index.v;
        row.x = island->rect().x0;
        row.y = island->rect().y0;
        row.w = island->rect().w;
        row.h = island->rect().h;
        const auto key = key_by_slot.find(row.slot);
        row.template_key = key == key_by_slot.end() ? "embedded" : key->second;
        out.islands.push_back(std::move(row));
    }
    out.world = std::move(created).value();
    out.fixed_map = true;
    out.name = file.stem().string();
    return out;
}

} // namespace

formats::Result<PlayLoad, std::string> load_play(const fs::path& game_dir, const fs::path& file,
                                                 const FixedPlayOptions& fixed) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::string("cannot read ") + path_text(file);
    }
    const std::vector<std::uint8_t> buf{std::istreambuf_iterator<char>(in), {}};
    const auto registry =
        anno::sim_load::load_original_registry(game_dir, anno::sim_load::FigureEntries::Required);
    if (!registry.types) {
        return std::string("registry: ") + registry.error;
    }
    const anno::sim_load::IslandTemplates templates =
        anno::sim_load::scan_island_templates(game_dir);
    // The route is the extension, not the content: `.txt` is a fixed map (the layout
    // text, data/maps) through load_fixed_map; anything else is a scenario file through
    // load_scenario_with_context. A failure names the file and the loader that refused it.
    if (extension_lower(file) == ".txt") {
        auto fixed_map = anno::formats::fixed_map::parse(buf);
        if (!fixed_map) {
            return path_text(file) +
                   ": parse_fixed_map: " + anno::formats::fixed_map::error_line(fixed_map.error());
        }
        auto built =
            load_fixed_play(game_dir, fixed_map.value(), registry.types, templates, file, fixed);
        if (!built) {
            return path_text(file) + ": load_fixed_map: " + built.error();
        }
        PlayLoad out = std::move(built).value();
        out.registry = registry.types;
        return out;
    }
    const sim::SimConfig cfg;
    auto resolved = anno::sim_load::load_scenario_with_context(buf, file, game_dir, templates,
                                                               *registry.types, cfg, {});
    if (!resolved) {
        return resolved.error();
    }
    auto created = sim::World::create(resolved.value().loaded.spec, registry.types, cfg);
    if (!created) {
        return created.error();
    }

    std::map<std::uint32_t, std::string> keys;
    std::vector<LinkTemplate> links;
    const auto& loaded = resolved.value().loaded;
    const auto& resolution = resolved.value().resolution;
    for (std::size_t i = 0; i < loaded.links.size() && i < resolution.links.size(); ++i) {
        const sim::ResolvedLink& link = resolution.links[i];
        if (!link.candidate || *link.candidate >= templates.files.size()) {
            continue;
        }
        const std::uint32_t slot = loaded.links[i].number & 0xFFFFu;
        keys[slot] = templates.summaries[*link.candidate].name;
        LinkTemplate chosen;
        chosen.number = loaded.links[i].number;
        chosen.file = templates.files[*link.candidate];
        links.push_back(std::move(chosen));
    }

    std::vector<PlayIsland> islands;
    for (const sim::ScenarioSpec::IslandSpec& spec : loaded.spec.islands) {
        if (!spec.terrain) {
            continue;
        }
        PlayIsland island;
        island.slot = spec.slot.v;
        island.x = spec.x0;
        island.y = spec.y0;
        island.w = spec.w;
        island.h = spec.h;
        const auto key = keys.find(spec.slot.v);
        island.template_key = key == keys.end() ? "embedded" : key->second;
        islands.push_back(std::move(island));
    }

    PlayLoad out;
    out.world = std::move(created).value();
    out.islands = std::move(islands);
    out.links = std::move(links);
    out.registry = registry.types;
    return out;
}

std::vector<sim::TypeId> buildable_types(const sim::TypeRegistry& registry,
                                         const sim::ListLayout& layout) {
    std::vector<sim::TypeId> out;
    for (const sim::BuildingType& type : registry.types()) {
        if (layout.list_for(type.prodtype).has_value() || (type.flags_word & sim::kRoadFlag) != 0) {
            out.push_back(type.id);
        }
    }
    std::sort(out.begin(), out.end(), [](sim::TypeId a, sim::TypeId b) { return a.v < b.v; });
    return out;
}

DevToolSetup dev_tool_setup(const PlayLoad& load, sim::PlayerId player) {
    DevToolSetup s;
    s.registry = load.registry;
    s.issuer = player;
    const sim::World& world = load.world;
    for (const auto& island : world.islands()) {
        if (!island || s.city) {
            continue;
        }
        for (const std::optional<sim::City>& city : island->cities) {
            if (city && city->owner == player && !s.city) {
                s.city = sim::CityRef{island->index, city->number};
                s.island = TileRect{island->tiles.x0(), island->tiles.y0(), island->tiles.width(),
                                    island->tiles.height()};
            }
        }
    }
    if (s.registry) {
        s.types = buildable_types(*s.registry, world.config().layout);
    }
    // A goods-free building of the tool's types and a free tile in another player's city, for the
    // scripted session's `foreign` step: the first plain type, the first tile away from the
    // island's edge that holds no building (World::building_at is the sim's own lookup).
    std::optional<sim::TypeId> plain;
    for (const bool square : {true, false}) { // a 2 x 2 first, else any goods-free non-road type
        for (const sim::TypeId id : s.types) {
            const sim::BuildingType* type = s.registry ? s.registry->find(id) : nullptr;
            if (!plain && type != nullptr && type->cost_goods.empty() &&
                (type->flags_word & sim::kRoadFlag) == 0 &&
                (!square || (type->width == 2 && type->height == 2))) {
                plain = id;
            }
        }
    }
    std::optional<sim::World> trial;
    for (const auto& island : world.islands()) {
        if (!island || !plain || s.foreign || !s.city || island->index == s.city->island) {
            continue;
        }
        for (const std::optional<sim::City>& city : island->cities) {
            if (!city || city->owner == player || s.foreign) {
                continue;
            }
            const sim::CityRef ref{island->index, city->number};
            const auto w = static_cast<std::int32_t>(island->tiles.width());
            const auto h = static_cast<std::int32_t>(island->tiles.height());
            for (std::int32_t y = 2; y + 4 <= h && !s.foreign; ++y) {
                for (std::int32_t x = 2; x + 4 <= w && !s.foreign; ++x) {
                    const sim::TileXY at{island->tiles.x0() + x, island->tiles.y0() + y};
                    if (world.building_at(ref, at.x, at.y) == nullptr) {
                        // The sim's placement rule refuses sloped ground (MAP-BUILD-1): the spot is
                        // one the city's own owner can build on, tried on a copy of the world (a
                        // refusal changes nothing, so one copy serves every try).
                        if (!trial) {
                            trial.emplace(world);
                        }
                        sim::BuildCmd build;
                        build.city = ref;
                        build.placements.push_back(sim::PlacementWire{*plain, at.x, at.y, 0, 0, 0});
                        if (trial->apply(sim::Command{city->owner, std::move(build)}).ok()) {
                            s.foreign = ForeignSpot{ref, city->owner, at, *plain};
                        }
                    }
                }
            }
        }
    }
    return s;
}

std::vector<SceneBuilding> play_buildings(const sim::World& world) {
    std::vector<SceneBuilding> out;
    const auto& islands = world.islands();
    for (std::size_t i = 0; i < islands.size(); ++i) {
        if (!islands[i]) {
            continue;
        }
        for (const std::optional<sim::City>& city : islands[i]->cities) {
            if (!city) {
                continue;
            }
            for (const sim::BuildingList& list : city->index.lists) {
                for (const std::optional<sim::Building>& building : list.slots) {
                    if (!building) {
                        continue;
                    }
                    // Building::x/y are world tiles (scenario_formats.cpp: x0 + the
                    // file's island-local item). The scene's objects are island-local.
                    const std::int32_t local_x = building->x - islands[i]->tiles.x0();
                    const std::int32_t local_y = building->y - islands[i]->tiles.y0();
                    if (local_x < 0 || local_y < 0 || local_x > 65535 || local_y > 65535) {
                        continue;
                    }
                    SceneBuilding placed;
                    placed.island = static_cast<std::uint32_t>(i);
                    placed.type = building->type.v;
                    placed.x = static_cast<std::uint16_t>(local_x);
                    placed.y = static_cast<std::uint16_t>(local_y);
                    placed.rotation = building->rotation;
                    placed.height = building->byte0f;
                    out.push_back(placed);
                }
            }
        }
    }
    return out;
}

bool anlage_is_building(const sim::World& world, std::uint16_t anlage) {
    // The scenario loader's split (scenario_formats.cpp): a PROD2 item whose
    // Anlagen type has a list in the layout is a building. The id on the tile
    // is the Anlagen entry, not the PRODTYPE number.
    const sim::BuildingType* type = world.types().find(sim::TypeId{anlage});
    if (type == nullptr) {
        return false;
    }
    return world.config().layout.list_for(type->prodtype).has_value();
}

PlayRunner::PlayRunner(sim::World world, unsigned speed_percent, PlaySchedule* schedule)
    : world_(std::move(world)), schedule_(schedule) {
    if (schedule_ == nullptr) {
        owned_schedule_ = std::make_unique<SteadySchedule>();
        schedule_ = owned_schedule_.get();
    }
    const auto step = sim::multiplayer_step_ms(speed_percent);
    step_ms_ = step.value_or(10);
    steps_ = sim::Timing{}.mp_steps_per_turn;
    snap_ = std::make_shared<const PlaySnapshot>();
    // The picture was built from these buildings at load, so the first diff starts here.
    buildings_ = play_buildings(world_);
    sort_buildings(buildings_);
    // The developer player's first city (the one the developer input builds in).
    for (std::size_t i = 0; i < world_.islands().size() && !developer_city_; ++i) {
        const auto& island = world_.islands()[i];
        if (!island) {
            continue;
        }
        for (const std::optional<sim::City>& city : island->cities) {
            if (city && city->owner == developer_player() && !developer_city_) {
                developer_city_ = sim::CityRef{island->index, city->number};
                has_city_.store(true);
            }
        }
    }
    // One second of turns. A window takes the mailbox once per frame, so it holds
    // about one frame's batches; a stalled or minimised window reaches the bound
    // after a second of turns and collapses to a resync (BuildingMailbox).
    const auto turn_ms = static_cast<std::size_t>(sim::Timing{}.mp_turn_real_ms);
    mailbox_ = BuildingMailbox(turn_ms == 0 ? 1 : std::max<std::size_t>(1, 1000 / turn_ms));
}

PlayRunner::~PlayRunner() {
    stop();
}

void PlayRunner::start() {
    if (thread_.joinable()) {
        return;
    }
    stop_.store(false);
    thread_ = std::thread([this] { loop(); });
}

void PlayRunner::stop() {
    stop_.store(true);
    if (schedule_ != nullptr) {
        schedule_->wake();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void PlayRunner::set_paused(bool paused) {
    paused_.store(paused);
}

void PlayRunner::set_turn_delay(std::chrono::milliseconds delay) {
    const auto ms = delay.count() < 0 ? 0 : delay.count();
    delay_ms_.store(static_cast<std::uint32_t>(ms > 60000 ? 60000 : ms));
}

void PlayRunner::set_catchup_turns(unsigned turns) {
    catchup_turns_ = turns;
}

void PlayRunner::run_n(std::uint32_t turns) {
    for (std::uint32_t i = 0; i < turns && simulation_error_.empty(); ++i) {
        one_turn();
    }
}

std::shared_ptr<const PlaySnapshot> PlayRunner::snapshot() const {
    std::lock_guard lock(snap_mu_);
    return snap_;
}

std::vector<PlayRunner::TurnHash> PlayRunner::recorded_hashes() const {
    const std::lock_guard lock(snap_mu_);
    return hashes_;
}

std::string PlayRunner::title_line() const {
    return title_of(*snapshot(), has_city_.load());
}

std::string PlayRunner::title_of(const PlaySnapshot& snapshot, bool money) {
    const PlaySnapshot* snap = &snapshot;
    char buf[192];
    std::snprintf(buf, sizeof buf, "tick %llu, game %llu ms, turn p95 %.1f ms",
                  static_cast<unsigned long long>(snap->tick),
                  static_cast<unsigned long long>(snap->game_ms),
                  static_cast<double>(snap->p95_us) / 1000.0);
    std::string line(buf);
    if (snap->behind_ms > 0) {
        line += ", behind " + std::to_string(snap->behind_ms) + " ms";
    }
    if (snap->paused) {
        line += ", paused";
    }
    if (money) {
        line += ", money " + std::to_string(snap->money);
    }
    if (!snap->last_rejection.empty()) {
        line += ", refused: " + snap->last_rejection;
    }
    return line;
}

void PlayRunner::loop() {
    // Absolute deadlines: turn k is due at t0 + k * mp_turn_real_ms
    // (docs/app/game-shell.md). Polling uses mp_step_real_ms so pause and
    // stop are noticed within one original step; each wake is aimed at the
    // deadline, not at "period minus what the last sleep was asked to take".
    const sim::Timing timing;
    const auto period_ms = static_cast<std::uint64_t>(timing.mp_turn_real_ms);
    const auto poll = std::chrono::milliseconds(timing.mp_step_real_ms);
    const unsigned bound = catchup_turns_;
    bool armed = false;
    bool pausing = false;
    PlaySchedule::time_point t0{};
    PlaySchedule::time_point pause_mark{};
    std::uint64_t k = 0;
    unsigned streak = 0;

    while (!stop_.load()) {
        if (paused_.load()) {
            if (!pausing) {
                pausing = true;
                pause_mark = schedule_->now();
                // The window title reads the snapshot. A pause publishes no
                // turn, so record it here or the title stays on the last turn.
                note_behind();
            }
            answer_inspection_while_paused();
            schedule_->sleep_for(poll);
            continue;
        }
        if (pausing) {
            if (armed) {
                t0 += schedule_->now() - pause_mark;
            }
            pausing = false;
            streak = 0;
        }
        if (!armed) {
            t0 = schedule_->now();
            k = 0;
            streak = 0;
            armed = true;
        }

        const auto deadline = deadline_at(t0, k, period_ms);
        const auto now = schedule_->now();
        if (now < deadline) {
            streak = 0;
            auto wake = deadline;
            const auto soon = now + poll;
            if (soon < wake) {
                wake = soon;
            }
            schedule_->sleep_until(wake);
            continue;
        }
        // Still late after `bound` back-to-back turns: skip to the first
        // deadline strictly after now and count the skipped slots.
        if (streak >= bound && now > deadline) {
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0);
            std::uint64_t next_k = k + 1;
            if (period_ms > 0 && elapsed_ms.count() > 0) {
                const auto elapsed = static_cast<std::uint64_t>(elapsed_ms.count());
                next_k = elapsed / period_ms + 1;
            }
            if (next_k <= k) {
                next_k = k + 1;
            }
            behind_ms_ += (next_k - k) * period_ms;
            k = next_k;
            streak = 0;
            note_behind();
            continue;
        }

        one_turn();
        if (stop_.load()) {
            break;
        }
        ++k;
        if (schedule_->now() >= deadline_at(t0, k, period_ms)) {
            ++streak;
        } else {
            streak = 0;
        }
    }
}

void PlayRunner::one_turn() {
    const auto poll = std::chrono::milliseconds(sim::Timing{}.mp_step_real_ms);
    auto left = std::chrono::milliseconds(delay_ms_.load());
    while (left.count() > 0 && !stop_.load()) {
        const auto slice = left > poll ? poll : left;
        schedule_->sleep_for(slice);
        left -= slice;
    }
    if (stop_.load()) {
        return;
    }
    sim::Turn turn;
    turn.index = index_++;
    turn.step_ms = step_ms_;
    // The script's commands for this turn, in file order (set_script sorted them by turn), then
    // the window's commands queued before this turn started, in push order. Which turn takes a
    // window command depends on wall time; its content and order do not (CommandQueue).
    std::vector<CommandId> ids;
    while (script_next_ < script_.size() && script_[script_next_].turn <= turn.index) {
        turn.commands.push_back(script_[script_next_].command);
        ids.push_back(CommandId{CommandOrigin::Script, script_next_ + 1});
        ++script_next_;
    }
    for (CommandQueue::Entry& entry : queue_.take()) {
        turn.commands.push_back(std::move(entry.command));
        ids.push_back(CommandId{CommandOrigin::Window, entry.n});
    }
    {
        std::lock_guard lock(snap_mu_);
        for (const sim::Command& command : turn.commands) {
            if (sim::has_wire_form(command.body)) {
                applied_.push_back({turn.index, command});
            }
        }
    }
    if (turn_gate_) {
        turn_gate_(turn.index); // test seam: the turn is in flight from here
    }
    const auto started = schedule_->now();
    // The order id a MoveCmd gets is the counter's value when it is applied: each accepted one
    // takes the next (ShipOrders::move), so the ids are known from the results below.
    std::uint64_t order_counter = world_.ids().next_order_id;
    // One execution path: the sim's own loop reports what each command did (round ee, A2).
    std::vector<sim::ApplyResult> results;
    simulation_error_ = sim::run_turn(world_, nullptr, turn, steps_, &results);
    if (!simulation_error_.empty()) {
        paused_.store(true);
    }
    // A turn that ran commands, and every kHashEvery-th turn: hashing the whole world each turn
    // slowed lan5's turns past their cadence in a debug build (round ee, E3's first run).
    if (record_hashes_ && (!turn.commands.empty() || turn.index % kHashEvery == 0)) {
        const std::uint64_t hash = world_.state_hash();
        const std::lock_guard lock(snap_mu_);
        hashes_.push_back({turn.index, hash});
    }
    const auto us =
        std::chrono::duration_cast<std::chrono::microseconds>(schedule_->now() - started).count();
    // The turn cost above is the sim's; the diff that publishes its changes is
    // timed separately (the bench reports both).
    // The sim's host events are drained (their refusals are in `results`, per command); the ships'
    // order events are kept for the opening's view.
    for (const sim::SimEvent& event : world_.drain_events()) {
        if (opening_) {
            ship_events_.append(event);
        }
    }
    std::vector<CommandOutcome> ran;
    ran.reserve(results.size());
    for (std::size_t i = 0; i < results.size(); ++i) {
        CommandOutcome o;
        o.id = ids[i];
        o.turn = turn.index;
        o.position = static_cast<std::uint32_t>(i);
        o.status = results[i].status;
        o.state = results[i].ok() ? CommandOutcome::State::Applied : CommandOutcome::State::Refused;
        o.records.reserve(results[i].records.size());
        for (const sim::RecordResult& r : results[i].records) {
            o.records.push_back(r.status);
            if (results[i].ok()) {
                o.transferred_88 += r.transferred_88;
                if (!o.created.valid() && r.building.valid()) {
                    o.created = r.building;
                }
            }
        }
        if (results[i].ok() && std::holds_alternative<sim::MoveCmd>(turn.commands[i].body)) {
            o.order_id = order_counter++;
        }
        ran.push_back(std::move(o));
    }
    publish(static_cast<std::uint64_t>(us < 0 ? 0 : us), turn.index, turn.commands.size(),
            building_batch(turn.index), std::move(ran));
    std::function<void(WorldCapture)> capture;
    {
        const std::lock_guard lock(capture_mu_);
        capture = std::exchange(capture_, {});
    }
    if (capture) {
        // After the publication: the capture is the state the newest snapshot describes.
        WorldCapture c;
        c.save = world_.save();
        c.hash = world_.state_hash();
        c.turn = turn.index;
        c.tick = world_.clock().tick();
        capture(std::move(c));
    }
}

bool PlayRunner::request_capture(std::function<void(WorldCapture)> done) {
    const std::lock_guard lock(capture_mu_);
    if (capture_) {
        return false;
    }
    capture_ = std::move(done);
    return true;
}

namespace {

// 8.8 fixed-point goods/output amount as tons, two decimals (W4-SETTLEMENT-1 W4: this package's
// own display convention for the developer inspection UI; no existing one to match, play.hpp's
// `PlaySnapshot::goods` comment just calls it "8.8 tons").
std::string tons(std::uint32_t raw_8_8) {
    const std::uint32_t whole = raw_8_8 / 256;
    const std::uint32_t frac = ((raw_8_8 % 256) * 100) / 256;
    return std::to_string(whole) + "." + (frac < 10 ? "0" : "") + std::to_string(frac) + "t";
}

std::string join_tons(const std::vector<std::uint32_t>& amounts) {
    std::string out;
    for (std::size_t i = 0; i < amounts.size(); ++i) {
        out += (i == 0 ? "" : ",") + tons(amounts[i]);
    }
    return out;
}

// One line per payload kind, the fields a developer watching production/worker activity cares
// about (output/raw/input stock, activity, workers/animals/carriers out). Inert payloads (every
// class without a ported tick) get no line: there is nothing live to show.
struct DescribePayload {
    std::string operator()(const sim::Inert&) const { return ""; }
    std::string operator()(const sim::Workshop& w) const {
        std::string activity;
        for (std::size_t i = 0; i < w.activity.size(); ++i) {
            activity += (i == 0 ? "" : ",") + std::to_string(w.activity[i]);
        }
        return "out=" + join_tons(w.out) + " in=" + join_tons(w.in) + " activity=" + activity +
               " carriers_out=" + std::to_string(w.carriers_out) +
               (w.suspended ? " suspended" : "") + (w.blocked ? " blocked" : "");
    }
    std::string operator()(const sim::WeaponsWorkshop& w) const {
        std::string activity;
        for (std::size_t i = 0; i < w.activity.size(); ++i) {
            activity += (i == 0 ? "" : ",") + std::to_string(w.activity[i]);
        }
        return "out=" + join_tons(w.out) + " in=" + join_tons(w.in) + " activity=" + activity +
               " carriers_out=" + std::to_string(w.carriers_out) +
               (w.suspended ? " suspended" : "") + (w.blocked ? " blocked" : "");
    }
    std::string operator()(const sim::Farm& f) const {
        return "out=" + tons(f.out) + " raw=" + tons(f.raw) +
               " activity=" + std::to_string(f.activity) +
               " workers_out=" + std::to_string(f.workers_out) + (f.suspended ? " suspended" : "") +
               (f.blocked ? " blocked" : "");
    }
    std::string operator()(const sim::SmallFarm& f) const {
        // activity and figures_out are plain counters (a u16 average, a per-line/animal-kind
        // headcount), not 8.8 goods amounts — only out/raw are tons (production.hpp comments).
        std::string activity, figures;
        for (std::size_t i = 0; i < f.activity.size(); ++i) {
            activity += (i == 0 ? "" : ",") + std::to_string(f.activity[i]);
        }
        for (std::size_t i = 0; i < f.figures_out.size(); ++i) {
            figures += (i == 0 ? "" : ",") + std::to_string(f.figures_out[i]);
        }
        return "out=" + join_tons(f.out) + " raw=" + join_tons(f.raw) + " activity=" + activity +
               " figures_out=" + figures +
               " status_code=" + std::to_string(static_cast<int>(f.status.code)) +
               (f.suspended ? " suspended" : "") + (f.blocked ? " blocked" : "");
    }
    std::string operator()(const sim::Mine& m) const {
        return "out=" + tons(m.out) + " activity=" + std::to_string(m.activity) +
               " deposit=" + (m.deposit ? std::to_string(*m.deposit) : "none") +
               (m.blocked ? " blocked" : "");
    }
    std::string operator()(const sim::Market&) const { return "(no stock of its own; see city)"; }
    std::string operator()(const sim::House& h) const {
        // h.stock is the house population formula's satisfaction value (trunc(Menge*s*residents*
        // 128/1536), docs/re/population.md §2), not an 8.8 goods amount: plain numbers, no "t".
        std::string stock;
        for (std::size_t i = 0; i < h.stock.size(); ++i) {
            stock += (i == 0 ? "" : ",") + std::to_string(h.stock[i]);
        }
        return "residents=" + std::to_string(h.residents_64 / 64) + " stock=" + stock;
    }
    std::string operator()(const sim::Inn& i) const {
        return "tavern=" + std::to_string(i.tavern) + " taken=" + std::to_string(i.taken) +
               " carriers_out=" + std::to_string(i.carriers_out);
    }
};

std::string payload_kind_name(const sim::Payload& payload) {
    return std::visit(
        [](const auto& p) -> std::string {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, sim::Inert>) {
                return "Inert";
            } else if constexpr (std::is_same_v<T, sim::Workshop>) {
                return "Workshop";
            } else if constexpr (std::is_same_v<T, sim::WeaponsWorkshop>) {
                return "WeaponsWorkshop";
            } else if constexpr (std::is_same_v<T, sim::Farm>) {
                return "Farm";
            } else if constexpr (std::is_same_v<T, sim::SmallFarm>) {
                return "SmallFarm";
            } else if constexpr (std::is_same_v<T, sim::Mine>) {
                return "Mine";
            } else if constexpr (std::is_same_v<T, sim::Market>) {
                return "Market";
            } else if constexpr (std::is_same_v<T, sim::House>) {
                return "House";
            } else {
                static_assert(std::is_same_v<T, sim::Inn>,
                              "a Payload alternative without a name here (building.hpp changed)");
                return "Inn";
            }
        },
        payload);
}

} // namespace

std::optional<BuildingInspection> inspect_building(const sim::World& world, sim::CityRef city,
                                                   sim::TileXY tile) {
    const sim::Building* b = world.building_at(city, tile.x, tile.y);
    if (b == nullptr) {
        return std::nullopt;
    }
    BuildingInspection out;
    out.tile = tile;
    out.id = b->id;
    out.type = b->type;
    if (const sim::BuildingType* t = world.types().find(b->type)) {
        out.prodtype = t->prodtype;
    }
    out.kind = payload_kind_name(b->payload);
    out.status = std::visit(DescribePayload{}, b->payload);
    return out;
}

std::uint64_t facts_turn_for(const PlaySnapshot* opening_snapshot,
                             const PlaySnapshot* live_snapshot) {
    if (opening_snapshot != nullptr) {
        return opening_snapshot->turn;
    }
    return live_snapshot != nullptr ? live_snapshot->turn : 0;
}

const BuildingInspection* live_inspection(const PlaySnapshot& snapshot, bool dev_enabled,
                                          std::optional<sim::TileXY> hover) {
    if (!dev_enabled || !snapshot.inspection || !hover || snapshot.inspection->tile != *hover) {
        return nullptr;
    }
    return snapshot.inspection.get();
}

std::shared_ptr<const BuildingBatch> PlayRunner::building_batch(std::uint64_t turn) {
    std::vector<SceneBuilding> cur = play_buildings(world_);
    sort_buildings(cur);
    std::vector<BuildingDelta> deltas = diff_sorted_buildings(buildings_, cur);
    buildings_ = std::move(cur);
    if (deltas.empty()) {
        return nullptr;
    }
    auto batch = std::make_shared<BuildingBatch>();
    batch->turn = turn;
    batch->deltas = std::move(deltas);
    return batch;
}

void PlayRunner::publish(std::uint64_t turn_us, std::uint64_t turn, std::size_t applied,
                         std::shared_ptr<const BuildingBatch> batch,
                         std::vector<CommandOutcome> ran) {
    const auto now = std::chrono::steady_clock::now();
    costs_.push_back({now, turn_us});
    const auto cut = now - std::chrono::seconds(1);
    while (!costs_.empty() && costs_.front().first < cut) {
        costs_.erase(costs_.begin());
    }
    auto next = std::make_shared<PlaySnapshot>();
    next->tick = world_.clock().tick();
    next->game_ms = world_.clock().ms;
    next->last_turn_us = turn_us;
    next->p95_us = p95_locked();
    next->behind_ms = behind_ms_;
    next->paused = paused_.load();
    next->turn = turn;
    // The local participant's money and its city's goods (copied; the window never reads the
    // World). The city is looked for again while there is none: a ship start founds it later.
    if (opening_) {
        OpeningView view = collect_opening_view(world_, local_player_, ship_events_, turn);
        std::optional<sim::FoundCmd> probe;
        {
            const std::lock_guard lock(probe_mu_);
            probe = probe_;
        }
        if (probe) {
            view.found_probe = probe_founding(world_, local_player_, *probe);
        }
        next->opening = std::make_shared<const OpeningView>(std::move(view));
        // The participant's city is the first of its cities in (island, city) order, every turn:
        // the same one the window's building tool follows (OpeningView::cities.front()).
        if (next->opening && !next->opening->cities.empty()) {
            developer_city_ = next->opening->cities.front().ref;
        } else {
            developer_city_.reset();
        }
        has_city_.store(developer_city_.has_value());
    } else if (!developer_city_) {
        find_local_city();
    }
    fill_inspection(*next); // also without a city: the requested tile is answered as empty
    if (developer_city_) {
        if (const sim::Player* player = world_.players().find(local_player_)) {
            next->money = player->money;
            next->costs_waived = player->free_build || world_.rules().free_build_cheat;
        }
        const auto& island = world_.islands()[developer_city_->island.v];
        if (island && developer_city_->city.v < island->cities.size() &&
            island->cities[developer_city_->city.v]) {
            next->goods = island->cities[developer_city_->city.v]->stock.stock;
        }
        // UI.2 C2: the construction menu's lock display reads this, never
        // World::construction_eligibility directly (that World belongs to this thread, not the
        // window's) and never data/ui/construction.txt's recorded state (RE evidence about the
        // original, not a live rule). A pure read (rule 0.2): no state, stream, hash or draw
        // changes.
        auto elig = std::make_shared<std::vector<std::pair<sim::TypeId, bool>>>();
        elig->reserve(world_.types().types().size());
        for (const sim::BuildingType& t : world_.types().types()) {
            elig->emplace_back(t.id,
                               world_.construction_eligibility(local_player_, t.id).allowed());
        }
        next->eligibility = std::move(elig);
        // H2 (the HUD's residents counter): World::city_population, a new read-only sim query
        // (rule 0.2 - no state, stream, hash or draw change; residents_at_or_above's own
        // existing aggregate, house_transition.hpp, wrapped for a CityRef). Same cross-thread-
        // avoidance reason as eligibility above: computed here, on the sim thread, never read
        // from the window's own copy of World.
        next->population = world_.city_population(*developer_city_);
    }
    for (const CommandOutcome& o : ran) {
        if (o.state == CommandOutcome::State::Refused) {
            // Attributed (round ee review of C, P2-1): the title must not let an earlier
            // command's refusal pass for a later one's.
            last_reason_ = std::string(sim::reject_name(o.status)) + " (" +
                           (o.id.origin == CommandOrigin::Script ? "script" : "window") +
                           " command #" + std::to_string(o.id.n) + ")";
            last_reason_turn_ = o.turn;
        }
    }
    if (!last_reason_.empty() && turn >= last_reason_turn_ + kRejectionShowTurns) {
        last_reason_.clear();
    }
    next->last_rejection = simulation_error_.empty() ? last_reason_ : simulation_error_;
    // One publication: the batch goes into the mailbox and the snapshot is swapped
    // under the same lock, batch first. A reader that sees snapshot N therefore
    // sees turn N's batch already waiting (W4.3, orchestrator correction 2).
    // A collapse copies the list before the lock (W4.3 A8). Between this check and the
    // push only the window can change the mailbox, and it can only take the waiting
    // batches (empty it); the sim thread is the only pusher. So a copy made for a
    // collapse can be unused, when the window emptied the mailbox in the gap. That copy
    // is then destroyed while the lock is held: its cost is the list's size, bounded by
    // the island count, and the test "a prepared copy made when full is unused" covers the
    // interleaving (round du, step 6).
    std::vector<SceneBuilding> full_list;
    bool may_collapse = false;
    {
        std::lock_guard lock(snap_mu_);
        may_collapse = mailbox_.full();
    }
    if (may_collapse) {
        full_list = buildings_;
    }
    const bool pushed = batch != nullptr;
    // The outcomes are recorded before the snapshot that follows them is published, so a reader
    // that sees the snapshot finds them (outcomes_next).
    for (CommandOutcome& o : ran) {
        outcomes_.record(o);
    }
    next->outcomes_next = outcomes_.next();
    if (publish_gate_) {
        publish_gate_(turn); // the outcomes are readable, the snapshot is not yet (R1's test)
    }
    // The window trace's record of the turn (opt-in), filled before the batch and the refusals
    // move into the mailbox.
    std::optional<TurnObservation> observed;
    if (turn_observer_) {
        observed.emplace();
        observed->turn = turn;
        observed->applied = applied;
        for (const CommandOutcome& o : ran) {
            if (o.state == CommandOutcome::State::Refused) {
                ++observed->rejected;
                observed->reasons += (observed->reasons.empty() ? "" : ",") +
                                     std::string(sim::reject_name(o.status));
            }
        }
        observed->batch = pushed;
        observed->deltas = batch ? batch->deltas.size() : 0;
        observed->published = std::chrono::steady_clock::now();
    }
    {
        std::lock_guard lock(snap_mu_);
        if (batch) {
            mailbox_.push_prepared(std::move(batch), std::move(full_list));
            batches_waiting_.store(true);
        }
        snap_ = std::move(next);
    }
    if (pushed || !ran.empty() || opening_) {
        // The window's wake-up, after the publication (outside the snapshot lock). With an
        // opening every turn moves the ships.
        const std::lock_guard lock(listener_mu_);
        if (listener_) {
            listener_();
        }
    }
    if (observed) {
        turn_observer_(*observed);
    }
}

void PlayRunner::set_wake_listener(std::function<void()> listener) {
    const std::lock_guard lock(listener_mu_);
    listener_ = std::move(listener);
}

void PlayRunner::set_turn_gate(std::function<void(std::uint64_t)> gate) {
    turn_gate_ = std::move(gate);
}

void PlayRunner::set_publish_gate(std::function<void(std::uint64_t)> gate) {
    publish_gate_ = std::move(gate);
}

PlayFeedback PlayRunner::feedback(std::optional<CommandId> receipt) const {
    // The snapshot first: its watermark (outcomes_next) bounds which outcomes may be shown with
    // it. Reading the outcome first and the snapshot second cannot give that, because the sim
    // thread records outcomes before it swaps the snapshot (publish).
    PlayFeedback read;
    read.snapshot = snapshot();
    if (receipt) {
        read.latest = outcomes_.find_published(*receipt, read.snapshot->outcomes_next);
    }
    return read;
}

void PlayRunner::set_turn_observer(std::function<void(const TurnObservation&)> observer) {
    turn_observer_ = std::move(observer);
}

std::vector<std::shared_ptr<const BuildingBatch>> PlayRunner::take_building_batches() {
    std::lock_guard lock(snap_mu_);
    batches_waiting_.store(false);
    return mailbox_.take();
}

std::vector<std::uint64_t> PlayRunner::pending_batch_turns() const {
    std::lock_guard lock(snap_mu_);
    std::vector<std::uint64_t> turns;
    for (const auto& batch : mailbox_.waiting()) {
        turns.push_back(batch->turn);
    }
    return turns;
}

void PlayRunner::set_batch_bound(std::size_t bound) {
    mailbox_ = BuildingMailbox(bound);
}

void PlayRunner::set_script(std::vector<ScriptedCommand> script) {
    std::stable_sort(
        script.begin(), script.end(),
        [](const ScriptedCommand& a, const ScriptedCommand& b) { return a.turn < b.turn; });
    script_ = std::move(script);
    script_next_ = 0;
    outcomes_.issued(CommandOrigin::Script, script_.size());
}

CommandId PlayRunner::post_command(sim::Command command) {
    const CommandId id{CommandOrigin::Window, queue_.push(std::move(command))};
    outcomes_.issued(CommandOrigin::Window, id.n);
    return id;
}

void PlayRunner::set_outcome_bounds(std::size_t outcomes, std::size_t records) {
    outcomes_.set_bounds(outcomes, records);
}

std::vector<ScriptedCommand> PlayRunner::applied_commands() const {
    std::lock_guard lock(snap_mu_);
    return applied_;
}

void PlayRunner::note_behind() {
    auto next = std::make_shared<PlaySnapshot>(*snapshot());
    next->behind_ms = behind_ms_;
    next->paused = paused_.load();
    publish_snapshot(std::move(next));
}

void PlayRunner::fill_inspection(PlaySnapshot& next) {
    next.inspection.reset();
    next.inspected.reset();
    std::optional<sim::TileXY> inspect_tile;
    {
        const std::lock_guard lock(inspect_mu_);
        inspect_tile = inspect_tile_;
    }
    if (!inspect_tile) {
        return;
    }
    // Answered even without a city (then never a building): a click must resolve, not wait.
    next.inspected = inspect_tile;
    if (!developer_city_) {
        return;
    }
    if (auto found = inspect_building(world_, *developer_city_, *inspect_tile)) {
        next.inspection = std::make_shared<const BuildingInspection>(std::move(*found));
    }
}

void PlayRunner::find_local_city() {
    for (std::size_t i = 0; i < world_.islands().size() && !developer_city_; ++i) {
        const auto& island = world_.islands()[i];
        if (!island) {
            continue;
        }
        for (const std::optional<sim::City>& city : island->cities) {
            if (city && city->owner == local_player_ && !developer_city_) {
                developer_city_ = sim::CityRef{island->index, city->number};
                has_city_.store(true);
            }
        }
    }
}

void PlayRunner::answer_inspection_while_paused() {
    if (!developer_city_) {
        find_local_city(); // set_local_player before the first turn: the turn would look it up
    }
    std::optional<sim::TileXY> requested;
    {
        const std::lock_guard lock(inspect_mu_);
        requested = inspect_tile_;
    }
    const auto current = snapshot();
    if (!current || current->inspected == requested) {
        return;
    }
    auto next = std::make_shared<PlaySnapshot>(*current);
    fill_inspection(*next);
    publish_snapshot(std::move(next));
}

void PlayRunner::publish_snapshot(std::shared_ptr<const PlaySnapshot> snapshot) {
    std::lock_guard lock(snap_mu_);
    snap_ = std::move(snapshot);
}

std::uint64_t PlayRunner::p95_locked() const {
    std::vector<std::uint64_t> values;
    values.reserve(costs_.size());
    for (const auto& [when, us] : costs_) {
        (void)when;
        values.push_back(us);
    }
    return percentile_95(std::move(values));
}

} // namespace anno::app
