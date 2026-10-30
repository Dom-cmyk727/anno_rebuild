// ScenePicture (docs/app/game-shell.md, "Immutable per-island object lists" and "Retained work").
// See the header. Ownership rules: the window thread drops nothing it holds the last reference to;
// everything it gives up goes to the reaper (Dropped), and so do the jobs and results the
// destructor still holds. Workers may drop what they hold: they are not the window thread.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <anno/app/scene_picture.hpp>

namespace anno::app {

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

/// What the window thread hands to the reaper: any shared object it gives up.
using Dropped = std::vector<std::shared_ptr<const void>>;

/// Every object of an island's lists, in one sorted list (below, above and other partition the
/// island's objects; the enhanced and river lists are subsets of them).
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

/// The objects that appear and disappear between two lists of one island, and the chunk keys
/// they touch. Layouts, Anlagen and lift are the same in every generation, so `scene` may be any
/// generation of the island's scene. Pure: safe on any thread.
struct ChangeSet {
    std::vector<render::Prepared> removed, added;
    std::vector<render::ChunkKey> keys;
};

ChangeSet change_set(const render::Scene& scene, std::size_t index,
                     const render::IslandLists& before, const render::IslandLists& after) {
    ChangeSet change;
    const std::vector<render::Prepared> old_objects = all_objects(before);
    const std::vector<render::Prepared> new_objects = all_objects(after);
    std::set_difference(old_objects.begin(), old_objects.end(), new_objects.begin(),
                        new_objects.end(), std::back_inserter(change.removed));
    std::set_difference(new_objects.begin(), new_objects.end(), old_objects.begin(),
                        old_objects.end(), std::back_inserter(change.added));
    std::set<render::ChunkKey> keys;
    add_keys(scene, index, change.removed, keys);
    add_keys(scene, index, change.added, keys);
    change.keys.assign(keys.begin(), keys.end());
    return change;
}

/// The buildings of the given island slots, per slot, in the order of `all` (one pass).
std::map<std::uint32_t, std::vector<SceneBuilding>>
buildings_by_island(const std::vector<SceneBuilding>& all, const std::set<std::uint32_t>& slots) {
    std::map<std::uint32_t, std::vector<SceneBuilding>> out;
    for (const SceneBuilding& b : all) {
        if (slots.contains(b.island)) {
            out[b.island].push_back(b);
        }
    }
    return out;
}

/// One request for an island's rebuild: the generation it is made from and the buildings it
/// applies.
struct PictureJob {
    std::uint64_t seq = 0;
    std::size_t index = 0;
    std::shared_ptr<const render::Scene> base;
    std::vector<SceneBuilding> buildings;
};

/// The result of a rebuild: the island with its records after the change, its lists, and the change
/// set taken against the lists the rebuild started from.
struct PictureResult {
    std::uint64_t seq = 0;
    std::size_t index = 0;
    /// The island with its records after the change; every byte it points into is in
    /// `storage` (copied by the worker), so the island needs no other owner.
    formats::insel::Island island;
    std::shared_ptr<const std::vector<std::vector<std::uint8_t>>> storage;
    std::shared_ptr<const render::IslandLists> lists;
    /// The lists the change set was taken from: the version the commit checks.
    std::shared_ptr<const render::IslandLists> base_lists;
    ChangeSet change;
};

} // namespace

/// State shared by the window thread and the workers. Everything below `mutex` is guarded by it.
struct ScenePicture::State {
    using Job = PictureJob;
    using Result = PictureResult;
    std::mutex mutex;
    std::condition_variable wake, idle;
    std::deque<Job> jobs;
    std::size_t running = 0;
    /// Finished results: at most one per island (a newer result of an island replaces it).
    std::vector<Result> done;
    /// Per island index: the newest request's sequence (0: none) and whether it is not yet
    /// committed (outstanding). `outstanding` counts the open islands.
    std::vector<std::uint64_t> latest;
    std::vector<bool> open;
    std::size_t outstanding = 0;
    /// Islands whose request waits for a free outstanding slot, in the order they waited.
    std::deque<std::size_t> deferred;
    std::vector<bool> is_deferred;
    std::uint64_t next_seq = 1;
    bool stop = false;
    /// Anlagen ids below 0x1000 that are buildings (the loader's rule, asked once).
    std::shared_ptr<const std::vector<bool>> is_building;
    Stats stats;
};

/// Releases what the window thread gave up, on its own thread: the last reference to a generation
/// (or a job, or a result) can be the reaper's, and freeing them is not free (ScenePicture, step
/// 7).
struct ScenePicture::Reaper {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::shared_ptr<const void>> queue;
    std::size_t peak = 0;
    bool stop = false;
    std::thread thread;
    std::function<void()> gate;
    std::atomic<std::uint64_t> reclaimed{0};

    void retire(Dropped items) {
        if (items.empty()) {
            return;
        }
        {
            const std::lock_guard lock(mutex);
            reclaimed += items.size();
            for (auto& item : items) {
                queue.push_back(std::move(item));
            }
            peak = std::max(peak, queue.size());
        }
        wake.notify_one();
    }
    std::size_t backlog() {
        const std::lock_guard lock(mutex);
        return queue.size();
    }
    std::size_t peak_backlog() {
        const std::lock_guard lock(mutex);
        return peak;
    }
    void run() {
        for (;;) {
            std::shared_ptr<const void> dead;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [this] { return stop || !queue.empty(); });
                if (queue.empty()) {
                    return; // stopping, and nothing left to release
                }
                dead = std::move(queue.front());
                queue.pop_front();
            }
            if (gate) {
                gate(); // test seam: a stalled reaper
            }
            // `dead` is released here, outside the lock and off the window thread.
        }
    }
};

namespace {

/// A worker's build of one request: the island's records rewritten with the island's buildings,
/// its object lists, and the change set against the lists the build started from. Pure apart from
/// the fault seam; throws on failure, and the caller records it.
PictureResult build_result(const PictureJob& job, const std::vector<bool>& is_building,
                           const std::function<void(std::size_t)>& fault) {
    if (fault) {
        fault(job.index);
    }
    PictureResult result;
    result.seq = job.seq;
    result.index = job.index;
    auto storage = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    formats::insel::Island island = job.base->islands[job.index];
    // Own every byte the island points into before the rewrite, so no generation's buffers are
    // needed afterwards.
    {
        std::vector<std::uint8_t> head(island.head.begin(), island.head.end());
        storage->push_back(std::move(head));
        island.head = formats::Bytes(storage->back().data(), storage->back().size());
        for (formats::chunk::Chunk& rec : island.records) {
            std::vector<std::uint8_t> payload(rec.payload.begin(), rec.payload.end());
            storage->push_back(std::move(payload));
            rec.payload = formats::Bytes(storage->back().data(), storage->back().size());
        }
    }
    SceneObjects objects;
    objects.is_building = [&is_building](std::uint16_t id) {
        return id < is_building.size() && is_building[id];
    };
    std::vector<std::string> notes;
    std::vector<OmittedObject> omitted;
    apply_island_buildings(island, job.buildings, objects, *storage, notes, omitted);
    auto lists = render::Scene::build_island_lists(island, job.base->anlagen);
    result.base_lists = job.base->island_lists_handle(job.index);
    result.change = change_set(*job.base, job.index, *result.base_lists, *lists);
    result.island = std::move(island);
    result.storage = std::move(storage);
    result.lists = std::move(lists);
    return result;
}

} // namespace

ScenePicture::ScenePicture(std::shared_ptr<const render::Scene> base,
                           std::vector<SceneBuilding> buildings,
                           const std::function<bool(std::uint16_t)>& is_building,
                           std::size_t threads, Limits limits)
    : current_(std::move(base)), picture_(std::move(buildings)), state_(std::make_shared<State>()),
      reaper_(std::make_shared<Reaper>()), limits_(std::move(limits)) {
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
    state_->is_deferred.assign(current_->islands.size(), false);
    reaper_->gate = limits_.reclaim_gate;
    for (std::size_t t = 0; t < std::max<std::size_t>(threads, 1); ++t) {
        threads_.emplace_back([st = state_, fault = limits_.build_fault] {
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
                State::Result result;
                bool built = false;
                // The catch-all of the worker seam: a build that fails must not end the thread or
                // leave the island's request open (the island keeps its committed lists).
                try {
                    result = build_result(job, *st->is_building, fault);
                    built = true;
                } catch (...) {
                    built = false;
                }
                const double ms = ms_since(start);
                // The job's generation is released here, on the worker: it may be the last
                // reference, and the worker is not the window thread.
                job.base.reset();
                {
                    const std::lock_guard lock(st->mutex);
                    --st->running;
                    st->stats.last_build_ms = ms;
                    const bool current = st->latest[job.index] == job.seq && st->open[job.index];
                    if (!built) {
                        ++st->stats.build_failures;
                        if (current) {
                            st->open[job.index] = false;
                            --st->outstanding;
                        }
                    } else if (current) {
                        // The window thread removes a finished result when it requests the island
                        // again, so none of the same island can be here; kept for safety.
                        auto same = std::find_if(
                            st->done.begin(), st->done.end(),
                            [&job](const State::Result& r) { return r.index == job.index; });
                        if (same != st->done.end()) {
                            ++st->stats.coalesced;
                            *same = std::move(result);
                        } else {
                            st->done.push_back(std::move(result));
                        }
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
    // Nothing a worker never ran, and no finished result, is freed on this thread.
    Dropped rest;
    {
        const std::lock_guard lock(state_->mutex);
        for (State::Job& j : state_->jobs) {
            rest.push_back(std::make_shared<State::Job>(std::move(j)));
        }
        state_->jobs.clear();
        for (State::Result& r : state_->done) {
            rest.push_back(std::make_shared<State::Result>(std::move(r)));
        }
        state_->done.clear();
    }
    rest.push_back(std::move(current_));
    reaper_->retire(std::move(rest));
    {
        const std::lock_guard lock(reaper_->mutex);
        reaper_->stop = true;
    }
    reaper_->wake.notify_all();
    reaper_->thread.join();
}

std::size_t ScenePicture::dispatch(const std::vector<std::size_t>& wanted, Dropped& dropped) {
    // The islands to request: the wanted ones, then the deferred ones in the order they waited
    // (at most as many as the outstanding limit, so the candidate list is bounded).
    std::vector<std::size_t> candidates;
    std::set<std::size_t> seen;
    for (std::size_t index : wanted) {
        if (seen.insert(index).second) {
            candidates.push_back(index);
        }
    }
    {
        const std::lock_guard lock(state_->mutex);
        std::size_t taken = 0;
        for (std::size_t index : state_->deferred) {
            if (taken == limits_.max_outstanding) {
                break;
            }
            if (state_->is_deferred[index] && seen.insert(index).second) {
                candidates.push_back(index);
                ++taken;
            }
        }
    }
    std::set<std::uint32_t> slots;
    for (std::size_t index : candidates) {
        slots.insert(current_->islands[index].number);
    }
    // The window thread owns the building list, so the buildings are grouped here, before the lock.
    auto groups = buildings_by_island(picture_, slots);
    // Overload: while the reaper is behind, replacing an open island would drop its queued job and
    // result to the reaper and grow the backlog further. Such an island is deferred instead (it
    // keeps its request); an island that is not open starts without any drop.
    const bool overloaded = reaper_->backlog() > limits_.max_reclaim_backlog;
    std::size_t requested = 0;
    {
        const std::lock_guard lock(state_->mutex);
        for (std::size_t index : candidates) {
            const bool is_wanted = std::find(wanted.begin(), wanted.end(), index) != wanted.end();
            if (state_->open[index]) {
                if (overloaded) {
                    if (!state_->is_deferred[index]) {
                        state_->is_deferred[index] = true;
                        state_->deferred.push_back(index);
                        ++state_->stats.deferred;
                    }
                    continue;
                }
                // A newer request replaces this island's queued job and finished result (the
                // running build is discarded when it completes).
                auto queued =
                    std::find_if(state_->jobs.begin(), state_->jobs.end(),
                                 [index](const State::Job& q) { return q.index == index; });
                if (queued != state_->jobs.end()) {
                    dropped.push_back(std::make_shared<State::Job>(std::move(*queued)));
                    state_->jobs.erase(queued);
                }
                auto finished =
                    std::find_if(state_->done.begin(), state_->done.end(),
                                 [index](const State::Result& r) { return r.index == index; });
                if (finished != state_->done.end()) {
                    ++state_->stats.coalesced;
                    dropped.push_back(std::make_shared<State::Result>(std::move(*finished)));
                    state_->done.erase(finished);
                }
            } else if (state_->outstanding < limits_.max_outstanding) {
                state_->open[index] = true;
                ++state_->outstanding;
                state_->stats.outstanding_peak =
                    std::max(state_->stats.outstanding_peak, state_->outstanding);
            } else {
                if (!state_->is_deferred[index]) {
                    state_->is_deferred[index] = true;
                    state_->deferred.push_back(index);
                    ++state_->stats.deferred;
                }
                continue;
            }
            if (state_->is_deferred[index]) {
                state_->is_deferred[index] = false;
                ++state_->stats.started_deferred;
            }
            State::Job job;
            job.seq = state_->next_seq++;
            job.index = index;
            job.base = current_;
            job.buildings = std::move(groups[current_->islands[index].number]);
            state_->latest[index] = job.seq;
            state_->jobs.push_back(std::move(job));
            if (is_wanted) {
                ++requested;
            }
        }
        std::erase_if(state_->deferred, [this](std::size_t i) { return !state_->is_deferred[i]; });
    }
    state_->wake.notify_all();
    return requested;
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
    std::vector<std::size_t> wanted;
    for (const std::uint32_t slot : touched) {
        const auto it = std::lower_bound(slots_.begin(), slots_.end(),
                                         std::pair<std::uint32_t, std::size_t>{slot, 0});
        if (it == slots_.end() || it->first != slot) {
            continue; // a building of an island the scene does not draw
        }
        wanted.push_back(it->second);
    }
    Dropped dropped;
    const std::size_t requested = dispatch(wanted, dropped);
    reaper_->retire(std::move(dropped));
    return requested;
}

std::size_t ScenePicture::commit(render::FrameRenderer& frames) {
    // Overload: a reaper that has not released the last commit's generations makes this commit
    // wait for the next call. The window thread never blocks on the reaper.
    if (reaper_->backlog() > limits_.max_reclaim_backlog) {
        const std::lock_guard lock(state_->mutex);
        ++state_->stats.commit_skipped;
        return 0;
    }
    Dropped dropped;
    std::vector<State::Result> ready;
    {
        const std::lock_guard lock(state_->mutex);
        for (State::Result& r : state_->done) {
            if (state_->latest[r.index] == r.seq && state_->open[r.index]) {
                state_->open[r.index] = false;
                --state_->outstanding;
                ready.push_back(std::move(r));
            } else {
                ++state_->stats.discarded;
                dropped.push_back(std::make_shared<State::Result>(std::move(r)));
            }
        }
        state_->done.clear();
    }
    if (ready.empty()) {
        dispatch({}, dropped); // the outstanding slots may have freed up
        reaper_->retire(std::move(dropped));
        return 0;
    }
    const auto start = Clock::now();
    const std::shared_ptr<const render::Scene> old = current_;
    std::vector<render::Scene::IslandUpdate> updates;
    updates.reserve(ready.size());
    std::set<render::ChunkKey> keys;
    std::uint64_t recomputed = 0;
    for (State::Result& r : ready) {
        // The worker's change set is valid when the island's lists are still the ones it was taken
        // from. Otherwise (not expected: a commit of this island is the only way its lists change,
        // and a newer request makes the result stale) the set is taken here, from the old lists.
        ChangeSet change;
        if (r.base_lists == old->island_lists_handle(r.index)) {
            change = std::move(r.change);
        } else {
            ++recomputed;
            change = change_set(*old, r.index, old->island_lists(r.index), *r.lists);
        }
        keys.insert(change.keys.begin(), change.keys.end());
        // The superseded lists leave with the commit: they are released by the reaper.
        dropped.push_back(std::move(r.base_lists));
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
    {
        const std::lock_guard lock(state_->mutex);
        // A queued job pins the generation it was requested from. Its island is unchanged by this
        // commit (a commit of the island would have replaced the job), so it moves to `next`; the
        // generation it held goes to the reaper.
        for (State::Job& q : state_->jobs) {
            if (q.base == old) {
                dropped.push_back(q.base);
                q.base = next;
                ++state_->stats.rebased;
            }
        }
        state_->stats.committed += ready.size();
        state_->stats.invalidated_keys += invalid.size();
        state_->stats.last_commit_ms = ms_since(start);
        state_->stats.recomputed_changes += recomputed;
    }
    dropped.push_back(old);
    dispatch({}, dropped); // the commit freed outstanding slots: start the deferred builds
    reaper_->retire(std::move(dropped));
    return ready.size();
}

void ScenePicture::wait_idle() {
    std::unique_lock lock(state_->mutex);
    state_->idle.wait(lock, [this] { return state_->jobs.empty() && state_->running == 0; });
}

std::size_t ScenePicture::pending() const {
    const std::lock_guard lock(state_->mutex);
    std::size_t n = 0;
    for (std::size_t i = 0; i < state_->open.size(); ++i) {
        n += (state_->open[i] || state_->is_deferred[i]) ? 1U : 0U;
    }
    return n;
}

ScenePicture::Stats ScenePicture::stats() const {
    Stats s;
    {
        const std::lock_guard lock(state_->mutex);
        s = state_->stats;
    }
    s.reclaimed = reaper_->reclaimed.load();
    s.reclaim_backlog_peak = reaper_->peak_backlog();
    return s;
}

} // namespace anno::app
