#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <optional>

#include <anno/render/frame.hpp>
#include <anno/render/sea.hpp>

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

Chunk render_chunk(const Scene& scene, std::size_t group, int cx, int cy, bool ships,
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
        try {
            chunk = render_chunk(scene_, group, cx, cy, ships, false);
        } catch (const std::exception& e) {
            // E.g. out of memory: the chunk stays missing (open sea) and is not
            // requested again; the worker keeps running.
            std::fprintf(stderr, "chunk (%zu, %d, %d) failed: %s\n", group, cx, cy, e.what());
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

std::tuple<std::size_t, double, std::size_t>
FrameRenderer::update_chunks(const Scene& scene, Rect view, std::size_t group) {
    const auto t = Clock::now();
    const auto chunks = visible_chunks(view);
    const std::size_t built =
        workers_ ? request_chunks(scene, group, view, chunks) : fill_cache(scene, group, chunks);
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
    auto get = [&](int cx, int cy) -> const Chunk& {
        const auto it = cache_.find({group, cx, cy});
        return it == cache_.end() ? empty : it->second;
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
        const auto it = cache_.find({group, cx, cy});
        if (it == cache_.end()) {
            return std::nullopt;
        }
        const std::size_t i = chunk_index(cx, cy, sx, sy);
        if (i >= it->second.terrain.size()) {
            return std::nullopt;
        }
        const std::uint16_t v = it->second.terrain[i];
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
        const RiverParams river_params = scene.river_cfg.params();
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

std::size_t FrameRenderer::request_chunks(const Scene& scene, std::size_t group, Rect view,
                                          const std::vector<std::pair<int, int>>& visible) {
    std::size_t received = 0;
    for (auto& [key, chunk] : workers_->take_done()) {
        // Workers shared with a renderer of the other ship setting: rebuilt.
        if (chunk.ships != ships_) {
            continue;
        }
        cache_.insert_or_assign(key, std::move(chunk));
        ++received;
    }
    const auto ring = visible_chunks(
        Rect{view.x - kChunk, view.y - kChunk, view.w + 2 * kChunk, view.h + 2 * kChunk});
    std::set<ChunkKey> keep;
    for (const auto& [cx, cy] : visible) {
        keep.insert({group, cx, cy});
    }
    for (const auto& [cx, cy] : ring) {
        keep.insert({group, cx, cy});
    }
    // Once the current group is (nearly) done, prepare the same area in the
    // neighbouring zoom groups, so zooming shows islands at once.
    std::size_t missing = 0;
    for (const auto* list : {&visible, &ring}) {
        for (const auto& [cx, cy] : *list) {
            missing += cache_.contains({group, cx, cy}) ? 0U : 1U;
        }
    }
    if (missing <= threads) {
        for (const std::size_t g : {group - 1, group + 1}) {
            // group − 1 wraps to SIZE_MAX for group 0 and is skipped.
            if (g < kGroups) {
                for (const auto& [cx, cy] : visible_chunks(zoomed_view(view, group, g))) {
                    keep.insert({g, cx, cy});
                }
            }
        }
    }
    // Visible chunks first, then the ring, then other zoom groups.
    std::vector<ChunkKey> order(keep.begin(), keep.end());
    auto rank = [&](const ChunkKey& k) {
        const auto [g, cx, cy] = k;
        const bool vis =
            std::find(visible.begin(), visible.end(), std::pair{cx, cy}) != visible.end();
        return std::pair{g != group, !vis};
    };
    std::stable_sort(order.begin(), order.end(),
                     [&](const ChunkKey& a, const ChunkKey& b) { return rank(a) < rank(b); });
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
    trim(group, view, keep);
    return received;
}

void FrameRenderer::trim(std::size_t group, Rect view, const std::set<ChunkKey>& keep) {
    if (cache_.size() <= keep.size() + spare_chunks) {
        return;
    }
    const int ccx = div_euclid(view.x + view.w / 2, kChunk);
    const int ccy = div_euclid(view.y + view.h / 2, kChunk);
    std::vector<std::tuple<bool, int, ChunkKey>> keys;
    for (const auto& [k, c] : cache_) {
        if (keep.contains(k)) {
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

Rect zoomed_view(Rect view, std::size_t from, std::size_t to) {
    const std::int64_t cx = std::int64_t{view.x} + std::int64_t{view.w} / 2;
    const std::int64_t cy = std::int64_t{view.y} + std::int64_t{view.h} / 2;
    auto scale = [](std::int64_t v, int a, int b) { return static_cast<int>(v * b / a); };
    const int x = scale(cx, kTile[from].w, kTile[to].w);
    const int y = scale(cy, kTile[from].h, kTile[to].h);
    return {x - view.w / 2, y - view.h / 2, view.w, view.h};
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
