// Route-only136. Explicit layout/semantics: docs/sim/ship-trade-routes.md R1.
#pragma once

#include <compare>
#include <cstdint>
#include <memory>
#include <type_traits>
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
struct FreightHeapNode {
    std::uint64_t inverse_room = 0;
    std::uint32_t hold = 0;
    friend auto operator<=>(const FreightHeapNode&, const FreightHeapNode&) = default;
};
static_assert(std::is_trivially_destructible_v<FreightHeapNode>);
struct FreightHoldIndex {
    std::uint64_t generation = 0;
    std::uint32_t stop = 0, action = 0, scanned = 0, empty_next = 0;
    std::vector<FreightHeapNode> rooms;
    std::vector<std::uint32_t> positions, empty;
    FreightHoldIndex() = default;
    FreightHoldIndex(const FreightHoldIndex& other)
        : generation(other.generation), stop(other.stop), action(other.action),
          scanned(other.scanned), empty_next(other.empty_next) {
        // Snapshot work occurs outside visits. Preserve reserved raw capacities,
        // so continuing a partially scanned copy cannot cause a prefix copy in a visit.
        rooms.reserve(other.rooms.capacity());
        positions.reserve(other.positions.capacity());
        empty.reserve(other.empty.capacity());
        rooms.assign(other.rooms.begin(), other.rooms.end());
        positions.assign(other.positions.begin(), other.positions.end());
        empty.assign(other.empty.begin(), other.empty.end());
    }
    FreightHoldIndex(FreightHoldIndex&&) noexcept = default;
    FreightHoldIndex& operator=(FreightHoldIndex&&) noexcept = default;
    void reset() {
        generation = 0;
        stop = 0;
        action = 0;
        scanned = 0;
        empty_next = 0;
        // Trivial elements: no per-hold destructor walk; retain raw buffers.
        rooms.clear();
        positions.clear();
        empty.clear();
    }
};
struct FreightWork {
    std::uint64_t visits = 0, steps = 0, hold_scans = 0, resolver_queries = 0;
    std::uint64_t index_updates = 0, target_checks = 0, transfers = 0, cold_scans = 0;
    std::uint64_t heap_steps = 0;
};

// One pointer on unrelated figures. Snapshots own independent canonical state
// and derived index; cancelling/ending the assignment frees both together.
class FreightSlot {
    struct Storage {
        FreightState state;
        FreightHoldIndex index;
        Storage() = default;
        Storage(const Storage& other) : state(other.state), index(other.index) {
            state.matching_holds.reserve(other.state.matching_holds.capacity());
            state.empty_holds.reserve(other.state.empty_holds.capacity());
        }
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
