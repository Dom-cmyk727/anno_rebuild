// Frame pipeline of the interactive view, independent of any window system:
// the viewer and benchmarks use the same code.
//
// Static data (ground, objects, the sea's per-pixel WaterField and whether
// animated water objects are present) is rendered once per kChunk-sized
// square of scene screen space and cached. A frame then composes, for the
// visible rectangle and split over row bands on all threads: cached ground
// → water objects below the waterline (at the frame time) → sea surface →
// water objects above it → cached objects.
//
// Chunks are built inside the frame (blocking: headless renders) or by
// background workers (ChunkWorkers): then a chunk that is not ready shows
// open sea for a few frames, and a ring of chunks around the view is built
// ahead, so panning never waits for chunk rendering.
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <anno/render/iso.hpp>
#include <anno/render/rect.hpp>
#include <anno/render/scene.hpp>

namespace anno::render {

/// Edge length of a cached chunk in screen pixels.
inline constexpr int kChunk = 512;
/// Scale of Chunk::terrain (and the GPU terrain texture).
inline constexpr float kTerrainScale = 64.0F;
/// Chunk::terrain where no island ground is drawn.
inline constexpr std::uint16_t kNoTerrain = 0xFFFF;

/// Cached static data of one chunk; empty vectors where nothing is drawn.
struct Chunk {
    /// Ground, opaque 0RGB; empty = no island ground (open-water colour).
    std::vector<std::uint32_t> ground;
    /// Objects except water objects, straight-alpha RGBA; empty = none.
    std::vector<Px> objects;
    /// Sea field (see sea::WaterField), four bytes per texel in the order
    /// cover, shore, depth, flags (the GPU's RGBA8Uint texel, uploaded as
    /// is); empty = open sea everywhere.
    std::vector<std::uint8_t> field;
    /// Terrain height seen at each pixel in 1/kTerrainScale HEIGHT units,
    /// kNoTerrain where no ground is drawn; empty = none.
    std::vector<std::uint16_t> terrain;
    /// Some island here has water objects below / above the waterline.
    bool water_below = false, water_above = false;
    /// Ships are drawn into `objects` (see render_chunk).
    bool ships = true;
    /// Canvas::base of `objects` (chunks without ships, for the GPU); empty
    /// = none.
    std::vector<std::uint8_t> base;
    /// The baked flow of enhanced rivers here (rivers.md 3.3); empty = this
    /// chunk has no river flow (no river pixels, or the flow is not ready).
    river::FlowPlane flow;
    /// Islands whose flow was not ready when the chunk was built: it shows the
    /// legacy river look on the correct mask until it is rebuilt (the window
    /// thread never waits for a flow, rivers.md 3.1).
    std::vector<std::uint32_t> flow_waiting;
    [[nodiscard]] bool flow_pending() const { return !flow_waiting.empty(); }
    /// A number that identifies this build of the chunk: every render_chunk()
    /// result and every inserted chunk gets a new one (never 0), so a consumer
    /// that keeps a copy (the GPU compositor's textures) sees that a chunk of
    /// the same key was rebuilt (e.g. after its river flow arrived).
    std::uint64_t serial = 0;
    /// The heap memory the chunk holds (its vectors' capacities), in bytes.
    [[nodiscard]] std::size_t bytes() const {
        auto of = [](const auto& v) { return v.capacity() * sizeof(v[0]); };
        return of(ground) + of(objects) + of(field) + of(terrain) + of(base) + of(flow.uv) +
               of(flow.aux) + of(flow.shear) + of(flow_waiting);
    }
};

/// (zoom group, chunk x, chunk y).
using ChunkKey = std::tuple<std::size_t, int, int>;

/// The static data of one chunk; with `ships` false the ships (units on
/// the sea surface) are left out of the objects layer.
///
/// Enhanced rivers: when the chunk touches an island with rivers it carries
/// the baked flow lattice (Chunk::flow). With `wait_for_flow` the flow of such
/// an island is built here if nobody has (blocking: headless, tests); without
/// it a flow that is not ready is queued on the scene's flow builders and the
/// chunk is tagged flow_pending (the background chunk workers).
Chunk render_chunk(const Scene& scene, std::size_t group, int cx, int cy, bool ships = true,
                   bool wait_for_flow = true);

/// Background chunk builders: a priority job list and a result queue. The
/// scene must outlive them. Destroying them stops the builders and waits for
/// the chunks in progress (at most one per thread; the Rust version waits at
/// the end of the viewer's thread scope instead).
class ChunkWorkers {
  public:
    ChunkWorkers(const Scene& scene, std::size_t threads);
    ~ChunkWorkers();
    ChunkWorkers(const ChunkWorkers&) = delete;
    ChunkWorkers& operator=(const ChunkWorkers&) = delete;

    /// Replaces the job list with `wanted` (priority order) minus the chunks
    /// being built, built with or without ships (render_chunk); whether any
    /// work is queued or running.
    bool submit(const std::vector<ChunkKey>& wanted, bool ships = true);
    /// Finished chunks since the last call.
    std::vector<std::pair<ChunkKey, Chunk>> take_done();
    /// Smoothed time one worker takes to build a chunk, in milliseconds (the
    /// latency of a request that finds a free worker); 100 until the first chunk.
    [[nodiscard]] double build_ms() const { return build_ms_.load(); }
    /// Takes chunks that are no longer needed and frees them on a
    /// background thread (freeing a chunk's vectors takes about a
    /// millisecond: the window thread must not).
    void discard(std::vector<Chunk> chunks);

  private:
    void run();
    void reap();

    const Scene& scene_;
    std::mutex mutex_;
    std::condition_variable ready_, garbage_ready_;
    std::vector<Chunk> garbage_;
    std::thread reaper_;
    std::deque<ChunkKey> jobs_;
    std::set<ChunkKey> busy_;
    /// Chunks whose build threw: not requested again (a deterministic
    /// failure would otherwise be retried every frame).
    std::set<ChunkKey> failed_;
    std::vector<std::pair<ChunkKey, Chunk>> done_;
    std::atomic<double> build_ms_{100.0};
    bool stop_ = false;
    bool ships_ = true;
    std::vector<std::thread> threads_;
};

/// Time spent per stage of one frame, in milliseconds.
struct FrameStats {
    std::size_t chunks_built = 0;
    double chunks_ms = 0;
    /// Visible chunks still being built (drawn as open sea).
    std::size_t chunks_missing = 0;
    double ground_ms = 0, water_objects_ms = 0, sea_ms = 0, objects_ms = 0, total_ms = 0;
};

/// Renders frames of a Scene into 0RGB buffers.
class FrameRenderer {
  public:
    /// `ships_in_chunks` false: chunks leave the ships out (for the GPU
    /// compositor, which draws them each frame); render() then omits them.
    explicit FrameRenderer(std::size_t threads, bool ships_in_chunks = true);
    [[nodiscard]] bool ships_in_chunks() const { return ships_; }

    /// Builds chunks with `workers` from now on (nullptr: in the frame).
    void set_workers(std::unique_ptr<ChunkWorkers> workers);
    /// Whether chunks are still being built in the background.
    [[nodiscard]] bool busy() const { return pending_; }
    [[nodiscard]] std::size_t cached() const { return cache_.size(); }
    /// The memory of the cached chunks (Chunk::bytes), O(cached chunks): for
    /// tools and reports, not per frame in the viewer.
    [[nodiscard]] std::size_t cached_bytes() const {
        std::size_t n = 0;
        for (const auto& [key, chunk] : cache_) {
            n += chunk.bytes();
        }
        return n;
    }
    /// Brings the chunks of `view` up to date; (built or received, ms spent,
    /// visible chunks still missing).
    std::tuple<std::size_t, double, std::size_t> update_chunks(const Scene& scene, Rect view,
                                                               std::size_t group);
    /// The chunks of the neighbouring zoom groups that update_chunks keeps built
    /// for the view a zoom step would show (a ring included), both groups
    /// interleaved, nearest to the view (as it will be in a moment) first. Not
    /// necessarily built yet.
    [[nodiscard]] const std::vector<ChunkKey>& adjacent_keys() const { return adjacent_; }
    /// The chunks of the group shown that update_chunks keeps built around the
    /// visible ones (the margin, grown in the direction of motion), the ones the
    /// motion brings into view first at the front. Not necessarily built yet.
    [[nodiscard]] const std::vector<ChunkKey>& margin_keys() const { return margin_; }
    /// The cached chunk, if built.
    [[nodiscard]] const Chunk* chunk(const ChunkKey& key) const;
    /// Puts `chunk` in the cache under `key` (tests and tools: a synthetic
    /// chunk without a scene to build it from); it gets a new serial.
    void insert_chunk(const ChunkKey& key, Chunk chunk);
    /// Draws `view` at time_ms into `out` (view.w × view.h, row-major 0RGB).
    FrameStats render(const Scene& scene, Rect view, std::size_t group, std::uint64_t time_ms,
                      WaterStyle style, std::uint32_t* out);

    /// Enhanced water: the river look render() draws (chunks carry the lattice
    /// either way, so switching needs no rebuild).
    RiverFlowMode river_flow = RiverFlowMode::Flow;
    /// The cost tier of the flow shading render() uses (RiverParams::quality): High
    /// is the reference look; the GPU compositor's automatic tier (GpuFrame::
    /// river_quality) can be Medium or Low, which a CPU reference compared against
    /// it must follow.
    RiverQuality river_quality = RiverQuality::High;
    /// Worker threads per stage.
    std::size_t threads;
    /// Chunks kept besides the visible ones.
    std::size_t spare_chunks = 48;
    /// Most chunks the margin around the view grows in the direction the view is
    /// moving (lookahead_rect). Each chunk-wide strip is (view height / kChunk + 2)
    /// chunks of up to 4.3 MB (a land chunk; sea chunks are far smaller) in the
    /// current group and in each neighbouring one, on top of spare_chunks.
    /// Counted for 2560 x 1440, three groups, cap 3: 144 chunks kept standing,
    /// about 200-216 on a pan along one axis, 297 on a diagonal, plus 48 spare:
    /// 0.8 / 1.1 / 1.5 GB at worst. Measured on Single_24.sze (the largest
    /// scene) with a pan while zooming: peak 0.44-0.71 GB, mean 0.36-0.54 GB,
    /// and cap 2 within 40 MB of cap 3 (docs/perf/README.md, T1e). 3 covers
    /// 1500 px of travel; the viewer's --lookahead-chunks sets it; 0 = a plain
    /// ring of one chunk.
    std::size_t max_lookahead_chunks = 3;
    /// render() draws a visible chunk that is not built from the nearest zoom
    /// group's chunks (stand_in_chunk) instead of open sea. Off by default: the
    /// CPU path is the reference and the tests' digests; the GPU compositor has
    /// its own switch (GpuLimits::stand_ins) and does the same on the card.
    bool stand_ins = false;
    /// Keep the built chunks of other zoom groups that would stand in for a
    /// visible chunk still being built, past the eviction in update_chunks. Set
    /// by GpuFrame::render from GpuLimits::stand_ins; also on with `stand_ins`.
    /// Off, the eviction is the one from before stand-ins existed.
    bool retain_stand_in_sources = false;
    /// Visible chunks that are built but that the GPU compositor still draws
    /// over their stand-in (not uploaded yet, or dissolving in over it for
    /// GpuLimits::chunk_fade_ms). Set by GpuFrame::render before update_chunks;
    /// their stand-in sources are kept like those of chunks not built.
    std::set<ChunkKey> shown_over_stand_in;
    /// What update_chunks keeps beyond the visible chunks for the stand-ins of
    /// the visible chunks that are not built or in shown_over_stand_in (empty
    /// when both switches are off).
    [[nodiscard]] std::set<ChunkKey>
    stand_in_retain(std::size_t group, const std::vector<std::pair<int, int>>& visible) const;
    /// The smoothed velocity of the view centre that sizes the margin, in scene
    /// pixels per second of the group shown (x, y).
    [[nodiscard]] std::pair<double, double> velocity() const { return {vx_, vy_}; }
    /// The time source of the velocity (a steady clock; tests step it by hand).
    std::function<std::chrono::steady_clock::time_point()> clock = [] {
        return std::chrono::steady_clock::now();
    };

  private:
    std::vector<ChunkKey> adjacent_, margin_;
    /// Velocity of the view centre in scene pixels per second of the group shown
    /// (smoothed), the centre and time of the last update_chunks, and its group.
    double vx_ = 0, vy_ = 0;
    std::optional<std::pair<double, double>> last_centre_;
    std::chrono::steady_clock::time_point last_time_;
    std::size_t last_group_ = 0;
    void track_velocity(Rect view, std::size_t group);
    /// The chunks to keep and to build, most urgent first (`keep` is filled
    /// with them), and adjacent_: the visible chunks, the margin around them
    /// grown in the direction of motion, and the neighbouring groups' views.
    std::vector<ChunkKey> plan_margin(Rect view, std::size_t group,
                                      const std::vector<std::pair<int, int>>& visible,
                                      double horizon_s, std::set<ChunkKey>& keep);
    std::size_t request_chunks(const Scene& scene, std::size_t group, Rect view,
                               const std::vector<std::pair<int, int>>& visible);
    void trim(std::size_t group, Rect view, const std::set<ChunkKey>& keep,
              const std::set<ChunkKey>& retain = {});
    std::size_t fill_cache(const Scene& scene, std::size_t group,
                           const std::vector<std::pair<int, int>>& wanted);

    std::map<ChunkKey, Chunk> cache_;
    std::unique_ptr<ChunkWorkers> workers_;
    bool pending_ = false;
    bool ships_ = true;
};

/// The rectangle of view's size at group `to` centred on the scene point
/// that `view` centres on at group `from`.
Rect zoomed_view(Rect view, std::size_t from, std::size_t to);

/// The area whose chunks are kept built around `view`: a ring of one chunk, and
/// where the view is moving (velocity vx, vy in scene pixels per second) the
/// distance it covers in `horizon_s` seconds on that side, at most
/// `max_chunks` chunks: the time to build a chunk (ChunkWorkers::build_ms) is
/// long against how fast a view can be dragged, so what comes into view must be
/// requested that much earlier.
Rect lookahead_rect(Rect view, double vx, double vy, double horizon_s, std::size_t max_chunks);

// ---- Stand-in chunks (open-questions OQ-077) ----
//
// A visible chunk that is not built yet (a zoom step to a group nobody has
// built here, an upload still waiting) shows the same screen area of the
// nearest zoom group whose chunks exist, scaled to the tile size of the group
// shown: scene pixels are linear in tile coordinates per group, so the scale
// of an axis is kTile[to] / kTile[from] there (2 between the first two groups
// of the original, 2.1 between the next two: derived from kTile, never
// written as a literal). The scaling is nearest-neighbour at pixel centres,
// the same integer rule on the CPU reference (stand_in_chunk) and in the GPU
// compositor (gpu.wgsl, Piece), and it keeps the world alignment: a stand-in
// next to a built chunk shows the same ground at the same place, up to the
// rounding of the tile sizes. Rivers keep the legacy look on the stand-in's
// mask (no flow lattice is scaled); animated layers (water objects, sea,
// ships) are drawn at the group shown as always.

/// Source pixel (on one axis) that destination pixel `p` shows: the pixel
/// containing the centre of `p` after scaling by num / den (the tile size of
/// the source group over the one of the destination group on that axis),
/// floor((2p + 1) * num / (2 den)).
int stand_in_source(int p, int num, int den);

/// The zoom groups whose chunks may stand in for a chunk of `group`, nearest
/// first, the finer group (smaller index, larger tiles) first at equal
/// distance. One entry per other group.
std::vector<std::size_t> stand_in_groups(std::size_t group);

/// The part of a destination chunk that one source chunk shows.
struct StandInSpan {
    /// Chunk of the source group.
    int src_cx = 0, src_cy = 0;
    /// Scene pixels (destination group) of the destination chunk whose centre
    /// lies in that source chunk after scaling.
    Rect dst;
};
/// The source chunks (group `from`) that chunk (cx, cy) of group `to` shows,
/// with their parts of it; the parts tile the destination chunk exactly, row by
/// row. O(chunks covered) = at most ceil(ratio) + 1 per axis.
std::vector<StandInSpan> stand_in_spans(int cx, int cy, std::size_t to, std::size_t from);

/// As stand_in_spans for the destination pixels `region` only (the parts tile
/// `region`).
std::vector<StandInSpan> stand_in_spans_in(Rect region, std::size_t to, std::size_t from);

/// A part of a stand-in and the zoom group its source chunk is in.
struct StandInPart {
    std::size_t from = 0;
    StandInSpan span;
};
/// What chunk (cx, cy) of group `to` shows when it is not built: per part of
/// it, the nearest group (stand_in_groups order) for which `has` says the
/// source chunk exists. A part whose source chunk is missing in the nearest
/// group is split by the source chunks of the next group, and so on; what no
/// group covers is absent (open sea). The parts are disjoint. Shared by the
/// CPU reference and the GPU compositor, so both choose alike.
std::vector<StandInPart> stand_in_cover(int cx, int cy, std::size_t to,
                                        const std::function<bool(const ChunkKey&)>& has);

/// The CPU reference of a stand-in: chunk (cx, cy) of `group` built from
/// stand_in_cover over the chunks `frames` holds (pixels no group covers stay
/// open sea of colour `open_water`). nullopt when there is none. The result carries no
/// flow lattice and no serial. The `base` plane (an object pixel's height
/// above its lowest row, in rows of the source group) is copied texel by texel,
/// so on a stand-in its values are in source-group rows, not scaled to the
/// destination group; the GPU sampling does the same. Cost: one pass over the planes, ~5 MB.
std::optional<Chunk> stand_in_chunk(const FrameRenderer& frames, std::size_t group, int cx, int cy,
                                    Px open_water);

/// Chunks (cx, cy) that overlap `view`, row by row.
std::vector<std::pair<int, int>> visible_chunks(Rect view);
/// Scene rectangle of chunk (cx, cy).
inline Rect chunk_rect(int cx, int cy) {
    return {cx * kChunk, cy * kChunk, kChunk, kChunk};
}

} // namespace anno::render
