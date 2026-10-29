#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <thread>
#include <utility>

#include <anno/app/cli.hpp>
#include <anno/app/play.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/sim_load/scenario_load.hpp>

namespace anno::app {

namespace {

namespace fs = std::filesystem;

std::string path_text(const fs::path& p) {
    return to_utf8(p);
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

formats::Result<PlayLoad, std::string> load_play(const fs::path& game_dir, const fs::path& file) {
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
    const sim::SimConfig cfg;
    auto resolved =
        anno::sim_load::load_scenario_with_links(buf, templates, *registry.types, cfg, {});
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
    return out;
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
                    if (local_x < 0 || local_y < 0 || local_x > 65535 || local_y > 65535 ||
                        building->type.v > 0x0FFF) {
                        continue;
                    }
                    SceneBuilding placed;
                    placed.island = static_cast<std::uint32_t>(i);
                    placed.type = static_cast<std::uint16_t>(building->type.v);
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
    for (std::uint32_t i = 0; i < turns; ++i) {
        one_turn();
    }
}

std::shared_ptr<const PlaySnapshot> PlayRunner::snapshot() const {
    std::lock_guard lock(snap_mu_);
    return snap_;
}

std::string PlayRunner::title_line() const {
    const auto snap = snapshot();
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
            }
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
    const auto started = schedule_->now();
    sim::run_turn(world_, nullptr, turn, steps_);
    const auto us =
        std::chrono::duration_cast<std::chrono::microseconds>(schedule_->now() - started).count();
    publish(static_cast<std::uint64_t>(us < 0 ? 0 : us));
}

void PlayRunner::publish(std::uint64_t turn_us) {
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
    publish_snapshot(std::move(next));
}

void PlayRunner::note_behind() {
    auto next = std::make_shared<PlaySnapshot>(*snapshot());
    next->behind_ms = behind_ms_;
    next->paused = paused_.load();
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
