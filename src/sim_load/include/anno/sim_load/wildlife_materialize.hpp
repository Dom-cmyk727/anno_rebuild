// Ordered selector plans only; docs/sim/wildlife-materialize-plan.md.
#pragma once
#include <anno/sim_load/wildlife_chunk.hpp>
#include <anno/sim_load/wildlife_species.hpp>

namespace anno::sim_load {
enum class WildlifeTileResolution { Unknown, Null, Present };
struct WildlifeFinalTileFact {
    std::uint8_t position = 0;
    WildlifeTileResolution resolution = WildlifeTileResolution::Unknown;
    std::optional<std::uint32_t> object_word; // Whole widened word, never a masked/grown ID.
    bool operator==(const WildlifeFinalTileFact&) const = default;
};
struct WildlifeFinalTypeFact {
    std::uint32_t object_word = 0;
    std::optional<sim::GoodsId> output0;
    std::optional<std::uint32_t> prodtype;
    bool operator==(const WildlifeFinalTypeFact&) const = default;
};
struct WildlifeCaveFact {
    std::uint8_t position = 0;
    std::optional<std::size_t> entrances; // Resolved count only; not geometry/provider execution.
    bool operator==(const WildlifeCaveFact&) const = default;
};
struct WildlifeMaterializeFacts {
    std::vector<WildlifeFinalTileFact> tiles;
    std::vector<WildlifeFinalTypeFact> types;
    std::vector<WildlifeCaveFact> caves;
    bool operator==(const WildlifeMaterializeFacts&) const = default;
};
struct WildlifeSpeciesSource {
    formats::Bytes welt, game;
    std::span<const formats::figuren::Entry> entries;
    formats::annowelt::WildlifeSpeciesProfile profile =
        formats::annowelt::WildlifeSpeciesProfile::GogClimate2;
    std::uint32_t climate = 0; // Required explicitly when the first selection is reached.
};
enum class WildlifeMaterializeDisposition {
    WrongCategory,
    Occupied,
    NotReady,
    NullType,
    NonqualifyingType,
    Selected,
    UnsupportedSelection,
    Faulted
};
struct WildlifeMaterializeStep {
    std::uint64_t token = 0;
    WildlifeMaterializeDisposition disposition = WildlifeMaterializeDisposition::Faulted;
    std::size_t selector_consumed = 0, cumulative_selector_consumed = 0;
    std::optional<WildlifeFinalTileFact> tile;
    std::optional<WildlifeFinalTypeFact> type;
    std::optional<WildlifeCaveFact> cave;
    std::optional<WildlifeSpeciesSelection> selection;
};
struct WildlifeMaterializeProgress {
    WildlifeChunkPlan unchanged_chunk;
    std::optional<WildlifeSpeciesCatalogue> catalogue;
    std::vector<WildlifeMaterializeStep> entries;
    std::size_t selector_consumed = 0;
    std::vector<std::uint32_t> unused_draws;
};
struct WildlifeMaterializePlan {
    WildlifeMaterializeProgress completed;
};
enum class WildlifeMaterializeFault {
    MissingFacts,
    AmbiguousFacts,
    InvalidFacts,
    Source,
    Selector,
    UnsupportedCreationKind
};
enum class WildlifeMaterializeStage {
    Token,
    Occupancy,
    Timestamp,
    Position,
    Tile,
    ActualType,
    Output0,
    Kind,
    Cave,
    SavedGoods,
    Catalogue,
    Selector,
    UnsupportedSelection
};
struct WildlifeMaterializeFailure {
    WildlifeMaterializeFault fault;
    WildlifeMaterializeStage stage;
    std::string reason;
    WildlifeMaterializeProgress diagnostic_only;
    WildlifeMaterializeStep failing_record;
    std::optional<WildlifePlanError> selection_error;
};
[[nodiscard]] formats::Result<WildlifeMaterializePlan, WildlifeMaterializeFailure>
prepare_wildlife_materialization(const WildlifeChunkPlan&, std::uint32_t time,
                                 const WildlifeMaterializeFacts&, const WildlifeSpeciesSource&,
                                 std::span<const std::uint32_t> draws);
} // namespace anno::sim_load
