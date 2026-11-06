// Route-only136. Explicit layout/semantics: docs/sim/ship-trade-routes.md R1.
#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <anno/sim/ids.hpp>
#include <anno/sim/reject.hpp>

namespace anno::sim {

enum class FreightDirection : std::uint8_t { LoadTarget = 0, UnloadAmount = 1 };
enum class FreightControl : std::uint8_t { Suspend = 0, Resume = 1, Cancel = 2 };
enum class FreightStatus : std::uint8_t { Running = 0, Suspended = 1, Failed = 2 };
enum class FreightPhase : std::uint8_t {
    NeedLeg = 0,
    Sailing = 1,
    Wait = 2,
    Scan = 3,
    Transfer = 4,
    CompleteStop = 5
};
struct FreightAction {
    FreightDirection direction = FreightDirection::LoadTarget;
    GoodsId goods = 0;
    std::uint64_t tons = 0;
    friend bool operator==(const FreightAction&, const FreightAction&) = default;
};
struct FreightStop {
    BuildingId warehouse;
    std::int32_t dock_x_q8 = 0, dock_y_q8 = 0;
    std::vector<FreightAction> actions;
    friend bool operator==(const FreightStop&, const FreightStop&) = default;
};
struct FreightState {
    std::vector<FreightStop> stops;
    FreightStatus status = FreightStatus::Running;
    FreightPhase phase = FreightPhase::NeedLeg;
    std::uint32_t stop_index = 0, action_index = 0;
    std::uint64_t expected_order_id = 0, order_generation = 0;
    std::uint64_t last_tick = 0, credit_ticks = 0;
    std::uint32_t wait_remaining = 0, scan_next = 0;
    std::uint64_t matching_raw = 0, remaining_raw = 0;
    std::vector<std::uint32_t> matching_holds, empty_holds;
    std::uint32_t candidate_next = 0;
    std::uint64_t completed_cycles = 0;
    Reject last_reject = Reject::None;
    std::uint8_t nav_failure = 0; // existing NavFailReason encoding, not a new enum layout
    friend bool operator==(const FreightState&, const FreightState&) = default;
};

// Deep-copy logical optional: unrelated figures pay an empty pointer instead of
// inline route/cursor/vector storage. World snapshots never share mutable progress.
class FreightSlot {
  public:
    FreightSlot() = default;
    FreightSlot(const FreightSlot& other)
        : state_(other.state_ ? std::make_unique<FreightState>(*other.state_) : nullptr) {}
    FreightSlot(FreightSlot&&) noexcept = default;
    FreightSlot& operator=(const FreightSlot& other) {
        if (this != &other)
            state_ = other.state_ ? std::make_unique<FreightState>(*other.state_) : nullptr;
        return *this;
    }
    FreightSlot& operator=(FreightSlot&&) noexcept = default;
    [[nodiscard]] bool has_value() const { return state_ != nullptr; }
    void reset() { state_.reset(); }
    FreightState& emplace() {
        state_ = std::make_unique<FreightState>();
        return *state_;
    }
    FreightState& operator*() { return *state_; }
    const FreightState& operator*() const { return *state_; }
    FreightState* operator->() { return state_.get(); }
    const FreightState* operator->() const { return state_.get(); }

  private:
    std::unique_ptr<FreightState> state_;
};
} // namespace anno::sim
