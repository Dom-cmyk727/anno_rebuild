// ScenePicture (docs/app/game-shell.md, "Immutable per-island object lists" and "Retained work and
// reclamation"). See the header. State, in one place:
//   desired[i]    the version of island i's newest change (advanced by every apply, deferred or
//   not); committed[i]  the version the committed generation has (set by commit); inflight[i]   the
//   version of the build in flight or finished for island i (0: none; the island is
//                 open until it is committed, dropped or failed);
//   dirty         the islands with desired != committed (pending() is their count: no false idle).
// A result is committed only when its version is still desired; a result whose version is older is
// dropped and its island is built again from the current desired state. The window thread never
// frees what it gives up: it hands it to the reaper (Dropped). The final destruction of a
// generation is the reclaimer's (render::SceneReclaimer), whichever thread released it last.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
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

#include <anno/app/credit.hpp>
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

/// The chunk keys (every zoom level) that the objects of `objects` touch in `scene`, at each built
/// group.
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

/// The objects that appear and disappear between two lists of one island, and the chunk keys they
/// touch. Layouts, Anlagen and lift are the same in every generation, so `scene` may be any
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

/// The bytes an island's records occupy (its head and every record payload).
std::size_t island_bytes(const formats::insel::Island& island) {
    std::size_t n = island.head.size();
    for (const formats::chunk::Chunk& rec : island.records) {
        n += rec.payload.size();
    }
    return n;
}

/// The descriptor bytes of an island's records (a copy of the island copies them).
std::size_t record_meta_bytes(const formats::insel::Island& island) {
    return island.records.capacity() * sizeof(formats::chunk::Chunk);
}

/// The objects an island's lists draw (the subsets are copies of some of them).
std::size_t list_objects(const render::IslandLists& lists) {
    return lists.below.size() + lists.above.size() + lists.other.size();
}

/// The bytes a build of island `island` (lists `lists`) with `desired` buildings is reserved for,
/// before it runs (round dy, A2/R2; a provisional estimate, docs/app/game-shell.md). It covers
/// what the build holds at its largest: the copy of the desired buildings; the copy of every
/// record (old payload) plus each rewritten block (old payload again, plus one PROD2 item per
/// desired building and a block header per building that opens a block); the new lists (the old
/// lists plus every desired building in a list and in a subset); the change set (every old and new
/// object, and a chunk key per object and group); and the two sorted object copies the change set
/// is taken from. The worker replaces the estimate with the measured size of its result
/// (result_bytes); a result larger than its estimate is counted (Stats::estimate_shortfalls).
std::size_t build_estimate(const formats::insel::Island& island, const render::IslandLists& lists,
                           std::size_t desired) {
    const std::size_t old_payload = island_bytes(island);
    const std::size_t old_objects = list_objects(lists);
    const std::size_t new_objects = old_objects + desired;
    const std::size_t buildings = desired * sizeof(SceneBuilding);
    const std::size_t storage =
        2 * old_payload + desired * (formats::insel::kItemLen + formats::insel::kBlockHeaderLen) +
        (island.records.size() + desired + 1) * sizeof(render::Buffers::value_type);
    const std::size_t meta = record_meta_bytes(island) + desired * sizeof(formats::chunk::Chunk);
    const std::size_t new_lists =
        render::lists_bytes(lists) + 2 * desired * sizeof(render::Prepared);
    const std::size_t change =
        (old_objects + new_objects) *
        (sizeof(render::Prepared) + render::kGroups * sizeof(render::ChunkKey));
    const std::size_t sorted = (old_objects + new_objects) * sizeof(render::Prepared);
    return buildings + storage + meta + new_lists + change + sorted;
}

/// A test seam's probe: calls the hook when the payload that holds it is destroyed. It is the
/// first member of a payload, so it is destroyed last of the payload, after every other member.
struct DestroyProbe {
    std::function<void(const char*, std::size_t)> hook;
    const char* what = "";
    std::size_t index = 0;
    DestroyProbe(const DestroyProbe&) = delete;
    DestroyProbe& operator=(const DestroyProbe&) = delete;
    DestroyProbe(std::function<void(const char*, std::size_t)> h, const char* w, std::size_t i)
        : hook(std::move(h)), what(w), index(i) {}
    ~DestroyProbe() {
        if (hook) {
            hook(what, index);
        }
    }
};

/// One request for an island's rebuild: the version it is made for, the generation it is made from
/// and the buildings it applies. Held as Accounted<JobPayload> (PictureJob): its credit outlives
/// it.
struct JobPayload {
    std::shared_ptr<DestroyProbe> probe;
    std::size_t index = 0;
    std::uint64_t version = 0;
    std::shared_ptr<const render::Scene> base;
    std::vector<SceneBuilding> buildings;
    /// The estimate the build was admitted by (build_estimate).
    std::size_t bytes = 0;
    /// When the job was handed to the workers (its queue delay is the time to its start).
    std::chrono::steady_clock::time_point submitted{};
};
using PictureJob = Accounted<JobPayload>;

/// The result of a rebuild: the island with its records after the change, its lists, and the change
/// set taken against the lists the rebuild started from. Held as Accounted<ResultPayload>
/// (PictureResult): its credit is released after this payload has been destroyed, or when the
/// commit has handed the payload to the next generation.
struct ResultPayload {
    std::shared_ptr<DestroyProbe> probe;
    std::size_t index = 0;
    std::uint64_t version = 0;
    /// The island with its records after the change; every byte it points into is in `storage`
    /// (copied by the worker), so the island needs no other owner.
    formats::insel::Island island;
    std::shared_ptr<const render::Buffers> storage;
    std::shared_ptr<const render::IslandLists> lists;
    /// The lists the change set was taken from: the version the commit checks. Shared with the
    /// generation the build started from (counted by that allocation, not by this result).
    std::shared_ptr<const render::IslandLists> base_lists;
    ChangeSet change;
};
using PictureResult = Accounted<ResultPayload>;

/// The measured bytes a result holds of its own: its record descriptors, its record storage, its
/// lists and its change set (capacities times element sizes; allocator overhead not counted).
std::size_t result_bytes(const PictureResult& r) {
    std::size_t n = record_meta_bytes(r.island);
    n += r.storage ? render::buffers_bytes(*r.storage) : 0;
    n += r.lists ? render::lists_bytes(*r.lists) : 0;
    n += (r.change.removed.capacity() + r.change.added.capacity()) * sizeof(render::Prepared);
    n += r.change.keys.capacity() * sizeof(render::ChunkKey);
    return n;
}

/// A worker's build of one request: the island's records rewritten with the island's buildings, its
/// object lists, and the change set against the lists the build started from. Throws on failure.
PictureResult build_result(const PictureJob& job, const std::vector<bool>& is_building,
                           const std::function<void(std::size_t)>& fault,
                           const std::function<void(const char*, std::size_t)>& destroy_hook) {
    if (fault) {
        fault(job.index);
    }
    PictureResult result;
    if (destroy_hook) {
        result.probe = std::make_shared<DestroyProbe>(destroy_hook, "result", job.index);
    }
    result.index = job.index;
    result.version = job.version;
    render::Buffers storage;
    formats::insel::Island island = job.base->islands[job.index];
    // Own every byte the island points into before the rewrite, so no generation's buffers are
    // needed afterwards. The buffers keep their addresses when the outer vector grows or moves.
    {
        std::vector<std::uint8_t> head(island.head.begin(), island.head.end());
        storage.push_back(std::move(head));
        island.head = formats::Bytes(storage.back().data(), storage.back().size());
        for (formats::chunk::Chunk& rec : island.records) {
            std::vector<std::uint8_t> payload(rec.payload.begin(), rec.payload.end());
            storage.push_back(std::move(payload));
            rec.payload = formats::Bytes(storage.back().data(), storage.back().size());
        }
    }
    SceneObjects objects;
    objects.is_building = [&is_building](std::uint16_t id) {
        return id < is_building.size() && is_building[id];
    };
    std::vector<std::string> notes;
    std::vector<OmittedObject> omitted;
    apply_island_buildings(island, job.buildings, objects, storage, notes, omitted);
    auto lists = render::Scene::build_island_lists(island, job.base->anlagen,
                                                   render::PayloadKind::Incremental);
    result.base_lists = job.base->island_lists_handle(job.index);
    result.change = change_set(*job.base, job.index, *result.base_lists, *lists);
    result.island = std::move(island);
    result.storage =
        render::accounted_buffers(std::move(storage), render::PayloadKind::Incremental);
    result.lists = std::move(lists);
    return result;
}

} // namespace

/// State shared by the window thread and the workers. Everything below `mutex` is guarded by it.
struct ScenePicture::State {
    std::mutex mutex;
    std::condition_variable wake, idle;
    std::deque<PictureJob> jobs;
    std::size_t running = 0;
    /// Finished results: at most one per island (a newer result of an island replaces it).
    std::vector<PictureResult> done;
    /// Per island: the desired and the committed version, and the version in flight or finished (0:
    /// none).
    std::vector<std::uint64_t> desired, committed, inflight;
    /// Islands whose committed state differs from the desired state.
    std::set<std::size_t> dirty;
    /// The builds that hold a credit (in flight, finished, superseded or dropped and not yet
    /// destroyed), and their bytes: the admission counts (Credit).
    std::shared_ptr<Ledger> ledger = std::make_shared<Ledger>();
    /// Islands whose start waits for a free slot or for the reaper, in the order they waited.
    std::deque<std::size_t> deferred;
    std::vector<bool> is_deferred;
    /// Failures per island since its last change; islands waiting for a retry, and when (commit
    /// call).
    std::vector<std::uint32_t> failures;
    std::vector<bool> failed_for_good;
    std::vector<std::uint64_t> retry_at;
    std::set<std::size_t> retrying;
    std::uint64_t commit_calls = 0;
    /// The message of each island's failure for good (empty when it has none); per island, so one
    /// island's edit or recovery does not erase another island's failure (round dx, DX3a).
    std::vector<std::string> failure;
    bool stop = false;
    /// Anlagen ids below 0x1000 that are buildings (the loader's rule, asked once).
    std::shared_ptr<const std::vector<bool>> is_building;
    Stats stats;
    /// Per build: the milliseconds in the queue and in the build (bounded; see the worker).
    std::vector<double> queue_ms, build_ms;

    /// Closes island `index`'s open build (committed, dropped or failed): the island may start
    /// another. The build's credit goes with its job or result (Credit).
    void close(std::size_t index) { inflight[index] = 0; }
};

/// Releases what the window thread gave up, on its own thread: a generation that is not the last
/// reference (its destruction is the reclaimer's), a job or a result.
struct ScenePicture::Reaper {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::shared_ptr<const void>> queue;
    std::size_t peak = 0;
    bool stop = false;
    std::thread thread;
    std::function<void()> gate;
    std::uint64_t queued = 0, completed = 0;

    void retire(Dropped items) {
        if (items.empty()) {
            return;
        }
        {
            const std::lock_guard lock(mutex);
            queued += items.size();
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
            dead.reset();
            const std::lock_guard lock(mutex);
            ++completed;
        }
    }
};

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
    {
        std::set<std::uint32_t> islands;
        for (const SceneBuilding& b : picture_) {
            islands.insert(b.island);
        }
        reindex(islands);
    }
    auto table = std::make_shared<std::vector<bool>>(0x1000, false);
    for (std::size_t id = 0; id < table->size(); ++id) {
        (*table)[id] = is_building(static_cast<std::uint16_t>(id));
    }
    const std::size_t n = current_->islands.size();
    state_->is_building = std::move(table);
    state_->desired.assign(n, 0);
    state_->committed.assign(n, 0);
    state_->inflight.assign(n, 0);
    state_->is_deferred.assign(n, false);
    state_->failures.assign(n, 0);
    state_->failed_for_good.assign(n, false);
    state_->failure.assign(n, std::string());
    state_->retry_at.assign(n, 0);
    reaper_->gate = limits_.reclaim_gate;
    for (std::size_t t = 0; t < std::max<std::size_t>(threads, 1); ++t) {
        threads_.emplace_back([st = state_, fault = limits_.build_fault,
                               max_attempts = limits_.max_attempts,
                               destroy_hook = limits_.payload_destroy_hook] {
            for (;;) {
                PictureJob job;
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
                const double queue_ms =
                    std::chrono::duration<double, std::milli>(start - job.submitted).count();
                PictureResult result;
                bool built = false;
                std::string message;
                // The catch-all of the worker seam: a failed build must not end the thread, and
                // must leave the island dirty so that it is built again
                // (ScenePicture::error()).
                try {
                    result = build_result(job, *st->is_building, fault, destroy_hook);
                    built = true;
                } catch (const std::exception& e) {
                    message = e.what();
                } catch (...) {
                    message = "unknown failure in an object build";
                }
                const double ms = ms_since(start);
                // The job's own payload is released here, on the worker, while the job still
                // holds the credit: its copy of the buildings, and its generation (the
                // reclaimer destroys that if this is the last reference). Only then does the
                // credit move to the result, resized to what the result measures (round dy,
                // A2). A failed build keeps the credit with its job until the job is gone.
                job.base.reset();
                std::vector<SceneBuilding>().swap(job.buildings);
                std::size_t measured = 0;
                if (built) {
                    result.credit = std::move(job.credit);
                    measured = result_bytes(result);
                    result.credit.resize(measured);
                }
                {
                    const std::lock_guard lock(st->mutex);
                    st->stats.last_build_ms = ms;
                    if (built) {
                        st->stats.result_bytes_peak =
                            std::max(st->stats.result_bytes_peak, measured);
                        if (measured > job.bytes) {
                            ++st->stats.estimate_shortfalls;
                            st->stats.estimate_shortfall_max =
                                std::max(st->stats.estimate_shortfall_max, measured - job.bytes);
                        }
                    }
                    // Bounded sample: the newest samples are kept (the bench runs far fewer
                    // builds).
                    constexpr std::size_t kSamples = 200000;
                    if (st->build_ms.size() < kSamples) {
                        st->build_ms.push_back(ms);
                        st->queue_ms.push_back(queue_ms);
                    }
                    // A failure belongs to the version it was built for: it counts only while
                    // that version is still the desired one and still in flight (round dx,
                    // DX3b).
                    const bool current = st->inflight[job.index] == job.version &&
                                         st->desired[job.index] == job.version;
                    if (!current) {
                        // Superseded while it ran (a newer change or a dropped request): its
                        // result and credit are destroyed when this call returns. An open
                        // build of this island that is not desired any more frees the island
                        // (it stays deferred until it starts again).
                        ++st->stats.discarded;
                        if (st->inflight[job.index] == job.version) {
                            st->close(job.index);
                        }
                    } else if (!built) {
                        ++st->stats.build_failures;
                        st->close(job.index);
                        ++st->failures[job.index];
                        if (st->failures[job.index] >= max_attempts) {
                            st->failed_for_good[job.index] = true;
                            st->failure[job.index] = message;
                        } else {
                            // Retried by the window's next commit calls, not by a timer.
                            const std::uint64_t backoff =
                                std::uint64_t{1}
                                << std::min<std::uint32_t>(st->failures[job.index] - 1, 16);
                            st->retry_at[job.index] = st->commit_calls + backoff;
                            st->retrying.insert(job.index);
                        }
                    } else {
                        auto same = std::find_if(
                            st->done.begin(), st->done.end(),
                            [&job](const PictureResult& r) { return r.index == job.index; });
                        if (same != st->done.end()) {
                            // The replaced result leaves in `result`, and is destroyed below,
                            // off the lock, payload first and credit last.
                            ++st->stats.coalesced;
                            std::swap(*same, result);
                        } else {
                            st->done.push_back(std::move(result));
                        }
                    }
                }
                // What was not kept is destroyed here, off the lock (its credit with it; a
                // kept result was moved). The worker counts itself as done only after that, so
                // wait_idle returns after every release of this build.
                job = PictureJob{};
                result = PictureResult{};
                {
                    const std::lock_guard lock(st->mutex);
                    --st->running;
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
    // Nothing a worker never ran, and no finished result, is released on this thread.
    Dropped rest;
    {
        const std::lock_guard lock(state_->mutex);
        for (PictureJob& j : state_->jobs) {
            rest.push_back(std::make_shared<PictureJob>(std::move(j)));
        }
        state_->jobs.clear();
        for (PictureResult& r : state_->done) {
            rest.push_back(std::make_shared<PictureResult>(std::move(r)));
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

bool ScenePicture::reclaim_overloaded() const {
    return reaper_->backlog() > limits_.max_reclaim_backlog ||
           render::SceneReclaimer::shared().stats().waiting > limits_.max_reclaim_backlog;
}

std::size_t ScenePicture::dispatch(const std::vector<std::size_t>& wanted,
                                   std::vector<std::shared_ptr<const void>>& dropped) {
    // The islands to consider: the wanted ones (changed now), the deferred ones in the order they
    // waited, and the islands due for a retry. Each list is bounded: deferred and retrying islands
    // are islands of this picture, and at most `max_outstanding` deferred ones are taken per call.
    std::vector<std::size_t> candidates;
    std::set<std::size_t> seen;
    auto add = [&](std::size_t index) {
        if (seen.insert(index).second) {
            candidates.push_back(index);
        }
    };
    for (std::size_t index : wanted) {
        add(index);
    }
    {
        const std::lock_guard lock(state_->mutex);
        std::size_t taken = 0;
        for (std::size_t index : state_->deferred) {
            if (taken == limits_.max_outstanding) {
                break;
            }
            if (state_->is_deferred[index]) {
                add(index);
                ++taken;
            }
        }
        for (std::size_t index : state_->retrying) {
            if (state_->retry_at[index] <= state_->commit_calls) {
                add(index);
            }
        }
    }
    // Idle: nothing to start (the cost does not grow with the city).
    if (candidates.empty()) {
        return 0;
    }
    // While the reaper is behind, an island in flight is not replaced: replacing it would hand its
    // queued job and result to the reaper and grow the backlog. Such an island is deferred. An
    // island that is not in flight starts without any hand-over.
    const bool overloaded = reclaim_overloaded();
    std::size_t requested = 0;
    {
        const std::lock_guard lock(state_->mutex);
        auto defer = [this](std::size_t index) {
            if (!state_->is_deferred[index]) {
                state_->is_deferred[index] = true;
                state_->deferred.push_back(index);
                ++state_->stats.deferred;
            }
        };
        for (std::size_t index : candidates) {
            const bool is_wanted = std::find(wanted.begin(), wanted.end(), index) != wanted.end();
            if (state_->failed_for_good[index] && !is_wanted) {
                continue; // terminal until the island changes again (ScenePicture::error())
            }
            if (state_->desired[index] == state_->committed[index]) {
                state_->is_deferred[index] = false;
                state_->retrying.erase(index);
                continue; // nothing changed since the commit
            }
            if (state_->inflight[index] == state_->desired[index]) {
                continue; // a build of the current version is already in flight or finished
            }
            // The reservation covers the incoming build (the desired buildings and what they
            // rewrite), not only the old records (build_estimate).
            const auto mine = by_island_.find(current_->islands[index].number);
            const std::size_t desired_count = mine == by_island_.end() ? 0 : mine->second.size();
            const std::size_t bytes = build_estimate(current_->islands[index],
                                                     current_->island_lists(index), desired_count);
            const bool replaces = state_->inflight[index] != 0;
            if (replaces && overloaded) {
                defer(index);
                continue;
            }
            if (replaces) {
                // A newer change hands this island's queued job and finished result to the reaper,
                // with their credits: those are released when the reaper destroys them. A running
                // build keeps its credit until its worker releases it (Credit), so it is not
                // handed over here.
                bool handed = false;
                auto queued =
                    std::find_if(state_->jobs.begin(), state_->jobs.end(),
                                 [index](const PictureJob& q) { return q.index == index; });
                if (queued != state_->jobs.end()) {
                    dropped.push_back(std::make_shared<PictureJob>(std::move(*queued)));
                    state_->jobs.erase(queued);
                    handed = true;
                }
                auto finished =
                    std::find_if(state_->done.begin(), state_->done.end(),
                                 [index](const PictureResult& r) { return r.index == index; });
                if (finished != state_->done.end()) {
                    ++state_->stats.coalesced;
                    dropped.push_back(std::make_shared<PictureResult>(std::move(*finished)));
                    state_->done.erase(finished);
                    handed = true;
                }
                if (handed) {
                    state_->inflight[index] = 0; // no open build of this island any more
                }
            }
            // Admission counts every credit still held: a running, finished, superseded or dropped
            // build counts until it is destroyed. The byte budget admits a build when others fit
            // beside it. A single build may exceed the budget alone: otherwise such an island would
            // never be built.
            const std::size_t held = state_->ledger->count.load(std::memory_order_relaxed);
            if (held >= limits_.max_outstanding) {
                defer(index);
                continue;
            }
            if (held > 0 && state_->ledger->bytes.load(std::memory_order_relaxed) + bytes >
                                limits_.max_outstanding_bytes) {
                ++state_->stats.budget_deferrals;
                defer(index);
                continue;
            }
            state_->inflight[index] = state_->desired[index];
            PictureJob job;
            job.credit = Credit(state_->ledger, bytes);
            if (limits_.payload_destroy_hook) {
                job.probe =
                    std::make_shared<DestroyProbe>(limits_.payload_destroy_hook, "job", index);
            }
            state_->stats.outstanding_peak =
                std::max(state_->stats.outstanding_peak, state_->ledger->count.load());
            if (state_->is_deferred[index]) {
                state_->is_deferred[index] = false;
                ++state_->stats.started_deferred;
            }
            if (state_->retrying.erase(index) != 0) {
                ++state_->stats.build_retries;
            }
            job.index = index;
            job.version = state_->desired[index];
            job.base = current_;
            // The island's buildings are copied only now, for an admitted build (DX2e).
            if (mine != by_island_.end()) {
                job.buildings = mine->second;
                state_->stats.copied_buildings += job.buildings.size();
            }
            job.bytes = bytes;
            job.submitted = Clock::now();
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

void ScenePicture::reindex(const std::set<std::uint32_t>& islands) {
    if (islands.empty()) {
        return;
    }
    for (const std::uint32_t island : islands) {
        by_island_.erase(island);
    }
    // One pass over the list: each island's buildings keep their order in the list.
    for (const SceneBuilding& b : picture_) {
        if (islands.contains(b.island)) {
            by_island_[b.island].push_back(b);
        }
    }
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
    reindex(touched);
    std::vector<std::size_t> wanted;
    {
        const std::lock_guard lock(state_->mutex);
        for (const std::uint32_t slot : touched) {
            const auto it = std::lower_bound(slots_.begin(), slots_.end(),
                                             std::pair<std::uint32_t, std::size_t>{slot, 0});
            if (it == slots_.end() || it->first != slot) {
                continue; // a building of an island the scene does not draw
            }
            // The desired version advances now, even if the build below is deferred.
            ++state_->desired[it->second];
            state_->dirty.insert(it->second);
            state_->failures[it->second] = 0;
            state_->failed_for_good[it->second] = false;
            state_->failure[it->second].clear();
            state_->retrying.erase(it->second);
            wanted.push_back(it->second);
        }
    }
    Dropped dropped;
    const std::size_t requested = dispatch(wanted, dropped);
    reaper_->retire(std::move(dropped));
    return requested;
}

std::size_t ScenePicture::commit(render::FrameRenderer& frames) {
    {
        const std::lock_guard lock(state_->mutex);
        ++state_->commit_calls;
    }
    // Overload: a reaper that has not released the last commit's items makes this commit wait for
    // the next call. The window thread never blocks on the reaper.
    if (reclaim_overloaded()) {
        const std::lock_guard lock(state_->mutex);
        ++state_->stats.commit_skipped;
        return 0;
    }
    Dropped dropped;
    std::vector<PictureResult> ready;
    {
        const std::lock_guard lock(state_->mutex);
        for (PictureResult& r : state_->done) {
            // Only the desired version may be published: a result of an older version (its island
            // changed after the build started) is dropped, and the island is built again.
            if (state_->inflight[r.index] == r.version && r.version == state_->desired[r.index]) {
                ready.push_back(std::move(r));
            } else {
                ++state_->stats.discarded;
                if (state_->inflight[r.index] == r.version) {
                    state_->close(r.index);
                }
                dropped.push_back(std::make_shared<PictureResult>(std::move(r)));
            }
        }
        state_->done.clear();
    }
    const auto start = Clock::now();
    const std::shared_ptr<const render::Scene> old = current_;
    std::vector<render::Scene::IslandUpdate> updates;
    std::set<render::ChunkKey> keys;
    std::uint64_t requeued = 0;
    std::vector<std::pair<std::size_t, std::uint64_t>> committed_now;
    updates.reserve(ready.size());
    for (PictureResult& r : ready) {
        // The worker's change set is valid when the island's lists are still the ones it was taken
        // from. Otherwise the result is requeued: the island is built again from the current
        // desired state. The change set is never recomputed on the window thread.
        if (r.base_lists != old->island_lists_handle(r.index)) {
            ++requeued;
            const std::lock_guard lock(state_->mutex);
            state_->close(r.index);
            dropped.push_back(r.base_lists);
            dropped.push_back(std::make_shared<PictureResult>(std::move(r)));
            continue;
        }
        keys.insert(r.change.keys.begin(), r.change.keys.end());
        // The superseded lists leave with the commit: they are released by the reaper.
        dropped.push_back(std::move(r.base_lists));
        render::Scene::IslandUpdate u;
        u.index = r.index;
        u.island = std::move(r.island);
        u.lists = std::move(r.lists);
        u.storage = std::move(r.storage);
        committed_now.emplace_back(r.index, r.version);
        updates.push_back(std::move(u));
    }
    if (updates.empty()) {
        {
            const std::lock_guard lock(state_->mutex);
            state_->stats.version_requeues += requeued;
        }
        dispatch({}, dropped);
        reaper_->retire(std::move(dropped));
        return 0;
    }
    std::shared_ptr<const render::Scene> next = old->with_islands(std::move(updates));
    std::vector<render::ChunkKey> invalid(keys.begin(), keys.end());
    frames.invalidate(invalid, next->generation());
    current_ = next;
    {
        const std::lock_guard lock(state_->mutex);
        for (const auto& [index, version] : committed_now) {
            state_->committed[index] = version;
            state_->close(index);
            if (state_->desired[index] == state_->committed[index]) {
                state_->dirty.erase(index);
            }
        }
        // A queued job pins the generation it was requested from. Its island is unchanged by this
        // commit (a commit of the island would have replaced the job), so it moves to `next`; the
        // old generation goes to the reaper.
        for (PictureJob& q : state_->jobs) {
            if (q.base == old) {
                dropped.push_back(q.base);
                q.base = next;
                ++state_->stats.rebased;
            }
        }
        state_->stats.committed += committed_now.size();
        state_->stats.invalidated_keys += invalid.size();
        state_->stats.version_requeues += requeued;
        state_->stats.last_commit_ms = ms_since(start);
    }
    dropped.push_back(old);
    dispatch({}, dropped); // the commit freed slots: start the deferred builds and the due retries
    reaper_->retire(std::move(dropped));
    return committed_now.size();
}

void ScenePicture::wait_idle() {
    std::unique_lock lock(state_->mutex);
    state_->idle.wait(lock, [this] { return state_->jobs.empty() && state_->running == 0; });
}

std::size_t ScenePicture::pending() const {
    const std::lock_guard lock(state_->mutex);
    return state_->dirty.size();
}

ScenePicture::Timings ScenePicture::timings() const {
    const std::lock_guard lock(state_->mutex);
    return Timings{state_->queue_ms, state_->build_ms};
}

std::string ScenePicture::error() const {
    const std::lock_guard lock(state_->mutex);
    std::string out;
    for (std::size_t i = 0; i < state_->failed_for_good.size(); ++i) {
        if (!state_->failed_for_good[i]) {
            continue;
        }
        if (!out.empty()) {
            out += "; ";
        }
        out += "island " + std::to_string(current_->islands[i].number) + ": " + state_->failure[i];
    }
    return out;
}

std::size_t ScenePicture::retry_failed() {
    std::vector<std::size_t> restart;
    {
        const std::lock_guard lock(state_->mutex);
        for (std::size_t i = 0; i < state_->failed_for_good.size(); ++i) {
            if (state_->failed_for_good[i]) {
                state_->failed_for_good[i] = false;
                state_->failure[i].clear();
                state_->failures[i] = 0;
                state_->retrying.erase(i);
                restart.push_back(i);
            }
        }
    }
    // The desired state is the current one: the build reads the picture's buildings, never a sim
    // command. The starts go through the same admission as any change (outstanding, byte budget,
    // reaper).
    Dropped dropped;
    const std::size_t started = dispatch(restart, dropped);
    reaper_->retire(std::move(dropped));
    return started;
}

ScenePicture::Stats ScenePicture::stats() const {
    Stats s;
    {
        const std::lock_guard lock(state_->mutex);
        s = state_->stats;
        s.dirty_islands = state_->dirty.size();
        s.outstanding = state_->ledger->count.load();
        s.outstanding_bytes = state_->ledger->bytes.load();
        s.outstanding_bytes_peak = state_->ledger->peak_bytes.load();
        s.failed_islands = static_cast<std::size_t>(
            std::count(state_->failed_for_good.begin(), state_->failed_for_good.end(), true));
    }
    {
        const std::lock_guard lock(reaper_->mutex);
        s.reclaim_queued = reaper_->queued;
        s.reclaim_completed = reaper_->completed;
    }
    s.reclaim_backlog_peak = reaper_->peak_backlog();
    return s;
}

} // namespace anno::app
