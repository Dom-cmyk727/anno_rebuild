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

Chunk render_chunk(const Scene& scene, std::size_t group, int cx, int cy) {
    const Rect rect = chunk_rect(cx, cy);
    std::vector<std::size_t> islands;
    for (const auto& p : scene.layouts[group]) {
        if (p.bounds.intersect(rect)) {
            islands.push_back(p.index);
        }
    }
    Layers objects_only = Layers::none();
    objects_only.objects = true;
    if (islands.empty()) {
        // Open sea; only units (ships) can add an objects layer.
        if (!scene.units_touch(rect, group)) {
            return {};
        }
        Chunk c;
        c.objects = to_px(scene.render(rect, group, objects_only).first);
        return c;
    }
    Chunk c;
    const Px water = scene.textures.water();
    auto [ground, probe] = scene.ground_with_probe(rect, group);
    c.ground.reserve(ground.width * ground.height);
    for (std::size_t i = 0; i < ground.width * ground.height; ++i) {
        Px px = water;
        over(px.data(), &ground.pixels[i * 4]);
        c.ground.push_back(rgb(px));
    }
    c.objects = to_px(scene.render(rect, group, objects_only).first);
    if (std::all_of(c.objects.begin(), c.objects.end(), [](const Px& p) { return p[3] == 0; })) {
        c.objects.clear();
    }
    auto field = scene.water_field_from(rect, probe);
    c.cover = std::move(field.cover);
    c.shore = std::move(field.shore);
    c.depth = std::move(field.depth);
    c.flags = std::move(field.flags);
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
    return c;
}

ChunkWorkers::ChunkWorkers(const Scene& scene, std::size_t threads) : scene_(scene) {
    for (std::size_t i = 0; i < std::max<std::size_t>(threads, 1); ++i) {
        threads_.emplace_back([this] { run(); });
    }
}

ChunkWorkers::~ChunkWorkers() {
    {
        const std::lock_guard lock(mutex_);
        stop_ = true;
    }
    ready_.notify_all();
    for (auto& t : threads_) {
        t.join();
    }
}

void ChunkWorkers::run() {
    for (;;) {
        ChunkKey key;
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
            if (stop_) {
                return;
            }
            key = jobs_.front();
            jobs_.pop_front();
            busy_.insert(key);
        }
        const auto [group, cx, cy] = key;
        std::optional<Chunk> chunk;
        try {
            chunk = render_chunk(scene_, group, cx, cy);
        } catch (const std::exception& e) {
            // E.g. out of memory: the chunk stays missing (open sea) and is
            // requested again; the worker keeps running.
            std::fprintf(stderr, "chunk (%zu, %d, %d) failed: %s\n", group, cx, cy, e.what());
        }
        const std::lock_guard lock(mutex_);
        busy_.erase(key);
        if (chunk) {
            done_.emplace_back(key, std::move(*chunk));
        }
    }
}

bool ChunkWorkers::submit(const std::vector<ChunkKey>& wanted) {
    const std::lock_guard lock(mutex_);
    jobs_.clear();
    for (const auto& k : wanted) {
        if (!busy_.contains(k)) {
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

FrameRenderer::FrameRenderer(std::size_t threads_) : threads(std::max<std::size_t>(threads_, 1)) {}

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
        workers_ ? request_chunks(group, view, chunks) : fill_cache(scene, group, chunks);
    std::size_t missing = 0;
    for (const auto& [cx, cy] : chunks) {
        missing += cache_.contains({group, cx, cy}) ? 0 : 1;
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
                    if (c.cover.empty()) {
                        return {255, 255, 255, 1};
                    }
                    const std::size_t s = chunk_index(ccx, ccy, rr.x + static_cast<int>(i % rw),
                                                      rr.y + static_cast<int>(i / rw));
                    return {c.cover[s], c.shore[s], c.depth[s], c.flags[s]};
                };
                sea::shade_sea(sea::XrgbTarget{band + d}, to_size(view.w), *r, field, *pattern,
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

std::size_t FrameRenderer::request_chunks(std::size_t group, Rect view,
                                          const std::vector<std::pair<int, int>>& visible) {
    std::size_t received = 0;
    for (auto& [key, chunk] : workers_->take_done()) {
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
            missing += cache_.contains({group, cx, cy}) ? 0 : 1;
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
    std::vector<ChunkKey> todo;
    for (const auto& k : order) {
        if (!cache_.contains(k)) {
            todo.push_back(k);
        }
    }
    pending_ = workers_->submit(todo);
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
    for (std::size_t i = spare_chunks; i < keys.size(); ++i) {
        cache_.erase(std::get<2>(keys[i]));
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
        return render_chunk(scene, group, c.first, c.second);
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
