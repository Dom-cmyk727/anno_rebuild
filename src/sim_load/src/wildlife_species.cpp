#include <algorithm>
#include <utility>

#include <anno/sim_load/wildlife_species.hpp>

namespace anno::sim_load {
formats::Result<WildlifeSpeciesCatalogue, WildlifePlanError> prepare_wildlife_species_catalogue(
    formats::Bytes welt, formats::Bytes game, std::span<const formats::figuren::Entry> entries,
    formats::annowelt::WildlifeSpeciesProfile profile, std::uint32_t climate) {
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
    for (const auto id : ring.value().ids) {
        const auto found = std::find_if(facts.value().begin(), facts.value().end(),
                                        [id](const auto& fact) { return fact.id.v == id; });
        if (found == facts.value().end())
            return WildlifePlanError{WildlifePlanFault::Source, "species plan: missing bound fact"};
        slots.push_back(*found);
    }
    return WildlifeSpeciesCatalogue(ring.value(), std::move(slots));
}
WildlifeSpeciesCatalogue::WildlifeSpeciesCatalogue(formats::annowelt::WildlifeSpeciesRing ring,
                                                   std::vector<WildlifeSpeciesFact> slots)
    : ring_(std::move(ring)), slots_(std::move(slots)) {
    for (const auto& fact : slots_)
        species_.push_back({fact.id, fact.yield});
}
formats::Result<WildlifeSpeciesSelection, WildlifePlanError>
WildlifeSpeciesCatalogue::select(bool cave, std::size_t entrances, sim::GoodsId saved,
                                 std::span<const std::uint32_t> draws) const {
    const auto choice = sim::wildlife_create(cave, entrances, species_, saved, draws);
    if (!choice)
        return WildlifePlanError{WildlifePlanFault::Selector, {}, choice.error()};
    const auto selected = slots_[choice.value().value.index];
    std::optional<WildlifeCreationDescriptor> creation;
    auto disposition = WildlifePlanDisposition::UnsupportedCreationKind;
    if (selected.kind == 49 || selected.kind == 51) {
        // Source preparation validated these same New fields. Only the selected
        // type is sealed; never eagerly New-bind unsupported slots or reparse PE.
        creation = WildlifeCreationDescriptor(
            selected.id, selected.kind, selected.hp, selected.yield, selected.image_sha256,
            selected.source_sha256, selected.logic_present, selected.ware_present);
        disposition = WildlifePlanDisposition::SupportedNew;
    }
    return WildlifeSpeciesSelection{choice.value(), selected, disposition, std::move(creation)};
}
formats::Result<WildlifeCreationPlan, WildlifePlanError> prepare_wildlife_creation_plan(
    formats::Bytes welt, formats::Bytes game, std::span<const formats::figuren::Entry> entries,
    formats::annowelt::WildlifeSpeciesProfile profile, std::uint32_t climate, bool cave,
    std::size_t entrances, sim::GoodsId saved, std::span<const std::uint32_t> draws) {
    const auto catalogue =
        prepare_wildlife_species_catalogue(welt, game, entries, profile, climate);
    if (!catalogue)
        return catalogue.error();
    auto selection = catalogue.value().select(cave, entrances, saved, draws);
    if (!selection)
        return selection.error();
    return WildlifeCreationPlan{
        catalogue.value().ring(),      catalogue.value().slots(),
        selection.value().choice,      selection.value().selected,
        selection.value().disposition, std::move(selection.value().creation)};
}
} // namespace anno::sim_load
