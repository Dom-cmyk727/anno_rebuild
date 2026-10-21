// A chunk's draw list: what a builder must know to composite the chunk's
// objects layer, as fixed-width records (docs/render/gpu-chunks.md section 3).
//
// The CPU builder builds the list and replays it with blit_sprite (replay());
// the GPU builder (stage 2) will upload the same list. That the replay
// reproduces the chunk bit for bit is the proof that the list carries
// everything: the order, the island clipping, the shadows, the base plane and
// the soldiers.
//
// Records are little-endian, written and read field by field; no record is
// ever a struct dump. The list holds ids into a FrameTable, never images.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <anno/render/frame_table.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/rect.hpp>

namespace anno::render {

/// Bytes of one objects record: the existing sprite instance stride of the GPU
/// compositor (gpu.cpp kInstanceBytes).
inline constexpr std::size_t kObjectRecordBytes = 32;
/// Bytes of one ground record (section 3.2).
inline constexpr std::size_t kGroundRecordBytes = 32;
/// ObjectRecord::clip of a draw that is not clipped to an island: it goes
/// straight onto the chunk canvas (ships, drawn after all islands; chunks for
/// the GPU compositor have none).
inline constexpr std::uint32_t kClipNone = 0xFFFFFFFFU;

/// ObjectRecord::flags.
namespace object_flags {
/// The pixels become kShadow (no base write).
inline constexpr std::uint32_t kShadow = 1U << 0;
/// Canvas::base is tracked for this draw (a non-shadow draw into a chunk that
/// is built with bases).
inline constexpr std::uint32_t kTrackBase = 1U << 1;
/// The object's height byte (HEIGHT units) in bits 8-15: for the water
/// objects' depth test and debugging.
inline constexpr std::uint32_t kHeightShift = 8;
inline constexpr std::uint32_t kHeightMask = 0xFFU << kHeightShift;
} // namespace object_flags

/// One sprite draw (section 3.1). The in-memory form of the 32 bytes.
struct ObjectRecord {
    /// Scene position of the frame box's top-left corner: the anchor point
    /// minus the frame's signed anchor.
    std::int32_t x = 0, y = 0;
    FrameId frame = 0;
    std::uint32_t flags = 0;
    /// Index into ChunkDraws::clips, or kClipNone.
    std::uint32_t clip = 0;
    /// Colour variant; 0 = the decoded image as it is (reserved, section 3.1).
    std::uint32_t colour = 0;
    /// The original's sprite depth (reserved, 0: section 3.1).
    std::int32_t depth = 0;
    bool operator==(const ObjectRecord&) const = default;
};

/// Appends the 32 bytes of `r`: x, y, frame, flags, clip, colour, depth, 0.
void append_object_record(std::vector<std::uint8_t>& out, const ObjectRecord& r);
/// Record `index` of a list of 32-byte records.
ObjectRecord read_object_record(std::span<const std::uint8_t> list, std::size_t index);

/// One ground tile (section 3.2). Defined for stage 2b; no builder writes it
/// yet (the ground stays on the CPU, decision C2).
struct GroundRecord {
    /// Scene position of the tile's top vertex at height 0.
    std::int32_t ox = 0, oy = 0;
    /// Layer in the ground texture array (0 = open water).
    std::uint32_t texture = 0;
    /// The GroundCell bits (u, v, rotation), widened.
    std::uint32_t cell = 0;
    /// Corner heights A (x, y), C (x + 1, y) and B (x, y + 1), D (x + 1, y + 1),
    /// in HEIGHT units.
    std::uint16_t h_a = 0, h_c = 0, h_b = 0, h_d = 0;
    std::uint32_t clip = 0;
    bool operator==(const GroundRecord&) const = default;
};
/// Appends the 32 bytes of `r`: ox, oy, texture, cell, h_a | h_c << 16,
/// h_b | h_d << 16, clip, 0.
void append_ground_record(std::vector<std::uint8_t>& out, const GroundRecord& r);
GroundRecord read_ground_record(std::span<const std::uint8_t> list, std::size_t index);

/// The clip table as the GPU reads it: x0, y0, x1, y1 (i32, x1 / y1 exclusive)
/// per entry.
std::vector<std::uint8_t> clip_table_bytes(std::span<const Rect> clips);

/// Pixels of a bin edge when the options give none. The objects pass of the GPU
/// walks, per pixel, the records of the pixel's bin: with 512 px chunks, 32 px
/// bins are 256 bins per chunk, small enough that a bin's list holds the few
/// records that can touch its pixels (the largest frame, 682 x 651 px at group
/// 0, touches 22 x 21 bins) and large enough that the index list (instances x
/// bins touched: 10-40 KB per chunk) stays small against the 3.9 MB of texels
/// it replaces. A tuning value; stage 2 measures it (gpu-chunks.md section 9).
inline constexpr std::uint32_t kDefaultBinPx = 32;

/// The draw list of one chunk's objects layer.
struct ChunkDraws {
    /// The chunk's scene rectangle (the canvas the list is drawn into).
    Rect rect;
    std::size_t group = 0;
    /// The chunk tracks Canvas::base; base_band is Canvas::base_band.
    bool bases = false;
    int base_band = 0;
    /// The sprite draws in blit order, kObjectRecordBytes each.
    std::vector<std::uint8_t> objects;
    /// Per island segment: its bounds intersected with `rect` (scene pixels).
    std::vector<Rect> clips;
    /// The sorted distinct frame ids the records name (residency, section 4.3).
    std::vector<FrameId> frame_set;
    /// Bins (objects only): the chunk cut into bin_px x bin_px squares, row by
    /// row (bins_x per row); per bin the indices of the records whose box
    /// (image size, clipped to the record's clip and the chunk) overlaps it, in
    /// list order, as CSR: bin b holds bin_indices[bin_offsets[b] ..
    /// bin_offsets[b + 1]). bin_offsets is empty when there are no records.
    std::uint32_t bin_px = kDefaultBinPx, bins_x = 0, bins_y = 0;
    std::vector<std::uint32_t> bin_offsets, bin_indices;

    [[nodiscard]] std::size_t object_count() const { return objects.size() / kObjectRecordBytes; }
    [[nodiscard]] ObjectRecord object(std::size_t i) const {
        return read_object_record(objects, i);
    }
    /// The heap memory the list holds, in bytes.
    [[nodiscard]] std::size_t bytes() const {
        auto of = [](const auto& v) { return v.capacity() * sizeof(v[0]); };
        return of(objects) + of(clips) + of(frame_set) + of(bin_offsets) + of(bin_indices);
    }
};

/// Collects the records of one chunk in blit order and finishes them into a
/// ChunkDraws (frame set and bins).
class ChunkDrawsBuilder {
  public:
    ChunkDrawsBuilder(Rect rect, std::size_t group, bool bases, int base_band,
                      std::uint32_t bin_px = kDefaultBinPx);
    /// The following records belong to an island whose canvas is `clip`; the
    /// clip table entry exists only if a record follows.
    void begin_island(Rect clip);
    /// Records that are not clipped to an island (kClipNone).
    void begin_unclipped();
    /// Appends a record of the current segment (its `clip` is set here).
    void add(ObjectRecord r);
    /// Records so far.
    [[nodiscard]] std::size_t size() const { return draws_.object_count(); }
    /// The frame set and the bins, from the frames' image sizes in `frames`.
    ChunkDraws finish(const FrameTable& frames);

  private:
    ChunkDraws draws_;
    std::vector<FrameId> ids_;
    Rect pending_{};
    bool has_pending_ = false, unclipped_ = false;
    std::uint32_t clip_ = kClipNone;
};

/// Draws the list into a new canvas over `draws.rect` (with the base plane when
/// `draws.bases`), as Scene::render does: records in list order with
/// blit_sprite, one local canvas per island segment that is composited when the
/// segment ends (the base plane follows composite()'s rule), unclipped records
/// straight onto the chunk canvas. A record whose frame the table does not hold
/// is skipped and counted in `*missing` (a builder bug: never expected).
Canvas replay(const ChunkDraws& draws, const FrameTable& frames, std::size_t* missing = nullptr);

} // namespace anno::render
