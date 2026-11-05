#include <anno/sim/build_availability.hpp>
#include <anno/sim/goods_metadata.hpp>
#include <anno/sim/world.hpp>
namespace anno::sim {
BuildEligibility World::construction_eligibility(PlayerId owner, TypeId type) const {
    if (script_.state.context != ScriptContext::Ready &&
        script_.state.context != ScriptContext::ExplicitNone)
        return {BuildEligibilityStatus::ContextUnavailable};
    const auto* player = players_.find(owner);
    if (!player)
        return {BuildEligibilityStatus::UnknownPlayer};
    return stored_build_eligibility(*types_, players_, owner, type, player->permissions);
}
BuildEligibility build_eligibility(const TypeRegistry& types, const PlayerTable& players,
                                   PlayerId owner, TypeId type,
                                   BuildPermissionContext permissions) {
    const Player* player = players.find(owner);
    if (player == nullptr) {
        return {BuildEligibilityStatus::UnknownPlayer};
    }
    const BuildingType* building = types.find(type);
    if (building == nullptr) {
        return {BuildEligibilityStatus::UnknownType};
    }
    const auto& facts = building->construction;
    if ((!permissions.entries.empty() && permissions.owner != owner) ||
        facts.stage >= kStageIndexCount) {
        return {BuildEligibilityStatus::InvalidPermissions, facts.stage};
    }
    for (std::size_t i = 0; i < permissions.entries.size(); ++i) {
        const auto& p = permissions.entries[i];
        if (p.value > 255 || types.find(p.type) == nullptr ||
            (i != 0 && !(permissions.entries[i - 1].type < p.type))) {
            return {BuildEligibilityStatus::InvalidPermissions, facts.stage};
        }
    }
    for (const auto& p : permissions.entries) {
        if (p.type == type) {
            return {p.value != 0 ? BuildEligibilityStatus::Allowed
                                 : BuildEligibilityStatus::ScenarioDenied,
                    facts.stage, p.value};
        }
    }
    if ((player->flags & (std::uint32_t{1} << facts.stage)) != 0) {
        return {BuildEligibilityStatus::Allowed, facts.stage, 1};
    }
    for (const auto research : facts.research_any) {
        if (player->research.researched(research)) {
            return {BuildEligibilityStatus::Allowed, facts.stage, 1};
        }
    }
    return {facts.research_any.empty() ? BuildEligibilityStatus::StageLocked
                                       : BuildEligibilityStatus::ResearchRequired,
            facts.stage};
}
} // namespace anno::sim
