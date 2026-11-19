// Source-backed creation plan only; docs/sim/wildlife-species-load.md.
#pragma once
#include <anno/formats/annowelt.hpp>
#include <anno/sim/wildlife.hpp>
#include <anno/sim_load/wildlife_create.hpp>

namespace anno::sim_load {
enum class WildlifePlanDisposition { SupportedNew, UnsupportedCreationKind };
enum class WildlifePlanFault { Source, Selector };
struct WildlifePlanError {
    WildlifePlanFault stage;
    std::string source;
    sim::WildlifeError selector{sim::WildlifeFault::InvalidInput, 0};
};
struct WildlifeCreationPlan {
    formats::annowelt::WildlifeSpeciesRing ring;
    std::vector<WildlifeSpeciesFact> slot_facts; // All 32 slots including duplicate IDs.
    sim::WildlifeDecision<sim::WildlifeCreation> choice;
    WildlifeSpeciesFact selected;
    WildlifePlanDisposition disposition;
    std::optional<WildlifeCreationDescriptor> creation;
};
[[nodiscard]] formats::Result<WildlifeCreationPlan, WildlifePlanError>
prepare_wildlife_creation_plan(formats::Bytes welt, formats::Bytes game,
                               std::span<const formats::figuren::Entry>,
                               formats::annowelt::WildlifeSpeciesProfile, std::uint32_t climate,
                               bool cave, std::size_t entrances, sim::GoodsId saved,
                               std::span<const std::uint32_t> draws);
} // namespace anno::sim_load
