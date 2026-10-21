// The residency of sprite frames in the GPU atlas, as a CPU model
// (docs/render/gpu-chunks.md 4.2-4.3). No GPU: it decides which frame lives
// where and what is evicted; the compositor (stage 2) mirrors its decisions
// with texture writes. Everything is deterministic (ordered containers, no
// clock): the same calls give the same places.
//
// The atlas is an array of layers of atlas_size x atlas_size texels (4 bytes
// each); the budget in bytes bounds how many layers there are. A layer holds
// frames of one class: Static (chunk frames, used only while a chunk pass is
// recorded) or Hot (water-object animation, later moving units: used every
// frame), so the LRU of the static class is not disturbed by the hot frames.
// Frames are packed on shelves; there is no repacking. Eviction empties a whole
// layer: the least recently used layer of the class that is not pinned.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <anno/render/frame_table.hpp>

namespace anno::render {

enum class AtlasClass : std::uint8_t { Static = 0, Hot = 1 };

/// A frame to make resident: its id and the size of its image in texels.
struct FrameRef {
    FrameId id = 0;
    std::uint32_t width = 0, height = 0;
};

/// Where a frame lies in the atlas.
struct AtlasPlace {
    std::uint32_t layer = 0, x = 0, y = 0, width = 0, height = 0;
    bool operator==(const AtlasPlace&) const = default;
};

enum class EnsureStatus {
    /// Every frame was resident already.
    Resident,
    /// The missing frames were placed (EnsureResult::uploads).
    Uploaded,
    /// The missing frames need more bytes than the caller's upload budget:
    /// nothing was placed.
    Deferred,
    /// The frames cannot all be made resident (a frame larger than a layer, or
    /// the budget is spent by pinned layers): the caller builds the chunk on the
    /// CPU. Counted in ResidencyStats::chunks_cpu_fallback. Frames placed before
    /// the failure stay resident.
    Overflow,
};

struct EnsureResult {
    EnsureStatus status = EnsureStatus::Resident;
    /// The frames placed by this call, to be written into the atlas.
    std::vector<std::pair<FrameId, AtlasPlace>> uploads;
    /// Texel bytes of `uploads`.
    std::uint64_t upload_bytes = 0;
};

struct ResidencyStats {
    std::size_t layers = 0, resident_frames = 0;
    /// Texel bytes of the resident frames and of all layers.
    std::uint64_t resident_bytes = 0, layer_bytes = 0;
    std::uint64_t uploaded_frames = 0, uploaded_bytes = 0, evicted_layers = 0, evicted_frames = 0;
    /// GpuStats::chunks_cpu_fallback: ensure() calls that ended in Overflow.
    std::uint64_t chunks_cpu_fallback = 0;
};

class AtlasResidency {
  public:
    /// An atlas of layers of `atlas_size`^2 texels that may use `budget_bytes`
    /// in all (whole layers; a budget below one layer holds none).
    AtlasResidency(std::uint32_t atlas_size, std::uint64_t budget_bytes);

    /// The next frame: layers used before it are no longer pinned.
    void begin_frame() { ++frame_; }

    /// Makes `frames` resident in layers of class `cls` and pins the layers they
    /// are in for this frame (a frame already resident in another class is
    /// used where it is). `max_upload_bytes` bounds the texel bytes this call
    /// may place. Never evicts a pinned layer.
    EnsureResult ensure(std::span<const FrameRef> frames, AtlasClass cls,
                        std::uint64_t max_upload_bytes = std::numeric_limits<std::uint64_t>::max());

    [[nodiscard]] std::optional<AtlasPlace> place(FrameId id) const;
    [[nodiscard]] bool resident(FrameId id) const { return places_.contains(id); }
    /// The frames evicted since the last call, in eviction order (the frame
    /// table buffer marks them non-resident).
    std::vector<FrameId> take_evicted() { return std::exchange(evicted_, {}); }
    [[nodiscard]] ResidencyStats stats() const;
    [[nodiscard]] std::uint32_t atlas_size() const { return size_; }
    /// Layers the budget allows.
    [[nodiscard]] std::size_t max_layers() const { return max_layers_; }

  private:
    struct Layer {
        AtlasClass cls = AtlasClass::Static;
        /// The frame number of the last ensure() that read a frame here.
        std::uint64_t last_used = 0;
        /// Shelf cursor: next free x on the shelf at y, the shelf's height.
        std::uint32_t x = 0, y = 0, shelf = 0;
        std::vector<FrameId> frames;
    };
    std::optional<AtlasPlace> allocate(AtlasClass cls, std::uint32_t w, std::uint32_t h);
    static std::optional<AtlasPlace> shelf_fit(Layer& layer, std::uint32_t size, std::uint32_t w,
                                               std::uint32_t h);
    void evict(std::size_t layer);

    std::uint32_t size_;
    std::size_t max_layers_;
    /// Starts at 1 so that a fresh layer (last_used 0) is never pinned.
    std::uint64_t frame_ = 1;
    std::vector<Layer> layers_;
    std::map<FrameId, AtlasPlace> places_;
    std::vector<FrameId> evicted_;
    ResidencyStats stats_;
};

} // namespace anno::render
