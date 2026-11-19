#include <algorithm>
#include <utility>

#include <anno/sim_load/wildlife_species.hpp>

namespace anno::sim_load {
formats::Result<WildlifeCreationPlan, WildlifePlanError> prepare_wildlife_creation_plan(
    formats::Bytes welt, formats::Bytes game, std::span<const formats::figuren::Entry> entries,
    formats::annowelt::WildlifeSpeciesProfile profile, std::uint32_t climate, bool cave,
    std::size_t entrances, sim::GoodsId saved, std::span<const std::uint32_t> draws) {
    const auto ring = formats::annowelt::read_wildlife_species_ring(welt, profile, climate);
    if (!ring)
        return WildlifePlanError{WildlifePlanFault::Source, ring.error()};
    std::vector<sim::TypeId> requested;
    for (const auto id : ring.value().ids)
        requested.emplace_back(id);
    const auto facts =
        bind_wildlife_species_facts(WildlifeDescriptorProfile::Gog, requested, entries, game);
    if (!facts)
        return WildlifePlanError{WildlifePlanFault::Source, facts.error()};
    std::vector<WildlifeSpeciesFact> slots;
    std::vector<sim::WildlifeSpecies> species;
    for (const auto id : ring.value().ids) {
        const auto found = std::find_if(facts.value().begin(), facts.value().end(),
                                        [id](const auto& fact) { return fact.id.v == id; });
        if (found == facts.value().end())
            return WildlifePlanError{WildlifePlanFault::Source, "species plan: missing bound fact"};
        slots.push_back(*found);
        species.push_back({found->id, found->yield});
    }
    const auto choice = sim::wildlife_create(cave, entrances, species, saved, draws);
    if (!choice)
        return WildlifePlanError{WildlifePlanFault::Selector, {}, choice.error()};
    const auto selected = slots[choice.value().value.index];
    std::optional<WildlifeCreationDescriptor> creation;
    auto disposition = WildlifePlanDisposition::UnsupportedCreationKind;
    if (selected.kind == 49 || selected.kind == 51) {
        const std::array requested_selected{selected.id};
        auto bound = bind_wildlife_creation_descriptors(WildlifeDescriptorProfile::Gog,
                                                        requested_selected, entries, game);
        if (!bound)
            return WildlifePlanError{WildlifePlanFault::Source, bound.error()};
        creation = std::move(bound.value().front());
        disposition = WildlifePlanDisposition::SupportedNew;
    }
    return WildlifeCreationPlan{ring.value(), std::move(slots), choice.value(),
                                selected,     disposition,      std::move(creation)};
}
} // namespace anno::sim_load
