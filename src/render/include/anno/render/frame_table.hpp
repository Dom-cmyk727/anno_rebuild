// The scene-wide table of sprite frames that chunk draw lists name
// (docs/render/gpu-chunks.md 4.1).
//
// A draw list (chunk_draws.hpp) does not hold images: it holds dense `FrameId`s
// into this table. The table interns a frame the first time a list references
// it (on the worker that builds the list: the decode happens there, not on the
// window thread) and keeps what a consumer needs of it: the frame header (the
// anchor), the decoded image and the per-column keel row that Canvas::base is
// measured against, computed once with the image instead of per blit.
//
// Ids are dense `u32` in the order frames are first interned. That order depends
// on which worker gets there first, so the ids of two runs differ; within one
// scene (one table) an id never changes meaning and is never reused, also when
// the GPU evicts the frame from its atlas (residency.hpp).
#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <utility>
#include <vector>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/render/iso.hpp>

namespace anno::render {

/// Index of a frame in a FrameTable.
using FrameId = std::uint32_t;

/// What a frame belongs to.
enum class FrameKind : std::uint8_t {
    /// An Anlagen.dat sprite: the key is a SpriteKey.
    Building = 0,
    /// A frame of a Scene::units entry: (unit index, 0, shadow, group, 0).
    Unit = 1,
};

/// The identity of a frame within a scene. Not stable across scenes (a unit's
/// index is its place in Scene::units).
struct FrameKey {
    FrameKind kind = FrameKind::Building;
    SpriteKey sprite;
    auto operator<=>(const FrameKey&) const = default;

    /// The key of the frame `key` of a building sprite.
    static FrameKey building(const SpriteKey& key) { return {FrameKind::Building, key}; }
    /// The frame (or its shadow) of unit `unit` at `group`.
    static FrameKey unit(std::size_t unit, std::size_t group, bool shadow) {
        return {FrameKind::Unit, SpriteKey{static_cast<std::uint32_t>(unit), 0, shadow,
                                           static_cast<std::uint32_t>(group), 0}};
    }
};

/// What an interned frame holds. Immutable once in the table.
struct FrameData {
    /// The frame header: anchor_x / anchor_y (signed) and the frame box.
    formats::figuren::Frame header;
    /// The decoded image; its size may exceed the frame box (rows past the
    /// frame width, docs/re/annodisplay.md, OQ-025).
    std::shared_ptr<const Rgba> image;
    /// keel_rows(*image): the lowest opaque row of each image column.
    std::vector<std::uint16_t> keel;

    [[nodiscard]] std::uint32_t width() const { return image->width; }
    [[nodiscard]] std::uint32_t height() const { return image->height; }
};

/// Read-mostly interning table behind a shared mutex, as SpriteCache.
class FrameTable {
  public:
    /// The id of frame `key`, creating the entry with `make()` on first use;
    /// nullopt when `make` yields no image (the frame cannot be decoded: it is
    /// not entered, so that a list leaves it out). `make` runs on the calling
    /// thread, outside the lock, and returns {header, image}; two threads that
    /// race to enter one key both run it and the first to enter wins.
    template <class Make> std::optional<FrameId> intern(const FrameKey& key, Make&& make) {
        {
            const std::shared_lock lock(mutex_);
            if (const auto it = ids_.find(key); it != ids_.end()) {
                return it->second;
            }
        }
        auto [header, image] = make();
        if (!image) {
            return std::nullopt;
        }
        auto data = std::make_shared<FrameData>();
        data->header = header;
        data->keel = keel_rows(*image);
        data->image = std::move(image);
        return enter(key, std::move(data));
    }

    /// The frame of a building sprite, decoded through `cache` on first use.
    std::optional<FrameId> intern_building(const formats::anlagen::AnlagenFile& anlagen,
                                           SpriteCache& cache, const SpriteKey& key,
                                           const formats::figuren::Frame& frame) {
        return intern(FrameKey::building(key),
                      [&] { return std::pair{frame, cache.get(anlagen, key, frame)}; });
    }

    /// The data of frame `id`; null for an id this table never handed out.
    [[nodiscard]] std::shared_ptr<const FrameData> get(FrameId id) const;
    /// Frames interned so far.
    [[nodiscard]] std::size_t size() const;

  private:
    FrameId enter(const FrameKey& key, std::shared_ptr<const FrameData> data);

    mutable std::shared_mutex mutex_;
    std::map<FrameKey, FrameId> ids_;
    std::vector<std::shared_ptr<const FrameData>> frames_;
};

} // namespace anno::render
