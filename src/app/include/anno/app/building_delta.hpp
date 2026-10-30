// Building changes for the play picture (docs/app/game-shell.md, "Building
// changes after tick 0"). The sim thread diffs the buildings it owns after
// each turn; the window applies the batches it takes from the mailbox. No
// sim type appears here: a change is the picture's own record.
#pragma once

#include <cstdint>
#include <vector>

#include <anno/app/loader.hpp>

namespace anno::app {

/// One building the picture must add or remove. A change of rotation, height
/// or tile is a removed delta and an added delta.
struct BuildingDelta {
    enum class Kind : std::uint8_t { Added, Removed };
    Kind kind = Kind::Added;
    SceneBuilding building;
    friend bool operator==(const BuildingDelta&, const BuildingDelta&) = default;
};

/// The deltas of one turn, in the order the diff produced them (removed,
/// then added, each in building order). Immutable once published.
struct BuildingBatch {
    std::uint64_t turn = 0;
    std::vector<BuildingDelta> deltas;
};

/// The order of the diff: island, then tile, then type, rotation, height.
/// It is a total order on the record, so equal buildings sit next to each other.
[[nodiscard]] bool building_before(const SceneBuilding& a, const SceneBuilding& b);

/// Sorts `buildings` by building_before (ordered, no hash iteration).
void sort_buildings(std::vector<SceneBuilding>& buildings);

/// The multiset difference of two lists sorted by building_before: removed =
/// prev minus cur, added = cur minus prev. A building present twice on both
/// sides yields nothing. Returns an empty vector when nothing changed.
[[nodiscard]] std::vector<BuildingDelta>
diff_sorted_buildings(const std::vector<SceneBuilding>& prev,
                      const std::vector<SceneBuilding>& cur);

} // namespace anno::app
