#include <algorithm>

#include <anno/render/residency.hpp>

namespace anno::render {

AtlasResidency::AtlasResidency(std::uint32_t atlas_size, std::uint64_t budget_bytes)
    : size_(atlas_size) {
    const std::uint64_t layer = std::uint64_t{atlas_size} * atlas_size * 4;
    max_layers_ = layer == 0 ? 0 : static_cast<std::size_t>(budget_bytes / layer);
}

std::optional<AtlasPlace> AtlasResidency::shelf_fit(Layer& layer, std::uint32_t size,
                                                    std::uint32_t w, std::uint32_t h) {
    std::uint32_t x = layer.x, y = layer.y, shelf = layer.shelf;
    if (x + w > size) {
        y += shelf;
        x = 0;
        shelf = 0;
    }
    if (y + h > size) {
        return std::nullopt;
    }
    layer.x = x + w;
    layer.y = y;
    layer.shelf = std::max(shelf, h);
    return AtlasPlace{0, x, y, w, h};
}

void AtlasResidency::evict(std::size_t index) {
    Layer& layer = layers_[index];
    for (const FrameId id : layer.frames) {
        places_.erase(id);
        evicted_.push_back(id);
    }
    stats_.evicted_frames += layer.frames.size();
    ++stats_.evicted_layers;
    layer.frames.clear();
    layer.x = layer.y = layer.shelf = 0;
}

std::optional<AtlasPlace> AtlasResidency::allocate(AtlasClass cls, std::uint32_t w,
                                                   std::uint32_t h) {
    auto place_in = [&](std::size_t index) -> std::optional<AtlasPlace> {
        auto p = shelf_fit(layers_[index], size_, w, h);
        if (p) {
            p->layer = static_cast<std::uint32_t>(index);
            layers_[index].last_used = frame_;
        }
        return p;
    };
    for (std::size_t i = 0; i < layers_.size(); ++i) {
        if (layers_[i].cls == cls) {
            if (const auto p = place_in(i)) {
                return p;
            }
        }
    }
    if (layers_.size() < max_layers_) {
        layers_.push_back(Layer{cls, frame_, 0, 0, 0, {}});
        return place_in(layers_.size() - 1);
    }
    // The least recently used layer of the class that nothing this frame reads.
    std::optional<std::size_t> victim;
    for (std::size_t i = 0; i < layers_.size(); ++i) {
        const Layer& l = layers_[i];
        if (l.cls == cls && l.last_used < frame_ &&
            (!victim || l.last_used < layers_[*victim].last_used)) {
            victim = i;
        }
    }
    if (!victim) {
        return std::nullopt;
    }
    evict(*victim);
    return place_in(*victim);
}

EnsureResult AtlasResidency::ensure(std::span<const FrameRef> frames, AtlasClass cls,
                                    std::uint64_t max_upload_bytes) {
    EnsureResult result;
    // Pin what is resident first, so that making room never evicts it.
    std::vector<FrameRef> missing;
    for (const FrameRef& f : frames) {
        if (const auto it = places_.find(f.id); it != places_.end()) {
            layers_[it->second.layer].last_used = frame_;
        } else {
            missing.push_back(f);
        }
    }
    std::sort(missing.begin(), missing.end(),
              [](const FrameRef& a, const FrameRef& b) { return a.id < b.id; });
    missing.erase(std::unique(missing.begin(), missing.end(),
                              [](const FrameRef& a, const FrameRef& b) { return a.id == b.id; }),
                  missing.end());
    if (missing.empty()) {
        return result;
    }
    std::uint64_t bytes = 0;
    for (const FrameRef& f : missing) {
        if (f.width == 0 || f.height == 0 || f.width > size_ || f.height > size_) {
            // Larger than a layer: no eviction helps.
            ++stats_.chunks_cpu_fallback;
            result.status = EnsureStatus::Overflow;
            return result;
        }
        bytes += std::uint64_t{f.width} * f.height * 4;
    }
    if (bytes > max_upload_bytes) {
        result.status = EnsureStatus::Deferred;
        return result;
    }
    // Tallest first packs the shelves tightest; the id breaks ties, so the
    // layout does not depend on the order the caller listed the frames in.
    std::sort(missing.begin(), missing.end(), [](const FrameRef& a, const FrameRef& b) {
        return a.height != b.height ? a.height > b.height : a.id < b.id;
    });
    result.status = EnsureStatus::Uploaded;
    for (const FrameRef& f : missing) {
        const auto p = allocate(cls, f.width, f.height);
        if (!p) {
            ++stats_.chunks_cpu_fallback;
            result.status = EnsureStatus::Overflow;
            return result;
        }
        places_.emplace(f.id, *p);
        layers_[p->layer].frames.push_back(f.id);
        result.uploads.emplace_back(f.id, *p);
        const std::uint64_t b = std::uint64_t{f.width} * f.height * 4;
        result.upload_bytes += b;
        ++stats_.uploaded_frames;
        stats_.uploaded_bytes += b;
    }
    return result;
}

std::optional<AtlasPlace> AtlasResidency::place(FrameId id) const {
    const auto it = places_.find(id);
    if (it == places_.end()) {
        return std::nullopt;
    }
    return it->second;
}

ResidencyStats AtlasResidency::stats() const {
    ResidencyStats s = stats_;
    s.layers = layers_.size();
    s.resident_frames = places_.size();
    s.layer_bytes = std::uint64_t{layers_.size()} * size_ * size_ * 4;
    for (const auto& [id, p] : places_) {
        s.resident_bytes += std::uint64_t{p.width} * p.height * 4;
    }
    return s;
}

} // namespace anno::render
