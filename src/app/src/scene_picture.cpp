// ScenePicture (docs/app/game-shell.md, "Immutable per-island object lists"). See the header.
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>

#include <anno/app/scene_picture.hpp>

namespace anno::app {

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

/// Every object of a scene generation's island, in one sorted list (below, above and other
/// partition the island's objects; the enhanced and river lists are subsets of them).
std::vector<render::Prepared> all_objects(const render::IslandLists& lists) {
    std::vector<render::Prepared> out;
    out.reserve(lists.below.size() + lists.above.size() + lists.other.size());
    out.insert(out.end(), lists.below.begin(), lists.below.end());
    out.insert(out.end(), lists.above.begin(), lists.above.end());
    out.insert(out.end(), lists.other.begin(), lists.other.end());
    std::sort(out.begin(), out.end());
    return out;
}

/// The chunk keys (every zoom level) that the objects of `objects` touch in `scene`, at each
/// built group: the screen box of each object at the group, grown to every derived level.
void add_keys(const render::Scene& scene, std::size_t index,
              const std::vector<render::Prepared>& objects, std::set<render::ChunkKey>& keys) {
    for (const render::Prepared& o : objects) {
        for (std::size_t group = 0; group < render::kGroups; ++group) {
            const auto box = scene.object_bounds(index, o, group);
            if (!box) {
                continue;
            }
            for (const render::ChunkKey& key : render::chunk_keys_touching(scene, *box, group)) {
                keys.insert(key);
            }
        }
    }
}

} // namespace

/// State shared by the window thread and the workers. Everything below `mutex` is guarded by it.
struct ScenePicture::State {
    struct Job {
        std::uint64_t seq = 0;
        std::size_t index = 0;
        std::shared_ptr<const render::Scene> base;
        std::vector<SceneBuilding> buildings;
    };
    struct Result {
        std::uint64_t seq = 0;
        std::size_t index = 0;
        /// The island with its records after the change; every byte it points into is in
        /// `storage` (copied by the worker), so the island needs no other owner.
        formats::insel::Island island;
        std::shared_ptr<const std::vector<std::vector<std::uint8_t>>> storage;
        std::shared_ptr<const render::IslandLists> lists;
    };
    std::mutex mutex;
    std::condition_variable wake, idle;
    std::deque<Job> jobs;
    std::size_t running = 0;
    std::vector<Result> done;
    /// Per island index: the newest request's sequence (0: none) and whether it is not yet
    /// committed.
    std::vector<std::uint64_t> latest;
    std::vector<bool> open;
    std::uint64_t next_seq = 1;
    bool stop = false;
    /// Anlagen ids below 0x1000 that are buildings (the loader's rule, asked once).
    std::shared_ptr<const std::vector<bool>> is_building;
    Stats stats;
};

/// Releases retired generations on its own thread: the last reference to a generation can be
/// the window thread's, and freeing a generation is not free (ScenePicture, step 7).
struct ScenePicture::Reaper {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::shared_ptr<const render::Scene>> queue;
    bool stop = false;
    std::thread thread;

    void retire(std::shared_ptr<const render::Scene> scene) {
        {
            const std::lock_guard lock(mutex);
            queue.push_back(std::move(scene));
        }
        wake.notify_one();
    }
    void run() {
        for (;;) {
            std::shared_ptr<const render::Scene> dead;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [this] { return stop || !queue.empty(); });
                if (queue.empty()) {
                    return; // stopping, and nothing left to release
                }
                dead = std::move(queue.front());
                queue.pop_front();
            }
            // `dead` is released here, outside the lock and off the window thread.
        }
    }
};

ScenePicture::ScenePicture(std::shared_ptr<const render::Scene> base,
                           std::shared_ptr<const void> owner, std::vector<SceneBuilding> buildings,
                           const std::function<bool(std::uint16_t)>& is_building,
                           std::size_t threads)
    : owner_(std::move(owner)), current_(std::move(base)), picture_(std::move(buildings)),
      state_(std::make_shared<State>()), reaper_(std::make_shared<Reaper>()) {
    for (std::size_t i = 0; i < current_->islands.size(); ++i) {
        slots_.emplace_back(current_->islands[i].number, i);
    }
    std::sort(slots_.begin(), slots_.end());
    auto table = std::make_shared<std::vector<bool>>(0x1000, false);
    for (std::size_t id = 0; id < table->size(); ++id) {
        (*table)[id] = is_building(static_cast<std::uint16_t>(id));
    }
    state_->is_building = std::move(table);
    state_->latest.assign(current_->islands.size(), 0);
    state_->open.assign(current_->islands.size(), false);
    for (std::size_t t = 0; t < std::max<std::size_t>(threads, 1); ++t) {
        threads_.emplace_back([st = state_] {
            for (;;) {
                State::Job job;
                {
                    std::unique_lock lock(st->mutex);
                    st->wake.wait(lock, [&] { return st->stop || !st->jobs.empty(); });
                    if (st->stop) {
                        return;
                    }
                    job = std::move(st->jobs.front());
                    st->jobs.pop_front();
                    ++st->running;
                }
                const auto start = Clock::now();
                // Immutable inputs only: the island as the job's generation has it, the Anlagen
                // file of that generation, and the job's building list.
                State::Result result;
                result.seq = job.seq;
                result.index = job.index;
                result.storage = nullptr;
                auto storage = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
                formats::insel::Island island = job.base->islands[job.index];
                // Own every byte the island points into before the rewrite, so no
                // generation's buffers are needed afterwards.
                std::vector<std::string> notes;
                std::vector<OmittedObject> omitted;
                {
                    std::vector<std::uint8_t> head(island.head.begin(), island.head.end());
                    storage->push_back(std::move(head));
                    island.head = formats::Bytes(storage->back().data(), storage->back().size());
                    for (formats::chunk::Chunk& rec : island.records) {
                        std::vector<std::uint8_t> payload(rec.payload.begin(), rec.payload.end());
                        storage->push_back(std::move(payload));
                        rec.payload =
                            formats::Bytes(storage->back().data(), storage->back().size());
                    }
                }
                SceneObjects objects;
                const auto table = st->is_building;
                objects.is_building = [table](std::uint16_t id) {
                    return id < table->size() && (*table)[id];
                };
                apply_island_buildings(island, job.buildings, objects, *storage, notes, omitted);
                auto lists = render::Scene::build_island_lists(island, job.base->anlagen);
                result.island = std::move(island);
                result.storage = std::move(storage);
                result.lists = std::move(lists);
                const double ms = ms_since(start);
                job.base.reset();
                {
                    const std::lock_guard lock(st->mutex);
                    --st->running;
                    st->stats.last_build_ms = ms;
                    if (st->latest[result.index] == result.seq && st->open[result.index]) {
                        st->done.push_back(std::move(result));
                    } else {
                        ++st->stats.discarded;
                    }
                    if (st->jobs.empty() && st->running == 0) {
                        st->idle.notify_all();
                    }
                }
            }
        });
    }
    reaper_->thread = std::thread([r = reaper_] { r->run(); });
}

ScenePicture::~ScenePicture() {
    {
        const std::lock_guard lock(state_->mutex);
        state_->stop = true;
    }
    state_->wake.notify_all();
    for (std::thread& t : threads_) {
        t.join();
    }
    {
        const std::lock_guard lock(reaper_->mutex);
        reaper_->stop = true;
    }
    reaper_->wake.notify_all();
    reaper_->thread.join();
}

std::size_t ScenePicture::apply(const std::vector<std::shared_ptr<const BuildingBatch>>& batches) {
    std::set<std::uint32_t> touched;
    for (const auto& batch : batches) {
        if (!batch) {
            continue;
        }
        if (batch->resync) {
            // Every island that had buildings, or has them now, may have changed.
            for (const SceneBuilding& b : picture_) {
                touched.insert(b.island);
            }
            for (const SceneBuilding& b : batch->full) {
                touched.insert(b.island);
            }
            picture_ = batch->full;
            continue;
        }
        std::vector<std::shared_ptr<const BuildingBatch>> one{batch};
        const ApplyResult applied = apply_batches(picture_, one);
        for (const BuildingDelta& d : applied.deltas) {
            touched.insert(d.building.island);
        }
    }
    std::size_t requested = 0;
    for (const std::uint32_t slot : touched) {
        const auto it = std::lower_bound(slots_.begin(), slots_.end(),
                                         std::pair<std::uint32_t, std::size_t>{slot, 0});
        if (it == slots_.end() || it->first != slot) {
            continue; // a building of an island the scene does not draw
        }
        const std::size_t index = it->second;
        State::Job job;
        job.index = index;
        job.base = current_;
        for (const SceneBuilding& b : picture_) {
            if (b.island == slot) {
                job.buildings.push_back(b);
            }
        }
        {
            const std::lock_guard lock(state_->mutex);
            job.seq = state_->next_seq++;
            state_->latest[index] = job.seq;
            state_->open[index] = true;
            // A newer request makes the queued one for this island pointless.
            std::erase_if(state_->jobs, [index](const State::Job& q) { return q.index == index; });
            state_->jobs.push_back(std::move(job));
        }
        state_->wake.notify_one();
        ++requested;
    }
    return requested;
}

std::size_t ScenePicture::commit(render::FrameRenderer& frames) {
    std::vector<State::Result> ready;
    {
        const std::lock_guard lock(state_->mutex);
        for (State::Result& r : state_->done) {
            if (state_->latest[r.index] == r.seq && state_->open[r.index]) {
                state_->open[r.index] = false;
                ready.push_back(std::move(r));
            } else {
                ++state_->stats.discarded;
            }
        }
        state_->done.clear();
    }
    if (ready.empty()) {
        return 0;
    }
    const auto start = Clock::now();
    const std::shared_ptr<const render::Scene> old = current_;
    // What changed: the objects of each island that the new lists lack (removed) or add.
    std::set<render::ChunkKey> keys;
    std::vector<render::Scene::IslandUpdate> updates;
    updates.reserve(ready.size());
    for (State::Result& r : ready) {
        const std::vector<render::Prepared> before = all_objects(old->island_lists(r.index));
        const std::vector<render::Prepared> after = all_objects(*r.lists);
        std::vector<render::Prepared> removed, added;
        std::set_difference(before.begin(), before.end(), after.begin(), after.end(),
                            std::back_inserter(removed));
        std::set_difference(after.begin(), after.end(), before.begin(), before.end(),
                            std::back_inserter(added));
        // The boxes of both sets are read from the old generation: layouts, Anlagen and lift
        // are the same in every generation of the scene.
        add_keys(*old, r.index, removed, keys);
        add_keys(*old, r.index, added, keys);
        render::Scene::IslandUpdate u;
        u.index = r.index;
        u.island = std::move(r.island);
        u.lists = std::move(r.lists);
        u.storage = std::move(r.storage);
        updates.push_back(std::move(u));
    }
    std::shared_ptr<const render::Scene> next = old->with_islands(std::move(updates));
    std::vector<render::ChunkKey> invalid(keys.begin(), keys.end());
    frames.invalidate(invalid, next->generation());
    current_ = next;
    reaper_->retire(old);
    {
        const std::lock_guard lock(state_->mutex);
        state_->stats.committed += ready.size();
        state_->stats.invalidated_keys += invalid.size();
        state_->stats.last_commit_ms = ms_since(start);
    }
    return ready.size();
}

void ScenePicture::wait_idle() {
    std::unique_lock lock(state_->mutex);
    state_->idle.wait(lock, [this] { return state_->jobs.empty() && state_->running == 0; });
}

std::size_t ScenePicture::pending() const {
    const std::lock_guard lock(state_->mutex);
    return static_cast<std::size_t>(std::count(state_->open.begin(), state_->open.end(), true));
}

ScenePicture::Stats ScenePicture::stats() const {
    const std::lock_guard lock(state_->mutex);
    return state_->stats;
}

} // namespace anno::app
