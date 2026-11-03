// The pure House calculation (MAP-POP-IMPL-1 D; docs/sim/population-contract.md section 4,
// docs/sim/population-readiness.md section 7). A function from explicit inputs to labelled outputs:
// no World, no mutation, no I/O, no PRNG, and it is not called from the driver - the House tick
// stays a no-op. It evaluates the numeric steps of the original's tick (AnnoWelt
// 0x10076230-0x100765b9 and the mood function 0x10075fd0-0x10076224) and returns the effects
// (walker spawn, upgrade, downgrade, removal) as deferred labels it never performs. Arithmetic is
// widened and checked: an output that cannot be represented, a reached zero divisor or a missing
// required input is a typed NotEvaluated result, never a clamp, a wrap or a native fault.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <anno/sim/types.hpp>

namespace anno::sim {

enum class HouseKernelStatus : std::uint8_t {
    Skipped,      // the gate was not taken: nothing changes
    Evaluated,    // every step of the taken tick was computed
    NotEvaluated, // see `reason`; fields computed before the failure are kept
};

enum class NotEvaluatedReason : std::uint8_t {
    None,
    GatePeriodOutOfDomain,       // period 0 or above 65535 (the gate compares 16-bit differences)
    MissingGroup,                // no population view
    StockSizeMismatch,           // stock count differs from the group's need count
    StageIndexOutOfDomain,       // a need's stage index is not a bit of the 32-bit mask
    NegativeSmoothedE,           // E' < 0 (OD-4)
    ZeroDivisor,                 // a stage-enabled need reached E' * Menge == 0 (OD-4)
    MissingFirstNeed,            // E' != 0 and the group has no need (mood reads slot 0)
    MoodOutOfRange,              // the mood is outside -2..2 (growth table index)
    Overflow,                    // residents leave the i32 field
    PreviousCapacityUnavailable, // the downgrade floor needs the previous Wohnmax (OD-5)
};

enum class HouseArm : std::uint8_t { None, Downgrade, Remove };

/// An effect the original performs and this function does not.
enum class DeferredEffect : std::uint8_t { None, UpgradeCheck, Downgrade, Remove };

struct WalkerFacts {
    std::uint32_t count = 0;    // walker slots in use
    std::uint32_t type_max = 0; // the type's maximum
};

struct HouseKernelInputs {
    const PopulationView* view = nullptr;
    std::uint32_t player_mask = 0; // the owner's Player::flags
    std::uint32_t now = 0;         // the caller's tick; the gate uses the low 16 bits of now - last
    std::uint16_t last = 0;
    std::uint32_t gate_period = 0; // ClassInfo::gate_period of the House class (2500)
    std::int32_t residents_64 = 0;
    std::uint32_t e = 0;
    std::span<const std::uint32_t> stock;
    std::uint8_t mood_flags = 0; // house +0x0c; only 0x10 and 0x20 are read
    /// The previous group's Wohnmax; nullopt = unavailable (never read from adjacent memory).
    std::optional<std::uint32_t> previous_wohnmax;
    std::optional<WalkerFacts> walkers;
};

struct HouseKernelResult {
    HouseKernelStatus status = HouseKernelStatus::Skipped;
    NotEvaluatedReason reason = NotEvaluatedReason::None;
    std::optional<std::uint16_t> last;   // after a taken gate: last + period (mod 2^16)
    std::optional<std::uint32_t> e;      // E'
    std::vector<std::uint32_t> stock;    // after the decay; empty until the loop ran
    std::optional<std::int64_t> lowest;  // 0x7fffffff when no need was stage-enabled
    std::optional<bool> spawn_condition; // only with walker facts; a condition, never a spawn
    /// Mood diagnostics (present when E' != 0): the slot-0 score (cap 128), the average of the
    /// goods and tavern scores (128 when none) and the lowest forced infrastructure score (start
    /// 160).
    std::optional<std::int64_t> food_score, average_score, infra_score;
    std::optional<int> mood;
    std::optional<std::int64_t> counter_sub, counter_add; // trunc(res/64) before / after growth
    std::optional<bool> cap_crossed;                      // mood >= 0 paths only
    std::optional<std::int64_t> cap_sub;                  // trunc(res/64) - Wohnmax when crossed
    std::optional<std::int64_t> below64_add;              // trunc(res/64) - 64, downgrade arm only
    std::optional<std::int32_t> residents_64;             // after growth, cap and correction
    HouseArm arm = HouseArm::None;
    DeferredEffect effect = DeferredEffect::None;
};

/// The mood combination of the three scores (the tail of AnnoWelt 0x10075fd0): any negative score
/// gives -2 when flag 0x20 is set, else the minimum (flag 0x10 is not read there); otherwise flags
/// 0x30 give -2 (0x20) or -1 (0x10) and the rest trunc((f + b + i + 1) / 3).
[[nodiscard]] int house_mood_combine(int f, int b, int i, std::uint8_t flags);

[[nodiscard]] HouseKernelResult house_kernel(const HouseKernelInputs& in);

} // namespace anno::sim
