// Deterministic UI-side helpers for ROADS-3 (see the header).
#include <anno/app/road_drag_ui.hpp>
#include <anno/sim/house_transition.hpp>

namespace anno::app {

namespace {

// Fixed odd increment for 64-bit SplitMix state advancement (UI-only visual variation).
constexpr std::uint64_t kSplitMixGamma = 0x9E3779B97F4A7C15ULL;

std::uint64_t mix64(std::uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

} // namespace

sim::TypeId road_variant_id(const sim::TypeRegistry& registry, sim::TypeId base_id,
                            std::uint8_t byte) {
    const sim::BuildingType* base = registry.find(base_id);
    if (base == nullptr) {
        return base_id;
    }
    const sim::VariantFacts vf = sim::house_variant_facts(*base);
    if (vf.count <= 1 || vf.step == 0) {
        return base_id;
    }
    const std::uint64_t offset = static_cast<std::uint64_t>(byte % vf.count) * vf.step;
    const std::uint64_t cand_raw = static_cast<std::uint64_t>(base_id.v) + offset;
    if (cand_raw > 0xFFFFFFFFULL) {
        return base_id;
    }
    const sim::TypeId cand{static_cast<std::uint32_t>(cand_raw)};
    const sim::BuildingType* vt = registry.find(cand);
    if (vt == nullptr || !sim::is_land_road(*vt)) {
        return base_id;
    }
    return cand;
}

RoadVariantDeck::RoadVariantDeck(std::uint64_t seed)
    : state_(seed != 0 ? seed : 0x524F414431353033ULL) {}

std::uint8_t RoadVariantDeck::next_byte() {
    state_ += kSplitMixGamma;
    return static_cast<std::uint8_t>((mix64(state_) >> 24) & 0xFFU);
}

std::uint8_t RoadVariantDeck::byte_at(std::size_t index) {
    while (bytes_.size() <= index) {
        bytes_.push_back(next_byte());
    }
    return bytes_[index];
}

sim::TypeId RoadVariantDeck::variant_for(const sim::TypeRegistry& registry, sim::TypeId base_id,
                                         std::size_t index) {
    return road_variant_id(registry, base_id, byte_at(index));
}

void RoadVariantDeck::reroll() {
    bytes_.clear();
    state_ += kSplitMixGamma;
    ++generation_;
}

RoadGhostPlan plan_road_ghost(const RoadGhostInputs& in) {
    RoadGhostPlan out;
    if (!in.armed_road || in.path_len == 0 || in.pointer_on_chrome) {
        out.state = GhostState::Hidden;
        return out;
    }
    if (!in.verdict) {
        out.state = in.previous;
        return out;
    }
    out.valid_count = std::min<std::size_t>(in.verdict->valid_prefix, in.path_len);
    out.affordable_count = std::min<std::size_t>(in.verdict->affordable_prefix, out.valid_count);
    if (out.valid_count == 0) {
        out.state = GhostState::Refused;
        out.draw_count = 0;
        out.draw_refusal_box = true;
        return out;
    }
    if (out.affordable_count >= out.valid_count) {
        out.state = GhostState::Valid;
        out.draw_count = out.valid_count;
        return out;
    }
    out.state = GhostState::Unaffordable;
    out.draw_count = in.blink_hidden ? out.affordable_count : out.valid_count;
    return out;
}

} // namespace anno::app
