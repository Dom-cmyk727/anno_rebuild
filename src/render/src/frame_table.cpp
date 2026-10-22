#include <anno/render/frame_table.hpp>

namespace anno::render {

FrameId FrameTable::enter(const FrameKey& key, std::shared_ptr<const FrameData> data) {
    const std::unique_lock lock(mutex_);
    const auto [it, inserted] = ids_.try_emplace(key, static_cast<FrameId>(frames_.size()));
    if (inserted) {
        // A u32 id limits one scene to 4 * 10^9 distinct frames: the GPU's
        // indices are 32-bit (docs/render/gpu-chunks.md section 9).
        frames_.push_back(std::move(data));
    }
    return it->second;
}

std::shared_ptr<const FrameData> FrameTable::get(FrameId id) const {
    const std::shared_lock lock(mutex_);
    return id < frames_.size() ? frames_[id] : nullptr;
}

std::size_t FrameTable::size() const {
    const std::shared_lock lock(mutex_);
    return frames_.size();
}

} // namespace anno::render
