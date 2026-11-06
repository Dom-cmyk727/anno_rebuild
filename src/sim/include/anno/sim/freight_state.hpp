// Route-only136. Explicit layout/semantics: docs/sim/ship-trade-routes.md R1.
#pragma once

#include <cstdint>
#include <memory>
#include <set>
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

// Derived per-action resolver index. Ordered by inverse room then hold index,
// exactly matching maximum-room/lowest-index and first-empty manual selection.
// Never serialized, hashed or used as transfer authority. Only active routes
// allocate it; a native reader rebuilds it outside figure visits.
struct FreightHoldIndex {
    std::uint64_t generation = 0;
    std::uint32_t stop = 0, action = 0, scanned = 0;
    std::set<std::pair<std::uint64_t, std::uint32_t>> rooms;
    std::set<std::uint32_t> empty;
};
struct FreightWork {
    std::uint64_t visits = 0, steps = 0, hold_scans = 0, resolver_queries = 0;
    std::uint64_t index_updates = 0, target_checks = 0, transfers = 0, cold_scans = 0;
};

// One pointer on unrelated figures. Snapshots own independent canonical state
// and derived index; cancelling/ending the assignment frees both together.
class FreightSlot {
    struct Storage {
        FreightState state;
        FreightHoldIndex index;
    };

  public:
    FreightSlot() = default;
    FreightSlot(const FreightSlot& other)
        : storage_(other.storage_ ? std::make_unique<Storage>(*other.storage_) : nullptr) {}
    FreightSlot(FreightSlot&&) noexcept = default;
    FreightSlot& operator=(const FreightSlot& other) {
        if (this != &other)
            storage_ = other.storage_ ? std::make_unique<Storage>(*other.storage_) : nullptr;
        return *this;
    }
    FreightSlot& operator=(FreightSlot&&) noexcept = default;
    [[nodiscard]] bool has_value() const { return storage_ != nullptr; }
    void reset() { storage_.reset(); }
    FreightState& emplace() {
        storage_ = std::make_unique<Storage>();
        return storage_->state;
    }
    FreightState& operator*() { return storage_->state; }
    const FreightState& operator*() const { return storage_->state; }
    FreightState* operator->() { return &storage_->state; }
    const FreightState* operator->() const { return &storage_->state; }
    FreightHoldIndex& index() { return storage_->index; }

  private:
    std::unique_ptr<Storage> storage_;
};
} // namespace anno::sim
