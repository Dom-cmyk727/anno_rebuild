#pragma once
#include <span>

#include <anno/sim/player.hpp>
#include <anno/sim/types.hpp>
namespace anno::sim {
// Explicit context until scenario ANLTRG/script permission state has its own integration contract.
struct BuildPermissionContext {
    PlayerId owner;
    std::span<const BuildPermission> entries;
};
enum class BuildEligibilityStatus {
    Allowed,
    UnknownPlayer,
    UnknownType,
    InvalidPermissions,
    ScenarioDenied,
    StageLocked,
    ResearchRequired
};
struct BuildEligibility {
    BuildEligibilityStatus status = BuildEligibilityStatus::UnknownPlayer;
    std::uint32_t stage = 0;
    /// Raw override byte when present; otherwise normalized0/1. Query preserves original
    /// truthiness.
    std::uint32_t value = 0;
    [[nodiscard]] bool allowed() const { return status == BuildEligibilityStatus::Allowed; }
};
/// Pure, allocation-free. Overrides must belong to owner and be unique ascending types,
/// values0..255. Result explains content eligibility only; terrain, payment and command
/// authorization are separate. This is not currently wired into BuildCmd (owner explicitly blocked
/// pending scenario contract).
[[nodiscard]] BuildEligibility build_eligibility(const TypeRegistry&, const PlayerTable&, PlayerId,
                                                 TypeId, BuildPermissionContext = {});
} // namespace anno::sim
