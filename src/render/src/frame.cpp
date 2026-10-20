#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <optional>

#include <anno/render/frame.hpp>
#include <anno/render/sea.hpp>

#include "thread_priority.hpp"

namespace anno::render {

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

std::size_t to_size(int v) {
    return static_cast<std::size_t>(v);
}

/// Index of scene pixel (x, y) in a buffer covering `rect`.
std::size_t index(Rect rect, int x, int y) {
    return to_size(y - rect.y) * to_size(rect.w) + to_size(x - rect.x);
}

std::size_t chunk_index(int cx, int cy, int x, int y) {
    return index(chunk_rect(cx, cy), x, y);
}

std::uint32_t rgb(Px c) {
    return std::uint32_t{c[0]} << 16 | std::uint32_t{c[1]} << 8 | std::uint32_t{c[2]};
}

/// `src` blended over an opaque 0RGB pixel.
void over_rgb(std::uint32_t& dst, Px src) {
    if (src[3] == 0) {
        return;
    }
    if (src[3] == 255) {
        dst = rgb(src);
        return;
    }
    Px d = sea::XrgbTarget{&dst}.get(0);
    over(d, src);
    dst = rgb(d);
}

/// Runs f(i) for i in [0, n) on up to `threads` threads.
template <class F> void par_for(std::size_t n, std::size_t threads, F&& f) {
    if (n == 0) {
        return;
    }
    std::atomic<std::size_t> next{0};
    auto work = [&] {
        for (;;) {
            const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
            if (i >= n) {
                return;
            }
            f(i);
        }
    };
    const std::size_t k = std::min(threads, n);
    if (k <= 1) {
        work();
        return;
    }
    std::vector<std::thread> pool;
    pool.reserve(k);
    for (std::size_t t = 0; t < k; ++t) {
        pool.emplace_back(work);
    }
    for (auto& t : pool) {
        t.join();
    }
}

/// Splits `out` (covering `view`) into row bands and runs f(band, rect) on
/// `threads` threads; `rect` is the scene rectangle of the band.
template <class F> void par_bands(std::uint32_t* out, Rect view, std::size_t threads, F&& f) {
    if (view.w <= 0 || view.h <= 0) {
        return;
    }
    // A few bands per thread balance uneven rows (sea vs. land).
    const std::size_t bands = std::min(threads * 4, to_size(view.h));
    const std::size_t rows = (to_size(view.h) + bands - 1) / bands;
    const std::size_t n = (to_size(view.h) + rows - 1) / rows;
    const std::size_t width = to_size(view.w);
    par_for(n, threads, [&](std::size_t i) {
        const std::size_t y0 = i * rows;
        const std::size_t h = std::min(rows, to_size(view.h) - y0);
        f(out + y0 * width,
          Rect{view.x, view.y + static_cast<int>(y0), view.w, static_cast<int>(h)});
    });
}

/// f over `items` on up to `threads` threads, results in item order.
template <class T, class F> auto par_map(const std::vector<T>& items, std::size_t threads, F&& f) {
    using R = decltype(f(items.front()));
    std::vector<std::optional<R>> slots(items.size());
    par_for(items.size(), threads, [&](std::size_t i) { slots[i] = f(items[i]); });
    std::vector<R> out;
    out.reserve(items.size());
    for (auto& s : slots) {
        out.push_back(std::move(*s));
    }
    return out;
}

/// Composites transparent canvases (scene coordinates) over `out`.
void composite_all(std::uint32_t* out, Rect view, const std::vector<Canvas>& canvases,
                   std::size_t threads) {
    if (canvases.empty()) {
        return;
    }
    par_bands(out, view, threads, [&](std::uint32_t* band, Rect rows) {
        for (const auto& c : canvases) {
            const Rect rect{c.origin.first, c.origin.second, static_cast<int>(c.width),
                            static_cast<int>(c.height)};
            const auto r = rect.intersect(rows);
            if (!r) {
                continue;
            }
            for (int y = r->y; y < r->y + r->h; ++y) {
                const std::size_t d = index(rows, r->x, y);
                const std::size_t s = index(rect, r->x, y);
                for (std::size_t k = 0; k < to_size(r->w); ++k) {
                    const std::uint8_t* p = &c.pixels[(s + k) * 4];
                    over_rgb(band[d + k], Px{p[0], p[1], p[2], p[3]});
                }
            }
        }
    });
}

std::vector<Px> to_px(const Canvas& c) {
    std::vector<Px> v(c.width * c.height);
    for (std::size_t i = 0; i < v.size(); ++i) {
        v[i] = {c.pixels[i * 4], c.pixels[i * 4 + 1], c.pixels[i * 4 + 2], c.pixels[i * 4 + 3]};
    }
    return v;
}

} // namespace

namespace {

/// The next chunk serial (Chunk::serial): unique in the process.
std::uint64_t next_serial() {
    static std::atomic<std::uint64_t> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

Chunk render_chunk_data(const Scene& scene, std::size_t group, int cx, int cy, bool ships,
                        bool wait_for_flow) {
    const Rect rect = chunk_rect(cx, cy);
    std::vector<std::size_t> islands;
    for (const auto& p : scene.layouts[group]) {
        if (p.bounds.intersect(rect)) {
            islands.push_back(p.index);
        }
    }
    Layers objects_only = Layers::none();
    objects_only.objects = true;
    objects_only.ships = ships;
    // Chunks for the GPU compositor (ships drawn there): with the object
    // base plane for the water against objects.
    objects_only.bases = !ships;
    if (islands.empty()) {
        // Open sea; only units (ships) can add an objects layer.
        if (!scene.units_touch(rect, group, ships)) {
            Chunk none;
            none.ships = ships;
            return none;
        }
        Chunk c;
        c.ships = ships;
        c.objects = to_px(scene.render(rect, group, objects_only).first);
        return c;
    }
    Chunk c;
    c.ships = ships;
    const Px water = scene.textures.water();
    auto [ground, probe] = scene.ground_with_probe(rect, group);
    c.ground.reserve(ground.width * ground.height);
    for (std::size_t i = 0; i < ground.width * ground.height; ++i) {
        Px px = water;
        over(px.data(), &ground.pixels[i * 4]);
        c.ground.push_back(rgb(px));
    }
    Canvas objects = scene.render(rect, group, objects_only).first;
    c.objects = to_px(objects);
    if (std::all_of(c.objects.begin(), c.objects.end(), [](const Px& p) { return p[3] == 0; })) {
        c.objects.clear();
    } else {
        c.base = std::move(objects.base);
    }
    const auto field = scene.water_field_from(rect, probe, group);
    {
        // The four planes have one entry per texel each; interleaved here,
        // on the builder thread, so the window thread uploads them as is.
        const std::size_t k = std::min(
            {field.cover.size(), field.shore.size(), field.depth.size(), field.flags.size()});
        if (k > 0) {
            c.field.resize(k * 4);
            std::uint8_t* o = c.field.data();
            for (std::size_t i = 0; i < k; ++i) {
                *o++ = field.cover[i];
                *o++ = field.shore[i];
                *o++ = field.depth[i];
                *o++ = field.flags[i];
            }
        }
    }
    c.terrain.reserve(probe.island.size());
    for (std::size_t i = 0; i < probe.island.size(); ++i) {
        const int x = rect.x + static_cast<int>(i % to_size(rect.w));
        const int y = rect.y + static_cast<int>(i / to_size(rect.w));
        const auto z = scene.terrain_at(probe, rect, x, y);
        c.terrain.push_back(z ? sat_cast<std::uint16_t>(clampf(roundf(*z * kTerrainScale), 0.0F,
                                                               static_cast<float>(kNoTerrain - 1)))
                              : kNoTerrain);
    }
    for (const std::size_t i : islands) {
        const auto [below, above] = scene.has_water_objects(i);
        c.water_below = c.water_below || below;
        c.water_above = c.water_above || above;
    }
    // The flow lattice of enhanced rivers (rivers.md 3.3).
    if (scene.river_active() && std::any_of(islands.begin(), islands.end(),
                                            [&](std::size_t i) { return scene.has_river(i); })) {
        c.flow = scene.flow_plane(rect, group, probe, rect, wait_for_flow, &c.flow_waiting);
        std::sort(c.flow_waiting.begin(), c.flow_waiting.end());
        c.flow_waiting.erase(std::unique(c.flow_waiting.begin(), c.flow_waiting.end()),
                             c.flow_waiting.end());
    }
    return c;
}

} // namespace

Chunk render_chunk(const Scene& scene, std::size_t group, int cx, int cy, bool ships,
                   bool wait_for_flow) {
    Chunk c = render_chunk_data(scene, group, cx, cy, ships, wait_for_flow);
    c.serial = next_serial();
    return c;
}

ChunkWorkers::ChunkWorkers(const Scene& scene, std::size_t threads) : scene_(scene) {
    for (std::size_t i = 0; i < std::max<std::size_t>(threads, 1); ++i) {
        threads_.emplace_back([this] { run(); });
    }
    reaper_ = std::thread([this] { reap(); });
}

ChunkWorkers::~ChunkWorkers() {
    {
        const std::lock_guard lock(mutex_);
        stop_ = true;
    }
    ready_.notify_all();
    garbage_ready_.notify_all();
    for (auto& t : threads_) {
        t.join();
    }
    reaper_.join();
}

void ChunkWorkers::reap() {
    lower_thread_priority();
    for (;;) {
        std::vector<Chunk> dead;
        {
            std::unique_lock lock(mutex_);
            garbage_ready_.wait(lock, [this] { return stop_ || !garbage_.empty(); });
            // Whatever is left when stopping is freed with the object.
            if (stop_) {
                return;
            }
            dead = std::exchange(garbage_, {});
        }
        // `dead` is freed here, outside the lock.
    }
}

void ChunkWorkers::discard(std::vector<Chunk> chunks) {
    {
        const std::lock_guard lock(mutex_);
        for (auto& c : chunks) {
            garbage_.push_back(std::move(c));
        }
    }
    garbage_ready_.notify_one();
}

void ChunkWorkers::run() {
    lower_thread_priority();
    for (;;) {
        ChunkKey key;
        bool ships = true;
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
            if (stop_) {
                return;
            }
            key = jobs_.front();
            jobs_.pop_front();
            busy_.insert(key);
            ships = ships_;
        }
        const auto [group, cx, cy] = key;
        std::optional<Chunk> chunk;
        const auto built_at = Clock::now();
        try {
            chunk = render_chunk(scene_, group, cx, cy, ships, false);
        } catch (const std::exception& e) {
            // E.g. out of memory: the chunk stays missing (open sea) and is not
            // requested again; the worker keeps running.
            std::fprintf(stderr, "chunk (%zu, %d, %d) failed: %s\n", group, cx, cy, e.what());
        }
        // A failed build says nothing about how long a chunk takes.
        if (chunk) {
            const double took = ms_since(built_at);
            build_ms_.store(0.8 * build_ms_.load() + 0.2 * took);
        }
        const std::lock_guard lock(mutex_);
        busy_.erase(key);
        if (chunk) {
            done_.emplace_back(key, std::move(*chunk));
        } else {
            failed_.insert(key);
        }
    }
}

bool ChunkWorkers::submit(const std::vector<ChunkKey>& wanted, bool ships) {
    const std::lock_guard lock(mutex_);
    ships_ = ships;
    jobs_.clear();
    for (const auto& k : wanted) {
        if (!busy_.contains(k) && !failed_.contains(k)) {
            jobs_.push_back(k);
        }
    }
    ready_.notify_all();
    return !jobs_.empty() || !busy_.empty();
}

std::vector<std::pair<ChunkKey, Chunk>> ChunkWorkers::take_done() {
    const std::lock_guard lock(mutex_);
    return std::exchange(done_, {});
}

FrameRenderer::FrameRenderer(std::size_t threads_, bool ships_in_chunks)
    : threads(std::max<std::size_t>(threads_, 1)), ships_(ships_in_chunks) {}

void FrameRenderer::set_workers(std::unique_ptr<ChunkWorkers> workers) {
    workers_ = std::move(workers);
    pending_ = false;
}

const Chunk* FrameRenderer::chunk(const ChunkKey& key) const {
    const auto it = cache_.find(key);
    return it == cache_.end() ? nullptr : &it->second;
}

void FrameRenderer::insert_chunk(const ChunkKey& key, Chunk chunk) {
    chunk.serial = next_serial();
    cache_.insert_or_assign(key, std::move(chunk));
}

std::tuple<std::size_t, double, std::size_t>
FrameRenderer::update_chunks(const Scene& scene, Rect view, std::size_t group) {
    const auto t = Clock::now();
    const auto chunks = visible_chunks(view);
    std::size_t built = 0;
    if (workers_) {
        built = request_chunks(scene, group, view, chunks);
    } else {
        built = fill_cache(scene, group, chunks);
        // Without background builds only the neighbouring groups' keys are of use
        // (the GPU compositor uploads those that are cached).
        std::set<ChunkKey> keep;
        plan_margin(view, group, chunks, 0.1, keep);
    }
    std::size_t missing = 0;
    for (const auto& [cx, cy] : chunks) {
        missing += cache_.contains({group, cx, cy}) ? 0U : 1U;
    }
    return {built, ms_since(t), missing};
}

FrameStats FrameRenderer::render(const Scene& scene, Rect view, std::size_t group,
                                 std::uint64_t time_ms, WaterStyle style, std::uint32_t* out) {
    const auto start = Clock::now();
    FrameStats stats;
    const auto chunks = visible_chunks(view);
    std::tie(stats.chunks_built, stats.chunks_ms, stats.chunks_missing) =
        update_chunks(scene, view, group);
    static const Chunk empty;
    // Visible chunks that are not built: the nearest group's chunks, scaled
    // (stand_ins), else open sea.
    std::map<std::pair<int, int>, Chunk> standing;
    if (stand_ins) {
        std::vector<std::pair<int, int>> missing;
        for (const auto& [cx, cy] : chunks) {
            if (!cache_.contains({group, cx, cy})) {
                missing.emplace_back(cx, cy);
            }
        }
        const Px open = scene.textures.water();
        auto built = par_map(missing, threads, [&](const std::pair<int, int>& c) {
            return stand_in_chunk(*this, group, c.first, c.second, open);
        });
        for (std::size_t i = 0; i < missing.size(); ++i) {
            if (built[i]) {
                standing.emplace(missing[i], std::move(*built[i]));
            }
        }
    }
    auto find = [&](int cx, int cy) -> const Chunk* {
        if (const auto it = cache_.find({group, cx, cy}); it != cache_.end()) {
            return &it->second;
        }
        const auto it = standing.find({cx, cy});
        return it == standing.end() ? nullptr : &it->second;
    };
    auto get = [&](int cx, int cy) -> const Chunk& {
        const Chunk* c = find(cx, cy);
        return c != nullptr ? *c : empty;
    };
    const std::uint32_t water = rgb(scene.textures.water());

    // Ground.
    auto t = Clock::now();
    par_bands(out, view, threads, [&](std::uint32_t* band, Rect rows) {
        for (const auto& [cx, cy] : chunks) {
            const auto r = chunk_rect(cx, cy).intersect(rows);
            if (!r) {
                continue;
            }
            const Chunk& c = get(cx, cy);
            for (int y = r->y; y < r->y + r->h; ++y) {
                std::uint32_t* dst = band + index(rows, r->x, y);
                if (c.ground.empty()) {
                    std::fill(dst, dst + r->w, water);
                } else {
                    const std::size_t s = chunk_index(cx, cy, r->x, y);
                    std::copy(c.ground.begin() + static_cast<std::ptrdiff_t>(s),
                              c.ground.begin() + static_cast<std::ptrdiff_t>(s) + r->w, dst);
                }
            }
        }
    });
    stats.ground_ms = ms_since(t);

    // Water objects at the frame time, rendered per visible chunk part.
    const std::function<std::optional<float>(int, int)> terrain =
        [&](int sx, int sy) -> std::optional<float> {
        const int cx = div_euclid(sx, kChunk), cy = div_euclid(sy, kChunk);
        const Chunk* c = find(cx, cy);
        if (c == nullptr) {
            return std::nullopt;
        }
        const std::size_t i = chunk_index(cx, cy, sx, sy);
        if (i >= c->terrain.size()) {
            return std::nullopt;
        }
        const std::uint16_t v = c->terrain[i];
        if (v == kNoTerrain) {
            return std::nullopt;
        }
        return static_cast<float>(v) / kTerrainScale;
    };
    auto render_parts = [&](bool below) {
        std::vector<Rect> parts;
        for (const auto& [cx, cy] : chunks) {
            const Chunk& c = get(cx, cy);
            if (!(below ? c.water_below : c.water_above)) {
                continue;
            }
            if (const auto r = chunk_rect(cx, cy).intersect(view)) {
                parts.push_back(*r);
            }
        }
        Layers layers = Layers::none();
        layers.water_below = below;
        layers.water_above = !below;
        layers.time_ms = time_ms;
        layers.water_style = style;
        return par_map(parts, threads, [&](const Rect& r) {
            return scene.render_with_terrain(r, group, layers, &terrain).first;
        });
    };
    t = Clock::now();
    composite_all(out, view, render_parts(true), threads);
    stats.water_objects_ms = ms_since(t);

    // Sea surface.
    t = Clock::now();
    if (const sea::SeaPattern* pattern = scene.sea(group)) {
        const Lift lift = scene.lift;
        const bool flowing = style == WaterStyle::Enhanced && river_flow == RiverFlowMode::Flow &&
                             scene.river_active();
        RiverParams river_params = scene.river_cfg.params();
        river_params.quality = river_quality;
        const sea::RiverShading shading{&river_params, scene.river_cfg.min_valid};
        par_bands(out, view, threads, [&](std::uint32_t* band, Rect rows) {
            for (const auto& [cx, cy] : chunks) {
                const auto r = chunk_rect(cx, cy).intersect(rows);
                if (!r) {
                    continue;
                }
                const Chunk& c = get(cx, cy);
                const std::size_t d = index(rows, r->x, r->y);
                const auto rw = to_size(r->w);
                const int ccx = cx, ccy = cy;
                const Rect rr = *r;
                auto field = [&c, rw, ccx, ccy, rr](std::size_t i) -> sea::FieldPx {
                    if (c.field.empty()) {
                        return {255, 255, 255, 1};
                    }
                    const std::size_t s = chunk_index(ccx, ccy, rr.x + static_cast<int>(i % rw),
                                                      rr.y + static_cast<int>(i / rw)) *
                                          4;
                    return {c.field[s], c.field[s + 1], c.field[s + 2], c.field[s + 3]};
                };
                auto flow = [&c, rw, rr](std::size_t i) -> FlowSample {
                    return river::flow_at(c.flow, rr.x + static_cast<int>(i % rw),
                                          rr.y + static_cast<int>(i / rw));
                };
                sea::shade_sea_flow(sea::XrgbTarget{band + d}, to_size(view.w), *r, field, flow,
                                    flowing && !c.flow.empty() ? &shading : nullptr, *pattern,
                                    time_ms, style, group, lift);
            }
        });
    }
    stats.sea_ms = ms_since(t);

    t = Clock::now();
    composite_all(out, view, render_parts(false), threads);
    stats.water_objects_ms += ms_since(t);

    // Cached objects.
    t = Clock::now();
    par_bands(out, view, threads, [&](std::uint32_t* band, Rect rows) {
        for (const auto& [cx, cy] : chunks) {
            const Chunk& c = get(cx, cy);
            if (c.objects.empty()) {
                continue;
            }
            const auto r = chunk_rect(cx, cy).intersect(rows);
            if (!r) {
                continue;
            }
            for (int y = r->y; y < r->y + r->h; ++y) {
                const std::size_t d = index(rows, r->x, y);
                const std::size_t s = chunk_index(cx, cy, r->x, y);
                for (std::size_t k = 0; k < to_size(r->w); ++k) {
                    over_rgb(band[d + k], c.objects[s + k]);
                }
            }
        }
    });
    stats.objects_ms = ms_since(t);
    stats.total_ms = ms_since(start);
    return stats;
}

std::vector<ChunkKey> FrameRenderer::plan_margin(Rect view, std::size_t group,
                                                 const std::vector<std::pair<int, int>>& visible,
                                                 double horizon_s, std::set<ChunkKey>& keep) {
    track_velocity(view, group);
    const auto ring =
        visible_chunks(lookahead_rect(view, vx_, vy_, horizon_s, max_lookahead_chunks));
    // The queue: visible chunks first, then the rest of the margin of this group,
    // then the neighbouring groups' views, each class nearest first to the view
    // as it will be in a moment (shifted by half the lookahead), so what the
    // motion brings into view first is built first, not what the key order says.
    struct Candidate {
        int rank;
        double distance;
        ChunkKey key;
        bool operator<(const Candidate& o) const {
            return std::tie(rank, distance, key) < std::tie(o.rank, o.distance, o.key);
        }
    };
    std::vector<Candidate> queue;
    auto add = [&](std::size_t g, const std::vector<std::pair<int, int>>& chunks, Rect centre_on,
                   double vx, double vy, auto&& rank_of) {
        const double reach = static_cast<double>(max_lookahead_chunks) * kChunk;
        const double px =
            centre_on.x + centre_on.w / 2.0 + std::clamp(vx * horizon_s, -reach, reach) / 2.0;
        const double py =
            centre_on.y + centre_on.h / 2.0 + std::clamp(vy * horizon_s, -reach, reach) / 2.0;
        for (const auto& [cx, cy] : chunks) {
            const ChunkKey k{g, cx, cy};
            if (!keep.insert(k).second) {
                continue;
            }
            const double dx = (cx + 0.5) * kChunk - px, dy = (cy + 0.5) * kChunk - py;
            queue.push_back({rank_of(cx, cy), dx * dx + dy * dy, k});
        }
    };
    add(group, visible, view, vx_, vy_, [](int, int) { return 0; });
    add(group, ring, view, vx_, vy_, [](int, int) { return 1; });
    // The view a zoom step would show (same window size, same centre point) in
    // each neighbouring zoom group, with the same margin, is built behind the
    // current group's chunks, whatever is still missing there: a pan during a
    // zoom lands on built content, not on open sea. The memory is the cache's:
    // these keys are kept like the ring (the budget is spare_chunks plus what is
    // kept), and adjacent_keys() hands them to the GPU compositor to upload when
    // it has nothing better to do.
    for (const std::size_t g : {group - 1, group + 1}) {
        // group − 1 wraps to SIZE_MAX for group 0 and is skipped.
        if (g < kGroups) {
            const Rect next = zoomed_view(view, group, g);
            const double sx = static_cast<double>(kTile[g].w) / kTile[group].w;
            const double sy = static_cast<double>(kTile[g].h) / kTile[group].h;
            add(g,
                visible_chunks(
                    lookahead_rect(next, vx_ * sx, vy_ * sy, horizon_s, max_lookahead_chunks)),
                next, vx_ * sx, vy_ * sy, [](int, int) { return 2; });
        }
    }
    std::sort(queue.begin(), queue.end());
    adjacent_.clear();
    margin_.clear();
    std::vector<ChunkKey> order;
    for (const auto& c : queue) {
        order.push_back(c.key);
        if (c.rank == 1) {
            margin_.push_back(c.key);
        } else if (c.rank == 2) {
            adjacent_.push_back(c.key);
        }
    }
    return order;
}

std::size_t FrameRenderer::request_chunks(const Scene& scene, std::size_t group, Rect view,
                                          const std::vector<std::pair<int, int>>& visible) {
    std::size_t received = 0;
    // A chunk replaced by its rebuild (the river flow arrived) is freed on the
    // reaper thread, like the evicted ones: freeing its ~4 MB of vectors costs
    // the window thread about a millisecond each.
    std::vector<Chunk> replaced;
    for (auto& [key, chunk] : workers_->take_done()) {
        // Workers shared with a renderer of the other ship setting: rebuilt.
        if (chunk.ships != ships_) {
            continue;
        }
        if (const auto it = cache_.find(key); it != cache_.end()) {
            replaced.push_back(std::move(it->second));
            it->second = std::move(chunk);
        } else {
            cache_.emplace(key, std::move(chunk));
        }
        ++received;
    }
    if (!replaced.empty()) {
        workers_->discard(std::move(replaced));
    }
    // The time a request takes to come back: a build on a free worker, doubled for
    // the queue in front of it, and the frames in between.
    std::set<ChunkKey> keep;
    const std::vector<ChunkKey> order =
        plan_margin(view, group, visible, 2.0 * workers_->build_ms() / 1000.0 + 0.1, keep);
    // A chunk built while an island's river flow was not ready is rebuilt once
    // the flow is: it stays in the cache (the legacy river look) until its
    // replacement arrives.
    auto stale = [&](const ChunkKey& k) {
        const auto it = cache_.find(k);
        return it != cache_.end() && it->second.flow_pending() &&
               std::all_of(it->second.flow_waiting.begin(), it->second.flow_waiting.end(),
                           [&](std::uint32_t i) { return scene.river_flow_done(i); });
    };
    std::vector<ChunkKey> todo;
    for (const auto& k : order) {
        if (!cache_.contains(k) || stale(k)) {
            todo.push_back(k);
        }
    }
    pending_ = workers_->submit(todo, ships_);
    const std::set<ChunkKey> retain = stand_in_retain(group, visible);
    trim(group, view, keep, retain);
    return received;
}

std::set<ChunkKey>
FrameRenderer::stand_in_retain(std::size_t group,
                               const std::vector<std::pair<int, int>>& visible) const {
    // The chunks that would stand in for a visible chunk still being built (the
    // zoom group the view came from, the prebuilt neighbours) are kept, though
    // nothing asks for them to be built: the eviction ranks other groups'
    // chunks by their distance in the current group's chunk coordinates, which
    // says nothing about what shows through. Only chunks already built, so no
    // more than (groups - 1) x the missing visible ones; none when stand-ins are
    // off (the eviction is then the one from before they existed).
    std::set<ChunkKey> retain;
    if (!stand_ins && !retain_stand_in_sources) {
        return retain;
    }
    for (const auto& [cx, cy] : visible) {
        if (cache_.contains({group, cx, cy})) {
            continue; // built: nothing to stand in for
        }
        for (const std::size_t from : stand_in_groups(group)) {
            for (const StandInSpan& span : stand_in_spans(cx, cy, group, from)) {
                const ChunkKey k{from, span.src_cx, span.src_cy};
                if (cache_.contains(k)) {
                    retain.insert(k);
                }
            }
        }
    }
    return retain;
}

void FrameRenderer::trim(std::size_t group, Rect view, const std::set<ChunkKey>& keep,
                         const std::set<ChunkKey>& retain) {
    if (cache_.size() <= keep.size() + spare_chunks) {
        return;
    }
    const int ccx = div_euclid(view.x + view.w / 2, kChunk);
    const int ccy = div_euclid(view.y + view.h / 2, kChunk);
    std::vector<std::tuple<bool, int, ChunkKey>> keys;
    for (const auto& [k, c] : cache_) {
        if (keep.contains(k) || retain.contains(k)) {
            continue;
        }
        const auto [g, cx, cy] = k;
        keys.emplace_back(g != group, std::abs(cx - ccx) + std::abs(cy - ccy), k);
    }
    std::sort(keys.begin(), keys.end());
    // Freeing a chunk's ~4 MB of vectors costs a millisecond each: with
    // background workers the evicted chunks are freed on their reaper thread.
    std::vector<Chunk> dead;
    for (std::size_t i = spare_chunks; i < keys.size(); ++i) {
        const auto it = cache_.find(std::get<2>(keys[i]));
        if (workers_) {
            dead.push_back(std::move(it->second));
        }
        cache_.erase(it);
    }
    if (workers_ && !dead.empty()) {
        workers_->discard(std::move(dead));
    }
}

std::size_t FrameRenderer::fill_cache(const Scene& scene, std::size_t group,
                                      const std::vector<std::pair<int, int>>& wanted) {
    std::vector<std::pair<int, int>> todo;
    for (const auto& [cx, cy] : wanted) {
        if (!cache_.contains({group, cx, cy})) {
            todo.emplace_back(cx, cy);
        }
    }
    auto built = par_map(todo, threads, [&](const std::pair<int, int>& c) {
        return render_chunk(scene, group, c.first, c.second, ships_);
    });
    for (std::size_t i = 0; i < todo.size(); ++i) {
        cache_.insert_or_assign({group, todo[i].first, todo[i].second}, std::move(built[i]));
    }
    std::set<ChunkKey> keep;
    for (const auto& [cx, cy] : wanted) {
        keep.insert({group, cx, cy});
    }
    const Rect view =
        wanted.empty() ? chunk_rect(0, 0) : chunk_rect(wanted.front().first, wanted.front().second);
    trim(group, view, keep);
    return todo.size();
}

Rect lookahead_rect(Rect view, double vx, double vy, double horizon_s, std::size_t max_chunks) {
    const double reach = static_cast<double>(max_chunks) * kChunk;
    const double dx = std::clamp(vx * horizon_s, -reach, reach);
    const double dy = std::clamp(vy * horizon_s, -reach, reach);
    // A chunk of margin on every side, the lookahead on the side of motion on top.
    const int left = kChunk + (dx < 0 ? static_cast<int>(std::ceil(-dx)) : 0);
    const int right = kChunk + (dx > 0 ? static_cast<int>(std::ceil(dx)) : 0);
    const int up = kChunk + (dy < 0 ? static_cast<int>(std::ceil(-dy)) : 0);
    const int down = kChunk + (dy > 0 ? static_cast<int>(std::ceil(dy)) : 0);
    return {view.x - left, view.y - up, view.w + left + right, view.h + up + down};
}

void FrameRenderer::track_velocity(Rect view, std::size_t group) {
    const auto now = std::chrono::steady_clock::now();
    const double cx = view.x + view.w / 2.0, cy = view.y + view.h / 2.0;
    if (last_centre_) {
        const double dt = std::chrono::duration<double>(now - last_time_).count();
        if (group != last_group_) {
            // A zoom step moves the centre by the cursor anchoring, not by a pan:
            // keep the velocity, in the new group's pixels.
            vx_ *= static_cast<double>(kTile[group].w) / kTile[last_group_].w;
            vy_ *= static_cast<double>(kTile[group].h) / kTile[last_group_].h;
        } else if (dt > 1e-4) {
            // Smoothed over about 150 ms: a mouse drag is noisy per frame.
            const double a = 1.0 - std::exp(-dt / 0.15);
            vx_ += a * ((cx - last_centre_->first) / dt - vx_);
            vy_ += a * ((cy - last_centre_->second) / dt - vy_);
        }
    }
    last_centre_ = std::pair{cx, cy};
    last_time_ = now;
    last_group_ = group;
}

Rect zoomed_view(Rect view, std::size_t from, std::size_t to) {
    const std::int64_t cx = std::int64_t{view.x} + std::int64_t{view.w} / 2;
    const std::int64_t cy = std::int64_t{view.y} + std::int64_t{view.h} / 2;
    auto scale = [](std::int64_t v, int a, int b) { return static_cast<int>(v * b / a); };
    const int x = scale(cx, kTile[from].w, kTile[to].w);
    const int y = scale(cy, kTile[from].h, kTile[to].h);
    return {x - view.w / 2, y - view.h / 2, view.w, view.h};
}

int stand_in_source(int p, int num, int den) {
    return static_cast<int>(
        div_euclid<std::int64_t>((std::int64_t{2} * p + 1) * num, std::int64_t{2} * den));
}

std::vector<std::size_t> stand_in_groups(std::size_t group) {
    std::vector<std::size_t> out;
    for (std::size_t d = 1; d < kGroups; ++d) {
        if (group >= d) {
            out.push_back(group - d);
        }
        if (group + d < kGroups) {
            out.push_back(group + d);
        }
    }
    return out;
}

namespace {

/// One axis of stand_in_spans: (source chunk, first and one past the last
/// destination pixel) for the destination pixels [d0, d1).
std::vector<std::array<int, 3>> stand_in_axis(int d0, int d1, int num, int den) {
    auto ceil_div = [](std::int64_t a, std::int64_t b) {
        return div_euclid<std::int64_t>(a + b - 1, b);
    };
    const int s0 = div_euclid(stand_in_source(d0, num, den), kChunk);
    const int s1 = div_euclid(stand_in_source(d1 - 1, num, den), kChunk);
    std::vector<std::array<int, 3>> out;
    for (int s = s0; s <= s1; ++s) {
        // The first destination pixel p whose centre maps to source pixel
        // `source` or beyond: the smallest p with (2p + 1) num >= 2 den source.
        const auto first = [&](std::int64_t source) {
            return ceil_div(2 * std::int64_t{den} * source - num, 2 * std::int64_t{num});
        };
        const int lo = std::max<int>(d0, static_cast<int>(first(std::int64_t{s} * kChunk)));
        const int hi = std::min<int>(d1, static_cast<int>(first((std::int64_t{s} + 1) * kChunk)));
        if (hi > lo) {
            out.push_back({s, lo, hi});
        }
    }
    return out;
}

} // namespace

std::vector<StandInSpan> stand_in_spans_in(Rect region, std::size_t to, std::size_t from) {
    std::vector<StandInSpan> out;
    if (to >= kGroups || from >= kGroups || region.w <= 0 || region.h <= 0) {
        return out;
    }
    const auto xs = stand_in_axis(region.x, region.x + region.w, kTile[from].w, kTile[to].w);
    const auto ys = stand_in_axis(region.y, region.y + region.h, kTile[from].h, kTile[to].h);
    for (const auto& y : ys) {
        for (const auto& x : xs) {
            out.push_back({x[0], y[0], Rect{x[1], y[1], x[2] - x[1], y[2] - y[1]}});
        }
    }
    return out;
}

std::vector<StandInSpan> stand_in_spans(int cx, int cy, std::size_t to, std::size_t from) {
    return stand_in_spans_in(chunk_rect(cx, cy), to, from);
}

std::vector<StandInPart> stand_in_cover(int cx, int cy, std::size_t to,
                                        const std::function<bool(const ChunkKey&)>& has) {
    std::vector<StandInPart> out;
    std::vector<Rect> open{chunk_rect(cx, cy)};
    for (const std::size_t from : stand_in_groups(to)) {
        if (open.empty()) {
            break;
        }
        std::vector<Rect> next;
        for (const Rect& region : open) {
            for (const StandInSpan& sp : stand_in_spans_in(region, to, from)) {
                if (has({from, sp.src_cx, sp.src_cy})) {
                    out.push_back({from, sp});
                } else {
                    next.push_back(sp.dst);
                }
            }
        }
        open = std::move(next);
    }
    // Parts of one source chunk that join into a rectangle are one draw (a hole
    // in the nearest group splits the regions of the next one).
    for (bool merged = true; merged;) {
        merged = false;
        for (std::size_t i = 0; i < out.size() && !merged; ++i) {
            for (std::size_t j = i + 1; j < out.size() && !merged; ++j) {
                StandInPart& a = out[i];
                const StandInPart& b = out[j];
                if (a.from != b.from || a.span.src_cx != b.span.src_cx ||
                    a.span.src_cy != b.span.src_cy) {
                    continue;
                }
                Rect& ra = a.span.dst;
                const Rect& rb = b.span.dst;
                if (ra.y == rb.y && ra.h == rb.h && (ra.x + ra.w == rb.x || rb.x + rb.w == ra.x)) {
                    ra = Rect{std::min(ra.x, rb.x), ra.y, ra.w + rb.w, ra.h};
                    merged = true;
                } else if (ra.x == rb.x && ra.w == rb.w &&
                           (ra.y + ra.h == rb.y || rb.y + rb.h == ra.y)) {
                    ra = Rect{ra.x, std::min(ra.y, rb.y), ra.w, ra.h + rb.h};
                    merged = true;
                }
                if (merged) {
                    out.erase(out.begin() + static_cast<std::ptrdiff_t>(j));
                }
            }
        }
    }
    return out;
}

std::optional<Chunk> stand_in_chunk(const FrameRenderer& frames, std::size_t group, int cx, int cy,
                                    Px open_water) {
    const auto cover = stand_in_cover(
        cx, cy, group, [&](const ChunkKey& k) { return frames.chunk(k) != nullptr; });
    if (cover.empty()) {
        return std::nullopt;
    }
    const auto n = to_size(kChunk);
    Chunk out;
    auto any = [&](auto member) {
        return std::any_of(cover.begin(), cover.end(), [&](const StandInPart& p) {
            return !(frames.chunk({p.from, p.span.src_cx, p.span.src_cy})->*member).empty();
        });
    };
    if (any(&Chunk::ground)) {
        out.ground.assign(n * n, rgb(open_water));
    }
    if (any(&Chunk::objects)) {
        out.objects.assign(n * n, Px{0, 0, 0, 0});
    }
    if (any(&Chunk::field)) {
        out.field.resize(n * n * 4);
        for (std::size_t i = 0; i < n * n; ++i) {
            out.field[i * 4] = out.field[i * 4 + 1] = out.field[i * 4 + 2] = 255;
            out.field[i * 4 + 3] = 1;
        }
    }
    if (any(&Chunk::terrain)) {
        out.terrain.assign(n * n, kNoTerrain);
    }
    if (any(&Chunk::base)) {
        out.base.assign(n * n, 255);
    }
    for (const StandInPart& part : cover) {
        const Chunk& src = *frames.chunk({part.from, part.span.src_cx, part.span.src_cy});
        const StandInSpan& span = part.span;
        const int nx = kTile[part.from].w, dx = kTile[group].w;
        const int ny = kTile[part.from].h, dy = kTile[group].h;
        out.water_below = out.water_below || src.water_below;
        out.water_above = out.water_above || src.water_above;
        const Rect r = span.dst;
        for (int y = r.y; y < r.y + r.h; ++y) {
            const int sy = stand_in_source(y, ny, dy);
            for (int x = r.x; x < r.x + r.w; ++x) {
                const int sx = stand_in_source(x, nx, dx);
                const std::size_t d = chunk_index(cx, cy, x, y);
                const std::size_t s = chunk_index(span.src_cx, span.src_cy, sx, sy);
                if (!src.ground.empty()) {
                    out.ground[d] = src.ground[s];
                }
                if (!src.objects.empty()) {
                    out.objects[d] = src.objects[s];
                }
                if (!src.field.empty()) {
                    std::copy_n(src.field.data() + s * 4, 4, out.field.data() + d * 4);
                }
                if (!src.terrain.empty()) {
                    out.terrain[d] = src.terrain[s];
                }
                if (!src.base.empty()) {
                    out.base[d] = src.base[s];
                }
            }
        }
    }
    out.ships =
        frames.chunk({cover.front().from, cover.front().span.src_cx, cover.front().span.src_cy})
            ->ships;
    return out;
}

std::vector<std::pair<int, int>> visible_chunks(Rect view) {
    const int x0 = div_euclid(view.x, kChunk), x1 = div_euclid(view.x + view.w - 1, kChunk);
    const int y0 = div_euclid(view.y, kChunk), y1 = div_euclid(view.y + view.h - 1, kChunk);
    std::vector<std::pair<int, int>> out;
    for (int cy = y0; cy <= y1; ++cy) {
        for (int cx = x0; cx <= x1; ++cx) {
            out.emplace_back(cx, cy);
        }
    }
    return out;
}

} // namespace anno::render
