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
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
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
};

/// (zoom group, chunk x, chunk y).
using ChunkKey = std::tuple<std::size_t, int, int>;

/// The static data of one chunk; with `ships` false the ships (units on
/// the sea surface) are left out of the objects layer.
Chunk render_chunk(const Scene& scene, std::size_t group, int cx, int cy, bool ships = true);

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
    /// Brings the chunks of `view` up to date; (built or received, ms spent,
    /// visible chunks still missing).
    std::tuple<std::size_t, double, std::size_t> update_chunks(const Scene& scene, Rect view,
                                                               std::size_t group);
    /// The cached chunk, if built.
    [[nodiscard]] const Chunk* chunk(const ChunkKey& key) const;
    /// Draws `view` at time_ms into `out` (view.w × view.h, row-major 0RGB).
    FrameStats render(const Scene& scene, Rect view, std::size_t group, std::uint64_t time_ms,
                      WaterStyle style, std::uint32_t* out);

    /// Worker threads per stage.
    std::size_t threads;
    /// Chunks kept besides the visible ones.
    std::size_t spare_chunks = 48;

  private:
    std::size_t request_chunks(std::size_t group, Rect view,
                               const std::vector<std::pair<int, int>>& visible);
    void trim(std::size_t group, Rect view, const std::set<ChunkKey>& keep);
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
/// Chunks (cx, cy) that overlap `view`, row by row.
std::vector<std::pair<int, int>> visible_chunks(Rect view);
/// Scene rectangle of chunk (cx, cy).
inline Rect chunk_rect(int cx, int cy) {
    return {cx * kChunk, cy * kChunk, kChunk, kChunk};
}

} // namespace anno::render
