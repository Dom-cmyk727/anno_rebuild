#include <map>
#include <set>
#include <utility>

#include <anno/sim_load/wildlife_materialize.hpp>

namespace anno::sim_load {
formats::Result<WildlifeMaterializePlan, WildlifeMaterializeFailure>
prepare_wildlife_materialization(const WildlifeChunkPlan& chunk, std::uint32_t time,
                                 const WildlifeMaterializeFacts& facts,
                                 const WildlifeSpeciesSource& source,
                                 std::span<const std::uint32_t> draws) {
    WildlifeMaterializeProgress work;
    work.unchanged_chunk = chunk;
    // Index keys only. Unreached ambiguous/missing properties never become an early refusal.
    std::map<std::uint8_t, const WildlifeFinalTileFact*> tiles;
    std::map<std::uint32_t, const WildlifeFinalTypeFact*> types;
    std::map<std::uint8_t, const WildlifeCaveFact*> caves;
    std::set<std::uint8_t> duplicate_tiles, duplicate_caves;
    std::set<std::uint32_t> duplicate_types;
    for (const auto& fact : facts.tiles)
        if (!tiles.emplace(fact.position, &fact).second)
            duplicate_tiles.insert(fact.position);
    for (const auto& fact : facts.types)
        if (!types.emplace(fact.object_word, &fact).second)
            duplicate_types.insert(fact.object_word);
    for (const auto& fact : facts.caves)
        if (!caves.emplace(fact.position, &fact).second)
            duplicate_caves.insert(fact.position);
    const auto suffix = [&] {
        const auto remaining = draws.subspan(work.selector_consumed);
        work.unused_draws.assign(remaining.begin(), remaining.end());
    };
    using F = WildlifeMaterializeFault;
    using S = WildlifeMaterializeStage;
    using D = WildlifeMaterializeDisposition;
    WildlifeMaterializeStep step;
    const auto fail = [&](F fault, S stage, std::string reason,
                          std::optional<WildlifePlanError> selection_error = {}) {
        suffix();
        return WildlifeMaterializeFailure{fault, stage, std::move(reason),
                                          work,  step,  std::move(selection_error)};
    };
    const auto skip = [&](D disposition) {
        step.disposition = disposition;
        step.cumulative_selector_consumed = work.selector_consumed;
        work.entries.push_back(step);
    };
    const auto now = (time / 1000U) & 255U;
    std::set<std::uint64_t> tokens;
    for (const auto& record : chunk.completed.chunk.records) {
        step = {};
        step.token = record.token;
        step.cumulative_selector_consumed = work.selector_consumed;
        if (!tokens.insert(record.token).second)
            return fail(F::AmbiguousFacts, S::Token, "materialize: duplicate record token");
        // Welt 0x10093780 ->0x10093798 ->0x100937a4: exact reached-field order.
        if (record.category != 0) {
            skip(D::WrongCategory);
            continue;
        }
        if (!record.occupied)
            return fail(F::MissingFacts, S::Occupancy, "materialize: unknown occupancy");
        if (*record.occupied) {
            skip(D::Occupied);
            continue;
        }
        if (!record.timestamp)
            return fail(F::MissingFacts, S::Timestamp, "materialize: unknown timestamp");
        const auto delta = (now + 256U - *record.timestamp) & 255U;
        if (delta >= 128U) {
            skip(D::NotReady);
            continue;
        }
        if (!record.position)
            return fail(F::MissingFacts, S::Position, "materialize: unknown position");
        if (*record.position > 63)
            return fail(F::InvalidFacts, S::Position,
                        "materialize: outside original local position");
        const auto position = *record.position;
        if (duplicate_tiles.contains(position))
            return fail(F::AmbiguousFacts, S::Tile, "materialize: ambiguous tile lookup");
        const auto tile = tiles.find(position);
        if (tile == tiles.end())
            return fail(F::MissingFacts, S::Tile, "materialize: missing tile lookup");
        step.tile = *tile->second;
        if (step.tile->resolution == WildlifeTileResolution::Unknown)
            return fail(F::MissingFacts, S::Tile, "materialize: unknown tile lookup outcome");
        if (step.tile->resolution == WildlifeTileResolution::Null) {
            skip(D::NullType);
            continue;
        }
        if (step.tile->resolution != WildlifeTileResolution::Present)
            return fail(F::InvalidFacts, S::Tile, "materialize: unsupported tile lookup outcome");
        if (!step.tile->object_word)
            return fail(F::MissingFacts, S::ActualType, "materialize: unknown actual object word");
        const auto word = *step.tile->object_word;
        if (duplicate_types.contains(word))
            return fail(F::AmbiguousFacts, S::ActualType, "materialize: ambiguous actual type");
        const auto found_type = types.find(word);
        if (found_type == types.end())
            return fail(F::MissingFacts, S::ActualType, "materialize: missing actual type");
        const auto* actual = found_type->second; // NO growing-type/id+1 substitution.
        step.type = *actual;
        if (!actual->output0)
            return fail(F::MissingFacts, S::Output0, "materialize: unknown output-0 goods");
        if (*actual->output0 != 0x46) {
            if (!actual->prodtype)
                return fail(F::MissingFacts, S::Kind,
                            "materialize: unknown actual production kind");
            if (*actual->prodtype != 0x145) {
                skip(D::NonqualifyingType);
                continue;
            }
        }
        // Helper also reads the actual kind even when sweep goods already qualified.
        if (!actual->prodtype)
            return fail(F::MissingFacts, S::Kind, "materialize: unknown helper production kind");
        const auto cave = *actual->prodtype == 0x145;
        std::size_t entrances = 0;
        if (cave) {
            if (duplicate_caves.contains(position))
                return fail(F::AmbiguousFacts, S::Cave, "materialize: ambiguous cave facts");
            const auto found_cave = caves.find(position);
            if (found_cave == caves.end() || !found_cave->second->entrances)
                return fail(F::MissingFacts, S::Cave, "materialize: missing entrance count");
            step.cave = *found_cave->second;
            entrances = *step.cave->entrances; // Known zero stays zero, not missing.
        }
        if (!record.goods)
            return fail(F::MissingFacts, S::SavedGoods, "materialize: unknown saved goods");
        if (!work.catalogue) {
            auto prepared = prepare_wildlife_species_catalogue(
                source.welt, source.game, source.entries, source.profile, source.climate);
            if (!prepared)
                return fail(F::Source, S::Catalogue, prepared.error().source, prepared.error());
            work.catalogue = std::move(prepared.value());
        }
        const auto remaining = draws.subspan(work.selector_consumed);
        auto selected = work.catalogue->select(cave, entrances, *record.goods, remaining);
        const auto used =
            selected ? selected.value().choice.consumed : selected.error().selector.consumed;
        if (used > remaining.size())
            return fail(F::InvalidFacts, S::Selector, "materialize: invalid selector accounting");
        step.selector_consumed = used;
        work.selector_consumed += used;
        step.cumulative_selector_consumed = work.selector_consumed;
        if (!selected)
            return fail(F::Selector, S::Selector, "materialize: selector refused",
                        selected.error());
        step.selection = std::move(selected.value());
        if (step.selection->disposition == WildlifePlanDisposition::UnsupportedCreationKind) {
            step.disposition = D::UnsupportedSelection;
            return fail(F::UnsupportedCreationKind, S::UnsupportedSelection,
                        "materialize: unsupported selected creation kind");
        }
        step.disposition = D::Selected;
        work.entries.push_back(std::move(step));
    }
    suffix();
    return WildlifeMaterializePlan{std::move(work)};
}
} // namespace anno::sim_load
